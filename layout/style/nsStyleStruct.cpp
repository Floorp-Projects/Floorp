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

#include "nsStyleStruct.h"
#include "nsStyleConsts.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIPresContext.h"
#include "nsIDeviceContext.h"
#include "nsIStyleRule.h"
#include "nsISupportsArray.h"
#include "nsCRT.h"

#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsLayoutAtoms.h"
#include "prenv.h"

#include "nsBidiUtils.h"

#include "imgIRequest.h"

inline PRBool IsFixedUnit(nsStyleUnit aUnit, PRBool aEnumOK)
{
  return PRBool((aUnit == eStyleUnit_Null) || 
                (aUnit == eStyleUnit_Coord) || 
                (aEnumOK && (aUnit == eStyleUnit_Enumerated)));
}

// XXX this is here to support deprecated calc spacing methods only
// XXXldb Probably shouldn't be inline.
inline nscoord CalcSideFor(const nsIFrame* aFrame, const nsStyleCoord& aCoord, 
                           PRUint8 aSpacing, PRUint8 aSide,
                           const nscoord* aEnumTable, PRInt32 aNumEnums)
{
  nscoord result = 0;

  switch (aCoord.GetUnit()) {
    case eStyleUnit_Auto:
      // Auto margins are handled by layout
      break;

    case eStyleUnit_Percent:
      {
        nscoord baseWidth = 0;
        nsIFrame* frame = aFrame->GetParent();
        while (frame) {
          if (frame->IsContainingBlock()) {
            baseWidth = frame->GetSize().width;
            // subtract border of containing block
            nsMargin border;
            frame->GetStyleBorder()->CalcBorderFor(frame, border);
            baseWidth -= (border.left + border.right);
            // if aFrame is not absolutely positioned, subtract
            // padding of containing block
            const nsStyleDisplay* displayData = aFrame->GetStyleDisplay();
            if (displayData->mPosition != NS_STYLE_POSITION_ABSOLUTE &&
                displayData->mPosition != NS_STYLE_POSITION_FIXED) {
              nsMargin padding;
              frame->GetStylePadding()->CalcPaddingFor(frame, padding);
              baseWidth -= (padding.left + padding.right);
            }
            break;
          }
          frame = frame->GetParent();
        }
        result = (nscoord)((float)baseWidth * aCoord.GetPercentValue());
      }
      break;

    case eStyleUnit_Coord:
      result = aCoord.GetCoordValue();
      break;

    case eStyleUnit_Enumerated:
      if (nsnull != aEnumTable) {
        PRInt32 value = aCoord.GetIntValue();
        if ((0 <= value) && (value < aNumEnums)) {
          return aEnumTable[aCoord.GetIntValue()];
        }
      }
      break;

    case eStyleUnit_Null:
    case eStyleUnit_Normal:
    case eStyleUnit_Integer:
    case eStyleUnit_Proportional:
    default:
      result = 0;
      break;
  }
  if ((NS_SPACING_PADDING == aSpacing) || (NS_SPACING_BORDER == aSpacing)) {
    if (result < 0) {
      result = 0;
    }
  }
  return result;
}

// XXXldb Probably shouldn't be inline.
inline void CalcSidesFor(const nsIFrame* aFrame, const nsStyleSides& aSides, 
                         PRUint8 aSpacing, 
                         const nscoord* aEnumTable, PRInt32 aNumEnums,
                         nsMargin& aResult)
{
  nsStyleCoord  coord;

  aResult.left = CalcSideFor(aFrame, aSides.GetLeft(coord), aSpacing, NS_SIDE_LEFT,
                             aEnumTable, aNumEnums);
  aResult.top = CalcSideFor(aFrame, aSides.GetTop(coord), aSpacing, NS_SIDE_TOP,
                            aEnumTable, aNumEnums);
  aResult.right = CalcSideFor(aFrame, aSides.GetRight(coord), aSpacing, NS_SIDE_RIGHT,
                              aEnumTable, aNumEnums);
  aResult.bottom = CalcSideFor(aFrame, aSides.GetBottom(coord), aSpacing, NS_SIDE_BOTTOM,
                               aEnumTable, aNumEnums);
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


nsStyleFont::nsStyleFont(nsIPresContext* aPresContext)
  : mFlags(NS_STYLE_FONT_DEFAULT),
    mFont(*(aPresContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID)))
{
  mSize = mFont.size = nsStyleFont::ZoomText(aPresContext, mFont.size);
}

void* 
nsStyleFont::operator new(size_t sz, nsIPresContext* aContext) CPP_THROW_NEW {
  void* result = aContext->AllocateFromShell(sz);
  if (result)
    memset(result, 0, sz);
  return result;
}
  
void 
nsStyleFont::Destroy(nsIPresContext* aContext) {
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

inline float
TextZoomFor(nsIPresContext* aPresContext)
{
  float textZoom;
  aPresContext->DeviceContext()->GetTextZoom(textZoom);
  return textZoom;
}

/* static */ nscoord
nsStyleFont::ZoomText(nsIPresContext *aPresContext, nscoord aSize)
{
  return nscoord(float(aSize) * TextZoomFor(aPresContext));
}

/* static */ nscoord
nsStyleFont::UnZoomText(nsIPresContext *aPresContext, nscoord aSize)
{
  return nscoord(float(aSize) / TextZoomFor(aPresContext));
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

static nscoord nsMargin::* const gMarginSides[4] = {
  &nsMargin::top, &nsMargin::right, &nsMargin::bottom, &nsMargin::left
};

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
nsStyleMargin::operator new(size_t sz, nsIPresContext* aContext) CPP_THROW_NEW {
  void* result = aContext->AllocateFromShell(sz);
  if (result)
    memset(result, 0, sz);
  return result;
}
  
void 
nsStyleMargin::Destroy(nsIPresContext* aContext) {
  this->~nsStyleMargin();
  aContext->FreeToShell(sizeof(nsStyleMargin), this);
}


void nsStyleMargin::RecalcData()
{
  if (IsFixedData(mMargin, PR_FALSE)) {
    nsStyleCoord coord;
    NS_FOR_CSS_SIDES(side) {
      mCachedMargin.*(gMarginSides[side]) =
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

void 
nsStyleMargin::CalcMarginFor(const nsIFrame* aFrame, nsMargin& aMargin) const
{
  if (mHasCachedMargin) {
    aMargin = mCachedMargin;
  } else {
    CalcSidesFor(aFrame, mMargin, NS_SPACING_MARGIN, nsnull, 0, aMargin);
  }
}

nsStylePadding::nsStylePadding() {
  mPadding.Reset();
  mHasCachedPadding = PR_FALSE;
}

nsStylePadding::nsStylePadding(const nsStylePadding& aSrc) {
  mPadding = aSrc.mPadding;
  mHasCachedPadding = PR_FALSE;
}

void* 
nsStylePadding::operator new(size_t sz, nsIPresContext* aContext) CPP_THROW_NEW {
  void* result = aContext->AllocateFromShell(sz);
  if (result)
    memset(result, 0, sz);
  return result;
}
  
void 
nsStylePadding::Destroy(nsIPresContext* aContext) {
  this->~nsStylePadding();
  aContext->FreeToShell(sizeof(nsStylePadding), this);
}

void nsStylePadding::RecalcData()
{
  if (IsFixedData(mPadding, PR_FALSE)) {
    nsStyleCoord coord;
    NS_FOR_CSS_SIDES(side) {
      mCachedPadding.*(gMarginSides[side]) =
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

void 
nsStylePadding::CalcPaddingFor(const nsIFrame* aFrame, nsMargin& aPadding) const
{
  if (mHasCachedPadding) {
    aPadding = mCachedPadding;
  } else {
    CalcSidesFor(aFrame, mPadding, NS_SPACING_PADDING, nsnull, 0, aPadding);
  }
}

nsStyleBorder::nsStyleBorder(nsIPresContext* aPresContext)
{
  // XXX support mBorderWidths until deprecated methods are removed
  float pixelsToTwips = 20.0f;
  if (aPresContext) {
    pixelsToTwips = aPresContext->PixelsToTwips();
  }
  mBorderWidths[NS_STYLE_BORDER_WIDTH_THIN] = NSIntPixelsToTwips(1, pixelsToTwips);
  mBorderWidths[NS_STYLE_BORDER_WIDTH_MEDIUM] = NSIntPixelsToTwips(3, pixelsToTwips);
  mBorderWidths[NS_STYLE_BORDER_WIDTH_THICK] = NSIntPixelsToTwips(5, pixelsToTwips);
   
  // spacing values not inherited
  nsStyleCoord  medium(NS_STYLE_BORDER_WIDTH_MEDIUM, eStyleUnit_Enumerated);
  mBorder.SetLeft(medium);
  mBorder.SetTop(medium);
  mBorder.SetRight(medium);
  mBorder.SetBottom(medium);
  
  mBorderStyle[0] = NS_STYLE_BORDER_STYLE_NONE;  
  mBorderStyle[1] = NS_STYLE_BORDER_STYLE_NONE; 
  mBorderStyle[2] = NS_STYLE_BORDER_STYLE_NONE; 
  mBorderStyle[3] = NS_STYLE_BORDER_STYLE_NONE;  

  mBorderColor[0] = NS_RGB(0, 0, 0);  
  mBorderColor[1] = NS_RGB(0, 0, 0);  
  mBorderColor[2] = NS_RGB(0, 0, 0);  
  mBorderColor[3] = NS_RGB(0, 0, 0); 

  mBorderColors = nsnull;

  mBorderRadius.Reset();

  mFloatEdge = NS_STYLE_FLOAT_EDGE_CONTENT;
  
  mHasCachedBorder = PR_FALSE;
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
  mHasCachedBorder = PR_FALSE;
}

void* 
nsStyleBorder::operator new(size_t sz, nsIPresContext* aContext) CPP_THROW_NEW {
  void* result = aContext->AllocateFromShell(sz);
  if (result)
    memset(result, 0, sz);
  return result;
}
  
void 
nsStyleBorder::Destroy(nsIPresContext* aContext) {
  this->~nsStyleBorder();
  aContext->FreeToShell(sizeof(nsStyleBorder), this);
}


PRBool nsStyleBorder::IsBorderSideVisible(PRUint8 aSide) const
{
	PRUint8 borderStyle = GetBorderStyle(aSide);
	return ((borderStyle != NS_STYLE_BORDER_STYLE_NONE)
       && (borderStyle != NS_STYLE_BORDER_STYLE_HIDDEN));
}

void nsStyleBorder::RecalcData()
{
  PRBool allFixed = PR_TRUE;
  {NS_FOR_CSS_SIDES(side) {
    if (IsBorderSideVisible(side) &&
        !IsFixedUnit(mBorder.GetUnit(side), PR_TRUE)) {
      allFixed = PR_FALSE;
      break;
    }
  }}
  if (allFixed) {
    nsStyleCoord coord;
    NS_FOR_CSS_SIDES(side) {
      mCachedBorder.*(gMarginSides[side]) = IsBorderSideVisible(side)
        ? CalcCoord(mBorder.Get(side, coord), mBorderWidths, 3)
        : 0;
    }
    mHasCachedBorder = PR_TRUE;
  }
  else {
    mHasCachedBorder = PR_FALSE;
  }

  NS_FOR_CSS_SIDES(side) {
    if ((mBorderStyle[side] & BORDER_COLOR_DEFINED) == 0) {
      NS_ASSERTION(!(mBorderStyle[side] & BORDER_COLOR_SPECIAL),
                   "Clearing special border because BORDER_COLOR_DEFINED is not set");
      SetBorderToForeground(side);
    }
  }
}

nsChangeHint nsStyleBorder::CalcDifference(const nsStyleBorder& aOther) const
{
  if ((mBorder == aOther.mBorder) && 
      (mFloatEdge == aOther.mFloatEdge)) {
    NS_FOR_CSS_SIDES(ix) {
      if ((mBorderStyle[ix] != aOther.mBorderStyle[ix]) || 
          (mBorderColor[ix] != aOther.mBorderColor[ix])) {
        if ((mBorderStyle[ix] != aOther.mBorderStyle[ix]) &&
            ((NS_STYLE_BORDER_STYLE_NONE == mBorderStyle[ix]) ||
             (NS_STYLE_BORDER_STYLE_NONE == aOther.mBorderStyle[ix]) ||
             (NS_STYLE_BORDER_STYLE_HIDDEN == mBorderStyle[ix]) ||          // bug 45754
             (NS_STYLE_BORDER_STYLE_HIDDEN == aOther.mBorderStyle[ix]))) {
          return NS_STYLE_HINT_REFLOW;  // border on or off
        }
        return NS_STYLE_HINT_VISUAL;
      }
    }
    if (mBorderRadius != aOther.mBorderRadius) {
      return NS_STYLE_HINT_VISUAL;
    }
    if (mBorderColors && !aOther.mBorderColors ||
        !mBorderColors && aOther.mBorderColors) {
      return NS_STYLE_HINT_VISUAL;
    }

    if (mBorderColors && aOther.mBorderColors) {
      NS_FOR_CSS_SIDES(ix) {
        if (mBorderColors[ix] && !aOther.mBorderColors[ix] ||
            !mBorderColors[ix] && aOther.mBorderColors[ix]) {
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

void 
nsStyleBorder::CalcBorderFor(const nsIFrame* aFrame, nsMargin& aBorder) const
{
  if (mHasCachedBorder) {
    aBorder = mCachedBorder;
  } else {
    CalcSidesFor(aFrame, mBorder, NS_SPACING_BORDER, mBorderWidths, 3, aBorder);
  }
}

void 
nsStyleBorder::CalcBorderFor(const nsIFrame* aFrame, PRUint8 aSide, nscoord& aWidth) const
{
  aWidth = 0;
  // using mCachedBorder as above, doesn't work properly 
  nsStyleCoord  coord;
  switch(aSide) {
  case NS_SIDE_TOP:
    coord = mBorder.GetTop(coord);
    break;
  case NS_SIDE_RIGHT:
    coord = mBorder.GetRight(coord);
    break;
  case NS_SIDE_BOTTOM:
    coord = mBorder.GetBottom(coord);
    break;
  default: // NS_SIDE_LEFT
    coord = mBorder.GetLeft(coord);
  }
  aWidth = CalcSideFor(aFrame, coord, NS_SPACING_BORDER, aSide, mBorderWidths, 3);
}

nsStyleOutline::nsStyleOutline(nsIPresContext* aPresContext)
{
  // XXX support mBorderWidths until deprecated methods are removed
  float pixelsToTwips = 20.0f;
  if (aPresContext)
    pixelsToTwips = aPresContext->PixelsToTwips();
  mBorderWidths[NS_STYLE_BORDER_WIDTH_THIN] = NSIntPixelsToTwips(1, pixelsToTwips);
  mBorderWidths[NS_STYLE_BORDER_WIDTH_MEDIUM] = NSIntPixelsToTwips(3, pixelsToTwips);
  mBorderWidths[NS_STYLE_BORDER_WIDTH_THICK] = NSIntPixelsToTwips(5, pixelsToTwips);
 
  // spacing values not inherited
  mOutlineRadius.Reset();

  nsStyleCoord  medium(NS_STYLE_BORDER_WIDTH_MEDIUM, eStyleUnit_Enumerated);
  mOutlineWidth = medium;
  mOutlineStyle = NS_STYLE_BORDER_STYLE_NONE;
  mOutlineColor = NS_RGB(0, 0, 0);

  mHasCachedOutline = PR_FALSE;
}

nsStyleOutline::nsStyleOutline(const nsStyleOutline& aSrc) {
  memcpy((nsStyleOutline*)this, &aSrc, sizeof(nsStyleOutline));
}

void 
nsStyleOutline::RecalcData(void)
{
  if ((NS_STYLE_BORDER_STYLE_NONE == GetOutlineStyle()) || 
     IsFixedUnit(mOutlineWidth.GetUnit(), PR_TRUE)) {
    if (NS_STYLE_BORDER_STYLE_NONE == GetOutlineStyle())
      mCachedOutlineWidth = 0;
    else
      mCachedOutlineWidth = CalcCoord(mOutlineWidth, mBorderWidths, 3);
    mHasCachedOutline = PR_TRUE;
  }
  else
    mHasCachedOutline = PR_FALSE;
}

nsChangeHint nsStyleOutline::CalcDifference(const nsStyleOutline& aOther) const
{
  if ((mOutlineWidth != aOther.mOutlineWidth) ||
      (mOutlineStyle != aOther.mOutlineStyle) ||
      (mOutlineColor != aOther.mOutlineColor) ||
      (mOutlineRadius != aOther.mOutlineRadius)) {
    return NS_STYLE_HINT_VISUAL;	// XXX: should be VISUAL: see bugs 9809 and 9816
  }
  return NS_STYLE_HINT_NONE;
}

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
  if (mListStylePosition == aOther.mListStylePosition &&
      EqualURIs(mListStyleImage, aOther.mListStyleImage) &&
      mListStyleType == aOther.mListStyleType) {
    if (mImageRegion == aOther.mImageRegion)
      return NS_STYLE_HINT_NONE;
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

#ifdef MOZ_SVG
// --------------------
// nsStyleSVG
//
nsStyleSVG::nsStyleSVG() 
{
    mFill.mType       = eStyleSVGPaintType_Color;
    mFill.mColor      = NS_RGB(0,0,0);
    mFillOpacity      = 1.0f;
    mFillRule         = NS_STYLE_FILL_RULE_NONZERO;
    mPointerEvents    = NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED;
    mShapeRendering   = NS_STYLE_SHAPE_RENDERING_AUTO;
    mStroke.mType     = eStyleSVGPaintType_None;
    mStroke.mColor    = NS_RGB(0,0,0);
    mStrokeDasharray.Truncate();
    mStrokeDashoffset = 0.0f;
    mStrokeLinecap    = NS_STYLE_STROKE_LINECAP_BUTT;
    mStrokeLinejoin   = NS_STYLE_STROKE_LINEJOIN_MITER;
    mStrokeMiterlimit = 4.0f;
    mStrokeOpacity    = 1.0f;
    mStrokeWidth      = 1.0f;
    mTextAnchor       = NS_STYLE_TEXT_ANCHOR_START;
    mTextRendering    = NS_STYLE_TEXT_RENDERING_AUTO;
}

nsStyleSVG::~nsStyleSVG() 
{
}

nsStyleSVG::nsStyleSVG(const nsStyleSVG& aSource)
{
  //memcpy((nsStyleSVG*)this, &aSource, sizeof(nsStyleSVG));

  mFill.mType = aSource.mFill.mType;
  if (mFill.mType == eStyleSVGPaintType_Color)
    mFill.mColor = aSource.mFill.mColor;
  mFillOpacity = aSource.mFillOpacity;
  mFillRule = aSource.mFillRule;
  mPointerEvents = aSource.mPointerEvents;
  mShapeRendering = aSource.mShapeRendering;
  mStroke.mType = aSource.mStroke.mType;
  if (mStroke.mType == eStyleSVGPaintType_Color)
    mStroke.mColor = aSource.mStroke.mColor;
  mStrokeDasharray = aSource.mStrokeDasharray;
  mStrokeDashoffset = aSource.mStrokeDashoffset;
  mStrokeLinecap = aSource.mStrokeLinecap;
  mStrokeLinejoin = aSource.mStrokeLinejoin;
  mStrokeMiterlimit = aSource.mStrokeMiterlimit;
  mStrokeOpacity = aSource.mStrokeOpacity;
  mStrokeWidth = aSource.mStrokeWidth;
  mTextAnchor = aSource.mTextAnchor;
  mTextRendering = aSource.mTextRendering;
}

nsChangeHint nsStyleSVG::CalcDifference(const nsStyleSVG& aOther) const
{
  if ( mFill.mType       != aOther.mFill.mType       ||
       mFillOpacity      != aOther.mFillOpacity      ||
       mFillRule         != aOther.mFillRule         ||
       mPointerEvents    != aOther.mPointerEvents    ||
       mShapeRendering   != aOther.mShapeRendering   ||
       mStroke.mType     != aOther.mStroke.mType     ||
       mStrokeDasharray  != aOther.mStrokeDasharray  ||
       mStrokeDashoffset != aOther.mStrokeDashoffset ||
       mStrokeLinecap    != aOther.mStrokeLinecap    ||
       mStrokeLinejoin   != aOther.mStrokeLinejoin   ||
       mStrokeMiterlimit != aOther.mStrokeMiterlimit ||
       mStrokeOpacity    != aOther.mStrokeOpacity    ||
       mStrokeWidth      != aOther.mStrokeWidth      ||
       mTextAnchor       != aOther.mTextAnchor       ||
       mTextRendering    != aOther.mTextRendering)
    return NS_STYLE_HINT_VISUAL;

  if ( (mStroke.mType == eStyleSVGPaintType_Color && mStroke.mColor != aOther.mStroke.mColor) ||
       (mFill.mType   == eStyleSVGPaintType_Color && mFill.mColor   != aOther.mFill.mColor) )
    return NS_STYLE_HINT_VISUAL;

  return NS_STYLE_HINT_NONE;
}

// --------------------
// nsStyleSVGReset
//
nsStyleSVGReset::nsStyleSVGReset() 
{
    mDominantBaseline = NS_STYLE_DOMINANT_BASELINE_AUTO;
}

nsStyleSVGReset::~nsStyleSVGReset() 
{
}

nsStyleSVGReset::nsStyleSVGReset(const nsStyleSVGReset& aSource)
{
  memcpy((nsStyleSVGReset*)this, &aSource, sizeof(nsStyleSVGReset));
}

nsChangeHint nsStyleSVGReset::CalcDifference(const nsStyleSVGReset& aOther) const
{
  if (mDominantBaseline != aOther.mDominantBaseline)
    return NS_STYLE_HINT_VISUAL;
  
  return NS_STYLE_HINT_NONE;
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

// -----------------------
// nsStyleTableBorder

nsStyleTableBorder::nsStyleTableBorder(nsIPresContext* aPresContext) 
{ 
  mBorderCollapse = NS_STYLE_BORDER_SEPARATE;

  nsCompatibility compatMode = eCompatibility_FullStandards;
  if (aPresContext)
    compatMode = aPresContext->CompatibilityMode();
  mEmptyCells = (compatMode == eCompatibility_NavQuirks)
                  ? NS_STYLE_TABLE_EMPTY_CELLS_SHOW_BACKGROUND     
                  : NS_STYLE_TABLE_EMPTY_CELLS_SHOW;
  mCaptionSide = NS_SIDE_TOP;
  mBorderSpacingX.Reset();
  mBorderSpacingY.Reset();
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

// --------------------
// nsStyleColor
//

nsStyleColor::nsStyleColor(nsIPresContext* aPresContext)
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

// --------------------
// nsStyleBackground
//

nsStyleBackground::nsStyleBackground(nsIPresContext* aPresContext)
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
  if (mBackgroundAttachment != aOther.mBackgroundAttachment
    && ((NS_STYLE_BG_ATTACHMENT_FIXED == mBackgroundAttachment) ||
        (NS_STYLE_BG_ATTACHMENT_FIXED == aOther.mBackgroundAttachment)))
    // this might require creation of a view
    // XXX This probably doesn't call ApplyRenderingChangeToTree, which
    // means we might not invalidate the canvas if this is the body.
    return NS_STYLE_HINT_FRAMECHANGE;

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
  mOverflow = NS_STYLE_OVERFLOW_VISIBLE;
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
  mOverflow = aSource.mOverflow;
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
      || mOverflow != aOther.mOverflow
      // might need to create a view to handle change from 1.0 to partial opacity
      || (mOpacity != aOther.mOpacity
          && ((mOpacity < 1.0) != (aOther.mOpacity < 1.0))))
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
    NS_UpdateHint(hint, nsChangeHint_SyncFrameView);

  return hint;
}

// --------------------
// nsStyleVisibility
//

nsStyleVisibility::nsStyleVisibility(nsIPresContext* aPresContext)
{
  PRUint32 bidiOptions;
  aPresContext->GetBidi(&bidiOptions);
  if (GET_BIDI_OPTION_DIRECTION(bidiOptions) == IBMBIDI_TEXTDIRECTION_RTL)
    mDirection = NS_STYLE_DIRECTION_RTL;
  else
    mDirection = NS_STYLE_DIRECTION_LTR;

  mLanguage = aPresContext->GetLanguage();
  mVisible = NS_STYLE_VISIBILITY_VISIBLE;
}

nsStyleVisibility::nsStyleVisibility(const nsStyleVisibility& aSource)
{
  mDirection = aSource.mDirection;
  mVisible = aSource.mVisible;
  mLanguage = aSource.mLanguage;
} 

nsChangeHint nsStyleVisibility::CalcDifference(const nsStyleVisibility& aOther) const
{
  if ((mDirection == aOther.mDirection) &&
      (mLanguage == aOther.mLanguage)) {
    if ((mVisible == aOther.mVisible)) {
      return NS_STYLE_HINT_NONE;
    }
    if ((mVisible != aOther.mVisible) && 
        ((NS_STYLE_VISIBILITY_COLLAPSE == mVisible) || 
         (NS_STYLE_VISIBILITY_COLLAPSE == aOther.mVisible))) {
      return NS_STYLE_HINT_REFLOW;
    }
    return NS_STYLE_HINT_VISUAL;
  }
  return NS_STYLE_HINT_REFLOW;
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
      aSource.GetCounterIncrementAt(index, mIncrements[index].mCounter,
                                           mIncrements[index].mValue);
    }
  }

  if (NS_SUCCEEDED(AllocateCounterResets(aSource.CounterResetCount()))) {
    for (index = 0; index < mResetCount; index++) {
      aSource.GetCounterResetAt(index, mResets[index].mCounter,
                                       mResets[index].mValue);
    }
  }
}

nsChangeHint nsStyleContent::CalcDifference(const nsStyleContent& aOther) const
{
  if (mContentCount == aOther.mContentCount) {
    if ((mMarkerOffset == aOther.mMarkerOffset) &&
        (mIncrementCount == aOther.mIncrementCount) && 
        (mResetCount == aOther.mResetCount)) {
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
          return NS_STYLE_HINT_REFLOW;
        }
      }
      ix = mResetCount;
      while (0 < ix--) {
        if ((mResets[ix].mValue != aOther.mResets[ix].mValue) || 
            (mResets[ix].mCounter != aOther.mResets[ix].mCounter)) {
          return NS_STYLE_HINT_REFLOW;
        }
      }
      return NS_STYLE_HINT_NONE;
    }
    return NS_STYLE_HINT_REFLOW;
  }
  return NS_STYLE_HINT_FRAMECHANGE;
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

//-----------------------
// nsStyleUserInterface
//

nsStyleUserInterface::nsStyleUserInterface(void) 
{ 
  mUserInput = NS_STYLE_USER_INPUT_AUTO;
  mUserModify = NS_STYLE_USER_MODIFY_READ_ONLY;
  mUserFocus = NS_STYLE_USER_FOCUS_NONE;

  mCursor = NS_STYLE_CURSOR_AUTO; // fix for bugzilla bug 51113
}

nsStyleUserInterface::nsStyleUserInterface(const nsStyleUserInterface& aSource) 
{ 
  mUserInput = aSource.mUserInput;
  mUserModify = aSource.mUserModify;
  mUserFocus = aSource.mUserFocus;
  
  mCursor = aSource.mCursor;
}

nsStyleUserInterface::~nsStyleUserInterface(void) 
{ 
}

nsChangeHint nsStyleUserInterface::CalcDifference(const nsStyleUserInterface& aOther) const
{
  if (mCursor != aOther.mCursor)
    return NS_STYLE_HINT_VISUAL;

  if (mUserInput == aOther.mUserInput) {
    if (mUserModify == aOther.mUserModify) {
      // ignore mUserFocus
      return NS_STYLE_HINT_NONE;
    }
    return NS_STYLE_HINT_VISUAL;
  }
  
  if ((mUserInput != aOther.mUserInput) &&
      ((NS_STYLE_USER_INPUT_NONE == mUserInput) || 
       (NS_STYLE_USER_INPUT_NONE == aOther.mUserInput))) {
    return NS_STYLE_HINT_FRAMECHANGE;
  }

  return NS_STYLE_HINT_VISUAL;
}

//-----------------------
// nsStyleUIReset
//

nsStyleUIReset::nsStyleUIReset(void) 
{ 
  mUserSelect = NS_STYLE_USER_SELECT_AUTO;
  mKeyEquivalent = PRUnichar(0); // XXX what type should this be?
  mForceBrokenImageIcon = 0;
}

nsStyleUIReset::nsStyleUIReset(const nsStyleUIReset& aSource) 
{
  mUserSelect = aSource.mUserSelect;
  mKeyEquivalent = aSource.mKeyEquivalent;
  mForceBrokenImageIcon = aSource.mForceBrokenImageIcon;
}

nsStyleUIReset::~nsStyleUIReset(void) 
{ 
}

nsChangeHint nsStyleUIReset::CalcDifference(const nsStyleUIReset& aOther) const
{
  if (mForceBrokenImageIcon == aOther.mForceBrokenImageIcon) {
    if (mUserSelect == aOther.mUserSelect) {
      // ignore mKeyEquivalent
      return NS_STYLE_HINT_NONE;
    }
    return NS_STYLE_HINT_VISUAL;
  }
  return NS_STYLE_HINT_FRAMECHANGE;
}
