/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFont.h"
#include "gfxFont.h"          // for gfxFontStyle
#include "gfxFontFeatures.h"  // for gfxFontFeature, etc
#include "gfxFontUtils.h"     // for TRUETYPE_TAG
#include "mozilla/ServoStyleConstsInlines.h"
#include "nsCRT.h"    // for nsCRT
#include "nsDebug.h"  // for NS_ASSERTION
#include "nsISupports.h"
#include "nsUnicharUtils.h"
#include "nscore.h"  // for char16_t
#include "mozilla/ArrayUtils.h"
#include "mozilla/gfx/2D.h"

using namespace mozilla;

nsFont::nsFont(const StyleFontFamily& aFamily, mozilla::Length aSize)
    : family(aFamily), size(aSize) {}

nsFont::nsFont(StyleGenericFontFamily aGenericType, mozilla::Length aSize)
    : family(*Servo_FontFamily_Generic(aGenericType)), size(aSize) {}

nsFont::nsFont(const nsFont& aOther) = default;

nsFont::~nsFont() = default;

nsFont& nsFont::operator=(const nsFont&) = default;

bool nsFont::Equals(const nsFont& aOther) const {
  return CalcDifference(aOther) == MaxDifference::eNone;
}

nsFont::MaxDifference nsFont::CalcDifference(const nsFont& aOther) const {
  if ((style != aOther.style) || (weight != aOther.weight) ||
      (stretch != aOther.stretch) || (size != aOther.size) ||
      (sizeAdjust != aOther.sizeAdjust) || (family != aOther.family) ||
      (kerning != aOther.kerning) || (opticalSizing != aOther.opticalSizing) ||
      (synthesisWeight != aOther.synthesisWeight) ||
      (synthesisStyle != aOther.synthesisStyle) ||
      (synthesisSmallCaps != aOther.synthesisSmallCaps) ||
      (synthesisPosition != aOther.synthesisPosition) ||
      (fontFeatureSettings != aOther.fontFeatureSettings) ||
      (fontVariationSettings != aOther.fontVariationSettings) ||
      (languageOverride != aOther.languageOverride) ||
      (variantAlternates != aOther.variantAlternates) ||
      (variantCaps != aOther.variantCaps) ||
      (variantEastAsian != aOther.variantEastAsian) ||
      (variantLigatures != aOther.variantLigatures) ||
      (variantNumeric != aOther.variantNumeric) ||
      (variantPosition != aOther.variantPosition) ||
      (variantWidth != aOther.variantWidth) ||
      (variantEmoji != aOther.variantEmoji)) {
    return MaxDifference::eLayoutAffecting;
  }

  if (smoothing != aOther.smoothing) {
    return MaxDifference::eVisual;
  }

  return MaxDifference::eNone;
}

// mapping from bitflag to font feature tag/value pair
//
// these need to be kept in sync with the constants listed
// in gfxFontConstants.h (e.g. NS_FONT_VARIANT_EAST_ASIAN_JIS78)

// NS_FONT_VARIANT_EAST_ASIAN_xxx values
const gfxFontFeature eastAsianDefaults[] = {
    {TRUETYPE_TAG('j', 'p', '7', '8'), 1},
    {TRUETYPE_TAG('j', 'p', '8', '3'), 1},
    {TRUETYPE_TAG('j', 'p', '9', '0'), 1},
    {TRUETYPE_TAG('j', 'p', '0', '4'), 1},
    {TRUETYPE_TAG('s', 'm', 'p', 'l'), 1},
    {TRUETYPE_TAG('t', 'r', 'a', 'd'), 1},
    {TRUETYPE_TAG('f', 'w', 'i', 'd'), 1},
    {TRUETYPE_TAG('p', 'w', 'i', 'd'), 1},
    {TRUETYPE_TAG('r', 'u', 'b', 'y'), 1}};

static_assert(MOZ_ARRAY_LENGTH(eastAsianDefaults) ==
                  NS_FONT_VARIANT_EAST_ASIAN_COUNT,
              "eastAsianDefaults[] should be correct");

// NS_FONT_VARIANT_LIGATURES_xxx values
const gfxFontFeature ligDefaults[] = {
    {TRUETYPE_TAG('l', 'i', 'g', 'a'), 0},  // none value means all off
    {TRUETYPE_TAG('l', 'i', 'g', 'a'), 1},
    {TRUETYPE_TAG('l', 'i', 'g', 'a'), 0},
    {TRUETYPE_TAG('d', 'l', 'i', 'g'), 1},
    {TRUETYPE_TAG('d', 'l', 'i', 'g'), 0},
    {TRUETYPE_TAG('h', 'l', 'i', 'g'), 1},
    {TRUETYPE_TAG('h', 'l', 'i', 'g'), 0},
    {TRUETYPE_TAG('c', 'a', 'l', 't'), 1},
    {TRUETYPE_TAG('c', 'a', 'l', 't'), 0}};

static_assert(MOZ_ARRAY_LENGTH(ligDefaults) == NS_FONT_VARIANT_LIGATURES_COUNT,
              "ligDefaults[] should be correct");

// NS_FONT_VARIANT_NUMERIC_xxx values
const gfxFontFeature numericDefaults[] = {
    {TRUETYPE_TAG('l', 'n', 'u', 'm'), 1},
    {TRUETYPE_TAG('o', 'n', 'u', 'm'), 1},
    {TRUETYPE_TAG('p', 'n', 'u', 'm'), 1},
    {TRUETYPE_TAG('t', 'n', 'u', 'm'), 1},
    {TRUETYPE_TAG('f', 'r', 'a', 'c'), 1},
    {TRUETYPE_TAG('a', 'f', 'r', 'c'), 1},
    {TRUETYPE_TAG('z', 'e', 'r', 'o'), 1},
    {TRUETYPE_TAG('o', 'r', 'd', 'n'), 1}};

static_assert(MOZ_ARRAY_LENGTH(numericDefaults) ==
                  NS_FONT_VARIANT_NUMERIC_COUNT,
              "numericDefaults[] should be correct");

static void AddFontFeaturesBitmask(uint32_t aValue, uint32_t aMin,
                                   uint32_t aMax,
                                   const gfxFontFeature aFeatureDefaults[],
                                   nsTArray<gfxFontFeature>& aFeaturesOut)

{
  uint32_t i, m;

  for (i = 0, m = aMin; m <= aMax; i++, m <<= 1) {
    if (m & aValue) {
      const gfxFontFeature& feature = aFeatureDefaults[i];
      aFeaturesOut.AppendElement(feature);
    }
  }
}

static uint32_t FontFeatureTagForVariantWidth(uint32_t aVariantWidth) {
  switch (aVariantWidth) {
    case NS_FONT_VARIANT_WIDTH_FULL:
      return TRUETYPE_TAG('f', 'w', 'i', 'd');
    case NS_FONT_VARIANT_WIDTH_HALF:
      return TRUETYPE_TAG('h', 'w', 'i', 'd');
    case NS_FONT_VARIANT_WIDTH_THIRD:
      return TRUETYPE_TAG('t', 'w', 'i', 'd');
    case NS_FONT_VARIANT_WIDTH_QUARTER:
      return TRUETYPE_TAG('q', 'w', 'i', 'd');
    default:
      return 0;
  }
}

void nsFont::AddFontFeaturesToStyle(gfxFontStyle* aStyle,
                                    bool aVertical) const {
  // add in font-variant features
  gfxFontFeature setting;

  // -- kerning
  setting.mTag = aVertical ? TRUETYPE_TAG('v', 'k', 'r', 'n')
                           : TRUETYPE_TAG('k', 'e', 'r', 'n');
  switch (kerning) {
    case NS_FONT_KERNING_NONE:
      setting.mValue = 0;
      aStyle->featureSettings.AppendElement(setting);
      break;
    case NS_FONT_KERNING_NORMAL:
      setting.mValue = 1;
      aStyle->featureSettings.AppendElement(setting);
      break;
    default:
      // auto case implies use user agent default
      break;
  }

  // -- alternates
  //
  // NOTE(emilio): We handle historical-forms here because it doesn't depend on
  // other values set by @font-face and thus may be less expensive to do here
  // than after font-matching.
  for (auto& alternate : variantAlternates.AsSpan()) {
    if (alternate.IsHistoricalForms()) {
      setting.mValue = 1;
      setting.mTag = TRUETYPE_TAG('h', 'i', 's', 't');
      aStyle->featureSettings.AppendElement(setting);
      break;
    }
  }

  // -- copy font-specific alternate info into style
  //    (this will be resolved after font-matching occurs)
  aStyle->variantAlternates = variantAlternates;

  // -- caps
  aStyle->variantCaps = variantCaps;

  // -- east-asian
  if (variantEastAsian) {
    AddFontFeaturesBitmask(variantEastAsian, NS_FONT_VARIANT_EAST_ASIAN_JIS78,
                           NS_FONT_VARIANT_EAST_ASIAN_RUBY, eastAsianDefaults,
                           aStyle->featureSettings);
  }

  // -- ligatures
  if (variantLigatures) {
    AddFontFeaturesBitmask(variantLigatures, NS_FONT_VARIANT_LIGATURES_NONE,
                           NS_FONT_VARIANT_LIGATURES_NO_CONTEXTUAL, ligDefaults,
                           aStyle->featureSettings);

    if (variantLigatures & NS_FONT_VARIANT_LIGATURES_COMMON) {
      // liga already enabled, need to enable clig also
      setting.mTag = TRUETYPE_TAG('c', 'l', 'i', 'g');
      setting.mValue = 1;
      aStyle->featureSettings.AppendElement(setting);
    } else if (variantLigatures & NS_FONT_VARIANT_LIGATURES_NO_COMMON) {
      // liga already disabled, need to disable clig also
      setting.mTag = TRUETYPE_TAG('c', 'l', 'i', 'g');
      setting.mValue = 0;
      aStyle->featureSettings.AppendElement(setting);
    } else if (variantLigatures & NS_FONT_VARIANT_LIGATURES_NONE) {
      // liga already disabled, need to disable dlig, hlig, calt, clig
      setting.mValue = 0;
      setting.mTag = TRUETYPE_TAG('d', 'l', 'i', 'g');
      aStyle->featureSettings.AppendElement(setting);
      setting.mTag = TRUETYPE_TAG('h', 'l', 'i', 'g');
      aStyle->featureSettings.AppendElement(setting);
      setting.mTag = TRUETYPE_TAG('c', 'a', 'l', 't');
      aStyle->featureSettings.AppendElement(setting);
      setting.mTag = TRUETYPE_TAG('c', 'l', 'i', 'g');
      aStyle->featureSettings.AppendElement(setting);
    }
  }

  // -- numeric
  if (variantNumeric) {
    AddFontFeaturesBitmask(variantNumeric, NS_FONT_VARIANT_NUMERIC_LINING,
                           NS_FONT_VARIANT_NUMERIC_ORDINAL, numericDefaults,
                           aStyle->featureSettings);
  }

  // -- position
  aStyle->variantSubSuper = variantPosition;

  // -- width
  setting.mTag = FontFeatureTagForVariantWidth(variantWidth);
  if (setting.mTag) {
    setting.mValue = 1;
    aStyle->featureSettings.AppendElement(setting);
  }

  // indicate common-path case when neither variantCaps or variantSubSuper are
  // set
  aStyle->noFallbackVariantFeatures =
      (aStyle->variantCaps == NS_FONT_VARIANT_CAPS_NORMAL) &&
      (variantPosition == NS_FONT_VARIANT_POSITION_NORMAL);

  // If the feature list is not empty, we insert a "fake" feature with tag=0
  // as delimiter between the above "high-level" features from font-variant-*
  // etc and those coming from the low-level font-feature-settings property.
  // This will allow us to distinguish high- and low-level settings when it
  // comes to potentially disabling ligatures because of letter-spacing.
  if (!aStyle->featureSettings.IsEmpty() || !fontFeatureSettings.IsEmpty()) {
    aStyle->featureSettings.AppendElement(gfxFontFeature{0, 0});
  }

  // add in features from font-feature-settings
  aStyle->featureSettings.AppendElements(fontFeatureSettings);

  // enable grayscale antialiasing for text
  if (smoothing == NS_FONT_SMOOTHING_GRAYSCALE) {
    aStyle->useGrayscaleAntialiasing = true;
  }
}

void nsFont::AddFontVariationsToStyle(gfxFontStyle* aStyle) const {
  // If auto optical sizing is enabled, and if there's no 'opsz' axis in
  // fontVariationSettings, then set the automatic value on the style.
  class VariationTagComparator {
   public:
    bool Equals(const gfxFontVariation& aVariation, uint32_t aTag) const {
      return aVariation.mTag == aTag;
    }
  };
  const uint32_t kTagOpsz = TRUETYPE_TAG('o', 'p', 's', 'z');
  if (opticalSizing == NS_FONT_OPTICAL_SIZING_AUTO &&
      !fontVariationSettings.Contains(kTagOpsz, VariationTagComparator())) {
    aStyle->autoOpticalSize = size.ToCSSPixels();
  }

  // Add in arbitrary values from font-variation-settings
  aStyle->variationSettings.AppendElements(fontVariationSettings);
}
