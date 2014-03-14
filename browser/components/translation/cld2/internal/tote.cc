// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//
// Author: dsites@google.com (Dick Sites)
//

#include "tote.h"
#include "lang_script.h"    // For LanguageCode in Dump

#include <stdio.h>
#include <string.h>         // For memset

namespace CLD2 {

// Take a set of <key, value> pairs and tote them up.
// After explicitly sorting, retrieve top key, value pairs
// Normal use is key=per-script language and value = probability score
Tote::Tote() {
  in_use_mask_ = 0;
  byte_count_ = 0;
  score_count_ = 0;
  // No need to initialize values
}

Tote::~Tote() {
}

void Tote::Reinit() {
  in_use_mask_ = 0;
  byte_count_ = 0;
  score_count_ = 0;
  // No need to initialize values
}
// Increment count of quadgrams/trigrams/unigrams scored
void Tote::AddScoreCount() {
  ++score_count_;
}


void Tote::Add(uint8 ikey, int idelta) {
  int key_group = ikey >> 2;
  uint64 groupmask = (1ULL << key_group);
  if ((in_use_mask_ & groupmask) == 0) {
    // Initialize this group
    gscore_[key_group] = 0;
    in_use_mask_ |= groupmask;
  }
  score_[ikey] += idelta;
}


// Return current top three keys
void Tote::CurrentTopThreeKeys(int* key3) const {
  key3[0] = -1;
  key3[1] = -1;
  key3[2] = -1;
  int score3[3] = {-1, -1, -1};
  uint64 tempmask = in_use_mask_;
  int base = 0;
  while (tempmask != 0) {
    if (tempmask & 1) {
      // Look at four in-use keys
      for (int i = 0; i < 4; ++i) {
        int insert_me = score_[base + i];
        // Favor lower numbers on ties
        if (insert_me > score3[2]) {
          // Insert
          int insert_at = 2;
          if (insert_me > score3[1]) {
            score3[2] = score3[1];
            key3[2] = key3[1];
            insert_at = 1;
            if (insert_me > score3[0]) {
              score3[1] = score3[0];
              key3[1] = key3[0];
              insert_at = 0;
            }
          }
          score3[insert_at] = insert_me;
          key3[insert_at] = base + i;
        }
      }
    }
    tempmask >>= 1;
    base += 4;
  }
}


// Take a set of <key, value> pairs and tote them up.
// After explicitly sorting, retrieve top key, value pairs
// 0xFFFF in key signifies unused
DocTote::DocTote() {
  // No need to initialize score_ or value_
  incr_count_ = 0;
  sorted_ = 0;
  memset(closepair_, 0, sizeof(closepair_));
  memset(key_, 0xFF, sizeof(key_));
}

DocTote::~DocTote() {
}

void DocTote::Reinit() {
  // No need to initialize score_ or value_
  incr_count_ = 0;
  sorted_ = 0;
  memset(closepair_, 0, sizeof(closepair_));
  memset(key_, 0xFF, sizeof(key_));
  runningscore_.Reinit();
}

// Weight reliability by ibytes
// Also see three-way associative comments above for Tote
void DocTote::Add(uint16 ikey, int ibytes,
                              int score, int ireliability) {
  ++incr_count_;

  // Look for existing entry in top 2 positions of 3, times 8 columns
  int sub0 = ikey & 15;
  if (key_[sub0] == ikey) {
    value_[sub0] += ibytes;
    score_[sub0] += score;
    reliability_[sub0] += ireliability * ibytes;
    return;
  }
  // Look for existing entry in other of top 2 positions of 3, times 8 columns
  int sub1 = sub0 ^ 8;
  if (key_[sub1] == ikey) {
    value_[sub1] += ibytes;
    score_[sub1] += score;
    reliability_[sub1] += ireliability * ibytes;
    return;
  }
  // Look for existing entry in third position of 3, times 8 columns
  int sub2 = (ikey & 7) + 16;
  if (key_[sub2] == ikey) {
    value_[sub2] += ibytes;
    score_[sub2] += score;
    reliability_[sub2] += ireliability * ibytes;
    return;
  }

  // Allocate new entry
  int alloc = -1;
  if (key_[sub0] == kUnusedKey) {
    alloc = sub0;
  } else if (key_[sub1] == kUnusedKey) {
    alloc = sub1;
  } else if (key_[sub2] == kUnusedKey) {
    alloc = sub2;
  } else {
    // All choices allocated, need to replace smallest one
    alloc = sub0;
    if (value_[sub1] < value_[alloc]) {alloc = sub1;}
    if (value_[sub2] < value_[alloc]) {alloc = sub2;}
  }
  key_[alloc] = ikey;
  value_[alloc] = ibytes;
  score_[alloc] = score;
  reliability_[alloc] = ireliability * ibytes;
  return;
}

// Find subscript of a given packed language, or -1
int DocTote::Find(uint16 ikey) {
  if (sorted_) {
    // Linear search if sorted
    for (int sub = 0; sub < kMaxSize_; ++sub) {
      if (key_[sub] == ikey) {return sub;}
    }
    return -1;
  }

  // Look for existing entry
  int sub0 = ikey & 15;
  if (key_[sub0] == ikey) {
    return sub0;
  }
  int sub1 = sub0 ^ 8;
  if (key_[sub1] == ikey) {
    return sub1;
  }
  int sub2 = (ikey & 7) + 16;
  if (key_[sub2] == ikey) {
    return sub2;
  }

  return -1;
}

// Return current top key
int DocTote::CurrentTopKey() {
  int top_key = 0;
  int top_value = -1;
  for (int sub = 0; sub < kMaxSize_; ++sub) {
    if (key_[sub] == kUnusedKey) {continue;}
    if (top_value < value_[sub]) {
      top_value = value_[sub];
      top_key = key_[sub];
    }
  }
  return top_key;
}


// Sort first n entries by decreasing order of value
// If key==0 other fields are not valid, treat value as -1
void DocTote::Sort(int n) {
  // This is n**2, but n is small
  for (int sub = 0; sub < n; ++sub) {
    if (key_[sub] == kUnusedKey) {value_[sub] = -1;}

    // Bubble sort key[sub] and entry[sub]
    for (int sub2 = sub + 1; sub2 < kMaxSize_; ++sub2) {
      if (key_[sub2] == kUnusedKey) {value_[sub2] = -1;}
      if (value_[sub] < value_[sub2]) {
        // swap
        uint16 tmpk = key_[sub];
        key_[sub] = key_[sub2];
        key_[sub2] = tmpk;

        int tmpv = value_[sub];
        value_[sub] = value_[sub2];
        value_[sub2] = tmpv;

        double tmps = score_[sub];
        score_[sub] = score_[sub2];
        score_[sub2] = tmps;

        int tmpr = reliability_[sub];
        reliability_[sub] = reliability_[sub2];
        reliability_[sub2] = tmpr;
      }
    }
  }
  sorted_ = 1;
}

void DocTote::Dump(FILE* f) {
  fprintf(f, "DocTote::Dump\n");
  for (int sub = 0; sub < kMaxSize_; ++sub) {
    if (key_[sub] != kUnusedKey) {
      Language lang = static_cast<Language>(key_[sub]);
      fprintf(f, "[%2d] %3s %6dB %5dp %4dR,\n", sub, LanguageCode(lang),
              value_[sub], score_[sub], reliability_[sub]);
    }
  }
  fprintf(f, "  %d chunks scored<br>\n", incr_count_);
}

}       // End namespace CLD2

