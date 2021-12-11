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
// File: lang_script.h
// ================
//
// Author: dsites@google.com (Dick Sites)
//
// This file declares language and script numbers and names for CLD2,
// plus routines that access side tables based on these
//

#ifndef I18N_ENCODINGS_CLD2_LANG_SCRIPT_H__
#define I18N_ENCODINGS_CLD2_LANG_SCRIPT_H__

#include "generated_language.h"
#include "generated_ulscript.h"
#include "integral_types.h"


// NOTE: The script numbers and language numbers here are not guaranteed to be
// stable. If you want to record a result for posterity, save the
// ULScriptCode(ULScript ulscript) result as character strings.
//
// The Unicode scripts recognized by CLD2 are numbered almost arbitrarily,
// specified in an enum. Each script has human-readable script name and a
// 4-letter ISO 15924 script code. Each has a C name (largely for use by
// programs that generate declarations in cld2_generated_scripts.h). Each
// also has a recognition type
//  r_type: 0 script-only, 1 nilgrams, 2 quadgrams, 3 CJK
//
// The declarations for a particular version of Unicode are machine-generated in
//   generated_scripts.h
//
// This file includes that one and declares the access routines. The type
// involved is called "ULScript" to signify Unicode Letters-Marks Scripts,
// which are not quite Unicode Scripts. In particular, the CJK scripts are
// merged into a single number because CLD2 recognizes the CJK languages from
// four scripts intermixed: Hani (both Hans  and Hant), Hangul, Hiragana, and
// Katakana.

// Each script has one of these four recognition types.
// RTypeNone: There is no language associated with this script. In extended
//  language recognition calls, return a fake language number that maps to
//  xx-Cham, with literally "xx" for the language code,and with the script
//  code instead of "Cham". In non-extended calls, return UNKNOWN_LANGUAGE.
// RTypeOne: The script maps 1:1 to a single language. No letters are examined
//  during recognition and no lookups done.
// RTypeMany: The usual quadgram + delta-octagram + distinctive-words scoring
//  is done to determine the languages involved.
// RTypeCJK: The CJK unigram + delta-bigram scoring is done to determine the
//  languages involved.
//
// Note that the choice of recognition type is a function of script, not
// language. In particular, some languges are recognized in multiple scripts
// and those have different recognition types (Mongolian mn-Latn vs. mn-Mong
// for example).

namespace CLD2 {

//----------------------------------------------------------------------------//
// Functions of ULScript                                                      //
//----------------------------------------------------------------------------//

// If the input is out of range or otherwise unrecognized, it is treated
// as ULScript_Common (which never participates in language recognition)
const char* ULScriptName(ULScript ulscript);
const char* ULScriptCode(ULScript ulscript);
const char* ULScriptDeclaredName(ULScript ulscript);
ULScriptRType ULScriptRecognitionType(ULScript ulscript);

// Name can be either full name or ISO code, or can be ISO code embedded in
// a language-script combination such as "en-Latn-GB"
ULScript GetULScriptFromName(const char* src);

// Map script into Latin, Cyrillic, Arabic, Other
int LScript4(ULScript ulscript);

//----------------------------------------------------------------------------//
// Functions of Language                                                      //
//----------------------------------------------------------------------------//

// The languages recognized by CLD2 are numbered almost arbitrarily,
// specified in an enum. Each language has human-readable language name and a
// 2- or 3-letter ISO 639 language code. Each has a C name (largely for use by
// programs that generate declarations in cld2_generated_languagess.h).
// Each has a list of up to four scripts in which it is currently recognized.
//
// The declarations for a particular set of recognized languages are
// machine-generated in
//   generated_languages.h
//
// The Language enum is intended to match the internal Google Language enum
// in i18n/languages/proto/languages.proto up to NUM_LANGUAGES, with additional
// languages assigned above that. Over time, some languages may be renumbered
// if they are moved into the Language enum.
//
// The Language enum includes the fake language numbers for RTypeNone above.
//


// If the input is out of range or otherwise unrecognized, it is treated
// as UNKNOWN_LANGUAGE
//
// LanguageCode
// ------------
// Given the Language, return the language code, e.g. "ko"
// This is determined by
// the following (in order of preference):
// - ISO-639-1 two-letter language code
//   (all except those mentioned below)
// - ISO-639-2 three-letter bibliographic language code
//   (Tibetan, Dhivehi, Cherokee, Syriac)
// - Google-specific language code
//   (ChineseT ("zh-TW"), Teragram Unknown, Unknown,
//   Portuguese-Portugal, Portuguese-Brazil, Limbu)
// - Fake RTypeNone names.

const char* LanguageName(Language lang);
const char* LanguageCode(Language lang);
const char* LanguageShortCode(Language lang);
const char* LanguageDeclaredName(Language lang);

// n is in 0..3. Trailing entries are filled with
// ULScript_Common (which never participates in language recognition)
ULScript LanguageRecognizedScript(Language lang, int n);

// Name can be either full name or ISO code, or can be ISO code embedded in
// a language-script combination such as "en-Latn-GB"
Language GetLanguageFromName(const char* src);

// Returns which set of statistically-close languages lang is in. 0 means none.
int LanguageCloseSet(Language lang);

//----------------------------------------------------------------------------//
// Functions of ULScript and Language                                         //
//----------------------------------------------------------------------------//

// Most common language in each script
Language DefaultLanguage(ULScript ulscript);

// For RTypeMany recognition,
// the CLD2 lookup tables are kept small by encoding a language into one byte.
// To avoid limiting CLD2 to at most 256 languages, a larger range of external
// Language numbers is mapped to a smaller range of per-script numbers. At
// the moment (January 2013) the Latin script has about 90 languages to be
// recognized, while all the other scripts total about 50 more languages. In
// addition, the RTypeNone scripts map to about 100 fake languages.
// So we map all Latin-script languages to one range of 1..255 per-script
// numbers and map all the other RTypeMany languages to an overlapping range
// 1..255 of per-script numbers.

uint8 PerScriptNumber(ULScript ulscript, Language lang);
Language FromPerScriptNumber(ULScript ulscript, uint8 perscript_number);

// While the speed-sensitive processing deals with per-script language numbers,
// there is a need for low-performance dealing with original language numbers
// and unknown scripts, mostly for processing language hints.
// These routines let one derive a script class from a bare language.
// For languages written in multiple scripts, both of these can return true.

bool IsLatnLanguage(Language lang);
bool IsOthrLanguage(Language lang);


//----------------------------------------------------------------------------//
// Other                                                                      //
//----------------------------------------------------------------------------//

// Utility routine to search alphabetical tables
int BinarySearch(const char* key, int lo, int hi, const CharIntPair* cipair);

}  // namespace CLD2

#endif  // I18N_ENCODINGS_CLD2_LANG_SCRIPT_H__
