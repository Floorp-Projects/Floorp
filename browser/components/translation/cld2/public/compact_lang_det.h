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

// NOTE:
// Baybayin (ancient script of the Philippines) is detected as TAGALOG.
// Chu Nom (Vietnamese ancient Han characters) is detected as VIETNAMESE.
// HAITIAN_CREOLE is detected as such.
// NORWEGIAN and NORWEGIAN_N are detected separately (but not robustly)
// PORTUGUESE, PORTUGUESE_P, and PORTUGUESE_B are all detected as PORTUGUESE.
// ROMANIAN-Latin is detected as ROMANIAN; ROMANIAN-Cyrillic as ROMANIAN.
// BOSNIAN is not detected as such, but likely scores as Croatian or Serbian.
// MONTENEGRIN is not detected as such, but likely scores as Serbian.
// CROATIAN is detected in the Latin script
// SERBIAN is detected in the Cyrililc and Latin scripts
// Zhuang is detected in the Latin script only.
//
// The languages X_PIG_LATIN and X_KLINGON are detected in the
//  extended calls ExtDetectLanguageSummary().
//
// UNKNOWN_LANGUAGE is returned if no language's internal reliablity measure
//  is high enough. This happens with non-text input such as the bytes of a
//  JPEG, and also with text in languages outside training set.
//
// The following languages are to be detected in multiple scripts:
//  AZERBAIJANI (Latin, Cyrillic*, Arabic*)
//  BURMESE (Latin, Myanmar)
//  HAUSA (Latin, Arabic)
//  KASHMIRI (Arabic, Devanagari)
//  KAZAKH (Latin, Cyrillic, Arabic)
//  KURDISH (Latin*, Arabic)
//  KYRGYZ (Cyrillic, Arabic)
//  LIMBU (Devanagari, Limbu)
//  MONGOLIAN (Cyrillic, Mongolian)
//  SANSKRIT (Latin, Devanagari)
//  SINDHI (Arabic, Devanagari)
//  TAGALOG (Latin, Tagalog)
//  TAJIK (Cyrillic, Arabic*)
//  TATAR (Latin, Cyrillic, Arabic)
//  TURKMEN (Latin, Cyrillic, Arabic)
//  UIGHUR (Latin, Cyrillic, Arabic)
//  UZBEK (Latin, Cyrillic, Arabic)
//
// * Due to a shortage of training text, AZERBAIJANI is not currently detected
//   in Arabic or Cyrillic scripts, nor KURDISH in Latin script, nor TAJIK in
//   Arabic script.
//

#ifndef I18N_ENCODINGS_CLD2_PUBLIC_COMPACT_LANG_DET_H_
#define I18N_ENCODINGS_CLD2_PUBLIC_COMPACT_LANG_DET_H_

#include <vector>
#include "../internal/lang_script.h"  // For Language

namespace CLD2 {

  // Scan interchange-valid UTF-8 bytes and detect most likely language,
  // or set of languages.
  //
  // Design goals:
  //   Skip over big stretches of HTML tags
  //   Able to return ranges of different languages
  //   Relatively small tables and relatively fast processing
  //   Thread safe
  //
  // For HTML documents, tags are skipped, along with <script> ... </script>
  // and <style> ... </style> sequences, and entities are expanded.
  //
  // We distinguish between bytes of the raw input buffer and bytes of non-tag
  // text letters. Since tags can be over 50% of the bytes of an HTML Page,
  // and are nearly all seven-bit ASCII English, we prefer to distinguish
  // language mixture fractions based on just the non-tag text.
  //
  // Inputs: text and text_length
  //  Code skips HTML tags and expands HTML entities, unless
  //  is_plain_text is true
  // Outputs:
  //  language3 is an array of the top 3 languages or UNKNOWN_LANGUAGE
  //  percent3 is an array of the text percentages 0..100 of the top 3 languages
  //  text_bytes is the amount of non-tag/letters-only text found
  //  is_reliable set true if the returned Language is some amount more
  //   probable then the second-best Language. Calculation is a complex function
  //   of the length of the text and the different-script runs of text.
  // Return value: the most likely Language for the majority of the input text
  //  Length 0 input returns UNKNOWN_LANGUAGE. Very short indeterminate text
  //  defaults to ENGLISH.
  //
  // The first two versions return ENGLISH instead of UNKNOWN_LANGUAGE, for
  // backwards compatibility with a different detector.
  //
  // The third version may return UNKNOWN_LANGUAGE, and also returns extended
  // language codes from lang_script.h
  //


  // Instead of individual arguments, pass in hints as an initialized struct
  // Init to {NULL, NULL, UNKNOWN_ENCODING, UNKNOWN_LANGUAGE} if not known.
  //
  // Pass in hints whenever possible; doing so improves detection accuracy. The
  // set of passed-in hints are all information that is external to the text
  // itself.
  //
  // The content_language_hint is intended to come from an HTTP header
  // Content-Language: field, the tld_hint from the hostname of a URL, the
  // encoding-hint from an encoding detector applied to the input
  // document, and the language hint from any other context you might have.
  // The lang= tags inside an HTML document will be picked up as hints
  // by code within the compact language detector.

  typedef struct {
    const char* content_language_hint;      // "mi,en" boosts Maori and English
    const char* tld_hint;                   // "id" boosts Indonesian
    int encoding_hint;                      // SJS boosts Japanese
    Language language_hint;                 // ITALIAN boosts it
  } CLDHints;

  static const int kMaxResultChunkBytes = 65535;

  // For returning a vector of per-language pieces of the input buffer
  // Unreliable and too-short are mapped to UNKNOWN_LANGUAGE
  typedef struct {
    int offset;                 // Starting byte offset in original buffer
    uint16 bytes;               // Number of bytes in chunk
    uint16 lang1;               // Top lang, as full Language. Apply
                                // static_cast<Language>() to this short value.
  } ResultChunk;
  typedef std::vector<ResultChunk> ResultChunkVector;


  // Scan interchange-valid UTF-8 bytes and detect most likely language
  Language DetectLanguage(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          bool* is_reliable);

  // Scan interchange-valid UTF-8 bytes and detect list of top 3 languages.
  // language3[0] is usually also the return value
  Language DetectLanguageSummary(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          Language* language3,
                          int* percent3,
                          int* text_bytes,
                          bool* is_reliable);

  // Same as above, with hints supplied
  // Scan interchange-valid UTF-8 bytes and detect list of top 3 languages.
  // language3[0] is usually also the return value
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
                          bool* is_reliable);

  // Scan interchange-valid UTF-8 bytes and detect list of top 3 extended
  // languages.
  //
  // Extended languages are additional interface languages and Unicode
  // single-language scripts, from lang_script.h
  //
  // language3[0] is usually also the return value
  Language ExtDetectLanguageSummary(
                          const char* buffer,
                          int buffer_length,
                          bool is_plain_text,
                          Language* language3,
                          int* percent3,
                          int* text_bytes,
                          bool* is_reliable);

  // Same as above, with hints supplied
  // Scan interchange-valid UTF-8 bytes and detect list of top 3 extended
  // languages.
  //
  // Extended languages are additional Google interface languages and Unicode
  // single-language scripts, from lang_script.h
  //
  // language3[0] is usually also the return value
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
                          bool* is_reliable);

  // Same as above, and also returns 3 internal language scores as a ratio to
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
                          bool* is_reliable);


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
                          bool* is_reliable);

  // Return version text string
  // String is "code_version - data_build_date"
  const char* DetectLanguageVersion();


  // Public use flags, debug output controls
  static const int kCLDFlagScoreAsQuads = 0x0100;  // Force Greek, etc. => quads
  static const int kCLDFlagHtml =         0x0200;  // Debug HTML => stderr
  static const int kCLDFlagCr =           0x0400;  // <cr> per chunk if HTML
  static const int kCLDFlagVerbose =      0x0800;  // More debug HTML => stderr
  static const int kCLDFlagQuiet =        0x1000;  // Less debug HTML => stderr
  static const int kCLDFlagEcho =         0x2000;  // Echo input => stderr


/***

Flag meanings:
 kCLDFlagScoreAsQuads
   Normally, several languages are detected solely by their Unicode script.
   Combined with appropritate lookup tables, this flag forces them instead
   to be detected via quadgrams. This can be a useful refinement when looking
   for meaningful text in these languages, instead of just character sets.
   The default tables do not support this use.
 kCLDFlagHtml
   For each detection call, write an HTML file to stderr, showing the text
   chunks and their detected languages.
 kCLDFlagCr
   In that HTML file, force a new line for each chunk.
 kCLDFlagVerbose
   In that HTML file, show every lookup entry.
 kCLDFlagQuiet
   In that HTML file, suppress most of the output detail.
 kCLDFlagEcho
  Echo every input buffer to stderr.
***/

// Debug output: Print the resultchunkvector to file f
void DumpResultChunkVector(FILE* f, const char* src,
                           ResultChunkVector* resultchunkvector);

#ifdef CLD2_DYNAMIC_MODE

// If compiled with dynamic mode, load data from the specified file location.
// If other data has already been loaded, it is discarded and the data is read
// in from the specified file location again (even if the file has not changed).
// WARNING: Before calling this method, language detection will always fail
// and will always return the unknown language.
void loadData(const char* fileName);

// If compiled with dynamic mode, unload the previously-loaded data.
// WARNING: After calling this method, language detection will no longer work
// and will always return the unknown language.
void unloadData();

// Returns true if and only if data has been loaded via a call to loadData(...)
// and has not been subsequently unladed via a call to unloadDate().
bool isDataLoaded();

#endif // #ifdef CLD2_DYNAMIC_MODE

};      // End namespace CLD2

#endif  // I18N_ENCODINGS_CLD2_PUBLIC_COMPACT_LANG_DET_H_
