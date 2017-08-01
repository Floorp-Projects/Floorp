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
#include "nsDeviceContext.h"
#include "nsStyleUtil.h"

#include "nsCOMPtr.h"

#include "nsBidiUtils.h"
#include "nsLayoutUtils.h"

#include "imgIRequest.h"
#include "imgIContainer.h"
#include "CounterStyleManager.h"

#include "mozilla/dom/AnimationEffectReadOnlyBinding.h" // for PlaybackDirection
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/ImageTracker.h"
#include "mozilla/Likely.h"
#include "nsIURI.h"
#include "nsIDocument.h"
#include <algorithm>
#include "ImageLoader.h"

using namespace mozilla;
using namespace mozilla::dom;

static_assert((((1 << nsStyleStructID_Length) - 1) &
               ~(NS_STYLE_INHERIT_MASK)) == 0,
              "Not enough bits in NS_STYLE_INHERIT_MASK");

/* static */ const int32_t nsStyleGridLine::kMinLine;
/* static */ const int32_t nsStyleGridLine::kMaxLine;

static bool
DefinitelyEqualURIs(css::URLValueData* aURI1,
                    css::URLValueData* aURI2)
{
  return aURI1 == aURI2 ||
         (aURI1 && aURI2 && aURI1->DefinitelyEqualURIs(*aURI2));
}

static bool
DefinitelyEqualURIsAndPrincipal(css::URLValueData* aURI1,
                                css::URLValueData* aURI2)
{
  return aURI1 == aURI2 ||
         (aURI1 && aURI2 && aURI1->DefinitelyEqualURIsAndPrincipal(*aURI2));
}

static bool
DefinitelyEqualImages(nsStyleImageRequest* aRequest1,
                      nsStyleImageRequest* aRequest2)
{
  if (aRequest1 == aRequest2) {
    return true;
  }

  if (!aRequest1 || !aRequest2) {
    return false;
  }

  return aRequest1->DefinitelyEquals(*aRequest2);
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

static bool AreShadowArraysEqual(nsCSSShadowArray* lhs, nsCSSShadowArray* rhs);

// --------------------
// nsStyleFont
//
nsStyleFont::nsStyleFont(const nsFont& aFont, const nsPresContext* aContext)
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

nsStyleFont::nsStyleFont(const nsPresContext* aContext)
  : nsStyleFont(*aContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID,
                                          nullptr),
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
nsStyleFont::ZoomText(const nsPresContext* aPresContext, nscoord aSize)
{
  // aSize can be negative (e.g.: calc(-1px)) so we can't assert that here.
  // The caller is expected deal with that.
  return NSToCoordTruncClamped(float(aSize) * aPresContext->EffectiveTextZoom());
}

/* static */ nscoord
nsStyleFont::UnZoomText(nsPresContext *aPresContext, nscoord aSize)
{
  // aSize can be negative (e.g.: calc(-1px)) so we can't assert that here.
  // The caller is expected deal with that.
  return NSToCoordTruncClamped(float(aSize) / aPresContext->EffectiveTextZoom());
}

/* static */ already_AddRefed<nsIAtom>
nsStyleFont::GetLanguage(const nsPresContext* aPresContext)
{
  RefPtr<nsIAtom> language = aPresContext->GetContentLanguage();
  if (!language) {
    // we didn't find a (usable) Content-Language, so we fall back
    // to whatever the presContext guessed from the charset
    // NOTE this should not be used elsewhere, because we want websites
    // to use UTF-8 with proper language tag, instead of relying on
    // deriving language from charset. See bug 1040668 comment 67.
    language = aPresContext->GetLanguageFromCharset();
  }
  return language.forget();
}

nsStyleMargin::nsStyleMargin(const nsPresContext* aContext)
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

nsStylePadding::nsStylePadding(const nsPresContext* aContext)
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
  // FIXME: It would be good to return a weaker hint here that doesn't
  // force reflow of all descendants, but the hint would need to force
  // reflow of the frame's children (see how
  // ReflowInput::InitResizeFlags initializes the inline-resize flag).
  return NS_STYLE_HINT_REFLOW & ~nsChangeHint_ClearDescendantIntrinsics;
}

nsStyleBorder::nsStyleBorder(const nsPresContext* aContext)
  : mBorderColors(nullptr)
  , mBorderImageFill(NS_STYLE_BORDER_IMAGE_SLICE_NOFILL)
  , mBorderImageRepeatH(NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH)
  , mBorderImageRepeatV(NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH)
  , mFloatEdge(StyleFloatEdge::ContentBox)
  , mBoxDecorationBreak(StyleBoxDecorationBreak::Slice)
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
    mBorderStyle[side] = NS_STYLE_BORDER_STYLE_NONE;
    mBorderColor[side] = StyleComplexColor::CurrentColor();
  }

  mTwipsPerPixel = aContext->DevPixelsToAppUnits(1);
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
    NS_FOR_CSS_SIDES(side) {
      CopyBorderColorsFrom(aSrc.mBorderColors[side], side);
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

void
nsStyleBorder::FinishStyle(nsPresContext* aPresContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPresContext->StyleSet()->IsServo());

  mBorderImageSource.ResolveImage(aPresContext);
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
nsStyleBorder::Destroy(nsPresContext* aContext)
{
  this->~nsStyleBorder();
  aContext->PresShell()->
    FreeByObjectID(eArenaObjectID_nsStyleBorder, this);
}

nsChangeHint
nsStyleBorder::CalcDifference(const nsStyleBorder& aNewData) const
{
  // FIXME: XXXbz: As in nsStylePadding::CalcDifference, many of these
  // differences should not need to clear descendant intrinsics.
  // FIXME: It would be good to return a weaker hint for the
  // GetComputedBorder() differences (and perhaps others) that doesn't
  // force reflow of all descendants, but the hint would need to force
  // reflow of the frame's children (see how
  // ReflowInput::InitResizeFlags initializes the inline-resize flag).
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

  // Loading status of the border image can be accessed in main thread only
  // while CalcDifference might be executed on a background thread. As a
  // result, we have to check mBorderImage* fields even before border image was
  // actually loaded.
  if (!mBorderImageSource.IsEmpty() || !aNewData.mBorderImageSource.IsEmpty()) {
    if (mBorderImageSource  != aNewData.mBorderImageSource  ||
        mBorderImageRepeatH != aNewData.mBorderImageRepeatH ||
        mBorderImageRepeatV != aNewData.mBorderImageRepeatV ||
        mBorderImageSlice   != aNewData.mBorderImageSlice   ||
        mBorderImageFill    != aNewData.mBorderImageFill    ||
        mBorderImageWidth   != aNewData.mBorderImageWidth) {
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

  // mBorderImage* fields are checked only when border-image is not 'none'.
  if (mBorderImageSource  != aNewData.mBorderImageSource  ||
      mBorderImageRepeatH != aNewData.mBorderImageRepeatH ||
      mBorderImageRepeatV != aNewData.mBorderImageRepeatV ||
      mBorderImageSlice   != aNewData.mBorderImageSlice   ||
      mBorderImageFill    != aNewData.mBorderImageFill    ||
      mBorderImageWidth   != aNewData.mBorderImageWidth) {
    return nsChangeHint_NeutralChange;
  }

  return nsChangeHint(0);
}

nsStyleOutline::nsStyleOutline(const nsPresContext* aContext)
  : mOutlineWidth((StaticPresData::Get()
                     ->GetBorderWidthTable())[NS_STYLE_BORDER_WIDTH_MEDIUM])
  , mOutlineOffset(0)
  , mOutlineColor(StyleComplexColor::CurrentColor())
  , mOutlineStyle(NS_STYLE_BORDER_STYLE_NONE)
  , mActualOutlineWidth(0)
  , mTwipsPerPixel(aContext->DevPixelsToAppUnits(1))
{
  MOZ_COUNT_CTOR(nsStyleOutline);
  // spacing values not inherited
  nsStyleCoord zero(0, nsStyleCoord::CoordConstructor);
  NS_FOR_CSS_HALF_CORNERS(corner) {
    mOutlineRadius.Set(corner, zero);
  }
}

nsStyleOutline::nsStyleOutline(const nsStyleOutline& aSrc)
  : mOutlineRadius(aSrc.mOutlineRadius)
  , mOutlineWidth(aSrc.mOutlineWidth)
  , mOutlineOffset(aSrc.mOutlineOffset)
  , mOutlineColor(aSrc.mOutlineColor)
  , mOutlineStyle(aSrc.mOutlineStyle)
  , mActualOutlineWidth(aSrc.mActualOutlineWidth)
  , mTwipsPerPixel(aSrc.mTwipsPerPixel)
{
  MOZ_COUNT_CTOR(nsStyleOutline);
}

void
nsStyleOutline::RecalcData()
{
  if (NS_STYLE_BORDER_STYLE_NONE == mOutlineStyle) {
    mActualOutlineWidth = 0;
  } else {
    mActualOutlineWidth =
      NS_ROUND_BORDER_TO_PIXELS(mOutlineWidth, mTwipsPerPixel);
  }
}

nsChangeHint
nsStyleOutline::CalcDifference(const nsStyleOutline& aNewData) const
{
  if (mActualOutlineWidth != aNewData.mActualOutlineWidth ||
      (mActualOutlineWidth > 0 &&
       mOutlineOffset != aNewData.mOutlineOffset)) {
    return nsChangeHint_UpdateOverflow |
           nsChangeHint_SchedulePaint |
           nsChangeHint_RepaintFrame;
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
nsStyleList::nsStyleList(const nsPresContext* aContext)
  : mListStylePosition(NS_STYLE_LIST_STYLE_POSITION_OUTSIDE)
{
  MOZ_COUNT_CTOR(nsStyleList);
  mCounterStyle = CounterStyleManager::GetDiscStyle();
  SetQuotesInitial();
}

nsStyleList::~nsStyleList()
{
  MOZ_COUNT_DTOR(nsStyleList);
}

nsStyleList::nsStyleList(const nsStyleList& aSource)
  : mListStylePosition(aSource.mListStylePosition)
  , mListStyleImage(aSource.mListStyleImage)
  , mCounterStyle(aSource.mCounterStyle)
  , mQuotes(aSource.mQuotes)
  , mImageRegion(aSource.mImageRegion)
{
  MOZ_COUNT_CTOR(nsStyleList);
}

void
nsStyleList::FinishStyle(nsPresContext* aPresContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPresContext->StyleSet()->IsServo());

  if (mListStyleImage && !mListStyleImage->IsResolved()) {
    mListStyleImage->Resolve(aPresContext);
  }
  mCounterStyle.Resolve(aPresContext->CounterStyleManager());
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
nsStyleList::CalcDifference(const nsStyleList& aNewData,
                            const nsStyleDisplay* aOldDisplay) const
{
  // If the quotes implementation is ever going to change we might not need
  // a framechange here and a reflow should be sufficient.  See bug 35768.
  if (mQuotes != aNewData.mQuotes &&
      (mQuotes || aNewData.mQuotes) &&
      GetQuotePairs() != aNewData.GetQuotePairs()) {
    return nsChangeHint_ReconstructFrame;
  }
  nsChangeHint hint = nsChangeHint(0);
  // Only elements whose display value is list-item can be affected by
  // list-style-position and list-style-type. If the old display struct
  // doesn't exist, assume it isn't affected by display value at all,
  // and thus these properties should not affect it either. This also
  // relies on that when the display value changes from something else
  // to list-item, that change itself would cause ReconstructFrame.
  if (aOldDisplay && aOldDisplay->mDisplay == StyleDisplay::ListItem) {
    if (mListStylePosition != aNewData.mListStylePosition) {
      return nsChangeHint_ReconstructFrame;
    }
    if (mCounterStyle != aNewData.mCounterStyle) {
      return NS_STYLE_HINT_REFLOW;
    }
  } else if (mListStylePosition != aNewData.mListStylePosition ||
             mCounterStyle != aNewData.mCounterStyle) {
    hint = nsChangeHint_NeutralChange;
  }
  // list-style-image and -moz-image-region may affect some XUL elements
  // regardless of display value, so we still need to check them.
  if (!DefinitelyEqualImages(mListStyleImage, aNewData.mListStyleImage)) {
    return NS_STYLE_HINT_REFLOW;
  }
  if (!mImageRegion.IsEqualInterior(aNewData.mImageRegion)) {
    if (mImageRegion.width != aNewData.mImageRegion.width ||
        mImageRegion.height != aNewData.mImageRegion.height) {
      return NS_STYLE_HINT_REFLOW;
    }
    return NS_STYLE_HINT_VISUAL;
  }
  return hint;
}

already_AddRefed<nsIURI>
nsStyleList::GetListStyleImageURI() const
{
  if (!mListStyleImage) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri = mListStyleImage->GetImageURI();
  return uri.forget();
}

StaticRefPtr<nsStyleQuoteValues>
nsStyleList::sInitialQuotes;

StaticRefPtr<nsStyleQuoteValues>
nsStyleList::sNoneQuotes;


// --------------------
// nsStyleXUL
//
nsStyleXUL::nsStyleXUL(const nsPresContext* aContext)
  : mBoxFlex(0.0f)
  , mBoxOrdinal(1)
  , mBoxAlign(StyleBoxAlign::Stretch)
  , mBoxDirection(StyleBoxDirection::Normal)
  , mBoxOrient(StyleBoxOrient::Horizontal)
  , mBoxPack(StyleBoxPack::Start)
  , mStackSizing(StyleStackSizing::StretchToFit)
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
  , mStackSizing(aSource.mStackSizing)
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
      mStackSizing == aNewData.mStackSizing) {
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
/* static */ const uint32_t nsStyleColumn::kMaxColumnCount;

nsStyleColumn::nsStyleColumn(const nsPresContext* aContext)
  : mColumnCount(NS_STYLE_COLUMN_COUNT_AUTO)
  , mColumnWidth(eStyleUnit_Auto)
  , mColumnGap(eStyleUnit_Normal)
  , mColumnRuleColor(StyleComplexColor::CurrentColor())
  , mColumnRuleStyle(NS_STYLE_BORDER_STYLE_NONE)
  , mColumnFill(NS_STYLE_COLUMN_FILL_BALANCE)
  , mColumnSpan(NS_STYLE_COLUMN_SPAN_NONE)
  , mColumnRuleWidth((StaticPresData::Get()
                        ->GetBorderWidthTable())[NS_STYLE_BORDER_WIDTH_MEDIUM])
  , mTwipsPerPixel(aContext->AppUnitsPerDevPixel())
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
  , mColumnSpan(aSource.mColumnSpan)
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
      mColumnCount != aNewData.mColumnCount ||
      mColumnSpan != aNewData.mColumnSpan) {
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
      mColumnRuleColor != aNewData.mColumnRuleColor) {
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
nsStyleSVG::nsStyleSVG(const nsPresContext* aContext)
  : mFill(eStyleSVGPaintType_Color) // Will be initialized to NS_RGB(0, 0, 0)
  , mStroke(eStyleSVGPaintType_None)
  , mStrokeDashoffset(0, nsStyleCoord::CoordConstructor)
  , mStrokeWidth(nsPresContext::CSSPixelsToAppUnits(1), nsStyleCoord::CoordConstructor)
  , mFillOpacity(1.0f)
  , mStrokeMiterlimit(4.0f)
  , mStrokeOpacity(1.0f)
  , mClipRule(StyleFillRule::Nonzero)
  , mColorInterpolation(NS_STYLE_COLOR_INTERPOLATION_SRGB)
  , mColorInterpolationFilters(NS_STYLE_COLOR_INTERPOLATION_LINEARRGB)
  , mFillRule(StyleFillRule::Nonzero)
  , mPaintOrder(NS_STYLE_PAINT_ORDER_NORMAL)
  , mShapeRendering(NS_STYLE_SHAPE_RENDERING_AUTO)
  , mStrokeLinecap(NS_STYLE_STROKE_LINECAP_BUTT)
  , mStrokeLinejoin(NS_STYLE_STROKE_LINEJOIN_MITER)
  , mTextAnchor(NS_STYLE_TEXT_ANCHOR_START)
  , mContextPropsBits(0)
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
  , mContextProps(aSource.mContextProps)
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
  , mContextPropsBits(aSource.mContextPropsBits)
  , mContextFlags(aSource.mContextFlags)
{
  MOZ_COUNT_CTOR(nsStyleSVG);
}

static bool
PaintURIChanged(const nsStyleSVGPaint& aPaint1, const nsStyleSVGPaint& aPaint2)
{
  if (aPaint1.Type() != aPaint2.Type()) {
    return aPaint1.Type() == eStyleSVGPaintType_Server ||
           aPaint2.Type() == eStyleSVGPaintType_Server;
  }
  return aPaint1.Type() == eStyleSVGPaintType_Server &&
         !DefinitelyEqualURIs(aPaint1.GetPaintServer(),
                              aPaint2.GetPaintServer());
}

nsChangeHint
nsStyleSVG::CalcDifference(const nsStyleSVG& aNewData) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (!DefinitelyEqualURIs(mMarkerEnd, aNewData.mMarkerEnd) ||
      !DefinitelyEqualURIs(mMarkerMid, aNewData.mMarkerMid) ||
      !DefinitelyEqualURIs(mMarkerStart, aNewData.mMarkerStart)) {
    // Markers currently contribute to SVGGeometryFrame::mRect,
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
      // XXXperf this is a waste on non SVGGeometryFrames.
      hint |= nsChangeHint_NeedReflow |
              nsChangeHint_NeedDirtyReflow; // XXX remove me: bug 876085
    }
    if (PaintURIChanged(mFill, aNewData.mFill) ||
        PaintURIChanged(mStroke, aNewData.mStroke)) {
      hint |= nsChangeHint_UpdateEffects;
    }
  }

  // Stroke currently contributes to SVGGeometryFrame::mRect, so
  // we need a reflow here. No intrinsic sizes need to change, so
  // nsChangeHint_NeedReflow is sufficient.
  // Note that stroke-dashoffset does not affect SVGGeometryFrame::mRect.
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
       mContextFlags          != aNewData.mContextFlags          ||
       mContextPropsBits      != aNewData.mContextPropsBits) {
    return hint | nsChangeHint_RepaintFrame;
  }

  if (!hint) {
    if (mContextProps != aNewData.mContextProps) {
      hint = nsChangeHint_NeutralChange;
    }
  }

  return hint;
}

// --------------------
// StyleBasicShape

nsCSSKeyword
StyleBasicShape::GetShapeTypeName() const
{
  switch (mType) {
    case StyleBasicShapeType::Polygon:
      return eCSSKeyword_polygon;
    case StyleBasicShapeType::Circle:
      return eCSSKeyword_circle;
    case StyleBasicShapeType::Ellipse:
      return eCSSKeyword_ellipse;
    case StyleBasicShapeType::Inset:
      return eCSSKeyword_inset;
  }
  NS_NOTREACHED("unexpected type");
  return eCSSKeyword_UNKNOWN;
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
    return DefinitelyEqualURIs(mURL, aOther.mURL);
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

bool
nsStyleFilter::SetURL(css::URLValue* aURL)
{
  ReleaseRef();
  mURL = aURL;
  mURL->AddRef();
  mType = NS_STYLE_FILTER_URL;
  return true;
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
nsStyleSVGReset::nsStyleSVGReset(const nsPresContext* aContext)
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
  this->~nsStyleSVGReset();
  aContext->PresShell()->
    FreeByObjectID(mozilla::eArenaObjectID_nsStyleSVGReset, this);
}

void
nsStyleSVGReset::FinishStyle(nsPresContext* aPresContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPresContext->StyleSet()->IsServo());

  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, mMask) {
    nsStyleImage& image = mMask.mLayers[i].mImage;
    if (image.GetType() == eStyleImageType_Image) {
      // If the url of mask resource contains a reference('#'), it should be a
      // <mask-source>, mostly. For a <mask-source>, there is no need to
      // resolve this style image, since we do not depend on it to get the
      // SVG mask resource.
      if (!image.GetURLValue()->HasRef()) {
        image.ResolveImage(aPresContext);
      }
    }
  }
}

nsChangeHint
nsStyleSVGReset::CalcDifference(const nsStyleSVGReset& aNewData) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (!mClipPath.DefinitelyEquals(aNewData.mClipPath)) {
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
    // Stroke currently affects SVGGeometryFrame::mRect, and
    // vector-effect affect stroke. As a result we need to reflow if
    // vector-effect changes in order to have SVGGeometryFrame::
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

  if (HasMask() != aNewData.HasMask()) {
    // A change from/to being a containing block for position:fixed.
    hint |= nsChangeHint_UpdateContainingBlock;
  }

  hint |= mMask.CalcDifference(aNewData.mMask,
                               nsStyleImageLayers::LayerType::Mask);

  return hint;
}

bool
nsStyleSVGReset::HasMask() const
{
  for (uint32_t i = 0; i < mMask.mImageCount; i++) {
    if (!mMask.mLayers[i].mImage.IsEmpty()) {
      return true;
    }
  }

  return false;
}

// nsStyleSVGPaint implementation
nsStyleSVGPaint::nsStyleSVGPaint(nsStyleSVGPaintType aType)
  : mType(aType)
  , mFallbackType(eStyleSVGFallbackType_NotSet)
  , mFallbackColor(NS_RGB(0, 0, 0))
{
  MOZ_ASSERT(aType == nsStyleSVGPaintType(0) ||
             aType == eStyleSVGPaintType_None ||
             aType == eStyleSVGPaintType_Color);
  mPaint.mColor = NS_RGB(0, 0, 0);
}

nsStyleSVGPaint::nsStyleSVGPaint(const nsStyleSVGPaint& aSource)
  : nsStyleSVGPaint(nsStyleSVGPaintType(0))
{
  Assign(aSource);
}

nsStyleSVGPaint::~nsStyleSVGPaint()
{
  Reset();
}

void
nsStyleSVGPaint::Reset()
{
  switch (mType) {
    case eStyleSVGPaintType_None:
      break;
    case eStyleSVGPaintType_Color:
      mPaint.mColor = NS_RGB(0, 0, 0);
      break;
    case eStyleSVGPaintType_Server:
      mPaint.mPaintServer->Release();
      mPaint.mPaintServer = nullptr;
      MOZ_FALLTHROUGH;
    case eStyleSVGPaintType_ContextFill:
    case eStyleSVGPaintType_ContextStroke:
      mFallbackType = eStyleSVGFallbackType_NotSet;
      mFallbackColor = NS_RGB(0, 0, 0);
      break;
  }
  mType = nsStyleSVGPaintType(0);
}

nsStyleSVGPaint&
nsStyleSVGPaint::operator=(const nsStyleSVGPaint& aOther)
{
  if (this != &aOther) {
    Assign(aOther);
  }
  return *this;
}

void
nsStyleSVGPaint::Assign(const nsStyleSVGPaint& aOther)
{
  MOZ_ASSERT(aOther.mType != nsStyleSVGPaintType(0),
             "shouldn't copy uninitialized nsStyleSVGPaint");

  switch (aOther.mType) {
    case eStyleSVGPaintType_None:
      SetNone();
      break;
    case eStyleSVGPaintType_Color:
      SetColor(aOther.mPaint.mColor);
      break;
    case eStyleSVGPaintType_Server:
      SetPaintServer(aOther.mPaint.mPaintServer,
                     aOther.mFallbackType,
                     aOther.mFallbackColor);
      break;
    case eStyleSVGPaintType_ContextFill:
    case eStyleSVGPaintType_ContextStroke:
      SetContextValue(aOther.mType,
                      aOther.mFallbackType,
                      aOther.mFallbackColor);
      break;
  }
}

void
nsStyleSVGPaint::SetNone()
{
  Reset();
  mType = eStyleSVGPaintType_None;
}

void
nsStyleSVGPaint::SetContextValue(nsStyleSVGPaintType aType,
                                 nsStyleSVGFallbackType aFallbackType,
                                 nscolor aFallbackColor)
{
  MOZ_ASSERT(aType == eStyleSVGPaintType_ContextFill ||
             aType == eStyleSVGPaintType_ContextStroke);
  Reset();
  mType = aType;
  mFallbackType = aFallbackType;
  mFallbackColor = aFallbackColor;
}

void
nsStyleSVGPaint::SetColor(nscolor aColor)
{
  Reset();
  mType = eStyleSVGPaintType_Color;
  mPaint.mColor = aColor;
}

void
nsStyleSVGPaint::SetPaintServer(css::URLValue* aPaintServer,
                                nsStyleSVGFallbackType aFallbackType,
                                nscolor aFallbackColor)
{
  MOZ_ASSERT(aPaintServer);
  Reset();
  mType = eStyleSVGPaintType_Server;
  mPaint.mPaintServer = aPaintServer;
  mPaint.mPaintServer->AddRef();
  mFallbackType = aFallbackType;
  mFallbackColor = aFallbackColor;
}

bool nsStyleSVGPaint::operator==(const nsStyleSVGPaint& aOther) const
{
  if (mType != aOther.mType) {
    return false;
  }
  switch (mType) {
    case eStyleSVGPaintType_Color:
      return mPaint.mColor == aOther.mPaint.mColor;
    case eStyleSVGPaintType_Server:
      return DefinitelyEqualURIs(mPaint.mPaintServer,
                                 aOther.mPaint.mPaintServer) &&
             mFallbackType == aOther.mFallbackType &&
             mFallbackColor == aOther.mFallbackColor;
    case eStyleSVGPaintType_ContextFill:
    case eStyleSVGPaintType_ContextStroke:
      return mFallbackType == aOther.mFallbackType &&
             mFallbackColor == aOther.mFallbackColor;
    default:
      MOZ_ASSERT(mType == eStyleSVGPaintType_None,
                 "Unexpected SVG paint type");
      return true;
  }
}

// --------------------
// nsStylePosition
//
nsStylePosition::nsStylePosition(const nsPresContext* aContext)
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
  , mSpecifiedJustifyItems(NS_STYLE_JUSTIFY_AUTO)
  , mJustifyItems(NS_STYLE_JUSTIFY_NORMAL)
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
  , mSpecifiedJustifyItems(aSource.mSpecifiedJustifyItems)
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

  // No need to do anything if mSpecifiedJustifyItems changes, as long as
  // mJustifyItems (tested above) is unchanged.
  if (mSpecifiedJustifyItems != aNewData.mSpecifiedJustifyItems) {
    hint |= nsChangeHint_NeutralChange;
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
      hint |= nsChangeHint_ReflowHintsForBSizeChange;
    }

    if (isVertical ? heightChanged : widthChanged) {
      hint |= nsChangeHint_ReflowHintsForISizeChange;
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
nsStylePosition::UsedAlignSelf(nsStyleContext* aParent) const
{
  if (mAlignSelf != NS_STYLE_ALIGN_AUTO) {
    return mAlignSelf;
  }
  if (MOZ_LIKELY(aParent)) {
    auto parentAlignItems = aParent->StylePosition()->mAlignItems;
    MOZ_ASSERT(!(parentAlignItems & NS_STYLE_ALIGN_LEGACY),
               "align-items can't have 'legacy'");
    return parentAlignItems;
  }
  return NS_STYLE_ALIGN_NORMAL;
}

uint8_t
nsStylePosition::UsedJustifySelf(nsStyleContext* aParent) const
{
  if (mJustifySelf != NS_STYLE_JUSTIFY_AUTO) {
    return mJustifySelf;
  }
  if (MOZ_LIKELY(aParent)) {
    auto inheritedJustifyItems = aParent->StylePosition()->mJustifyItems;
    return inheritedJustifyItems & ~NS_STYLE_JUSTIFY_LEGACY;
  }
  return NS_STYLE_JUSTIFY_NORMAL;
}

// --------------------
// nsStyleTable
//

nsStyleTable::nsStyleTable(const nsPresContext* aContext)
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

nsStyleTableBorder::nsStyleTableBorder(const nsPresContext* aContext)
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

nsStyleColor::nsStyleColor(const nsPresContext* aContext)
  : mColor(aContext->DefaultColor())
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
      mMozLegacySyntax != aOther.mMozLegacySyntax ||
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
  , mMozLegacySyntax(false)
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
// nsStyleImageRequest

/**
 * Runnable to release the nsStyleImageRequest's mRequestProxy,
 * mImageValue and mImageTracker on the main thread, and to perform
 * any necessary unlocking and untracking of the image.
 */
class StyleImageRequestCleanupTask : public mozilla::Runnable
{
public:
  typedef nsStyleImageRequest::Mode Mode;

  StyleImageRequestCleanupTask(Mode aModeFlags,
                               already_AddRefed<imgRequestProxy> aRequestProxy,
                               already_AddRefed<css::ImageValue> aImageValue,
                               already_AddRefed<ImageTracker> aImageTracker)
    : mozilla::Runnable("StyleImageRequestCleanupTask")
    , mModeFlags(aModeFlags)
    , mRequestProxy(aRequestProxy)
    , mImageValue(aImageValue)
    , mImageTracker(aImageTracker)
  {
  }

  NS_IMETHOD Run() final
  {
    MOZ_ASSERT(!mRequestProxy || NS_IsMainThread(),
               "If mRequestProxy is non-null, we need to run on main thread!");

    if (!mRequestProxy) {
      return NS_OK;
    }

    if (mModeFlags & Mode::Track) {
      MOZ_ASSERT(mImageTracker);
      mImageTracker->Remove(mRequestProxy);
    } else {
      mRequestProxy->UnlockImage();
    }

    if (mModeFlags & Mode::Discard) {
      mRequestProxy->RequestDiscard();
    }

    return NS_OK;
  }

protected:
  virtual ~StyleImageRequestCleanupTask()
  {
    MOZ_ASSERT(mImageValue->mRequests.Count() == 0 || NS_IsMainThread(),
               "If mImageValue has any mRequests, we need to run on main "
               "thread to release ImageValues!");
    MOZ_ASSERT((!mRequestProxy && !mImageTracker) || NS_IsMainThread(),
               "mRequestProxy and mImageTracker's destructor need to run "
               "on the main thread!");
  }

private:
  Mode mModeFlags;
  // Since we always dispatch this runnable to the main thread, these will be
  // released on the main thread when the runnable itself is released.
  RefPtr<imgRequestProxy> mRequestProxy;
  RefPtr<css::ImageValue> mImageValue;
  RefPtr<ImageTracker> mImageTracker;
};

nsStyleImageRequest::nsStyleImageRequest(Mode aModeFlags,
                                         imgRequestProxy* aRequestProxy,
                                         css::ImageValue* aImageValue,
                                         ImageTracker* aImageTracker)
  : mRequestProxy(aRequestProxy)
  , mImageValue(aImageValue)
  , mImageTracker(aImageTracker)
  , mModeFlags(aModeFlags)
  , mResolved(true)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aImageValue);
  MOZ_ASSERT(!!(aModeFlags & Mode::Track) == !!aImageTracker);

  if (mRequestProxy) {
    MaybeTrackAndLock();
  }
}

nsStyleImageRequest::nsStyleImageRequest(
    Mode aModeFlags,
    mozilla::css::ImageValue* aImageValue)
  : mImageValue(aImageValue)
  , mModeFlags(aModeFlags)
  , mResolved(false)
{
}

nsStyleImageRequest::~nsStyleImageRequest()
{
  // We may or may not be being destroyed on the main thread.  To clean
  // up, we must untrack and unlock the image (depending on mModeFlags),
  // and release mRequestProxy and mImageValue, all on the main thread.
  {
    RefPtr<StyleImageRequestCleanupTask> task =
        new StyleImageRequestCleanupTask(mModeFlags,
                                         mRequestProxy.forget(),
                                         mImageValue.forget(),
                                         mImageTracker.forget());
    if (NS_IsMainThread()) {
      task->Run();
    } else {
      if (mDocGroup) {
        mDocGroup->Dispatch(TaskCategory::Other, task.forget());
      } else {
        // if Resolve was not called at some point, mDocGroup is not set.
        NS_DispatchToMainThread(task.forget());
      }
    }
  }

  MOZ_ASSERT(!mRequestProxy);
  MOZ_ASSERT(!mImageValue);
  MOZ_ASSERT(!mImageTracker);
}

bool
nsStyleImageRequest::Resolve(nsPresContext* aPresContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!IsResolved(), "already resolved");
  MOZ_ASSERT(aPresContext);

  mResolved = true;

  nsIDocument* doc = aPresContext->Document();
  nsIURI* docURI = doc->GetDocumentURI();
  if (GetImageValue()->HasRef()) {
    bool isEqualExceptRef = false;
    RefPtr<nsIURI> imageURI = GetImageURI();
    imageURI->EqualsExceptRef(docURI, &isEqualExceptRef);
    if (isEqualExceptRef) {
      // Prevent loading an internal resource.
      return true;
    }
  }

  mDocGroup = doc->GetDocGroup();

  mImageValue->Initialize(doc);

  nsCSSValue value;
  value.SetImageValue(mImageValue);
  mRequestProxy = value.GetPossiblyStaticImageValue(aPresContext->Document(),
                                                    aPresContext);

  if (!mRequestProxy) {
    // The URL resolution or image load failed.
    return false;
  }

  if (mModeFlags & Mode::Track) {
    mImageTracker = aPresContext->Document()->ImageTracker();
  }

  MaybeTrackAndLock();
  return true;
}

void
nsStyleImageRequest::MaybeTrackAndLock()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsResolved());
  MOZ_ASSERT(mRequestProxy);

  if (mModeFlags & Mode::Track) {
    MOZ_ASSERT(mImageTracker);
    mImageTracker->Add(mRequestProxy);
  } else {
    MOZ_ASSERT(!mImageTracker);
    mRequestProxy->LockImage();
  }
}

bool
nsStyleImageRequest::DefinitelyEquals(const nsStyleImageRequest& aOther) const
{
  return DefinitelyEqualURIs(mImageValue, aOther.mImageValue);
}

// --------------------
// CachedBorderImageData
//
void
CachedBorderImageData::SetCachedSVGViewportSize(
  const mozilla::Maybe<nsSize>& aSVGViewportSize)
{
  mCachedSVGViewportSize = aSVGViewportSize;
}

const mozilla::Maybe<nsSize>&
CachedBorderImageData::GetCachedSVGViewportSize()
{
  return mCachedSVGViewportSize;
}

struct PurgeCachedImagesTask : mozilla::Runnable
{
  PurgeCachedImagesTask() : mozilla::Runnable("PurgeCachedImagesTask") {}
  NS_IMETHOD Run() final
  {
    mSubImages.Clear();
    return NS_OK;
  }

  nsCOMArray<imgIContainer> mSubImages;
};

void
CachedBorderImageData::PurgeCachedImages()
{
  if (ServoStyleSet::IsInServoTraversal()) {
    RefPtr<PurgeCachedImagesTask> task = new PurgeCachedImagesTask();
    task->mSubImages.SwapElements(mSubImages);
    // This will run the task immediately if we're already on the main thread,
    // but that is fine.
    NS_DispatchToMainThread(task.forget());
  } else {
    mSubImages.Clear();
  }
}

void
CachedBorderImageData::SetSubImage(uint8_t aIndex, imgIContainer* aSubImage)
{
  mSubImages.ReplaceObjectAt(aSubImage, aIndex);
}

imgIContainer*
CachedBorderImageData::GetSubImage(uint8_t aIndex)
{
  imgIContainer* subImage = nullptr;
  if (aIndex < mSubImages.Count())
    subImage = mSubImages[aIndex];
  return subImage;
}

// --------------------
// nsStyleImage
//

nsStyleImage::nsStyleImage()
  : mType(eStyleImageType_Null)
  , mCropRect(nullptr)
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
    SetImageRequest(do_AddRef(aOther.mImage));
  } else if (aOther.mType == eStyleImageType_Gradient) {
    SetGradientData(aOther.mGradient);
  } else if (aOther.mType == eStyleImageType_Element) {
    SetElementId(do_AddRef(aOther.mElementId));
  } else if (aOther.mType == eStyleImageType_URL) {
    SetURLValue(do_AddRef(aOther.mURLValue));
  }

  UniquePtr<nsStyleSides> cropRectCopy;
  if (aOther.mCropRect) {
    cropRectCopy = MakeUnique<nsStyleSides>(*aOther.mCropRect.get());
  }
  SetCropRect(Move(cropRectCopy));
}

void
nsStyleImage::SetNull()
{
  if (mType == eStyleImageType_Gradient) {
    mGradient->Release();
  } else if (mType == eStyleImageType_Image) {
    NS_RELEASE(mImage);
  } else if (mType == eStyleImageType_Element) {
    NS_RELEASE(mElementId);
  } else if (mType == eStyleImageType_URL) {
    NS_RELEASE(mURLValue);
  }

  mType = eStyleImageType_Null;
  mCropRect = nullptr;
}

void
nsStyleImage::SetImageRequest(already_AddRefed<nsStyleImageRequest> aImage)
{
  RefPtr<nsStyleImageRequest> image = aImage;

  if (mType != eStyleImageType_Null) {
    SetNull();
  }

  if (image) {
    mImage = image.forget().take();
    mType = eStyleImageType_Image;
  }
  if (mCachedBIData) {
    mCachedBIData->PurgeCachedImages();
  }
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
nsStyleImage::SetElementId(already_AddRefed<nsIAtom> aElementId)
{
  if (mType != eStyleImageType_Null) {
    SetNull();
  }

  if (nsCOMPtr<nsIAtom> atom = aElementId) {
    mElementId = atom.forget().take();
    mType = eStyleImageType_Element;
  }
}

void
nsStyleImage::SetCropRect(UniquePtr<nsStyleSides> aCropRect)
{
    mCropRect = Move(aCropRect);
}

void
nsStyleImage::SetURLValue(already_AddRefed<URLValue> aValue)
{
  RefPtr<URLValue> value = aValue;

  if (mType != eStyleImageType_Null) {
    SetNull();
  }

  if (value) {
    mURLValue = value.forget().take();
    mType = eStyleImageType_URL;
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

already_AddRefed<nsIURI>
nsStyleImageRequest::GetImageURI() const
{
  nsCOMPtr<nsIURI> uri;

  if (mRequestProxy) {
    mRequestProxy->GetURI(getter_AddRefs(uri));
    if (uri) {
      return uri.forget();
    }
  }

  // If we had some problem resolving the mRequestProxy, use the URL stored
  // in the mImageValue.
  if (!mImageValue) {
    return nullptr;
  }

  uri = mImageValue->GetURI();
  return uri.forget();
}

bool
nsStyleImage::ComputeActualCropRect(nsIntRect& aActualCropRect,
                                    bool* aIsEntireImage) const
{
  MOZ_ASSERT(mType == eStyleImageType_Image,
             "This function is designed to be used only when mType"
             "is eStyleImageType_Image.");

  imgRequestProxy* req = GetImageData();
  if (!req) {
    return false;
  }

  nsCOMPtr<imgIContainer> imageContainer;
  req->GetImage(getter_AddRefs(imageContainer));
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

bool
nsStyleImage::StartDecoding() const
{
  if (mType == eStyleImageType_Image) {
    imgRequestProxy* req = GetImageData();
    if (!req) {
      return false;
    }
    return req->StartDecodingWithResult(imgIContainer::FLAG_ASYNC_NOTIFY);
  }
  // null image types always return false from IsComplete, so we do the same here.
  return mType != eStyleImageType_Null ? true : false;
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

  if (mType == eStyleImageType_Element || mType == eStyleImageType_URL) {
    return false;
  }

  MOZ_ASSERT(mType == eStyleImageType_Image, "unexpected image type");
  MOZ_ASSERT(GetImageData(), "should've returned earlier above");

  nsCOMPtr<imgIContainer> imageContainer;
  GetImageData()->GetImage(getter_AddRefs(imageContainer));
  MOZ_ASSERT(imageContainer, "IsComplete() said image container is ready");

  // Check if the crop region of the image is opaque.
  if (imageContainer->WillDrawOpaqueNow()) {
    if (!mCropRect) {
      return true;
    }

    // Must make sure if mCropRect contains at least a pixel.
    // XXX Is this optimization worth it? Maybe I should just return false.
    nsIntRect actualCropRect;
    return ComputeActualCropRect(actualCropRect) && !actualCropRect.IsEmpty();
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
    case eStyleImageType_URL:
      return true;
    case eStyleImageType_Image: {
      if (!IsResolved()) {
        return false;
      }
      imgRequestProxy* req = GetImageData();
      if (!req) {
        return false;
      }
      uint32_t status = imgIRequest::STATUS_ERROR;
      return NS_SUCCEEDED(req->GetImageStatus(&status)) &&
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
    case eStyleImageType_URL:
      return true;
    case eStyleImageType_Image: {
      imgRequestProxy* req = GetImageData();
      if (!req) {
        return false;
      }
      uint32_t status = imgIRequest::STATUS_ERROR;
      return NS_SUCCEEDED(req->GetImageStatus(&status)) &&
             !(status & imgIRequest::STATUS_ERROR) &&
             (status & imgIRequest::STATUS_LOAD_COMPLETE);
    }
    default:
      NS_NOTREACHED("unexpected image type");
      return false;
  }
}

static inline bool
EqualRects(const UniquePtr<nsStyleSides>& aRect1, const UniquePtr<nsStyleSides>& aRect2)
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
    return DefinitelyEqualImages(mImage, aOther.mImage);
  }

  if (mType == eStyleImageType_Gradient) {
    return *mGradient == *aOther.mGradient;
  }

  if (mType == eStyleImageType_Element) {
    return mElementId == aOther.mElementId;
  }

  if (mType == eStyleImageType_URL) {
    return DefinitelyEqualURIs(mURLValue, aOther.mURLValue);
  }

  return true;
}

void
nsStyleImage::PurgeCacheForViewportChange(
  const mozilla::Maybe<nsSize>& aSVGViewportSize,
  const bool aHasIntrinsicRatio) const
{
  EnsureCachedBIData();

  // If we're redrawing with a different viewport-size than we used for our
  // cached subimages, then we can't trust that our subimages are valid;
  // any percent sizes/positions in our SVG doc may be different now. Purge!
  // (We don't have to purge if the SVG document has an intrinsic ratio,
  // though, because the actual size of elements in SVG documant's coordinate
  // axis are fixed in this case.)
  if (aSVGViewportSize != mCachedBIData->GetCachedSVGViewportSize() &&
      !aHasIntrinsicRatio) {
    mCachedBIData->PurgeCachedImages();
    mCachedBIData->SetCachedSVGViewportSize(aSVGViewportSize);
  }
}

already_AddRefed<nsIURI>
nsStyleImage::GetImageURI() const
{
  if (mType != eStyleImageType_Image) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri = mImage->GetImageURI();
  return uri.forget();
}

css::URLValueData*
nsStyleImage::GetURLValue() const
{
  if (mType == eStyleImageType_Image) {
    return mImage->GetImageValue();
  } else if (mType == eStyleImageType_URL) {
    return mURLValue;
  }

  return nullptr;
}

// --------------------
// nsStyleImageLayers
//

const nsCSSPropertyID nsStyleImageLayers::kBackgroundLayerTable[] = {
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
const nsCSSPropertyID nsStyleImageLayers::kMaskLayerTable[] = {
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
                                   nsStyleImageLayers::LayerType aType) const
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
        moreLayers.mLayers[i].CalcDifference(lessLayers.mLayers[i]);
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

  if (aType == nsStyleImageLayers::LayerType::Mask &&
      mImageCount != aNewLayers.mImageCount) {
    hint |= nsChangeHint_UpdateEffects;
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

nsStyleImageLayers&
nsStyleImageLayers::operator=(const nsStyleImageLayers& aOther)
{
  mAttachmentCount = aOther.mAttachmentCount;
  mClipCount = aOther.mClipCount;
  mOriginCount = aOther.mOriginCount;
  mRepeatCount = aOther.mRepeatCount;
  mPositionXCount = aOther.mPositionXCount;
  mPositionYCount = aOther.mPositionYCount;
  mImageCount = aOther.mImageCount;
  mSizeCount = aOther.mSizeCount;
  mMaskModeCount = aOther.mMaskModeCount;
  mBlendModeCount = aOther.mBlendModeCount;
  mCompositeCount = aOther.mCompositeCount;
  mLayers = aOther.mLayers;

  uint32_t count = mLayers.Length();
  if (count != aOther.mLayers.Length()) {
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

  return *this;
}

nsStyleImageLayers&
nsStyleImageLayers::operator=(nsStyleImageLayers&& aOther)
{
  mAttachmentCount = aOther.mAttachmentCount;
  mClipCount = aOther.mClipCount;
  mOriginCount = aOther.mOriginCount;
  mRepeatCount = aOther.mRepeatCount;
  mPositionXCount = aOther.mPositionXCount;
  mPositionYCount = aOther.mPositionYCount;
  mImageCount = aOther.mImageCount;
  mSizeCount = aOther.mSizeCount;
  mMaskModeCount = aOther.mMaskModeCount;
  mBlendModeCount = aOther.mBlendModeCount;
  mCompositeCount = aOther.mCompositeCount;
  mLayers = Move(aOther.mLayers);

  uint32_t count = mLayers.Length();
  if (count != aOther.mLayers.Length()) {
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

  return *this;
}

bool nsStyleImageLayers::operator==(const nsStyleImageLayers& aOther) const
{
  if (mAttachmentCount != aOther.mAttachmentCount ||
      mClipCount != aOther.mClipCount ||
      mOriginCount != aOther.mOriginCount ||
      mRepeatCount != aOther.mRepeatCount ||
      mPositionXCount != aOther.mPositionXCount ||
      mPositionYCount != aOther.mPositionYCount ||
      mImageCount != aOther.mImageCount ||
      mSizeCount != aOther.mSizeCount ||
      mMaskModeCount != aOther.mMaskModeCount ||
      mBlendModeCount != aOther.mBlendModeCount) {
    return false;
  }

  if (mLayers.Length() != aOther.mLayers.Length()) {
    return false;
  }

  for (uint32_t i = 0; i < mLayers.Length(); i++) {
    if (mLayers[i].mPosition != aOther.mLayers[i].mPosition ||
        !DefinitelyEqualURIs(mLayers[i].mImage.GetURLValue(),
                             aOther.mLayers[i].mImage.GetURLValue()) ||
        mLayers[i].mImage != aOther.mLayers[i].mImage ||
        mLayers[i].mSize != aOther.mLayers[i].mSize ||
        mLayers[i].mClip != aOther.mLayers[i].mClip ||
        mLayers[i].mOrigin != aOther.mLayers[i].mOrigin ||
        mLayers[i].mAttachment != aOther.mLayers[i].mAttachment ||
        mLayers[i].mBlendMode != aOther.mLayers[i].mBlendMode ||
        mLayers[i].mComposite != aOther.mLayers[i].mComposite ||
        mLayers[i].mMaskMode != aOther.mLayers[i].mMaskMode ||
        mLayers[i].mRepeat != aOther.mLayers[i].mRepeat) {
      return false;
    }
  }

  return true;
}

bool
nsStyleImageLayers::IsInitialPositionForLayerType(Position aPosition, LayerType aType)
{
  if (aPosition.mXPosition.mPercent == 0.0f &&
      aPosition.mXPosition.mLength == 0 &&
      aPosition.mXPosition.mHasPercent &&
      aPosition.mYPosition.mPercent == 0.0f &&
      aPosition.mYPosition.mLength == 0 &&
      aPosition.mYPosition.mHasPercent) {
    return true;
  }

  return false;
}

void
Position::SetInitialPercentValues(float aPercentVal)
{
  mXPosition.mPercent = aPercentVal;
  mXPosition.mLength = 0;
  mXPosition.mHasPercent = true;
  mYPosition.mPercent = aPercentVal;
  mYPosition.mLength = 0;
  mYPosition.mHasPercent = true;
}

void
Position::SetInitialZeroValues()
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
    if (imgRequestProxy* req = aImage.GetImageData()) {
      req->GetImage(getter_AddRefs(imgContainer));
    }
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

nsStyleImageLayers::Layer::Layer()
  : mClip(StyleGeometryBox::BorderBox)
  , mAttachment(NS_STYLE_IMAGELAYER_ATTACHMENT_SCROLL)
  , mBlendMode(NS_STYLE_BLEND_NORMAL)
  , mComposite(NS_STYLE_MASK_COMPOSITE_ADD)
  , mMaskMode(NS_STYLE_MASK_MODE_MATCH_SOURCE)
{
  mImage.SetNull();
  mSize.SetInitialValues();
}

nsStyleImageLayers::Layer::~Layer()
{
}

void
nsStyleImageLayers::Layer::Initialize(nsStyleImageLayers::LayerType aType)
{
  mRepeat.SetInitialValues();

  mPosition.SetInitialPercentValues(0.0f);

  if (aType == LayerType::Background) {
    mOrigin = StyleGeometryBox::PaddingBox;
  } else {
    MOZ_ASSERT(aType == LayerType::Mask, "unsupported layer type.");
    mOrigin = StyleGeometryBox::BorderBox;
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
         mComposite == aOther.mComposite;
}

nsChangeHint
nsStyleImageLayers::Layer::CalcDifference(const nsStyleImageLayers::Layer& aNewLayer) const
{
  nsChangeHint hint = nsChangeHint(0);
  if (!DefinitelyEqualURIs(mImage.GetURLValue(),
                           aNewLayer.mImage.GetURLValue())) {
    hint |= nsChangeHint_RepaintFrame | nsChangeHint_UpdateEffects;

    // If mImage links to an SVG mask, the URL in mImage must have a fragment.
    // Not vice versa.
    // Here are examples of URI contains a fragment, two of them link to an
    // SVG mask:
    //   mask:url(a.svg#maskID); // The fragment of this URI is an ID of a mask
    //                           // element in a.svg.
    //   mask:url(#localMaskID); // The fragment of this URI is an ID of a mask
    //                           // element in local document.
    //   mask:url(b.svg#viewBoxID); // The fragment of this URI is an ID of a
    //                              // viewbox defined in b.svg.
    // That is, if the URL in mImage has a fragment, it may link to an SVG
    // mask; If not, it "must" not link to an SVG mask.
    bool maybeSVGMask = false;
    if (mImage.GetURLValue()) {
      maybeSVGMask = mImage.GetURLValue()->MightHaveRef();
    }

    if (!maybeSVGMask && aNewLayer.mImage.GetURLValue()) {
      maybeSVGMask = aNewLayer.mImage.GetURLValue()->MightHaveRef();
    }

    // Return nsChangeHint_UpdateOverflow if either URI might link to an SVG
    // mask.
    if (maybeSVGMask) {
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
    hint |= nsChangeHint_UpdateBackgroundPosition;
  }

  return hint;
}

// --------------------
// nsStyleBackground
//

nsStyleBackground::nsStyleBackground(const nsPresContext* aContext)
  : mImage(nsStyleImageLayers::LayerType::Background)
  , mBackgroundColor(StyleComplexColor::FromColor(NS_RGBA(0, 0, 0, 0)))
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
  this->~nsStyleBackground();
  aContext->PresShell()->
    FreeByObjectID(eArenaObjectID_nsStyleBackground, this);
}

void
nsStyleBackground::FinishStyle(nsPresContext* aPresContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPresContext->StyleSet()->IsServo());

  mImage.ResolveImages(aPresContext);
}

nsChangeHint
nsStyleBackground::CalcDifference(const nsStyleBackground& aNewData) const
{
  nsChangeHint hint = nsChangeHint(0);
  if (mBackgroundColor != aNewData.mBackgroundColor) {
    hint |= nsChangeHint_RepaintFrame;
  }

  hint |= mImage.CalcDifference(aNewData.mImage,
                                nsStyleImageLayers::LayerType::Background);

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

nscolor
nsStyleBackground::BackgroundColor(const nsIFrame* aFrame) const
{
  return BackgroundColor(aFrame->StyleContext());
}

nscolor
nsStyleBackground::BackgroundColor(nsStyleContext* aContext) const
{
  // In majority of cases, background-color should just be a numeric color.
  // In that case, we can skip resolving StyleColor().
  return mBackgroundColor.IsNumericColor()
    ? mBackgroundColor.mColor
    : aContext->StyleColor()->CalcComplexColor(mBackgroundColor);
}

bool
nsStyleBackground::IsTransparent(const nsIFrame* aFrame) const
{
  return IsTransparent(aFrame->StyleContext());
}

bool
nsStyleBackground::IsTransparent(nsStyleContext* aContext) const
{
  return BottomLayer().mImage.IsEmpty() &&
         mImage.mImageCount == 1 &&
         NS_GET_A(BackgroundColor(aContext)) == 0;
}

void
nsTimingFunction::AssignFromKeyword(int32_t aTimingFunctionType)
{
  switch (aTimingFunctionType) {
    case NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_START:
      mType = Type::StepStart;
      mStepsOrFrames = 1;
      return;
    default:
      MOZ_FALLTHROUGH_ASSERT("aTimingFunctionType must be a keyword value");
    case NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END:
      mType = Type::StepEnd;
      mStepsOrFrames = 1;
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

StyleTransition::StyleTransition(const StyleTransition& aCopy)
  : mTimingFunction(aCopy.mTimingFunction)
  , mDuration(aCopy.mDuration)
  , mDelay(aCopy.mDelay)
  , mProperty(aCopy.mProperty)
  , mUnknownProperty(aCopy.mUnknownProperty)
{
}

void
StyleTransition::SetInitialValues()
{
  mTimingFunction = nsTimingFunction(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE);
  mDuration = 0.0;
  mDelay = 0.0;
  mProperty = eCSSPropertyExtra_all_properties;
}

void
StyleTransition::SetUnknownProperty(nsCSSPropertyID aProperty,
                                    const nsAString& aPropertyString)
{
  MOZ_ASSERT(nsCSSProps::LookupProperty(aPropertyString,
                                        CSSEnabledState::eForAllContent) ==
               aProperty,
             "property and property string should match");
  nsCOMPtr<nsIAtom> temp = NS_Atomize(aPropertyString);
  SetUnknownProperty(aProperty, temp);
}

void
StyleTransition::SetUnknownProperty(nsCSSPropertyID aProperty,
                                    nsIAtom* aPropertyString)
{
  MOZ_ASSERT(aProperty == eCSSProperty_UNKNOWN ||
             aProperty == eCSSPropertyExtra_variable,
             "should be either unknown or custom property");
  mProperty = aProperty;
  mUnknownProperty = aPropertyString;
}

bool
StyleTransition::operator==(const StyleTransition& aOther) const
{
  return mTimingFunction == aOther.mTimingFunction &&
         mDuration == aOther.mDuration &&
         mDelay == aOther.mDelay &&
         mProperty == aOther.mProperty &&
         (mProperty != eCSSProperty_UNKNOWN ||
          mUnknownProperty == aOther.mUnknownProperty);
}

StyleAnimation::StyleAnimation(const StyleAnimation& aCopy)
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
StyleAnimation::SetInitialValues()
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
StyleAnimation::operator==(const StyleAnimation& aOther) const
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
nsStyleDisplay::nsStyleDisplay(const nsPresContext* aContext)
  : mDisplay(StyleDisplay::Inline)
  , mOriginalDisplay(StyleDisplay::Inline)
  , mContain(NS_STYLE_CONTAIN_NONE)
  , mAppearance(NS_THEME_NONE)
  , mPosition(NS_STYLE_POSITION_STATIC)
  , mFloat(StyleFloat::None)
  , mOriginalFloat(StyleFloat::None)
  , mBreakType(StyleClear::None)
  , mBreakInside(NS_STYLE_PAGE_BREAK_AUTO)
  , mBreakBefore(false)
  , mBreakAfter(false)
  , mOverflowX(NS_STYLE_OVERFLOW_VISIBLE)
  , mOverflowY(NS_STYLE_OVERFLOW_VISIBLE)
  , mOverflowClipBox(NS_STYLE_OVERFLOW_CLIP_BOX_PADDING_BOX)
  , mResize(NS_STYLE_RESIZE_NONE)
  , mOrient(StyleOrient::Inline)
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
  , mTransformBox(StyleGeometryBox::BorderBox)
  , mSpecifiedTransform(nullptr)
  , mTransformOrigin{ {0.5f, eStyleUnit_Percent}, // Transform is centered on origin
                      {0.5f, eStyleUnit_Percent},
                      {0, nsStyleCoord::CoordConstructor} }
  , mChildPerspective(eStyleUnit_None)
  , mPerspectiveOrigin{ {0.5f, eStyleUnit_Percent},
                        {0.5f, eStyleUnit_Percent} }
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
  , mTransformOrigin{ aSource.mTransformOrigin[0],
                      aSource.mTransformOrigin[1],
                      aSource.mTransformOrigin[2] }
  , mChildPerspective(aSource.mChildPerspective)
  , mPerspectiveOrigin{ aSource.mPerspectiveOrigin[0],
                        aSource.mPerspectiveOrigin[1] }
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
  , mShapeOutside(aSource.mShapeOutside)
{
  MOZ_COUNT_CTOR(nsStyleDisplay);
}

nsStyleDisplay::~nsStyleDisplay()
{
  // We don't allow releasing nsCSSValues with refcounted data in the Servo
  // traversal, since the refcounts aren't threadsafe. Since Servo may trigger
  // the deallocation of style structs during styling, we need to handle it
  // here.
  if (mSpecifiedTransform && ServoStyleSet::IsInServoTraversal()) {
    // The default behavior of NS_ReleaseOnMainThreadSystemGroup is to only
    // proxy the release if we're not already on the main thread. This is a nice
    // optimization for the cases we happen to be doing a sequential traversal
    // (i.e. a single-core machine), but it trips our assertions which check
    // whether we're in a Servo traversal, parallel or not. So we
    // unconditionally proxy in debug builds.
    bool alwaysProxy =
#ifdef DEBUG
      true;
#else
      false;
#endif
    NS_ReleaseOnMainThreadSystemGroup(
      "nsStyleDisplay::mSpecifiedTransform",
      mSpecifiedTransform.forget(), alwaysProxy);
  }

  MOZ_COUNT_DTOR(nsStyleDisplay);
}

nsChangeHint
nsStyleDisplay::CalcDifference(const nsStyleDisplay& aNewData) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (!DefinitelyEqualURIsAndPrincipal(mBinding.ForceGet(), aNewData.mBinding.ForceGet())
      || mPosition != aNewData.mPosition
      || mDisplay != aNewData.mDisplay
      || mContain != aNewData.mContain
      || (mFloat == StyleFloat::None) != (aNewData.mFloat == StyleFloat::None)
      || mScrollBehavior != aNewData.mScrollBehavior
      || mScrollSnapTypeX != aNewData.mScrollSnapTypeX
      || mScrollSnapTypeY != aNewData.mScrollSnapTypeY
      || mScrollSnapPointsX != aNewData.mScrollSnapPointsX
      || mScrollSnapPointsY != aNewData.mScrollSnapPointsY
      || mScrollSnapDestination != aNewData.mScrollSnapDestination
      || mTopLayer != aNewData.mTopLayer
      || mResize != aNewData.mResize) {
    return nsChangeHint_ReconstructFrame;
  }

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

  if (mOverflowX != aNewData.mOverflowX
      || mOverflowY != aNewData.mOverflowY) {
    hint |= nsChangeHint_CSSOverflowChange;
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

  if (mFloat != aNewData.mFloat) {
    // Changing which side we're floating on (float:none was handled above).
    hint |= nsChangeHint_ReflowHintsForFloatAreaChange;
  }

  if (!mShapeOutside.DefinitelyEquals(aNewData.mShapeOutside)) {
    if (aNewData.mFloat != StyleFloat::None) {
      // If we are floating, and our shape-outside property changes, our
      // descendants are not impacted, but our ancestor and siblings are.
      // This is similar to a float-only change, but since the ISize of the
      // float area changes arbitrarily along its block axis, more is required
      // to get the siblings to adjust properly. Hinting overflow change is
      // sufficient to trigger the correct calculation, but may be too
      // heavyweight.

      // XXX What is the minimum hint to ensure mShapeInfo is regenerated in
      // the next reflow?
      hint |= nsChangeHint_ReflowHintsForFloatAreaChange |
              nsChangeHint_CSSOverflowChange;
    } else {
      // shape-outside changed, but we don't need to reflow because we're not
      // floating.
      hint |= nsChangeHint_NeutralChange;
    }
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
            nsChangeHint_AddOrRemoveTransform |
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

nsStyleVisibility::nsStyleVisibility(const nsPresContext* aContext)
  : mDirection(aContext->GetBidi() == IBMBIDI_TEXTDIRECTION_RTL
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
    // Also, the used writing-mode value is now a field on nsIFrame and some
    // classes (e.g. table rows/cells) copy their value from an ancestor.
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
  MOZ_COUNT_DTOR(nsStyleContentData);

  if (mType == eStyleContentType_Image) {
    NS_ReleaseOnMainThreadSystemGroup(
      "nsStyleContentData::mContent.mImage", dont_AddRef(mContent.mImage));
    mContent.mImage = nullptr;
  } else if (mType == eStyleContentType_Counter ||
             mType == eStyleContentType_Counters) {
    mContent.mCounters->Release();
  } else if (mContent.mString) {
    free(mContent.mString);
  }
}

nsStyleContentData::nsStyleContentData(const nsStyleContentData& aOther)
  : mType(aOther.mType)
{
  MOZ_COUNT_CTOR(nsStyleContentData);
  if (mType == eStyleContentType_Image) {
    mContent.mImage = aOther.mContent.mImage;
    mContent.mImage->AddRef();
  } else if (mType == eStyleContentType_Counter ||
             mType == eStyleContentType_Counters) {
    mContent.mCounters = aOther.mContent.mCounters;
    mContent.mCounters->AddRef();
  } else if (aOther.mContent.mString) {
    mContent.mString = NS_strdup(aOther.mContent.mString);
  } else {
    mContent.mString = nullptr;
  }
}

bool
nsStyleContentData::
CounterFunction::operator==(const CounterFunction& aOther) const
{
  return mIdent == aOther.mIdent &&
    mSeparator == aOther.mSeparator &&
    mCounterStyle == aOther.mCounterStyle;
}

nsStyleContentData&
nsStyleContentData::operator=(const nsStyleContentData& aOther)
{
  if (this == &aOther) {
    return *this;
  }
  this->~nsStyleContentData();
  new (this) nsStyleContentData(aOther);

  return *this;
}

bool
nsStyleContentData::operator==(const nsStyleContentData& aOther) const
{
  if (mType != aOther.mType) {
    return false;
  }
  if (mType == eStyleContentType_Image) {
    return DefinitelyEqualImages(mContent.mImage, aOther.mContent.mImage);
  }
  if (mType == eStyleContentType_Counter ||
      mType == eStyleContentType_Counters) {
    return *mContent.mCounters == *aOther.mContent.mCounters;
  }
  return safe_strcmp(mContent.mString, aOther.mContent.mString) == 0;
}

void
nsStyleContentData::Resolve(nsPresContext* aPresContext)
{
  switch (mType) {
    case eStyleContentType_Image:
      if (!mContent.mImage->IsResolved()) {
        mContent.mImage->Resolve(aPresContext);
      }
      break;
    case eStyleContentType_Counter:
    case eStyleContentType_Counters: {
      mContent.mCounters->
        mCounterStyle.Resolve(aPresContext->CounterStyleManager());
      break;
    }
    default:
      break;
  }
}


//-----------------------
// nsStyleContent
//

nsStyleContent::nsStyleContent(const nsPresContext* aContext)
{
  MOZ_COUNT_CTOR(nsStyleContent);
}

nsStyleContent::~nsStyleContent()
{
  MOZ_COUNT_DTOR(nsStyleContent);
}

void
nsStyleContent::Destroy(nsPresContext* aContext)
{
  this->~nsStyleContent();
  aContext->PresShell()->FreeByObjectID(eArenaObjectID_nsStyleContent, this);
}

void
nsStyleContent::FinishStyle(nsPresContext* aPresContext)
{
  for (nsStyleContentData& data : mContents) {
    data.Resolve(aPresContext);
  }
}

nsStyleContent::nsStyleContent(const nsStyleContent& aSource)
  : mContents(aSource.mContents)
  , mIncrements(aSource.mIncrements)
  , mResets(aSource.mResets)
{
  MOZ_COUNT_CTOR(nsStyleContent);
}

nsChangeHint
nsStyleContent::CalcDifference(const nsStyleContent& aNewData) const
{
  // In ElementRestyler::Restyle we assume that if there's no existing
  // ::before or ::after and we don't have to restyle children of the
  // node then we can't end up with a ::before or ::after due to the
  // restyle of the node itself.  That's not quite true, but the only
  // exception to the above is when the 'content' property of the node
  // changes and the pseudo-element inherits the changed value.  Since
  // the code here triggers a frame change on the node in that case,
  // the optimization in ElementRestyler::Restyle is ok.  But if we ever
  // change this code to not reconstruct frames on changes to the
  // 'content' property, then we will need to revisit the optimization
  // in ElementRestyler::Restyle.

  // Unfortunately we need to reframe even if the content lengths are the same;
  // a simple reflow will not pick up different text or different image URLs,
  // since we set all that up in the CSSFrameConstructor
  //
  // Also note that we also rely on this to return ReconstructFrame when
  // content changes to ensure that nsCounterUseNode wouldn't reference
  // to stale counter stylex.
  if (mContents != aNewData.mContents ||
      mIncrements != aNewData.mIncrements ||
      mResets != aNewData.mResets) {
    return nsChangeHint_ReconstructFrame;
  }

  return nsChangeHint(0);
}

// --------------------
// nsStyleTextReset
//

nsStyleTextReset::nsStyleTextReset(const nsPresContext* aContext)
  : mTextDecorationLine(NS_STYLE_TEXT_DECORATION_LINE_NONE)
  , mTextDecorationStyle(NS_STYLE_TEXT_DECORATION_STYLE_SOLID)
  , mUnicodeBidi(NS_STYLE_UNICODE_BIDI_NORMAL)
  , mInitialLetterSink(0)
  , mInitialLetterSize(0.0f)
  , mTextDecorationColor(StyleComplexColor::CurrentColor())
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
  if (mUnicodeBidi != aNewData.mUnicodeBidi ||
      mInitialLetterSink != aNewData.mInitialLetterSink ||
      mInitialLetterSize != aNewData.mInitialLetterSize) {
    return NS_STYLE_HINT_REFLOW;
  }

  if (mTextDecorationLine != aNewData.mTextDecorationLine ||
      mTextDecorationStyle != aNewData.mTextDecorationStyle) {
    // Changes to our text-decoration line can impact our overflow area &
    // also our descendants' overflow areas (particularly for text-frame
    // descendants).  So, we update those areas & trigger a repaint.
    return nsChangeHint_RepaintFrame |
           nsChangeHint_UpdateSubtreeOverflow |
           nsChangeHint_SchedulePaint;
  }

  // Repaint for decoration color changes
  if (mTextDecorationColor != aNewData.mTextDecorationColor) {
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

nsStyleText::nsStyleText(const nsPresContext* aContext)
  : mTextAlign(NS_STYLE_TEXT_ALIGN_START)
  , mTextAlignLast(NS_STYLE_TEXT_ALIGN_AUTO)
  , mTextAlignTrue(false)
  , mTextAlignLastTrue(false)
  , mTextJustify(StyleTextJustify::Auto)
  , mTextTransform(NS_STYLE_TEXT_TRANSFORM_NONE)
  , mWhiteSpace(StyleWhiteSpace::Normal)
  , mWordBreak(NS_STYLE_WORDBREAK_NORMAL)
  , mOverflowWrap(NS_STYLE_OVERFLOWWRAP_NORMAL)
  , mHyphens(StyleHyphens::Manual)
  , mRubyAlign(NS_STYLE_RUBY_ALIGN_SPACE_AROUND)
  , mRubyPosition(NS_STYLE_RUBY_POSITION_OVER)
  , mTextSizeAdjust(NS_STYLE_TEXT_SIZE_ADJUST_AUTO)
  , mTextCombineUpright(NS_STYLE_TEXT_COMBINE_UPRIGHT_NONE)
  , mControlCharacterVisibility(nsCSSParser::ControlCharVisibilityDefault())
  , mTextEmphasisStyle(NS_STYLE_TEXT_EMPHASIS_STYLE_NONE)
  , mTextRendering(NS_STYLE_TEXT_RENDERING_AUTO)
  , mTextEmphasisColor(StyleComplexColor::CurrentColor())
  , mWebkitTextFillColor(StyleComplexColor::CurrentColor())
  , mWebkitTextStrokeColor(StyleComplexColor::CurrentColor())
  , mTabSize(float(NS_STYLE_TABSIZE_INITIAL), eStyleUnit_Factor)
  , mWordSpacing(0, nsStyleCoord::CoordConstructor)
  , mLetterSpacing(eStyleUnit_Normal)
  , mLineHeight(eStyleUnit_Normal)
  , mTextIndent(0, nsStyleCoord::CoordConstructor)
  , mWebkitTextStrokeWidth(0)
  , mTextShadow(nullptr)
{
  MOZ_COUNT_CTOR(nsStyleText);
  nsCOMPtr<nsIAtom> language = aContext->GetContentLanguage();
  mTextEmphasisPosition = language &&
    nsStyleUtil::MatchesLanguagePrefix(language, u"zh") ?
    NS_STYLE_TEXT_EMPHASIS_POSITION_DEFAULT_ZH :
    NS_STYLE_TEXT_EMPHASIS_POSITION_DEFAULT;
}

nsStyleText::nsStyleText(const nsStyleText& aSource)
  : mTextAlign(aSource.mTextAlign)
  , mTextAlignLast(aSource.mTextAlignLast)
  , mTextAlignTrue(false)
  , mTextAlignLastTrue(false)
  , mTextJustify(aSource.mTextJustify)
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
  , mTextEmphasisColor(aSource.mTextEmphasisColor)
  , mWebkitTextFillColor(aSource.mWebkitTextFillColor)
  , mWebkitTextStrokeColor(aSource.mWebkitTextStrokeColor)
  , mTabSize(aSource.mTabSize)
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
      (mTextJustify != aNewData.mTextJustify) ||
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

  if (mTextEmphasisColor != aNewData.mTextEmphasisColor ||
      mWebkitTextFillColor != aNewData.mWebkitTextFillColor ||
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
  , mImage(aOther.mImage)
{
}

nsCursorImage&
nsCursorImage::operator=(const nsCursorImage& aOther)
{
  if (this != &aOther) {
    mHaveHotspot = aOther.mHaveHotspot;
    mHotspotX = aOther.mHotspotX;
    mHotspotY = aOther.mHotspotY;
    mImage = aOther.mImage;
  }

  return *this;
}

bool
nsCursorImage::operator==(const nsCursorImage& aOther) const
{
  NS_ASSERTION(mHaveHotspot ||
               (mHotspotX == 0 && mHotspotY == 0),
               "expected mHotspot{X,Y} to be 0 when mHaveHotspot is false");
  NS_ASSERTION(aOther.mHaveHotspot ||
               (aOther.mHotspotX == 0 && aOther.mHotspotY == 0),
               "expected mHotspot{X,Y} to be 0 when mHaveHotspot is false");
  return mHaveHotspot == aOther.mHaveHotspot &&
         mHotspotX == aOther.mHotspotX &&
         mHotspotY == aOther.mHotspotY &&
         DefinitelyEqualImages(mImage, aOther.mImage);
}

nsStyleUserInterface::nsStyleUserInterface(const nsPresContext* aContext)
  : mUserInput(StyleUserInput::Auto)
  , mUserModify(StyleUserModify::ReadOnly)
  , mUserFocus(StyleUserFocus::None)
  , mPointerEvents(NS_STYLE_POINTER_EVENTS_AUTO)
  , mCursor(NS_STYLE_CURSOR_AUTO)
  , mCaretColor(StyleComplexColor::Auto())
{
  MOZ_COUNT_CTOR(nsStyleUserInterface);
}

nsStyleUserInterface::nsStyleUserInterface(const nsStyleUserInterface& aSource)
  : mUserInput(aSource.mUserInput)
  , mUserModify(aSource.mUserModify)
  , mUserFocus(aSource.mUserFocus)
  , mPointerEvents(aSource.mPointerEvents)
  , mCursor(aSource.mCursor)
  , mCursorImages(aSource.mCursorImages)
  , mCaretColor(aSource.mCaretColor)
{
  MOZ_COUNT_CTOR(nsStyleUserInterface);
}

nsStyleUserInterface::~nsStyleUserInterface()
{
  MOZ_COUNT_DTOR(nsStyleUserInterface);
}

void
nsStyleUserInterface::FinishStyle(nsPresContext* aPresContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPresContext->StyleSet()->IsServo());

  for (nsCursorImage& cursor : mCursorImages) {
    if (cursor.mImage && !cursor.mImage->IsResolved()) {
      cursor.mImage->Resolve(aPresContext);
    }
  }
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
  if (mCursorImages != aNewData.mCursorImages) {
    hint |= nsChangeHint_UpdateCursor;
  }

  if (mPointerEvents != aNewData.mPointerEvents) {
    // SVGGeometryFrame's mRect depends on stroke _and_ on the value
    // of pointer-events. See SVGGeometryFrame::ReflowSVG's use of
    // GetHitTestFlags. (Only a reflow, no visual change.)
    hint |= nsChangeHint_NeedReflow |
            nsChangeHint_NeedDirtyReflow; // XXX remove me: bug 876085
  }

  if (mUserModify != aNewData.mUserModify) {
    hint |= NS_STYLE_HINT_VISUAL;
  }

  if (mUserInput != aNewData.mUserInput) {
    if (StyleUserInput::None == mUserInput ||
        StyleUserInput::None == aNewData.mUserInput) {
      hint |= nsChangeHint_ReconstructFrame;
    } else {
      hint |= nsChangeHint_NeutralChange;
    }
  }

  if (mUserFocus != aNewData.mUserFocus) {
    hint |= nsChangeHint_NeutralChange;
  }

  if (mCaretColor != aNewData.mCaretColor) {
    hint |= nsChangeHint_RepaintFrame;
  }

  return hint;
}

//-----------------------
// nsStyleUIReset
//

nsStyleUIReset::nsStyleUIReset(const nsPresContext* aContext)
  : mUserSelect(StyleUserSelect::Auto)
  , mForceBrokenImageIcon(0)
  , mIMEMode(NS_STYLE_IME_MODE_AUTO)
  , mWindowDragging(StyleWindowDragging::Default)
  , mWindowShadow(NS_STYLE_WINDOW_SHADOW_DEFAULT)
  , mWindowOpacity(1.0)
  , mSpecifiedWindowTransform(nullptr)
  , mWindowTransformOrigin{ {0.5f, eStyleUnit_Percent}, // Transform is centered on origin
                            {0.5f, eStyleUnit_Percent} }
{
  MOZ_COUNT_CTOR(nsStyleUIReset);
}

nsStyleUIReset::nsStyleUIReset(const nsStyleUIReset& aSource)
  : mUserSelect(aSource.mUserSelect)
  , mForceBrokenImageIcon(aSource.mForceBrokenImageIcon)
  , mIMEMode(aSource.mIMEMode)
  , mWindowDragging(aSource.mWindowDragging)
  , mWindowShadow(aSource.mWindowShadow)
  , mWindowOpacity(aSource.mWindowOpacity)
  , mSpecifiedWindowTransform(aSource.mSpecifiedWindowTransform)
  , mWindowTransformOrigin{ aSource.mWindowTransformOrigin[0],
                            aSource.mWindowTransformOrigin[1] }
{
  MOZ_COUNT_CTOR(nsStyleUIReset);
}

nsStyleUIReset::~nsStyleUIReset()
{
  MOZ_COUNT_DTOR(nsStyleUIReset);

  // See the nsStyleDisplay destructor for why we're doing this.
  if (mSpecifiedWindowTransform && ServoStyleSet::IsInServoTraversal()) {
    bool alwaysProxy =
#ifdef DEBUG
      true;
#else
      false;
#endif
    NS_ReleaseOnMainThreadSystemGroup(
      "nsStyleUIReset::mSpecifiedWindowTransform",
      mSpecifiedWindowTransform.forget(), alwaysProxy);
  }
}

nsChangeHint
nsStyleUIReset::CalcDifference(const nsStyleUIReset& aNewData) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (mForceBrokenImageIcon != aNewData.mForceBrokenImageIcon) {
    hint |= nsChangeHint_ReconstructFrame;
  }
  if (mWindowShadow != aNewData.mWindowShadow) {
    // We really need just an nsChangeHint_SyncFrameView, except
    // on an ancestor of the frame, so we get that by doing a
    // reflow.
    hint |= NS_STYLE_HINT_REFLOW;
  }
  if (mUserSelect != aNewData.mUserSelect) {
    hint |= NS_STYLE_HINT_VISUAL;
  }

  if (mWindowDragging != aNewData.mWindowDragging) {
    hint |= nsChangeHint_SchedulePaint;
  }

  if (mWindowOpacity != aNewData.mWindowOpacity ||
      !mSpecifiedWindowTransform != !aNewData.mSpecifiedWindowTransform ||
      (mSpecifiedWindowTransform &&
       *mSpecifiedWindowTransform != *aNewData.mSpecifiedWindowTransform)) {
    hint |= nsChangeHint_UpdateWidgetProperties;
  } else {
    for (uint8_t index = 0; index < 2; ++index) {
      if (mWindowTransformOrigin[index] !=
            aNewData.mWindowTransformOrigin[index]) {
        hint |= nsChangeHint_UpdateWidgetProperties;
        break;
      }
    }
  }

  if (!hint &&
      mIMEMode != aNewData.mIMEMode) {
    hint |= nsChangeHint_NeutralChange;
  }

  return hint;
}

//-----------------------
// nsStyleVariables
//

nsStyleVariables::nsStyleVariables()
{
  MOZ_COUNT_CTOR(nsStyleVariables);
}

nsStyleVariables::nsStyleVariables(const nsPresContext* aContext)
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

nsStyleEffects::nsStyleEffects(const nsPresContext* aContext)
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
