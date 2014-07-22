/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#define NS_FONT_STYLE_NORMAL            0
#define NS_FONT_STYLE_ITALIC            1
#define NS_FONT_STYLE_OBLIQUE           2

#define NS_FONT_WEIGHT_NORMAL           400
#define NS_FONT_WEIGHT_BOLD             700

#define NS_FONT_STRETCH_ULTRA_CONDENSED (-4)
#define NS_FONT_STRETCH_EXTRA_CONDENSED (-3)
#define NS_FONT_STRETCH_CONDENSED       (-2)
#define NS_FONT_STRETCH_SEMI_CONDENSED  (-1)
#define NS_FONT_STRETCH_NORMAL          0
#define NS_FONT_STRETCH_SEMI_EXPANDED   1
#define NS_FONT_STRETCH_EXPANDED        2
#define NS_FONT_STRETCH_EXTRA_EXPANDED  3
#define NS_FONT_STRETCH_ULTRA_EXPANDED  4

#define NS_FONT_SMOOTHING_AUTO          0
#define NS_FONT_SMOOTHING_GRAYSCALE     1

#define NS_FONT_KERNING_AUTO                        0
#define NS_FONT_KERNING_NONE                        1
#define NS_FONT_KERNING_NORMAL                      2

#define NS_FONT_SYNTHESIS_WEIGHT                    0x1
#define NS_FONT_SYNTHESIS_STYLE                     0x2

enum {
    eFeatureAlternates_historical,
    eFeatureAlternates_stylistic,
    eFeatureAlternates_styleset,
    eFeatureAlternates_character_variant,
    eFeatureAlternates_swash,
    eFeatureAlternates_ornaments,
    eFeatureAlternates_annotation,

    eFeatureAlternates_numFeatures
};

// alternates - simple enumerated values
#define NS_FONT_VARIANT_ALTERNATES_HISTORICAL        (1 << eFeatureAlternates_historical)

// alternates - values that use functional syntax
#define NS_FONT_VARIANT_ALTERNATES_STYLISTIC         (1 << eFeatureAlternates_stylistic)
#define NS_FONT_VARIANT_ALTERNATES_STYLESET          (1 << eFeatureAlternates_styleset)
#define NS_FONT_VARIANT_ALTERNATES_CHARACTER_VARIANT (1 << eFeatureAlternates_character_variant)
#define NS_FONT_VARIANT_ALTERNATES_SWASH             (1 << eFeatureAlternates_swash)
#define NS_FONT_VARIANT_ALTERNATES_ORNAMENTS         (1 << eFeatureAlternates_ornaments)
#define NS_FONT_VARIANT_ALTERNATES_ANNOTATION        (1 << eFeatureAlternates_annotation)

#define NS_FONT_VARIANT_ALTERNATES_ENUMERATED_MASK \
    NS_FONT_VARIANT_ALTERNATES_HISTORICAL

#define NS_FONT_VARIANT_ALTERNATES_FUNCTIONAL_MASK ( \
    NS_FONT_VARIANT_ALTERNATES_STYLISTIC | \
    NS_FONT_VARIANT_ALTERNATES_STYLESET | \
    NS_FONT_VARIANT_ALTERNATES_CHARACTER_VARIANT | \
    NS_FONT_VARIANT_ALTERNATES_SWASH | \
    NS_FONT_VARIANT_ALTERNATES_ORNAMENTS | \
    NS_FONT_VARIANT_ALTERNATES_ANNOTATION )

#define NS_FONT_VARIANT_CAPS_NORMAL                 0
#define NS_FONT_VARIANT_CAPS_SMALLCAPS              1
#define NS_FONT_VARIANT_CAPS_ALLSMALL               2
#define NS_FONT_VARIANT_CAPS_PETITECAPS             3
#define NS_FONT_VARIANT_CAPS_ALLPETITE              4
#define NS_FONT_VARIANT_CAPS_TITLING                5
#define NS_FONT_VARIANT_CAPS_UNICASE                6

enum {
    eFeatureEastAsian_jis78,
    eFeatureEastAsian_jis83,
    eFeatureEastAsian_jis90,
    eFeatureEastAsian_jis04,
    eFeatureEastAsian_simplified,
    eFeatureEastAsian_traditional,
    eFeatureEastAsian_full_width,
    eFeatureEastAsian_prop_width,
    eFeatureEastAsian_ruby,

    eFeatureEastAsian_numFeatures
};

#define NS_FONT_VARIANT_EAST_ASIAN_JIS78        (1 << eFeatureEastAsian_jis78)
#define NS_FONT_VARIANT_EAST_ASIAN_JIS83        (1 << eFeatureEastAsian_jis83)
#define NS_FONT_VARIANT_EAST_ASIAN_JIS90        (1 << eFeatureEastAsian_jis90)
#define NS_FONT_VARIANT_EAST_ASIAN_JIS04        (1 << eFeatureEastAsian_jis04)
#define NS_FONT_VARIANT_EAST_ASIAN_SIMPLIFIED   (1 << eFeatureEastAsian_simplified)
#define NS_FONT_VARIANT_EAST_ASIAN_TRADITIONAL  (1 << eFeatureEastAsian_traditional)
#define NS_FONT_VARIANT_EAST_ASIAN_FULL_WIDTH   (1 << eFeatureEastAsian_full_width)
#define NS_FONT_VARIANT_EAST_ASIAN_PROP_WIDTH   (1 << eFeatureEastAsian_prop_width)
#define NS_FONT_VARIANT_EAST_ASIAN_RUBY         (1 << eFeatureEastAsian_ruby)

#define NS_FONT_VARIANT_EAST_ASIAN_VARIANT_MASK ( \
    NS_FONT_VARIANT_EAST_ASIAN_JIS78 | \
    NS_FONT_VARIANT_EAST_ASIAN_JIS83 | \
    NS_FONT_VARIANT_EAST_ASIAN_JIS90 | \
    NS_FONT_VARIANT_EAST_ASIAN_JIS04 | \
    NS_FONT_VARIANT_EAST_ASIAN_SIMPLIFIED | \
    NS_FONT_VARIANT_EAST_ASIAN_TRADITIONAL )

#define NS_FONT_VARIANT_EAST_ASIAN_WIDTH_MASK ( \
    NS_FONT_VARIANT_EAST_ASIAN_FULL_WIDTH | \
    NS_FONT_VARIANT_EAST_ASIAN_PROP_WIDTH )

enum {
    eFeatureLigatures_none,
    eFeatureLigatures_common,
    eFeatureLigatures_no_common,
    eFeatureLigatures_discretionary,
    eFeatureLigatures_no_discretionary,
    eFeatureLigatures_historical,
    eFeatureLigatures_no_historical,
    eFeatureLigatures_contextual,
    eFeatureLigatures_no_contextual,

    eFeatureLigatures_numFeatures
};

#define NS_FONT_VARIANT_LIGATURES_NONE              (1 << eFeatureLigatures_none)
#define NS_FONT_VARIANT_LIGATURES_COMMON            (1 << eFeatureLigatures_common)
#define NS_FONT_VARIANT_LIGATURES_NO_COMMON         (1 << eFeatureLigatures_no_common)
#define NS_FONT_VARIANT_LIGATURES_DISCRETIONARY     (1 << eFeatureLigatures_discretionary)
#define NS_FONT_VARIANT_LIGATURES_NO_DISCRETIONARY  (1 << eFeatureLigatures_no_discretionary)
#define NS_FONT_VARIANT_LIGATURES_HISTORICAL        (1 << eFeatureLigatures_historical)
#define NS_FONT_VARIANT_LIGATURES_NO_HISTORICAL     (1 << eFeatureLigatures_no_historical)
#define NS_FONT_VARIANT_LIGATURES_CONTEXTUAL        (1 << eFeatureLigatures_contextual)
#define NS_FONT_VARIANT_LIGATURES_NO_CONTEXTUAL     (1 << eFeatureLigatures_no_contextual)

#define NS_FONT_VARIANT_LIGATURES_COMMON_MASK ( \
    NS_FONT_VARIANT_LIGATURES_COMMON | \
    NS_FONT_VARIANT_LIGATURES_NO_COMMON )

#define NS_FONT_VARIANT_LIGATURES_DISCRETIONARY_MASK ( \
    NS_FONT_VARIANT_LIGATURES_DISCRETIONARY | \
    NS_FONT_VARIANT_LIGATURES_NO_DISCRETIONARY )

#define NS_FONT_VARIANT_LIGATURES_HISTORICAL_MASK ( \
    NS_FONT_VARIANT_LIGATURES_HISTORICAL | \
    NS_FONT_VARIANT_LIGATURES_NO_HISTORICAL )

#define NS_FONT_VARIANT_LIGATURES_CONTEXTUAL_MASK \
    NS_FONT_VARIANT_LIGATURES_CONTEXTUAL | \
    NS_FONT_VARIANT_LIGATURES_NO_CONTEXTUAL

enum {
    eFeatureNumeric_lining,
    eFeatureNumeric_oldstyle,
    eFeatureNumeric_proportional,
    eFeatureNumeric_tabular,
    eFeatureNumeric_diagonal_fractions,
    eFeatureNumeric_stacked_fractions,
    eFeatureNumeric_slashedzero,
    eFeatureNumeric_ordinal,

    eFeatureNumeric_numFeatures
};

#define NS_FONT_VARIANT_NUMERIC_LINING              (1 << eFeatureNumeric_lining)
#define NS_FONT_VARIANT_NUMERIC_OLDSTYLE            (1 << eFeatureNumeric_oldstyle)
#define NS_FONT_VARIANT_NUMERIC_PROPORTIONAL        (1 << eFeatureNumeric_proportional)
#define NS_FONT_VARIANT_NUMERIC_TABULAR             (1 << eFeatureNumeric_tabular)
#define NS_FONT_VARIANT_NUMERIC_DIAGONAL_FRACTIONS  (1 << eFeatureNumeric_diagonal_fractions)
#define NS_FONT_VARIANT_NUMERIC_STACKED_FRACTIONS   (1 << eFeatureNumeric_stacked_fractions)
#define NS_FONT_VARIANT_NUMERIC_SLASHZERO           (1 << eFeatureNumeric_slashedzero)
#define NS_FONT_VARIANT_NUMERIC_ORDINAL             (1 << eFeatureNumeric_ordinal)

#define NS_FONT_VARIANT_NUMERIC_FIGURE_MASK \
    NS_FONT_VARIANT_NUMERIC_LINING | \
    NS_FONT_VARIANT_NUMERIC_OLDSTYLE

#define NS_FONT_VARIANT_NUMERIC_SPACING_MASK \
    NS_FONT_VARIANT_NUMERIC_PROPORTIONAL | \
    NS_FONT_VARIANT_NUMERIC_TABULAR

#define NS_FONT_VARIANT_NUMERIC_FRACTION_MASK \
    NS_FONT_VARIANT_NUMERIC_DIAGONAL_FRACTIONS | \
    NS_FONT_VARIANT_NUMERIC_STACKED_FRACTIONS

#define NS_FONT_VARIANT_POSITION_NORMAL             0
#define NS_FONT_VARIANT_POSITION_SUPER              1
#define NS_FONT_VARIANT_POSITION_SUB                2

// based on fixed offset values used within WebKit
#define NS_FONT_SUBSCRIPT_OFFSET_RATIO     (0.20)
#define NS_FONT_SUPERSCRIPT_OFFSET_RATIO   (0.34)

// this roughly corresponds to font-size: smaller behavior
// at smaller sizes <20px the ratio is closer to 0.8 while at
// larger sizes >45px the ratio is closer to 0.667 and in between
// a blend of values is used
#define NS_FONT_SUB_SUPER_SIZE_RATIO_SMALL       (0.82)
#define NS_FONT_SUB_SUPER_SIZE_RATIO_LARGE       (0.667)
#define NS_FONT_SUB_SUPER_SMALL_SIZE             (20.0)
#define NS_FONT_SUB_SUPER_LARGE_SIZE             (45.0)

#endif
