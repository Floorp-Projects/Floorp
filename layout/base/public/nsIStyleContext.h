/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsIStyleContext_h___
#define nsIStyleContext_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsMargin.h"
#include "nsFont.h"
#include "nsVoidArray.h"
#include "nsStyleCoord.h"
#include "nsStyleStruct.h"
#include "nsStyleConsts.h"

class nsIFrame;
class nsIPresContext;
class nsISupportsArray;


// The lifetime of these objects is managed by the nsIStyleContext.

struct nsStyleFont : public nsStyleStruct {
  nsFont  mFont;        // [inherited]
  nsFont  mFixedFont;   // [inherited]
  PRUint8 mFlags;       // [inherited] See nsStyleConsts.h

protected:
  nsStyleFont(const nsFont& aVariableFont, const nsFont& aFixedFont);
  nsStyleFont(nsIPresContext& aPresContext);
  ~nsStyleFont();
};

struct nsStyleColor : public nsStyleStruct {
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

protected:
  nsStyleColor(void);
  ~nsStyleColor();
};

struct nsStyleSpacing: public nsStyleStruct {
  nsStyleSides  mMargin;          // [reset] length, percent, auto, inherit
  nsStyleSides  mPadding;         // [reset] length, percent, inherit
  nsStyleSides  mBorder;          // [reset] length, percent, See nsStyleConsts.h for enum

  nsStyleCoord  mBorderRadius;    // [reset] length, percent, inherit

  nsStyleCoord  mOutlineWidth;    // [reset] length, enum (see nsStyleConsts.h)

  PRBool GetMargin(nsMargin& aMargin) const;
  PRBool GetPadding(nsMargin& aPadding) const;
  PRBool GetBorder(nsMargin& aBorder) const;
  PRBool GetBorderPadding(nsMargin& aBorderPadding) const;

  PRUint8 GetBorderStyle(PRUint8 aSide) const; 
  void    SetBorderStyle(PRUint8 aSide, PRUint8 aStyle); 
  PRBool  GetBorderColor(PRUint8 aSide, nscolor& aColor) const;   // PR_FALSE means TRANSPARENT
  void    SetBorderColor(PRUint8 aSide, nscolor aColor); 
  void    SetBorderTransparent(PRUint8 aSide); 
  void    UnsetBorderColor(PRUint8 aSide);

  PRBool  GetOutlineWidth(nscoord& aWidth) const; // PR_TRUE if pre-computed
  PRUint8 GetOutlineStyle(void) const;
  void    SetOutlineStyle(PRUint8 aStyle);
  PRBool  GetOutlineColor(nscolor& aColor) const; // PR_FALSE means INVERT
  void    SetOutlineColor(nscolor aColor); 
  void    SetOutlineInvert(void); 

// XXX these are deprecated methods
  void CalcMarginFor(const nsIFrame* aFrame, nsMargin& aMargin) const;
  void CalcPaddingFor(const nsIFrame* aFrame, nsMargin& aPadding) const;
  void CalcBorderFor(const nsIFrame* aFrame, nsMargin& aBorder) const;
  void CalcBorderPaddingFor(const nsIFrame* aFrame, nsMargin& aBorderPadding) const;

protected:
  nsStyleSpacing(void);

  PRBool    mHasCachedMargin;
  PRBool    mHasCachedPadding;
  PRBool    mHasCachedBorder;
  PRBool    mHasCachedOutline;
  nsMargin  mCachedMargin;
  nsMargin  mCachedPadding;
  nsMargin  mCachedBorder;
  nsMargin  mCachedBorderPadding;

  PRUint8   mBorderStyle[4];  // [reset] See nsStyleConsts.h
  nscolor   mBorderColor[4];  // [reset] 
  nscoord   mCachedOutlineWidth;
  PRUint8   mOutlineStyle;    // [reset] See nsStyleConsts.h
  nscolor   mOutlineColor;    // [reset] 
};

struct nsStyleList : public nsStyleStruct {
  PRUint8   mListStyleType;             // [inherited] See nsStyleConsts.h
  PRUint8   mListStylePosition;         // [inherited] 
  nsString  mListStyleImage;            // [inherited] absolute url string

protected:
  nsStyleList(void);
  ~nsStyleList(void);
};

struct nsStylePosition : public nsStyleStruct {
  PRUint8   mPosition;                  // [reset] see nsStyleConsts.h

  nsStyleSides  mOffset;                // [reset]
  nsStyleCoord  mWidth;                 // [reset] coord, percent, auto, inherit
  nsStyleCoord  mMinWidth;              // [reset] coord, percent, inherit
  nsStyleCoord  mMaxWidth;              // [reset] coord, percent, null, inherit
  nsStyleCoord  mHeight;                // [reset] coord, percent, auto, inherit
  nsStyleCoord  mMinHeight;             // [reset] coord, percent, inherit
  nsStyleCoord  mMaxHeight;             // [reset] coord, percent, null, inherit

  nsStyleCoord  mZIndex;                // [reset] 

  PRBool IsAbsolutelyPositioned() const {return (NS_STYLE_POSITION_ABSOLUTE == mPosition) ||
                                                (NS_STYLE_POSITION_FIXED == mPosition);}

protected:
  nsStylePosition(void);
};

struct nsStyleText : public nsStyleStruct {
  PRUint8 mTextAlign;                   // [inherited] see nsStyleConsts.h
  PRUint8 mTextDecoration;              // [reset] see nsStyleConsts.h
  PRUint8 mTextTransform;               // [inherited] see nsStyleConsts.h
  PRUint8 mWhiteSpace;                  // [inherited] see nsStyleConsts.h

  nsStyleCoord  mLetterSpacing;         // [inherited] 
  nsStyleCoord  mLineHeight;            // [inherited] 
  nsStyleCoord  mTextIndent;            // [inherited] 
  nsStyleCoord  mWordSpacing;           // [inherited] 
  nsStyleCoord  mVerticalAlign;         // [reset] see nsStyleConsts.h for enums

protected:
  nsStyleText(void);
};

struct nsStyleDisplay : public nsStyleStruct {
  PRUint8 mDirection;           // [inherited] see nsStyleConsts.h NS_STYLE_DIRECTION_*
  PRUint8 mDisplay;             // [reset] see nsStyleConsts.h NS_STYLE_DISPLAY_*
  PRUint8 mFloats;              // [reset] see nsStyleConsts.h NS_STYLE_FLOAT_*
  PRUint8 mBreakType;           // [reset] see nsStyleConsts.h NS_STYLE_CLEAR_*
  PRPackedBool mBreakBefore;    // [reset] 
  PRPackedBool mBreakAfter;     // [reset] 
  PRUint8   mVisible;           // [inherited]
  PRUint8   mOverflow;          // [reset] see nsStyleConsts.h

  PRUint8   mClipFlags;         // [reset] see nsStyleConsts.h
  nsMargin  mClip;              // [reset] offsets from respective edge

  PRBool IsBlockLevel() const {return (NS_STYLE_DISPLAY_BLOCK == mDisplay) ||
                                      (NS_STYLE_DISPLAY_LIST_ITEM == mDisplay) ||
                                      (NS_STYLE_DISPLAY_TABLE == mDisplay);}

  PRBool IsFloating() const {
    return NS_STYLE_FLOAT_NONE != mFloats;
  }

protected:
  nsStyleDisplay(void);
};

struct nsStyleTable: public nsStyleStruct {
  PRUint8       mLayoutStrategy;// [reset] see nsStyleConsts.h NS_STYLE_TABLE_LAYOUT_*
  PRUint8       mFrame;         // [reset] see nsStyleConsts.h NS_STYLE_TABLE_FRAME_*
  PRUint8       mRules;         // [reset] see nsStyleConsts.h NS_STYLE_TABLE_RULES_*
  PRUint8       mBorderCollapse;// [inherited]    XXX: inherit not implemented
  nsStyleCoord  mBorderSpacingX;// [inherited]    XXX: inherit not implemented
  nsStyleCoord  mBorderSpacingY;// [inherited]    XXX: inherit not implemented
  nsStyleCoord  mCellPadding;   // [reset] 
  PRUint8       mCaptionSide;   // [inherited]    XXX: inherit not implemented
  PRUint8       mEmptyCells;    // [inherited]    XXX: inherit not implemented
  PRInt32       mCols;          // [reset] an integer if set, or see nsStyleConsts.h NS_STYLE_TABLE_COLS_*
  PRInt32       mSpan;          // [reset] the number of columns spanned by a colgroup or col
  nsStyleCoord  mSpanWidth;     // [reset] the amount of width this col gets from a spanning cell, if any

protected:
  nsStyleTable(void);
};

#if 0 // not ready for prime time
struct nsStyleContent: public nsStyleStruct {
  PRUint32  ContentCount(void) const;
  PRBool    GetContentAt(PRUint32 aIndex, nsStyleXXX& aContent);

protected:
  nsStyleContent(void);
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

/** an encapsulation of a border defined by its edges */
struct nsBorderEdges
{
  nsVoidArray mEdges[4];
  nsMargin    mMaxBorderWidth;
  PRBool      mOutsideEdge;

  nsBorderEdges();
};

inline nsBorderEdges::nsBorderEdges()
{
  mMaxBorderWidth.SizeTo(0,0,0,0);
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
                                const nsISupportsArray* aRules,
                                nsIStyleContext*& aResult) = 0;

  // get a style data struct by ID, may return null 
  virtual const nsStyleStruct* GetStyleData(nsStyleStructID aSID) = 0;

  // get a style data struct by ID, may return null 
  virtual nsStyleStruct* GetMutableStyleData(nsStyleStructID aSID) = 0;

  // call this to prevent context from getting shared
  virtual void  ForceUnique(void) = 0;

  NS_IMETHOD RemapStyle(nsIPresContext* aPresContext) = 0;

  // call if you change style data after creation
  virtual void    RecalcAutomaticData(nsIPresContext* aPresContext) = 0;

  // compute the effective difference between two contexts
  NS_IMETHOD  CalcStyleDifference(nsIStyleContext* aOther, PRInt32& aHint) const = 0;

  // debugging
  virtual void  List(FILE* out, PRInt32 aIndent) = 0;
};

// this is private to nsStyleSet, don't call it
extern NS_LAYOUT nsresult
  NS_NewStyleContext(nsIStyleContext** aInstancePtrResult,
                     nsIStyleContext* aParentContext,
                     nsIAtom* aPseudoType,
                     nsISupportsArray* aRules,
                     nsIPresContext* aPresContext);

#endif /* nsIStyleContext_h___ */
