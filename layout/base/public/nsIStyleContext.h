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
 */
#ifndef nsIStyleContext_h___
#define nsIStyleContext_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsMargin.h"
#include "nsRect.h"
#include "nsFont.h"
#include "nsVoidArray.h"
#include "nsStyleCoord.h"
#include "nsStyleStruct.h"
#include "nsStyleConsts.h"
#include "nsIStyleSet.h"
#include "nsCOMPtr.h"
#include "nsILanguageAtom.h"
#include "nsIFrame.h"

class nsISizeOfHandler;

class nsIFrame;
class nsIPresContext;
class nsISupportsArray;
class nsIStyleContext;


inline void CalcSidesFor(const nsIFrame* aFrame, const nsStyleSides& aSides, 
                         PRUint8 aSpacing,
                         const nscoord* aEnumTable, PRInt32 aNumEnums,
                         nsMargin& aResult);


#define SHARE_STYLECONTEXTS

// The lifetime of these objects is managed by the nsIStyleContext.

struct nsStyleFont : public nsStyleStruct {
  nsStyleFont(void)
    : mFont(nsnull, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
            NS_FONT_WEIGHT_NORMAL, NS_FONT_DECORATION_NONE, 0),
      mFixedFont(nsnull, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
                 NS_FONT_WEIGHT_NORMAL, NS_FONT_DECORATION_NONE, 0)
    {}
  ~nsStyleFont(void) {};

  nsFont  mFont;        // [inherited]
  nsFont  mFixedFont;   // [inherited]
  PRUint8 mFlags;       // [inherited] See nsStyleConsts.h

protected:
  nsStyleFont(const nsFont& aVariableFont, const nsFont& aFixedFont);
  nsStyleFont(nsIPresContext* aPresContext);
};

struct nsStyleColor : public nsStyleStruct {
  nsStyleColor(void) {}
  ~nsStyleColor(void) {}

  nscolor mColor;                 // [inherited]
 
  PRUint8 mBackgroundAttachment;  // [reset] See nsStyleConsts.h
  PRUint8 mBackgroundFlags;       // [reset] See nsStyleConsts.h
  PRUint8 mBackgroundRepeat;      // [reset] See nsStyleConsts.h

  nscolor mBackgroundColor;       // [reset]
  nscoord mBackgroundXPosition;   // [reset]
  nscoord mBackgroundYPosition;   // [reset]
  nsString mBackgroundImage;      // [reset] absolute url string

  PRUint8 mCursor;                // [reset] See nsStyleConsts.h NS_STYLE_CURSOR_*
  nsString mCursorImage;          // [reset] url string
  float mOpacity;                 // [inherited] percentage

  PRBool BackgroundIsTransparent() const {return (mBackgroundFlags &
    (NS_STYLE_BG_COLOR_TRANSPARENT | NS_STYLE_BG_IMAGE_NONE)) ==
    (NS_STYLE_BG_COLOR_TRANSPARENT | NS_STYLE_BG_IMAGE_NONE);}
};


#define BORDER_COLOR_DEFINED  0x80  
#define BORDER_COLOR_SPECIAL  0x40  
#define BORDER_STYLE_MASK     0x3F

#define NS_SPACING_MARGIN   0
#define NS_SPACING_PADDING  1
#define NS_SPACING_BORDER   2


struct nsStyleMargin: public nsStyleStruct {
  nsStyleMargin(void) {};
  ~nsStyleMargin(void) {};

  nsStyleSides  mMargin;          // [reset] length, percent, auto, inherit

  PRBool GetMargin(nsMargin& aMargin) const
  {
    if (mHasCachedMargin) {
      aMargin = mCachedMargin;
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  // XXX this is a deprecated method
  void CalcMarginFor(const nsIFrame* aFrame, nsMargin& aMargin) const
  {
    if (mHasCachedMargin) {
      aMargin = mCachedMargin;
    } else {
      CalcSidesFor(aFrame, mMargin, NS_SPACING_MARGIN, nsnull, 0, aMargin);
    }
  }

protected:
  PRPackedBool  mHasCachedMargin;
  nsMargin      mCachedMargin;
};


struct nsStylePadding: public nsStyleStruct {
  nsStylePadding(void) {};
  ~nsStylePadding(void) {};

  nsStyleSides  mPadding;         // [reset] length, percent, inherit

  PRBool GetPadding(nsMargin& aPadding) const
  {
    if (mHasCachedPadding) {
      aPadding = mCachedPadding;
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  // XXX this is a deprecated method
  void CalcPaddingFor(const nsIFrame* aFrame, nsMargin& aPadding) const
  {
    if (mHasCachedPadding) {
      aPadding = mCachedPadding;
    } else {
      CalcSidesFor(aFrame, mPadding, NS_SPACING_PADDING, nsnull, 0, aPadding);
    }
  }

protected:
  PRPackedBool  mHasCachedPadding;
  nsMargin      mCachedPadding;
};


struct nsStyleBorder: public nsStyleStruct {
  nsStyleBorder(void) {};
  ~nsStyleBorder(void) {};

  nsStyleSides  mBorder;          // [reset] length, enum (see nsStyleConsts.h)
  nsStyleSides  mBorderRadius;    // [reset] length, percent, inherit
  PRUint8       mFloatEdge;       // [reset] see nsStyleConsts.h

  PRBool GetBorder(nsMargin& aBorder) const
  {
    if (mHasCachedBorder) {
      aBorder = mCachedBorder;
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  PRUint8 GetBorderStyle(PRUint8 aSide) const
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
    return (mBorderStyle[aSide] & BORDER_STYLE_MASK); 
  }

  void SetBorderStyle(PRUint8 aSide, PRUint8 aStyle)
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
    mBorderStyle[aSide] &= ~BORDER_STYLE_MASK; 
    mBorderStyle[aSide] |= (aStyle & BORDER_STYLE_MASK);

  }

  // PR_FALSE means TRANSPARENT 
  PRBool GetBorderColor(PRUint8 aSide, nscolor& aColor) const
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
    if ((mBorderStyle[aSide] & BORDER_COLOR_SPECIAL) == 0) {
      aColor = mBorderColor[aSide]; 
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  void SetBorderColor(PRUint8 aSide, nscolor aColor) 
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
    mBorderColor[aSide] = aColor; 
    mBorderStyle[aSide] &= ~BORDER_COLOR_SPECIAL;
    mBorderStyle[aSide] |= BORDER_COLOR_DEFINED; 
  }

  void SetBorderTransparent(PRUint8 aSide)
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
    mBorderStyle[aSide] |= (BORDER_COLOR_DEFINED | BORDER_COLOR_SPECIAL); 
  }

  void UnsetBorderColor(PRUint8 aSide)
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
    mBorderStyle[aSide] &= BORDER_STYLE_MASK; 
  }

  // XXX these are deprecated methods
  void CalcBorderFor(const nsIFrame* aFrame, nsMargin& aBorder) const
  {
    if (mHasCachedBorder) {
      aBorder = mCachedBorder;
    } else {
      CalcSidesFor(aFrame, mBorder, NS_SPACING_BORDER, mBorderWidths, 3, aBorder);
    }
  }

protected:
  PRPackedBool  mHasCachedBorder;
  nsMargin      mCachedBorder;

  PRUint8       mBorderStyle[4];  // [reset] See nsStyleConsts.h
  nscolor       mBorderColor[4];  // [reset] 

  // XXX remove with deprecated methods
  nscoord       mBorderWidths[3];
};


struct nsStyleBorderPadding: public nsStyleStruct {
  nsStyleBorderPadding(void) { mHasCachedBorderPadding = PR_FALSE; };
  ~nsStyleBorderPadding(void) {};

  PRBool GetBorderPadding(nsMargin& aBorderPadding) const {
    if (mHasCachedBorderPadding) {
      aBorderPadding = mCachedBorderPadding;
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  void SetBorderPadding(nsMargin aBorderPadding) {
    mCachedBorderPadding = aBorderPadding;
    mHasCachedBorderPadding = PR_TRUE;
  }

protected:
  nsMargin      mCachedBorderPadding;
  PRPackedBool  mHasCachedBorderPadding;
};


struct nsStyleOutline: public nsStyleStruct {
  nsStyleOutline(void) {};
  ~nsStyleOutline(void) {};

  nsStyleSides  mOutlineRadius;    // [reset] length, percent, inherit
  																// (top=topLeft, right=topRight, bottom=bottomRight, left=bottomLeft)

  nsStyleCoord  mOutlineWidth;    // [reset] length, enum (see nsStyleConsts.h)

  PRBool GetOutlineWidth(nscoord& aWidth) const
  {
    if (mHasCachedOutline) {
      aWidth = mCachedOutlineWidth;
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  PRUint8 GetOutlineStyle(void) const
  {
    return (mOutlineStyle & BORDER_STYLE_MASK);
  }

  void SetOutlineStyle(PRUint8 aStyle)
  {
    mOutlineStyle &= ~BORDER_STYLE_MASK;
    mOutlineStyle |= (aStyle & BORDER_STYLE_MASK);
  }

  // PR_FALSE means INVERT 
  PRBool GetOutlineColor(nscolor& aColor) const
  {
    if ((mOutlineStyle & BORDER_COLOR_SPECIAL) == 0) {
      aColor = mOutlineColor;
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  void SetOutlineColor(nscolor aColor)
  {
    mOutlineColor = aColor;
    mOutlineStyle &= ~BORDER_COLOR_SPECIAL;
    mOutlineStyle |= BORDER_COLOR_DEFINED;
  }

  void SetOutlineInvert(void)
  {
    mOutlineStyle |= (BORDER_COLOR_DEFINED | BORDER_COLOR_SPECIAL);
  }

protected:
  PRPackedBool  mHasCachedOutline;
  nscoord       mCachedOutlineWidth;

  PRUint8       mOutlineStyle;    // [reset] See nsStyleConsts.h
  nscolor       mOutlineColor;    // [reset] 

  // XXX remove with deprecated methods
  nscoord       mBorderWidths[3];
};


struct nsStyleList : public nsStyleStruct {
  nsStyleList(void);
  ~nsStyleList(void);

  PRUint8   mListStyleType;             // [inherited] See nsStyleConsts.h
  PRUint8   mListStylePosition;         // [inherited] 
  nsString  mListStyleImage;            // [inherited] absolute url string
};

struct nsStylePosition : public nsStyleStruct {
  nsStylePosition(void);
  ~nsStylePosition(void);

  PRUint8   mPosition;                  // [reset] see nsStyleConsts.h

  nsStyleSides  mOffset;                // [reset]
  nsStyleCoord  mWidth;                 // [reset] coord, percent, auto, inherit
  nsStyleCoord  mMinWidth;              // [reset] coord, percent, inherit
  nsStyleCoord  mMaxWidth;              // [reset] coord, percent, null, inherit
  nsStyleCoord  mHeight;                // [reset] coord, percent, auto, inherit
  nsStyleCoord  mMinHeight;             // [reset] coord, percent, inherit
  nsStyleCoord  mMaxHeight;             // [reset] coord, percent, null, inherit
  PRUint8       mBoxSizing;             // [reset] see nsStyleConsts.h

  nsStyleCoord  mZIndex;                // [reset] 

  PRBool IsAbsolutelyPositioned() const {return (NS_STYLE_POSITION_ABSOLUTE == mPosition) ||
                                                (NS_STYLE_POSITION_FIXED == mPosition);}

  PRBool IsPositioned() const {return IsAbsolutelyPositioned() ||
                                      (NS_STYLE_POSITION_RELATIVE == mPosition);}
};

struct nsStyleText : public nsStyleStruct {
  nsStyleText(void);
  ~nsStyleText(void);

  PRUint8 mTextAlign;                   // [inherited] see nsStyleConsts.h
  PRUint8 mTextDecoration;              // [reset] see nsStyleConsts.h
  PRUint8 mTextTransform;               // [inherited] see nsStyleConsts.h
  PRUint8 mWhiteSpace;                  // [inherited] see nsStyleConsts.h

  nsStyleCoord  mLetterSpacing;         // [inherited] 
  nsStyleCoord  mLineHeight;            // [inherited] 
  nsStyleCoord  mTextIndent;            // [inherited] 
  nsStyleCoord  mWordSpacing;           // [inherited] 
  nsStyleCoord  mVerticalAlign;         // [reset] see nsStyleConsts.h for enums

  PRBool WhiteSpaceIsSignificant() const {
    return mWhiteSpace == NS_STYLE_WHITESPACE_PRE;
  }
};

struct nsStyleDisplay : public nsStyleStruct {
  nsStyleDisplay(void) {};
  ~nsStyleDisplay(void) {};

  PRUint8 mDirection;           // [inherited] see nsStyleConsts.h NS_STYLE_DIRECTION_*
  PRUint8 mDisplay;             // [reset] see nsStyleConsts.h NS_STYLE_DISPLAY_*
  PRUint8 mFloats;              // [reset] see nsStyleConsts.h NS_STYLE_FLOAT_*
  PRUint8 mBreakType;           // [reset] see nsStyleConsts.h NS_STYLE_CLEAR_*
  PRPackedBool mBreakBefore;    // [reset] 
  PRPackedBool mBreakAfter;     // [reset] 
  PRUint8   mVisible;           // [inherited]
  PRUint8   mOverflow;          // [reset] see nsStyleConsts.h

  PRUint8   mClipFlags;         // [reset] see nsStyleConsts.h
#if 0
  // XXX This is how it is defined in the CSS2 spec, but the errata
  // changed it to be consistent with the positioning draft and how
  // Nav and IE implement it
  nsMargin  mClip;              // [reset] offsets from respective edge
#else
  nsRect    mClip;              // [reset] offsets from upper-left border edge
#endif
  nsCOMPtr<nsILanguageAtom> mLanguage; // [inherited]

  PRBool IsBlockLevel() const {return (NS_STYLE_DISPLAY_BLOCK == mDisplay) ||
                                      (NS_STYLE_DISPLAY_LIST_ITEM == mDisplay) ||
                                      (NS_STYLE_DISPLAY_TABLE == mDisplay);}

  PRBool IsFloating() const {
    return NS_STYLE_FLOAT_NONE != mFloats;
  }

	PRBool IsVisible() const {
		return (mVisible == NS_STYLE_VISIBILITY_VISIBLE);
	}

	PRBool IsVisibleOrCollapsed() const {
		return ((mVisible == NS_STYLE_VISIBILITY_VISIBLE) ||
						(mVisible == NS_STYLE_VISIBILITY_COLLAPSE));
	}
};

struct nsStyleTable: public nsStyleStruct {
  nsStyleTable(void);
  ~nsStyleTable(void);

  PRUint8       mLayoutStrategy;// [reset] see nsStyleConsts.h NS_STYLE_TABLE_LAYOUT_*
  PRUint8       mFrame;         // [reset] see nsStyleConsts.h NS_STYLE_TABLE_FRAME_*
  PRUint8       mRules;         // [reset] see nsStyleConsts.h NS_STYLE_TABLE_RULES_*
  PRUint8       mBorderCollapse;// [inherited]
  nsStyleCoord  mBorderSpacingX;// [inherited]
  nsStyleCoord  mBorderSpacingY;// [inherited]
  nsStyleCoord  mCellPadding;   // [reset] 
  PRUint8       mCaptionSide;   // [inherited]
  PRUint8       mEmptyCells;    // [inherited]
  PRInt32       mCols;          // [reset] an integer if set, or see nsStyleConsts.h NS_STYLE_TABLE_COLS_*
  PRInt32       mSpan;          // [reset] the number of columns spanned by a colgroup or col
  nsStyleCoord  mSpanWidth;     // [reset] the amount of width this col gets from a spanning cell, if any
};

enum nsStyleContentType {
  eStyleContentType_String        = 1,
  eStyleContentType_URL           = 10,
  eStyleContentType_Attr          = 20,
  eStyleContentType_Counter       = 30,
  eStyleContentType_Counters      = 31,
  eStyleContentType_OpenQuote     = 40,
  eStyleContentType_CloseQuote    = 41,
  eStyleContentType_NoOpenQuote   = 42,
  eStyleContentType_NoCloseQuote  = 43
};

struct nsStyleContentData {
  nsStyleContentType  mType;
  nsString            mContent;
};

struct nsStyleCounterData {
  nsString  mCounter;
  PRInt32   mValue;
};


#define DELETE_ARRAY_IF(array)  if (array) { delete[] array; array = nsnull; }

struct nsStyleContent: public nsStyleStruct {
  nsStyleContent(void);
  ~nsStyleContent(void);

  PRUint32  ContentCount(void) const  { return mContentCount; } // [reset]
  nsresult  GetContentAt(PRUint32 aIndex, nsStyleContentType& aType, nsString& aContent) const {
    if (aIndex < mContentCount) {
      aType = mContents[aIndex].mType;
      aContent = mContents[aIndex].mContent;
      return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsresult  AllocateContents(PRUint32 aCount) {
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

  nsresult  SetContentAt(PRUint32 aIndex, nsStyleContentType aType, const nsString& aContent) {
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

  PRUint32  CounterIncrementCount(void) const { return mIncrementCount; }  // [reset]
  nsresult  GetCounterIncrementAt(PRUint32 aIndex, nsString& aCounter, PRInt32& aIncrement) const {
    if (aIndex < mIncrementCount) {
      aCounter = mIncrements[aIndex].mCounter;
      aIncrement = mIncrements[aIndex].mValue;
      return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsresult  AllocateCounterIncrements(PRUint32 aCount) {
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

  nsresult  SetCounterIncrementAt(PRUint32 aIndex, const nsString& aCounter, PRInt32 aIncrement) {
    if (aIndex < mIncrementCount) {
      mIncrements[aIndex].mCounter = aCounter;
      mIncrements[aIndex].mValue = aIncrement;
      return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
  }

  PRUint32  CounterResetCount(void) const { return mResetCount; }  // [reset]
  nsresult  GetCounterResetAt(PRUint32 aIndex, nsString& aCounter, PRInt32& aValue) const {
    if (aIndex < mResetCount) {
      aCounter = mResets[aIndex].mCounter;
      aValue = mResets[aIndex].mValue;
      return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsresult  AllocateCounterResets(PRUint32 aCount) {
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

  nsresult  SetCounterResetAt(PRUint32 aIndex, const nsString& aCounter, PRInt32 aValue) {
    if (aIndex < mResetCount) {
      mResets[aIndex].mCounter = aCounter;
      mResets[aIndex].mValue = aValue;
      return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsStyleCoord  mMarkerOffset;  // [reset]

  PRUint32  QuotesCount(void) const { return mQuotesCount; } // [inherited]
  nsresult  GetQuotesAt(PRUint32 aIndex, nsString& aOpen, nsString& aClose) const {
    if (aIndex < mQuotesCount) {
      aIndex *= 2;
      aOpen = mQuotes[aIndex];
      aClose = mQuotes[++aIndex];
      return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsresult  AllocateQuotes(PRUint32 aCount) {
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

  nsresult  SetQuotesAt(PRUint32 aIndex, const nsString& aOpen, const nsString& aClose) {
    if (aIndex < mQuotesCount) {
      aIndex *= 2;
      mQuotes[aIndex] = aOpen;
      mQuotes[++aIndex] = aClose;
      return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
  }

protected:
  PRUint32            mContentCount;
  nsStyleContentData* mContents;

  PRUint32            mIncrementCount;
  nsStyleCounterData* mIncrements;

  PRUint32            mResetCount;
  nsStyleCounterData* mResets;

  PRUint32            mQuotesCount;
  nsString*           mQuotes;
};

struct nsStyleUserInterface: public nsStyleStruct {
  nsStyleUserInterface(void);
  ~nsStyleUserInterface(void);

  PRUint8   mUserInput;       // [inherited]
  PRUint8   mUserModify;      // [inherited] (modify-content)
  PRUint8   mUserSelect;      // [reset] (selection-style)
  PRUint8   mUserFocus;       // [inherited] (auto-select)
  PRUnichar mKeyEquivalent;   // [reset] XXX what type should this be?
  PRUint8   mResizer;         // [reset]
  nsString  mBehavior;        // [reset] absolute url string

};

struct nsStylePrint: public nsStyleStruct {
  nsStylePrint(void);
  ~nsStylePrint(void);

  PRUint8       mPageBreakBefore;	// [reset] see nsStyleConsts.h NS_STYLE_PAGE_BREAK_*
  PRUint8       mPageBreakAfter;	// [reset] see nsStyleConsts.h NS_STYLE_PAGE_BREAK_*
  PRUint8       mPageBreakInside;	// [reset] see nsStyleConsts.h NS_STYLE_PAGE_BREAK_*
	nsString			mPage;
  PRUint32      mWidows;					// [reset] = 2, number of isolated lines at the top of a page
  PRUint32      mOrphans;					// [reset] = 2, number of isolated lines at the bottom of a page
  PRUint8       mMarks;						// [reset] see nsStyleConsts.h NS_STYLE_PAGE_MARKS_*
  nsStyleCoord  mSizeWidth;				// [reset] length, enum: see nsStyleConsts.h NS_STYLE_PAGE_SIZE_*
  nsStyleCoord  mSizeHeight;			// [reset] length, enum: see nsStyleConsts.h NS_STYLE_PAGE_SIZE_*
};

#ifdef INCLUDE_XUL
struct nsStyleXUL : public nsStyleStruct {
  nsStyleXUL();
  ~nsStyleXUL();

  // There will be seven more properties coming,
  // which is why we warrant our own struct.
  // box-align, box-direction, box-flex, box-flex-group, box-pack,
  // stack-stretch, stack-policy
  PRUint8       mBoxOrient;             // [reset] see nsStyleConsts.h
};
#endif

#define BORDER_PRECEDENT_EQUAL  0
#define BORDER_PRECEDENT_LOWER  1
#define BORDER_PRECEDENT_HIGHER 2

struct nsBorderEdges;

/** an encapsulation of border edge info */
struct nsBorderEdge
{
  /** the thickness of the edge */
  nscoord mWidth;
  /** the length of the edge */
  nscoord mLength;
  PRUint8 mStyle;  
  nscolor mColor;
  /** which side does this edge represent? */
  PRUint8 mSide;
  /** if this edge is an outside edge, the border infor for the adjacent inside object */
  nsBorderEdges * mInsideNeighbor;

  nsBorderEdge();
};

inline nsBorderEdge::nsBorderEdge()
{
  mWidth=0;
  mLength=0;
  mStyle=NS_STYLE_BORDER_STYLE_NONE;
  mColor=0;
  mSide=NS_SIDE_LEFT;
  mInsideNeighbor = nsnull;
};

/** an encapsulation of a border defined by its edges 
  * owner of this struct is responsible for freeing any data stored in mEdges
  */
struct nsBorderEdges
{
  nsVoidArray  mEdges[4];
  nsMargin     mMaxBorderWidth;
  PRPackedBool mOutsideEdge;

  nsBorderEdges();
};

inline nsBorderEdges::nsBorderEdges()
{
  mMaxBorderWidth.SizeTo(0,0,0,0);
  mOutsideEdge = PR_TRUE;
};


//----------------------------------------------------------------------

#define NS_ISTYLECONTEXT_IID   \
 { 0x26a4d970, 0xa342, 0x11d1, \
   {0x89, 0x74, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

class nsIStyleContext : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISTYLECONTEXT_IID; return iid; }

  virtual PRBool    Equals(const nsIStyleContext* aOther) const = 0;
  virtual PRUint32  HashValue(void) const = 0;

  virtual nsIStyleContext*  GetParent(void) const = 0;
  virtual nsISupportsArray* GetStyleRules(void) const = 0;
  virtual PRInt32 GetStyleRuleCount(void) const = 0;
  NS_IMETHOD GetPseudoType(nsIAtom*& aPseudoTag) const = 0;

  NS_IMETHOD FindChildWithRules(const nsIAtom* aPseudoTag, 
                                nsISupportsArray* aRules,
                                nsIStyleContext*& aResult) = 0;

  // Fill a style struct with data
  NS_IMETHOD GetStyle(nsStyleStructID aSID, nsStyleStruct& aStruct) const = 0;

  // compute the effective difference between two contexts
  NS_IMETHOD  CalcStyleDifference(nsIStyleContext* aOther, PRInt32& aHint, PRBool aStopAtFirst = PR_FALSE) const = 0;

  // debugging
  virtual void  List(FILE* out, PRInt32 aIndent) = 0;

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize) = 0;

#ifdef DEBUG
  virtual void DumpRegressionData(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) = 0;
#endif

#ifdef SHARE_STYLECONTEXTS
  // sets aMatches to PR_TRUE if the style data of aStyleContextToMatch matches the 
  // style data of this, PR_FALSE otherwise
  NS_IMETHOD StyleDataMatches(nsIStyleContext* aStyleContextToMatch, PRBool *aMatches) = 0;
  NS_IMETHOD GetStyleContextKey(scKey &aKey) const = 0;
#endif

  // -------------------------------------------------------------
  // DEPRECATED METHODS - these are all going away, stop using them
  // get a style data struct by ID, may return null 
  // Replace calls to this with calls to GetStyle();
  virtual const nsStyleStruct* GetStyleData(nsStyleStructID aSID) = 0;

  // get a style data struct by ID, may return null 
  virtual nsStyleStruct* GetMutableStyleData(nsStyleStructID aSID) = 0;

  // call this to prevent context from getting shared
  virtual void  ForceUnique(void) = 0;

  NS_IMETHOD RemapStyle(nsIPresContext* aPresContext, PRBool aRecurse = PR_TRUE) = 0;

  // call if you change style data after creation
  virtual void    RecalcAutomaticData(nsIPresContext* aPresContext) = 0;

  // utility function: more convenient than 2 calls to GetStyleData to get border and padding
  virtual void    CalcBorderPaddingFor(const nsIFrame* aFrame, nsMargin& aBorderPadding) const = 0;
};


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


// this is private to nsStyleSet, don't call it
extern NS_LAYOUT nsresult
  NS_NewStyleContext(nsIStyleContext** aInstancePtrResult,
                     nsIStyleContext* aParentContext,
                     nsIAtom* aPseudoType,
                     nsISupportsArray* aRules,
                     nsIPresContext* aPresContext);


#endif /* nsIStyleContext_h___ */
