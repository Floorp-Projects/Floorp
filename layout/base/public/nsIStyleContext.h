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
#include "nsStyleCoord.h"

class nsIFrame;
class nsIPresContext;
class nsIContent;
class nsISupportsArray;

// SID e31e1bc0-ca9b-11d1-8031-006008159b5a
#define NS_STYLEFONT_SID   \
{0xe31e1bc0, 0xca9b, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

// SID 05953860-ca9c-11d1-8031-006008159b5a
#define NS_STYLECOLOR_SID   \
{0x05953860, 0xca9c, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

// SID 0ba04d20-d89e-11d1-8031-006008159b5a
#define NS_STYLESPACING_SID   \
{0x0ba04d20, 0xd89e, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

// SID 4fb83b60-cf27-11d1-8031-006008159b5a
#define NS_STYLELIST_SID   \
{0x4fb83b60, 0xcf27, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

// SID AD5993F0-DA2B-11d1-80B9-00805F8A274D
#define NS_STYLEPOSITION_SID  \
{0xad5993f0, 0xda2b, 0x11d1, {0x80, 0xb9, 0x00, 0x80, 0x5f, 0x8a, 0x27, 0x4d}}

// SID 3C29D620-DAF5-11D1-932B-00805F8ADD32
#define NS_STYLETEXT_SID  \
{0x3c29d620, 0xdaf5, 0x11d1, {0x93, 0x2b, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

// SID 3C29D621-DAF5-11D1-932B-00805F8ADD32
#define NS_STYLEDISPLAY_SID  \
{0x3c29d621, 0xdaf5, 0x11d1, {0x93, 0x2b, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

// SID 8a9d1f50-e094-11d1-8031-006008159b5a
#define NS_STYLETABLE_SID  \
{0x8a9d1f50, 0xe094, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}


// Indicies into border/padding/margin arrays
#define NS_SIDE_TOP     0
#define NS_SIDE_RIGHT   1
#define NS_SIDE_BOTTOM  2
#define NS_SIDE_LEFT    3

// The lifetime of these objects is managed by the nsIStyleContext.

struct nsStyleStruct {
};

struct nsStyleFont : public nsStyleStruct {
  nsFont  mFont;
  PRUint8 mThreeD;  // XXX fold this into nsFont

protected:
  nsStyleFont(const nsFont& aFont);
  ~nsStyleFont(void);
};

struct nsStyleColor : public nsStyleStruct {
  nscolor mColor;
 
  PRUint8 mBackgroundAttachment; // See nsStyleConsts.h
  PRUint8 mBackgroundFlags;      // See nsStyleConsts.h
  PRUint8 mBackgroundRepeat;     // See nsStyleConsts.h
  PRUint8 mCursor;               // See nsStyleConsts.h NS_STYLE_CURSOR_*

  nscolor mBackgroundColor;
  nscoord mBackgroundXPosition;
  nscoord mBackgroundYPosition;
  nsString mBackgroundImage;     // absolute url string

  nsString mCursorImage;         // url string

protected:
  nsStyleColor(void);
  ~nsStyleColor(void);
};

struct nsStyleSpacing: public nsStyleStruct {
  nsStyleSides  mMargin;          // length, percent, auto, inherit
  nsStyleSides  mPadding;         // length, percent, inherit
  nsStyleSides  mBorder;          // length, percent, See nsStyleConsts.h for enum
  PRUint8       mBorderStyle[4];  // See nsStyleConsts.h
  nscolor       mBorderColor[4];

  void CalcMarginFor(const nsIFrame* aFrame, nsMargin& aMargin) const;
  void CalcPaddingFor(const nsIFrame* aFrame, nsMargin& aPadding) const;
  void CalcBorderFor(const nsIFrame* aFrame, nsMargin& aBorder) const;
  void CalcBorderPaddingFor(const nsIFrame* aFrame, nsMargin& aBorderPadding) const;

protected:
  nsStyleSpacing(void);

  PRBool    mHasCachedMargin;
  PRBool    mHasCachedPadding;
  PRBool    mHasCachedBorder;
  nsMargin  mCachedMargin;
  nsMargin  mCachedPadding;
  nsMargin  mCachedBorder;
  nsMargin  mCachedBorderPadding;
};

struct nsStyleList : public nsStyleStruct {
  PRUint8   mListStyleType;             // See nsStyleConsts.h
  PRUint8   mListStylePosition;
  nsString  mListStyleImage;            // absolute url string

protected:
  nsStyleList(void);
  ~nsStyleList(void);
};

struct nsStylePosition : public nsStyleStruct {
  PRUint8   mPosition;                  // see nsStyleConsts.h
  PRUint8   mOverflow;                  // see nsStyleConsts.h
  PRUint8   mClipFlags;                 // see nsStyleConsts.h

  nsStyleCoord  mLeftOffset;
  nsStyleCoord  mTopOffset;
  nsStyleCoord  mWidth;
  nsStyleCoord  mHeight;

  nsStyleCoord  mZIndex;
  nsMargin  mClip;                      // offsets from respective edge

protected:
  nsStylePosition(void);
};

// XXX missing: length, inherit and percentage support
struct nsStyleText : public nsStyleStruct {
  PRUint8 mTextAlign;                   // see nsStyleConsts.h
  PRUint8 mTextDecoration;              // see nsStyleConsts.h
  PRUint8 mTextTransform;               // see nsStyleConsts.h
  PRUint8 mWhiteSpace;                  // see nsStyleConsts.h

  nsStyleCoord  mLetterSpacing;
  nsStyleCoord  mLineHeight;
  nsStyleCoord  mTextIndent;
  nsStyleCoord  mWordSpacing;
  nsStyleCoord  mVerticalAlign;         // see nsStyleConsts.h for enums

protected:
  nsStyleText(void);
};

struct nsStyleDisplay : public nsStyleStruct {
  PRUint8 mDirection;           // see nsStyleConsts.h NS_STYLE_DIRECTION_*
  PRUint8 mDisplay;             // see nsStyleConsts.h NS_STYLE_DISPLAY_*
  PRUint8 mFloats;              // see nsStyleConsts.h NS_STYLE_FLOAT_*
  PRUint8 mBreakType;           // see nsStyleConsts.h NS_STYLE_CLEAR_*
  PRPackedBool mBreakBefore;
  PRPackedBool mBreakAfter;

protected:
  nsStyleDisplay(void);
};

struct nsStyleTable: public nsStyleStruct {
  PRUint8       mFrame;         // see nsStyleConsts.h NS_STYLE_TABLE_FRAME_*
  PRUint8       mRules;         // see nsStyleConsts.h NS_STYLE_TABLE_RULES_*
  nsStyleCoord  mCellPadding;
  nsStyleCoord  mCellSpacing;

protected:
  nsStyleTable(void);
};

//----------------------------------------------------------------------

#define NS_ISTYLECONTEXT_IID   \
 { 0x26a4d970, 0xa342, 0x11d1, \
   {0x89, 0x74, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

class nsIStyleContext : public nsISupports {
public:
  virtual PRBool    Equals(const nsIStyleContext* aOther) const = 0;
  virtual PRUint32  HashValue(void) const = 0;

  virtual nsIStyleContext*  GetParent(void) const = 0;
  virtual nsISupportsArray* GetStyleRules(void) const = 0;

  // get a style data struct by ID, may return null 
  virtual nsStyleStruct* GetData(const nsIID& aSID) = 0;

  // call if you change style data after creation
  virtual void    RecalcAutomaticData(nsIPresContext* aPresContext) = 0;

  // debugging
  virtual void  List(FILE* out, PRInt32 aIndent) = 0;
};

// this is private to nsStyleSet, don't call it
extern NS_LAYOUT nsresult
  NS_NewStyleContext(nsIStyleContext** aInstancePtrResult,
                     nsISupportsArray* aRules,
                     nsIPresContext* aPresContext,
                     nsIFrame* aParentFrame);

#endif /* nsIStyleContext_h___ */
