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

#include "cldutil_shared.h"
#include <string>

#include "cld2tablesummary.h"
#include "integral_types.h"
#include "port.h"
#include "utf8statetable.h"

namespace CLD2 {

// Runtime routines for hashing, looking up, and scoring
// unigrams (CJK), bigrams (CJK), quadgrams, and octagrams.
// Unigrams and bigrams are for CJK languages only, including simplified/
// traditional Chinese, Japanese, Korean, Vietnamese Han characters, and
// Zhuang Han characters. Surrounding spaces are not considered.
// Quadgrams and octagrams for for non-CJK and include two bits indicating
// preceding and trailing spaces (word boundaries).


// Indicator bits for leading/trailing space around quad/octagram
// NOTE: 4444 bits are chosen to flip constant bits in hash of four chars of
// 1-, 2-, or 3-bytes each.
static const uint32 kPreSpaceIndicator =  0x00004444;
static const uint32 kPostSpaceIndicator = 0x44440000;

// Little-endian masks for 0..24 bytes picked up as uint32's
static const uint32 kWordMask0[4] = {
  0xFFFFFFFF, 0x000000FF, 0x0000FFFF, 0x00FFFFFF
};

static const int kMinCJKUTF8CharBytes = 3;

static const int kMinGramCount = 3;
static const int kMaxGramCount = 16;

static const int UTFmax = 4;        // Max number of bytes in a UTF-8 character


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

// Design principles for these hash functions
// - Few operations
// - Handle 1-, 2-, and 3-byte UTF-8 scripts, ignoring intermixing except in
//   Latin script expect 1- and 2-byte mixtures.
// - Last byte of each character has about 5 bits of information
// - Spread good bits around so they can interact in at least two ways
//   with other characters
// - Use add for additional mixing thorugh carries

// CJK Three-byte bigram
//   ....dddd..cccccc..bbbbbb....aaaa
//   ..................ffffff..eeeeee
// make
//   ....dddd..cccccc..bbbbbb....aaaa
//   000....dddd..cccccc..bbbbbb....a
//   ..................ffffff..eeeeee
//   ffffff..eeeeee000000000000000000
//
// CJK Four-byte bigram
//   ..dddddd..cccccc....bbbb....aaaa
//   ..hhhhhh..gggggg....ffff....eeee
// make
//   ..dddddd..cccccc....bbbb....aaaa
//   000..dddddd..cccccc....bbbb....a
//   ..hhhhhh..gggggg....ffff....eeee
//   ..ffff....eeee000000000000000000

// BIGRAM
// Pick up 1..8 bytes and hash them via mask/shift/add. NO pre/post
// OVERSHOOTS up to 3 bytes
// For runtime use of tables
// Does X86 unaligned loads
uint32 BiHashV2(const char* word_ptr, int bytecount) {
  if (bytecount == 0) {return 0;}
  const uint32* word_ptr32 = reinterpret_cast<const uint32*>(word_ptr);
  uint32 word0, word1;
  if (bytecount <= 4) {
    word0 = UNALIGNED_LOAD32(word_ptr32) & kWordMask0[bytecount & 3];
    word0 = word0 ^ (word0 >> 3);
    return word0;
  }
  // Else do 8 bytes
  word0 = UNALIGNED_LOAD32(word_ptr32);
  word0 = word0 ^ (word0 >> 3);
  word1 = UNALIGNED_LOAD32(word_ptr32 + 1) & kWordMask0[bytecount & 3];
  word1 = word1 ^ (word1 << 18);
  return word0 + word1;
}

//
// Ascii-7 One-byte chars
//   ...ddddd...ccccc...bbbbb...aaaaa
// make
//   ...ddddd...ccccc...bbbbb...aaaaa
//   000...ddddd...ccccc...bbbbb...aa
//
// Latin 1- and 2-byte chars
//   ...ddddd...ccccc...bbbbb...aaaaa
//   ...................fffff...eeeee
// make
//   ...ddddd...ccccc...bbbbb...aaaaa
//   000...ddddd...ccccc...bbbbb...aa
//   ...................fffff...eeeee
//   ...............fffff...eeeee0000
//
// Non-CJK Two-byte chars
//   ...ddddd...........bbbbb........
//   ...hhhhh...........fffff........
// make
//   ...ddddd...........bbbbb........
//   000...ddddd...........bbbbb.....
//   ...hhhhh...........fffff........
//   hhhh...........fffff........0000
//
// Non-CJK Three-byte chars
//   ...........ccccc................
//   ...................fffff........
//   ...lllll...................iiiii
// make
//   ...........ccccc................
//   000...........ccccc.............
//   ...................fffff........
//   ...............fffff........0000
//   ...lllll...................iiiii
//   .lllll...................iiiii00
//

// QUADGRAM
// Pick up 1..12 bytes plus pre/post space and hash them via mask/shift/add
// OVERSHOOTS up to 3 bytes
// For runtime use of tables
// Does X86 unaligned loads
uint32 QuadHashV2Mix(const char* word_ptr, int bytecount, uint32 prepost) {
  const uint32* word_ptr32 = reinterpret_cast<const uint32*>(word_ptr);
  uint32 word0, word1, word2;
  if (bytecount <= 4) {
    word0 = UNALIGNED_LOAD32(word_ptr32) & kWordMask0[bytecount & 3];
    word0 = word0 ^ (word0 >> 3);
    return word0 ^ prepost;
  } else if (bytecount <= 8) {
    word0 = UNALIGNED_LOAD32(word_ptr32);
    word0 = word0 ^ (word0 >> 3);
    word1 = UNALIGNED_LOAD32(word_ptr32 + 1) & kWordMask0[bytecount & 3];
    word1 = word1 ^ (word1 << 4);
    return (word0 ^ prepost) + word1;
  }
  // else do 12 bytes
  word0 = UNALIGNED_LOAD32(word_ptr32);
  word0 = word0 ^ (word0 >> 3);
  word1 = UNALIGNED_LOAD32(word_ptr32 + 1);
  word1 = word1 ^ (word1 << 4);
  word2 = UNALIGNED_LOAD32(word_ptr32 + 2) & kWordMask0[bytecount & 3];
  word2 = word2 ^ (word2 << 2);
  return (word0 ^ prepost) + word1 + word2;
}


// QUADGRAM wrapper with surrounding spaces
// Pick up 1..12 bytes plus pre/post space and hash them via mask/shift/add
// UNDERSHOOTS 1 byte, OVERSHOOTS up to 3 bytes
// For runtime use of tables
uint32 QuadHashV2(const char* word_ptr, int bytecount) {
  if (bytecount == 0) {return 0;}
  uint32 prepost = 0;
  if (word_ptr[-1] == ' ') {prepost |= kPreSpaceIndicator;}
  if (word_ptr[bytecount] == ' ') {prepost |= kPostSpaceIndicator;}
  return QuadHashV2Mix(word_ptr, bytecount, prepost);
}

// QUADGRAM wrapper with surrounding underscores (offline use)
// Pick up 1..12 bytes plus pre/post '_' and hash them via mask/shift/add
// OVERSHOOTS up to 3 bytes
// For offline construction of tables
uint32 QuadHashV2Underscore(const char* word_ptr, int bytecount) {
  if (bytecount == 0) {return 0;}
  const char* local_word_ptr = word_ptr;
  int local_bytecount = bytecount;
  uint32 prepost = 0;
  if (local_word_ptr[0] == '_') {
    prepost |= kPreSpaceIndicator;
    ++local_word_ptr;
    --local_bytecount;
  }
  if (local_word_ptr[local_bytecount - 1] == '_') {
    prepost |= kPostSpaceIndicator;
    --local_bytecount;
  }
  return QuadHashV2Mix(local_word_ptr, local_bytecount, prepost);
}


// OCTAGRAM
// Pick up 1..24 bytes plus pre/post space and hash them via mask/shift/add
// UNDERSHOOTS 1 byte, OVERSHOOTS up to 3 bytes
//
// The low 32 bits follow the pattern from above, tuned to different scripts
// The high 8 bits are a simple sum of all bytes, shifted by 0/1/2/3 bits each
// For runtime use of tables V3
// Does X86 unaligned loads
uint64 OctaHash40Mix(const char* word_ptr, int bytecount, uint64 prepost) {
  const uint32* word_ptr32 = reinterpret_cast<const uint32*>(word_ptr);
  uint64 word0;
  uint64 word1;
  uint64 sum;

  if (word_ptr[-1] == ' ') {prepost |= kPreSpaceIndicator;}
  if (word_ptr[bytecount] == ' ') {prepost |= kPostSpaceIndicator;}
  switch ((bytecount - 1) >> 2) {
  case 0:       // 1..4 bytes
    word0 = UNALIGNED_LOAD32(word_ptr32) & kWordMask0[bytecount & 3];
    sum = word0;
    word0 = word0 ^ (word0 >> 3);
    break;
  case 1:       // 5..8 bytes
    word0 = UNALIGNED_LOAD32(word_ptr32);
    sum = word0;
    word0 = word0 ^ (word0 >> 3);
    word1 = UNALIGNED_LOAD32(word_ptr32 + 1) & kWordMask0[bytecount & 3];
    sum += word1;
    word1 = word1 ^ (word1 << 4);
    word0 += word1;
    break;
  case 2:       // 9..12 bytes
    word0 = UNALIGNED_LOAD32(word_ptr32);
    sum = word0;
    word0 = word0 ^ (word0 >> 3);
    word1 = UNALIGNED_LOAD32(word_ptr32 + 1);
    sum += word1;
    word1 = word1 ^ (word1 << 4);
    word0 += word1;
    word1 = UNALIGNED_LOAD32(word_ptr32 + 2) & kWordMask0[bytecount & 3];
    sum += word1;
    word1 = word1 ^ (word1 << 2);
    word0 += word1;
    break;
  case 3:       // 13..16 bytes
    word0 =UNALIGNED_LOAD32(word_ptr32);
    sum = word0;
    word0 = word0 ^ (word0 >> 3);
    word1 = UNALIGNED_LOAD32(word_ptr32 + 1);
    sum += word1;
    word1 = word1 ^ (word1 << 4);
    word0 += word1;
    word1 = UNALIGNED_LOAD32(word_ptr32 + 2);
    sum += word1;
    word1 = word1 ^ (word1 << 2);
    word0 += word1;
    word1 = UNALIGNED_LOAD32(word_ptr32 + 3) & kWordMask0[bytecount & 3];
    sum += word1;
    word1 = word1 ^ (word1 >> 8);
    word0 += word1;
    break;
  case 4:       // 17..20 bytes
    word0 = UNALIGNED_LOAD32(word_ptr32);
    sum = word0;
    word0 = word0 ^ (word0 >> 3);
    word1 = UNALIGNED_LOAD32(word_ptr32 + 1);
    sum += word1;
    word1 = word1 ^ (word1 << 4);
    word0 += word1;
    word1 = UNALIGNED_LOAD32(word_ptr32 + 2);
    sum += word1;
    word1 = word1 ^ (word1 << 2);
    word0 += word1;
    word1 = UNALIGNED_LOAD32(word_ptr32 + 3);
    sum += word1;
    word1 = word1 ^ (word1 >> 8);
    word0 += word1;
    word1 = UNALIGNED_LOAD32(word_ptr32 + 4) & kWordMask0[bytecount & 3];
    sum += word1;
    word1 = word1 ^ (word1 >> 4);
    word0 += word1;
    break;
  default:      // 21..24 bytes and higher (ignores beyond 24)
    word0 = UNALIGNED_LOAD32(word_ptr32);
    sum = word0;
    word0 = word0 ^ (word0 >> 3);
    word1 = UNALIGNED_LOAD32(word_ptr32 + 1);
    sum += word1;
    word1 = word1 ^ (word1 << 4);
    word0 += word1;
    word1 = UNALIGNED_LOAD32(word_ptr32 + 2);
    sum += word1;
    word1 = word1 ^ (word1 << 2);
    word0 += word1;
    word1 = UNALIGNED_LOAD32(word_ptr32 + 3);
    sum += word1;
    word1 = word1 ^ (word1 >> 8);
    word0 += word1;
    word1 = UNALIGNED_LOAD32(word_ptr32 + 4);
    sum += word1;
    word1 = word1 ^ (word1 >> 4);
    word0 += word1;
    word1 = UNALIGNED_LOAD32(word_ptr32 + 5) & kWordMask0[bytecount & 3];
    sum += word1;
    word1 = word1 ^ (word1 >> 6);
    word0 += word1;
    break;
  }

  sum += (sum >> 17);             // extra 1-bit shift for bytes 2 & 3
  sum += (sum >> 9);              // extra 1-bit shift for bytes 1 & 3
  sum = (sum & 0xff) << 32;
  return (word0 ^ prepost) + sum;
}

// OCTAGRAM wrapper with surrounding spaces
// Pick up 1..24 bytes plus pre/post space and hash them via mask/shift/add
// UNDERSHOOTS 1 byte, OVERSHOOTS up to 3 bytes
//
// The low 32 bits follow the pattern from above, tuned to different scripts
// The high 8 bits are a simple sum of all bytes, shifted by 0/1/2/3 bits each
// For runtime use of tables V3
uint64 OctaHash40(const char* word_ptr, int bytecount) {
  if (bytecount == 0) {return 0;}
  uint64 prepost = 0;
  if (word_ptr[-1] == ' ') {prepost |= kPreSpaceIndicator;}
  if (word_ptr[bytecount] == ' ') {prepost |= kPostSpaceIndicator;}
  return OctaHash40Mix(word_ptr, bytecount, prepost);
}


// OCTAGRAM wrapper with surrounding underscores (offline use)
// Pick up 1..24 bytes plus pre/post space and hash them via mask/shift/add
// UNDERSHOOTS 1 byte, OVERSHOOTS up to 3 bytes
//
// The low 32 bits follow the pattern from above, tuned to different scripts
// The high 8 bits are a simple sum of all bytes, shifted by 0/1/2/3 bits each
// For offline construction of tables
uint64 OctaHash40underscore(const char* word_ptr, int bytecount) {
  if (bytecount == 0) {return 0;}
  const char* local_word_ptr = word_ptr;
  int local_bytecount = bytecount;
  uint64 prepost = 0;
  if (local_word_ptr[0] == '_') {
    prepost |= kPreSpaceIndicator;
    ++local_word_ptr;
    --local_bytecount;
  }
  if (local_word_ptr[local_bytecount - 1] == '_') {
    prepost |= kPostSpaceIndicator;
    --local_bytecount;
  }
  return OctaHash40Mix(local_word_ptr, local_bytecount, prepost);
}

// Hash a consecutive pair of tokens/words A B
// Old: hash is B - A, which gives too many false hits on one-char diffs
// Now: rotate(A,13) + B
uint64 PairHash(uint64 worda_hash, uint64 wordb_hash) {
   return ((worda_hash >> 13) | (worda_hash << (64 - 13))) + wordb_hash;
}




//----------------------------------------------------------------------------//
// Finding groups of 1/2/4/8 letters                                          //
//----------------------------------------------------------------------------//

// src points to a letter. Find the byte length of a unigram starting there.
int UniLen(const char* src) {
  const char* src_end = src;
  src_end += kAdvanceOneCharButSpace[(uint8)src_end[0]];
  return src_end - src;
}

// src points to a letter. Find the byte length of a bigram starting there.
int BiLen(const char* src) {
  const char* src_end = src;
  src_end += kAdvanceOneCharButSpace[(uint8)src_end[0]];
  src_end += kAdvanceOneCharButSpace[(uint8)src_end[0]];
  return src_end - src;
}

// src points to a letter. Find the byte length of a quadgram starting there.
int QuadLen(const char* src) {
  const char* src_end = src;
  src_end += kAdvanceOneCharButSpace[(uint8)src_end[0]];
  src_end += kAdvanceOneCharButSpace[(uint8)src_end[0]];
  src_end += kAdvanceOneCharButSpace[(uint8)src_end[0]];
  src_end += kAdvanceOneCharButSpace[(uint8)src_end[0]];
  return src_end - src;
}

// src points to a letter. Find the byte length of an octagram starting there.
int OctaLen(const char* src) {
  const char* src_end = src;
  int charcount = 0;
  while (src_end[0] != ' ') {
    src_end += UTF8OneCharLen(src);
    ++charcount;
    if (charcount == 8) {break;}
  }
  return src_end - src;
}

}       // End namespace CLD2





