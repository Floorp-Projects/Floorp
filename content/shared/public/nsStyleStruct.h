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
#ifndef nsStyleStruct_h___
#define nsStyleStruct_h___

#include "nsColor.h"
#include "nsCoord.h"
#include "nsMargin.h"
#include "nsRect.h"
#include "nsFont.h"
#include "nsVoidArray.h"
#include "nsStyleCoord.h"
#include "nsStyleConsts.h"
#include "nsChangeHint.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIURI.h"

class nsIFrame;
class imgIRequest;

enum nsStyleStructID {

/*
 * Define the constants eStyleStruct_Font, etc.
 *
 * The C++ standard, section 7.2, guarantees that enums begin with 0 and
 * increase by 1.
 */

#define STYLE_STRUCT(name, checkdata_cb, ctor_args) eStyleStruct_##name,
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

nsStyleStructID_Length /* one past the end; length of 0-based list */

};

// Bits for each struct.
#define NS_STYLE_INHERIT_BIT(sid_)        (1 << PRInt32(eStyleStruct_##sid_))
#define NS_STYLE_INHERIT_MASK             0x00ffffff

// Additional bits for nsStyleContext's mBits:
// A bit to test whether or not we have any text decorations.
#define NS_STYLE_HAS_TEXT_DECORATIONS     0x01000000

// Additional bits for nsRuleNode's mDependentBits:
#define NS_RULE_NODE_GC_MARK              0x02000000

#define NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(the_sid) \
  static nsStyleStructID GetStyleStructID() {return the_sid;}

#define NS_GET_STYLESTRUCTID(type) (type::GetStyleStructID())

// The actual structs start here
struct nsStyleStruct {
};

// The lifetime of these objects is managed by the presshell's arena.

struct nsStyleFont : public nsStyleStruct {
  nsStyleFont(void);
  nsStyleFont(const nsFont& aFont);
  nsStyleFont(const nsStyleFont& aStyleFont);
  nsStyleFont(nsPresContext *aPresContext);
  ~nsStyleFont(void) {};

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_Font)

  nsChangeHint CalcDifference(const nsStyleFont& aOther) const;
  static nsChangeHint CalcFontDifference(const nsFont& aFont1, const nsFont& aFont2);

  static nscoord ZoomText(nsPresContext* aPresContext, nscoord aSize);
  static nscoord UnZoomText(nsPresContext* aPresContext, nscoord aSize);
  
  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW;
  void Destroy(nsPresContext* aContext);

  PRUint8 mFlags;       // [inherited] See nsStyleConsts.h
  nsFont  mFont;        // [inherited]
  nscoord mSize;        // [inherited] Our "computed size". Can be different from mFont.size
                        // which is our "actual size" and is enforced to be >= the user's
                        // preferred min-size. mFont.size should be used for display purposes
                        // while mSize is the value to return in getComputedStyle() for example.
};

struct nsStyleColor : public nsStyleStruct {
  nsStyleColor(nsPresContext* aPresContext);
  nsStyleColor(const nsStyleColor& aOther);
  ~nsStyleColor(void) {};

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_Color)

  nsChangeHint CalcDifference(const nsStyleColor& aOther) const;
  
  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleColor();
    aContext->FreeToShell(sizeof(nsStyleColor), this);
  };

  // Don't add ANY members to this struct!  We can achieve caching in the rule
  // tree (rather than the style tree) by letting color stay by itself! -dwh
  nscolor mColor;                 // [inherited]
};

struct nsStyleBackground : public nsStyleStruct {
  nsStyleBackground(nsPresContext* aPresContext);
  nsStyleBackground(const nsStyleBackground& aOther);
  ~nsStyleBackground();

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_Background)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleBackground();
    aContext->FreeToShell(sizeof(nsStyleBackground), this);
  };

  nsChangeHint CalcDifference(const nsStyleBackground& aOther) const;

  // On Linux (others?), there is an extra byte being used up by
  // inheritance so we only have 3 bytes to fit these 6 things into.
  // Fortunately, the properties are enums which have few possible
  // values.
  PRUint8 mBackgroundFlags;            // [reset] See nsStyleConsts.h
  PRUint8 mBackgroundAttachment   : 4; // [reset] See nsStyleConsts.h
  PRUint8 mBackgroundClip         : 3; // [reset] See nsStyleConsts.h
  PRUint8 mBackgroundInlinePolicy : 2; // [reset] See nsStyleConsts.h
  PRUint8 mBackgroundOrigin       : 3; // [reset] See nsStyleConsts.h
  PRUint8 mBackgroundRepeat       : 4; // [reset] See nsStyleConsts.h

  // Note: a member of this union is valid IFF the appropriate bit flag
  // is set in mBackgroundFlags.
  union {
    nscoord mCoord;
    float   mFloat;
  } mBackgroundXPosition,         // [reset]
    mBackgroundYPosition;         // [reset]

  nscolor mBackgroundColor;       // [reset]
  nsCOMPtr<imgIRequest> mBackgroundImage; // [reset]

  PRBool IsTransparent() const
  {
    return (mBackgroundFlags &
            (NS_STYLE_BG_COLOR_TRANSPARENT | NS_STYLE_BG_IMAGE_NONE)) ==
            (NS_STYLE_BG_COLOR_TRANSPARENT | NS_STYLE_BG_IMAGE_NONE);
  }
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

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_Margin)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW;
  void Destroy(nsPresContext* aContext);

  void RecalcData();
  nsChangeHint CalcDifference(const nsStyleMargin& aOther) const;

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

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_Padding)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW;
  void Destroy(nsPresContext* aContext);

  void RecalcData();
  nsChangeHint CalcDifference(const nsStylePadding& aOther) const;
  
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

struct nsBorderColors {
  nsBorderColors* mNext;
  nscolor mColor;
  PRBool mTransparent;

  nsBorderColors* CopyColors() {
    nsBorderColors* next = nsnull;
    if (mNext)
      next = mNext->CopyColors();
    return new nsBorderColors(mColor, mTransparent, next);
  }

  nsBorderColors() :mNext(nsnull) { mColor = NS_RGB(0,0,0); };

  nsBorderColors(const nscolor& aColor, PRBool aTransparent, nsBorderColors* aNext=nsnull) {
    mColor = aColor;
    mTransparent = aTransparent;
    mNext = aNext;
  }

  ~nsBorderColors() {
    delete mNext;
  }

  PRBool Equals(nsBorderColors* aOther) {
    nsBorderColors* c1 = this;
    nsBorderColors* c2 = aOther;
    while (c1 && c2) {
      if (c1->mColor != c2->mColor ||
          c1->mTransparent != c2->mTransparent)
        return PR_FALSE;
      c1 = c1->mNext;
      c2 = c2->mNext;
    }
    return !c1 && !c2;
  }
};

struct nsStyleBorder: public nsStyleStruct {
  nsStyleBorder() :mBorderColors(nsnull) {};
  nsStyleBorder(nsPresContext* aContext);
  nsStyleBorder(const nsStyleBorder& aBorder);
  ~nsStyleBorder(void) {
    if (mBorderColors) {
      for (PRInt32 i = 0; i < 4; i++)
        delete mBorderColors[i];
      delete []mBorderColors;
    }
  };

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_Border)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW;
  void Destroy(nsPresContext* aContext);

  PRBool IsBorderSideVisible(PRUint8 aSide) const;
  void RecalcData();
  nsChangeHint CalcDifference(const nsStyleBorder& aOther) const;
 
  nsStyleSides  mBorder;          // [reset] length, enum (see nsStyleConsts.h)
  nsStyleSides  mBorderRadius;    // [reset] length, percent, inherit
  PRUint8       mFloatEdge;       // [reset] see nsStyleConsts.h
  nsBorderColors** mBorderColors; // [reset] multiple levels of color for a border.

  void EnsureBorderColors() {
    if (!mBorderColors) {
      mBorderColors = new nsBorderColors*[4];
      if (mBorderColors)
        for (PRInt32 i = 0; i < 4; i++)
          mBorderColors[i] = nsnull;
    }
  }

  void ClearBorderColors(PRUint8 aSide) {
    if (mBorderColors[aSide]) {
      delete mBorderColors[aSide];
      mBorderColors[aSide] = nsnull;
    }
  }

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

  void GetCompositeColors(PRInt32 aIndex, nsBorderColors** aColors) const
  {
    if (!mBorderColors)
      *aColors = nsnull;
    else
      *aColors = mBorderColors[aIndex];
  }

  void AppendBorderColor(PRInt32 aIndex, nscolor aColor, PRBool aTransparent)
  {
    NS_ASSERTION(aIndex >= 0 && aIndex <= 3, "bad side for composite border color");
    nsBorderColors* colorEntry = new nsBorderColors(aColor, aTransparent);
    if (!mBorderColors[aIndex])
      mBorderColors[aIndex] = colorEntry;
    else {
      nsBorderColors* last = mBorderColors[aIndex];
      while (last->mNext)
        last = last->mNext;
      last->mNext = colorEntry;
    }
    mBorderStyle[aIndex] &= ~BORDER_COLOR_SPECIAL;
    mBorderStyle[aIndex] |= BORDER_COLOR_DEFINED;
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
  void CalcBorderFor(const nsIFrame* aFrame, PRUint8 aSide, nscoord& aWidth) const;
  
protected:
  PRPackedBool  mHasCachedBorder;
  nsMargin      mCachedBorder;

  PRUint8       mBorderStyle[4];  // [reset] See nsStyleConsts.h
  nscolor       mBorderColor[4];  // [reset] the colors to use for a simple border.  not used
                                  // if -moz-border-colors is specified
  
  // XXX remove with deprecated methods
  nscoord       mBorderWidths[3];
};


struct nsStyleBorderPadding: public nsStyleStruct {
  nsStyleBorderPadding(void) { mHasCachedBorderPadding = PR_FALSE; };
  ~nsStyleBorderPadding(void) {};

  // No accessor for this struct, since it's not a real struct.  At
  // least not for now...

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
  nsStyleOutline(nsPresContext* aPresContext);
  nsStyleOutline(const nsStyleOutline& aOutline);
  ~nsStyleOutline(void) {};

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_Outline)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleOutline();
    aContext->FreeToShell(sizeof(nsStyleOutline), this);
  };

  void RecalcData();
  nsChangeHint CalcDifference(const nsStyleOutline& aOther) const;
 
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
  nscoord       mCachedOutlineWidth;

  nscolor       mOutlineColor;    // [reset] 

  // XXX remove with deprecated methods
  nscoord       mBorderWidths[3];

  PRPackedBool  mHasCachedOutline;
  PRUint8       mOutlineStyle;    // [reset] See nsStyleConsts.h
};


struct nsStyleList : public nsStyleStruct {
  nsStyleList(void);
  nsStyleList(const nsStyleList& aStyleList);
  ~nsStyleList(void);

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_List)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleList();
    aContext->FreeToShell(sizeof(nsStyleList), this);
  };

  nsChangeHint CalcDifference(const nsStyleList& aOther) const;
  
  PRUint8   mListStyleType;             // [inherited] See nsStyleConsts.h
  PRUint8   mListStylePosition;         // [inherited] 
  nsCOMPtr<imgIRequest> mListStyleImage; // [inherited]
  nsRect        mImageRegion;           // [inherited] the rect to use within an image  
};

struct nsStylePosition : public nsStyleStruct {
  nsStylePosition(void);
  nsStylePosition(const nsStylePosition& aOther);
  ~nsStylePosition(void);

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_Position)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStylePosition();
    aContext->FreeToShell(sizeof(nsStylePosition), this);
  };

  nsChangeHint CalcDifference(const nsStylePosition& aOther) const;
  
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

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_TextReset)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleTextReset();
    aContext->FreeToShell(sizeof(nsStyleTextReset), this);
  };

  nsChangeHint CalcDifference(const nsStyleTextReset& aOther) const;
  
  PRUint8 mTextDecoration;              // [reset] see nsStyleConsts.h
  PRUint8 mUnicodeBidi;                 // [reset] see nsStyleConsts.h

  nsStyleCoord  mVerticalAlign;         // [reset] see nsStyleConsts.h for enums
};

struct nsStyleText : public nsStyleStruct {
  nsStyleText(void);
  nsStyleText(const nsStyleText& aOther);
  ~nsStyleText(void);

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_Text)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleText();
    aContext->FreeToShell(sizeof(nsStyleText), this);
  };

  nsChangeHint CalcDifference(const nsStyleText& aOther) const;

  PRUint8 mTextAlign;                   // [inherited] see nsStyleConsts.h
  PRUint8 mTextTransform;               // [inherited] see nsStyleConsts.h
  PRUint8 mWhiteSpace;                  // [inherited] see nsStyleConsts.h

  nsStyleCoord  mLetterSpacing;         // [inherited] 
  nsStyleCoord  mLineHeight;            // [inherited] 
  nsStyleCoord  mTextIndent;            // [inherited] 
  nsStyleCoord  mWordSpacing;           // [inherited] 
  
  PRBool WhiteSpaceIsSignificant() const {
    return mWhiteSpace == NS_STYLE_WHITESPACE_PRE ||
           mWhiteSpace == NS_STYLE_WHITESPACE_MOZ_PRE_WRAP;
  }
};

struct nsStyleVisibility : public nsStyleStruct {
  nsStyleVisibility(nsPresContext* aPresContext);
  nsStyleVisibility(const nsStyleVisibility& aVisibility);
  ~nsStyleVisibility() {};

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_Visibility)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleVisibility();
    aContext->FreeToShell(sizeof(nsStyleVisibility), this);
  };

  nsChangeHint CalcDifference(const nsStyleVisibility& aOther) const;
  
  PRUint8 mDirection;                  // [inherited] see nsStyleConsts.h NS_STYLE_DIRECTION_*
  PRUint8   mVisible;                  // [inherited]
  nsCOMPtr<nsIAtom> mLangGroup;        // [inherited]
 
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

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_Display)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleDisplay();
    aContext->FreeToShell(sizeof(nsStyleDisplay), this);
  };

  nsChangeHint CalcDifference(const nsStyleDisplay& aOther) const;
  
  nsCOMPtr<nsIURI> mBinding;    // [reset]
#if 0
  // XXX This is how it is defined in the CSS2 spec, but the errata
  // changed it to be consistent with the positioning draft and how
  // Nav and IE implement it
  nsMargin  mClip;              // [reset] offsets from respective edge
#else
  nsRect    mClip;              // [reset] offsets from upper-left border edge
#endif
  float   mOpacity;             // [reset]
  PRUint8 mDisplay;             // [reset] see nsStyleConsts.h NS_STYLE_DISPLAY_*
  PRUint8 mOriginalDisplay;     // [reset] saved mDisplay for position:absolute/fixed
  PRUint8 mAppearance;          // [reset]
  PRUint8 mPosition;            // [reset] see nsStyleConsts.h
  PRUint8 mFloats;              // [reset] see nsStyleConsts.h NS_STYLE_FLOAT_*
  PRUint8 mBreakType;           // [reset] see nsStyleConsts.h NS_STYLE_CLEAR_*
  PRPackedBool mBreakBefore;    // [reset] 
  PRPackedBool mBreakAfter;     // [reset] 
  PRUint8   mOverflow;          // [reset] see nsStyleConsts.h
  PRUint8   mClipFlags;         // [reset] see nsStyleConsts.h
  
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

  PRBool IsScrollableOverflow() const {
    return mOverflow != NS_STYLE_OVERFLOW_VISIBLE &&
           mOverflow != NS_STYLE_OVERFLOW_CLIP;
  }

  // For table elements that don't support scroll frame creation, we
  // support 'overflow: hidden' to mean 'overflow: -moz-hidden-unscrollable'.
  PRBool IsTableClip() const {
    return mOverflow == NS_STYLE_OVERFLOW_CLIP ||
           mOverflow == NS_STYLE_OVERFLOW_HIDDEN;
  }
};

struct nsStyleTable: public nsStyleStruct {
  nsStyleTable(void);
  nsStyleTable(const nsStyleTable& aOther);
  ~nsStyleTable(void);

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_Table)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleTable();
    aContext->FreeToShell(sizeof(nsStyleTable), this);
  };

  nsChangeHint CalcDifference(const nsStyleTable& aOther) const;
  
  PRUint8       mLayoutStrategy;// [reset] see nsStyleConsts.h NS_STYLE_TABLE_LAYOUT_*
  PRUint8       mFrame;         // [reset] see nsStyleConsts.h NS_STYLE_TABLE_FRAME_*
  PRUint8       mRules;         // [reset] see nsStyleConsts.h NS_STYLE_TABLE_RULES_*
  PRInt32       mCols;          // [reset] an integer if set, or see nsStyleConsts.h NS_STYLE_TABLE_COLS_*
  PRInt32       mSpan;          // [reset] the number of columns spanned by a colgroup or col
};

struct nsStyleTableBorder: public nsStyleStruct {
  nsStyleTableBorder(nsPresContext* aContext);
  nsStyleTableBorder(const nsStyleTableBorder& aOther);
  ~nsStyleTableBorder(void);

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_TableBorder)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleTableBorder();
    aContext->FreeToShell(sizeof(nsStyleTableBorder), this);
  };

  nsChangeHint CalcDifference(const nsStyleTableBorder& aOther) const;
  
  nsStyleCoord  mBorderSpacingX;// [inherited]
  nsStyleCoord  mBorderSpacingY;// [inherited]
  PRUint8       mBorderCollapse;// [inherited]
  PRUint8       mCaptionSide;   // [inherited]
  PRUint8       mEmptyCells;    // [inherited]
};

enum nsStyleContentType {
  eStyleContentType_String        = 1,
  eStyleContentType_Image         = 10,
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
  union {
    PRUnichar *mString;
    imgIRequest *mImage;
  } mContent;

  // empty constructor to keep Sun compiler happy
  nsStyleContentData() {}
  ~nsStyleContentData();
  nsStyleContentData& operator=(const nsStyleContentData& aOther);
  PRBool operator==(const nsStyleContentData& aOther);

  PRBool operator!=(const nsStyleContentData& aOther) {
    return !(*this == aOther);
  }
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

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_Quotes)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleQuotes();
    aContext->FreeToShell(sizeof(nsStyleQuotes), this);
  };

  nsChangeHint CalcDifference(const nsStyleQuotes& aOther) const;

  
  PRUint32  QuotesCount(void) const { return mQuotesCount; } // [inherited]

  const nsString* OpenQuoteAt(PRUint32 aIndex) const
  {
    NS_ASSERTION(aIndex < mQuotesCount, "out of range");
    return mQuotes + (aIndex * 2);
  }
  const nsString* CloseQuoteAt(PRUint32 aIndex) const
  {
    NS_ASSERTION(aIndex < mQuotesCount, "out of range");
    return mQuotes + (aIndex * 2 + 1);
  }
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

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_Content)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleContent();
    aContext->FreeToShell(sizeof(nsStyleContent), this);
  };

  nsChangeHint CalcDifference(const nsStyleContent& aOther) const;

  PRUint32  ContentCount(void) const  { return mContentCount; } // [reset]

  const nsStyleContentData& ContentAt(PRUint32 aIndex) const {
    NS_ASSERTION(aIndex < mContentCount, "out of range");
    return mContents[aIndex];
  }

  nsStyleContentData& ContentAt(PRUint32 aIndex) {
    NS_ASSERTION(aIndex < mContentCount, "out of range");
    return mContents[aIndex];
  }

  nsresult AllocateContents(PRUint32 aCount);

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

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_UIReset)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleUIReset();
    aContext->FreeToShell(sizeof(nsStyleUIReset), this);
  };

  nsChangeHint CalcDifference(const nsStyleUIReset& aOther) const;

  PRUnichar mKeyEquivalent;   // [reset] XXX what type should this be?
  PRUint8   mUserSelect;      // [reset] (selection-style)
  PRUint8   mForceBrokenImageIcon; // [reset]  (0 if not forcing, otherwise forcing)
};

struct nsStyleUserInterface: public nsStyleStruct {
  nsStyleUserInterface(void);
  nsStyleUserInterface(const nsStyleUserInterface& aOther);
  ~nsStyleUserInterface(void);

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_UserInterface)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleUserInterface();
    aContext->FreeToShell(sizeof(nsStyleUserInterface), this);
  };

  nsChangeHint CalcDifference(const nsStyleUserInterface& aOther) const;

  PRUint8   mUserInput;       // [inherited]
  PRUint8   mUserModify;      // [inherited] (modify-content)
  PRUint8   mUserFocus;       // [inherited] (auto-select)
  
  PRUint8   mCursor;          // [inherited] See nsStyleConsts.h
};

struct nsStyleXUL : public nsStyleStruct {
  nsStyleXUL();
  nsStyleXUL(const nsStyleXUL& aSource);
  ~nsStyleXUL();

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_XUL)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleXUL();
    aContext->FreeToShell(sizeof(nsStyleXUL), this);
  };

  nsChangeHint CalcDifference(const nsStyleXUL& aOther) const;
  
  float         mBoxFlex;               // [reset] see nsStyleConsts.h
  PRUint32      mBoxOrdinal;            // [reset] see nsStyleConsts.h
  PRUint8       mBoxAlign;              // [reset] see nsStyleConsts.h
  PRUint8       mBoxDirection;          // [reset] see nsStyleConsts.h
  PRUint8       mBoxOrient;             // [reset] see nsStyleConsts.h
  PRUint8       mBoxPack;               // [reset] see nsStyleConsts.h
};

struct nsStyleColumn : public nsStyleStruct {
  nsStyleColumn();
  nsStyleColumn(const nsStyleColumn& aSource);
  ~nsStyleColumn();

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_Column)
  
  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleColumn();
    aContext->FreeToShell(sizeof(nsStyleColumn), this);
  };

  nsChangeHint CalcDifference(const nsStyleColumn& aOther) const;

  PRUint32     mColumnCount; // [reset] see nsStyleConsts.h
  nsStyleCoord mColumnWidth; // [reset]
  nsStyleCoord mColumnGap;   // [reset]
};

#ifdef MOZ_SVG
enum nsStyleSVGPaintType {
  eStyleSVGPaintType_None = 0,
  eStyleSVGPaintType_Color,
  eStyleSVGPaintType_Server
};

struct nsStyleSVGPaint
{
  nsStyleSVGPaintType mType;
  nscolor mColor;
};

struct nsStyleSVG : public nsStyleStruct {
  nsStyleSVG();
  nsStyleSVG(const nsStyleSVG& aSource);
  ~nsStyleSVG();

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_SVG)

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleSVG();
    aContext->FreeToShell(sizeof(nsStyleSVG), this);
  };

  nsChangeHint CalcDifference(const nsStyleSVG& aOther) const;

  nsStyleSVGPaint  mFill;             // [inherited]
  float            mFillOpacity;      // [inherited]
  PRUint8          mFillRule;         // [inherited] see nsStyleConsts.h
  PRUint8          mPointerEvents;    // [inherited] see nsStyleConsts.h
  PRUint8          mShapeRendering;   // [inherited] see nsStyleConsts.h
  nsStyleSVGPaint  mStroke;           // [inherited]
  nsString         mStrokeDasharray;  // [inherited] XXX we want a parsed value here
  float            mStrokeDashoffset; // [inherited]
  PRUint8          mStrokeLinecap;    // [inherited] see nsStyleConsts.h
  PRUint8          mStrokeLinejoin;   // [inherited] see nsStyleConsts.h
  float            mStrokeMiterlimit; // [inherited]
  float            mStrokeOpacity;    // [inherited]
  float            mStrokeWidth;      // [inherited] in pixels
  PRUint8          mTextAnchor;       // [inherited] see nsStyleConsts.h
  PRUint8          mTextRendering;    // [inherited] see nsStyleConsts.h
};

struct nsStyleSVGReset : public nsStyleStruct {
  nsStyleSVGReset();
  nsStyleSVGReset(const nsStyleSVGReset& aSource);
  ~nsStyleSVGReset();

  NS_DEFINE_STATIC_STYLESTRUCTID_ACCESSOR(eStyleStruct_SVGReset)
  
  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleSVGReset();
    aContext->FreeToShell(sizeof(nsStyleSVGReset), this);
  };

  nsChangeHint CalcDifference(const nsStyleSVGReset& aOther) const;

  PRUint8          mDominantBaseline; // [reset] see nsStyleConsts.h
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
  nscolor mColor;
  /** if this edge is an outside edge, the border infor for the adjacent inside object */
  nsBorderEdges * mInsideNeighbor;
  PRUint8 mStyle;  
  /** which side does this edge represent? */
  PRUint8 mSide;

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
}

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
}

#endif /* nsStyleStruct_h___ */
