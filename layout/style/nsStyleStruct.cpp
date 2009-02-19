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
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Michael Ventnor <m.ventnor@gmail.com>
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
#include "nsIDeviceContext.h"
#include "nsIStyleRule.h"
#include "nsCRT.h"

#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsHTMLReflowState.h"
#include "prenv.h"

#include "nsBidiUtils.h"

#include "imgIRequest.h"
#include "prlog.h"

// Make sure we have enough bits in NS_STYLE_INHERIT_MASK.
PR_STATIC_ASSERT((((1 << nsStyleStructID_Length) - 1) &
                  ~(NS_STYLE_INHERIT_MASK)) == 0);

inline PRBool IsFixedUnit(nsStyleUnit aUnit, PRBool aEnumOK)
{
  return PRBool((aUnit == eStyleUnit_Coord) || 
                (aEnumOK && (aUnit == eStyleUnit_Enumerated)));
}

static PRBool EqualURIs(nsIURI *aURI1, nsIURI *aURI2)
{
  PRBool eq;
  return aURI1 == aURI2 ||    // handle null==null, and optimize
         (aURI1 && aURI2 &&
          NS_SUCCEEDED(aURI1->Equals(aURI2, &eq)) && // not equal on fail
          eq);
}

static PRBool EqualURIs(nsCSSValue::URL *aURI1, nsCSSValue::URL *aURI2)
{
  return aURI1 == aURI2 ||    // handle null==null, and optimize
         (aURI1 && aURI2 && aURI1->URIEquals(*aURI2));
}

static PRBool EqualImages(imgIRequest *aImage1, imgIRequest* aImage2)
{
  if (aImage1 == aImage2) {
    return PR_TRUE;
  }

  if (!aImage1 || !aImage2) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIURI> uri1, uri2;
  aImage1->GetURI(getter_AddRefs(uri1));
  aImage2->GetURI(getter_AddRefs(uri2));
  return EqualURIs(uri1, uri2);
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
  mSize = mFont.size = nsStyleFont::ZoomText(aPresContext, mFont.size);
#ifdef MOZ_MATHML
  mScriptUnconstrainedSize = mSize;
  mScriptMinSize = aPresContext->TwipsToAppUnits(
      NS_POINTS_TO_TWIPS(NS_MATHML_DEFAULT_SCRIPT_MIN_SIZE_PT));
  mScriptLevel = 0;
  mScriptSizeMultiplier = NS_MATHML_DEFAULT_SCRIPT_SIZE_MULTIPLIER;
#endif
}

nsStyleFont::nsStyleFont(const nsStyleFont& aSrc)
  : mFont(aSrc.mFont)
  , mSize(aSrc.mSize)
  , mGenericID(aSrc.mGenericID)
#ifdef MOZ_MATHML
  , mScriptLevel(aSrc.mScriptLevel)
  , mScriptUnconstrainedSize(aSrc.mScriptUnconstrainedSize)
  , mScriptMinSize(aSrc.mScriptMinSize)
  , mScriptSizeMultiplier(aSrc.mScriptSizeMultiplier)
#endif
{
}

nsStyleFont::nsStyleFont(nsPresContext* aPresContext)
  : mFont(*(aPresContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID))),
    mGenericID(kGenericFont_NONE)
{
  mSize = mFont.size = nsStyleFont::ZoomText(aPresContext, mFont.size);
#ifdef MOZ_MATHML
  mScriptUnconstrainedSize = mSize;
  mScriptMinSize = aPresContext->TwipsToAppUnits(
      NS_POINTS_TO_TWIPS(NS_MATHML_DEFAULT_SCRIPT_MIN_SIZE_PT));
  mScriptLevel = 0;
  mScriptSizeMultiplier = NS_MATHML_DEFAULT_SCRIPT_SIZE_MULTIPLIER;
#endif
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
      (aFont1.familyNameQuirks == aFont2.familyNameQuirks) &&
      (aFont1.weight == aFont2.weight) &&
      (aFont1.stretch == aFont2.stretch) &&
      (aFont1.name == aFont2.name)) {
    if ((aFont1.decorations == aFont2.decorations)) {
      return NS_STYLE_HINT_NONE;
    }
    return NS_STYLE_HINT_VISUAL;
  }
  return NS_STYLE_HINT_REFLOW;
}

static PRBool IsFixedData(const nsStyleSides& aSides, PRBool aEnumOK)
{
  NS_FOR_CSS_SIDES(side) {
    if (!IsFixedUnit(aSides.GetUnit(side), aEnumOK))
      return PR_FALSE;
  }
  return PR_TRUE;
}

static nscoord CalcCoord(const nsStyleCoord& aCoord, 
                         const nscoord* aEnumTable, 
                         PRInt32 aNumEnums)
{
  switch (aCoord.GetUnit()) {
    case eStyleUnit_Coord:
      return aCoord.GetCoordValue();
    case eStyleUnit_Enumerated:
      if (nsnull != aEnumTable) {
        PRInt32 value = aCoord.GetIntValue();
        if ((0 <= value) && (value < aNumEnums)) {
          return aEnumTable[aCoord.GetIntValue()];
        }
      }
      break;
    default:
      NS_ERROR("bad unit type");
      break;
  }
  return 0;
}

nsStyleMargin::nsStyleMargin() {
  nsStyleCoord zero(0);
  NS_FOR_CSS_SIDES(side) {
    mMargin.Set(side, zero);
  }
  mHasCachedMargin = PR_FALSE;
}

nsStyleMargin::nsStyleMargin(const nsStyleMargin& aSrc) {
  mMargin = aSrc.mMargin;
  mHasCachedMargin = PR_FALSE;
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
  if (IsFixedData(mMargin, PR_FALSE)) {
    NS_FOR_CSS_SIDES(side) {
      mCachedMargin.side(side) = CalcCoord(mMargin.Get(side), nsnull, 0);
    }
    mHasCachedMargin = PR_TRUE;
  }
  else
    mHasCachedMargin = PR_FALSE;
}

nsChangeHint nsStyleMargin::CalcDifference(const nsStyleMargin& aOther) const
{
  if (mMargin == aOther.mMargin) {
    return NS_STYLE_HINT_NONE;
  }
  return NS_STYLE_HINT_REFLOW;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleMargin::MaxDifference()
{
  return NS_STYLE_HINT_REFLOW;
}
#endif

nsStylePadding::nsStylePadding() {
  nsStyleCoord zero(0);
  NS_FOR_CSS_SIDES(side) {
    mPadding.Set(side, zero);
  }
  mHasCachedPadding = PR_FALSE;
}

nsStylePadding::nsStylePadding(const nsStylePadding& aSrc) {
  mPadding = aSrc.mPadding;
  mHasCachedPadding = PR_FALSE;
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
  if (IsFixedData(mPadding, PR_FALSE)) {
    NS_FOR_CSS_SIDES(side) {
      mCachedPadding.side(side) = CalcCoord(mPadding.Get(side), nsnull, 0);
    }
    mHasCachedPadding = PR_TRUE;
  }
  else
    mHasCachedPadding = PR_FALSE;
}

nsChangeHint nsStylePadding::CalcDifference(const nsStylePadding& aOther) const
{
  if (mPadding == aOther.mPadding) {
    return NS_STYLE_HINT_NONE;
  }
  return NS_STYLE_HINT_REFLOW;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStylePadding::MaxDifference()
{
  return NS_STYLE_HINT_REFLOW;
}
#endif

nsStyleBorder::nsStyleBorder(nsPresContext* aPresContext)
  : mHaveBorderImageWidth(PR_FALSE),
    mComputedBorder(0, 0, 0, 0),
    mBorderImage(nsnull)
{
  nscoord medium =
    (aPresContext->GetBorderWidthTable())[NS_STYLE_BORDER_WIDTH_MEDIUM];
  NS_FOR_CSS_SIDES(side) {
    mBorder.side(side) = medium;
    mBorderStyle[side] = NS_STYLE_BORDER_STYLE_NONE | BORDER_COLOR_FOREGROUND;
    mBorderColor[side] = NS_RGB(0, 0, 0);
  }
  NS_FOR_CSS_HALF_CORNERS(corner) {
    mBorderRadius.Set(corner, nsStyleCoord(0));
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
nsBorderColors::Clone(PRBool aDeep) const
{
  nsBorderColors* result = new nsBorderColors(mColor);
  if (NS_UNLIKELY(!result))
    return result;
  if (aDeep)
    NS_CSS_CLONE_LIST_MEMBER(nsBorderColors, this, mNext, result, (PR_FALSE));
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
  this->~nsStyleBorder();
  aContext->FreeToShell(sizeof(nsStyleBorder), this);
}


nsChangeHint nsStyleBorder::CalcDifference(const nsStyleBorder& aOther) const
{
  nsChangeHint shadowDifference =
    CalcShadowDifference(mBoxShadow, aOther.mBoxShadow);

  // Note that differences in mBorder don't affect rendering (which should only
  // use mComputedBorder), so don't need to be tested for here.
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

PRBool
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

nsStyleOutline::nsStyleOutline(nsPresContext* aPresContext)
{
  // spacing values not inherited
  nsStyleCoord zero(0);
  NS_FOR_CSS_HALF_CORNERS(corner) {
    mOutlineRadius.Set(corner, zero);
  }

  mOutlineOffset = 0;

  mOutlineWidth = nsStyleCoord(NS_STYLE_BORDER_WIDTH_MEDIUM, eStyleUnit_Enumerated);
  mOutlineStyle = NS_STYLE_BORDER_STYLE_NONE;
  mOutlineColor = NS_RGB(0, 0, 0);

  mHasCachedOutline = PR_FALSE;
  mTwipsPerPixel = aPresContext->DevPixelsToAppUnits(1);
}

nsStyleOutline::nsStyleOutline(const nsStyleOutline& aSrc) {
  memcpy((nsStyleOutline*)this, &aSrc, sizeof(nsStyleOutline));
}

void 
nsStyleOutline::RecalcData(nsPresContext* aContext)
{
  if (NS_STYLE_BORDER_STYLE_NONE == GetOutlineStyle()) {
    mCachedOutlineWidth = 0;
    mHasCachedOutline = PR_TRUE;
  } else if (IsFixedUnit(mOutlineWidth.GetUnit(), PR_TRUE)) {
    mCachedOutlineWidth =
      CalcCoord(mOutlineWidth, aContext->GetBorderWidthTable(), 3);
    mCachedOutlineWidth =
      NS_ROUND_BORDER_TO_PIXELS(mCachedOutlineWidth, mTwipsPerPixel);
    mHasCachedOutline = PR_TRUE;
  }
  else
    mHasCachedOutline = PR_FALSE;
}

nsChangeHint nsStyleOutline::CalcDifference(const nsStyleOutline& aOther) const
{
  PRBool outlineWasVisible =
    mCachedOutlineWidth > 0 && mOutlineStyle != NS_STYLE_BORDER_STYLE_NONE;
  PRBool outlineIsVisible = 
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
}

nsStyleList::~nsStyleList() 
{
}

nsStyleList::nsStyleList(const nsStyleList& aSource)
  : mListStyleType(aSource.mListStyleType),
    mListStylePosition(aSource.mListStylePosition),
    mListStyleImage(aSource.mListStyleImage),
    mImageRegion(aSource.mImageRegion)
{
}

nsChangeHint nsStyleList::CalcDifference(const nsStyleList& aOther) const
{
  if (mListStylePosition != aOther.mListStylePosition)
    return NS_STYLE_HINT_FRAMECHANGE;
  if (EqualImages(mListStyleImage, aOther.mListStyleImage) &&
      mListStyleType == aOther.mListStyleType) {
    if (mImageRegion == aOther.mImageRegion)
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
  mBoxAlign  = NS_STYLE_BOX_ALIGN_STRETCH;
  mBoxDirection = NS_STYLE_BOX_DIRECTION_NORMAL;
  mBoxFlex = 0.0f;
  mBoxOrient = NS_STYLE_BOX_ORIENT_HORIZONTAL;
  mBoxPack   = NS_STYLE_BOX_PACK_START;
  mBoxOrdinal = 1;
  mStretchStack = PR_TRUE;
}

nsStyleXUL::~nsStyleXUL() 
{
}

nsStyleXUL::nsStyleXUL(const nsStyleXUL& aSource)
{
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
  mColumnCount = NS_STYLE_COLUMN_COUNT_AUTO;
  mColumnWidth.SetAutoValue();
  mColumnGap.SetNormalValue();

  mColumnRuleWidth = (aPresContext->GetBorderWidthTable())[NS_STYLE_BORDER_WIDTH_MEDIUM];
  mColumnRuleStyle = NS_STYLE_BORDER_STYLE_NONE;
  mColumnRuleColor = NS_RGB(0, 0, 0);
  mColumnRuleColorIsForeground = PR_TRUE;

  mTwipsPerPixel = aPresContext->AppUnitsPerDevPixel();
}

nsStyleColumn::~nsStyleColumn() 
{
}

nsStyleColumn::nsStyleColumn(const nsStyleColumn& aSource)
{
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

#ifdef MOZ_SVG
// --------------------
// nsStyleSVG
//
nsStyleSVG::nsStyleSVG() 
{
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
    mPointerEvents           = NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED;
    mShapeRendering          = NS_STYLE_SHAPE_RENDERING_AUTO;
    mStrokeLinecap           = NS_STYLE_STROKE_LINECAP_BUTT;
    mStrokeLinejoin          = NS_STYLE_STROKE_LINEJOIN_MITER;
    mTextAnchor              = NS_STYLE_TEXT_ANCHOR_START;
    mTextRendering           = NS_STYLE_TEXT_RENDERING_AUTO;
}

nsStyleSVG::~nsStyleSVG() 
{
  delete [] mStrokeDasharray;
}

nsStyleSVG::nsStyleSVG(const nsStyleSVG& aSource)
{
  //memcpy((nsStyleSVG*)this, &aSource, sizeof(nsStyleSVG));

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
  mPointerEvents = aSource.mPointerEvents;
  mShapeRendering = aSource.mShapeRendering;
  mStrokeLinecap = aSource.mStrokeLinecap;
  mStrokeLinejoin = aSource.mStrokeLinejoin;
  mTextAnchor = aSource.mTextAnchor;
  mTextRendering = aSource.mTextRendering;
}

static PRBool PaintURIChanged(const nsStyleSVGPaint& aPaint1,
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
}

nsStyleSVGReset::nsStyleSVGReset(const nsStyleSVGReset& aSource)
{
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

PRBool nsStyleSVGPaint::operator==(const nsStyleSVGPaint& aOther) const
{
  if (mType != aOther.mType)
    return PR_FALSE;
  if (mType == eStyleSVGPaintType_Server)
    return EqualURIs(mPaint.mPaintServer, aOther.mPaint.mPaintServer) &&
           mFallbackColor == aOther.mFallbackColor;
  if (mType == eStyleSVGPaintType_None)
    return PR_TRUE;
  return mPaint.mColor == aOther.mPaint.mColor;
}

#endif // MOZ_SVG


// --------------------
// nsStylePosition
//
nsStylePosition::nsStylePosition(void) 
{ 
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
}

nsStylePosition::nsStylePosition(const nsStylePosition& aSource)
{
  memcpy((nsStylePosition*)this, &aSource, sizeof(nsStylePosition));
}

nsChangeHint nsStylePosition::CalcDifference(const nsStylePosition& aOther) const
{
  if (mZIndex != aOther.mZIndex) {
    return NS_STYLE_HINT_REFLOW;
  }
  
  if ((mOffset == aOther.mOffset) &&
      (mWidth == aOther.mWidth) &&
      (mMinWidth == aOther.mMinWidth) &&
      (mMaxWidth == aOther.mMaxWidth) &&
      (mHeight == aOther.mHeight) &&
      (mMinHeight == aOther.mMinHeight) &&
      (mMaxHeight == aOther.mMaxHeight) &&
      (mBoxSizing == aOther.mBoxSizing))
    return NS_STYLE_HINT_NONE;
  
  return nsChangeHint_ReflowFrame;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStylePosition::MaxDifference()
{
  return NS_STYLE_HINT_REFLOW;
}
#endif

// --------------------
// nsStyleTable
//

nsStyleTable::nsStyleTable() 
{ 
  // values not inherited
  mLayoutStrategy = NS_STYLE_TABLE_LAYOUT_AUTO;
  mCols  = NS_STYLE_TABLE_COLS_NONE;
  mFrame = NS_STYLE_TABLE_FRAME_NONE;
  mRules = NS_STYLE_TABLE_RULES_NONE;
  mSpan = 1;
}

nsStyleTable::~nsStyleTable(void) 
{ 
}

nsStyleTable::nsStyleTable(const nsStyleTable& aSource)
{
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
}

nsStyleTableBorder::nsStyleTableBorder(const nsStyleTableBorder& aSource)
{
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
  mColor = aPresContext->DefaultColor();
}

nsStyleColor::nsStyleColor(const nsStyleColor& aSource)
{
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
// nsStyleBackground
//

nsStyleBackground::nsStyleBackground()
  : mBackgroundFlags(NS_STYLE_BG_IMAGE_NONE),
    mBackgroundAttachment(NS_STYLE_BG_ATTACHMENT_SCROLL),
    mBackgroundClip(NS_STYLE_BG_CLIP_BORDER),
    mBackgroundInlinePolicy(NS_STYLE_BG_INLINE_POLICY_CONTINUOUS),
    mBackgroundOrigin(NS_STYLE_BG_ORIGIN_PADDING),
    mBackgroundRepeat(NS_STYLE_BG_REPEAT_XY),
    mBackgroundColor(NS_RGBA(0, 0, 0, 0))
{
}

nsStyleBackground::nsStyleBackground(const nsStyleBackground& aSource)
  : mBackgroundFlags(aSource.mBackgroundFlags),
    mBackgroundAttachment(aSource.mBackgroundAttachment),
    mBackgroundClip(aSource.mBackgroundClip),
    mBackgroundInlinePolicy(aSource.mBackgroundInlinePolicy),
    mBackgroundOrigin(aSource.mBackgroundOrigin),
    mBackgroundRepeat(aSource.mBackgroundRepeat),
    mBackgroundXPosition(aSource.mBackgroundXPosition),
    mBackgroundYPosition(aSource.mBackgroundYPosition),
    mBackgroundColor(aSource.mBackgroundColor),
    mBackgroundImage(aSource.mBackgroundImage)
{
}

nsStyleBackground::~nsStyleBackground()
{
}

nsChangeHint nsStyleBackground::CalcDifference(const nsStyleBackground& aOther) const
{
  if ((mBackgroundAttachment == aOther.mBackgroundAttachment) &&
      (mBackgroundFlags == aOther.mBackgroundFlags) &&
      (mBackgroundRepeat == aOther.mBackgroundRepeat) &&
      (mBackgroundColor == aOther.mBackgroundColor) &&
      (mBackgroundClip == aOther.mBackgroundClip) &&
      (mBackgroundInlinePolicy == aOther.mBackgroundInlinePolicy) &&
      (mBackgroundOrigin == aOther.mBackgroundOrigin) &&
      EqualImages(mBackgroundImage, aOther.mBackgroundImage) &&
      ((!(mBackgroundFlags & NS_STYLE_BG_X_POSITION_PERCENT) ||
       (mBackgroundXPosition.mFloat == aOther.mBackgroundXPosition.mFloat)) &&
       (!(mBackgroundFlags & NS_STYLE_BG_X_POSITION_LENGTH) ||
        (mBackgroundXPosition.mCoord == aOther.mBackgroundXPosition.mCoord))) &&
      ((!(mBackgroundFlags & NS_STYLE_BG_Y_POSITION_PERCENT) ||
       (mBackgroundYPosition.mFloat == aOther.mBackgroundYPosition.mFloat)) &&
       (!(mBackgroundFlags & NS_STYLE_BG_Y_POSITION_LENGTH) ||
        (mBackgroundYPosition.mCoord == aOther.mBackgroundYPosition.mCoord))))
    return NS_STYLE_HINT_NONE;
  return NS_STYLE_HINT_VISUAL;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleBackground::MaxDifference()
{
  return NS_STYLE_HINT_VISUAL;
}
#endif

PRBool nsStyleBackground::HasFixedBackground() const
{
  return mBackgroundAttachment == NS_STYLE_BG_ATTACHMENT_FIXED &&
         mBackgroundImage;
}

// --------------------
// nsStyleDisplay
//

nsStyleDisplay::nsStyleDisplay()
{
  mAppearance = NS_THEME_NONE;
  mDisplay = NS_STYLE_DISPLAY_INLINE;
  mOriginalDisplay = NS_STYLE_DISPLAY_NONE;
  mPosition = NS_STYLE_POSITION_STATIC;
  mFloats = NS_STYLE_FLOAT_NONE;
  mBreakType = NS_STYLE_CLEAR_NONE;
  mBreakBefore = PR_FALSE;
  mBreakAfter = PR_FALSE;
  mOverflowX = NS_STYLE_OVERFLOW_VISIBLE;
  mOverflowY = NS_STYLE_OVERFLOW_VISIBLE;
  mClipFlags = NS_STYLE_CLIP_AUTO;
  mClip.SetRect(0,0,0,0);
  mOpacity = 1.0f;
  mTransformPresent = PR_FALSE; // No transform
  mTransformOrigin[0].SetPercentValue(0.5f); // Transform is centered on origin
  mTransformOrigin[1].SetPercentValue(0.5f); 
}

nsStyleDisplay::nsStyleDisplay(const nsStyleDisplay& aSource)
{
  mAppearance = aSource.mAppearance;
  mDisplay = aSource.mDisplay;
  mOriginalDisplay = aSource.mOriginalDisplay;
  mBinding = aSource.mBinding;
  mPosition = aSource.mPosition;
  mFloats = aSource.mFloats;
  mBreakType = aSource.mBreakType;
  mBreakBefore = aSource.mBreakBefore;
  mBreakAfter = aSource.mBreakAfter;
  mOverflowX = aSource.mOverflowX;
  mOverflowY = aSource.mOverflowY;
  mClipFlags = aSource.mClipFlags;
  mClip = aSource.mClip;
  mOpacity = aSource.mOpacity;

  /* Copy over the transformation information. */
  mTransformPresent = aSource.mTransformPresent;
  if (mTransformPresent)
    mTransform = aSource.mTransform;
  
  /* Copy over transform origin. */
  mTransformOrigin[0] = aSource.mTransformOrigin[0];
  mTransformOrigin[1] = aSource.mTransformOrigin[1];
}

nsChangeHint nsStyleDisplay::CalcDifference(const nsStyleDisplay& aOther) const
{
  nsChangeHint hint = nsChangeHint(0);

  if (!EqualURIs(mBinding, aOther.mBinding)
      || mPosition != aOther.mPosition
      || mDisplay != aOther.mDisplay
      || (mFloats == NS_STYLE_FLOAT_NONE) != (aOther.mFloats == NS_STYLE_FLOAT_NONE)
      || mOverflowX != aOther.mOverflowX
      || mOverflowY != aOther.mOverflowY)
    NS_UpdateHint(hint, nsChangeHint_ReconstructFrame);

  if (mFloats != aOther.mFloats)
    NS_UpdateHint(hint, nsChangeHint_ReflowFrame);    

  if (mClipFlags != aOther.mClipFlags || mClip != aOther.mClip) {
    NS_UpdateHint(hint, nsChangeHint_ReflowFrame);
  }
  // XXX the following is conservative, for now: changing float breaking shouldn't
  // necessarily require a repaint, reflow should suffice.
  if (mBreakType != aOther.mBreakType
      || mBreakBefore != aOther.mBreakBefore
      || mBreakAfter != aOther.mBreakAfter
      || mAppearance != aOther.mAppearance)
    NS_UpdateHint(hint, NS_CombineHint(nsChangeHint_ReflowFrame, nsChangeHint_RepaintFrame));

  if (mOpacity != aOther.mOpacity)
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);

  /* If we've added or removed the transform property, we need to reconstruct the frame to add
   * or remove the view object, and also to handle abs-pos and fixed-pos containers.
   */
  if (mTransformPresent != aOther.mTransformPresent) {
    NS_UpdateHint(hint, nsChangeHint_ReconstructFrame);
  }
  else if (mTransformPresent) {
    /* Otherwise, if we've kept the property lying around and we already had a
     * transform, we need to see whether or not we've changed the transform.
     * If so, we need to do a reflow and a repaint. The reflow is to recompute
     * the overflow rect (which probably changed if the transform changed)
     * and to redraw within the bounds of that new overflow rect.
     */
    if (mTransform != aOther.mTransform)
      NS_UpdateHint(hint, NS_CombineHint(nsChangeHint_ReflowFrame,
                                         nsChangeHint_RepaintFrame));
    
    for (PRUint8 index = 0; index < 2; ++index)
      if (mTransformOrigin[index] != aOther.mTransformOrigin[index]) {
        NS_UpdateHint(hint, NS_CombineHint(nsChangeHint_ReflowFrame,
                                           nsChangeHint_RepaintFrame));
        break;
      }
  }
  
  
  return hint;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleDisplay::MaxDifference()
{
  // All the parts of FRAMECHANGE are present above in CalcDifference.
  return NS_STYLE_HINT_FRAMECHANGE;
}
#endif

// --------------------
// nsStyleVisibility
//

nsStyleVisibility::nsStyleVisibility(nsPresContext* aPresContext)
{
  PRUint32 bidiOptions = aPresContext->GetBidi();
  if (GET_BIDI_OPTION_DIRECTION(bidiOptions) == IBMBIDI_TEXTDIRECTION_RTL)
    mDirection = NS_STYLE_DIRECTION_RTL;
  else
    mDirection = NS_STYLE_DIRECTION_LTR;

  mLangGroup = aPresContext->GetLangGroup();
  mVisible = NS_STYLE_VISIBILITY_VISIBLE;
}

nsStyleVisibility::nsStyleVisibility(const nsStyleVisibility& aSource)
{
  mDirection = aSource.mDirection;
  mVisible = aSource.mVisible;
  mLangGroup = aSource.mLangGroup;
} 

nsChangeHint nsStyleVisibility::CalcDifference(const nsStyleVisibility& aOther) const
{
  if ((mDirection == aOther.mDirection) &&
      (mLangGroup == aOther.mLangGroup)) {
    if ((mVisible == aOther.mVisible)) {
      return NS_STYLE_HINT_NONE;
    }
    if ((NS_STYLE_VISIBILITY_COLLAPSE == mVisible) || 
        (NS_STYLE_VISIBILITY_COLLAPSE == aOther.mVisible)) {
      return NS_STYLE_HINT_REFLOW;
    }
    return NS_STYLE_HINT_VISUAL;
  }
  return NS_STYLE_HINT_REFLOW;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleVisibility::MaxDifference()
{
  return NS_STYLE_HINT_REFLOW;
}
#endif

nsStyleContentData::~nsStyleContentData()
{
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

PRBool nsStyleContentData::operator==(const nsStyleContentData& aOther) const
{
  if (mType != aOther.mType)
    return PR_FALSE;
  if (mType == eStyleContentType_Image) {
    if (!mContent.mImage || !aOther.mContent.mImage)
      return mContent.mImage == aOther.mContent.mImage;
    PRBool eq;
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
  return nsCRT::strcmp(mContent.mString, aOther.mContent.mString) == 0;
}

//-----------------------
// nsStyleContent
//

nsStyleContent::nsStyleContent(void)
  : mMarkerOffset(),
    mContentCount(0),
    mContents(nsnull),
    mIncrementCount(0),
    mIncrements(nsnull),
    mResetCount(0),
    mResets(nsnull)
{
  mMarkerOffset.SetAutoValue();
}

nsStyleContent::~nsStyleContent(void)
{
  DELETE_ARRAY_IF(mContents);
  DELETE_ARRAY_IF(mIncrements);
  DELETE_ARRAY_IF(mResets);
}

nsStyleContent::nsStyleContent(const nsStyleContent& aSource)
   :mMarkerOffset(),
    mContentCount(0),
    mContents(nsnull),
    mIncrementCount(0),
    mIncrements(nsnull),
    mResetCount(0),
    mResets(nsnull)

{
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
  SetInitial();
}

nsStyleQuotes::~nsStyleQuotes(void)
{
  DELETE_ARRAY_IF(mQuotes);
}

nsStyleQuotes::nsStyleQuotes(const nsStyleQuotes& aSource)
  : mQuotesCount(0),
    mQuotes(nsnull)
{
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
  mVerticalAlign.SetIntValue(NS_STYLE_VERTICAL_ALIGN_BASELINE, eStyleUnit_Enumerated);
  mTextDecoration = NS_STYLE_TEXT_DECORATION_NONE;
  mUnicodeBidi = NS_STYLE_UNICODE_BIDI_NORMAL;
}

nsStyleTextReset::nsStyleTextReset(const nsStyleTextReset& aSource) 
{ 
  memcpy((nsStyleTextReset*)this, &aSource, sizeof(nsStyleTextReset));
}

nsStyleTextReset::~nsStyleTextReset(void) { }

nsChangeHint nsStyleTextReset::CalcDifference(const nsStyleTextReset& aOther) const
{
  if (mVerticalAlign == aOther.mVerticalAlign
      && mUnicodeBidi == aOther.mUnicodeBidi) {
    if (mTextDecoration != aOther.mTextDecoration) {
      // Reflow for blink changes, repaint for others
      return
        (mTextDecoration & NS_STYLE_TEXT_DECORATION_BLINK) ==
        (aOther.mTextDecoration & NS_STYLE_TEXT_DECORATION_BLINK) ?
          NS_STYLE_HINT_VISUAL : NS_STYLE_HINT_REFLOW;
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

// --------------------
// nsCSSShadowArray
// nsCSSShadowItem
//

nsrefcnt
nsCSSShadowArray::Release()
{
  if (mRefCnt == PR_UINT32_MAX) {
    NS_WARNING("refcount overflow, leaking object");
    return mRefCnt;
  }
  mRefCnt--;
  if (mRefCnt == 0) {
    delete this;
    return 0;
  }
  return mRefCnt;
}

// Allowed to return one of NS_STYLE_HINT_NONE, NS_STYLE_HINT_REFLOW
// or NS_STYLE_HINT_VISUAL. Currently we just return NONE or REFLOW, though.
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
  mTextAlign = NS_STYLE_TEXT_ALIGN_DEFAULT;
  mTextTransform = NS_STYLE_TEXT_TRANSFORM_NONE;
  mWhiteSpace = NS_STYLE_WHITESPACE_NORMAL;
  mWordWrap = NS_STYLE_WORDWRAP_NORMAL;

  mLetterSpacing.SetNormalValue();
  mLineHeight.SetNormalValue();
  mTextIndent.SetCoordValue(0);
  mWordSpacing = 0;

  mTextShadow = nsnull;
}

nsStyleText::nsStyleText(const nsStyleText& aSource)
  : mTextAlign(aSource.mTextAlign),
    mTextTransform(aSource.mTextTransform),
    mWhiteSpace(aSource.mWhiteSpace),
    mWordWrap(aSource.mWordWrap),
    mLetterSpacing(aSource.mLetterSpacing),
    mLineHeight(aSource.mLineHeight),
    mTextIndent(aSource.mTextIndent),
    mWordSpacing(aSource.mWordSpacing),
    mTextShadow(aSource.mTextShadow)
{ }

nsStyleText::~nsStyleText(void) { }

nsChangeHint nsStyleText::CalcDifference(const nsStyleText& aOther) const
{
  if ((mTextAlign != aOther.mTextAlign) ||
      (mTextTransform != aOther.mTextTransform) ||
      (mWhiteSpace != aOther.mWhiteSpace) ||
      (mWordWrap != aOther.mWordWrap) ||
      (mLetterSpacing != aOther.mLetterSpacing) ||
      (mLineHeight != aOther.mLineHeight) ||
      (mTextIndent != aOther.mTextIndent) ||
      (mWordSpacing != aOther.mWordSpacing))
    return NS_STYLE_HINT_REFLOW;

  return CalcShadowDifference(mTextShadow, aOther.mTextShadow);
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleText::MaxDifference()
{
  return NS_STYLE_HINT_REFLOW;
}
#endif

//-----------------------
// nsStyleUserInterface
//

nsCursorImage::nsCursorImage()
  : mHaveHotspot(PR_FALSE)
  , mHotspotX(0.0f)
  , mHotspotY(0.0f)
{
}

nsStyleUserInterface::nsStyleUserInterface(void) 
{ 
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
  CopyCursorArrayFrom(aSource);
}

nsStyleUserInterface::~nsStyleUserInterface(void) 
{ 
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
  mUserSelect = NS_STYLE_USER_SELECT_AUTO;
  mForceBrokenImageIcon = 0;
  mIMEMode = NS_STYLE_IME_MODE_AUTO;
  mWindowShadow = NS_STYLE_WINDOW_SHADOW_DEFAULT;
}

nsStyleUIReset::nsStyleUIReset(const nsStyleUIReset& aSource) 
{
  mUserSelect = aSource.mUserSelect;
  mForceBrokenImageIcon = aSource.mForceBrokenImageIcon;
  mIMEMode = aSource.mIMEMode;
  mWindowShadow = aSource.mWindowShadow;
}

nsStyleUIReset::~nsStyleUIReset(void) 
{ 
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

