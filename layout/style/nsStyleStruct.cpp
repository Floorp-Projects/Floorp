/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Hyatt (hyatt@netscape.com)
 *   Mats Palmgren <matspal@gmail.com>
 *   Michael Ventnor <m.ventnor@gmail.com>
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>, Collabora Ltd.
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIStyleRule.h"
#include "nsCRTGlue.h"
#include "nsCSSProps.h"

#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsHTMLReflowState.h"
#include "prenv.h"

#include "nsSVGUtils.h"
#include "nsBidiUtils.h"
#include "nsLayoutUtils.h"

#include "imgIRequest.h"
#include "imgIContainer.h"
#include "prlog.h"

// Make sure we have enough bits in NS_STYLE_INHERIT_MASK.
PR_STATIC_ASSERT((((1 << nsStyleStructID_Length) - 1) &
                  ~(NS_STYLE_INHERIT_MASK)) == 0);

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

static bool EqualURIs(nsCSSValue::URL *aURI1, nsCSSValue::URL *aURI2)
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
static int safe_strcmp(const PRUnichar* a, const PRUnichar* b)
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
  : mFont(aFont),
    mGenericID(kGenericFont_NONE)
{
  MOZ_COUNT_CTOR(nsStyleFont);
  mSize = mFont.size = nsStyleFont::ZoomText(aPresContext, mFont.size);
  mScriptUnconstrainedSize = mSize;
  mScriptMinSize = aPresContext->CSSTwipsToAppUnits(
      NS_POINTS_TO_TWIPS(NS_MATHML_DEFAULT_SCRIPT_MIN_SIZE_PT));
  mScriptLevel = 0;
  mScriptSizeMultiplier = NS_MATHML_DEFAULT_SCRIPT_SIZE_MULTIPLIER;
}

nsStyleFont::nsStyleFont(const nsStyleFont& aSrc)
  : mFont(aSrc.mFont)
  , mSize(aSrc.mSize)
  , mGenericID(aSrc.mGenericID)
  , mScriptLevel(aSrc.mScriptLevel)
  , mScriptUnconstrainedSize(aSrc.mScriptUnconstrainedSize)
  , mScriptMinSize(aSrc.mScriptMinSize)
  , mScriptSizeMultiplier(aSrc.mScriptSizeMultiplier)
{
  MOZ_COUNT_CTOR(nsStyleFont);
}

nsStyleFont::nsStyleFont(nsPresContext* aPresContext)
  : mFont(*(aPresContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID))),
    mGenericID(kGenericFont_NONE)
{
  MOZ_COUNT_CTOR(nsStyleFont);
  mSize = mFont.size = nsStyleFont::ZoomText(aPresContext, mFont.size);
  mScriptUnconstrainedSize = mSize;
  mScriptMinSize = aPresContext->CSSTwipsToAppUnits(
      NS_POINTS_TO_TWIPS(NS_MATHML_DEFAULT_SCRIPT_MIN_SIZE_PT));
  mScriptLevel = 0;
  mScriptSizeMultiplier = NS_MATHML_DEFAULT_SCRIPT_SIZE_MULTIPLIER;
}

void* 
nsStyleFont::operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
  void* result = aContext->AllocateFromShell(sz);
  if (result)
    memset(result, 0, sz);
  return result;
}
  
void 
nsStyleFont::Destroy(nsPresContext* aContext) {
  this->~nsStyleFont();
  aContext->FreeToShell(sizeof(nsStyleFont), this);
}

nsChangeHint nsStyleFont::CalcDifference(const nsStyleFont& aOther) const
{
  if (mSize == aOther.mSize) {
    return CalcFontDifference(mFont, aOther.mFont);
  }
  return NS_STYLE_HINT_REFLOW;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleFont::MaxDifference()
{
  return NS_STYLE_HINT_REFLOW;
}
#endif

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
      (aFont1.variant == aFont2.variant) &&
      (aFont1.weight == aFont2.weight) &&
      (aFont1.stretch == aFont2.stretch) &&
      (aFont1.name == aFont2.name) &&
      (aFont1.featureSettings == aFont2.featureSettings) &&
      (aFont1.languageOverride == aFont2.languageOverride)) {
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
                         PRInt32 aNumEnums)
{
  if (aCoord.GetUnit() == eStyleUnit_Enumerated) {
    NS_ABORT_IF_FALSE(aEnumTable, "must have enum table");
    PRInt32 value = aCoord.GetIntValue();
    if (0 <= value && value < aNumEnums) {
      return aEnumTable[aCoord.GetIntValue()];
    }
    NS_NOTREACHED("unexpected enum value");
    return 0;
  }
  NS_ABORT_IF_FALSE(aCoord.ConvertsToLength(), "unexpected unit");
  return nsRuleNode::ComputeCoordPercentCalc(aCoord, 0);
}

nsStyleMargin::nsStyleMargin() {
  MOZ_COUNT_CTOR(nsStyleMargin);
  nsStyleCoord zero(0, nsStyleCoord::CoordConstructor);
  NS_FOR_CSS_SIDES(side) {
    mMargin.Set(side, zero);
  }
  mHasCachedMargin = false;
}

nsStyleMargin::nsStyleMargin(const nsStyleMargin& aSrc) {
  MOZ_COUNT_CTOR(nsStyleMargin);
  mMargin = aSrc.mMargin;
  mHasCachedMargin = false;
}

void* 
nsStyleMargin::operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
  void* result = aContext->AllocateFromShell(sz);
  if (result)
    memset(result, 0, sz);
  return result;
}
  
void 
nsStyleMargin::Destroy(nsPresContext* aContext) {
  this->~nsStyleMargin();
  aContext->FreeToShell(sizeof(nsStyleMargin), this);
}


void nsStyleMargin::RecalcData()
{
  if (IsFixedData(mMargin, false)) {
    NS_FOR_CSS_SIDES(side) {
      mCachedMargin.Side(side) = CalcCoord(mMargin.Get(side), nsnull, 0);
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
  return NS_SubtractHint(NS_STYLE_HINT_REFLOW,
                         NS_CombineHint(nsChangeHint_ClearDescendantIntrinsics,
                                        nsChangeHint_NeedDirtyReflow));
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleMargin::MaxDifference()
{
  return NS_SubtractHint(NS_STYLE_HINT_REFLOW,
                         NS_CombineHint(nsChangeHint_ClearDescendantIntrinsics,
                                        nsChangeHint_NeedDirtyReflow));
}
#endif

nsStylePadding::nsStylePadding() {
  MOZ_COUNT_CTOR(nsStylePadding);
  nsStyleCoord zero(0, nsStyleCoord::CoordConstructor);
  NS_FOR_CSS_SIDES(side) {
    mPadding.Set(side, zero);
  }
  mHasCachedPadding = false;
}

nsStylePadding::nsStylePadding(const nsStylePadding& aSrc) {
  MOZ_COUNT_CTOR(nsStylePadding);
  mPadding = aSrc.mPadding;
  mHasCachedPadding = false;
}

void* 
nsStylePadding::operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
  void* result = aContext->AllocateFromShell(sz);
  if (result)
    memset(result, 0, sz);
  return result;
}
  
void 
nsStylePadding::Destroy(nsPresContext* aContext) {
  this->~nsStylePadding();
  aContext->FreeToShell(sizeof(nsStylePadding), this);
}

void nsStylePadding::RecalcData()
{
  if (IsFixedData(mPadding, false)) {
    NS_FOR_CSS_SIDES(side) {
      // Clamp negative calc() to 0.
      mCachedPadding.Side(side) =
        NS_MAX(CalcCoord(mPadding.Get(side), nsnull, 0), 0);
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

#ifdef DEBUG
/* static */
nsChangeHint nsStylePadding::MaxDifference()
{
  return NS_SubtractHint(NS_STYLE_HINT_REFLOW,
                         nsChangeHint_ClearDescendantIntrinsics);
}
#endif

nsStyleBorder::nsStyleBorder(nsPresContext* aPresContext)
  : mHaveBorderImageWidth(false)
#ifdef DEBUG
  , mImageTracked(false)
#endif
  , mComputedBorder(0, 0, 0, 0)
  , mBorderImage(nsnull)
{
  MOZ_COUNT_CTOR(nsStyleBorder);
  nscoord medium =
    (aPresContext->GetBorderWidthTable())[NS_STYLE_BORDER_WIDTH_MEDIUM];
  NS_FOR_CSS_SIDES(side) {
    mBorder.Side(side) = medium;
    mBorderStyle[side] = NS_STYLE_BORDER_STYLE_NONE | BORDER_COLOR_FOREGROUND;
    mBorderColor[side] = NS_RGB(0, 0, 0);
  }
  NS_FOR_CSS_HALF_CORNERS(corner) {
    mBorderRadius.Set(corner, nsStyleCoord(0, nsStyleCoord::CoordConstructor));
  }

  mBorderColors = nsnull;
  mBoxShadow = nsnull;

  mFloatEdge = NS_STYLE_FLOAT_EDGE_CONTENT;

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
  if (NS_UNLIKELY(!result))
    return result;
  if (aDeep)
    NS_CSS_CLONE_LIST_MEMBER(nsBorderColors, this, mNext, result, (false));
  return result;
}

nsStyleBorder::nsStyleBorder(const nsStyleBorder& aSrc)
  : mBorderRadius(aSrc.mBorderRadius),
    mBorderImageSplit(aSrc.mBorderImageSplit),
    mFloatEdge(aSrc.mFloatEdge),
    mBorderImageHFill(aSrc.mBorderImageHFill),
    mBorderImageVFill(aSrc.mBorderImageVFill),
    mBorderColors(nsnull),
    mBoxShadow(aSrc.mBoxShadow),
    mHaveBorderImageWidth(aSrc.mHaveBorderImageWidth),
    mBorderImageWidth(aSrc.mBorderImageWidth),
    mComputedBorder(aSrc.mComputedBorder),
    mBorder(aSrc.mBorder),
    mBorderImage(aSrc.mBorderImage),
    mTwipsPerPixel(aSrc.mTwipsPerPixel)
{
  MOZ_COUNT_CTOR(nsStyleBorder);
  if (aSrc.mBorderColors) {
    EnsureBorderColors();
    for (PRInt32 i = 0; i < 4; i++)
      if (aSrc.mBorderColors[i])
        mBorderColors[i] = aSrc.mBorderColors[i]->Clone();
      else
        mBorderColors[i] = nsnull;
  }

  NS_FOR_CSS_SIDES(side) {
    mBorderStyle[side] = aSrc.mBorderStyle[side];
    mBorderColor[side] = aSrc.mBorderColor[side];
  }
  NS_FOR_CSS_HALF_CORNERS(corner) {
    mBorderRadius.Set(corner, aSrc.mBorderRadius.Get(corner));
  }
}

nsStyleBorder::~nsStyleBorder()
{
  NS_ABORT_IF_FALSE(!mImageTracked,
                    "nsStyleBorder being destroyed while still tracking image!");
  MOZ_COUNT_DTOR(nsStyleBorder);
  if (mBorderColors) {
    for (PRInt32 i = 0; i < 4; i++)
      delete mBorderColors[i];
    delete [] mBorderColors;
  }
}

void* 
nsStyleBorder::operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
  void* result = aContext->AllocateFromShell(sz);
  if (result)
    memset(result, 0, sz);
  return result;
}
  
void 
nsStyleBorder::Destroy(nsPresContext* aContext) {
  if (mBorderImage)
    UntrackImage(aContext);
  this->~nsStyleBorder();
  aContext->FreeToShell(sizeof(nsStyleBorder), this);
}


nsChangeHint nsStyleBorder::CalcDifference(const nsStyleBorder& aOther) const
{
  nsChangeHint shadowDifference =
    CalcShadowDifference(mBoxShadow, aOther.mBoxShadow);

  // Note that differences in mBorder don't affect rendering (which should only
  // use mComputedBorder), so don't need to be tested for here.
  // XXXbz we should be able to return a more specific change hint for
  // at least GetActualBorder() differences...
  if (mTwipsPerPixel != aOther.mTwipsPerPixel ||
      GetActualBorder() != aOther.GetActualBorder() || 
      mFloatEdge != aOther.mFloatEdge ||
      (shadowDifference & nsChangeHint_ReflowFrame))
    return NS_STYLE_HINT_REFLOW;

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
    if (mBorderImage != aOther.mBorderImage ||
        mBorderImageHFill != aOther.mBorderImageHFill ||
        mBorderImageVFill != aOther.mBorderImageVFill ||
        mBorderImageSplit != aOther.mBorderImageSplit)
      return NS_STYLE_HINT_VISUAL;
    // The call to GetActualBorder above already considered
    // mBorderImageWidth and mHaveBorderImageWidth.
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

  return shadowDifference;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleBorder::MaxDifference()
{
  return NS_STYLE_HINT_REFLOW;
}
#endif

bool
nsStyleBorder::ImageBorderDiffers() const
{
  return mComputedBorder !=
           (mHaveBorderImageWidth ? mBorderImageWidth : mBorder);
}

const nsMargin&
nsStyleBorder::GetActualBorder() const
{
  if (IsBorderImageLoaded())
    if (mHaveBorderImageWidth)
      return mBorderImageWidth;
    else
      return mBorder;
  else
    return mComputedBorder;
}

void
nsStyleBorder::TrackImage(nsPresContext* aContext)
{
  // Sanity
  NS_ABORT_IF_FALSE(!mImageTracked, "Already tracking image!");
  NS_ABORT_IF_FALSE(mBorderImage, "Can't track null image!");

  // Register the image with the document
  nsIDocument* doc = aContext->Document();
  if (doc)
    doc->AddImage(mBorderImage);

  // Mark state
#ifdef DEBUG
  mImageTracked = true;
#endif
}

void
nsStyleBorder::UntrackImage(nsPresContext* aContext)
{
  // Sanity
  NS_ABORT_IF_FALSE(mImageTracked, "Image not tracked!");
  NS_ABORT_IF_FALSE(mBorderImage, "Can't track null image!");

  // Unregister the image with the document
  nsIDocument* doc = aContext->Document();
  if (doc)
    doc->RemoveImage(mBorderImage);

  // Mark state
#ifdef DEBUG
  mImageTracked = false;
#endif
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

nsStyleOutline::nsStyleOutline(const nsStyleOutline& aSrc) {
  MOZ_COUNT_CTOR(nsStyleOutline);
  memcpy((nsStyleOutline*)this, &aSrc, sizeof(nsStyleOutline));
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
      NS_MAX(CalcCoord(mOutlineWidth, aContext->GetBorderWidthTable(), 3), 0);
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
    return NS_CombineHint(nsChangeHint_ReflowFrame, nsChangeHint_RepaintFrame);
  }
  if ((mOutlineStyle != aOther.mOutlineStyle) ||
      (mOutlineColor != aOther.mOutlineColor) ||
      (mOutlineRadius != aOther.mOutlineRadius)) {
    return nsChangeHint_RepaintFrame;
  }
  return NS_STYLE_HINT_NONE;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleOutline::MaxDifference()
{
  return NS_CombineHint(nsChangeHint_ReflowFrame, nsChangeHint_RepaintFrame);
}
#endif

// --------------------
// nsStyleList
//
nsStyleList::nsStyleList() 
  : mListStyleType(NS_STYLE_LIST_STYLE_DISC),
    mListStylePosition(NS_STYLE_LIST_STYLE_POSITION_OUTSIDE)
{
  MOZ_COUNT_CTOR(nsStyleList);
}

nsStyleList::~nsStyleList() 
{
  MOZ_COUNT_DTOR(nsStyleList);
}

nsStyleList::nsStyleList(const nsStyleList& aSource)
  : mListStyleType(aSource.mListStyleType),
    mListStylePosition(aSource.mListStylePosition),
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
      mListStyleType == aOther.mListStyleType) {
    if (mImageRegion.IsEqualInterior(aOther.mImageRegion))
      return NS_STYLE_HINT_NONE;
    if (mImageRegion.width == aOther.mImageRegion.width &&
        mImageRegion.height == aOther.mImageRegion.height)
      return NS_STYLE_HINT_VISUAL;
  }
  return NS_STYLE_HINT_REFLOW;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleList::MaxDifference()
{
  return NS_STYLE_HINT_FRAMECHANGE;
}
#endif

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
{
  MOZ_COUNT_CTOR(nsStyleXUL);
  memcpy((nsStyleXUL*)this, &aSource, sizeof(nsStyleXUL));
}

nsChangeHint nsStyleXUL::CalcDifference(const nsStyleXUL& aOther) const
{
  if (mBoxAlign == aOther.mBoxAlign &&
      mBoxDirection == aOther.mBoxDirection &&
      mBoxFlex == aOther.mBoxFlex &&
      mBoxOrient == aOther.mBoxOrient &&
      mBoxPack == aOther.mBoxPack &&
      mBoxOrdinal == aOther.mBoxOrdinal)
    return NS_STYLE_HINT_NONE;
  if (mBoxOrdinal != aOther.mBoxOrdinal)
    return NS_STYLE_HINT_FRAMECHANGE;
  return NS_STYLE_HINT_REFLOW;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleXUL::MaxDifference()
{
  return NS_STYLE_HINT_FRAMECHANGE;
}
#endif

// --------------------
// nsStyleColumn
//
nsStyleColumn::nsStyleColumn(nsPresContext* aPresContext)
{
  MOZ_COUNT_CTOR(nsStyleColumn);
  mColumnCount = NS_STYLE_COLUMN_COUNT_AUTO;
  mColumnWidth.SetAutoValue();
  mColumnGap.SetNormalValue();

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
{
  MOZ_COUNT_CTOR(nsStyleColumn);
  memcpy((nsStyleColumn*)this, &aSource, sizeof(nsStyleColumn));
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
      mColumnGap != aOther.mColumnGap)
    return NS_STYLE_HINT_REFLOW;

  if (GetComputedColumnRuleWidth() != aOther.GetComputedColumnRuleWidth() ||
      mColumnRuleStyle != aOther.mColumnRuleStyle ||
      mColumnRuleColor != aOther.mColumnRuleColor ||
      mColumnRuleColorIsForeground != aOther.mColumnRuleColorIsForeground)
    return NS_STYLE_HINT_VISUAL;

  return NS_STYLE_HINT_NONE;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleColumn::MaxDifference()
{
  return NS_STYLE_HINT_FRAMECHANGE;
}
#endif

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
    mStrokeDasharray         = nsnull;

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
    mShapeRendering          = NS_STYLE_SHAPE_RENDERING_AUTO;
    mStrokeLinecap           = NS_STYLE_STROKE_LINECAP_BUTT;
    mStrokeLinejoin          = NS_STYLE_STROKE_LINEJOIN_MITER;
    mTextAnchor              = NS_STYLE_TEXT_ANCHOR_START;
    mTextRendering           = NS_STYLE_TEXT_RENDERING_AUTO;
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
    if (mStrokeDasharray)
      memcpy(mStrokeDasharray,
             aSource.mStrokeDasharray,
             mStrokeDasharrayLength * sizeof(nsStyleCoord));
    else
      mStrokeDasharrayLength = 0;
  } else {
    mStrokeDasharray = nsnull;
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
  mShapeRendering = aSource.mShapeRendering;
  mStrokeLinecap = aSource.mStrokeLinecap;
  mStrokeLinejoin = aSource.mStrokeLinejoin;
  mTextAnchor = aSource.mTextAnchor;
  mTextRendering = aSource.mTextRendering;
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

  if (mTextRendering != aOther.mTextRendering) {
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
    // May be needed for non-svg frames
    NS_UpdateHint(hint, nsChangeHint_ReflowFrame);
  }

  if (!EqualURIs(mMarkerEnd, aOther.mMarkerEnd) ||
      !EqualURIs(mMarkerMid, aOther.mMarkerMid) ||
      !EqualURIs(mMarkerStart, aOther.mMarkerStart)) {
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
    NS_UpdateHint(hint, nsChangeHint_UpdateEffects);
    return hint;
  }

  if (mFill != aOther.mFill ||
      mStroke != aOther.mStroke) {
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
    if (PaintURIChanged(mFill, aOther.mFill) ||
        PaintURIChanged(mStroke, aOther.mStroke)) {
      NS_UpdateHint(hint, nsChangeHint_UpdateEffects);
    }
    // Nothing more to do, below we can only set "repaint"
    return hint;
  }

  if ( mStrokeDashoffset      != aOther.mStrokeDashoffset      ||
       mStrokeWidth           != aOther.mStrokeWidth           ||

       mFillOpacity           != aOther.mFillOpacity           ||
       mStrokeMiterlimit      != aOther.mStrokeMiterlimit      ||
       mStrokeOpacity         != aOther.mStrokeOpacity         ||

       mClipRule              != aOther.mClipRule              ||
       mColorInterpolation    != aOther.mColorInterpolation    ||
       mColorInterpolationFilters != aOther.mColorInterpolationFilters ||
       mFillRule              != aOther.mFillRule              ||
       mImageRendering        != aOther.mImageRendering        ||
       mShapeRendering        != aOther.mShapeRendering        ||
       mStrokeDasharrayLength != aOther.mStrokeDasharrayLength ||
       mStrokeLinecap         != aOther.mStrokeLinecap         ||
       mStrokeLinejoin        != aOther.mStrokeLinejoin        ||
       mTextAnchor            != aOther.mTextAnchor) {
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
    return hint;
  }

  // length of stroke dasharrays are the same (tested above) - check entries
  for (PRUint32 i=0; i<mStrokeDasharrayLength; i++)
    if (mStrokeDasharray[i] != aOther.mStrokeDasharray[i]) {
      NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
      return hint;
    }

  return hint;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleSVG::MaxDifference()
{
  return NS_CombineHint(NS_CombineHint(nsChangeHint_UpdateEffects,
                                       nsChangeHint_ReflowFrame),
                                       nsChangeHint_RepaintFrame);
}
#endif

// --------------------
// nsStyleSVGReset
//
nsStyleSVGReset::nsStyleSVGReset() 
{
    MOZ_COUNT_CTOR(nsStyleSVGReset);
    mStopColor               = NS_RGB(0,0,0);
    mFloodColor              = NS_RGB(0,0,0);
    mLightingColor           = NS_RGB(255,255,255);
    mClipPath                = nsnull;
    mFilter                  = nsnull;
    mMask                    = nsnull;
    mStopOpacity             = 1.0f;
    mFloodOpacity            = 1.0f;
    mDominantBaseline        = NS_STYLE_DOMINANT_BASELINE_AUTO;
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
  mFilter = aSource.mFilter;
  mMask = aSource.mMask;
  mStopOpacity = aSource.mStopOpacity;
  mFloodOpacity = aSource.mFloodOpacity;
  mDominantBaseline = aSource.mDominantBaseline;
}

nsChangeHint nsStyleSVGReset::CalcDifference(const nsStyleSVGReset& aOther) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (!EqualURIs(mClipPath, aOther.mClipPath) ||
      !EqualURIs(mFilter, aOther.mFilter)     ||
      !EqualURIs(mMask, aOther.mMask)) {
    NS_UpdateHint(hint, nsChangeHint_UpdateEffects);
    NS_UpdateHint(hint, nsChangeHint_ReflowFrame);
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
  } else if (mStopColor        != aOther.mStopColor     ||
             mFloodColor       != aOther.mFloodColor    ||
             mLightingColor    != aOther.mLightingColor ||
             mStopOpacity      != aOther.mStopOpacity   ||
             mFloodOpacity     != aOther.mFloodOpacity  ||
             mDominantBaseline != aOther.mDominantBaseline)
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);

  return hint;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleSVGReset::MaxDifference()
{
  return NS_CombineHint(NS_CombineHint(nsChangeHint_UpdateEffects,
                                       nsChangeHint_ReflowFrame),
                                       nsChangeHint_RepaintFrame);
}
#endif

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
  if (mType == eStyleSVGPaintType_None)
    return true;
  return mPaint.mColor == aOther.mPaint.mColor;
}


// --------------------
// nsStylePosition
//
nsStylePosition::nsStylePosition(void) 
{ 
  MOZ_COUNT_CTOR(nsStylePosition);
  // positioning values not inherited
  nsStyleCoord  autoCoord(eStyleUnit_Auto);
  mOffset.SetLeft(autoCoord);
  mOffset.SetTop(autoCoord);
  mOffset.SetRight(autoCoord);
  mOffset.SetBottom(autoCoord);
  mWidth.SetAutoValue();
  mMinWidth.SetCoordValue(0);
  mMaxWidth.SetNoneValue();
  mHeight.SetAutoValue();
  mMinHeight.SetCoordValue(0);
  mMaxHeight.SetNoneValue();
  mBoxSizing = NS_STYLE_BOX_SIZING_CONTENT;
  mZIndex.SetAutoValue();
}

nsStylePosition::~nsStylePosition(void) 
{ 
  MOZ_COUNT_DTOR(nsStylePosition);
}

nsStylePosition::nsStylePosition(const nsStylePosition& aSource)
{
  MOZ_COUNT_CTOR(nsStylePosition);
  memcpy((nsStylePosition*)this, &aSource, sizeof(nsStylePosition));
}

nsChangeHint nsStylePosition::CalcDifference(const nsStylePosition& aOther) const
{
  nsChangeHint hint =
    (mZIndex == aOther.mZIndex) ? NS_STYLE_HINT_NONE : nsChangeHint_RepaintFrame;

  if (mBoxSizing != aOther.mBoxSizing) {
    // Can affect both widths and heights; just a bad scene.
    return NS_CombineHint(hint, nsChangeHint_ReflowFrame);
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
    return NS_CombineHint(hint, nsChangeHint_ReflowFrame);
  }

  if ((mWidth == aOther.mWidth) &&
      (mMinWidth == aOther.mMinWidth) &&
      (mMaxWidth == aOther.mMaxWidth)) {
    if (mOffset == aOther.mOffset) {
      return hint;
    } else {
      // Offset changes only affect positioned content, and can't affect any
      // intrinsic widths.  They also don't need to force reflow of
      // descendants.
      return NS_CombineHint(hint, nsChangeHint_NeedReflow);
    }
  }

  // None of our width differences can affect descendant intrinsic
  // sizes and none of them need to force children to reflow.
  return
    NS_CombineHint(hint,
                   NS_SubtractHint(nsChangeHint_ReflowFrame,
                                   NS_CombineHint(nsChangeHint_ClearDescendantIntrinsics,
                                                  nsChangeHint_NeedDirtyReflow)));
}

#ifdef DEBUG
/* static */
nsChangeHint nsStylePosition::MaxDifference()
{
  return NS_STYLE_HINT_REFLOW;
}
#endif

/* static */ bool
nsStylePosition::WidthCoordDependsOnContainer(const nsStyleCoord &aCoord)
{
  return aCoord.GetUnit() == eStyleUnit_Auto ||
         aCoord.HasPercent() ||
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
  mCols  = NS_STYLE_TABLE_COLS_NONE;
  mFrame = NS_STYLE_TABLE_FRAME_NONE;
  mRules = NS_STYLE_TABLE_RULES_NONE;
  mSpan = 1;
}

nsStyleTable::~nsStyleTable(void) 
{
  MOZ_COUNT_DTOR(nsStyleTable);
}

nsStyleTable::nsStyleTable(const nsStyleTable& aSource)
{
  MOZ_COUNT_CTOR(nsStyleTable);
  memcpy((nsStyleTable*)this, &aSource, sizeof(nsStyleTable));
}

nsChangeHint nsStyleTable::CalcDifference(const nsStyleTable& aOther) const
{
  // Changes in mRules may require reframing (if border-collapse stuff changes, for example).
  if (mRules != aOther.mRules || mSpan != aOther.mSpan ||
      mLayoutStrategy != aOther.mLayoutStrategy)
    return NS_STYLE_HINT_FRAMECHANGE;
  if (mFrame != aOther.mFrame || mCols != aOther.mCols)
    return NS_STYLE_HINT_REFLOW;
  return NS_STYLE_HINT_NONE;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleTable::MaxDifference()
{
  return NS_STYLE_HINT_FRAMECHANGE;
}
#endif

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
{
  MOZ_COUNT_CTOR(nsStyleTableBorder);
  memcpy((nsStyleTableBorder*)this, &aSource, sizeof(nsStyleTableBorder));
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

#ifdef DEBUG
/* static */
nsChangeHint nsStyleTableBorder::MaxDifference()
{
  return NS_STYLE_HINT_FRAMECHANGE;
}
#endif

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

#ifdef DEBUG
/* static */
nsChangeHint nsStyleColor::MaxDifference()
{
  return NS_STYLE_HINT_VISUAL;
}
#endif

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
      mToCorner != aOther.mToCorner ||
      mBgPosX != aOther.mBgPosX ||
      mBgPosY != aOther.mBgPosY ||
      mAngle != aOther.mAngle)
    return false;

  if (mStops.Length() != aOther.mStops.Length())
    return false;

  for (PRUint32 i = 0; i < mStops.Length(); i++) {
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
  , mToCorner(false)
{
}

bool
nsStyleGradient::IsOpaque()
{
  for (PRUint32 i = 0; i < mStops.Length(); i++) {
    if (NS_GET_A(mStops[i].mColor) < 255)
      return false;
  }
  return true;
}

// --------------------
// nsStyleImage
//

nsStyleImage::nsStyleImage()
  : mType(eStyleImageType_Null)
  , mCropRect(nsnull)
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
  , mCropRect(nsnull)
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
  mCropRect = nsnull;
}

void
nsStyleImage::SetImageData(imgIRequest* aImage)
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
    doc->RemoveImage(mImage);

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
nsStyleImage::SetElementId(const PRUnichar* aElementId)
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
    mCropRect = nsnull;
  }
}

static PRInt32
ConvertToPixelCoord(const nsStyleCoord& aCoord, PRInt32 aPercentScale)
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
  pixelValue = NS_MIN(pixelValue, double(PR_INT32_MAX)); // avoid overflow
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

  PRInt32 left   = ConvertToPixelCoord(mCropRect->GetLeft(),   imageSize.width);
  PRInt32 top    = ConvertToPixelCoord(mCropRect->GetTop(),    imageSize.height);
  PRInt32 right  = ConvertToPixelCoord(mCropRect->GetRight(),  imageSize.width);
  PRInt32 bottom = ConvertToPixelCoord(mCropRect->GetBottom(), imageSize.height);

  // IntersectRect() returns an empty rect if we get negative width or height
  nsIntRect cropRect(left, top, right - left, bottom - top);
  nsIntRect imageRect(nsIntPoint(0, 0), imageSize);
  aActualCropRect.IntersectRect(imageRect, cropRect);

  if (aIsEntireImage)
    *aIsEntireImage = aActualCropRect.IsEqualInterior(imageRect);
  return true;
}

nsresult
nsStyleImage::RequestDecode() const
{
  if ((mType == eStyleImageType_Image) && mImage)
    return mImage->RequestDecode();
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

  // Check if the crop region of the current image frame is opaque
  bool isOpaque;
  if (NS_SUCCEEDED(imageContainer->GetCurrentFrameIsOpaque(&isOpaque)) &&
      isOpaque) {
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
      PRUint32 status = imgIRequest::STATUS_ERROR;
      return NS_SUCCEEDED(mImage->GetImageStatus(&status)) &&
             (status & imgIRequest::STATUS_SIZE_AVAILABLE) &&
             (status & imgIRequest::STATUS_FRAME_COMPLETE);
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
  , mBackgroundColor(NS_RGBA(0, 0, 0, 0))
  , mBackgroundInlinePolicy(NS_STYLE_BG_INLINE_POLICY_CONTINUOUS)
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
  , mLayers(aSource.mLayers) // deep copy
  , mBackgroundColor(aSource.mBackgroundColor)
  , mBackgroundInlinePolicy(aSource.mBackgroundInlinePolicy)
{
  MOZ_COUNT_CTOR(nsStyleBackground);
  // If the deep copy of mLayers failed, truncate the counts.
  PRUint32 count = mLayers.Length();
  if (count != aSource.mLayers.Length()) {
    NS_WARNING("truncating counts due to out-of-memory");
    mAttachmentCount = NS_MAX(mAttachmentCount, count);
    mClipCount = NS_MAX(mClipCount, count);
    mOriginCount = NS_MAX(mOriginCount, count);
    mRepeatCount = NS_MAX(mRepeatCount, count);
    mPositionCount = NS_MAX(mPositionCount, count);
    mImageCount = NS_MAX(mImageCount, count);
    mSizeCount = NS_MAX(mSizeCount, count);
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
  for (PRUint32 i = 0; i < mImageCount; ++i)
    mLayers[i].UntrackImages(aContext);

  this->~nsStyleBackground();
  aContext->FreeToShell(sizeof(nsStyleBackground), this);
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

  if (hasVisualDifference ||
      mBackgroundColor != aOther.mBackgroundColor ||
      mBackgroundInlinePolicy != aOther.mBackgroundInlinePolicy)
    return NS_STYLE_HINT_VISUAL;

  return NS_STYLE_HINT_NONE;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleBackground::MaxDifference()
{
  return NS_CombineHint(nsChangeHint_UpdateEffects, NS_STYLE_HINT_VISUAL);
}
#endif

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
nsStyleBackground::Position::SetInitialValues()
{
  // Initial value is "0% 0%"
  mXPosition.mPercent = 0.0f;
  mXPosition.mLength = 0;
  mXPosition.mHasPercent = true;
  mYPosition.mPercent = 0.0f;
  mYPosition.mLength = 0;
  mYPosition.mHasPercent = true;
}

bool
nsStyleBackground::Size::DependsOnFrameSize(const nsStyleImage& aImage) const
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
  mRepeat = NS_STYLE_BG_REPEAT_XY;
  mPosition.SetInitialValues();
  mSize.SetInitialValues();
  mImage.SetNull();
}

bool
nsStyleBackground::Layer::RenderingMightDependOnFrameSize() const
{
  // Do we even have an image?
  if (mImage.IsEmpty()) {
    return false;
  }

  return mPosition.DependsOnFrameSize() || mSize.DependsOnFrameSize(mImage);
}

bool
nsStyleBackground::Layer::operator==(const Layer& aOther) const
{
  return mAttachment == aOther.mAttachment &&
         mClip == aOther.mClip &&
         mOrigin == aOther.mOrigin &&
         mRepeat == aOther.mRepeat &&
         mPosition == aOther.mPosition &&
         mSize == aOther.mSize &&
         mImage == aOther.mImage;
}

// --------------------
// nsStyleDisplay
//
void nsTimingFunction::AssignFromKeyword(PRInt32 aTimingFunctionType)
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

  PR_STATIC_ASSERT(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE == 0);
  PR_STATIC_ASSERT(NS_STYLE_TRANSITION_TIMING_FUNCTION_LINEAR == 1);
  PR_STATIC_ASSERT(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN == 2);
  PR_STATIC_ASSERT(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_OUT == 3);
  PR_STATIC_ASSERT(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN_OUT == 4);

  static const float timingFunctionValues[5][4] = {
    { 0.25, 0.10, 0.25, 1.00 }, // ease
    { 0.00, 0.00, 1.00, 1.00 }, // linear
    { 0.42, 0.00, 1.00, 1.00 }, // ease-in
    { 0.00, 0.00, 0.58, 1.00 }, // ease-out
    { 0.42, 0.00, 0.58, 1.00 }  // ease-in-out
  };

  NS_ABORT_IF_FALSE(0 <= aTimingFunctionType && aTimingFunctionType < 5,
                    "keyword out of range");
  mFunc.mX1 = timingFunctionValues[aTimingFunctionType][0];
  mFunc.mY1 = timingFunctionValues[aTimingFunctionType][1];
  mFunc.mX2 = timingFunctionValues[aTimingFunctionType][2];
  mFunc.mY2 = timingFunctionValues[aTimingFunctionType][3];
}

nsTransition::nsTransition(const nsTransition& aCopy)
  : mTimingFunction(aCopy.mTimingFunction)
  , mDuration(aCopy.mDuration)
  , mDelay(aCopy.mDelay)
  , mProperty(aCopy.mProperty)
  , mUnknownProperty(aCopy.mUnknownProperty)
{
}

void nsTransition::SetInitialValues()
{
  mTimingFunction = nsTimingFunction(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE);
  mDuration = 0.0;
  mDelay = 0.0;
  mProperty = eCSSPropertyExtra_all_properties;
}

void nsTransition::SetUnknownProperty(const nsAString& aUnknownProperty)
{
  NS_ASSERTION(nsCSSProps::LookupProperty(aUnknownProperty) ==
                 eCSSProperty_UNKNOWN,
               "should be unknown property");
  mProperty = eCSSProperty_UNKNOWN;
  mUnknownProperty = do_GetAtom(aUnknownProperty);
}

nsAnimation::nsAnimation(const nsAnimation& aCopy)
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
nsAnimation::SetInitialValues()
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

nsStyleDisplay::nsStyleDisplay()
{
  MOZ_COUNT_CTOR(nsStyleDisplay);
  mAppearance = NS_THEME_NONE;
  mDisplay = NS_STYLE_DISPLAY_INLINE;
  mOriginalDisplay = mDisplay;
  mPosition = NS_STYLE_POSITION_STATIC;
  mFloats = NS_STYLE_FLOAT_NONE;
  mOriginalFloats = mFloats;
  mBreakType = NS_STYLE_CLEAR_NONE;
  mBreakBefore = false;
  mBreakAfter = false;
  mOverflowX = NS_STYLE_OVERFLOW_VISIBLE;
  mOverflowY = NS_STYLE_OVERFLOW_VISIBLE;
  mResize = NS_STYLE_RESIZE_NONE;
  mClipFlags = NS_STYLE_CLIP_AUTO;
  mClip.SetRect(0,0,0,0);
  mOpacity = 1.0f;
  mSpecifiedTransform = nsnull;
  mTransformOrigin[0].SetPercentValue(0.5f); // Transform is centered on origin
  mTransformOrigin[1].SetPercentValue(0.5f);
  mTransformOrigin[2].SetCoordValue(0);
  mPerspectiveOrigin[0].SetPercentValue(0.5f);
  mPerspectiveOrigin[1].SetPercentValue(0.5f);
  mChildPerspective.SetCoordValue(0);
  mBackfaceVisibility = NS_STYLE_BACKFACE_VISIBILITY_VISIBLE;
  mTransformStyle = NS_STYLE_TRANSFORM_STYLE_FLAT;
  mOrient = NS_STYLE_ORIENT_HORIZONTAL;

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
  : mTransitions(aSource.mTransitions)
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
  mAppearance = aSource.mAppearance;
  mDisplay = aSource.mDisplay;
  mOriginalDisplay = aSource.mOriginalDisplay;
  mOriginalFloats = aSource.mOriginalFloats;
  mBinding = aSource.mBinding;
  mPosition = aSource.mPosition;
  mFloats = aSource.mFloats;
  mBreakType = aSource.mBreakType;
  mBreakBefore = aSource.mBreakBefore;
  mBreakAfter = aSource.mBreakAfter;
  mOverflowX = aSource.mOverflowX;
  mOverflowY = aSource.mOverflowY;
  mResize = aSource.mResize;
  mClipFlags = aSource.mClipFlags;
  mClip = aSource.mClip;
  mOpacity = aSource.mOpacity;
  mOrient = aSource.mOrient;

  /* Copy over the transformation information. */
  mSpecifiedTransform = aSource.mSpecifiedTransform;
  
  /* Copy over transform origin. */
  mTransformOrigin[0] = aSource.mTransformOrigin[0];
  mTransformOrigin[1] = aSource.mTransformOrigin[1];
  mTransformOrigin[2] = aSource.mTransformOrigin[2];
  mPerspectiveOrigin[0] = aSource.mPerspectiveOrigin[0];
  mPerspectiveOrigin[1] = aSource.mPerspectiveOrigin[1];
  mChildPerspective = aSource.mChildPerspective;
  mBackfaceVisibility = aSource.mBackfaceVisibility;
  mTransformStyle = aSource.mTransformStyle;
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

  if (mFloats != aOther.mFloats) {
    // Changing which side we float on doesn't affect descendants directly
    NS_UpdateHint(hint,
       NS_SubtractHint(nsChangeHint_ReflowFrame,
                       NS_CombineHint(nsChangeHint_ClearDescendantIntrinsics,
                                      nsChangeHint_NeedDirtyReflow)));
  }

  // XXX the following is conservative, for now: changing float breaking shouldn't
  // necessarily require a repaint, reflow should suffice.
  if (mBreakType != aOther.mBreakType
      || mBreakBefore != aOther.mBreakBefore
      || mBreakAfter != aOther.mBreakAfter
      || mAppearance != aOther.mAppearance
      || mOrient != aOther.mOrient
      || mClipFlags != aOther.mClipFlags || !mClip.IsEqualInterior(aOther.mClip))
    NS_UpdateHint(hint, NS_CombineHint(nsChangeHint_ReflowFrame, nsChangeHint_RepaintFrame));

  if (mOpacity != aOther.mOpacity) {
    NS_UpdateHint(hint, nsChangeHint_UpdateOpacityLayer);
  }

  /* If we've added or removed the transform property, we need to reconstruct the frame to add
   * or remove the view object, and also to handle abs-pos and fixed-pos containers.
   */
  if (HasTransform() != aOther.HasTransform()) {
    NS_UpdateHint(hint, nsChangeHint_ReconstructFrame);
  }
  else if (HasTransform()) {
    /* Otherwise, if we've kept the property lying around and we already had a
     * transform, we need to see whether or not we've changed the transform.
     * If so, we need to do a reflow and a repaint. The reflow is to recompute
     * the overflow rect (which probably changed if the transform changed)
     * and to redraw within the bounds of that new overflow rect.
     */
    if (!mSpecifiedTransform != !aOther.mSpecifiedTransform ||
        (mSpecifiedTransform && *mSpecifiedTransform != *aOther.mSpecifiedTransform))
      NS_UpdateHint(hint, NS_CombineHint(nsChangeHint_ReflowFrame,
                                         nsChangeHint_UpdateTransformLayer));
    
    for (PRUint8 index = 0; index < 3; ++index)
      if (mTransformOrigin[index] != aOther.mTransformOrigin[index]) {
        NS_UpdateHint(hint, NS_CombineHint(nsChangeHint_ReflowFrame,
                                           nsChangeHint_RepaintFrame));
        break;
      }
    
    for (PRUint8 index = 0; index < 2; ++index)
      if (mPerspectiveOrigin[index] != aOther.mPerspectiveOrigin[index]) {
        NS_UpdateHint(hint, NS_CombineHint(nsChangeHint_ReflowFrame,
                                           nsChangeHint_RepaintFrame));
        break;
      }

    if (mChildPerspective != aOther.mChildPerspective)
      NS_UpdateHint(hint, NS_CombineHint(nsChangeHint_ReflowFrame,
                                         nsChangeHint_RepaintFrame));

    if (mBackfaceVisibility != aOther.mBackfaceVisibility)
      NS_UpdateHint(hint, nsChangeHint_RepaintFrame);

    if (mTransformStyle != aOther.mTransformStyle)
      NS_UpdateHint(hint, NS_CombineHint(nsChangeHint_ReflowFrame,
                                         nsChangeHint_RepaintFrame));
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

  return hint;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleDisplay::MaxDifference()
{
  // All the parts of FRAMECHANGE are present above in CalcDifference.
  return nsChangeHint(NS_STYLE_HINT_FRAMECHANGE | nsChangeHint_UpdateOpacityLayer |
                      nsChangeHint_UpdateTransformLayer);
}
#endif

// --------------------
// nsStyleVisibility
//

nsStyleVisibility::nsStyleVisibility(nsPresContext* aPresContext)
{
  MOZ_COUNT_CTOR(nsStyleVisibility);
  PRUint32 bidiOptions = aPresContext->GetBidi();
  if (GET_BIDI_OPTION_DIRECTION(bidiOptions) == IBMBIDI_TEXTDIRECTION_RTL)
    mDirection = NS_STYLE_DIRECTION_RTL;
  else
    mDirection = NS_STYLE_DIRECTION_LTR;

  nsAutoString language;
  aPresContext->Document()->GetContentLanguage(language);
  language.StripWhitespace();

  // Content-Language may be a comma-separated list of language codes,
  // in which case the HTML5 spec says to treat it as unknown
  if (!language.IsEmpty() &&
      language.FindChar(PRUnichar(',')) == kNotFound) {
    mLanguage = do_GetAtom(language);
  } else {
    // we didn't find a (usable) Content-Language, so we fall back
    // to whatever the presContext guessed from the charset
    mLanguage = aPresContext->GetLanguageFromCharset();
  }

  mVisible = NS_STYLE_VISIBILITY_VISIBLE;
  mPointerEvents = NS_STYLE_POINTER_EVENTS_AUTO;
}

nsStyleVisibility::nsStyleVisibility(const nsStyleVisibility& aSource)
{
  MOZ_COUNT_CTOR(nsStyleVisibility);
  mDirection = aSource.mDirection;
  mVisible = aSource.mVisible;
  mLanguage = aSource.mLanguage;
  mPointerEvents = aSource.mPointerEvents;
} 

nsChangeHint nsStyleVisibility::CalcDifference(const nsStyleVisibility& aOther) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (mDirection != aOther.mDirection) {
    NS_UpdateHint(hint, nsChangeHint_ReconstructFrame);
  } else if (mLanguage == aOther.mLanguage) {
    if (mVisible != aOther.mVisible) {
      if ((NS_STYLE_VISIBILITY_COLLAPSE == mVisible) ||
          (NS_STYLE_VISIBILITY_COLLAPSE == aOther.mVisible)) {
        NS_UpdateHint(hint, NS_STYLE_HINT_REFLOW);
      } else {
        NS_UpdateHint(hint, NS_STYLE_HINT_VISUAL);
      }
    }
  } else {
    NS_UpdateHint(hint, NS_STYLE_HINT_REFLOW);
  }
  return hint;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleVisibility::MaxDifference()
{
  return NS_STYLE_HINT_FRAMECHANGE;
}
#endif

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
    mContent.mString = nsnull;
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
                    "Tryingto do image tracking on non-image!");
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
    doc->RemoveImage(mContent.mImage);

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
    mContents(nsnull),
    mIncrements(nsnull),
    mResets(nsnull),
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
  for (PRUint32 i = 0; i < mContentCount; ++i) {
    if ((mContents[i].mType == eStyleContentType_Image) &&
        mContents[i].mContent.mImage) {
      mContents[i].UntrackImage(aContext);
    }
  }

  this->~nsStyleContent();
  aContext->FreeToShell(sizeof(nsStyleContent), this);
}

nsStyleContent::nsStyleContent(const nsStyleContent& aSource)
   :mMarkerOffset(),
    mContents(nsnull),
    mIncrements(nsnull),
    mResets(nsnull),
    mContentCount(0),
    mIncrementCount(0),
    mResetCount(0)

{
  MOZ_COUNT_CTOR(nsStyleContent);
  mMarkerOffset = aSource.mMarkerOffset;

  PRUint32 index;
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

  PRUint32 ix = mContentCount;
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

#ifdef DEBUG
/* static */
nsChangeHint nsStyleContent::MaxDifference()
{
  return NS_STYLE_HINT_FRAMECHANGE;
}
#endif

nsresult nsStyleContent::AllocateContents(PRUint32 aCount)
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
    mQuotes(nsnull)
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
    mQuotes(nsnull)
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
  static const PRUnichar initialQuotes[8] = {
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
    PRUint32 count = (mQuotesCount * 2);
    for (PRUint32 index = 0; index < count; index += 2) {
      aSource.GetQuotesAt(index, mQuotes[index], mQuotes[index + 1]);
    }
  }
}

nsChangeHint nsStyleQuotes::CalcDifference(const nsStyleQuotes& aOther) const
{
  // If the quotes implementation is ever going to change we might not need
  // a framechange here and a reflow should be sufficient.  See bug 35768.
  if (mQuotesCount == aOther.mQuotesCount) {
    PRUint32 ix = (mQuotesCount * 2);
    while (0 < ix--) {
      if (mQuotes[ix] != aOther.mQuotes[ix]) {
        return NS_STYLE_HINT_FRAMECHANGE;
      }
    }

    return NS_STYLE_HINT_NONE;
  }
  return NS_STYLE_HINT_FRAMECHANGE;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleQuotes::MaxDifference()
{
  return NS_STYLE_HINT_FRAMECHANGE;
}
#endif

// --------------------
// nsStyleTextReset
//

nsStyleTextReset::nsStyleTextReset(void) 
{ 
  MOZ_COUNT_CTOR(nsStyleTextReset);
  mVerticalAlign.SetIntValue(NS_STYLE_VERTICAL_ALIGN_BASELINE, eStyleUnit_Enumerated);
  mTextBlink = NS_STYLE_TEXT_BLINK_NONE;
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
    // Reflow for blink changes
    if (mTextBlink != aOther.mTextBlink) {
      return NS_STYLE_HINT_REFLOW;
    }

    PRUint8 lineStyle = GetDecorationStyle();
    PRUint8 otherLineStyle = aOther.GetDecorationStyle();
    if (mTextDecorationLine != aOther.mTextDecorationLine ||
        lineStyle != otherLineStyle) {
      // Reflow for decoration line style changes only to or from double or
      // wave because that may cause overflow area changes
      if (lineStyle == NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE ||
          lineStyle == NS_STYLE_TEXT_DECORATION_STYLE_WAVY ||
          otherLineStyle == NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE ||
          otherLineStyle == NS_STYLE_TEXT_DECORATION_STYLE_WAVY) {
        return NS_STYLE_HINT_REFLOW;
      }
      // Repaint for other style decoration lines because they must be in
      // default overflow rect
      return NS_STYLE_HINT_VISUAL;
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

#ifdef DEBUG
/* static */
nsChangeHint nsStyleTextReset::MaxDifference()
{
  return NS_STYLE_HINT_REFLOW;
}
#endif

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

  for (PRUint32 i = 0; i < lhs->Length(); ++i) {
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
  mTextTransform = NS_STYLE_TEXT_TRANSFORM_NONE;
  mWhiteSpace = NS_STYLE_WHITESPACE_NORMAL;
  mWordWrap = NS_STYLE_WORDWRAP_NORMAL;
  mHyphens = NS_STYLE_HYPHENS_MANUAL;
  mTextSizeAdjust = NS_STYLE_TEXT_SIZE_ADJUST_AUTO;

  mLetterSpacing.SetNormalValue();
  mLineHeight.SetNormalValue();
  mTextIndent.SetCoordValue(0);
  mWordSpacing = 0;

  mTextShadow = nsnull;
  mTabSize = NS_STYLE_TABSIZE_INITIAL;
}

nsStyleText::nsStyleText(const nsStyleText& aSource)
  : mTextAlign(aSource.mTextAlign),
    mTextTransform(aSource.mTextTransform),
    mWhiteSpace(aSource.mWhiteSpace),
    mWordWrap(aSource.mWordWrap),
    mHyphens(aSource.mHyphens),
    mTextSizeAdjust(aSource.mTextSizeAdjust),
    mTabSize(aSource.mTabSize),
    mLetterSpacing(aSource.mLetterSpacing),
    mLineHeight(aSource.mLineHeight),
    mTextIndent(aSource.mTextIndent),
    mWordSpacing(aSource.mWordSpacing),
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
  if (NewlineIsSignificant() != aOther.NewlineIsSignificant()) {
    // This may require construction of suppressed text frames
    return NS_STYLE_HINT_FRAMECHANGE;
  }

  if ((mTextAlign != aOther.mTextAlign) ||
      (mTextTransform != aOther.mTextTransform) ||
      (mWhiteSpace != aOther.mWhiteSpace) ||
      (mWordWrap != aOther.mWordWrap) ||
      (mHyphens != aOther.mHyphens) ||
      (mTextSizeAdjust != aOther.mTextSizeAdjust) ||
      (mLetterSpacing != aOther.mLetterSpacing) ||
      (mLineHeight != aOther.mLineHeight) ||
      (mTextIndent != aOther.mTextIndent) ||
      (mWordSpacing != aOther.mWordSpacing) ||
      (mTabSize != aOther.mTabSize))
    return NS_STYLE_HINT_REFLOW;

  return CalcShadowDifference(mTextShadow, aOther.mTextShadow);
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleText::MaxDifference()
{
  return NS_STYLE_HINT_FRAMECHANGE;
}
#endif

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
  SetImage(nsnull);
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

  mCursor = NS_STYLE_CURSOR_AUTO; // fix for bugzilla bug 51113

  mCursorArrayLength = 0;
  mCursorArray = nsnull;
}

nsStyleUserInterface::nsStyleUserInterface(const nsStyleUserInterface& aSource) :
  mUserInput(aSource.mUserInput),
  mUserModify(aSource.mUserModify),
  mUserFocus(aSource.mUserFocus),
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
  
  if ((mUserInput != aOther.mUserInput) &&
      ((NS_STYLE_USER_INPUT_NONE == mUserInput) || 
       (NS_STYLE_USER_INPUT_NONE == aOther.mUserInput))) {
    NS_UpdateHint(hint, NS_STYLE_HINT_FRAMECHANGE);
  }

  // ignore mUserFocus

  return hint;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleUserInterface::MaxDifference()
{
  return nsChangeHint(nsChangeHint_UpdateCursor | NS_STYLE_HINT_FRAMECHANGE);
}
#endif

void
nsStyleUserInterface::CopyCursorArrayFrom(const nsStyleUserInterface& aSource)
{
  mCursorArray = nsnull;
  mCursorArrayLength = 0;
  if (aSource.mCursorArrayLength) {
    mCursorArray = new nsCursorImage[aSource.mCursorArrayLength];
    if (mCursorArray) {
      mCursorArrayLength = aSource.mCursorArrayLength;
      for (PRUint32 i = 0; i < mCursorArrayLength; ++i)
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

#ifdef DEBUG
/* static */
nsChangeHint nsStyleUIReset::MaxDifference()
{
  return NS_STYLE_HINT_FRAMECHANGE;
}
#endif

