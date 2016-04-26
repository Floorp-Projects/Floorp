
/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Derived from the Unicode Character Database by genUnicodePropertyData.pl
 *
 * For Unicode terms of use, see http://www.unicode.org/terms_of_use.html
 */

/*
 * Created on Tue Apr 26 07:40:35 2016 from UCD data files with version info:
 *

# Date: 2015-06-16, 20:24:00 GMT [KW]
#
# Unicode Character Database
# Copyright (c) 1991-2015 Unicode, Inc.
# For terms of use, see http://www.unicode.org/terms_of_use.html
#
# For documentation, see the following:
# NamesList.html
# UAX #38, "Unicode Han Database (Unihan)"
# UAX #44, "Unicode Character Database."
#
# The UAXes can be accessed at http://www.unicode.org/versions/Unicode8.0.0/

This directory contains the final data files
for the Unicode Character Database, for Version 8.0.0 of the Unicode
Standard.


# Scripts-8.0.0.txt
# Date: 2015-03-11, 22:29:42 GMT [MD]

# BidiMirroring-8.0.0.txt
# Date: 2015-01-20, 18:30:00 GMT [KW, LI]

# BidiBrackets-8.0.0.txt
# Date: 2015-01-20, 19:00:00 GMT [AG, LI, KW]

# HangulSyllableType-8.0.0.txt
# Date: 2014-12-16, 23:07:45 GMT [MD]

# LineBreak-8.0.0.txt
# Date: 2015-02-13, 09:15:00 GMT [KW, LI]

# File: xidmodifications.txt
# Version: 8.0.0
# Generated: 2015-05-17, 03:09:04 GMT

#
# Unihan_Variants.txt
# Date: 2015-04-30 18:38:20 GMT [JHJ]

# VerticalOrientation-13.txt
# Date: 2014-09-03, 17:30:00 GMT [EM, KI, LI]

 *
 * * * * * This file contains MACHINE-GENERATED DATA, do not edit! * * * * *
 */

#ifndef NS_UNICODE_SCRIPT_CODES
#define NS_UNICODE_SCRIPT_CODES

#pragma pack(1)

#if !ENABLE_INTL_API

struct nsCharProps1 {
  unsigned char mMirrorOffsetIndex:5;
  unsigned char mHangulType:3;
  unsigned char mCombiningClass:8;
};

#endif

#if ENABLE_INTL_API

struct nsCharProps2 {
  unsigned char mPairedBracketType:2;
  unsigned char mVertOrient:2;
  unsigned char mXidmod:4;
};

#endif

#if !ENABLE_INTL_API

struct nsCharProps2 {
  unsigned char mScriptCode:8;
  unsigned char mPairedBracketType:3; // only 2 bits actually needed
  unsigned char mCategory:5;
  unsigned char mBidiCategory:5;
  unsigned char mXidmod:4;
  signed char   mNumericValue:5;
  unsigned char mVertOrient:2;
  unsigned char mLineBreak; // only 6 bits actually needed
};

#endif

#pragma pack()

namespace mozilla {
namespace unicode {
enum class Script {
  COMMON = 0,
  INHERITED = 1,
  ARABIC = 2,
  ARMENIAN = 3,
  BENGALI = 4,
  BOPOMOFO = 5,
  CHEROKEE = 6,
  COPTIC = 7,
  CYRILLIC = 8,
  DESERET = 9,
  DEVANAGARI = 10,
  ETHIOPIC = 11,
  GEORGIAN = 12,
  GOTHIC = 13,
  GREEK = 14,
  GUJARATI = 15,
  GURMUKHI = 16,
  HAN = 17,
  HANGUL = 18,
  HEBREW = 19,
  HIRAGANA = 20,
  KANNADA = 21,
  KATAKANA = 22,
  KHMER = 23,
  LAO = 24,
  LATIN = 25,
  MALAYALAM = 26,
  MONGOLIAN = 27,
  MYANMAR = 28,
  OGHAM = 29,
  OLD_ITALIC = 30,
  ORIYA = 31,
  RUNIC = 32,
  SINHALA = 33,
  SYRIAC = 34,
  TAMIL = 35,
  TELUGU = 36,
  THAANA = 37,
  THAI = 38,
  TIBETAN = 39,
  CANADIAN_ABORIGINAL = 40,
  YI = 41,
  TAGALOG = 42,
  HANUNOO = 43,
  BUHID = 44,
  TAGBANWA = 45,
  BRAILLE = 46,
  CYPRIOT = 47,
  LIMBU = 48,
  LINEAR_B = 49,
  OSMANYA = 50,
  SHAVIAN = 51,
  TAI_LE = 52,
  UGARITIC = 53,
  KATAKANA_OR_HIRAGANA = 54,
  BUGINESE = 55,
  GLAGOLITIC = 56,
  KHAROSHTHI = 57,
  SYLOTI_NAGRI = 58,
  NEW_TAI_LUE = 59,
  TIFINAGH = 60,
  OLD_PERSIAN = 61,
  BALINESE = 62,
  BATAK = 63,
  BLISSYMBOLS = 64,
  BRAHMI = 65,
  CHAM = 66,
  CIRTH = 67,
  OLD_CHURCH_SLAVONIC_CYRILLIC = 68,
  DEMOTIC_EGYPTIAN = 69,
  HIERATIC_EGYPTIAN = 70,
  EGYPTIAN_HIEROGLYPHS = 71,
  KHUTSURI = 72,
  SIMPLIFIED_HAN = 73,
  TRADITIONAL_HAN = 74,
  PAHAWH_HMONG = 75,
  OLD_HUNGARIAN = 76,
  HARAPPAN_INDUS = 77,
  JAVANESE = 78,
  KAYAH_LI = 79,
  LATIN_FRAKTUR = 80,
  LATIN_GAELIC = 81,
  LEPCHA = 82,
  LINEAR_A = 83,
  MANDAIC = 84,
  MAYAN_HIEROGLYPHS = 85,
  MEROITIC_HIEROGLYPHS = 86,
  NKO = 87,
  OLD_TURKIC = 88,
  OLD_PERMIC = 89,
  PHAGS_PA = 90,
  PHOENICIAN = 91,
  MIAO = 92,
  RONGORONGO = 93,
  SARATI = 94,
  ESTRANGELO_SYRIAC = 95,
  WESTERN_SYRIAC = 96,
  EASTERN_SYRIAC = 97,
  TENGWAR = 98,
  VAI = 99,
  VISIBLE_SPEECH = 100,
  CUNEIFORM = 101,
  UNWRITTEN_LANGUAGES = 102,
  UNKNOWN = 103,
  CARIAN = 104,
  JAPANESE = 105,
  TAI_THAM = 106,
  LYCIAN = 107,
  LYDIAN = 108,
  OL_CHIKI = 109,
  REJANG = 110,
  SAURASHTRA = 111,
  SIGNWRITING = 112,
  SUNDANESE = 113,
  MOON = 114,
  MEETEI_MAYEK = 115,
  IMPERIAL_ARAMAIC = 116,
  AVESTAN = 117,
  CHAKMA = 118,
  KOREAN = 119,
  KAITHI = 120,
  MANICHAEAN = 121,
  INSCRIPTIONAL_PAHLAVI = 122,
  PSALTER_PAHLAVI = 123,
  BOOK_PAHLAVI = 124,
  INSCRIPTIONAL_PARTHIAN = 125,
  SAMARITAN = 126,
  TAI_VIET = 127,
  MATHEMATICAL_NOTATION = 128,
  SYMBOLS = 129,
  BAMUM = 130,
  LISU = 131,
  NAKHI_GEBA = 132,
  OLD_SOUTH_ARABIAN = 133,
  BASSA_VAH = 134,
  DUPLOYAN = 135,
  ELBASAN = 136,
  GRANTHA = 137,
  KPELLE = 138,
  LOMA = 139,
  MENDE_KIKAKUI = 140,
  MEROITIC_CURSIVE = 141,
  OLD_NORTH_ARABIAN = 142,
  NABATAEAN = 143,
  PALMYRENE = 144,
  KHUDAWADI = 145,
  WARANG_CITI = 146,
  AFAKA = 147,
  JURCHEN = 148,
  MRO = 149,
  NUSHU = 150,
  SHARADA = 151,
  SORA_SOMPENG = 152,
  TAKRI = 153,
  TANGUT = 154,
  WOLEAI = 155,
  ANATOLIAN_HIEROGLYPHS = 156,
  KHOJKI = 157,
  TIRHUTA = 158,
  CAUCASIAN_ALBANIAN = 159,
  MAHAJANI = 160,
  AHOM = 161,
  HATRAN = 162,
  MODI = 163,
  MULTANI = 164,
  PAU_CIN_HAU = 165,
  SIDDHAM = 166,

  NUM_SCRIPT_CODES = 167,

  INVALID = -1
};
} // namespace unicode
} // namespace mozilla

#endif
/*
 * * * * * This file contains MACHINE-GENERATED DATA, do not edit! * * * * *
 */
