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
#ifndef nsICSSDeclaration_h___
#define nsICSSDeclaration_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsColor.h"
#include "stdio.h"
#include "nsString.h"
#include "nsCoord.h"
#include "nsCSSValue.h"


struct nsCSSStruct {
  virtual const nsID& GetID(void) = 0;
  virtual ~nsCSSStruct();
};


// SID for the nsCSSFont struct {f645dbf8-b48a-11d1-9ca5-0060088f9ff7}
#define NS_CSS_FONT_SID   \
{0xf645dbf8, 0xb48a, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

// SID for the nsCSSColor struct {fd825f22-b48a-11d1-9ca5-0060088f9ff7}
#define NS_CSS_COLOR_SID   \
{0xfd825f22, 0xb48a, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

// SID for the nsCSSText struct {fe13ce94-b48a-11d1-9ca5-0060088f9ff7}
#define NS_CSS_TEXT_SID   \
{0xfe13ce94, 0xb48a, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

// SID for the nsCSSDisplay struct {fe13ce95-b48a-11d1-9ca5-0060088f9ff7}
#define NS_CSS_DISPLAY_SID   \
{0xfe13ce95, 0xb48a, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

// SID for the nsCSSMargin struct {fe6019d4-b48a-11d1-9ca5-0060088f9ff7}
#define NS_CSS_MARGIN_SID   \
{0xfe6019d4, 0xb48a, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

// SID for the nsCSSPosition struct {fee33b2a-b48a-11d1-9ca5-0060088f9ff7}
#define NS_CSS_POSITION_SID   \
{0xfee33b2a, 0xb48a, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

// SID for the nsCSSList struct {603f8266-b48b-11d1-9ca5-0060088f9ff7}
#define NS_CSS_LIST_SID   \
{0x603f8266, 0xb48b, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

// SID for the nsCSSTable struct {16aa4b30-5a3b-11d2-803b-006008159b5a}
#define NS_CSS_TABLE_SID  \
{0x16aa4b30, 0x5a3b, 0x11d2, {0x80, 0x3b, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

// SID for the nsCSSBreaks struct {15124c20-5a3b-11d2-803b-006008159b5a}
#define NS_CSS_BREAKS_SID \
{0x15124c20, 0x5a3b, 0x11d2, {0x80, 0x3b, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

// SID for the nsCSSPage struct {15dd8810-5a3b-11d2-803b-006008159b5a}
#define NS_CSS_PAGE_SID  \
{0x15dd8810, 0x5a3b, 0x11d2, {0x80, 0x3b, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

// SID for the nsCSSContent struct {1629ef70-5a3b-11d2-803b-006008159b5a}
#define NS_CSS_CONTENT_SID  \
{0x1629ef70, 0x5a3b, 0x11d2, {0x80, 0x3b, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

// SID for the nsCSSAural struct {166d2bb0-5a3b-11d2-803b-006008159b5a}
#define NS_CSS_AURAL_SID  \
{0x166d2bb0, 0x5a3b, 0x11d2, {0x80, 0x3b, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}


// IID for the nsICSSDeclaration interface {7b36b9ac-b48d-11d1-9ca5-0060088f9ff7}
#define NS_ICSS_DECLARATION_IID   \
{0x7b36b9ac, 0xb48d, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}



struct nsCSSFont : public nsCSSStruct {
  virtual ~nsCSSFont();
  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue mFamily;
  nsCSSValue mStyle;
  nsCSSValue mVariant;
  nsCSSValue mWeight;
  nsCSSValue mSize;
  nsCSSValue mSizeAdjust; // NEW
  nsCSSValue mStretch; // NEW
};

struct nsCSSValueList {
  nsCSSValueList(void);
  ~nsCSSValueList(void);

  nsCSSValue      mValue;
  nsCSSValueList* mNext;
};

struct nsCSSColor : public nsCSSStruct  {
  nsCSSColor(void);
  virtual ~nsCSSColor(void);
  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue      mColor;
  nsCSSValue      mBackColor;
  nsCSSValue      mBackImage;
  nsCSSValue      mBackRepeat;
  nsCSSValue      mBackAttachment;
  nsCSSValue      mBackPositionX;
  nsCSSValue      mBackPositionY;
  nsCSSValue      mBackFilter;
  nsCSSValueList* mCursor;
  nsCSSValue      mOpacity;
};

struct nsCSSShadow {
  nsCSSShadow(void);
  ~nsCSSShadow(void);

  nsCSSValue mColor;
  nsCSSValue mXOffset;
  nsCSSValue mYOffset;
  nsCSSValue mRadius;
  nsCSSShadow*  mNext;
};

struct nsCSSText : public nsCSSStruct  {
  nsCSSText(void);
  virtual ~nsCSSText();

  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue mWordSpacing;
  nsCSSValue mLetterSpacing;
  nsCSSValue mDecoration;
  nsCSSValue mVerticalAlign;
  nsCSSValue mTextTransform;
  nsCSSValue mTextAlign;
  nsCSSValue mTextIndent;
  nsCSSShadow* mTextShadow; // NEW
  nsCSSValue mUnicodeBidi;  // NEW
  nsCSSValue mLineHeight;
  nsCSSValue mWhiteSpace;
};

struct nsCSSRect {
  void List(FILE* out = 0, PRInt32 aPropID = -1, PRInt32 aIndent = 0) const;
  void List(FILE* out, PRInt32 aIndent, PRIntn aTRBL[]) const;

  nsCSSValue mTop;
  nsCSSValue mRight;
  nsCSSValue mBottom;
  nsCSSValue mLeft;
};

struct nsCSSDisplay : public nsCSSStruct  {
  nsCSSDisplay(void);
  virtual ~nsCSSDisplay();

  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue mDirection;
  nsCSSValue mDisplay;
  nsCSSValue mFloat;
  nsCSSValue mClear;
  nsCSSRect* mClip;
  nsCSSValue mOverflow;
  nsCSSValue mVisibility;
  nsCSSValue mFilter;
};

struct nsCSSMargin : public nsCSSStruct  {
  nsCSSMargin(void);
  virtual ~nsCSSMargin(void);

  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSRect*  mMargin;
  nsCSSRect*  mPadding;
  nsCSSRect*  mBorderWidth; // CHANGED
  nsCSSRect*  mBorderColor; // CHANGED
  nsCSSRect*  mBorderStyle; // CHANGED
  nsCSSValue  mBorderRadius;  // NEW (extension)
  nsCSSValue  mOutlineWidth; // NEW
  nsCSSValue  mOutlineColor; // NEW
  nsCSSValue  mOutlineStyle; // NEW 
};

struct nsCSSPosition : public nsCSSStruct  {
  nsCSSPosition(void);
  virtual ~nsCSSPosition();

  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue  mPosition;
  nsCSSValue  mWidth;
  nsCSSValue  mMinWidth; // NEW
  nsCSSValue  mMaxWidth; // NEW
  nsCSSValue  mHeight;
  nsCSSValue  mMinHeight; // NEW
  nsCSSValue  mMaxHeight; // NEW
  nsCSSRect*  mOffset;  // NEW
  nsCSSValue  mZIndex;
};

struct nsCSSList : public nsCSSStruct  {
  virtual ~nsCSSList();

  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue mType;
  nsCSSValue mImage;
  nsCSSValue mPosition;
};

struct nsCSSTable : public nsCSSStruct  { // NEW
  virtual ~nsCSSTable();

  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue mBorderCollapse;
  nsCSSValue mBorderSpacingX;
  nsCSSValue mBorderSpacingY;
  nsCSSValue mCaptionSide;
  nsCSSValue mEmptyCells;
  nsCSSValue mLayout;
};

struct nsCSSBreaks : public nsCSSStruct  { // NEW
  virtual ~nsCSSBreaks();

  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue mOrphans;
  nsCSSValue mWidows;
  nsCSSValue mPage;
  nsCSSValue mPageBreakAfter;
  nsCSSValue mPageBreakBefore;
  nsCSSValue mPageBreakInside;
};

struct nsCSSPage : public nsCSSStruct  { // NEW
  virtual ~nsCSSPage();

  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue mMarks;
  nsCSSValue mSizeWidth;
  nsCSSValue mSizeHeight;
};

struct nsCSSCounterData {
  nsCSSCounterData(void);
  ~nsCSSCounterData(void);

  nsCSSValue        mCounter;
  nsCSSValue        mValue;
  nsCSSCounterData* mNext;
};

struct nsCSSQuotes {
  nsCSSQuotes(void);
  ~nsCSSQuotes(void);

  nsCSSValue    mOpen;
  nsCSSValue    mClose;
  nsCSSQuotes*  mNext;
};

struct nsCSSContent : public nsCSSStruct  { // NEW
  nsCSSContent(void);
  virtual ~nsCSSContent();

  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValueList*   mContent;
  nsCSSCounterData* mCounterIncrement;
  nsCSSCounterData* mCounterReset;
  nsCSSValue        mMarkerOffset;
  nsCSSQuotes*      mQuotes;
};

struct nsCSSAural : public nsCSSStruct  { // NEW
  virtual ~nsCSSAural();

  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue mAzimuth;
  nsCSSValue mElevation;
  nsCSSValue mCueAfter;
  nsCSSValue mCueBefore;
  nsCSSValue mPauseAfter;
  nsCSSValue mPauseBefore;
  nsCSSValue mPitch;
  nsCSSValue mPitchRange;
  nsCSSValue mPlayDuring;
  nsCSSValue mPlayDuringFlags;
  nsCSSValue mRichness;
  nsCSSValue mSpeak;
  nsCSSValue mSpeakHeader;
  nsCSSValue mSpeakNumeral;
  nsCSSValue mSpeakPunctuation;
  nsCSSValue mSpeechRate;
  nsCSSValue mStress;
  nsCSSValue mVoiceFamily;
  nsCSSValue mVolume;
};

class nsICSSDeclaration : public nsISupports {
public:
  NS_IMETHOD GetData(const nsID& aIID, nsCSSStruct** aData) = 0;
  NS_IMETHOD EnsureData(const nsID& aSID, nsCSSStruct** aData) = 0;

  NS_IMETHOD AppendValue(PRInt32 aProperty, const nsCSSValue& aValue) = 0;
  NS_IMETHOD AppendStructValue(PRInt32 aProperty, void* aStruct) = 0;
  NS_IMETHOD SetValueImportant(PRInt32 aProperty) = 0;
  NS_IMETHOD AppendComment(const nsString& aComment) = 0;

// XXX make nscolor a struct to avoid type conflicts
  NS_IMETHOD GetValue(PRInt32 aProperty, nsCSSValue& aValue) = 0;

  NS_IMETHOD GetValue(PRInt32 aProperty, nsString& aValue) = 0;
  NS_IMETHOD GetValue(const nsString& aProperty, nsString& aValue) = 0;

  NS_IMETHOD GetImportantValues(nsICSSDeclaration*& aResult) = 0;
  NS_IMETHOD GetValueIsImportant(PRInt32 aProperty, PRBool& aIsImportant) = 0;
  NS_IMETHOD GetValueIsImportant(const nsString& aProperty, PRBool& aIsImportant) = 0;

  NS_IMETHOD Count(PRUint32* aCount) = 0;
  NS_IMETHOD GetNthProperty(PRUint32 aIndex, nsString& aReturn) = 0;

  NS_IMETHOD GetStyleImpact(PRInt32* aHint) const = 0;

  NS_IMETHOD ToString(nsString& aString) = 0;

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;
};

extern NS_HTML nsresult
  NS_NewCSSDeclaration(nsICSSDeclaration** aInstancePtrResult);

#endif /* nsICSSDeclaration_h___ */

