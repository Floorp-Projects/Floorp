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
// Degenerate CLD2 scoring lookup table, for use as placeholder
//
#include "cld2tablesummary.h"

namespace CLD2 {

static const uint32 kDistinctBiTableBuildDate = 20130101;    // yyyymmdd
static const uint32 kDistinctBiTableSize = 1;       // Total Bucket count
static const uint32 kDistinctBiTableKeyMask = 0xffffffff;    // Mask hash key
static const char* const kDistinctBiTableRecognizedLangScripts = "";

// Empty table
static const IndirectProbBucket4 kDistinctBiTable[kDistinctBiTableSize] = {
  // key[4], words[4] in UTF-8
  // value[4]
  { {0x00000000,0x00000000,0x00000000,0x00000000}},	// [000]
};

static const uint32 kDistinctBiTableSizeOne = 1;    // One-langprob count
static const uint32 kDistinctBiTableIndSize = 1;       // Largest subscript
static const uint32 kDistinctBiTableInd[kDistinctBiTableIndSize] = {
  // [0000]
  0x00000000, };

extern const CLD2TableSummary kDistinctBiTable_obj = {
  kDistinctBiTable,
  kDistinctBiTableInd,
  kDistinctBiTableSizeOne,
  kDistinctBiTableSize,
  kDistinctBiTableKeyMask,
  kDistinctBiTableBuildDate,
  kDistinctBiTableRecognizedLangScripts,
};

}       // End namespace CLD2

// End of generated tables
