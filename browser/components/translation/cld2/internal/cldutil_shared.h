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
// Just the stuff shared between offline table builder and online detector
//

#ifndef I18N_ENCODINGS_CLD2_INTERNAL_NEW_CLDUTIL_SHARED_H__
#define I18N_ENCODINGS_CLD2_INTERNAL_NEW_CLDUTIL_SHARED_H__

#include "integral_types.h"
#include "cld2tablesummary.h"

namespace CLD2 {

// Runtime routines for hashing, looking up, and scoring
// unigrams (CJK), bigrams (CJK), quadgrams, and octagrams.
// Unigrams and bigrams are for CJK languages only, including simplified/
// traditional Chinese, Japanese, Korean, Vietnamese Han characters, and
// Zhuang Han characters. Surrounding spaces are not considered.
// Quadgrams and octagrams for for non-CJK and include two bits indicating
// preceding and trailing spaces (word boundaries).


//----------------------------------------------------------------------------//
// Main quantized probability table                                           //
//----------------------------------------------------------------------------//

  // Table has 240 eight-byte entries. Each entry has a five-byte array and
  // a three-byte array of log base 2 probabilities in the range 1..12.
  // The intended use is to express five or three probabilities in a single-byte
  // subscript, then decode via this table. These probabilities are
  // intended to go with an array of five or three language numbers.
  //
  // The corresponding language numbers will have to be sorted by descending
  // probability, then the actual probability subscript chosen to match the
  // closest available entry in this table.
  //
  // Pattern of probability values:
  // hi 3/4 1/2 1/4 lo    hi mid lo
  // where "3/4" is (hi*3+lo)/4, "1/2" is (hi+lo)/2, and "1/4" is (hi+lo*3)/4
  // and mid is one of 3/4 1/2 or 1/4.
  // There are three groups of 78 (=12*13/2) entries, with hi running 1..12 and
  // lo running 1..hi. Only the first group is used for five-entry lookups.
  // The mid value in the first group is 1/2, the second group 3/4, and the
  // third group 1/4. For three-entry lookups, this allows the mid entry to be
  // somewhat higher or lower than the midpoint, to allow a better match to the
  // original probabilities.
  static const int kLgProbV2TblSize = 240;
  static const uint8 kLgProbV2Tbl[kLgProbV2TblSize * 8] = {
    1,1,1,1,1, 1,1,1,     // [0]
    2,2,2,1,1, 2,2,1,     // [1]
    2,2,2,2,2, 2,2,2,
    3,3,2,2,1, 3,2,1,     // [3]
    3,3,3,2,2, 3,3,2,
    3,3,3,3,3, 3,3,3,
    4,3,3,2,1, 4,3,1,     // [6]
    4,4,3,3,2, 4,3,2,
    4,4,4,3,3, 4,4,3,
    4,4,4,4,4, 4,4,4,
    5,4,3,2,1, 5,3,1,     // [10]
    5,4,4,3,2, 5,4,2,
    5,5,4,4,3, 5,4,3,
    5,5,5,4,4, 5,5,4,
    5,5,5,5,5, 5,5,5,
    6,5,4,2,1, 6,4,1,     // [15]
    6,5,4,3,2, 6,4,2,
    6,5,5,4,3, 6,5,3,
    6,6,5,5,4, 6,5,4,
    6,6,6,5,5, 6,6,5,
    6,6,6,6,6, 6,6,6,
    7,6,4,3,1, 7,4,1,     // [21]
    7,6,5,3,2, 7,5,2,
    7,6,5,4,3, 7,5,3,
    7,6,6,5,4, 7,6,4,
    7,7,6,6,5, 7,6,5,
    7,7,7,6,6, 7,7,6,
    7,7,7,7,7, 7,7,7,
    8,6,5,3,1, 8,5,1,     // [28]
    8,7,5,4,2, 8,5,2,
    8,7,6,4,3, 8,6,3,
    8,7,6,5,4, 8,6,4,
    8,7,7,6,5, 8,7,5,
    8,8,7,7,6, 8,7,6,
    8,8,8,7,7, 8,8,7,
    8,8,8,8,8, 8,8,8,
    9,7,5,3,1, 9,5,1,     // [36]
    9,7,6,4,2, 9,6,2,
    9,8,6,5,3, 9,6,3,
    9,8,7,5,4, 9,7,4,
    9,8,7,6,5, 9,7,5,
    9,8,8,7,6, 9,8,6,
    9,9,8,8,7, 9,8,7,
    9,9,9,8,8, 9,9,8,
    9,9,9,9,9, 9,9,9,
    10,8,6,3,1, 10,6,1,   // [45]
    10,8,6,4,2, 10,6,2,
    10,8,7,5,3, 10,7,3,
    10,9,7,6,4, 10,7,4,
    10,9,8,6,5, 10,8,5,
    10,9,8,7,6, 10,8,6,
    10,9,9,8,7, 10,9,7,
    10,10,9,9,8, 10,9,8,
    10,10,10,9,9, 10,10,9,
    10,10,10,10,10, 10,10,10,
    11,9,6,4,1, 11,6,1,   // [55]
    11,9,7,4,2, 11,7,2,
    11,9,7,5,3, 11,7,3,
    11,9,8,6,4, 11,8,4,
    11,10,8,7,5, 11,8,5,
    11,10,9,7,6, 11,9,6,
    11,10,9,8,7, 11,9,7,
    11,10,10,9,8, 11,10,8,
    11,11,10,10,9, 11,10,9,
    11,11,11,10,10, 11,11,10,
    11,11,11,11,11, 11,11,11,
    12,9,7,4,1, 12,7,1,   // [66]
    12,10,7,5,2, 12,7,2,
    12,10,8,5,3, 12,8,3,
    12,10,8,6,4, 12,8,4,
    12,10,9,7,5, 12,9,5,
    12,11,9,8,6, 12,9,6,
    12,11,10,8,7, 12,10,7,
    12,11,10,9,8, 12,10,8,
    12,11,11,10,9, 12,11,9,
    12,12,11,11,10, 12,11,10,
    12,12,12,11,11, 12,12,11,
    12,12,12,12,12, 12,12,12,

    1,1,1,1,1, 1,1,1,
    2,2,2,1,1, 2,2,1,
    2,2,2,2,2, 2,2,2,
    3,3,2,2,1, 3,3,1,
    3,3,3,2,2, 3,3,2,
    3,3,3,3,3, 3,3,3,
    4,3,3,2,1, 4,3,1,
    4,4,3,3,2, 4,4,2,
    4,4,4,3,3, 4,4,3,
    4,4,4,4,4, 4,4,4,
    5,4,3,2,1, 5,4,1,
    5,4,4,3,2, 5,4,2,
    5,5,4,4,3, 5,5,3,
    5,5,5,4,4, 5,5,4,
    5,5,5,5,5, 5,5,5,
    6,5,4,2,1, 6,5,1,
    6,5,4,3,2, 6,5,2,
    6,5,5,4,3, 6,5,3,
    6,6,5,5,4, 6,6,4,
    6,6,6,5,5, 6,6,5,
    6,6,6,6,6, 6,6,6,
    7,6,4,3,1, 7,6,1,
    7,6,5,3,2, 7,6,2,
    7,6,5,4,3, 7,6,3,
    7,6,6,5,4, 7,6,4,
    7,7,6,6,5, 7,7,5,
    7,7,7,6,6, 7,7,6,
    7,7,7,7,7, 7,7,7,
    8,6,5,3,1, 8,6,1,
    8,7,5,4,2, 8,7,2,
    8,7,6,4,3, 8,7,3,
    8,7,6,5,4, 8,7,4,
    8,7,7,6,5, 8,7,5,
    8,8,7,7,6, 8,8,6,
    8,8,8,7,7, 8,8,7,
    8,8,8,8,8, 8,8,8,
    9,7,5,3,1, 9,7,1,
    9,7,6,4,2, 9,7,2,
    9,8,6,5,3, 9,8,3,
    9,8,7,5,4, 9,8,4,
    9,8,7,6,5, 9,8,5,
    9,8,8,7,6, 9,8,6,
    9,9,8,8,7, 9,9,7,
    9,9,9,8,8, 9,9,8,
    9,9,9,9,9, 9,9,9,
    10,8,6,3,1, 10,8,1,
    10,8,6,4,2, 10,8,2,
    10,8,7,5,3, 10,8,3,
    10,9,7,6,4, 10,9,4,
    10,9,8,6,5, 10,9,5,
    10,9,8,7,6, 10,9,6,
    10,9,9,8,7, 10,9,7,
    10,10,9,9,8, 10,10,8,
    10,10,10,9,9, 10,10,9,
    10,10,10,10,10, 10,10,10,
    11,9,6,4,1, 11,9,1,
    11,9,7,4,2, 11,9,2,
    11,9,7,5,3, 11,9,3,
    11,9,8,6,4, 11,9,4,
    11,10,8,7,5, 11,10,5,
    11,10,9,7,6, 11,10,6,
    11,10,9,8,7, 11,10,7,
    11,10,10,9,8, 11,10,8,
    11,11,10,10,9, 11,11,9,
    11,11,11,10,10, 11,11,10,
    11,11,11,11,11, 11,11,11,
    12,9,7,4,1, 12,9,1,
    12,10,7,5,2, 12,10,2,
    12,10,8,5,3, 12,10,3,
    12,10,8,6,4, 12,10,4,
    12,10,9,7,5, 12,10,5,
    12,11,9,8,6, 12,11,6,
    12,11,10,8,7, 12,11,7,
    12,11,10,9,8, 12,11,8,
    12,11,11,10,9, 12,11,9,
    12,12,11,11,10, 12,12,10,
    12,12,12,11,11, 12,12,11,
    12,12,12,12,12, 12,12,12,

    1,1,1,1,1, 1,1,1,
    2,2,2,1,1, 2,1,1,
    2,2,2,2,2, 2,2,2,
    3,3,2,2,1, 3,2,1,
    3,3,3,2,2, 3,2,2,
    3,3,3,3,3, 3,3,3,
    4,3,3,2,1, 4,2,1,
    4,4,3,3,2, 4,3,2,
    4,4,4,3,3, 4,3,3,
    4,4,4,4,4, 4,4,4,
    5,4,3,2,1, 5,2,1,
    5,4,4,3,2, 5,3,2,
    5,5,4,4,3, 5,4,3,
    5,5,5,4,4, 5,4,4,
    5,5,5,5,5, 5,5,5,
    6,5,4,2,1, 6,2,1,
    6,5,4,3,2, 6,3,2,
    6,5,5,4,3, 6,4,3,
    6,6,5,5,4, 6,5,4,
    6,6,6,5,5, 6,5,5,
    6,6,6,6,6, 6,6,6,
    7,6,4,3,1, 7,3,1,
    7,6,5,3,2, 7,3,2,
    7,6,5,4,3, 7,4,3,
    7,6,6,5,4, 7,5,4,
    7,7,6,6,5, 7,6,5,
    7,7,7,6,6, 7,6,6,
    7,7,7,7,7, 7,7,7,
    8,6,5,3,1, 8,3,1,
    8,7,5,4,2, 8,4,2,
    8,7,6,4,3, 8,4,3,
    8,7,6,5,4, 8,5,4,
    8,7,7,6,5, 8,6,5,
    8,8,7,7,6, 8,7,6,
    8,8,8,7,7, 8,7,7,
    8,8,8,8,8, 8,8,8,
    9,7,5,3,1, 9,3,1,
    9,7,6,4,2, 9,4,2,
    9,8,6,5,3, 9,5,3,
    9,8,7,5,4, 9,5,4,
    9,8,7,6,5, 9,6,5,
    9,8,8,7,6, 9,7,6,
    9,9,8,8,7, 9,8,7,
    9,9,9,8,8, 9,8,8,
    9,9,9,9,9, 9,9,9,
    10,8,6,3,1, 10,3,1,
    10,8,6,4,2, 10,4,2,
    10,8,7,5,3, 10,5,3,
    10,9,7,6,4, 10,6,4,
    10,9,8,6,5, 10,6,5,
    10,9,8,7,6, 10,7,6,
    10,9,9,8,7, 10,8,7,
    10,10,9,9,8, 10,9,8,
    10,10,10,9,9, 10,9,9,
    10,10,10,10,10, 10,10,10,
    11,9,6,4,1, 11,4,1,
    11,9,7,4,2, 11,4,2,
    11,9,7,5,3, 11,5,3,
    11,9,8,6,4, 11,6,4,
    11,10,8,7,5, 11,7,5,
    11,10,9,7,6, 11,7,6,
    11,10,9,8,7, 11,8,7,
    11,10,10,9,8, 11,9,8,
    11,11,10,10,9, 11,10,9,
    11,11,11,10,10, 11,10,10,
    11,11,11,11,11, 11,11,11,
    12,9,7,4,1, 12,4,1,
    12,10,7,5,2, 12,5,2,
    12,10,8,5,3, 12,5,3,
    12,10,8,6,4, 12,6,4,
    12,10,9,7,5, 12,7,5,
    12,11,9,8,6, 12,8,6,
    12,11,10,8,7, 12,8,7,
    12,11,10,9,8, 12,9,8,
    12,11,11,10,9, 12,10,9,
    12,12,11,11,10, 12,11,10,
    12,12,12,11,11, 12,11,11,
    12,12,12,12,12, 12,12,12,

    // Added 2013.01.28 for CJK compatible mapping
    8,5,2,2,2, 8,2,2,
    6,6,6,4,2, 6,6,2,
    6,5,4,4,4, 6,4,4,
    6,4,2,2,2, 6,2,2,
    4,3,2,2,2, 4,2,2,
    2,2,2,2,2, 2,2,2,
  };

  // Backmap a single desired probability into an entry in kLgProbV2Tbl
  static const uint8 kLgProbV2TblBackmap[13] = {
    0,
    0, 1, 3, 6,   10, 15, 21, 28,   36, 45, 55, 66,
  };

  // Return address of 8-byte entry[i]
  inline const uint8* LgProb2TblEntry(int i) {
    return &kLgProbV2Tbl[i * 8];
  }

  // Return one of three probabilities in an entry
  inline uint8 LgProb3(const uint8* entry, int j) {
    return entry[j + 5];
  }


// Routines to access a hash table of <key:wordhash, value:probs> pairs
// Buckets have 4-byte wordhash for sizes < 32K buckets, but only
// 2-byte wordhash for sizes >= 32K buckets, with other wordhash bits used as
// bucket subscript.
// Probs is a packed: three languages plus a subscript for probability table
// Buckets have all the keys together, then all the values.Key array never
// crosses a cache-line boundary, so no-match case takes exactly one cache miss.
// Match case may sometimes take an additional cache miss on value access.
//
// Other possibilites include 5 or 10 6-byte entries plus pad to make 32 or 64
// byte buckets with single cache miss.
// Or 2-byte key and 6-byte value, allowing 5 languages instead  of three.


//----------------------------------------------------------------------------//
// Hashing groups of 1/2/4/8 letters, perhaps with spaces or underscores      //
//----------------------------------------------------------------------------//

// BIGRAM
// Pick up 1..8 bytes and hash them via mask/shift/add. NO pre/post
// OVERSHOOTS up to 3 bytes
// For runtime use of tables
// Does X86 unaligned loads if !defined(NEED_ALIGNED_LOADS)UNALIGNED_LOAD32(_p)
uint32 BiHashV2(const char* word_ptr, int bytecount);

// QUADGRAM wrapper with surrounding spaces
// Pick up 1..12 bytes plus pre/post space and hash them via mask/shift/add
// UNDERSHOOTS 1 byte, OVERSHOOTS up to 3 bytes
// For runtime use of tables
uint32 QuadHashV2(const char* word_ptr, int bytecount);

// QUADGRAM wrapper with surrounding underscores (offline use)
// Pick up 1..12 bytes plus pre/post '_' and hash them via mask/shift/add
// OVERSHOOTS up to 3 bytes
// For offline construction of tables
uint32 QuadHashV2Underscore(const char* word_ptr, int bytecount);

// OCTAGRAM wrapper with surrounding spaces
// Pick up 1..24 bytes plus pre/post space and hash them via mask/shift/add
// UNDERSHOOTS 1 byte, OVERSHOOTS up to 3 bytes
uint64 OctaHash40(const char* word_ptr, int bytecount);


// OCTAGRAM wrapper with surrounding underscores (offline use)
// Pick up 1..24 bytes plus pre/post space and hash them via mask/shift/add
// UNDERSHOOTS 1 byte, OVERSHOOTS up to 3 bytes
uint64 OctaHash40underscore(const char* word_ptr, int bytecount);

// Hash a consecutive pair of tokens/words A B
uint64 PairHash(uint64 worda_hash, uint64 wordb_hash);


// From 32-bit gram FP, return hash table subscript and remaining key
inline void QuadFPJustHash(uint32 quadhash,
                                uint32 keymask,
                                int bucketcount,
                                uint32* subscr, uint32* hashkey) {
  *subscr = (quadhash + (quadhash >> 12)) & (bucketcount - 1);
  *hashkey = quadhash & keymask;
}

// From 40-bit gram FP, return hash table subscript and remaining key
inline void OctaFPJustHash(uint64 longwordhash,
                                  uint32 keymask,
                                  int bucketcount,
                                  uint32* subscr, uint32* hashkey) {
  uint32 temp = (longwordhash + (longwordhash >> 12)) & (bucketcount - 1);
  *subscr = temp;
  temp = longwordhash >> 4;
  *hashkey = temp & keymask;
}


// Look up 32-bit gram FP in caller-passed table
// Typical size 256K entries (1.5MB)
// Two-byte hashkey
inline const uint32 QuadHashV3Lookup4(const CLD2TableSummary* gram_obj,
                                      uint32 quadhash) {
  uint32 subscr, hashkey;
  const IndirectProbBucket4* quadtable = gram_obj->kCLDTable;
  uint32 keymask = gram_obj->kCLDTableKeyMask;
  int bucketcount = gram_obj->kCLDTableSize;
  QuadFPJustHash(quadhash, keymask, bucketcount, &subscr, &hashkey);
  const IndirectProbBucket4* bucket_ptr = &quadtable[subscr];
  // Four-way associative, 4 compares
  if (((hashkey ^ bucket_ptr->keyvalue[0]) & keymask) == 0) {
    return bucket_ptr->keyvalue[0];
  }
  if (((hashkey ^ bucket_ptr->keyvalue[1]) & keymask) == 0) {
    return bucket_ptr->keyvalue[1];
  }
  if (((hashkey ^ bucket_ptr->keyvalue[2]) & keymask) == 0) {
    return bucket_ptr->keyvalue[2];
  }
  if (((hashkey ^ bucket_ptr->keyvalue[3]) & keymask) == 0) {
    return bucket_ptr->keyvalue[3];
  }
  return 0;
}

// Look up 40-bit gram FP in caller-passed table
// Typical size 256K-4M entries (1-16MB)
// 24-12 bit hashkey packed with 8-20 bit indirect lang/probs
// keymask is 0xfffff000 for 20-bit hashkey and 12-bit indirect
inline const uint32 OctaHashV3Lookup4(const CLD2TableSummary* gram_obj,
                                          uint64 longwordhash) {
  uint32 subscr, hashkey;
  const IndirectProbBucket4* octatable = gram_obj->kCLDTable;
  uint32 keymask = gram_obj->kCLDTableKeyMask;
  int bucketcount = gram_obj->kCLDTableSize;
  OctaFPJustHash(longwordhash, keymask, bucketcount,
                        &subscr, &hashkey);
  const IndirectProbBucket4* bucket_ptr = &octatable[subscr];
  // Four-way associative, 4 compares
  if (((hashkey ^ bucket_ptr->keyvalue[0]) & keymask) == 0) {
    return bucket_ptr->keyvalue[0];
  }
  if (((hashkey ^ bucket_ptr->keyvalue[1]) & keymask) == 0) {
    return bucket_ptr->keyvalue[1];
  }
  if (((hashkey ^ bucket_ptr->keyvalue[2]) & keymask) == 0) {
    return bucket_ptr->keyvalue[2];
  }
  if (((hashkey ^ bucket_ptr->keyvalue[3]) & keymask) == 0) {
    return bucket_ptr->keyvalue[3];
  }
  return 0;
}


//----------------------------------------------------------------------------//
// Finding groups of 1/2/4/8 letters                                          //
//----------------------------------------------------------------------------//

// Does not advance past space or tab/cr/lf/nul
static const uint8 kAdvanceOneCharButSpace[256] = {
 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
 0,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,

 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,
 3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,
};


// Advances *only* on space or ASCII vowel (or illegal byte)
static const uint8 kAdvanceOneCharSpaceVowel[256] = {
 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
 1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
 0,1,0,0,0,1,0,0, 0,1,0,0,0,0,0,1, 0,0,0,0,0,1,0,0, 0,0,0,0,0,0,0,0,
 0,1,0,0,0,1,0,0, 0,1,0,0,0,0,0,1, 0,0,0,0,0,1,0,0, 0,0,0,0,0,0,0,0,

 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
};


// src points to a letter. Find the byte length of a unigram starting there.
int UniLen(const char* src);

// src points to a letter. Find the byte length of a bigram starting there.
int BiLen(const char* src);

// src points to a letter. Find the byte length of a quadgram starting there.
int QuadLen(const char* src);

// src points to a letter. Find the byte length of an octagram starting there.
int OctaLen(const char* src);

}       // End namespace CLD2

#endif  // I18N_ENCODINGS_CLD2_INTERNAL_NEW_CLDUTIL_SHARED_H__






