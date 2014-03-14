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

#include <stdio.h>
#include <stdlib.h>

#include "../public/compact_lang_det.h"
#include "../public/encodings.h"
#include "compact_lang_det_impl.h"
#include "integral_types.h"
#include "lang_script.h"

namespace CLD2 {

// String is "code_version - data_scrape_date"
//static const char* kDetectLanguageVersion = "V2.0 - 20130715";


// Large-table version for all ~160 languages
// Small-table version for all ~60 languages

// Scan interchange-valid UTF-8 bytes and detect most likely language
Language DetectLanguage(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          bool* is_reliable) {
  bool allow_extended_lang = false;
  Language language3[3];
  int percent3[3];
  double normalized_score3[3];
  int text_bytes;
  int flags = 0;
  Language plus_one = UNKNOWN_LANGUAGE;
  const char* tld_hint = "";
  int encoding_hint = UNKNOWN_ENCODING;
  Language language_hint = UNKNOWN_LANGUAGE;
  CLDHints cldhints = {NULL, tld_hint, encoding_hint, language_hint};

  Language lang = DetectLanguageSummaryV2(
                          buffer,
                          buffer_length,
                          is_plain_text,
                          &cldhints,
                          allow_extended_lang,
                          flags,
                          plus_one,
                          language3,
                          percent3,
                          normalized_score3,
                          NULL,
                          &text_bytes,
                          is_reliable);
  // Default to English
  if (lang == UNKNOWN_LANGUAGE) {
    lang = ENGLISH;
  }
  return lang;
}

// Scan interchange-valid UTF-8 bytes and detect list of top 3 languages.
Language DetectLanguageSummary(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          Language* language3,
                          int* percent3,
                          int* text_bytes,
                          bool* is_reliable) {
  double normalized_score3[3];
  bool allow_extended_lang = false;
  int flags = 0;
  Language plus_one = UNKNOWN_LANGUAGE;
  const char* tld_hint = "";
  int encoding_hint = UNKNOWN_ENCODING;
  Language language_hint = UNKNOWN_LANGUAGE;
  CLDHints cldhints = {NULL, tld_hint, encoding_hint, language_hint};

  Language lang = DetectLanguageSummaryV2(
                          buffer,
                          buffer_length,
                          is_plain_text,
                          &cldhints,
                          allow_extended_lang,
                          flags,
                          plus_one,
                          language3,
                          percent3,
                          normalized_score3,
                          NULL,
                          text_bytes,
                          is_reliable);
  // Default to English
  if (lang == UNKNOWN_LANGUAGE) {
    lang = ENGLISH;
  }
  return lang;
}

// Same as above, with hints supplied
// Scan interchange-valid UTF-8 bytes and detect list of top 3 languages.
Language DetectLanguageSummary(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          const char* tld_hint,       // "id" boosts Indonesian
                          int encoding_hint,          // SJS boosts Japanese
                          Language language_hint,     // ITALIAN boosts it
                          Language* language3,
                          int* percent3,
                          int* text_bytes,
                          bool* is_reliable) {
  double normalized_score3[3];
  bool allow_extended_lang = false;
  int flags = 0;
  Language plus_one = UNKNOWN_LANGUAGE;
  CLDHints cldhints = {NULL, tld_hint, encoding_hint, language_hint};

  Language lang = DetectLanguageSummaryV2(
                          buffer,
                          buffer_length,
                          is_plain_text,
                          &cldhints,
                          allow_extended_lang,
                          flags,
                          plus_one,
                          language3,
                          percent3,
                          normalized_score3,
                          NULL,
                          text_bytes,
                          is_reliable);
  // Default to English
  if (lang == UNKNOWN_LANGUAGE) {
    lang = ENGLISH;
  }
  return lang;
}


// Scan interchange-valid UTF-8 bytes and detect list of top 3 extended
// languages.
// Extended languages are additional Google interface languages and Unicode
// single-language scripts, from ext_lang_enc.h
Language ExtDetectLanguageSummary(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          Language* language3,
                          int* percent3,
                          int* text_bytes,
                          bool* is_reliable) {
  double normalized_score3[3];
  bool allow_extended_lang = true;
  int flags = 0;
  Language plus_one = UNKNOWN_LANGUAGE;
  const char* tld_hint = "";
  int encoding_hint = UNKNOWN_ENCODING;
  Language language_hint = UNKNOWN_LANGUAGE;
  CLDHints cldhints = {NULL, tld_hint, encoding_hint, language_hint};

  Language lang = DetectLanguageSummaryV2(
                          buffer,
                          buffer_length,
                          is_plain_text,
                          &cldhints,
                          allow_extended_lang,
                          flags,
                          plus_one,
                          language3,
                          percent3,
                          normalized_score3,
                          NULL,
                          text_bytes,
                          is_reliable);
  // Do not default to English
  return lang;
}

// Same as above, with hints supplied
// Scan interchange-valid UTF-8 bytes and detect list of top 3 extended
// languages.
// Extended languages are additional Google interface languages and Unicode
// single-language scripts, from ext_lang_enc.h
Language ExtDetectLanguageSummary(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          const char* tld_hint,       // "id" boosts Indonesian
                          int encoding_hint,          // SJS boosts Japanese
                          Language language_hint,     // ITALIAN boosts it
                          Language* language3,
                          int* percent3,
                          int* text_bytes,
                          bool* is_reliable) {
  double normalized_score3[3];
  bool allow_extended_lang = true;
  int flags = 0;
  Language plus_one = UNKNOWN_LANGUAGE;
  CLDHints cldhints = {NULL, tld_hint, encoding_hint, language_hint};

  Language lang = DetectLanguageSummaryV2(
                          buffer,
                          buffer_length,
                          is_plain_text,
                          &cldhints,
                          allow_extended_lang,
                          flags,
                          plus_one,
                          language3,
                          percent3,
                          normalized_score3,
                          NULL,
                          text_bytes,
                          is_reliable);
  // Do not default to English
  return lang;
}

// Same as above, and also returns internal language scores as a ratio to
// normal score for real text in that language. Scores close to 1.0 indicate
// normal text, while scores far away from 1.0 indicate badly-skewed text or
// gibberish
//
Language ExtDetectLanguageSummary(
                        const char* buffer,
                        int buffer_length,
                        bool is_plain_text,
                        const char* tld_hint,       // "id" boosts Indonesian
                        int encoding_hint,          // SJS boosts Japanese
                        Language language_hint,     // ITALIAN boosts it
                        Language* language3,
                        int* percent3,
                        double* normalized_score3,
                        int* text_bytes,
                        bool* is_reliable) {
  bool allow_extended_lang = true;
  int flags = 0;
  Language plus_one = UNKNOWN_LANGUAGE;
  CLDHints cldhints = {NULL, tld_hint, encoding_hint, language_hint};

  Language lang = DetectLanguageSummaryV2(
                          buffer,
                          buffer_length,
                          is_plain_text,
                          &cldhints,
                          allow_extended_lang,
                          flags,
                          plus_one,
                          language3,
                          percent3,
                          normalized_score3,
                          NULL,
                          text_bytes,
                          is_reliable);
  // Do not default to English
  return lang;
}

// Use this one.
// Hints are collected into a struct.
// Flags are passed in (normally zero).
//
// Also returns 3 internal language scores as a ratio to
// normal score for real text in that language. Scores close to 1.0 indicate
// normal text, while scores far away from 1.0 indicate badly-skewed text or
// gibberish
//
// Returns a vector of chunks in different languages, so that caller may
// spell-check, translate, or otherwaise process different parts of the input
// buffer in language-dependant ways.
//
Language ExtDetectLanguageSummary(
                        const char* buffer,
                        int buffer_length,
                        bool is_plain_text,
                        const CLDHints* cld_hints,
                        int flags,
                        Language* language3,
                        int* percent3,
                        double* normalized_score3,
                        ResultChunkVector* resultchunkvector,
                        int* text_bytes,
                        bool* is_reliable) {
  bool allow_extended_lang = true;
  Language plus_one = UNKNOWN_LANGUAGE;

  Language lang = DetectLanguageSummaryV2(
                          buffer,
                          buffer_length,
                          is_plain_text,
                          cld_hints,
                          allow_extended_lang,
                          flags,
                          plus_one,
                          language3,
                          percent3,
                          normalized_score3,
                          resultchunkvector,
                          text_bytes,
                          is_reliable);
  // Do not default to English
  return lang;
}

}       // End namespace CLD2

