/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   IBM Corporation 
 * 
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/20/2000   IBM Corp.       BiDi - ability to change the default direction of the browser
 *
 */
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsStyleConsts.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIPresContext.h"
#include "nsIStyleRule.h"
#include "nsISupportsArray.h"
#include "nsCRT.h"

#include "nsIFrame.h"

#include "nsCOMPtr.h"
#include "nsIStyleSet.h"
#include "nsISizeOfHandler.h"
#include "nsIPresShell.h"

static NS_DEFINE_IID(kIStyleContextIID, NS_ISTYLECONTEXT_IID);

#define DELETE_ARRAY_IF(array)  if (array) { delete[] array; array = nsnull; }

//#ifdef DEBUG_SC_SHARING


// --------------------
// nsStyleFont
//
nsStyleFont::nsStyleFont(void)
  : mFont(nsnull, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL, NS_FONT_DECORATION_NONE, 0),
    mFixedFont(nsnull, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL, NS_FONT_WEIGHT_NORMAL, NS_FONT_DECORATION_NONE, 0)
{}

nsStyleFont::nsStyleFont(const nsFont& aVariableFont, const nsFont& aFixedFont)
  : mFont(aVariableFont),
    mFixedFont(aFixedFont)
{ }

nsStyleFont::~nsStyleFont(void) { }

struct StyleFontImpl : public nsStyleFont {
  StyleFontImpl(const nsFont& aVariableFont, const nsFont& aFixedFont)
    : nsStyleFont(aVariableFont, aFixedFont)
  {}

  void ResetFrom(const nsStyleFont* aParent, nsIPresContext* aPresContext);
  void SetFrom(const nsStyleFont& aSource);
  void CopyTo(nsStyleFont& aDest) const;
  PRInt32 CalcDifference(const StyleFontImpl& aOther) const;
  static PRInt32 CalcFontDifference(const nsFont& aFont1, const nsFont& aFont2);

private:  // These are not allowed
  StyleFontImpl(const StyleFontImpl& aOther);
  StyleFontImpl& operator=(const StyleFontImpl& aOther);
};

void StyleFontImpl::ResetFrom(const nsStyleFont* aParent, nsIPresContext* aPresContext)
{
  if (nsnull != aParent) {
    mFont = aParent->mFont;
    mFixedFont = aParent->mFixedFont;
    mFlags = aParent->mFlags;
  }
  else {
    aPresContext->GetDefaultFont(mFont);
    aPresContext->GetDefaultFixedFont(mFixedFont);
    mFlags = NS_STYLE_FONT_DEFAULT;
  }
}

void StyleFontImpl::SetFrom(const nsStyleFont& aSource)
{
  mFont = aSource.mFont;
  mFixedFont = aSource.mFixedFont;
  mFlags = aSource.mFlags;
}

void StyleFontImpl::CopyTo(nsStyleFont& aDest) const
{
  aDest.mFont = mFont;
  aDest.mFixedFont = mFixedFont;
  aDest.mFlags = mFlags;
}

PRInt32 StyleFontImpl::CalcDifference(const StyleFontImpl& aOther) const
{
  if (mFlags == aOther.mFlags) {
    PRInt32 impact = CalcFontDifference(mFont, aOther.mFont);
    if (impact < NS_STYLE_HINT_REFLOW) {
      impact = CalcFontDifference(mFixedFont, aOther.mFixedFont);
    }
    return impact;
  }
  return NS_STYLE_HINT_REFLOW;
}

PRInt32 StyleFontImpl::CalcFontDifference(const nsFont& aFont1, const nsFont& aFont2)
{
  if ((aFont1.size == aFont2.size) && 
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


// --------------------
// nsStyleColor
//
nsStyleColor::nsStyleColor(void) { }
nsStyleColor::~nsStyleColor(void) { }

struct StyleColorImpl: public nsStyleColor {
  StyleColorImpl(void)  { }

  void ResetFrom(const nsStyleColor* aParent, nsIPresContext* aPresContext);
  void SetFrom(const nsStyleColor& aSource);
  void CopyTo(nsStyleColor& aDest) const;
  PRInt32 CalcDifference(const StyleColorImpl& aOther) const;

private:  // These are not allowed
  StyleColorImpl(const StyleColorImpl& aOther);
  StyleColorImpl& operator=(const StyleColorImpl& aOther);
};

void StyleColorImpl::ResetFrom(const nsStyleColor* aParent, nsIPresContext* aPresContext)
{
  if (nsnull != aParent) {
    mColor = aParent->mColor;
    mOpacity = aParent->mOpacity;
  }
  else {
    if (nsnull != aPresContext) {
      aPresContext->GetDefaultColor(&mColor);
    }
    else {
      mColor = NS_RGB(0x00, 0x00, 0x00);
    }
    mOpacity = 1.0f;
  }

  mBackgroundFlags = NS_STYLE_BG_COLOR_TRANSPARENT | NS_STYLE_BG_IMAGE_NONE;
  if (nsnull != aPresContext) {
    aPresContext->GetDefaultBackgroundColor(&mBackgroundColor);
    aPresContext->GetDefaultBackgroundImageAttachment(&mBackgroundAttachment);
    aPresContext->GetDefaultBackgroundImageRepeat(&mBackgroundRepeat);
    aPresContext->GetDefaultBackgroundImageOffset(&mBackgroundXPosition, &mBackgroundYPosition);
    aPresContext->GetDefaultBackgroundImage(mBackgroundImage);
  }
  else {
    mBackgroundColor = NS_RGB(192,192,192);
    mBackgroundAttachment = NS_STYLE_BG_ATTACHMENT_SCROLL;
    mBackgroundRepeat = NS_STYLE_BG_REPEAT_XY;
    mBackgroundXPosition = 0;
    mBackgroundYPosition = 0;
  }

  mCursor = NS_STYLE_CURSOR_AUTO;
}

void StyleColorImpl::SetFrom(const nsStyleColor& aSource)
{
  mColor = aSource.mColor;
 
  mBackgroundAttachment = aSource.mBackgroundAttachment;
  mBackgroundFlags = aSource.mBackgroundFlags;
  mBackgroundRepeat = aSource.mBackgroundRepeat;

  mBackgroundColor = aSource.mBackgroundColor;
  mBackgroundXPosition = aSource.mBackgroundXPosition;
  mBackgroundYPosition = aSource.mBackgroundYPosition;
  mBackgroundImage = aSource.mBackgroundImage;

  mCursor = aSource.mCursor;
  mCursorImage = aSource.mCursorImage;
  mOpacity = aSource.mOpacity;
}

void StyleColorImpl::CopyTo(nsStyleColor& aDest) const
{
  aDest.mColor = mColor;
 
  aDest.mBackgroundAttachment = mBackgroundAttachment;
  aDest.mBackgroundFlags = mBackgroundFlags;
  aDest.mBackgroundRepeat = mBackgroundRepeat;

  aDest.mBackgroundColor = mBackgroundColor;
  aDest.mBackgroundXPosition = mBackgroundXPosition;
  aDest.mBackgroundYPosition = mBackgroundYPosition;
  aDest.mBackgroundImage = mBackgroundImage;

  aDest.mCursor = mCursor;
  aDest.mCursorImage = mCursorImage;
  aDest.mOpacity = mOpacity;
}

PRInt32 StyleColorImpl::CalcDifference(const StyleColorImpl& aOther) const
{
  if ((mColor == aOther.mColor) && 
      (mBackgroundAttachment == aOther.mBackgroundAttachment) &&
      (mBackgroundFlags == aOther.mBackgroundFlags) &&
      (mBackgroundRepeat == aOther.mBackgroundRepeat) &&
      (mBackgroundColor == aOther.mBackgroundColor) &&
      (mBackgroundXPosition == aOther.mBackgroundXPosition) &&
      (mBackgroundYPosition == aOther.mBackgroundYPosition) &&
      (mBackgroundImage == aOther.mBackgroundImage) &&
      (mCursor == aOther.mCursor) &&
      (mCursorImage == aOther.mCursorImage) &&
      (mOpacity == aOther.mOpacity)) {
    return NS_STYLE_HINT_NONE;
  }
  return NS_STYLE_HINT_VISUAL;
}

// --------------------
// nsStyleSpacing
//
// XXX this is here to support deprecated calc spacing methods only
static nscoord kBorderWidths[3];
static PRBool  kWidthsInitialized = PR_FALSE;

nsStyleSpacing::nsStyleSpacing(void) { }

#define NS_SPACING_MARGIN   0
#define NS_SPACING_PADDING  1
#define NS_SPACING_BORDER   2

static nscoord CalcSideFor(const nsIFrame* aFrame, const nsStyleCoord& aCoord, 
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
          const nsStyleSpacing* parentSpacing = (const nsStyleSpacing*)parentContext->GetStyleData(eStyleStruct_Spacing);
          nsMargin  parentMargin;
          switch (aSpacing) {
            case NS_SPACING_MARGIN:   parentSpacing->CalcMarginFor(parentFrame, parentMargin);  
              break;
            case NS_SPACING_PADDING:  parentSpacing->CalcPaddingFor(parentFrame, parentMargin);  
              break;
            case NS_SPACING_BORDER:   parentSpacing->CalcBorderFor(parentFrame, parentMargin);  
              break;
          }
          switch (aSide) {
            case NS_SIDE_LEFT:    result = parentMargin.left;   break;
            case NS_SIDE_TOP:     result = parentMargin.top;    break;
            case NS_SIDE_RIGHT:   result = parentMargin.right;  break;
            case NS_SIDE_BOTTOM:  result = parentMargin.bottom; break;
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
            baseWidth = size.width; // not really width, need to subtract out padding...
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

static void CalcSidesFor(const nsIFrame* aFrame, const nsStyleSides& aSides, 
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

void nsStyleSpacing::CalcMarginFor(const nsIFrame* aFrame, nsMargin& aMargin) const
{
  if (mHasCachedMargin) {
    aMargin = mCachedMargin;
  }
  else {
    CalcSidesFor(aFrame, mMargin, NS_SPACING_MARGIN, nsnull, 0, aMargin);
  }
}

void nsStyleSpacing::CalcPaddingFor(const nsIFrame* aFrame, nsMargin& aPadding) const
{
  if (mHasCachedPadding) {
    aPadding = mCachedPadding;
  }
  else {
    CalcSidesFor(aFrame, mPadding, NS_SPACING_PADDING, nsnull, 0, aPadding);
  }
}

void nsStyleSpacing::CalcBorderFor(const nsIFrame* aFrame, nsMargin& aBorder) const
{
  if (mHasCachedBorder) {
    aBorder = mCachedBorder;
  }
  else {
    CalcSidesFor(aFrame, mBorder, NS_SPACING_BORDER, kBorderWidths, 3, aBorder);
  }
}

void nsStyleSpacing::CalcBorderPaddingFor(const nsIFrame* aFrame, nsMargin& aBorderPadding) const
{
  if (mHasCachedPadding && mHasCachedBorder) {
    aBorderPadding = mCachedBorderPadding;
  }
  else {
    nsMargin border;
    CalcBorderFor(aFrame, border);
    CalcPaddingFor(aFrame, aBorderPadding);
    aBorderPadding += border;
  }
}

PRBool nsStyleSpacing::GetMargin(nsMargin& aMargin) const
{
  if (mHasCachedMargin) {
    aMargin = mCachedMargin;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool nsStyleSpacing::GetPadding(nsMargin& aPadding) const
{
  if (mHasCachedPadding) {
    aPadding = mCachedPadding;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool nsStyleSpacing::GetBorder(nsMargin& aBorder) const
{
  if (mHasCachedBorder) {
    aBorder = mCachedBorder;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool nsStyleSpacing::GetBorderPadding(nsMargin& aBorderPadding) const
{
  if (mHasCachedPadding && mHasCachedBorder) {
    aBorderPadding = mCachedBorderPadding;
    return PR_TRUE;
  }
  return PR_FALSE;
}

#define BORDER_COLOR_DEFINED  0x80  
#define BORDER_COLOR_SPECIAL  0x40  
#define BORDER_STYLE_MASK     0x3F


PRUint8 nsStyleSpacing::GetBorderStyle(PRUint8 aSide) const
{
  NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
  return (mBorderStyle[aSide] & BORDER_STYLE_MASK); 
}

void nsStyleSpacing::SetBorderStyle(PRUint8 aSide, PRUint8 aStyle)
{
  NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
  mBorderStyle[aSide] &= ~BORDER_STYLE_MASK; 
  mBorderStyle[aSide] |= (aStyle & BORDER_STYLE_MASK);

}

PRBool nsStyleSpacing::GetBorderColor(PRUint8 aSide, nscolor& aColor) const
{
  NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
  if ((mBorderStyle[aSide] & BORDER_COLOR_SPECIAL) == 0) {
    aColor = mBorderColor[aSide]; 
    return PR_TRUE;
  }
  return PR_FALSE;
}

void nsStyleSpacing::SetBorderColor(PRUint8 aSide, nscolor aColor) 
{
  NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
  mBorderColor[aSide] = aColor; 
  mBorderStyle[aSide] &= ~BORDER_COLOR_SPECIAL;
  mBorderStyle[aSide] |= BORDER_COLOR_DEFINED; 
}

void nsStyleSpacing::SetBorderTransparent(PRUint8 aSide)
{
  NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
  mBorderStyle[aSide] |= (BORDER_COLOR_DEFINED | BORDER_COLOR_SPECIAL); 
}

void nsStyleSpacing::UnsetBorderColor(PRUint8 aSide)
{
  NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
  mBorderStyle[aSide] &= BORDER_STYLE_MASK; 
}

PRBool nsStyleSpacing::GetOutlineWidth(nscoord& aWidth) const
{
  if (mHasCachedOutline) {
    aWidth = mCachedOutlineWidth;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRUint8 nsStyleSpacing::GetOutlineStyle(void) const
{
  return (mOutlineStyle & BORDER_STYLE_MASK);
}

void nsStyleSpacing::SetOutlineStyle(PRUint8 aStyle)
{
  mOutlineStyle &= ~BORDER_STYLE_MASK;
  mOutlineStyle |= (aStyle & BORDER_STYLE_MASK);
}

PRBool nsStyleSpacing::GetOutlineColor(nscolor& aColor) const
{
  if ((mOutlineStyle & BORDER_COLOR_SPECIAL) == 0) {
    aColor = mOutlineColor;
    return PR_TRUE;
  }
  return PR_FALSE;
}

void nsStyleSpacing::SetOutlineColor(nscolor aColor)
{
  mOutlineColor = aColor;
  mOutlineStyle &= ~BORDER_COLOR_SPECIAL;
  mOutlineStyle |= BORDER_COLOR_DEFINED;
}

void nsStyleSpacing::SetOutlineInvert(void)
{
  mOutlineStyle |= (BORDER_COLOR_DEFINED | BORDER_COLOR_SPECIAL);
}



struct StyleSpacingImpl: public nsStyleSpacing {
  StyleSpacingImpl(void)
    : nsStyleSpacing()
  {}

  void ResetFrom(const nsStyleSpacing* aParent, nsIPresContext* aPresContext);
  void SetFrom(const nsStyleSpacing& aSource);
  void CopyTo(nsStyleSpacing& aDest) const;
  void RecalcData(nsIPresContext* aPresContext, nscolor color);
  PRInt32 CalcDifference(const StyleSpacingImpl& aOther) const;
};

void StyleSpacingImpl::ResetFrom(const nsStyleSpacing* aParent, nsIPresContext* aPresContext)
{
  // XXX support kBorderWidhts until deprecated methods are removed
  if (! kWidthsInitialized) {
    float pixelsToTwips = 20.0f;
    if (aPresContext) {
      aPresContext->GetPixelsToTwips(&pixelsToTwips);
    }
    kBorderWidths[NS_STYLE_BORDER_WIDTH_THIN] = NSIntPixelsToTwips(1, pixelsToTwips);
    kBorderWidths[NS_STYLE_BORDER_WIDTH_MEDIUM] = NSIntPixelsToTwips(3, pixelsToTwips);
    kBorderWidths[NS_STYLE_BORDER_WIDTH_THICK] = NSIntPixelsToTwips(5, pixelsToTwips);
    kWidthsInitialized = PR_TRUE;
  }


  // spacing values not inherited
  mMargin.Reset();
  mPadding.Reset();
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
  mOutlineRadius.Reset();

  mOutlineWidth = medium;
  mOutlineStyle = NS_STYLE_BORDER_STYLE_NONE;
  mOutlineColor = NS_RGB(0, 0, 0);

  mFloatEdge = NS_STYLE_FLOAT_EDGE_CONTENT;
  
  mHasCachedMargin = PR_FALSE;
  mHasCachedPadding = PR_FALSE;
  mHasCachedBorder = PR_FALSE;
  mHasCachedOutline = PR_FALSE;
}

void StyleSpacingImpl::SetFrom(const nsStyleSpacing& aSource)
{
  nsCRT::memcpy((nsStyleSpacing*)this, &aSource, sizeof(nsStyleSpacing));
}

void StyleSpacingImpl::CopyTo(nsStyleSpacing& aDest) const
{
  nsCRT::memcpy(&aDest, (const nsStyleSpacing*)this, sizeof(nsStyleSpacing));
}

inline PRBool IsFixedUnit(nsStyleUnit aUnit, PRBool aEnumOK)
{
  return PRBool((aUnit == eStyleUnit_Null) || 
                (aUnit == eStyleUnit_Coord) || 
                (aEnumOK && (aUnit == eStyleUnit_Enumerated)));
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

void StyleSpacingImpl::RecalcData(nsIPresContext* aPresContext, nscolor aColor)
{
  nscoord borderWidths[3];
  float pixelsToTwips = 20.0f;
  if (aPresContext) {
    aPresContext->GetPixelsToTwips(&pixelsToTwips);
  }
  borderWidths[NS_STYLE_BORDER_WIDTH_THIN] = NSIntPixelsToTwips(1, pixelsToTwips);
  borderWidths[NS_STYLE_BORDER_WIDTH_MEDIUM] = NSIntPixelsToTwips(3, pixelsToTwips);
  borderWidths[NS_STYLE_BORDER_WIDTH_THICK] = NSIntPixelsToTwips(5, pixelsToTwips);

  if (IsFixedData(mMargin, PR_FALSE)) {
    nsStyleCoord  coord;
    mCachedMargin.left = CalcCoord(mMargin.GetLeft(coord), nsnull, 0);
    mCachedMargin.top = CalcCoord(mMargin.GetTop(coord), nsnull, 0);
    mCachedMargin.right = CalcCoord(mMargin.GetRight(coord), nsnull, 0);
    mCachedMargin.bottom = CalcCoord(mMargin.GetBottom(coord), nsnull, 0);

    mHasCachedMargin = PR_TRUE;
  }
  else {
    mHasCachedMargin = PR_FALSE;
  }

  if (IsFixedData(mPadding, PR_FALSE)) {
    nsStyleCoord  coord;
    mCachedPadding.left = CalcCoord(mPadding.GetLeft(coord), nsnull, 0);
    mCachedPadding.top = CalcCoord(mPadding.GetTop(coord), nsnull, 0);
    mCachedPadding.right = CalcCoord(mPadding.GetRight(coord), nsnull, 0);
    mCachedPadding.bottom = CalcCoord(mPadding.GetBottom(coord), nsnull, 0);

    mHasCachedPadding = PR_TRUE;
  }
  else {
    mHasCachedPadding = PR_FALSE;
  }

  if (((NS_STYLE_BORDER_STYLE_NONE == GetBorderStyle(NS_SIDE_LEFT))|| 
       IsFixedUnit(mBorder.GetLeftUnit(), PR_TRUE)) &&
      ((NS_STYLE_BORDER_STYLE_NONE == GetBorderStyle(NS_SIDE_TOP)) || 
       IsFixedUnit(mBorder.GetTopUnit(), PR_TRUE)) &&
      ((NS_STYLE_BORDER_STYLE_NONE == GetBorderStyle(NS_SIDE_RIGHT)) || 
       IsFixedUnit(mBorder.GetRightUnit(), PR_TRUE)) &&
      ((NS_STYLE_BORDER_STYLE_NONE == GetBorderStyle(NS_SIDE_BOTTOM)) || 
       IsFixedUnit(mBorder.GetBottomUnit(), PR_TRUE))) {
    nsStyleCoord  coord;
    if (NS_STYLE_BORDER_STYLE_NONE == GetBorderStyle(NS_SIDE_LEFT)) {
      mCachedBorder.left = 0;
    }
    else {
      mCachedBorder.left = CalcCoord(mBorder.GetLeft(coord), borderWidths, 3);
    }
    if (NS_STYLE_BORDER_STYLE_NONE == GetBorderStyle(NS_SIDE_TOP)) {
      mCachedBorder.top = 0;
    }
    else {
      mCachedBorder.top = CalcCoord(mBorder.GetTop(coord), borderWidths, 3);
    }
    if (NS_STYLE_BORDER_STYLE_NONE == GetBorderStyle(NS_SIDE_RIGHT)) {
      mCachedBorder.right = 0;
    }
    else {
      mCachedBorder.right = CalcCoord(mBorder.GetRight(coord), borderWidths, 3);
    }
    if (NS_STYLE_BORDER_STYLE_NONE == GetBorderStyle(NS_SIDE_BOTTOM)) {
      mCachedBorder.bottom = 0;
    }
    else {
      mCachedBorder.bottom = CalcCoord(mBorder.GetBottom(coord), borderWidths, 3);
    }

    mHasCachedBorder = PR_TRUE;
  }
  else {
    mHasCachedBorder = PR_FALSE;
  }

  if (mHasCachedBorder && mHasCachedPadding) {
    mCachedBorderPadding = mCachedPadding;
    mCachedBorderPadding += mCachedBorder;
  }
  
  if ((mBorderStyle[NS_SIDE_TOP] & BORDER_COLOR_DEFINED) == 0) {
    mBorderColor[NS_SIDE_TOP] = aColor;
  }
  if ((mBorderStyle[NS_SIDE_BOTTOM] & BORDER_COLOR_DEFINED) == 0) {
    mBorderColor[NS_SIDE_BOTTOM] = aColor;
  }
  if ((mBorderStyle[NS_SIDE_LEFT]& BORDER_COLOR_DEFINED) == 0) {
    mBorderColor[NS_SIDE_LEFT] = aColor;
  }
  if ((mBorderStyle[NS_SIDE_RIGHT] & BORDER_COLOR_DEFINED) == 0) {
    mBorderColor[NS_SIDE_RIGHT] = aColor;
  }

  if ((NS_STYLE_BORDER_STYLE_NONE == GetOutlineStyle()) || 
      IsFixedUnit(mOutlineWidth.GetUnit(), PR_TRUE)) {
    if (NS_STYLE_BORDER_STYLE_NONE == GetOutlineStyle()) {
      mCachedOutlineWidth = 0;
    }
    else {
      mCachedOutlineWidth = CalcCoord(mOutlineWidth, borderWidths, 3);
    }
    mHasCachedOutline = PR_TRUE;
  }
  else {
    mHasCachedOutline = PR_FALSE;
  }
}

PRInt32 StyleSpacingImpl::CalcDifference(const StyleSpacingImpl& aOther) const
{
  if ((mMargin == aOther.mMargin) && 
      (mPadding == aOther.mPadding) && 
      (mBorder == aOther.mBorder) && 
      (mFloatEdge == aOther.mFloatEdge)) {
    PRInt32 ix;
    for (ix = 0; ix < 4; ix++) {
      if ((mBorderStyle[ix] != aOther.mBorderStyle[ix]) || 
          (mBorderColor[ix] != aOther.mBorderColor[ix])) {
        if ((mBorderStyle[ix] != aOther.mBorderStyle[ix]) &&
            ((NS_STYLE_BORDER_STYLE_NONE == mBorderStyle[ix]) ||
             (NS_STYLE_BORDER_STYLE_NONE == aOther.mBorderStyle[ix]))) {
          return NS_STYLE_HINT_REFLOW;  // border on or off
        }
        return NS_STYLE_HINT_VISUAL;
      }
    }
    if (mBorderRadius != aOther.mBorderRadius) {
      return NS_STYLE_HINT_VISUAL;
    }
    if ((mOutlineWidth != aOther.mOutlineWidth) ||
        (mOutlineStyle != aOther.mOutlineStyle) ||
        (mOutlineColor != aOther.mOutlineColor) ||
        (mOutlineRadius != aOther.mOutlineRadius)) {
      return NS_STYLE_HINT_VISUAL;	// XXX: should be VISUAL: see bugs 9809 and 9816
    }
    return NS_STYLE_HINT_NONE;
  }
  return NS_STYLE_HINT_REFLOW;
}


// --------------------
// nsStyleList
//
nsStyleList::nsStyleList(void) { }
nsStyleList::~nsStyleList(void) { }

struct StyleListImpl: public nsStyleList {
  StyleListImpl(void) { }

  void ResetFrom(const nsStyleList* aParent, nsIPresContext* aPresContext);
  void SetFrom(const nsStyleList& aSource);
  void CopyTo(nsStyleList& aDest) const;
  PRInt32 CalcDifference(const StyleListImpl& aOther) const;
};

void StyleListImpl::ResetFrom(const nsStyleList* aParent, nsIPresContext* aPresContext)
{
  if (nsnull != aParent) {
    mListStyleType = aParent->mListStyleType;
    mListStyleImage = aParent->mListStyleImage;
    mListStylePosition = aParent->mListStylePosition;
  }
  else {
    mListStyleType = NS_STYLE_LIST_STYLE_BASIC;
    mListStylePosition = NS_STYLE_LIST_STYLE_POSITION_OUTSIDE;
    mListStyleImage.Truncate();
  }
}

void StyleListImpl::SetFrom(const nsStyleList& aSource)
{
  mListStyleType = aSource.mListStyleType;
  mListStylePosition = aSource.mListStylePosition;
  mListStyleImage = aSource.mListStyleImage;
}

void StyleListImpl::CopyTo(nsStyleList& aDest) const
{
  aDest.mListStyleType = mListStyleType;
  aDest.mListStylePosition = mListStylePosition;
  aDest.mListStyleImage = mListStyleImage;
}

PRInt32 StyleListImpl::CalcDifference(const StyleListImpl& aOther) const
{
  if (mListStylePosition == aOther.mListStylePosition) {
    if (mListStyleImage == aOther.mListStyleImage) {
      if (mListStyleType == aOther.mListStyleType) {
        return NS_STYLE_HINT_NONE;
      }
      return NS_STYLE_HINT_REFLOW;
    }
    return NS_STYLE_HINT_REFLOW;
  }
  return NS_STYLE_HINT_REFLOW;
}


// --------------------
// nsStylePosition
//
nsStylePosition::nsStylePosition(void) { }

struct StylePositionImpl: public nsStylePosition {
  StylePositionImpl(void) { }

  void ResetFrom(const nsStylePosition* aParent, nsIPresContext* aPresContext);
  void SetFrom(const nsStylePosition& aSource);
  void CopyTo(nsStylePosition& aDest) const;
  PRInt32 CalcDifference(const StylePositionImpl& aOther) const;

private:  // These are not allowed
  StylePositionImpl(const StylePositionImpl& aOther);
  StylePositionImpl& operator=(const StylePositionImpl& aOther);
};

void StylePositionImpl::ResetFrom(const nsStylePosition* aParent, nsIPresContext* aPresContext)
{
  // positioning values not inherited
  mPosition = NS_STYLE_POSITION_NORMAL;
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

void StylePositionImpl::SetFrom(const nsStylePosition& aSource)
{
  nsCRT::memcpy((nsStylePosition*)this, &aSource, sizeof(nsStylePosition));
}

void StylePositionImpl::CopyTo(nsStylePosition& aDest) const
{
  nsCRT::memcpy(&aDest, (const nsStylePosition*)this, sizeof(nsStylePosition));
}

PRInt32 StylePositionImpl::CalcDifference(const StylePositionImpl& aOther) const
{
  if (mPosition == aOther.mPosition) {
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
  return NS_STYLE_HINT_FRAMECHANGE;
}

// --------------------
// nsStyleText
//

nsStyleText::nsStyleText(void) { }

struct StyleTextImpl: public nsStyleText {
  StyleTextImpl(void) { }

  void ResetFrom(const nsStyleText* aParent, nsIPresContext* aPresContext);
  void SetFrom(const nsStyleText& aSource);
  void CopyTo(nsStyleText& aDest) const;
  PRInt32 CalcDifference(const StyleTextImpl& aOther) const;
};

void StyleTextImpl::ResetFrom(const nsStyleText* aParent, nsIPresContext* aPresContext)
{
  // These properties not inherited
  mTextDecoration = NS_STYLE_TEXT_DECORATION_NONE;
  mVerticalAlign.SetIntValue(NS_STYLE_VERTICAL_ALIGN_BASELINE, eStyleUnit_Enumerated);
//  mVerticalAlign.Reset(); TBI

  if (nsnull != aParent) {
    mTextAlign = aParent->mTextAlign;
    mTextTransform = aParent->mTextTransform;
    mWhiteSpace = aParent->mWhiteSpace;
    mLetterSpacing = aParent->mLetterSpacing;

    // Inherit everything except percentage line-height values
    nsStyleUnit unit = aParent->mLineHeight.GetUnit();
    if ((eStyleUnit_Normal == unit) || (eStyleUnit_Factor == unit) ||
        (eStyleUnit_Coord == unit)) {
      mLineHeight = aParent->mLineHeight;
    }
    else {
      mLineHeight.SetInheritValue();
    }
    mTextIndent = aParent->mTextIndent;
    mWordSpacing = aParent->mWordSpacing;
  }
  else {
    mTextAlign = NS_STYLE_TEXT_ALIGN_DEFAULT;
    mTextTransform = NS_STYLE_TEXT_TRANSFORM_NONE;
    mWhiteSpace = NS_STYLE_WHITESPACE_NORMAL;

    mLetterSpacing.SetNormalValue();
    mLineHeight.SetNormalValue();
    mTextIndent.SetCoordValue(0);
    mWordSpacing.SetNormalValue();
  }
}

void StyleTextImpl::SetFrom(const nsStyleText& aSource)
{
  nsCRT::memcpy((nsStyleText*)this, &aSource, sizeof(nsStyleText));
}

void StyleTextImpl::CopyTo(nsStyleText& aDest) const
{
  nsCRT::memcpy(&aDest, (const nsStyleText*)this, sizeof(nsStyleText));
}

PRInt32 StyleTextImpl::CalcDifference(const StyleTextImpl& aOther) const
{
  if ((mTextAlign == aOther.mTextAlign) &&
      (mTextTransform == aOther.mTextTransform) &&
      (mWhiteSpace == aOther.mWhiteSpace) &&
      (mLetterSpacing == aOther.mLetterSpacing) &&
      (mLineHeight == aOther.mLineHeight) &&
      (mTextIndent == aOther.mTextIndent) &&
      (mWordSpacing == aOther.mWordSpacing) &&
      (mVerticalAlign == aOther.mVerticalAlign)) {
    if (mTextDecoration == aOther.mTextDecoration) {
      return NS_STYLE_HINT_NONE;
    }
    return NS_STYLE_HINT_VISUAL;
  }
  return NS_STYLE_HINT_REFLOW;
}

// --------------------
// nsStyleDisplay
//

nsStyleDisplay::nsStyleDisplay(void) { }

struct StyleDisplayImpl: public nsStyleDisplay {
  StyleDisplayImpl(void) { }

  void ResetFrom(const nsStyleDisplay* aParent, nsIPresContext* aPresContext);
  void SetFrom(const nsStyleDisplay& aSource);
  void CopyTo(nsStyleDisplay& aDest) const;
  PRInt32 CalcDifference(const StyleDisplayImpl& aOther) const;
};

void StyleDisplayImpl::ResetFrom(const nsStyleDisplay* aParent, nsIPresContext* aPresContext)
{
  if (nsnull != aParent) {
    mDirection = aParent->mDirection;
    mVisible = aParent->mVisible;
  }
  else {
    aPresContext->GetDefaultDirection(&mDirection);
    mVisible = NS_STYLE_VISIBILITY_VISIBLE;
  }
  mDisplay = NS_STYLE_DISPLAY_INLINE;
  mFloats = NS_STYLE_FLOAT_NONE;
  mBreakType = NS_STYLE_CLEAR_NONE;
  mBreakBefore = PR_FALSE;
  mBreakAfter = PR_FALSE;
  mOverflow = NS_STYLE_OVERFLOW_VISIBLE;
  mClipFlags = NS_STYLE_CLIP_AUTO;
  mClip.SizeTo(0,0,0,0);
}

void StyleDisplayImpl::SetFrom(const nsStyleDisplay& aSource)
{
  nsCRT::memcpy((nsStyleDisplay*)this, &aSource, sizeof(nsStyleDisplay));
}

void StyleDisplayImpl::CopyTo(nsStyleDisplay& aDest) const
{
  nsCRT::memcpy(&aDest, (const nsStyleDisplay*)this, sizeof(nsStyleDisplay));
}

PRInt32 StyleDisplayImpl::CalcDifference(const StyleDisplayImpl& aOther) const
{
  if ((mDisplay == aOther.mDisplay) &&
      (mFloats == aOther.mFloats) &&
      (mOverflow == aOther.mOverflow)) {
    if ((mDirection == aOther.mDirection) &&
        (mBreakType == aOther.mBreakType) &&
        (mBreakBefore == aOther.mBreakBefore) &&
        (mBreakAfter == aOther.mBreakAfter)) {
      if ((mVisible == aOther.mVisible) &&
          (mClipFlags == aOther.mClipFlags) &&
          (mClip == aOther.mClip)) {
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
  return NS_STYLE_HINT_FRAMECHANGE;
}

// --------------------
// nsStyleTable
//

nsStyleTable::nsStyleTable(void) { }

struct StyleTableImpl: public nsStyleTable {
  StyleTableImpl(void);

  void ResetFrom(const nsStyleTable* aParent, nsIPresContext* aPresContext);
  void SetFrom(const nsStyleTable& aSource);
  void CopyTo(nsStyleTable& aDest) const;
  PRInt32 CalcDifference(const StyleTableImpl& aOther) const;
};

StyleTableImpl::StyleTableImpl()
{ 
  ResetFrom(nsnull, nsnull);
}

void StyleTableImpl::ResetFrom(const nsStyleTable* aParent, nsIPresContext* aPresContext)
{
  // values not inherited
  mLayoutStrategy = NS_STYLE_TABLE_LAYOUT_AUTO;
  mCols  = NS_STYLE_TABLE_COLS_NONE;
  mFrame = NS_STYLE_TABLE_FRAME_NONE;
  mRules = NS_STYLE_TABLE_RULES_ALL;
  mCellPadding.Reset();
  mSpan = 1;

  if (aParent) {  // handle inherited properties
    mBorderCollapse = aParent->mBorderCollapse;
    mEmptyCells     = aParent->mEmptyCells;
    mCaptionSide    = aParent->mCaptionSide;
    mBorderSpacingX = aParent->mBorderSpacingX;
    mBorderSpacingY = aParent->mBorderSpacingY;
    mSpanWidth      = aParent->mSpanWidth;
  }
  else {
    mBorderCollapse = NS_STYLE_BORDER_SEPARATE;

    nsCompatibility compatMode = eCompatibility_Standard;
    if (aPresContext) {
		  aPresContext->GetCompatibilityMode(&compatMode);
		}
    mEmptyCells = (compatMode == eCompatibility_NavQuirks
                    ? NS_STYLE_TABLE_EMPTY_CELLS_HIDE     // bug 33244
                    : NS_STYLE_TABLE_EMPTY_CELLS_SHOW);

    mCaptionSide = NS_SIDE_TOP;
    mBorderSpacingX.Reset();
    mBorderSpacingY.Reset();
    mSpanWidth.Reset();
  }
}

void StyleTableImpl::SetFrom(const nsStyleTable& aSource)
{
  nsCRT::memcpy((nsStyleTable*)this, &aSource, sizeof(nsStyleTable));
}

void StyleTableImpl::CopyTo(nsStyleTable& aDest) const
{
  nsCRT::memcpy(&aDest, (const nsStyleTable*)this, sizeof(nsStyleTable));
}

PRInt32 StyleTableImpl::CalcDifference(const StyleTableImpl& aOther) const
{
  if ((mLayoutStrategy == aOther.mLayoutStrategy) &&
      (mFrame == aOther.mFrame) &&
      (mRules == aOther.mRules) &&
      (mBorderCollapse == aOther.mBorderCollapse) &&
      (mBorderSpacingX == aOther.mBorderSpacingX) &&
      (mBorderSpacingY == aOther.mBorderSpacingY) &&
      (mCellPadding == aOther.mCellPadding) &&
      (mCaptionSide == aOther.mCaptionSide) &&
      (mCols == aOther.mCols) &&
      (mSpan == aOther.mSpan) &&
      (mSpanWidth == aOther.mSpanWidth)) {
    if (mEmptyCells == aOther.mEmptyCells) {
      return NS_STYLE_HINT_NONE;
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
    mResets(nsnull),
    mQuotesCount(0),
    mQuotes(nsnull)
{
}

nsStyleContent::~nsStyleContent(void)
{
  DELETE_ARRAY_IF(mContents);
  DELETE_ARRAY_IF(mIncrements);
  DELETE_ARRAY_IF(mResets);
  DELETE_ARRAY_IF(mQuotes);
}

nsresult 
nsStyleContent::GetContentAt(PRUint32 aIndex, nsStyleContentType& aType, nsString& aContent) const
{
  if (aIndex < mContentCount) {
    aType = mContents[aIndex].mType;
    aContent = mContents[aIndex].mContent;
    return NS_OK;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult 
nsStyleContent::AllocateContents(PRUint32 aCount)
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

nsresult 
nsStyleContent::SetContentAt(PRUint32 aIndex, nsStyleContentType aType, const nsString& aContent)
{
  if (aIndex < mContentCount) {
    mContents[aIndex].mType = aType;
    if (aType < eStyleContentType_OpenQuote) {
      mContents[aIndex].mContent = aContent;
    }
    else {
      mContents[aIndex].mContent.Truncate();
    }
    return NS_OK;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult 
nsStyleContent::GetCounterIncrementAt(PRUint32 aIndex, nsString& aCounter, PRInt32& aIncrement) const
{
  if (aIndex < mIncrementCount) {
    aCounter = mIncrements[aIndex].mCounter;
    aIncrement = mIncrements[aIndex].mValue;
    return NS_OK;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult 
nsStyleContent::AllocateCounterIncrements(PRUint32 aCount)
{
  if (aCount != mIncrementCount) {
    DELETE_ARRAY_IF(mIncrements);
    if (aCount) {
      mIncrements = new nsStyleCounterData[aCount];
      if (! mIncrements) {
        mIncrementCount = 0;
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    mIncrementCount = aCount;
  }
  return NS_OK;
}

nsresult 
nsStyleContent::SetCounterIncrementAt(PRUint32 aIndex, const nsString& aCounter, PRInt32 aIncrement)
{
  if (aIndex < mIncrementCount) {
    mIncrements[aIndex].mCounter = aCounter;
    mIncrements[aIndex].mValue = aIncrement;
    return NS_OK;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult 
nsStyleContent::GetCounterResetAt(PRUint32 aIndex, nsString& aCounter, PRInt32& aValue) const
{
  if (aIndex < mResetCount) {
    aCounter = mResets[aIndex].mCounter;
    aValue = mResets[aIndex].mValue;
    return NS_OK;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult 
nsStyleContent::AllocateCounterResets(PRUint32 aCount)
{
  if (aCount != mResetCount) {
    DELETE_ARRAY_IF(mResets);
    if (aCount) {
      mResets = new nsStyleCounterData[aCount];
      if (! mResets) {
        mResetCount = 0;
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    mResetCount = aCount;
  }
  return NS_OK;
}

nsresult 
nsStyleContent::SetCounterResetAt(PRUint32 aIndex, const nsString& aCounter, PRInt32 aValue)
{
  if (aIndex < mResetCount) {
    mResets[aIndex].mCounter = aCounter;
    mResets[aIndex].mValue = aValue;
    return NS_OK;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult 
nsStyleContent::GetQuotesAt(PRUint32 aIndex, nsString& aOpen, nsString& aClose) const
{
  if (aIndex < mQuotesCount) {
    aIndex *= 2;
    aOpen = mQuotes[aIndex];
    aClose = mQuotes[++aIndex];
    return NS_OK;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult 
nsStyleContent::AllocateQuotes(PRUint32 aCount)
{
  if (aCount != mQuotesCount) {
    DELETE_ARRAY_IF(mQuotes);
    if (aCount) {
      mQuotes = new nsString[aCount * 2];
      if (! mQuotes) {
        mQuotesCount = 0;
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    mQuotesCount = aCount;
  }
  return NS_OK;
}

nsresult 
nsStyleContent::SetQuotesAt(PRUint32 aIndex, const nsString& aOpen, const nsString& aClose)
{
  if (aIndex < mQuotesCount) {
    aIndex *= 2;
    mQuotes[aIndex] = aOpen;
    mQuotes[++aIndex] = aClose;
    return NS_OK;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

struct StyleContentImpl: public nsStyleContent {
  StyleContentImpl(void) : nsStyleContent() { };

  void ResetFrom(const StyleContentImpl* aParent, nsIPresContext* aPresContext);
  void SetFrom(const nsStyleContent& aSource);
  void CopyTo(nsStyleContent& aDest) const;
  PRInt32 CalcDifference(const StyleContentImpl& aOther) const;
};

void
StyleContentImpl::ResetFrom(const StyleContentImpl* aParent, nsIPresContext* aPresContext)
{
  // reset data
  mMarkerOffset.Reset();
  mContentCount = 0;
  DELETE_ARRAY_IF(mContents);
  mIncrementCount = 0;
  DELETE_ARRAY_IF(mIncrements);
  mResetCount = 0;
  DELETE_ARRAY_IF(mResets);

  // inherited data
  if (aParent) {
    if (NS_SUCCEEDED(AllocateQuotes(aParent->mQuotesCount))) {
      PRUint32 ix = (mQuotesCount * 2);
      while (0 < ix--) {
        mQuotes[ix] = aParent->mQuotes[ix];
      }
    }
  }
  else {
    mQuotesCount = 0;
    DELETE_ARRAY_IF(mQuotes);
  }
}

void StyleContentImpl::SetFrom(const nsStyleContent& aSource)
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

  if (NS_SUCCEEDED(AllocateQuotes(aSource.QuotesCount()))) {
    PRUint32 count = (mQuotesCount * 2);
    for (index = 0; index < count; index += 2) {
      aSource.GetQuotesAt(index, mQuotes[index], mQuotes[index + 1]);
    }
  }
}

void StyleContentImpl::CopyTo(nsStyleContent& aDest) const
{
  aDest.mMarkerOffset = mMarkerOffset;

  PRUint32 index;
  if (NS_SUCCEEDED(aDest.AllocateContents(mContentCount))) {
    for (index = 0; index < mContentCount; index++) {
      aDest.SetContentAt(index, mContents[index].mType,
                                mContents[index].mContent);
    }
  }

  if (NS_SUCCEEDED(aDest.AllocateCounterIncrements(mIncrementCount))) {
    for (index = 0; index < mIncrementCount; index++) {
      aDest.SetCounterIncrementAt(index, mIncrements[index].mCounter,
                                         mIncrements[index].mValue);
    }
  }

  if (NS_SUCCEEDED(aDest.AllocateCounterResets(mResetCount))) {
    for (index = 0; index < mResetCount; index++) {
      aDest.SetCounterResetAt(index, mResets[index].mCounter,
                                     mResets[index].mValue);
    }
  }

  if (NS_SUCCEEDED(aDest.AllocateQuotes(mQuotesCount))) {
    PRUint32 count = (mQuotesCount * 2);
    for (index = 0; index < count; index += 2) {
      aDest.SetQuotesAt(index, mQuotes[index], mQuotes[index + 1]);
    }
  }
}

PRInt32 
StyleContentImpl::CalcDifference(const StyleContentImpl& aOther) const
{
  if (mContentCount == aOther.mContentCount) {
    if ((mMarkerOffset == aOther.mMarkerOffset) &&
        (mIncrementCount == aOther.mIncrementCount) && 
        (mResetCount == aOther.mResetCount) &&
        (mQuotesCount == aOther.mQuotesCount)) {
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
      ix = (mQuotesCount * 2);
      while (0 < ix--) {
        if (mQuotes[ix] != aOther.mQuotes[ix]) {
          return NS_STYLE_HINT_REFLOW;
        }
      }
      return NS_STYLE_HINT_NONE;
    }
    return NS_STYLE_HINT_REFLOW;
  }
  return NS_STYLE_HINT_FRAMECHANGE;
}

//-----------------------
// nsStyleUserInterface
//

nsStyleUserInterface::nsStyleUserInterface(void) { }

struct StyleUserInterfaceImpl: public nsStyleUserInterface {
  StyleUserInterfaceImpl(void)  { }

  void ResetFrom(const nsStyleUserInterface* aParent, nsIPresContext* aPresContext);
  void SetFrom(const nsStyleUserInterface& aSource);
  void CopyTo(nsStyleUserInterface& aDest) const;
  PRInt32 CalcDifference(const StyleUserInterfaceImpl& aOther) const;

private:  // These are not allowed
  StyleUserInterfaceImpl(const StyleUserInterfaceImpl& aOther);
  StyleUserInterfaceImpl& operator=(const StyleUserInterfaceImpl& aOther);
};

void StyleUserInterfaceImpl::ResetFrom(const nsStyleUserInterface* aParent, nsIPresContext* aPresContext)
{
  if (aParent) {
    mUserInput = aParent->mUserInput;
    mUserModify = aParent->mUserModify;
    mUserFocus = aParent->mUserFocus;
  }
  else {
    mUserInput = NS_STYLE_USER_INPUT_AUTO;
    mUserModify = NS_STYLE_USER_MODIFY_READ_ONLY;
    mUserFocus = NS_STYLE_USER_FOCUS_NONE;
  }

  mUserSelect = NS_STYLE_USER_SELECT_AUTO;
  mKeyEquivalent = PRUnichar(0); // XXX what type should this be?
  mResizer = NS_STYLE_RESIZER_AUTO;
  mBehavior.SetLength(0);
}

void StyleUserInterfaceImpl::SetFrom(const nsStyleUserInterface& aSource)
{
  mUserInput = aSource.mUserInput;
  mUserModify = aSource.mUserModify;
  mUserFocus = aSource.mUserFocus;

  mUserSelect = aSource.mUserSelect;
  mKeyEquivalent = aSource.mKeyEquivalent;
  mResizer = aSource.mResizer;
  mBehavior = aSource.mBehavior;
}

void StyleUserInterfaceImpl::CopyTo(nsStyleUserInterface& aDest) const
{
  aDest.mUserInput = mUserInput;
  aDest.mUserModify = mUserModify;
  aDest.mUserFocus = mUserFocus;

  aDest.mUserSelect = mUserSelect;
  aDest.mKeyEquivalent = mKeyEquivalent;
  aDest.mResizer = mResizer;
  aDest.mBehavior = mBehavior;
}

PRInt32 StyleUserInterfaceImpl::CalcDifference(const StyleUserInterfaceImpl& aOther) const
{
  if (mBehavior != aOther.mBehavior)
    return NS_STYLE_HINT_FRAMECHANGE;

  if ((mUserInput == aOther.mUserInput) && 
      (mResizer == aOther.mResizer)) {
    if ((mUserModify == aOther.mUserModify) && 
        (mUserSelect == aOther.mUserSelect)) {
      if ((mKeyEquivalent == aOther.mKeyEquivalent) &&
          (mUserFocus == aOther.mUserFocus) &&
          (mResizer == aOther.mResizer)) {
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
// nsStylePrint
//

nsStylePrint::nsStylePrint(void) { }

struct StylePrintImpl: public nsStylePrint {
  StylePrintImpl(void)  { }

  void ResetFrom(const nsStylePrint* aParent, nsIPresContext* aPresContext);
  void SetFrom(const nsStylePrint& aSource);
  void CopyTo(nsStylePrint& aDest) const;
  PRInt32 CalcDifference(const StylePrintImpl& aOther) const;

private:  // These are not allowed
  StylePrintImpl(const StylePrintImpl& aOther);
  StylePrintImpl& operator=(const StylePrintImpl& aOther);
};

void StylePrintImpl::ResetFrom(const nsStylePrint* aParent, nsIPresContext* aPresContext)
{
  if (aParent) {
    mPageBreakBefore = aParent->mPageBreakBefore;
    mPageBreakAfter = aParent->mPageBreakAfter;
    mPageBreakInside = aParent->mPageBreakInside;
    mWidows = aParent->mWidows;
    mOrphans = aParent->mOrphans;
		mMarks = aParent->mMarks;
		mSizeWidth = aParent->mSizeWidth;
		mSizeHeight = aParent->mSizeHeight;
  }
  else {
    mPageBreakBefore = NS_STYLE_PAGE_BREAK_AUTO;
    mPageBreakAfter = NS_STYLE_PAGE_BREAK_AUTO;
    mPageBreakInside = NS_STYLE_PAGE_BREAK_AUTO;
    mWidows = 2;
    mOrphans = 2;
		mMarks = NS_STYLE_PAGE_MARKS_NONE;
		mSizeWidth.SetAutoValue();
		mSizeHeight.SetAutoValue();
  }
}

void StylePrintImpl::SetFrom(const nsStylePrint& aSource)
{
  nsCRT::memcpy((nsStylePrint*)this, &aSource, sizeof(nsStylePrint));
}

void StylePrintImpl::CopyTo(nsStylePrint& aDest) const
{
  nsCRT::memcpy(&aDest, (const nsStylePrint*)this, sizeof(nsStylePrint));
}

PRInt32 StylePrintImpl::CalcDifference(const StylePrintImpl& aOther) const
{
	if ((mPageBreakBefore == aOther.mPageBreakBefore)
	&& (mPageBreakAfter == aOther.mPageBreakAfter)
	&& (mPageBreakInside == aOther.mPageBreakInside)
	&& (mWidows == aOther.mWidows)
	&& (mOrphans == aOther.mOrphans)
	&& (mMarks == aOther.mMarks)
	&& (mSizeWidth == aOther.mSizeWidth)
	&& (mSizeHeight == aOther.mSizeHeight)) {
  	return NS_STYLE_HINT_NONE;
	}

	if (mMarks != aOther.mMarks) {
  	return NS_STYLE_HINT_VISUAL;
	}
  return NS_STYLE_HINT_REFLOW;
}

//----------------------------------------------------------------------

class StyleContextImpl : public nsIStyleContext,
                         protected nsIMutableStyleContext { // you can't QI to nsIMutableStyleContext
public:
  StyleContextImpl(nsIStyleContext* aParent, nsIAtom* aPseudoTag, 
                   nsISupportsArray* aRules, 
                   nsIPresContext* aPresContext);
  virtual ~StyleContextImpl();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  virtual nsIStyleContext*  GetParent(void) const;
  virtual nsISupportsArray* GetStyleRules(void) const;
  virtual PRInt32 GetStyleRuleCount(void) const;
  NS_IMETHOD GetPseudoType(nsIAtom*& aPseudoTag) const;

  NS_IMETHOD FindChildWithRules(const nsIAtom* aPseudoTag, nsISupportsArray* aRules,
                                nsIStyleContext*& aResult);

  virtual PRBool    Equals(const nsIStyleContext* aOther) const;
  virtual PRUint32  HashValue(void) const;

  NS_IMETHOD RemapStyle(nsIPresContext* aPresContext, PRBool aRecurse = PR_TRUE);

  NS_IMETHOD GetStyle(nsStyleStructID aSID, nsStyleStruct& aStruct) const;
  NS_IMETHOD SetStyle(nsStyleStructID aSID, const nsStyleStruct& aStruct);

  virtual const nsStyleStruct* GetStyleData(nsStyleStructID aSID);
  virtual nsStyleStruct* GetMutableStyleData(nsStyleStructID aSID);

  virtual void ForceUnique(void);
  virtual void RecalcAutomaticData(nsIPresContext* aPresContext);
  NS_IMETHOD  CalcStyleDifference(nsIStyleContext* aOther, PRInt32& aHint) const;

  virtual void  List(FILE* out, PRInt32 aIndent);

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);

protected:
  void AppendChild(StyleContextImpl* aChild);
  void RemoveChild(StyleContextImpl* aChild);

  StyleContextImpl* mParent;
  StyleContextImpl* mChild;
  StyleContextImpl* mEmptyChild;
  StyleContextImpl* mPrevSibling;
  StyleContextImpl* mNextSibling;

  nsIAtom*          mPseudoTag;

  PRUint32          mRuleHash;
  nsISupportsArray* mRules;
  PRInt32           mDataCode;

  // the style data...
  StyleFontImpl           mFont;
  StyleColorImpl          mColor;
  StyleSpacingImpl        mSpacing;
  StyleListImpl           mList;
  StylePositionImpl       mPosition;
  StyleTextImpl           mText;
  StyleDisplayImpl        mDisplay;
  StyleTableImpl          mTable;
  StyleContentImpl        mContent;
  StyleUserInterfaceImpl  mUserInterface;
	StylePrintImpl					mPrint;

  nsCOMPtr<nsIStyleSet>   mStyleSet;
};

static PRInt32 gLastDataCode;

static PRBool HashStyleRule(nsISupports* aRule, void* aData)
{
  *((PRUint32*)aData) ^= PRUint32(aRule);
  return PR_TRUE;
}

StyleContextImpl::StyleContextImpl(nsIStyleContext* aParent,
                                   nsIAtom* aPseudoTag,
                                   nsISupportsArray* aRules, 
                                   nsIPresContext* aPresContext)
  : mParent((StyleContextImpl*)aParent),
    mChild(nsnull),
    mEmptyChild(nsnull),
    mPseudoTag(aPseudoTag),
    mRules(aRules),
    mDataCode(-1),
    mFont(aPresContext->GetDefaultFontDeprecated(), aPresContext->GetDefaultFixedFontDeprecated()),
    mColor(),
    mSpacing(),
    mList(),
    mPosition(),
    mText(),
    mDisplay(),
    mTable(),
    mContent(),
    mUserInterface(),
    mPrint()
{
  NS_INIT_REFCNT();
  NS_IF_ADDREF(mPseudoTag);
  NS_IF_ADDREF(mRules);

  mNextSibling = this;
  mPrevSibling = this;
  if (nsnull != mParent) {
    NS_ADDREF(mParent);
    mParent->AppendChild(this);
  }

  mRuleHash = 0;
  if (nsnull != mRules) {
    mRules->EnumerateForwards(HashStyleRule, &mRuleHash);
  }

#ifdef DEBUG_SC_SHARING
  // add this instance to the style set
  nsIPresShell* shell = nsnull;
  aPresContext->GetShell(&shell);
  if (shell) {
    shell->GetStyleSet( getter_AddRefs(mStyleSet) );
    if (mStyleSet) {
      // add it to the set: the set does NOT addref or release
      // NOTE: QI here is not a good idea - this is the constructor so we are not whole yet...
      mStyleSet->AddStyleContext((nsIStyleContext *)this);
    }
    NS_RELEASE( shell );
  }
#endif // DEBUG_SC_SHARING
}

StyleContextImpl::~StyleContextImpl()
{
  NS_ASSERTION((nsnull == mChild) && (nsnull == mEmptyChild), "destructing context with children");

  if (mParent) {
    mParent->RemoveChild(this);
    NS_RELEASE(mParent);
  }

  NS_IF_RELEASE(mPseudoTag);
  NS_IF_RELEASE(mRules);

#ifdef DEBUG_SC_SHARING
  // remove this instance from the style set (remember, the set does not AddRef or Release us)
  if (mStyleSet) {
    mStyleSet->RemoveStyleContext((nsIStyleContext *)this);
  }
#endif // DEBUG_SC_SHARING
}

NS_IMPL_ADDREF(StyleContextImpl)
NS_IMPL_RELEASE(StyleContextImpl)

NS_IMETHODIMP
StyleContextImpl::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null pointer");
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(NS_GET_IID(nsIStyleContext))) {
    *aInstancePtr = (void*)(nsIStyleContext*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) (nsISupports*)(nsIStyleContext*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsIStyleContext* StyleContextImpl::GetParent(void) const
{
  NS_IF_ADDREF(mParent);
  return mParent;
}

void StyleContextImpl::AppendChild(StyleContextImpl* aChild)
{
  if (0 == aChild->GetStyleRuleCount()) {
    if (nsnull == mEmptyChild) {
      mEmptyChild = aChild;
    }
    else {
      aChild->mNextSibling = mEmptyChild;
      aChild->mPrevSibling = mEmptyChild->mPrevSibling;
      mEmptyChild->mPrevSibling->mNextSibling = aChild;
      mEmptyChild->mPrevSibling = aChild;
    }
  }
  else {
    if (nsnull == mChild) {
      mChild = aChild;
    }
    else {
      aChild->mNextSibling = mChild;
      aChild->mPrevSibling = mChild->mPrevSibling;
      mChild->mPrevSibling->mNextSibling = aChild;
      mChild->mPrevSibling = aChild;
    }
  }
}

void StyleContextImpl::RemoveChild(StyleContextImpl* aChild)
{
  NS_ASSERTION((nsnull != aChild) && (this == aChild->mParent), "bad argument");

  if ((nsnull == aChild) || (this != aChild->mParent)) {
    return;
  }

  if (0 == aChild->GetStyleRuleCount()) { // is empty 
    if (aChild->mPrevSibling != aChild) { // has siblings
      if (mEmptyChild == aChild) {
        mEmptyChild = mEmptyChild->mNextSibling;
      }
    } 
    else {
      NS_ASSERTION(mEmptyChild == aChild, "bad sibling pointers");
      mEmptyChild = nsnull;
    }
  }
  else {  // isn't empty
    if (aChild->mPrevSibling != aChild) { // has siblings
      if (mChild == aChild) {
        mChild = mChild->mNextSibling;
      }
    }
    else {
      NS_ASSERTION(mChild == aChild, "bad sibling pointers");
      if (mChild == aChild) {
        mChild = nsnull;
      }
    }
  }
  aChild->mPrevSibling->mNextSibling = aChild->mNextSibling;
  aChild->mNextSibling->mPrevSibling = aChild->mPrevSibling;
  aChild->mNextSibling = aChild;
  aChild->mPrevSibling = aChild;
}

nsISupportsArray* StyleContextImpl::GetStyleRules(void) const
{
  nsISupportsArray* result = mRules;
  NS_IF_ADDREF(result);
  return result;
}

PRInt32 StyleContextImpl::GetStyleRuleCount(void) const
{
  if (nsnull != mRules) {
    PRUint32 cnt;
    nsresult rv = mRules->Count(&cnt);
    if (NS_FAILED(rv)) return 0;        // XXX error?
    return cnt;
  }
  return 0;
}

NS_IMETHODIMP
StyleContextImpl::GetPseudoType(nsIAtom*& aPseudoTag) const
{
  aPseudoTag = mPseudoTag;
  NS_IF_ADDREF(aPseudoTag);
  return NS_OK;
}

NS_IMETHODIMP
StyleContextImpl::FindChildWithRules(const nsIAtom* aPseudoTag, 
                                     nsISupportsArray* aRules,
                                     nsIStyleContext*& aResult)
{
  aResult = nsnull;

  if ((nsnull != mChild) || (nsnull != mEmptyChild)) {
    StyleContextImpl* child;
    PRInt32 ruleCount;
    if (aRules) {
      PRUint32 cnt;
      nsresult rv = aRules->Count(&cnt);
      if (NS_FAILED(rv)) return rv;
      ruleCount = cnt;
    }
    else
      ruleCount = 0;
    if (0 == ruleCount) {
      if (nsnull != mEmptyChild) {
        child = mEmptyChild;
        do {
          if ((0 == child->mDataCode) &&  // only look at children with un-twiddled data
              (aPseudoTag == child->mPseudoTag)) {
            aResult = child;
            break;
          }
          child = child->mNextSibling;
        } while (child != mEmptyChild);
      }
    }
    else if (nsnull != mChild) {
      PRUint32 hash = 0;
      aRules->EnumerateForwards(HashStyleRule, &hash);
      child = mChild;
      do {
        PRUint32 cnt;
        if ((0 == child->mDataCode) &&  // only look at children with un-twiddled data
            (child->mRuleHash == hash) &&
            (child->mPseudoTag == aPseudoTag) &&
            (nsnull != child->mRules) &&
            NS_SUCCEEDED(child->mRules->Count(&cnt)) && 
            (PRInt32)cnt == ruleCount) {
          if (child->mRules->Equals(aRules)) {
            aResult = child;
            break;
          }
        }
        child = child->mNextSibling;
      } while (child != mChild);
    }
  }
  NS_IF_ADDREF(aResult);
  return NS_OK;
}


PRBool StyleContextImpl::Equals(const nsIStyleContext* aOther) const
{
  PRBool  result = PR_TRUE;
  const StyleContextImpl* other = (StyleContextImpl*)aOther;

  if (other != this) {
    if (mParent != other->mParent) {
      result = PR_FALSE;
    }
    else if (mDataCode != other->mDataCode) {
      result = PR_FALSE;
    }
    else if (mPseudoTag != other->mPseudoTag) {
      result = PR_FALSE;
    }
    else {
      if ((nsnull != mRules) && (nsnull != other->mRules)) {
        if (mRuleHash == other->mRuleHash) {
          result = mRules->Equals(other->mRules);
        }
        else {
          result = PR_FALSE;
        }
      }
      else {
        result = PRBool((nsnull == mRules) && (nsnull == other->mRules));
      }
    }
  }
  return result;
}

PRUint32 StyleContextImpl::HashValue(void) const
{
  return mRuleHash;
}


const nsStyleStruct* StyleContextImpl::GetStyleData(nsStyleStructID aSID)
{
  nsStyleStruct*  result = nsnull;

  switch (aSID) {
    case eStyleStruct_Font:
      result = &mFont;
      break;
    case eStyleStruct_Color:
      result = &mColor;
      break;
    case eStyleStruct_Spacing:
      result = &mSpacing;
      break;
    case eStyleStruct_List:
      result = &mList;
      break;
    case eStyleStruct_Position:
      result = &mPosition;
      break;
    case eStyleStruct_Text:
      result = &mText;
      break;
    case eStyleStruct_Display:
      result = &mDisplay;
      break;
    case eStyleStruct_Table:
      result = &mTable;
      break;
    case eStyleStruct_Content:
      result = &mContent;
      break;
    case eStyleStruct_UserInterface:
      result = &mUserInterface;
      break;
    case eStyleStruct_Print:
    	result = &mPrint;
    	break;
    default:
      NS_ERROR("Invalid style struct id");
      break;
  }
  return result;
}

nsStyleStruct* StyleContextImpl::GetMutableStyleData(nsStyleStructID aSID)
{
  nsStyleStruct*  result = nsnull;

  switch (aSID) {
    case eStyleStruct_Font:
      result = &mFont;
      break;
    case eStyleStruct_Color:
      result = &mColor;
      break;
    case eStyleStruct_Spacing:
      result = &mSpacing;
      break;
    case eStyleStruct_List:
      result = &mList;
      break;
    case eStyleStruct_Position:
      result = &mPosition;
      break;
    case eStyleStruct_Text:
      result = &mText;
      break;
    case eStyleStruct_Display:
      result = &mDisplay;
      break;
    case eStyleStruct_Table:
      result = &mTable;
      break;
    case eStyleStruct_Content:
      result = &mContent;
      break;
    case eStyleStruct_UserInterface:
      result = &mUserInterface;
      break;
    case eStyleStruct_Print:
    	result = &mPrint;
    	break;
    default:
      NS_ERROR("Invalid style struct id");
      break;
  }
  if (nsnull != result) {
    if (0 == mDataCode) {
//      mDataCode = ++gLastDataCode;  // XXX temp disable, this is still used but not needed to force unique
    }
  }
  return result;
}

NS_IMETHODIMP
StyleContextImpl::GetStyle(nsStyleStructID aSID, nsStyleStruct& aStruct) const
{
  nsresult result = NS_OK;
  switch (aSID) {
    case eStyleStruct_Font:
      mFont.CopyTo((nsStyleFont&)aStruct);
      break;
    case eStyleStruct_Color:
      mColor.CopyTo((nsStyleColor&)aStruct);
      break;
    case eStyleStruct_Spacing:
      mSpacing.CopyTo((nsStyleSpacing&)aStruct);
      break;
    case eStyleStruct_List:
      mList.CopyTo((nsStyleList&)aStruct);
      break;
    case eStyleStruct_Position:
      mPosition.CopyTo((nsStylePosition&)aStruct);
      break;
    case eStyleStruct_Text:
      mText.CopyTo((nsStyleText&)aStruct);
      break;
    case eStyleStruct_Display:
      mDisplay.CopyTo((nsStyleDisplay&)aStruct);
      break;
    case eStyleStruct_Table:
      mTable.CopyTo((nsStyleTable&)aStruct);
      break;
    case eStyleStruct_Content:
      mContent.CopyTo((nsStyleContent&)aStruct);
      break;
    case eStyleStruct_UserInterface:
      mUserInterface.CopyTo((nsStyleUserInterface&)aStruct);
      break;
    case eStyleStruct_Print:
      mPrint.CopyTo((nsStylePrint&)aStruct);
    	break;
    default:
      NS_ERROR("Invalid style struct id");
      result = NS_ERROR_INVALID_ARG;
      break;
  }
  return result;
}

NS_IMETHODIMP
StyleContextImpl::SetStyle(nsStyleStructID aSID, const nsStyleStruct& aStruct)
{
  nsresult result = NS_OK;
  switch (aSID) {
    case eStyleStruct_Font:
      mFont.SetFrom((const nsStyleFont&)aStruct);
      break;
    case eStyleStruct_Color:
      mColor.SetFrom((const nsStyleColor&)aStruct);
      break;
    case eStyleStruct_Spacing:
      mSpacing.SetFrom((const nsStyleSpacing&)aStruct);
      break;
    case eStyleStruct_List:
      mList.SetFrom((const nsStyleList&)aStruct);
      break;
    case eStyleStruct_Position:
      mPosition.SetFrom((const nsStylePosition&)aStruct);
      break;
    case eStyleStruct_Text:
      mText.SetFrom((const nsStyleText&)aStruct);
      break;
    case eStyleStruct_Display:
      mDisplay.SetFrom((const nsStyleDisplay&)aStruct);
      break;
    case eStyleStruct_Table:
      mTable.SetFrom((const nsStyleTable&)aStruct);
      break;
    case eStyleStruct_Content:
      mContent.SetFrom((const nsStyleContent&)aStruct);
      break;
    case eStyleStruct_UserInterface:
      mUserInterface.SetFrom((const nsStyleUserInterface&)aStruct);
      break;
    case eStyleStruct_Print:
      mPrint.SetFrom((const nsStylePrint&)aStruct);
    	break;
    default:
      NS_ERROR("Invalid style struct id");
      result = NS_ERROR_INVALID_ARG;
      break;
  }
  return result;
}



struct MapStyleData {
  MapStyleData(nsIMutableStyleContext* aStyleContext, nsIPresContext* aPresContext)
  {
    mStyleContext = aStyleContext;
    mPresContext = aPresContext;
  }
  nsIMutableStyleContext*  mStyleContext;
  nsIPresContext*   mPresContext;
};

static PRBool MapStyleRuleFont(nsISupports* aRule, void* aData)
{
  nsIStyleRule* rule = (nsIStyleRule*)aRule;
  MapStyleData* data = (MapStyleData*)aData;
  rule->MapFontStyleInto(data->mStyleContext, data->mPresContext);
  return PR_TRUE;
}

static PRBool MapStyleRule(nsISupports* aRule, void* aData)
{
  nsIStyleRule* rule = (nsIStyleRule*)aRule;
  MapStyleData* data = (MapStyleData*)aData;
  rule->MapStyleInto(data->mStyleContext, data->mPresContext);
  return PR_TRUE;
}

NS_IMETHODIMP
StyleContextImpl::RemapStyle(nsIPresContext* aPresContext, PRBool aRecurse)
{
  mDataCode = -1;
  if (nsnull != mParent) {
    mFont.ResetFrom(&(mParent->mFont), aPresContext);
    mColor.ResetFrom(&(mParent->mColor), aPresContext);
    mSpacing.ResetFrom(&(mParent->mSpacing), aPresContext);
    mList.ResetFrom(&(mParent->mList), aPresContext);
    mPosition.ResetFrom(&(mParent->mPosition), aPresContext);
    mText.ResetFrom(&(mParent->mText), aPresContext);
    mDisplay.ResetFrom(&(mParent->mDisplay), aPresContext);
    mTable.ResetFrom(&(mParent->mTable), aPresContext);
    mContent.ResetFrom(&(mParent->mContent), aPresContext);
    mUserInterface.ResetFrom(&(mParent->mUserInterface), aPresContext);
    mPrint.ResetFrom(&(mParent->mPrint), aPresContext);
  }
  else {
    mFont.ResetFrom(nsnull, aPresContext);
    mColor.ResetFrom(nsnull, aPresContext);
    mSpacing.ResetFrom(nsnull, aPresContext);
    mList.ResetFrom(nsnull, aPresContext);
    mPosition.ResetFrom(nsnull, aPresContext);
    mText.ResetFrom(nsnull, aPresContext);
    mDisplay.ResetFrom(nsnull, aPresContext);
    mTable.ResetFrom(nsnull, aPresContext);
    mContent.ResetFrom(nsnull, aPresContext);
    mUserInterface.ResetFrom(nsnull, aPresContext);
    mPrint.ResetFrom(nsnull, aPresContext);
  }

  PRUint32 cnt = 0;
  if (mRules) {
    nsresult rv = mRules->Count(&cnt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
  }
  if (0 < cnt) {
    MapStyleData  data(this, aPresContext);
    mRules->EnumerateForwards(MapStyleRuleFont, &data);
    if (mFont.mFlags & NS_STYLE_FONT_USE_FIXED) {
      mFont.mFont = mFont.mFixedFont;
    }
    mRules->EnumerateForwards(MapStyleRule, &data);
  }
  if (-1 == mDataCode) {
    mDataCode = 0;
  }

  nsCompatibility quirkMode = eCompatibility_Standard;
  aPresContext->GetCompatibilityMode(&quirkMode);
  if (eCompatibility_NavQuirks == quirkMode) {
    if ((mDisplay.mDisplay == NS_STYLE_DISPLAY_TABLE) || 
        (mDisplay.mDisplay == NS_STYLE_DISPLAY_TABLE_CAPTION)) {

      StyleContextImpl* holdParent = mParent;
      mParent = nsnull; // cut off all inheritance. this really blows

      // XXX the style we do preserve is visibility, direction
      PRUint8 visible = mDisplay.mVisible;
      PRUint8 direction = mDisplay.mDirection;

      // time to emulate a sub-document
      // This is ugly, but we need to map style once to determine display type
      // then reset and map it again so that all local style is preserved
      if (mDisplay.mDisplay != NS_STYLE_DISPLAY_TABLE) {
        mFont.ResetFrom(nsnull, aPresContext);
      }
      mColor.ResetFrom(nsnull, aPresContext);
      mSpacing.ResetFrom(nsnull, aPresContext);
      mList.ResetFrom(nsnull, aPresContext);
      mText.ResetFrom(nsnull, aPresContext);
      mPosition.ResetFrom(nsnull, aPresContext);
      mDisplay.ResetFrom(nsnull, aPresContext);
      mTable.ResetFrom(nsnull, aPresContext);
      mContent.ResetFrom(nsnull, aPresContext);
      mUserInterface.ResetFrom(nsnull, aPresContext);
      mPrint.ResetFrom(nsnull, aPresContext);
      mDisplay.mVisible = visible;
      mDisplay.mDirection = direction;

      PRUint32 numRules = 0;
      if (mRules) {
        nsresult rv = mRules->Count(&numRules);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
      }
      if (0 < numRules) {
        MapStyleData  data(this, aPresContext);
        mRules->EnumerateForwards(MapStyleRuleFont, &data);
        if (mFont.mFlags & NS_STYLE_FONT_USE_FIXED) {
          mFont.mFont = mFont.mFixedFont;
        }
        mRules->EnumerateForwards(MapStyleRule, &data);
      }
      // reset all font data for tables again
      if (mDisplay.mDisplay == NS_STYLE_DISPLAY_TABLE) {
        mFont.ResetFrom(nsnull, aPresContext);
      }
      mParent = holdParent;
    }
  }

  RecalcAutomaticData(aPresContext);

  if (aRecurse) {
    if (nsnull != mChild) {
      StyleContextImpl* child = mChild;
      do {
        child->RemapStyle(aPresContext);
        child = child->mNextSibling;
      } while (mChild != child);
    }
    if (nsnull != mEmptyChild) {
      StyleContextImpl* child = mEmptyChild;
      do {
        child->RemapStyle(aPresContext);
        child = child->mNextSibling;
      } while (mEmptyChild != child);
    }
  }
  return NS_OK;
}

void StyleContextImpl::ForceUnique(void)
{
  if (mDataCode <= 0) {
    mDataCode = ++gLastDataCode;
  }
}

void StyleContextImpl::RecalcAutomaticData(nsIPresContext* aPresContext)
{
  mSpacing.RecalcData(aPresContext, mColor.mColor);
}

NS_IMETHODIMP
StyleContextImpl::CalcStyleDifference(nsIStyleContext* aOther, PRInt32& aHint) const
{
  if (aOther) {
    PRInt32 hint;
    const StyleContextImpl* other = (const StyleContextImpl*)aOther;

    aHint = mFont.CalcDifference(other->mFont);
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = mColor.CalcDifference(other->mColor);
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = mSpacing.CalcDifference(other->mSpacing);
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = mList.CalcDifference(other->mList);
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = mPosition.CalcDifference(other->mPosition);
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = mText.CalcDifference(other->mText);
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = mDisplay.CalcDifference(other->mDisplay);
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = mTable.CalcDifference(other->mTable);
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = mContent.CalcDifference(other->mContent);
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = mUserInterface.CalcDifference(other->mUserInterface);
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = mPrint.CalcDifference(other->mPrint);
      if (aHint < hint) {
        aHint = hint;
      }
    }
  }
  return NS_OK;
}


void StyleContextImpl::List(FILE* out, PRInt32 aIndent)
{
  // Indent
  PRInt32 ix;
  for (ix = aIndent; --ix >= 0; ) fputs("  ", out);
  fprintf(out, "%p(%d) ", this, mRefCnt);
  if (nsnull != mPseudoTag) {
    nsAutoString  buffer;
    mPseudoTag->ToString(buffer);
    fputs(buffer, out);
    fputs(" ", out);
  }
  PRInt32 count = GetStyleRuleCount();
  if (0 < count) {
    fputs("{\n", out);

    for (ix = 0; ix < count; ix++) {
      nsIStyleRule* rule = (nsIStyleRule*)mRules->ElementAt(ix);
      rule->List(out, aIndent + 1);
      NS_RELEASE(rule);
    }

    for (ix = aIndent; --ix >= 0; ) fputs("  ", out);
    fputs("}\n", out);
  }
  else {
    fputs("{}\n", out);
  }

  if (nsnull != mChild) {
    StyleContextImpl* child = mChild;
    do {
      child->List(out, aIndent + 1);
      child = child->mNextSibling;
    } while (mChild != child);
  }
  if (nsnull != mEmptyChild) {
    StyleContextImpl* child = mEmptyChild;
    do {
      child->List(out, aIndent + 1);
      child = child->mNextSibling;
    } while (mEmptyChild != child);
  }
}


/******************************************************************************
* SizeOf method:
*  
*  Self (reported as StyleContextImpl's size): 
*    1) sizeof(*this) which gets all of the data members
*    2) adds in the size of the PseudoTag, if there is one
*  
*  Contained / Aggregated data (not reported as StyleContextImpl's size):
*    1) the Style Rules in mRules are not counted as part of sizeof(*this)
*       (though the size of the nsISupportsArray ptr. is) so we need to 
*       count the rules seperately. For each rule in the mRules collection
*       we call the SizeOf method and let it report itself.
*
*  Children / siblings / parents:
*    1) We recurse over the mChild and mEmptyChild instances if they exist.
*       These instances will then be accumulated seperately (not part of 
*       the containing instance's size)
*    2) We recurse over the siblings of the Child and Empty Child instances
*       and count then seperately as well.
*    3) We recurse over our direct siblings (if any).
*   
******************************************************************************/
void StyleContextImpl::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  static PRBool bDetailDumpDone = PR_FALSE;
  if (!bDetailDumpDone) {
    bDetailDumpDone = PR_TRUE;
    PRUint32 totalSize=0;

    printf( "Detailed StyleContextImpl dump: basic class sizes of members\n" );
    printf( "*************************************\n");
    printf( " - StyleFontImpl:          %ld\n", (long)sizeof(mFont) );
    totalSize += (long)sizeof(mFont);
    printf( " - StyleColorImpl:         %ld\n", (long)sizeof(mColor) );
    totalSize += (long)sizeof(mColor);
    printf( " - StyleSpacingImpl:       %ld\n", (long)sizeof(mSpacing) );
    totalSize += (long)sizeof(mSpacing);
    printf( " - StyleListImpl:          %ld\n", (long)sizeof(mList) );
    totalSize += (long)sizeof(mList);
    printf( " - StylePositionImpl:      %ld\n", (long)sizeof(mPosition) );
    totalSize += (long)sizeof(mPosition);
    printf( " - StyleTextImpl:          %ld\n", (long)sizeof(mText) );
    totalSize += (long)sizeof(mText);
    printf( " - StyleDisplayImpl:       %ld\n", (long)sizeof(mDisplay) );
    totalSize += (long)sizeof(mDisplay);
    printf( " - StyleTableImpl:         %ld\n", (long)sizeof(mTable) );
    totalSize += (long)sizeof(mTable);
    printf( " - StyleContentImpl:       %ld\n", (long)sizeof(mContent) );
    totalSize += (long)sizeof(mContent);
    printf( " - StyleUserInterfaceImpl: %ld\n", (long)sizeof(mUserInterface) );
    totalSize += (long)sizeof(mUserInterface);
	  printf( " - StylePrintImpl:         %ld\n", (long)sizeof(mPrint));
    totalSize += (long)sizeof(mPrint);
    printf( " - Total:                  %ld\n", (long)totalSize);
    printf( "*************************************\n");
  }

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);

  if(! uniqueItems->AddItem((void*)this) ){
    // object has already been accounted for
    return;
  }

  PRUint32 localSize=0;

  // get or create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("StyleContextImpl"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);
  // add in the size of the member mPseudoTag
  if(mPseudoTag){
    mPseudoTag->SizeOf(aSizeOfHandler, &localSize);
    aSize += localSize;
  }
  aSizeOfHandler->AddSize(tag,aSize);

  // size up the rules (if not already done)
  // XXX - overhead of the collection???
  if(mRules && uniqueItems->AddItem(mRules)){
    PRUint32 curRule, ruleCount;
    mRules->Count(&ruleCount);
    if (ruleCount > 0) {
      for (curRule = 0; curRule < ruleCount; curRule++) {
        nsIStyleRule* rule = (nsIStyleRule*)mRules->ElementAt(curRule);
        NS_ASSERTION(rule, "null entry in Rules list is bad news");
        rule->SizeOf(aSizeOfHandler, localSize);
        NS_RELEASE(rule);
      }
    }
  }  
  // now follow up with the child (and empty child) recursion
  if (nsnull != mChild) {
    StyleContextImpl* child = mChild;
    do {
      child->SizeOf(aSizeOfHandler, localSize);
      child = child->mNextSibling;
    } while (mChild != child);
  }
  if (nsnull != mEmptyChild) {
    StyleContextImpl* child = mEmptyChild;
    do {
      child->SizeOf(aSizeOfHandler, localSize);
      child = child->mNextSibling;
    } while (mEmptyChild != child);
  }
  // and finally our direct siblings (if any)
  if (nsnull != mNextSibling) {
    mNextSibling->SizeOf(aSizeOfHandler, localSize);
  }
}

NS_LAYOUT nsresult
NS_NewStyleContext(nsIStyleContext** aInstancePtrResult,
                   nsIStyleContext* aParentContext,
                   nsIAtom* aPseudoTag,
                   nsISupportsArray* aRules,
                   nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  StyleContextImpl* context = new StyleContextImpl(aParentContext, aPseudoTag, 
                                                   aRules, aPresContext);
  if (nsnull == context) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult result = context->QueryInterface(kIStyleContextIID, (void **) aInstancePtrResult);
  context->RemapStyle(aPresContext);  // remap after initial ref-count is set
  return result;
}
