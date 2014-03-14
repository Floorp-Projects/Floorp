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

#ifndef I18N_ENCODINGS_CLD2_INTERNAL_TOTE_H_
#define I18N_ENCODINGS_CLD2_INTERNAL_TOTE_H_

#include <stdio.h>
#include "integral_types.h"        // for uint8 etc

namespace CLD2 {


// Take a set of <key, score> pairs and tote them up.
// Key is an 8-bit per-script language
// After explicitly sorting, retrieve top key, score pairs
// Normal use is key=per-script language
// The main data structure is an array of 256 uint16 counts. We normally
// expect this to be initialized, added-to about 60 times, then the top three
// items found. The reduce the initial and final time, we also keep a bit vector
// of unused (and uninitialized) parts, each of 64 bits covering four keys.
class Tote {
 public:
  Tote();
  ~Tote();
  void Reinit();
  void AddScoreCount();
  void Add(uint8 ikey, int idelta);
  void AddBytes(int ibytes) {byte_count_ += ibytes;}
  void CurrentTopThreeKeys(int* key3) const;
  int GetScoreCount() const {return score_count_;}
  int GetByteCount() const {return byte_count_;}
  int GetScore(int i) const {return score_[i];}
  void SetScoreCount(uint16 v) {score_count_ = v;}
  void SetScore(int i, int v) {score_[i] = v;}

 private:
  uint64 in_use_mask_;      // 64 bits, one for each group of 4 scores.
                            //    0 = not initialized,not used
  int byte_count_;          // Bytes of text scored
  int score_count_;         // Number of quadgrams/etc. scored
  union {
    uint64 gscore_[64];     // For alignment and clearing quickly
    uint16 score_[256];     // Probability score sum
  };

};


// Take a set of <key, score, reliability> triples and tote them up.
// Key is a 16-bit full language
// After explicitly sorting, retrieve top key, score, reliability triples
class DocTote {
 public:
  DocTote();
  ~DocTote();
  void Reinit();
  void Add(uint16 ikey, int ibytes, int score, int ireliability);
  int Find(uint16 ikey);
  void AddClosePair(int subscr, int val) {closepair_[subscr] += val;}
  int CurrentTopKey();
  Tote* RunningScore() {return &runningscore_;}
  void Sort(int n);
  void Dump(FILE* f);

  int GetIncrCount() const {return incr_count_;}
  int GetClosePair(int subscr) const {return closepair_[subscr];}
  int MaxSize() const {return kMaxSize_;}
  uint16 Key(int i) const {return key_[i];}
  int Value(int i) const {return value_[i];}      // byte count
  int Score(int i) const {return score_[i];}      // sum lg prob
  int Reliability(int i) const {return reliability_[i];}
  void SetKey(int i, int v) {key_[i] = v;}
  void SetValue(int i, int v) {value_[i] = v;}
  void SetScore(int i, int v) {score_[i] = v;}
  void SetReliability(int i, int v) {reliability_[i] = v;}

  static const uint16 kUnusedKey = 0xFFFF;

 private:
  static const int kMaxSize_ = 24;
  static const int kMaxClosePairSize_ = 8;

  int incr_count_;         // Number of Add calls
  int sorted_;             // Contents have been sorted, cannot Add
  Tote runningscore_;      // Top lang scores across entire doc, for
                           // helping resolve close pairs
  // Align at multiple of 8 bytes
  int closepair_[kMaxClosePairSize_];
  uint16 key_[kMaxSize_];   // Lang unassigned = 0xFFFF, valid = 1..1023
  int value_[kMaxSize_];    // Bytecount this lang
  int score_[kMaxSize_];    // Probability score sum
  int reliability_[kMaxSize_];  // Percentage 0..100
};

}       // End namespace CLD2

#endif  // I18N_ENCODINGS_CLD2_INTERNAL_TOTE_H_
