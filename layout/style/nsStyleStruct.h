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
#ifndef nsStyleStruct_h___
#define nsStyleStruct_h___

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
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsCOMPtr.h"
#include "nsILanguageAtom.h"

class nsIFrame;

enum nsStyleStructID {
  eStyleStruct_Font           = 1,
  eStyleStruct_Color          = 2,
  eStyleStruct_Background     = 3,
  eStyleStruct_List           = 4,
  eStyleStruct_Position       = 5,
  eStyleStruct_Text           = 6,
  eStyleStruct_TextReset      = 7,
  eStyleStruct_Display        = 8,
  eStyleStruct_Visibility     = 9,
  eStyleStruct_Content        = 10,
  eStyleStruct_Quotes         = 11,
  eStyleStruct_UserInterface  = 12,
  eStyleStruct_UIReset        = 13,
  eStyleStruct_Table          = 14,
  eStyleStruct_TableBorder    = 15,
  eStyleStruct_Margin         = 16,
  eStyleStruct_Padding        = 17,
  eStyleStruct_Border         = 18,
  eStyleStruct_Outline        = 19,
  eStyleStruct_XUL            = 20,
  eStyleStruct_Max            = eStyleStruct_XUL,
  eStyleStruct_BorderPaddingShortcut = 21       // only for use in GetStyle()
};

// Bits for each struct.
#define NS_STYLE_INHERIT_BIT(sid_)        (1 << (PRInt32(sid_) - 1))
#define NS_STYLE_INHERIT_FONT             NS_STYLE_INHERIT_BIT(eStyleStruct_Font)
#define NS_STYLE_INHERIT_COLOR            NS_STYLE_INHERIT_BIT(eStyleStruct_Color)
#define NS_STYLE_INHERIT_BACKGROUND       NS_STYLE_INHERIT_BIT(eStyleStruct_Background)
#define NS_STYLE_INHERIT_LIST             NS_STYLE_INHERIT_BIT(eStyleStruct_List)
#define NS_STYLE_INHERIT_POSITION         NS_STYLE_INHERIT_BIT(eStyleStruct_Position)
#define NS_STYLE_INHERIT_TEXT             NS_STYLE_INHERIT_BIT(eStyleStruct_Text)
#define NS_STYLE_INHERIT_TEXT_RESET       NS_STYLE_INHERIT_BIT(eStyleStruct_TextReset)
#define NS_STYLE_INHERIT_DISPLAY          NS_STYLE_INHERIT_BIT(eStyleStruct_Display)
#define NS_STYLE_INHERIT_VISIBILITY       NS_STYLE_INHERIT_BIT(eStyleStruct_Visibility)
#define NS_STYLE_INHERIT_TABLE            NS_STYLE_INHERIT_BIT(eStyleStruct_Table)
#define NS_STYLE_INHERIT_TABLE_BORDER     NS_STYLE_INHERIT_BIT(eStyleStruct_TableBorder)
#define NS_STYLE_INHERIT_CONTENT          NS_STYLE_INHERIT_BIT(eStyleStruct_Content)
#define NS_STYLE_INHERIT_QUOTES           NS_STYLE_INHERIT_BIT(eStyleStruct_Quotes)
#define NS_STYLE_INHERIT_UI               NS_STYLE_INHERIT_BIT(eStyleStruct_UserInterface)
#define NS_STYLE_INHERIT_UI_RESET         NS_STYLE_INHERIT_BIT(eStyleStruct_UIReset)
#define NS_STYLE_INHERIT_MARGIN           NS_STYLE_INHERIT_BIT(eStyleStruct_Margin)
#define NS_STYLE_INHERIT_PADDING          NS_STYLE_INHERIT_BIT(eStyleStruct_Padding)
#define NS_STYLE_INHERIT_BORDER           NS_STYLE_INHERIT_BIT(eStyleStruct_Border)
#define NS_STYLE_INHERIT_OUTLINE          NS_STYLE_INHERIT_BIT(eStyleStruct_Outline)
#define NS_STYLE_INHERIT_XUL              NS_STYLE_INHERIT_BIT(eStyleStruct_XUL)

#define NS_STYLE_INHERIT_MASK             0x0fffff

// A bit to test whether or not a style context can be shared
// by siblings.
#define NS_STYLE_UNIQUE_CONTEXT           0x100000

// A bit to test whether or not we have any text decorations.
#define NS_STYLE_HAS_TEXT_DECORATIONS     0x200000

// The actual structs start here
struct nsStyleStruct {
};

// The lifetime of these objects is managed by the presshell's arena.

struct nsStyleFont : public nsStyleStruct {
  nsStyleFont(void);
  ~nsStyleFont(void) {};

  PRInt32 CalcDifference(const nsStyleFont& aOther) const;
  static PRInt32 CalcFontDifference(const nsFont& aFont1, const nsFont& aFont2);
  
  void* operator new(size_t sz, nsIPresContext* aContext);
  void Destroy(nsIPresContext* aContext);

  PRUint8 mFlags;       // [inherited] See nsStyleConsts.h
  nsFont  mFont;        // [inherited]
  nscoord mSize;        // [inherited] Our "computed size". Can be different from mFont.size
                        // which is our "actual size" and is enforced to be >= the user's
                        // preferred min-size. mFont.size should be used for display purposes
                        // while mSize is the value to return in getComputedStyle() for example.

  nsStyleFont(const nsFont& aFont);
  nsStyleFont(const nsStyleFont& aStyleFont);
  nsStyleFont(nsIPresContext* aPresContext);
};

struct nsStyleColor : public nsStyleStruct {
  nsStyleColor(nsIPresContext* aPresContext);
  nsStyleColor(const nsStyleColor& aOther);
  ~nsStyleColor(void) {};

  PRInt32 CalcDifference(const nsStyleColor& aOther) const;
  
  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStyleColor();
    aContext->FreeToShell(sizeof(nsStyleColor), this);
  };

  // Don't add ANY members to this struct!  We can achieve caching in the rule
  // tree (rather than the style tree) by letting color stay by itself! -dwh
  nscolor mColor;                 // [inherited]
};

struct nsStyleBackground : public nsStyleStruct {
  nsStyleBackground(nsIPresContext* aPresContext);
  nsStyleBackground(const nsStyleBackground& aOther);
  ~nsStyleBackground() {};

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStyleBackground();
    aContext->FreeToShell(sizeof(nsStyleBackground), this);
  };

  PRInt32 CalcDifference(const nsStyleBackground& aOther) const;
  
  PRUint8 mBackgroundAttachment;  // [reset] See nsStyleConsts.h
  PRUint8 mBackgroundFlags;       // [reset] See nsStyleConsts.h
  PRUint8 mBackgroundRepeat;      // [reset] See nsStyleConsts.h

  nscolor mBackgroundColor;       // [reset]
  nscoord mBackgroundXPosition;   // [reset]
  nscoord mBackgroundYPosition;   // [reset]
  nsString mBackgroundImage;      // [reset] absolute url string

  PRBool BackgroundIsTransparent() const {return (mBackgroundFlags &
    (NS_STYLE_BG_COLOR_TRANSPARENT | NS_STYLE_BG_IMAGE_NONE)) ==
    (NS_STYLE_BG_COLOR_TRANSPARENT | NS_STYLE_BG_IMAGE_NONE);}
};

#define BORDER_COLOR_DEFINED      0x80  
#define BORDER_COLOR_TRANSPARENT  0x40
#define BORDER_COLOR_FOREGROUND   0x20
#define BORDER_COLOR_SPECIAL      0x60 // TRANSPARENT | FOREGROUND 
#define BORDER_STYLE_MASK         0x1F

#define NS_SPACING_MARGIN   0
#define NS_SPACING_PADDING  1
#define NS_SPACING_BORDER   2


struct nsStyleMargin: public nsStyleStruct {
  nsStyleMargin(void);
  nsStyleMargin(const nsStyleMargin& aMargin);
  ~nsStyleMargin(void) {};

  void* operator new(size_t sz, nsIPresContext* aContext);
  void Destroy(nsIPresContext* aContext);

  void RecalcData();
  PRInt32 CalcDifference(const nsStyleMargin& aOther) const;

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
  void CalcMarginFor(const nsIFrame* aFrame, nsMargin& aMargin) const;

protected:
  PRPackedBool  mHasCachedMargin;
  nsMargin      mCachedMargin;
};


struct nsStylePadding: public nsStyleStruct {
  nsStylePadding(void);
  nsStylePadding(const nsStylePadding& aPadding);
  ~nsStylePadding(void) {};

  void* operator new(size_t sz, nsIPresContext* aContext);
  void Destroy(nsIPresContext* aContext);

  void RecalcData();
  PRInt32 CalcDifference(const nsStylePadding& aOther) const;
  
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
  void CalcPaddingFor(const nsIFrame* aFrame, nsMargin& aPadding) const;
  
protected:
  PRPackedBool  mHasCachedPadding;
  nsMargin      mCachedPadding;
};


struct nsStyleBorder: public nsStyleStruct {
  nsStyleBorder() {};
  nsStyleBorder(nsIPresContext* aContext);
  nsStyleBorder(const nsStyleBorder& aBorder);
  ~nsStyleBorder(void) {};

  void* operator new(size_t sz, nsIPresContext* aContext);
  void Destroy(nsIPresContext* aContext);

  PRBool IsBorderSideVisible(PRUint8 aSide) const;
  void RecalcData();
  PRInt32 CalcDifference(const nsStyleBorder& aOther) const;
 
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

  void GetBorderColor(PRUint8 aSide, nscolor& aColor,
                      PRBool& aTransparent, PRBool& aForeground) const
  {
    aTransparent = aForeground = PR_FALSE;
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
    if ((mBorderStyle[aSide] & BORDER_COLOR_SPECIAL) == 0)
      aColor = mBorderColor[aSide]; 
    else if (mBorderStyle[aSide] & BORDER_COLOR_FOREGROUND)
      aForeground = PR_TRUE;
    else
      aTransparent = PR_TRUE;
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
    mBorderStyle[aSide] &= ~BORDER_COLOR_SPECIAL;
    mBorderStyle[aSide] |= (BORDER_COLOR_DEFINED | BORDER_COLOR_TRANSPARENT); 
  }

  void SetBorderToForeground(PRUint8 aSide)
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
    mBorderStyle[aSide] &= ~BORDER_COLOR_SPECIAL;
    mBorderStyle[aSide] |= (BORDER_COLOR_DEFINED | BORDER_COLOR_FOREGROUND); 
  }

  void UnsetBorderColor(PRUint8 aSide)
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
    mBorderStyle[aSide] &= BORDER_STYLE_MASK; 
  }

  // XXX these are deprecated methods
  void CalcBorderFor(const nsIFrame* aFrame, nsMargin& aBorder) const;
 
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
  nsStyleOutline(nsIPresContext* aPresContext);
  nsStyleOutline(const nsStyleOutline& aOutline);
  ~nsStyleOutline(void) {};

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStyleOutline();
    aContext->FreeToShell(sizeof(nsStyleOutline), this);
  };

  void RecalcData();
  PRInt32 CalcDifference(const nsStyleOutline& aOther) const;
 
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

  PRBool  GetOutlineInvert(void) const
  {
    return(mOutlineStyle & BORDER_COLOR_SPECIAL);
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
  nsStyleList(const nsStyleList& aStyleList);
  ~nsStyleList(void);

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStyleList();
    aContext->FreeToShell(sizeof(nsStyleList), this);
  };

  PRInt32 CalcDifference(const nsStyleList& aOther) const;
  
  PRUint8   mListStyleType;             // [inherited] See nsStyleConsts.h
  PRUint8   mListStylePosition;         // [inherited] 
  nsString  mListStyleImage;            // [inherited] absolute url string
};

struct nsStylePosition : public nsStyleStruct {
  nsStylePosition(void);
  nsStylePosition(const nsStylePosition& aOther);
  ~nsStylePosition(void);

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStylePosition();
    aContext->FreeToShell(sizeof(nsStylePosition), this);
  };

  PRInt32 CalcDifference(const nsStylePosition& aOther) const;
  
  nsStyleSides  mOffset;                // [reset]
  nsStyleCoord  mWidth;                 // [reset] coord, percent, auto, inherit
  nsStyleCoord  mMinWidth;              // [reset] coord, percent, inherit
  nsStyleCoord  mMaxWidth;              // [reset] coord, percent, null, inherit
  nsStyleCoord  mHeight;                // [reset] coord, percent, auto, inherit
  nsStyleCoord  mMinHeight;             // [reset] coord, percent, inherit
  nsStyleCoord  mMaxHeight;             // [reset] coord, percent, null, inherit
  PRUint8       mBoxSizing;             // [reset] see nsStyleConsts.h

  nsStyleCoord  mZIndex;                // [reset] 
};

struct nsStyleTextReset : public nsStyleStruct {
  nsStyleTextReset(void);
  nsStyleTextReset(const nsStyleTextReset& aOther);
  ~nsStyleTextReset(void);

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStyleTextReset();
    aContext->FreeToShell(sizeof(nsStyleTextReset), this);
  };

  PRInt32 CalcDifference(const nsStyleTextReset& aOther) const;
  
  PRUint8 mTextDecoration;              // [reset] see nsStyleConsts.h
#ifdef IBMBIDI
  PRUint8 mUnicodeBidi;                 // [reset] see nsStyleConsts.h
#endif // IBMBIDI

  nsStyleCoord  mVerticalAlign;         // [reset] see nsStyleConsts.h for enums
};

struct nsStyleText : public nsStyleStruct {
  nsStyleText(void);
  nsStyleText(const nsStyleText& aOther);
  ~nsStyleText(void);

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStyleText();
    aContext->FreeToShell(sizeof(nsStyleText), this);
  };

  PRInt32 CalcDifference(const nsStyleText& aOther) const;

  PRUint8 mTextAlign;                   // [inherited] see nsStyleConsts.h
  PRUint8 mTextTransform;               // [inherited] see nsStyleConsts.h
  PRUint8 mWhiteSpace;                  // [inherited] see nsStyleConsts.h

  nsStyleCoord  mLetterSpacing;         // [inherited] 
  nsStyleCoord  mLineHeight;            // [inherited] 
  nsStyleCoord  mTextIndent;            // [inherited] 
  nsStyleCoord  mWordSpacing;           // [inherited] 
  
  PRBool WhiteSpaceIsSignificant() const {
    return mWhiteSpace == NS_STYLE_WHITESPACE_PRE;
  }
};

struct nsStyleVisibility : public nsStyleStruct {
  nsStyleVisibility(nsIPresContext* aPresContext);
  nsStyleVisibility(const nsStyleVisibility& aVisibility);
  ~nsStyleVisibility() {};

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStyleVisibility();
    aContext->FreeToShell(sizeof(nsStyleVisibility), this);
  };

  PRInt32 CalcDifference(const nsStyleVisibility& aOther) const;
  
  PRUint8 mDirection;                  // [inherited] see nsStyleConsts.h NS_STYLE_DIRECTION_*
  PRUint8   mVisible;                  // [inherited]
  nsCOMPtr<nsILanguageAtom> mLanguage; // [inherited]
  float mOpacity;                      // [inherited] percentage
 
  PRBool IsVisible() const {
		return (mVisible == NS_STYLE_VISIBILITY_VISIBLE);
	}

	PRBool IsVisibleOrCollapsed() const {
		return ((mVisible == NS_STYLE_VISIBILITY_VISIBLE) ||
						(mVisible == NS_STYLE_VISIBILITY_COLLAPSE));
	}
};

struct nsStyleDisplay : public nsStyleStruct {
  nsStyleDisplay();
  nsStyleDisplay(const nsStyleDisplay& aOther); 
  ~nsStyleDisplay() {};

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStyleDisplay();
    aContext->FreeToShell(sizeof(nsStyleDisplay), this);
  };

  PRInt32 CalcDifference(const nsStyleDisplay& aOther) const;
  
  PRUint8 mDisplay;             // [reset] see nsStyleConsts.h NS_STYLE_DISPLAY_*
  nsString  mBinding;           // [reset] absolute url string
  PRUint8   mPosition;          // [reset] see nsStyleConsts.h
  PRUint8 mFloats;              // [reset] see nsStyleConsts.h NS_STYLE_FLOAT_*
  PRUint8 mBreakType;           // [reset] see nsStyleConsts.h NS_STYLE_CLEAR_*
  PRPackedBool mBreakBefore;    // [reset] 
  PRPackedBool mBreakAfter;     // [reset] 
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
  
  PRBool IsBlockLevel() const {return (NS_STYLE_DISPLAY_BLOCK == mDisplay) ||
                                      (NS_STYLE_DISPLAY_LIST_ITEM == mDisplay) ||
                                      (NS_STYLE_DISPLAY_TABLE == mDisplay);}

  PRBool IsFloating() const {
    return NS_STYLE_FLOAT_NONE != mFloats;
  }

  PRBool IsAbsolutelyPositioned() const {return (NS_STYLE_POSITION_ABSOLUTE == mPosition) ||
                                                (NS_STYLE_POSITION_FIXED == mPosition);}

  PRBool IsPositioned() const {return IsAbsolutelyPositioned() ||
                                      (NS_STYLE_POSITION_RELATIVE == mPosition);}
};

struct nsStyleTable: public nsStyleStruct {
  nsStyleTable(void);
  nsStyleTable(const nsStyleTable& aOther);
  ~nsStyleTable(void);

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStyleTable();
    aContext->FreeToShell(sizeof(nsStyleTable), this);
  };

  PRInt32 CalcDifference(const nsStyleTable& aOther) const;
  
  PRUint8       mLayoutStrategy;// [reset] see nsStyleConsts.h NS_STYLE_TABLE_LAYOUT_*
  PRUint8       mFrame;         // [reset] see nsStyleConsts.h NS_STYLE_TABLE_FRAME_*
  PRUint8       mRules;         // [reset] see nsStyleConsts.h NS_STYLE_TABLE_RULES_*
  PRInt32       mCols;          // [reset] an integer if set, or see nsStyleConsts.h NS_STYLE_TABLE_COLS_*
  PRInt32       mSpan;          // [reset] the number of columns spanned by a colgroup or col
};

struct nsStyleTableBorder: public nsStyleStruct {
  nsStyleTableBorder(nsIPresContext* aContext);
  nsStyleTableBorder(const nsStyleTableBorder& aOther);
  ~nsStyleTableBorder(void);

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStyleTableBorder();
    aContext->FreeToShell(sizeof(nsStyleTableBorder), this);
  };

  PRInt32 CalcDifference(const nsStyleTableBorder& aOther) const;
  
  PRUint8       mBorderCollapse;// [inherited]
  nsStyleCoord  mBorderSpacingX;// [inherited]
  nsStyleCoord  mBorderSpacingY;// [inherited]
  PRUint8       mCaptionSide;   // [inherited]
  PRUint8       mEmptyCells;    // [inherited]
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

struct nsStyleQuotes : public nsStyleStruct {
  nsStyleQuotes();
  nsStyleQuotes(const nsStyleQuotes& aQuotes);
  ~nsStyleQuotes();

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStyleQuotes();
    aContext->FreeToShell(sizeof(nsStyleQuotes), this);
  };

  PRInt32 CalcDifference(const nsStyleQuotes& aOther) const;

  
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
  PRUint32            mQuotesCount;
  nsString*           mQuotes;
};

struct nsStyleContent: public nsStyleStruct {
  nsStyleContent(void);
  nsStyleContent(const nsStyleContent& aContent);
  ~nsStyleContent(void);

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStyleContent();
    aContext->FreeToShell(sizeof(nsStyleContent), this);
  };

  PRInt32 CalcDifference(const nsStyleContent& aOther) const;

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

protected:
  PRUint32            mContentCount;
  nsStyleContentData* mContents;

  PRUint32            mIncrementCount;
  nsStyleCounterData* mIncrements;

  PRUint32            mResetCount;
  nsStyleCounterData* mResets;
};

struct nsStyleUIReset: public nsStyleStruct {
  nsStyleUIReset(void);
  nsStyleUIReset(const nsStyleUIReset& aOther);
  ~nsStyleUIReset(void);

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStyleUIReset();
    aContext->FreeToShell(sizeof(nsStyleUIReset), this);
  };

  PRInt32 CalcDifference(const nsStyleUIReset& aOther) const;

  PRUint8   mUserSelect;      // [reset] (selection-style)
  PRUnichar mKeyEquivalent;   // [reset] XXX what type should this be?
  PRUint8   mResizer;         // [reset]
};

struct nsStyleUserInterface: public nsStyleStruct {
  nsStyleUserInterface(void);
  nsStyleUserInterface(const nsStyleUserInterface& aOther);
  ~nsStyleUserInterface(void);

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStyleUserInterface();
    aContext->FreeToShell(sizeof(nsStyleUserInterface), this);
  };

  PRInt32 CalcDifference(const nsStyleUserInterface& aOther) const;

  PRUint8   mUserInput;       // [inherited]
  PRUint8   mUserModify;      // [inherited] (modify-content)
  PRUint8   mUserFocus;       // [inherited] (auto-select)
  
  PRUint8 mCursor;                // [inherited] See nsStyleConsts.h NS_STYLE_CURSOR_*
  nsString mCursorImage;          // [inherited] url string
};

#ifdef INCLUDE_XUL
struct nsStyleXUL : public nsStyleStruct {
  nsStyleXUL();
  nsStyleXUL(const nsStyleXUL& aSource);
  ~nsStyleXUL();

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~nsStyleXUL();
    aContext->FreeToShell(sizeof(nsStyleXUL), this);
  };

  PRInt32 CalcDifference(const nsStyleXUL& aOther) const;
  
  PRUint8       mBoxAlign;              // [reset] see nsStyleConsts.h
  PRUint8       mBoxDirection;          // [reset] see nsStyleConsts.h
  float         mBoxFlex;               // [reset] see nsStyleConsts.h
  PRUint8       mBoxOrient;             // [reset] see nsStyleConsts.h
  PRUint8       mBoxPack;               // [reset] see nsStyleConsts.h
  PRUint32      mBoxOrdinal;            // [reset] see nsStyleConsts.h
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


#endif /* nsStyleStruct_h___ */

