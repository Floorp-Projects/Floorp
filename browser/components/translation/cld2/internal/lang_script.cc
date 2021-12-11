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
// File: lang_script.cc
// ================
//
// Author: dsites@google.com (Dick Sites)
//
// This file declares language and script numbers and names for CLD2
//

#include "lang_script.h"

#include <stdlib.h>
#include <string.h>

#include "generated_language.h"
#include "generated_ulscript.h"

namespace CLD2 {

// Language tables
// Subscripted by enum Language
extern const int kLanguageToNameSize;
extern const char* const kLanguageToName[];
extern const int kLanguageToCodeSize;
extern const char* const kLanguageToCode[];
extern const int kLanguageToCNameSize;
extern const char* const kLanguageToCName[];
extern const int kLanguageToScriptsSize;
extern const FourScripts kLanguageToScripts[];

// Subscripted by Language
extern const int kLanguageToPLangSize;
extern const uint8 kLanguageToPLang[];
// Subscripted by per-script language
extern const uint16 kPLangToLanguageLatn[];
extern const uint16 kPLangToLanguageOthr[];

// Alphabetical order for binary search
extern const int kNameToLanguageSize;
extern const CharIntPair kNameToLanguage[];
extern const int kCodeToLanguageSize;
extern const CharIntPair kCodeToLanguage[];

// ULScript tables
// Subscripted by enum ULScript
extern const int kULScriptToNameSize;
extern const char* const kULScriptToName[];
extern const int kULScriptToCodeSize;
extern const char* const kULScriptToCode[];
extern const int kULScriptToCNameSize;
extern const char* const kULScriptToCName[];
extern const int kULScriptToRtypeSize;
extern const ULScriptRType kULScriptToRtype[];
extern const int kULScriptToDefaultLangSize;
extern const Language kULScriptToDefaultLang[];

// Alphabetical order for binary search
extern const int kNameToULScriptSize;
extern const CharIntPair kNameToULScript[];
extern const int kCodeToULScriptSize;
extern const CharIntPair kCodeToULScript[];


//
// File: lang_script.h
// ================
//
// Author: dsites@google.com (Dick Sites)
//
// This file declares language and script numbers and names for CLD2
//


// NOTE: The script numbers and language numbers here are not guaranteed to be
// stable. If you want to record a result for posterity, save the ISO codes
// as character strings.
//
//
// The Unicode scripts recognized by CLD2 are numbered almost arbitrarily,
// specified in an enum. Each script has human-readable script name and a
// 4-letter ISO 15924 script code. Each has a C name (largely for use by
// programs that generate declarations in cld2_generated_scripts.h). Each
// also has a recognition type
//  r_type: 0 script-only, 1 nilgrams, 2 quadgrams, 3 CJK
//
// The declarations for a particular version of Unicode are machine-generated in
//   cld2_generated_scripts.h
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

//----------------------------------------------------------------------------//
// Functions of ULScript                                                      //
//----------------------------------------------------------------------------//

// If the input is out of range or otherwise unrecognized, it is treated
// as UNKNOWN_ULSCRIPT (which never participates in language recognition)
const char* ULScriptName(ULScript ulscript) {
  int i_ulscript = ulscript;
  if (i_ulscript < 0) {i_ulscript = UNKNOWN_ULSCRIPT;}
  if (i_ulscript >= NUM_ULSCRIPTS) {i_ulscript = UNKNOWN_ULSCRIPT;}
  return kULScriptToName[i_ulscript];
}

const char* ULScriptCode(ULScript ulscript) {
  int i_ulscript = ulscript;
  if (i_ulscript < 0) {i_ulscript = UNKNOWN_ULSCRIPT;}
  if (i_ulscript >= NUM_ULSCRIPTS) {i_ulscript = UNKNOWN_ULSCRIPT;}
  return kULScriptToCode[i_ulscript];
}

const char* ULScriptDeclaredName(ULScript ulscript) {
  int i_ulscript = ulscript;
  if (i_ulscript < 0) {i_ulscript = UNKNOWN_ULSCRIPT;}
  if (i_ulscript >= NUM_ULSCRIPTS) {i_ulscript = UNKNOWN_ULSCRIPT;}
  return kULScriptToCName[i_ulscript];
}

ULScriptRType ULScriptRecognitionType(ULScript ulscript) {
  int i_ulscript = ulscript;
  if (i_ulscript < 0) {i_ulscript = UNKNOWN_ULSCRIPT;}
  if (i_ulscript >= NUM_ULSCRIPTS) {i_ulscript = UNKNOWN_ULSCRIPT;}
  return kULScriptToRtype[i_ulscript];
}



// The languages recognized by CLD2 are numbered almost arbitrarily,
// specified in an enum. Each language has human-readable language name and a
// 2- or 3-letter ISO 639 language code. Each has a C name (largely for use by
// programs that generate declarations in cld2_generated_languagess.h).
// Each has a list of up to four scripts in which it is currently recognized.
//
// The declarations for a particular set of recognized languages are
// machine-generated in
//   cld2_generated_languages.h
//
// The Language enum is intended to match the internal Google Language enum
// in i18n/languages/proto/languages.proto up to NUM_LANGUAGES, with additional
// languages assigned above that. Over time, some languages may be renumbered
// if they are moved into the Language enum.
//
// The Language enum includes the fake language numbers for RTypeNone above.
//
// In an open-source environment, the Google-specific Language enum is not
// available. Language decouples the two environments while maintaining
// internal compatibility.


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

//----------------------------------------------------------------------------//
// Functions of Language                                                      //
//----------------------------------------------------------------------------//

const char* LanguageName(Language lang) {
  int i_lang = lang;
  if (i_lang < 0) {i_lang = UNKNOWN_LANGUAGE;}
  if (i_lang >= NUM_LANGUAGES) {i_lang = UNKNOWN_LANGUAGE;}
  return kLanguageToName[i_lang];
}
const char* LanguageCode(Language lang) {
  int i_lang = lang;
  if (i_lang < 0) {i_lang = UNKNOWN_LANGUAGE;}
  if (i_lang >= NUM_LANGUAGES) {i_lang = UNKNOWN_LANGUAGE;}
  return kLanguageToCode[i_lang];
}

const char* LanguageDeclaredName(Language lang) {
  int i_lang = lang;
  if (i_lang < 0) {i_lang = UNKNOWN_LANGUAGE;}
  if (i_lang >= NUM_LANGUAGES) {i_lang = UNKNOWN_LANGUAGE;}
  return kLanguageToCName[i_lang];
}

// n is in 0..3. Trailing entries are filled with
// UNKNOWN_LANGUAGE (which never participates in language recognition)
ULScript LanguageRecognizedScript(Language lang, int n) {
  int i_lang = lang;
  if (i_lang < 0) {i_lang = UNKNOWN_LANGUAGE;}
  if (i_lang >= NUM_LANGUAGES) {i_lang = UNKNOWN_LANGUAGE;}
  return static_cast<ULScript>(kLanguageToScripts[i_lang][n]);
}

// Given the Language, returns its string name used as the output by
// the lang/enc identifier, e.g. "Korean"
// "invalid_language" if the input is invalid.
// TG_UNKNOWN_LANGUAGE is used as a placeholder for the "ignore me" language,
// used to subtract out HTML, link farms, DNA strings, and alittle English porn
const char* ExtLanguageName(const Language lang) {
  return LanguageName(lang);
}

// Given the Language, return the language code, e.g. "ko"
const char* ExtLanguageCode(const Language lang) {
  return LanguageCode(lang);
}


// Given the Language, returns its Language enum spelling, for use by
// programs that create C declarations, e.g. "KOREAN"
// "UNKNOWN_LANGUAGE" if the input is invalid.
const char* ExtLanguageDeclaredName(const Language lang) {
  return LanguageDeclaredName(lang);
}


extern const int kCloseSetSize = 10;

// Returns which set of statistically-close languages lang is in. 0 means none.
int LanguageCloseSet(Language lang) {
  // Scaffolding
  // id ms         # INDONESIAN MALAY coef=0.4698    Problematic w/o extra words
  // bo dz         # TIBETAN DZONGKHA coef=0.4571
  // cs sk         # CZECH SLOVAK coef=0.4273
  // zu xh         # ZULU XHOSA coef=0.3716
  //
  // bs hr sr srm  # BOSNIAN CROATIAN SERBIAN MONTENEGRIN
  // hi mr bh ne   # HINDI MARATHI BIHARI NEPALI
  // no nn da      # NORWEGIAN NORWEGIAN_N DANISH
  // gl es pt      # GALICIAN SPANISH PORTUGUESE
  // rw rn         # KINYARWANDA RUNDI

  if (lang == INDONESIAN) {return 1;}
  if (lang == MALAY) {return 1;}

  if (lang == TIBETAN) {return 2;}
  if (lang == DZONGKHA) {return 2;}

  if (lang == CZECH) {return 3;}
  if (lang == SLOVAK) {return 3;}

  if (lang == ZULU) {return 4;}
  if (lang == XHOSA) {return 4;}

  if (lang == BOSNIAN) {return 5;}
  if (lang == CROATIAN) {return 5;}
  if (lang == SERBIAN) {return 5;}
  if (lang == MONTENEGRIN) {return 5;}

  if (lang == HINDI) {return 6;}
  if (lang == MARATHI) {return 6;}
  if (lang == BIHARI) {return 6;}
  if (lang == NEPALI) {return 6;}

  if (lang == NORWEGIAN) {return 7;}
  if (lang == NORWEGIAN_N) {return 7;}
  if (lang == DANISH) {return 7;}

  if (lang == GALICIAN) {return 8;}
  if (lang == SPANISH) {return 8;}
  if (lang == PORTUGUESE) {return 8;}

  if (lang == KINYARWANDA) {return 9;}
  if (lang == RUNDI) {return 9;}

  return 0;
}

//----------------------------------------------------------------------------//
// Functions of ULScript and Language                                         //
//----------------------------------------------------------------------------//

Language DefaultLanguage(ULScript ulscript) {
  if (ulscript < 0) {return UNKNOWN_LANGUAGE;}
  if (ulscript >= NUM_ULSCRIPTS) {return UNKNOWN_LANGUAGE;}
  return kULScriptToDefaultLang[ulscript];
}

uint8 PerScriptNumber(ULScript ulscript, Language lang) {
  if (ulscript < 0) {return 0;}
  if (ulscript >= NUM_ULSCRIPTS) {return 0;}
  if (kULScriptToRtype[ulscript] == RTypeNone) {return 1;}
  if (lang >= kLanguageToPLangSize) {return 0;}
  return kLanguageToPLang[lang];
}

Language FromPerScriptNumber(ULScript ulscript, uint8 perscript_number) {
  if (ulscript < 0) {return UNKNOWN_LANGUAGE;}
  if (ulscript >= NUM_ULSCRIPTS) {return UNKNOWN_LANGUAGE;}
  if ((kULScriptToRtype[ulscript] == RTypeNone) ||
      (kULScriptToRtype[ulscript] == RTypeOne)) {
    return kULScriptToDefaultLang[ulscript];
  }

  if (ulscript == ULScript_Latin) {
     return static_cast<Language>(kPLangToLanguageLatn[perscript_number]);
  } else {
     return static_cast<Language>(kPLangToLanguageOthr[perscript_number]);
  }
}

// Return true if language can be in the Latin script
bool IsLatnLanguage(Language lang) {
  if (lang >= kLanguageToPLangSize) {return false;}
  return (lang == kPLangToLanguageLatn[kLanguageToPLang[lang]]);
}

// Return true if language can be in a non-Latin script
bool IsOthrLanguage(Language lang) {
  if (lang >= kLanguageToPLangSize) {return false;}
  return (lang == kPLangToLanguageOthr[kLanguageToPLang[lang]]);
}


//----------------------------------------------------------------------------//
// Other                                                                      //
//----------------------------------------------------------------------------//

// Returns mid if key found in lo <= mid < hi, else -1
int BinarySearch(const char* key, int lo, int hi, const CharIntPair* cipair) {
  // binary search
  while (lo < hi) {
    int mid = (lo + hi) >> 1;
    if (strcmp(key, cipair[mid].s) < 0) {
      hi = mid;
    } else if (strcmp(key, cipair[mid].s) > 0) {
      lo = mid + 1;
    } else {
      return mid;
    }
  }
  return -1;
}

Language MakeLang(int i) {return static_cast<Language>(i);}

// Name can be either full name or ISO code, or can be ISO code embedded in
// a language-script combination such as "ABKHAZIAN", "en", "en-Latn-GB"
Language GetLanguageFromName(const char* src) {
  const char* hyphen1 = strchr(src, '-');
  const char* hyphen2 = NULL;
  if (hyphen1 != NULL) {hyphen2 = strchr(hyphen1 + 1, '-');}

  int match = -1;
  if (hyphen1 == NULL) {
    // Bare name. Look at full name, then code
    match = BinarySearch(src, 0, kNameToLanguageSize, kNameToLanguage);
    if (match >= 0) {return MakeLang(kNameToLanguage[match].i);}    // aa
    match = BinarySearch(src, 0, kCodeToLanguageSize, kCodeToLanguage);
    if (match >= 0) {return MakeLang(kCodeToLanguage[match].i);}    // aa
    return UNKNOWN_LANGUAGE;
  }

  if (hyphen2 == NULL) {
    // aa-bb. Not a full name; must be code-something. Try zh-TW then bare zh
    match = BinarySearch(src, 0, kCodeToLanguageSize, kCodeToLanguage);
    if (match >= 0) {return MakeLang(kCodeToLanguage[match].i);}    // aa-bb

    int len = strlen(src);
    if (len >= 16) {return UNKNOWN_LANGUAGE;}   // Real codes are shorter

    char temp[16];
    int hyphen1_offset = hyphen1 - src;
    // Take off part after hyphen1
    memcpy(temp, src, len);
    temp[hyphen1_offset] = '\0';
    match = BinarySearch(temp, 0, kCodeToLanguageSize, kCodeToLanguage);
    if (match >= 0) {return MakeLang(kCodeToLanguage[match].i);}    // aa

    return UNKNOWN_LANGUAGE;
  }

  // aa-bb-cc. Must be code-something. Try en-Latn-US, en-Latn, en-US, en
  match = BinarySearch(src, 0, kCodeToLanguageSize, kCodeToLanguage);
  if (match >= 0) {return MakeLang(kCodeToLanguage[match].i);}    // aa-bb-cc


  int len = strlen(src);
  if (len >= 16) {return UNKNOWN_LANGUAGE;}   // Real codes are shorter

  char temp[16];
  int hyphen1_offset = hyphen1 - src;
  int hyphen2_offset = hyphen2 - src;
  // Take off part after hyphen2
  memcpy(temp, src, len);
  temp[hyphen2_offset] = '\0';
  match = BinarySearch(temp, 0, kCodeToLanguageSize, kCodeToLanguage);
  if (match >= 0) {return MakeLang(kCodeToLanguage[match].i);}    // aa-bb


  // Take off part between hyphen1 and hyphen2
  int len2 = len - hyphen2_offset;
  memcpy(temp, src, len);
  memcpy(&temp[hyphen1_offset], hyphen2, len2);
  temp[hyphen1_offset + len2] = '\0';
  match = BinarySearch(temp, 0, kCodeToLanguageSize, kCodeToLanguage);
  if (match >= 0) {return MakeLang(kCodeToLanguage[match].i);}    // aa-cc


  // Take off everything after hyphen1
  memcpy(temp, src, len);
  temp[hyphen1_offset] = '\0';
  match = BinarySearch(temp, 0, kCodeToLanguageSize, kCodeToLanguage);
  if (match >= 0) {return MakeLang(kCodeToLanguage[match].i);}    // aa


  return UNKNOWN_LANGUAGE;
}


// Name can be either full name or ISO code, or can be ISO code embedded in
// a language-script combination such as "en-Latn-GB"
// MORE WORK to do here. also kLanguageToScripts [4] is bogus
// if bare language name, no script, want  zh, ja, ko to Hani, pt to Latn, etc.
// Something like map code to Language, then Language to kLanguageToScripts[x][0]
// ADD BIAS: kLanguageToScripts lists default script first
// If total mismatch, reutrn Latn
//   if (strcmp(src, "nd") == 0) {return NDEBELE;}         // [nd was wrong]
//   if (strcmp(src, "sit-NP-Limb") == 0) {return ULScript_Limbu;}

ULScript MakeULScr(int i) {return static_cast<ULScript>(i);}

ULScript GetULScriptFromName(const char* src) {
  const char* hyphen1 = strchr(src, '-');
  const char* hyphen2 = NULL;
  if (hyphen1 != NULL) {hyphen2 = strchr(hyphen1 + 1, '-');}

  int match = -1;
  if (hyphen1 == NULL) {
    // Bare name. Look at full name, then code, then try backmapping as Language
    match = BinarySearch(src, 0, kNameToULScriptSize, kNameToULScript);
    if (match >= 0) {return MakeULScr(kNameToULScript[match].i);}    // aa
    match = BinarySearch(src, 0, kCodeToULScriptSize, kCodeToULScript);
    if (match >= 0) {return MakeULScr(kCodeToULScript[match].i);}    // aa

    Language backmap_me = GetLanguageFromName(src);
    if (backmap_me != UNKNOWN_LANGUAGE) {
      return static_cast<ULScript>(kLanguageToScripts[backmap_me][0]);
    }
    return ULScript_Latin;
  }

  if (hyphen2 == NULL) {
    // aa-bb. Not a full name; must be code-something. Try en-Latn, bare Latn
    if (strcmp(src, "zh-TW") == 0) {return ULScript_Hani;}
    if (strcmp(src, "zh-CN") == 0) {return ULScript_Hani;}
    if (strcmp(src, "sit-NP") == 0) {return ULScript_Limbu;}
    if (strcmp(src, "sit-Limb") == 0) {return ULScript_Limbu;}
    if (strcmp(src, "sr-ME") == 0) {return ULScript_Latin;}
    match = BinarySearch(src, 0, kCodeToULScriptSize, kCodeToULScript);
    if (match >= 0) {return MakeULScr(kCodeToULScript[match].i);}    // aa-bb

    int len = strlen(src);
    if (len >= 16) {return ULScript_Latin;}   // Real codes are shorter

    char temp[16];
    int hyphen1_offset = hyphen1 - src;
    int len1 = len - hyphen1_offset - 1;    // Exclude the hyphen
    // Take off part before hyphen1
    memcpy(temp, hyphen1 + 1, len1);
    temp[len1] = '\0';
    match = BinarySearch(temp, 0, kCodeToULScriptSize, kCodeToULScript);
    if (match >= 0) {return MakeULScr(kCodeToULScript[match].i);}    // bb

    // Take off part after hyphen1
    memcpy(temp, src, len);
    temp[hyphen1_offset] = '\0';
    match = BinarySearch(temp, 0, kCodeToULScriptSize, kCodeToULScript);
    if (match >= 0) {return MakeULScr(kCodeToULScript[match].i);}    // aa

    return ULScript_Latin;
  }

  // aa-bb-cc. Must be code-something. Try en-Latn-US, en-Latn, en-US, en
  if (strcmp(src, "sit-NP-Limb") == 0) {return ULScript_Limbu;}
  if (strcmp(src, "sr-ME-Latn") == 0) {return ULScript_Latin;}
  if (strcmp(src, "sr-ME-Cyrl") == 0) {return ULScript_Cyrillic;}
  match = BinarySearch(src, 0, kCodeToULScriptSize, kCodeToULScript);
  if (match >= 0) {return MakeULScr(kCodeToULScript[match].i);}    // aa-bb-cc

  int len = strlen(src);
  if (len >= 16) {return ULScript_Latin;}   // Real codes are shorter

  char temp[16];
  int hyphen1_offset = hyphen1 - src;
  int hyphen2_offset = hyphen2 - src;
  int len2 = len - hyphen2_offset - 1;                // Exclude the hyphen
  int lenmid = hyphen2_offset - hyphen1_offset - 1;   // Exclude the hyphen
  // Keep part between hyphen1 and hyphen2
  memcpy(temp, hyphen1 + 1, lenmid);
  temp[lenmid] = '\0';
  match = BinarySearch(temp, 0, kCodeToULScriptSize, kCodeToULScript);
  if (match >= 0) {return MakeULScr(kCodeToULScript[match].i);}    // bb

  // Keep part after hyphen2
  memcpy(temp, hyphen2 + 1, len2);
  temp[len2] = '\0';
  match = BinarySearch(temp, 0, kCodeToULScriptSize, kCodeToULScript);
  if (match >= 0) {return MakeULScr(kCodeToULScript[match].i);}    // cc

  // Keep part before hyphen1
  memcpy(temp, src, len);
  temp[hyphen1_offset] = '\0';
  match = BinarySearch(temp, 0, kCodeToULScriptSize, kCodeToULScript);
  if (match >= 0) {return MakeULScr(kCodeToULScript[match].i);}    // aa

  return ULScript_Latin;
}

// Map script into Latin, Cyrillic, Arabic, Other
int LScript4(ULScript ulscript) {
  if (ulscript == ULScript_Latin) {return 0;}
  if (ulscript == ULScript_Cyrillic) {return 1;}
  if (ulscript == ULScript_Arabic) {return 2;}
  return 3;
}

}  // namespace CLD2

