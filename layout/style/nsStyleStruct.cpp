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
#include "nsStyleConsts.h"
#include "nsString.h"
#include "nsUnitConversion.h"
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

inline PRBool IsFixedUnit(nsStyleUnit aUnit, PRBool aEnumOK)
{
  return PRBool((aUnit == eStyleUnit_Null) || 
                (aUnit == eStyleUnit_Coord) || 
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

// --------------------
// nsStyleFont
//
nsStyleFont::nsStyleFont()
  : mFlags(NS_STYLE_FONT_DEFAULT),
    mFont(nsnull, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
          NS_FONT_WEIGHT_NORMAL, NS_FONT_DECORATION_NONE, 0),
    mSize(0)
{ }

nsStyleFont::nsStyleFont(const nsFont& aFont)
  : mFlags(NS_STYLE_FONT_DEFAULT),
    mFont(aFont),
    mSize(aFont.size)
{
}

nsStyleFont::nsStyleFont(const nsStyleFont& aSrc)
  : mFlags(aSrc.mFlags),
    mFont(aSrc.mFont),
    mSize(aSrc.mSize)
{
}


nsStyleFont::nsStyleFont(nsPresContext* aPresContext)
  : mFlags(NS_STYLE_FONT_DEFAULT),
    mFont(*(aPresContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID)))
{
  mSize = mFont.size = nsStyleFont::ZoomText(aPresContext, mFont.size);
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
    case eStyleUnit_Null:
      return 0;
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
    case eStyleUnit_Chars:
      // XXX we need a frame and a rendering context to calculate this, bug 281972, bug 282126.
      NS_NOTYETIMPLEMENTED("CalcCoord: eStyleUnit_Chars");
      return 0;
    default:
      NS_ERROR("bad unit type");
      break;
  }
  return 0;
}

nsStyleMargin::nsStyleMargin() {
  mMargin.Reset();
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
    nsStyleCoord coord;
    NS_FOR_CSS_SIDES(side) {
      mCachedMargin.side(side) =
        CalcCoord(mMargin.Get(side, coord), nsnull, 0);
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
  mPadding.Reset();
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
    nsStyleCoord coord;
    NS_FOR_CSS_SIDES(side) {
      mCachedPadding.side(side) =
        CalcCoord(mPadding.Get(side, coord), nsnull, 0);
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
  : mActualBorder(0, 0, 0, 0)
{
  nscoord medium =
    (aPresContext->GetBorderWidthTable())[NS_STYLE_BORDER_WIDTH_MEDIUM];
  NS_FOR_CSS_SIDES(side) {
    mBorder.side(side) = medium;
    mBorderStyle[side] = NS_STYLE_BORDER_STYLE_NONE | BORDER_COLOR_FOREGROUND;
    mBorderColor[side] = NS_RGB(0, 0, 0);
    mBorderRadius.Set(side, nsStyleCoord(0));
  }

  mBorderColors = nsnull;

  mFloatEdge = NS_STYLE_FLOAT_EDGE_CONTENT;

  mTwipsPerPixel = aPresContext->DevPixelsToAppUnits(1);
}

nsStyleBorder::nsStyleBorder(const nsStyleBorder& aSrc)
{
  memcpy((nsStyleBorder*)this, &aSrc, sizeof(nsStyleBorder));
  mBorderColors = nsnull;
  if (aSrc.mBorderColors) {
    EnsureBorderColors();
    for (PRInt32 i = 0; i < 4; i++)
      if (aSrc.mBorderColors[i])
        mBorderColors[i] = aSrc.mBorderColors[i]->CopyColors();
      else
        mBorderColors[i] = nsnull;
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
  // Note that differences in mBorder don't affect rendering (which should only
  // use mComputedBorder), so don't need to be tested for here.
  if (mTwipsPerPixel == aOther.mTwipsPerPixel &&
      mActualBorder == aOther.mActualBorder && 
      mFloatEdge == aOther.mFloatEdge) {
    // Note that mBorderStyle stores not only the border style but also
    // color-related flags.  Given that we've already done an mComputedBorder
    // comparison, border-style differences can only lead to a VISUAL hint.  So
    // it's OK to just compare the values directly -- if either the actual
    // style or the color flags differ we want to repaint.
    NS_FOR_CSS_SIDES(ix) {
      if (mBorderStyle[ix] != aOther.mBorderStyle[ix] || 
          mBorderColor[ix] != aOther.mBorderColor[ix]) {
        return NS_STYLE_HINT_VISUAL;
      }
    }

    if (mBorderRadius != aOther.mBorderRadius ||
        !mBorderColors != !aOther.mBorderColors) {
      return NS_STYLE_HINT_VISUAL;
    }

    // Note that at this point if mBorderColors is non-null so is
    // aOther.mBorderColors
    if (mBorderColors) {
      NS_FOR_CSS_SIDES(ix) {
        if (!mBorderColors[ix] != !aOther.mBorderColors[ix]) {
          return NS_STYLE_HINT_VISUAL;
        } else if (mBorderColors[ix] && aOther.mBorderColors[ix]) {
          if (!mBorderColors[ix]->Equals(aOther.mBorderColors[ix]))
            return NS_STYLE_HINT_VISUAL;
        }
      }
    }


    return NS_STYLE_HINT_NONE;
  }
  return NS_STYLE_HINT_REFLOW;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleBorder::MaxDifference()
{
  return NS_STYLE_HINT_REFLOW;
}
#endif

nsStyleOutline::nsStyleOutline(nsPresContext* aPresContext)
{
  // spacing values not inherited
  nsStyleCoord zero(0);
  NS_FOR_CSS_SIDES(side) {
    mOutlineRadius.Set(side, zero);
  }

  mOutlineOffset.SetCoordValue(0);

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
nsStyleColumn::nsStyleColumn() 
{ 
  mColumnCount = NS_STYLE_COLUMN_COUNT_AUTO;
  mColumnWidth.SetAutoValue();
  mColumnGap.SetNormalValue();
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
    return nsChangeHint_ReconstructFrame;

  if (mColumnWidth != aOther.mColumnWidth ||
      mColumnGap != aOther.mColumnGap)
    return nsChangeHint_ReflowFrame;

  return NS_STYLE_HINT_NONE;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleColumn::MaxDifference()
{
  return NS_CombineHint(nsChangeHint_ReconstructFrame,
                        nsChangeHint_ReflowFrame);
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

    mStrokeDashoffset.SetFactorValue(0.0f);
    mStrokeWidth.SetFactorValue(1.0f);

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
  mMarkerEnd = aSource.mMarkerStart;

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

nsChangeHint nsStyleSVG::CalcDifference(const nsStyleSVG& aOther) const
{
  if ( mFill                  != aOther.mFill                  ||
       mStroke                != aOther.mStroke                ||

       !EqualURIs(mMarkerEnd, aOther.mMarkerEnd)               ||
       !EqualURIs(mMarkerMid, aOther.mMarkerMid)               ||
       !EqualURIs(mMarkerStart, aOther.mMarkerStart)           ||

       mStrokeDashoffset      != aOther.mStrokeDashoffset      ||
       mStrokeWidth           != aOther.mStrokeWidth           ||

       mFillOpacity           != aOther.mFillOpacity           ||
       mStrokeMiterlimit      != aOther.mStrokeMiterlimit      ||
       mStrokeOpacity         != aOther.mStrokeOpacity         ||

       mClipRule              != aOther.mClipRule              ||
       mColorInterpolation    != aOther.mColorInterpolation    ||
       mColorInterpolationFilters != aOther.mColorInterpolationFilters ||
       mFillRule              != aOther.mFillRule              ||
       mPointerEvents         != aOther.mPointerEvents         ||
       mShapeRendering        != aOther.mShapeRendering        ||
       mStrokeDasharrayLength != aOther.mStrokeDasharrayLength ||
       mStrokeLinecap         != aOther.mStrokeLinecap         ||
       mStrokeLinejoin        != aOther.mStrokeLinejoin        ||
       mTextAnchor            != aOther.mTextAnchor            ||
       mTextRendering         != aOther.mTextRendering)
    return NS_STYLE_HINT_VISUAL;

  // length of stroke dasharrays are the same (tested above) - check entries
  for (PRUint32 i=0; i<mStrokeDasharrayLength; i++)
    if (mStrokeDasharray[i] != aOther.mStrokeDasharray[i])
      return NS_STYLE_HINT_VISUAL;

  return NS_STYLE_HINT_NONE;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleSVG::MaxDifference()
{
  return NS_STYLE_HINT_VISUAL;
}
#endif

// --------------------
// nsStyleSVGReset
//
nsStyleSVGReset::nsStyleSVGReset() 
{
    mStopColor               = NS_RGB(0,0,0);
    mFloodColor              = NS_RGB(0,0,0);
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
  mClipPath = aSource.mClipPath;
  mFilter = aSource.mFilter;
  mMask = aSource.mMask;
  mStopOpacity = aSource.mStopOpacity;
  mFloodOpacity = aSource.mFloodOpacity;
  mDominantBaseline = aSource.mDominantBaseline;
}

nsChangeHint nsStyleSVGReset::CalcDifference(const nsStyleSVGReset& aOther) const
{
  if (mStopColor             != aOther.mStopColor    ||
      mFloodColor            != aOther.mFloodColor   ||
      !EqualURIs(mClipPath, aOther.mClipPath)        ||
      !EqualURIs(mFilter, aOther.mFilter)            ||
      !EqualURIs(mMask, aOther.mMask)                ||
      mStopOpacity           != aOther.mStopOpacity  ||
      mFloodOpacity          != aOther.mFloodOpacity ||
      mDominantBaseline != aOther.mDominantBaseline)
    return NS_STYLE_HINT_VISUAL;
  
  return NS_STYLE_HINT_NONE;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleSVGReset::MaxDifference()
{
  return NS_STYLE_HINT_VISUAL;
}
#endif

// nsStyleSVGPaint implementation
nsStyleSVGPaint::~nsStyleSVGPaint() {
  if (mType == eStyleSVGPaintType_Server) {
    NS_IF_RELEASE(mPaint.mPaintServer);
  } 
}

nsStyleSVGPaint& nsStyleSVGPaint::operator=(const nsStyleSVGPaint& aOther) 
{
  mType = aOther.mType;
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
  mMaxWidth.Reset();
  mHeight.SetAutoValue();
  mMinHeight.SetCoordValue(0);
  mMaxHeight.Reset();
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
  if (mRules != aOther.mRules)
    return NS_STYLE_HINT_FRAMECHANGE;

  if ((mLayoutStrategy == aOther.mLayoutStrategy) &&
      (mFrame == aOther.mFrame) &&
      (mCols == aOther.mCols) &&
      (mSpan == aOther.mSpan))
    return NS_STYLE_HINT_NONE;
  return NS_STYLE_HINT_REFLOW;
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
  mCaptionSide = NS_SIDE_TOP;
  mBorderSpacingX.SetCoordValue(0);
  mBorderSpacingY.SetCoordValue(0);
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

nsStyleBackground::nsStyleBackground(nsPresContext* aPresContext)
  : mBackgroundFlags(NS_STYLE_BG_COLOR_TRANSPARENT | NS_STYLE_BG_IMAGE_NONE),
    mBackgroundAttachment(NS_STYLE_BG_ATTACHMENT_SCROLL),
    mBackgroundClip(NS_STYLE_BG_CLIP_BORDER),
    mBackgroundInlinePolicy(NS_STYLE_BG_INLINE_POLICY_CONTINUOUS),
    mBackgroundOrigin(NS_STYLE_BG_ORIGIN_PADDING),
    mBackgroundRepeat(NS_STYLE_BG_REPEAT_XY)
{
  mBackgroundColor = aPresContext->DefaultBackgroundColor();
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
  mAppearance = 0;
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

  // XXX the following is conservative, for now: changing float breaking shouldn't
  // necessarily require a repaint, reflow should suffice.
  if (mBreakType != aOther.mBreakType
      || mBreakBefore != aOther.mBreakBefore
      || mBreakAfter != aOther.mBreakAfter
      || mAppearance != aOther.mAppearance)
    NS_UpdateHint(hint, NS_CombineHint(nsChangeHint_ReflowFrame, nsChangeHint_RepaintFrame));

  if (mClipFlags != aOther.mClipFlags
      || mClip != aOther.mClip
      || mOpacity != aOther.mOpacity)
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);

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
  if (aCount != mContentCount) {
    DELETE_ARRAY_IF(mContents);
    if (aCount) {
      mContents = new nsStyleContentData[aCount];
      if (! mContents) {
        mContentCount = 0;
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    mContentCount = aCount;
  }
  return NS_OK;
}

// ---------------------
// nsStyleQuotes
//

nsStyleQuotes::nsStyleQuotes(void)
  : mQuotesCount(0),
    mQuotes(nsnull)
{
}

nsStyleQuotes::~nsStyleQuotes(void)
{
  DELETE_ARRAY_IF(mQuotes);
}

nsStyleQuotes::nsStyleQuotes(const nsStyleQuotes& aSource)
  : mQuotesCount(0),
    mQuotes(nsnull)
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
// nsStyleText
//

nsStyleText::nsStyleText(void) 
{ 
  mTextAlign = NS_STYLE_TEXT_ALIGN_DEFAULT;
  mTextTransform = NS_STYLE_TEXT_TRANSFORM_NONE;
  mWhiteSpace = NS_STYLE_WHITESPACE_NORMAL;

  mLetterSpacing.SetNormalValue();
  mLineHeight.SetNormalValue();
  mTextIndent.SetCoordValue(0);
  mWordSpacing.SetNormalValue();
}

nsStyleText::nsStyleText(const nsStyleText& aSource) 
{ 
  memcpy((nsStyleText*)this, &aSource, sizeof(nsStyleText));
}

nsStyleText::~nsStyleText(void) { }

nsChangeHint nsStyleText::CalcDifference(const nsStyleText& aOther) const
{
  if ((mTextAlign == aOther.mTextAlign) &&
      (mTextTransform == aOther.mTextTransform) &&
      (mWhiteSpace == aOther.mWhiteSpace) &&
      (mLetterSpacing == aOther.mLetterSpacing) &&
      (mLineHeight == aOther.mLineHeight) &&
      (mTextIndent == aOther.mTextIndent) &&
      (mWordSpacing == aOther.mWordSpacing))
    return NS_STYLE_HINT_NONE;
  return NS_STYLE_HINT_REFLOW;
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
}

nsStyleUIReset::nsStyleUIReset(const nsStyleUIReset& aSource) 
{
  mUserSelect = aSource.mUserSelect;
  mForceBrokenImageIcon = aSource.mForceBrokenImageIcon;
}

nsStyleUIReset::~nsStyleUIReset(void) 
{ 
}

nsChangeHint nsStyleUIReset::CalcDifference(const nsStyleUIReset& aOther) const
{
  if (mForceBrokenImageIcon == aOther.mForceBrokenImageIcon) {
    if (mUserSelect == aOther.mUserSelect) {
      return NS_STYLE_HINT_NONE;
    }
    return NS_STYLE_HINT_VISUAL;
  }
  return NS_STYLE_HINT_FRAMECHANGE;
}

#ifdef DEBUG
/* static */
nsChangeHint nsStyleUIReset::MaxDifference()
{
  return NS_STYLE_HINT_FRAMECHANGE;
}
#endif

