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
 * 03/20/2000   IBM Corp.       Bidi - ability to change the default direction of the browser
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

#include "nsCOMPtr.h"
#include "nsIStyleSet.h"
#include "nsISizeOfHandler.h"
#include "nsIPresShell.h"
#include "nsLayoutAtoms.h"
#include "prenv.h"

#ifdef IBMBIDI
#include "nsIUBidiUtils.h"
#endif

#ifdef DEBUG
//#define NOISY_DEBUG
#endif


#ifdef USE_FAST_CACHE
  // define COMPUTE_STYLEDATA_CRC to actually compute a valid CRC32
  // - Fast cache uses a CRC32 on the style context to quickly find sharing candidates.
  //   Enabling it by defining USE_FAST_CACHE makes style sharing significantly faster.
#define COMPUTE_STYLEDATA_CRC
#endif


//------------------------------------------------------------------------------
//  Helper functions
//------------------------------------------------------------------------------
//

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

#ifdef COMPUTE_STYLEDATA_CRC
static void gen_crc_table();
static PRUint32 AccumulateCRC(PRUint32 crc_accum, const char *data_blk_ptr, int data_blk_size);
static PRUint32 StyleSideCRC(PRUint32 crc,const nsStyleSides *aStyleSides);
static PRUint32 StyleCoordCRC(PRUint32 crc, const nsStyleCoord* aCoord);
static PRUint32 StyleMarginCRC(PRUint32 crc, const nsMargin *aMargin);
static PRUint32 StyleStringCRC(PRUint32 aCrc, const nsString *aString);
#define STYLEDATA_NO_CRC (0)
#define STYLEDATA_DEFAULT_CRC (0xcafebabe)
#endif


#ifdef XP_MAC
#pragma mark -
#endif

struct StyleBlob {
  virtual ~StyleBlob(void) {}

  virtual void           ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext) = 0;
  virtual void           CopyTo(nsStyleStruct* aDest) const = 0;
  virtual void           SetFrom(const nsStyleStruct* aSource) = 0;
  virtual PRInt32        CalcDifference(const nsStyleStruct* aOther) const = 0;
  virtual PRUint32       ComputeCRC32(PRUint32 aCrc) const = 0;

  inline virtual const nsStyleStruct* GetData(void) const = 0;
};

//------------------------------------------------------------------------------
//  nsStyleFont
//------------------------------------------------------------------------------
//
struct StyleFontBlob : public nsStyleFont, public StyleBlob {
  StyleFontBlob(const nsFont& aVariableFont, const nsFont& aFixedFont)
  : nsStyleFont(aVariableFont, aFixedFont)
                               { MOZ_COUNT_CTOR(StyleFontBlob); }

  StyleFontBlob(void)          { MOZ_COUNT_CTOR(StyleFontBlob); }
  virtual ~StyleFontBlob(void) { MOZ_COUNT_DTOR(StyleFontBlob); }

  virtual void ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext);
  virtual void SetFrom(const nsStyleStruct* aSource);
  virtual void CopyTo(nsStyleStruct* aDest) const;
  virtual PRInt32 CalcDifference(const nsStyleStruct* aOther) const;
  virtual PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  inline virtual const nsStyleStruct* GetData(void) const {return (const nsStyleFont*)this;};

  static PRInt32 CalcFontDifference(const nsFont& aFont1, const nsFont& aFont2);

private:  // These are not allowed
  StyleFontBlob(const StyleFontBlob& aOther);
  StyleFontBlob& operator=(const StyleFontBlob& aOther);
};

void StyleFontBlob::ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext)
{
  const nsStyleFont* parent = (nsStyleFont*)aParent;

  if (nsnull != parent) {
    mFont = parent->mFont;
    mFixedFont = parent->mFixedFont;
    mFlags = parent->mFlags;
  }
  else {
    aPresContext->GetDefaultFont(mFont);
    aPresContext->GetDefaultFixedFont(mFixedFont);
    mFlags = NS_STYLE_FONT_DEFAULT;
  }
}

void StyleFontBlob::SetFrom(const nsStyleStruct* aSource)
{
  const nsStyleFont* source = (nsStyleFont*)aSource;

  mFont = source->mFont;
  mFixedFont = source->mFixedFont;
  mFlags = source->mFlags;
}

void StyleFontBlob::CopyTo(nsStyleStruct* aDest) const
{
  nsStyleFont* dest = (nsStyleFont*)aDest;

  dest->mFont = mFont;
  dest->mFixedFont = mFixedFont;
  dest->mFlags = mFlags;
}

PRInt32 StyleFontBlob::CalcDifference(const nsStyleStruct* aOther) const
{
  const nsStyleFont* other = (nsStyleFont*)aOther;

  if (mFlags == other->mFlags) {
    PRInt32 impact = CalcFontDifference(mFont, other->mFont);
    if (impact < NS_STYLE_HINT_REFLOW) {
      impact = CalcFontDifference(mFixedFont, other->mFixedFont);
    }
    return impact;
  }
  return NS_STYLE_HINT_REFLOW;
}

PRInt32 StyleFontBlob::CalcFontDifference(const nsFont& aFont1, const nsFont& aFont2)
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

PRUint32 StyleFontBlob::ComputeCRC32(PRUint32 aCrc) const
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

#ifdef XP_MAC
#pragma mark -
#endif

//------------------------------------------------------------------------------
//  nsStyleColor
//------------------------------------------------------------------------------
//
struct StyleColorBlob: public nsStyleColor, public StyleBlob {
  StyleColorBlob(void)          { MOZ_COUNT_CTOR(StyleColorBlob); }
  virtual ~StyleColorBlob(void) { MOZ_COUNT_DTOR(StyleColorBlob); }

  virtual void ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext);
  virtual void SetFrom(const nsStyleStruct* aSource);
  virtual void CopyTo(nsStyleStruct* aDest) const;
  virtual PRInt32 CalcDifference(const nsStyleStruct* aOther) const;
  virtual PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  inline virtual const nsStyleStruct* GetData(void) const {return (const nsStyleColor*)this;};

private:  // These are not allowed
  StyleColorBlob(const StyleColorBlob& aOther);
  StyleColorBlob& operator=(const StyleColorBlob& aOther);
};

void StyleColorBlob::ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext)
{
  const nsStyleColor* parent = (nsStyleColor*)aParent;

  if (nsnull != parent) {
    mColor = parent->mColor;
    mOpacity = parent->mOpacity;
    mCursor = parent->mCursor; // fix for bugzilla bug 51113
  }
  else {
    if (nsnull != aPresContext) {
      aPresContext->GetDefaultColor(&mColor);
    }
    else {
      mColor = NS_RGB(0x00, 0x00, 0x00);
    }
    mOpacity = 1.0f;
    mCursor = NS_STYLE_CURSOR_AUTO; // fix for bugzilla bug 51113
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
}

void StyleColorBlob::SetFrom(const nsStyleStruct* aSource)
{
  const nsStyleColor* source = (nsStyleColor*)aSource;

  mColor = source->mColor;
 
  mBackgroundAttachment = source->mBackgroundAttachment;
  mBackgroundFlags = source->mBackgroundFlags;
  mBackgroundRepeat = source->mBackgroundRepeat;

  mBackgroundColor = source->mBackgroundColor;
  mBackgroundXPosition = source->mBackgroundXPosition;
  mBackgroundYPosition = source->mBackgroundYPosition;
  mBackgroundImage = source->mBackgroundImage;

  mCursor = source->mCursor;
  mCursorImage = source->mCursorImage;
  mOpacity = source->mOpacity;
}

void StyleColorBlob::CopyTo(nsStyleStruct* aDest) const
{
  nsStyleColor* dest = (nsStyleColor*)aDest;

  dest->mColor = mColor;
 
  dest->mBackgroundAttachment = mBackgroundAttachment;
  dest->mBackgroundFlags = mBackgroundFlags;
  dest->mBackgroundRepeat = mBackgroundRepeat;

  dest->mBackgroundColor = mBackgroundColor;
  dest->mBackgroundXPosition = mBackgroundXPosition;
  dest->mBackgroundYPosition = mBackgroundYPosition;
  dest->mBackgroundImage = mBackgroundImage;

  dest->mCursor = mCursor;
  dest->mCursorImage = mCursorImage;
  dest->mOpacity = mOpacity;
}

PRInt32 StyleColorBlob::CalcDifference(const nsStyleStruct* aOther) const
{
  const nsStyleColor* other = (nsStyleColor*)aOther;

  if ((mColor == other->mColor) && 
      (mBackgroundAttachment == other->mBackgroundAttachment) &&
      (mBackgroundFlags == other->mBackgroundFlags) &&
      (mBackgroundRepeat == other->mBackgroundRepeat) &&
      (mBackgroundColor == other->mBackgroundColor) &&
      (mBackgroundXPosition == other->mBackgroundXPosition) &&
      (mBackgroundYPosition == other->mBackgroundYPosition) &&
      (mBackgroundImage == other->mBackgroundImage) &&
      (mCursor == other->mCursor) &&
      (mCursorImage == other->mCursorImage) &&
      (mOpacity == other->mOpacity)) {
    return NS_STYLE_HINT_NONE;
  }
  return NS_STYLE_HINT_VISUAL;
}

PRUint32 StyleColorBlob::ComputeCRC32(PRUint32 aCrc) const
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


#ifdef XP_MAC
#pragma mark -
#endif

//------------------------------------------------------------------------------
//  nsStyleMargin
//------------------------------------------------------------------------------
//
struct StyleMarginBlob: public nsStyleMargin, public StyleBlob {
  StyleMarginBlob(void)          { MOZ_COUNT_CTOR(StyleMarginBlob); }
  virtual ~StyleMarginBlob(void) { MOZ_COUNT_DTOR(StyleMarginBlob); }

  virtual void ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext);
  virtual void SetFrom(const nsStyleStruct* aSource);
  virtual void CopyTo(nsStyleStruct* aDest) const;
  virtual PRInt32 CalcDifference(const nsStyleStruct* aOther) const;
  virtual PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  inline virtual const nsStyleStruct* GetData(void) const {return (const nsStyleMargin*)this;};

  void RecalcData(void);

private:  // These are not allowed
  StyleMarginBlob(const StyleMarginBlob& aOther);
  StyleMarginBlob& operator=(const StyleMarginBlob& aOther);
};

void StyleMarginBlob::ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext)
{
  // spacing values not inherited
  mMargin.Reset();
  mHasCachedMargin = PR_FALSE;
}

void StyleMarginBlob::SetFrom(const nsStyleStruct* aSource)
{
  nsCRT::memcpy((nsStyleMargin*)this, aSource, sizeof(nsStyleMargin));
}

void StyleMarginBlob::CopyTo(nsStyleStruct* aDest) const
{
  nsCRT::memcpy(aDest, (const nsStyleMargin*)this, sizeof(nsStyleMargin));
}

void StyleMarginBlob::RecalcData(void)
{
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
}

PRInt32 StyleMarginBlob::CalcDifference(const nsStyleStruct* aOther) const
{
  const nsStyleMargin* other = (nsStyleMargin*)aOther;

  if (mMargin == other->mMargin) {
    return NS_STYLE_HINT_NONE;
  }
  return NS_STYLE_HINT_REFLOW;
}

PRUint32 StyleMarginBlob::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
  
#ifdef COMPUTE_STYLEDATA_CRC
  crc = StyleSideCRC(crc,&mMargin);
  crc = AccumulateCRC(crc,(const char *)&mHasCachedMargin,sizeof(mHasCachedMargin));
  if (mHasCachedMargin) {
    crc = StyleMarginCRC(crc,&mCachedMargin);
  }
#endif
  return crc;
}


#ifdef XP_MAC
#pragma mark -
#endif

//------------------------------------------------------------------------------
//  nsStylePadding
//------------------------------------------------------------------------------
//
struct StylePaddingBlob: public nsStylePadding, public StyleBlob {
  StylePaddingBlob(void)          { MOZ_COUNT_CTOR(StylePaddingBlob); }
  virtual ~StylePaddingBlob(void) { MOZ_COUNT_DTOR(StylePaddingBlob); }

  virtual void ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext);
  virtual void SetFrom(const nsStyleStruct* aSource);
  virtual void CopyTo(nsStyleStruct* aDest) const;
  virtual PRInt32 CalcDifference(const nsStyleStruct* aOther) const;
  virtual PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  inline virtual const nsStyleStruct* GetData(void) const {return (const nsStylePadding*)this;};

  void RecalcData(void);

private:  // These are not allowed
  StylePaddingBlob(const StylePaddingBlob& aOther);
  StylePaddingBlob& operator=(const StylePaddingBlob& aOther);
};

void StylePaddingBlob::ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext)
{
  // spacing values not inherited
  mPadding.Reset();
  mHasCachedPadding = PR_FALSE;
}

void StylePaddingBlob::SetFrom(const nsStyleStruct* aSource)
{
  nsCRT::memcpy((nsStylePadding*)this, aSource, sizeof(nsStylePadding));
}

void StylePaddingBlob::CopyTo(nsStyleStruct* aDest) const
{
  nsCRT::memcpy(aDest, (const nsStylePadding*)this, sizeof(nsStylePadding));
}

void StylePaddingBlob::RecalcData(void)
{
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
}

PRInt32 StylePaddingBlob::CalcDifference(const nsStyleStruct* aOther) const
{
  const nsStylePadding* other = (nsStylePadding*)aOther;

  if (mPadding == other->mPadding) {
    return NS_STYLE_HINT_NONE;
  }
  return NS_STYLE_HINT_REFLOW;
}

PRUint32 StylePaddingBlob::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
  
#ifdef COMPUTE_STYLEDATA_CRC
  crc = StyleSideCRC(crc,&mPadding);
  crc = AccumulateCRC(crc,(const char *)&mHasCachedPadding,sizeof(mHasCachedPadding));
  if (mHasCachedPadding) {
    crc = StyleMarginCRC(crc,&mCachedPadding);
  }
#endif
  return crc;
}


#ifdef XP_MAC
#pragma mark -
#endif

//------------------------------------------------------------------------------
//  nsStyleBorder
//------------------------------------------------------------------------------
//
struct StyleBorderBlob: public nsStyleBorder, public StyleBlob {
  StyleBorderBlob(void)
    : mWidthsInitialized(PR_FALSE)
                                 { MOZ_COUNT_CTOR(StyleBorderBlob); }
  virtual ~StyleBorderBlob(void) { MOZ_COUNT_DTOR(StyleBorderBlob); }

  virtual void ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext);
  virtual void SetFrom(const nsStyleStruct* aSource);
  virtual void CopyTo(nsStyleStruct* aDest) const;
  virtual PRInt32 CalcDifference(const nsStyleStruct* aOther) const;
  virtual PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  inline virtual const nsStyleStruct* GetData(void) const {return (const nsStyleBorder*)this;};

  PRBool IsBorderSideVisible(PRUint8 aSide) const;
  void RecalcData(nscolor color);

private:  // These are not allowed
  StyleBorderBlob(const StyleBorderBlob& aOther);
  StyleBorderBlob& operator=(const StyleBorderBlob& aOther);

  // XXX remove with deprecated methods
  PRBool        mWidthsInitialized;
};

void StyleBorderBlob::ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext)
{
  // XXX support mBorderWidhts until deprecated methods are removed
  if (! mWidthsInitialized) {
    float pixelsToTwips = 20.0f;
    if (aPresContext) {
      aPresContext->GetPixelsToTwips(&pixelsToTwips);
    }
    mBorderWidths[NS_STYLE_BORDER_WIDTH_THIN] = NSIntPixelsToTwips(1, pixelsToTwips);
    mBorderWidths[NS_STYLE_BORDER_WIDTH_MEDIUM] = NSIntPixelsToTwips(3, pixelsToTwips);
    mBorderWidths[NS_STYLE_BORDER_WIDTH_THICK] = NSIntPixelsToTwips(5, pixelsToTwips);
    mWidthsInitialized = PR_TRUE;
  }

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

void StyleBorderBlob::SetFrom(const nsStyleStruct* aSource)
{
  nsCRT::memcpy((nsStyleBorder*)this, aSource, sizeof(nsStyleBorder));
}

void StyleBorderBlob::CopyTo(nsStyleStruct* aDest) const
{
  nsCRT::memcpy(aDest, (const nsStyleBorder*)this, sizeof(nsStyleBorder));
}

PRBool StyleBorderBlob::IsBorderSideVisible(PRUint8 aSide) const
{
	PRUint8 borderStyle = GetBorderStyle(aSide);
	return ((borderStyle != NS_STYLE_BORDER_STYLE_NONE)
       && (borderStyle != NS_STYLE_BORDER_STYLE_HIDDEN));
}

void StyleBorderBlob::RecalcData(nscolor aColor)
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
}

PRInt32 StyleBorderBlob::CalcDifference(const nsStyleStruct* aOther) const
{
  const nsStyleBorder* other = (nsStyleBorder*)aOther;

  if ((mBorder == other->mBorder) && 
      (mFloatEdge == other->mFloatEdge)) {
    PRInt32 ix;
    for (ix = 0; ix < 4; ix++) {
      if ((mBorderStyle[ix] != other->mBorderStyle[ix]) || 
          (mBorderColor[ix] != other->mBorderColor[ix])) {
        if ((mBorderStyle[ix] != other->mBorderStyle[ix]) &&
            ((NS_STYLE_BORDER_STYLE_NONE == mBorderStyle[ix]) ||
             (NS_STYLE_BORDER_STYLE_NONE == other->mBorderStyle[ix]) ||
             (NS_STYLE_BORDER_STYLE_HIDDEN == mBorderStyle[ix]) ||          // bug 45754
             (NS_STYLE_BORDER_STYLE_HIDDEN == other->mBorderStyle[ix]))) {
          return NS_STYLE_HINT_REFLOW;  // border on or off
        }
        return NS_STYLE_HINT_VISUAL;
      }
    }
    if (mBorderRadius != other->mBorderRadius) {
      return NS_STYLE_HINT_VISUAL;
    }
    return NS_STYLE_HINT_NONE;
  }
  return NS_STYLE_HINT_REFLOW;
}

PRUint32 StyleBorderBlob::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
  
#ifdef COMPUTE_STYLEDATA_CRC
  crc = StyleSideCRC(crc,&mBorder);
  crc = StyleSideCRC(crc,&mBorderRadius);
  crc = AccumulateCRC(crc,(const char *)&mFloatEdge,sizeof(mFloatEdge));
  crc = AccumulateCRC(crc,(const char *)&mHasCachedBorder,sizeof(mHasCachedBorder));
  if (mHasCachedBorder) {
    crc = StyleMarginCRC(crc,&mCachedBorder);
  }
  crc = AccumulateCRC(crc,(const char *)mBorderStyle,sizeof(mBorderStyle)); // array of 4 elements
  crc = AccumulateCRC(crc,(const char *)mBorderColor,sizeof(mBorderColor)); // array ...
#endif
  return crc;
}


#ifdef XP_MAC
#pragma mark -
#endif

//------------------------------------------------------------------------------
//  nsStyleOutline
//------------------------------------------------------------------------------
//
struct StyleOutlineBlob: public nsStyleOutline, public StyleBlob {
  StyleOutlineBlob(void)
    : mWidthsInitialized(PR_FALSE)
                                  { MOZ_COUNT_CTOR(StyleOutlineBlob); }
  virtual ~StyleOutlineBlob(void) { MOZ_COUNT_DTOR(StyleOutlineBlob); }

  virtual void ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext);
  virtual void SetFrom(const nsStyleStruct* aSource);
  virtual void CopyTo(nsStyleStruct* aDest) const;
  virtual PRInt32 CalcDifference(const nsStyleStruct* aOther) const;
  virtual PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  inline virtual const nsStyleStruct* GetData(void) const {return (const nsStyleOutline*)this;};

  void RecalcData(void);

private:  // These are not allowed
  StyleOutlineBlob(const StyleOutlineBlob& aOther);
  StyleOutlineBlob& operator=(const StyleOutlineBlob& aOther);

  // XXX remove with deprecated methods
  PRBool        mWidthsInitialized;
};

void StyleOutlineBlob::ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext)
{
  // XXX support mBorderWidhts until deprecated methods are removed
  if (! mWidthsInitialized) {
    float pixelsToTwips = 20.0f;
    if (aPresContext) {
      aPresContext->GetPixelsToTwips(&pixelsToTwips);
    }
    mBorderWidths[NS_STYLE_BORDER_WIDTH_THIN] = NSIntPixelsToTwips(1, pixelsToTwips);
    mBorderWidths[NS_STYLE_BORDER_WIDTH_MEDIUM] = NSIntPixelsToTwips(3, pixelsToTwips);
    mBorderWidths[NS_STYLE_BORDER_WIDTH_THICK] = NSIntPixelsToTwips(5, pixelsToTwips);
    mWidthsInitialized = PR_TRUE;
  }

  // spacing values not inherited
  mOutlineRadius.Reset();

  nsStyleCoord  medium(NS_STYLE_BORDER_WIDTH_MEDIUM, eStyleUnit_Enumerated);
  mOutlineWidth = medium;
  mOutlineStyle = NS_STYLE_BORDER_STYLE_NONE;
  mOutlineColor = NS_RGB(0, 0, 0);

  mHasCachedOutline = PR_FALSE;
}

void StyleOutlineBlob::SetFrom(const nsStyleStruct* aSource)
{
  nsCRT::memcpy((nsStyleOutline*)this, aSource, sizeof(nsStyleOutline));
}

void StyleOutlineBlob::CopyTo(nsStyleStruct* aDest) const
{
  nsCRT::memcpy(aDest, (const nsStyleOutline*)this, sizeof(nsStyleOutline));
}

void StyleOutlineBlob::RecalcData(void)
{
  if ((NS_STYLE_BORDER_STYLE_NONE == GetOutlineStyle()) || 
      IsFixedUnit(mOutlineWidth.GetUnit(), PR_TRUE)) {
    if (NS_STYLE_BORDER_STYLE_NONE == GetOutlineStyle()) {
      mCachedOutlineWidth = 0;
    }
    else {
      mCachedOutlineWidth = CalcCoord(mOutlineWidth, mBorderWidths, 3);
    }
    mHasCachedOutline = PR_TRUE;
  }
  else {
    mHasCachedOutline = PR_FALSE;
  }
}

PRInt32 StyleOutlineBlob::CalcDifference(const nsStyleStruct* aOther) const
{
  const nsStyleOutline* other = (nsStyleOutline*)aOther;

  if ((mOutlineWidth != other->mOutlineWidth) ||
      (mOutlineStyle != other->mOutlineStyle) ||
      (mOutlineColor != other->mOutlineColor) ||
      (mOutlineRadius != other->mOutlineRadius)) {
    return NS_STYLE_HINT_VISUAL;	// XXX: should be VISUAL: see bugs 9809 and 9816
  }
  return NS_STYLE_HINT_NONE;
}

PRUint32 StyleOutlineBlob::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
  
#ifdef COMPUTE_STYLEDATA_CRC
  crc = StyleSideCRC(crc,&mOutlineRadius);
  crc = StyleCoordCRC(crc,&mOutlineWidth);
  crc = AccumulateCRC(crc,(const char *)&mHasCachedOutline,sizeof(mHasCachedOutline));
  if (mHasCachedOutline) {
    crc = AccumulateCRC(crc,(const char *)&mCachedOutlineWidth,sizeof(mCachedOutlineWidth));
  }
  crc = AccumulateCRC(crc,(const char *)&mOutlineStyle,sizeof(mOutlineStyle));
  crc = AccumulateCRC(crc,(const char *)&mOutlineColor,sizeof(mOutlineColor));
#endif
  return crc;
}

#ifdef XP_MAC
#pragma mark -
#endif

//------------------------------------------------------------------------------
//  nsStyleList
//------------------------------------------------------------------------------
//
struct StyleListBlob: public nsStyleList, public StyleBlob {
  StyleListBlob(void)          { MOZ_COUNT_CTOR(StyleListBlob); }
  virtual ~StyleListBlob(void) { MOZ_COUNT_DTOR(StyleListBlob); }

  virtual void ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext);
  virtual void SetFrom(const nsStyleStruct* aSource);
  virtual void CopyTo(nsStyleStruct* aDest) const;
  virtual PRInt32 CalcDifference(const nsStyleStruct* aOther) const;
  virtual PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  inline virtual const nsStyleStruct* GetData(void) const {return (const nsStyleList*)this;};

private:  // These are not allowed
  StyleListBlob(const StyleListBlob& aOther);
  StyleListBlob& operator=(const StyleListBlob& aOther);
};

void StyleListBlob::ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext)
{
  const nsStyleList* parent = (nsStyleList*)aParent;

  if (nsnull != parent) {
    mListStyleType = parent->mListStyleType;
    mListStyleImage = parent->mListStyleImage;
    mListStylePosition = parent->mListStylePosition;
  }
  else {
    mListStyleType = NS_STYLE_LIST_STYLE_BASIC;
    mListStylePosition = NS_STYLE_LIST_STYLE_POSITION_OUTSIDE;
    mListStyleImage.Truncate();
  }
}

void StyleListBlob::SetFrom(const nsStyleStruct* aSource)
{
  const nsStyleList* source = (nsStyleList*)aSource;

  mListStyleType = source->mListStyleType;
  mListStylePosition = source->mListStylePosition;
  mListStyleImage = source->mListStyleImage;
}

void StyleListBlob::CopyTo(nsStyleStruct* aDest) const
{
  nsStyleList* dest = (nsStyleList*)aDest;

  dest->mListStyleType = mListStyleType;
  dest->mListStylePosition = mListStylePosition;
  dest->mListStyleImage = mListStyleImage;
}

PRInt32 StyleListBlob::CalcDifference(const nsStyleStruct* aOther) const
{
  const nsStyleList* other = (nsStyleList*)aOther;

  if (mListStylePosition == other->mListStylePosition) {
    if (mListStyleImage == other->mListStyleImage) {
      if (mListStyleType == other->mListStyleType) {
        return NS_STYLE_HINT_NONE;
      }
      return NS_STYLE_HINT_REFLOW;
    }
    return NS_STYLE_HINT_REFLOW;
  }
  return NS_STYLE_HINT_REFLOW;
}

PRUint32 StyleListBlob::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
#ifdef COMPUTE_STYLEDATA_CRC
  crc = AccumulateCRC(crc,(const char *)&mListStyleType,sizeof(mListStyleType));
  crc = AccumulateCRC(crc,(const char *)&mListStylePosition,sizeof(mListStylePosition));
  crc = StyleStringCRC(crc,&mListStyleImage);
#endif
  return crc;
}

#ifdef XP_MAC
#pragma mark -
#endif

//------------------------------------------------------------------------------
//  nsStylePosition
//------------------------------------------------------------------------------
//
struct StylePositionBlob: public nsStylePosition, public StyleBlob {
  StylePositionBlob(void)          { MOZ_COUNT_CTOR(StylePositionBlob); }
  virtual ~StylePositionBlob(void) { MOZ_COUNT_DTOR(StylePositionBlob); }

  virtual void ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext);
  virtual void SetFrom(const nsStyleStruct* aSource);
  virtual void CopyTo(nsStyleStruct* aDest) const;
  virtual PRInt32 CalcDifference(const nsStyleStruct* aOther) const;
  virtual PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  inline virtual const nsStyleStruct* GetData(void) const {return (const nsStylePosition*)this;};

private:  // These are not allowed
  StylePositionBlob(const StylePositionBlob& aOther);
  StylePositionBlob& operator=(const StylePositionBlob& aOther);
};

void StylePositionBlob::ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext)
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

void StylePositionBlob::SetFrom(const nsStyleStruct* aSource)
{
  nsCRT::memcpy((nsStylePosition*)this, aSource, sizeof(nsStylePosition));
}

void StylePositionBlob::CopyTo(nsStyleStruct* aDest) const
{
  nsCRT::memcpy(aDest, (const nsStylePosition*)this, sizeof(nsStylePosition));
}

PRInt32 StylePositionBlob::CalcDifference(const nsStyleStruct* aOther) const
{
  const nsStylePosition* other = (nsStylePosition*)aOther;

  if (mPosition == other->mPosition) {
    if ((mOffset == other->mOffset) &&
        (mWidth == other->mWidth) &&
        (mMinWidth == other->mMinWidth) &&
        (mMaxWidth == other->mMaxWidth) &&
        (mHeight == other->mHeight) &&
        (mMinHeight == other->mMinHeight) &&
        (mMaxHeight == other->mMaxHeight) &&
        (mBoxSizing == other->mBoxSizing) &&
        (mZIndex == other->mZIndex)) {
      return NS_STYLE_HINT_NONE;
    }
    return NS_STYLE_HINT_REFLOW;
  }
  return NS_STYLE_HINT_FRAMECHANGE;
}

PRUint32 StylePositionBlob::ComputeCRC32(PRUint32 aCrc) const
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

#ifdef XP_MAC
#pragma mark -
#endif

//------------------------------------------------------------------------------
//  nsStyleText
//------------------------------------------------------------------------------
//
struct StyleTextBlob: public nsStyleText, public StyleBlob {
  StyleTextBlob(void)          { MOZ_COUNT_CTOR(StyleTextBlob); }
  virtual ~StyleTextBlob(void) { MOZ_COUNT_DTOR(StyleTextBlob); }

  virtual void ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext);
  virtual void SetFrom(const nsStyleStruct* aSource);
  virtual void CopyTo(nsStyleStruct* aDest) const;
  virtual PRInt32 CalcDifference(const nsStyleStruct* aOther) const;
  virtual PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  inline virtual const nsStyleStruct* GetData(void) const {return (const nsStyleText*)this;};

private:  // These are not allowed
  StyleTextBlob(const StyleTextBlob& aOther);
  StyleTextBlob& operator=(const StyleTextBlob& aOther);
};

void StyleTextBlob::ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext)
{
  const nsStyleText* parent = (nsStyleText*)aParent;

  // These properties not inherited
  mTextDecoration = NS_STYLE_TEXT_DECORATION_NONE;
  mVerticalAlign.SetIntValue(NS_STYLE_VERTICAL_ALIGN_BASELINE, eStyleUnit_Enumerated);
//  mVerticalAlign.Reset(); TBI

  if (nsnull != parent) {
    mTextAlign = parent->mTextAlign;
    mTextTransform = parent->mTextTransform;
    mWhiteSpace = parent->mWhiteSpace;
    mLetterSpacing = parent->mLetterSpacing;

    // Inherit everything except percentage line-height values
    nsStyleUnit unit = parent->mLineHeight.GetUnit();
    if ((eStyleUnit_Normal == unit) || (eStyleUnit_Factor == unit) ||
        (eStyleUnit_Coord == unit)) {
      mLineHeight = parent->mLineHeight;
    }
    else {
      mLineHeight.SetInheritValue();
    }
    mTextIndent = parent->mTextIndent;
    mWordSpacing = parent->mWordSpacing;
#ifdef IBMBIDI
    mUnicodeBidi = parent->mUnicodeBidi;
#endif // IBMBIDI
  }
  else {
    mTextAlign = NS_STYLE_TEXT_ALIGN_DEFAULT;
    mTextTransform = NS_STYLE_TEXT_TRANSFORM_NONE;
    mWhiteSpace = NS_STYLE_WHITESPACE_NORMAL;

    mLetterSpacing.SetNormalValue();
    mLineHeight.SetNormalValue();
    mTextIndent.SetCoordValue(0);
    mWordSpacing.SetNormalValue();
#ifdef IBMBIDI
    mUnicodeBidi = NS_STYLE_UNICODE_BIDI_INHERIT;
#endif // IBMBIDI
  }
}

void StyleTextBlob::SetFrom(const nsStyleStruct* aSource)
{
  nsCRT::memcpy((nsStyleText*)this, aSource, sizeof(nsStyleText));
}

void StyleTextBlob::CopyTo(nsStyleStruct* aDest) const
{
  nsCRT::memcpy(aDest, (const nsStyleText*)this, sizeof(nsStyleText));
}

PRInt32 StyleTextBlob::CalcDifference(const nsStyleStruct* aOther) const
{
  const nsStyleText* other = (nsStyleText*)aOther;

  if ((mTextAlign == other->mTextAlign) &&
      (mTextTransform == other->mTextTransform) &&
      (mWhiteSpace == other->mWhiteSpace) &&
      (mLetterSpacing == other->mLetterSpacing) &&
      (mLineHeight == other->mLineHeight) &&
      (mTextIndent == other->mTextIndent) &&
      (mWordSpacing == other->mWordSpacing) &&
#ifdef IBMBIDI
      (mUnicodeBidi == other->mUnicodeBidi) &&
#endif // IBMBIDI
      (mVerticalAlign == other->mVerticalAlign)) {
    if (mTextDecoration == other->mTextDecoration) {
      return NS_STYLE_HINT_NONE;
    }
    return NS_STYLE_HINT_VISUAL;
  }
  return NS_STYLE_HINT_REFLOW;
}

PRUint32 StyleTextBlob::ComputeCRC32(PRUint32 aCrc) const
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
#ifdef IBMBIDI
  crc = AccumulateCRC(crc,(const char *)&mUnicodeBidi,sizeof(mUnicodeBidi));
#endif // IBMBIDI
#endif
  return crc;
}

#ifdef XP_MAC
#pragma mark -
#endif

//------------------------------------------------------------------------------
//  nsStyleDisplay
//------------------------------------------------------------------------------
//
struct StyleDisplayBlob: public nsStyleDisplay, public StyleBlob {
  StyleDisplayBlob(void)          { MOZ_COUNT_CTOR(StyleDisplayBlob); }
  virtual ~StyleDisplayBlob(void) { MOZ_COUNT_DTOR(StyleDisplayBlob); }

  virtual void ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext);
  virtual void SetFrom(const nsStyleStruct* aSource);
  virtual void CopyTo(nsStyleStruct* aDest) const;
  virtual PRInt32 CalcDifference(const nsStyleStruct* aOther) const;
  virtual PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  inline virtual const nsStyleStruct* GetData(void) const {return (const nsStyleDisplay*)this;};

private:  // These are not allowed
  StyleDisplayBlob(const StyleDisplayBlob& aOther);
  StyleDisplayBlob& operator=(const StyleDisplayBlob& aOther);
};

void StyleDisplayBlob::ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext)
{
  const nsStyleDisplay* parent = (nsStyleDisplay*)aParent;

  if (nsnull != parent) {
    mDirection = parent->mDirection;
    mLanguage = parent->mLanguage;
    mVisible = parent->mVisible;
  }
  else {
#ifdef IBMBIDI
    PRUint32 mBidioptions;
    aPresContext->GetBidi(&mBidioptions);
    if (GET_BIDI_OPTION_DIRECTION(mBidioptions) == IBMBIDI_TEXTDIRECTION_RTL) {
      mDirection = NS_STYLE_DIRECTION_RTL;
    }
    else {
      mDirection = NS_STYLE_DIRECTION_LTR;
    }
#else // ifdef IBMBIDI
    aPresContext->GetDefaultDirection(&mDirection);
#endif // IBMBIDI
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
#ifdef IBMBIDI
  mExplicitDirection = NS_STYLE_DIRECTION_INHERIT;
#endif // IBMBIDI
}

void StyleDisplayBlob::SetFrom(const nsStyleStruct* aSource)
{
  const nsStyleDisplay* source = (nsStyleDisplay*)aSource;

  mDirection = source->mDirection;
#ifdef IBMBIDI
  mExplicitDirection = source->mExplicitDirection;
#endif // IBMBIDI
  mDisplay = source->mDisplay;
  mFloats = source->mFloats;
  mBreakType = source->mBreakType;
  mBreakBefore = source->mBreakBefore;
  mBreakAfter = source->mBreakAfter;
  mVisible = source->mVisible;
  mOverflow = source->mOverflow;
  mClipFlags = source->mClipFlags;
  mClip = source->mClip;
  mLanguage = source->mLanguage;
}

void StyleDisplayBlob::CopyTo(nsStyleStruct* aDest) const
{
  nsStyleDisplay* dest = (nsStyleDisplay*)aDest;

  dest->mDirection = mDirection;
#ifdef IBMBIDI
  dest->mExplicitDirection = mExplicitDirection;
#endif // IBMBIDI
  dest->mDisplay = mDisplay;
  dest->mFloats = mFloats;
  dest->mBreakType = mBreakType;
  dest->mBreakBefore = mBreakBefore;
  dest->mBreakAfter = mBreakAfter;
  dest->mVisible = mVisible;
  dest->mOverflow = mOverflow;
  dest->mClipFlags = mClipFlags;
  dest->mClip = mClip;
  dest->mLanguage = mLanguage;
}

PRInt32 StyleDisplayBlob::CalcDifference(const nsStyleStruct* aOther) const
{
  const nsStyleDisplay* other = (nsStyleDisplay*)aOther;

  if ((mDisplay == other->mDisplay) &&
      (mFloats == other->mFloats) &&
      (mOverflow == other->mOverflow)) {
    if ((mDirection == other->mDirection) &&
        (mLanguage == other->mLanguage) &&
        (mBreakType == other->mBreakType) &&
        (mBreakBefore == other->mBreakBefore) &&
        (mBreakAfter == other->mBreakAfter)) {
      if ((mVisible == other->mVisible) &&
          (mClipFlags == other->mClipFlags) &&
          (mClip == other->mClip)) {
        return NS_STYLE_HINT_NONE;
      }
      if ((mVisible != other->mVisible) && 
          ((NS_STYLE_VISIBILITY_COLLAPSE == mVisible) || 
           (NS_STYLE_VISIBILITY_COLLAPSE == other->mVisible))) {
        return NS_STYLE_HINT_REFLOW;
      }
      return NS_STYLE_HINT_VISUAL;
    }
    return NS_STYLE_HINT_REFLOW;
  }
  return NS_STYLE_HINT_FRAMECHANGE;
}

PRUint32 StyleDisplayBlob::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
#ifdef COMPUTE_STYLEDATA_CRC
  crc = AccumulateCRC(crc,(const char *)&mDirection,sizeof(mDirection));
#ifdef IBMBIDI
  crc = AccumulateCRC(crc,(const char *)&mExplicitDirection,sizeof(mExplicitDirection));
#endif // IBMBIDI
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

#ifdef XP_MAC
#pragma mark -
#endif

//------------------------------------------------------------------------------
//  nsStyleTable
//------------------------------------------------------------------------------
//
struct StyleTableBlob: public nsStyleTable, public StyleBlob {
  StyleTableBlob(void);
  virtual ~StyleTableBlob(void) { MOZ_COUNT_DTOR(StyleTableBlob); };

  virtual void ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext);
  virtual void SetFrom(const nsStyleStruct* aSource);
  virtual void CopyTo(nsStyleStruct* aDest) const;
  virtual PRInt32 CalcDifference(const nsStyleStruct* aOther) const;
  virtual PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  inline virtual const nsStyleStruct* GetData(void) const {return (const nsStyleTable*)this;};

private:  // These are not allowed
  StyleTableBlob(const StyleTableBlob& aOther);
  StyleTableBlob& operator=(const StyleTableBlob& aOther);
};

StyleTableBlob::StyleTableBlob()
{ 
  MOZ_COUNT_CTOR(StyleTableBlob);
  ResetFrom(nsnull, nsnull);
}

void StyleTableBlob::ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext)
{
  const nsStyleTable* parent = (nsStyleTable*)aParent;

  // values not inherited
  mLayoutStrategy = NS_STYLE_TABLE_LAYOUT_AUTO;
  mCols  = NS_STYLE_TABLE_COLS_NONE;
  mFrame = NS_STYLE_TABLE_FRAME_NONE;
  mRules = NS_STYLE_TABLE_RULES_ALL;
  mCellPadding.Reset();
  mSpan = 1;

  if (parent) {  // handle inherited properties
    mBorderCollapse = parent->mBorderCollapse;
    mEmptyCells     = parent->mEmptyCells;
    mCaptionSide    = parent->mCaptionSide;
    mBorderSpacingX = parent->mBorderSpacingX;
    mBorderSpacingY = parent->mBorderSpacingY;
    mSpanWidth      = parent->mSpanWidth;
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

void StyleTableBlob::SetFrom(const nsStyleStruct* aSource)
{
  nsCRT::memcpy((nsStyleTable*)this, aSource, sizeof(nsStyleTable));
}

void StyleTableBlob::CopyTo(nsStyleStruct* aDest) const
{
  nsCRT::memcpy(aDest, (const nsStyleTable*)this, sizeof(nsStyleTable));
}

PRInt32 StyleTableBlob::CalcDifference(const nsStyleStruct* aOther) const
{
  const nsStyleTable* other = (nsStyleTable*)aOther;

  if ((mLayoutStrategy == other->mLayoutStrategy) &&
      (mFrame == other->mFrame) &&
      (mRules == other->mRules) &&
      (mBorderCollapse == other->mBorderCollapse) &&
      (mBorderSpacingX == other->mBorderSpacingX) &&
      (mBorderSpacingY == other->mBorderSpacingY) &&
      (mCellPadding == other->mCellPadding) &&
      (mCaptionSide == other->mCaptionSide) &&
      (mCols == other->mCols) &&
      (mSpan == other->mSpan) &&
      (mSpanWidth == other->mSpanWidth)) {
    if (mEmptyCells == other->mEmptyCells) {
      return NS_STYLE_HINT_NONE;
    }
    return NS_STYLE_HINT_VISUAL;
  }
  return NS_STYLE_HINT_REFLOW;
}

PRUint32 StyleTableBlob::ComputeCRC32(PRUint32 aCrc) const
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

#ifdef XP_MAC
#pragma mark -
#endif

//------------------------------------------------------------------------------
//  nsStyleContent
//------------------------------------------------------------------------------
//
struct StyleContentBlob: public nsStyleContent, public StyleBlob {
  StyleContentBlob(void)          { MOZ_COUNT_CTOR(StyleContentBlob); };
  virtual ~StyleContentBlob(void) { MOZ_COUNT_DTOR(StyleContentBlob); };

  virtual void ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext);
  virtual void SetFrom(const nsStyleStruct* aSource);
  virtual void CopyTo(nsStyleStruct* aDest) const;
  virtual PRInt32 CalcDifference(const nsStyleStruct* aOther) const;
  virtual PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  inline virtual const nsStyleStruct* GetData(void) const {return (const nsStyleContent*)this;};

private:  // These are not allowed
  StyleContentBlob(const StyleContentBlob& aOther);
  StyleContentBlob& operator=(const StyleContentBlob& aOther);
};

void
StyleContentBlob::ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext)
{
  const nsStyleContent* parent = (nsStyleContent*)aParent;

  // reset data
  mMarkerOffset.Reset();
  mContentCount = 0;
  DELETE_ARRAY_IF(mContents);
  mIncrementCount = 0;
  DELETE_ARRAY_IF(mIncrements);
  mResetCount = 0;
  DELETE_ARRAY_IF(mResets);

  // inherited data
  if (parent) {
    if (NS_SUCCEEDED(AllocateQuotes(parent->mQuotesCount))) {
      PRUint32 ix = (mQuotesCount * 2);
      while (0 < ix--) {
        mQuotes[ix] = parent->mQuotes[ix];
      }
    }
  }
  else {
    mQuotesCount = 0;
    DELETE_ARRAY_IF(mQuotes);
  }
}

void StyleContentBlob::SetFrom(const nsStyleStruct* aSource)
{
  const nsStyleContent* source = (nsStyleContent*)aSource;

  mMarkerOffset = source->mMarkerOffset;

  PRUint32 index;
  if (NS_SUCCEEDED(AllocateContents(source->ContentCount()))) {
    for (index = 0; index < mContentCount; index++) {
      source->GetContentAt(index, mContents[index].mType, mContents[index].mContent);
    }
  }

  if (NS_SUCCEEDED(AllocateCounterIncrements(source->CounterIncrementCount()))) {
    for (index = 0; index < mIncrementCount; index++) {
      source->GetCounterIncrementAt(index, mIncrements[index].mCounter,
                                           mIncrements[index].mValue);
    }
  }

  if (NS_SUCCEEDED(AllocateCounterResets(source->CounterResetCount()))) {
    for (index = 0; index < mResetCount; index++) {
      source->GetCounterResetAt(index, mResets[index].mCounter,
                                       mResets[index].mValue);
    }
  }

  if (NS_SUCCEEDED(AllocateQuotes(source->QuotesCount()))) {
    PRUint32 count = (mQuotesCount * 2);
    for (index = 0; index < count; index += 2) {
      source->GetQuotesAt(index, mQuotes[index], mQuotes[index + 1]);
    }
  }
}

void StyleContentBlob::CopyTo(nsStyleStruct* aDest) const
{
  nsStyleContent* dest = (nsStyleContent*)aDest;

  dest->mMarkerOffset = mMarkerOffset;

  PRUint32 index;
  if (NS_SUCCEEDED(dest->AllocateContents(mContentCount))) {
    for (index = 0; index < mContentCount; index++) {
      dest->SetContentAt(index, mContents[index].mType,
                                mContents[index].mContent);
    }
  }

  if (NS_SUCCEEDED(dest->AllocateCounterIncrements(mIncrementCount))) {
    for (index = 0; index < mIncrementCount; index++) {
      dest->SetCounterIncrementAt(index, mIncrements[index].mCounter,
                                         mIncrements[index].mValue);
    }
  }

  if (NS_SUCCEEDED(dest->AllocateCounterResets(mResetCount))) {
    for (index = 0; index < mResetCount; index++) {
      dest->SetCounterResetAt(index, mResets[index].mCounter,
                                     mResets[index].mValue);
    }
  }

  if (NS_SUCCEEDED(dest->AllocateQuotes(mQuotesCount))) {
    PRUint32 count = (mQuotesCount * 2);
    for (index = 0; index < count; index += 2) {
      dest->SetQuotesAt(index, mQuotes[index], mQuotes[index + 1]);
    }
  }
}

PRInt32 
StyleContentBlob::CalcDifference(const nsStyleStruct* aOther) const
{
  const nsStyleContent* other = (nsStyleContent*)aOther;

  if (mContentCount == other->mContentCount) {
    if ((mMarkerOffset == other->mMarkerOffset) &&
        (mIncrementCount == other->mIncrementCount) && 
        (mResetCount == other->mResetCount) &&
        (mQuotesCount == other->mQuotesCount)) {
      PRUint32 ix = mContentCount;
      while (0 < ix--) {
        if ((mContents[ix].mType != other->mContents[ix].mType) || 
            (mContents[ix].mContent != other->mContents[ix].mContent)) {
          return NS_STYLE_HINT_REFLOW;
        }
      }
      ix = mIncrementCount;
      while (0 < ix--) {
        if ((mIncrements[ix].mValue != other->mIncrements[ix].mValue) || 
            (mIncrements[ix].mCounter != other->mIncrements[ix].mCounter)) {
          return NS_STYLE_HINT_REFLOW;
        }
      }
      ix = mResetCount;
      while (0 < ix--) {
        if ((mResets[ix].mValue != other->mResets[ix].mValue) || 
            (mResets[ix].mCounter != other->mResets[ix].mCounter)) {
          return NS_STYLE_HINT_REFLOW;
        }
      }
      ix = (mQuotesCount * 2);
      while (0 < ix--) {
        if (mQuotes[ix] != other->mQuotes[ix]) {
          return NS_STYLE_HINT_REFLOW;
        }
      }
      return NS_STYLE_HINT_NONE;
    }
    return NS_STYLE_HINT_REFLOW;
  }
  return NS_STYLE_HINT_FRAMECHANGE;
}

PRUint32 StyleContentBlob::ComputeCRC32(PRUint32 aCrc) const
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

#ifdef XP_MAC
#pragma mark -
#endif

//------------------------------------------------------------------------------
//  nsStyleUserInterface
//------------------------------------------------------------------------------
//
struct StyleUserInterfaceBlob: public nsStyleUserInterface, public StyleBlob {
  StyleUserInterfaceBlob(void)           { MOZ_COUNT_CTOR(StyleUserInterfaceBlob); }
  virtual ~StyleUserInterfaceBlob(void)  { MOZ_COUNT_DTOR(StyleUserInterfaceBlob); }

  virtual void ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext);
  virtual void SetFrom(const nsStyleStruct* aSource);
  virtual void CopyTo(nsStyleStruct* aDest) const;
  virtual PRInt32 CalcDifference(const nsStyleStruct* aOther) const;
  virtual PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  inline virtual const nsStyleStruct* GetData(void) const {return (const nsStyleUserInterface*)this;};

private:  // These are not allowed
  StyleUserInterfaceBlob(const StyleUserInterfaceBlob& aOther);
  StyleUserInterfaceBlob& operator=(const StyleUserInterfaceBlob& aOther);
};

void StyleUserInterfaceBlob::ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext)
{
  const nsStyleUserInterface* parent = (nsStyleUserInterface*)aParent;

  if (parent) {
    mUserInput = parent->mUserInput;
    mUserModify = parent->mUserModify;
    mUserFocus = parent->mUserFocus;
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

void StyleUserInterfaceBlob::SetFrom(const nsStyleStruct* aSource)
{
  const nsStyleUserInterface* source = (nsStyleUserInterface*)aSource;

  mUserInput = source->mUserInput;
  mUserModify = source->mUserModify;
  mUserFocus = source->mUserFocus;

  mUserSelect = source->mUserSelect;
  mKeyEquivalent = source->mKeyEquivalent;
  mResizer = source->mResizer;
  mBehavior = source->mBehavior;
}

void StyleUserInterfaceBlob::CopyTo(nsStyleStruct* aDest) const
{
  nsStyleUserInterface* dest = (nsStyleUserInterface*)aDest;

  dest->mUserInput = mUserInput;
  dest->mUserModify = mUserModify;
  dest->mUserFocus = mUserFocus;

  dest->mUserSelect = mUserSelect;
  dest->mKeyEquivalent = mKeyEquivalent;
  dest->mResizer = mResizer;
  dest->mBehavior = mBehavior;
}

PRInt32 StyleUserInterfaceBlob::CalcDifference(const nsStyleStruct* aOther) const
{
  const nsStyleUserInterface* other = (nsStyleUserInterface*)aOther;

  if (mBehavior != other->mBehavior)
    return NS_STYLE_HINT_FRAMECHANGE;

  if ((mUserInput == other->mUserInput) && 
      (mResizer == other->mResizer)) {
    if ((mUserModify == other->mUserModify) && 
        (mUserSelect == other->mUserSelect)) {
      if ((mKeyEquivalent == other->mKeyEquivalent) &&
          (mUserFocus == other->mUserFocus) &&
          (mResizer == other->mResizer)) {
        return NS_STYLE_HINT_NONE;
      }
      return NS_STYLE_HINT_CONTENT;
    }
    return NS_STYLE_HINT_VISUAL;
  }
  if ((mUserInput != other->mUserInput) &&
      ((NS_STYLE_USER_INPUT_NONE == mUserInput) || 
       (NS_STYLE_USER_INPUT_NONE == other->mUserInput))) {
    return NS_STYLE_HINT_FRAMECHANGE;
  }
  return NS_STYLE_HINT_VISUAL;
}

PRUint32 StyleUserInterfaceBlob::ComputeCRC32(PRUint32 aCrc) const
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

#ifdef XP_MAC
#pragma mark -
#endif

//------------------------------------------------------------------------------
//  nsStylePrint
//------------------------------------------------------------------------------
//
struct StylePrintBlob: public nsStylePrint, public StyleBlob {
  StylePrintBlob(void)          { MOZ_COUNT_CTOR(StylePrintBlob); }
  virtual ~StylePrintBlob(void) { MOZ_COUNT_DTOR(StylePrintBlob); }

  virtual void ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext);
  virtual void SetFrom(const nsStyleStruct* aSource);
  virtual void CopyTo(nsStyleStruct* aDest) const;
  virtual PRInt32 CalcDifference(const nsStyleStruct* aOther) const;
  virtual PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  inline virtual const nsStyleStruct* GetData(void) const {return (const nsStylePrint*)this;};

private:  // These are not allowed
  StylePrintBlob(const StylePrintBlob& aOther);
  StylePrintBlob& operator=(const StylePrintBlob& aOther);
};

void StylePrintBlob::ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext)
{
  const nsStylePrint* parent = (nsStylePrint*)aParent;

  if (parent) {
    mPageBreakBefore = parent->mPageBreakBefore;
    mPageBreakAfter = parent->mPageBreakAfter;
    mPageBreakInside = parent->mPageBreakInside;
    mWidows = parent->mWidows;
    mOrphans = parent->mOrphans;
		mMarks = parent->mMarks;
		mSizeWidth = parent->mSizeWidth;
		mSizeHeight = parent->mSizeHeight;
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

void StylePrintBlob::SetFrom(const nsStyleStruct* aSource)
{
  const nsStylePrint* source = (nsStylePrint*)aSource;

  mPageBreakBefore = source->mPageBreakBefore;
  mPageBreakAfter = source->mPageBreakAfter;
  mPageBreakInside = source->mPageBreakInside;
  mPage = source->mPage;
  mWidows = source->mWidows;
  mOrphans = source->mOrphans;
  mMarks = source->mMarks;
  mSizeWidth = source->mSizeWidth;
  mSizeHeight = source->mSizeHeight;
}

void StylePrintBlob::CopyTo(nsStyleStruct* aDest) const
{
  nsStylePrint* dest = (nsStylePrint*)aDest;

  dest->mPageBreakBefore = mPageBreakBefore;
  dest->mPageBreakAfter = mPageBreakAfter;
  dest->mPageBreakInside = mPageBreakInside;
  dest->mPage = mPage;
  dest->mWidows = mWidows;
  dest->mOrphans = mOrphans;
  dest->mMarks = mMarks;
  dest->mSizeWidth = mSizeWidth;
  dest->mSizeHeight = mSizeHeight;
}

PRInt32 StylePrintBlob::CalcDifference(const nsStyleStruct* aOther) const
{
  const nsStylePrint* other = (nsStylePrint*)aOther;

	if ((mPageBreakBefore == other->mPageBreakBefore)
	&& (mPageBreakAfter == other->mPageBreakAfter)
	&& (mPageBreakInside == other->mPageBreakInside)
	&& (mWidows == other->mWidows)
	&& (mOrphans == other->mOrphans)
	&& (mMarks == other->mMarks)
	&& (mSizeWidth == other->mSizeWidth)
	&& (mSizeHeight == other->mSizeHeight)) {
  	return NS_STYLE_HINT_NONE;
	}

	if (mMarks != other->mMarks) {
  	return NS_STYLE_HINT_VISUAL;
	}
  return NS_STYLE_HINT_REFLOW;
}

PRUint32 StylePrintBlob::ComputeCRC32(PRUint32 aCrc) const
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

#ifdef XP_MAC
#pragma mark -
#endif

#ifdef INCLUDE_XUL
//------------------------------------------------------------------------------
//  nsStyleXUL
//------------------------------------------------------------------------------
//
struct StyleXULBlob: public nsStyleXUL, public StyleBlob {
  StyleXULBlob()          { MOZ_COUNT_CTOR(StyleXULBlob); }
  virtual ~StyleXULBlob() { MOZ_COUNT_DTOR(StyleXULBlob); }

  virtual void ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext);
  virtual void SetFrom(const nsStyleStruct* aSource);
  virtual void CopyTo(nsStyleStruct* aDest) const;
  virtual PRInt32 CalcDifference(const nsStyleStruct* aOther) const;
  virtual PRUint32 ComputeCRC32(PRUint32 aCrc) const;
  inline virtual const nsStyleStruct* GetData(void) const {return (const nsStyleXUL*)this;};

private:  // These are not allowed
  StyleXULBlob(const StyleXULBlob& aOther);
  StyleXULBlob& operator=(const StyleXULBlob& aOther);
};

void StyleXULBlob::ResetFrom(const nsStyleStruct* aParent, nsIPresContext* aPresContext)
{
  mBoxOrient = NS_STYLE_BOX_ORIENT_HORIZONTAL;
}

void StyleXULBlob::SetFrom(const nsStyleStruct* aSource)
{
  const nsStyleXUL* source = (nsStyleXUL*)aSource;

  mBoxOrient = source->mBoxOrient;
}

void StyleXULBlob::CopyTo(nsStyleStruct* aDest) const
{
  nsStyleXUL* dest = (nsStyleXUL*)aDest;

  dest->mBoxOrient = mBoxOrient;
}

PRInt32 StyleXULBlob::CalcDifference(const nsStyleStruct* aOther) const
{
  const nsStyleXUL* other = (nsStyleXUL*)aOther;

  if (mBoxOrient == other->mBoxOrient)
    return NS_STYLE_HINT_NONE;
  return NS_STYLE_HINT_REFLOW;
}

PRUint32 StyleXULBlob::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;
#ifdef COMPUTE_STYLEDATA_CRC
  crc = AccumulateCRC(crc,(const char *)&mBoxOrient,sizeof(mBoxOrient));
#endif
  return crc;
}
#endif // INCLUDE_XUL

#ifdef XP_MAC
#pragma mark -
#endif

//------------------------------------------------------------------------------
//  nsStyleContextData
//------------------------------------------------------------------------------
//

// ---------------------
// Macros for getting style data (StyleXXXBlob and nsStyleXXX)
//
#define STYLEDATA(type)   nsStyle##type
#define STYLEBLOB(type)   Style##type##Blob
#define BLOBARRAY         mStyleData->mData.mBlobArray
#define GETDATA(type)     ((const nsStyle##type##*)FetchInheritedStyleStruct(eStyleStruct_##type))
#define GETBLOB(type)     ((STYLEBLOB(##type##)*)FetchInheritedStyleBlob(eStyleStruct_##type))

                        // Use the macro below with extreme caution.  The only place where
                        // we want, and where we *can*, access the data pointers directly
                        // for modification (ie. non-const pointers) is in RemapStyle()
                        // because we are then garanteed to have the blobs in place.
                        // Everywhere else, the pointers can be null. 
                        //
                        // On a related note... We never want to cast the pointer returned
                        // by FetchInheritedStyleStruct() to a non-const pointer because it
                        // would modify the parent's style (or the grand-parent's style) 
                        // but not the current context's style.
#define GETMUTABLEDATAPTR(type)   ((nsStyle##type##*)BLOBARRAY[eStyleStruct_##type - 1]->GetData())

                        // Same warning as above, except that it returns a const pointer.
                        // It's just a faster alternative to GETDATA() for use inside
                        // RemapStyle() only.
#define GETDATAPTR(type)          ((const nsStyle##type##*)BLOBARRAY[eStyleStruct_##type - 1]->GetData())


// ---------------------
// Debug definitions
//
#ifdef DEBUG //xxx pierre
                        // Define this to get stats on the use of the
                        // nsStyleStructs inside a nsStyleContextData.
//#define LOG_STYLE_STRUCTS

                        // Define this to get statistics on the number of calls
                        // to GetStyleData() and their depth in the Style tree.
//#define LOG_GET_STYLE_DATA_CALLS

                        // Define this to get statistics on the number of calls
                        // to WriteMutableStyleData().
//#define LOG_WRITE_STYLE_DATA_CALLS


#ifdef LOG_STYLE_STRUCTS
class StyleFontImplLog;
class StyleColorImplLog;
class StyleListImplLog;
class StylePositionImplLog;
class StyleTextImplLog;
class StyleDisplayImplLog;
class StyleTableImplLog;
class StyleContentImplLog;
class StyleUserInterfaceImplLog;
class StylePrintImplLog;
class StyleMarginImplLog;
class StylePaddingImplLog;
class StyleBorderImplLog;
class StyleOutlineImplLog;
#undef STYLEBLOB(data)
#define STYLEBLOB(data)  Style##data##ImplLog
#endif //LOG_STYLE_STRUCTS

#endif //DEBUG


// ---------------------
// nsStyleContextData
//
class nsStyleContextData
{
public:

  friend class StyleContextImpl;

  void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);

private:  // all data and methods private: only friends have access

#ifdef LOG_STYLE_STRUCTS
public:
#endif
#ifdef LOG_WRITE_STYLE_DATA_CALLS
public:
#endif

#ifdef SHARE_STYLECONTEXTS
  static nsStyleContextData *Create(nsIStyleContext* aStyleContext, nsIPresContext *aPresContext);
#endif
  static StyleBlob* AllocateOneBlob(nsStyleStructID aSID, nsIPresContext *aPresContext);
  nsresult AllocateStyleBlobs(nsIStyleContext* aStyleContext, nsIPresContext *aPresContext);
  void DeleteStyleBlobs();

  nsStyleContextData();
  ~nsStyleContextData(void);

  // the style data...
  // - StyleContextImpl gets friend-access
  //
  union {
    StyleBlob*   mBlobArray[eStyleStruct_Max];
    struct {
      STYLEBLOB(Font)*           mFont;
      STYLEBLOB(Color)*          mColor;
      STYLEBLOB(List)*           mList;
      STYLEBLOB(Position)*       mPosition;
      STYLEBLOB(Text)*           mText;
      STYLEBLOB(Display)*        mDisplay;
      STYLEBLOB(Table)*          mTable;
      STYLEBLOB(Content)*        mContent;
      STYLEBLOB(UserInterface)*  mUserInterface;
      STYLEBLOB(Print)*					 mPrint;
      STYLEBLOB(Margin)*				 mMargin;
      STYLEBLOB(Padding)*				 mPadding;
      STYLEBLOB(Border)*				 mBorder;
      STYLEBLOB(Outline)*				 mOutline;
#ifdef INCLUDE_XUL
      STYLEBLOB(XUL)*			    	 mXUL;
#endif
      //#insert new style structs here#
    } mBlobs;
  } mData;

#ifdef SHARE_STYLECONTEXTS
  PRUint32    mRefCnt;
  inline PRUint32 AddRef(void);
  inline PRUint32 Release(void);
#ifdef DEBUG
  static PRUint32 gInstanceCount;
#endif
#ifdef COMPUTE_STYLEDATA_CRC
  PRUint32    mCRC;
  PRUint32    ComputeCRC32(PRUint32 aCrc) const;
  PRUint32    GetCRC32(void) const { return mCRC; }
  void        SetCRC32(void) {
                mCRC = ComputeCRC32(0);
                if (mCRC==STYLEDATA_NO_CRC) {
                  mCRC = STYLEDATA_DEFAULT_CRC;
                }
              }
#endif // COMPUTE_STYLEDATA_CRC
#endif // SHARE_STYLECONTEXTS

#ifdef LOG_STYLE_STRUCTS
	PRBool mGotMutable[eStyleStruct_Max];
#endif
};

#ifdef DEBUG //xxx pierre
#include "nsStyleContextDebug.h"
#endif

#ifdef SHARE_STYLECONTEXTS
// ---------------------
// AddRef
//
inline PRUint32 nsStyleContextData::AddRef(void)
{
  ++mRefCnt;
  NS_LOG_ADDREF(this,mRefCnt,"nsStyleContextData",sizeof(*this));
  return mRefCnt;
}

// ---------------------
// Release
//
inline PRUint32 nsStyleContextData::Release(void)
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

// ---------------------
// Create
//
nsStyleContextData *nsStyleContextData::Create(nsIStyleContext* aStyleContext, nsIPresContext *aPresContext)
{
  NS_ASSERTION(aPresContext != nsnull, "parameter cannot be null");
  nsStyleContextData *pData = nsnull;
  if (aPresContext) {
    pData = new nsStyleContextData();
    if (pData) {
      if (NS_SUCCEEDED(pData->AllocateStyleBlobs(aStyleContext, aPresContext))) {
#ifdef SHARE_STYLECONTEXTS
        NS_ADDREF(pData);
#ifdef NOISY_DEBUG
        printf("new nsStyleContextData instance: (%ld) CRC=%lu\n", 
             (long)(++gInstanceCount), (unsigned long)pData->ComputeCRC32(0));
#endif // NOISY_DEBUG
#endif // SHARE_STYLECONTEXTS
      }
      else {
        delete pData;
        pData = nsnull;
      }
    }
  }
  return pData;
}
#endif //SHARE_STYLECONTEXTS


// ---------------------
// nsStyleContextData ctor
//
nsStyleContextData::nsStyleContextData()
{
#ifdef SHARE_STYLECONTEXTS
  mRefCnt = 0;
#ifdef COMPUTE_STYLEDATA_CRC
  mCRC = 0;
#endif
#endif

  for (short i = 0; i < eStyleStruct_Max; i ++) {
    mData.mBlobArray[i] = nsnull;
#ifdef LOG_STYLE_STRUCTS
    mGotMutable[i] = PR_FALSE;
#endif
  }
}

// ---------------------
// AllocateOneBlob
//
StyleBlob* nsStyleContextData::AllocateOneBlob(nsStyleStructID aSID, nsIPresContext *aPresContext)
{
  StyleBlob* result = nsnull;
  switch (aSID) {
    case eStyleStruct_Font:
      if (aPresContext) {
        result = new STYLEBLOB(Font)(aPresContext->GetDefaultFontDeprecated(), aPresContext->GetDefaultFixedFontDeprecated());
      }
      else {
        result = new STYLEBLOB(Font);
      }
      break;
    case eStyleStruct_Color:          result = new STYLEBLOB(Color);         break;
    case eStyleStruct_List:           result = new STYLEBLOB(List);          break;
    case eStyleStruct_Position:       result = new STYLEBLOB(Position);      break;
    case eStyleStruct_Text:           result = new STYLEBLOB(Text);          break;
    case eStyleStruct_Display:        result = new STYLEBLOB(Display);       break;
    case eStyleStruct_Table:          result = new STYLEBLOB(Table);         break;
    case eStyleStruct_Content:        result = new STYLEBLOB(Content);       break;
    case eStyleStruct_UserInterface:  result = new STYLEBLOB(UserInterface); break;
    case eStyleStruct_Print:          result = new STYLEBLOB(Print);         break;
    case eStyleStruct_Margin:         result = new STYLEBLOB(Margin);        break;
    case eStyleStruct_Padding:        result = new STYLEBLOB(Padding);       break;
    case eStyleStruct_Border:         result = new STYLEBLOB(Border);        break;
    case eStyleStruct_Outline:        result = new STYLEBLOB(Outline);       break;
#ifdef INCLUDE_XUL
    case eStyleStruct_XUL:            result = new STYLEBLOB(XUL);       break;
#endif
    //#insert new style structs here#
    default:
      NS_ERROR("Invalid style struct id");
      break;
  }
  NS_ASSERTION(result != nsnull, "Out of mem");
  return result;
}

// ---------------------
// CanShareStyleData
//       Sharing of style data for the GfxScrollFrame is problematic
//       and is currently disabled. These pseudos indicate the use of a GfxScrollFrame
//       so we check for them and disallow sharing when any are found.
//       If you haven't guessed it, this is a total hack until we can figure out
//       why the GfxScrollFrame is not happy having its style data shared... 
//       (See bugzilla bug 39618 which also documents this problem)
//
static PRBool CanShareStyleData(nsIStyleContext* aStyleContext)
{
  PRBool isSharingSupported = PR_TRUE;

  nsIAtom* pseudoTag;
  aStyleContext->GetPseudoType(pseudoTag);
  if (pseudoTag) {
    if (pseudoTag == nsLayoutAtoms::viewportPseudo ||
        pseudoTag == nsLayoutAtoms::canvasPseudo ||
        pseudoTag == nsLayoutAtoms::viewportScrollPseudo ||
        pseudoTag == nsLayoutAtoms::scrolledContentPseudo ||
        pseudoTag == nsLayoutAtoms::selectScrolledContentPseudo) {
      isSharingSupported = PR_FALSE;
    } else {
      isSharingSupported = PR_TRUE;
    }
  }
  NS_IF_RELEASE(pseudoTag);

  return isSharingSupported;
}


// ---------------------
// AllocateStyleBlobs
//
nsresult nsStyleContextData::AllocateStyleBlobs(nsIStyleContext* aStyleContext, nsIPresContext *aPresContext)
{
  short i;

  // clear the pointers: we'll inherit from the parent (if we have a parent)
  for (i = 0; i < eStyleStruct_Max; i ++) {
    mData.mBlobArray[i] = nsnull;
  }

  if (CanShareStyleData(aStyleContext)) {
  	nsIStyleContext* parent = aStyleContext->GetParent();
    if (parent != nsnull) {
      NS_RELEASE(parent);
      return NS_OK;
    }
  }

  // no parent: allocate the blobs
  for (i = 0; i < eStyleStruct_Max; i ++) {
    nsStyleStructID structID = (nsStyleStructID)(i + 1);
    mData.mBlobArray[i] = AllocateOneBlob(structID, aPresContext);
    if (!mData.mBlobArray[i]) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return NS_OK;
}

// ---------------------
// DeleteStyleBlobs
//
#define NS_IF_DELETE(ptr)   \
  if (nsnull != ptr) { delete ptr; ptr = nsnull; }

void nsStyleContextData::DeleteStyleBlobs() {
  for (short i = 0; i < eStyleStruct_Max; i ++) {
    NS_IF_DELETE(mData.mBlobArray[i]);
  }
}

// ---------------------
// nsStyleContextData dtor
//
nsStyleContextData::~nsStyleContextData(void)
{
#ifdef SHARE_STYLECONTEXTS
  NS_ASSERTION(0 == mRefCnt, "RefCount error in ~nsStyleContextData");
#endif
  // debug here...
#ifdef LOG_STYLE_STRUCTS
  LogStyleStructs(this);
#endif
  DeleteStyleBlobs();
}

// ---------------------
// ComputeCRC32
//
#ifdef COMPUTE_STYLEDATA_CRC
PRUint32 nsStyleContextData::ComputeCRC32(PRUint32 aCrc) const
{
  PRUint32 crc = aCrc;

  // have each style blob compute its own CRC, propagating the previous value...
  for (short i = 0; i < eStyleStruct_Max; i ++) {
    if (mData.mBlobArray[i]) {
      crc = mData.mBlobArray[i]->ComputeCRC32(crc);
    }
  }
  return crc;
}
#endif

// ---------------------
// SizeOf
//
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
  // add the sizes of the individual style blobs
  if (mData.mBlobs.mFont)          aSize += sizeof(StyleFontBlob);
  if (mData.mBlobs.mColor)         aSize += sizeof(StyleColorBlob);
  if (mData.mBlobs.mList)          aSize += sizeof(StyleListBlob);
  if (mData.mBlobs.mPosition)      aSize += sizeof(StylePositionBlob);
  if (mData.mBlobs.mText)          aSize += sizeof(StyleTextBlob);
  if (mData.mBlobs.mDisplay)       aSize += sizeof(StyleDisplayBlob);
  if (mData.mBlobs.mTable)         aSize += sizeof(StyleTableBlob);
  if (mData.mBlobs.mContent)       aSize += sizeof(StyleContentBlob);
  if (mData.mBlobs.mUserInterface) aSize += sizeof(StyleUserInterfaceBlob);
  if (mData.mBlobs.mPrint)         aSize += sizeof(StylePrintBlob);
  if (mData.mBlobs.mMargin)        aSize += sizeof(StyleMarginBlob);
  if (mData.mBlobs.mPadding)       aSize += sizeof(StylePaddingBlob);
  if (mData.mBlobs.mBorder)        aSize += sizeof(StyleBorderBlob);
  if (mData.mBlobs.mOutline)       aSize += sizeof(StyleOutlineBlob);
#ifdef INCLUDE_XUL
  if (mData.mBlobs.mXUL)           aSize += sizeof(StyleXULBlob);
#endif
  //#insert new style structs here#

  aSizeOfHandler->AddSize(tag,aSize);
}


//------------------------------------------------------------------------------
//  StyleContextImpl
//------------------------------------------------------------------------------
//
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

  virtual nsresult WrapRemapStyle(nsIPresContext* aPresContext, PRBool aEnterRemapStyle);
  NS_IMETHOD RemapStyle(nsIPresContext* aPresContext, PRBool aRecurse = PR_TRUE);

  //---------------------------
  //  Read/Write Style Data
  NS_IMETHOD GetStyle(nsStyleStructID aSID, nsStyleStruct& aStruct) const;
  inline virtual const nsStyleStruct* GetStyleData(nsStyleStructID aSID);

protected:
  inline const nsStyleStruct* FetchInheritedStyleStruct(nsStyleStructID aSID) const {
    return FetchInheritedStyleBlob(aSID)->GetData();
  };
  virtual StyleBlob* FetchInheritedStyleBlob(nsStyleStructID aSID) const;
  virtual void ReadMutableStyleData(nsStyleStructID aSID, nsStyleStruct** aStyleStructPtr);
  virtual nsresult WriteMutableStyleData(nsStyleStructID aSID, nsStyleStruct* aStyleStruct);
  virtual nsresult WriteStyleData(nsStyleStructID aSID, const nsStyleStruct* aStyleStruct);

  //---------------------------
public:
  virtual void ForceUnique(void);
  virtual void RecalcAutomaticData(nsIPresContext* aPresContext);
  virtual void CalcBorderPaddingFor(const nsIFrame* aFrame, nsMargin& aBorderPadding) const;
  NS_IMETHOD  CalcStyleDifference(nsIStyleContext* aOther, PRInt32& aHint,PRBool aStopAtFirstDifference = PR_FALSE) const;

#ifdef SHARE_STYLECONTEXTS
  //---------------------------
  // Style Context Sharing
  //
  // evaluate and execute the style data sharing
  // - if nothing to share, it leaves the current style data intact,
  //   otherwise it calls ShareStyleDataFrom to share another context's data
  //   and releases the old data
  nsresult ShareStyleData(void);

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
#endif // SHARE_STYLECONTEXTS

  //---------------------------
  // Regression Data
  //
  virtual void  List(FILE* out, PRInt32 aIndent);

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);

#ifdef DEBUG
  virtual void DumpRegressionData(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent);
#endif

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
  PRInt16           mDataCode;

#ifdef LOG_WRITE_STYLE_DATA_CALLS
public:
#endif
  nsStyleContextData*     mStyleData;
#ifndef SHARE_STYLECONTEXTS
  nsStyleContextData      mStyleDataImpl;
#endif

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

#ifdef XP_MAC
#pragma mark -
#endif

//------------------------------------------------------------------------------
//  StyleContextImpl ctor
//------------------------------------------------------------------------------
//
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
    mStyleData(nsnull)
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
  // remember the style set (for style context sharing)
  nsIPresShell* shell = nsnull;
  aPresContext->GetShell(&shell);
  if (shell) {
    shell->GetStyleSet( getter_AddRefs(mStyleSet) );
    NS_RELEASE( shell );
  }
#endif // SHARE_STYLECONTEXTS
}


//------------------------------------------------------------------------------
//  StyleContextImpl dtor
//------------------------------------------------------------------------------
//
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


//------------------------------------------------------------------------------
//  QueryInterface
//------------------------------------------------------------------------------
//
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

//------------------------------------------------------------------------------
//  GetParent
//------------------------------------------------------------------------------
//
nsIStyleContext* StyleContextImpl::GetParent(void) const
{
  NS_IF_ADDREF(mParent);
  return mParent;
}

//------------------------------------------------------------------------------
//  AppendChild
//------------------------------------------------------------------------------
//
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

//------------------------------------------------------------------------------
//  RemoveChild
//------------------------------------------------------------------------------
//
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

//------------------------------------------------------------------------------
//  GetStyleRules
//------------------------------------------------------------------------------
//
nsISupportsArray* StyleContextImpl::GetStyleRules(void) const
{
  nsISupportsArray* result = mRules;
  NS_IF_ADDREF(result);
  return result;
}

//------------------------------------------------------------------------------
//  GetStyleRuleCount
//------------------------------------------------------------------------------
//
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

//------------------------------------------------------------------------------
//  GetPseudoType
//------------------------------------------------------------------------------
//
NS_IMETHODIMP
StyleContextImpl::GetPseudoType(nsIAtom*& aPseudoTag) const
{
  aPseudoTag = mPseudoTag;
  NS_IF_ADDREF(aPseudoTag);
  return NS_OK;
}

//------------------------------------------------------------------------------
//  FindChildWithRules
//------------------------------------------------------------------------------
//
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

//------------------------------------------------------------------------------
//  Equals
//------------------------------------------------------------------------------
//
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

//------------------------------------------------------------------------------
//  HashValue
//------------------------------------------------------------------------------
//
PRUint32 StyleContextImpl::HashValue(void) const
{
  return mRuleHash;
}

//------------------------------------------------------------------------------
//  FetchInheritedStyleBlob
//------------------------------------------------------------------------------
// For a given type of style data, get the first non-null pointer
// in the style context hierarchy, .  When a pointer is null, it means
// that we are inheriting that particular style from the parent.  Style
// contexts without parent have all their style data allocated.
//
// IMPORTANT: Do not use the GETDATA() macros in this function
//
StyleBlob* StyleContextImpl::FetchInheritedStyleBlob(nsStyleStructID aSID) const
{
  NS_PRECONDITION(aSID > 0 && aSID <= eStyleStruct_Max, "Invalid style struct id");

  short index = aSID - 1;
  StyleBlob* result = nsnull;
  const StyleContextImpl* sc = this;
  while (sc) {
    result = sc->BLOBARRAY[index];
    if (result) {
      break;
    }
    sc = sc->mParent;
  }
  if (result == nsnull) {
    NS_WARNING("Did not find style blob");
    // Allocate the missing blob.  This should never happen, but who knows?
    BLOBARRAY[index] = mStyleData->AllocateOneBlob(aSID, nsnull);
    result = BLOBARRAY[index];
  }
  return result;
}

//------------------------------------------------------------------------------
//  GetStyleData
//------------------------------------------------------------------------------
//
const nsStyleStruct* StyleContextImpl::GetStyleData(nsStyleStructID aSID)
{
  NS_PRECONDITION(aSID > 0 && aSID <= eStyleStruct_Max, "Invalid style struct id");

#ifdef LOG_GET_STYLE_DATA_CALLS
  LogGetStyleDataCall(aSID, logCallType_GetStyleData, this, PR_TRUE);
#endif

  const nsStyleStruct* result = FetchInheritedStyleStruct(aSID);

#ifdef LOG_GET_STYLE_DATA_CALLS
  LogGetStyleDataCall(aSID, logCallType_GetStyleData, this, PR_FALSE);
#endif

  return result;
}

//------------------------------------------------------------------------------
//  GetNewMutableStyleStruct [static]
//------------------------------------------------------------------------------
// Notes:
// - We can't get more than 4 mutable style structs of the same type at the same time.
// - Keep in sync with the comments in the declaration of ReadMutableStyleData()
//   in nsIMutableStyleContext.h and nsIStyleContext.h
const short kStructIndexMax = 4;

static STYLEDATA(Font)             gStructFont[kStructIndexMax];
static STYLEDATA(Color)            gStructColor[kStructIndexMax];
static STYLEDATA(List)             gStructList[kStructIndexMax];
static STYLEDATA(Position)         gStructPosition[kStructIndexMax];
static STYLEDATA(Text)             gStructText[kStructIndexMax];
static STYLEDATA(Display)          gStructDisplay[kStructIndexMax];
static STYLEDATA(Table)            gStructTable[kStructIndexMax];
static STYLEDATA(Content)          gStructContent[kStructIndexMax];
static STYLEDATA(UserInterface)    gStructUserInterface[kStructIndexMax];
static STYLEDATA(Print)					   gStructPrint[kStructIndexMax];
static STYLEDATA(Margin)				   gStructMargin[kStructIndexMax];
static STYLEDATA(Padding)				   gStructPadding[kStructIndexMax];
static STYLEDATA(Border)				   gStructBorder[kStructIndexMax];
static STYLEDATA(Outline)				   gStructOutline[kStructIndexMax];
#ifdef INCLUDE_XUL
static STYLEDATA(XUL)     				 gStructXUL[kStructIndexMax];
#endif
//#insert new style structs here#

nsStyleStruct* gStructPointer[eStyleStruct_Max][kStructIndexMax];
PRBool         gStructBusy[eStyleStruct_Max][kStructIndexMax];


static nsStyleStruct* GetNewMutableStyleStruct(nsStyleStructID aSID) {

  nsStyleStruct*  result = nsnull;

  static PRBool initialized = PR_FALSE;
  if (!initialized) {
    initialized = PR_TRUE;
    for (short structType = 0; structType < eStyleStruct_Max; structType++) {
      nsStyleStructID structID = (nsStyleStructID)(structType + 1);
      for (short structIndex = 0; structIndex < kStructIndexMax; structIndex++) {
        nsStyleStruct* structPtr = nsnull;
        switch (structID) {
          case eStyleStruct_Font:            structPtr = &gStructFont[structIndex];            break;
          case eStyleStruct_Color:           structPtr = &gStructColor[structIndex];           break;
          case eStyleStruct_List:            structPtr = &gStructList[structIndex];            break;
          case eStyleStruct_Position:        structPtr = &gStructPosition[structIndex];        break;
          case eStyleStruct_Text:            structPtr = &gStructText[structIndex];            break;
          case eStyleStruct_Display:         structPtr = &gStructDisplay[structIndex];         break;
          case eStyleStruct_Table:           structPtr = &gStructTable[structIndex];           break;
          case eStyleStruct_Content:         structPtr = &gStructContent[structIndex];         break;
          case eStyleStruct_UserInterface:   structPtr = &gStructUserInterface[structIndex];   break;
          case eStyleStruct_Print:           structPtr = &gStructPrint[structIndex];           break;
          case eStyleStruct_Margin:          structPtr = &gStructMargin[structIndex];          break;
          case eStyleStruct_Padding:         structPtr = &gStructPadding[structIndex];         break;
          case eStyleStruct_Border:          structPtr = &gStructBorder[structIndex];          break;
          case eStyleStruct_Outline:         structPtr = &gStructOutline[structIndex];         break;
#ifdef INCLUDE_XUL
          case eStyleStruct_XUL:             structPtr = &gStructXUL[structIndex];         break;
#endif
          //#insert new style structs here#
          default:
            NS_ERROR("Invalid style struct id");
            break;
        }
        gStructPointer[structType][structIndex] = structPtr;
        gStructBusy[structType][structIndex] = PR_FALSE;
      }
    }
  }

  short structType = aSID - 1;
  for (short structIndex = 0; structIndex < kStructIndexMax; structIndex++) {
    if (!gStructBusy[structType][structIndex]) {
      result = gStructPointer[structType][structIndex];
      gStructBusy[structType][structIndex] = PR_TRUE;
      break;
    }
  }
  if (!result) {
    NS_ERROR("Reached maximum number of mutable style structs of a certain type");
    // Return something or we gonna crash: callers don't always check the returned pointer.
    // By returning a structure which is already in use, we'll only have rendering errors.
    // Anyhow, this is just a safety fallback, in case people in the future don't follow
    // the instructions in "nsIMutableStyleContext.h" about the limit on mutable pointers
    // that one can get at any single time.
    result = gStructPointer[structType][0];
  }
  return result;
}

//------------------------------------------------------------------------------
//  ReleaseMutableStyleStruct [static]
//------------------------------------------------------------------------------
// Retrieve the pointer we gave them and mark it free again.
// A linear search is fine: we have only 4 elements to iterate through.
//
static void  ReleaseMutableStyleStruct(nsStyleStructID aSID, nsStyleStruct* aStyleStruct)
{
  short structType = aSID - 1;
  for (short structIndex = 0; structIndex < kStructIndexMax; structIndex++) {
    if (gStructPointer[structType][structIndex] == aStyleStruct) {
      gStructBusy[structType][structIndex] = PR_FALSE;
      return;
    }
  }
  NS_ASSERTION(PR_FALSE, "Can't release mutable style struct");
}


//------------------------------------------------------------------------------
//  ReadMutableStyleData
//------------------------------------------------------------------------------
// Receive a style structure and fill it up with the inherited style.
//
void StyleContextImpl::ReadMutableStyleData(nsStyleStructID aSID, nsStyleStruct** aStyleStructPtr)
{
  NS_PRECONDITION(aSID > 0 && aSID <= eStyleStruct_Max, "Invalid style struct id");

#ifdef LOG_GET_STYLE_DATA_CALLS
  LogGetStyleDataCall(aSID, logCallType_ReadMutableStyleData, this, PR_TRUE);
#endif
#ifdef LOG_STYLE_STRUCTS
  mStyleData->mGotMutable[aSID] = PR_TRUE;
#endif

  nsStyleStruct*   result  = GetNewMutableStyleStruct(aSID);
  const StyleBlob* current = FetchInheritedStyleBlob(aSID);
  current->CopyTo(result);

#if 0  // was there in the previous function that returned direct pointers to the style data
  if (nsnull != result) {
    if (0 == mDataCode) {
//      mDataCode = ++gLastDataCode;  // XXX temp disable, this is still used but not needed to force unique
    }
  }
#endif

#ifdef LOG_GET_STYLE_DATA_CALLS
  LogGetStyleDataCall(aSID, logCallType_ReadMutableStyleData, this, PR_FALSE);
#endif

  *aStyleStructPtr = result;
}


//------------------------------------------------------------------------------
//  WriteStyleData
//------------------------------------------------------------------------------
// Basic function to write a style structure.
// Check whether the style is any different from the inherited style and if so,
// allocate a new blob (if needed) and copy the new style into it.
//
nsresult StyleContextImpl::WriteStyleData(nsStyleStructID aSID, const nsStyleStruct* aStyleStruct)
{
  NS_PRECONDITION(aSID > 0 && aSID <= eStyleStruct_Max, "Invalid style struct id");

  short index = aSID - 1;
  const StyleBlob* current = FetchInheritedStyleBlob(aSID);
  PRInt32 hint = current->CalcDifference(aStyleStruct);

  if (hint != NS_STYLE_HINT_NONE) {
    if (!BLOBARRAY[index]) {
      BLOBARRAY[index] = mStyleData->AllocateOneBlob(aSID, nsnull);
      if (!BLOBARRAY[index]) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    BLOBARRAY[index]->SetFrom(aStyleStruct);
  }
  return NS_OK;
}

//------------------------------------------------------------------------------
//  WriteMutableStyleData
//------------------------------------------------------------------------------
// Write a style structure that was filled-up in ReadMutableStyleData and that
// might have been modified by the app.
//
nsresult StyleContextImpl::WriteMutableStyleData(nsStyleStructID aSID, nsStyleStruct* aStyleStruct)
{
  NS_PRECONDITION(aSID > 0 && aSID <= eStyleStruct_Max, "Invalid style struct id");

#ifdef LOG_GET_STYLE_DATA_CALLS
  LogGetStyleDataCall(aSID, logCallType_WriteMutableStyleData, this, PR_TRUE);
#endif
#ifdef LOG_WRITE_STYLE_DATA_CALLS
  LogWriteMutableStyleDataCall(aSID, aStyleStruct, this);
#endif

  WriteStyleData(aSID, aStyleStruct);
  ReleaseMutableStyleStruct(aSID, aStyleStruct);

#ifdef LOG_GET_STYLE_DATA_CALLS
  LogGetStyleDataCall(aSID, logCallType_WriteMutableStyleData, this, PR_FALSE);
#endif
  return NS_OK;
}

//------------------------------------------------------------------------------
//  CalcBorderPaddingFor
//------------------------------------------------------------------------------
//
void StyleContextImpl::CalcBorderPaddingFor(const nsIFrame* aFrame, nsMargin& aBorderPadding) const
{
  nsMargin border, padding;

  if (!GETDATA(Border)->GetBorder(border)) {
    GETDATA(Border)->CalcBorderFor(aFrame, border);
  }
  if (!GETDATA(Padding)->GetPadding(padding)) {
    GETDATA(Padding)->CalcPaddingFor(aFrame, padding);
  }
  aBorderPadding = border + padding;
}

//------------------------------------------------------------------------------
//  GetStyle
//------------------------------------------------------------------------------
//
NS_IMETHODIMP
StyleContextImpl::GetStyle(nsStyleStructID aSID, nsStyleStruct& aStruct) const
{
#ifdef LOG_GET_STYLE_DATA_CALLS
  LogGetStyleDataCall(aSID, logCallType_GetStyle, (nsIStyleContext*)this, PR_TRUE);
#endif

  if (aSID == eStyleStruct_BorderPaddingShortcut) {
    nsMargin border, padding;
    PRBool hasBorder  = GETDATA(Border)->GetBorder(border);
    PRBool hasPadding = GETDATA(Padding)->GetPadding(padding);
    if (hasBorder) {
      if (hasPadding) {
        border += padding;
      }
      ((nsStyleBorderPadding&)aStruct).SetBorderPadding(border);
    }
    else {
      if (hasPadding) {
        ((nsStyleBorderPadding&)aStruct).SetBorderPadding(padding);
      }
    }
    
  }
  else {
    NS_PRECONDITION(aSID > 0 && aSID <= eStyleStruct_Max, "Invalid style struct id");
    const StyleBlob* current = FetchInheritedStyleBlob(aSID);
    current->CopyTo(&aStruct);
  }

#ifdef LOG_GET_STYLE_DATA_CALLS
  LogGetStyleDataCall(aSID, logCallType_GetStyle, (nsIStyleContext*)this, PR_FALSE);
#endif
  return NS_OK;
}

//------------------------------------------------------------------------------
//  RemapStyle rules and helpers
//------------------------------------------------------------------------------
//
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


//--------------------------
// EnsureBlockDisplay
//   In order to enforce the floated/positioned element CSS2 rules,
//   if the display type is not a block-type then we set it to a
//   valid block display type.
//
static void EnsureBlockDisplay(/*in out*/PRUint8 &display)
{
  // see if the display value is already a block
  switch (display) {
  case NS_STYLE_DISPLAY_NONE :
    // never change display:none *ever*
    break;

  case NS_STYLE_DISPLAY_TABLE :
  case NS_STYLE_DISPLAY_BLOCK :
    // do not muck with these at all - already blocks
    break;

  case NS_STYLE_DISPLAY_LIST_ITEM :
    // do not change list items to blocks - retain the bullet/numbering
    break;

  case NS_STYLE_DISPLAY_TABLE_ROW_GROUP :
  case NS_STYLE_DISPLAY_TABLE_COLUMN :
  case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP :
  case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP :
  case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP :
  case NS_STYLE_DISPLAY_TABLE_ROW :
  case NS_STYLE_DISPLAY_TABLE_CELL :
  case NS_STYLE_DISPLAY_TABLE_CAPTION :
    // special cases: don't do anything since these cannot really be floated anyway
    break;

  case NS_STYLE_DISPLAY_INLINE_TABLE :
    // make inline tables into tables
    display = NS_STYLE_DISPLAY_TABLE;
    break;

  default :
    // make it a block
    display = NS_STYLE_DISPLAY_BLOCK;
  }
}


//------------------------------------------------------------------------------
//  WrapRemapStyle
//------------------------------------------------------------------------------
// The way it works is...
// - When we enter RemapStyle(), we set the blob pointers that are null
//   to point to the static blobs below.  Then the app does all the
//   remapping it wants, modifying as many of the style structs it needs.
// - When we exit RemapStyle(), we check whether each of the style structs are
//   identical to our parent's.  If it is, we set its blob to null (possibly
//   deallocating the memory).  If it is not, we keep it (possibly storing the
//   data into a newly allocated blob).
//
nsresult StyleContextImpl::WrapRemapStyle(nsIPresContext* aPresContext, PRBool aEnterRemapStyle)
{
  static STYLEBLOB(Font)           sFont;
  static STYLEBLOB(Color)          sColor;
  static STYLEBLOB(List)           sList;
  static STYLEBLOB(Position)       sPosition;
  static STYLEBLOB(Text)           sText;
  static STYLEBLOB(Display)        sDisplay;
  static STYLEBLOB(Table)          sTable;
  static STYLEBLOB(Content)        sContent;
  static STYLEBLOB(UserInterface)  sUserInterface;
  static STYLEBLOB(Print)          sPrint;
  static STYLEBLOB(Margin)         sMargin;
  static STYLEBLOB(Padding)        sPadding;
  static STYLEBLOB(Border)         sBorder;
  static STYLEBLOB(Outline)        sOutline;
#ifdef INCLUDE_XUL
  static STYLEBLOB(XUL)            sXUL;
#endif
  //#insert new style structs here#
  static StyleBlob* sBlobPtr[eStyleStruct_Max];

  static PRBool initialized = PR_FALSE;
  if (!initialized) {
    initialized = PR_TRUE;
    for (short i = 0; i < eStyleStruct_Max; i ++) {
      StyleBlob* blobPtr = nsnull;
      switch (i + 1) {
        case eStyleStruct_Font:            blobPtr = &sFont;            break;
        case eStyleStruct_Color:           blobPtr = &sColor;           break;
        case eStyleStruct_List:            blobPtr = &sList;            break;
        case eStyleStruct_Position:        blobPtr = &sPosition;        break;
        case eStyleStruct_Text:            blobPtr = &sText;            break;
        case eStyleStruct_Display:         blobPtr = &sDisplay;         break;
        case eStyleStruct_Table:           blobPtr = &sTable;           break;
        case eStyleStruct_Content:         blobPtr = &sContent;         break;
        case eStyleStruct_UserInterface:   blobPtr = &sUserInterface;   break;
        case eStyleStruct_Print:           blobPtr = &sPrint;           break;
        case eStyleStruct_Margin:          blobPtr = &sMargin;          break;
        case eStyleStruct_Padding:         blobPtr = &sPadding;         break;
        case eStyleStruct_Border:          blobPtr = &sBorder;          break;
        case eStyleStruct_Outline:         blobPtr = &sOutline;         break;
#ifdef INCLUDE_XUL
        case eStyleStruct_XUL:             blobPtr = &sXUL;             break;
#endif
        //#insert new style structs here#
        default:
          NS_ERROR("Invalid style struct id");
          break;
      }
      sBlobPtr[i] = blobPtr;
    }
  }

  if (aEnterRemapStyle) {
    //------------------------
    // Entering RemapStyle()...
    // Set the blob pointers that are null
    // to point to the static blobs.
#ifdef DEBUG
    if (mParent == nsnull) {
      for (short i = 0; i < eStyleStruct_Max; i ++) {
        if (BLOBARRAY[i] == nsnull) {
          NS_ERROR("Parent context is null but style blob is not allocated");
          break;
        }
      }
    }
#endif
    for (short i = 0; i < eStyleStruct_Max; i ++) {
      if (!BLOBARRAY[i]) {
        BLOBARRAY[i] = sBlobPtr[i];
      }
      nsStyleStructID structID = (nsStyleStructID)(i + 1);
      const nsStyleStruct* parentStyle = nsnull;
      if (mParent) {
        parentStyle = mParent->FetchInheritedStyleStruct(structID);
      }
      BLOBARRAY[i]->ResetFrom(parentStyle, aPresContext);
    }
  }
  else {
    //------------------------
    // Exiting RemapStyle()...
    // Check whether each of our style structs are identical to our parent's.
    // If it is, we set its blob to null (possibly deallocating the memory).
    // If it is not, we keep it (possibly storing the data into a newly allocated blob).
    if (mParent != nsnull) {
      PRInt32 hint;
      for (short i = 0; i < eStyleStruct_Max; i ++) {
        nsStyleStructID structID = (nsStyleStructID)(i + 1);
        hint = BLOBARRAY[i]->CalcDifference(mParent->FetchInheritedStyleStruct(structID));
        if (hint == NS_STYLE_HINT_NONE) {
          if (BLOBARRAY[i] == sBlobPtr[i]) {
            BLOBARRAY[i] = nsnull;
          }
          else {
            NS_IF_DELETE(BLOBARRAY[i]);
          }
        }
        else {
          if (BLOBARRAY[i] == sBlobPtr[i]) {
            BLOBARRAY[i] = mStyleData->AllocateOneBlob(structID, nsnull);
            if (!BLOBARRAY[i]) {
              return NS_ERROR_OUT_OF_MEMORY;
            }
            BLOBARRAY[i]->SetFrom(sBlobPtr[i]->GetData());
          }
        }
      }
    }
  }
  return NS_OK;
}

//------------------------------------------------------------------------------
//  RemapStyle
//------------------------------------------------------------------------------
//
NS_IMETHODIMP
StyleContextImpl::RemapStyle(nsIPresContext* aPresContext, PRBool aRecurse)
{
  mDataCode = -1;

  if (NS_FAILED(EnsureStyleData(aPresContext))) {
    return NS_ERROR_UNEXPECTED;
  }

  //------------------------
  // Memory allocation
  WrapRemapStyle(aPresContext, PR_TRUE);  // Use a temporary set of blobs while we remap style


  //------------------------
  // Normal style remapping
  PRUint32 cnt = 0;
  if (mRules) {
    nsresult rv = mRules->Count(&cnt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
  }
  if (0 < cnt) {
    MapStyleData  data(this, aPresContext);
    mRules->EnumerateForwards(MapStyleRuleFont, &data);
    if (GETDATAPTR(Font)->mFlags & NS_STYLE_FONT_USE_FIXED) {
      GETMUTABLEDATAPTR(Font)->mFont = GETDATAPTR(Font)->mFixedFont;
    }
    mRules->EnumerateForwards(MapStyleRule, &data);
  }
  if (-1 == mDataCode) {
    mDataCode = 0;
  }

  //------------------------
  // CSS2 specified fixups:
  //  - these must be done after all declarations are mapped since they can cross style-structs

  // #1 if float is not none, and display is not none, then we must set display to block
  //    XXX - there are problems with following the spec here: what we will do instead of
  //          following the letter of the spec is to make sure that floated elements are
  //          some kind of block, not strictly 'block' - see EnsureBlockDisplay method
  PRUint8 oldDisplay = GETDATAPTR(Display)->mDisplay;
  PRUint8 oldFloats = GETDATAPTR(Display)->mFloats;
  PRUint8 newDisplay = oldDisplay;
  PRUint8 newFloats = oldFloats;
  if (newDisplay != NS_STYLE_DISPLAY_NONE && newFloats != NS_STYLE_FLOAT_NONE ) {
    EnsureBlockDisplay(newDisplay);
  }
  // #2 if position is 'absolute' or 'fixed' then display must be 'block and float must be 'none'
  //    XXX - see note for fixup #1 above...
  if (GETDATAPTR(Position)->IsAbsolutelyPositioned()) {
    if (newDisplay != NS_STYLE_DISPLAY_NONE) {
	    EnsureBlockDisplay(newDisplay);
      newFloats = NS_STYLE_FLOAT_NONE;
    }
  }
  if (newDisplay != oldDisplay || newFloats != oldFloats) {
    nsMutableStyleDisplay display((nsIStyleContext*)this);
    if (newDisplay != oldDisplay) {
      display->mDisplay = newDisplay;
    }
    if (newFloats != oldFloats) {
      display->mFloats = newFloats;
    }
  }

  //------------------------
  // Table style remapping
  PRUint8 displayType = GETDATAPTR(Display)->mDisplay;
  nsCompatibility quirkMode = eCompatibility_Standard;
  aPresContext->GetCompatibilityMode(&quirkMode);
  if (eCompatibility_NavQuirks == quirkMode) {
    if (((displayType == NS_STYLE_DISPLAY_TABLE) || 
         (displayType == NS_STYLE_DISPLAY_TABLE_CAPTION)) &&
        (nsnull == mPseudoTag)) {

      // time to emulate a sub-document
      // This is ugly, but we need to map style once to determine display type
      // then reset and map it again so that all local style is preserved

      // XXX the style we do preserve is visibility, direction, language
      PRUint8 visible = GETDATAPTR(Display)->mVisible;
      PRUint8 direction = GETDATAPTR(Display)->mDirection;
      nsCOMPtr<nsILanguageAtom> language = GETDATAPTR(Display)->mLanguage;

      // cut off all inheritance. this really blows
      StyleContextImpl* holdParent = mParent;
      mParent = nsnull;

      // NOTE: We cut off the inheritance but we don't need to allocate the blobs
      // that were inherited from the parent because we are inside RemapStyle(),
      // meaning that we have all the blobs already.

      // Reset the blobs to default style
      for (short i = 0; i < eStyleStruct_Max; i ++) {
        nsStyleStructID structID = (nsStyleStructID)(i + 1);
        if (BLOBARRAY[i]) {
          if ((structID == eStyleStruct_Font) && (displayType == NS_STYLE_DISPLAY_TABLE)) {
            // Inherit the font struct from the parent (tables only, not table captions)...
            const nsStyleStruct* parentStyle = nsnull;
            if (holdParent) {
              parentStyle = holdParent->FetchInheritedStyleStruct(structID);
            }
            BLOBARRAY[i]->ResetFrom(parentStyle, aPresContext);
          }
          else {
            // ... But reset all the other blobs
            BLOBARRAY[i]->ResetFrom(nsnull, aPresContext);
          }
        }
      }

      // Restore the style we preserved
      GETMUTABLEDATAPTR(Display)->mVisible = visible;
      GETMUTABLEDATAPTR(Display)->mDirection = direction;
      GETMUTABLEDATAPTR(Display)->mLanguage = language;

      // Do the normal style remapping all over again
      PRUint32 numRules = 0;
      if (mRules) {
        nsresult rv = mRules->Count(&numRules);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
      }
      if (0 < numRules) {
        MapStyleData  data(this, aPresContext);
        mRules->EnumerateForwards(MapStyleRuleFont, &data);
        if (GETDATAPTR(Font)->mFlags & NS_STYLE_FONT_USE_FIXED) {
          GETMUTABLEDATAPTR(Font)->mFont = GETDATAPTR(Font)->mFixedFont;
        }
        mRules->EnumerateForwards(MapStyleRule, &data);
      }

      // reset all font data for tables again
      if (GETDATAPTR(Display)->mDisplay == NS_STYLE_DISPLAY_TABLE) {
        // get the font-name to reset: this property we preserve
        nsAutoString strName(GETDATAPTR(Font)->mFont.name);
        nsAutoString strFixedName(GETDATAPTR(Font)->mFixedFont.name);
   
        short index = eStyleStruct_Font - 1;
        BLOBARRAY[index]->ResetFrom(nsnull, aPresContext);
   
        // now reset the font names back to original
        GETMUTABLEDATAPTR(Font)->mFont.name = strName;
        GETMUTABLEDATAPTR(Font)->mFixedFont.name = strFixedName;
      }
      mParent = holdParent;
    }
  } else {
    // In strict mode, we still have to support one "quirky" thing
    // for tables.  HTML's alignment attributes have always worked
    // so they don't inherit into tables, but instead align the
    // tables.  We should keep doing this, because HTML alignment
    // is just weird, and we shouldn't force it to match CSS.
    if (displayType == NS_STYLE_DISPLAY_TABLE) {
      // -moz-center and -moz-right are used for HTML's alignment
      if ((GETDATAPTR(Text)->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_CENTER) ||
          (GETDATAPTR(Text)->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_RIGHT))
      {
        GETMUTABLEDATAPTR(Text)->mTextAlign = NS_STYLE_TEXT_ALIGN_DEFAULT;
      }
    }
  }

  RecalcAutomaticData(aPresContext);

  //------------------------
  // Memory deallocation
  WrapRemapStyle(aPresContext, PR_FALSE); // Keep the blobs that have changed and dump the rest.
                                          // Must be called before we recurse.

  //------------------------
  // Context is mapped, share it if we can
#ifdef SHARE_STYLECONTEXTS
  nsresult result = ShareStyleData();
#endif

  //------------------------
  // Recurse through children
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

//------------------------------------------------------------------------------
//  ForceUnique
//------------------------------------------------------------------------------
//
void StyleContextImpl::ForceUnique(void)
{
  if (mDataCode <= 0) {
    mDataCode = ++gLastDataCode;
  }
}

//------------------------------------------------------------------------------
//  RecalcAutomaticData
//------------------------------------------------------------------------------
//
void StyleContextImpl::RecalcAutomaticData(nsIPresContext* aPresContext)
{
  if (NS_FAILED(EnsureStyleData(aPresContext))) {
    return /*NS_FAILURE*/;
  }
  GETBLOB(Margin)->RecalcData();
  GETBLOB(Padding)->RecalcData();
  GETBLOB(Border)->RecalcData(GETDATA(Color)->mColor);
  GETBLOB(Outline)->RecalcData();
}

//------------------------------------------------------------------------------
//  CalcStyleDifference
//------------------------------------------------------------------------------
//
NS_IMETHODIMP
StyleContextImpl::CalcStyleDifference(nsIStyleContext* aOther, PRInt32& aHint,PRBool aStopAtFirstDifference /*= PR_FALSE*/) const
{
  if (NS_FAILED(HaveStyleData())) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aOther) {
    PRInt32 hint = aHint = NS_STYLE_HINT_NONE;
    const StyleContextImpl* otherContext = (const StyleContextImpl*)aOther;

    for (short i = 0; i < eStyleStruct_Max; i ++) {
      nsStyleStructID structID = (nsStyleStructID)(i + 1);
      const StyleBlob* myStyle = FetchInheritedStyleBlob(structID);
      hint = myStyle->CalcDifference(otherContext->FetchInheritedStyleStruct(structID));
      if (aHint < hint) {
        aHint = hint;
      }
      if ((aStopAtFirstDifference && (aHint > NS_STYLE_HINT_NONE)) || (aHint == NS_STYLE_HINT_MAX)) {
        return NS_OK;
      }
    }
  }
  return NS_OK;
}

//------------------------------------------------------------------------------
//  EnsureStyleData
//------------------------------------------------------------------------------
//
nsresult StyleContextImpl::EnsureStyleData(nsIPresContext* aPresContext)
{
  nsresult rv = NS_OK;
  NS_ASSERTION(aPresContext, "null presContext argument is illegal and immoral");
  if (nsnull == aPresContext) {
    // no pres context provided, and we have no style data, so send back an error
    return NS_ERROR_FAILURE;
  }

  // See if we already have data...
  if (NS_FAILED(HaveStyleData())) {
    // we were provided a pres context so create a new style data
#ifdef SHARE_STYLECONTEXTS
    mStyleData = nsStyleContextData::Create(this, aPresContext);
#else
    mStyleData = &mStyleDataImpl;
    if (NS_FAILED(mStyleData->AllocateStyleBlobs(this, aPresContext))) {
      mStyleData = nsnull;
    }
#endif
    if (nsnull == mStyleData) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return rv;
}

//------------------------------------------------------------------------------
//  HaveStyleData
//------------------------------------------------------------------------------
//
nsresult StyleContextImpl::HaveStyleData(void) const
{
  return (nsnull == mStyleData) ? NS_ERROR_NULL_POINTER : NS_OK;
}


//------------------------------------------------------------------------------
//  ShareStyleData
//------------------------------------------------------------------------------
//
#ifdef SHARE_STYLECONTEXTS
nsresult StyleContextImpl::ShareStyleData(void)
{
  nsresult result = NS_OK;

  // Enable flag: this is TRUE by default, however the env. var. moz_disable_style_sharing
  // can be set to '1' to disable the sharing before the app is launched
  static char *disableSharing = PR_GetEnv("moz_disable_style_sharing");
  static PRBool isSharingEnabled = (disableSharing == nsnull) || 
                                 (*disableSharing != '1');
#ifdef DEBUG
  static PRBool bOnce = PR_FALSE;
  if(!bOnce){
    printf( "Style Data Sharing is %s\n", isSharingEnabled ? "Enabled :)" : "Disabled :(" );
    bOnce = PR_TRUE;
  }
#endif

  PRBool isSharingSupported = CanShareStyleData(this);

  if (isSharingEnabled && isSharingSupported) {
    NS_ASSERTION(mStyleSet, "Expected to have a style set ref...");
    nsIStyleContext *matchingSC = nsnull;

    // save the old crc before replacing it
    PRUint32 oldCRC = mStyleData->GetCRC32();
    
    // set the CRC based on the new data, and retrieve the new value
    mStyleData->SetCRC32();
    PRUint32 newCRC = mStyleData->GetCRC32();

    // if the crc was previously set AND it has changed, notify the styleset
    if((oldCRC != STYLEDATA_NO_CRC)&&
       (oldCRC != newCRC)) {
      result = mStyleSet->UpdateStyleContextKey(oldCRC, newCRC);
#ifdef NOISY_DEBUG
      printf("CRC changed on context: updating from %ld to %ld\n", oldCRC, newCRC);
#endif
      if(NS_FAILED(result)) {
        return result;
      }
    }

    // check if there is a matching context...
    result = mStyleSet->FindMatchingContext(this, &matchingSC);
    if ((NS_SUCCEEDED(result)) && 
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
  }

  return result;
}
#endif // SHARE_STYLECONTEXTS

//------------------------------------------------------------------------------
//  ShareStyleDataFrom
//------------------------------------------------------------------------------
//
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
#endif // SHARE_STYLECONTEXTS

//------------------------------------------------------------------------------
//  StyleDataMatches
//------------------------------------------------------------------------------
//
#ifdef SHARE_STYLECONTEXTS
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
#endif // SHARE_STYLECONTEXTS

//------------------------------------------------------------------------------
//  GetStyleContextKey
//------------------------------------------------------------------------------
//
#ifdef SHARE_STYLECONTEXTS
NS_IMETHODIMP StyleContextImpl::GetStyleContextKey(scKey &aKey) const
{
  aKey = mStyleData->GetCRC32();
  return NS_OK;
}
#endif // SHARE_STYLECONTEXTS

//------------------------------------------------------------------------------
//  UpdateStyleSetCache
//------------------------------------------------------------------------------
//
#ifdef SHARE_STYLECONTEXTS
nsresult StyleContextImpl::UpdateStyleSetCache( void ) const
{
  if (mStyleSet) {
    // add it to the set: the set does NOT addref or release
    return mStyleSet->AddStyleContext((nsIStyleContext *)this);
  } else {
    return NS_ERROR_FAILURE;
  }
}
#endif // SHARE_STYLECONTEXTS

//------------------------------------------------------------------------------
//  Regression tests (List, SizeOf, DumpRegressionData)
//------------------------------------------------------------------------------
//
void StyleContextImpl::List(FILE* out, PRInt32 aIndent)
{
  // Indent
  PRInt32 ix;
  for (ix = aIndent; --ix >= 0; ) fputs("  ", out);
  fprintf(out, "%p(%d) ", (void*)this, mRefCnt);
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

    struct sizeInfo {
      char* name;
      long  size;
    };

    static sizeInfo styleDataInfo[eStyleStruct_Max] = {
       {"StyleFontBlob",          sizeof(STYLEBLOB(Font))}
      ,{"StyleColorBlob",         sizeof(STYLEBLOB(Color))}
      ,{"StyleListBlob",          sizeof(STYLEBLOB(List))}
      ,{"StylePositionBlob",      sizeof(STYLEBLOB(Position))}
      ,{"StyleTextBlob",          sizeof(STYLEBLOB(Text))}
      ,{"StyleDisplayBlob",       sizeof(STYLEBLOB(Display))}
      ,{"StyleTableBlob",         sizeof(STYLEBLOB(Table))}
      ,{"StyleContentBlob",       sizeof(STYLEBLOB(Content))}
      ,{"StyleUserInterfaceBlob", sizeof(STYLEBLOB(UserInterface))}
      ,{"StylePrintBlob",         sizeof(STYLEBLOB(Print))}
      ,{"StyleMarginBlob",        sizeof(STYLEBLOB(Margin))}
      ,{"StylePaddingBlob",       sizeof(STYLEBLOB(Padding))}
      ,{"StyleBorderBlob",        sizeof(STYLEBLOB(Border))}
      ,{"StyleOutlineBlob",       sizeof(STYLEBLOB(Outline))}
#ifdef INCLUDE_XUL
      ,{"StyleXULBlob",           sizeof(STYLEBLOB(XUL))}
#endif
      //#insert new style structs here#
    };

    printf("Detailed StyleContextImpl dump: basic class sizes of members\n" );
    printf("*************************************\n");
    PRUint32 totalSize=0;
    for (short i = 0; i < eStyleStruct_Max; i ++) {
      printf(" - %-24s %ld\n", styleDataInfo[i].name, styleDataInfo[i].size);
      totalSize += styleDataInfo[i].size;
    }
    printf(" - %-24s %ld\n", "Total", totalSize);
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
#ifndef SHARE_STYLECONTEXTS
  aSize -= sizeof(this->mStyleDataImpl);
#endif
  // add in the size of the member mPseudoTag
  if(mPseudoTag){
    mPseudoTag->SizeOf(aSizeOfHandler, &localSize);
    aSize += localSize;
  }
  aSizeOfHandler->AddSize(tag,aSize);

  // count the style data seperately
  if (mStyleData) {
    mStyleData->SizeOf(aSizeOfHandler,localSize);
  }

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

#ifdef DEBUG
static void IndentBy(FILE* out, PRInt32 aIndent) {
  while (--aIndent >= 0) fputs("  ", out);
}
// virtual 
void StyleContextImpl::DumpRegressionData(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent)
{
  nsAutoString str;

  // FONT
  IndentBy(out,aIndent);
  fprintf(out, "<font %s %s %d />\n", 
          NS_ConvertUCS2toUTF8(GETDATA(Font)->mFont.name).get(),
          NS_ConvertUCS2toUTF8(GETDATA(Font)->mFixedFont.name).get(),
          GETDATA(Font)->mFlags);

  // COLOR
  IndentBy(out,aIndent);
  fprintf(out, "<color data=\"%ld %d %d %d %ld %ld %ld %s %d %s %f\"/>\n", 
    (long)GETDATA(Color)->mColor,
    (int)GETDATA(Color)->mBackgroundAttachment,
    (int)GETDATA(Color)->mBackgroundFlags,
    (int)GETDATA(Color)->mBackgroundRepeat,
    (long)GETDATA(Color)->mBackgroundColor,
    (long)GETDATA(Color)->mBackgroundXPosition,
    (long)GETDATA(Color)->mBackgroundYPosition,
    NS_ConvertUCS2toUTF8(GETDATA(Color)->mBackgroundImage).get(),
    (int)GETDATA(Color)->mCursor,
    NS_ConvertUCS2toUTF8(GETDATA(Color)->mCursorImage).get(),
    GETDATA(Color)->mOpacity);

  // SPACING (ie. margin, padding, border, outline)
  IndentBy(out,aIndent);
  fprintf(out, "<spacing data=\"");

  GETDATA(Margin)->mMargin.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Padding)->mPadding.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Border)->mBorder.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Border)->mBorderRadius.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Outline)->mOutlineRadius.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Outline)->mOutlineWidth.ToString(str);
  fprintf(out, "%s", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "%d", (int)GETDATA(Border)->mFloatEdge);
  fprintf(out, "\" />\n");

  // LIST
  IndentBy(out,aIndent);
  fprintf(out, "<list data=\"%d %d %s\" />\n",
    (int)GETDATA(List)->mListStyleType,
    (int)GETDATA(List)->mListStyleType,
    NS_ConvertUCS2toUTF8(GETDATA(List)->mListStyleImage).get());


  // POSITION
  IndentBy(out,aIndent);
  fprintf(out, "<position data=\"%d ", (int)GETDATA(Position)->mPosition);
  GETDATA(Position)->mOffset.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Position)->mWidth.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Position)->mMinWidth.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Position)->mMaxWidth.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Position)->mHeight.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Position)->mMinHeight.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Position)->mMaxHeight.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "%d ", (int)GETDATA(Position)->mBoxSizing);
  GETDATA(Position)->mZIndex.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "\" />\n");

  // TEXT
  IndentBy(out,aIndent);
  fprintf(out, "<text data=\"%d %d %d %d ",
    (int)GETDATA(Text)->mTextAlign,
    (int)GETDATA(Text)->mTextDecoration,
    (int)GETDATA(Text)->mTextTransform,
    (int)GETDATA(Text)->mWhiteSpace);
  GETDATA(Text)->mLetterSpacing.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Text)->mLineHeight.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Text)->mTextIndent.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Text)->mWordSpacing.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Text)->mVerticalAlign.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "\" />\n");
  
  // DISPLAY
  IndentBy(out,aIndent);
  fprintf(out, "<display data=\"%d %d %d %d %d %d %d %d %d %ld %ld %ld %ld\" />\n",
    (int)GETDATA(Display)->mDirection,
    (int)GETDATA(Display)->mDisplay,
    (int)GETDATA(Display)->mFloats,
    (int)GETDATA(Display)->mBreakType,
    (int)GETDATA(Display)->mBreakBefore,
    (int)GETDATA(Display)->mBreakAfter,
    (int)GETDATA(Display)->mVisible,
    (int)GETDATA(Display)->mOverflow,
    (int)GETDATA(Display)->mClipFlags,
    (long)GETDATA(Display)->mClip.x,
    (long)GETDATA(Display)->mClip.y,
    (long)GETDATA(Display)->mClip.width,
    (long)GETDATA(Display)->mClip.height
    );
  
  // TABLE
  IndentBy(out,aIndent);
  fprintf(out, "<table data=\"%d %d %d %d ",
    (int)GETDATA(Table)->mLayoutStrategy,
    (int)GETDATA(Table)->mFrame,
    (int)GETDATA(Table)->mRules,
    (int)GETDATA(Table)->mBorderCollapse);
  GETDATA(Table)->mBorderSpacingX.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Table)->mBorderSpacingY.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  GETDATA(Table)->mCellPadding.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "%d %d %ld %ld ",
    (int)GETDATA(Table)->mCaptionSide,
    (int)GETDATA(Table)->mEmptyCells,
    (long)GETDATA(Table)->mCols,
    (long)GETDATA(Table)->mSpan);
  GETDATA(Table)->mSpanWidth.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "\" />\n");

  // CONTENT
  IndentBy(out,aIndent);
  fprintf(out, "<content data=\"%ld %ld %ld %ld ",
    (long)GETDATA(Content)->ContentCount(),
    (long)GETDATA(Content)->CounterIncrementCount(),
    (long)GETDATA(Content)->CounterResetCount(),
    (long)GETDATA(Content)->QuotesCount());
  // XXX: iterate over the content, counters and quotes...
  GETDATA(Content)->mMarkerOffset.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "\" />\n");

  // UI
  IndentBy(out,aIndent);
  fprintf(out, "<UI data=\"%d %d %d %d %d %d %s\" />\n",
    (int)GETDATA(UserInterface)->mUserInput,
    (int)GETDATA(UserInterface)->mUserModify,
    (int)GETDATA(UserInterface)->mUserSelect,
    (int)GETDATA(UserInterface)->mUserFocus,
    (int)GETDATA(UserInterface)->mKeyEquivalent,
    (int)GETDATA(UserInterface)->mResizer,
    NS_ConvertUCS2toUTF8(GETDATA(UserInterface)->mBehavior).get());

  // PRINT
  IndentBy(out,aIndent);
  fprintf(out, "<print data=\"%d %d %d %s %ld %ld %d ",
    (int)GETDATA(Print)->mPageBreakBefore,
    (int)GETDATA(Print)->mPageBreakAfter,
    (int)GETDATA(Print)->mPageBreakInside,
    NS_ConvertUCS2toUTF8(GETDATA(Print)->mPage).get(),
    (long)GETDATA(Print)->mWidows,
    (long)GETDATA(Print)->mOrphans,
    (int)GETDATA(Print)->mMarks);
  GETDATA(Print)->mSizeWidth.ToString(str);
  fprintf(out, "%s ", NS_ConvertUCS2toUTF8(str).get());
  fprintf(out, "\" />\n");

  // XUL
#ifdef INCLUDE_XUL
  IndentBy(out,aIndent);
  fprintf(out, "<xul data=\"%d",
    (int)GETDATA(XUL)->mBoxOrient);
  fprintf(out, "\" />\n");
#endif
  //#insert new style structs here#
}
#endif

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
  nsresult result = context->QueryInterface(NS_GET_IID(nsIStyleContext), (void **) aInstancePtrResult);
  context->RemapStyle(aPresContext);  // remap after initial ref-count is set

#ifdef SHARE_STYLECONTEXTS
  context->UpdateStyleSetCache();     // add it to the style set cache now that the CRC is set
#endif

  return result;
}


//------------------------------------------------------------------------------
//  CRC Calculations
//------------------------------------------------------------------------------
//
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
  PRUint32 crc = aCrc;
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

#ifdef DEBUG //xxx pierre
#include "nsStyleContextDebug.cpp"
#endif
