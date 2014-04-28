
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
 * Created on Mon Apr 28 11:24:35 2014 from UCD data files with version info:
 *

# Date: 2013-09-27, 23:05:00 GMT [KW]
#
# Unicode Character Database
# Copyright (c) 1991-2013 Unicode, Inc.
# For terms of use, see http://www.unicode.org/terms_of_use.html
#
# For documentation, see NamesList.html,
# UAX #38, "Unicode Han Database (Unihan)," and
# UAX #44, "Unicode Character Database."
#

This directory contains the final data files
for the Unicode Character Database (UCD) for Unicode 6.3.0.



# Scripts-6.3.0.txt
# Date: 2013-07-05, 14:09:02 GMT [MD]

# EastAsianWidth-6.3.0.txt
# Date: 2013-02-05, 20:09:00 GMT [KW, LI]

# BidiMirroring-6.3.0.txt
# Date: 2013-02-12, 08:20:00 GMT [KW, LI]

# HangulSyllableType-6.3.0.txt
# Date: 2012-12-20, 22:18:29 GMT [MD]

# File: xidmodifications.txt
# Version: 3.0-draft
# Generated: 2012-05-07, 07:52:41 GMT

#
# Unihan_Variants.txt
# Date: 2013-02-25 22:46:17 GMT [JHJ]

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
  signed char mNumericValue:5;
  unsigned char mHanVariant:2;
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
  MOZ_SCRIPT_MATHEMATICAL_NOTATION = 103,

  MOZ_NUM_SCRIPT_CODES = 104,

  MOZ_SCRIPT_INVALID = -1
};

#endif
/*
 * * * * * This file contains MACHINE-GENERATED DATA, do not edit! * * * * *
 */
