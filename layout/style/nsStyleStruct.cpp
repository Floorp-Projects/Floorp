/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * structs that contain the data provided by nsStyleContext, the
 * internal API for computed style data for an element
 */

#include "nsStyleStruct.h"
#include "nsStyleStructInlines.h"
#include "nsStyleConsts.h"
#include "nsThemeConstants.h"
#include "nsString.h"
#include "nsPresContext.h"
#include "nsIWidget.h"
#include "nsCRTGlue.h"
#include "nsCSSProps.h"

#include "nsCOMPtr.h"

#include "nsBidiUtils.h"
#include "nsLayoutUtils.h"

#include "imgIRequest.h"
#include "imgIContainer.h"
#include "CounterStyleManager.h"

#include "mozilla/Likely.h"
#include "nsIURI.h"
#include "nsIDocument.h"
#include <algorithm>

static_assert((((1 << nsStyleStructID_Length) - 1) &
               ~(NS_STYLE_INHERIT_MASK)) == 0,
              "Not enough bits in NS_STYLE_INHERIT_MASK");

inline bool IsFixedUnit(const nsStyleCoord& aCoord, bool aEnumOK)
{
  return aCoord.ConvertsToLength() || 
         (aEnumOK && aCoord.GetUnit() == eStyleUnit_Enumerated);
}

static bool EqualURIs(nsIURI *aURI1, nsIURI *aURI2)
{
  bool eq;
  return aURI1 == aURI2 ||    // handle null==null, and optimize
         (aURI1 && aURI2 &&
          NS_SUCCEEDED(aURI1->Equals(aURI2, &eq)) && // not equal on fail
          eq);
}

static bool EqualURIs(mozilla::css::URLValue *aURI1, mozilla::css::URLValue *aURI2)
{
  return aURI1 == aURI2 ||    // handle null==null, and optimize
         (aURI1 && aURI2 && aURI1->URIEquals(*aURI2));
}

static bool EqualImages(imgIRequest *aImage1, imgIRequest* aImage2)
{
  if (aImage1 == aImage2) {
    return true;
  }

  if (!aImage1 || !aImage2) {
    return false;
  }

  nsCOMPtr<nsIURI> uri1, uri2;
  aImage1->GetURI(getter_AddRefs(uri1));
  aImage2->GetURI(getter_AddRefs(uri2));
  return EqualURIs(uri1, uri2);
}

// A nullsafe wrapper for strcmp. We depend on null-safety.
static int safe_strcmp(const char16_t* a, const char16_t* b)
{
  if (!a || !b) {
    return (int)(a - b);
  }
  return NS_strcmp(a, b);
}

static nsChangeHint CalcShadowDifference(nsCSSShadowArray* lhs,
                                         nsCSSShadowArray* rhs);

// --------------------
// nsStyleFont
//
nsStyleFont::nsStyleFont(const nsFont& aFont, nsPresContext *aPresContext)
  : mFont(aFont)
  , mGenericID(kGenericFont_NONE)
  , mExplicitLanguage(false)
{
  MOZ_COUNT_CTOR(nsStyleFont);
  Init(aPresContext);
}

nsStyleFont::nsStyleFont(const nsStyleFont& aSrc)
  : mFont(aSrc.mFont)
  , mSize(aSrc.mSize)
  , mGenericID(aSrc.mGenericID)
  , mScriptLevel(aSrc.mScriptLevel)
  , mMathVariant(aSrc.mMathVariant)
  , mMathDisplay(aSrc.mMathDisplay)
  , mExplicitLanguage(aSrc.mExplicitLanguage)
  , mAllowZoom(aSrc.mAllowZoom)
  , mScriptUnconstrainedSize(aSrc.mScriptUnconstrainedSize)
  , mScriptMinSize(aSrc.mScriptMinSize)
  , mScriptSizeMultiplier(aSrc.mScriptSizeMultiplier)
  , mLanguage(aSrc.mLanguage)
{
  MOZ_COUNT_CTOR(nsStyleFont);
}

nsStyleFont::nsStyleFont(nsPresContext* aPresContext)
  // passing nullptr to GetDefaultFont make it use the doc language
  : mFont(*(aPresContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID,
                                         nullptr)))
  , mGenericID(kGenericFont_NONE)
  , mExplicitLanguage(false)
{
  MOZ_COUNT_CTOR(nsStyleFont);
  Init(aPresContext);
}

void
nsStyleFont::Init(nsPresContext* aPresContext)
{
  mSize = mFont.size = nsStyleFont::ZoomText(aPresContext, mFont.size);
  mScriptUnconstrainedSize = mSize;
  mScriptMinSize = aPresContext->CSSTwipsToAppUnits(
      NS_POINTS_TO_TWIPS(NS_MATHML_DEFAULT_SCRIPT_MIN_SIZE_PT));
  mScriptLevel = 0;
  mScriptSizeMultiplier = NS_MATHML_DEFAULT_SCRIPT_SIZE_MULTIPLIER;
  mMathVariant = NS_MATHML_MATHVARIANT_NONE;
  mMathDisplay = NS_MATHML_DISPLAYSTYLE_INLINE;
  mAllowZoom = true;

  nsAutoString language;
  aPresContext->Document()->GetContentLanguage(language);
  language.StripWhitespace();

  // Content-Language may be a comma-separated list of language codes,
  // in which case the HTML5 spec says to treat it as unknown
  if (!language.IsEmpty() &&
      language.FindChar(char16_t(',')) == kNotFound) {
    mLanguage = do_GetAtom(language);
    // NOTE:  This does *not* count as an explicit language; in other
    // words, it doesn't trigger language-specific hyphenation.
  } else {
    // we didn't find a (usable) Content-Language, so we fall back
    // to whatever the presContext guessed from the charset
    mLanguage = aPresContext->GetLanguageFromCharset();
  }
}

void 
nsStyleFont::Destroy(nsPresContext* aContext) {
  this->~nsStyleFont();
  aContext->PresShell()->
    FreeByObjectID(nsPresArena::nsStyleFont_id, this);
}

void
nsStyleFont::EnableZoom(nsPresContext* aContext, bool aEnable)
{
  if (mAllowZoom == aEnable) {
    return;
  }
  mAllowZoom = aEnable;
  if (mAllowZoom) {
    mSize = nsStyleFont::ZoomText(aContext, mSize);
    mFont.size = nsStyleFont::ZoomText(aContext, mFont.size);
    mScriptUnconstrainedSize =
      nsStyleFont::ZoomText(aContext, mScriptUnconstrainedSize);
  } else {
    mSize = nsStyleFont::UnZoomText(aContext, mSize);
    mFont.size = nsStyleFont::UnZoomText(aContext, mFont.size);
    mScriptUnconstrainedSize =
      nsStyleFont::UnZoomText(aContext, mScriptUnconstrainedSize);
  }
}

nsChangeHint nsStyleFont::CalcDifference(const nsStyleFont& aOther) const
{
  MOZ_ASSERT(mAllowZoom == aOther.mAllowZoom,
             "expected mAllowZoom to be the same on both nsStyleFonts");
  if (mSize != aOther.mSize ||
      mLanguage != aOther.mLanguage ||
      mExplicitLanguage != aOther.mExplicitLanguage ||
      mMathVariant != aOther.mMathVariant ||
      mMathDisplay != aOther.mMathDisplay) {
    return NS_STYLE_HINT_REFLOW;
  }

  nsChangeHint hint = CalcFontDifference(mFont, aOther.mFont);
  if (hint) {
    return hint;
  }

  // XXX Should any of these cause a non-nsChangeHint_NeutralChange change?
  if (mGenericID != aOther.mGenericID ||
      mScriptLevel != aOther.mScriptLevel ||
      mScriptUnconstrainedSize != aOther.mScriptUnconstrainedSize ||
      mScriptMinSize != aOther.mScriptMinSize ||
      mScriptSizeMultiplier != aOther.mScriptSizeMultiplier) {
    return nsChangeHint_NeutralChange;
  }

  return NS_STYLE_HINT_NONE;
}

/* static */ nscoord
nsStyleFont::ZoomText(nsPresContext *aPresContext, nscoord aSize)
{
  return nscoord(float(aSize) * aPresContext->TextZoom());
}

/* static */ nscoord
nsStyleFont::UnZoomText(nsPresContext *aPresContext, nscoord aSize)
{
  return nscoord(float(aSize) / aPresContext->TextZoom());
}

nsChangeHint nsStyleFont::CalcFontDifference(const nsFont& aFont1, const nsFont& aFont2)
{
  if ((aFont1.size == aFont2.size) && 
      (aFont1.sizeAdjust == aFont2.sizeAdjust) && 
      (aFont1.style == aFont2.style) &&
      (aFont1.weight == aFont2.weight) &&
      (aFont1.stretch == aFont2.stretch) &&
      (aFont1.smoothing == aFont2.smoothing) &&
      (aFont1.fontlist == aFont2.fontlist) &&
      (aFont1.kerning == aFont2.kerning) &&
      (aFont1.synthesis == aFont2.synthesis) &&
      (aFont1.variantAlternates == aFont2.variantAlternates) &&
      (aFont1.alternateValues == aFont2.alternateValues) &&
      (aFont1.featureValueLookup == aFont2.featureValueLookup) &&
      (aFont1.variantCaps == aFont2.variantCaps) &&
      (aFont1.variantEastAsian == aFont2.variantEastAsian) &&
      (aFont1.variantLigatures == aFont2.variantLigatures) &&
      (aFont1.variantNumeric == aFont2.variantNumeric) &&
      (aFont1.variantPosition == aFont2.variantPosition) &&
      (aFont1.fontFeatureSettings == aFont2.fontFeatureSettings) &&
      (aFont1.languageOverride == aFont2.languageOverride) &&
      (aFont1.systemFont == aFont2.systemFont)) {
    if ((aFont1.decorations == aFont2.decorations)) {
      return NS_STYLE_HINT_NONE;
    }
    return NS_STYLE_HINT_VISUAL;
  }
  return NS_STYLE_HINT_REFLOW;
}

static bool IsFixedData(const nsStyleSides& aSides, bool aEnumOK)
{
  NS_FOR_CSS_SIDES(side) {
    if (!IsFixedUnit(aSides.Get(side), aEnumOK))
      return false;
  }
  return true;
}

static nscoord CalcCoord(const nsStyleCoord& aCoord, 
                         const nscoord* aEnumTable, 
                         int32_t aNumEnums)
{
  if (aCoord.GetUnit() == eStyleUnit_Enumerated) {
    NS_ABORT_IF_FALSE(aEnumTable, "must have enum table");
    int32_t value = aCoord.GetIntValue();
    if (0 <= value && value < aNumEnums) {
      return aEnumTable[aCoord.GetIntValue()];
    }
    NS_NOTREACHED("unexpected enum value");
    return 0;
  }
  NS_ABORT_IF_FALSE(aCoord.ConvertsToLength(), "unexpected unit");
  return nsRuleNode::ComputeCoordPercentCalc(aCoord, 0);
}

nsStyleMargin::nsStyleMargin()
  : mHasCachedMargin(false)
  , mCachedMargin(0, 0, 0, 0)
{
  MOZ_COUNT_CTOR(nsStyleMargin);
  nsStyleCoord zero(0, nsStyleCoord::CoordConstructor);
  NS_FOR_CSS_SIDES(side) {
    mMargin.Set(side, zero);
  }
}

nsStyleMargin::nsStyleMargin(const nsStyleMargin& aSrc)
  : mMargin(aSrc.mMargin)
  , mHasCachedMargin(false)
  , mCachedMargin(0, 0, 0, 0)
{
  MOZ_COUNT_CTOR(nsStyleMargin);
}

void 
nsStyleMargin::Destroy(nsPresContext* aContext) {
  this->~nsStyleMargin();
  aContext->PresShell()->
    FreeByObjectID(nsPresArena::nsStyleMargin_id, this);
}


void nsStyleMargin::RecalcData()
{
  if (IsFixedData(mMargin, false)) {
    NS_FOR_CSS_SIDES(side) {
      mCachedMargin.Side(side) = CalcCoord(mMargin.Get(side), nullptr, 0);
    }
    mHasCachedMargin = true;
  }
  else
    mHasCachedMargin = false;
}

nsChangeHint nsStyleMargin::CalcDifference(const nsStyleMargin& aOther) const
{
  if (mMargin == aOther.mMargin) {
    return NS_STYLE_HINT_NONE;
  }
  // Margin differences can't affect descendant intrinsic sizes and
  // don't need to force children to reflow.
  return NS_CombineHint(nsChangeHint_NeedReflow,
                        nsChangeHint_ClearAncestorIntrinsics);
}

nsStylePadding::nsStylePadding()
  : mHasCachedPadding(false)
  , mCachedPadding(0, 0, 0, 0)
{
  MOZ_COUNT_CTOR(nsStylePadding);
  nsStyleCoord zero(0, nsStyleCoord::CoordConstructor);
  NS_FOR_CSS_SIDES(side) {
    mPadding.Set(side, zero);
  }
}

nsStylePadding::nsStylePadding(const nsStylePadding& aSrc)
  : mPadding(aSrc.mPadding)
  , mHasCachedPadding(false)
  , mCachedPadding(0, 0, 0, 0)
{
  MOZ_COUNT_CTOR(nsStylePadding);
}

void 
nsStylePadding::Destroy(nsPresContext* aContext) {
  this->~nsStylePadding();
  aContext->PresShell()->
    FreeByObjectID(nsPresArena::nsStylePadding_id, this);
}

void nsStylePadding::RecalcData()
{
  if (IsFixedData(mPadding, false)) {
    NS_FOR_CSS_SIDES(side) {
      // Clamp negative calc() to 0.
      mCachedPadding.Side(side) =
        std::max(CalcCoord(mPadding.Get(side), nullptr, 0), 0);
    }
    mHasCachedPadding = true;
  }
  else
    mHasCachedPadding = false;
}

nsChangeHint nsStylePadding::CalcDifference(const nsStylePadding& aOther) const
{
  if (mPadding == aOther.mPadding) {
    return NS_STYLE_HINT_NONE;
  }
  // Padding differences can't affect descendant intrinsic sizes, but do need
  // to force children to reflow so that we can reposition them, since their
  // offsets are from our frame bounds but our content rect's position within
  // those bounds is moving.
  return NS_SubtractHint(NS_STYLE_HINT_REFLOW,
                         nsChangeHint_ClearDescendantIntrinsics);
}

nsStyleBorder::nsStyleBorder(nsPresContext* aPresContext)
  : mBorderColors(nullptr),
    mBoxShadow(nullptr),
    mBorderImageFill(NS_STYLE_BORDER_IMAGE_SLICE_NOFILL),
    mBorderImageRepeatH(NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH),
    mBorderImageRepeatV(NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH),
    mFloatEdge(NS_STYLE_FLOAT_EDGE_CONTENT),
    mBoxDecorationBreak(NS_STYLE_BOX_DECORATION_BREAK_SLICE),
    mComputedBorder(0, 0, 0, 0)
{
  MOZ_COUNT_CTOR(nsStyleBorder);

  NS_FOR_CSS_HALF_CORNERS (corner) {
    mBorderRadius.Set(corner, nsStyleCoord(0, nsStyleCoord::CoordConstructor));
  }

  nscoord medium =
    (aPresContext->GetBorderWidthTable())[NS_STYLE_BORDER_WIDTH_MEDIUM];
  NS_FOR_CSS_SIDES(side) {
    mBorderImageSlice.Set(side, nsStyleCoord(1.0f, eStyleUnit_Percent));
    mBorderImageWidth.Set(side, nsStyleCoord(1.0f, eStyleUnit_Factor));
    mBorderImageOutset.Set(side, nsStyleCoord(0.0f, eStyleUnit_Factor));

    mBorder.Side(side) = medium;
    mBorderStyle[side] = NS_STYLE_BORDER_STYLE_NONE | BORDER_COLOR_FOREGROUND;
    mBorderColor[side] = NS_RGB(0, 0, 0);
  }

  mTwipsPerPixel = aPresContext->DevPixelsToAppUnits(1);
}

nsBorderColors::~nsBorderColors()
{
  NS_CSS_DELETE_LIST_MEMBER(nsBorderColors, this, mNext);
}

nsBorderColors*
nsBorderColors::Clone(bool aDeep) const
{
  nsBorderColors* result = new nsBorderColors(mColor);
  if (MOZ_UNLIKELY(!result))
    return result;
  if (aDeep)
    NS_CSS_CLONE_LIST_MEMBER(nsBorderColors, this, mNext, result, (false));
  return result;
}

nsStyleBorder::nsStyleBorder(const nsStyleBorder& aSrc)
  : mBorderColors(nullptr),
    mBoxShadow(aSrc.mBoxShadow),
    mBorderRadius(aSrc.mBorderRadius),
    mBorderImageSource(aSrc.mBorderImageSource),
    mBorderImageSlice(aSrc.mBorderImageSlice),
    mBorderImageWidth(aSrc.mBorderImageWidth),
    mBorderImageOutset(aSrc.mBorderImageOutset),
    mBorderImageFill(aSrc.mBorderImageFill),
    mBorderImageRepeatH(aSrc.mBorderImageRepeatH),
    mBorderImageRepeatV(aSrc.mBorderImageRepeatV),
    mFloatEdge(aSrc.mFloatEdge),
    mBoxDecorationBreak(aSrc.mBoxDecorationBreak),
    mComputedBorder(aSrc.mComputedBorder),
    mBorder(aSrc.mBorder),
    mTwipsPerPixel(aSrc.mTwipsPerPixel)
{
  MOZ_COUNT_CTOR(nsStyleBorder);
  if (aSrc.mBorderColors) {
    EnsureBorderColors();
    for (int32_t i = 0; i < 4; i++)
      if (aSrc.mBorderColors[i])
        mBorderColors[i] = aSrc.mBorderColors[i]->Clone();
      else
        mBorderColors[i] = nullptr;
  }

  NS_FOR_CSS_SIDES(side) {
    mBorderStyle[side] = aSrc.mBorderStyle[side];
    mBorderColor[side] = aSrc.mBorderColor[side];
  }
}

nsStyleBorder::~nsStyleBorder()
{
  MOZ_COUNT_DTOR(nsStyleBorder);
  if (mBorderColors) {
    for (int32_t i = 0; i < 4; i++)
      delete mBorderColors[i];
    delete [] mBorderColors;
  }
}

nsMargin
nsStyleBorder::GetImageOutset() const
{
  // We don't check whether there is a border-image (which is OK since
  // the initial values yields 0 outset) so that we don't have to
  // reflow to update overflow areas when an image loads.
  nsMargin outset;
  NS_FOR_CSS_SIDES(s) {
    nsStyleCoord coord = mBorderImageOutset.Get(s);
    nscoord value;
    switch (coord.GetUnit()) {
      case eStyleUnit_Coord:
        value = coord.GetCoordValue();
        break;
      case eStyleUnit_Factor:
        value = coord.GetFactorValue() * mComputedBorder.Side(s);
        break;
      default:
        NS_NOTREACHED("unexpected CSS unit for image outset");
        value = 0;
        break;
    }
    outset.Side(s) = value;
  }
  return outset;
}

void
nsStyleBorder::Destroy(nsPresContext* aContext) {
  UntrackImage(aContext);
  this->~nsStyleBorder();
  aContext->PresShell()->
    FreeByObjectID(nsPresArena::nsStyleBorder_id, this);
}

nsChangeHint nsStyleBorder::CalcDifference(const nsStyleBorder& aOther) const
{
  nsChangeHint shadowDifference =
    CalcShadowDifference(mBoxShadow, aOther.mBoxShadow);
  NS_ABORT_IF_FALSE(shadowDifference == unsigned(NS_STYLE_HINT_REFLOW) ||
                    shadowDifference == unsigned(NS_STYLE_HINT_VISUAL) ||
                    shadowDifference == unsigned(NS_STYLE_HINT_NONE),
                    "should do more with shadowDifference");

  // XXXbz we should be able to return a more specific change hint for
  // at least GetComputedBorder() differences...
  if (mTwipsPerPixel != aOther.mTwipsPerPixel ||
      GetComputedBorder() != aOther.GetComputedBorder() ||
      mFloatEdge != aOther.mFloatEdge ||
      mBorderImageOutset != aOther.mBorderImageOutset ||
      (shadowDifference & nsChangeHint_NeedReflow) ||
      mBoxDecorationBreak != aOther.mBoxDecorationBreak)
    return NS_STYLE_HINT_REFLOW;

  NS_FOR_CSS_SIDES(ix) {
    // See the explanation in nsChangeHint.h of
    // nsChangeHint_BorderStyleNoneChange .
    // Furthermore, even though we know *this* side is 0 width, just
    // assume a visual hint for some other change rather than bother
    // tracking this result through the rest of the function.
    if (HasVisibleStyle(ix) != aOther.HasVisibleStyle(ix)) {
      return NS_CombineHint(NS_STYLE_HINT_VISUAL,
                            nsChangeHint_BorderStyleNoneChange);
    }
  }

  // Note that mBorderStyle stores not only the border style but also
  // color-related flags.  Given that we've already done an mComputedBorder
  // comparison, border-style differences can only lead to a VISUAL hint.  So
  // it's OK to just compare the values directly -- if either the actual
  // style or the color flags differ we want to repaint.
  NS_FOR_CSS_SIDES(ix) {
    if (mBorderStyle[ix] != aOther.mBorderStyle[ix] || 
        mBorderColor[ix] != aOther.mBorderColor[ix])
      return NS_STYLE_HINT_VISUAL;
  }

  if (mBorderRadius != aOther.mBorderRadius ||
      !mBorderColors != !aOther.mBorderColors)
    return NS_STYLE_HINT_VISUAL;

  if (IsBorderImageLoaded() || aOther.IsBorderImageLoaded()) {
    if (mBorderImageSource  != aOther.mBorderImageSource  ||
        mBorderImageRepeatH != aOther.mBorderImageRepeatH ||
        mBorderImageRepeatV != aOther.mBorderImageRepeatV ||
        mBorderImageSlice   != aOther.mBorderImageSlice   ||
        mBorderImageFill    != aOther.mBorderImageFill    ||
        mBorderImageWidth   != aOther.mBorderImageWidth   ||
        mBorderImageOutset  != aOther.mBorderImageOutset)
      return NS_STYLE_HINT_VISUAL;
  }

  // Note that at this point if mBorderColors is non-null so is
  // aOther.mBorderColors
  if (mBorderColors) {
    NS_FOR_CSS_SIDES(ix) {
      if (!nsBorderColors::Equal(mBorderColors[ix],
                                 aOther.mBorderColors[ix]))
        return NS_STYLE_HINT_VISUAL;
    }
  }

  if (shadowDifference) {
    return shadowDifference;
  }

  // mBorder is the specified border value.  Changes to this don't
  // need any change processing, since we operate on the computed
  // border values instead.
  if (mBorder != aOther.mBorder) {
    return nsChangeHint_NeutralChange;
  }

  return NS_STYLE_HINT_NONE;
}

nsStyleOutline::nsStyleOutline(nsPresContext* aPresContext)
{
  MOZ_COUNT_CTOR(nsStyleOutline);
  // spacing values not inherited
  nsStyleCoord zero(0, nsStyleCoord::CoordConstructor);
  NS_FOR_CSS_HALF_CORNERS(corner) {
    mOutlineRadius.Set(corner, zero);
  }

  mOutlineOffset = 0;

  mOutlineWidth = nsStyleCoord(NS_STYLE_BORDER_WIDTH_MEDIUM, eStyleUnit_Enumerated);
  mOutlineStyle = NS_STYLE_BORDER_STYLE_NONE;
  mOutlineColor = NS_RGB(0, 0, 0);

  mHasCachedOutline = false;
  mTwipsPerPixel = aPresContext->DevPixelsToAppUnits(1);
}

nsStyleOutline::nsStyleOutline(const nsStyleOutline& aSrc)
  : mOutlineRadius(aSrc.mOutlineRadius)
  , mOutlineWidth(aSrc.mOutlineWidth)
  , mOutlineOffset(aSrc.mOutlineOffset)
  , mCachedOutlineWidth(aSrc.mCachedOutlineWidth)
  , mOutlineColor(aSrc.mOutlineColor)
  , mHasCachedOutline(aSrc.mHasCachedOutline)
  , mOutlineStyle(aSrc.mOutlineStyle)
  , mTwipsPerPixel(aSrc.mTwipsPerPixel)
{
  MOZ_COUNT_CTOR(nsStyleOutline);
}

void 
nsStyleOutline::RecalcData(nsPresContext* aContext)
{
  if (NS_STYLE_BORDER_STYLE_NONE == GetOutlineStyle()) {
    mCachedOutlineWidth = 0;
    mHasCachedOutline = true;
  } else if (IsFixedUnit(mOutlineWidth, true)) {
    // Clamp negative calc() to 0.
    mCachedOutlineWidth =
      std::max(CalcCoord(mOutlineWidth, aContext->GetBorderWidthTable(), 3), 0);
    mCachedOutlineWidth =
      NS_ROUND_BORDER_TO_PIXELS(mCachedOutlineWidth, mTwipsPerPixel);
    mHasCachedOutline = true;
  }
  else
    mHasCachedOutline = false;
}

nsChangeHint nsStyleOutline::CalcDifference(const nsStyleOutline& aOther) const
{
  bool outlineWasVisible =
    mCachedOutlineWidth > 0 && mOutlineStyle != NS_STYLE_BORDER_STYLE_NONE;
  bool outlineIsVisible = 
    aOther.mCachedOutlineWidth > 0 && aOther.mOutlineStyle != NS_STYLE_BORDER_STYLE_NONE;
  if (outlineWasVisible != outlineIsVisible ||
      (outlineIsVisible && (mOutlineOffset != aOther.mOutlineOffset ||
                            mOutlineWidth != aOther.mOutlineWidth ||
                            mTwipsPerPixel != aOther.mTwipsPerPixel))) {
    return NS_CombineHint(nsChangeHint_AllReflowHints,
                          nsChangeHint_RepaintFrame);
  }

  if ((mOutlineStyle != aOther.mOutlineStyle) ||
      (mOutlineColor != aOther.mOutlineColor) ||
      (mOutlineRadius != aOther.mOutlineRadius)) {
    return nsChangeHint_RepaintFrame;
  }

  // XXX Not exactly sure if we need to check the cached outline values.
  if (mOutlineWidth != aOther.mOutlineWidth ||
      mOutlineOffset != aOther.mOutlineOffset ||
      mTwipsPerPixel != aOther.mTwipsPerPixel ||
      mHasCachedOutline != aOther.mHasCachedOutline ||
      (mHasCachedOutline &&
       (mCachedOutlineWidth != aOther.mCachedOutlineWidth))) {
    return nsChangeHint_NeutralChange;
  }

  return NS_STYLE_HINT_NONE;
}

// --------------------
// nsStyleList
//
nsStyleList::nsStyleList(nsPresContext* aPresContext) 
  : mListStylePosition(NS_STYLE_LIST_STYLE_POSITION_OUTSIDE),
    mListStyleType(NS_LITERAL_STRING("disc")),
    mCounterStyle(aPresContext->CounterStyleManager()->
                  BuildCounterStyle(mListStyleType))
{
  MOZ_COUNT_CTOR(nsStyleList);
}

nsStyleList::~nsStyleList() 
{
  MOZ_COUNT_DTOR(nsStyleList);
}

nsStyleList::nsStyleList(const nsStyleList& aSource)
  : mListStylePosition(aSource.mListStylePosition),
    mListStyleType(aSource.mListStyleType),
    mCounterStyle(aSource.mCounterStyle),
    mImageRegion(aSource.mImageRegion)
{
  SetListStyleImage(aSource.GetListStyleImage());
  MOZ_COUNT_CTOR(nsStyleList);
}

nsChangeHint nsStyleList::CalcDifference(const nsStyleList& aOther) const
{
  if (mListStylePosition != aOther.mListStylePosition)
    return NS_STYLE_HINT_FRAMECHANGE;
  if (EqualImages(mListStyleImage, aOther.mListStyleImage) &&
      mCounterStyle == aOther.mCounterStyle) {
    if (mImageRegion.IsEqualInterior(aOther.mImageRegion)) {
      if (mListStyleType != aOther.mListStyleType)
        return nsChangeHint_NeutralChange;
      return NS_STYLE_HINT_NONE;
    }
    if (mImageRegion.width == aOther.mImageRegion.width &&
        mImageRegion.height == aOther.mImageRegion.height)
      return NS_STYLE_HINT_VISUAL;
  }
  return NS_STYLE_HINT_REFLOW;
}

// --------------------
// nsStyleXUL
//
nsStyleXUL::nsStyleXUL() 
{ 
  MOZ_COUNT_CTOR(nsStyleXUL);
  mBoxAlign  = NS_STYLE_BOX_ALIGN_STRETCH;
  mBoxDirection = NS_STYLE_BOX_DIRECTION_NORMAL;
  mBoxFlex = 0.0f;
  mBoxOrient = NS_STYLE_BOX_ORIENT_HORIZONTAL;
  mBoxPack   = NS_STYLE_BOX_PACK_START;
  mBoxOrdinal = 1;
  mStretchStack = true;
}

nsStyleXUL::~nsStyleXUL() 
{
  MOZ_COUNT_DTOR(nsStyleXUL);
}

nsStyleXUL::nsStyleXUL(const nsStyleXUL& aSource)
  : mBoxFlex(aSource.mBoxFlex)
  , mBoxOrdinal(aSource.mBoxOrdinal)
  , mBoxAlign(aSource.mBoxAlign)
  , mBoxDirection(aSource.mBoxDirection)
  , mBoxOrient(aSource.mBoxOrient)
  , mBoxPack(aSource.mBoxPack)
  , mStretchStack(aSource.mStretchStack)
{
  MOZ_COUNT_CTOR(nsStyleXUL);
}

nsChangeHint nsStyleXUL::CalcDifference(const nsStyleXUL& aOther) const
{
  if (mBoxAlign == aOther.mBoxAlign &&
      mBoxDirection == aOther.mBoxDirection &&
      mBoxFlex == aOther.mBoxFlex &&
      mBoxOrient == aOther.mBoxOrient &&
      mBoxPack == aOther.mBoxPack &&
      mBoxOrdinal == aOther.mBoxOrdinal &&
      mStretchStack == aOther.mStretchStack)
    return NS_STYLE_HINT_NONE;
  if (mBoxOrdinal != aOther.mBoxOrdinal)
    return NS_STYLE_HINT_FRAMECHANGE;
  return NS_STYLE_HINT_REFLOW;
}

// --------------------
// nsStyleColumn
//
/* static */ const uint32_t nsStyleColumn::kMaxColumnCount = 1000;

nsStyleColumn::nsStyleColumn(nsPresContext* aPresContext)
{
  MOZ_COUNT_CTOR(nsStyleColumn);
  mColumnCount = NS_STYLE_COLUMN_COUNT_AUTO;
  mColumnWidth.SetAutoValue();
  mColumnGap.SetNormalValue();
  mColumnFill = NS_STYLE_COLUMN_FILL_BALANCE;

  mColumnRuleWidth = (aPresContext->GetBorderWidthTable())[NS_STYLE_BORDER_WIDTH_MEDIUM];
  mColumnRuleStyle = NS_STYLE_BORDER_STYLE_NONE;
  mColumnRuleColor = NS_RGB(0, 0, 0);
  mColumnRuleColorIsForeground = true;

  mTwipsPerPixel = aPresContext->AppUnitsPerDevPixel();
}

nsStyleColumn::~nsStyleColumn() 
{
  MOZ_COUNT_DTOR(nsStyleColumn);
}

nsStyleColumn::nsStyleColumn(const nsStyleColumn& aSource)
  : mColumnCount(aSource.mColumnCount)
  , mColumnWidth(aSource.mColumnWidth)
  , mColumnGap(aSource.mColumnGap)
  , mColumnRuleColor(aSource.mColumnRuleColor)
  , mColumnRuleStyle(aSource.mColumnRuleStyle)
  , mColumnFill(aSource.mColumnFill)
  , mColumnRuleColorIsForeground(aSource.mColumnRuleColorIsForeground)
  , mColumnRuleWidth(aSource.mColumnRuleWidth)
  , mTwipsPerPixel(aSource.mTwipsPerPixel)
{
  MOZ_COUNT_CTOR(nsStyleColumn);
}

nsChangeHint nsStyleColumn::CalcDifference(const nsStyleColumn& aOther) const
{
  if ((mColumnWidth.GetUnit() == eStyleUnit_Auto)
      != (aOther.mColumnWidth.GetUnit() == eStyleUnit_Auto) ||
      mColumnCount != aOther.mColumnCount)
    // We force column count changes to do a reframe, because it's tricky to handle
    // some edge cases where the column count gets smaller and content overflows.
    // XXX not ideal
    return NS_STYLE_HINT_FRAMECHANGE;

  if (mColumnWidth != aOther.mColumnWidth ||
      mColumnGap != aOther.mColumnGap ||
      mColumnFill != aOther.mColumnFill)
    return NS_STYLE_HINT_REFLOW;

  if (GetComputedColumnRuleWidth() != aOther.GetComputedColumnRuleWidth() ||
      mColumnRuleStyle != aOther.mColumnRuleStyle ||
      mColumnRuleColor != aOther.mColumnRuleColor ||
      mColumnRuleColorIsForeground != aOther.mColumnRuleColorIsForeground)
    return NS_STYLE_HINT_VISUAL;

  // XXX Is it right that we never check mTwipsPerPixel to return a
  // non-nsChangeHint_NeutralChange hint?
  if (mColumnRuleWidth != aOther.mColumnRuleWidth ||
      mTwipsPerPixel != aOther.mTwipsPerPixel) {
    return nsChangeHint_NeutralChange;
  }

  return NS_STYLE_HINT_NONE;
}

// --------------------
// nsStyleSVG
//
nsStyleSVG::nsStyleSVG() 
{
    MOZ_COUNT_CTOR(nsStyleSVG);
    mFill.mType              = eStyleSVGPaintType_Color;
    mFill.mPaint.mColor      = NS_RGB(0,0,0);
    mFill.mFallbackColor     = NS_RGB(0,0,0);
    mStroke.mType            = eStyleSVGPaintType_None;
    mStroke.mPaint.mColor    = NS_RGB(0,0,0);
    mStroke.mFallbackColor   = NS_RGB(0,0,0);
    mStrokeDasharray         = nullptr;

    mStrokeDashoffset.SetCoordValue(0);
    mStrokeWidth.SetCoordValue(nsPresContext::CSSPixelsToAppUnits(1));

    mFillOpacity             = 1.0f;
    mStrokeMiterlimit        = 4.0f;
    mStrokeOpacity           = 1.0f;

    mStrokeDasharrayLength   = 0;
    mClipRule                = NS_STYLE_FILL_RULE_NONZERO;
    mColorInterpolation      = NS_STYLE_COLOR_INTERPOLATION_SRGB;
    mColorInterpolationFilters = NS_STYLE_COLOR_INTERPOLATION_LINEARRGB;
    mFillRule                = NS_STYLE_FILL_RULE_NONZERO;
    mImageRendering          = NS_STYLE_IMAGE_RENDERING_AUTO;
    mPaintOrder              = NS_STYLE_PAINT_ORDER_NORMAL;
    mShapeRendering          = NS_STYLE_SHAPE_RENDERING_AUTO;
    mStrokeLinecap           = NS_STYLE_STROKE_LINECAP_BUTT;
    mStrokeLinejoin          = NS_STYLE_STROKE_LINEJOIN_MITER;
    mTextAnchor              = NS_STYLE_TEXT_ANCHOR_START;
    mTextRendering           = NS_STYLE_TEXT_RENDERING_AUTO;
    mFillOpacitySource       = eStyleSVGOpacitySource_Normal;
    mStrokeOpacitySource     = eStyleSVGOpacitySource_Normal;
    mStrokeDasharrayFromObject = false;
    mStrokeDashoffsetFromObject = false;
    mStrokeWidthFromObject   = false;
}

nsStyleSVG::~nsStyleSVG() 
{
  MOZ_COUNT_DTOR(nsStyleSVG);
  delete [] mStrokeDasharray;
}

nsStyleSVG::nsStyleSVG(const nsStyleSVG& aSource)
{
  MOZ_COUNT_CTOR(nsStyleSVG);
  mFill = aSource.mFill;
  mStroke = aSource.mStroke;

  mMarkerEnd = aSource.mMarkerEnd;
  mMarkerMid = aSource.mMarkerMid;
  mMarkerStart = aSource.mMarkerStart;

  mStrokeDasharrayLength = aSource.mStrokeDasharrayLength;
  if (aSource.mStrokeDasharray) {
    mStrokeDasharray = new nsStyleCoord[mStrokeDasharrayLength];
    if (mStrokeDasharray) {
      for (size_t i = 0; i < mStrokeDasharrayLength; i++) {
        mStrokeDasharray[i] = aSource.mStrokeDasharray[i];
      }
    } else {
      mStrokeDasharrayLength = 0;
    }
  } else {
    mStrokeDasharray = nullptr;
  }

  mStrokeDashoffset = aSource.mStrokeDashoffset;
  mStrokeWidth = aSource.mStrokeWidth;

  mFillOpacity = aSource.mFillOpacity;
  mStrokeMiterlimit = aSource.mStrokeMiterlimit;
  mStrokeOpacity = aSource.mStrokeOpacity;

  mClipRule = aSource.mClipRule;
  mColorInterpolation = aSource.mColorInterpolation;
  mColorInterpolationFilters = aSource.mColorInterpolationFilters;
  mFillRule = aSource.mFillRule;
  mImageRendering = aSource.mImageRendering;
  mPaintOrder = aSource.mPaintOrder;
  mShapeRendering = aSource.mShapeRendering;
  mStrokeLinecap = aSource.mStrokeLinecap;
  mStrokeLinejoin = aSource.mStrokeLinejoin;
  mTextAnchor = aSource.mTextAnchor;
  mTextRendering = aSource.mTextRendering;
  mFillOpacitySource = aSource.mFillOpacitySource;
  mStrokeOpacitySource = aSource.mStrokeOpacitySource;
  mStrokeDasharrayFromObject = aSource.mStrokeDasharrayFromObject;
  mStrokeDashoffsetFromObject = aSource.mStrokeDashoffsetFromObject;
  mStrokeWidthFromObject = aSource.mStrokeWidthFromObject;
}

static bool PaintURIChanged(const nsStyleSVGPaint& aPaint1,
                              const nsStyleSVGPaint& aPaint2)
{
  if (aPaint1.mType != aPaint2.mType) {
    return aPaint1.mType == eStyleSVGPaintType_Server ||
           aPaint2.mType == eStyleSVGPaintType_Server;
  }
  return aPaint1.mType == eStyleSVGPaintType_Server &&
    !EqualURIs(aPaint1.mPaint.mPaintServer, aPaint2.mPaint.mPaintServer);
}

nsChangeHint nsStyleSVG::CalcDifference(const nsStyleSVG& aOther) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (!EqualURIs(mMarkerEnd, aOther.mMarkerEnd) ||
      !EqualURIs(mMarkerMid, aOther.mMarkerMid) ||
      !EqualURIs(mMarkerStart, aOther.mMarkerStart)) {
    // Markers currently contribute to nsSVGPathGeometryFrame::mRect,
    // so we need a reflow as well as a repaint. No intrinsic sizes need
    // to change, so nsChangeHint_NeedReflow is sufficient.
    NS_UpdateHint(hint, nsChangeHint_UpdateEffects);
    NS_UpdateHint(hint, nsChangeHint_NeedReflow);
    NS_UpdateHint(hint, nsChangeHint_NeedDirtyReflow); // XXX remove me: bug 876085
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
    return hint;
  }

  if (mFill != aOther.mFill ||
      mStroke != aOther.mStroke ||
      mFillOpacity != aOther.mFillOpacity ||
      mStrokeOpacity != aOther.mStrokeOpacity) {
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
    if (HasStroke() != aOther.HasStroke() ||
        (!HasStroke() && HasFill() != aOther.HasFill())) {
      // Frame bounds and overflow rects depend on whether we "have" fill or
      // stroke. Whether we have stroke or not just changed, or else we have no
      // stroke (in which case whether we have fill or not is significant to frame
      // bounds) and whether we have fill or not just changed. In either case we
      // need to reflow so the frame rect is updated.
      // XXXperf this is a waste on non nsSVGPathGeometryFrames.
      NS_UpdateHint(hint, nsChangeHint_NeedReflow);
      NS_UpdateHint(hint, nsChangeHint_NeedDirtyReflow); // XXX remove me: bug 876085
    }
    if (PaintURIChanged(mFill, aOther.mFill) ||
        PaintURIChanged(mStroke, aOther.mStroke)) {
      NS_UpdateHint(hint, nsChangeHint_UpdateEffects);
    }
  }

  // Stroke currently contributes to nsSVGPathGeometryFrame::mRect, so
  // we need a reflow here. No intrinsic sizes need to change, so
  // nsChangeHint_NeedReflow is sufficient.
  // Note that stroke-dashoffset does not affect nsSVGPathGeometryFrame::mRect.
  // text-anchor and text-rendering changes also require a reflow since they
  // change frames' rects.
  if (mStrokeWidth           != aOther.mStrokeWidth           ||
      mStrokeMiterlimit      != aOther.mStrokeMiterlimit      ||
      mStrokeLinecap         != aOther.mStrokeLinecap         ||
      mStrokeLinejoin        != aOther.mStrokeLinejoin        ||
      mTextAnchor            != aOther.mTextAnchor            ||
      mTextRendering         != aOther.mTextRendering) {
    NS_UpdateHint(hint, nsChangeHint_NeedReflow);
    NS_UpdateHint(hint, nsChangeHint_NeedDirtyReflow); // XXX remove me: bug 876085
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
    return hint;
  }

  if (hint & nsChangeHint_RepaintFrame) {
    return hint; // we don't add anything else below
  }

  if ( mStrokeDashoffset      != aOther.mStrokeDashoffset      ||
       mClipRule              != aOther.mClipRule              ||
       mColorInterpolation    != aOther.mColorInterpolation    ||
       mColorInterpolationFilters != aOther.mColorInterpolationFilters ||
       mFillRule              != aOther.mFillRule              ||
       mImageRendering        != aOther.mImageRendering        ||
       mPaintOrder            != aOther.mPaintOrder            ||
       mShapeRendering        != aOther.mShapeRendering        ||
       mStrokeDasharrayLength != aOther.mStrokeDasharrayLength ||
       mFillOpacitySource     != aOther.mFillOpacitySource     ||
       mStrokeOpacitySource   != aOther.mStrokeOpacitySource   ||
       mStrokeDasharrayFromObject != aOther.mStrokeDasharrayFromObject ||
       mStrokeDashoffsetFromObject != aOther.mStrokeDashoffsetFromObject ||
       mStrokeWidthFromObject != aOther.mStrokeWidthFromObject) {
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
    return hint;
  }

  // length of stroke dasharrays are the same (tested above) - check entries
  for (uint32_t i=0; i<mStrokeDasharrayLength; i++)
    if (mStrokeDasharray[i] != aOther.mStrokeDasharray[i]) {
      NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
      return hint;
    }

  return hint;
}

// --------------------
// nsStyleClipPath
//
nsStyleClipPath::nsStyleClipPath()
  : mType(NS_STYLE_CLIP_PATH_NONE)
  , mURL(nullptr)
  , mSizingBox(NS_STYLE_CLIP_SHAPE_SIZING_NOBOX)
{
}

nsStyleClipPath::nsStyleClipPath(const nsStyleClipPath& aSource)
  : mType(NS_STYLE_CLIP_PATH_NONE)
  , mURL(nullptr)
  , mSizingBox(NS_STYLE_CLIP_SHAPE_SIZING_NOBOX)
{
  if (aSource.mType == NS_STYLE_CLIP_PATH_URL) {
    SetURL(aSource.mURL);
  } else if (aSource.mType == NS_STYLE_CLIP_PATH_SHAPE) {
    SetBasicShape(aSource.mBasicShape, aSource.mSizingBox);
  } else if (aSource.mType == NS_STYLE_CLIP_PATH_SHAPE) {
    SetSizingBox(aSource.mSizingBox);
  }
}

nsStyleClipPath::~nsStyleClipPath()
{
  ReleaseRef();
}

nsStyleClipPath&
nsStyleClipPath::operator=(const nsStyleClipPath& aOther)
{
  if (this == &aOther) {
    return *this;
  }

  ReleaseRef();

  if (aOther.mType == NS_STYLE_CLIP_PATH_URL) {
    SetURL(aOther.mURL);
  } else if (aOther.mType == NS_STYLE_CLIP_PATH_SHAPE) {
    SetBasicShape(aOther.mBasicShape, aOther.mSizingBox);
  } else if (aOther.mType == NS_STYLE_CLIP_PATH_BOX) {
    SetSizingBox(aOther.mSizingBox);
  } else {
    mSizingBox = NS_STYLE_CLIP_SHAPE_SIZING_NOBOX;
    mType = NS_STYLE_CLIP_PATH_NONE;
  }
  return *this;
}


bool
nsStyleClipPath::operator==(const nsStyleClipPath& aOther) const
{
  if (mType != aOther.mType) {
    return false;
  }

  if (mType == NS_STYLE_CLIP_PATH_URL) {
    return EqualURIs(mURL, aOther.mURL);
  } else if (mType == NS_STYLE_CLIP_PATH_SHAPE) {
    return *mBasicShape == *aOther.mBasicShape &&
           mSizingBox == aOther.mSizingBox;
  } else if (mType == NS_STYLE_CLIP_PATH_BOX) {
    return mSizingBox == aOther.mSizingBox;
  }

  return true;
}

void
nsStyleClipPath::ReleaseRef()
{
  if (mType == NS_STYLE_CLIP_PATH_SHAPE) {
    NS_ASSERTION(mBasicShape, "expected pointer");
    mBasicShape->Release();
  } else if (mType == NS_STYLE_CLIP_PATH_URL) {
    NS_ASSERTION(mURL, "expected pointer");
    mURL->Release();
  }
  mURL = nullptr;
}

void
nsStyleClipPath::SetURL(nsIURI* aURL)
{
  NS_ASSERTION(aURL, "expected pointer");
  ReleaseRef();
  mURL = aURL;
  mURL->AddRef();
  mType = NS_STYLE_CLIP_PATH_URL;
}

void
nsStyleClipPath::SetBasicShape(nsStyleBasicShape* aBasicShape, uint8_t aSizingBox)
{
  NS_ASSERTION(aBasicShape, "expected pointer");
  ReleaseRef();
  mBasicShape = aBasicShape;
  mBasicShape->AddRef();
  mSizingBox = aSizingBox;
  mType = NS_STYLE_CLIP_PATH_SHAPE;
}

void
nsStyleClipPath::SetSizingBox(uint8_t aSizingBox)
{
  ReleaseRef();
  mSizingBox = aSizingBox;
  mType = NS_STYLE_CLIP_PATH_BOX;
}

// --------------------
// nsStyleFilter
//
nsStyleFilter::nsStyleFilter()
  : mType(NS_STYLE_FILTER_NONE)
  , mDropShadow(nullptr)
{
  MOZ_COUNT_CTOR(nsStyleFilter);
}

nsStyleFilter::nsStyleFilter(const nsStyleFilter& aSource)
  : mType(NS_STYLE_FILTER_NONE)
  , mDropShadow(nullptr)
{
  MOZ_COUNT_CTOR(nsStyleFilter);
  if (aSource.mType == NS_STYLE_FILTER_URL) {
    SetURL(aSource.mURL);
  } else if (aSource.mType == NS_STYLE_FILTER_DROP_SHADOW) {
    SetDropShadow(aSource.mDropShadow);
  } else if (aSource.mType != NS_STYLE_FILTER_NONE) {
    SetFilterParameter(aSource.mFilterParameter, aSource.mType);
  }
}

nsStyleFilter::~nsStyleFilter()
{
  ReleaseRef();
  MOZ_COUNT_DTOR(nsStyleFilter);
}

nsStyleFilter&
nsStyleFilter::operator=(const nsStyleFilter& aOther)
{
  if (this == &aOther)
    return *this;

  if (aOther.mType == NS_STYLE_FILTER_URL) {
    SetURL(aOther.mURL);
  } else if (aOther.mType == NS_STYLE_FILTER_DROP_SHADOW) {
    SetDropShadow(aOther.mDropShadow);
  } else if (aOther.mType != NS_STYLE_FILTER_NONE) {
    SetFilterParameter(aOther.mFilterParameter, aOther.mType);
  }
  return *this;
}


bool
nsStyleFilter::operator==(const nsStyleFilter& aOther) const
{
  if (mType != aOther.mType) {
      return false;
  }

  if (mType == NS_STYLE_FILTER_URL) {
    return EqualURIs(mURL, aOther.mURL);
  } else if (mType == NS_STYLE_FILTER_DROP_SHADOW) {
    return *mDropShadow == *aOther.mDropShadow;
  } else if (mType != NS_STYLE_FILTER_NONE) {
    return mFilterParameter == aOther.mFilterParameter;
  }

  return true;
}

void
nsStyleFilter::ReleaseRef()
{
  if (mType == NS_STYLE_FILTER_DROP_SHADOW) {
    NS_ASSERTION(mDropShadow, "expected pointer");
    mDropShadow->Release();
  } else if (mType == NS_STYLE_FILTER_URL) {
    NS_ASSERTION(mURL, "expected pointer");
    mURL->Release();
  }
}

void
nsStyleFilter::SetFilterParameter(const nsStyleCoord& aFilterParameter,
                                  int32_t aType)
{
  ReleaseRef();
  mFilterParameter = aFilterParameter;
  mType = aType;
}

void
nsStyleFilter::SetURL(nsIURI* aURL)
{
  NS_ASSERTION(aURL, "expected pointer");
  ReleaseRef();
  mURL = aURL;
  mURL->AddRef();
  mType = NS_STYLE_FILTER_URL;
}

void
nsStyleFilter::SetDropShadow(nsCSSShadowArray* aDropShadow)
{
  NS_ASSERTION(aDropShadow, "expected pointer");
  ReleaseRef();
  mDropShadow = aDropShadow;
  mDropShadow->AddRef();
  mType = NS_STYLE_FILTER_DROP_SHADOW;
}

// --------------------
// nsStyleSVGReset
//
nsStyleSVGReset::nsStyleSVGReset() 
{
    MOZ_COUNT_CTOR(nsStyleSVGReset);
    mStopColor               = NS_RGB(0,0,0);
    mFloodColor              = NS_RGB(0,0,0);
    mLightingColor           = NS_RGB(255,255,255);
    mMask                    = nullptr;
    mStopOpacity             = 1.0f;
    mFloodOpacity            = 1.0f;
    mDominantBaseline        = NS_STYLE_DOMINANT_BASELINE_AUTO;
    mVectorEffect            = NS_STYLE_VECTOR_EFFECT_NONE;
    mMaskType                = NS_STYLE_MASK_TYPE_LUMINANCE;
}

nsStyleSVGReset::~nsStyleSVGReset() 
{
  MOZ_COUNT_DTOR(nsStyleSVGReset);
}

nsStyleSVGReset::nsStyleSVGReset(const nsStyleSVGReset& aSource)
{
  MOZ_COUNT_CTOR(nsStyleSVGReset);
  mStopColor = aSource.mStopColor;
  mFloodColor = aSource.mFloodColor;
  mLightingColor = aSource.mLightingColor;
  mClipPath = aSource.mClipPath;
  mFilters = aSource.mFilters;
  mMask = aSource.mMask;
  mStopOpacity = aSource.mStopOpacity;
  mFloodOpacity = aSource.mFloodOpacity;
  mDominantBaseline = aSource.mDominantBaseline;
  mVectorEffect = aSource.mVectorEffect;
  mMaskType = aSource.mMaskType;
}

nsChangeHint nsStyleSVGReset::CalcDifference(const nsStyleSVGReset& aOther) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (mClipPath != aOther.mClipPath ||
      !EqualURIs(mMask, aOther.mMask) ||
      mFilters != aOther.mFilters) {
    NS_UpdateHint(hint, nsChangeHint_UpdateEffects);
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
    // We only actually need to update the overflow area for filter
    // changes.  However, mask and clip-path changes require that we
    // update the PreEffectsBBoxProperty, which is done during overflow
    // computation.
    NS_UpdateHint(hint, nsChangeHint_UpdateOverflow);
  }

  if (mDominantBaseline != aOther.mDominantBaseline) {
    // XXXjwatt: why NS_STYLE_HINT_REFLOW? Isn't that excessive?
    NS_UpdateHint(hint, NS_STYLE_HINT_REFLOW);
  } else if (mVectorEffect  != aOther.mVectorEffect) {
    // Stroke currently affects nsSVGPathGeometryFrame::mRect, and
    // vector-effect affect stroke. As a result we need to reflow if
    // vector-effect changes in order to have nsSVGPathGeometryFrame::
    // ReflowSVG called to update its mRect. No intrinsic sizes need
    // to change so nsChangeHint_NeedReflow is sufficient.
    NS_UpdateHint(hint, nsChangeHint_NeedReflow);
    NS_UpdateHint(hint, nsChangeHint_NeedDirtyReflow); // XXX remove me: bug 876085
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
  } else if (mStopColor     != aOther.mStopColor     ||
             mFloodColor    != aOther.mFloodColor    ||
             mLightingColor != aOther.mLightingColor ||
             mStopOpacity   != aOther.mStopOpacity   ||
             mFloodOpacity  != aOther.mFloodOpacity  ||
             mMaskType      != aOther.mMaskType) {
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
  }

  return hint;
}

// nsStyleSVGPaint implementation
nsStyleSVGPaint::~nsStyleSVGPaint()
{
  if (mType == eStyleSVGPaintType_Server) {
    NS_IF_RELEASE(mPaint.mPaintServer);
  }
}

void
nsStyleSVGPaint::SetType(nsStyleSVGPaintType aType)
{
  if (mType == eStyleSVGPaintType_Server) {
    this->~nsStyleSVGPaint();
    new (this) nsStyleSVGPaint();
  }
  mType = aType;
}

nsStyleSVGPaint& nsStyleSVGPaint::operator=(const nsStyleSVGPaint& aOther) 
{
  if (this == &aOther)
    return *this;

  SetType(aOther.mType);

  mFallbackColor = aOther.mFallbackColor;
  if (mType == eStyleSVGPaintType_Server) {
    mPaint.mPaintServer = aOther.mPaint.mPaintServer;
    NS_IF_ADDREF(mPaint.mPaintServer);
  } else {
    mPaint.mColor = aOther.mPaint.mColor;
  }
  return *this;
}

bool nsStyleSVGPaint::operator==(const nsStyleSVGPaint& aOther) const
{
  if (mType != aOther.mType)
    return false;
  if (mType == eStyleSVGPaintType_Server)
    return EqualURIs(mPaint.mPaintServer, aOther.mPaint.mPaintServer) &&
           mFallbackColor == aOther.mFallbackColor;
  if (mType == eStyleSVGPaintType_Color)
    return mPaint.mColor == aOther.mPaint.mColor;
  return true;
}


// --------------------
// nsStylePosition
//
nsStylePosition::nsStylePosition(void)
{
  MOZ_COUNT_CTOR(nsStylePosition);

  // positioning values not inherited

  mObjectPosition.SetInitialPercentValues(0.5f);

  nsStyleCoord  autoCoord(eStyleUnit_Auto);
  mOffset.SetLeft(autoCoord);
  mOffset.SetTop(autoCoord);
  mOffset.SetRight(autoCoord);
  mOffset.SetBottom(autoCoord);
  mWidth.SetAutoValue();
  mMinWidth.SetAutoValue();
  mMaxWidth.SetNoneValue();
  mHeight.SetAutoValue();
  mMinHeight.SetAutoValue();
  mMaxHeight.SetNoneValue();
  mFlexBasis.SetIntValue(NS_STYLE_FLEX_BASIS_MAIN_SIZE, eStyleUnit_Enumerated);

  // The initial value of grid-auto-columns and grid-auto-rows is 'auto',
  // which computes to 'minmax(min-content, max-content)'.
  mGridAutoColumnsMin.SetIntValue(NS_STYLE_GRID_TRACK_BREADTH_MIN_CONTENT,
                                  eStyleUnit_Enumerated);
  mGridAutoColumnsMax.SetIntValue(NS_STYLE_GRID_TRACK_BREADTH_MAX_CONTENT,
                                  eStyleUnit_Enumerated);
  mGridAutoRowsMin.SetIntValue(NS_STYLE_GRID_TRACK_BREADTH_MIN_CONTENT,
                               eStyleUnit_Enumerated);
  mGridAutoRowsMax.SetIntValue(NS_STYLE_GRID_TRACK_BREADTH_MAX_CONTENT,
                               eStyleUnit_Enumerated);

  mGridAutoFlow = NS_STYLE_GRID_AUTO_FLOW_ROW;
  mBoxSizing = NS_STYLE_BOX_SIZING_CONTENT;
  mAlignContent = NS_STYLE_ALIGN_CONTENT_STRETCH;
  mAlignItems = NS_STYLE_ALIGN_ITEMS_INITIAL_VALUE;
  mAlignSelf = NS_STYLE_ALIGN_SELF_AUTO;
  mFlexDirection = NS_STYLE_FLEX_DIRECTION_ROW;
  mFlexWrap = NS_STYLE_FLEX_WRAP_NOWRAP;
  mJustifyContent = NS_STYLE_JUSTIFY_CONTENT_FLEX_START;
  mObjectFit = NS_STYLE_OBJECT_FIT_FILL;
  mOrder = NS_STYLE_ORDER_INITIAL;
  mFlexGrow = 0.0f;
  mFlexShrink = 1.0f;
  mZIndex.SetAutoValue();
  // Other members get their default constructors
  // which initialize them to representations of their respective initial value.
  // mGridTemplateAreas: nullptr for 'none'
  // mGridTemplate{Rows,Columns}: false and empty arrays for 'none'
  // mGrid{Column,Row}{Start,End}: false/0/empty values for 'auto'
}

nsStylePosition::~nsStylePosition(void)
{
  MOZ_COUNT_DTOR(nsStylePosition);
}

nsStylePosition::nsStylePosition(const nsStylePosition& aSource)
  : mObjectPosition(aSource.mObjectPosition)
  , mOffset(aSource.mOffset)
  , mWidth(aSource.mWidth)
  , mMinWidth(aSource.mMinWidth)
  , mMaxWidth(aSource.mMaxWidth)
  , mHeight(aSource.mHeight)
  , mMinHeight(aSource.mMinHeight)
  , mMaxHeight(aSource.mMaxHeight)
  , mFlexBasis(aSource.mFlexBasis)
  , mGridAutoColumnsMin(aSource.mGridAutoColumnsMin)
  , mGridAutoColumnsMax(aSource.mGridAutoColumnsMax)
  , mGridAutoRowsMin(aSource.mGridAutoRowsMin)
  , mGridAutoRowsMax(aSource.mGridAutoRowsMax)
  , mGridAutoFlow(aSource.mGridAutoFlow)
  , mBoxSizing(aSource.mBoxSizing)
  , mAlignContent(aSource.mAlignContent)
  , mAlignItems(aSource.mAlignItems)
  , mAlignSelf(aSource.mAlignSelf)
  , mFlexDirection(aSource.mFlexDirection)
  , mFlexWrap(aSource.mFlexWrap)
  , mJustifyContent(aSource.mJustifyContent)
  , mObjectFit(aSource.mObjectFit)
  , mOrder(aSource.mOrder)
  , mFlexGrow(aSource.mFlexGrow)
  , mFlexShrink(aSource.mFlexShrink)
  , mZIndex(aSource.mZIndex)
  , mGridTemplateColumns(aSource.mGridTemplateColumns)
  , mGridTemplateRows(aSource.mGridTemplateRows)
  , mGridTemplateAreas(aSource.mGridTemplateAreas)
  , mGridColumnStart(aSource.mGridColumnStart)
  , mGridColumnEnd(aSource.mGridColumnEnd)
  , mGridRowStart(aSource.mGridRowStart)
  , mGridRowEnd(aSource.mGridRowEnd)
{
  MOZ_COUNT_CTOR(nsStylePosition);
}

static bool
IsAutonessEqual(const nsStyleSides& aSides1, const nsStyleSides& aSides2)
{
  NS_FOR_CSS_SIDES(side) {
    if ((aSides1.GetUnit(side) == eStyleUnit_Auto) !=
        (aSides2.GetUnit(side) == eStyleUnit_Auto)) {
      return false;
    }
  }
  return true;
}

nsChangeHint nsStylePosition::CalcDifference(const nsStylePosition& aOther) const
{
  nsChangeHint hint = nsChangeHint(0);

  // Changes to "z-index", "object-fit", & "object-position" require a repaint.
  if (mZIndex != aOther.mZIndex ||
      mObjectFit != aOther.mObjectFit ||
      mObjectPosition != aOther.mObjectPosition) {
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
  }

  if (mOrder != aOther.mOrder) {
    // "order" impacts both layout order and stacking order, so we need both a
    // reflow and a repaint when it changes.  (Technically, we only need a
    // reflow if we're in a multi-line flexbox (which we can't be sure about,
    // since that's determined by styling on our parent) -- there, "order" can
    // affect which flex line we end up on, & hence can affect our sizing by
    // changing the group of flex items we're competing with for space.)
    return NS_CombineHint(hint, NS_CombineHint(nsChangeHint_RepaintFrame,
                                               nsChangeHint_AllReflowHints));
  }

  if (mBoxSizing != aOther.mBoxSizing) {
    // Can affect both widths and heights; just a bad scene.
    return NS_CombineHint(hint, nsChangeHint_AllReflowHints);
  }

  // Properties that apply to flex items:
  // XXXdholbert These should probably be more targeted (bug 819536)
  if (mAlignSelf != aOther.mAlignSelf ||
      mFlexBasis != aOther.mFlexBasis ||
      mFlexGrow != aOther.mFlexGrow ||
      mFlexShrink != aOther.mFlexShrink) {
    return NS_CombineHint(hint, nsChangeHint_AllReflowHints);
  }

  // Properties that apply to flex containers:
  // - flex-direction can swap a flex container between vertical & horizontal.
  // - align-items can change the sizing of a flex container & the positioning
  //   of its children.
  // - flex-wrap changes whether a flex container's children are wrapped, which
  //   impacts their sizing/positioning and hence impacts the container's size.
  if (mAlignItems != aOther.mAlignItems ||
      mFlexDirection != aOther.mFlexDirection ||
      mFlexWrap != aOther.mFlexWrap) {
    return NS_CombineHint(hint, nsChangeHint_AllReflowHints);
  }

  // Properties that apply to grid containers:
  // FIXME: only for grid containers
  // (ie. 'display: grid' or 'display: inline-grid')
  if (mGridTemplateColumns != aOther.mGridTemplateColumns ||
      mGridTemplateRows != aOther.mGridTemplateRows ||
      mGridTemplateAreas != aOther.mGridTemplateAreas ||
      mGridAutoColumnsMin != aOther.mGridAutoColumnsMin ||
      mGridAutoColumnsMax != aOther.mGridAutoColumnsMax ||
      mGridAutoRowsMin != aOther.mGridAutoRowsMin ||
      mGridAutoRowsMax != aOther.mGridAutoRowsMax ||
      mGridAutoFlow != aOther.mGridAutoFlow) {
    return NS_CombineHint(hint, nsChangeHint_AllReflowHints);
  }

  // Properties that apply to grid items:
  // FIXME: only for grid items
  // (ie. parent frame is 'display: grid' or 'display: inline-grid')
  if (mGridColumnStart != aOther.mGridColumnStart ||
      mGridColumnEnd != aOther.mGridColumnEnd ||
      mGridRowStart != aOther.mGridRowStart ||
      mGridRowEnd != aOther.mGridRowEnd) {
    return NS_CombineHint(hint, nsChangeHint_AllReflowHints);
  }

  // Changing justify-content on a flexbox might affect the positioning of its
  // children, but it won't affect any sizing.
  if (mJustifyContent != aOther.mJustifyContent) {
    NS_UpdateHint(hint, nsChangeHint_NeedReflow);
  }

  // Properties that apply only to multi-line flex containers:
  // 'align-content' can change the positioning & sizing of a multi-line flex
  // container's children when there's extra space in the cross axis, but it
  // shouldn't affect the container's own sizing.
  //
  // NOTE: If we get here, we know that mFlexWrap == aOther.mFlexWrap
  // (otherwise, we would've returned earlier). So it doesn't matter which one
  // of those we check to see if we're multi-line.
  if (mFlexWrap != NS_STYLE_FLEX_WRAP_NOWRAP &&
      mAlignContent != aOther.mAlignContent) {
    NS_UpdateHint(hint, nsChangeHint_NeedReflow);
  }

  if (mHeight != aOther.mHeight ||
      mMinHeight != aOther.mMinHeight ||
      mMaxHeight != aOther.mMaxHeight) {
    // Height changes can affect descendant intrinsic sizes due to replaced
    // elements with percentage heights in descendants which also have
    // percentage heights.  And due to our not-so-great computation of mVResize
    // in nsHTMLReflowState, they do need to force reflow of the whole subtree.
    // XXXbz due to XUL caching heights as well, height changes also need to
    // clear ancestor intrinsics!
    return NS_CombineHint(hint, nsChangeHint_AllReflowHints);
  }

  if (mWidth != aOther.mWidth ||
      mMinWidth != aOther.mMinWidth ||
      mMaxWidth != aOther.mMaxWidth) {
    // None of our width differences can affect descendant intrinsic
    // sizes and none of them need to force children to reflow.
    return
      NS_CombineHint(hint,
                     NS_SubtractHint(nsChangeHint_AllReflowHints,
                                     NS_CombineHint(nsChangeHint_ClearDescendantIntrinsics,
                                                    nsChangeHint_NeedDirtyReflow)));
  }

  // If width and height have not changed, but any of the offsets have changed,
  // then return the respective hints so that we would hopefully be able to
  // avoid reflowing.
  // Note that it is possible that we'll need to reflow when processing
  // restyles, but we don't have enough information to make a good decision
  // right now.
  // Don't try to handle changes between "auto" and non-auto efficiently;
  // that's tricky to do and will hardly ever be able to avoid a reflow.
  if (mOffset != aOther.mOffset) {
    if (IsAutonessEqual(mOffset, aOther.mOffset)) {
      NS_UpdateHint(hint, nsChangeHint(nsChangeHint_RecomputePosition |
                                       nsChangeHint_UpdateOverflow));
    } else {
      return NS_CombineHint(hint, nsChangeHint_AllReflowHints);
    }
  }
  return hint;
}

/* static */ bool
nsStylePosition::WidthCoordDependsOnContainer(const nsStyleCoord &aCoord)
{
  return aCoord.HasPercent() ||
         (aCoord.GetUnit() == eStyleUnit_Enumerated &&
          (aCoord.GetIntValue() == NS_STYLE_WIDTH_FIT_CONTENT ||
           aCoord.GetIntValue() == NS_STYLE_WIDTH_AVAILABLE));
}

// --------------------
// nsStyleTable
//

nsStyleTable::nsStyleTable() 
{ 
  MOZ_COUNT_CTOR(nsStyleTable);
  // values not inherited
  mLayoutStrategy = NS_STYLE_TABLE_LAYOUT_AUTO;
  mSpan = 1;
}

nsStyleTable::~nsStyleTable(void) 
{
  MOZ_COUNT_DTOR(nsStyleTable);
}

nsStyleTable::nsStyleTable(const nsStyleTable& aSource)
  : mLayoutStrategy(aSource.mLayoutStrategy)
  , mSpan(aSource.mSpan)
{
  MOZ_COUNT_CTOR(nsStyleTable);
}

nsChangeHint nsStyleTable::CalcDifference(const nsStyleTable& aOther) const
{
  if (mSpan != aOther.mSpan ||
      mLayoutStrategy != aOther.mLayoutStrategy)
    return NS_STYLE_HINT_FRAMECHANGE;
  return NS_STYLE_HINT_NONE;
}

// -----------------------
// nsStyleTableBorder

nsStyleTableBorder::nsStyleTableBorder(nsPresContext* aPresContext) 
{ 
  MOZ_COUNT_CTOR(nsStyleTableBorder);
  mBorderCollapse = NS_STYLE_BORDER_SEPARATE;

  nsCompatibility compatMode = eCompatibility_FullStandards;
  if (aPresContext)
    compatMode = aPresContext->CompatibilityMode();
  mEmptyCells = (compatMode == eCompatibility_NavQuirks)
                  ? NS_STYLE_TABLE_EMPTY_CELLS_SHOW_BACKGROUND     
                  : NS_STYLE_TABLE_EMPTY_CELLS_SHOW;
  mCaptionSide = NS_STYLE_CAPTION_SIDE_TOP;
  mBorderSpacingX = 0;
  mBorderSpacingY = 0;
}

nsStyleTableBorder::~nsStyleTableBorder(void) 
{
  MOZ_COUNT_DTOR(nsStyleTableBorder);
}

nsStyleTableBorder::nsStyleTableBorder(const nsStyleTableBorder& aSource)
  : mBorderSpacingX(aSource.mBorderSpacingX)
  , mBorderSpacingY(aSource.mBorderSpacingY)
  , mBorderCollapse(aSource.mBorderCollapse)
  , mCaptionSide(aSource.mCaptionSide)
  , mEmptyCells(aSource.mEmptyCells)
{
  MOZ_COUNT_CTOR(nsStyleTableBorder);
}

nsChangeHint nsStyleTableBorder::CalcDifference(const nsStyleTableBorder& aOther) const
{
  // Border-collapse changes need a reframe, because we use a different frame
  // class for table cells in the collapsed border model.  This is used to
  // conserve memory when using the separated border model (collapsed borders
  // require extra state to be stored).
  if (mBorderCollapse != aOther.mBorderCollapse) {
    return NS_STYLE_HINT_FRAMECHANGE;
  }
  
  if ((mCaptionSide == aOther.mCaptionSide) &&
      (mBorderSpacingX == aOther.mBorderSpacingX) &&
      (mBorderSpacingY == aOther.mBorderSpacingY)) {
    if (mEmptyCells == aOther.mEmptyCells)
      return NS_STYLE_HINT_NONE;
    return NS_STYLE_HINT_VISUAL;
  }
  else
    return NS_STYLE_HINT_REFLOW;
}

// --------------------
// nsStyleColor
//

nsStyleColor::nsStyleColor(nsPresContext* aPresContext)
{
  MOZ_COUNT_CTOR(nsStyleColor);
  mColor = aPresContext->DefaultColor();
}

nsStyleColor::nsStyleColor(const nsStyleColor& aSource)
{
  MOZ_COUNT_CTOR(nsStyleColor);
  mColor = aSource.mColor;
}

nsChangeHint nsStyleColor::CalcDifference(const nsStyleColor& aOther) const
{
  if (mColor == aOther.mColor)
    return NS_STYLE_HINT_NONE;
  return NS_STYLE_HINT_VISUAL;
}

// --------------------
// nsStyleGradient
//
bool
nsStyleGradient::operator==(const nsStyleGradient& aOther) const
{
  NS_ABORT_IF_FALSE(mSize == NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER ||
                    mShape != NS_STYLE_GRADIENT_SHAPE_LINEAR,
                    "incorrect combination of shape and size");
  NS_ABORT_IF_FALSE(aOther.mSize == NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER ||
                    aOther.mShape != NS_STYLE_GRADIENT_SHAPE_LINEAR,
                    "incorrect combination of shape and size");

  if (mShape != aOther.mShape ||
      mSize != aOther.mSize ||
      mRepeating != aOther.mRepeating ||
      mLegacySyntax != aOther.mLegacySyntax ||
      mBgPosX != aOther.mBgPosX ||
      mBgPosY != aOther.mBgPosY ||
      mAngle != aOther.mAngle ||
      mRadiusX != aOther.mRadiusX ||
      mRadiusY != aOther.mRadiusY)
    return false;

  if (mStops.Length() != aOther.mStops.Length())
    return false;

  for (uint32_t i = 0; i < mStops.Length(); i++) {
    if (mStops[i].mLocation != aOther.mStops[i].mLocation ||
        mStops[i].mColor != aOther.mStops[i].mColor)
      return false;
  }

  return true;
}

nsStyleGradient::nsStyleGradient(void)
  : mShape(NS_STYLE_GRADIENT_SHAPE_LINEAR)
  , mSize(NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER)
  , mRepeating(false)
  , mLegacySyntax(false)
{
}

bool
nsStyleGradient::IsOpaque()
{
  for (uint32_t i = 0; i < mStops.Length(); i++) {
    if (NS_GET_A(mStops[i].mColor) < 255)
      return false;
  }
  return true;
}

bool
nsStyleGradient::HasCalc()
{
  for (uint32_t i = 0; i < mStops.Length(); i++) {
    if (mStops[i].mLocation.IsCalcUnit())
      return true;
  }
  return mBgPosX.IsCalcUnit() || mBgPosY.IsCalcUnit() || mAngle.IsCalcUnit() ||
         mRadiusX.IsCalcUnit() || mRadiusY.IsCalcUnit();
}

// --------------------
// nsStyleImage
//

nsStyleImage::nsStyleImage()
  : mType(eStyleImageType_Null)
  , mCropRect(nullptr)
#ifdef DEBUG
  , mImageTracked(false)
#endif
{
  MOZ_COUNT_CTOR(nsStyleImage);
}

nsStyleImage::~nsStyleImage()
{
  MOZ_COUNT_DTOR(nsStyleImage);
  if (mType != eStyleImageType_Null)
    SetNull();
}

nsStyleImage::nsStyleImage(const nsStyleImage& aOther)
  : mType(eStyleImageType_Null)
  , mCropRect(nullptr)
#ifdef DEBUG
  , mImageTracked(false)
#endif
{
  // We need our own copy constructor because we don't want
  // to copy the reference count
  MOZ_COUNT_CTOR(nsStyleImage);
  DoCopy(aOther);
}

nsStyleImage&
nsStyleImage::operator=(const nsStyleImage& aOther)
{
  if (this != &aOther)
    DoCopy(aOther);

  return *this;
}

void
nsStyleImage::DoCopy(const nsStyleImage& aOther)
{
  SetNull();

  if (aOther.mType == eStyleImageType_Image)
    SetImageData(aOther.mImage);
  else if (aOther.mType == eStyleImageType_Gradient)
    SetGradientData(aOther.mGradient);
  else if (aOther.mType == eStyleImageType_Element)
    SetElementId(aOther.mElementId);

  SetCropRect(aOther.mCropRect);
}

void
nsStyleImage::SetNull()
{
  NS_ABORT_IF_FALSE(!mImageTracked,
                    "Calling SetNull() with image tracked!");

  if (mType == eStyleImageType_Gradient)
    mGradient->Release();
  else if (mType == eStyleImageType_Image)
    NS_RELEASE(mImage);
  else if (mType == eStyleImageType_Element)
    NS_Free(mElementId);

  mType = eStyleImageType_Null;
  mCropRect = nullptr;
}

void
nsStyleImage::SetImageData(imgRequestProxy* aImage)
{
  NS_ABORT_IF_FALSE(!mImageTracked,
                    "Setting a new image without untracking the old one!");

  NS_IF_ADDREF(aImage);

  if (mType != eStyleImageType_Null)
    SetNull();

  if (aImage) {
    mImage = aImage;
    mType = eStyleImageType_Image;
  }
  mSubImages.Clear();
}

void
nsStyleImage::TrackImage(nsPresContext* aContext)
{
  // Sanity
  NS_ABORT_IF_FALSE(!mImageTracked, "Already tracking image!");
  NS_ABORT_IF_FALSE(mType == eStyleImageType_Image,
                    "Can't track image when there isn't one!");

  // Register the image with the document
  nsIDocument* doc = aContext->Document();
  if (doc)
    doc->AddImage(mImage);

  // Mark state
#ifdef DEBUG
  mImageTracked = true;
#endif
}

void
nsStyleImage::UntrackImage(nsPresContext* aContext)
{
  // Sanity
  NS_ABORT_IF_FALSE(mImageTracked, "Image not tracked!");
  NS_ABORT_IF_FALSE(mType == eStyleImageType_Image,
                    "Can't untrack image when there isn't one!");

  // Unregister the image with the document
  nsIDocument* doc = aContext->Document();
  if (doc)
    doc->RemoveImage(mImage, nsIDocument::REQUEST_DISCARD);

  // Mark state
#ifdef DEBUG
  mImageTracked = false;
#endif
}

void
nsStyleImage::SetGradientData(nsStyleGradient* aGradient)
{
  if (aGradient)
    aGradient->AddRef();

  if (mType != eStyleImageType_Null)
    SetNull();

  if (aGradient) {
    mGradient = aGradient;
    mType = eStyleImageType_Gradient;
  }
}

void
nsStyleImage::SetElementId(const char16_t* aElementId)
{
  if (mType != eStyleImageType_Null)
    SetNull();

  if (aElementId) {
    mElementId = NS_strdup(aElementId);
    mType = eStyleImageType_Element;
  }
}

void
nsStyleImage::SetCropRect(nsStyleSides* aCropRect)
{
  if (aCropRect) {
    mCropRect = new nsStyleSides(*aCropRect);
    // There is really not much we can do if 'new' fails
  } else {
    mCropRect = nullptr;
  }
}

static int32_t
ConvertToPixelCoord(const nsStyleCoord& aCoord, int32_t aPercentScale)
{
  double pixelValue;
  switch (aCoord.GetUnit()) {
    case eStyleUnit_Percent:
      pixelValue = aCoord.GetPercentValue() * aPercentScale;
      break;
    case eStyleUnit_Factor:
      pixelValue = aCoord.GetFactorValue();
      break;
    default:
      NS_NOTREACHED("unexpected unit for image crop rect");
      return 0;
  }
  NS_ABORT_IF_FALSE(pixelValue >= 0, "we ensured non-negative while parsing");
  pixelValue = std::min(pixelValue, double(INT32_MAX)); // avoid overflow
  return NS_lround(pixelValue);
}

bool
nsStyleImage::ComputeActualCropRect(nsIntRect& aActualCropRect,
                                    bool* aIsEntireImage) const
{
  if (mType != eStyleImageType_Image)
    return false;

  nsCOMPtr<imgIContainer> imageContainer;
  mImage->GetImage(getter_AddRefs(imageContainer));
  if (!imageContainer)
    return false;

  nsIntSize imageSize;
  imageContainer->GetWidth(&imageSize.width);
  imageContainer->GetHeight(&imageSize.height);
  if (imageSize.width <= 0 || imageSize.height <= 0)
    return false;

  int32_t left   = ConvertToPixelCoord(mCropRect->GetLeft(),   imageSize.width);
  int32_t top    = ConvertToPixelCoord(mCropRect->GetTop(),    imageSize.height);
  int32_t right  = ConvertToPixelCoord(mCropRect->GetRight(),  imageSize.width);
  int32_t bottom = ConvertToPixelCoord(mCropRect->GetBottom(), imageSize.height);

  // IntersectRect() returns an empty rect if we get negative width or height
  nsIntRect cropRect(left, top, right - left, bottom - top);
  nsIntRect imageRect(nsIntPoint(0, 0), imageSize);
  aActualCropRect.IntersectRect(imageRect, cropRect);

  if (aIsEntireImage)
    *aIsEntireImage = aActualCropRect.IsEqualInterior(imageRect);
  return true;
}

nsresult
nsStyleImage::StartDecoding() const
{
  if ((mType == eStyleImageType_Image) && mImage)
    return mImage->StartDecoding();
  return NS_OK;
}

bool
nsStyleImage::IsOpaque() const
{
  if (!IsComplete())
    return false;

  if (mType == eStyleImageType_Gradient)
    return mGradient->IsOpaque();

  if (mType == eStyleImageType_Element)
    return false;

  NS_ABORT_IF_FALSE(mType == eStyleImageType_Image, "unexpected image type");

  nsCOMPtr<imgIContainer> imageContainer;
  mImage->GetImage(getter_AddRefs(imageContainer));
  NS_ABORT_IF_FALSE(imageContainer, "IsComplete() said image container is ready");

  // Check if the crop region of the current image frame is opaque.
  if (imageContainer->FrameIsOpaque(imgIContainer::FRAME_CURRENT)) {
    if (!mCropRect)
      return true;

    // Must make sure if mCropRect contains at least a pixel.
    // XXX Is this optimization worth it? Maybe I should just return false.
    nsIntRect actualCropRect;
    bool rv = ComputeActualCropRect(actualCropRect);
    NS_ASSERTION(rv, "ComputeActualCropRect() can not fail here");
    return rv && !actualCropRect.IsEmpty();
  }

  return false;
}

bool
nsStyleImage::IsComplete() const
{
  switch (mType) {
    case eStyleImageType_Null:
      return false;
    case eStyleImageType_Gradient:
    case eStyleImageType_Element:
      return true;
    case eStyleImageType_Image:
    {
      uint32_t status = imgIRequest::STATUS_ERROR;
      return NS_SUCCEEDED(mImage->GetImageStatus(&status)) &&
             (status & imgIRequest::STATUS_SIZE_AVAILABLE) &&
             (status & imgIRequest::STATUS_FRAME_COMPLETE);
    }
    default:
      NS_NOTREACHED("unexpected image type");
      return false;
  }
}

bool
nsStyleImage::IsLoaded() const
{
  switch (mType) {
    case eStyleImageType_Null:
      return false;
    case eStyleImageType_Gradient:
    case eStyleImageType_Element:
      return true;
    case eStyleImageType_Image:
    {
      uint32_t status = imgIRequest::STATUS_ERROR;
      return NS_SUCCEEDED(mImage->GetImageStatus(&status)) &&
             !(status & imgIRequest::STATUS_ERROR) &&
             (status & imgIRequest::STATUS_LOAD_COMPLETE);
    }
    default:
      NS_NOTREACHED("unexpected image type");
      return false;
  }
}

static inline bool
EqualRects(const nsStyleSides* aRect1, const nsStyleSides* aRect2)
{
  return aRect1 == aRect2 || /* handles null== null, and optimize */
         (aRect1 && aRect2 && *aRect1 == *aRect2);
}

bool
nsStyleImage::operator==(const nsStyleImage& aOther) const
{
  if (mType != aOther.mType)
    return false;

  if (!EqualRects(mCropRect, aOther.mCropRect))
    return false;

  if (mType == eStyleImageType_Image)
    return EqualImages(mImage, aOther.mImage);

  if (mType == eStyleImageType_Gradient)
    return *mGradient == *aOther.mGradient;

  if (mType == eStyleImageType_Element)
    return NS_strcmp(mElementId, aOther.mElementId) == 0;

  return true;
}

// --------------------
// nsStyleBackground
//

nsStyleBackground::nsStyleBackground()
  : mAttachmentCount(1)
  , mClipCount(1)
  , mOriginCount(1)
  , mRepeatCount(1)
  , mPositionCount(1)
  , mImageCount(1)
  , mSizeCount(1)
  , mBlendModeCount(1)
  , mBackgroundColor(NS_RGBA(0, 0, 0, 0))
{
  MOZ_COUNT_CTOR(nsStyleBackground);
  Layer *onlyLayer = mLayers.AppendElement();
  NS_ASSERTION(onlyLayer, "auto array must have room for 1 element");
  onlyLayer->SetInitialValues();
}

nsStyleBackground::nsStyleBackground(const nsStyleBackground& aSource)
  : mAttachmentCount(aSource.mAttachmentCount)
  , mClipCount(aSource.mClipCount)
  , mOriginCount(aSource.mOriginCount)
  , mRepeatCount(aSource.mRepeatCount)
  , mPositionCount(aSource.mPositionCount)
  , mImageCount(aSource.mImageCount)
  , mSizeCount(aSource.mSizeCount)
  , mBlendModeCount(aSource.mBlendModeCount)
  , mLayers(aSource.mLayers) // deep copy
  , mBackgroundColor(aSource.mBackgroundColor)
{
  MOZ_COUNT_CTOR(nsStyleBackground);
  // If the deep copy of mLayers failed, truncate the counts.
  uint32_t count = mLayers.Length();
  if (count != aSource.mLayers.Length()) {
    NS_WARNING("truncating counts due to out-of-memory");
    mAttachmentCount = std::max(mAttachmentCount, count);
    mClipCount = std::max(mClipCount, count);
    mOriginCount = std::max(mOriginCount, count);
    mRepeatCount = std::max(mRepeatCount, count);
    mPositionCount = std::max(mPositionCount, count);
    mImageCount = std::max(mImageCount, count);
    mSizeCount = std::max(mSizeCount, count);
    mBlendModeCount = std::max(mSizeCount, count);
  }
}

nsStyleBackground::~nsStyleBackground()
{
  MOZ_COUNT_DTOR(nsStyleBackground);
}

void
nsStyleBackground::Destroy(nsPresContext* aContext)
{
  // Untrack all the images stored in our layers
  for (uint32_t i = 0; i < mImageCount; ++i)
    mLayers[i].UntrackImages(aContext);

  this->~nsStyleBackground();
  aContext->PresShell()->
    FreeByObjectID(nsPresArena::nsStyleBackground_id, this);
}

nsChangeHint nsStyleBackground::CalcDifference(const nsStyleBackground& aOther) const
{
  const nsStyleBackground* moreLayers =
    mImageCount > aOther.mImageCount ? this : &aOther;
  const nsStyleBackground* lessLayers =
    mImageCount > aOther.mImageCount ? &aOther : this;

  bool hasVisualDifference = false;

  NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, moreLayers) {
    if (i < lessLayers->mImageCount) {
      if (moreLayers->mLayers[i] != lessLayers->mLayers[i]) {
        if ((moreLayers->mLayers[i].mImage.GetType() == eStyleImageType_Element) ||
            (lessLayers->mLayers[i].mImage.GetType() == eStyleImageType_Element))
          return NS_CombineHint(nsChangeHint_UpdateEffects, NS_STYLE_HINT_VISUAL);
        hasVisualDifference = true;
      }
    } else {
      if (moreLayers->mLayers[i].mImage.GetType() == eStyleImageType_Element)
        return NS_CombineHint(nsChangeHint_UpdateEffects, NS_STYLE_HINT_VISUAL);
      hasVisualDifference = true;
    }
  }

  if (hasVisualDifference || mBackgroundColor != aOther.mBackgroundColor)
    return NS_STYLE_HINT_VISUAL;

  if (mAttachmentCount != aOther.mAttachmentCount ||
      mClipCount != aOther.mClipCount ||
      mOriginCount != aOther.mOriginCount ||
      mRepeatCount != aOther.mRepeatCount ||
      mPositionCount != aOther.mPositionCount ||
      mSizeCount != aOther.mSizeCount) {
    return nsChangeHint_NeutralChange;
  }

  return NS_STYLE_HINT_NONE;
}

bool nsStyleBackground::HasFixedBackground() const
{
  NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, this) {
    const Layer &layer = mLayers[i];
    if (layer.mAttachment == NS_STYLE_BG_ATTACHMENT_FIXED &&
        !layer.mImage.IsEmpty()) {
      return true;
    }
  }
  return false;
}

bool nsStyleBackground::IsTransparent() const
{
  return BottomLayer().mImage.IsEmpty() &&
         mImageCount == 1 &&
         NS_GET_A(mBackgroundColor) == 0;
}

void
nsStyleBackground::Position::SetInitialPercentValues(float aPercentVal)
{
  mXPosition.mPercent = aPercentVal;
  mXPosition.mLength = 0;
  mXPosition.mHasPercent = true;
  mYPosition.mPercent = aPercentVal;
  mYPosition.mLength = 0;
  mYPosition.mHasPercent = true;
}

bool
nsStyleBackground::Size::DependsOnPositioningAreaSize(const nsStyleImage& aImage) const
{
  NS_ABORT_IF_FALSE(aImage.GetType() != eStyleImageType_Null,
                    "caller should have handled this");

  // If either dimension contains a non-zero percentage, rendering for that
  // dimension straightforwardly depends on frame size.
  if ((mWidthType == eLengthPercentage && mWidth.mPercent != 0.0f) ||
      (mHeightType == eLengthPercentage && mHeight.mPercent != 0.0f)) {
    return true;
  }

  // So too for contain and cover.
  if (mWidthType == eContain || mWidthType == eCover) {
    return true;
  }

  // If both dimensions are fixed lengths, there's no dependency.
  if (mWidthType == eLengthPercentage && mHeightType == eLengthPercentage) {
    return false;
  }

  NS_ABORT_IF_FALSE((mWidthType == eLengthPercentage && mHeightType == eAuto) ||
                    (mWidthType == eAuto && mHeightType == eLengthPercentage) ||
                    (mWidthType == eAuto && mHeightType == eAuto),
                    "logic error");

  nsStyleImageType type = aImage.GetType();

  // Gradient rendering depends on frame size when auto is involved because
  // gradients have no intrinsic ratio or dimensions, and therefore the relevant
  // dimension is "treat[ed] as 100%".
  if (type == eStyleImageType_Gradient) {
    return true;
  }

  // XXX Element rendering for auto or fixed length doesn't depend on frame size
  //     according to the spec.  However, we don't implement the spec yet, so
  //     for now we bail and say element() plus auto affects ultimate size.
  if (type == eStyleImageType_Element) {
    return true;
  }

  if (type == eStyleImageType_Image) {
    nsCOMPtr<imgIContainer> imgContainer;
    aImage.GetImageData()->GetImage(getter_AddRefs(imgContainer));
    if (imgContainer) {
      nsIntSize imageSize;
      nsSize imageRatio;
      bool hasWidth, hasHeight;
      nsLayoutUtils::ComputeSizeForDrawing(imgContainer, imageSize, imageRatio,
                                           hasWidth, hasHeight);

      // If the image has a fixed width and height, rendering never depends on
      // the frame size.
      if (hasWidth && hasHeight) {
        return false;
      }

      // If the image has an intrinsic ratio, rendering will depend on frame
      // size when background-size is all auto.
      if (imageRatio != nsSize(0, 0)) {
        return mWidthType == mHeightType;
      }

      // Otherwise, rendering depends on frame size when the image dimensions
      // and background-size don't complement each other.
      return !(hasWidth && mHeightType == eLengthPercentage) &&
             !(hasHeight && mWidthType == eLengthPercentage);
    }
  } else {
    NS_NOTREACHED("missed an enum value");
  }

  // Passed the gauntlet: no dependency.
  return false;
}

void
nsStyleBackground::Size::SetInitialValues()
{
  mWidthType = mHeightType = eAuto;
}

bool
nsStyleBackground::Size::operator==(const Size& aOther) const
{
  NS_ABORT_IF_FALSE(mWidthType < eDimensionType_COUNT,
                    "bad mWidthType for this");
  NS_ABORT_IF_FALSE(mHeightType < eDimensionType_COUNT,
                    "bad mHeightType for this");
  NS_ABORT_IF_FALSE(aOther.mWidthType < eDimensionType_COUNT,
                    "bad mWidthType for aOther");
  NS_ABORT_IF_FALSE(aOther.mHeightType < eDimensionType_COUNT,
                    "bad mHeightType for aOther");

  return mWidthType == aOther.mWidthType &&
         mHeightType == aOther.mHeightType &&
         (mWidthType != eLengthPercentage || mWidth == aOther.mWidth) &&
         (mHeightType != eLengthPercentage || mHeight == aOther.mHeight);
}

void
nsStyleBackground::Repeat::SetInitialValues() 
{
  mXRepeat = NS_STYLE_BG_REPEAT_REPEAT;
  mYRepeat = NS_STYLE_BG_REPEAT_REPEAT;
}

nsStyleBackground::Layer::Layer()
{
}

nsStyleBackground::Layer::~Layer()
{
}

void
nsStyleBackground::Layer::SetInitialValues()
{
  mAttachment = NS_STYLE_BG_ATTACHMENT_SCROLL;
  mClip = NS_STYLE_BG_CLIP_BORDER;
  mOrigin = NS_STYLE_BG_ORIGIN_PADDING;
  mRepeat.SetInitialValues();
  mBlendMode = NS_STYLE_BLEND_NORMAL;
  mPosition.SetInitialPercentValues(0.0f); // Initial value is "0% 0%"
  mSize.SetInitialValues();
  mImage.SetNull();
}

bool
nsStyleBackground::Layer::RenderingMightDependOnPositioningAreaSizeChange() const
{
  // Do we even have an image?
  if (mImage.IsEmpty()) {
    return false;
  }

  return mPosition.DependsOnPositioningAreaSize() ||
      mSize.DependsOnPositioningAreaSize(mImage);
}

bool
nsStyleBackground::Layer::operator==(const Layer& aOther) const
{
  return mAttachment == aOther.mAttachment &&
         mClip == aOther.mClip &&
         mOrigin == aOther.mOrigin &&
         mRepeat == aOther.mRepeat &&
         mBlendMode == aOther.mBlendMode &&
         mPosition == aOther.mPosition &&
         mSize == aOther.mSize &&
         mImage == aOther.mImage;
}

// --------------------
// nsStyleDisplay
//
void nsTimingFunction::AssignFromKeyword(int32_t aTimingFunctionType)
{
  switch (aTimingFunctionType) {
    case NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_START:
      mType = StepStart;
      mSteps = 1;
      return;
    case NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END:
      mType = StepEnd;
      mSteps = 1;
      return;
    default:
      mType = Function;
      break;
  }

  static_assert(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE == 0 &&
                NS_STYLE_TRANSITION_TIMING_FUNCTION_LINEAR == 1 &&
                NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN == 2 &&
                NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_OUT == 3 &&
                NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN_OUT == 4,
                "transition timing function constants not as expected");

  static const float timingFunctionValues[5][4] = {
    { 0.25f, 0.10f, 0.25f, 1.00f }, // ease
    { 0.00f, 0.00f, 1.00f, 1.00f }, // linear
    { 0.42f, 0.00f, 1.00f, 1.00f }, // ease-in
    { 0.00f, 0.00f, 0.58f, 1.00f }, // ease-out
    { 0.42f, 0.00f, 0.58f, 1.00f }  // ease-in-out
  };

  NS_ABORT_IF_FALSE(0 <= aTimingFunctionType && aTimingFunctionType < 5,
                    "keyword out of range");
  mFunc.mX1 = timingFunctionValues[aTimingFunctionType][0];
  mFunc.mY1 = timingFunctionValues[aTimingFunctionType][1];
  mFunc.mX2 = timingFunctionValues[aTimingFunctionType][2];
  mFunc.mY2 = timingFunctionValues[aTimingFunctionType][3];
}

mozilla::StyleTransition::StyleTransition(const StyleTransition& aCopy)
  : mTimingFunction(aCopy.mTimingFunction)
  , mDuration(aCopy.mDuration)
  , mDelay(aCopy.mDelay)
  , mProperty(aCopy.mProperty)
  , mUnknownProperty(aCopy.mUnknownProperty)
{
}

void
mozilla::StyleTransition::SetInitialValues()
{
  mTimingFunction = nsTimingFunction(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE);
  mDuration = 0.0;
  mDelay = 0.0;
  mProperty = eCSSPropertyExtra_all_properties;
}

void
mozilla::StyleTransition::SetUnknownProperty(const nsAString& aUnknownProperty)
{
  NS_ASSERTION(nsCSSProps::LookupProperty(aUnknownProperty,
                                          nsCSSProps::eEnabledForAllContent) ==
                 eCSSProperty_UNKNOWN,
               "should be unknown property");
  mProperty = eCSSProperty_UNKNOWN;
  mUnknownProperty = do_GetAtom(aUnknownProperty);
}

bool
mozilla::StyleTransition::operator==(const mozilla::StyleTransition& aOther) const
{
  return mTimingFunction == aOther.mTimingFunction &&
         mDuration == aOther.mDuration &&
         mDelay == aOther.mDelay &&
         mProperty == aOther.mProperty &&
         (mProperty != eCSSProperty_UNKNOWN ||
          mUnknownProperty == aOther.mUnknownProperty);
}

mozilla::StyleAnimation::StyleAnimation(const mozilla::StyleAnimation& aCopy)
  : mTimingFunction(aCopy.mTimingFunction)
  , mDuration(aCopy.mDuration)
  , mDelay(aCopy.mDelay)
  , mName(aCopy.mName)
  , mDirection(aCopy.mDirection)
  , mFillMode(aCopy.mFillMode)
  , mPlayState(aCopy.mPlayState)
  , mIterationCount(aCopy.mIterationCount)
{
}

void
mozilla::StyleAnimation::SetInitialValues()
{
  mTimingFunction = nsTimingFunction(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE);
  mDuration = 0.0;
  mDelay = 0.0;
  mName = EmptyString();
  mDirection = NS_STYLE_ANIMATION_DIRECTION_NORMAL;
  mFillMode = NS_STYLE_ANIMATION_FILL_MODE_NONE;
  mPlayState = NS_STYLE_ANIMATION_PLAY_STATE_RUNNING;
  mIterationCount = 1.0f;
}

bool
mozilla::StyleAnimation::operator==(const mozilla::StyleAnimation& aOther) const
{
  return mTimingFunction == aOther.mTimingFunction &&
         mDuration == aOther.mDuration &&
         mDelay == aOther.mDelay &&
         mName == aOther.mName &&
         mDirection == aOther.mDirection &&
         mFillMode == aOther.mFillMode &&
         mPlayState == aOther.mPlayState &&
         mIterationCount == aOther.mIterationCount;
}

nsStyleDisplay::nsStyleDisplay()
  : mWillChangeBitField(0)
{
  MOZ_COUNT_CTOR(nsStyleDisplay);
  mAppearance = NS_THEME_NONE;
  mDisplay = NS_STYLE_DISPLAY_INLINE;
  mOriginalDisplay = mDisplay;
  mPosition = NS_STYLE_POSITION_STATIC;
  mFloats = NS_STYLE_FLOAT_NONE;
  mOriginalFloats = mFloats;
  mBreakType = NS_STYLE_CLEAR_NONE;
  mBreakInside = NS_STYLE_PAGE_BREAK_AUTO;
  mBreakBefore = false;
  mBreakAfter = false;
  mOverflowX = NS_STYLE_OVERFLOW_VISIBLE;
  mOverflowY = NS_STYLE_OVERFLOW_VISIBLE;
  mOverflowClipBox = NS_STYLE_OVERFLOW_CLIP_BOX_PADDING_BOX;
  mResize = NS_STYLE_RESIZE_NONE;
  mClipFlags = NS_STYLE_CLIP_AUTO;
  mClip.SetRect(0,0,0,0);
  mOpacity = 1.0f;
  mSpecifiedTransform = nullptr;
  mTransformOrigin[0].SetPercentValue(0.5f); // Transform is centered on origin
  mTransformOrigin[1].SetPercentValue(0.5f);
  mTransformOrigin[2].SetCoordValue(0);
  mPerspectiveOrigin[0].SetPercentValue(0.5f);
  mPerspectiveOrigin[1].SetPercentValue(0.5f);
  mChildPerspective.SetNoneValue();
  mBackfaceVisibility = NS_STYLE_BACKFACE_VISIBILITY_VISIBLE;
  mTransformStyle = NS_STYLE_TRANSFORM_STYLE_FLAT;
  mOrient = NS_STYLE_ORIENT_AUTO;
  mMixBlendMode = NS_STYLE_BLEND_NORMAL;
  mIsolation = NS_STYLE_ISOLATION_AUTO;
  mTouchAction = NS_STYLE_TOUCH_ACTION_AUTO;

  mTransitions.AppendElement();
  NS_ABORT_IF_FALSE(mTransitions.Length() == 1,
                    "appending within auto buffer should never fail");
  mTransitions[0].SetInitialValues();
  mTransitionTimingFunctionCount = 1;
  mTransitionDurationCount = 1;
  mTransitionDelayCount = 1;
  mTransitionPropertyCount = 1;

  mAnimations.AppendElement();
  NS_ABORT_IF_FALSE(mAnimations.Length() == 1,
                    "appending within auto buffer should never fail");
  mAnimations[0].SetInitialValues();
  mAnimationTimingFunctionCount = 1;
  mAnimationDurationCount = 1;
  mAnimationDelayCount = 1;
  mAnimationNameCount = 1;
  mAnimationDirectionCount = 1;
  mAnimationFillModeCount = 1;
  mAnimationPlayStateCount = 1;
  mAnimationIterationCountCount = 1;
}

nsStyleDisplay::nsStyleDisplay(const nsStyleDisplay& aSource)
  : mBinding(aSource.mBinding)
  , mClip(aSource.mClip)
  , mOpacity(aSource.mOpacity)
  , mDisplay(aSource.mDisplay)
  , mOriginalDisplay(aSource.mOriginalDisplay)
  , mAppearance(aSource.mAppearance)
  , mPosition(aSource.mPosition)
  , mFloats(aSource.mFloats)
  , mOriginalFloats(aSource.mOriginalFloats)
  , mBreakType(aSource.mBreakType)
  , mBreakInside(aSource.mBreakInside)
  , mBreakBefore(aSource.mBreakBefore)
  , mBreakAfter(aSource.mBreakAfter)
  , mOverflowX(aSource.mOverflowX)
  , mOverflowY(aSource.mOverflowY)
  , mOverflowClipBox(aSource.mOverflowClipBox)
  , mResize(aSource.mResize)
  , mClipFlags(aSource.mClipFlags)
  , mOrient(aSource.mOrient)
  , mMixBlendMode(aSource.mMixBlendMode)
  , mIsolation(aSource.mIsolation)
  , mWillChangeBitField(aSource.mWillChangeBitField)
  , mWillChange(aSource.mWillChange)
  , mTouchAction(aSource.mTouchAction)
  , mBackfaceVisibility(aSource.mBackfaceVisibility)
  , mTransformStyle(aSource.mTransformStyle)
  , mSpecifiedTransform(aSource.mSpecifiedTransform)
  , mChildPerspective(aSource.mChildPerspective)
  , mTransitions(aSource.mTransitions)
  , mTransitionTimingFunctionCount(aSource.mTransitionTimingFunctionCount)
  , mTransitionDurationCount(aSource.mTransitionDurationCount)
  , mTransitionDelayCount(aSource.mTransitionDelayCount)
  , mTransitionPropertyCount(aSource.mTransitionPropertyCount)
  , mAnimations(aSource.mAnimations)
  , mAnimationTimingFunctionCount(aSource.mAnimationTimingFunctionCount)
  , mAnimationDurationCount(aSource.mAnimationDurationCount)
  , mAnimationDelayCount(aSource.mAnimationDelayCount)
  , mAnimationNameCount(aSource.mAnimationNameCount)
  , mAnimationDirectionCount(aSource.mAnimationDirectionCount)
  , mAnimationFillModeCount(aSource.mAnimationFillModeCount)
  , mAnimationPlayStateCount(aSource.mAnimationPlayStateCount)
  , mAnimationIterationCountCount(aSource.mAnimationIterationCountCount)
{
  MOZ_COUNT_CTOR(nsStyleDisplay);

  /* Copy over transform origin. */
  mTransformOrigin[0] = aSource.mTransformOrigin[0];
  mTransformOrigin[1] = aSource.mTransformOrigin[1];
  mTransformOrigin[2] = aSource.mTransformOrigin[2];
  mPerspectiveOrigin[0] = aSource.mPerspectiveOrigin[0];
  mPerspectiveOrigin[1] = aSource.mPerspectiveOrigin[1];
}

nsChangeHint nsStyleDisplay::CalcDifference(const nsStyleDisplay& aOther) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (!EqualURIs(mBinding, aOther.mBinding)
      || mPosition != aOther.mPosition
      || mDisplay != aOther.mDisplay
      || (mFloats == NS_STYLE_FLOAT_NONE) != (aOther.mFloats == NS_STYLE_FLOAT_NONE)
      || mOverflowX != aOther.mOverflowX
      || mOverflowY != aOther.mOverflowY
      || mResize != aOther.mResize)
    NS_UpdateHint(hint, nsChangeHint_ReconstructFrame);

  if ((mAppearance == NS_THEME_TEXTFIELD &&
       aOther.mAppearance != NS_THEME_TEXTFIELD) ||
      (mAppearance != NS_THEME_TEXTFIELD &&
       aOther.mAppearance == NS_THEME_TEXTFIELD)) {
    // This is for <input type=number> where we allow authors to specify a
    // |-moz-appearance:textfield| to get a control without a spinner. (The
    // spinner is present for |-moz-appearance:number-input| but also other
    // values such as 'none'.) We need to reframe since we want to use
    // nsTextControlFrame instead of nsNumberControlFrame if the author
    // specifies 'textfield'.
    return nsChangeHint_ReconstructFrame;
  }

  if (mFloats != aOther.mFloats) {
    // Changing which side we float on doesn't affect descendants directly
    NS_UpdateHint(hint,
       NS_SubtractHint(nsChangeHint_AllReflowHints,
                       NS_CombineHint(nsChangeHint_ClearDescendantIntrinsics,
                                      nsChangeHint_NeedDirtyReflow)));
  }

  // XXX the following is conservative, for now: changing float breaking shouldn't
  // necessarily require a repaint, reflow should suffice.
  if (mBreakType != aOther.mBreakType
      || mBreakInside != aOther.mBreakInside
      || mBreakBefore != aOther.mBreakBefore
      || mBreakAfter != aOther.mBreakAfter
      || mAppearance != aOther.mAppearance
      || mOrient != aOther.mOrient
      || mOverflowClipBox != aOther.mOverflowClipBox
      || mClipFlags != aOther.mClipFlags)
    NS_UpdateHint(hint, NS_CombineHint(nsChangeHint_AllReflowHints,
                                       nsChangeHint_RepaintFrame));

  if (!mClip.IsEqualInterior(aOther.mClip)) {
    // If the clip has changed, we just need to update overflow areas. DLBI
    // will handle the invalidation.
    NS_UpdateHint(hint, NS_CombineHint(nsChangeHint_UpdateOverflow,
                                       nsChangeHint_SchedulePaint));
  }

  if (mOpacity != aOther.mOpacity) {
    // If we're going from the optimized >=0.99 opacity value to 1.0 or back, then
    // repaint the frame because DLBI will not catch the invalidation.  Otherwise,
    // just update the opacity layer.
    if ((mOpacity >= 0.99f && mOpacity < 1.0f && aOther.mOpacity == 1.0f) ||
        (aOther.mOpacity >= 0.99f && aOther.mOpacity < 1.0f && mOpacity == 1.0f)) {
      NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
    } else {
      NS_UpdateHint(hint, nsChangeHint_UpdateOpacityLayer);
    }
  }

  if (mMixBlendMode != aOther.mMixBlendMode
      || mIsolation != aOther.mIsolation) {
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
  }

  /* If we've added or removed the transform property, we need to reconstruct the frame to add
   * or remove the view object, and also to handle abs-pos and fixed-pos containers.
   */
  if (HasTransformStyle() != aOther.HasTransformStyle()) {
    // We do not need to apply nsChangeHint_UpdateTransformLayer since
    // nsChangeHint_RepaintFrame will forcibly invalidate the frame area and
    // ensure layers are rebuilt (or removed).
    NS_UpdateHint(hint, NS_CombineHint(nsChangeHint_AddOrRemoveTransform,
                          NS_CombineHint(nsChangeHint_UpdateOverflow,
                                         nsChangeHint_RepaintFrame)));
  } else {
    /* Otherwise, if we've kept the property lying around and we already had a
     * transform, we need to see whether or not we've changed the transform.
     * If so, we need to recompute its overflow rect (which probably changed
     * if the transform changed) and to redraw within the bounds of that new
     * overflow rect.
     *
     * If the property isn't present in either style struct, we still do the
     * comparisons but turn all the resulting change hints into
     * nsChangeHint_NeutralChange.
     */
    nsChangeHint transformHint = nsChangeHint(0);

    if (!mSpecifiedTransform != !aOther.mSpecifiedTransform ||
        (mSpecifiedTransform &&
         *mSpecifiedTransform != *aOther.mSpecifiedTransform)) {
      NS_UpdateHint(transformHint, nsChangeHint_UpdateTransformLayer);

      if (mSpecifiedTransform &&
          aOther.mSpecifiedTransform) {
        NS_UpdateHint(transformHint, nsChangeHint_UpdatePostTransformOverflow);
      } else {
        NS_UpdateHint(transformHint, nsChangeHint_UpdateOverflow);
      }
    }

    const nsChangeHint kUpdateOverflowAndRepaintHint =
      NS_CombineHint(nsChangeHint_UpdateOverflow, nsChangeHint_RepaintFrame);
    for (uint8_t index = 0; index < 3; ++index)
      if (mTransformOrigin[index] != aOther.mTransformOrigin[index]) {
        NS_UpdateHint(transformHint, kUpdateOverflowAndRepaintHint);
        break;
      }
    
    for (uint8_t index = 0; index < 2; ++index)
      if (mPerspectiveOrigin[index] != aOther.mPerspectiveOrigin[index]) {
        NS_UpdateHint(transformHint, kUpdateOverflowAndRepaintHint);
        break;
      }

    if (mChildPerspective != aOther.mChildPerspective ||
        mTransformStyle != aOther.mTransformStyle)
      NS_UpdateHint(transformHint, kUpdateOverflowAndRepaintHint);

    if (mBackfaceVisibility != aOther.mBackfaceVisibility)
      NS_UpdateHint(transformHint, nsChangeHint_RepaintFrame);

    if (transformHint) {
      if (HasTransformStyle()) {
        NS_UpdateHint(hint, transformHint);
      } else {
        NS_UpdateHint(hint, nsChangeHint_NeutralChange);
      }
    }
  }

  uint8_t willChangeBitsChanged =
    mWillChangeBitField ^ aOther.mWillChangeBitField;
  if (willChangeBitsChanged & NS_STYLE_WILL_CHANGE_STACKING_CONTEXT) {
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
  }
  if (willChangeBitsChanged & ~uint8_t(NS_STYLE_WILL_CHANGE_STACKING_CONTEXT)) {
    // FIXME (Bug 974125): Don't reconstruct the frame
    NS_UpdateHint(hint, nsChangeHint_ReconstructFrame);
  }

  // Note:  Our current behavior for handling changes to the
  // transition-duration, transition-delay, and transition-timing-function
  // properties is to do nothing.  In other words, the transition
  // property that matters is what it is when the transition begins, and
  // we don't stop a transition later because the transition property
  // changed.
  // We do handle changes to transition-property, but we don't need to
  // bother with anything here, since the transition manager is notified
  // of any style context change anyway.

  // Note: Likewise, for animation-*, the animation manager gets
  // notified about every new style context constructed, and it uses
  // that opportunity to handle dynamic changes appropriately.

  // But we still need to return nsChangeHint_NeutralChange for these
  // properties, since some data did change in the style struct.

  if (!hint &&
      (!mClip.IsEqualEdges(aOther.mClip) ||
       mOriginalDisplay != aOther.mOriginalDisplay ||
       mOriginalFloats != aOther.mOriginalFloats ||
       mTransitions != aOther.mTransitions ||
       mTransitionTimingFunctionCount !=
         aOther.mTransitionTimingFunctionCount ||
       mTransitionDurationCount != aOther.mTransitionDurationCount ||
       mTransitionDelayCount != aOther.mTransitionDelayCount ||
       mTransitionPropertyCount != aOther.mTransitionPropertyCount ||
       mAnimations != aOther.mAnimations ||
       mAnimationTimingFunctionCount != aOther.mAnimationTimingFunctionCount ||
       mAnimationDurationCount != aOther.mAnimationDurationCount ||
       mAnimationDelayCount != aOther.mAnimationDelayCount ||
       mAnimationNameCount != aOther.mAnimationNameCount ||
       mAnimationDirectionCount != aOther.mAnimationDirectionCount ||
       mAnimationFillModeCount != aOther.mAnimationFillModeCount ||
       mAnimationPlayStateCount != aOther.mAnimationPlayStateCount ||
       mAnimationIterationCountCount != aOther.mAnimationIterationCountCount)) {
    NS_UpdateHint(hint, nsChangeHint_NeutralChange);
  }

  return hint;
}

// --------------------
// nsStyleVisibility
//

nsStyleVisibility::nsStyleVisibility(nsPresContext* aPresContext)
{
  MOZ_COUNT_CTOR(nsStyleVisibility);
  uint32_t bidiOptions = aPresContext->GetBidi();
  if (GET_BIDI_OPTION_DIRECTION(bidiOptions) == IBMBIDI_TEXTDIRECTION_RTL)
    mDirection = NS_STYLE_DIRECTION_RTL;
  else
    mDirection = NS_STYLE_DIRECTION_LTR;

  mVisible = NS_STYLE_VISIBILITY_VISIBLE;
  mPointerEvents = NS_STYLE_POINTER_EVENTS_AUTO;
  mWritingMode = NS_STYLE_WRITING_MODE_HORIZONTAL_TB;
}

nsStyleVisibility::nsStyleVisibility(const nsStyleVisibility& aSource)
{
  MOZ_COUNT_CTOR(nsStyleVisibility);
  mImageOrientation = aSource.mImageOrientation;
  mDirection = aSource.mDirection;
  mVisible = aSource.mVisible;
  mPointerEvents = aSource.mPointerEvents;
  mWritingMode = aSource.mWritingMode;
} 

nsChangeHint nsStyleVisibility::CalcDifference(const nsStyleVisibility& aOther) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (mDirection != aOther.mDirection || mWritingMode != aOther.mWritingMode) {
    NS_UpdateHint(hint, nsChangeHint_ReconstructFrame);
  } else {
    if ((mImageOrientation != aOther.mImageOrientation)) {
      NS_UpdateHint(hint, nsChangeHint_AllReflowHints);
      NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
    }
    if (mVisible != aOther.mVisible) {
      if ((NS_STYLE_VISIBILITY_COLLAPSE == mVisible) ||
          (NS_STYLE_VISIBILITY_COLLAPSE == aOther.mVisible)) {
        NS_UpdateHint(hint, NS_STYLE_HINT_REFLOW);
      } else {
        NS_UpdateHint(hint, NS_STYLE_HINT_VISUAL);
      }
    }
    if (mPointerEvents != aOther.mPointerEvents) {
      // nsSVGPathGeometryFrame's mRect depends on stroke _and_ on the value
      // of pointer-events. See nsSVGPathGeometryFrame::ReflowSVG's use of
      // GetHitTestFlags. (Only a reflow, no visual change.)
      NS_UpdateHint(hint, nsChangeHint_NeedReflow);
      NS_UpdateHint(hint, nsChangeHint_NeedDirtyReflow); // XXX remove me: bug 876085
    }
  }
  return hint;
}

nsStyleContentData::~nsStyleContentData()
{
  NS_ABORT_IF_FALSE(!mImageTracked,
                    "nsStyleContentData being destroyed while still tracking image!");
  if (mType == eStyleContentType_Image) {
    NS_IF_RELEASE(mContent.mImage);
  } else if (mType == eStyleContentType_Counter ||
             mType == eStyleContentType_Counters) {
    mContent.mCounters->Release();
  } else if (mContent.mString) {
    NS_Free(mContent.mString);
  }
}

nsStyleContentData& nsStyleContentData::operator=(const nsStyleContentData& aOther)
{
  if (this == &aOther)
    return *this;
  this->~nsStyleContentData();
  new (this) nsStyleContentData();

  mType = aOther.mType;
  if (mType == eStyleContentType_Image) {
    mContent.mImage = aOther.mContent.mImage;
    NS_IF_ADDREF(mContent.mImage);
  } else if (mType == eStyleContentType_Counter ||
             mType == eStyleContentType_Counters) {
    mContent.mCounters = aOther.mContent.mCounters;
    mContent.mCounters->AddRef();
  } else if (aOther.mContent.mString) {
    mContent.mString = NS_strdup(aOther.mContent.mString);
  } else {
    mContent.mString = nullptr;
  }
  return *this;
}

bool nsStyleContentData::operator==(const nsStyleContentData& aOther) const
{
  if (mType != aOther.mType)
    return false;
  if (mType == eStyleContentType_Image) {
    if (!mContent.mImage || !aOther.mContent.mImage)
      return mContent.mImage == aOther.mContent.mImage;
    bool eq;
    nsCOMPtr<nsIURI> thisURI, otherURI;
    mContent.mImage->GetURI(getter_AddRefs(thisURI));
    aOther.mContent.mImage->GetURI(getter_AddRefs(otherURI));
    return thisURI == otherURI ||  // handles null==null
           (thisURI && otherURI &&
            NS_SUCCEEDED(thisURI->Equals(otherURI, &eq)) &&
            eq);
  }
  if (mType == eStyleContentType_Counter ||
      mType == eStyleContentType_Counters)
    return *mContent.mCounters == *aOther.mContent.mCounters;
  return safe_strcmp(mContent.mString, aOther.mContent.mString) == 0;
}

void
nsStyleContentData::TrackImage(nsPresContext* aContext)
{
  // Sanity
  NS_ABORT_IF_FALSE(!mImageTracked, "Already tracking image!");
  NS_ABORT_IF_FALSE(mType == eStyleContentType_Image,
                    "Trying to do image tracking on non-image!");
  NS_ABORT_IF_FALSE(mContent.mImage,
                    "Can't track image when there isn't one!");

  // Register the image with the document
  nsIDocument* doc = aContext->Document();
  if (doc)
    doc->AddImage(mContent.mImage);

  // Mark state
#ifdef DEBUG
  mImageTracked = true;
#endif
}

void
nsStyleContentData::UntrackImage(nsPresContext* aContext)
{
  // Sanity
  NS_ABORT_IF_FALSE(mImageTracked, "Image not tracked!");
  NS_ABORT_IF_FALSE(mType == eStyleContentType_Image,
                    "Trying to do image tracking on non-image!");
  NS_ABORT_IF_FALSE(mContent.mImage,
                    "Can't untrack image when there isn't one!");

  // Unregister the image with the document
  nsIDocument* doc = aContext->Document();
  if (doc)
    doc->RemoveImage(mContent.mImage, nsIDocument::REQUEST_DISCARD);

  // Mark state
#ifdef DEBUG
  mImageTracked = false;
#endif
}


//-----------------------
// nsStyleContent
//

nsStyleContent::nsStyleContent(void)
  : mMarkerOffset(),
    mContents(nullptr),
    mIncrements(nullptr),
    mResets(nullptr),
    mContentCount(0),
    mIncrementCount(0),
    mResetCount(0)
{
  MOZ_COUNT_CTOR(nsStyleContent);
  mMarkerOffset.SetAutoValue();
}

nsStyleContent::~nsStyleContent(void)
{
  MOZ_COUNT_DTOR(nsStyleContent);
  DELETE_ARRAY_IF(mContents);
  DELETE_ARRAY_IF(mIncrements);
  DELETE_ARRAY_IF(mResets);
}

void 
nsStyleContent::Destroy(nsPresContext* aContext)
{
  // Unregister any images we might have with the document.
  for (uint32_t i = 0; i < mContentCount; ++i) {
    if ((mContents[i].mType == eStyleContentType_Image) &&
        mContents[i].mContent.mImage) {
      mContents[i].UntrackImage(aContext);
    }
  }

  this->~nsStyleContent();
  aContext->PresShell()->
    FreeByObjectID(nsPresArena::nsStyleContent_id, this);
}

nsStyleContent::nsStyleContent(const nsStyleContent& aSource)
   :mMarkerOffset(),
    mContents(nullptr),
    mIncrements(nullptr),
    mResets(nullptr),
    mContentCount(0),
    mIncrementCount(0),
    mResetCount(0)

{
  MOZ_COUNT_CTOR(nsStyleContent);
  mMarkerOffset = aSource.mMarkerOffset;

  uint32_t index;
  if (NS_SUCCEEDED(AllocateContents(aSource.ContentCount()))) {
    for (index = 0; index < mContentCount; index++) {
      ContentAt(index) = aSource.ContentAt(index);
    }
  }

  if (NS_SUCCEEDED(AllocateCounterIncrements(aSource.CounterIncrementCount()))) {
    for (index = 0; index < mIncrementCount; index++) {
      const nsStyleCounterData *data = aSource.GetCounterIncrementAt(index);
      mIncrements[index].mCounter = data->mCounter;
      mIncrements[index].mValue = data->mValue;
    }
  }

  if (NS_SUCCEEDED(AllocateCounterResets(aSource.CounterResetCount()))) {
    for (index = 0; index < mResetCount; index++) {
      const nsStyleCounterData *data = aSource.GetCounterResetAt(index);
      mResets[index].mCounter = data->mCounter;
      mResets[index].mValue = data->mValue;
    }
  }
}

nsChangeHint nsStyleContent::CalcDifference(const nsStyleContent& aOther) const
{
  // In ReResolveStyleContext we assume that if there's no existing
  // ::before or ::after and we don't have to restyle children of the
  // node then we can't end up with a ::before or ::after due to the
  // restyle of the node itself.  That's not quite true, but the only
  // exception to the above is when the 'content' property of the node
  // changes and the pseudo-element inherits the changed value.  Since
  // the code here triggers a frame change on the node in that case,
  // the optimization in ReResolveStyleContext is ok.  But if we ever
  // change this code to not reconstruct frames on changes to the
  // 'content' property, then we will need to revisit the optimization
  // in ReResolveStyleContext.

  if (mContentCount != aOther.mContentCount ||
      mIncrementCount != aOther.mIncrementCount || 
      mResetCount != aOther.mResetCount) {
    return NS_STYLE_HINT_FRAMECHANGE;
  }

  uint32_t ix = mContentCount;
  while (0 < ix--) {
    if (mContents[ix] != aOther.mContents[ix]) {
      // Unfortunately we need to reframe here; a simple reflow
      // will not pick up different text or different image URLs,
      // since we set all that up in the CSSFrameConstructor
      return NS_STYLE_HINT_FRAMECHANGE;
    }
  }
  ix = mIncrementCount;
  while (0 < ix--) {
    if ((mIncrements[ix].mValue != aOther.mIncrements[ix].mValue) || 
        (mIncrements[ix].mCounter != aOther.mIncrements[ix].mCounter)) {
      return NS_STYLE_HINT_FRAMECHANGE;
    }
  }
  ix = mResetCount;
  while (0 < ix--) {
    if ((mResets[ix].mValue != aOther.mResets[ix].mValue) || 
        (mResets[ix].mCounter != aOther.mResets[ix].mCounter)) {
      return NS_STYLE_HINT_FRAMECHANGE;
    }
  }
  if (mMarkerOffset != aOther.mMarkerOffset) {
    return NS_STYLE_HINT_REFLOW;
  }
  return NS_STYLE_HINT_NONE;
}

nsresult nsStyleContent::AllocateContents(uint32_t aCount)
{
  // We need to run the destructors of the elements of mContents, so we
  // delete and reallocate even if aCount == mContentCount.  (If
  // nsStyleContentData had its members private and managed their
  // ownership on setting, we wouldn't need this, but that seems
  // unnecessary at this point.)
  DELETE_ARRAY_IF(mContents);
  if (aCount) {
    mContents = new nsStyleContentData[aCount];
    if (! mContents) {
      mContentCount = 0;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  mContentCount = aCount;
  return NS_OK;
}

// ---------------------
// nsStyleQuotes
//

nsStyleQuotes::nsStyleQuotes(void)
  : mQuotesCount(0),
    mQuotes(nullptr)
{
  MOZ_COUNT_CTOR(nsStyleQuotes);
  SetInitial();
}

nsStyleQuotes::~nsStyleQuotes(void)
{
  MOZ_COUNT_DTOR(nsStyleQuotes);
  DELETE_ARRAY_IF(mQuotes);
}

nsStyleQuotes::nsStyleQuotes(const nsStyleQuotes& aSource)
  : mQuotesCount(0),
    mQuotes(nullptr)
{
  MOZ_COUNT_CTOR(nsStyleQuotes);
  CopyFrom(aSource);
}

void
nsStyleQuotes::SetInitial()
{
  // The initial value for quotes is the en-US typographic convention:
  // outermost are LEFT and RIGHT DOUBLE QUOTATION MARK, alternating
  // with LEFT and RIGHT SINGLE QUOTATION MARK.
  static const char16_t initialQuotes[8] = {
    0x201C, 0, 0x201D, 0, 0x2018, 0, 0x2019, 0
  };
  
  if (NS_SUCCEEDED(AllocateQuotes(2))) {
    SetQuotesAt(0,
                nsDependentString(&initialQuotes[0], 1),
                nsDependentString(&initialQuotes[2], 1));
    SetQuotesAt(1,
                nsDependentString(&initialQuotes[4], 1),
                nsDependentString(&initialQuotes[6], 1));
  }
}

void
nsStyleQuotes::CopyFrom(const nsStyleQuotes& aSource)
{
  if (NS_SUCCEEDED(AllocateQuotes(aSource.QuotesCount()))) {
    uint32_t count = (mQuotesCount * 2);
    for (uint32_t index = 0; index < count; index += 2) {
      aSource.GetQuotesAt(index, mQuotes[index], mQuotes[index + 1]);
    }
  }
}

nsChangeHint nsStyleQuotes::CalcDifference(const nsStyleQuotes& aOther) const
{
  // If the quotes implementation is ever going to change we might not need
  // a framechange here and a reflow should be sufficient.  See bug 35768.
  if (mQuotesCount == aOther.mQuotesCount) {
    uint32_t ix = (mQuotesCount * 2);
    while (0 < ix--) {
      if (mQuotes[ix] != aOther.mQuotes[ix]) {
        return NS_STYLE_HINT_FRAMECHANGE;
      }
    }

    return NS_STYLE_HINT_NONE;
  }
  return NS_STYLE_HINT_FRAMECHANGE;
}

// --------------------
// nsStyleTextReset
//

nsStyleTextReset::nsStyleTextReset(void) 
{ 
  MOZ_COUNT_CTOR(nsStyleTextReset);
  mVerticalAlign.SetIntValue(NS_STYLE_VERTICAL_ALIGN_BASELINE, eStyleUnit_Enumerated);
  mTextDecorationLine = NS_STYLE_TEXT_DECORATION_LINE_NONE;
  mTextDecorationColor = NS_RGB(0,0,0);
  mTextDecorationStyle =
    NS_STYLE_TEXT_DECORATION_STYLE_SOLID | BORDER_COLOR_FOREGROUND;
  mUnicodeBidi = NS_STYLE_UNICODE_BIDI_NORMAL;
}

nsStyleTextReset::nsStyleTextReset(const nsStyleTextReset& aSource) 
{ 
  MOZ_COUNT_CTOR(nsStyleTextReset);
  *this = aSource;
}

nsStyleTextReset::~nsStyleTextReset(void)
{
  MOZ_COUNT_DTOR(nsStyleTextReset);
}

nsChangeHint nsStyleTextReset::CalcDifference(const nsStyleTextReset& aOther) const
{
  if (mVerticalAlign == aOther.mVerticalAlign
      && mUnicodeBidi == aOther.mUnicodeBidi) {
    uint8_t lineStyle = GetDecorationStyle();
    uint8_t otherLineStyle = aOther.GetDecorationStyle();
    if (mTextDecorationLine != aOther.mTextDecorationLine ||
        lineStyle != otherLineStyle) {
      // Repaint for other style decoration lines because they must be in
      // default overflow rect
      nsChangeHint hint = NS_STYLE_HINT_VISUAL;
      NS_UpdateHint(hint, nsChangeHint_UpdateSubtreeOverflow);
      return hint;
    }

    // Repaint for decoration color changes
    nscolor decColor, otherDecColor;
    bool isFG, otherIsFG;
    GetDecorationColor(decColor, isFG);
    aOther.GetDecorationColor(otherDecColor, otherIsFG);
    if (isFG != otherIsFG || (!isFG && decColor != otherDecColor)) {
      return NS_STYLE_HINT_VISUAL;
    }

    if (mTextOverflow != aOther.mTextOverflow) {
      return NS_STYLE_HINT_VISUAL;
    }
    return NS_STYLE_HINT_NONE;
  }
  return NS_STYLE_HINT_REFLOW;
}

// Allowed to return one of NS_STYLE_HINT_NONE, NS_STYLE_HINT_REFLOW
// or NS_STYLE_HINT_VISUAL. Currently we just return NONE or REFLOW, though.
// XXXbz can this not return a more specific hint?  If that's ever
// changed, nsStyleBorder::CalcDifference will need changing too.
static nsChangeHint
CalcShadowDifference(nsCSSShadowArray* lhs,
                     nsCSSShadowArray* rhs)
{
  if (lhs == rhs)
    return NS_STYLE_HINT_NONE;

  if (!lhs || !rhs || lhs->Length() != rhs->Length())
    return NS_STYLE_HINT_REFLOW;

  for (uint32_t i = 0; i < lhs->Length(); ++i) {
    if (*lhs->ShadowAt(i) != *rhs->ShadowAt(i))
      return NS_STYLE_HINT_REFLOW;
  }
  return NS_STYLE_HINT_NONE;
}

// --------------------
// nsStyleText
//

nsStyleText::nsStyleText(void)
{ 
  MOZ_COUNT_CTOR(nsStyleText);
  mTextAlign = NS_STYLE_TEXT_ALIGN_DEFAULT;
  mTextAlignLast = NS_STYLE_TEXT_ALIGN_AUTO;
  mTextAlignTrue = false;
  mTextAlignLastTrue = false;
  mTextTransform = NS_STYLE_TEXT_TRANSFORM_NONE;
  mWhiteSpace = NS_STYLE_WHITESPACE_NORMAL;
  mWordBreak = NS_STYLE_WORDBREAK_NORMAL;
  mWordWrap = NS_STYLE_WORDWRAP_NORMAL;
  mHyphens = NS_STYLE_HYPHENS_MANUAL;
  mTextSizeAdjust = NS_STYLE_TEXT_SIZE_ADJUST_AUTO;
  mTextOrientation = NS_STYLE_TEXT_ORIENTATION_MIXED;
  mTextCombineUpright = NS_STYLE_TEXT_COMBINE_UPRIGHT_NONE;
  mControlCharacterVisibility = NS_STYLE_CONTROL_CHARACTER_VISIBILITY_HIDDEN;

  mLetterSpacing.SetNormalValue();
  mLineHeight.SetNormalValue();
  mTextIndent.SetCoordValue(0);
  mWordSpacing = 0;

  mTextShadow = nullptr;
  mTabSize = NS_STYLE_TABSIZE_INITIAL;
}

nsStyleText::nsStyleText(const nsStyleText& aSource)
  : mTextAlign(aSource.mTextAlign),
    mTextAlignLast(aSource.mTextAlignLast),
    mTextAlignTrue(false),
    mTextAlignLastTrue(false),
    mTextTransform(aSource.mTextTransform),
    mWhiteSpace(aSource.mWhiteSpace),
    mWordBreak(aSource.mWordBreak),
    mWordWrap(aSource.mWordWrap),
    mHyphens(aSource.mHyphens),
    mTextSizeAdjust(aSource.mTextSizeAdjust),
    mTextOrientation(aSource.mTextOrientation),
    mTextCombineUpright(aSource.mTextCombineUpright),
    mControlCharacterVisibility(aSource.mControlCharacterVisibility),
    mTabSize(aSource.mTabSize),
    mWordSpacing(aSource.mWordSpacing),
    mLetterSpacing(aSource.mLetterSpacing),
    mLineHeight(aSource.mLineHeight),
    mTextIndent(aSource.mTextIndent),
    mTextShadow(aSource.mTextShadow)
{
  MOZ_COUNT_CTOR(nsStyleText);
}

nsStyleText::~nsStyleText(void)
{
  MOZ_COUNT_DTOR(nsStyleText);
}

nsChangeHint nsStyleText::CalcDifference(const nsStyleText& aOther) const
{
  if (WhiteSpaceOrNewlineIsSignificant() !=
      aOther.WhiteSpaceOrNewlineIsSignificant()) {
    // This may require construction of suppressed text frames
    return NS_STYLE_HINT_FRAMECHANGE;
  }

  if (mTextCombineUpright != aOther.mTextCombineUpright ||
      mControlCharacterVisibility != aOther.mControlCharacterVisibility) {
    return nsChangeHint_ReconstructFrame;
  }

  if ((mTextAlign != aOther.mTextAlign) ||
      (mTextAlignLast != aOther.mTextAlignLast) ||
      (mTextAlignTrue != aOther.mTextAlignTrue) ||
      (mTextAlignLastTrue != aOther.mTextAlignLastTrue) ||
      (mTextTransform != aOther.mTextTransform) ||
      (mWhiteSpace != aOther.mWhiteSpace) ||
      (mWordBreak != aOther.mWordBreak) ||
      (mWordWrap != aOther.mWordWrap) ||
      (mHyphens != aOther.mHyphens) ||
      (mTextSizeAdjust != aOther.mTextSizeAdjust) ||
      (mTextOrientation != aOther.mTextOrientation) ||
      (mLetterSpacing != aOther.mLetterSpacing) ||
      (mLineHeight != aOther.mLineHeight) ||
      (mTextIndent != aOther.mTextIndent) ||
      (mWordSpacing != aOther.mWordSpacing) ||
      (mTabSize != aOther.mTabSize))
    return NS_STYLE_HINT_REFLOW;

  return CalcShadowDifference(mTextShadow, aOther.mTextShadow);
}

//-----------------------
// nsStyleUserInterface
//

nsCursorImage::nsCursorImage()
  : mHaveHotspot(false)
  , mHotspotX(0.0f)
  , mHotspotY(0.0f)
{
}

nsCursorImage::nsCursorImage(const nsCursorImage& aOther)
  : mHaveHotspot(aOther.mHaveHotspot)
  , mHotspotX(aOther.mHotspotX)
  , mHotspotY(aOther.mHotspotY)
{
  SetImage(aOther.GetImage());
}

nsCursorImage::~nsCursorImage()
{
  SetImage(nullptr);
}

nsCursorImage&
nsCursorImage::operator=(const nsCursorImage& aOther)
{
  if (this != &aOther) {
    mHaveHotspot = aOther.mHaveHotspot;
    mHotspotX = aOther.mHotspotX;
    mHotspotY = aOther.mHotspotY;
    SetImage(aOther.GetImage());
  }

  return *this;
}

nsStyleUserInterface::nsStyleUserInterface(void) 
{ 
  MOZ_COUNT_CTOR(nsStyleUserInterface);
  mUserInput = NS_STYLE_USER_INPUT_AUTO;
  mUserModify = NS_STYLE_USER_MODIFY_READ_ONLY;
  mUserFocus = NS_STYLE_USER_FOCUS_NONE;
  mWindowDragging = NS_STYLE_WINDOW_DRAGGING_NO_DRAG;

  mCursor = NS_STYLE_CURSOR_AUTO; // fix for bugzilla bug 51113

  mCursorArrayLength = 0;
  mCursorArray = nullptr;
}

nsStyleUserInterface::nsStyleUserInterface(const nsStyleUserInterface& aSource) :
  mUserInput(aSource.mUserInput),
  mUserModify(aSource.mUserModify),
  mUserFocus(aSource.mUserFocus),
  mWindowDragging(aSource.mWindowDragging),
  mCursor(aSource.mCursor)
{ 
  MOZ_COUNT_CTOR(nsStyleUserInterface);
  CopyCursorArrayFrom(aSource);
}

nsStyleUserInterface::~nsStyleUserInterface(void) 
{ 
  MOZ_COUNT_DTOR(nsStyleUserInterface);
  delete [] mCursorArray;
}

nsChangeHint nsStyleUserInterface::CalcDifference(const nsStyleUserInterface& aOther) const
{
  nsChangeHint hint = nsChangeHint(0);
  if (mCursor != aOther.mCursor)
    NS_UpdateHint(hint, nsChangeHint_UpdateCursor);

  // We could do better. But it wouldn't be worth it, URL-specified cursors are
  // rare.
  if (mCursorArrayLength > 0 || aOther.mCursorArrayLength > 0)
    NS_UpdateHint(hint, nsChangeHint_UpdateCursor);

  if (mUserModify != aOther.mUserModify)
    NS_UpdateHint(hint, NS_STYLE_HINT_VISUAL);
  
  if (mUserInput != aOther.mUserInput) {
    if (NS_STYLE_USER_INPUT_NONE == mUserInput ||
        NS_STYLE_USER_INPUT_NONE == aOther.mUserInput) {
      NS_UpdateHint(hint, NS_STYLE_HINT_FRAMECHANGE);
    } else {
      NS_UpdateHint(hint, nsChangeHint_NeutralChange);
    }
  }

  if (mUserFocus != aOther.mUserFocus) {
    NS_UpdateHint(hint, nsChangeHint_NeutralChange);
  }

  if (mWindowDragging != aOther.mWindowDragging) {
    NS_UpdateHint(hint, nsChangeHint_SchedulePaint);
  }

  return hint;
}

void
nsStyleUserInterface::CopyCursorArrayFrom(const nsStyleUserInterface& aSource)
{
  mCursorArray = nullptr;
  mCursorArrayLength = 0;
  if (aSource.mCursorArrayLength) {
    mCursorArray = new nsCursorImage[aSource.mCursorArrayLength];
    if (mCursorArray) {
      mCursorArrayLength = aSource.mCursorArrayLength;
      for (uint32_t i = 0; i < mCursorArrayLength; ++i)
        mCursorArray[i] = aSource.mCursorArray[i];
    }
  }
}

//-----------------------
// nsStyleUIReset
//

nsStyleUIReset::nsStyleUIReset(void) 
{ 
  MOZ_COUNT_CTOR(nsStyleUIReset);
  mUserSelect = NS_STYLE_USER_SELECT_AUTO;
  mForceBrokenImageIcon = 0;
  mIMEMode = NS_STYLE_IME_MODE_AUTO;
  mWindowShadow = NS_STYLE_WINDOW_SHADOW_DEFAULT;
}

nsStyleUIReset::nsStyleUIReset(const nsStyleUIReset& aSource) 
{
  MOZ_COUNT_CTOR(nsStyleUIReset);
  mUserSelect = aSource.mUserSelect;
  mForceBrokenImageIcon = aSource.mForceBrokenImageIcon;
  mIMEMode = aSource.mIMEMode;
  mWindowShadow = aSource.mWindowShadow;
}

nsStyleUIReset::~nsStyleUIReset(void) 
{ 
  MOZ_COUNT_DTOR(nsStyleUIReset);
}

nsChangeHint nsStyleUIReset::CalcDifference(const nsStyleUIReset& aOther) const
{
  // ignore mIMEMode
  if (mForceBrokenImageIcon != aOther.mForceBrokenImageIcon)
    return NS_STYLE_HINT_FRAMECHANGE;
  if (mWindowShadow != aOther.mWindowShadow) {
    // We really need just an nsChangeHint_SyncFrameView, except
    // on an ancestor of the frame, so we get that by doing a
    // reflow.
    return NS_STYLE_HINT_REFLOW;
  }
  if (mUserSelect != aOther.mUserSelect)
    return NS_STYLE_HINT_VISUAL;
  return NS_STYLE_HINT_NONE;
}

//-----------------------
// nsStyleVariables
//

nsStyleVariables::nsStyleVariables()
{
  MOZ_COUNT_CTOR(nsStyleVariables);
}

nsStyleVariables::nsStyleVariables(const nsStyleVariables& aSource)
{
  MOZ_COUNT_CTOR(nsStyleVariables);
}

nsStyleVariables::~nsStyleVariables(void)
{
  MOZ_COUNT_DTOR(nsStyleVariables);
}

nsChangeHint
nsStyleVariables::CalcDifference(const nsStyleVariables& aOther) const
{
  return nsChangeHint(0);
}
