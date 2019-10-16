#ifndef READANALYZER_HPP
#define READANALYZER_HPP

#include "bloomfilter.h"
#include "kmer_utils.hpp"
#include <vector>
#include <array>

using namespace std;

class ReadAnalyzer {
private:
  BF *bf;
  const vector<string>& legend_ID;
  uint k;
  double c;

public:
  ReadAnalyzer(BF *_bf, const vector<string>& _legend_ID, uint _k, double _c) :
    bf(_bf), legend_ID(_legend_ID), k(_k), c(_c) {}

  vector<array<string, 4>>* operator()(vector<pair<string, string>> *reads) const {
    vector<array<string, 4>> *associations = new vector<array<string, 4>>();
    vector<int> genes_idx;
    typedef pair<pair<unsigned int, unsigned int>, unsigned int> gene_cov_t;
    map<int, gene_cov_t> classification_id;
    for(const auto & p : *reads) {
      classification_id.clear();
      const string& read_name = p.first;
      const string& read_seq = p.second;
      if(read_seq.size() >= k) {
        int pos = 0;
        uint64_t kmer = build_kmer(read_seq, pos, k);
        if(kmer == (uint64_t)-1) continue;
        uint64_t rckmer = revcompl(kmer, k);
        IDView id_kmer = bf->get_index(min(kmer, rckmer));

	if(id_kmer.empty() && bf->test_dummy_kmer(min(kmer, rckmer))) {
	  auto& gene_cov = classification_id[100000]; // FIXME: hardcoded, in our tests we have less than 100 genes
	  gene_cov.first.first += min(k, pos - gene_cov.second);
          gene_cov.first.second = 1;
          gene_cov.second = pos - 1;
	}

        while (id_kmer.has_next()) {
          auto& gene_cov = classification_id[*(id_kmer.get_next())];
          gene_cov.first.first += min(k, pos - gene_cov.second);
          gene_cov.first.second = 1;
          gene_cov.second = pos - 1;
        }

        for (; pos < (int)read_seq.size(); ++pos) {
          uint8_t new_char = to_int[read_seq[pos]];
          if(new_char == 0) { // Found a char different from A, C, G, T
            ++pos; // we skip this character then we build a new kmer
            kmer = build_kmer(read_seq, pos, k);
            if(kmer == (uint64_t)-1) break;
            rckmer = revcompl(kmer, k);
            --pos; // p must point to the ending position of the kmer, it will be incremented by the for
          } else {
            --new_char; // A is 1 but it should be 0
            kmer = lsappend(kmer, new_char, k);
            rckmer = rsprepend(rckmer, reverse_char(new_char), k);
          }
          id_kmer = bf->get_index(min(kmer, rckmer));
          // cerr << "POS: " << pos << endl;

	  if(id_kmer.empty() && bf->test_dummy_kmer(min(kmer, rckmer))) {
	    auto& gene_cov = classification_id[100000]; // FIXME: hardcoded, in our tests we have less than 100 genes
	    gene_cov.first.first += min(k, pos - gene_cov.second);
	    gene_cov.first.second = 1;
	    gene_cov.second = pos - 1;
	  }

          while (id_kmer.has_next()) {
            const auto& gene_id = *(id_kmer.get_next());
            auto& gene_cov = classification_id[gene_id];
            gene_cov.first.first += min(k, pos - gene_cov.second);
            gene_cov.first.second += 1;
            // cerr << "gid=" << gene_id << "\tPREV_POS=" << gene_cov.second << "\tNEW_COV=" << gene_cov.first.first << "," << gene_cov.first.second << endl;
            gene_cov.second = pos;
          }
        }
      }

      unsigned int max = 0;
      unsigned int maxk = 0;
      genes_idx.clear();
      for(auto it=classification_id.cbegin(); it!=classification_id.cend(); ++it) {
        if(it->second.first.first == max && it->second.first.second == maxk) {
          genes_idx.push_back(it->first);
        } else if(it->second.first.first > max || (it->second.first.first == max && it->second.first.second > maxk)) {
          genes_idx.clear();
          max = it->second.first.first;
          maxk = it->second.first.second;
          genes_idx.push_back(it->first);
        }
      }

      if(max >= c*read_seq.size()) {
        for(const auto idx : genes_idx) {
	  string gene_idx;
	  if(idx == 100000) // FIXME: hardcoded, in our tests we have less than 100 genes
	    gene_idx = "DUMMY";
	  else
	    gene_idx = legend_ID[idx];
          associations->push_back({ read_name, gene_idx, read_seq, to_string(max) });
        }
      }
    }
    delete reads;
    if(associations->size())
      return associations;
    else
      return NULL;
  }
};

#endif
