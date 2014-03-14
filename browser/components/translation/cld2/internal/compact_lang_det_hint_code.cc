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

#include "compact_lang_det_hint_code.h"

#include <stdlib.h>     // for abs()
#include <stdio.h>      // for sprintf()
#include <string.h>     //
#include "lang_script.h"
#include "port.h"

using namespace std;

namespace CLD2 {

static const int kCLDPriorEncodingWeight = 4;   // 100x more likely
static const int kCLDPriorLanguageWeight = 8;   // 10000x more likely


// Tables to map lang="..." language code lists to actual languages.
// based on scraping and hand-edits, dsites June 2011

// n = f(string, &a) gives list of n<=4 language pairs: primary, secondary

// For close pairs like ms/id, more weight on TLD and lang=
// Alternately, weaker boost but mark others of set as negative;
// makes "neither" an easier result.
// lang=en low weight 4
// tld=lu boost lu maaybe 4. but lang= alwyas overcomes tld and encoding
// (except maybe en)

// TLD to separate, e.g., burundi from rwanda

// Encoding lookup: OneLangProb array
// TLD lookup:   tld OneLangProb pairs


typedef struct {
  const char* const langtag;    // Lowercased, hyphen only lookup key
  const char* const langcode;   // Canonical language codes; two if ambiguous
  OneCLDLangPrior onelangprior1;
  OneCLDLangPrior onelangprior2;
} LangTagLookup;

typedef struct {
  const char* const tld;        // Lowercased, hyphen only lookup key
  OneCLDLangPrior onelangprior1;
  OneCLDLangPrior onelangprior2;
} TLDLookup;


#define W2 (2 << 10)            // 3**2 = 10x more likely
#define W4 (4 << 10)            // 3**4 = 100x more likely
#define W6 (6 << 10)            // 3**6 = 1000x more likely
#define W8 (8 << 10)            // 3**8 = 10K x more likely
#define W10 (10 << 10)          // 3**10 = 100K x more likely
#define W12 (12 << 10)          // 3**12 = 1M x more likely

// TODO: more about ba hr sr sr-ME and sl
// Temporary state of affairs:
//   BOSNIAN CROATIAN MONTENEGRIN SERBIAN detecting just CROATIAN SERBIAN
// Eventually, we want to do all four, but it requires a CLD change to handle
// up to six languages per quadgram.


// Close pairs boost one of pair, demote other.
//   Statistically close pairs:
//   INDONESIAN/MALAY difficult to distinguish -- extra word-based lookups used
//
//   INDONESIAN MALAY coef=0.4698        Problematic w/o extra words
//   TIBETAN DZONGKHA coef=0.4571
//   CZECH SLOVAK coef=0.4273
//   NORWEGIAN NORWEGIAN_N coef=0.4182
//
//   HINDI MARATHI coef=0.3795
//   ZULU XHOSA coef=0.3716
//
//   DANISH NORWEGIAN coef=0.3672        Usually OK
//   BIHARI HINDI coef=0.3668            Usually OK
//   ICELANDIC FAROESE coef=0.3519       Usually OK

//
// Table to look up lang= tags longer than three characters
// Overrides table below, which is truncated at first hyphen
// In alphabetical order for binary search
static const int kCLDTable1Size = 213;
static const LangTagLookup kCLDLangTagsHintTable1[kCLDTable1Size] = {
  {"abkhazian", "ab", ABKHAZIAN + W10, 0},
  {"afar", "aa", AFAR + W10, 0},
  {"afrikaans", "af", AFRIKAANS + W10, 0},
  {"akan", "ak", AKAN + W10, 0},
  {"albanian", "sq", ALBANIAN + W10, 0},
  {"am-am", "hy", ARMENIAN + W10, 0},        // 1:2 Armenian, not ambiguous
  {"amharic", "am", AMHARIC + W10, 0},
  {"arabic", "ar", ARABIC + W10, 0},
  {"argentina", "es", SPANISH + W10, 0},
  {"armenian", "hy", ARMENIAN + W10, 0},
  {"assamese", "as", ASSAMESE + W10, 0},
  {"aymara", "ay", AYMARA + W10, 0},
  {"azerbaijani", "az", AZERBAIJANI + W10, 0},

  {"bangla", "bn", BENGALI + W10, 0},
  {"bashkir", "ba", BASHKIR + W10, 0},
  {"basque", "eu", BASQUE + W10, 0},
  {"belarusian", "be", BELARUSIAN + W10, 0},
  {"bengali", "bn", BENGALI + W10, 0},
  {"bihari", "bh", BIHARI + W10, HINDI - W4},
  {"bislama", "bi", BISLAMA + W10, 0},
  {"bosnian", "bs", BOSNIAN + W10, 0},      // Bosnian => Bosnian
  {"br-br", "pt", PORTUGUESE + W10, 0},     // 1:2 Portuguese, not ambiguous
  {"br-fr", "br", BRETON + W10, 0},         // 1:2 Breton, not ambiguous
  {"breton", "br", BRETON + W10, 0},
  {"bulgarian", "bg", BULGARIAN + W10, 0},
  {"burmese", "my", BURMESE + W10, 0},      // Myanmar

  {"catalan", "ca", CATALAN + W10, 0},
  {"cherokee", "chr", CHEROKEE + W10, 0},
  {"chichewa", "ny", NYANJA + W10, 0},

  {"chinese", "zh", CHINESE + W10, 0},
  {"chinese-t", "zhT", CHINESE_T + W10, 0},
  {"chineset", "zhT", CHINESE_T + W10, 0},
  {"corsican", "co", CORSICAN + W10, 0},
  {"cpf-hat", "ht", HAITIAN_CREOLE + W10, 0}, // Creole, French-based
  {"croatian", "hr", CROATIAN + W10, 0},
  {"czech", "cs", CZECH + W10, SLOVAK - W4},

  {"danish", "da", DANISH + W10, NORWEGIAN - W4},
  {"deutsch", "de", GERMAN + W10, 0},
  {"dhivehi", "dv", DHIVEHI + W10, 0},
  {"dutch", "nl", DUTCH + W10, 0},
  {"dzongkha", "dz", DZONGKHA + W10,  TIBETAN - W4},

  {"ell-gr", "el", GREEK + W10, 0},
  {"english", "en", ENGLISH + W4, 0},
  {"esperanto", "eo", ESPERANTO + W10, 0},
  {"estonian", "et", ESTONIAN + W10, 0},
  {"euc-jp", "ja", JAPANESE + W10, 0},       // Japanese encoding
  {"euc-kr", "ko", KOREAN + W10, 0},         // Korean encoding

  {"faroese", "fo", FAROESE + W10, ICELANDIC - W4},
  {"fijian", "fj", FIJIAN + W10, 0},
  {"finnish", "fi", FINNISH + W10, 0},
  {"fran", "fr", FRENCH + W10, 0},            // Truncated at non-ASCII
  {"francais", "fr", FRENCH + W10, 0},
  {"french", "fr", FRENCH + W10, 0},
  {"frisian", "fy", FRISIAN + W10, 0},

  {"ga-es", "gl", GALICIAN + W10, 0},         // 1:2 Galician, not ambiguous
  {"galician", "gl", GALICIAN + W10, 0},
  {"ganda", "lg", GANDA + W10, 0},
  {"georgian", "ka", GEORGIAN + W10, 0},
  {"german", "de", GERMAN + W10, 0},
  {"greek", "el", GREEK + W10, 0},
  {"greenlandic", "kl", GREENLANDIC + W10, 0},
  {"guarani", "gn", GUARANI + W10, 0},
  {"gujarati", "gu", GUJARATI + W10, 0},

  {"haitian_creole", "ht", HAITIAN_CREOLE + W10, 0},
  {"hausa", "ha", HAUSA + W10, 0},
  {"hawaiian", "haw", HAWAIIAN + W10, 0},
  {"hebrew", "iw", HEBREW + W10, 0},
  {"hindi", "hi", HINDI + W10, MARATHI - W4},
  {"hn-in", "hi", HINDI + W10, MARATHI - W4},
  {"hungarian", "hu", HUNGARIAN + W10, 0},

  {"icelandic", "is", ICELANDIC + W10, FAROESE - W4},
  {"igbo", "ig", IGBO + W10, 0},
  {"indonesian", "id", INDONESIAN + W10, MALAY - W4},
  {"interlingua", "ia", INTERLINGUA + W10, 0},
  {"interlingue", "ie", INTERLINGUE + W10, 0},
  // 1:2 iu-Cans ik-Latn
  {"inuktitut", "iu,ik", INUKTITUT + W10, INUPIAK + W10}, // 1:2
  {"inupiak", "ik,iu", INUPIAK + W10, INUKTITUT + W10},   // 1:2
  {"ir-ie", "ga", IRISH + W10, 0},          // Irish
  {"irish", "ga", IRISH + W10, 0},
  {"italian", "it", ITALIAN + W10, 0},

  {"ja-euc", "ja", JAPANESE + W10, 0},      // Japanese encoding
  {"jan-jp", "ja", JAPANESE + W10, 0},      // Japanese encoding
  {"japanese", "ja", JAPANESE + W10, 0},
  {"javanese", "jw", JAVANESE + W10, 0},

  {"kannada", "kn", KANNADA + W10, 0},
  {"kashmiri", "ks", KASHMIRI + W10, 0},
  {"kazakh", "kk", KAZAKH + W10, 0},
  {"khasi", "kha", KHASI + W10, 0},
  {"khmer", "km", KHMER + W10, 0},
  {"kinyarwanda", "rw", KINYARWANDA + W10, 0},
  {"klingon", "tlh", X_KLINGON + W10, 0},
  {"korean", "ko", KOREAN + W10, 0},
  {"kurdish", "ku", KURDISH + W10, 0},
  {"kyrgyz", "ky", KYRGYZ + W10, 0},

  {"laothian", "lo", LAOTHIAN + W10, 0},
  {"latin", "la", LATIN + W10, 0},
  {"latvian", "lv", LATVIAN + W10, 0},
  {"limbu", "sit", LIMBU + W10, 0},
  {"lingala", "ln", LINGALA + W10, 0},
  {"lithuanian", "lt", LITHUANIAN + W10, 0},
  {"luxembourgish", "lb", LUXEMBOURGISH + W10, 0},

  {"macedonian", "mk", MACEDONIAN + W10, 0},
  {"malagasy", "mg", MALAGASY + W10, 0},
  {"malay", "ms", MALAY + W10, INDONESIAN - W4},
  {"malayalam", "ml", MALAYALAM + W10, 0},
  {"maltese", "mt", MALTESE + W10, 0},
  {"manx", "gv", MANX + W10, 0},
  {"maori", "mi", MAORI + W10, 0},
  {"marathi", "mr", MARATHI + W10, HINDI - W4},
  {"mauritian_creole", "mfe", MAURITIAN_CREOLE + W10, 0},
  {"moldavian", "mo", ROMANIAN + W10, 0},
  {"mongolian", "mn", MONGOLIAN + W10, 0},
  {"montenegrin", "sr-me", MONTENEGRIN + W10, 0},
  {"myanmar", "my", BURMESE + W10, 0},      // Myanmar
  {"nauru", "na", NAURU + W10, 0},
  {"ndebele", "nr", NDEBELE + W10, 0},
  {"nepali", "ne", NEPALI + W10, 0},
  {"no-bok", "no", NORWEGIAN + W10, NORWEGIAN_N - W4},       // Bokmaal
  {"no-bokmaal", "no", NORWEGIAN + W10, NORWEGIAN_N - W4},
  {"no-nb", "no", NORWEGIAN + W10, NORWEGIAN_N - W4},        // Bokmaal
  {"no-no", "no", NORWEGIAN + W10, NORWEGIAN_N - W4},
  {"no-nyn", "nn", NORWEGIAN_N + W10, NORWEGIAN - W4},       // Nynorsk
  {"no-nynorsk", "nn", NORWEGIAN_N + W10, NORWEGIAN - W4},
  {"norwegian", "no", NORWEGIAN + W10, NORWEGIAN_N - W4},
  {"norwegian_n", "nn", NORWEGIAN_N + W10, NORWEGIAN - W4},
  {"nyanja", "ny", NYANJA + W10, 0},

  {"occitan", "oc", OCCITAN + W10, 0},
  {"oriya", "or", ORIYA + W10, 0},
  {"oromo", "om", OROMO + W10, 0},
  {"parsi", "fa", PERSIAN + W10, 0},

  {"pashto", "ps", PASHTO + W10, 0},
  {"pedi", "nso", PEDI + W10, 0},
  {"persian", "fa", PERSIAN + W10, 0},
  {"polish", "pl", POLISH + W10, 0},
  {"polska", "pl", POLISH + W10, 0},
  {"polski", "pl", POLISH + W10, 0},
  {"portugu", "pt", PORTUGUESE + W10, 0},     // Truncated at non-ASCII
  {"portuguese", "pt", PORTUGUESE + W10, 0},
  {"punjabi", "pa", PUNJABI + W10, 0},

  {"quechua", "qu", QUECHUA + W10, 0},

  {"rhaeto_romance", "rm", RHAETO_ROMANCE + W10, 0},
  {"romanian", "ro", ROMANIAN + W10, 0},
  {"rundi", "rn", RUNDI + W10, 0},
  {"russian", "ru", RUSSIAN + W10, 0},

  {"samoan", "sm", SAMOAN + W10, 0},
  {"sango", "sg", SANGO + W10, 0},
  {"sanskrit", "sa", SANSKRIT + W10, 0},
  {"scots", "sco", SCOTS + W10, ENGLISH - W4},
  {"scots_gaelic", "gd", SCOTS_GAELIC + W10, 0},
  {"serbian", "sr", SERBIAN + W10, 0},
  {"seselwa", "crs", SESELWA + W10, 0},
  {"sesotho", "st", SESOTHO + W10, 0},
  {"shift-jis", "ja", JAPANESE + W10, 0},   // Japanese encoding
  {"shift-js", "ja", JAPANESE + W10, 0},    // Japanese encoding
  {"shona", "sn", SHONA + W10, 0},
  {"si-lk", "si", SINHALESE + W10, 0},      // 1:2 Sri Lanka, not ambiguous
  {"si-si", "sl", SLOVENIAN + W10, 0},      // 1:2 Slovenia, not ambiguous
  {"si-sl", "sl", SLOVENIAN + W10, 0},      // 1:2 Slovenia, not ambiguous
  {"sindhi", "sd", SINDHI + W10, 0},
  {"sinhalese", "si", SINHALESE + W10, 0},
  {"siswant", "ss", SISWANT + W10, 0},
  {"sit-np", "sit", LIMBU + W10, 0},
  {"slovak", "sk", SLOVAK + W10, CZECH - W4},
  {"slovenian", "sl", SLOVENIAN + W10, 0},
  {"somali", "so", SOMALI + W10, 0},
  {"spanish", "es", SPANISH + W10, 0},
  {"sr-me", "sr-me", MONTENEGRIN + W10, 0}, // Montenegrin => Montenegrin
  {"sundanese", "su", SUNDANESE + W10, 0},
  {"suomi", "fi", FINNISH + W10, 0},        // Finnish
  {"swahili", "sw", SWAHILI + W10, 0},
  {"swedish", "sv", SWEDISH + W10, 0},
  {"syriac", "syr", SYRIAC + W10, 0},

  {"tagalog", "tl", TAGALOG + W10, 0},
  {"tajik", "tg", TAJIK + W10, 0},
  {"tamil", "ta", TAMIL + W10, 0},
  {"tatar", "tt", TATAR + W10, 0},
  {"tb-tb", "bo", TIBETAN + W10, DZONGKHA - W4},        // Tibet
  {"tchinese", "zhT", CHINESE_T + W10, 0},
  {"telugu", "te", TELUGU + W10, 0},
  {"thai", "th", THAI + W10, 0},
  {"tibetan", "bo", TIBETAN + W10, DZONGKHA - W4},
  {"tigrinya", "ti", TIGRINYA + W10, 0},
  {"tonga", "to", TONGA + W10, 0},
  {"tsonga", "ts", TSONGA + W10, 0},
  {"tswana", "tn", TSWANA + W10, 0},
  {"tt-ru", "tt", TATAR + W10, 0},
  {"tur-tr", "tr", TURKISH + W10, 0},
  {"turkish", "tr", TURKISH + W10, 0},
  {"turkmen", "tk", TURKMEN + W10, 0},
  {"uighur", "ug", UIGHUR + W10, 0},
  {"ukrainian", "uk", UKRAINIAN + W10, 0},
  {"urdu", "ur", URDU + W10, 0},
  {"uzbek", "uz", UZBEK + W10, 0},

  {"venda", "ve", VENDA + W10, 0},
  {"vietnam", "vi", VIETNAMESE + W10, 0},
  {"vietnamese", "vi", VIETNAMESE + W10, 0},
  {"volapuk", "vo", VOLAPUK + W10, 0},

  {"welsh", "cy", WELSH + W10, 0},
  {"wolof", "wo", WOLOF + W10, 0},

  {"xhosa", "xh", XHOSA + W10, ZULU - W4},

  {"yiddish", "yi", YIDDISH + W10, 0},
  {"yoruba", "yo", YORUBA + W10, 0},

  {"zh-classical", "zhT", CHINESE_T + W10, 0},
  {"zh-cn", "zh", CHINESE + W10, 0},
  {"zh-hans", "zh", CHINESE + W10, 0},
  {"zh-hant", "zhT", CHINESE_T + W10, 0},
  {"zh-hk", "zhT", CHINESE_T + W10, 0},
  {"zh-min-nan", "zhT", CHINESE_T + W10, 0}, // Min Nan => ChineseT
  {"zh-sg", "zhT", CHINESE_T + W10, 0},
  {"zh-tw", "zhT", CHINESE_T + W10, 0},
  {"zh-yue", "zh", CHINESE + W10, 0},       // Yue (Cantonese) => Chinese
  {"zhuang", "za", ZHUANG + W10, 0},
  {"zulu", "zu", ZULU + W10, XHOSA - W4},
};



// Table to look up lang= tags of two/three characters after truncate at hyphen
// In alphabetical order for binary search
static const int kCLDTable2Size = 257;
static const LangTagLookup kCLDLangTagsHintTable2[kCLDTable2Size] = {
  {"aa", "aa", AFAR + W10, 0},
  {"ab", "ab", ABKHAZIAN + W10, 0},
  {"af", "af", AFRIKAANS + W10, 0},
  {"ak", "ak", AKAN + W10, 0},
  {"al", "sq", ALBANIAN + W10, 0},          // Albania
  {"am", "am,hy", AMHARIC + W10, ARMENIAN + W10},  // 1:2 Amharic Armenian
  {"ar", "ar", ARABIC + W10, 0},
  {"ara", "ar", ARABIC + W10, 0},
  {"arm", "hy", ARMENIAN + W10, 0},         // Armenia
  {"arz", "ar", ARABIC + W10, 0},           // Egyptian Arabic
  {"as", "as", ASSAMESE + W10, 0},
  {"at", "de", GERMAN + W10, 0},            // Austria
  {"au", "de", GERMAN + W10, 0},            // Austria
  {"ay", "ay", AYMARA + W10, 0},
  {"az", "az", AZERBAIJANI + W10, 0},
  {"aze", "az", AZERBAIJANI + W10, 0},

  {"ba", "ba,bs", BASHKIR + W10, BOSNIAN + W10},  // 1:2  Bashkir Bosnia
  {"be", "be", BELARUSIAN + W10, 0},
  {"bel", "be", BELARUSIAN + W10, 0},
  {"bg", "bg", BULGARIAN + W10, 0},
  {"bh", "bh", BIHARI + W10, HINDI - W4},
  {"bi", "bi", BISLAMA + W10, 0},
  {"big", "zhT", CHINESE_T + W10, 0},        // Big5 encoding
  {"bm", "ms", MALAY + W10, INDONESIAN - W4},             // Bahasa Malaysia
  {"bn", "bn", BENGALI + W10, 0},
  {"bo", "bo", TIBETAN + W10, DZONGKHA - W4},
  // 1:2 Breton, Brazil country code, both Latn .br TLD enough for pt to win
  {"br", "br,pt", BRETON + W10, PORTUGUESE + W8}, // 1:2 Breton, Brazil
  {"bs", "bs", BOSNIAN + W10, 0},           // Bosnian => Bosnian

  {"ca", "ca", CATALAN + W10, 0},
  {"cat", "ca", CATALAN + W10, 0},
  {"ch", "de,fr", GERMAN + W10, FRENCH + W10},    // 1:2 Switzerland
  {"chn", "zh", CHINESE + W10, 0},
  {"chr", "chr", CHEROKEE + W10, 0},
  {"ckb", "ku", KURDISH + W10, 0},          // Central Kurdish
  {"cn", "zh,zhT", CHINESE + W6, CHINESE_T + W4},   // Ambiguous, so weaker.
                                                // Offset by 2 so that TLD=tw or
                                                // enc=big5 will put zhT ahead
  {"co", "co", CORSICAN + W10, 0},
  {"cro", "hr", CROATIAN + W10, 0},          // Croatia
  {"crs", "crs", SESELWA + W10, 0},
  {"cs", "cs", CZECH + W10, SLOVAK - W4},
  {"ct", "ca", CATALAN + W10, 0},
  {"cy", "cy", WELSH + W10, 0},
  {"cym", "cy", WELSH + W10, 0},
  {"cz", "cs", CZECH + W10, SLOVAK - W4},

  {"da", "da", DANISH + W10, NORWEGIAN - W4},
  {"dan", "da", DANISH + W10, NORWEGIAN - W4},
  {"de", "de", GERMAN + W10, 0},
  {"deu", "de", GERMAN + W10, 0},
  {"div", "dv", DHIVEHI + W10, 0},
  {"dk", "da", DANISH + W10, NORWEGIAN - W4},            // Denmark
  {"dut", "nl", DUTCH + W10, 0},            // Dutch
  {"dv", "dv", DHIVEHI + W10, 0},
  {"dz", "dz", DZONGKHA + W10, TIBETAN - W4},

  {"ee", "et", ESTONIAN + W10, 0},          // Estonia
  {"eg", "ar", ARABIC + W10, 0},            // Egypt
  {"el", "el", GREEK + W10, 0},
  {"en", "en", ENGLISH + W4, 0},
  {"eng", "en", ENGLISH + W4, 0},
  {"eo", "eo", ESPERANTO + W10, 0},
  {"er", "ur", URDU + W10, 0},              // "Erdu"
  {"es", "es", SPANISH + W10, 0},
  {"esp", "es", SPANISH + W10, 0},
  {"est", "et", ESTONIAN + W10, 0},
  {"et", "et", ESTONIAN + W10, 0},
  {"eu", "eu", BASQUE + W10, 0},

  {"fa", "fa", PERSIAN + W10, 0},
  {"far", "fa", PERSIAN + W10, 0},
  {"fi", "fi", FINNISH + W10, 0},
  {"fil", "tl", TAGALOG + W10, 0},          // Philippines
  {"fj", "fj", FIJIAN + W10, 0},
  {"fo", "fo", FAROESE + W10, ICELANDIC - W4},
  {"fr", "fr", FRENCH + W10, 0},
  {"fra", "fr", FRENCH + W10, 0},
  {"fre", "fr", FRENCH + W10, 0},
  {"fy", "fy", FRISIAN + W10, 0},

  {"ga", "ga,gl", IRISH + W10, GALICIAN + W10},       // 1:2 Irish, Galician
  {"gae", "gd,ga", SCOTS_GAELIC + W10, IRISH + W10},  // 1:2 Gaelic, either
  {"gal", "gl", GALICIAN + W10, 0},
  {"gb", "zh", CHINESE + W10, 0},           // GB2312 encoding
  {"gbk", "zh", CHINESE + W10, 0},          // GBK encoding
  {"gd", "gd", SCOTS_GAELIC + W10, 0},
  {"ge", "ka", GEORGIAN + W10, 0},          // Georgia
  {"geo", "ka", GEORGIAN + W10, 0},
  {"ger", "de", GERMAN + W10, 0},
  {"gl", "gl", GALICIAN + W10, 0},          // Also Greenland; hard to confuse
  {"gn", "gn", GUARANI + W10, 0},
  {"gr", "el", GREEK + W10, 0},             // Greece
  {"gu", "gu", GUJARATI + W10, 0},
  {"gv", "gv", MANX + W10, 0},

  {"ha", "ha", HAUSA + W10, 0},
  {"hat", "ht", HAITIAN_CREOLE + W10, 0},   // Haiti
  {"haw", "haw", HAWAIIAN + W10, 0},
  {"hb", "iw", HEBREW + W10, 0},
  {"he", "iw", HEBREW + W10, 0},
  {"heb", "iw", HEBREW + W10, 0},
  {"hi", "hi", HINDI + W10, MARATHI - W4},
  {"hk", "zhT", CHINESE_T + W10, 0},          // Hong Kong
  {"hr", "hr", CROATIAN + W10, 0},
  {"ht", "ht", HAITIAN_CREOLE + W10, 0},
  {"hu", "hu", HUNGARIAN + W10, 0},
  {"hun", "hu", HUNGARIAN + W10, 0},
  {"hy", "hy", ARMENIAN + W10, 0},

  {"ia", "ia", INTERLINGUA + W10, 0},
  {"ice", "is", ICELANDIC + W10, FAROESE - W4},        // Iceland
  {"id", "id", INDONESIAN + W10, MALAY - W4},
  {"ids", "id", INDONESIAN + W10, MALAY - W4},
  {"ie", "ie", INTERLINGUE + W10, 0},
  {"ig", "ig", IGBO + W10, 0},
  // 1:2 iu-Cans ik-Latn
  {"ik", "ik,iu", INUPIAK + W10, INUKTITUT + W10},        // 1:2
  {"in", "id", INDONESIAN + W10, MALAY - W4},
  {"ind", "id", INDONESIAN + W10, MALAY - W4},       // Indonesia
  {"inu", "iu,ik", INUKTITUT + W10, INUPIAK + W10},       // 1:2
  {"is", "is", ICELANDIC + W10, FAROESE - W4},
  {"it", "it", ITALIAN + W10, 0},
  {"ita", "it", ITALIAN + W10, 0},
  {"iu", "iu,ik", INUKTITUT + W10, INUPIAK + W10},        // 1:2
  {"iw", "iw", HEBREW + W10, 0},

  {"ja", "ja", JAPANESE + W10, 0},
  {"jp", "ja", JAPANESE + W10, 0},          // Japan
  {"jpn", "ja", JAPANESE + W10, 0},
  {"jv", "jw", JAVANESE + W10, 0},
  {"jw", "jw", JAVANESE + W10, 0},

  {"ka", "ka", GEORGIAN + W10, 0},
  {"kc", "qu", QUECHUA + W10, 0},           // (K)Quechua
  {"kg", "ky", KYRGYZ + W10, 0},            // Kyrgyzstan
  {"kh", "km", KHMER + W10, 0},             // Country code Khmer (Cambodia)
  {"kha", "kha", KHASI + W10, 0},
  {"kk", "kk", KAZAKH + W10, 0},            // Kazakh
  {"kl", "kl", GREENLANDIC + W10, 0},
  {"km", "km", KHMER + W10, 0},
  {"kn", "kn", KANNADA + W10, 0},
  {"ko", "ko", KOREAN + W10, 0},
  {"kor", "ko", KOREAN + W10, 0},
  {"kr", "ko", KOREAN + W10, 0},            // Country code Korea
  {"ks", "ks", KASHMIRI + W10, 0},
  {"ksc", "ko", KOREAN + W10, 0},           // KSC encoding
  {"ku", "ku", KURDISH + W10, 0},
  {"ky", "ky", KYRGYZ + W10, 0},
  {"kz", "kk", KAZAKH + W10, 0},            // Kazakhstan
  {"la", "la", LATIN + W10, 0},
  {"lao", "lo", LAOTHIAN + W10, 0},         // Laos

  {"lb", "lb", LUXEMBOURGISH + W10, 0},
  {"lg", "lg", GANDA + W10, 0},
  {"lit", "lt", LITHUANIAN + W10, 0},
  {"ln", "ln", LINGALA + W10, 0},
  {"lo", "lo", LAOTHIAN + W10, 0},
  {"lt", "lt", LITHUANIAN + W10, 0},
  {"ltu", "lt", LITHUANIAN + W10, 0},
  {"lv", "lv", LATVIAN + W10, 0},

  {"mfe", "mfe", MAURITIAN_CREOLE + W10, 0},
  {"mg", "mg", MALAGASY + W10, 0},
  {"mi", "mi", MAORI + W10, 0},
  {"mk", "mk", MACEDONIAN + W10, 0},
  {"ml", "ml", MALAYALAM + W10, 0},
  {"mn", "mn", MONGOLIAN + W10, 0},
  {"mo", "mo", ROMANIAN + W10, 0},
  {"mon", "mn", MONGOLIAN + W10, 0},        // Mongolian
  {"mr", "mr", MARATHI + W10, HINDI - W4},
  {"ms", "ms", MALAY + W10, INDONESIAN - W4},
  {"mt", "mt", MALTESE + W10, 0},
  {"mx", "es", SPANISH + W10, 0},           // Mexico
  {"my", "my,ms", BURMESE + W10, MALAY + W10}, // Myanmar, Malaysia

  {"na", "na", NAURU + W10, 0},
  {"nb", "no", NORWEGIAN + W10, NORWEGIAN_N - W4},
  {"ne", "ne", NEPALI + W10, 0},
  {"nl", "nl", DUTCH + W10, 0},
  {"nn", "nn", NORWEGIAN_N + W10, NORWEGIAN - W4},
  {"no", "no", NORWEGIAN + W10, NORWEGIAN_N - W4},
  {"nr", "nr", NDEBELE + W10, 0},
  {"nso", "nso", PEDI + W10, 0},
  {"ny", "ny", NYANJA + W10, 0},

  {"oc", "oc", OCCITAN + W10, 0},
  {"om", "om", OROMO + W10, 0},
  {"or", "or", ORIYA + W10, 0},

  {"pa", "pa,ps", PUNJABI + W10, PASHTO + W10},   // 1:2 pa-Guru ps-Arab
  {"per", "fa", PERSIAN + W10, 0},
  {"ph", "tl", TAGALOG + W10, 0},           // Philippines
  {"pk", "ur", URDU + W10, 0},              // Pakistan
  {"pl", "pl", POLISH + W10, 0},
  {"pnb", "pa", PUNJABI + W10, 0},          // Western Punjabi
  {"pol", "pl", POLISH + W10, 0},
  {"por", "pt", PORTUGUESE + W10, 0},
  {"ps", "ps", PASHTO + W10, 0},
  {"pt", "pt", PORTUGUESE + W10, 0},
  {"ptg", "pt", PORTUGUESE + W10, 0},
  {"qc", "fr", FRENCH + W10, 0},            // Quebec "country" code
  {"qu", "qu", QUECHUA + W10, 0},

  {"rm", "rm", RHAETO_ROMANCE + W10, 0},
  {"rn", "rn", RUNDI + W10, 0},
  {"ro", "ro", ROMANIAN + W10, 0},
  {"rs", "sr", SERBIAN + W10, 0},           // Serbia country code
  {"ru", "ru", RUSSIAN + W10, 0},
  {"rus", "ru", RUSSIAN + W10, 0},
  {"rw", "rw", KINYARWANDA + W10, 0},

  {"sa", "sa", SANSKRIT + W10, 0},
  {"sco", "sco", SCOTS + W10, ENGLISH - W4},
  {"sd", "sd", SINDHI + W10, 0},
  {"se", "sv", SWEDISH + W10, 0},
  {"sg", "sg", SANGO + W10, 0},
  {"si", "si,sl", SINHALESE + W10, SLOVENIAN + W10},  // 1:2 Sinhalese, Slovinia
  {"sk", "sk", SLOVAK + W10, CZECH - W4},
  {"sl", "sl", SLOVENIAN + W10, 0},
  {"slo", "sl", SLOVENIAN + W10, 0},
  {"sm", "sm", SAMOAN + W10, 0},
  {"sn", "sn", SHONA + W10, 0},
  {"so", "so", SOMALI + W10, 0},
  {"sp", "es", SPANISH + W10, 0},
  {"sq", "sq", ALBANIAN + W10, 0},
  {"sr", "sr", SERBIAN + W10, 0},
  {"srb", "sr", SERBIAN + W10, 0},
  {"srl", "sr", SERBIAN + W10, 0},          // Serbian Latin
  {"srp", "sr", SERBIAN + W10, 0},
  {"ss", "ss", SISWANT + W10, 0},
  {"st", "st", SESOTHO + W10, 0},
  {"su", "su", SUNDANESE + W10, 0},
  {"sv", "sv", SWEDISH + W10, 0},
  {"sve", "sv", SWEDISH + W10, 0},
  {"sw", "sw", SWAHILI + W10, 0},
  {"swe", "sv", SWEDISH + W10, 0},
  {"sy", "syr", SYRIAC + W10, 0},
  {"syr", "syr", SYRIAC + W10, 0},

  {"ta", "ta", TAMIL + W10, 0},
  {"te", "te", TELUGU + W10, 0},
  {"tg", "tg", TAJIK + W10, 0},
  {"th", "th", THAI + W10, 0},
  {"ti", "ti,bo", TIGRINYA + W10, TIBETAN + W10},    // 1:2 Tigrinya, Tibet
  {"tj", "tg", TAJIK + W10, 0},             // Tajikistan
  {"tk", "tk", TURKMEN + W10, 0},
  {"tl", "tl", TAGALOG + W10, 0},
  {"tlh", "tlh", X_KLINGON + W10, 0},
  {"tn", "tn", TSWANA + W10, 0},
  {"to", "to", TONGA + W10, 0},
  {"tr", "tr", TURKISH + W10, 0},
  {"ts", "ts", TSONGA + W10, 0},
  {"tt", "tt", TATAR + W10, 0},
  {"tw", "ak,zhT", AKAN + W10, CHINESE_T + W10},   // 1:2 Twi => Akan, Taiwan
  {"twi", "ak", AKAN + W10, 0},             // Twi => Akan

  {"ua", "uk", UKRAINIAN + W10, 0},         // Ukraine
  {"ug", "ug", UIGHUR + W10, 0},
  {"uk", "uk", UKRAINIAN + W10, 0},
  {"ur", "ur", URDU + W10, 0},
  {"uz", "uz", UZBEK + W10, 0},

  {"va", "ca", CATALAN + W10, 0},           // Valencia => Catalan
  {"val", "ca", CATALAN + W10, 0},          // Valencia => Catalan
  {"ve", "ve", VENDA + W10, 0},
  {"vi", "vi", VIETNAMESE + W10, 0},
  {"vie", "vi", VIETNAMESE + W10, 0},
  {"vn", "vi", VIETNAMESE + W10, 0},
  {"vo", "vo", VOLAPUK + W10, 0},

  {"wo", "wo", WOLOF + W10, 0},

  {"xh", "xh", XHOSA + W10, ZULU - W4},
  {"xho", "xh", XHOSA + W10, ZULU - W4},

  {"yi", "yi", YIDDISH + W10, 0},
  {"yo", "yo", YORUBA + W10, 0},

  {"za", "za", ZHUANG + W10, 0},
  {"zh", "zh", CHINESE + W10, 0},
  {"zht", "zhT", CHINESE_T + W10, 0},
  {"zu", "zu", ZULU + W10, XHOSA - W4},
};


// Possibly map to tl:
// -LangTags tl-Latn /7val.com/ ,bcl 2 Central Bicolano
// -LangTags tl-Latn /7val.com/ ,ceb 6 Cebuano
// -LangTags tl-Latn /7val.com/ ,war 1 Waray



// Table to look up country TLD (no general TLD)
// In alphabetical order for binary search
static const int kCLDTable3Size = 181;
static const TLDLookup kCLDTLDHintTable[kCLDTable3Size] = {
  {"ac", JAPANESE + W2, 0},
  {"ad", CATALAN + W4, 0},
  {"ae", ARABIC + W4, 0},
  {"af", PASHTO + W4, PERSIAN + W4},
  {"ag", GERMAN + W2, 0},                // meager
  // {"ai", 0, 0},                          // meager
  {"al", ALBANIAN + W4, 0},
  {"am", ARMENIAN + W4, 0},
  {"an", DUTCH + W4, 0},                 // meager
  {"ao", PORTUGUESE + W4, 0},
  // {"aq", 0, 0},                          // meager
  {"ar", SPANISH + W4, 0},
  // {"as", 0, 0},
  {"at", GERMAN + W4, 0},
  {"au", ENGLISH + W2, 0},
  {"aw", DUTCH + W4, 0},
  {"ax", SWEDISH + W4, 0},
  {"az", AZERBAIJANI + W4, 0},

  {"ba", BOSNIAN + W8, CROATIAN - W4},
  // {"bb", 0, 0},
  {"bd", BENGALI + W4, 0},
  {"be", DUTCH + W4, FRENCH + W4},
  {"bf", FRENCH + W4, 0},
  {"bg", BULGARIAN + W4, 0},
  {"bh", ARABIC + W4, 0},
  {"bi", RUNDI + W4, FRENCH + W4},
  {"bj", FRENCH + W4, 0},
  {"bm", ENGLISH + W2, 0},
  {"bn", MALAY + W4, INDONESIAN - W4},
  {"bo", SPANISH + W4, AYMARA + W2},   // and GUARANI QUECHUA
  {"br", PORTUGUESE + W4, 0},
  // {"bs", 0, 0},
  {"bt", DZONGKHA + W10, TIBETAN - W10},      // Strong presumption of Dzongha
  {"bw", TSWANA + W4, 0},
  {"by", BELARUSIAN + W4, 0},
  // {"bz", 0, 0},

  {"ca", FRENCH + W4, ENGLISH + W2},
  {"cat", CATALAN + W4, 0},
  {"cc", 0, 0},
  {"cd", FRENCH + W4, 0},
  {"cf", FRENCH + W4, 0},
  {"cg", FRENCH + W4, 0},
  {"ch", GERMAN + W4, FRENCH + W4},
  {"ci", FRENCH + W4, 0},
  // {"ck", 0, 0},
  {"cl", SPANISH + W4, 0},
  {"cm", FRENCH + W4, 0},
  {"cn", CHINESE + W4, 0},
  {"co", SPANISH + W4, 0},
  {"cr", SPANISH + W4, 0},
  {"cu", SPANISH + W4, 0},
  {"cv", PORTUGUESE + W4, 0},
  // {"cx", 0, 0},
  {"cy", GREEK + W4, TURKISH + W4},
  {"cz", CZECH + W4, SLOVAK - W4},

  {"de", GERMAN + W4, 0},
  {"dj", 0, 0},
  {"dk", DANISH + W4, NORWEGIAN - W4},
  {"dm", 0, 0},
  {"do", SPANISH + W4, 0},
  {"dz", FRENCH + W4, ARABIC + W4},

  {"ec", SPANISH + W4, 0},
  {"ee", ESTONIAN + W4, 0},
  {"eg", ARABIC + W4, 0},
  {"er", AFAR + W4, 0},
  {"es", SPANISH + W4, 0},
  {"et", AMHARIC + W4, AFAR + W4},

  {"fi", FINNISH + W4, 0},
  {"fj", FIJIAN + W4, 0},
  // {"fk", 0, 0},
  // {"fm", 0, 0},
  {"fo", FAROESE + W4, ICELANDIC - W4},
  {"fr", FRENCH + W4, 0},

  {"ga", FRENCH + W4, 0},
  {"gd", 0, 0},
  {"ge", GEORGIAN + W4, 0},
  {"gf", FRENCH + W4, 0},
  // {"gg", 0, 0},
  // {"gh", 0, 0},
  // {"gi", 0, 0},
  {"gl", GREENLANDIC + W4, DANISH + W4},
  // {"gm", 0, 0},
  {"gn", FRENCH + W4, 0},
  // {"gp", 0, 0},
  // {"gq", 0, 0},
  {"gr", GREEK + W4, 0},
  // {"gs", 0, 0},
  {"gt", SPANISH + W4, 0},
  // {"gu", 0, 0},
  // {"gy", 0, 0},

  {"hk", CHINESE_T + W4, 0},
  // {"hm", 0, 0},
  {"hn", SPANISH + W4, 0},
  {"hr", CROATIAN + W8, BOSNIAN - W4},
  {"ht", HAITIAN_CREOLE + W4, FRENCH + W4},
  {"hu", HUNGARIAN + W4, 0},

  {"id", INDONESIAN + W4, MALAY - W4},
  {"ie", IRISH + W4, 0},
  {"il", HEBREW + W4, 0},
  {"im", MANX + W4, 0},
  // {"in", 0, 0},
  // {"io", 0, 0},
  {"iq", ARABIC + W4, 0},
  {"ir", PERSIAN + W4, 0},
  {"is", ICELANDIC + W4, FAROESE - W4},
  {"it", ITALIAN + W4, 0},

  // {"je", 0, 0},
  // {"jm", 0, 0},
  {"jo", ARABIC + W4, 0},
  {"jp", JAPANESE + W4, 0},

  // {"ke", 0, 0},
  {"kg", KYRGYZ + W4, 0},
  {"kh", KHMER + W4, 0},
  // {"ki", 0, 0},
  {"km", FRENCH + W4, 0},
  // {"kn", 0, 0},
  {"kp", KOREAN + W4, 0},
  {"kr", KOREAN + W4, 0},
  {"kw", ARABIC + W4, 0},
  // {"ky", 0, 0},
  {"kz", KAZAKH + W4, 0},

  {"la", LAOTHIAN + W4, 0},
  {"lb", ARABIC + W4, FRENCH + W4},
  // {"lc", 0, 0},
  {"li", GERMAN + W4, 0},
  {"lk", SINHALESE + W4, 0},
  // {"lr", 0, 0},
  {"ls", SESOTHO + W4, 0},
  {"lt", LITHUANIAN + W4, 0},
  {"lu", LUXEMBOURGISH + W4},
  {"lv", LATVIAN + W4, 0},
  {"ly", ARABIC + W4, 0},

  {"ma", FRENCH + W4, 0},
  {"mc", FRENCH + W4, 0},
  {"md", ROMANIAN + W4, 0},
  {"me", MONTENEGRIN + W8, SERBIAN - W4},
  {"mg", FRENCH + W4, 0},
  {"mk", MACEDONIAN + W4, 0},
  {"ml", FRENCH + W4, 0},
  {"mm", BURMESE + W4, 0},
  {"mn", MONGOLIAN + W4, 0},
  {"mo", CHINESE_T + W4, PORTUGUESE + W4},
  // {"mp", 0, 0},
  {"mq", FRENCH + W4, 0},
  {"mr", FRENCH + W4, ARABIC + W4},
  // {"ms", 0, 0},
  {"mt", MALTESE + W4, 0},
  // {"mu", 0, 0},
  {"mv", DHIVEHI + W4, 0},
  // {"mw", 0, 0},
  {"mx", SPANISH + W4, 0},
  {"my", MALAY + W4, INDONESIAN - W4},
  {"mz", PORTUGUESE + W4, 0},

  {"na", 0, 0},            // Namibia
  {"nc", FRENCH + W4, 0},
  {"ne", FRENCH + W4, 0},
  {"nf", FRENCH + W4, 0},
  // {"ng", 0, 0},
  {"ni", SPANISH + W4, 0},
  {"nl", DUTCH + W4, 0},
  {"no", NORWEGIAN + W4, NORWEGIAN_N + W2},
  {"np", NEPALI + W4, 0},
  {"nr", NAURU + W4, 0},
  {"nu", SWEDISH + W4, 0},
  {"nz", MAORI + W4, ENGLISH + W2},

  {"om", ARABIC + W4, 0},

  {"pa", SPANISH + W4, 0},
  {"pe", SPANISH + W4, QUECHUA + W2},   // also AYMARA
  {"pf", FRENCH + W4, 0},
  // {"pg", 0, 0},
  {"ph", TAGALOG + W4, 0},
  {"pk", URDU + W4, 0},
  {"pl", POLISH + W4, 0},
  // {"pn", 0, 0},
  {"pr", SPANISH + W4, 0},
  {"ps", ARABIC + W4, 0},
  {"pt", PORTUGUESE + W4, 0},
  {"py", SPANISH + W4, GUARANI + W2},

  {"qa", ARABIC + W4, 0},

  {"re", FRENCH + W4, 0},
  {"ro", ROMANIAN + W4, 0},
  {"rs", SERBIAN + W8, MONTENEGRIN - W4},
  {"ru", RUSSIAN + W4, 0},
  {"rw", KINYARWANDA + W4, FRENCH + W2},

  {"sa", ARABIC + W4, 0},
  // {"sb", 0, 0},
  {"sc", SESELWA + W4, 0},
  {"sd", ARABIC + W4, 0},
  {"se", SWEDISH + W4, 0},
  // {"sg", 0, 0},
  // {"sh", 0, 0},
  {"si", SLOVENIAN + W4, 0},
  {"sk", SLOVAK + W4, CZECH - W4},
  // {"sl", 0, 0},
  {"sm", ITALIAN + W4, 0},
  {"sn", FRENCH + W4, 0},
  // {"sr", 0, 0},
  {"ss", ARABIC + W4, 0},     // Presumed South Sudan TLD. dsites 2011.07.07
  // {"st", 0, 0},
  {"su", RUSSIAN + W4, 0},
  {"sv", SPANISH + W4, 0},
  {"sy", ARABIC + W4, 0},
  // {"sz", 0, 0},

  // {"tc", 0, 0},
  {"td", FRENCH + W4, 0},
  // {"tf", 0, 0},
  {"tg", FRENCH + W4, 0},
  {"th", THAI + W4, 0},
                              // Tibet has no country code (see .cn)
  {"tj", TAJIK + W4, 0},
  // {"tk", 0, 0},
  // {"tl", 0, 0},
  {"tm", TURKISH + W4, 0},
  {"tn", FRENCH + W4, ARABIC + W4},
  // {"to", 0, 0},
  {"tp", JAPANESE + W4, 0},
  {"tr", TURKISH + W4, 0},
  // {"tt", 0, 0},
  // {"tv", 0, 0},
  {"tw", CHINESE_T + W4, 0},
  {"tz", SWAHILI + W4, AKAN + W4},

  {"ua", UKRAINIAN + W4, 0},
  {"ug", GANDA + W4, 0},
  {"uk", ENGLISH + W2, 0},
  {"us", ENGLISH + W2, 0},
  {"uy", SPANISH + W4, 0},
  {"uz", UZBEK + W4, 0},

  {"va", ITALIAN + W4, LATIN + W2},
  // {"vc", 0, 0},
  {"ve", SPANISH + W4, 0},
  // {"vg", 0, 0},
  // {"vi", 0, 0},
  {"vn", VIETNAMESE + W4, 0},
  // {"vu", 0, 0},

  {"wf", FRENCH + W4, 0},
  // {"ws", 0, 0},

  {"ye", ARABIC + W4, 0},

  {"za", AFRIKAANS + W4, 0},
  // {"zm", 0, 0},
  // {"zw", 0, 0},
};

#undef W2
#undef W4
#undef W6
#undef W8
#undef W10
#undef W12





inline void SetCLDPriorWeight(int w, OneCLDLangPrior* olp) {
  *olp = (*olp & 0x3ff) + (w << 10);
}
inline void SetCLDPriorLang(Language lang, OneCLDLangPrior* olp) {
  *olp = (*olp & ~0x3ff) + lang;
}

OneCLDLangPrior PackCLDPriorLangWeight(Language lang, int w) {
  return (w << 10) + lang;
}

inline int MaxInt(int a, int b) {
  return (a >= b) ? a : b;
}

// Merge in another language prior, taking max if already there
void MergeCLDLangPriorsMax(OneCLDLangPrior olp, CLDLangPriors* lps) {
  if (olp == 0) {return;}
  Language target_lang = GetCLDPriorLang(olp);
  for (int i = 0; i < lps->n; ++i) {
    if (GetCLDPriorLang(lps->prior[i]) == target_lang) {
      int new_weight = MaxInt(GetCLDPriorWeight(lps->prior[i]),
                              GetCLDPriorWeight(olp));
      SetCLDPriorWeight(new_weight, &lps->prior[i]);
      return;
    }
  }
  // Not found; add it if room
  if (lps->n >= kMaxOneCLDLangPrior) {return;}
  lps->prior[lps->n++] = olp;
}

// Merge in another language prior, boosting 10x if already there
void MergeCLDLangPriorsBoost(OneCLDLangPrior olp, CLDLangPriors* lps) {
  if (olp == 0) {return;}
  Language target_lang = GetCLDPriorLang(olp);
  for (int i = 0; i < lps->n; ++i) {
    if (GetCLDPriorLang(lps->prior[i]) == target_lang) {
      int new_weight = GetCLDPriorWeight(lps->prior[i]) + 2;
      SetCLDPriorWeight(new_weight, &lps->prior[i]);
      return;
    }
  }
  // Not found; add it if room
  if (lps->n >= kMaxOneCLDLangPrior) {return;}
  lps->prior[lps->n++] = olp;
}


// Trim language priors to no more than max_entries, keeping largest abs weights
void TrimCLDLangPriors(int max_entries, CLDLangPriors* lps) {
  if (lps->n <= max_entries) {return;}

  // Insertion sort in-place by abs(weight)
  for (int i = 0; i < lps->n; ++i) {
    OneCLDLangPrior temp_olp = lps->prior[i];
    int w = abs(GetCLDPriorWeight(temp_olp));
    int kk = i;
    for (; kk > 0; --kk) {
      if (abs(GetCLDPriorWeight(lps->prior[kk - 1])) < w) {
        // Move down and continue
        lps->prior[kk] = lps->prior[kk - 1];
      } else {
        // abs(weight[kk - 1]) >= w, time to stop
        break;
      }
    }
    lps->prior[kk] = temp_olp;
  }

  lps->n = max_entries;
}

int CountCommas(const string& langtags) {
  int commas = 0;
  for (int i = 0; i < static_cast<int>(langtags.size()); ++i) {
    if (langtags[i] == ',') {++commas;}
  }
  return commas;
}

// Binary lookup on language tag
const LangTagLookup* DoLangTagLookup(const char* key,
                                     const LangTagLookup* tbl, int tbl_size) {
  // Key is always in range [lo..hi)
  int lo = 0;
  int hi = tbl_size;
  while (lo < hi) {
    int mid = (lo + hi) >> 1;
    int comp = strcmp(tbl[mid].langtag, key);
    if (comp < 0) {
      lo = mid + 1;
    } else if (comp > 0) {
      hi = mid;
    } else {
      return &tbl[mid];
    }
  }
  return NULL;
}

// Binary lookup on tld
const TLDLookup* DoTLDLookup(const char* key,
                             const TLDLookup* tbl, int tbl_size) {
  // Key is always in range [lo..hi)
  int lo = 0;
  int hi = tbl_size;
  while (lo < hi) {
    int mid = (lo + hi) >> 1;
    int comp = strcmp(tbl[mid].tld, key);
    if (comp < 0) {
      lo = mid + 1;
    } else if (comp > 0) {
      hi = mid;
    } else {
      return &tbl[mid];
    }
  }
  return NULL;
}



// Trim language tag string to canonical form for each language
// Input is from GetLangTagsFromHtml(), already lowercased
string TrimCLDLangTagsHint(const string& langtags) {
  string retval;
  if (langtags.empty()) {return retval;}
  int commas = CountCommas(langtags);
  if (commas > 4) {return retval;}       // Ignore if too many language tags

  char temp[20];
  int pos = 0;
  while (pos < static_cast<int>(langtags.size())) {
    int comma = langtags.find(',', pos);
    if (comma == string::npos) {comma = langtags.size();} // fake trailing comma
    int len = comma - pos;
    if (len <= 16) {
      // Short enough to use
      memcpy(temp, &langtags[pos], len);
      temp[len] = '\0';
      const LangTagLookup* entry = DoLangTagLookup(temp,
                                                   kCLDLangTagsHintTable1,
                                                   kCLDTable1Size);
      if (entry != NULL) {
        // First table hit
        retval.append(entry->langcode);     // may be "code1,code2"
        retval.append(1, ',');
      } else {
        // Try second table with language code truncated at first hyphen
        char* hyphen = strchr(temp, '-');
        if (hyphen != NULL) {*hyphen = '\0';}
        len = strlen(temp);
        if (len <= 3) {                 // Short enough to use
          entry = DoLangTagLookup(temp,
                                  kCLDLangTagsHintTable2,
                                  kCLDTable2Size);
          if (entry != NULL) {
            // Second table hit
            retval.append(entry->langcode);     // may be "code1,code2"
            retval.append(1, ',');
          }
        }
      }
    }
    pos = comma + 1;
  }

  // Remove trainling comma, if any
  if (!retval.empty()) {retval.resize(retval.size() - 1);}
  return retval;
}



//==============================================================================

// Little state machine to scan insides of language attribute quoted-string.
// Each language code is lowercased and copied to the output string. Underscore
// is mapped to minus. Space, tab, and comma are all mapped to comma, and
// multiple consecutive commas are removed.
// Each language code in the output list will be followed by a single comma.

// There are three states, and we start in state 1:
// State 0: After a letter.
//  Copy all letters/minus[0], copy comma[1]; all others copy comma and skip [2]
// State 1: Just after a comma.
//  Copy letter [0], Ignore subsequent commas[1]. minus and all others skip [2]
// State 2: Skipping.
//  All characters except comma skip and stay in [2]. comma goes to [1]

// The thing that is copied is kLangCodeRemap[c] when going to state 0,
// and always comma when going to state 1 or 2. The design depends on copying
// a comma at the *beginning* of skipping, and in state 2 never doing a copy.

// We pack all this into 8 bits:
//    +--+---+---+
//    |78|654|321|
//    +--+---+---+
//
// Shift byte right by 3*state, giving [0] 321, [1] 654, [2] .78
// where . is always zero
// Of these 3 bits, low two are next state ss, high bit is copy bit C.
// If C=1 and ss == 0, copy kLangCodeRemap[c], else copy a comma

#define SKIP0 0
#define SKIP1 1
#define SKIP2 2
#define COPY0 4   // copy kLangCodeRemap[c]
#define COPY1 5   // copy ','
#define COPY2 6   // copy ','

// These combined actions pack three states into one byte.
// Ninth bit must be zero, so all state 2 values must be skips.
//              state[2]       state[1]      state[0]
#define LTR   ((SKIP2 << 6) + (COPY0 << 3) + COPY0)
#define MINUS ((SKIP2 << 6) + (COPY2 << 3) + COPY0)
#define COMMA ((SKIP1 << 6) + (SKIP1 << 3) + COPY1)
#define Bad   ((SKIP2 << 6) + (COPY2 << 3) + COPY2)

// Treat as letter: a-z,  A-Z
// Treat as minus:  2D minus,  5F underscore
// Treat as comma:  09 tab,  20 space,  2C comma

static const unsigned char kLangCodeAction[256] = {
  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,  Bad,COMMA,Bad,Bad,Bad,Bad,Bad,Bad,
  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,
  COMMA,Bad,Bad,Bad,Bad,Bad,Bad,Bad,  Bad,Bad,Bad,Bad,COMMA,MINUS,Bad,Bad,
  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,

  Bad,LTR,LTR,LTR,LTR,LTR,LTR,LTR,  LTR,LTR,LTR,LTR,LTR,LTR,LTR,LTR,
  LTR,LTR,LTR,LTR,LTR,LTR,LTR,LTR,  LTR,LTR,LTR,Bad,Bad,Bad,Bad,MINUS,
  Bad,LTR,LTR,LTR,LTR,LTR,LTR,LTR,  LTR,LTR,LTR,LTR,LTR,LTR,LTR,LTR,
  LTR,LTR,LTR,LTR,LTR,LTR,LTR,LTR,  LTR,LTR,LTR,Bad,Bad,Bad,Bad,Bad,

  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,
  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,
  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,
  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,

  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,
  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,
  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,
  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,  Bad,Bad,Bad,Bad,Bad,Bad,Bad,Bad,
};

// This does lowercasing, maps underscore to minus, and maps tab/space to comma
static const unsigned char kLangCodeRemap[256] = {
  0,0,0,0,0,0,0,0,  0,',',0,0,0,0,0,0,          // 09 tab
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  ',',0,0,0,0,0,0,0,  0,0,0,0,',','-',0,0,      // 20 space 2C comma 2D minus
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,

    0,'a','b','c','d','e','f','g',  'h','i','j','k','l','m','n','o',
  'p','q','r','s','t','u','v','w',  'x','y','z',0,0,0,0,'-',  // 5F underscore
    0,'a','b','c','d','e','f','g',  'h','i','j','k','l','m','n','o',
  'p','q','r','s','t','u','v','w',  'x','y','z',0,0,0,0,0,

  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,

  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
};

#undef LTR
#undef MINUS
#undef COMMA
#undef Bad

#undef SKIP0
#undef SKIP1
#undef SKIP2
#undef COPY0
#undef COPY1
#undef COPY2


// Find opening '<' for HTML tag
// Note: this is all somewhat insensitive to mismatched quotes
int32 FindTagStart(const char* utf8_body, int32 pos, int32 max_pos) {
  int i = pos;
  // Advance i by 4 if none of the next 4 bytes are '<'
  for (i = pos; i < (max_pos - 3); i += 4) {
    // Fast check for any <
    const char* p = &utf8_body[i];
    uint32 s0123 = UNALIGNED_LOAD32(p);
    uint32 temp = s0123 ^ 0x3c3c3c3c;    // <<<<
    if (((temp - 0x01010101) & (~temp & 0x80808080)) != 0) {
      // At least one byte is '<'
      break;
    }
  }
  // Continue, advancing i by 1
  for (; i < max_pos; ++i) {
    if (utf8_body[i] == '<') {return i;}
  }
  return -1;
}


// Find closing '>' for HTML tag. Also stop on < and & (simplistic parsing)
int32 FindTagEnd(const char* utf8_body, int32 pos, int32 max_pos) {
  // Always outside quotes
  for (int i = pos; i < max_pos; ++i) {
    char c = utf8_body[i];
    if (c == '>') {return i;}
    if (c == '<') {return i - 1;}
    if (c == '&') {return i - 1;}
  }
  return -1;              // nothing found
}

// Find opening quote or apostrophe, skipping spaces
// Note: this is all somewhat insensitive to mismatched quotes
int32 FindQuoteStart(const char* utf8_body, int32 pos, int32 max_pos) {
  for (int i = pos; i < max_pos; ++i) {
    char c = utf8_body[i];
    if (c == '"') {return i;}
    if (c == '\'') {return i;}
    if (c != ' ') {return -1;}
  }
  return -1;
}

// Find closing quot/apos. Also stop on = > < and & (simplistic parsing)
int32 FindQuoteEnd(const char* utf8_body, int32 pos, int32 max_pos) {
  // Always outside quotes
  for (int i = pos; i < max_pos; ++i) {
    char c = utf8_body[i];
    if (c == '"') {return i;}
    if (c == '\'') {return i;}
    if (c == '>') {return i - 1;}
    if (c == '=') {return i - 1;}
    if (c == '<') {return i - 1;}
    if (c == '&') {return i - 1;}
  }
  return -1;              // nothing found
}

int32 FindEqualSign(const char* utf8_body, int32 pos, int32 max_pos) {
  // Outside quotes/apostrophes loop
  for (int i = pos; i < max_pos; ++i) {
    char c = utf8_body[i];
    if (c == '=') {       // Found bare equal sign inside tag
      return i;
    } else if (c == '"') {
      // Inside quotes loop
      int j;
      for (j = i + 1; j < max_pos; ++j) {
        if (utf8_body[j] == '"') {
          break;
        } else if (utf8_body[j] == '\\') {
          ++j;
        }
      }
      i = j;
    } else if (c == '\'') {
      // Inside apostrophes loop
      int j;
      for (j = i + 1; j < max_pos; ++j) {
        if (utf8_body[j] == '\'') {
          break;
        } else if (utf8_body[j] == '\\') {
          ++j;
        }
      }
      i = j;
    }

  }
  return -1;              // nothing found
}

// Scan backwards for case-insensitive string s in [min_pos..pos)
// Bytes of s must already be lowercase, i.e. in [20..3f] or [60..7f]
// Cheap lowercase. Control codes will masquerade as 20..3f
bool FindBefore(const char* utf8_body,
                 int32 min_pos, int32 pos, const char* s) {
  int len = strlen(s);
  if ((pos - min_pos) < len) {return false;}     // Too small to fit s

  // Skip trailing spaces
  int i = pos;
  while ((i > (min_pos + len)) && (utf8_body[i - 1] == ' ')) {--i;}
  i -= len;
  if (i < min_pos) {return false;}   // pos - min_pos < len, so s can't be found

  const char* p = &utf8_body[i];
  for (int j = 0; j < len; ++j) {
    if ((p[j] | 0x20) != s[j])  {return false;}    // Unequal byte
  }
  return true;                                     // All bytes equal at i
}

// Scan forwards for case-insensitive string s in [pos..max_pos)
// Bytes of s must already be lowercase, i.e. in [20..3f] or [60..7f]
// Cheap lowercase. Control codes will masquerade as 20..3f
// Allows but does not require quoted/apostrophe string
bool FindAfter(const char* utf8_body,
                 int32 pos, int32 max_pos, const char* s) {
  int len = strlen(s);
  if ((max_pos - pos) < len) {return false;}     // Too small to fit s

  // Skip leading spaces, quote, apostrophe
  int i = pos;
  while (i < (max_pos - len)) {
    unsigned char c = utf8_body[i];
    if ((c == ' ') || (c == '"') || (c == '\'')) {++i;}
    else {break;}
  }

  const char* p = &utf8_body[i];
  for (int j = 0; j < len; ++j) {
    if ((p[j] | 0x20) != s[j])  {return false;}    // Unequal byte
  }
  return true;                                     // All bytes equal
}



// Copy attribute value in [pos..max_pos)
// pos is just after an opening quote/apostrophe and max_pos is the ending one
// String must all be on a single line.
// Return slightly-normalized language list, empty or ending in comma
// Does lowercasing and removes excess punctuation/space
string CopyOneQuotedString(const char* utf8_body,
                         int32 pos, int32 max_pos) {
  string s;
  int state = 1;        // Front is logically just after a comma
  for (int i = pos; i < max_pos; ++i) {
    unsigned char c = utf8_body[i];
    int e = kLangCodeAction[c] >> (3 * state);
    state = e & 3;      // Update to next state
    if ((e & 4) != 0) {
      // Copy a remapped byte if going to state 0, else copy a comma
      if (state == 0) {
        s.append(1, kLangCodeRemap[c]);
      } else {
        s.append(1, ',');
      }
    }
  }

  // Add final comma if needed
  if (state == 0) {
    s.append(1, ',');
  }
  return s;
}

// Find and copy attribute value: quoted string in [pos..max_pos)
// Return slightly-normalized language list, empty or ending in comma
string CopyQuotedString(const char* utf8_body,
                         int32 pos, int32 max_pos) {
  int32 start_quote = FindQuoteStart(utf8_body, pos, max_pos);
  if (start_quote < 0) {return string("");}
  int32 end_quote = FindQuoteEnd(utf8_body, start_quote + 1, max_pos);
  if (end_quote < 0) {return string("");}

  return CopyOneQuotedString(utf8_body, start_quote + 1, end_quote);
}

// Add hints to vector of langpriors
// Input is from GetLangTagsFromHtml(), already lowercased
void SetCLDLangTagsHint(const string& langtags, CLDLangPriors* langpriors) {
  if (langtags.empty()) {return;}
  int commas = CountCommas(langtags);
  if (commas > 4) {return;}       // Ignore if too many language tags

  char temp[20];
  int pos = 0;
  while (pos < static_cast<int>(langtags.size())) {
    int comma = langtags.find(',', pos);
    if (comma == string::npos) {comma = langtags.size();} // fake trailing comma
    int len = comma - pos;
    if (len <= 16) {
      // Short enough to use
      memcpy(temp, &langtags[pos], len);
      temp[len] = '\0';
      const LangTagLookup* entry = DoLangTagLookup(temp,
                                                   kCLDLangTagsHintTable1,
                                                   kCLDTable1Size);
      if (entry != NULL) {
        // First table hit
        MergeCLDLangPriorsMax(entry->onelangprior1, langpriors);
        MergeCLDLangPriorsMax(entry->onelangprior2, langpriors);
      } else {
        // Try second table with language code truncated at first hyphen
        char* hyphen = strchr(temp, '-');
        if (hyphen != NULL) {*hyphen = '\0';}
        len = strlen(temp);
        if (len <= 3) {                 // Short enough to use
          entry = DoLangTagLookup(temp,
                                  kCLDLangTagsHintTable2,
                                  kCLDTable2Size);
          if (entry != NULL) {
            // Second table hit
            MergeCLDLangPriorsMax(entry->onelangprior1, langpriors);
            MergeCLDLangPriorsMax(entry->onelangprior2, langpriors);
          }
        }
      }
    }
    pos = comma + 1;
  }
}

// Add hints to vector of langpriors
// Input is string after HTTP header Content-Language:
void SetCLDContentLangHint(const char* contentlang, CLDLangPriors* langpriors) {
  string langtags = CopyOneQuotedString(contentlang, 0, strlen(contentlang));
  SetCLDLangTagsHint(langtags, langpriors);
}

// Add hints to vector of langpriors
// Input is last element of hostname (no dot), e.g. from GetTLD()
void SetCLDTLDHint(const char* tld, CLDLangPriors* langpriors) {
  int len = strlen(tld);
  if (len > 3) {return;}        // Ignore if more than three letters
  char local_tld[4];
  strncpy(local_tld, tld, 4);
  local_tld[3] = '\0';          // Safety move
  // Lowercase
  for (int i = 0; i < len; ++i) {local_tld[i] |= 0x20;}
  const TLDLookup* entry = DoTLDLookup(local_tld,
                                       kCLDTLDHintTable,
                                       kCLDTable3Size);
  if (entry != NULL) {
    // Table hit
    MergeCLDLangPriorsBoost(entry->onelangprior1, langpriors);
    MergeCLDLangPriorsBoost(entry->onelangprior2, langpriors);
  }
}

// Add hints to vector of langpriors
// Input is from DetectEncoding()
void SetCLDEncodingHint(Encoding enc, CLDLangPriors* langpriors) {
  OneCLDLangPrior olp;
  switch (enc) {
  case CHINESE_GB:
  case GBK:
  case GB18030:
  case ISO_2022_CN:
  case HZ_GB_2312:
    olp = PackCLDPriorLangWeight(CHINESE, kCLDPriorEncodingWeight);
    MergeCLDLangPriorsBoost(olp, langpriors);
    break;
  case CHINESE_BIG5:
  case CHINESE_BIG5_CP950:
  case BIG5_HKSCS:
    olp = PackCLDPriorLangWeight(CHINESE_T, kCLDPriorEncodingWeight);
    MergeCLDLangPriorsBoost(olp, langpriors);
    break;
  case JAPANESE_EUC_JP:
  case JAPANESE_SHIFT_JIS:
  case JAPANESE_CP932:
  case JAPANESE_JIS:          // ISO-2022-JP
    olp = PackCLDPriorLangWeight(JAPANESE, kCLDPriorEncodingWeight);
    MergeCLDLangPriorsBoost(olp, langpriors);
    break;
  case KOREAN_EUC_KR:
  case ISO_2022_KR:
    olp = PackCLDPriorLangWeight(KOREAN, kCLDPriorEncodingWeight);
    MergeCLDLangPriorsBoost(olp, langpriors);
    break;

  default:
    break;
  }
}

// Add hints to vector of langpriors
// Input is from random source
void SetCLDLanguageHint(Language lang, CLDLangPriors* langpriors) {
  OneCLDLangPrior olp = PackCLDPriorLangWeight(lang, kCLDPriorLanguageWeight);
  MergeCLDLangPriorsBoost(olp, langpriors);
}


// Make printable string of priors
string DumpCLDLangPriors(const CLDLangPriors* langpriors) {
  string retval;
  for (int i = 0; i < langpriors->n; ++i) {
    char temp[64];
    sprintf(temp, "%s.%d ",
             LanguageCode(GetCLDPriorLang(langpriors->prior[i])),
             GetCLDPriorWeight(langpriors->prior[i]));
    retval.append(temp);
  }
  return retval;
}




// Look for
//  <html lang="en">
//  <doc xml:lang="en">
//  <html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en-US">
//  <meta http-equiv="content-language" content="en-GB" />
//  <meta name="language" content="Srpski">
//  <meta name="DC.language" scheme="RFCOMMA766" content="en">
//  <SPAN id="msg1" class="info" lang='en'>
//
// Do not trigger on
//  <!-- lang=french ...-->
//  <font lang=postscript ...>
//  <link href="index.fr.html" hreflang="fr-FR" xml:lang="fr-FR" />
//  <META name="Author" lang="fr" content="Arnaud Le Hors">
//
// Stop fairly quickly on mismatched quotes
//
// Allowed language characters
//  a-z A-Z -_ , space\t
// Think about: GB2312, big5, shift-jis, euc-jp, ksc euc-kr
//  zh-hans zh-TW cmn-Hani zh_cn.gb18030_CN zh-min-nan zh-yue
//  de-x-mtfrom-en  zh-tw-x-mtfrom-en  (machine translation)
// GB2312 => gb
// Big5 => big
// zh_CN.gb18030_C => zh-cn
//
// Remove duplicates and extra spaces as we go
// Lowercase as we go.

// Get language tag hints from HTML body
// Normalize: remove spaces and make lowercase comma list

string GetLangTagsFromHtml(const char* utf8_body, int32 utf8_body_len,
                           int32 max_scan_bytes) {
  string retval;
  if (max_scan_bytes > utf8_body_len) {
    max_scan_bytes = utf8_body_len;
  }

  int32 k = 0;
  while (k < max_scan_bytes) {
    int32 start_tag = FindTagStart(utf8_body, k, max_scan_bytes);
    if (start_tag < 0) {break;}
    int32 end_tag = FindTagEnd(utf8_body, start_tag + 1, max_scan_bytes);
    // FindTagEnd exits on < > &
    if (end_tag < 0) {break;}

    // Skip <!--...>
    // Skip <font ...>
    // Skip <script ...>
    // Skip <link ...>
    // Skip <img ...>
    // Skip <a ...>
    if (FindAfter(utf8_body, start_tag + 1, end_tag, "!--") ||
        FindAfter(utf8_body, start_tag + 1, end_tag, "font ") ||
        FindAfter(utf8_body, start_tag + 1, end_tag, "script ") ||
        FindAfter(utf8_body, start_tag + 1, end_tag, "link ") ||
        FindAfter(utf8_body, start_tag + 1, end_tag, "img ") ||
        FindAfter(utf8_body, start_tag + 1, end_tag, "a ")) {
      k = end_tag + 1;
      continue;
    }

    // Remember <meta ...>
    bool in_meta = false;
    if (FindAfter(utf8_body, start_tag + 1, end_tag, "meta ")) {
      in_meta = true;
    }

    // Scan for each equal sign inside tag
    bool content_is_lang = false;
    int32 kk = start_tag + 1;
    int32 equal_sign;
    while ((equal_sign = FindEqualSign(utf8_body, kk, end_tag)) >= 0) {
      // eq exits on < > &

      // Look inside a meta tag
      // <meta ... http-equiv="content-language" ...>
      // <meta ... name="language" ...>
      // <meta ... name="dc.language" ...>
      if (in_meta) {
        if (FindBefore(utf8_body, kk, equal_sign, " http-equiv") &&
            FindAfter(utf8_body, equal_sign + 1, end_tag,
                      "content-language ")) {
          content_is_lang = true;
        } else if (FindBefore(utf8_body, kk, equal_sign, " name") &&
                   (FindAfter(utf8_body, equal_sign + 1, end_tag,
                              "dc.language ") ||
                    FindAfter(utf8_body, equal_sign + 1, end_tag,
                              "language "))) {
          content_is_lang = true;
        }
      }

      // Look inside any tag
      // <meta ... content="lang-list" ...>
      // <... lang="lang-list" ...>
      // <... xml:lang="lang-list" ...>
      if ((content_is_lang && FindBefore(utf8_body, kk, equal_sign,
                                         " content")) ||
          FindBefore(utf8_body, kk, equal_sign, " lang") ||
          FindBefore(utf8_body, kk, equal_sign, ":lang")) {
        string temp = CopyQuotedString(utf8_body, equal_sign + 1, end_tag);

        // Append new lang tag(s) if not a duplicate
        if (!temp.empty() && (retval.find(temp) == string::npos)) {
          retval.append(temp);
        }
      }

      kk = equal_sign + 1;
    }
    k = end_tag + 1;
  }

  // Strip last comma
  if (retval.size() > 1) {
    retval.erase(retval.size() - 1);
  }
  return retval;
}

}       // End namespace CLD2

//==============================================================================


