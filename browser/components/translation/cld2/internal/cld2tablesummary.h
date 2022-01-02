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


#ifndef I18N_ENCODINGS_CLD2_INTERNAL_CLD2TABLESUMMARY_H_
#define I18N_ENCODINGS_CLD2_INTERNAL_CLD2TABLESUMMARY_H_

#include "integral_types.h"

namespace CLD2 {

// Hash bucket for four-way associative lookup, indirect probabilities
// 16 bytes per bucket, 4-byte entries
typedef struct {
  uint32 keyvalue[4];   // Upper part of word is hash, lower is indirect prob
} IndirectProbBucket4;


// Expanded version December 2012.
// Moves cutoff for 6-language vs. 3-language indirects
// Has list of recognized lang-script combinations
typedef struct {
  const IndirectProbBucket4* kCLDTable;
                                      // Each bucket has four entries, part
                                      //  key and part indirect subscript
  const uint32* kCLDTableInd;         // Each entry is three packed lang/prob
  uint32 kCLDTableSizeOne;            // Indirect subscripts >= this: 2 entries
  uint32 kCLDTableSize;               // Bucket count
  uint32 kCLDTableKeyMask;            // Mask hash key
  uint32 kCLDTableBuildDate;          // yyyymmdd
  const char* kRecognizedLangScripts; // Character string of lang-Scripts
                                      //  recognized: "en-Latn az-Arab ..."
                                      //  Single space delimiter, Random order
} CLD2TableSummary;

}       // End namespace CLD2

#endif  // I18N_ENCODINGS_CLD2_INTERNAL_CLD2TABLESUMMARY_H_


