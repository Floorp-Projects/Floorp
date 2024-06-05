/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* font constants shared by both thebes and layout */

#ifndef GFX_FONT_CONSTANTS_H
#define GFX_FONT_CONSTANTS_H

/*
 * This file is separate from gfxFont.h so that layout can include it
 * without bringing in gfxFont.h and everything it includes.
 */

#define NS_FONT_STYLE_NORMAL 0
#define NS_FONT_STYLE_ITALIC 1
#define NS_FONT_STYLE_OBLIQUE 2

#define NS_FONT_WEIGHT_NORMAL 400
#define NS_FONT_WEIGHT_BOLD 700
#define NS_FONT_WEIGHT_THIN 100

#define NS_FONT_STRETCH_ULTRA_CONDENSED 50
#define NS_FONT_STRETCH_EXTRA_CONDENSED 62
#define NS_FONT_STRETCH_CONDENSED 75
#define NS_FONT_STRETCH_SEMI_CONDENSED 87
#define NS_FONT_STRETCH_NORMAL 100
#define NS_FONT_STRETCH_SEMI_EXPANDED 112
#define NS_FONT_STRETCH_EXPANDED 125
#define NS_FONT_STRETCH_EXTRA_EXPANDED 150
#define NS_FONT_STRETCH_ULTRA_EXPANDED 200

#define NS_FONT_SMOOTHING_AUTO 0
#define NS_FONT_SMOOTHING_GRAYSCALE 1
/* For -webkit-font-smoothing; behaves the same as AUTO, but not aliased for
   parsing/serialization because that would confuse tests. */
#define NS_FONT_SMOOTHING_SUBPIXEL_ANTIALIASED 2

#define NS_FONT_KERNING_AUTO 0
#define NS_FONT_KERNING_NONE 1
#define NS_FONT_KERNING_NORMAL 2

#define NS_FONT_OPTICAL_SIZING_AUTO 0
#define NS_FONT_OPTICAL_SIZING_NONE 1

#define NS_FONT_VARIANT_ALTERNATES_NORMAL 0
// alternates - simple enumerated values
#define NS_FONT_VARIANT_ALTERNATES_HISTORICAL (1 << 0)

// alternates - values that use functional syntax
#define NS_FONT_VARIANT_ALTERNATES_STYLISTIC (1 << 1)
#define NS_FONT_VARIANT_ALTERNATES_STYLESET (1 << 2)
#define NS_FONT_VARIANT_ALTERNATES_CHARACTER_VARIANT (1 << 3)
#define NS_FONT_VARIANT_ALTERNATES_SWASH (1 << 4)
#define NS_FONT_VARIANT_ALTERNATES_ORNAMENTS (1 << 5)
#define NS_FONT_VARIANT_ALTERNATES_ANNOTATION (1 << 6)
#define NS_FONT_VARIANT_ALTERNATES_COUNT 7

#define NS_FONT_VARIANT_ALTERNATES_ENUMERATED_MASK \
  NS_FONT_VARIANT_ALTERNATES_HISTORICAL

#define NS_FONT_VARIANT_ALTERNATES_FUNCTIONAL_MASK                           \
  (NS_FONT_VARIANT_ALTERNATES_STYLISTIC |                                    \
   NS_FONT_VARIANT_ALTERNATES_STYLESET |                                     \
   NS_FONT_VARIANT_ALTERNATES_CHARACTER_VARIANT |                            \
   NS_FONT_VARIANT_ALTERNATES_SWASH | NS_FONT_VARIANT_ALTERNATES_ORNAMENTS | \
   NS_FONT_VARIANT_ALTERNATES_ANNOTATION)

#define NS_FONT_VARIANT_CAPS_NORMAL 0
#define NS_FONT_VARIANT_CAPS_SMALLCAPS 1
#define NS_FONT_VARIANT_CAPS_ALLSMALL 2
#define NS_FONT_VARIANT_CAPS_PETITECAPS 3
#define NS_FONT_VARIANT_CAPS_ALLPETITE 4
#define NS_FONT_VARIANT_CAPS_TITLING 5
#define NS_FONT_VARIANT_CAPS_UNICASE 6

#define NS_FONT_VARIANT_POSITION_NORMAL 0
#define NS_FONT_VARIANT_POSITION_SUPER 1
#define NS_FONT_VARIANT_POSITION_SUB 2

#define NS_FONT_VARIANT_WIDTH_NORMAL 0
#define NS_FONT_VARIANT_WIDTH_FULL 1
#define NS_FONT_VARIANT_WIDTH_HALF 2
#define NS_FONT_VARIANT_WIDTH_THIRD 3
#define NS_FONT_VARIANT_WIDTH_QUARTER 4

enum class StyleFontVariantEmoji : uint8_t { Normal, Text, Emoji, Unicode };

// based on fixed offset values used within WebKit
#define NS_FONT_SUBSCRIPT_OFFSET_RATIO (0.20)
#define NS_FONT_SUPERSCRIPT_OFFSET_RATIO (0.34)

// this roughly corresponds to font-size: smaller behavior
// at smaller sizes <20px the ratio is closer to 0.8 while at
// larger sizes >45px the ratio is closer to 0.667 and in between
// a blend of values is used
#define NS_FONT_SUB_SUPER_SIZE_RATIO_SMALL (0.82)
#define NS_FONT_SUB_SUPER_SIZE_RATIO_LARGE (0.667)
#define NS_FONT_SUB_SUPER_SMALL_SIZE (20.0)
#define NS_FONT_SUB_SUPER_LARGE_SIZE (45.0)

// pref lang id's for font prefs
enum eFontPrefLang {
#define FONT_PREF_LANG(enum_id_, str_, atom_id_) eFontPrefLang_##enum_id_
#include "gfxFontPrefLangList.h"
#undef FONT_PREF_LANG

  ,
  eFontPrefLang_CJKSet  // special code for CJK set
  ,
  eFontPrefLang_Emoji  // special code for emoji presentation
  ,
  eFontPrefLang_First = eFontPrefLang_Western,
  eFontPrefLang_Last = eFontPrefLang_Others,
  eFontPrefLang_Count = (eFontPrefLang_Last - eFontPrefLang_First + 1)
};

#endif
