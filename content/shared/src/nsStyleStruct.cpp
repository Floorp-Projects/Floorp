/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   David Hyatt (hyatt@netscape.com
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsStyleConsts.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIPresContext.h"
#include "nsIStyleRule.h"
#include "nsISupportsArray.h"
#include "nsCRT.h"

#include "nsCOMPtr.h"
#include "nsIStyleSet.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsLayoutAtoms.h"
#include "prenv.h"

#ifdef IBMBIDI
#include "nsIUBidiUtils.h"
#endif

inline PRBool IsFixedUnit(nsStyleUnit aUnit, PRBool aEnumOK)
{
  return PRBool((aUnit == eStyleUnit_Null) || 
                (aUnit == eStyleUnit_Coord) || 
                (aEnumOK && (aUnit == eStyleUnit_Enumerated)));
}

// XXX this is here to support deprecated calc spacing methods only
inline nscoord CalcSideFor(const nsIFrame* aFrame, const nsStyleCoord& aCoord, 
                           PRUint8 aSpacing, PRUint8 aSide,
                           const nscoord* aEnumTable, PRInt32 aNumEnums)
{
  nscoord result = 0;

  switch (aCoord.GetUnit()) {
    case eStyleUnit_Auto:
      // Auto margins are handled by layout
      break;

    case eStyleUnit_Inherit:
      nsIFrame* parentFrame;
      aFrame->GetParent(&parentFrame);  // XXX may not be direct parent...
      if (nsnull != parentFrame) {
        nsIStyleContext* parentContext;
        parentFrame->GetStyleContext(&parentContext);
        if (nsnull != parentContext) {
          nsMargin  parentSpacing;
          switch (aSpacing) {
            case NS_SPACING_MARGIN:
              {
                const nsStyleMargin* parentMargin = (const nsStyleMargin*)parentContext->GetStyleData(eStyleStruct_Margin);
                parentMargin->CalcMarginFor(parentFrame, parentSpacing);  
              }

              break;
            case NS_SPACING_PADDING:
              {
                const nsStylePadding* parentPadding = (const nsStylePadding*)parentContext->GetStyleData(eStyleStruct_Padding);
                parentPadding->CalcPaddingFor(parentFrame, parentSpacing);  
              }

              break;
            case NS_SPACING_BORDER:
              {
                const nsStyleBorder* parentBorder = (const nsStyleBorder*)parentContext->GetStyleData(eStyleStruct_Border);
                parentBorder->CalcBorderFor(parentFrame, parentSpacing);  
              }

              break;
          }
          switch (aSide) {
            case NS_SIDE_LEFT:    result = parentSpacing.left;   break;
            case NS_SIDE_TOP:     result = parentSpacing.top;    break;
            case NS_SIDE_RIGHT:   result = parentSpacing.right;  break;
            case NS_SIDE_BOTTOM:  result = parentSpacing.bottom; break;
          }
          NS_RELEASE(parentContext);
        }
      }
      break;

    case eStyleUnit_Percent:
      {
        nscoord baseWidth = 0;
        PRBool  isBase = PR_FALSE;
        nsIFrame* frame;
        aFrame->GetParent(&frame);
        while (nsnull != frame) {
          frame->IsPercentageBase(isBase);
          if (isBase) {
            nsSize  size;
            frame->GetSize(size);
            baseWidth = size.width;
            // subtract border of containing block
            const nsStyleBorder* borderData = nsnull;
            frame->GetStyleData(eStyleStruct_Border,
                                (const nsStyleStruct*&)borderData);
            if (borderData) {
              nsMargin border;
              borderData->CalcBorderFor(frame, border);
              baseWidth -= (border.left + border.right);
            }
            // if aFrame is not absolutely positioned, subtract
            // padding of containing block
            const nsStyleDisplay* displayData = nsnull;
            aFrame->GetStyleData(eStyleStruct_Display,
                                 (const nsStyleStruct*&)displayData);
            if (displayData &&
                displayData->mPosition != NS_STYLE_POSITION_ABSOLUTE &&
                displayData->mPosition != NS_STYLE_POSITION_FIXED) {
              const nsStylePadding* paddingData = nsnull;
              frame->GetStyleData(eStyleStruct_Padding,
                                  (const nsStyleStruct*&)paddingData);
              if (paddingData) {
                nsMargin padding;
                paddingData->CalcPaddingFor(frame, padding);
                baseWidth -= (padding.left + padding.right);
              }
            }
            break;
          }
          frame->GetParent(&frame);
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
    mFont(aFont)
{
  mSize = aFont.size;
}

nsStyleFont::nsStyleFont(const nsStyleFont& aSrc)
:mFont(aSrc.mFont)
{
  mSize = aSrc.mSize;
  mFlags = aSrc.mFlags;
}

void* 
nsStyleFont::operator new(size_t sz, nsIPresContext* aContext) {
  void* result = nsnull;
  aContext->AllocateFromShell(sz, &result);
  if (result)
  nsCRT::zero(result, sz);
  return result;
}
  
void 
nsStyleFont::Destroy(nsIPresContext* aContext) {
  this->~nsStyleFont();
  aContext->FreeToShell(sizeof(nsStyleFont), this);
}

PRInt32 nsStyleFont::CalcDifference(const nsStyleFont& aOther) const
{
  if (mSize == aOther.mSize) {
    return CalcFontDifference(mFont, aOther.mFont);
  }
  return NS_STYLE_HINT_REFLOW;
}

PRInt32 nsStyleFont::CalcFontDifference(const nsFont& aFont1, const nsFont& aFont2)
{
  if ((aFont1.size == aFont2.size) && 
      (aFont1.sizeAdjust == aFont2.sizeAdjust) && 
      (aFont1.style == aFont2.style) &&
      (aFont1.variant == aFont2.variant) &&
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
  return PRBool(IsFixedUnit(aSides.GetLeftUnit(), aEnumOK) &&
                IsFixedUnit(aSides.GetTopUnit(), aEnumOK) &&
                IsFixedUnit(aSides.GetRightUnit(), aEnumOK) &&
                IsFixedUnit(aSides.GetBottomUnit(), aEnumOK));
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
nsStyleMargin::operator new(size_t sz, nsIPresContext* aContext) {
  void* result = nsnull;
  aContext->AllocateFromShell(sz, &result);
  if (result)
  nsCRT::zero(result, sz);
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
    nsStyleCoord  coord;
    mCachedMargin.left = CalcCoord(mMargin.GetLeft(coord), nsnull, 0);
    mCachedMargin.top = CalcCoord(mMargin.GetTop(coord), nsnull, 0);
    mCachedMargin.right = CalcCoord(mMargin.GetRight(coord), nsnull, 0);
    mCachedMargin.bottom = CalcCoord(mMargin.GetBottom(coord), nsnull, 0);

    mHasCachedMargin = PR_TRUE;
  }
  else
    mHasCachedMargin = PR_FALSE;
}

PRInt32 nsStyleMargin::CalcDifference(const nsStyleMargin& aOther) const
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
nsStylePadding::operator new(size_t sz, nsIPresContext* aContext) {
  void* result = nsnull;
  aContext->AllocateFromShell(sz, &result);
  if (result)
  nsCRT::zero(result, sz);
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
    nsStyleCoord  coord;
    mCachedPadding.left = CalcCoord(mPadding.GetLeft(coord), nsnull, 0);
    mCachedPadding.top = CalcCoord(mPadding.GetTop(coord), nsnull, 0);
    mCachedPadding.right = CalcCoord(mPadding.GetRight(coord), nsnull, 0);
    mCachedPadding.bottom = CalcCoord(mPadding.GetBottom(coord), nsnull, 0);

    mHasCachedPadding = PR_TRUE;
  }
  else
    mHasCachedPadding = PR_FALSE;
}

PRInt32 nsStylePadding::CalcDifference(const nsStylePadding& aOther) const
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
    aPresContext->GetPixelsToTwips(&pixelsToTwips);
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

  mBorderRadius.Reset();

  mFloatEdge = NS_STYLE_FLOAT_EDGE_CONTENT;
  
  mHasCachedBorder = PR_FALSE;
}

nsStyleBorder::nsStyleBorder(const nsStyleBorder& aSrc)
{
  nsCRT::memcpy((nsStyleBorder*)this, &aSrc, sizeof(nsStyleBorder));
  mHasCachedBorder = PR_FALSE;
}

void* 
nsStyleBorder::operator new(size_t sz, nsIPresContext* aContext) {
  void* result = nsnull;
  aContext->AllocateFromShell(sz, &result);
  if (result)
  nsCRT::zero(result, sz);
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
  if (((!IsBorderSideVisible(NS_SIDE_LEFT))|| 
       IsFixedUnit(mBorder.GetLeftUnit(), PR_TRUE)) &&
      ((!IsBorderSideVisible(NS_SIDE_TOP)) || 
       IsFixedUnit(mBorder.GetTopUnit(), PR_TRUE)) &&
      ((!IsBorderSideVisible(NS_SIDE_RIGHT)) || 
       IsFixedUnit(mBorder.GetRightUnit(), PR_TRUE)) &&
      ((!IsBorderSideVisible(NS_SIDE_BOTTOM)) || 
       IsFixedUnit(mBorder.GetBottomUnit(), PR_TRUE))) {
    nsStyleCoord  coord;
    if (!IsBorderSideVisible(NS_SIDE_LEFT)) {
      mCachedBorder.left = 0;
    }
    else {
      mCachedBorder.left = CalcCoord(mBorder.GetLeft(coord), mBorderWidths, 3);
    }
    if (!IsBorderSideVisible(NS_SIDE_TOP)) {
      mCachedBorder.top = 0;
    }
    else {
      mCachedBorder.top = CalcCoord(mBorder.GetTop(coord), mBorderWidths, 3);
    }
    if (!IsBorderSideVisible(NS_SIDE_RIGHT)) {
      mCachedBorder.right = 0;
    }
    else {
      mCachedBorder.right = CalcCoord(mBorder.GetRight(coord), mBorderWidths, 3);
    }
    if (!IsBorderSideVisible(NS_SIDE_BOTTOM)) {
      mCachedBorder.bottom = 0;
    }
    else {
      mCachedBorder.bottom = CalcCoord(mBorder.GetBottom(coord), mBorderWidths, 3);
    }
    mHasCachedBorder = PR_TRUE;
  }
  else {
    mHasCachedBorder = PR_FALSE;
  }

  if ((mBorderStyle[NS_SIDE_TOP] & BORDER_COLOR_DEFINED) == 0) {
    mBorderStyle[NS_SIDE_TOP] = BORDER_COLOR_DEFINED | BORDER_COLOR_FOREGROUND;
  }
  if ((mBorderStyle[NS_SIDE_BOTTOM] & BORDER_COLOR_DEFINED) == 0) {
    mBorderStyle[NS_SIDE_BOTTOM] = BORDER_COLOR_DEFINED | BORDER_COLOR_FOREGROUND;
  }
  if ((mBorderStyle[NS_SIDE_LEFT]& BORDER_COLOR_DEFINED) == 0) {
    mBorderStyle[NS_SIDE_LEFT] = BORDER_COLOR_DEFINED | BORDER_COLOR_FOREGROUND;
  }
  if ((mBorderStyle[NS_SIDE_RIGHT] & BORDER_COLOR_DEFINED) == 0) {
    mBorderStyle[NS_SIDE_RIGHT] = BORDER_COLOR_DEFINED | BORDER_COLOR_FOREGROUND;
  }
}

PRInt32 nsStyleBorder::CalcDifference(const nsStyleBorder& aOther) const
{
  if ((mBorder == aOther.mBorder) && 
      (mFloatEdge == aOther.mFloatEdge)) {
    PRInt32 ix;
    for (ix = 0; ix < 4; ix++) {
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

nsStyleOutline::nsStyleOutline(nsIPresContext* aPresContext)
{
  // XXX support mBorderWidths until deprecated methods are removed
  float pixelsToTwips = 20.0f;
  if (aPresContext)
    aPresContext->GetPixelsToTwips(&pixelsToTwips);
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
  nsCRT::memcpy((nsStyleOutline*)this, &aSrc, sizeof(nsStyleOutline));
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

PRInt32 
nsStyleOutline::CalcDifference(const nsStyleOutline& aOther) const
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
{
  mListStyleType = NS_STYLE_LIST_STYLE_BASIC;
  mListStylePosition = NS_STYLE_LIST_STYLE_POSITION_OUTSIDE;
  mListStyleImage.Truncate();  
}

nsStyleList::~nsStyleList() 
{
}

nsStyleList::nsStyleList(const nsStyleList& aSource)
{
  mListStyleType = aSource.mListStyleType;
  mListStylePosition = aSource.mListStylePosition;
  mListStyleImage = aSource.mListStyleImage;
}

PRInt32 nsStyleList::CalcDifference(const nsStyleList& aOther) const
{
  if (mListStylePosition == aOther.mListStylePosition)
    if (mListStyleImage == aOther.mListStyleImage)
      if (mListStyleType == aOther.mListStyleType)
        return NS_STYLE_HINT_NONE;
      return NS_STYLE_HINT_REFLOW;
    return NS_STYLE_HINT_REFLOW;
  return NS_STYLE_HINT_REFLOW;
}

#ifdef INCLUDE_XUL
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
  nsCRT::memcpy((nsStyleXUL*)this, &aSource, sizeof(nsStyleXUL));
}

PRInt32 
nsStyleXUL::CalcDifference(const nsStyleXUL& aOther) const
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

#endif // INCLUDE_XUL

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
  nsCRT::memcpy((nsStylePosition*)this, &aSource, sizeof(nsStylePosition));
}

PRInt32 nsStylePosition::CalcDifference(const nsStylePosition& aOther) const
{
  if ((mOffset == aOther.mOffset) &&
      (mWidth == aOther.mWidth) &&
      (mMinWidth == aOther.mMinWidth) &&
      (mMaxWidth == aOther.mMaxWidth) &&
      (mHeight == aOther.mHeight) &&
      (mMinHeight == aOther.mMinHeight) &&
      (mMaxHeight == aOther.mMaxHeight) &&
      (mBoxSizing == aOther.mBoxSizing) &&
      (mZIndex == aOther.mZIndex)) {
    return NS_STYLE_HINT_NONE;
  }
  return NS_STYLE_HINT_REFLOW;
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
  mRules = NS_STYLE_TABLE_RULES_ALL;
  mSpan = 1;
}

nsStyleTable::~nsStyleTable(void) 
{ 
}

nsStyleTable::nsStyleTable(const nsStyleTable& aSource)
{
  nsCRT::memcpy((nsStyleTable*)this, &aSource, sizeof(nsStyleTable));
}

PRInt32 nsStyleTable::CalcDifference(const nsStyleTable& aOther) const
{
  if ((mLayoutStrategy == aOther.mLayoutStrategy) &&
      (mFrame == aOther.mFrame) &&
      (mRules == aOther.mRules) &&
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

  nsCompatibility compatMode = eCompatibility_Standard;
  if (aPresContext)
		aPresContext->GetCompatibilityMode(&compatMode);
  mEmptyCells = (compatMode == eCompatibility_NavQuirks
                  ? NS_STYLE_TABLE_EMPTY_CELLS_SHOW_BACKGROUND     
                  : NS_STYLE_TABLE_EMPTY_CELLS_SHOW);
  mCaptionSide = NS_SIDE_TOP;
  mBorderSpacingX.Reset();
  mBorderSpacingY.Reset();
}

nsStyleTableBorder::~nsStyleTableBorder(void) 
{ 
}

nsStyleTableBorder::nsStyleTableBorder(const nsStyleTableBorder& aSource)
{
  nsCRT::memcpy((nsStyleTableBorder*)this, &aSource, sizeof(nsStyleTableBorder));
}

PRInt32 nsStyleTableBorder::CalcDifference(const nsStyleTableBorder& aOther) const
{
  if ((mBorderCollapse == aOther.mBorderCollapse) &&
      (mCaptionSide == aOther.mCaptionSide) &&
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
  aPresContext->GetDefaultColor(&mColor);
}

nsStyleColor::nsStyleColor(const nsStyleColor& aSource)
{
  mColor = aSource.mColor;
}

PRInt32 nsStyleColor::CalcDifference(const nsStyleColor& aOther) const
{
  if (mColor == aOther.mColor)
    return NS_STYLE_HINT_NONE;
  return NS_STYLE_HINT_VISUAL;
}

// --------------------
// nsStyleBackground
//

nsStyleBackground::nsStyleBackground(nsIPresContext* aPresContext)
{
  mBackgroundFlags = NS_STYLE_BG_COLOR_TRANSPARENT | NS_STYLE_BG_IMAGE_NONE;
  aPresContext->GetDefaultBackgroundColor(&mBackgroundColor);
  aPresContext->GetDefaultBackgroundImageAttachment(&mBackgroundAttachment);
  aPresContext->GetDefaultBackgroundImageRepeat(&mBackgroundRepeat);
  aPresContext->GetDefaultBackgroundImageOffset(&mBackgroundXPosition, &mBackgroundYPosition);
  aPresContext->GetDefaultBackgroundImage(mBackgroundImage);
}

nsStyleBackground::nsStyleBackground(const nsStyleBackground& aSource)
{
  mBackgroundAttachment = aSource.mBackgroundAttachment;
  mBackgroundFlags = aSource.mBackgroundFlags;
  mBackgroundRepeat = aSource.mBackgroundRepeat;

  mBackgroundColor = aSource.mBackgroundColor;
  mBackgroundXPosition = aSource.mBackgroundXPosition;
  mBackgroundYPosition = aSource.mBackgroundYPosition;
  mBackgroundImage = aSource.mBackgroundImage;
}

PRInt32 nsStyleBackground::CalcDifference(const nsStyleBackground& aOther) const
{
  if ((mBackgroundAttachment == aOther.mBackgroundAttachment) &&
      (mBackgroundFlags == aOther.mBackgroundFlags) &&
      (mBackgroundRepeat == aOther.mBackgroundRepeat) &&
      (mBackgroundColor == aOther.mBackgroundColor) &&
      (mBackgroundXPosition == aOther.mBackgroundXPosition) &&
      (mBackgroundYPosition == aOther.mBackgroundYPosition) &&
      (mBackgroundImage == aOther.mBackgroundImage))
    return NS_STYLE_HINT_NONE;
  return NS_STYLE_HINT_VISUAL;
}

// --------------------
// nsStyleDisplay
//

nsStyleDisplay::nsStyleDisplay()
{
  mDisplay = NS_STYLE_DISPLAY_INLINE;
  mPosition = NS_STYLE_POSITION_NORMAL;
  mFloats = NS_STYLE_FLOAT_NONE;
  mBreakType = NS_STYLE_CLEAR_NONE;
  mBreakBefore = PR_FALSE;
  mBreakAfter = PR_FALSE;
  mOverflow = NS_STYLE_OVERFLOW_VISIBLE;
  mClipFlags = NS_STYLE_CLIP_AUTO;
  mClip.SetRect(0,0,0,0);
}

nsStyleDisplay::nsStyleDisplay(const nsStyleDisplay& aSource)
{
  mDisplay = aSource.mDisplay;
  mBinding = aSource.mBinding;
  mPosition = aSource.mPosition;
  mFloats = aSource.mFloats;
  mBreakType = aSource.mBreakType;
  mBreakBefore = aSource.mBreakBefore;
  mBreakAfter = aSource.mBreakAfter;
  mOverflow = aSource.mOverflow;
  mClipFlags = aSource.mClipFlags;
  mClip = aSource.mClip;
}

PRInt32 nsStyleDisplay::CalcDifference(const nsStyleDisplay& aOther) const
{
  if (mBinding != aOther.mBinding || mPosition != aOther.mPosition)
    return NS_STYLE_HINT_FRAMECHANGE;

  if ((mDisplay == aOther.mDisplay) &&
      (mFloats == aOther.mFloats) &&
      (mOverflow == aOther.mOverflow)) {
    if ((mBreakType == aOther.mBreakType) &&
        (mBreakBefore == aOther.mBreakBefore) &&
        (mBreakAfter == aOther.mBreakAfter) &&
        (mClipFlags == aOther.mClipFlags) &&
        (mClip == aOther.mClip)) {
      return NS_STYLE_HINT_NONE;
    }
    return NS_STYLE_HINT_REFLOW;
  }
  return NS_STYLE_HINT_FRAMECHANGE;
}

// --------------------
// nsStyleVisibility
//

nsStyleVisibility::nsStyleVisibility(nsIPresContext* aPresContext)
{
#ifdef IBMBIDI
  PRUint32 bidiOptions;
  aPresContext->GetBidi(&bidiOptions);
  if (GET_BIDI_OPTION_DIRECTION(bidiOptions) == IBMBIDI_TEXTDIRECTION_RTL)
    mDirection = NS_STYLE_DIRECTION_RTL;
  else
    mDirection = NS_STYLE_DIRECTION_LTR;
#else
  mDirection = NS_STYLE_DIRECTION_LTR;
#endif // IBMBIDI

  aPresContext->GetLanguage(getter_AddRefs(mLanguage));
  mVisible = NS_STYLE_VISIBILITY_VISIBLE;
  mOpacity = 1.0f;
}

nsStyleVisibility::nsStyleVisibility(const nsStyleVisibility& aSource)
{
  mDirection = aSource.mDirection;
  mVisible = aSource.mVisible;
  mLanguage = aSource.mLanguage;
  mOpacity = aSource.mOpacity;
} 

PRInt32 nsStyleVisibility::CalcDifference(const nsStyleVisibility& aOther) const
{
  if (mOpacity != aOther.mOpacity)
    return NS_STYLE_HINT_VISUAL;

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
      aSource.GetContentAt(index, mContents[index].mType, mContents[index].mContent);
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

PRInt32 
nsStyleContent::CalcDifference(const nsStyleContent& aOther) const
{
  if (mContentCount == aOther.mContentCount) {
    if ((mMarkerOffset == aOther.mMarkerOffset) &&
        (mIncrementCount == aOther.mIncrementCount) && 
        (mResetCount == aOther.mResetCount)) {
      PRUint32 ix = mContentCount;
      while (0 < ix--) {
        if ((mContents[ix].mType != aOther.mContents[ix].mType) || 
            (mContents[ix].mContent != aOther.mContents[ix].mContent)) {
          return NS_STYLE_HINT_REFLOW;
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
{
  if (NS_SUCCEEDED(AllocateQuotes(aSource.QuotesCount()))) {
    PRUint32 count = (mQuotesCount * 2);
    for (PRUint32 index = 0; index < count; index += 2) {
      aSource.GetQuotesAt(index, mQuotes[index], mQuotes[index + 1]);
    }
  }
}

PRInt32 
nsStyleQuotes::CalcDifference(const nsStyleQuotes& aOther) const
{
  if (mQuotesCount == aOther.mQuotesCount) {
    PRUint32 ix = (mQuotesCount * 2);
    while (0 < ix--) {
      if (mQuotes[ix] != aOther.mQuotes[ix]) {
        return NS_STYLE_HINT_REFLOW;
      }
    }

    return NS_STYLE_HINT_NONE;
  }
  return NS_STYLE_HINT_REFLOW;
}

// --------------------
// nsStyleTextReset
//

nsStyleTextReset::nsStyleTextReset(void) 
{ 
  mVerticalAlign.SetIntValue(NS_STYLE_VERTICAL_ALIGN_BASELINE, eStyleUnit_Enumerated);
  mTextDecoration = NS_STYLE_TEXT_DECORATION_NONE;
#ifdef IBMBIDI
  mUnicodeBidi = NS_STYLE_UNICODE_BIDI_NORMAL;
#endif // IBMBIDI
}

nsStyleTextReset::nsStyleTextReset(const nsStyleTextReset& aSource) 
{ 
  nsCRT::memcpy((nsStyleTextReset*)this, &aSource, sizeof(nsStyleTextReset));
}

nsStyleTextReset::~nsStyleTextReset(void) { }

PRInt32 nsStyleTextReset::CalcDifference(const nsStyleTextReset& aOther) const
{
  if (mVerticalAlign == aOther.mVerticalAlign
#ifdef IBMBIDI
      && mUnicodeBidi == aOther.mUnicodeBidi
#endif // IBMBIDI
      ) {
    if (mTextDecoration != aOther.mTextDecoration)
      return NS_STYLE_HINT_VISUAL;
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
  nsCRT::memcpy((nsStyleText*)this, &aSource, sizeof(nsStyleText));
}

nsStyleText::~nsStyleText(void) { }

PRInt32 nsStyleText::CalcDifference(const nsStyleText& aOther) const
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
  mCursorImage = aSource.mCursorImage;
}

nsStyleUserInterface::~nsStyleUserInterface(void) 
{ 
}

PRInt32 nsStyleUserInterface::CalcDifference(const nsStyleUserInterface& aOther) const
{
  if ((mCursor != aOther.mCursor) ||
      (mCursorImage != aOther.mCursorImage))
    return NS_STYLE_HINT_VISUAL;

  if (mUserInput == aOther.mUserInput) {
    if (mUserModify == aOther.mUserModify) {
      if (mUserFocus == aOther.mUserFocus) {
        return NS_STYLE_HINT_NONE;
      }
      return NS_STYLE_HINT_CONTENT;
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
  mResizer = NS_STYLE_RESIZER_AUTO;
}

nsStyleUIReset::nsStyleUIReset(const nsStyleUIReset& aSource) 
{
  mUserSelect = aSource.mUserSelect;
  mKeyEquivalent = aSource.mKeyEquivalent;
  mResizer = aSource.mResizer;
}

nsStyleUIReset::~nsStyleUIReset(void) 
{ 
}

PRInt32 nsStyleUIReset::CalcDifference(const nsStyleUIReset& aOther) const
{
  if (mResizer == aOther.mResizer) {
    if (mUserSelect == aOther.mUserSelect) {
      if (mKeyEquivalent == aOther.mKeyEquivalent) {
        return NS_STYLE_HINT_NONE;
      }
      return NS_STYLE_HINT_CONTENT;
    }
    return NS_STYLE_HINT_VISUAL;
  }
  
  return NS_STYLE_HINT_VISUAL;
}
