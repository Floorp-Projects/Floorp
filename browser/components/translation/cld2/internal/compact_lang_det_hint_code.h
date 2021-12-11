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

#ifndef I18N_ENCODINGS_COMPACT_LANG_DET_COMPACT_LANG_DET_HINT_CODE_H__
#define I18N_ENCODINGS_COMPACT_LANG_DET_COMPACT_LANG_DET_HINT_CODE_H__


#include <string>
#include "integral_types.h"
#include "lang_script.h"
#include "../public/encodings.h"

namespace CLD2 {

// Packed <Language, weight>, weight in [-32..31] (powers of 2**1.6 ~=3.03)
// Full language in bottom 10 bits, weight in top 6 bits
typedef int16 OneCLDLangPrior;

const int kMaxOneCLDLangPrior = 14;
typedef struct {
  int32 n;
  OneCLDLangPrior prior[kMaxOneCLDLangPrior];
} CLDLangPriors;

// Reading exposed here; setting hidden in .cc
inline int GetCLDPriorWeight(OneCLDLangPrior olp) {
  return olp >> 10;
}
inline Language GetCLDPriorLang(OneCLDLangPrior olp) {
  return static_cast<Language>(olp & 0x3ff);
}

inline int32 GetCLDLangPriorCount(CLDLangPriors* lps) {
  return lps->n;
}

inline void InitCLDLangPriors(CLDLangPriors* lps) {
  lps->n = 0;
}

// Trim language priors to no more than max_entries, keeping largest abs weights
void TrimCLDLangPriors(int max_entries, CLDLangPriors* lps);

// Trim language tag string to canonical form for each language
// Input is from GetLangTagsFromHtml(), already lowercased
std::string TrimCLDLangTagsHint(const std::string& langtags);

// Add hints to vector of langpriors
// Input is from GetLangTagsFromHtml(), already lowercased
void SetCLDLangTagsHint(const std::string& langtags, CLDLangPriors* langpriors);

// Add hints to vector of langpriors
// Input is from HTTP content-language
void SetCLDContentLangHint(const char* contentlang, CLDLangPriors* langpriors);

// Add hints to vector of langpriors
// Input is from GetTLD(), already lowercased
void SetCLDTLDHint(const char* tld, CLDLangPriors* langpriors);

// Add hints to vector of langpriors
// Input is from DetectEncoding()
void SetCLDEncodingHint(Encoding enc, CLDLangPriors* langpriors);

// Add hints to vector of langpriors
// Input is from random source
void SetCLDLanguageHint(Language lang, CLDLangPriors* langpriors);

// Make printable string of priors
std::string DumpCLDLangPriors(const CLDLangPriors* langpriors);


// Get language tag hints from HTML body
// Normalize: remove spaces and make lowercase comma list
std::string GetLangTagsFromHtml(const char* utf8_body, int32 utf8_body_len,
                           int32 max_scan_bytes);

}       // End namespace CLD2

#endif  // I18N_ENCODINGS_COMPACT_LANG_DET_COMPACT_LANG_DET_HINT_CODE_H__

