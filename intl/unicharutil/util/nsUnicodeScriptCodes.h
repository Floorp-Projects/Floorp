
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
 * Created on Mon Jul 13 19:06:12 2015 from UCD data files with version info:
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

# EastAsianWidth-8.0.0.txt
# Date: 2015-02-10, 21:00:00 GMT [KW, LI]

# BidiMirroring-8.0.0.txt
# Date: 2015-01-20, 18:30:00 GMT [KW, LI]

# HangulSyllableType-8.0.0.txt
# Date: 2014-12-16, 23:07:45 GMT [MD]

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


struct nsCharProps1 {
  unsigned char mMirrorOffsetIndex:5;
  unsigned char mHangulType:3;
  unsigned char mCombiningClass:8;
};



struct nsCharProps2 {
  unsigned char mScriptCode:8;
  unsigned char mEAW:3;
  unsigned char mCategory:5;
  unsigned char mBidiCategory:5;
  unsigned char mXidmod:4;
  signed char   mNumericValue:5;
  unsigned char mVertOrient:2;
};


#pragma pack()

enum {
  MOZ_SCRIPT_COMMON = 0,
  MOZ_SCRIPT_INHERITED = 1,
  MOZ_SCRIPT_ARABIC = 2,
  MOZ_SCRIPT_ARMENIAN = 3,
  MOZ_SCRIPT_BENGALI = 4,
  MOZ_SCRIPT_BOPOMOFO = 5,
  MOZ_SCRIPT_CHEROKEE = 6,
  MOZ_SCRIPT_COPTIC = 7,
  MOZ_SCRIPT_CYRILLIC = 8,
  MOZ_SCRIPT_DESERET = 9,
  MOZ_SCRIPT_DEVANAGARI = 10,
  MOZ_SCRIPT_ETHIOPIC = 11,
  MOZ_SCRIPT_GEORGIAN = 12,
  MOZ_SCRIPT_GOTHIC = 13,
  MOZ_SCRIPT_GREEK = 14,
  MOZ_SCRIPT_GUJARATI = 15,
  MOZ_SCRIPT_GURMUKHI = 16,
  MOZ_SCRIPT_HAN = 17,
  MOZ_SCRIPT_HANGUL = 18,
  MOZ_SCRIPT_HEBREW = 19,
  MOZ_SCRIPT_HIRAGANA = 20,
  MOZ_SCRIPT_KANNADA = 21,
  MOZ_SCRIPT_KATAKANA = 22,
  MOZ_SCRIPT_KHMER = 23,
  MOZ_SCRIPT_LAO = 24,
  MOZ_SCRIPT_LATIN = 25,
  MOZ_SCRIPT_MALAYALAM = 26,
  MOZ_SCRIPT_MONGOLIAN = 27,
  MOZ_SCRIPT_MYANMAR = 28,
  MOZ_SCRIPT_OGHAM = 29,
  MOZ_SCRIPT_OLD_ITALIC = 30,
  MOZ_SCRIPT_ORIYA = 31,
  MOZ_SCRIPT_RUNIC = 32,
  MOZ_SCRIPT_SINHALA = 33,
  MOZ_SCRIPT_SYRIAC = 34,
  MOZ_SCRIPT_TAMIL = 35,
  MOZ_SCRIPT_TELUGU = 36,
  MOZ_SCRIPT_THAANA = 37,
  MOZ_SCRIPT_THAI = 38,
  MOZ_SCRIPT_TIBETAN = 39,
  MOZ_SCRIPT_CANADIAN_ABORIGINAL = 40,
  MOZ_SCRIPT_YI = 41,
  MOZ_SCRIPT_TAGALOG = 42,
  MOZ_SCRIPT_HANUNOO = 43,
  MOZ_SCRIPT_BUHID = 44,
  MOZ_SCRIPT_TAGBANWA = 45,
  MOZ_SCRIPT_BRAILLE = 46,
  MOZ_SCRIPT_CYPRIOT = 47,
  MOZ_SCRIPT_LIMBU = 48,
  MOZ_SCRIPT_OSMANYA = 49,
  MOZ_SCRIPT_SHAVIAN = 50,
  MOZ_SCRIPT_LINEAR_B = 51,
  MOZ_SCRIPT_TAI_LE = 52,
  MOZ_SCRIPT_UGARITIC = 53,
  MOZ_SCRIPT_NEW_TAI_LUE = 54,
  MOZ_SCRIPT_BUGINESE = 55,
  MOZ_SCRIPT_GLAGOLITIC = 56,
  MOZ_SCRIPT_TIFINAGH = 57,
  MOZ_SCRIPT_SYLOTI_NAGRI = 58,
  MOZ_SCRIPT_OLD_PERSIAN = 59,
  MOZ_SCRIPT_KHAROSHTHI = 60,
  MOZ_SCRIPT_UNKNOWN = 61,
  MOZ_SCRIPT_BALINESE = 62,
  MOZ_SCRIPT_CUNEIFORM = 63,
  MOZ_SCRIPT_PHOENICIAN = 64,
  MOZ_SCRIPT_PHAGS_PA = 65,
  MOZ_SCRIPT_NKO = 66,
  MOZ_SCRIPT_KAYAH_LI = 67,
  MOZ_SCRIPT_LEPCHA = 68,
  MOZ_SCRIPT_REJANG = 69,
  MOZ_SCRIPT_SUNDANESE = 70,
  MOZ_SCRIPT_SAURASHTRA = 71,
  MOZ_SCRIPT_CHAM = 72,
  MOZ_SCRIPT_OL_CHIKI = 73,
  MOZ_SCRIPT_VAI = 74,
  MOZ_SCRIPT_CARIAN = 75,
  MOZ_SCRIPT_LYCIAN = 76,
  MOZ_SCRIPT_LYDIAN = 77,
  MOZ_SCRIPT_AVESTAN = 78,
  MOZ_SCRIPT_BAMUM = 79,
  MOZ_SCRIPT_EGYPTIAN_HIEROGLYPHS = 80,
  MOZ_SCRIPT_IMPERIAL_ARAMAIC = 81,
  MOZ_SCRIPT_INSCRIPTIONAL_PAHLAVI = 82,
  MOZ_SCRIPT_INSCRIPTIONAL_PARTHIAN = 83,
  MOZ_SCRIPT_JAVANESE = 84,
  MOZ_SCRIPT_KAITHI = 85,
  MOZ_SCRIPT_LISU = 86,
  MOZ_SCRIPT_MEETEI_MAYEK = 87,
  MOZ_SCRIPT_OLD_SOUTH_ARABIAN = 88,
  MOZ_SCRIPT_OLD_TURKIC = 89,
  MOZ_SCRIPT_SAMARITAN = 90,
  MOZ_SCRIPT_TAI_THAM = 91,
  MOZ_SCRIPT_TAI_VIET = 92,
  MOZ_SCRIPT_BATAK = 93,
  MOZ_SCRIPT_BRAHMI = 94,
  MOZ_SCRIPT_MANDAIC = 95,
  MOZ_SCRIPT_CHAKMA = 96,
  MOZ_SCRIPT_MEROITIC_CURSIVE = 97,
  MOZ_SCRIPT_MEROITIC_HIEROGLYPHS = 98,
  MOZ_SCRIPT_MIAO = 99,
  MOZ_SCRIPT_SHARADA = 100,
  MOZ_SCRIPT_SORA_SOMPENG = 101,
  MOZ_SCRIPT_TAKRI = 102,
  MOZ_SCRIPT_BASSA_VAH = 103,
  MOZ_SCRIPT_CAUCASIAN_ALBANIAN = 104,
  MOZ_SCRIPT_DUPLOYAN = 105,
  MOZ_SCRIPT_ELBASAN = 106,
  MOZ_SCRIPT_GRANTHA = 107,
  MOZ_SCRIPT_KHOJKI = 108,
  MOZ_SCRIPT_KHUDAWADI = 109,
  MOZ_SCRIPT_LINEAR_A = 110,
  MOZ_SCRIPT_MAHAJANI = 111,
  MOZ_SCRIPT_MANICHAEAN = 112,
  MOZ_SCRIPT_MENDE_KIKAKUI = 113,
  MOZ_SCRIPT_MODI = 114,
  MOZ_SCRIPT_MRO = 115,
  MOZ_SCRIPT_NABATAEAN = 116,
  MOZ_SCRIPT_OLD_NORTH_ARABIAN = 117,
  MOZ_SCRIPT_OLD_PERMIC = 118,
  MOZ_SCRIPT_PAHAWH_HMONG = 119,
  MOZ_SCRIPT_PALMYRENE = 120,
  MOZ_SCRIPT_PAU_CIN_HAU = 121,
  MOZ_SCRIPT_PSALTER_PAHLAVI = 122,
  MOZ_SCRIPT_SIDDHAM = 123,
  MOZ_SCRIPT_TIRHUTA = 124,
  MOZ_SCRIPT_WARANG_CITI = 125,
  MOZ_SCRIPT_AHOM = 126,
  MOZ_SCRIPT_ANATOLIAN_HIEROGLYPHS = 127,
  MOZ_SCRIPT_HATRAN = 128,
  MOZ_SCRIPT_MULTANI = 129,
  MOZ_SCRIPT_OLD_HUNGARIAN = 130,
  MOZ_SCRIPT_SIGNWRITING = 131,
  MOZ_SCRIPT_MATHEMATICAL_NOTATION = 132,

  MOZ_NUM_SCRIPT_CODES = 133,

  MOZ_SCRIPT_INVALID = -1
};

#endif
/*
 * * * * * This file contains MACHINE-GENERATED DATA, do not edit! * * * * *
 */
