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

#ifndef I18N_ENCODINGS_CLD2_PUBLIC_ENCODINGS_H__
#define I18N_ENCODINGS_CLD2_PUBLIC_ENCODINGS_H__

namespace CLD2 {

enum Encoding {
  ISO_8859_1           =  0,  //   ASCII
  ISO_8859_2           =  1,  //   Latin2
  ISO_8859_3           =  2,  //
  ISO_8859_4           =  3,  //   Latin4
  ISO_8859_5           =  4,  //   ISO-8859-5
  ISO_8859_6           =  5,  //   Arabic
  ISO_8859_7           =  6,  //   Greek
  ISO_8859_8           =  7,  //   Hebrew
  ISO_8859_9           =  8,  //
  ISO_8859_10          =  9,  //
  JAPANESE_EUC_JP      = 10,  //   EUC_JP
  JAPANESE_SHIFT_JIS   = 11,  //   SJS
  JAPANESE_JIS         = 12,  //   JIS
  CHINESE_BIG5         = 13,  //   BIG5
  CHINESE_GB           = 14,  //   GB
  CHINESE_EUC_CN       = 15,  // Misnamed. Should be EUC_TW. Was Basis Tech
                              // CNS11643EUC, before that   EUC-CN(!)
  KOREAN_EUC_KR        = 16,  //   KSC
  UNICODE_UNUSED       = 17,  //   Unicode
  CHINESE_EUC_DEC      = 18,  // Misnamed. Should be EUC_TW. Was
                              // CNS11643EUC, before that   EUC.
  CHINESE_CNS          = 19,  // Misnamed. Should be EUC_TW. Was
                              // CNS11643EUC, before that   CNS.
  CHINESE_BIG5_CP950   = 20,  //   BIG5_CP950
  JAPANESE_CP932       = 21,  //   CP932
  UTF8                 = 22,
  UNKNOWN_ENCODING     = 23,
  ASCII_7BIT           = 24,  // ISO_8859_1 with all characters <= 127.
  RUSSIAN_KOI8_R       = 25,  //   KOI8R
  RUSSIAN_CP1251       = 26,  //   CP1251

  //----------------------------------------------------------
  MSFT_CP1252          = 27,  // 27: CP1252 aka MSFT euro ascii
  RUSSIAN_KOI8_RU      = 28,  // CP21866 aka KOI8-U, used for Ukrainian.
                              // Misnamed, this is _not_ KOI8-RU but KOI8-U.
                              // KOI8-U is used much more often than KOI8-RU.
  MSFT_CP1250          = 29,  // CP1250 aka MSFT eastern european
  ISO_8859_15          = 30,  // aka ISO_8859_0 aka ISO_8859_1 euroized
  //----------------------------------------------------------

  //----------------------------------------------------------
  MSFT_CP1254          = 31,  // used for Turkish
  MSFT_CP1257          = 32,  // used in Baltic countries
  //----------------------------------------------------------

  //----------------------------------------------------------
  //----------------------------------------------------------
  ISO_8859_11          = 33,  // aka TIS-620, used for Thai
  MSFT_CP874           = 34,  // used for Thai
  MSFT_CP1256          = 35,  // used for Arabic

  //----------------------------------------------------------
  MSFT_CP1255          = 36,  // Logical Hebrew Microsoft
  ISO_8859_8_I         = 37,  // Iso Hebrew Logical
  HEBREW_VISUAL        = 38,  // Iso Hebrew Visual
  //----------------------------------------------------------

  //----------------------------------------------------------
  CZECH_CP852          = 39,
  CZECH_CSN_369103     = 40,  // aka ISO_IR_139 aka KOI8_CS
  MSFT_CP1253          = 41,  // used for Greek
  RUSSIAN_CP866        = 42,
  //----------------------------------------------------------

  //----------------------------------------------------------
  // Handled by iconv in glibc
  ISO_8859_13          = 43,
  ISO_2022_KR          = 44,
  GBK                  = 45,
  GB18030              = 46,
  BIG5_HKSCS           = 47,
  ISO_2022_CN          = 48,

  //-----------------------------------------------------------
  // Following 4 encodings are deprecated (font encodings)
  TSCII                = 49,
  TAMIL_MONO           = 50,
  TAMIL_BI             = 51,
  JAGRAN               = 52,


  MACINTOSH_ROMAN      = 53,
  UTF7                 = 54,

  //-----------------------------------------------------------
  // Following 2 encodings are deprecated (font encodings)
  BHASKAR              = 55,  // Indic encoding - Devanagari
  HTCHANAKYA           = 56,  // 56 Indic encoding - Devanagari

  //-----------------------------------------------------------
  UTF16BE              = 57,  // big-endian UTF-16
  UTF16LE              = 58,  // little-endian UTF-16
  UTF32BE              = 59,  // big-endian UTF-32
  UTF32LE              = 60,  // little-endian UTF-32
  //-----------------------------------------------------------

  //-----------------------------------------------------------
  // An encoding that means "This is not text, but it may have some
  // simple ASCII text embedded". Intended input conversion
  // is to keep strings of >=4 seven-bit ASCII characters
  BINARYENC            = 61,
  //-----------------------------------------------------------

  //-----------------------------------------------------------
  // Some Web pages allow a mixture of HZ-GB and GB-2312 by using
  // ~{ ... ~} for 2-byte pairs, and the browsers support this.
  HZ_GB_2312           = 62,
  //-----------------------------------------------------------

  //-----------------------------------------------------------
  // Some external vendors make the common input error of
  // converting MSFT_CP1252 to UTF8 *twice*.
  UTF8UTF8             = 63,
  //-----------------------------------------------------------

  //-----------------------------------------------------------
  // Following 6 encodings are deprecated (font encodings)
  TAM_ELANGO           = 64,  // Elango - Tamil
  TAM_LTTMBARANI       = 65,  // Barani - Tamil
  TAM_SHREE            = 66,  // Shree - Tamil
  TAM_TBOOMIS          = 67,  // TBoomis - Tamil
  TAM_TMNEWS           = 68,  // TMNews - Tamil
  TAM_WEBTAMIL         = 69,  // Webtamil - Tamil
  //-----------------------------------------------------------

  //-----------------------------------------------------------
  // Shift_JIS variants used by Japanese cell phone carriers.
  KDDI_SHIFT_JIS       = 70,
  DOCOMO_SHIFT_JIS     = 71,
  SOFTBANK_SHIFT_JIS   = 72,
  // ISO-2022-JP variants used by KDDI and SoftBank.
  KDDI_ISO_2022_JP     = 73,
  SOFTBANK_ISO_2022_JP = 74,
  //-----------------------------------------------------------

  NUM_ENCODINGS        = 75,  // Always keep this at the end. It is not a
                              // valid Encoding enum, it is only used to
                              // indicate the total number of Encodings.
};

}       // End namespace CLD2

#endif  // I18N_ENCODINGS_CLD2_PUBLIC_ENCODINGS_H__


