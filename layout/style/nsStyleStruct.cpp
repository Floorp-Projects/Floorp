/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsIAppShellService.h"
#include "nsIWidget.h"
#include "nsCRTGlue.h"
#include "nsCSSParser.h"
#include "nsCSSProps.h"

#include "nsCOMPtr.h"

#include "nsBidiUtils.h"
#include "nsLayoutUtils.h"

#include "imgIRequest.h"
#include "imgIContainer.h"
#include "CounterStyleManager.h"

#include "mozilla/dom/AnimationEffectReadOnlyBinding.h" // for PlaybackDirection
#include "mozilla/Likely.h"
#include "nsIURI.h"
#include "nsIDocument.h"
#include <algorithm>

using namespace mozilla;

static_assert((((1 << nsStyleStructID_Length) - 1) &
               ~(NS_STYLE_INHERIT_MASK)) == 0,
              "Not enough bits in NS_STYLE_INHERIT_MASK");

// These are the limits that we choose to clamp grid line numbers to.
// http://dev.w3.org/csswg/css-grid/#overlarge-grids
const int32_t nsStyleGridLine::kMinLine = -10000;
const int32_t nsStyleGridLine::kMaxLine = 10000;

static bool
EqualURIs(nsIURI *aURI1, nsIURI *aURI2)
{
  bool eq;
  return aURI1 == aURI2 ||    // handle null==null, and optimize
         (aURI1 && aURI2 &&
          NS_SUCCEEDED(aURI1->Equals(aURI2, &eq)) && // not equal on fail
          eq);
}

static bool
EqualURIs(mozilla::css::URLValue *aURI1, mozilla::css::URLValue *aURI2)
{
  return aURI1 == aURI2 ||    // handle null==null, and optimize
         (aURI1 && aURI2 && aURI1->URIEquals(*aURI2));
}

static bool
EqualImages(imgIRequest *aImage1, imgIRequest* aImage2)
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
static int
safe_strcmp(const char16_t* a, const char16_t* b)
{
  if (!a || !b) {
    return (int)(a - b);
  }
  return NS_strcmp(a, b);
}

int32_t
StyleStructContext::AppUnitsPerDevPixel()
{
  return DeviceContext()->AppUnitsPerDevPixel();
}

nsDeviceContext*
StyleStructContext::HackilyFindSomeDeviceContext()
{
  nsCOMPtr<nsIAppShellService> appShell(do_GetService("@mozilla.org/appshell/appShellService;1"));
  MOZ_ASSERT(appShell);
  nsCOMPtr<mozIDOMWindowProxy> win;
  appShell->GetHiddenDOMWindow(getter_AddRefs(win));
  return nsLayoutUtils::GetDeviceContextForScreenInfo(static_cast<nsPIDOMWindowOuter*>(win.get()));
}

static bool AreShadowArraysEqual(nsCSSShadowArray* lhs, nsCSSShadowArray* rhs);

// --------------------
// nsStyleFont
//
nsStyleFont::nsStyleFont(const nsFont& aFont, StyleStructContext aContext)
  : mFont(aFont)
  , mSize(nsStyleFont::ZoomText(aContext, mFont.size))
  , mGenericID(kGenericFont_NONE)
  , mScriptLevel(0)
  , mMathVariant(NS_MATHML_MATHVARIANT_NONE)
  , mMathDisplay(NS_MATHML_DISPLAYSTYLE_INLINE)
  , mMinFontSizeRatio(100) // 100%
  , mExplicitLanguage(false)
  , mAllowZoom(true)
  , mScriptUnconstrainedSize(mSize)
  , mScriptMinSize(nsPresContext::CSSTwipsToAppUnits(
      NS_POINTS_TO_TWIPS(NS_MATHML_DEFAULT_SCRIPT_MIN_SIZE_PT)))
  , mScriptSizeMultiplier(NS_MATHML_DEFAULT_SCRIPT_SIZE_MULTIPLIER)
  , mLanguage(GetLanguage(aContext))
{
  MOZ_COUNT_CTOR(nsStyleFont);
  mFont.size = mSize;
}

nsStyleFont::nsStyleFont(const nsStyleFont& aSrc)
  : mFont(aSrc.mFont)
  , mSize(aSrc.mSize)
  , mGenericID(aSrc.mGenericID)
  , mScriptLevel(aSrc.mScriptLevel)
  , mMathVariant(aSrc.mMathVariant)
  , mMathDisplay(aSrc.mMathDisplay)
  , mMinFontSizeRatio(aSrc.mMinFontSizeRatio)
  , mExplicitLanguage(aSrc.mExplicitLanguage)
  , mAllowZoom(aSrc.mAllowZoom)
  , mScriptUnconstrainedSize(aSrc.mScriptUnconstrainedSize)
  , mScriptMinSize(aSrc.mScriptMinSize)
  , mScriptSizeMultiplier(aSrc.mScriptSizeMultiplier)
  , mLanguage(aSrc.mLanguage)
{
  MOZ_COUNT_CTOR(nsStyleFont);
}

nsStyleFont::nsStyleFont(StyleStructContext aContext)
  : nsStyleFont(*aContext.GetDefaultFont(kPresContext_DefaultVariableFont_ID),
                aContext)
{
}

void 
nsStyleFont::Destroy(nsPresContext* aContext) {
  this->~nsStyleFont();
  aContext->PresShell()->
    FreeByObjectID(eArenaObjectID_nsStyleFont, this);
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

nsChangeHint
nsStyleFont::CalcDifference(const nsStyleFont& aNewData) const
{
  MOZ_ASSERT(mAllowZoom == aNewData.mAllowZoom,
             "expected mAllowZoom to be the same on both nsStyleFonts");
  if (mSize != aNewData.mSize ||
      mFont != aNewData.mFont ||
      mLanguage != aNewData.mLanguage ||
      mExplicitLanguage != aNewData.mExplicitLanguage ||
      mMathVariant != aNewData.mMathVariant ||
      mMathDisplay != aNewData.mMathDisplay ||
      mMinFontSizeRatio != aNewData.mMinFontSizeRatio) {
    return NS_STYLE_HINT_REFLOW;
  }

  // XXX Should any of these cause a non-nsChangeHint_NeutralChange change?
  if (mGenericID != aNewData.mGenericID ||
      mScriptLevel != aNewData.mScriptLevel ||
      mScriptUnconstrainedSize != aNewData.mScriptUnconstrainedSize ||
      mScriptMinSize != aNewData.mScriptMinSize ||
      mScriptSizeMultiplier != aNewData.mScriptSizeMultiplier) {
    return nsChangeHint_NeutralChange;
  }

  return nsChangeHint(0);
}

/* static */ nscoord
nsStyleFont::ZoomText(StyleStructContext aContext, nscoord aSize)
{
  // aSize can be negative (e.g.: calc(-1px)) so we can't assert that here.
  // The caller is expected deal with that.
  return NSToCoordTruncClamped(float(aSize) * aContext.TextZoom());
}

/* static */ nscoord
nsStyleFont::UnZoomText(nsPresContext *aPresContext, nscoord aSize)
{
  // aSize can be negative (e.g.: calc(-1px)) so we can't assert that here.
  // The caller is expected deal with that.
  return NSToCoordTruncClamped(float(aSize) / aPresContext->TextZoom());
}

/* static */ already_AddRefed<nsIAtom>
nsStyleFont::GetLanguage(StyleStructContext aContext)
{
  RefPtr<nsIAtom> language = aContext.GetContentLanguage();
  if (!language) {
    // we didn't find a (usable) Content-Language, so we fall back
    // to whatever the presContext guessed from the charset
    // NOTE this should not be used elsewhere, because we want websites
    // to use UTF-8 with proper language tag, instead of relying on
    // deriving language from charset. See bug 1040668 comment 67.
    language = aContext.GetLanguageFromCharset();
  }
  return language.forget();
}

static nscoord
CalcCoord(const nsStyleCoord& aCoord, const nscoord* aEnumTable, int32_t aNumEnums)
{
  if (aCoord.GetUnit() == eStyleUnit_Enumerated) {
    MOZ_ASSERT(aEnumTable, "must have enum table");
    int32_t value = aCoord.GetIntValue();
    if (0 <= value && value < aNumEnums) {
      return aEnumTable[aCoord.GetIntValue()];
    }
    NS_NOTREACHED("unexpected enum value");
    return 0;
  }
  MOZ_ASSERT(aCoord.ConvertsToLength(), "unexpected unit");
  return nsRuleNode::ComputeCoordPercentCalc(aCoord, 0);
}

nsStyleMargin::nsStyleMargin(StyleStructContext aContext)
{
  MOZ_COUNT_CTOR(nsStyleMargin);
  nsStyleCoord zero(0, nsStyleCoord::CoordConstructor);
  NS_FOR_CSS_SIDES(side) {
    mMargin.Set(side, zero);
  }
}

nsStyleMargin::nsStyleMargin(const nsStyleMargin& aSrc)
  : mMargin(aSrc.mMargin)
{
  MOZ_COUNT_CTOR(nsStyleMargin);
}

void 
nsStyleMargin::Destroy(nsPresContext* aContext) {
  this->~nsStyleMargin();
  aContext->PresShell()->
    FreeByObjectID(eArenaObjectID_nsStyleMargin, this);
}

nsChangeHint
nsStyleMargin::CalcDifference(const nsStyleMargin& aNewData) const
{
  if (mMargin == aNewData.mMargin) {
    return nsChangeHint(0);
  }
  // Margin differences can't affect descendant intrinsic sizes and
  // don't need to force children to reflow.
  return nsChangeHint_NeedReflow |
         nsChangeHint_ReflowChangesSizeOrPosition |
         nsChangeHint_ClearAncestorIntrinsics;
}

nsStylePadding::nsStylePadding(StyleStructContext aContext)
{
  MOZ_COUNT_CTOR(nsStylePadding);
  nsStyleCoord zero(0, nsStyleCoord::CoordConstructor);
  NS_FOR_CSS_SIDES(side) {
    mPadding.Set(side, zero);
  }
}

nsStylePadding::nsStylePadding(const nsStylePadding& aSrc)
  : mPadding(aSrc.mPadding)
{
  MOZ_COUNT_CTOR(nsStylePadding);
}

void 
nsStylePadding::Destroy(nsPresContext* aContext) {
  this->~nsStylePadding();
  aContext->PresShell()->
    FreeByObjectID(eArenaObjectID_nsStylePadding, this);
}

nsChangeHint
nsStylePadding::CalcDifference(const nsStylePadding& aNewData) const
{
  if (mPadding == aNewData.mPadding) {
    return nsChangeHint(0);
  }
  // Padding differences can't affect descendant intrinsic sizes, but do need
  // to force children to reflow so that we can reposition them, since their
  // offsets are from our frame bounds but our content rect's position within
  // those bounds is moving.
  return NS_STYLE_HINT_REFLOW & ~nsChangeHint_ClearDescendantIntrinsics;
}

nsStyleBorder::nsStyleBorder(StyleStructContext aContext)
  : mBorderColors(nullptr)
  , mBorderImageFill(NS_STYLE_BORDER_IMAGE_SLICE_NOFILL)
  , mBorderImageRepeatH(NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH)
  , mBorderImageRepeatV(NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH)
  , mFloatEdge(NS_STYLE_FLOAT_EDGE_CONTENT_BOX)
  , mBoxDecorationBreak(NS_STYLE_BOX_DECORATION_BREAK_SLICE)
  , mComputedBorder(0, 0, 0, 0)
{
  MOZ_COUNT_CTOR(nsStyleBorder);

  NS_FOR_CSS_HALF_CORNERS (corner) {
    mBorderRadius.Set(corner, nsStyleCoord(0, nsStyleCoord::CoordConstructor));
  }

  nscoord medium =
    (StaticPresData::Get()->GetBorderWidthTable())[NS_STYLE_BORDER_WIDTH_MEDIUM];
  NS_FOR_CSS_SIDES(side) {
    mBorderImageSlice.Set(side, nsStyleCoord(1.0f, eStyleUnit_Percent));
    mBorderImageWidth.Set(side, nsStyleCoord(1.0f, eStyleUnit_Factor));
    mBorderImageOutset.Set(side, nsStyleCoord(0.0f, eStyleUnit_Factor));

    mBorder.Side(side) = medium;
    mBorderStyle[side] = NS_STYLE_BORDER_STYLE_NONE | BORDER_COLOR_FOREGROUND;
    mBorderColor[side] = NS_RGB(0, 0, 0);
  }

  mTwipsPerPixel = aContext.DevPixelsToAppUnits(1);
}

nsBorderColors::~nsBorderColors()
{
  NS_CSS_DELETE_LIST_MEMBER(nsBorderColors, this, mNext);
}

nsBorderColors*
nsBorderColors::Clone(bool aDeep) const
{
  nsBorderColors* result = new nsBorderColors(mColor);
  if (MOZ_UNLIKELY(!result)) {
    return result;
  }
  if (aDeep) {
    NS_CSS_CLONE_LIST_MEMBER(nsBorderColors, this, mNext, result, (false));
  }
  return result;
}

nsStyleBorder::nsStyleBorder(const nsStyleBorder& aSrc)
  : mBorderColors(nullptr)
  , mBorderRadius(aSrc.mBorderRadius)
  , mBorderImageSource(aSrc.mBorderImageSource)
  , mBorderImageSlice(aSrc.mBorderImageSlice)
  , mBorderImageWidth(aSrc.mBorderImageWidth)
  , mBorderImageOutset(aSrc.mBorderImageOutset)
  , mBorderImageFill(aSrc.mBorderImageFill)
  , mBorderImageRepeatH(aSrc.mBorderImageRepeatH)
  , mBorderImageRepeatV(aSrc.mBorderImageRepeatV)
  , mFloatEdge(aSrc.mFloatEdge)
  , mBoxDecorationBreak(aSrc.mBoxDecorationBreak)
  , mComputedBorder(aSrc.mComputedBorder)
  , mBorder(aSrc.mBorder)
  , mTwipsPerPixel(aSrc.mTwipsPerPixel)
{
  MOZ_COUNT_CTOR(nsStyleBorder);
  if (aSrc.mBorderColors) {
    EnsureBorderColors();
    for (int32_t i = 0; i < 4; i++) {
      if (aSrc.mBorderColors[i]) {
        mBorderColors[i] = aSrc.mBorderColors[i]->Clone();
      } else {
        mBorderColors[i] = nullptr;
      }
    }
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
    for (int32_t i = 0; i < 4; i++) {
      delete mBorderColors[i];
    }
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
    FreeByObjectID(eArenaObjectID_nsStyleBorder, this);
}

nsChangeHint
nsStyleBorder::CalcDifference(const nsStyleBorder& aNewData) const
{
  // XXXbz we should be able to return a more specific change hint for
  // at least GetComputedBorder() differences...
  if (mTwipsPerPixel != aNewData.mTwipsPerPixel ||
      GetComputedBorder() != aNewData.GetComputedBorder() ||
      mFloatEdge != aNewData.mFloatEdge ||
      mBorderImageOutset != aNewData.mBorderImageOutset ||
      mBoxDecorationBreak != aNewData.mBoxDecorationBreak) {
    return NS_STYLE_HINT_REFLOW;
  }

  NS_FOR_CSS_SIDES(ix) {
    // See the explanation in nsChangeHint.h of
    // nsChangeHint_BorderStyleNoneChange .
    // Furthermore, even though we know *this* side is 0 width, just
    // assume a repaint hint for some other change rather than bother
    // tracking this result through the rest of the function.
    if (HasVisibleStyle(ix) != aNewData.HasVisibleStyle(ix)) {
      return nsChangeHint_RepaintFrame |
             nsChangeHint_BorderStyleNoneChange;
    }
  }

  // Note that mBorderStyle stores not only the border style but also
  // color-related flags.  Given that we've already done an mComputedBorder
  // comparison, border-style differences can only lead to a repaint hint.  So
  // it's OK to just compare the values directly -- if either the actual
  // style or the color flags differ we want to repaint.
  NS_FOR_CSS_SIDES(ix) {
    if (mBorderStyle[ix] != aNewData.mBorderStyle[ix] ||
        mBorderColor[ix] != aNewData.mBorderColor[ix]) {
      return nsChangeHint_RepaintFrame;
    }
  }

  if (mBorderRadius != aNewData.mBorderRadius ||
      !mBorderColors != !aNewData.mBorderColors) {
    return nsChangeHint_RepaintFrame;
  }

  if (IsBorderImageLoaded() || aNewData.IsBorderImageLoaded()) {
    if (mBorderImageSource  != aNewData.mBorderImageSource  ||
        mBorderImageRepeatH != aNewData.mBorderImageRepeatH ||
        mBorderImageRepeatV != aNewData.mBorderImageRepeatV ||
        mBorderImageSlice   != aNewData.mBorderImageSlice   ||
        mBorderImageFill    != aNewData.mBorderImageFill    ||
        mBorderImageWidth   != aNewData.mBorderImageWidth   ||
        mBorderImageOutset  != aNewData.mBorderImageOutset) {
      return nsChangeHint_RepaintFrame;
    }
  }

  // Note that at this point if mBorderColors is non-null so is
  // aNewData.mBorderColors
  if (mBorderColors) {
    NS_FOR_CSS_SIDES(ix) {
      if (!nsBorderColors::Equal(mBorderColors[ix],
                                 aNewData.mBorderColors[ix])) {
        return nsChangeHint_RepaintFrame;
      }
    }
  }

  // mBorder is the specified border value.  Changes to this don't
  // need any change processing, since we operate on the computed
  // border values instead.
  if (mBorder != aNewData.mBorder) {
    return nsChangeHint_NeutralChange;
  }

  return nsChangeHint(0);
}

nsStyleOutline::nsStyleOutline(StyleStructContext aContext)
  : mOutlineWidth(NS_STYLE_BORDER_WIDTH_MEDIUM, eStyleUnit_Enumerated)
  , mOutlineOffset(0)
  , mActualOutlineWidth(0)
  , mOutlineColor(NS_RGB(0, 0, 0))
  , mOutlineStyle(NS_STYLE_BORDER_STYLE_NONE)
  , mTwipsPerPixel(aContext.DevPixelsToAppUnits(1))
{
  MOZ_COUNT_CTOR(nsStyleOutline);
  // spacing values not inherited
  nsStyleCoord zero(0, nsStyleCoord::CoordConstructor);
  NS_FOR_CSS_HALF_CORNERS(corner) {
    mOutlineRadius.Set(corner, zero);
  }

  SetOutlineInitialColor();
}

nsStyleOutline::nsStyleOutline(const nsStyleOutline& aSrc)
  : mOutlineRadius(aSrc.mOutlineRadius)
  , mOutlineWidth(aSrc.mOutlineWidth)
  , mOutlineOffset(aSrc.mOutlineOffset)
  , mActualOutlineWidth(aSrc.mActualOutlineWidth)
  , mOutlineColor(aSrc.mOutlineColor)
  , mOutlineStyle(aSrc.mOutlineStyle)
  , mTwipsPerPixel(aSrc.mTwipsPerPixel)
{
  MOZ_COUNT_CTOR(nsStyleOutline);
}

void 
nsStyleOutline::RecalcData()
{
  if (NS_STYLE_BORDER_STYLE_NONE == GetOutlineStyle()) {
    mActualOutlineWidth = 0;
  } else {
    MOZ_ASSERT(mOutlineWidth.ConvertsToLength() ||
               mOutlineWidth.GetUnit() == eStyleUnit_Enumerated);
    // Clamp negative calc() to 0.
    mActualOutlineWidth =
      std::max(CalcCoord(mOutlineWidth,
                         StaticPresData::Get()->GetBorderWidthTable(), 3), 0);
    mActualOutlineWidth =
      NS_ROUND_BORDER_TO_PIXELS(mActualOutlineWidth, mTwipsPerPixel);
  }
}

nsChangeHint
nsStyleOutline::CalcDifference(const nsStyleOutline& aNewData) const
{
  if (mActualOutlineWidth != aNewData.mActualOutlineWidth ||
      (mActualOutlineWidth > 0 &&
       mOutlineOffset != aNewData.mOutlineOffset)) {
    return nsChangeHint_UpdateOverflow |
           nsChangeHint_SchedulePaint;
  }

  if (mOutlineStyle != aNewData.mOutlineStyle ||
      mOutlineColor != aNewData.mOutlineColor ||
      mOutlineRadius != aNewData.mOutlineRadius) {
    if (mActualOutlineWidth > 0) {
      return nsChangeHint_RepaintFrame;
    }
    return nsChangeHint_NeutralChange;
  }

  if (mOutlineWidth != aNewData.mOutlineWidth ||
      mOutlineOffset != aNewData.mOutlineOffset ||
      mTwipsPerPixel != aNewData.mTwipsPerPixel) {
    return nsChangeHint_NeutralChange;
  }

  return nsChangeHint(0);
}

// --------------------
// nsStyleList
//
nsStyleList::nsStyleList(StyleStructContext aContext) 
  : mListStylePosition(NS_STYLE_LIST_STYLE_POSITION_OUTSIDE)
  , mCounterStyle(aContext.BuildCounterStyle(NS_LITERAL_STRING("disc")))
{
  MOZ_COUNT_CTOR(nsStyleList);
  SetQuotesInitial();
}

nsStyleList::~nsStyleList() 
{
  MOZ_COUNT_DTOR(nsStyleList);
}

nsStyleList::nsStyleList(const nsStyleList& aSource)
  : mListStylePosition(aSource.mListStylePosition)
  , mCounterStyle(aSource.mCounterStyle)
  , mQuotes(aSource.mQuotes)
  , mImageRegion(aSource.mImageRegion)
{
  SetListStyleImage(aSource.GetListStyleImage());
  MOZ_COUNT_CTOR(nsStyleList);
}

void
nsStyleList::SetQuotesInherit(const nsStyleList* aOther)
{
  mQuotes = aOther->mQuotes;
}

void
nsStyleList::SetQuotesInitial()
{
  if (!sInitialQuotes) {
    // The initial value for quotes is the en-US typographic convention:
    // outermost are LEFT and RIGHT DOUBLE QUOTATION MARK, alternating
    // with LEFT and RIGHT SINGLE QUOTATION MARK.
    static const char16_t initialQuotes[8] = {
      0x201C, 0, 0x201D, 0, 0x2018, 0, 0x2019, 0
    };

    sInitialQuotes = new nsStyleQuoteValues;
    sInitialQuotes->mQuotePairs.AppendElement(
        std::make_pair(nsDependentString(&initialQuotes[0], 1),
                       nsDependentString(&initialQuotes[2], 1)));
    sInitialQuotes->mQuotePairs.AppendElement(
        std::make_pair(nsDependentString(&initialQuotes[4], 1),
                       nsDependentString(&initialQuotes[6], 1)));
  }

  mQuotes = sInitialQuotes;
}

void
nsStyleList::SetQuotesNone()
{
  if (!sNoneQuotes) {
    sNoneQuotes = new nsStyleQuoteValues;
  }
  mQuotes = sNoneQuotes;
}

void
nsStyleList::SetQuotes(nsStyleQuoteValues::QuotePairArray&& aValues)
{
  mQuotes = new nsStyleQuoteValues;
  mQuotes->mQuotePairs = Move(aValues);
}

const nsStyleQuoteValues::QuotePairArray&
nsStyleList::GetQuotePairs() const
{
  return mQuotes->mQuotePairs;
}

nsChangeHint
nsStyleList::CalcDifference(const nsStyleList& aNewData) const
{
  // If the quotes implementation is ever going to change we might not need
  // a framechange here and a reflow should be sufficient.  See bug 35768.
  if (mQuotes != aNewData.mQuotes &&
      (mQuotes || aNewData.mQuotes) &&
      GetQuotePairs() != aNewData.GetQuotePairs()) {
    return nsChangeHint_ReconstructFrame;
  }
  if (mListStylePosition != aNewData.mListStylePosition) {
    return nsChangeHint_ReconstructFrame;
  }
  if (EqualImages(mListStyleImage, aNewData.mListStyleImage) &&
      mCounterStyle == aNewData.mCounterStyle) {
    if (mImageRegion.IsEqualInterior(aNewData.mImageRegion)) {
      return nsChangeHint(0);
    }
    if (mImageRegion.width == aNewData.mImageRegion.width &&
        mImageRegion.height == aNewData.mImageRegion.height) {
      return NS_STYLE_HINT_VISUAL;
    }
  }
  return NS_STYLE_HINT_REFLOW;
}

StaticRefPtr<nsStyleQuoteValues>
nsStyleList::sInitialQuotes;

StaticRefPtr<nsStyleQuoteValues>
nsStyleList::sNoneQuotes;


// --------------------
// nsStyleXUL
//
nsStyleXUL::nsStyleXUL(StyleStructContext aContext)
  : mBoxFlex(0.0f)
  , mBoxOrdinal(1)
  , mBoxAlign(NS_STYLE_BOX_ALIGN_STRETCH)
  , mBoxDirection(NS_STYLE_BOX_DIRECTION_NORMAL)
  , mBoxOrient(NS_STYLE_BOX_ORIENT_HORIZONTAL)
  , mBoxPack(NS_STYLE_BOX_PACK_START)
  , mStretchStack(true)
{
  MOZ_COUNT_CTOR(nsStyleXUL);
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

nsChangeHint
nsStyleXUL::CalcDifference(const nsStyleXUL& aNewData) const
{
  if (mBoxAlign == aNewData.mBoxAlign &&
      mBoxDirection == aNewData.mBoxDirection &&
      mBoxFlex == aNewData.mBoxFlex &&
      mBoxOrient == aNewData.mBoxOrient &&
      mBoxPack == aNewData.mBoxPack &&
      mBoxOrdinal == aNewData.mBoxOrdinal &&
      mStretchStack == aNewData.mStretchStack) {
    return nsChangeHint(0);
  }
  if (mBoxOrdinal != aNewData.mBoxOrdinal) {
    return nsChangeHint_ReconstructFrame;
  }
  return NS_STYLE_HINT_REFLOW;
}

// --------------------
// nsStyleColumn
//
/* static */ const uint32_t nsStyleColumn::kMaxColumnCount = 1000;

nsStyleColumn::nsStyleColumn(StyleStructContext aContext)
  : mColumnCount(NS_STYLE_COLUMN_COUNT_AUTO)
  , mColumnWidth(eStyleUnit_Auto)
  , mColumnGap(eStyleUnit_Normal)
  , mColumnRuleColor(NS_RGB(0, 0, 0))
  , mColumnRuleStyle(NS_STYLE_BORDER_STYLE_NONE)
  , mColumnFill(NS_STYLE_COLUMN_FILL_BALANCE)
  , mColumnRuleColorIsForeground(true)
  , mColumnRuleWidth((StaticPresData::Get()
                        ->GetBorderWidthTable())[NS_STYLE_BORDER_WIDTH_MEDIUM])
  , mTwipsPerPixel(aContext.AppUnitsPerDevPixel())
{
  MOZ_COUNT_CTOR(nsStyleColumn);
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

nsChangeHint
nsStyleColumn::CalcDifference(const nsStyleColumn& aNewData) const
{
  if ((mColumnWidth.GetUnit() == eStyleUnit_Auto)
      != (aNewData.mColumnWidth.GetUnit() == eStyleUnit_Auto) ||
      mColumnCount != aNewData.mColumnCount) {
    // We force column count changes to do a reframe, because it's tricky to handle
    // some edge cases where the column count gets smaller and content overflows.
    // XXX not ideal
    return nsChangeHint_ReconstructFrame;
  }

  if (mColumnWidth != aNewData.mColumnWidth ||
      mColumnGap != aNewData.mColumnGap ||
      mColumnFill != aNewData.mColumnFill) {
    return NS_STYLE_HINT_REFLOW;
  }

  if (GetComputedColumnRuleWidth() != aNewData.GetComputedColumnRuleWidth() ||
      mColumnRuleStyle != aNewData.mColumnRuleStyle ||
      mColumnRuleColor != aNewData.mColumnRuleColor ||
      mColumnRuleColorIsForeground != aNewData.mColumnRuleColorIsForeground) {
    return NS_STYLE_HINT_VISUAL;
  }

  // XXX Is it right that we never check mTwipsPerPixel to return a
  // non-nsChangeHint_NeutralChange hint?
  if (mColumnRuleWidth != aNewData.mColumnRuleWidth ||
      mTwipsPerPixel != aNewData.mTwipsPerPixel) {
    return nsChangeHint_NeutralChange;
  }

  return nsChangeHint(0);
}

// --------------------
// nsStyleSVG
//
nsStyleSVG::nsStyleSVG(StyleStructContext aContext)
  : mFill(eStyleSVGPaintType_Color) // Will be initialized to NS_RGB(0, 0, 0)
  , mStroke(eStyleSVGPaintType_None)
  , mStrokeDashoffset(0, nsStyleCoord::CoordConstructor)
  , mStrokeWidth(nsPresContext::CSSPixelsToAppUnits(1), nsStyleCoord::CoordConstructor)
  , mFillOpacity(1.0f)
  , mStrokeMiterlimit(4.0f)
  , mStrokeOpacity(1.0f)
  , mClipRule(NS_STYLE_FILL_RULE_NONZERO)
  , mColorInterpolation(NS_STYLE_COLOR_INTERPOLATION_SRGB)
  , mColorInterpolationFilters(NS_STYLE_COLOR_INTERPOLATION_LINEARRGB)
  , mFillRule(NS_STYLE_FILL_RULE_NONZERO)
  , mPaintOrder(NS_STYLE_PAINT_ORDER_NORMAL)
  , mShapeRendering(NS_STYLE_SHAPE_RENDERING_AUTO)
  , mStrokeLinecap(NS_STYLE_STROKE_LINECAP_BUTT)
  , mStrokeLinejoin(NS_STYLE_STROKE_LINEJOIN_MITER)
  , mTextAnchor(NS_STYLE_TEXT_ANCHOR_START)
  , mContextFlags((eStyleSVGOpacitySource_Normal << FILL_OPACITY_SOURCE_SHIFT) |
                  (eStyleSVGOpacitySource_Normal << STROKE_OPACITY_SOURCE_SHIFT))
{
  MOZ_COUNT_CTOR(nsStyleSVG);
}

nsStyleSVG::~nsStyleSVG()
{
  MOZ_COUNT_DTOR(nsStyleSVG);
}

nsStyleSVG::nsStyleSVG(const nsStyleSVG& aSource)
  : mFill(aSource.mFill)
  , mStroke(aSource.mStroke)
  , mMarkerEnd(aSource.mMarkerEnd)
  , mMarkerMid(aSource.mMarkerMid)
  , mMarkerStart(aSource.mMarkerStart)
  , mStrokeDasharray(aSource.mStrokeDasharray)
  , mStrokeDashoffset(aSource.mStrokeDashoffset)
  , mStrokeWidth(aSource.mStrokeWidth)
  , mFillOpacity(aSource.mFillOpacity)
  , mStrokeMiterlimit(aSource.mStrokeMiterlimit)
  , mStrokeOpacity(aSource.mStrokeOpacity)
  , mClipRule(aSource.mClipRule)
  , mColorInterpolation(aSource.mColorInterpolation)
  , mColorInterpolationFilters(aSource.mColorInterpolationFilters)
  , mFillRule(aSource.mFillRule)
  , mPaintOrder(aSource.mPaintOrder)
  , mShapeRendering(aSource.mShapeRendering)
  , mStrokeLinecap(aSource.mStrokeLinecap)
  , mStrokeLinejoin(aSource.mStrokeLinejoin)
  , mTextAnchor(aSource.mTextAnchor)
  , mContextFlags(aSource.mContextFlags)
{
  MOZ_COUNT_CTOR(nsStyleSVG);
}

static bool
PaintURIChanged(const nsStyleSVGPaint& aPaint1, const nsStyleSVGPaint& aPaint2)
{
  if (aPaint1.mType != aPaint2.mType) {
    return aPaint1.mType == eStyleSVGPaintType_Server ||
           aPaint2.mType == eStyleSVGPaintType_Server;
  }
  return aPaint1.mType == eStyleSVGPaintType_Server &&
    !EqualURIs(aPaint1.mPaint.mPaintServer, aPaint2.mPaint.mPaintServer);
}

nsChangeHint
nsStyleSVG::CalcDifference(const nsStyleSVG& aNewData) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (!EqualURIs(mMarkerEnd, aNewData.mMarkerEnd) ||
      !EqualURIs(mMarkerMid, aNewData.mMarkerMid) ||
      !EqualURIs(mMarkerStart, aNewData.mMarkerStart)) {
    // Markers currently contribute to nsSVGPathGeometryFrame::mRect,
    // so we need a reflow as well as a repaint. No intrinsic sizes need
    // to change, so nsChangeHint_NeedReflow is sufficient.
    return nsChangeHint_UpdateEffects |
           nsChangeHint_NeedReflow |
           nsChangeHint_NeedDirtyReflow | // XXX remove me: bug 876085
           nsChangeHint_RepaintFrame;
  }

  if (mFill != aNewData.mFill ||
      mStroke != aNewData.mStroke ||
      mFillOpacity != aNewData.mFillOpacity ||
      mStrokeOpacity != aNewData.mStrokeOpacity) {
    hint |= nsChangeHint_RepaintFrame;
    if (HasStroke() != aNewData.HasStroke() ||
        (!HasStroke() && HasFill() != aNewData.HasFill())) {
      // Frame bounds and overflow rects depend on whether we "have" fill or
      // stroke. Whether we have stroke or not just changed, or else we have no
      // stroke (in which case whether we have fill or not is significant to frame
      // bounds) and whether we have fill or not just changed. In either case we
      // need to reflow so the frame rect is updated.
      // XXXperf this is a waste on non nsSVGPathGeometryFrames.
      hint |= nsChangeHint_NeedReflow |
              nsChangeHint_NeedDirtyReflow; // XXX remove me: bug 876085
    }
    if (PaintURIChanged(mFill, aNewData.mFill) ||
        PaintURIChanged(mStroke, aNewData.mStroke)) {
      hint |= nsChangeHint_UpdateEffects;
    }
  }

  // Stroke currently contributes to nsSVGPathGeometryFrame::mRect, so
  // we need a reflow here. No intrinsic sizes need to change, so
  // nsChangeHint_NeedReflow is sufficient.
  // Note that stroke-dashoffset does not affect nsSVGPathGeometryFrame::mRect.
  // text-anchor changes also require a reflow since it changes frames' rects.
  if (mStrokeWidth           != aNewData.mStrokeWidth           ||
      mStrokeMiterlimit      != aNewData.mStrokeMiterlimit      ||
      mStrokeLinecap         != aNewData.mStrokeLinecap         ||
      mStrokeLinejoin        != aNewData.mStrokeLinejoin        ||
      mTextAnchor            != aNewData.mTextAnchor) {
    return hint |
           nsChangeHint_NeedReflow |
           nsChangeHint_NeedDirtyReflow | // XXX remove me: bug 876085
           nsChangeHint_RepaintFrame;
  }

  if (hint & nsChangeHint_RepaintFrame) {
    return hint; // we don't add anything else below
  }

  if ( mStrokeDashoffset      != aNewData.mStrokeDashoffset      ||
       mClipRule              != aNewData.mClipRule              ||
       mColorInterpolation    != aNewData.mColorInterpolation    ||
       mColorInterpolationFilters != aNewData.mColorInterpolationFilters ||
       mFillRule              != aNewData.mFillRule              ||
       mPaintOrder            != aNewData.mPaintOrder            ||
       mShapeRendering        != aNewData.mShapeRendering        ||
       mStrokeDasharray       != aNewData.mStrokeDasharray       ||
       mContextFlags          != aNewData.mContextFlags) {
    return hint | nsChangeHint_RepaintFrame;
  }

  return hint;
}

// --------------------
// nsStyleBasicShape

nsCSSKeyword
nsStyleBasicShape::GetShapeTypeName() const
{
  switch (mType) {
    case nsStyleBasicShape::Type::ePolygon:
      return eCSSKeyword_polygon;
    case nsStyleBasicShape::Type::eCircle:
      return eCSSKeyword_circle;
    case nsStyleBasicShape::Type::eEllipse:
      return eCSSKeyword_ellipse;
    case nsStyleBasicShape::Type::eInset:
      return eCSSKeyword_inset;
  }
  NS_NOTREACHED("unexpected type");
  return eCSSKeyword_UNKNOWN;
}

// --------------------
// nsStyleClipPath
//
nsStyleClipPath::nsStyleClipPath()
  : mType(NS_STYLE_CLIP_PATH_NONE)
  , mURL(nullptr)
  , mSizingBox(mozilla::StyleClipShapeSizing::NoBox)
{
}

nsStyleClipPath::nsStyleClipPath(const nsStyleClipPath& aSource)
  : mType(NS_STYLE_CLIP_PATH_NONE)
  , mURL(nullptr)
  , mSizingBox(mozilla::StyleClipShapeSizing::NoBox)
{
  if (aSource.mType == NS_STYLE_CLIP_PATH_URL) {
    SetURL(aSource.mURL);
  } else if (aSource.mType == NS_STYLE_CLIP_PATH_SHAPE) {
    SetBasicShape(aSource.mBasicShape, aSource.mSizingBox);
  } else if (aSource.mType == NS_STYLE_CLIP_PATH_BOX) {
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

  if (aOther.mType == NS_STYLE_CLIP_PATH_URL) {
    SetURL(aOther.mURL);
  } else if (aOther.mType == NS_STYLE_CLIP_PATH_SHAPE) {
    SetBasicShape(aOther.mBasicShape, aOther.mSizingBox);
  } else if (aOther.mType == NS_STYLE_CLIP_PATH_BOX) {
    SetSizingBox(aOther.mSizingBox);
  } else {
    ReleaseRef();
    mSizingBox = mozilla::StyleClipShapeSizing::NoBox;
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
  // mBasicShap, mURL, etc. are all pointers in a union of pointers. Nulling
  // one of them nulls all of them:
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
nsStyleClipPath::SetBasicShape(nsStyleBasicShape* aBasicShape,
                               mozilla::StyleClipShapeSizing aSizingBox)
{
  NS_ASSERTION(aBasicShape, "expected pointer");
  ReleaseRef();
  mBasicShape = aBasicShape;
  mBasicShape->AddRef();
  mSizingBox = aSizingBox;
  mType = NS_STYLE_CLIP_PATH_SHAPE;
}

void
nsStyleClipPath::SetSizingBox(mozilla::StyleClipShapeSizing aSizingBox)
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
  if (this == &aOther) {
    return *this;
  }

  if (aOther.mType == NS_STYLE_FILTER_URL) {
    SetURL(aOther.mURL);
  } else if (aOther.mType == NS_STYLE_FILTER_DROP_SHADOW) {
    SetDropShadow(aOther.mDropShadow);
  } else if (aOther.mType != NS_STYLE_FILTER_NONE) {
    SetFilterParameter(aOther.mFilterParameter, aOther.mType);
  } else {
    ReleaseRef();
    mType = NS_STYLE_FILTER_NONE;
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
  mURL = nullptr;
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
nsStyleSVGReset::nsStyleSVGReset(StyleStructContext aContext)
  : mMask(nsStyleImageLayers::LayerType::Mask)
  , mStopColor(NS_RGB(0, 0, 0))
  , mFloodColor(NS_RGB(0, 0, 0))
  , mLightingColor(NS_RGB(255, 255, 255))
  , mStopOpacity(1.0f)
  , mFloodOpacity(1.0f)
  , mDominantBaseline(NS_STYLE_DOMINANT_BASELINE_AUTO)
  , mVectorEffect(NS_STYLE_VECTOR_EFFECT_NONE)
  , mMaskType(NS_STYLE_MASK_TYPE_LUMINANCE)
{
  MOZ_COUNT_CTOR(nsStyleSVGReset);
}

nsStyleSVGReset::~nsStyleSVGReset()
{
  MOZ_COUNT_DTOR(nsStyleSVGReset);
}

nsStyleSVGReset::nsStyleSVGReset(const nsStyleSVGReset& aSource)
  : mMask(aSource.mMask)
  , mClipPath(aSource.mClipPath)
  , mStopColor(aSource.mStopColor)
  , mFloodColor(aSource.mFloodColor)
  , mLightingColor(aSource.mLightingColor)
  , mStopOpacity(aSource.mStopOpacity)
  , mFloodOpacity(aSource.mFloodOpacity)
  , mDominantBaseline(aSource.mDominantBaseline)
  , mVectorEffect(aSource.mVectorEffect)
  , mMaskType(aSource.mMaskType)
{
  MOZ_COUNT_CTOR(nsStyleSVGReset);
}

void
nsStyleSVGReset::Destroy(nsPresContext* aContext)
{
  mMask.UntrackImages(aContext);

  this->~nsStyleSVGReset();
  aContext->PresShell()->
    FreeByObjectID(mozilla::eArenaObjectID_nsStyleSVGReset, this);
}

nsChangeHint
nsStyleSVGReset::CalcDifference(const nsStyleSVGReset& aNewData) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (mClipPath != aNewData.mClipPath) {
    hint |= nsChangeHint_UpdateEffects |
            nsChangeHint_RepaintFrame;
    // clip-path changes require that we update the PreEffectsBBoxProperty,
    // which is done during overflow computation.
    hint |= nsChangeHint_UpdateOverflow;
  }

  if (mDominantBaseline != aNewData.mDominantBaseline) {
    // XXXjwatt: why NS_STYLE_HINT_REFLOW? Isn't that excessive?
    hint |= NS_STYLE_HINT_REFLOW;
  } else if (mVectorEffect  != aNewData.mVectorEffect) {
    // Stroke currently affects nsSVGPathGeometryFrame::mRect, and
    // vector-effect affect stroke. As a result we need to reflow if
    // vector-effect changes in order to have nsSVGPathGeometryFrame::
    // ReflowSVG called to update its mRect. No intrinsic sizes need
    // to change so nsChangeHint_NeedReflow is sufficient.
    hint |= nsChangeHint_NeedReflow |
            nsChangeHint_NeedDirtyReflow | // XXX remove me: bug 876085
            nsChangeHint_RepaintFrame;
  } else if (mStopColor     != aNewData.mStopColor     ||
             mFloodColor    != aNewData.mFloodColor    ||
             mLightingColor != aNewData.mLightingColor ||
             mStopOpacity   != aNewData.mStopOpacity   ||
             mFloodOpacity  != aNewData.mFloodOpacity  ||
             mMaskType      != aNewData.mMaskType) {
    hint |= nsChangeHint_RepaintFrame;
  }

  hint |= mMask.CalcDifference(aNewData.mMask, nsChangeHint_RepaintFrame);

  return hint;
}

// nsStyleSVGPaint implementation
nsStyleSVGPaint::nsStyleSVGPaint(nsStyleSVGPaintType aType)
  : mType(nsStyleSVGPaintType(0))
  , mFallbackColor(NS_RGB(0, 0, 0))
{
  SetType(aType);
}

nsStyleSVGPaint::nsStyleSVGPaint(const nsStyleSVGPaint& aSource)
  : mType(nsStyleSVGPaintType(0))
  , mFallbackColor(NS_RGB(0, 0, 0))
{
  SetType(aSource.mType);

  mFallbackColor = aSource.mFallbackColor;
  if (mType == eStyleSVGPaintType_Server) {
    mPaint.mPaintServer = aSource.mPaint.mPaintServer;
    NS_IF_ADDREF(mPaint.mPaintServer);
  } else {
    mPaint.mColor = aSource.mPaint.mColor;
  }
}

nsStyleSVGPaint::~nsStyleSVGPaint()
{
  Reset();
}

void
nsStyleSVGPaint::Reset()
{
  SetType(nsStyleSVGPaintType(0));
}

void
nsStyleSVGPaint::SetType(nsStyleSVGPaintType aType)
{
  if (mType == eStyleSVGPaintType_Server) {
    NS_IF_RELEASE(mPaint.mPaintServer);
    mPaint.mPaintServer = nullptr;
  } else {
    mPaint.mColor = NS_RGB(0, 0, 0);
  }
  mType = aType;
}

nsStyleSVGPaint&
nsStyleSVGPaint::operator=(const nsStyleSVGPaint& aOther)
{
  if (this == &aOther) {
    return *this;
  }

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
  if (mType != aOther.mType) {
    return false;
  }
  if (mType == eStyleSVGPaintType_Server) {
    return EqualURIs(mPaint.mPaintServer, aOther.mPaint.mPaintServer) &&
           mFallbackColor == aOther.mFallbackColor;
  }
  if (mType == eStyleSVGPaintType_Color) {
    return mPaint.mColor == aOther.mPaint.mColor;
  }
  return true;
}

// --------------------
// nsStylePosition
//
nsStylePosition::nsStylePosition(StyleStructContext aContext)
  : mWidth(eStyleUnit_Auto)
  , mMinWidth(eStyleUnit_Auto)
  , mMaxWidth(eStyleUnit_None)
  , mHeight(eStyleUnit_Auto)
  , mMinHeight(eStyleUnit_Auto)
  , mMaxHeight(eStyleUnit_None)
  , mFlexBasis(eStyleUnit_Auto)
  , mGridAutoColumnsMin(eStyleUnit_Auto)
  , mGridAutoColumnsMax(eStyleUnit_Auto)
  , mGridAutoRowsMin(eStyleUnit_Auto)
  , mGridAutoRowsMax(eStyleUnit_Auto)
  , mGridAutoFlow(NS_STYLE_GRID_AUTO_FLOW_ROW)
  , mBoxSizing(StyleBoxSizing::Content)
  , mAlignContent(NS_STYLE_ALIGN_NORMAL)
  , mAlignItems(NS_STYLE_ALIGN_NORMAL)
  , mAlignSelf(NS_STYLE_ALIGN_AUTO)
  , mJustifyContent(NS_STYLE_JUSTIFY_NORMAL)
  , mJustifyItems(NS_STYLE_JUSTIFY_AUTO)
  , mJustifySelf(NS_STYLE_JUSTIFY_AUTO)
  , mFlexDirection(NS_STYLE_FLEX_DIRECTION_ROW)
  , mFlexWrap(NS_STYLE_FLEX_WRAP_NOWRAP)
  , mObjectFit(NS_STYLE_OBJECT_FIT_FILL)
  , mOrder(NS_STYLE_ORDER_INITIAL)
  , mFlexGrow(0.0f)
  , mFlexShrink(1.0f)
  , mZIndex(eStyleUnit_Auto)
  , mGridColumnGap(nscoord(0), nsStyleCoord::CoordConstructor)
  , mGridRowGap(nscoord(0), nsStyleCoord::CoordConstructor)
{
  MOZ_COUNT_CTOR(nsStylePosition);

  // positioning values not inherited

  mObjectPosition.SetInitialPercentValues(0.5f);

  nsStyleCoord  autoCoord(eStyleUnit_Auto);
  NS_FOR_CSS_SIDES(side) {
    mOffset.Set(side, autoCoord);
  }

  // The initial value of grid-auto-columns and grid-auto-rows is 'auto',
  // which computes to 'minmax(auto, auto)'.

  // Other members get their default constructors
  // which initialize them to representations of their respective initial value.
  // mGridTemplateAreas: nullptr for 'none'
  // mGridTemplate{Rows,Columns}: false and empty arrays for 'none'
  // mGrid{Column,Row}{Start,End}: false/0/empty values for 'auto'
}

nsStylePosition::~nsStylePosition()
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
  , mJustifyContent(aSource.mJustifyContent)
  , mJustifyItems(aSource.mJustifyItems)
  , mJustifySelf(aSource.mJustifySelf)
  , mFlexDirection(aSource.mFlexDirection)
  , mFlexWrap(aSource.mFlexWrap)
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
  , mGridColumnGap(aSource.mGridColumnGap)
  , mGridRowGap(aSource.mGridRowGap)
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

nsChangeHint
nsStylePosition::CalcDifference(const nsStylePosition& aNewData,
                                const nsStyleVisibility* aOldStyleVisibility) const
{
  nsChangeHint hint = nsChangeHint(0);

  // Changes to "z-index" require a repaint.
  if (mZIndex != aNewData.mZIndex) {
    hint |= nsChangeHint_RepaintFrame;
  }

  // Changes to "object-fit" & "object-position" require a repaint.  They
  // may also require a reflow, if we have a nsSubDocumentFrame, so that we
  // can adjust the size & position of the subdocument.
  if (mObjectFit != aNewData.mObjectFit ||
      mObjectPosition != aNewData.mObjectPosition) {
    hint |= nsChangeHint_RepaintFrame |
            nsChangeHint_NeedReflow;
  }

  if (mOrder != aNewData.mOrder) {
    // "order" impacts both layout order and stacking order, so we need both a
    // reflow and a repaint when it changes.  (Technically, we only need a
    // reflow if we're in a multi-line flexbox (which we can't be sure about,
    // since that's determined by styling on our parent) -- there, "order" can
    // affect which flex line we end up on, & hence can affect our sizing by
    // changing the group of flex items we're competing with for space.)
    return hint |
           nsChangeHint_RepaintFrame |
           nsChangeHint_AllReflowHints;
  }

  if (mBoxSizing != aNewData.mBoxSizing) {
    // Can affect both widths and heights; just a bad scene.
    return hint |
           nsChangeHint_AllReflowHints;
  }

  // Properties that apply to flex items:
  // XXXdholbert These should probably be more targeted (bug 819536)
  if (mAlignSelf != aNewData.mAlignSelf ||
      mFlexBasis != aNewData.mFlexBasis ||
      mFlexGrow != aNewData.mFlexGrow ||
      mFlexShrink != aNewData.mFlexShrink) {
    return hint |
           nsChangeHint_AllReflowHints;
  }

  // Properties that apply to flex containers:
  // - flex-direction can swap a flex container between vertical & horizontal.
  // - align-items can change the sizing of a flex container & the positioning
  //   of its children.
  // - flex-wrap changes whether a flex container's children are wrapped, which
  //   impacts their sizing/positioning and hence impacts the container's size.
  if (mAlignItems != aNewData.mAlignItems ||
      mFlexDirection != aNewData.mFlexDirection ||
      mFlexWrap != aNewData.mFlexWrap) {
    return hint |
           nsChangeHint_AllReflowHints;
  }

  // Properties that apply to grid containers:
  // FIXME: only for grid containers
  // (ie. 'display: grid' or 'display: inline-grid')
  if (mGridTemplateColumns != aNewData.mGridTemplateColumns ||
      mGridTemplateRows != aNewData.mGridTemplateRows ||
      mGridTemplateAreas != aNewData.mGridTemplateAreas ||
      mGridAutoColumnsMin != aNewData.mGridAutoColumnsMin ||
      mGridAutoColumnsMax != aNewData.mGridAutoColumnsMax ||
      mGridAutoRowsMin != aNewData.mGridAutoRowsMin ||
      mGridAutoRowsMax != aNewData.mGridAutoRowsMax ||
      mGridAutoFlow != aNewData.mGridAutoFlow) {
    return hint |
           nsChangeHint_AllReflowHints;
  }

  // Properties that apply to grid items:
  // FIXME: only for grid items
  // (ie. parent frame is 'display: grid' or 'display: inline-grid')
  if (mGridColumnStart != aNewData.mGridColumnStart ||
      mGridColumnEnd != aNewData.mGridColumnEnd ||
      mGridRowStart != aNewData.mGridRowStart ||
      mGridRowEnd != aNewData.mGridRowEnd ||
      mGridColumnGap != aNewData.mGridColumnGap ||
      mGridRowGap != aNewData.mGridRowGap) {
    return hint |
           nsChangeHint_AllReflowHints;
  }

  // Changing 'justify-content/items/self' might affect the positioning,
  // but it won't affect any sizing.
  if (mJustifyContent != aNewData.mJustifyContent ||
      mJustifyItems != aNewData.mJustifyItems ||
      mJustifySelf != aNewData.mJustifySelf) {
    hint |= nsChangeHint_NeedReflow;
  }

  // 'align-content' doesn't apply to a single-line flexbox but we don't know
  // if we're a flex container at this point so we can't optimize for that.
  if (mAlignContent != aNewData.mAlignContent) {
    hint |= nsChangeHint_NeedReflow;
  }

  bool widthChanged = mWidth != aNewData.mWidth ||
                      mMinWidth != aNewData.mMinWidth ||
                      mMaxWidth != aNewData.mMaxWidth;
  bool heightChanged = mHeight != aNewData.mHeight ||
                       mMinHeight != aNewData.mMinHeight ||
                       mMaxHeight != aNewData.mMaxHeight;

  // If aOldStyleVisibility is null, we don't need to bother with any of
  // these tests, since we know that the element never had its
  // nsStyleVisibility accessed, which means it couldn't have done
  // layout.
  // Note that we pass an nsStyleVisibility here because we don't want
  // to cause a new struct to be computed during
  // nsStyleContext::CalcStyleDifference, which can lead to incorrect
  // style data.
  // It doesn't matter whether we're looking at the old or new
  // visibility struct, since a change between vertical and horizontal
  // writing-mode will cause a reframe, and it's easier to pass the old.
  if (aOldStyleVisibility) {
    bool isVertical = WritingMode(aOldStyleVisibility).IsVertical();
    if (isVertical ? widthChanged : heightChanged) {
      // Block-size changes can affect descendant intrinsic sizes due to
      // replaced elements with percentage bsizes in descendants which
      // also have percentage bsizes. This is handled via
      // nsChangeHint_UpdateComputedBSize which clears intrinsic sizes
      // for frames that have such replaced elements.
      hint |= nsChangeHint_NeedReflow |
              nsChangeHint_UpdateComputedBSize |
              nsChangeHint_ReflowChangesSizeOrPosition;
    }

    if (isVertical ? heightChanged : widthChanged) {
      // None of our inline-size differences can affect descendant
      // intrinsic sizes and none of them need to force children to
      // reflow.
      hint |= nsChangeHint_AllReflowHints &
              ~(nsChangeHint_ClearDescendantIntrinsics |
                nsChangeHint_NeedDirtyReflow);
    }
  } else {
    if (widthChanged || heightChanged) {
      hint |= nsChangeHint_NeutralChange;
    }
  }

  // If any of the offsets have changed, then return the respective hints
  // so that we would hopefully be able to avoid reflowing.
  // Note that it is possible that we'll need to reflow when processing
  // restyles, but we don't have enough information to make a good decision
  // right now.
  // Don't try to handle changes between "auto" and non-auto efficiently;
  // that's tricky to do and will hardly ever be able to avoid a reflow.
  if (mOffset != aNewData.mOffset) {
    if (IsAutonessEqual(mOffset, aNewData.mOffset)) {
      hint |= nsChangeHint_RecomputePosition |
              nsChangeHint_UpdateParentOverflow;
    } else {
      hint |= nsChangeHint_AllReflowHints;
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

uint8_t
nsStylePosition::ComputedAlignSelf(nsStyleContext* aParent) const
{
  if (mAlignSelf != NS_STYLE_ALIGN_AUTO) {
    return mAlignSelf;
  }
  if (MOZ_LIKELY(aParent)) {
    auto parentAlignItems = aParent->StylePosition()->ComputedAlignItems();
    MOZ_ASSERT(!(parentAlignItems & NS_STYLE_ALIGN_LEGACY),
               "align-items can't have 'legacy'");
    return parentAlignItems;
  }
  return NS_STYLE_ALIGN_NORMAL;
}

uint8_t
nsStylePosition::ComputedJustifyItems(nsStyleContext* aParent) const
{
  if (mJustifyItems != NS_STYLE_JUSTIFY_AUTO) {
    return mJustifyItems;
  }
  if (MOZ_LIKELY(aParent)) {
    auto inheritedJustifyItems =
      aParent->StylePosition()->ComputedJustifyItems(aParent->GetParent());
    // "If the inherited value of justify-items includes the 'legacy' keyword,
    // 'auto' computes to the inherited value."  Otherwise, 'normal'.
    if (inheritedJustifyItems & NS_STYLE_JUSTIFY_LEGACY) {
      return inheritedJustifyItems;
    }
  }
  return NS_STYLE_JUSTIFY_NORMAL;
}

uint8_t
nsStylePosition::ComputedJustifySelf(nsStyleContext* aParent) const
{
  if (mJustifySelf != NS_STYLE_JUSTIFY_AUTO) {
    return mJustifySelf;
  }
  if (MOZ_LIKELY(aParent)) {
    auto inheritedJustifyItems = aParent->StylePosition()->
      ComputedJustifyItems(aParent->GetParent());
    return inheritedJustifyItems & ~NS_STYLE_JUSTIFY_LEGACY;
  }
  return NS_STYLE_JUSTIFY_NORMAL;
}

// --------------------
// nsStyleTable
//

nsStyleTable::nsStyleTable(StyleStructContext aContext)
  : mLayoutStrategy(NS_STYLE_TABLE_LAYOUT_AUTO)
  , mSpan(1)
{
  MOZ_COUNT_CTOR(nsStyleTable);
}

nsStyleTable::~nsStyleTable()
{
  MOZ_COUNT_DTOR(nsStyleTable);
}

nsStyleTable::nsStyleTable(const nsStyleTable& aSource)
  : mLayoutStrategy(aSource.mLayoutStrategy)
  , mSpan(aSource.mSpan)
{
  MOZ_COUNT_CTOR(nsStyleTable);
}

nsChangeHint
nsStyleTable::CalcDifference(const nsStyleTable& aNewData) const
{
  if (mSpan != aNewData.mSpan ||
      mLayoutStrategy != aNewData.mLayoutStrategy) {
    return nsChangeHint_ReconstructFrame;
  }
  return nsChangeHint(0);
}

// -----------------------
// nsStyleTableBorder

nsStyleTableBorder::nsStyleTableBorder(StyleStructContext aContext)
  : mBorderSpacingCol(0)
  , mBorderSpacingRow(0)
  , mBorderCollapse(NS_STYLE_BORDER_SEPARATE)
  , mCaptionSide(NS_STYLE_CAPTION_SIDE_TOP)
  , mEmptyCells(NS_STYLE_TABLE_EMPTY_CELLS_SHOW)
{
  MOZ_COUNT_CTOR(nsStyleTableBorder);
}

nsStyleTableBorder::~nsStyleTableBorder()
{
  MOZ_COUNT_DTOR(nsStyleTableBorder);
}

nsStyleTableBorder::nsStyleTableBorder(const nsStyleTableBorder& aSource)
  : mBorderSpacingCol(aSource.mBorderSpacingCol)
  , mBorderSpacingRow(aSource.mBorderSpacingRow)
  , mBorderCollapse(aSource.mBorderCollapse)
  , mCaptionSide(aSource.mCaptionSide)
  , mEmptyCells(aSource.mEmptyCells)
{
  MOZ_COUNT_CTOR(nsStyleTableBorder);
}

nsChangeHint
nsStyleTableBorder::CalcDifference(const nsStyleTableBorder& aNewData) const
{
  // Border-collapse changes need a reframe, because we use a different frame
  // class for table cells in the collapsed border model.  This is used to
  // conserve memory when using the separated border model (collapsed borders
  // require extra state to be stored).
  if (mBorderCollapse != aNewData.mBorderCollapse) {
    return nsChangeHint_ReconstructFrame;
  }

  if ((mCaptionSide == aNewData.mCaptionSide) &&
      (mBorderSpacingCol == aNewData.mBorderSpacingCol) &&
      (mBorderSpacingRow == aNewData.mBorderSpacingRow)) {
    if (mEmptyCells == aNewData.mEmptyCells) {
      return nsChangeHint(0);
    }
    return NS_STYLE_HINT_VISUAL;
  } else {
    return NS_STYLE_HINT_REFLOW;
  }
}

// --------------------
// nsStyleColor
//

nsStyleColor::nsStyleColor(StyleStructContext aContext)
  : mColor(aContext.DefaultColor())
{
  MOZ_COUNT_CTOR(nsStyleColor);
}

nsStyleColor::nsStyleColor(const nsStyleColor& aSource)
  : mColor(aSource.mColor)
{
  MOZ_COUNT_CTOR(nsStyleColor);
}

nsChangeHint
nsStyleColor::CalcDifference(const nsStyleColor& aNewData) const
{
  if (mColor == aNewData.mColor) {
    return nsChangeHint(0);
  }
  return nsChangeHint_RepaintFrame;
}

// --------------------
// nsStyleGradient
//
bool
nsStyleGradient::operator==(const nsStyleGradient& aOther) const
{
  MOZ_ASSERT(mSize == NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER ||
             mShape != NS_STYLE_GRADIENT_SHAPE_LINEAR,
             "incorrect combination of shape and size");
  MOZ_ASSERT(aOther.mSize == NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER ||
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
      mRadiusY != aOther.mRadiusY) {
    return false;
  }

  if (mStops.Length() != aOther.mStops.Length()) {
    return false;
  }

  for (uint32_t i = 0; i < mStops.Length(); i++) {
    const auto& stop1 = mStops[i];
    const auto& stop2 = aOther.mStops[i];
    if (stop1.mLocation != stop2.mLocation ||
        stop1.mIsInterpolationHint != stop2.mIsInterpolationHint ||
        (!stop1.mIsInterpolationHint && stop1.mColor != stop2.mColor)) {
      return false;
    }
  }

  return true;
}

nsStyleGradient::nsStyleGradient()
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
    if (NS_GET_A(mStops[i].mColor) < 255) {
      return false;
    }
  }
  return true;
}

bool
nsStyleGradient::HasCalc()
{
  for (uint32_t i = 0; i < mStops.Length(); i++) {
    if (mStops[i].mLocation.IsCalcUnit()) {
      return true;
    }
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
  if (mType != eStyleImageType_Null) {
    SetNull();
  }
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
  if (this != &aOther) {
    DoCopy(aOther);
  }

  return *this;
}

void
nsStyleImage::DoCopy(const nsStyleImage& aOther)
{
  SetNull();

  if (aOther.mType == eStyleImageType_Image) {
    SetImageData(aOther.mImage);
  } else if (aOther.mType == eStyleImageType_Gradient) {
    SetGradientData(aOther.mGradient);
  } else if (aOther.mType == eStyleImageType_Element) {
    SetElementId(aOther.mElementId);
  }

  SetCropRect(aOther.mCropRect);
}

void
nsStyleImage::SetNull()
{
  MOZ_ASSERT(!mImageTracked,
             "Calling SetNull() with image tracked!");

  if (mType == eStyleImageType_Gradient) {
    mGradient->Release();
  } else if (mType == eStyleImageType_Image) {
    NS_RELEASE(mImage);
  } else if (mType == eStyleImageType_Element) {
    free(mElementId);
  }

  mType = eStyleImageType_Null;
  mCropRect = nullptr;
}

void
nsStyleImage::SetImageData(imgRequestProxy* aImage)
{
  MOZ_ASSERT(!mImageTracked,
             "Setting a new image without untracking the old one!");

  NS_IF_ADDREF(aImage);

  if (mType != eStyleImageType_Null) {
    SetNull();
  }

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
  MOZ_ASSERT(!mImageTracked, "Already tracking image!");
  MOZ_ASSERT(mType == eStyleImageType_Image,
             "Can't track image when there isn't one!");

  // Register the image with the document
  nsIDocument* doc = aContext->Document();
  if (doc) {
    doc->AddImage(mImage);
  }

  // Mark state
#ifdef DEBUG
  mImageTracked = true;
#endif
}

void
nsStyleImage::UntrackImage(nsPresContext* aContext)
{
  // Sanity
  MOZ_ASSERT(mImageTracked, "Image not tracked!");
  MOZ_ASSERT(mType == eStyleImageType_Image,
             "Can't untrack image when there isn't one!");

  // Unregister the image with the document
  nsIDocument* doc = aContext->Document();
  if (doc) {
    doc->RemoveImage(mImage);
  }

  // Mark state
#ifdef DEBUG
  mImageTracked = false;
#endif
}

void
nsStyleImage::SetGradientData(nsStyleGradient* aGradient)
{
  if (aGradient) {
    aGradient->AddRef();
  }

  if (mType != eStyleImageType_Null) {
    SetNull();
  }

  if (aGradient) {
    mGradient = aGradient;
    mType = eStyleImageType_Gradient;
  }
}

void
nsStyleImage::SetElementId(const char16_t* aElementId)
{
  if (mType != eStyleImageType_Null) {
    SetNull();
  }

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
  MOZ_ASSERT(pixelValue >= 0, "we ensured non-negative while parsing");
  pixelValue = std::min(pixelValue, double(INT32_MAX)); // avoid overflow
  return NS_lround(pixelValue);
}

bool
nsStyleImage::ComputeActualCropRect(nsIntRect& aActualCropRect,
                                    bool* aIsEntireImage) const
{
  if (mType != eStyleImageType_Image) {
    return false;
  }

  nsCOMPtr<imgIContainer> imageContainer;
  mImage->GetImage(getter_AddRefs(imageContainer));
  if (!imageContainer) {
    return false;
  }

  nsIntSize imageSize;
  imageContainer->GetWidth(&imageSize.width);
  imageContainer->GetHeight(&imageSize.height);
  if (imageSize.width <= 0 || imageSize.height <= 0) {
    return false;
  }

  int32_t left   = ConvertToPixelCoord(mCropRect->GetLeft(),   imageSize.width);
  int32_t top    = ConvertToPixelCoord(mCropRect->GetTop(),    imageSize.height);
  int32_t right  = ConvertToPixelCoord(mCropRect->GetRight(),  imageSize.width);
  int32_t bottom = ConvertToPixelCoord(mCropRect->GetBottom(), imageSize.height);

  // IntersectRect() returns an empty rect if we get negative width or height
  nsIntRect cropRect(left, top, right - left, bottom - top);
  nsIntRect imageRect(nsIntPoint(0, 0), imageSize);
  aActualCropRect.IntersectRect(imageRect, cropRect);

  if (aIsEntireImage) {
    *aIsEntireImage = aActualCropRect.IsEqualInterior(imageRect);
  }
  return true;
}

nsresult
nsStyleImage::StartDecoding() const
{
  if ((mType == eStyleImageType_Image) && mImage) {
    return mImage->StartDecoding();
  }
  return NS_OK;
}

bool
nsStyleImage::IsOpaque() const
{
  if (!IsComplete()) {
    return false;
  }

  if (mType == eStyleImageType_Gradient) {
    return mGradient->IsOpaque();
  }

  if (mType == eStyleImageType_Element) {
    return false;
  }

  MOZ_ASSERT(mType == eStyleImageType_Image, "unexpected image type");

  nsCOMPtr<imgIContainer> imageContainer;
  mImage->GetImage(getter_AddRefs(imageContainer));
  MOZ_ASSERT(imageContainer, "IsComplete() said image container is ready");

  // Check if the crop region of the image is opaque.
  if (imageContainer->IsOpaque()) {
    if (!mCropRect) {
      return true;
    }

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
  if (mType != aOther.mType) {
    return false;
  }

  if (!EqualRects(mCropRect, aOther.mCropRect)) {
    return false;
  }

  if (mType == eStyleImageType_Image) {
    return EqualImages(mImage, aOther.mImage);
  }

  if (mType == eStyleImageType_Gradient) {
    return *mGradient == *aOther.mGradient;
  }

  if (mType == eStyleImageType_Element) {
    return NS_strcmp(mElementId, aOther.mElementId) == 0;
  }

  return true;
}

// --------------------
// nsStyleImageLayers
//

const nsCSSProperty nsStyleImageLayers::kBackgroundLayerTable[] = {
  eCSSProperty_background,                // shorthand
  eCSSProperty_background_color,          // color
  eCSSProperty_background_image,          // image
  eCSSProperty_background_repeat,         // repeat
  eCSSProperty_background_position_x,     // positionX
  eCSSProperty_background_position_y,     // positionY
  eCSSProperty_background_clip,           // clip
  eCSSProperty_background_origin,         // origin
  eCSSProperty_background_size,           // size
  eCSSProperty_background_attachment,     // attachment
  eCSSProperty_UNKNOWN,                   // maskMode
  eCSSProperty_UNKNOWN                    // composite
};

#ifdef MOZ_ENABLE_MASK_AS_SHORTHAND
const nsCSSProperty nsStyleImageLayers::kMaskLayerTable[] = {
  eCSSProperty_mask,                      // shorthand
  eCSSProperty_UNKNOWN,                   // color
  eCSSProperty_mask_image,                // image
  eCSSProperty_mask_repeat,               // repeat
  eCSSProperty_mask_position_x,           // positionX
  eCSSProperty_mask_position_y,           // positionY
  eCSSProperty_mask_clip,                 // clip
  eCSSProperty_mask_origin,               // origin
  eCSSProperty_mask_size,                 // size
  eCSSProperty_UNKNOWN,                   // attachment
  eCSSProperty_mask_mode,                 // maskMode
  eCSSProperty_mask_composite             // composite
};
#endif

nsStyleImageLayers::nsStyleImageLayers(nsStyleImageLayers::LayerType aType)
  : mAttachmentCount(1)
  , mClipCount(1)
  , mOriginCount(1)
  , mRepeatCount(1)
  , mPositionXCount(1)
  , mPositionYCount(1)
  , mImageCount(1)
  , mSizeCount(1)
  , mMaskModeCount(1)
  , mBlendModeCount(1)
  , mCompositeCount(1)
  , mLayers(nsStyleAutoArray<Layer>::WITH_SINGLE_INITIAL_ELEMENT)
{
  MOZ_COUNT_CTOR(nsStyleImageLayers);

  // Ensure first layer is initialized as specified layer type
  mLayers[0].Initialize(aType);
}

nsStyleImageLayers::nsStyleImageLayers(const nsStyleImageLayers &aSource)
  : mAttachmentCount(aSource.mAttachmentCount)
  , mClipCount(aSource.mClipCount)
  , mOriginCount(aSource.mOriginCount)
  , mRepeatCount(aSource.mRepeatCount)
  , mPositionXCount(aSource.mPositionXCount)
  , mPositionYCount(aSource.mPositionYCount)
  , mImageCount(aSource.mImageCount)
  , mSizeCount(aSource.mSizeCount)
  , mMaskModeCount(aSource.mMaskModeCount)
  , mBlendModeCount(aSource.mBlendModeCount)
  , mCompositeCount(aSource.mCompositeCount)
  , mLayers(aSource.mLayers) // deep copy
{
  MOZ_COUNT_CTOR(nsStyleImageLayers);
  // If the deep copy of mLayers failed, truncate the counts.
  uint32_t count = mLayers.Length();
  if (count != aSource.mLayers.Length()) {
    NS_WARNING("truncating counts due to out-of-memory");
    mAttachmentCount = std::max(mAttachmentCount, count);
    mClipCount = std::max(mClipCount, count);
    mOriginCount = std::max(mOriginCount, count);
    mRepeatCount = std::max(mRepeatCount, count);
    mPositionXCount = std::max(mPositionXCount, count);
    mPositionYCount = std::max(mPositionYCount, count);
    mImageCount = std::max(mImageCount, count);
    mSizeCount = std::max(mSizeCount, count);
    mMaskModeCount = std::max(mMaskModeCount, count);
    mBlendModeCount = std::max(mBlendModeCount, count);
    mCompositeCount = std::max(mCompositeCount, count);
  }
}

nsChangeHint
nsStyleImageLayers::CalcDifference(const nsStyleImageLayers& aNewLayers,
                                   nsChangeHint aPositionChangeHint) const
{
  nsChangeHint hint = nsChangeHint(0);

  const nsStyleImageLayers& moreLayers =
    mImageCount > aNewLayers.mImageCount ?
      *this : aNewLayers;
  const nsStyleImageLayers& lessLayers =
    mImageCount > aNewLayers.mImageCount ?
      aNewLayers : *this;

  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, moreLayers) {
    if (i < lessLayers.mImageCount) {
      nsChangeHint layerDifference =
        moreLayers.mLayers[i].CalcDifference(lessLayers.mLayers[i],
                                             aPositionChangeHint);
      hint |= layerDifference;
      if (layerDifference &&
          ((moreLayers.mLayers[i].mImage.GetType() == eStyleImageType_Element) ||
           (lessLayers.mLayers[i].mImage.GetType() == eStyleImageType_Element))) {
        hint |= nsChangeHint_UpdateEffects | nsChangeHint_RepaintFrame;
      }
    } else {
      hint |= nsChangeHint_RepaintFrame;
      if (moreLayers.mLayers[i].mImage.GetType() == eStyleImageType_Element) {
        hint |= nsChangeHint_UpdateEffects | nsChangeHint_RepaintFrame;
      }
    }
  }

  if (hint) {
    return hint;
  }

  if (mAttachmentCount != aNewLayers.mAttachmentCount ||
      mBlendModeCount != aNewLayers.mBlendModeCount ||
      mClipCount != aNewLayers.mClipCount ||
      mCompositeCount != aNewLayers.mCompositeCount ||
      mMaskModeCount != aNewLayers.mMaskModeCount ||
      mOriginCount != aNewLayers.mOriginCount ||
      mRepeatCount != aNewLayers.mRepeatCount ||
      mPositionXCount != aNewLayers.mPositionXCount ||
      mPositionYCount != aNewLayers.mPositionYCount ||
      mSizeCount != aNewLayers.mSizeCount) {
    hint |= nsChangeHint_NeutralChange;
  }

  return hint;
}

bool
nsStyleImageLayers::HasLayerWithImage() const
{
  for (uint32_t i = 0; i < mImageCount; i++) {
    // mLayers[i].mSourceURI can be nullptr if mask-image prop value is
    // <element-reference> or <gradient>
    // mLayers[i].mImage can be empty if mask-image prop value is a reference
    // to SVG mask element.
    // So we need to test both mSourceURI and mImage.
    if (mLayers[i].mSourceURI || !mLayers[i].mImage.IsEmpty()) {
      return true;
    }
  }

  return false;
}

bool
nsStyleImageLayers::Position::IsInitialValue() const
{
  if (mXPosition.mPercent == 0.0 && mXPosition.mLength == 0 &&
      mXPosition.mHasPercent && mYPosition.mPercent == 0.0 &&
      mYPosition.mLength == 0 && mYPosition.mHasPercent) {
    return true;
  }

  return false;
}

void
nsStyleImageLayers::Position::SetInitialPercentValues(float aPercentVal)
{
  mXPosition.mPercent = aPercentVal;
  mXPosition.mLength = 0;
  mXPosition.mHasPercent = true;
  mYPosition.mPercent = aPercentVal;
  mYPosition.mLength = 0;
  mYPosition.mHasPercent = true;
}

void
nsStyleImageLayers::Position::SetInitialZeroValues()
{
  mXPosition.mPercent = 0;
  mXPosition.mLength = 0;
  mXPosition.mHasPercent = false;
  mYPosition.mPercent = 0;
  mYPosition.mLength = 0;
  mYPosition.mHasPercent = false;
}

bool
nsStyleImageLayers::Size::DependsOnPositioningAreaSize(const nsStyleImage& aImage) const
{
  MOZ_ASSERT(aImage.GetType() != eStyleImageType_Null,
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

  MOZ_ASSERT((mWidthType == eLengthPercentage && mHeightType == eAuto) ||
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
      CSSIntSize imageSize;
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
nsStyleImageLayers::Size::SetInitialValues()
{
  mWidthType = mHeightType = eAuto;
}

bool
nsStyleImageLayers::Size::operator==(const Size& aOther) const
{
  MOZ_ASSERT(mWidthType < eDimensionType_COUNT,
             "bad mWidthType for this");
  MOZ_ASSERT(mHeightType < eDimensionType_COUNT,
             "bad mHeightType for this");
  MOZ_ASSERT(aOther.mWidthType < eDimensionType_COUNT,
             "bad mWidthType for aOther");
  MOZ_ASSERT(aOther.mHeightType < eDimensionType_COUNT,
             "bad mHeightType for aOther");

  return mWidthType == aOther.mWidthType &&
         mHeightType == aOther.mHeightType &&
         (mWidthType != eLengthPercentage || mWidth == aOther.mWidth) &&
         (mHeightType != eLengthPercentage || mHeight == aOther.mHeight);
}

bool
nsStyleImageLayers::Repeat::IsInitialValue(LayerType aType) const
{
  if (aType == LayerType::Background ||
      aType == LayerType::Mask) {
    // bug 1258623 - mask-repeat initial value should be no-repeat
    return mXRepeat == NS_STYLE_IMAGELAYER_REPEAT_REPEAT &&
           mYRepeat == NS_STYLE_IMAGELAYER_REPEAT_REPEAT;
  }

  MOZ_ASSERT_UNREACHABLE("unsupported layer type.");
  return false;
}

void
nsStyleImageLayers::Repeat::SetInitialValues(LayerType aType)
{
  if (aType == LayerType::Background ||
      aType == LayerType::Mask) {
    // bug 1258623 - mask-repeat initial value should be no-repeat
    mXRepeat = NS_STYLE_IMAGELAYER_REPEAT_REPEAT;
    mYRepeat = NS_STYLE_IMAGELAYER_REPEAT_REPEAT;
  } else {
    MOZ_ASSERT_UNREACHABLE("unsupported layer type.");
  }
}

nsStyleImageLayers::Layer::Layer()
  : mClip(NS_STYLE_IMAGELAYER_CLIP_BORDER)
  , mAttachment(NS_STYLE_IMAGELAYER_ATTACHMENT_SCROLL)
  , mBlendMode(NS_STYLE_BLEND_NORMAL)
  , mComposite(NS_STYLE_MASK_COMPOSITE_ADD)
  , mMaskMode(NS_STYLE_MASK_MODE_MATCH_SOURCE)
{
  mPosition.SetInitialPercentValues(0.0f); // Initial value is "0% 0%"
  mImage.SetNull();
  mSize.SetInitialValues();
}

nsStyleImageLayers::Layer::~Layer()
{
}

void
nsStyleImageLayers::Layer::Initialize(nsStyleImageLayers::LayerType aType)
{
  mRepeat.SetInitialValues(aType);

  if (aType == LayerType::Background) {
    mOrigin = NS_STYLE_IMAGELAYER_ORIGIN_PADDING;
  } else {
    MOZ_ASSERT(aType == LayerType::Mask, "unsupported layer type.");
    mOrigin = NS_STYLE_IMAGELAYER_ORIGIN_BORDER;
  }
}

bool
nsStyleImageLayers::Layer::RenderingMightDependOnPositioningAreaSizeChange() const
{
  // Do we even have an image?
  if (mImage.IsEmpty()) {
    return false;
  }

  return mPosition.DependsOnPositioningAreaSize() ||
      mSize.DependsOnPositioningAreaSize(mImage) ||
      mRepeat.DependsOnPositioningAreaSize();
}

bool
nsStyleImageLayers::Layer::operator==(const Layer& aOther) const
{
  return mAttachment == aOther.mAttachment &&
         mClip == aOther.mClip &&
         mOrigin == aOther.mOrigin &&
         mRepeat == aOther.mRepeat &&
         mBlendMode == aOther.mBlendMode &&
         mPosition == aOther.mPosition &&
         mSize == aOther.mSize &&
         mImage == aOther.mImage &&
         mMaskMode == aOther.mMaskMode &&
         mComposite == aOther.mComposite &&
         EqualURIs(mSourceURI, aOther.mSourceURI);
}

nsChangeHint
nsStyleImageLayers::Layer::CalcDifference(const nsStyleImageLayers::Layer& aNewLayer,
                                          nsChangeHint aPositionChangeHint) const
{
  nsChangeHint hint = nsChangeHint(0);
  if (!EqualURIs(mSourceURI, aNewLayer.mSourceURI)) {
    hint |= nsChangeHint_RepaintFrame;

    // If Layer::mSourceURI links to a SVG mask, it has a fragment. Not vice
    // versa. Here are examples of URI contains a fragment, two of them link
    // to a SVG mask:
    //   mask:url(a.svg#maskID); // The fragment of this URI is an ID of a mask
    //                           // element in a.svg.
    //   mask:url(#localMaskID); // The fragment of this URI is an ID of a mask
    //                           // element in local document.
    //   mask:url(b.svg#viewBoxID); // The fragment of this URI is an ID of a
    //                              // viewbox defined in b.svg.
    // That is, if mSourceURI has a fragment, it may link to a SVG mask; If
    // not, it "must" not link to a SVG mask.
    bool maybeSVGMask = false;
    if (mSourceURI) {
      mSourceURI->GetHasRef(&maybeSVGMask);
    }
    if (!maybeSVGMask && aNewLayer.mSourceURI) {
      aNewLayer.mSourceURI->GetHasRef(&maybeSVGMask);
    }

    // Return nsChangeHint_UpdateEffects and nsChangeHint_UpdateOverflow if
    // either URI might link to an SVG mask.
    if (maybeSVGMask) {
      hint |= nsChangeHint_UpdateEffects;
      // Mask changes require that we update the PreEffectsBBoxProperty,
      // which is done during overflow computation.
      hint |= nsChangeHint_UpdateOverflow;
    }
  } else if (mAttachment != aNewLayer.mAttachment ||
             mClip != aNewLayer.mClip ||
             mOrigin != aNewLayer.mOrigin ||
             mRepeat != aNewLayer.mRepeat ||
             mBlendMode != aNewLayer.mBlendMode ||
             mSize != aNewLayer.mSize ||
             mImage != aNewLayer.mImage ||
             mMaskMode != aNewLayer.mMaskMode ||
             mComposite != aNewLayer.mComposite) {
    hint |= nsChangeHint_RepaintFrame;
  }

  if (mPosition != aNewLayer.mPosition) {
    hint |= aPositionChangeHint;
  }

  return hint;
}

// --------------------
// nsStyleBackground
//

nsStyleBackground::nsStyleBackground(StyleStructContext aContext)
  : mImage(nsStyleImageLayers::LayerType::Background)
  , mBackgroundColor(NS_RGBA(0, 0, 0, 0))
{
  MOZ_COUNT_CTOR(nsStyleBackground);
}

nsStyleBackground::nsStyleBackground(const nsStyleBackground& aSource)
  : mImage(aSource.mImage)
  , mBackgroundColor(aSource.mBackgroundColor)
{
  MOZ_COUNT_CTOR(nsStyleBackground);
}

nsStyleBackground::~nsStyleBackground()
{
  MOZ_COUNT_DTOR(nsStyleBackground);
}

void
nsStyleBackground::Destroy(nsPresContext* aContext)
{
  // Untrack all the images stored in our layers
  mImage.UntrackImages(aContext);

  this->~nsStyleBackground();
  aContext->PresShell()->
    FreeByObjectID(eArenaObjectID_nsStyleBackground, this);
}

nsChangeHint
nsStyleBackground::CalcDifference(const nsStyleBackground& aNewData) const
{
  nsChangeHint hint = nsChangeHint(0);
  if (mBackgroundColor != aNewData.mBackgroundColor) {
    hint |= nsChangeHint_RepaintFrame;
  }

  hint |= mImage.CalcDifference(aNewData.mImage,
                                nsChangeHint_UpdateBackgroundPosition);

  return hint;
}

bool
nsStyleBackground::HasFixedBackground(nsIFrame* aFrame) const
{
  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, mImage) {
    const nsStyleImageLayers::Layer &layer = mImage.mLayers[i];
    if (layer.mAttachment == NS_STYLE_IMAGELAYER_ATTACHMENT_FIXED &&
        !layer.mImage.IsEmpty() &&
        !nsLayoutUtils::IsTransformed(aFrame)) {
      return true;
    }
  }
  return false;
}

bool
nsStyleBackground::IsTransparent() const
{
  return BottomLayer().mImage.IsEmpty() &&
         mImage.mImageCount == 1 &&
         NS_GET_A(mBackgroundColor) == 0;
}

void
nsTimingFunction::AssignFromKeyword(int32_t aTimingFunctionType)
{
  switch (aTimingFunctionType) {
    case NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_START:
      mType = Type::StepStart;
      mStepSyntax = StepSyntax::Keyword;
      mSteps = 1;
      return;
    default:
      MOZ_FALLTHROUGH_ASSERT("aTimingFunctionType must be a keyword value");
    case NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END:
      mType = Type::StepEnd;
      mStepSyntax = StepSyntax::Keyword;
      mSteps = 1;
      return;
    case NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE:
    case NS_STYLE_TRANSITION_TIMING_FUNCTION_LINEAR:
    case NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN:
    case NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_OUT:
    case NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN_OUT:
      mType = static_cast<Type>(aTimingFunctionType);
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

  MOZ_ASSERT(0 <= aTimingFunctionType && aTimingFunctionType < 5,
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
mozilla::StyleTransition::SetUnknownProperty(nsCSSProperty aProperty,
                                             const nsAString& aPropertyString)
{
  MOZ_ASSERT(nsCSSProps::LookupProperty(aPropertyString,
                                        CSSEnabledState::eForAllContent) ==
               aProperty,
             "property and property string should match");
  MOZ_ASSERT(aProperty == eCSSProperty_UNKNOWN ||
             aProperty == eCSSPropertyExtra_variable,
             "should be either unknown or custom property");
  mProperty = aProperty;
  mUnknownProperty = NS_Atomize(aPropertyString);
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
  mDirection = dom::PlaybackDirection::Normal;
  mFillMode = dom::FillMode::None;
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

// --------------------
// nsStyleDisplay
//
nsStyleDisplay::nsStyleDisplay(StyleStructContext aContext)
  : mDisplay(NS_STYLE_DISPLAY_INLINE)
  , mOriginalDisplay(NS_STYLE_DISPLAY_INLINE)
  , mContain(NS_STYLE_CONTAIN_NONE)
  , mAppearance(NS_THEME_NONE)
  , mPosition(NS_STYLE_POSITION_STATIC)
  , mFloat(NS_STYLE_FLOAT_NONE)
  , mOriginalFloat(NS_STYLE_FLOAT_NONE)
  , mBreakType(NS_STYLE_CLEAR_NONE)
  , mBreakInside(NS_STYLE_PAGE_BREAK_AUTO)
  , mBreakBefore(false)
  , mBreakAfter(false)
  , mOverflowX(NS_STYLE_OVERFLOW_VISIBLE)
  , mOverflowY(NS_STYLE_OVERFLOW_VISIBLE)
  , mOverflowClipBox(NS_STYLE_OVERFLOW_CLIP_BOX_PADDING_BOX)
  , mResize(NS_STYLE_RESIZE_NONE)
  , mOrient(NS_STYLE_ORIENT_INLINE)
  , mIsolation(NS_STYLE_ISOLATION_AUTO)
  , mTopLayer(NS_STYLE_TOP_LAYER_NONE)
  , mWillChangeBitField(0)
  , mTouchAction(NS_STYLE_TOUCH_ACTION_AUTO)
  , mScrollBehavior(NS_STYLE_SCROLL_BEHAVIOR_AUTO)
  , mScrollSnapTypeX(NS_STYLE_SCROLL_SNAP_TYPE_NONE)
  , mScrollSnapTypeY(NS_STYLE_SCROLL_SNAP_TYPE_NONE)
  , mScrollSnapPointsX(eStyleUnit_None)
  , mScrollSnapPointsY(eStyleUnit_None)
  , mBackfaceVisibility(NS_STYLE_BACKFACE_VISIBILITY_VISIBLE)
  , mTransformStyle(NS_STYLE_TRANSFORM_STYLE_FLAT)
  , mTransformBox(NS_STYLE_TRANSFORM_BOX_BORDER_BOX)
  , mSpecifiedTransform(nullptr)
  , mChildPerspective(eStyleUnit_None)
  , mVerticalAlign(NS_STYLE_VERTICAL_ALIGN_BASELINE, eStyleUnit_Enumerated)
  , mTransitions(nsStyleAutoArray<StyleTransition>::WITH_SINGLE_INITIAL_ELEMENT)
  , mTransitionTimingFunctionCount(1)
  , mTransitionDurationCount(1)
  , mTransitionDelayCount(1)
  , mTransitionPropertyCount(1)
  , mAnimations(nsStyleAutoArray<StyleAnimation>::WITH_SINGLE_INITIAL_ELEMENT)
  , mAnimationTimingFunctionCount(1)
  , mAnimationDurationCount(1)
  , mAnimationDelayCount(1)
  , mAnimationNameCount(1)
  , mAnimationDirectionCount(1)
  , mAnimationFillModeCount(1)
  , mAnimationPlayStateCount(1)
  , mAnimationIterationCountCount(1)
{
  MOZ_COUNT_CTOR(nsStyleDisplay);
  mTransformOrigin[0].SetPercentValue(0.5f); // Transform is centered on origin
  mTransformOrigin[1].SetPercentValue(0.5f);
  mTransformOrigin[2].SetCoordValue(0);
  mPerspectiveOrigin[0].SetPercentValue(0.5f);
  mPerspectiveOrigin[1].SetPercentValue(0.5f);

  // Initial value for mScrollSnapDestination is "0px 0px"
  mScrollSnapDestination.SetInitialZeroValues();

  mTransitions[0].SetInitialValues();
  mAnimations[0].SetInitialValues();
}

nsStyleDisplay::nsStyleDisplay(const nsStyleDisplay& aSource)
  : mBinding(aSource.mBinding)
  , mDisplay(aSource.mDisplay)
  , mOriginalDisplay(aSource.mOriginalDisplay)
  , mContain(aSource.mContain)
  , mAppearance(aSource.mAppearance)
  , mPosition(aSource.mPosition)
  , mFloat(aSource.mFloat)
  , mOriginalFloat(aSource.mOriginalFloat)
  , mBreakType(aSource.mBreakType)
  , mBreakInside(aSource.mBreakInside)
  , mBreakBefore(aSource.mBreakBefore)
  , mBreakAfter(aSource.mBreakAfter)
  , mOverflowX(aSource.mOverflowX)
  , mOverflowY(aSource.mOverflowY)
  , mOverflowClipBox(aSource.mOverflowClipBox)
  , mResize(aSource.mResize)
  , mOrient(aSource.mOrient)
  , mIsolation(aSource.mIsolation)
  , mTopLayer(aSource.mTopLayer)
  , mWillChangeBitField(aSource.mWillChangeBitField)
  , mWillChange(aSource.mWillChange)
  , mTouchAction(aSource.mTouchAction)
  , mScrollBehavior(aSource.mScrollBehavior)
  , mScrollSnapTypeX(aSource.mScrollSnapTypeX)
  , mScrollSnapTypeY(aSource.mScrollSnapTypeY)
  , mScrollSnapPointsX(aSource.mScrollSnapPointsX)
  , mScrollSnapPointsY(aSource.mScrollSnapPointsY)
  , mScrollSnapDestination(aSource.mScrollSnapDestination)
  , mScrollSnapCoordinate(aSource.mScrollSnapCoordinate)
  , mBackfaceVisibility(aSource.mBackfaceVisibility)
  , mTransformStyle(aSource.mTransformStyle)
  , mTransformBox(aSource.mTransformBox)
  , mSpecifiedTransform(aSource.mSpecifiedTransform)
  , mChildPerspective(aSource.mChildPerspective)
  , mVerticalAlign(aSource.mVerticalAlign)
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

nsChangeHint
nsStyleDisplay::CalcDifference(const nsStyleDisplay& aNewData) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (!EqualURIs(mBinding, aNewData.mBinding)
      || mPosition != aNewData.mPosition
      || mDisplay != aNewData.mDisplay
      || mContain != aNewData.mContain
      || (mFloat == NS_STYLE_FLOAT_NONE) != (aNewData.mFloat == NS_STYLE_FLOAT_NONE)
      || mOverflowX != aNewData.mOverflowX
      || mOverflowY != aNewData.mOverflowY
      || mScrollBehavior != aNewData.mScrollBehavior
      || mScrollSnapTypeX != aNewData.mScrollSnapTypeX
      || mScrollSnapTypeY != aNewData.mScrollSnapTypeY
      || mScrollSnapPointsX != aNewData.mScrollSnapPointsX
      || mScrollSnapPointsY != aNewData.mScrollSnapPointsY
      || mScrollSnapDestination != aNewData.mScrollSnapDestination
      || mTopLayer != aNewData.mTopLayer
      || mResize != aNewData.mResize) {
    hint |= nsChangeHint_ReconstructFrame;
  }

  /* Note: When mScrollBehavior, mScrollSnapTypeX, mScrollSnapTypeY,
   * mScrollSnapPointsX, mScrollSnapPointsY, or mScrollSnapDestination are
   * changed, nsChangeHint_NeutralChange is not sufficient to enter
   * nsCSSFrameConstructor::PropagateScrollToViewport. By using the same hint
   * as used when the overflow css property changes,
   * nsChangeHint_ReconstructFrame, PropagateScrollToViewport will be called.
   *
   * The scroll-behavior css property is not expected to change often (the
   * CSSOM-View DOM methods are likely to be used in those cases); however,
   * if this does become common perhaps a faster-path might be worth while.
   */

  if ((mAppearance == NS_THEME_TEXTFIELD &&
       aNewData.mAppearance != NS_THEME_TEXTFIELD) ||
      (mAppearance != NS_THEME_TEXTFIELD &&
       aNewData.mAppearance == NS_THEME_TEXTFIELD)) {
    // This is for <input type=number> where we allow authors to specify a
    // |-moz-appearance:textfield| to get a control without a spinner. (The
    // spinner is present for |-moz-appearance:number-input| but also other
    // values such as 'none'.) We need to reframe since we want to use
    // nsTextControlFrame instead of nsNumberControlFrame if the author
    // specifies 'textfield'.
    return nsChangeHint_ReconstructFrame;
  }

  if (mFloat != aNewData.mFloat) {
    // Changing which side we float on doesn't affect descendants directly
    hint |= nsChangeHint_AllReflowHints &
            ~(nsChangeHint_ClearDescendantIntrinsics |
              nsChangeHint_NeedDirtyReflow);
  }

  if (mVerticalAlign != aNewData.mVerticalAlign) {
    // XXX Can this just be AllReflowHints + RepaintFrame, and be included in
    // the block below?
    hint |= NS_STYLE_HINT_REFLOW;
  }

  // XXX the following is conservative, for now: changing float breaking shouldn't
  // necessarily require a repaint, reflow should suffice.
  if (mBreakType != aNewData.mBreakType
      || mBreakInside != aNewData.mBreakInside
      || mBreakBefore != aNewData.mBreakBefore
      || mBreakAfter != aNewData.mBreakAfter
      || mAppearance != aNewData.mAppearance
      || mOrient != aNewData.mOrient
      || mOverflowClipBox != aNewData.mOverflowClipBox) {
    hint |= nsChangeHint_AllReflowHints |
            nsChangeHint_RepaintFrame;
  }

  if (mIsolation != aNewData.mIsolation) {
    hint |= nsChangeHint_RepaintFrame;
  }

  /* If we've added or removed the transform property, we need to reconstruct the frame to add
   * or remove the view object, and also to handle abs-pos and fixed-pos containers.
   */
  if (HasTransformStyle() != aNewData.HasTransformStyle()) {
    // We do not need to apply nsChangeHint_UpdateTransformLayer since
    // nsChangeHint_RepaintFrame will forcibly invalidate the frame area and
    // ensure layers are rebuilt (or removed).
    hint |= nsChangeHint_UpdateContainingBlock |
            nsChangeHint_UpdateOverflow |
            nsChangeHint_RepaintFrame;
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

    if (!mSpecifiedTransform != !aNewData.mSpecifiedTransform ||
        (mSpecifiedTransform &&
         *mSpecifiedTransform != *aNewData.mSpecifiedTransform)) {
      transformHint |= nsChangeHint_UpdateTransformLayer;

      if (mSpecifiedTransform &&
          aNewData.mSpecifiedTransform) {
        transformHint |= nsChangeHint_UpdatePostTransformOverflow;
      } else {
        transformHint |= nsChangeHint_UpdateOverflow;
      }
    }

    const nsChangeHint kUpdateOverflowAndRepaintHint =
      nsChangeHint_UpdateOverflow | nsChangeHint_RepaintFrame;
    for (uint8_t index = 0; index < 3; ++index) {
      if (mTransformOrigin[index] != aNewData.mTransformOrigin[index]) {
        transformHint |= nsChangeHint_UpdateTransformLayer |
                         nsChangeHint_UpdatePostTransformOverflow;
        break;
      }
    }

    for (uint8_t index = 0; index < 2; ++index) {
      if (mPerspectiveOrigin[index] != aNewData.mPerspectiveOrigin[index]) {
        transformHint |= kUpdateOverflowAndRepaintHint;
        break;
      }
    }

    if (HasPerspectiveStyle() != aNewData.HasPerspectiveStyle()) {
      // A change from/to being a containing block for position:fixed.
      hint |= nsChangeHint_UpdateContainingBlock;
    }

    if (mChildPerspective != aNewData.mChildPerspective ||
        mTransformStyle != aNewData.mTransformStyle ||
        mTransformBox != aNewData.mTransformBox) {
      transformHint |= kUpdateOverflowAndRepaintHint;
    }

    if (mBackfaceVisibility != aNewData.mBackfaceVisibility) {
      transformHint |= nsChangeHint_RepaintFrame;
    }

    if (transformHint) {
      if (HasTransformStyle()) {
        hint |= transformHint;
      } else {
        hint |= nsChangeHint_NeutralChange;
      }
    }
  }

  // Note that the HasTransformStyle() != aNewData.HasTransformStyle() 
  // test above handles relevant changes in the
  // NS_STYLE_WILL_CHANGE_TRANSFORM bit, which in turn handles frame 
  // reconstruction for changes in the containing block of 
  // fixed-positioned elements.
  uint8_t willChangeBitsChanged =
    mWillChangeBitField ^ aNewData.mWillChangeBitField;
  if (willChangeBitsChanged & (NS_STYLE_WILL_CHANGE_STACKING_CONTEXT |
                               NS_STYLE_WILL_CHANGE_SCROLL |
                               NS_STYLE_WILL_CHANGE_OPACITY)) {
    hint |= nsChangeHint_RepaintFrame;
  }

  if (willChangeBitsChanged & NS_STYLE_WILL_CHANGE_FIXPOS_CB) {
    hint |= nsChangeHint_UpdateContainingBlock;
  }

  // If touch-action is changed, we need to regenerate the event regions on
  // the layers and send it over to the compositor for APZ to handle.
  if (mTouchAction != aNewData.mTouchAction) {
    hint |= nsChangeHint_RepaintFrame;
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
      (mOriginalDisplay != aNewData.mOriginalDisplay ||
       mOriginalFloat != aNewData.mOriginalFloat ||
       mTransitions != aNewData.mTransitions ||
       mTransitionTimingFunctionCount !=
         aNewData.mTransitionTimingFunctionCount ||
       mTransitionDurationCount != aNewData.mTransitionDurationCount ||
       mTransitionDelayCount != aNewData.mTransitionDelayCount ||
       mTransitionPropertyCount != aNewData.mTransitionPropertyCount ||
       mAnimations != aNewData.mAnimations ||
       mAnimationTimingFunctionCount != aNewData.mAnimationTimingFunctionCount ||
       mAnimationDurationCount != aNewData.mAnimationDurationCount ||
       mAnimationDelayCount != aNewData.mAnimationDelayCount ||
       mAnimationNameCount != aNewData.mAnimationNameCount ||
       mAnimationDirectionCount != aNewData.mAnimationDirectionCount ||
       mAnimationFillModeCount != aNewData.mAnimationFillModeCount ||
       mAnimationPlayStateCount != aNewData.mAnimationPlayStateCount ||
       mAnimationIterationCountCount != aNewData.mAnimationIterationCountCount ||
       mScrollSnapCoordinate != aNewData.mScrollSnapCoordinate)) {
    hint |= nsChangeHint_NeutralChange;
  }

  return hint;
}

// --------------------
// nsStyleVisibility
//

nsStyleVisibility::nsStyleVisibility(StyleStructContext aContext)
  : mDirection(aContext.GetBidi() == IBMBIDI_TEXTDIRECTION_RTL
                 ? NS_STYLE_DIRECTION_RTL
                 : NS_STYLE_DIRECTION_LTR)
  , mVisible(NS_STYLE_VISIBILITY_VISIBLE)
  , mImageRendering(NS_STYLE_IMAGE_RENDERING_AUTO)
  , mWritingMode(NS_STYLE_WRITING_MODE_HORIZONTAL_TB)
  , mTextOrientation(NS_STYLE_TEXT_ORIENTATION_MIXED)
  , mColorAdjust(NS_STYLE_COLOR_ADJUST_ECONOMY)
{
  MOZ_COUNT_CTOR(nsStyleVisibility);
}

nsStyleVisibility::nsStyleVisibility(const nsStyleVisibility& aSource)
  : mImageOrientation(aSource.mImageOrientation)
  , mDirection(aSource.mDirection)
  , mVisible(aSource.mVisible)
  , mImageRendering(aSource.mImageRendering)
  , mWritingMode(aSource.mWritingMode)
  , mTextOrientation(aSource.mTextOrientation)
  , mColorAdjust(aSource.mColorAdjust)
{
  MOZ_COUNT_CTOR(nsStyleVisibility);
}

nsChangeHint
nsStyleVisibility::CalcDifference(const nsStyleVisibility& aNewData) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (mDirection != aNewData.mDirection || mWritingMode != aNewData.mWritingMode) {
    // It's important that a change in mWritingMode results in frame
    // reconstruction, because it may affect intrinsic size (see
    // nsSubDocumentFrame::GetIntrinsicISize/BSize).
    hint |= nsChangeHint_ReconstructFrame;
  } else {
    if ((mImageOrientation != aNewData.mImageOrientation)) {
      hint |= nsChangeHint_AllReflowHints |
              nsChangeHint_RepaintFrame;
    }
    if (mVisible != aNewData.mVisible) {
      if ((NS_STYLE_VISIBILITY_COLLAPSE == mVisible) ||
          (NS_STYLE_VISIBILITY_COLLAPSE == aNewData.mVisible)) {
        hint |= NS_STYLE_HINT_REFLOW;
      } else {
        hint |= NS_STYLE_HINT_VISUAL;
      }
    }
    if (mTextOrientation != aNewData.mTextOrientation) {
      hint |= NS_STYLE_HINT_REFLOW;
    }
    if (mImageRendering != aNewData.mImageRendering) {
      hint |= nsChangeHint_RepaintFrame;
    }
    if (mColorAdjust != aNewData.mColorAdjust) {
      // color-adjust only affects media where dynamic changes can't happen.
      hint |= nsChangeHint_NeutralChange;
    }
  }
  return hint;
}

nsStyleContentData::~nsStyleContentData()
{
  MOZ_ASSERT(!mImageTracked,
             "nsStyleContentData being destroyed while still tracking image!");
  if (mType == eStyleContentType_Image) {
    NS_IF_RELEASE(mContent.mImage);
  } else if (mType == eStyleContentType_Counter ||
             mType == eStyleContentType_Counters) {
    mContent.mCounters->Release();
  } else if (mContent.mString) {
    free(mContent.mString);
  }
}

nsStyleContentData&
nsStyleContentData::operator=(const nsStyleContentData& aOther)
{
  if (this == &aOther) {
    return *this;
  }
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

bool
nsStyleContentData::operator==(const nsStyleContentData& aOther) const
{
  if (mType != aOther.mType) {
    return false;
  }
  if (mType == eStyleContentType_Image) {
    if (!mContent.mImage || !aOther.mContent.mImage) {
      return mContent.mImage == aOther.mContent.mImage;
    }
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
      mType == eStyleContentType_Counters) {
    return *mContent.mCounters == *aOther.mContent.mCounters;
  }
  return safe_strcmp(mContent.mString, aOther.mContent.mString) == 0;
}

void
nsStyleContentData::TrackImage(nsPresContext* aContext)
{
  // Sanity
  MOZ_ASSERT(!mImageTracked, "Already tracking image!");
  MOZ_ASSERT(mType == eStyleContentType_Image,
             "Trying to do image tracking on non-image!");
  MOZ_ASSERT(mContent.mImage,
             "Can't track image when there isn't one!");

  // Register the image with the document
  nsIDocument* doc = aContext->Document();
  if (doc) {
    doc->AddImage(mContent.mImage);
  }

  // Mark state
#ifdef DEBUG
  mImageTracked = true;
#endif
}

void
nsStyleContentData::UntrackImage(nsPresContext* aContext)
{
  // Sanity
  MOZ_ASSERT(mImageTracked, "Image not tracked!");
  MOZ_ASSERT(mType == eStyleContentType_Image,
             "Trying to do image tracking on non-image!");
  MOZ_ASSERT(mContent.mImage,
             "Can't untrack image when there isn't one!");

  // Unregister the image with the document
  nsIDocument* doc = aContext->Document();
  if (doc) {
    doc->RemoveImage(mContent.mImage);
  }

  // Mark state
#ifdef DEBUG
  mImageTracked = false;
#endif
}


//-----------------------
// nsStyleContent
//

nsStyleContent::nsStyleContent(StyleStructContext aContext)
  : mMarkerOffset(eStyleUnit_Auto)
  , mContents(nullptr)
  , mIncrements(nullptr)
  , mResets(nullptr)
  , mContentCount(0)
  , mIncrementCount(0)
  , mResetCount(0)
{
  MOZ_COUNT_CTOR(nsStyleContent);
}

nsStyleContent::~nsStyleContent()
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
    FreeByObjectID(eArenaObjectID_nsStyleContent, this);
}

nsStyleContent::nsStyleContent(const nsStyleContent& aSource)
  : mMarkerOffset(aSource.mMarkerOffset)
  , mContents(nullptr)
  , mIncrements(nullptr)
  , mResets(nullptr)
  , mContentCount(0)
  , mIncrementCount(0)
  , mResetCount(0)

{
  MOZ_COUNT_CTOR(nsStyleContent);

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

nsChangeHint
nsStyleContent::CalcDifference(const nsStyleContent& aNewData) const
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

  if (mContentCount != aNewData.mContentCount ||
      mIncrementCount != aNewData.mIncrementCount ||
      mResetCount != aNewData.mResetCount) {
    return nsChangeHint_ReconstructFrame;
  }

  uint32_t ix = mContentCount;
  while (0 < ix--) {
    if (mContents[ix] != aNewData.mContents[ix]) {
      // Unfortunately we need to reframe here; a simple reflow
      // will not pick up different text or different image URLs,
      // since we set all that up in the CSSFrameConstructor
      return nsChangeHint_ReconstructFrame;
    }
  }
  ix = mIncrementCount;
  while (0 < ix--) {
    if ((mIncrements[ix].mValue != aNewData.mIncrements[ix].mValue) ||
        (mIncrements[ix].mCounter != aNewData.mIncrements[ix].mCounter)) {
      return nsChangeHint_ReconstructFrame;
    }
  }
  ix = mResetCount;
  while (0 < ix--) {
    if ((mResets[ix].mValue != aNewData.mResets[ix].mValue) ||
        (mResets[ix].mCounter != aNewData.mResets[ix].mCounter)) {
      return nsChangeHint_ReconstructFrame;
    }
  }
  if (mMarkerOffset != aNewData.mMarkerOffset) {
    return NS_STYLE_HINT_REFLOW;
  }
  return nsChangeHint(0);
}

nsresult
nsStyleContent::AllocateContents(uint32_t aCount)
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


// --------------------
// nsStyleTextReset
//

nsStyleTextReset::nsStyleTextReset(StyleStructContext aContext)
  : mTextDecorationLine(NS_STYLE_TEXT_DECORATION_LINE_NONE)
  , mUnicodeBidi(NS_STYLE_UNICODE_BIDI_NORMAL)
  , mTextDecorationStyle(NS_STYLE_TEXT_DECORATION_STYLE_SOLID | BORDER_COLOR_FOREGROUND)
  , mTextDecorationColor(NS_RGB(0, 0, 0))
{
  MOZ_COUNT_CTOR(nsStyleTextReset);
}

nsStyleTextReset::nsStyleTextReset(const nsStyleTextReset& aSource)
{
  MOZ_COUNT_CTOR(nsStyleTextReset);
  *this = aSource;
}

nsStyleTextReset::~nsStyleTextReset()
{
  MOZ_COUNT_DTOR(nsStyleTextReset);
}

nsChangeHint
nsStyleTextReset::CalcDifference(const nsStyleTextReset& aNewData) const
{
  if (mUnicodeBidi != aNewData.mUnicodeBidi) {
    return NS_STYLE_HINT_REFLOW;
  }

  uint8_t lineStyle = GetDecorationStyle();
  uint8_t otherLineStyle = aNewData.GetDecorationStyle();
  if (mTextDecorationLine != aNewData.mTextDecorationLine ||
      lineStyle != otherLineStyle) {
    // Changes to our text-decoration line can impact our overflow area &
    // also our descendants' overflow areas (particularly for text-frame
    // descendants).  So, we update those areas & trigger a repaint.
    return nsChangeHint_RepaintFrame |
           nsChangeHint_UpdateSubtreeOverflow |
           nsChangeHint_SchedulePaint;
  }

  // Repaint for decoration color changes
  nscolor decColor, otherDecColor;
  bool isFG, otherIsFG;
  GetDecorationColor(decColor, isFG);
  aNewData.GetDecorationColor(otherDecColor, otherIsFG);
  if (isFG != otherIsFG || (!isFG && decColor != otherDecColor)) {
    return nsChangeHint_RepaintFrame;
  }

  if (mTextOverflow != aNewData.mTextOverflow) {
    return nsChangeHint_RepaintFrame;
  }

  return nsChangeHint(0);
}

// Returns true if the given shadow-arrays are equal.
static bool
AreShadowArraysEqual(nsCSSShadowArray* lhs,
                     nsCSSShadowArray* rhs)
{
  if (lhs == rhs) {
    return true;
  }

  if (!lhs || !rhs || lhs->Length() != rhs->Length()) {
    return false;
  }

  for (uint32_t i = 0; i < lhs->Length(); ++i) {
    if (*lhs->ShadowAt(i) != *rhs->ShadowAt(i)) {
      return false;
    }
  }
  return true;
}

// --------------------
// nsStyleText
//

nsStyleText::nsStyleText(StyleStructContext aContext)
  : mTextAlign(NS_STYLE_TEXT_ALIGN_START)
  , mTextAlignLast(NS_STYLE_TEXT_ALIGN_AUTO)
  , mTextAlignTrue(false)
  , mTextAlignLastTrue(false)
  , mTextEmphasisColorForeground(true)
  , mWebkitTextFillColorForeground(true)
  , mWebkitTextStrokeColorForeground(true)
  , mTextTransform(NS_STYLE_TEXT_TRANSFORM_NONE)
  , mWhiteSpace(NS_STYLE_WHITESPACE_NORMAL)
  , mWordBreak(NS_STYLE_WORDBREAK_NORMAL)
  , mOverflowWrap(NS_STYLE_OVERFLOWWRAP_NORMAL)
  , mHyphens(NS_STYLE_HYPHENS_MANUAL)
  , mRubyAlign(NS_STYLE_RUBY_ALIGN_SPACE_AROUND)
  , mRubyPosition(NS_STYLE_RUBY_POSITION_OVER)
  , mTextSizeAdjust(NS_STYLE_TEXT_SIZE_ADJUST_AUTO)
  , mTextCombineUpright(NS_STYLE_TEXT_COMBINE_UPRIGHT_NONE)
  , mControlCharacterVisibility(nsCSSParser::ControlCharVisibilityDefault())
  , mTextEmphasisStyle(NS_STYLE_TEXT_EMPHASIS_STYLE_NONE)
  , mTextRendering(NS_STYLE_TEXT_RENDERING_AUTO)
  , mTabSize(NS_STYLE_TABSIZE_INITIAL)
  , mTextEmphasisColor(aContext.DefaultColor())
  , mWebkitTextFillColor(aContext.DefaultColor())
  , mWebkitTextStrokeColor(aContext.DefaultColor())
  , mWordSpacing(0, nsStyleCoord::CoordConstructor)
  , mLetterSpacing(eStyleUnit_Normal)
  , mLineHeight(eStyleUnit_Normal)
  , mTextIndent(0, nsStyleCoord::CoordConstructor)
  , mWebkitTextStrokeWidth(0, nsStyleCoord::CoordConstructor)
  , mTextShadow(nullptr)
{
  MOZ_COUNT_CTOR(nsStyleText);
  nsCOMPtr<nsIAtom> language = aContext.GetContentLanguage();
  mTextEmphasisPosition = language &&
    nsStyleUtil::MatchesLanguagePrefix(language, MOZ_UTF16("zh")) ?
    NS_STYLE_TEXT_EMPHASIS_POSITION_DEFAULT_ZH :
    NS_STYLE_TEXT_EMPHASIS_POSITION_DEFAULT;
}

nsStyleText::nsStyleText(const nsStyleText& aSource)
  : mTextAlign(aSource.mTextAlign)
  , mTextAlignLast(aSource.mTextAlignLast)
  , mTextAlignTrue(false)
  , mTextAlignLastTrue(false)
  , mTextEmphasisColorForeground(aSource.mTextEmphasisColorForeground)
  , mWebkitTextFillColorForeground(aSource.mWebkitTextFillColorForeground)
  , mWebkitTextStrokeColorForeground(aSource.mWebkitTextStrokeColorForeground)
  , mTextTransform(aSource.mTextTransform)
  , mWhiteSpace(aSource.mWhiteSpace)
  , mWordBreak(aSource.mWordBreak)
  , mOverflowWrap(aSource.mOverflowWrap)
  , mHyphens(aSource.mHyphens)
  , mRubyAlign(aSource.mRubyAlign)
  , mRubyPosition(aSource.mRubyPosition)
  , mTextSizeAdjust(aSource.mTextSizeAdjust)
  , mTextCombineUpright(aSource.mTextCombineUpright)
  , mControlCharacterVisibility(aSource.mControlCharacterVisibility)
  , mTextEmphasisPosition(aSource.mTextEmphasisPosition)
  , mTextEmphasisStyle(aSource.mTextEmphasisStyle)
  , mTextRendering(aSource.mTextRendering)
  , mTabSize(aSource.mTabSize)
  , mTextEmphasisColor(aSource.mTextEmphasisColor)
  , mWebkitTextFillColor(aSource.mWebkitTextFillColor)
  , mWebkitTextStrokeColor(aSource.mWebkitTextStrokeColor)
  , mWordSpacing(aSource.mWordSpacing)
  , mLetterSpacing(aSource.mLetterSpacing)
  , mLineHeight(aSource.mLineHeight)
  , mTextIndent(aSource.mTextIndent)
  , mWebkitTextStrokeWidth(aSource.mWebkitTextStrokeWidth)
  , mTextShadow(aSource.mTextShadow)
  , mTextEmphasisStyleString(aSource.mTextEmphasisStyleString)
{
  MOZ_COUNT_CTOR(nsStyleText);
}

nsStyleText::~nsStyleText()
{
  MOZ_COUNT_DTOR(nsStyleText);
}

nsChangeHint
nsStyleText::CalcDifference(const nsStyleText& aNewData) const
{
  if (WhiteSpaceOrNewlineIsSignificant() !=
      aNewData.WhiteSpaceOrNewlineIsSignificant()) {
    // This may require construction of suppressed text frames
    return nsChangeHint_ReconstructFrame;
  }

  if (mTextCombineUpright != aNewData.mTextCombineUpright ||
      mControlCharacterVisibility != aNewData.mControlCharacterVisibility) {
    return nsChangeHint_ReconstructFrame;
  }

  if ((mTextAlign != aNewData.mTextAlign) ||
      (mTextAlignLast != aNewData.mTextAlignLast) ||
      (mTextAlignTrue != aNewData.mTextAlignTrue) ||
      (mTextAlignLastTrue != aNewData.mTextAlignLastTrue) ||
      (mTextTransform != aNewData.mTextTransform) ||
      (mWhiteSpace != aNewData.mWhiteSpace) ||
      (mWordBreak != aNewData.mWordBreak) ||
      (mOverflowWrap != aNewData.mOverflowWrap) ||
      (mHyphens != aNewData.mHyphens) ||
      (mRubyAlign != aNewData.mRubyAlign) ||
      (mRubyPosition != aNewData.mRubyPosition) ||
      (mTextSizeAdjust != aNewData.mTextSizeAdjust) ||
      (mLetterSpacing != aNewData.mLetterSpacing) ||
      (mLineHeight != aNewData.mLineHeight) ||
      (mTextIndent != aNewData.mTextIndent) ||
      (mWordSpacing != aNewData.mWordSpacing) ||
      (mTabSize != aNewData.mTabSize)) {
    return NS_STYLE_HINT_REFLOW;
  }

  if (HasTextEmphasis() != aNewData.HasTextEmphasis() ||
      (HasTextEmphasis() &&
       mTextEmphasisPosition != aNewData.mTextEmphasisPosition)) {
    // Text emphasis position change could affect line height calculation.
    return nsChangeHint_AllReflowHints |
           nsChangeHint_RepaintFrame;
  }

  nsChangeHint hint = nsChangeHint(0);

  // text-rendering changes require a reflow since they change SVG
  // frames' rects.
  if (mTextRendering != aNewData.mTextRendering) {
    hint |= nsChangeHint_NeedReflow |
            nsChangeHint_NeedDirtyReflow | // XXX remove me: bug 876085
            nsChangeHint_RepaintFrame;
  }

  if (!AreShadowArraysEqual(mTextShadow, aNewData.mTextShadow) ||
      mTextEmphasisStyle != aNewData.mTextEmphasisStyle ||
      mTextEmphasisStyleString != aNewData.mTextEmphasisStyleString ||
      mWebkitTextStrokeWidth != aNewData.mWebkitTextStrokeWidth) {
    hint |= nsChangeHint_UpdateSubtreeOverflow |
            nsChangeHint_SchedulePaint |
            nsChangeHint_RepaintFrame;

    // We don't add any other hints below.
    return hint;
  }

  MOZ_ASSERT(!mTextEmphasisColorForeground ||
             !aNewData.mTextEmphasisColorForeground ||
             mTextEmphasisColor == aNewData.mTextEmphasisColor,
             "If the text-emphasis-color are both foreground color, "
             "mTextEmphasisColor should also be identical");
  if (mTextEmphasisColorForeground != aNewData.mTextEmphasisColorForeground ||
      mTextEmphasisColor != aNewData.mTextEmphasisColor ||
      mWebkitTextFillColorForeground != aNewData.mWebkitTextFillColorForeground ||
      mWebkitTextFillColor != aNewData.mWebkitTextFillColor ||
      mWebkitTextStrokeColorForeground != aNewData.mWebkitTextStrokeColorForeground ||
      mWebkitTextStrokeColor != aNewData.mWebkitTextStrokeColor) {
    hint |= nsChangeHint_SchedulePaint |
            nsChangeHint_RepaintFrame;
  }

  if (hint) {
    return hint;
  }

  if (mTextEmphasisPosition != aNewData.mTextEmphasisPosition) {
    return nsChangeHint_NeutralChange;
  }

  return nsChangeHint(0);
}

LogicalSide
nsStyleText::TextEmphasisSide(WritingMode aWM) const
{
  MOZ_ASSERT(
    (!(mTextEmphasisPosition & NS_STYLE_TEXT_EMPHASIS_POSITION_LEFT) !=
     !(mTextEmphasisPosition & NS_STYLE_TEXT_EMPHASIS_POSITION_RIGHT)) &&
    (!(mTextEmphasisPosition & NS_STYLE_TEXT_EMPHASIS_POSITION_OVER) !=
     !(mTextEmphasisPosition & NS_STYLE_TEXT_EMPHASIS_POSITION_UNDER)));
  mozilla::Side side = aWM.IsVertical() ?
    (mTextEmphasisPosition & NS_STYLE_TEXT_EMPHASIS_POSITION_LEFT
     ? eSideLeft : eSideRight) :
    (mTextEmphasisPosition & NS_STYLE_TEXT_EMPHASIS_POSITION_OVER
     ? eSideTop : eSideBottom);
  LogicalSide result = aWM.LogicalSideForPhysicalSide(side);
  MOZ_ASSERT(IsBlock(result));
  return result;
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

nsStyleUserInterface::nsStyleUserInterface(StyleStructContext aContext)
  : mUserInput(NS_STYLE_USER_INPUT_AUTO)
  , mUserModify(NS_STYLE_USER_MODIFY_READ_ONLY)
  , mUserFocus(NS_STYLE_USER_FOCUS_NONE)
  , mPointerEvents(NS_STYLE_POINTER_EVENTS_AUTO)
  , mCursor(NS_STYLE_CURSOR_AUTO)
  , mCursorArrayLength(0)
  , mCursorArray(nullptr)
{
  MOZ_COUNT_CTOR(nsStyleUserInterface);
}

nsStyleUserInterface::nsStyleUserInterface(const nsStyleUserInterface& aSource)
  : mUserInput(aSource.mUserInput)
  , mUserModify(aSource.mUserModify)
  , mUserFocus(aSource.mUserFocus)
  , mPointerEvents(aSource.mPointerEvents)
  , mCursor(aSource.mCursor)
{
  MOZ_COUNT_CTOR(nsStyleUserInterface);
  CopyCursorArrayFrom(aSource);
}

nsStyleUserInterface::~nsStyleUserInterface()
{
  MOZ_COUNT_DTOR(nsStyleUserInterface);
  delete [] mCursorArray;
}

nsChangeHint
nsStyleUserInterface::CalcDifference(const nsStyleUserInterface& aNewData) const
{
  nsChangeHint hint = nsChangeHint(0);
  if (mCursor != aNewData.mCursor) {
    hint |= nsChangeHint_UpdateCursor;
  }

  // We could do better. But it wouldn't be worth it, URL-specified cursors are
  // rare.
  if (mCursorArrayLength > 0 || aNewData.mCursorArrayLength > 0) {
    hint |= nsChangeHint_UpdateCursor;
  }

  if (mPointerEvents != aNewData.mPointerEvents) {
    // nsSVGPathGeometryFrame's mRect depends on stroke _and_ on the value
    // of pointer-events. See nsSVGPathGeometryFrame::ReflowSVG's use of
    // GetHitTestFlags. (Only a reflow, no visual change.)
    hint |= nsChangeHint_NeedReflow |
            nsChangeHint_NeedDirtyReflow; // XXX remove me: bug 876085
  }

  if (mUserModify != aNewData.mUserModify) {
    hint |= NS_STYLE_HINT_VISUAL;
  }

  if (mUserInput != aNewData.mUserInput) {
    if (NS_STYLE_USER_INPUT_NONE == mUserInput ||
        NS_STYLE_USER_INPUT_NONE == aNewData.mUserInput) {
      hint |= nsChangeHint_ReconstructFrame;
    } else {
      hint |= nsChangeHint_NeutralChange;
    }
  }

  if (mUserFocus != aNewData.mUserFocus) {
    hint |= nsChangeHint_NeutralChange;
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
      for (uint32_t i = 0; i < mCursorArrayLength; ++i) {
        mCursorArray[i] = aSource.mCursorArray[i];
      }
    }
  }
}

//-----------------------
// nsStyleUIReset
//

nsStyleUIReset::nsStyleUIReset(StyleStructContext aContext)
  : mUserSelect(NS_STYLE_USER_SELECT_AUTO)
  , mForceBrokenImageIcon(0)
  , mIMEMode(NS_STYLE_IME_MODE_AUTO)
  , mWindowDragging(NS_STYLE_WINDOW_DRAGGING_DEFAULT)
  , mWindowShadow(NS_STYLE_WINDOW_SHADOW_DEFAULT)
{
  MOZ_COUNT_CTOR(nsStyleUIReset);
}

nsStyleUIReset::nsStyleUIReset(const nsStyleUIReset& aSource)
  : mUserSelect(aSource.mUserSelect)
  , mForceBrokenImageIcon(aSource.mForceBrokenImageIcon)
  , mIMEMode(aSource.mIMEMode)
  , mWindowDragging(aSource.mWindowDragging)
  , mWindowShadow(aSource.mWindowShadow)
{
  MOZ_COUNT_CTOR(nsStyleUIReset);
}

nsStyleUIReset::~nsStyleUIReset()
{
  MOZ_COUNT_DTOR(nsStyleUIReset);
}

nsChangeHint
nsStyleUIReset::CalcDifference(const nsStyleUIReset& aNewData) const
{
  // ignore mIMEMode
  if (mForceBrokenImageIcon != aNewData.mForceBrokenImageIcon) {
    return nsChangeHint_ReconstructFrame;
  }
  if (mWindowShadow != aNewData.mWindowShadow) {
    // We really need just an nsChangeHint_SyncFrameView, except
    // on an ancestor of the frame, so we get that by doing a
    // reflow.
    return NS_STYLE_HINT_REFLOW;
  }
  if (mUserSelect != aNewData.mUserSelect) {
    return NS_STYLE_HINT_VISUAL;
  }

  if (mWindowDragging != aNewData.mWindowDragging) {
    return nsChangeHint_SchedulePaint;
  }

  return nsChangeHint(0);
}

//-----------------------
// nsStyleVariables
//

nsStyleVariables::nsStyleVariables(StyleStructContext aContext)
{
  MOZ_COUNT_CTOR(nsStyleVariables);
}

nsStyleVariables::nsStyleVariables(const nsStyleVariables& aSource)
  : mVariables(aSource.mVariables)
{
  MOZ_COUNT_CTOR(nsStyleVariables);
}

nsStyleVariables::~nsStyleVariables()
{
  MOZ_COUNT_DTOR(nsStyleVariables);
}

nsChangeHint
nsStyleVariables::CalcDifference(const nsStyleVariables& aNewData) const
{
  return nsChangeHint(0);
}

//-----------------------
// nsStyleEffects
//

nsStyleEffects::nsStyleEffects(StyleStructContext aContext)
  : mBoxShadow(nullptr)
  , mClip(0, 0, 0, 0)
  , mOpacity(1.0f)
  , mClipFlags(NS_STYLE_CLIP_AUTO)
  , mMixBlendMode(NS_STYLE_BLEND_NORMAL)
{
  MOZ_COUNT_CTOR(nsStyleEffects);
}

nsStyleEffects::nsStyleEffects(const nsStyleEffects& aSource)
  : mFilters(aSource.mFilters)
  , mBoxShadow(aSource.mBoxShadow)
  , mClip(aSource.mClip)
  , mOpacity(aSource.mOpacity)
  , mClipFlags(aSource.mClipFlags)
  , mMixBlendMode(aSource.mMixBlendMode)
{
  MOZ_COUNT_CTOR(nsStyleEffects);
}

nsStyleEffects::~nsStyleEffects()
{
  MOZ_COUNT_DTOR(nsStyleEffects);
}

nsChangeHint
nsStyleEffects::CalcDifference(const nsStyleEffects& aNewData) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (!AreShadowArraysEqual(mBoxShadow, aNewData.mBoxShadow)) {
    // Update overflow regions & trigger DLBI to be sure it's noticed.
    // Also request a repaint, since it's possible that only the color
    // of the shadow is changing (and UpdateOverflow/SchedulePaint won't
    // repaint for that, since they won't know what needs invalidating.)
    hint |= nsChangeHint_UpdateOverflow |
            nsChangeHint_SchedulePaint |
            nsChangeHint_RepaintFrame;
  }

  if (mClipFlags != aNewData.mClipFlags) {
    hint |= nsChangeHint_AllReflowHints |
            nsChangeHint_RepaintFrame;
  }

  if (!mClip.IsEqualInterior(aNewData.mClip)) {
    // If the clip has changed, we just need to update overflow areas. DLBI
    // will handle the invalidation.
    hint |= nsChangeHint_UpdateOverflow |
            nsChangeHint_SchedulePaint;
  }

  if (mOpacity != aNewData.mOpacity) {
    // If we're going from the optimized >=0.99 opacity value to 1.0 or back, then
    // repaint the frame because DLBI will not catch the invalidation.  Otherwise,
    // just update the opacity layer.
    if ((mOpacity >= 0.99f && mOpacity < 1.0f && aNewData.mOpacity == 1.0f) ||
        (aNewData.mOpacity >= 0.99f && aNewData.mOpacity < 1.0f && mOpacity == 1.0f)) {
      hint |= nsChangeHint_RepaintFrame;
    } else {
      hint |= nsChangeHint_UpdateOpacityLayer;
      if ((mOpacity == 1.0f) != (aNewData.mOpacity == 1.0f)) {
        hint |= nsChangeHint_UpdateUsesOpacity;
      }
    }
  }

  if (HasFilters() != aNewData.HasFilters()) {
    // A change from/to being a containing block for position:fixed.
    hint |= nsChangeHint_UpdateContainingBlock;
  }

  if (mFilters != aNewData.mFilters) {
    hint |= nsChangeHint_UpdateEffects |
            nsChangeHint_RepaintFrame |
            nsChangeHint_UpdateOverflow;
  }

  if (mMixBlendMode != aNewData.mMixBlendMode) {
    hint |= nsChangeHint_RepaintFrame;
  }

  if (!hint &&
      !mClip.IsEqualEdges(aNewData.mClip)) {
    hint |= nsChangeHint_NeutralChange;
  }

  return hint;
}
