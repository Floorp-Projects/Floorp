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

#ifdef DEBUG
// #define NOISY_DEBUG
#endif

// --------------------------------------
// Macros for getting style data structs
// - if using external data, get from
//   the member style data instance
// - if internal, get the data member
#ifdef SHARE_STYLECONTEXTS
#define GETSCDATA(data) mStyleData->m##data
#else
#define GETSCDATA(data) m##data
#endif


#ifdef SHARE_STYLECONTEXTS
  // define COMPUTE_STYLEDATA_CRC to actually compute a valid CRC32
  //  - if not defined then the CRC will simply be 0 (see StyleContextData::ComputeCRC)
  //  - this is to avoid the cost of computing a CRC if it is not being used
  //    by the style set in caching the style contexts (not using the FAST_CACHE)
  //    which is the current situation since the CRC can change when GetMutableStyleData
  //    is used to poke values into the style context data.
// #define COMPUTE_STYLEDATA_CRC
#endif //SHARE_STYLECONTEXTS

#ifdef COMPUTE_STYLEDATA_CRC
  // helpers for computing CRC32 on style data
static void gen_crc_table();
static PRUint32 AccumulateCRC(PRUint32 crc_accum, const char *data_blk_ptr, int data_blk_size);
static PRUint32 StyleSideCRC(PRUint32 crc,const nsStyleSides *aStyleSides);
static PRUint32 StyleCoordCRC(PRUint32 crc, const nsStyleCoord* aCoord);
static PRUint32 StyleMarginCRC(PRUint32 crc, const nsMargin *aMargin);
static PRUint32 StyleStringCRC(PRUint32 aCrc, const nsString *aString);
#endif // COMPUTE_STYLEDATA_CRC

inline PRBool IsFixedUnit(nsStyleUnit aUnit, PRBool aEnumOK)
{
  return PRBool((aUnit == eStyleUnit_Null) || 
                (aUnit == eStyleUnit_Coord) || 
                (aEnumOK && (aUnit == eStyleUnit_Enumerated)));
}

static PRBool IsFixedData(const nsStyleSides& aSides, PRBool aEnumOK);
static nscoord CalcCoord(const nsStyleCoord& aCoord, 
                         const nscoord* aEnumTable, 
                         PRInt32 aNumEnums);

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
  PRUint32 ComputeCRC32(PRUint32 crc) const;

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

PRUint32 StyleFontImpl::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;

#ifdef COMPUTE_STYLEDATA_CRC
  crc = AccumulateCRC(crc,(const char *)&(mFont.size),sizeof(mFont.size));
  crc = AccumulateCRC(crc,(const char *)&(mFont.style),sizeof(mFont.style));
  crc = AccumulateCRC(crc,(const char *)&(mFont.variant),sizeof(mFont.variant));
  crc = AccumulateCRC(crc,(const char *)&(mFont.weight),sizeof(mFont.weight));
  crc = AccumulateCRC(crc,(const char *)&(mFont.decorations),sizeof(mFont.decorations));
  crc = StyleStringCRC(crc,&(mFont.name));
  crc = AccumulateCRC(crc,(const char *)&(mFixedFont.size),sizeof(mFixedFont.size));
  crc = AccumulateCRC(crc,(const char *)&(mFixedFont.style),sizeof(mFixedFont.style));
  crc = AccumulateCRC(crc,(const char *)&(mFixedFont.variant),sizeof(mFixedFont.variant));
  crc = AccumulateCRC(crc,(const char *)&(mFixedFont.weight),sizeof(mFixedFont.weight));
  crc = AccumulateCRC(crc,(const char *)&(mFixedFont.decorations),sizeof(mFixedFont.decorations));
  crc = StyleStringCRC(crc,&(mFixedFont.name));
  crc = AccumulateCRC(crc,(const char *)&mFlags,sizeof(mFlags));
#endif

  return crc;
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
  PRUint32 ComputeCRC32(PRUint32 aCrc) const;

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

PRUint32 StyleColorImpl::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
#ifdef COMPUTE_STYLEDATA_CRC
  crc = AccumulateCRC(crc,(const char *)&mColor,sizeof(mColor));
  crc = AccumulateCRC(crc,(const char *)&mBackgroundAttachment,sizeof(mBackgroundAttachment));
  crc = AccumulateCRC(crc,(const char *)&mBackgroundFlags,sizeof(mBackgroundFlags));
  crc = AccumulateCRC(crc,(const char *)&mBackgroundRepeat,sizeof(mBackgroundRepeat));
  crc = AccumulateCRC(crc,(const char *)&mBackgroundColor,sizeof(mBackgroundColor));
  crc = AccumulateCRC(crc,(const char *)&mBackgroundXPosition,sizeof(mBackgroundXPosition));
  crc = AccumulateCRC(crc,(const char *)&mBackgroundYPosition,sizeof(mBackgroundYPosition));
  crc = StyleStringCRC(crc,&mBackgroundImage);
  crc = AccumulateCRC(crc,(const char *)&mCursor,sizeof(mCursor));
  crc = StyleStringCRC(crc,&mCursorImage);
  crc = AccumulateCRC(crc,(const char *)&mOpacity,sizeof(mOpacity));
#endif
  return crc;
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
  } else {
    CalcSidesFor(aFrame, mMargin, NS_SPACING_MARGIN, nsnull, 0, aMargin);
  }
}

void nsStyleSpacing::CalcPaddingFor(const nsIFrame* aFrame, nsMargin& aPadding) const
{
  if (mHasCachedPadding) {
    aPadding = mCachedPadding;
  } else {
    CalcSidesFor(aFrame, mPadding, NS_SPACING_PADDING, nsnull, 0, aPadding);
  }
}

void nsStyleSpacing::CalcBorderFor(const nsIFrame* aFrame, nsMargin& aBorder) const
{
  if (mHasCachedBorder) {
    aBorder = mCachedBorder;
  } else {
    CalcSidesFor(aFrame, mBorder, NS_SPACING_BORDER, kBorderWidths, 3, aBorder);
  }
}

void nsStyleSpacing::CalcBorderPaddingFor(const nsIFrame* aFrame, nsMargin& aBorderPadding) const
{
  if (mHasCachedPadding && mHasCachedBorder) {
    aBorderPadding = mCachedBorderPadding;
  } else {
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
  PRBool IsBorderSideVisible(PRUint8 aSide) const;
  void RecalcData(nsIPresContext* aPresContext, nscolor color);
  PRInt32 CalcDifference(const StyleSpacingImpl& aOther) const;
  PRUint32 ComputeCRC32(PRUint32 aCrc) const;

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

PRBool StyleSpacingImpl::IsBorderSideVisible(PRUint8 aSide) const
{
	PRUint8 borderStyle = GetBorderStyle(aSide);
	return ((borderStyle != NS_STYLE_BORDER_STYLE_NONE)
       && (borderStyle != NS_STYLE_BORDER_STYLE_HIDDEN));
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
      mCachedBorder.left = CalcCoord(mBorder.GetLeft(coord), borderWidths, 3);
    }
    if (!IsBorderSideVisible(NS_SIDE_TOP)) {
      mCachedBorder.top = 0;
    }
    else {
      mCachedBorder.top = CalcCoord(mBorder.GetTop(coord), borderWidths, 3);
    }
    if (!IsBorderSideVisible(NS_SIDE_RIGHT)) {
      mCachedBorder.right = 0;
    }
    else {
      mCachedBorder.right = CalcCoord(mBorder.GetRight(coord), borderWidths, 3);
    }
    if (!IsBorderSideVisible(NS_SIDE_BOTTOM)) {
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

PRUint32 StyleSpacingImpl::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
  
#ifdef COMPUTE_STYLEDATA_CRC
  crc = StyleSideCRC(crc,&mMargin);
  crc = StyleSideCRC(crc,&mPadding);
  crc = StyleSideCRC(crc,&mBorder);
  crc = StyleSideCRC(crc,&mBorderRadius);
  crc = StyleSideCRC(crc,&mOutlineRadius);
  crc = StyleCoordCRC(crc,&mOutlineWidth);
  crc = AccumulateCRC(crc,(const char *)&mFloatEdge,sizeof(mFloatEdge));
  crc = AccumulateCRC(crc,(const char *)&mHasCachedMargin,sizeof(mHasCachedMargin));
  crc = AccumulateCRC(crc,(const char *)&mHasCachedPadding,sizeof(mHasCachedPadding));
  crc = AccumulateCRC(crc,(const char *)&mHasCachedBorder,sizeof(mHasCachedBorder));
  crc = AccumulateCRC(crc,(const char *)&mHasCachedOutline,sizeof(mHasCachedOutline));
  crc = StyleMarginCRC(crc,&mCachedMargin);
  crc = StyleMarginCRC(crc,&mCachedPadding);
  crc = StyleMarginCRC(crc,&mCachedBorder);
  crc = StyleMarginCRC(crc,&mCachedBorderPadding);
  crc = AccumulateCRC(crc,(const char *)&mCachedOutlineWidth,sizeof(mCachedOutlineWidth));
  crc = AccumulateCRC(crc,(const char *)mBorderStyle,sizeof(mBorderStyle)); // array of 4 elements
  crc = AccumulateCRC(crc,(const char *)mBorderColor,sizeof(mBorderColor)); // array ...
  crc = AccumulateCRC(crc,(const char *)&mOutlineStyle,sizeof(mOutlineStyle));
  crc = AccumulateCRC(crc,(const char *)&mOutlineColor,sizeof(mOutlineColor));
#endif
  return crc;
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
  PRUint32 ComputeCRC32(PRUint32 aCrc) const;
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

PRUint32 StyleListImpl::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
#ifdef COMPUTE_STYLEDATA_CRC
  crc = AccumulateCRC(crc,(const char *)&mListStyleType,sizeof(mListStyleType));
  crc = AccumulateCRC(crc,(const char *)&mListStylePosition,sizeof(mListStylePosition));
  crc = StyleStringCRC(crc,&mListStyleImage);
#endif
  return crc;
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
  PRUint32 ComputeCRC32(PRUint32 aCrc) const;

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

PRUint32 StylePositionImpl::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
#ifdef COMPUTE_STYLEDATA_CRC
  crc = AccumulateCRC(crc,(const char *)&mPosition,sizeof(mPosition));
  crc = StyleSideCRC(crc,&mOffset);
  crc = StyleCoordCRC(crc,&mWidth);
  crc = StyleCoordCRC(crc,&mMinWidth);
  crc = StyleCoordCRC(crc,&mMaxWidth);
  crc = StyleCoordCRC(crc,&mHeight);
  crc = StyleCoordCRC(crc,&mMinHeight);
  crc = StyleCoordCRC(crc,&mMaxHeight);
  crc = AccumulateCRC(crc,(const char *)&mBoxSizing,sizeof(mBoxSizing));
  crc = AccumulateCRC(crc,(const char *)&mZIndex,sizeof(mZIndex));
#endif
  return crc;
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
  PRUint32 ComputeCRC32(PRUint32 aCrc) const;
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

PRUint32 StyleTextImpl::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;

#ifdef COMPUTE_STYLEDATA_CRC
  crc = AccumulateCRC(crc,(const char *)&mTextAlign,sizeof(mTextAlign));
  crc = AccumulateCRC(crc,(const char *)&mTextDecoration,sizeof(mTextDecoration));
  crc = AccumulateCRC(crc,(const char *)&mTextTransform,sizeof(mTextTransform));
  crc = AccumulateCRC(crc,(const char *)&mWhiteSpace,sizeof(mWhiteSpace));
  crc = StyleCoordCRC(crc,&mLetterSpacing);
  crc = StyleCoordCRC(crc,&mLineHeight);
  crc = StyleCoordCRC(crc,&mTextIndent);
  crc = StyleCoordCRC(crc,&mWordSpacing);
  crc = StyleCoordCRC(crc,&mVerticalAlign);
#endif
  return crc;
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
  PRUint32 ComputeCRC32(PRUint32 aCrc) const;
};

void StyleDisplayImpl::ResetFrom(const nsStyleDisplay* aParent, nsIPresContext* aPresContext)
{
  if (nsnull != aParent) {
    mDirection = aParent->mDirection;
    mLanguage = aParent->mLanguage;
    mVisible = aParent->mVisible;
  }
  else {
    aPresContext->GetDefaultDirection(&mDirection);
    aPresContext->GetLanguage(getter_AddRefs(mLanguage));
    mVisible = NS_STYLE_VISIBILITY_VISIBLE;
  }
  mDisplay = NS_STYLE_DISPLAY_INLINE;
  mFloats = NS_STYLE_FLOAT_NONE;
  mBreakType = NS_STYLE_CLEAR_NONE;
  mBreakBefore = PR_FALSE;
  mBreakAfter = PR_FALSE;
  mOverflow = NS_STYLE_OVERFLOW_VISIBLE;
  mClipFlags = NS_STYLE_CLIP_AUTO;
  mClip.SetRect(0,0,0,0);
}

void StyleDisplayImpl::SetFrom(const nsStyleDisplay& aSource)
{
  mDirection = aSource.mDirection;
  mDisplay = aSource.mDisplay;
  mFloats = aSource.mFloats;
  mBreakType = aSource.mBreakType;
  mBreakBefore = aSource.mBreakBefore;
  mBreakAfter = aSource.mBreakAfter;
  mVisible = aSource.mVisible;
  mOverflow = aSource.mOverflow;
  mClipFlags = aSource.mClipFlags;
  mClip = aSource.mClip;
  mLanguage = aSource.mLanguage;
}

void StyleDisplayImpl::CopyTo(nsStyleDisplay& aDest) const
{
  aDest.mDirection = mDirection;
  aDest.mDisplay = mDisplay;
  aDest.mFloats = mFloats;
  aDest.mBreakType = mBreakType;
  aDest.mBreakBefore = mBreakBefore;
  aDest.mBreakAfter = mBreakAfter;
  aDest.mVisible = mVisible;
  aDest.mOverflow = mOverflow;
  aDest.mClipFlags = mClipFlags;
  aDest.mClip = mClip;
  aDest.mLanguage = mLanguage;
}

PRInt32 StyleDisplayImpl::CalcDifference(const StyleDisplayImpl& aOther) const
{
  if ((mDisplay == aOther.mDisplay) &&
      (mFloats == aOther.mFloats) &&
      (mOverflow == aOther.mOverflow)) {
    if ((mDirection == aOther.mDirection) &&
        (mLanguage == aOther.mLanguage) &&
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

PRUint32 StyleDisplayImpl::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
#ifdef COMPUTE_STYLEDATA_CRC
  crc = AccumulateCRC(crc,(const char *)&mDirection,sizeof(mDirection));
  crc = AccumulateCRC(crc,(const char *)&mDisplay,sizeof(mDisplay));
  crc = AccumulateCRC(crc,(const char *)&mFloats,sizeof(mFloats));
  crc = AccumulateCRC(crc,(const char *)&mBreakType,sizeof(mBreakType));
  crc = AccumulateCRC(crc,(const char *)&mBreakBefore,sizeof(mBreakBefore));
  crc = AccumulateCRC(crc,(const char *)&mBreakAfter,sizeof(mBreakAfter));
  crc = AccumulateCRC(crc,(const char *)&mVisible,sizeof(mVisible));
  crc = AccumulateCRC(crc,(const char *)&mOverflow,sizeof(mOverflow));
  crc = AccumulateCRC(crc,(const char *)&mClipFlags,sizeof(mClipFlags));
  // crc = StyleMarginCRC(crc,&mClip);
#endif
  return crc;
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
  PRUint32 ComputeCRC32(PRUint32 aCrc) const;
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

PRUint32 StyleTableImpl::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
#ifdef COMPUTE_STYLEDATA_CRC
  crc = AccumulateCRC(crc,(const char *)&mLayoutStrategy,sizeof(mLayoutStrategy));
  crc = AccumulateCRC(crc,(const char *)&mFrame,sizeof(mFrame));
  crc = AccumulateCRC(crc,(const char *)&mRules,sizeof(mRules));
  crc = AccumulateCRC(crc,(const char *)&mBorderCollapse,sizeof(mBorderCollapse));
  crc = StyleCoordCRC(crc,&mBorderSpacingX);
  crc = StyleCoordCRC(crc,&mBorderSpacingY);
  crc = StyleCoordCRC(crc,&mCellPadding);
  crc = AccumulateCRC(crc,(const char *)&mCaptionSide,sizeof(mCaptionSide));
  crc = AccumulateCRC(crc,(const char *)&mEmptyCells,sizeof(mEmptyCells));
  crc = AccumulateCRC(crc,(const char *)&mCols,sizeof(mCols));
  crc = AccumulateCRC(crc,(const char *)&mSpan,sizeof(mSpan));
  crc = StyleCoordCRC(crc,&mSpanWidth);
#endif
  return crc;
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
  PRUint32 ComputeCRC32(PRUint32 aCrc) const;
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

PRUint32 StyleContentImpl::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
#ifdef COMPUTE_STYLEDATA_CRC
  crc = StyleCoordCRC(crc,&mMarkerOffset);
  crc = AccumulateCRC(crc,(const char *)&mContentCount,sizeof(mContentCount));

  crc = AccumulateCRC(crc,(const char *)&mIncrementCount,sizeof(mIncrementCount));
  crc = AccumulateCRC(crc,(const char *)&mResetCount,sizeof(mResetCount));
  crc = AccumulateCRC(crc,(const char *)&mQuotesCount,sizeof(mQuotesCount));
  if (mContents) {
    crc = AccumulateCRC(crc,(const char *)&(mContents->mType),sizeof(mContents->mType));
  }
  if (mIncrements) {
    crc = AccumulateCRC(crc,(const char *)&(mIncrements->mValue),sizeof(mIncrements->mValue));
  }
  if (mResets) {
    crc = AccumulateCRC(crc,(const char *)&(mResets->mValue),sizeof(mResets->mValue));
  }
  if (mQuotes) {
    crc = StyleStringCRC(crc,mQuotes);
  }
#endif
  return crc;
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
  PRUint32 ComputeCRC32(PRUint32 aCrc) const;

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

PRUint32 StyleUserInterfaceImpl::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
#ifdef COMPUTE_STYLEDATA_CRC
  crc = AccumulateCRC(crc,(const char *)&mUserInput,sizeof(mUserInput));
  crc = AccumulateCRC(crc,(const char *)&mUserModify,sizeof(mUserModify));
  crc = AccumulateCRC(crc,(const char *)&mUserSelect,sizeof(mUserSelect));
  crc = AccumulateCRC(crc,(const char *)&mUserFocus,sizeof(mUserFocus));
  crc = AccumulateCRC(crc,(const char *)&mKeyEquivalent,sizeof(mKeyEquivalent));
  crc = AccumulateCRC(crc,(const char *)&mResizer,sizeof(mResizer));
  PRUint32 len;
  len = mBehavior.Length();
  crc = AccumulateCRC(crc,(const char *)&len,sizeof(len));
#endif
  return crc;
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
  PRUint32 ComputeCRC32(PRUint32 aCrc) const;

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

PRUint32 StylePrintImpl::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
#ifdef COMPUTE_STYLEDATA_CRC
  crc = AccumulateCRC(crc,(const char *)&mPageBreakBefore,sizeof(mPageBreakBefore));
  crc = AccumulateCRC(crc,(const char *)&mPageBreakAfter,sizeof(mPageBreakAfter));
  crc = AccumulateCRC(crc,(const char *)&mPageBreakInside,sizeof(mPageBreakInside));
  PRUint32 len = mPage.Length();
  crc = AccumulateCRC(crc,(const char *)&len,sizeof(len));
  crc = AccumulateCRC(crc,(const char *)&mWidows,sizeof(mWidows));
  crc = AccumulateCRC(crc,(const char *)&mOrphans,sizeof(mOrphans));
  crc = AccumulateCRC(crc,(const char *)&mMarks,sizeof(mMarks));
  crc = StyleCoordCRC(crc,&mSizeWidth);
  crc = StyleCoordCRC(crc,&mSizeHeight);
#endif
  return crc;
}


//----------------------------------------------------------------------

#ifdef SHARE_STYLECONTEXTS

class nsStyleContextData
{
public:

  friend class StyleContextImpl;

  void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);

private:  // all data and methods private: only friends have access

  static nsStyleContextData *Create(nsIPresContext *aPresContext);
  
  nsStyleContextData(nsIPresContext *aPresContext);
  ~nsStyleContextData(void);

  PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  void SetCRC32(void) { mCRC = ComputeCRC32(0); }
  PRUint32 GetCRC32(void) const { return mCRC; }

  PRUint32 AddRef(void);
  PRUint32 Release(void);

  // the style data...
  // - StyleContextImpl gets friend-access
  //
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

  PRUint32                mRefCnt;
  PRUint32                mCRC;

#ifdef DEBUG
  static PRUint32         gInstanceCount;
#endif
};

#ifndef DEBUG
inline
#endif
PRUint32 nsStyleContextData::AddRef(void)
{
  ++mRefCnt;
  NS_LOG_ADDREF(this,mRefCnt,"nsStyleContextData",sizeof(*this));
  return mRefCnt;
}

#ifndef DEBUG
inline
#endif
PRUint32 nsStyleContextData::Release(void)
{
  NS_ASSERTION(mRefCnt > 0, "RefCount error in nsStyleContextData");
  --mRefCnt;
  NS_LOG_RELEASE(this,mRefCnt,"nsStyleContextData");
  if (0 == mRefCnt) {
#ifdef NOISY_DEBUG
    printf("deleting nsStyleContextData instance: (%ld)\n", (long)(--gInstanceCount));
#endif
    delete this;
    return 0;
  }
  return mRefCnt;
}

#ifdef DEBUG
/*static*/ PRUint32 nsStyleContextData::gInstanceCount;
#endif  // DEBUG

nsStyleContextData *nsStyleContextData::Create(nsIPresContext *aPresContext)
{
  NS_ASSERTION(aPresContext != nsnull, "parameter cannot be null");
  nsStyleContextData *pData = nsnull;
  if (aPresContext) {
    pData = new nsStyleContextData(aPresContext);
    if (pData) {
      NS_ADDREF(pData);
#ifdef NOISY_DEBUG
      printf("new nsStyleContextData instance: (%ld) CRC=%lu\n", 
             (long)(++gInstanceCount), (unsigned long)pData->ComputeCRC32(0));
#endif // NOISY_DEBUG
    }
  }
  return pData;
}

nsStyleContextData::nsStyleContextData(nsIPresContext *aPresContext)
: mFont(aPresContext->GetDefaultFontDeprecated(), 
        aPresContext->GetDefaultFixedFontDeprecated()),
  mRefCnt(0), mCRC(0)
{
}

nsStyleContextData::~nsStyleContextData(void)
{
  NS_ASSERTION(0 == mRefCnt, "RefCount error in ~nsStyleContextData");
  // debug here...
}

PRUint32 nsStyleContextData::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;

#ifdef COMPUTE_STYLEDATA_CRC
  // have each style struct compute its own CRC, propogating the previous value...
  crc = mFont.ComputeCRC32(crc);
  crc = mColor.ComputeCRC32(crc);
  crc = mSpacing.ComputeCRC32(crc);
  crc = mList.ComputeCRC32(crc);
  crc = mPosition.ComputeCRC32(crc);
  crc = mText.ComputeCRC32(crc);
  crc = mDisplay.ComputeCRC32(crc);
  crc = mTable.ComputeCRC32(crc);
  crc = mContent.ComputeCRC32(crc);
  crc = mUserInterface.ComputeCRC32(crc);
	crc = mPrint.ComputeCRC32(crc);
#else
  crc = 0;
#endif

  return crc;
}

void nsStyleContextData::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler, "SizeOfHandler cannot be null in SizeOf");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);

  if(! uniqueItems->AddItem((void*)this) ){
    // object has already been accounted for
    return;
  }
  // get or create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("StyleContextData"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);
  aSizeOfHandler->AddSize(tag,aSize);
}

#endif //#ifdef SHARE_STYLECONTEXTS

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
  NS_IMETHOD  CalcStyleDifference(nsIStyleContext* aOther, PRInt32& aHint,PRBool aStopAtFirstDifference = PR_FALSE) const;

#ifdef SHARE_STYLECONTEXTS
  // share the style data of the StyleDataDonor
  // NOTE: the donor instance is cast to a StyleContextImpl to keep the
  //       interface free of implementation types. This may be invalid in 
  //       the future
  // XXX - reconfigure APIs to avoid casting the interface to the impl
  nsresult ShareStyleDataFrom(nsIStyleContext*aStyleDataDonor);

  // sets aMatches to PR_TRUE if the style data of aStyleContextToMatch matches the 
  // style data of this, PR_FALSE otherwise
  NS_IMETHOD StyleDataMatches(nsIStyleContext* aStyleContextToMatch, PRBool *aMatches);

  // get the key for this context (CRC32 value)
  NS_IMETHOD GetStyleContextKey(scKey &aKey) const;

  // update the style set cache by adding this context to it
  // - NOTE: mStyleSet member must be set
  nsresult UpdateStyleSetCache( void ) const;
#endif

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

#ifdef SHARE_STYLECONTEXTS

  nsStyleContextData*     mStyleData;

#else

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

#endif // #ifdef SHARE_STYLECONTEXTS

  // make sure we have valid style data
  nsresult EnsureStyleData(nsIPresContext* aPresContext);
  nsresult HaveStyleData(void) const;

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
#ifdef SHARE_STYLECONTEXTS
    mStyleData(nsnull)
#else
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
#endif
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

#ifdef SHARE_STYLECONTEXTS
  // remember the style set
  nsIPresShell* shell = nsnull;
  aPresContext->GetShell(&shell);
  if (shell) {
    shell->GetStyleSet( getter_AddRefs(mStyleSet) );
    NS_RELEASE( shell );
  }
#endif // SHARE_STYLECONTEXTS
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

#ifdef SHARE_STYLECONTEXTS
  // remove this instance from the style set (remember, the set does not AddRef or Release us)
  if (mStyleSet) {
    mStyleSet->RemoveStyleContext((nsIStyleContext *)this);
  } else {
    NS_ASSERTION(0, "StyleSet is not optional in a StyleContext's dtor...");
  }
  // release the style data so it can be reclaimed when no longer referenced
  NS_IF_RELEASE(mStyleData);
#endif // SHARE_STYLECONTEXTS
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
      result = & GETSCDATA(Font);
      break;
    case eStyleStruct_Color:
      result = & GETSCDATA(Color);
      break;
    case eStyleStruct_Spacing:
      result = & GETSCDATA(Spacing);
      break;
    case eStyleStruct_List:
      result = & GETSCDATA(List);
      break;
    case eStyleStruct_Position:
      result = & GETSCDATA(Position);
      break;
    case eStyleStruct_Text:
      result = & GETSCDATA(Text);
      break;
    case eStyleStruct_Display:
      result = & GETSCDATA(Display);
      break;
    case eStyleStruct_Table:
      result = & GETSCDATA(Table);
      break;
    case eStyleStruct_Content:
      result = & GETSCDATA(Content);
      break;
    case eStyleStruct_UserInterface:
      result = & GETSCDATA(UserInterface);
      break;
    case eStyleStruct_Print:
    	result = & GETSCDATA(Print);
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
      result = & GETSCDATA(Font);
      break;
    case eStyleStruct_Color:
      result = & GETSCDATA(Color);
      break;
    case eStyleStruct_Spacing:
      result = & GETSCDATA(Spacing);
      break;
    case eStyleStruct_List:
      result = & GETSCDATA(List);
      break;
    case eStyleStruct_Position:
      result = & GETSCDATA(Position);
      break;
    case eStyleStruct_Text:
      result = & GETSCDATA(Text);
      break;
    case eStyleStruct_Display:
      result = & GETSCDATA(Display);
      break;
    case eStyleStruct_Table:
      result = & GETSCDATA(Table);
      break;
    case eStyleStruct_Content:
      result = & GETSCDATA(Content);
      break;
    case eStyleStruct_UserInterface:
      result = & GETSCDATA(UserInterface);
      break;
    case eStyleStruct_Print:
    	result = & GETSCDATA(Print);
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
      GETSCDATA(Font).CopyTo((nsStyleFont&)aStruct);
      break;
    case eStyleStruct_Color:
      GETSCDATA(Color).CopyTo((nsStyleColor&)aStruct);
      break;
    case eStyleStruct_Spacing:
      GETSCDATA(Spacing).CopyTo((nsStyleSpacing&)aStruct);
      break;
    case eStyleStruct_List:
      GETSCDATA(List).CopyTo((nsStyleList&)aStruct);
      break;
    case eStyleStruct_Position:
      GETSCDATA(Position).CopyTo((nsStylePosition&)aStruct);
      break;
    case eStyleStruct_Text:
      GETSCDATA(Text).CopyTo((nsStyleText&)aStruct);
      break;
    case eStyleStruct_Display:
      GETSCDATA(Display).CopyTo((nsStyleDisplay&)aStruct);
      break;
    case eStyleStruct_Table:
      GETSCDATA(Table).CopyTo((nsStyleTable&)aStruct);
      break;
    case eStyleStruct_Content:
      GETSCDATA(Content).CopyTo((nsStyleContent&)aStruct);
      break;
    case eStyleStruct_UserInterface:
      GETSCDATA(UserInterface).CopyTo((nsStyleUserInterface&)aStruct);
      break;
    case eStyleStruct_Print:
      GETSCDATA(Print).CopyTo((nsStylePrint&)aStruct);
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
      GETSCDATA(Font).SetFrom((const nsStyleFont&)aStruct);
      break;
    case eStyleStruct_Color:
      GETSCDATA(Color).SetFrom((const nsStyleColor&)aStruct);
      break;
    case eStyleStruct_Spacing:
      GETSCDATA(Spacing).SetFrom((const nsStyleSpacing&)aStruct);
      break;
    case eStyleStruct_List:
      GETSCDATA(List).SetFrom((const nsStyleList&)aStruct);
      break;
    case eStyleStruct_Position:
      GETSCDATA(Position).SetFrom((const nsStylePosition&)aStruct);
      break;
    case eStyleStruct_Text:
      GETSCDATA(Text).SetFrom((const nsStyleText&)aStruct);
      break;
    case eStyleStruct_Display:
      GETSCDATA(Display).SetFrom((const nsStyleDisplay&)aStruct);
      break;
    case eStyleStruct_Table:
      GETSCDATA(Table).SetFrom((const nsStyleTable&)aStruct);
      break;
    case eStyleStruct_Content:
      GETSCDATA(Content).SetFrom((const nsStyleContent&)aStruct);
      break;
    case eStyleStruct_UserInterface:
      GETSCDATA(UserInterface).SetFrom((const nsStyleUserInterface&)aStruct);
      break;
    case eStyleStruct_Print:
      GETSCDATA(Print).SetFrom((const nsStylePrint&)aStruct);
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

  if (NS_FAILED(EnsureStyleData(aPresContext))) {
    return NS_ERROR_UNEXPECTED;
  }

  if (nsnull != mParent) {
    GETSCDATA(Font).ResetFrom(&(mParent->GETSCDATA(Font)), aPresContext);
    GETSCDATA(Color).ResetFrom(&(mParent->GETSCDATA(Color)), aPresContext);
    GETSCDATA(Spacing).ResetFrom(&(mParent->GETSCDATA(Spacing)), aPresContext);
    GETSCDATA(List).ResetFrom(&(mParent->GETSCDATA(List)), aPresContext);
    GETSCDATA(Position).ResetFrom(&(mParent->GETSCDATA(Position)), aPresContext);
    GETSCDATA(Text).ResetFrom(&(mParent->GETSCDATA(Text)), aPresContext);
    GETSCDATA(Display).ResetFrom(&(mParent->GETSCDATA(Display)), aPresContext);
    GETSCDATA(Table).ResetFrom(&(mParent->GETSCDATA(Table)), aPresContext);
    GETSCDATA(Content).ResetFrom(&(mParent->GETSCDATA(Content)), aPresContext);
    GETSCDATA(UserInterface).ResetFrom(&(mParent->GETSCDATA(UserInterface)), aPresContext);
    GETSCDATA(Print).ResetFrom(&(mParent->GETSCDATA(Print)), aPresContext);
  }
  else {
    GETSCDATA(Font).ResetFrom(nsnull, aPresContext);
    GETSCDATA(Color).ResetFrom(nsnull, aPresContext);
    GETSCDATA(Spacing).ResetFrom(nsnull, aPresContext);
    GETSCDATA(List).ResetFrom(nsnull, aPresContext);
    GETSCDATA(Position).ResetFrom(nsnull, aPresContext);
    GETSCDATA(Text).ResetFrom(nsnull, aPresContext);
    GETSCDATA(Display).ResetFrom(nsnull, aPresContext);
    GETSCDATA(Table).ResetFrom(nsnull, aPresContext);
    GETSCDATA(Content).ResetFrom(nsnull, aPresContext);
    GETSCDATA(UserInterface).ResetFrom(nsnull, aPresContext);
    GETSCDATA(Print).ResetFrom(nsnull, aPresContext);
  }

  PRUint32 cnt = 0;
  if (mRules) {
    nsresult rv = mRules->Count(&cnt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
  }
  if (0 < cnt) {
    MapStyleData  data(this, aPresContext);
    mRules->EnumerateForwards(MapStyleRuleFont, &data);
    if (GETSCDATA(Font).mFlags & NS_STYLE_FONT_USE_FIXED) {
      GETSCDATA(Font).mFont = GETSCDATA(Font).mFixedFont;
    }
    mRules->EnumerateForwards(MapStyleRule, &data);
  }
  if (-1 == mDataCode) {
    mDataCode = 0;
  }

  nsCompatibility quirkMode = eCompatibility_Standard;
  aPresContext->GetCompatibilityMode(&quirkMode);
  if (eCompatibility_NavQuirks == quirkMode) {
    if (((GETSCDATA(Display).mDisplay == NS_STYLE_DISPLAY_TABLE) || 
         (GETSCDATA(Display).mDisplay == NS_STYLE_DISPLAY_TABLE_CAPTION)) &&
        (nsnull == mPseudoTag)) {

      StyleContextImpl* holdParent = mParent;
      mParent = nsnull; // cut off all inheritance. this really blows

      // XXX the style we do preserve is visibility, direction, language
      PRUint8 visible = GETSCDATA(Display).mVisible;
      PRUint8 direction = GETSCDATA(Display).mDirection;
      nsCOMPtr<nsILanguageAtom> language = GETSCDATA(Display).mLanguage;

      // time to emulate a sub-document
      // This is ugly, but we need to map style once to determine display type
      // then reset and map it again so that all local style is preserved
      if (GETSCDATA(Display).mDisplay != NS_STYLE_DISPLAY_TABLE) {
        GETSCDATA(Font).ResetFrom(nsnull, aPresContext);
      }
      GETSCDATA(Color).ResetFrom(nsnull, aPresContext);
      GETSCDATA(Spacing).ResetFrom(nsnull, aPresContext);
      GETSCDATA(List).ResetFrom(nsnull, aPresContext);
      GETSCDATA(Text).ResetFrom(nsnull, aPresContext);
      GETSCDATA(Position).ResetFrom(nsnull, aPresContext);
      GETSCDATA(Display).ResetFrom(nsnull, aPresContext);
      GETSCDATA(Table).ResetFrom(nsnull, aPresContext);
      GETSCDATA(Content).ResetFrom(nsnull, aPresContext);
      GETSCDATA(UserInterface).ResetFrom(nsnull, aPresContext);
      GETSCDATA(Print).ResetFrom(nsnull, aPresContext);
      GETSCDATA(Display).mVisible = visible;
      GETSCDATA(Display).mDirection = direction;
      GETSCDATA(Display).mLanguage = language;

      PRUint32 numRules = 0;
      if (mRules) {
        nsresult rv = mRules->Count(&numRules);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
      }
      if (0 < numRules) {
        MapStyleData  data(this, aPresContext);
        mRules->EnumerateForwards(MapStyleRuleFont, &data);
        if (GETSCDATA(Font).mFlags & NS_STYLE_FONT_USE_FIXED) {
          GETSCDATA(Font).mFont = GETSCDATA(Font).mFixedFont;
        }
        mRules->EnumerateForwards(MapStyleRule, &data);
      }
      // reset all font data for tables again
      if (GETSCDATA(Display).mDisplay == NS_STYLE_DISPLAY_TABLE) {
        // get the font-name to reset: this property we preserve
        nsAutoString strName(GETSCDATA(Font).mFont.name);
        nsAutoString strMixedName(GETSCDATA(Font).mFixedFont.name);
   
        GETSCDATA(Font).ResetFrom(nsnull, aPresContext);
   
        // now reset the font names back to original
        GETSCDATA(Font).mFont.name = strName;
        GETSCDATA(Font).mFixedFont.name = strMixedName;
      }
      mParent = holdParent;
    }
  } else {
    // In strict mode, we still have to support one "quirky" thing
    // for tables.  HTML's alignment attributes have always worked
    // so they don't inherit into tables, but instead align the
    // tables.  We should keep doing this, because HTML alignment
    // is just weird, and we shouldn't force it to match CSS.
    if (GETSCDATA(Display).mDisplay == NS_STYLE_DISPLAY_TABLE) {
      // -moz-center and -moz-right are used for HTML's alignment
      if ((GETSCDATA(Text).mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_CENTER) ||
          (GETSCDATA(Text).mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_RIGHT))
      {
        GETSCDATA(Text).mTextAlign = NS_STYLE_TEXT_ALIGN_DEFAULT;
      }
    }
  }

  RecalcAutomaticData(aPresContext);

#ifdef SHARE_STYLECONTEXTS

  static PRBool bEnableSharing = PR_FALSE; 
    // set to FALSE in debugger to turn off sharing of sc data

  if (bEnableSharing) {
    // set the CRC
    mStyleData->SetCRC32();

    NS_ASSERTION(mStyleSet, "Expected to have a style set ref...");
    nsIStyleContext *matchingSC = nsnull;

    // check if there is a matching context...
    if ((NS_SUCCEEDED(mStyleSet->FindMatchingContext(this, &matchingSC))) && 
        (nsnull != matchingSC)) {
      ShareStyleDataFrom(matchingSC);
#ifdef NOISY_DEBUG
      printf("SC Data Shared :)\n");
#endif
      NS_IF_RELEASE(matchingSC);
    } else {
#ifdef NOISY_DEBUG
      printf("Unique SC Data - Not Shared :(\n");
#endif
    }
  } // if(bDisableSharing==false)

#endif

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
  if (NS_FAILED(EnsureStyleData(aPresContext))) {
    return /*NS_FAILURE*/;
  }
  GETSCDATA(Spacing).RecalcData(aPresContext, GETSCDATA(Color).mColor);
}

NS_IMETHODIMP
StyleContextImpl::CalcStyleDifference(nsIStyleContext* aOther, PRInt32& aHint,PRBool aStopAtFirstDifference /*= PR_FALSE*/) const
{
  if (NS_FAILED(HaveStyleData())) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aOther) {
    PRInt32 hint;
    const StyleContextImpl* other = (const StyleContextImpl*)aOther;

    aHint = GETSCDATA(Font).CalcDifference(other->GETSCDATA(Font));
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = GETSCDATA(Color).CalcDifference(other->GETSCDATA(Color));
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = GETSCDATA(Spacing).CalcDifference(other->GETSCDATA(Spacing));
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = GETSCDATA(List).CalcDifference(other->GETSCDATA(List));
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = GETSCDATA(Position).CalcDifference(other->GETSCDATA(Position));
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = GETSCDATA(Text).CalcDifference(other->GETSCDATA(Text));
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = GETSCDATA(Display).CalcDifference(other->GETSCDATA(Display));
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = GETSCDATA(Table).CalcDifference(other->GETSCDATA(Table));
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = GETSCDATA(Content).CalcDifference(other->GETSCDATA(Content));
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = GETSCDATA(UserInterface).CalcDifference(other->GETSCDATA(UserInterface));
      if (aHint < hint) {
        aHint = hint;
      }
    }
    if (aStopAtFirstDifference && aHint > NS_STYLE_HINT_NONE) return NS_OK;
    if (aHint < NS_STYLE_HINT_MAX) {
      hint = GETSCDATA(Print).CalcDifference(other->GETSCDATA(Print));
      if (aHint < hint) {
        aHint = hint;
      }
    }
  }
  return NS_OK;
}


nsresult StyleContextImpl::EnsureStyleData(nsIPresContext* aPresContext)
{
  nsresult rv = NS_OK;
  NS_ASSERTION(aPresContext, "null presContext argument is illegal and immoral");
  if (nsnull == aPresContext) {
    // no pres context provided, and we have no style data, so send back an error
    return NS_ERROR_FAILURE;
  }

#ifdef SHARE_STYLECONTEXTS
  // See if we already have data...
  if (NS_FAILED(HaveStyleData())) {
    // we were provided a pres context so create a new style data
    mStyleData = nsStyleContextData::Create(aPresContext);
    if (nsnull == mStyleData) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    // already have style data - OK!
    rv = NS_OK;
  }
#endif //#ifdef SHARE_STYLECONTEXTS

  return rv;
}

nsresult StyleContextImpl::HaveStyleData(void) const
{
#ifdef SHARE_STYLECONTEXTS
  return (nsnull == mStyleData) ? NS_ERROR_NULL_POINTER : NS_OK;
#else
  return NS_OK;
#endif
}


#ifdef SHARE_STYLECONTEXTS

nsresult StyleContextImpl::ShareStyleDataFrom(nsIStyleContext*aStyleDataDonor)
{
  nsresult rv = NS_OK;

  if (aStyleDataDonor) {
    // XXX - static cast is nasty, especially if there are multiple 
    //       nsIStyleContext implementors...
    StyleContextImpl *donor = NS_STATIC_CAST(StyleContextImpl*,aStyleDataDonor);
    
    // get the data from the donor
    nsStyleContextData *pData = donor->mStyleData;
    if (pData != nsnull) {
      NS_ADDREF(pData);
      NS_IF_RELEASE(mStyleData);
      mStyleData = pData;
    }
  }
  return rv;
}

#ifdef DEBUG
long gFalsePos=0;
long gScreenedByCRC=0;
#endif
NS_IMETHODIMP
StyleContextImpl::StyleDataMatches(nsIStyleContext* aStyleContextToMatch, 
                                   PRBool *aMatches)
{
  NS_ASSERTION((aStyleContextToMatch != nsnull) &&
               (aStyleContextToMatch != this) &&           
               (aMatches != nsnull), "invalid parameter");

  nsresult rv = NS_OK;

  // XXX - static cast is nasty, especially if there are multiple 
  //       nsIStyleContext implementors...
  StyleContextImpl* other = NS_STATIC_CAST(StyleContextImpl*,aStyleContextToMatch);
  *aMatches = PR_FALSE;

  // check same pointer value
  if (other->mStyleData != mStyleData) {
    // check using the crc first: 
    // this will get past the vast majority which do not match, 
    // and if it does match then we'll double check with the more complete comparison
    if (other->mStyleData->GetCRC32() == mStyleData->GetCRC32()) {
      // Might match: validate using the more comprehensive (and slower) CalcStyleDifference
      PRInt32 hint = 0;
      CalcStyleDifference(aStyleContextToMatch, hint, PR_TRUE);
#ifdef DEBUG
      if (hint > 0) {
        gFalsePos++;
        // printf("CRC match but CalcStyleDifference shows differences: crc is not sufficient?");
      }
#endif
      *aMatches = (0 == hint) ? PR_TRUE : PR_FALSE;
    }
#ifdef DEBUG
    else
    {
      // in DEBUG we make sure we are not missing any by getting false-negatives on the CRC
      // XXX NOTE: this should be removed when we trust the CRC matching
      //           as this slows it way down
      gScreenedByCRC++;
      /*
      PRInt32 hint = 0;
      CalcStyleDifference(aStyleContextToMatch, hint, PR_TRUE);
      NS_ASSERTION(hint>0,"!!!FALSE-NEGATIVE in StyleMatchesData!!!");
      */
    }
    // printf("False-Pos: %ld - Screened: %ld\n", gFalsePos, gScreenedByCRC);
#endif
  }
  return rv;
}

NS_IMETHODIMP StyleContextImpl::GetStyleContextKey(scKey &aKey) const
{
  aKey = mStyleData->GetCRC32();
  return NS_OK;
}

nsresult StyleContextImpl::UpdateStyleSetCache( void ) const
{
  if (mStyleSet) {
    // add it to the set: the set does NOT addref or release
    return mStyleSet->AddStyleContext((nsIStyleContext *)this);
  } else {
    return NS_ERROR_FAILURE;
  }
}

#endif

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
    printf( " - StyleFontImpl:          %ld\n", (long)sizeof(GETSCDATA(Font)) );
    totalSize += (long)sizeof(GETSCDATA(Font));
    printf( " - StyleColorImpl:         %ld\n", (long)sizeof(GETSCDATA(Color)) );
    totalSize += (long)sizeof(GETSCDATA(Color));
    printf( " - StyleSpacingImpl:       %ld\n", (long)sizeof(GETSCDATA(Spacing)) );
    totalSize += (long)sizeof(GETSCDATA(Spacing));
    printf( " - StyleListImpl:          %ld\n", (long)sizeof(GETSCDATA(List)) );
    totalSize += (long)sizeof(GETSCDATA(List));
    printf( " - StylePositionImpl:      %ld\n", (long)sizeof(GETSCDATA(Position)) );
    totalSize += (long)sizeof(GETSCDATA(Position));
    printf( " - StyleTextImpl:          %ld\n", (long)sizeof(GETSCDATA(Text)) );
    totalSize += (long)sizeof(GETSCDATA(Text));
    printf( " - StyleDisplayImpl:       %ld\n", (long)sizeof(GETSCDATA(Display)) );
    totalSize += (long)sizeof(GETSCDATA(Display));
    printf( " - StyleTableImpl:         %ld\n", (long)sizeof(GETSCDATA(Table)) );
    totalSize += (long)sizeof(GETSCDATA(Table));
    printf( " - StyleContentImpl:       %ld\n", (long)sizeof(GETSCDATA(Content)) );
    totalSize += (long)sizeof(GETSCDATA(Content));
    printf( " - StyleUserInterfaceImpl: %ld\n", (long)sizeof(GETSCDATA(UserInterface)) );
    totalSize += (long)sizeof(GETSCDATA(UserInterface));
	  printf( " - StylePrintImpl:         %ld\n", (long)sizeof(GETSCDATA(Print)));
    totalSize += (long)sizeof(GETSCDATA(Print));
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

#ifdef SHARE_STYLECONTEXTS
  // count the style data seperately
  if (mStyleData) {
    mStyleData->SizeOf(aSizeOfHandler,localSize);
  }
#endif

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

#ifdef SHARE_STYLECONTEXTS
  context->UpdateStyleSetCache();     // add it to the style set cache
#endif
  return result;
}


//----------------------------------------------------------

#ifdef COMPUTE_STYLEDATA_CRC
/*************************************************************************
 *  The table lookup technique was adapted from the algorithm described  *
 *  by Avram Perez, Byte-wise CRC Calculations, IEEE Micro 3, 40 (1983). *
 *************************************************************************/

#define POLYNOMIAL 0x04c11db7L

static PRBool crc_table_initialized;
static PRUint32 crc_table[256];

void gen_crc_table() {
 /* generate the table of CRC remainders for all possible bytes */
  int i, j;  
  PRUint32 crc_accum;
  for ( i = 0;  i < 256;  i++ ) { 
    crc_accum = ( (unsigned long) i << 24 );
    for ( j = 0;  j < 8;  j++ ) { 
      if ( crc_accum & 0x80000000L )
        crc_accum = ( crc_accum << 1 ) ^ POLYNOMIAL;
      else crc_accum = ( crc_accum << 1 ); 
    }
    crc_table[i] = crc_accum; 
  }
  return; 
}

PRUint32 AccumulateCRC(PRUint32 crc_accum, const char *data_blk_ptr, int data_blk_size)  {
  if (!crc_table_initialized) {
    gen_crc_table();
    crc_table_initialized = PR_TRUE;
  }

 /* update the CRC on the data block one byte at a time */
  int i, j;
  for ( j = 0;  j < data_blk_size;  j++ ) { 
    i = ( (int) ( crc_accum >> 24) ^ *data_blk_ptr++ ) & 0xff;
    crc_accum = ( crc_accum << 8 ) ^ crc_table[i]; 
  }
  return crc_accum; 
}


PRUint32 StyleSideCRC(PRUint32 aCrc,const nsStyleSides *aStyleSides)
{
  PRUint32 crc = 0;
  nsStyleCoord theCoord;
  aStyleSides->GetLeft(theCoord);
  crc = StyleCoordCRC(crc,&theCoord);
  aStyleSides->GetTop(theCoord);
  crc = StyleCoordCRC(crc,&theCoord);
  aStyleSides->GetRight(theCoord);
  crc = StyleCoordCRC(crc,&theCoord);
  aStyleSides->GetBottom(theCoord);
  crc = StyleCoordCRC(crc,&theCoord);
  return crc;
}

PRUint32 StyleCoordCRC(PRUint32 aCrc, const nsStyleCoord* aCoord)
{
  PRUint32 crc = aCrc;
  crc = AccumulateCRC(crc,(const char *)&aCoord->mUnit,sizeof(aCoord->mUnit));
  // using the mInt part of the union below (float and PRInt32 should be the same size...)
  crc = AccumulateCRC(crc,(const char *)&aCoord->mValue.mInt,sizeof(aCoord->mValue.mInt));
  return crc;
}

PRUint32 StyleMarginCRC(PRUint32 aCrc, const nsMargin *aMargin)
{
  PRUint32 crc = aCrc;
  crc = AccumulateCRC(crc,(const char *)&aMargin->left,sizeof(aMargin->left));
  crc = AccumulateCRC(crc,(const char *)&aMargin->top,sizeof(aMargin->top));
  crc = AccumulateCRC(crc,(const char *)&aMargin->right,sizeof(aMargin->right));
  crc = AccumulateCRC(crc,(const char *)&aMargin->bottom,sizeof(aMargin->bottom));
  return crc;
}

PRUint32 StyleStringCRC(PRUint32 aCrc, const nsString *aString)
{
  PRUint32 crc = aCrc;
  PRUint32 len = aString->Length();
  const PRUnichar *p = aString->GetUnicode();
  if (p) {
    crc = AccumulateCRC(crc,(const char *)p,(len*2));
  }
  crc = AccumulateCRC(crc,(const char *)&len,sizeof(len));
  return crc;
}
#endif // #ifdef COMPUTE_STYLEDATA_CRC
