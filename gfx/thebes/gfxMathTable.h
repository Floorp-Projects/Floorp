/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_MATH_TABLE_H
#define GFX_MATH_TABLE_H

#include "gfxFont.h"

/**
 * Used by |gfxFont| to represent the MATH table of an OpenType font.
 * Each |gfxFont| owns at most one |gfxMathTable| instance.
 */
class gfxMathTable {
 public:
  /**
   * @param aFace The HarfBuzz face containing the math table.
   * @param aSize The font size to pass to HarfBuzz.
   */
  gfxMathTable(hb_face_t* aFace, gfxFloat aSize);

  /**
   * Releases our reference to the MATH table and cleans up everything else.
   */
  ~gfxMathTable();

  enum MathConstant {
    // The order of the constants must match the order of the fields
    // defined in the MATH table.
    ScriptPercentScaleDown,
    ScriptScriptPercentScaleDown,
    DelimitedSubFormulaMinHeight,
    DisplayOperatorMinHeight,
    MathLeading,
    AxisHeight,
    AccentBaseHeight,
    FlattenedAccentBaseHeight,
    SubscriptShiftDown,
    SubscriptTopMax,
    SubscriptBaselineDropMin,
    SuperscriptShiftUp,
    SuperscriptShiftUpCramped,
    SuperscriptBottomMin,
    SuperscriptBaselineDropMax,
    SubSuperscriptGapMin,
    SuperscriptBottomMaxWithSubscript,
    SpaceAfterScript,
    UpperLimitGapMin,
    UpperLimitBaselineRiseMin,
    LowerLimitGapMin,
    LowerLimitBaselineDropMin,
    StackTopShiftUp,
    StackTopDisplayStyleShiftUp,
    StackBottomShiftDown,
    StackBottomDisplayStyleShiftDown,
    StackGapMin,
    StackDisplayStyleGapMin,
    StretchStackTopShiftUp,
    StretchStackBottomShiftDown,
    StretchStackGapAboveMin,
    StretchStackGapBelowMin,
    FractionNumeratorShiftUp,
    FractionNumeratorDisplayStyleShiftUp,
    FractionDenominatorShiftDown,
    FractionDenominatorDisplayStyleShiftDown,
    FractionNumeratorGapMin,
    FractionNumDisplayStyleGapMin,
    FractionRuleThickness,
    FractionDenominatorGapMin,
    FractionDenomDisplayStyleGapMin,
    SkewedFractionHorizontalGap,
    SkewedFractionVerticalGap,
    OverbarVerticalGap,
    OverbarRuleThickness,
    OverbarExtraAscender,
    UnderbarVerticalGap,
    UnderbarRuleThickness,
    UnderbarExtraDescender,
    RadicalVerticalGap,
    RadicalDisplayStyleVerticalGap,
    RadicalRuleThickness,
    RadicalExtraAscender,
    RadicalKernBeforeDegree,
    RadicalKernAfterDegree,
    RadicalDegreeBottomRaisePercent
  };

  /**
   * Returns the value of the specified constant from the MATH table.
   */
  gfxFloat Constant(MathConstant aConstant) const;

  /**
   * Returns the value of the specified constant in app units.
   */
  nscoord Constant(MathConstant aConstant,
                   uint32_t aAppUnitsPerDevPixel) const {
    return NSToCoordRound(Constant(aConstant) * aAppUnitsPerDevPixel);
  }

  /**
   *  If the MATH table contains an italic correction for that glyph, this
   *  function returns the corresponding value. Otherwise it returns 0.
   */
  gfxFloat ItalicsCorrection(uint32_t aGlyphID) const;

  /**
   * @param aGlyphID  glyph index of the character we want to stretch
   * @param aVertical direction of the stretching (vertical/horizontal)
   * @param aSize     the desired size variant
   *
   * Returns the glyph index of the desired size variant or 0 if there is not
   * any such size variant.
   */
  uint32_t VariantsSize(uint32_t aGlyphID, bool aVertical,
                        uint16_t aSize) const;

  /**
   * @param aGlyphID  glyph index of the character we want to stretch
   * @param aVertical direction of the stretching (vertical/horizontal)
   * @param aGlyphs   pre-allocated buffer of 4 elements where the glyph
   * indexes (or 0 for absent parts) will be stored. The parts are stored in
   * the order expected by the nsMathMLChar: Top (or Left), Middle, Bottom
   * (or Right), Glue.
   *
   * Tries to fill-in aGlyphs with the relevant glyph indexes and returns
   * whether the operation was successful. The function returns false if
   * there is not any assembly for the character we want to stretch or if
   * the format is not supported by the nsMathMLChar code.
   *
   */
  bool VariantsParts(uint32_t aGlyphID, bool aVertical,
                     uint32_t aGlyphs[4]) const;

 private:
  // size-specific font object, owned by the gfxMathTable
  hb_font_t* mHBFont;

  static const unsigned int kMaxCachedSizeCount = 10;
  struct MathVariantCacheEntry {
    uint32_t glyphID;
    bool vertical;
    uint32_t sizes[kMaxCachedSizeCount];
    uint32_t parts[4];
    bool arePartsValid;
  };
  mutable MathVariantCacheEntry mMathVariantCache;
  void ClearCache() const;
  void UpdateMathVariantCache(uint32_t aGlyphID, bool aVertical) const;
};

#endif
