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
#include "nsICSSDeclaration.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsCSSProps.h"
#include "nsCSSPropIDs.h"
#include "nsUnitConversion.h"
#include "nsVoidArray.h"

#include "nsStyleConsts.h"

//#define DEBUG_REFS


static NS_DEFINE_IID(kCSSFontSID, NS_CSS_FONT_SID);
static NS_DEFINE_IID(kCSSColorSID, NS_CSS_COLOR_SID);
static NS_DEFINE_IID(kCSSDisplaySID, NS_CSS_DISPLAY_SID);
static NS_DEFINE_IID(kCSSTextSID, NS_CSS_TEXT_SID);
static NS_DEFINE_IID(kCSSMarginSID, NS_CSS_MARGIN_SID);
static NS_DEFINE_IID(kCSSPositionSID, NS_CSS_POSITION_SID);
static NS_DEFINE_IID(kCSSListSID, NS_CSS_LIST_SID);
static NS_DEFINE_IID(kCSSTableSID, NS_CSS_TABLE_SID);
static NS_DEFINE_IID(kCSSBreaksSID, NS_CSS_BREAKS_SID);
static NS_DEFINE_IID(kCSSPageSID, NS_CSS_PAGE_SID);
static NS_DEFINE_IID(kCSSContentSID, NS_CSS_CONTENT_SID);
static NS_DEFINE_IID(kCSSAuralSID, NS_CSS_AURAL_SID);
static NS_DEFINE_IID(kICSSDeclarationIID, NS_ICSS_DECLARATION_IID);


#define CSS_IF_DELETE(ptr)  if (nsnull != ptr)  { delete ptr; ptr = nsnull; }

nsCSSStruct::~nsCSSStruct()
{
}

// --- nsCSSFont -----------------

nsCSSFont::~nsCSSFont()
{
}

const nsID& nsCSSFont::GetID(void)
{
  return kCSSFontSID;
}

void nsCSSFont::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mFamily.AppendToString(buffer, PROP_FONT_FAMILY);
  mStyle.AppendToString(buffer, PROP_FONT_STYLE);
  mVariant.AppendToString(buffer, PROP_FONT_VARIANT);
  mWeight.AppendToString(buffer, PROP_FONT_WEIGHT);
  mSize.AppendToString(buffer, PROP_FONT_SIZE);
  mSizeAdjust.AppendToString(buffer, PROP_FONT_SIZE_ADJUST);
  mStretch.AppendToString(buffer, PROP_FONT_STRETCH);
  fputs(buffer, out);
}

// --- support -----------------

nsCSSValueList::nsCSSValueList(void)
  : mValue(),
    mNext(nsnull)
{
}

nsCSSValueList::~nsCSSValueList(void)
{
  CSS_IF_DELETE(mNext);
}

// --- nsCSSColor -----------------

nsCSSColor::nsCSSColor(void)
  : mCursor(nsnull)
{
}

nsCSSColor::~nsCSSColor(void)
{
  CSS_IF_DELETE(mCursor);
}

const nsID& nsCSSColor::GetID(void)
{
  return kCSSColorSID;
}

void nsCSSColor::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mColor.AppendToString(buffer, PROP_COLOR);
  mBackColor.AppendToString(buffer, PROP_BACKGROUND_COLOR);
  mBackImage.AppendToString(buffer, PROP_BACKGROUND_IMAGE);
  mBackRepeat.AppendToString(buffer, PROP_BACKGROUND_REPEAT);
  mBackAttachment.AppendToString(buffer, PROP_BACKGROUND_ATTACHMENT);
  mBackPositionX.AppendToString(buffer, PROP_BACKGROUND_X_POSITION);
  mBackPositionY.AppendToString(buffer, PROP_BACKGROUND_Y_POSITION);
  mBackFilter.AppendToString(buffer, PROP_BACKGROUND_FILTER);
  nsCSSValueList*  cursor = mCursor;
  while (nsnull != cursor) {
    cursor->mValue.AppendToString(buffer, PROP_CURSOR);
    cursor = cursor->mNext;
  }
  mOpacity.AppendToString(buffer, PROP_OPACITY);
  fputs(buffer, out);
}

// --- nsCSSText support -----------------

nsCSSShadow::nsCSSShadow(void)
  : mNext(nsnull)
{
}

nsCSSShadow::~nsCSSShadow(void)
{
  CSS_IF_DELETE(mNext);
}

// --- nsCSSText -----------------

nsCSSText::nsCSSText(void)
  : mTextShadow(nsnull)
{
}

nsCSSText::~nsCSSText(void)
{
  CSS_IF_DELETE(mTextShadow);
}

const nsID& nsCSSText::GetID(void)
{
  return kCSSTextSID;
}

void nsCSSText::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mWordSpacing.AppendToString(buffer, PROP_WORD_SPACING);
  mLetterSpacing.AppendToString(buffer, PROP_LETTER_SPACING);
  mDecoration.AppendToString(buffer, PROP_TEXT_DECORATION);
  mVerticalAlign.AppendToString(buffer, PROP_VERTICAL_ALIGN);
  mTextTransform.AppendToString(buffer, PROP_TEXT_TRANSFORM);
  mTextAlign.AppendToString(buffer, PROP_TEXT_ALIGN);
  mTextIndent.AppendToString(buffer, PROP_TEXT_INDENT);
  if (nsnull != mTextShadow) {
    if (mTextShadow->mXOffset.IsLengthUnit()) {
      nsCSSShadow*  shadow = mTextShadow;
      while (nsnull != shadow) {
        shadow->mColor.AppendToString(buffer, PROP_TEXT_SHADOW_COLOR);
        shadow->mXOffset.AppendToString(buffer, PROP_TEXT_SHADOW_X);
        shadow->mYOffset.AppendToString(buffer, PROP_TEXT_SHADOW_Y);
        shadow->mRadius.AppendToString(buffer, PROP_TEXT_SHADOW_RADIUS);
        shadow = shadow->mNext;
      }
    }
    else {
      mTextShadow->mXOffset.AppendToString(buffer, PROP_TEXT_SHADOW);
    }
  }
  mUnicodeBidi.AppendToString(buffer, PROP_UNICODE_BIDI);
  mLineHeight.AppendToString(buffer, PROP_LINE_HEIGHT);
  mWhiteSpace.AppendToString(buffer, PROP_WHITE_SPACE);
  fputs(buffer, out);
}

// --- nsCSSRect -----------------

void nsCSSRect::List(FILE* out, PRInt32 aPropID, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  if (-1 < aPropID) {
    buffer.Append(nsCSSProps::kNameTable[aPropID].name);
    buffer.Append(": ");
  }

  mTop.AppendToString(buffer);
  mRight.AppendToString(buffer);
  mBottom.AppendToString(buffer); 
  mLeft.AppendToString(buffer);
  fputs(buffer, out);
}

void nsCSSRect::List(FILE* out, PRInt32 aIndent, PRIntn aTRBL[]) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  if (eCSSUnit_Null != mTop.GetUnit()) {
    buffer.Append(nsCSSProps::kNameTable[aTRBL[0]].name);
    buffer.Append(": ");
    mTop.AppendToString(buffer);
  }
  if (eCSSUnit_Null != mRight.GetUnit()) {
    buffer.Append(nsCSSProps::kNameTable[aTRBL[1]].name);
    buffer.Append(": ");
    mRight.AppendToString(buffer);
  }
  if (eCSSUnit_Null != mBottom.GetUnit()) {
    buffer.Append(nsCSSProps::kNameTable[aTRBL[2]].name);
    buffer.Append(": ");
    mBottom.AppendToString(buffer); 
  }
  if (eCSSUnit_Null != mLeft.GetUnit()) {
    buffer.Append(nsCSSProps::kNameTable[aTRBL[3]].name);
    buffer.Append(": ");
    mLeft.AppendToString(buffer);
  }

  fputs(buffer, out);
}

// --- nsCSSDisplay -----------------

nsCSSDisplay::nsCSSDisplay(void)
  : mClip(nsnull)
{
}

nsCSSDisplay::~nsCSSDisplay(void)
{
  CSS_IF_DELETE(mClip);
}

const nsID& nsCSSDisplay::GetID(void)
{
  return kCSSDisplaySID;
}

void nsCSSDisplay::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mDirection.AppendToString(buffer, PROP_DIRECTION);
  mDisplay.AppendToString(buffer, PROP_DISPLAY);
  mFloat.AppendToString(buffer, PROP_FLOAT);
  mClear.AppendToString(buffer, PROP_CLEAR);
  mVisibility.AppendToString(buffer, PROP_VISIBILITY);
  mFilter.AppendToString(buffer, PROP_FILTER);
  fputs(buffer, out);
  if (nsnull != mClip) {
    mClip->List(out, PROP_CLIP);
  }
  buffer.SetLength(0);
  mOverflow.AppendToString(buffer, PROP_OVERFLOW);
  fputs(buffer, out);
}

// --- nsCSSMargin -----------------

nsCSSMargin::nsCSSMargin(void)
  : mMargin(nsnull), mPadding(nsnull), 
    mBorderWidth(nsnull), mBorderColor(nsnull), mBorderStyle(nsnull)
{
}

nsCSSMargin::~nsCSSMargin(void)
{
  CSS_IF_DELETE(mMargin);
  CSS_IF_DELETE(mPadding);
  CSS_IF_DELETE(mBorderWidth);
  CSS_IF_DELETE(mBorderColor);
  CSS_IF_DELETE(mBorderStyle);
}

const nsID& nsCSSMargin::GetID(void)
{
  return kCSSMarginSID;
}

void nsCSSMargin::List(FILE* out, PRInt32 aIndent) const
{
  if (nsnull != mMargin) {
    static PRIntn trbl[] = {
      PROP_MARGIN_TOP,
      PROP_MARGIN_RIGHT,
      PROP_MARGIN_BOTTOM,
      PROP_MARGIN_LEFT
    };
    mMargin->List(out, aIndent, trbl);
  }
  if (nsnull != mPadding) {
    static PRIntn trbl[] = {
      PROP_PADDING_TOP,
      PROP_PADDING_RIGHT,
      PROP_PADDING_BOTTOM,
      PROP_PADDING_LEFT
    };
    mPadding->List(out, aIndent, trbl);
  }
  if (nsnull != mBorderWidth) {
    static PRIntn trbl[] = {
      PROP_BORDER_TOP_WIDTH,
      PROP_BORDER_RIGHT_WIDTH,
      PROP_BORDER_BOTTOM_WIDTH,
      PROP_BORDER_LEFT_WIDTH
    };
    mBorderWidth->List(out, aIndent, trbl);
  }
  if (nsnull != mBorderColor) {
    mBorderColor->List(out, PROP_BORDER_COLOR, aIndent);
  }
  if (nsnull != mBorderStyle) {
    mBorderStyle->List(out, PROP_BORDER_STYLE, aIndent);
  }

  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);
 
  nsAutoString  buffer;
  mBorderRadius.AppendToString(buffer, PROP__MOZ_BORDER_RADIUS);
  mOutlineWidth.AppendToString(buffer, PROP_OUTLINE_WIDTH);
  mOutlineColor.AppendToString(buffer, PROP_OUTLINE_COLOR);
  mOutlineStyle.AppendToString(buffer, PROP_OUTLINE_STYLE);
  fputs(buffer, out);
}

// --- nsCSSPosition -----------------

nsCSSPosition::nsCSSPosition(void)
  : mOffset(nsnull)
{
}

nsCSSPosition::~nsCSSPosition(void)
{
  CSS_IF_DELETE(mOffset);
}

const nsID& nsCSSPosition::GetID(void)
{
  return kCSSPositionSID;
}

void nsCSSPosition::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mPosition.AppendToString(buffer, PROP_POSITION);
  mWidth.AppendToString(buffer, PROP_WIDTH);
  mMinWidth.AppendToString(buffer, PROP_MIN_WIDTH);
  mMaxWidth.AppendToString(buffer, PROP_MAX_WIDTH);
  mHeight.AppendToString(buffer, PROP_HEIGHT);
  mMinHeight.AppendToString(buffer, PROP_MIN_HEIGHT);
  mMaxHeight.AppendToString(buffer, PROP_MAX_HEIGHT);
  mZIndex.AppendToString(buffer, PROP_Z_INDEX);
  fputs(buffer, out);

  if (nsnull != mOffset) {
    static PRIntn trbl[] = {
      PROP_TOP,
      PROP_RIGHT,
      PROP_BOTTOM,
      PROP_LEFT
    };
    mOffset->List(out, aIndent, trbl);
  }
}

// --- nsCSSList -----------------

nsCSSList::~nsCSSList()
{
}

const nsID& nsCSSList::GetID(void)
{
  return kCSSListSID;
}

void nsCSSList::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mType.AppendToString(buffer, PROP_LIST_STYLE_TYPE);
  mImage.AppendToString(buffer, PROP_LIST_STYLE_IMAGE);
  mPosition.AppendToString(buffer, PROP_LIST_STYLE_POSITION);
  fputs(buffer, out);
}

// --- nsCSSTable -----------------

nsCSSTable::~nsCSSTable()
{
}

const nsID& nsCSSTable::GetID(void)
{
  return kCSSTableSID;
}

void nsCSSTable::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mBorderCollapse.AppendToString(buffer, PROP_BORDER_COLLAPSE);
  mBorderSpacingX.AppendToString(buffer, PROP_BORDER_X_SPACING);
  mBorderSpacingY.AppendToString(buffer, PROP_BORDER_Y_SPACING);
  mCaptionSide.AppendToString(buffer, PROP_CAPTION_SIDE);
  mEmptyCells.AppendToString(buffer, PROP_EMPTY_CELLS);
  mLayout.AppendToString(buffer, PROP_TABLE_LAYOUT);

  fputs(buffer, out);
}

// --- nsCSSBreaks -----------------

nsCSSBreaks::~nsCSSBreaks()
{
}

const nsID& nsCSSBreaks::GetID(void)
{
  return kCSSBreaksSID;
}

void nsCSSBreaks::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mOrphans.AppendToString(buffer, PROP_ORPHANS);
  mWidows.AppendToString(buffer, PROP_WIDOWS);
  mPage.AppendToString(buffer, PROP_PAGE);
  mPageBreakAfter.AppendToString(buffer, PROP_PAGE_BREAK_AFTER);
  mPageBreakBefore.AppendToString(buffer, PROP_PAGE_BREAK_BEFORE);
  mPageBreakInside.AppendToString(buffer, PROP_PAGE_BREAK_INSIDE);

  fputs(buffer, out);
}

// --- nsCSSPage -----------------

nsCSSPage::~nsCSSPage()
{
}

const nsID& nsCSSPage::GetID(void)
{
  return kCSSPageSID;
}

void nsCSSPage::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mMarks.AppendToString(buffer, PROP_MARKS);
  mSizeWidth.AppendToString(buffer, PROP_SIZE_WIDTH);
  mSizeHeight.AppendToString(buffer, PROP_SIZE_HEIGHT);

  fputs(buffer, out);
}

// --- nsCSSContent support -----------------

nsCSSCounterData::nsCSSCounterData(void)
  : mNext(nsnull)
{
}

nsCSSCounterData::~nsCSSCounterData(void)
{
  CSS_IF_DELETE(mNext);
}

nsCSSQuotes::nsCSSQuotes(void)
  : mNext(nsnull)
{
}

nsCSSQuotes::~nsCSSQuotes(void)
{
  CSS_IF_DELETE(mNext);
}

// --- nsCSSContent -----------------

nsCSSContent::nsCSSContent(void)
  : mContent(nsnull),
    mCounterIncrement(nsnull),
    mCounterReset(nsnull),
    mQuotes(nsnull)
{
}

nsCSSContent::~nsCSSContent(void)
{
  CSS_IF_DELETE(mContent);
  CSS_IF_DELETE(mCounterIncrement);
  CSS_IF_DELETE(mCounterReset);
  CSS_IF_DELETE(mQuotes);
}

const nsID& nsCSSContent::GetID(void)
{
  return kCSSContentSID;
}

void nsCSSContent::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  nsCSSValueList*  content = mContent;
  while (nsnull != content) {
    content->mValue.AppendToString(buffer, PROP_CONTENT);
    content = content->mNext;
  }
  nsCSSCounterData* counter = mCounterIncrement;
  while (nsnull != counter) {
    counter->mCounter.AppendToString(buffer, PROP_COUNTER_INCREMENT);
    counter->mValue.AppendToString(buffer, -1);
    counter = counter->mNext;
  }
  counter = mCounterReset;
  while (nsnull != counter) {
    counter->mCounter.AppendToString(buffer, PROP_COUNTER_RESET);
    counter->mValue.AppendToString(buffer, -1);
    counter = counter->mNext;
  }
  mMarkerOffset.AppendToString(buffer, PROP_MARKER_OFFSET);
  nsCSSQuotes*  quotes = mQuotes;
  while (nsnull != quotes) {
    quotes->mOpen.AppendToString(buffer, PROP_QUOTES_OPEN);
    quotes->mClose.AppendToString(buffer, PROP_QUOTES_CLOSE);
    quotes = quotes->mNext;
  }

  fputs(buffer, out);
}

// --- nsCSSAural -----------------

nsCSSAural::~nsCSSAural()
{
}

const nsID& nsCSSAural::GetID(void)
{
  return kCSSAuralSID;
}

void nsCSSAural::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mAzimuth.AppendToString(buffer, PROP_AZIMUTH);
  mElevation.AppendToString(buffer, PROP_ELEVATION);
  mCueAfter.AppendToString(buffer, PROP_CUE_AFTER);
  mCueBefore.AppendToString(buffer, PROP_CUE_BEFORE);
  mPauseAfter.AppendToString(buffer, PROP_PAUSE_AFTER);
  mPauseBefore.AppendToString(buffer, PROP_PAUSE_BEFORE);
  mPitch.AppendToString(buffer, PROP_PITCH);
  mPitchRange.AppendToString(buffer, PROP_PITCH_RANGE);
  mPlayDuring.AppendToString(buffer, PROP_PLAY_DURING);
  mPlayDuringFlags.AppendToString(buffer, PROP_PLAY_DURING_FLAGS);
  mRichness.AppendToString(buffer, PROP_RICHNESS);
  mSpeak.AppendToString(buffer, PROP_SPEAK);
  mSpeakHeader.AppendToString(buffer, PROP_SPEAK_HEADER);
  mSpeakNumeral.AppendToString(buffer, PROP_SPEAK_NUMERAL);
  mSpeakPunctuation.AppendToString(buffer, PROP_SPEAK_PUNCTUATION);
  mSpeechRate.AppendToString(buffer, PROP_SPEECH_RATE);
  mStress.AppendToString(buffer, PROP_STRESS);
  mVoiceFamily.AppendToString(buffer, PROP_VOICE_FAMILY);
  mVolume.AppendToString(buffer, PROP_VOLUME);

  fputs(buffer, out);
}



// --- nsCSSDeclaration -----------------


class CSSDeclarationImpl : public nsICSSDeclaration {
public:
  CSSDeclarationImpl(void);
  virtual ~CSSDeclarationImpl(void);

  NS_DECL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetData(const nsID& aSID, nsCSSStruct** aData);
  NS_IMETHOD EnsureData(const nsID& aSID, nsCSSStruct** aData);

  NS_IMETHOD AppendValue(PRInt32 aProperty, const nsCSSValue& aValue);
  NS_IMETHOD AppendStructValue(PRInt32 aProperty, void* aStruct);
  NS_IMETHOD SetValueImportant(PRInt32 aProperty);
  NS_IMETHOD AppendComment(const nsString& aComment);

  NS_IMETHOD GetValue(PRInt32 aProperty, nsCSSValue& aValue);
  NS_IMETHOD GetValue(PRInt32 aProperty, nsString& aValue);
  NS_IMETHOD GetValue(const nsString& aProperty, nsString& aValue);

  NS_IMETHOD GetImportantValues(nsICSSDeclaration*& aResult);
  NS_IMETHOD GetValueIsImportant(PRInt32 aProperty, PRBool& aIsImportant);
  NS_IMETHOD GetValueIsImportant(const nsString& aProperty, PRBool& aIsImportant);

  PRBool   AppendValueToString(PRInt32 aProperty, nsString& aResult);
  PRBool   AppendValueToString(PRInt32 aProperty, const nsCSSValue& aValue, nsString& aResult);

  NS_IMETHOD ToString(nsString& aString);

  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
  
  NS_IMETHOD Count(PRUint32* aCount);
  NS_IMETHOD GetNthProperty(PRUint32 aIndex, nsString& aReturn);

  NS_IMETHOD GetStyleImpact(PRInt32* aHint) const;

private:
  CSSDeclarationImpl(const CSSDeclarationImpl& aCopy);
  CSSDeclarationImpl& operator=(const CSSDeclarationImpl& aCopy);
  PRBool operator==(const CSSDeclarationImpl& aCopy) const;

protected:
  nsCSSFont*      mFont;
  nsCSSColor*     mColor;
  nsCSSText*      mText;
  nsCSSMargin*    mMargin;
  nsCSSPosition*  mPosition;
  nsCSSList*      mList;
  nsCSSDisplay*   mDisplay;
  nsCSSTable*     mTable;
  nsCSSBreaks*    mBreaks;
  nsCSSPage*      mPage;
  nsCSSContent*   mContent;
  nsCSSAural*     mAural;

  CSSDeclarationImpl* mImportant;

  nsVoidArray*    mOrder;
  nsVoidArray*    mComments;
};

#ifdef DEBUG_REFS
static PRInt32 gInstanceCount;
#endif


NS_IMPL_ZEROING_OPERATOR_NEW(CSSDeclarationImpl)

CSSDeclarationImpl::CSSDeclarationImpl(void)
{
  NS_INIT_REFCNT();
#ifdef DEBUG_REFS
  ++gInstanceCount;
  fprintf(stdout, "%d + CSSDeclaration\n", gInstanceCount);
#endif
}

CSSDeclarationImpl::~CSSDeclarationImpl(void)
{
  CSS_IF_DELETE(mFont);
  CSS_IF_DELETE(mColor);
  CSS_IF_DELETE(mText);
  CSS_IF_DELETE(mMargin);
  CSS_IF_DELETE(mPosition);
  CSS_IF_DELETE(mList);
  CSS_IF_DELETE(mDisplay);
  CSS_IF_DELETE(mTable);
  CSS_IF_DELETE(mBreaks);
  CSS_IF_DELETE(mPage);
  CSS_IF_DELETE(mContent);
  CSS_IF_DELETE(mAural);

  NS_IF_RELEASE(mImportant);
  CSS_IF_DELETE(mOrder);
  if (nsnull != mComments) {
    PRInt32 index = mComments->Count();
    while (0 < --index) {
      nsString* comment = (nsString*)mComments->ElementAt(index);
      delete comment;
    }
    delete mComments;
  }

#ifdef DEBUG_REFS
  --gInstanceCount;
  fprintf(stdout, "%d - CSSDeclaration\n", gInstanceCount);
#endif
}

NS_IMPL_ISUPPORTS(CSSDeclarationImpl, kICSSDeclarationIID);

#define CSS_IF_GET_ELSE(sid,ptr,result) \
  if (sid.Equals(kCSS##ptr##SID))  { *result = m##ptr;  } else

NS_IMETHODIMP
CSSDeclarationImpl::GetData(const nsID& aSID, nsCSSStruct** aDataPtr)
{
  if (nsnull == aDataPtr) {
    return NS_ERROR_NULL_POINTER;
  }

  CSS_IF_GET_ELSE(aSID, Font, aDataPtr)
  CSS_IF_GET_ELSE(aSID, Color, aDataPtr)
  CSS_IF_GET_ELSE(aSID, Display, aDataPtr)
  CSS_IF_GET_ELSE(aSID, Text, aDataPtr)
  CSS_IF_GET_ELSE(aSID, Margin, aDataPtr)
  CSS_IF_GET_ELSE(aSID, Position, aDataPtr)
  CSS_IF_GET_ELSE(aSID, List, aDataPtr)
  CSS_IF_GET_ELSE(aSID, Table, aDataPtr)
  CSS_IF_GET_ELSE(aSID, Breaks, aDataPtr)
  CSS_IF_GET_ELSE(aSID, Page, aDataPtr)
  CSS_IF_GET_ELSE(aSID, Content, aDataPtr)
  CSS_IF_GET_ELSE(aSID, Aural, aDataPtr) {
    return NS_NOINTERFACE;
  }
  return NS_OK;
}

#define CSS_IF_ENSURE_ELSE(sid,ptr,result)                \
  if (sid.Equals(kCSS##ptr##SID)) {                       \
    if (nsnull == m##ptr) { m##ptr = new nsCSS##ptr(); }  \
    *result = m##ptr;                                     \
  } else

NS_IMETHODIMP
CSSDeclarationImpl::EnsureData(const nsID& aSID, nsCSSStruct** aDataPtr)
{
  if (nsnull == aDataPtr) {
    return NS_ERROR_NULL_POINTER;
  }

  CSS_IF_ENSURE_ELSE(aSID, Font, aDataPtr)
  CSS_IF_ENSURE_ELSE(aSID, Color, aDataPtr)
  CSS_IF_ENSURE_ELSE(aSID, Display, aDataPtr)
  CSS_IF_ENSURE_ELSE(aSID, Text, aDataPtr)
  CSS_IF_ENSURE_ELSE(aSID, Margin, aDataPtr)
  CSS_IF_ENSURE_ELSE(aSID, Position, aDataPtr)
  CSS_IF_ENSURE_ELSE(aSID, List, aDataPtr)
  CSS_IF_ENSURE_ELSE(aSID, Table, aDataPtr)
  CSS_IF_ENSURE_ELSE(aSID, Breaks, aDataPtr)
  CSS_IF_ENSURE_ELSE(aSID, Page, aDataPtr)
  CSS_IF_ENSURE_ELSE(aSID, Content, aDataPtr)
  CSS_IF_ENSURE_ELSE(aSID, Aural, aDataPtr) {
    return NS_NOINTERFACE;
  }
  if (nsnull == *aDataPtr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

#define CSS_ENSURE(data)              \
  if (nsnull == m##data) {            \
    m##data = new nsCSS##data();      \
  }                                   \
  if (nsnull == m##data) {            \
    result = NS_ERROR_OUT_OF_MEMORY;  \
  }                                   \
  else

#define CSS_ENSURE_RECT(data)         \
  if (nsnull == data) {               \
    data = new nsCSSRect();           \
  }                                   \
  if (nsnull == data) {               \
    result = NS_ERROR_OUT_OF_MEMORY;  \
  }                                   \
  else

#define CSS_ENSURE_DATA(data,type)    \
  if (nsnull == data) {               \
    data = new type();                \
  }                                   \
  if (nsnull == data) {               \
    result = NS_ERROR_OUT_OF_MEMORY;  \
  }                                   \
  else

NS_IMETHODIMP
CSSDeclarationImpl::AppendValue(PRInt32 aProperty, const nsCSSValue& aValue)
{
  nsresult result = NS_OK;

  switch (aProperty) {
    // nsCSSFont
    case PROP_FONT_FAMILY:
    case PROP_FONT_STYLE:
    case PROP_FONT_VARIANT:
    case PROP_FONT_WEIGHT:
    case PROP_FONT_SIZE:
    case PROP_FONT_SIZE_ADJUST:
    case PROP_FONT_STRETCH:
      CSS_ENSURE(Font) {
        switch (aProperty) {
          case PROP_FONT_FAMILY:      mFont->mFamily = aValue;      break;
          case PROP_FONT_STYLE:       mFont->mStyle = aValue;       break;
          case PROP_FONT_VARIANT:     mFont->mVariant = aValue;     break;
          case PROP_FONT_WEIGHT:      mFont->mWeight = aValue;      break;
          case PROP_FONT_SIZE:        mFont->mSize = aValue;        break;
          case PROP_FONT_SIZE_ADJUST: mFont->mSizeAdjust = aValue;  break;
          case PROP_FONT_STRETCH:     mFont->mStretch = aValue;     break;
        }
      }
      break;

    // nsCSSColor
    case PROP_COLOR:
    case PROP_BACKGROUND_COLOR:
    case PROP_BACKGROUND_IMAGE:
    case PROP_BACKGROUND_REPEAT:
    case PROP_BACKGROUND_ATTACHMENT:
    case PROP_BACKGROUND_X_POSITION:
    case PROP_BACKGROUND_Y_POSITION:
    case PROP_BACKGROUND_FILTER:
    case PROP_CURSOR:
    case PROP_OPACITY:
      CSS_ENSURE(Color) {
        switch (aProperty) {
          case PROP_COLOR:                  mColor->mColor = aValue;           break;
          case PROP_BACKGROUND_COLOR:       mColor->mBackColor = aValue;       break;
          case PROP_BACKGROUND_IMAGE:       mColor->mBackImage = aValue;       break;
          case PROP_BACKGROUND_REPEAT:      mColor->mBackRepeat = aValue;      break;
          case PROP_BACKGROUND_ATTACHMENT:  mColor->mBackAttachment = aValue;  break;
          case PROP_BACKGROUND_X_POSITION:  mColor->mBackPositionX = aValue;   break;
          case PROP_BACKGROUND_Y_POSITION:  mColor->mBackPositionY = aValue;   break;
          case PROP_BACKGROUND_FILTER:      mColor->mBackFilter = aValue;      break;
          case PROP_CURSOR:
            CSS_ENSURE_DATA(mColor->mCursor, nsCSSValueList) {
              mColor->mCursor->mValue = aValue;
              CSS_IF_DELETE(mColor->mCursor->mNext);
            }
            break;
          case PROP_OPACITY:                mColor->mOpacity = aValue;         break;
        }
      }
      break;

    // nsCSSText
    case PROP_WORD_SPACING:
    case PROP_LETTER_SPACING:
    case PROP_TEXT_DECORATION:
    case PROP_VERTICAL_ALIGN:
    case PROP_TEXT_TRANSFORM:
    case PROP_TEXT_ALIGN:
    case PROP_TEXT_INDENT:
    case PROP_UNICODE_BIDI:
    case PROP_LINE_HEIGHT:
    case PROP_WHITE_SPACE:
      CSS_ENSURE(Text) {
        switch (aProperty) {
          case PROP_WORD_SPACING:     mText->mWordSpacing = aValue;    break;
          case PROP_LETTER_SPACING:   mText->mLetterSpacing = aValue;  break;
          case PROP_TEXT_DECORATION:  mText->mDecoration = aValue;     break;
          case PROP_VERTICAL_ALIGN:   mText->mVerticalAlign = aValue;  break;
          case PROP_TEXT_TRANSFORM:   mText->mTextTransform = aValue;  break;
          case PROP_TEXT_ALIGN:       mText->mTextAlign = aValue;      break;
          case PROP_TEXT_INDENT:      mText->mTextIndent = aValue;     break;
          case PROP_UNICODE_BIDI:     mText->mUnicodeBidi = aValue;    break;
          case PROP_LINE_HEIGHT:      mText->mLineHeight = aValue;     break;
          case PROP_WHITE_SPACE:      mText->mWhiteSpace = aValue;     break;
        }
      }
      break;

    case PROP_TEXT_SHADOW_COLOR:
    case PROP_TEXT_SHADOW_RADIUS:
    case PROP_TEXT_SHADOW_X:
    case PROP_TEXT_SHADOW_Y:
      CSS_ENSURE(Text) {
        CSS_ENSURE_DATA(mText->mTextShadow, nsCSSShadow) {
          switch (aProperty) {
            case PROP_TEXT_SHADOW_COLOR:  mText->mTextShadow->mColor = aValue;    break;
            case PROP_TEXT_SHADOW_RADIUS: mText->mTextShadow->mRadius = aValue;   break;
            case PROP_TEXT_SHADOW_X:      mText->mTextShadow->mXOffset = aValue;  break;
            case PROP_TEXT_SHADOW_Y:      mText->mTextShadow->mYOffset = aValue;  break;
          }
          CSS_IF_DELETE(mText->mTextShadow->mNext);
        }
      }
      break;

      // nsCSSDisplay
    case PROP_FLOAT:
    case PROP_CLEAR:
    case PROP_DISPLAY:
    case PROP_DIRECTION:
    case PROP_VISIBILITY:
    case PROP_OVERFLOW:
    case PROP_FILTER:
      CSS_ENSURE(Display) {
        switch (aProperty) {
          case PROP_FLOAT:      mDisplay->mFloat = aValue;      break;
          case PROP_CLEAR:      mDisplay->mClear = aValue;      break;
          case PROP_DISPLAY:    mDisplay->mDisplay = aValue;    break;
          case PROP_DIRECTION:  mDisplay->mDirection = aValue;  break;
          case PROP_VISIBILITY: mDisplay->mVisibility = aValue; break;
          case PROP_OVERFLOW:   mDisplay->mOverflow = aValue;   break;
          case PROP_FILTER:     mDisplay->mFilter = aValue;     break;
        }
      }
      break;

    case PROP_CLIP_TOP:
    case PROP_CLIP_RIGHT:
    case PROP_CLIP_BOTTOM:
    case PROP_CLIP_LEFT:
      CSS_ENSURE(Display) {
        CSS_ENSURE_RECT(mDisplay->mClip) {
          switch(aProperty) {
            case PROP_CLIP_TOP:     mDisplay->mClip->mTop = aValue;     break;
            case PROP_CLIP_RIGHT:   mDisplay->mClip->mRight = aValue;   break;
            case PROP_CLIP_BOTTOM:  mDisplay->mClip->mBottom = aValue;  break;
            case PROP_CLIP_LEFT:    mDisplay->mClip->mLeft = aValue;    break;
          }
        }
      }
      break;

    // nsCSSMargin
    case PROP_MARGIN_TOP:
    case PROP_MARGIN_RIGHT:
    case PROP_MARGIN_BOTTOM:
    case PROP_MARGIN_LEFT:
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(mMargin->mMargin) {
          switch (aProperty) {
            case PROP_MARGIN_TOP:     mMargin->mMargin->mTop = aValue;     break;
            case PROP_MARGIN_RIGHT:   mMargin->mMargin->mRight = aValue;   break;
            case PROP_MARGIN_BOTTOM:  mMargin->mMargin->mBottom = aValue;  break;
            case PROP_MARGIN_LEFT:    mMargin->mMargin->mLeft = aValue;    break;
          }
        }
      }
      break;

    case PROP_PADDING_TOP:
    case PROP_PADDING_RIGHT:
    case PROP_PADDING_BOTTOM:
    case PROP_PADDING_LEFT:
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(mMargin->mPadding) {
          switch (aProperty) {
            case PROP_PADDING_TOP:    mMargin->mPadding->mTop = aValue;    break;
            case PROP_PADDING_RIGHT:  mMargin->mPadding->mRight = aValue;  break;
            case PROP_PADDING_BOTTOM: mMargin->mPadding->mBottom = aValue; break;
            case PROP_PADDING_LEFT:   mMargin->mPadding->mLeft = aValue;   break;
          }
        }
      }
      break;

    case PROP_BORDER_TOP_WIDTH:
    case PROP_BORDER_RIGHT_WIDTH:
    case PROP_BORDER_BOTTOM_WIDTH:
    case PROP_BORDER_LEFT_WIDTH:
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(mMargin->mBorderWidth) {
          switch (aProperty) {
            case PROP_BORDER_TOP_WIDTH:     mMargin->mBorderWidth->mTop = aValue;     break;
            case PROP_BORDER_RIGHT_WIDTH:   mMargin->mBorderWidth->mRight = aValue;   break;
            case PROP_BORDER_BOTTOM_WIDTH:  mMargin->mBorderWidth->mBottom = aValue;  break;
            case PROP_BORDER_LEFT_WIDTH:    mMargin->mBorderWidth->mLeft = aValue;    break;
          }
        }
      }
      break;

    case PROP_BORDER_TOP_COLOR:
    case PROP_BORDER_RIGHT_COLOR:
    case PROP_BORDER_BOTTOM_COLOR:
    case PROP_BORDER_LEFT_COLOR:
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(mMargin->mBorderColor) {
          switch (aProperty) {
            case PROP_BORDER_TOP_COLOR:     mMargin->mBorderColor->mTop = aValue;    break;
            case PROP_BORDER_RIGHT_COLOR:   mMargin->mBorderColor->mRight = aValue;  break;
            case PROP_BORDER_BOTTOM_COLOR:  mMargin->mBorderColor->mBottom = aValue; break;
            case PROP_BORDER_LEFT_COLOR:    mMargin->mBorderColor->mLeft = aValue;   break;
          }
        }
      }
      break;

    case PROP_BORDER_TOP_STYLE:
    case PROP_BORDER_RIGHT_STYLE:
    case PROP_BORDER_BOTTOM_STYLE:
    case PROP_BORDER_LEFT_STYLE:
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(mMargin->mBorderStyle) {
          switch (aProperty) {
            case PROP_BORDER_TOP_STYLE:     mMargin->mBorderStyle->mTop = aValue;    break;
            case PROP_BORDER_RIGHT_STYLE:   mMargin->mBorderStyle->mRight = aValue;  break;
            case PROP_BORDER_BOTTOM_STYLE:  mMargin->mBorderStyle->mBottom = aValue; break;
            case PROP_BORDER_LEFT_STYLE:    mMargin->mBorderStyle->mLeft = aValue;   break;
          }
        }
      }
      break;

    case PROP__MOZ_BORDER_RADIUS:
    case PROP_OUTLINE_WIDTH:
    case PROP_OUTLINE_COLOR:
    case PROP_OUTLINE_STYLE:
      CSS_ENSURE(Margin) {
        switch (aProperty) {
          case PROP__MOZ_BORDER_RADIUS: mMargin->mBorderRadius = aValue;  break;
          case PROP_OUTLINE_WIDTH:      mMargin->mOutlineWidth = aValue;  break;
          case PROP_OUTLINE_COLOR:      mMargin->mOutlineColor = aValue;  break;
          case PROP_OUTLINE_STYLE:      mMargin->mOutlineStyle = aValue;  break;
        }
      }
      break;

    // nsCSSPosition
    case PROP_POSITION:
    case PROP_WIDTH:
    case PROP_MIN_WIDTH:
    case PROP_MAX_WIDTH:
    case PROP_HEIGHT:
    case PROP_MIN_HEIGHT:
    case PROP_MAX_HEIGHT:
    case PROP_Z_INDEX:
      CSS_ENSURE(Position) {
        switch (aProperty) {
          case PROP_POSITION:   mPosition->mPosition = aValue;   break;
          case PROP_WIDTH:      mPosition->mWidth = aValue;      break;
          case PROP_MIN_WIDTH:  mPosition->mMinWidth = aValue;   break;
          case PROP_MAX_WIDTH:  mPosition->mMaxWidth = aValue;   break;
          case PROP_HEIGHT:     mPosition->mHeight = aValue;     break;
          case PROP_MIN_HEIGHT: mPosition->mMinHeight = aValue;  break;
          case PROP_MAX_HEIGHT: mPosition->mMaxHeight = aValue;  break;
          case PROP_Z_INDEX:    mPosition->mZIndex = aValue;     break;
        }
      }
      break;

    case PROP_TOP:
    case PROP_RIGHT:
    case PROP_BOTTOM:
    case PROP_LEFT:
      CSS_ENSURE(Position) {
        CSS_ENSURE_RECT(mPosition->mOffset) {
          switch (aProperty) {
            case PROP_TOP:    mPosition->mOffset->mTop = aValue;    break;
            case PROP_RIGHT:  mPosition->mOffset->mRight= aValue;   break;
            case PROP_BOTTOM: mPosition->mOffset->mBottom = aValue; break;
            case PROP_LEFT:   mPosition->mOffset->mLeft = aValue;   break;
          }
        }
      }
      break;

      // nsCSSList
    case PROP_LIST_STYLE_TYPE:
    case PROP_LIST_STYLE_IMAGE:
    case PROP_LIST_STYLE_POSITION:
      CSS_ENSURE(List) {
        switch (aProperty) {
          case PROP_LIST_STYLE_TYPE:      mList->mType = aValue;     break;
          case PROP_LIST_STYLE_IMAGE:     mList->mImage = aValue;    break;
          case PROP_LIST_STYLE_POSITION:  mList->mPosition = aValue; break;
        }
      }
      break;

      // nsCSSTable
    case PROP_BORDER_COLLAPSE:
    case PROP_BORDER_X_SPACING:
    case PROP_BORDER_Y_SPACING:
    case PROP_CAPTION_SIDE:
    case PROP_EMPTY_CELLS:
    case PROP_TABLE_LAYOUT:
      CSS_ENSURE(Table) {
        switch (aProperty) {
          case PROP_BORDER_COLLAPSE:  mTable->mBorderCollapse = aValue; break;
          case PROP_BORDER_X_SPACING: mTable->mBorderSpacingX = aValue; break;
          case PROP_BORDER_Y_SPACING: mTable->mBorderSpacingY = aValue; break;
          case PROP_CAPTION_SIDE:     mTable->mCaptionSide = aValue;    break;
          case PROP_EMPTY_CELLS:      mTable->mEmptyCells = aValue;     break;
          case PROP_TABLE_LAYOUT:     mTable->mLayout = aValue;         break;
        }
      }
      break;

      // nsCSSBreaks
    case PROP_ORPHANS:
    case PROP_WIDOWS:
    case PROP_PAGE:
    case PROP_PAGE_BREAK_AFTER:
    case PROP_PAGE_BREAK_BEFORE:
    case PROP_PAGE_BREAK_INSIDE:
      CSS_ENSURE(Breaks) {
        switch (aProperty) {
          case PROP_ORPHANS:            mBreaks->mOrphans = aValue;         break;
          case PROP_WIDOWS:             mBreaks->mWidows = aValue;          break;
          case PROP_PAGE:               mBreaks->mPage = aValue;            break;
          case PROP_PAGE_BREAK_AFTER:   mBreaks->mPageBreakAfter = aValue;  break;
          case PROP_PAGE_BREAK_BEFORE:  mBreaks->mPageBreakBefore = aValue; break;
          case PROP_PAGE_BREAK_INSIDE:  mBreaks->mPageBreakInside = aValue; break;
        }
      }
      break;

      // nsCSSPage
    case PROP_MARKS:
    case PROP_SIZE_WIDTH:
    case PROP_SIZE_HEIGHT:
      CSS_ENSURE(Page) {
        switch (aProperty) {
          case PROP_MARKS:        mPage->mMarks = aValue; break;
          case PROP_SIZE_WIDTH:   mPage->mSizeWidth = aValue;  break;
          case PROP_SIZE_HEIGHT:  mPage->mSizeHeight = aValue;  break;
        }
      }
      break;

      // nsCSSContent
    case PROP_CONTENT:
    case PROP_COUNTER_INCREMENT:
    case PROP_COUNTER_RESET:
    case PROP_MARKER_OFFSET:
    case PROP_QUOTES_OPEN:
    case PROP_QUOTES_CLOSE:
      CSS_ENSURE(Content) {
        switch (aProperty) {
          case PROP_CONTENT:
            CSS_ENSURE_DATA(mContent->mContent, nsCSSValueList) {
              mContent->mContent->mValue = aValue;          
              CSS_IF_DELETE(mContent->mContent->mNext);
            }
            break;
          case PROP_COUNTER_INCREMENT:
            CSS_ENSURE_DATA(mContent->mCounterIncrement, nsCSSCounterData) {
              mContent->mCounterIncrement->mCounter = aValue; 
              CSS_IF_DELETE(mContent->mCounterIncrement->mNext);
            }
            break;
          case PROP_COUNTER_RESET:
            CSS_ENSURE_DATA(mContent->mCounterReset, nsCSSCounterData) {
              mContent->mCounterReset->mCounter = aValue;
              CSS_IF_DELETE(mContent->mCounterReset->mNext);
            }
            break;
          case PROP_MARKER_OFFSET:      mContent->mMarkerOffset = aValue;     break;
          case PROP_QUOTES_OPEN:
            CSS_ENSURE_DATA(mContent->mQuotes, nsCSSQuotes) {
              mContent->mQuotes->mOpen = aValue;          
              CSS_IF_DELETE(mContent->mQuotes->mNext);
            }
            break;
          case PROP_QUOTES_CLOSE:
            CSS_ENSURE_DATA(mContent->mQuotes, nsCSSQuotes) {
              mContent->mQuotes->mClose = aValue;          
              CSS_IF_DELETE(mContent->mQuotes->mNext);
            }
            break;
        }
      }
      break;

      // nsCSSAural
    case PROP_AZIMUTH:
    case PROP_ELEVATION:
    case PROP_CUE_AFTER:
    case PROP_CUE_BEFORE:
    case PROP_PAUSE_AFTER:
    case PROP_PAUSE_BEFORE:
    case PROP_PITCH:
    case PROP_PITCH_RANGE:
    case PROP_PLAY_DURING:
    case PROP_PLAY_DURING_FLAGS:
    case PROP_RICHNESS:
    case PROP_SPEAK:
    case PROP_SPEAK_HEADER:
    case PROP_SPEAK_NUMERAL:
    case PROP_SPEAK_PUNCTUATION:
    case PROP_SPEECH_RATE:
    case PROP_STRESS:
    case PROP_VOICE_FAMILY:
    case PROP_VOLUME:
      CSS_ENSURE(Aural) {
        switch (aProperty) {
          case PROP_AZIMUTH:            mAural->mAzimuth = aValue;          break;
          case PROP_ELEVATION:          mAural->mElevation = aValue;        break;
          case PROP_CUE_AFTER:          mAural->mCueAfter = aValue;         break;
          case PROP_CUE_BEFORE:         mAural->mCueBefore = aValue;        break;
          case PROP_PAUSE_AFTER:        mAural->mPauseAfter = aValue;       break;
          case PROP_PAUSE_BEFORE:       mAural->mPauseBefore = aValue;      break;
          case PROP_PITCH:              mAural->mPitch = aValue;            break;
          case PROP_PITCH_RANGE:        mAural->mPitchRange = aValue;       break;
          case PROP_PLAY_DURING:        mAural->mPlayDuring = aValue;       break;
          case PROP_PLAY_DURING_FLAGS:  mAural->mPlayDuringFlags = aValue;  break;
          case PROP_RICHNESS:           mAural->mRichness = aValue;         break;
          case PROP_SPEAK:              mAural->mSpeak = aValue;            break;
          case PROP_SPEAK_HEADER:       mAural->mSpeakHeader = aValue;      break;
          case PROP_SPEAK_NUMERAL:      mAural->mSpeakNumeral = aValue;     break;
          case PROP_SPEAK_PUNCTUATION:  mAural->mSpeakPunctuation = aValue; break;
          case PROP_SPEECH_RATE:        mAural->mSpeechRate = aValue;       break;
          case PROP_STRESS:             mAural->mStress = aValue;           break;
          case PROP_VOICE_FAMILY:       mAural->mVoiceFamily = aValue;      break;
          case PROP_VOLUME:             mAural->mVolume = aValue;           break;
        }
      }
      break;

      // Shorthands
    case PROP_BACKGROUND:
    case PROP_BORDER:
    case PROP_BORDER_SPACING:
    case PROP_CLIP:
    case PROP_CUE:
    case PROP_FONT:
    case PROP_LIST_STYLE:
    case PROP_MARGIN:
    case PROP_OUTLINE:
    case PROP_PADDING:
    case PROP_PAUSE:
    case PROP_QUOTES:
    case PROP_SIZE:
    case PROP_TEXT_SHADOW:
    case PROP_BACKGROUND_POSITION:
    case PROP_BORDER_TOP:
    case PROP_BORDER_RIGHT:
    case PROP_BORDER_BOTTOM:
    case PROP_BORDER_LEFT:
    case PROP_BORDER_COLOR:
    case PROP_BORDER_STYLE:
    case PROP_BORDER_WIDTH:
      NS_ERROR("can't append shorthand properties");
    default:
      result = NS_ERROR_ILLEGAL_VALUE;
      break;
  }

  if (NS_OK == result) {
    if (nsnull == mOrder) {
      mOrder = new nsVoidArray();
    }
    if (nsnull != mOrder) {
      PRInt32 index = mOrder->IndexOf((void*)aProperty);
      if (-1 != index) {
        mOrder->RemoveElementAt(index);
      }
      if (eCSSUnit_Null != aValue.GetUnit()) {
        mOrder->AppendElement((void*)aProperty);
      }
    }
  }
  return result;
}

NS_IMETHODIMP
CSSDeclarationImpl::AppendStructValue(PRInt32 aProperty, void* aStruct)
{
  NS_ASSERTION(nsnull != aStruct, "must have struct");
  if (nsnull == aStruct) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult result = NS_OK;
  switch (aProperty) {
    case PROP_CURSOR:
      CSS_ENSURE(Color) {
        CSS_IF_DELETE(mColor->mCursor);
        mColor->mCursor = (nsCSSValueList*)aStruct;
      }
      break;

    case PROP_TEXT_SHADOW:
      CSS_ENSURE(Text) {
        CSS_IF_DELETE(mText->mTextShadow);
        mText->mTextShadow = (nsCSSShadow*)aStruct;
      }
      break;

    case PROP_CONTENT:
      CSS_ENSURE(Content) {
        CSS_IF_DELETE(mContent->mContent);
        mContent->mContent = (nsCSSValueList*)aStruct;
      }
      break;

    case PROP_COUNTER_INCREMENT:
      CSS_ENSURE(Content) {
        CSS_IF_DELETE(mContent->mCounterIncrement);
        mContent->mCounterIncrement = (nsCSSCounterData*)aStruct;
      }
      break;

    case PROP_COUNTER_RESET:
      CSS_ENSURE(Content) {
        CSS_IF_DELETE(mContent->mCounterReset);
        mContent->mCounterReset = (nsCSSCounterData*)aStruct;
      }
      break;

    case PROP_QUOTES:
      CSS_ENSURE(Content) {
        CSS_IF_DELETE(mContent->mQuotes);
        mContent->mQuotes = (nsCSSQuotes*)aStruct;
      }
      break;

    default:
      NS_ERROR("not a struct property");
      result = NS_ERROR_ILLEGAL_VALUE;
      break;
  }

  if (NS_OK == result) {
    if (nsnull == mOrder) {
      mOrder = new nsVoidArray();
    }
    if (nsnull != mOrder) {
      PRInt32 index = mOrder->IndexOf((void*)aProperty);
      if (-1 != index) {
        mOrder->RemoveElementAt(index);
      }
      mOrder->AppendElement((void*)aProperty);
    }
  }
  return result;
}


#define CSS_ENSURE_IMPORTANT(data)            \
  if (nsnull == mImportant->m##data) {        \
    mImportant->m##data = new nsCSS##data();  \
  }                                           \
  if (nsnull == mImportant->m##data) {        \
    result = NS_ERROR_OUT_OF_MEMORY;          \
  }                                           \
  else

#define CSS_CASE_IMPORTANT(prop,data) \
  case prop: mImportant->data = data; data.Reset(); break

NS_IMETHODIMP
CSSDeclarationImpl::SetValueImportant(PRInt32 aProperty)
{
  nsresult result = NS_OK;

  if (nsnull == mImportant) {
    mImportant = new CSSDeclarationImpl();
    if (nsnull != mImportant) {
      NS_ADDREF(mImportant);
    }
    else {
      result = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  if (NS_OK == result) {
    switch (aProperty) {
      // nsCSSFont
      case PROP_FONT_FAMILY:
      case PROP_FONT_STYLE:
      case PROP_FONT_VARIANT:
      case PROP_FONT_WEIGHT:
      case PROP_FONT_SIZE:
      case PROP_FONT_SIZE_ADJUST:
      case PROP_FONT_STRETCH:
        if (nsnull != mFont) {
          CSS_ENSURE_IMPORTANT(Font) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(PROP_FONT_FAMILY, mFont->mFamily);
              CSS_CASE_IMPORTANT(PROP_FONT_STYLE, mFont->mStyle);
              CSS_CASE_IMPORTANT(PROP_FONT_VARIANT, mFont->mVariant);
              CSS_CASE_IMPORTANT(PROP_FONT_WEIGHT, mFont->mWeight);
              CSS_CASE_IMPORTANT(PROP_FONT_SIZE, mFont->mSize);
              CSS_CASE_IMPORTANT(PROP_FONT_SIZE_ADJUST, mFont->mSizeAdjust);
              CSS_CASE_IMPORTANT(PROP_FONT_STRETCH, mFont->mStretch);
            }
          }
        }
        break;

      // nsCSSColor
      case PROP_COLOR:
      case PROP_BACKGROUND_COLOR:
      case PROP_BACKGROUND_IMAGE:
      case PROP_BACKGROUND_REPEAT:
      case PROP_BACKGROUND_ATTACHMENT:
      case PROP_BACKGROUND_X_POSITION:
      case PROP_BACKGROUND_Y_POSITION:
      case PROP_BACKGROUND_FILTER:
      case PROP_OPACITY:
        if (nsnull != mColor) {
          CSS_ENSURE_IMPORTANT(Color) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(PROP_COLOR,                  mColor->mColor);
              CSS_CASE_IMPORTANT(PROP_BACKGROUND_COLOR,       mColor->mBackColor);
              CSS_CASE_IMPORTANT(PROP_BACKGROUND_IMAGE,       mColor->mBackImage);
              CSS_CASE_IMPORTANT(PROP_BACKGROUND_REPEAT,      mColor->mBackRepeat);
              CSS_CASE_IMPORTANT(PROP_BACKGROUND_ATTACHMENT,  mColor->mBackAttachment);
              CSS_CASE_IMPORTANT(PROP_BACKGROUND_X_POSITION,  mColor->mBackPositionX);
              CSS_CASE_IMPORTANT(PROP_BACKGROUND_Y_POSITION,  mColor->mBackPositionY);
              CSS_CASE_IMPORTANT(PROP_BACKGROUND_FILTER,      mColor->mBackFilter);
              CSS_CASE_IMPORTANT(PROP_OPACITY,                mColor->mOpacity);
            }
          }
        }
        break;

      case PROP_CURSOR:
        if (nsnull != mColor) {
          if (nsnull != mColor->mCursor) {
            CSS_ENSURE_IMPORTANT(Color) {
              CSS_IF_DELETE(mImportant->mColor->mCursor);
              mImportant->mColor->mCursor = mColor->mCursor;
              mColor->mCursor = nsnull;
            }
          }
        }
        break;

      // nsCSSText
      case PROP_WORD_SPACING:
      case PROP_LETTER_SPACING:
      case PROP_TEXT_DECORATION:
      case PROP_VERTICAL_ALIGN:
      case PROP_TEXT_TRANSFORM:
      case PROP_TEXT_ALIGN:
      case PROP_TEXT_INDENT:
      case PROP_UNICODE_BIDI:
      case PROP_LINE_HEIGHT:
      case PROP_WHITE_SPACE:
        if (nsnull != mText) {
          CSS_ENSURE_IMPORTANT(Text) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(PROP_WORD_SPACING,     mText->mWordSpacing);
              CSS_CASE_IMPORTANT(PROP_LETTER_SPACING,   mText->mLetterSpacing);
              CSS_CASE_IMPORTANT(PROP_TEXT_DECORATION,  mText->mDecoration);
              CSS_CASE_IMPORTANT(PROP_VERTICAL_ALIGN,   mText->mVerticalAlign);
              CSS_CASE_IMPORTANT(PROP_TEXT_TRANSFORM,   mText->mTextTransform);
              CSS_CASE_IMPORTANT(PROP_TEXT_ALIGN,       mText->mTextAlign);
              CSS_CASE_IMPORTANT(PROP_TEXT_INDENT,      mText->mTextIndent);
              CSS_CASE_IMPORTANT(PROP_UNICODE_BIDI,     mText->mUnicodeBidi);
              CSS_CASE_IMPORTANT(PROP_LINE_HEIGHT,      mText->mLineHeight);
              CSS_CASE_IMPORTANT(PROP_WHITE_SPACE,      mText->mWhiteSpace);
            }
          }
        }
        break;

      case PROP_TEXT_SHADOW:
        if (nsnull != mText) {
          if (nsnull != mText->mTextShadow) {
            CSS_ENSURE_IMPORTANT(Text) {
              CSS_IF_DELETE(mImportant->mText->mTextShadow);
              mImportant->mText->mTextShadow = mText->mTextShadow;
              mText->mTextShadow = nsnull;
            }
          }
        }
        break;

        // nsCSSDisplay
      case PROP_DIRECTION:
      case PROP_DISPLAY:
      case PROP_FLOAT:
      case PROP_CLEAR:
      case PROP_OVERFLOW:
      case PROP_VISIBILITY:
      case PROP_FILTER:
        if (nsnull != mDisplay) {
          CSS_ENSURE_IMPORTANT(Display) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(PROP_DIRECTION,  mDisplay->mDirection);
              CSS_CASE_IMPORTANT(PROP_DISPLAY,    mDisplay->mDisplay);
              CSS_CASE_IMPORTANT(PROP_FLOAT,      mDisplay->mFloat);
              CSS_CASE_IMPORTANT(PROP_CLEAR,      mDisplay->mClear);
              CSS_CASE_IMPORTANT(PROP_OVERFLOW,   mDisplay->mOverflow);
              CSS_CASE_IMPORTANT(PROP_VISIBILITY, mDisplay->mVisibility);
              CSS_CASE_IMPORTANT(PROP_FILTER,     mDisplay->mFilter);
            }
          }
        }
        break;

      case PROP_CLIP_TOP:
      case PROP_CLIP_RIGHT:
      case PROP_CLIP_BOTTOM:
      case PROP_CLIP_LEFT:
        if (nsnull != mDisplay) {
          if (nsnull != mDisplay->mClip) {
            CSS_ENSURE_IMPORTANT(Display) {
              CSS_ENSURE_RECT(mImportant->mDisplay->mClip) {
                switch(aProperty) {
                  CSS_CASE_IMPORTANT(PROP_CLIP_TOP,     mDisplay->mClip->mTop);
                  CSS_CASE_IMPORTANT(PROP_CLIP_RIGHT,   mDisplay->mClip->mRight);
                  CSS_CASE_IMPORTANT(PROP_CLIP_BOTTOM,  mDisplay->mClip->mBottom);
                  CSS_CASE_IMPORTANT(PROP_CLIP_LEFT,    mDisplay->mClip->mLeft);
                }
              }
            }
          }
        }
        break;

      // nsCSSMargin
      case PROP_MARGIN_TOP:
      case PROP_MARGIN_RIGHT:
      case PROP_MARGIN_BOTTOM:
      case PROP_MARGIN_LEFT:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mMargin) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(mImportant->mMargin->mMargin) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(PROP_MARGIN_TOP,     mMargin->mMargin->mTop);
                  CSS_CASE_IMPORTANT(PROP_MARGIN_RIGHT,   mMargin->mMargin->mRight);
                  CSS_CASE_IMPORTANT(PROP_MARGIN_BOTTOM,  mMargin->mMargin->mBottom);
                  CSS_CASE_IMPORTANT(PROP_MARGIN_LEFT,    mMargin->mMargin->mLeft);
                }
              }
            }
          }
        }
        break;

      case PROP_PADDING_TOP:
      case PROP_PADDING_RIGHT:
      case PROP_PADDING_BOTTOM:
      case PROP_PADDING_LEFT:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mPadding) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(mImportant->mMargin->mPadding) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(PROP_PADDING_TOP,    mMargin->mPadding->mTop);
                  CSS_CASE_IMPORTANT(PROP_PADDING_RIGHT,  mMargin->mPadding->mRight);
                  CSS_CASE_IMPORTANT(PROP_PADDING_BOTTOM, mMargin->mPadding->mBottom);
                  CSS_CASE_IMPORTANT(PROP_PADDING_LEFT,   mMargin->mPadding->mLeft);
                }
              }
            }
          }
        }
        break;

      case PROP_BORDER_TOP_WIDTH:
      case PROP_BORDER_RIGHT_WIDTH:
      case PROP_BORDER_BOTTOM_WIDTH:
      case PROP_BORDER_LEFT_WIDTH:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mBorderWidth) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(mImportant->mMargin->mBorderWidth) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(PROP_BORDER_TOP_WIDTH,     mMargin->mBorderWidth->mTop);
                  CSS_CASE_IMPORTANT(PROP_BORDER_RIGHT_WIDTH,   mMargin->mBorderWidth->mRight);
                  CSS_CASE_IMPORTANT(PROP_BORDER_BOTTOM_WIDTH,  mMargin->mBorderWidth->mBottom);
                  CSS_CASE_IMPORTANT(PROP_BORDER_LEFT_WIDTH,    mMargin->mBorderWidth->mLeft);
                }
              }
            }
          }
        }
        break;

      case PROP_BORDER_TOP_COLOR:
      case PROP_BORDER_RIGHT_COLOR:
      case PROP_BORDER_BOTTOM_COLOR:
      case PROP_BORDER_LEFT_COLOR:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mBorderColor) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(mImportant->mMargin->mBorderColor) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(PROP_BORDER_TOP_COLOR,     mMargin->mBorderColor->mTop);
                  CSS_CASE_IMPORTANT(PROP_BORDER_RIGHT_COLOR,   mMargin->mBorderColor->mRight);
                  CSS_CASE_IMPORTANT(PROP_BORDER_BOTTOM_COLOR,  mMargin->mBorderColor->mBottom);
                  CSS_CASE_IMPORTANT(PROP_BORDER_LEFT_COLOR,    mMargin->mBorderColor->mLeft);
                }
              }
            }
          }
        }
        break;

      case PROP_BORDER_TOP_STYLE:
      case PROP_BORDER_RIGHT_STYLE:
      case PROP_BORDER_BOTTOM_STYLE:
      case PROP_BORDER_LEFT_STYLE:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mBorderStyle) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(mImportant->mMargin->mBorderStyle) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(PROP_BORDER_TOP_STYLE,     mMargin->mBorderStyle->mTop);
                  CSS_CASE_IMPORTANT(PROP_BORDER_RIGHT_STYLE,   mMargin->mBorderStyle->mRight);
                  CSS_CASE_IMPORTANT(PROP_BORDER_BOTTOM_STYLE,  mMargin->mBorderStyle->mBottom);
                  CSS_CASE_IMPORTANT(PROP_BORDER_LEFT_STYLE,    mMargin->mBorderStyle->mLeft);
                }
              }
            }
          }
        }
        break;

      case PROP__MOZ_BORDER_RADIUS:
      case PROP_OUTLINE_WIDTH:
      case PROP_OUTLINE_COLOR:
      case PROP_OUTLINE_STYLE:
        if (nsnull != mMargin) {
          CSS_ENSURE_IMPORTANT(Margin) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(PROP__MOZ_BORDER_RADIUS, mMargin->mBorderRadius);
              CSS_CASE_IMPORTANT(PROP_OUTLINE_WIDTH,      mMargin->mOutlineWidth);
              CSS_CASE_IMPORTANT(PROP_OUTLINE_COLOR,      mMargin->mOutlineColor);
              CSS_CASE_IMPORTANT(PROP_OUTLINE_STYLE,      mMargin->mOutlineStyle);
            }
          }
        }
        break;

      // nsCSSPosition
      case PROP_POSITION:
      case PROP_WIDTH:
      case PROP_MIN_WIDTH:
      case PROP_MAX_WIDTH:
      case PROP_HEIGHT:
      case PROP_MIN_HEIGHT:
      case PROP_MAX_HEIGHT:
      case PROP_Z_INDEX:
        if (nsnull != mPosition) {
          CSS_ENSURE_IMPORTANT(Position) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(PROP_POSITION,   mPosition->mPosition);
              CSS_CASE_IMPORTANT(PROP_WIDTH,      mPosition->mWidth);
              CSS_CASE_IMPORTANT(PROP_MIN_WIDTH,  mPosition->mMinWidth);
              CSS_CASE_IMPORTANT(PROP_MAX_WIDTH,  mPosition->mMaxWidth);
              CSS_CASE_IMPORTANT(PROP_HEIGHT,     mPosition->mHeight);
              CSS_CASE_IMPORTANT(PROP_MIN_HEIGHT, mPosition->mMinHeight);
              CSS_CASE_IMPORTANT(PROP_MAX_HEIGHT, mPosition->mMaxHeight);
              CSS_CASE_IMPORTANT(PROP_Z_INDEX,    mPosition->mZIndex);
            }
          }
        }
        break;

      case PROP_TOP:
      case PROP_RIGHT:
      case PROP_BOTTOM:
      case PROP_LEFT:
        if (nsnull != mPosition) {
          if (nsnull != mPosition->mOffset) {
            CSS_ENSURE_IMPORTANT(Position) {
              CSS_ENSURE_RECT(mImportant->mPosition->mOffset) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(PROP_TOP,    mPosition->mOffset->mTop);
                  CSS_CASE_IMPORTANT(PROP_RIGHT,  mPosition->mOffset->mRight);
                  CSS_CASE_IMPORTANT(PROP_BOTTOM, mPosition->mOffset->mBottom);
                  CSS_CASE_IMPORTANT(PROP_LEFT,   mPosition->mOffset->mLeft);
                }
              }
            }
          }
        }
        break;

        // nsCSSList
      case PROP_LIST_STYLE_TYPE:
      case PROP_LIST_STYLE_IMAGE:
      case PROP_LIST_STYLE_POSITION:
        if (nsnull != mList) {
          CSS_ENSURE_IMPORTANT(List) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(PROP_LIST_STYLE_TYPE,      mList->mType);
              CSS_CASE_IMPORTANT(PROP_LIST_STYLE_IMAGE,     mList->mImage);
              CSS_CASE_IMPORTANT(PROP_LIST_STYLE_POSITION,  mList->mPosition);
            }
          }
        }
        break;

        // nsCSSTable
      case PROP_BORDER_COLLAPSE:
      case PROP_BORDER_X_SPACING:
      case PROP_BORDER_Y_SPACING:
      case PROP_CAPTION_SIDE:
      case PROP_EMPTY_CELLS:
      case PROP_TABLE_LAYOUT:
        if (nsnull != mTable) {
          CSS_ENSURE_IMPORTANT(Table) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(PROP_BORDER_COLLAPSE,  mTable->mBorderCollapse);
              CSS_CASE_IMPORTANT(PROP_BORDER_X_SPACING, mTable->mBorderSpacingX);
              CSS_CASE_IMPORTANT(PROP_BORDER_Y_SPACING, mTable->mBorderSpacingY);
              CSS_CASE_IMPORTANT(PROP_CAPTION_SIDE,     mTable->mCaptionSide);
              CSS_CASE_IMPORTANT(PROP_EMPTY_CELLS,      mTable->mEmptyCells);
              CSS_CASE_IMPORTANT(PROP_TABLE_LAYOUT,     mTable->mLayout);
            }
          }
        }
        break;

        // nsCSSBreaks
      case PROP_ORPHANS:
      case PROP_WIDOWS:
      case PROP_PAGE:
      case PROP_PAGE_BREAK_AFTER:
      case PROP_PAGE_BREAK_BEFORE:
      case PROP_PAGE_BREAK_INSIDE:
        if (nsnull != mBreaks) {
          CSS_ENSURE_IMPORTANT(Breaks) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(PROP_ORPHANS,            mBreaks->mOrphans);
              CSS_CASE_IMPORTANT(PROP_WIDOWS,             mBreaks->mWidows);
              CSS_CASE_IMPORTANT(PROP_PAGE,               mBreaks->mPage);
              CSS_CASE_IMPORTANT(PROP_PAGE_BREAK_AFTER,   mBreaks->mPageBreakAfter);
              CSS_CASE_IMPORTANT(PROP_PAGE_BREAK_BEFORE,  mBreaks->mPageBreakBefore);
              CSS_CASE_IMPORTANT(PROP_PAGE_BREAK_INSIDE,  mBreaks->mPageBreakInside);
            }
          }
        }
        break;

        // nsCSSPage
      case PROP_MARKS:
      case PROP_SIZE_WIDTH:
      case PROP_SIZE_HEIGHT:
        if (nsnull != mPage) {
          CSS_ENSURE_IMPORTANT(Page) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(PROP_MARKS,        mPage->mMarks);
              CSS_CASE_IMPORTANT(PROP_SIZE_WIDTH,   mPage->mSizeWidth);
              CSS_CASE_IMPORTANT(PROP_SIZE_HEIGHT,  mPage->mSizeHeight);
            }
          }
        }
        break;

        // nsCSSContent
      case PROP_CONTENT:
        if (nsnull != mContent) {
          if (nsnull != mContent->mContent) {
            CSS_ENSURE_IMPORTANT(Content) {
              CSS_IF_DELETE(mImportant->mContent->mContent);
              mImportant->mContent->mContent = mContent->mContent;
              mContent->mContent = nsnull;
            }
          }
        }
        break;

      case PROP_COUNTER_INCREMENT:
        if (nsnull != mContent) {
          if (nsnull != mContent->mCounterIncrement) {
            CSS_ENSURE_IMPORTANT(Content) {
              CSS_IF_DELETE(mImportant->mContent->mCounterIncrement);
              mImportant->mContent->mCounterIncrement = mContent->mCounterIncrement;
              mContent->mCounterIncrement = nsnull;
            }
          }
        }
        break;

      case PROP_COUNTER_RESET:
        if (nsnull != mContent) {
          if (nsnull != mContent->mCounterReset) {
            CSS_ENSURE_IMPORTANT(Content) {
              CSS_IF_DELETE(mImportant->mContent->mCounterReset);
              mImportant->mContent->mCounterReset = mContent->mCounterReset;
              mContent->mCounterReset = nsnull;
            }
          }
        }
        break;

      case PROP_MARKER_OFFSET:
        if (nsnull != mContent) {
          CSS_ENSURE_IMPORTANT(Content) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(PROP_MARKER_OFFSET,  mContent->mMarkerOffset);
            }
          }
        }
        break;

      case PROP_QUOTES:
        if (nsnull != mContent) {
          if (nsnull != mContent->mQuotes) {
            CSS_ENSURE_IMPORTANT(Content) {
              CSS_IF_DELETE(mImportant->mContent->mQuotes);
              mImportant->mContent->mQuotes = mContent->mQuotes;
              mContent->mQuotes = nsnull;
            }
          }
        }
        break;

        // nsCSSAural
      case PROP_AZIMUTH:
      case PROP_ELEVATION:
      case PROP_CUE_AFTER:
      case PROP_CUE_BEFORE:
      case PROP_PAUSE_AFTER:
      case PROP_PAUSE_BEFORE:
      case PROP_PITCH:
      case PROP_PITCH_RANGE:
      case PROP_RICHNESS:
      case PROP_SPEAK:
      case PROP_SPEAK_HEADER:
      case PROP_SPEAK_NUMERAL:
      case PROP_SPEAK_PUNCTUATION:
      case PROP_SPEECH_RATE:
      case PROP_STRESS:
      case PROP_VOICE_FAMILY:
      case PROP_VOLUME:
        if (nsnull != mAural) {
          CSS_ENSURE_IMPORTANT(Aural) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(PROP_AZIMUTH,            mAural->mAzimuth);
              CSS_CASE_IMPORTANT(PROP_ELEVATION,          mAural->mElevation);
              CSS_CASE_IMPORTANT(PROP_CUE_AFTER,          mAural->mCueAfter);
              CSS_CASE_IMPORTANT(PROP_CUE_BEFORE,         mAural->mCueBefore);
              CSS_CASE_IMPORTANT(PROP_PAUSE_AFTER,        mAural->mPauseAfter);
              CSS_CASE_IMPORTANT(PROP_PAUSE_BEFORE,       mAural->mPauseBefore);
              CSS_CASE_IMPORTANT(PROP_PITCH,              mAural->mPitch);
              CSS_CASE_IMPORTANT(PROP_PITCH_RANGE,        mAural->mPitchRange);
              CSS_CASE_IMPORTANT(PROP_RICHNESS,           mAural->mRichness);
              CSS_CASE_IMPORTANT(PROP_SPEAK,              mAural->mSpeak);
              CSS_CASE_IMPORTANT(PROP_SPEAK_HEADER,       mAural->mSpeakHeader);
              CSS_CASE_IMPORTANT(PROP_SPEAK_NUMERAL,      mAural->mSpeakNumeral);
              CSS_CASE_IMPORTANT(PROP_SPEAK_PUNCTUATION,  mAural->mSpeakPunctuation);
              CSS_CASE_IMPORTANT(PROP_SPEECH_RATE,        mAural->mSpeechRate);
              CSS_CASE_IMPORTANT(PROP_STRESS,             mAural->mStress);
              CSS_CASE_IMPORTANT(PROP_VOICE_FAMILY,       mAural->mVoiceFamily);
              CSS_CASE_IMPORTANT(PROP_VOLUME,             mAural->mVolume);
            }
          }
        }
        break;

      case PROP_PLAY_DURING:
        if (nsnull != mAural) {
          CSS_ENSURE_IMPORTANT(Aural) {
            mImportant->mAural->mPlayDuring = mAural->mPlayDuring;
            mAural->mPlayDuring.Reset();
            mImportant->mAural->mPlayDuringFlags = mAural->mPlayDuringFlags;
            mAural->mPlayDuringFlags.Reset();
          }
        }
        break;

        // Shorthands
      case PROP_BACKGROUND:
        SetValueImportant(PROP_BACKGROUND_COLOR);
        SetValueImportant(PROP_BACKGROUND_IMAGE);
        SetValueImportant(PROP_BACKGROUND_REPEAT);
        SetValueImportant(PROP_BACKGROUND_ATTACHMENT);
        SetValueImportant(PROP_BACKGROUND_X_POSITION);
        SetValueImportant(PROP_BACKGROUND_Y_POSITION);
        SetValueImportant(PROP_BACKGROUND_FILTER);
        break;
      case PROP_BORDER:
        SetValueImportant(PROP_BORDER_TOP_WIDTH);
        SetValueImportant(PROP_BORDER_RIGHT_WIDTH);
        SetValueImportant(PROP_BORDER_BOTTOM_WIDTH);
        SetValueImportant(PROP_BORDER_LEFT_WIDTH);
        SetValueImportant(PROP_BORDER_TOP_STYLE);
        SetValueImportant(PROP_BORDER_RIGHT_STYLE);
        SetValueImportant(PROP_BORDER_BOTTOM_STYLE);
        SetValueImportant(PROP_BORDER_LEFT_STYLE);
        SetValueImportant(PROP_BORDER_TOP_COLOR);
        SetValueImportant(PROP_BORDER_RIGHT_COLOR);
        SetValueImportant(PROP_BORDER_BOTTOM_COLOR);
        SetValueImportant(PROP_BORDER_LEFT_COLOR);
        break;
      case PROP_BORDER_SPACING:
        SetValueImportant(PROP_BORDER_X_SPACING);
        SetValueImportant(PROP_BORDER_Y_SPACING);
      case PROP_CLIP:
        SetValueImportant(PROP_CLIP_TOP);
        SetValueImportant(PROP_CLIP_RIGHT);
        SetValueImportant(PROP_CLIP_BOTTOM);
        SetValueImportant(PROP_CLIP_LEFT);
        break;
      case PROP_CUE:
        SetValueImportant(PROP_CUE_AFTER);
        SetValueImportant(PROP_CUE_BEFORE);
        break;
      case PROP_FONT:
        SetValueImportant(PROP_FONT_FAMILY);
        SetValueImportant(PROP_FONT_STYLE);
        SetValueImportant(PROP_FONT_VARIANT);
        SetValueImportant(PROP_FONT_WEIGHT);
        SetValueImportant(PROP_FONT_SIZE);
        SetValueImportant(PROP_LINE_HEIGHT);
        break;
      case PROP_LIST_STYLE:
        SetValueImportant(PROP_LIST_STYLE_TYPE);
        SetValueImportant(PROP_LIST_STYLE_IMAGE);
        SetValueImportant(PROP_LIST_STYLE_POSITION);
        break;
      case PROP_MARGIN:
        SetValueImportant(PROP_MARGIN_TOP);
        SetValueImportant(PROP_MARGIN_RIGHT);
        SetValueImportant(PROP_MARGIN_BOTTOM);
        SetValueImportant(PROP_MARGIN_LEFT);
        break;
      case PROP_OUTLINE:
        SetValueImportant(PROP_OUTLINE_COLOR);
        SetValueImportant(PROP_OUTLINE_STYLE);
        SetValueImportant(PROP_OUTLINE_WIDTH);
        break;
      case PROP_PADDING:
        SetValueImportant(PROP_PADDING_TOP);
        SetValueImportant(PROP_PADDING_RIGHT);
        SetValueImportant(PROP_PADDING_BOTTOM);
        SetValueImportant(PROP_PADDING_LEFT);
        break;
      case PROP_PAUSE:
        SetValueImportant(PROP_PAUSE_AFTER);
        SetValueImportant(PROP_PAUSE_BEFORE);
        break;
      case PROP_SIZE:
        SetValueImportant(PROP_SIZE_WIDTH);
        SetValueImportant(PROP_SIZE_HEIGHT);
        break;
      case PROP_BACKGROUND_POSITION:
        SetValueImportant(PROP_BACKGROUND_X_POSITION);
        SetValueImportant(PROP_BACKGROUND_Y_POSITION);
        break;
      case PROP_BORDER_TOP:
        SetValueImportant(PROP_BORDER_TOP_WIDTH);
        SetValueImportant(PROP_BORDER_TOP_STYLE);
        SetValueImportant(PROP_BORDER_TOP_COLOR);
        break;
      case PROP_BORDER_RIGHT:
        SetValueImportant(PROP_BORDER_RIGHT_WIDTH);
        SetValueImportant(PROP_BORDER_RIGHT_STYLE);
        SetValueImportant(PROP_BORDER_RIGHT_COLOR);
        break;
      case PROP_BORDER_BOTTOM:
        SetValueImportant(PROP_BORDER_BOTTOM_WIDTH);
        SetValueImportant(PROP_BORDER_BOTTOM_STYLE);
        SetValueImportant(PROP_BORDER_BOTTOM_COLOR);
        break;
      case PROP_BORDER_LEFT:
        SetValueImportant(PROP_BORDER_LEFT_WIDTH);
        SetValueImportant(PROP_BORDER_LEFT_STYLE);
        SetValueImportant(PROP_BORDER_LEFT_COLOR);
        break;
      case PROP_BORDER_COLOR:
        SetValueImportant(PROP_BORDER_TOP_COLOR);
        SetValueImportant(PROP_BORDER_RIGHT_COLOR);
        SetValueImportant(PROP_BORDER_BOTTOM_COLOR);
        SetValueImportant(PROP_BORDER_LEFT_COLOR);
        break;
      case PROP_BORDER_STYLE:
        SetValueImportant(PROP_BORDER_TOP_STYLE);
        SetValueImportant(PROP_BORDER_RIGHT_STYLE);
        SetValueImportant(PROP_BORDER_BOTTOM_STYLE);
        SetValueImportant(PROP_BORDER_LEFT_STYLE);
        break;
      case PROP_BORDER_WIDTH:
        SetValueImportant(PROP_BORDER_TOP_WIDTH);
        SetValueImportant(PROP_BORDER_RIGHT_WIDTH);
        SetValueImportant(PROP_BORDER_BOTTOM_WIDTH);
        SetValueImportant(PROP_BORDER_LEFT_WIDTH);
        break;
      default:
        result = NS_ERROR_ILLEGAL_VALUE;
        break;
    }
  }
  return result;
}

NS_IMETHODIMP
CSSDeclarationImpl::AppendComment(const nsString& aComment)
{
  nsresult result = NS_ERROR_OUT_OF_MEMORY;

  if (nsnull == mOrder) {
    mOrder = new nsVoidArray();
  }
  if (nsnull == mComments) {
    mComments = new nsVoidArray();
  }
  if ((nsnull != mComments) && (nsnull != mOrder)) {
    mComments->AppendElement(new nsString(aComment));
    mOrder->AppendElement((void*)-mComments->Count());
    result = NS_OK;
  }
  return result;
}

NS_IMETHODIMP
CSSDeclarationImpl::GetValue(PRInt32 aProperty, nsCSSValue& aValue)
{
  nsresult result = NS_OK;

  switch (aProperty) {
    // nsCSSFont
    case PROP_FONT_FAMILY:
    case PROP_FONT_STYLE:
    case PROP_FONT_VARIANT:
    case PROP_FONT_WEIGHT:
    case PROP_FONT_SIZE:
    case PROP_FONT_SIZE_ADJUST:
    case PROP_FONT_STRETCH:
      if (nsnull != mFont) {
        switch (aProperty) {
          case PROP_FONT_FAMILY:      aValue = mFont->mFamily;      break;
          case PROP_FONT_STYLE:       aValue = mFont->mStyle;       break;
          case PROP_FONT_VARIANT:     aValue = mFont->mVariant;     break;
          case PROP_FONT_WEIGHT:      aValue = mFont->mWeight;      break;
          case PROP_FONT_SIZE:        aValue = mFont->mSize;        break;
          case PROP_FONT_SIZE_ADJUST: aValue = mFont->mSizeAdjust;  break;
          case PROP_FONT_STRETCH:     aValue = mFont->mStretch;     break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSColor
    case PROP_COLOR:
    case PROP_BACKGROUND_COLOR:
    case PROP_BACKGROUND_IMAGE:
    case PROP_BACKGROUND_REPEAT:
    case PROP_BACKGROUND_ATTACHMENT:
    case PROP_BACKGROUND_X_POSITION:
    case PROP_BACKGROUND_Y_POSITION:
    case PROP_BACKGROUND_FILTER:
    case PROP_CURSOR:
    case PROP_OPACITY:
      if (nsnull != mColor) {
        switch (aProperty) {
          case PROP_COLOR:                  aValue = mColor->mColor;           break;
          case PROP_BACKGROUND_COLOR:       aValue = mColor->mBackColor;       break;
          case PROP_BACKGROUND_IMAGE:       aValue = mColor->mBackImage;       break;
          case PROP_BACKGROUND_REPEAT:      aValue = mColor->mBackRepeat;      break;
          case PROP_BACKGROUND_ATTACHMENT:  aValue = mColor->mBackAttachment;  break;
          case PROP_BACKGROUND_X_POSITION:  aValue = mColor->mBackPositionX;   break;
          case PROP_BACKGROUND_Y_POSITION:  aValue = mColor->mBackPositionY;   break;
          case PROP_BACKGROUND_FILTER:      aValue = mColor->mBackFilter;      break;
          case PROP_CURSOR:
            if (nsnull != mColor->mCursor) {
              aValue = mColor->mCursor->mValue;
            }
            break;
          case PROP_OPACITY:                aValue = mColor->mOpacity;         break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSText
    case PROP_WORD_SPACING:
    case PROP_LETTER_SPACING:
    case PROP_TEXT_DECORATION:
    case PROP_VERTICAL_ALIGN:
    case PROP_TEXT_TRANSFORM:
    case PROP_TEXT_ALIGN:
    case PROP_TEXT_INDENT:
    case PROP_UNICODE_BIDI:
    case PROP_LINE_HEIGHT:
    case PROP_WHITE_SPACE:
      if (nsnull != mText) {
        switch (aProperty) {
          case PROP_WORD_SPACING:     aValue = mText->mWordSpacing;    break;
          case PROP_LETTER_SPACING:   aValue = mText->mLetterSpacing;  break;
          case PROP_TEXT_DECORATION:  aValue = mText->mDecoration;     break;
          case PROP_VERTICAL_ALIGN:   aValue = mText->mVerticalAlign;  break;
          case PROP_TEXT_TRANSFORM:   aValue = mText->mTextTransform;  break;
          case PROP_TEXT_ALIGN:       aValue = mText->mTextAlign;      break;
          case PROP_TEXT_INDENT:      aValue = mText->mTextIndent;     break;
          case PROP_UNICODE_BIDI:     aValue = mText->mUnicodeBidi;    break;
          case PROP_LINE_HEIGHT:      aValue = mText->mLineHeight;     break;
          case PROP_WHITE_SPACE:      aValue = mText->mWhiteSpace;     break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_TEXT_SHADOW_COLOR:
    case PROP_TEXT_SHADOW_X:
    case PROP_TEXT_SHADOW_Y:
    case PROP_TEXT_SHADOW_RADIUS:
      if ((nsnull != mText) && (nsnull != mText->mTextShadow)) {
        switch (aProperty) {
          case PROP_TEXT_SHADOW_COLOR:  aValue = mText->mTextShadow->mColor;    break;
          case PROP_TEXT_SHADOW_X:      aValue = mText->mTextShadow->mXOffset;  break;
          case PROP_TEXT_SHADOW_Y:      aValue = mText->mTextShadow->mYOffset;  break;
          case PROP_TEXT_SHADOW_RADIUS: aValue = mText->mTextShadow->mRadius;   break;
        }
      }
      break;

      // nsCSSDisplay
    case PROP_FLOAT:
    case PROP_CLEAR:
    case PROP_DISPLAY:
    case PROP_DIRECTION:
    case PROP_VISIBILITY:
    case PROP_OVERFLOW:
    case PROP_FILTER:
      if (nsnull != mDisplay) {
        switch (aProperty) {
          case PROP_FLOAT:      aValue = mDisplay->mFloat;      break;
          case PROP_CLEAR:      aValue = mDisplay->mClear;      break;
          case PROP_DISPLAY:    aValue = mDisplay->mDisplay;    break;
          case PROP_DIRECTION:  aValue = mDisplay->mDirection;  break;
          case PROP_VISIBILITY: aValue = mDisplay->mVisibility; break;
          case PROP_OVERFLOW:   aValue = mDisplay->mOverflow;   break;
          case PROP_FILTER:     aValue = mDisplay->mFilter;     break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_CLIP_TOP:
    case PROP_CLIP_RIGHT:
    case PROP_CLIP_BOTTOM:
    case PROP_CLIP_LEFT:
      if ((nsnull != mDisplay) && (nsnull != mDisplay->mClip)) {
        switch(aProperty) {
          case PROP_CLIP_TOP:     aValue = mDisplay->mClip->mTop;     break;
          case PROP_CLIP_RIGHT:   aValue = mDisplay->mClip->mRight;   break;
          case PROP_CLIP_BOTTOM:  aValue = mDisplay->mClip->mBottom;  break;
          case PROP_CLIP_LEFT:    aValue = mDisplay->mClip->mLeft;    break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSMargin
    case PROP_MARGIN_TOP:
    case PROP_MARGIN_RIGHT:
    case PROP_MARGIN_BOTTOM:
    case PROP_MARGIN_LEFT:
      if ((nsnull != mMargin) && (nsnull != mMargin->mMargin)) {
        switch (aProperty) {
          case PROP_MARGIN_TOP:     aValue = mMargin->mMargin->mTop;     break;
          case PROP_MARGIN_RIGHT:   aValue = mMargin->mMargin->mRight;   break;
          case PROP_MARGIN_BOTTOM:  aValue = mMargin->mMargin->mBottom;  break;
          case PROP_MARGIN_LEFT:    aValue = mMargin->mMargin->mLeft;    break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_PADDING_TOP:
    case PROP_PADDING_RIGHT:
    case PROP_PADDING_BOTTOM:
    case PROP_PADDING_LEFT:
      if ((nsnull != mMargin) && (nsnull != mMargin->mPadding)) {
        switch (aProperty) {
          case PROP_PADDING_TOP:    aValue = mMargin->mPadding->mTop;    break;
          case PROP_PADDING_RIGHT:  aValue = mMargin->mPadding->mRight;  break;
          case PROP_PADDING_BOTTOM: aValue = mMargin->mPadding->mBottom; break;
          case PROP_PADDING_LEFT:   aValue = mMargin->mPadding->mLeft;   break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_BORDER_TOP_WIDTH:
    case PROP_BORDER_RIGHT_WIDTH:
    case PROP_BORDER_BOTTOM_WIDTH:
    case PROP_BORDER_LEFT_WIDTH:
      if ((nsnull != mMargin) && (nsnull != mMargin->mBorderWidth)) {
        switch (aProperty) {
          case PROP_BORDER_TOP_WIDTH:     aValue = mMargin->mBorderWidth->mTop;     break;
          case PROP_BORDER_RIGHT_WIDTH:   aValue = mMargin->mBorderWidth->mRight;   break;
          case PROP_BORDER_BOTTOM_WIDTH:  aValue = mMargin->mBorderWidth->mBottom;  break;
          case PROP_BORDER_LEFT_WIDTH:    aValue = mMargin->mBorderWidth->mLeft;    break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_BORDER_TOP_COLOR:
    case PROP_BORDER_RIGHT_COLOR:
    case PROP_BORDER_BOTTOM_COLOR:
    case PROP_BORDER_LEFT_COLOR:
      if ((nsnull != mMargin) && (nsnull != mMargin->mBorderColor)) {
        switch (aProperty) {
          case PROP_BORDER_TOP_COLOR:     aValue = mMargin->mBorderColor->mTop;    break;
          case PROP_BORDER_RIGHT_COLOR:   aValue = mMargin->mBorderColor->mRight;  break;
          case PROP_BORDER_BOTTOM_COLOR:  aValue = mMargin->mBorderColor->mBottom; break;
          case PROP_BORDER_LEFT_COLOR:    aValue = mMargin->mBorderColor->mLeft;   break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_BORDER_TOP_STYLE:
    case PROP_BORDER_RIGHT_STYLE:
    case PROP_BORDER_BOTTOM_STYLE:
    case PROP_BORDER_LEFT_STYLE:
      if ((nsnull != mMargin) && (nsnull != mMargin->mBorderStyle)) {
        switch (aProperty) {
          case PROP_BORDER_TOP_STYLE:     aValue = mMargin->mBorderStyle->mTop;    break;
          case PROP_BORDER_RIGHT_STYLE:   aValue = mMargin->mBorderStyle->mRight;  break;
          case PROP_BORDER_BOTTOM_STYLE:  aValue = mMargin->mBorderStyle->mBottom; break;
          case PROP_BORDER_LEFT_STYLE:    aValue = mMargin->mBorderStyle->mLeft;   break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP__MOZ_BORDER_RADIUS:
    case PROP_OUTLINE_WIDTH:
    case PROP_OUTLINE_COLOR:
    case PROP_OUTLINE_STYLE:
      if (nsnull != mMargin) {
        switch (aProperty) {
          case PROP__MOZ_BORDER_RADIUS: aValue = mMargin->mBorderRadius; break;
          case PROP_OUTLINE_WIDTH:      aValue = mMargin->mOutlineWidth; break;
          case PROP_OUTLINE_COLOR:      aValue = mMargin->mOutlineColor; break;
          case PROP_OUTLINE_STYLE:      aValue = mMargin->mOutlineStyle; break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSPosition
    case PROP_POSITION:
    case PROP_WIDTH:
    case PROP_MIN_WIDTH:
    case PROP_MAX_WIDTH:
    case PROP_HEIGHT:
    case PROP_MIN_HEIGHT:
    case PROP_MAX_HEIGHT:
    case PROP_Z_INDEX:
      if (nsnull != mPosition) {
        switch (aProperty) {
          case PROP_POSITION:   aValue = mPosition->mPosition;   break;
          case PROP_WIDTH:      aValue = mPosition->mWidth;      break;
          case PROP_MIN_WIDTH:  aValue = mPosition->mMinWidth;   break;
          case PROP_MAX_WIDTH:  aValue = mPosition->mMaxWidth;   break;
          case PROP_HEIGHT:     aValue = mPosition->mHeight;     break;
          case PROP_MIN_HEIGHT: aValue = mPosition->mMinHeight;  break;
          case PROP_MAX_HEIGHT: aValue = mPosition->mMaxHeight;  break;
          case PROP_Z_INDEX:    aValue = mPosition->mZIndex;     break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_TOP:
    case PROP_RIGHT:
    case PROP_BOTTOM:
    case PROP_LEFT:
      if ((nsnull != mPosition) && (nsnull != mPosition->mOffset)) {
        switch (aProperty) {
          case PROP_TOP:    aValue = mPosition->mOffset->mTop;    break;
          case PROP_RIGHT:  aValue = mPosition->mOffset->mRight;  break;
          case PROP_BOTTOM: aValue = mPosition->mOffset->mBottom; break;
          case PROP_LEFT:   aValue = mPosition->mOffset->mLeft;   break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

      // nsCSSList
    case PROP_LIST_STYLE_TYPE:
    case PROP_LIST_STYLE_IMAGE:
    case PROP_LIST_STYLE_POSITION:
      if (nsnull != mList) {
        switch (aProperty) {
          case PROP_LIST_STYLE_TYPE:      aValue = mList->mType;     break;
          case PROP_LIST_STYLE_IMAGE:     aValue = mList->mImage;    break;
          case PROP_LIST_STYLE_POSITION:  aValue = mList->mPosition; break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

      // nsCSSTable
    case PROP_BORDER_COLLAPSE:
    case PROP_BORDER_X_SPACING:
    case PROP_BORDER_Y_SPACING:
    case PROP_CAPTION_SIDE:
    case PROP_EMPTY_CELLS:
    case PROP_TABLE_LAYOUT:
      if (nsnull != mTable) {
        switch (aProperty) {
          case PROP_BORDER_COLLAPSE:  aValue = mTable->mBorderCollapse; break;
          case PROP_BORDER_X_SPACING: aValue = mTable->mBorderSpacingX; break;
          case PROP_BORDER_Y_SPACING: aValue = mTable->mBorderSpacingY; break;
          case PROP_CAPTION_SIDE:     aValue = mTable->mCaptionSide;    break;
          case PROP_EMPTY_CELLS:      aValue = mTable->mEmptyCells;     break;
          case PROP_TABLE_LAYOUT:     aValue = mTable->mLayout;         break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

      // nsCSSBreaks
    case PROP_ORPHANS:
    case PROP_WIDOWS:
    case PROP_PAGE:
    case PROP_PAGE_BREAK_AFTER:
    case PROP_PAGE_BREAK_BEFORE:
    case PROP_PAGE_BREAK_INSIDE:
      if (nsnull != mBreaks) {
        switch (aProperty) {
          case PROP_ORPHANS:            aValue = mBreaks->mOrphans;         break;
          case PROP_WIDOWS:             aValue = mBreaks->mWidows;          break;
          case PROP_PAGE:               aValue = mBreaks->mPage;            break;
          case PROP_PAGE_BREAK_AFTER:   aValue = mBreaks->mPageBreakAfter;  break;
          case PROP_PAGE_BREAK_BEFORE:  aValue = mBreaks->mPageBreakBefore; break;
          case PROP_PAGE_BREAK_INSIDE:  aValue = mBreaks->mPageBreakInside; break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

      // nsCSSPage
    case PROP_MARKS:
    case PROP_SIZE_WIDTH:
    case PROP_SIZE_HEIGHT:
      if (nsnull != mPage) {
        switch (aProperty) {
          case PROP_MARKS:        aValue = mPage->mMarks;       break;
          case PROP_SIZE_WIDTH:   aValue = mPage->mSizeWidth;   break;
          case PROP_SIZE_HEIGHT:  aValue = mPage->mSizeHeight;  break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

      // nsCSSContent
    case PROP_CONTENT:
    case PROP_COUNTER_INCREMENT:
    case PROP_COUNTER_RESET:
    case PROP_MARKER_OFFSET:
    case PROP_QUOTES_OPEN:
    case PROP_QUOTES_CLOSE:
      if (nsnull != mContent) {
        switch (aProperty) {
          case PROP_CONTENT:
            if (nsnull != mContent->mContent) {
              aValue = mContent->mContent->mValue;
            }
            break;
          case PROP_COUNTER_INCREMENT:  
            if (nsnull != mContent->mCounterIncrement) {
              aValue = mContent->mCounterIncrement->mCounter;
            }
            break;
          case PROP_COUNTER_RESET:
            if (nsnull != mContent->mCounterReset) {
              aValue = mContent->mCounterReset->mCounter;
            }
            break;
          case PROP_MARKER_OFFSET:      aValue = mContent->mMarkerOffset;     break;
          case PROP_QUOTES_OPEN:
            if (nsnull != mContent->mQuotes) {
              aValue = mContent->mQuotes->mOpen;
            }
            break;
          case PROP_QUOTES_CLOSE:
            if (nsnull != mContent->mQuotes) {
              aValue = mContent->mQuotes->mClose;
            }
            break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

      // nsCSSAural
    case PROP_AZIMUTH:
    case PROP_ELEVATION:
    case PROP_CUE_AFTER:
    case PROP_CUE_BEFORE:
    case PROP_PAUSE_AFTER:
    case PROP_PAUSE_BEFORE:
    case PROP_PITCH:
    case PROP_PITCH_RANGE:
    case PROP_PLAY_DURING:
    case PROP_PLAY_DURING_FLAGS:
    case PROP_RICHNESS:
    case PROP_SPEAK:
    case PROP_SPEAK_HEADER:
    case PROP_SPEAK_NUMERAL:
    case PROP_SPEAK_PUNCTUATION:
    case PROP_SPEECH_RATE:
    case PROP_STRESS:
    case PROP_VOICE_FAMILY:
    case PROP_VOLUME:
      if (nsnull != mAural) {
        switch (aProperty) {
          case PROP_AZIMUTH:            aValue = mAural->mAzimuth;          break;
          case PROP_ELEVATION:          aValue = mAural->mElevation;        break;
          case PROP_CUE_AFTER:          aValue = mAural->mCueAfter;         break;
          case PROP_CUE_BEFORE:         aValue = mAural->mCueBefore;        break;
          case PROP_PAUSE_AFTER:        aValue = mAural->mPauseAfter;       break;
          case PROP_PAUSE_BEFORE:       aValue = mAural->mPauseBefore;      break;
          case PROP_PITCH:              aValue = mAural->mPitch;            break;
          case PROP_PITCH_RANGE:        aValue = mAural->mPitchRange;       break;
          case PROP_PLAY_DURING:        aValue = mAural->mPlayDuring;       break;
          case PROP_PLAY_DURING_FLAGS:  aValue = mAural->mPlayDuringFlags;  break;
          case PROP_RICHNESS:           aValue = mAural->mRichness;         break;
          case PROP_SPEAK:              aValue = mAural->mSpeak;            break;
          case PROP_SPEAK_HEADER:       aValue = mAural->mSpeakHeader;      break;
          case PROP_SPEAK_NUMERAL:      aValue = mAural->mSpeakNumeral;     break;
          case PROP_SPEAK_PUNCTUATION:  aValue = mAural->mSpeakPunctuation; break;
          case PROP_SPEECH_RATE:        aValue = mAural->mSpeechRate;       break;
          case PROP_STRESS:             aValue = mAural->mStress;           break;
          case PROP_VOICE_FAMILY:       aValue = mAural->mVoiceFamily;      break;
          case PROP_VOLUME:             aValue = mAural->mVolume;           break;
        }
      }
      else {
        aValue.Reset();
      }
      break;


      // Shorthands
    case PROP_BACKGROUND:
    case PROP_BORDER:
    case PROP_BORDER_SPACING:
    case PROP_CLIP:
    case PROP_CUE:
    case PROP_FONT:
    case PROP_LIST_STYLE:
    case PROP_MARGIN:
    case PROP_OUTLINE:
    case PROP_PADDING:
    case PROP_PAUSE:
    case PROP_QUOTES:
    case PROP_SIZE:
    case PROP_TEXT_SHADOW:
    case PROP_BACKGROUND_POSITION:
    case PROP_BORDER_TOP:
    case PROP_BORDER_RIGHT:
    case PROP_BORDER_BOTTOM:
    case PROP_BORDER_LEFT:
    case PROP_BORDER_COLOR:
    case PROP_BORDER_STYLE:
    case PROP_BORDER_WIDTH:
      NS_ERROR("can't query for shorthand properties");
    default:
      result = NS_ERROR_ILLEGAL_VALUE;
      break;
  }
  return result;
}


NS_IMETHODIMP
CSSDeclarationImpl::GetValue(const nsString& aProperty, nsString& aValue)
{
  char prop[50];
  aProperty.ToCString(prop, sizeof(prop));
  PRInt32 propID = nsCSSProps::LookupName(prop);
  return GetValue(propID, aValue);
}

PRBool CSSDeclarationImpl::AppendValueToString(PRInt32 aProperty, nsString& aResult)
{
  nsCSSValue  value;
  GetValue(aProperty, value);
  return AppendValueToString(aProperty, value, aResult);
}

PRBool CSSDeclarationImpl::AppendValueToString(PRInt32 aProperty, const nsCSSValue& aValue, nsString& aResult)
{
  nsCSSUnit unit = aValue.GetUnit();

  if (eCSSUnit_Null == unit) {
    return PR_FALSE;
  }

  if ((eCSSUnit_String <= unit) && (unit <= eCSSUnit_Counters)) {
    switch (unit) {
      case eCSSUnit_URL:      aResult.Append("url(");       break;
      case eCSSUnit_Attr:     aResult.Append("attr(");      break;
      case eCSSUnit_Counter:  aResult.Append("counter(");   break;
      case eCSSUnit_Counters: aResult.Append("counters(");  break;
      default:  break;
    }
    nsAutoString  buffer;
    aValue.GetStringValue(buffer);
    aResult.Append(buffer);
  }
  else if (eCSSUnit_Integer == unit) {
    aResult.Append(aValue.GetIntValue(), 10);
  }
  else if (eCSSUnit_Enumerated == unit) {
    if (PROP_TEXT_DECORATION == aProperty) {
      PRInt32 intValue = aValue.GetIntValue();
      if (NS_STYLE_TEXT_DECORATION_NONE != intValue) {
        PRInt32 mask;
        for (mask = NS_STYLE_TEXT_DECORATION_UNDERLINE; 
             mask <= NS_STYLE_TEXT_DECORATION_BLINK; 
             mask <<= 1) {
          if ((mask & intValue) == mask) {
            aResult.Append(nsCSSProps::LookupProperty(aProperty, mask));
            intValue &= ~mask;
            if (0 != intValue) { // more left
              aResult.Append(' ');
            }
          }
        }
      }
      else {
        aResult.Append(nsCSSProps::LookupProperty(aProperty, NS_STYLE_TEXT_DECORATION_NONE));
      }
    }
    else if (PROP_AZIMUTH == aProperty) {
      PRInt32 intValue = aValue.GetIntValue();
      aResult.Append(nsCSSProps::LookupProperty(aProperty, (intValue & ~NS_STYLE_AZIMUTH_BEHIND)));
      if ((NS_STYLE_AZIMUTH_BEHIND & intValue) != 0) {
        aResult.Append(' ');
        aResult.Append(nsCSSProps::LookupProperty(aProperty, NS_STYLE_AZIMUTH_BEHIND));
      }
    }
    else if (PROP_PLAY_DURING_FLAGS == aProperty) {
      PRInt32 intValue = aValue.GetIntValue();
      if ((NS_STYLE_PLAY_DURING_MIX & intValue) != 0) {
        aResult.Append(nsCSSProps::LookupProperty(aProperty, NS_STYLE_PLAY_DURING_MIX));
      }
      if ((NS_STYLE_PLAY_DURING_REPEAT & intValue) != 0) {
        if (NS_STYLE_PLAY_DURING_REPEAT != intValue) {
          aResult.Append(' ');
        }
        aResult.Append(nsCSSProps::LookupProperty(aProperty, NS_STYLE_PLAY_DURING_REPEAT));
      }
    }
    else if (PROP_MARKS == aProperty) {
      PRInt32 intValue = aValue.GetIntValue();
      if ((NS_STYLE_PAGE_MARKS_CROP & intValue) != 0) {
        aResult.Append(nsCSSProps::LookupProperty(aProperty, NS_STYLE_PAGE_MARKS_CROP));
      }
      if ((NS_STYLE_PAGE_MARKS_REGISTER & intValue) != 0) {
        if ((NS_STYLE_PAGE_MARKS_CROP & intValue) != 0) {
          aResult.Append(' ');
        }
        aResult.Append(nsCSSProps::LookupProperty(aProperty, NS_STYLE_PAGE_MARKS_REGISTER));
      }
    }
    else {
      const char* name = nsCSSProps::LookupProperty(aProperty, aValue.GetIntValue());
      if (name != nsnull) {
        aResult.Append(name);
      }
    }
  }
  else if (eCSSUnit_Color == unit){
    nscolor color = aValue.GetColorValue();
    aResult.Append("rgb(");
    aResult.Append(NS_GET_R(color), 10);
    aResult.Append(",");
    aResult.Append(NS_GET_G(color), 10);
    aResult.Append(",");
    aResult.Append(NS_GET_B(color), 10);
    aResult.Append(')');
  }
  else if (eCSSUnit_Percent == unit) {
    aResult.Append(aValue.GetPercentValue() * 100.0f);
  }
  else if (eCSSUnit_Percent < unit) {  // length unit
    aResult.Append(aValue.GetFloatValue());
  }

  switch (unit) {
    case eCSSUnit_Null:         break;
    case eCSSUnit_Auto:         aResult.Append("auto");     break;
    case eCSSUnit_Inherit:      aResult.Append("inherit");  break;
    case eCSSUnit_None:         aResult.Append("none");     break;
    case eCSSUnit_Normal:       aResult.Append("normal");   break;

    case eCSSUnit_String:       break;
    case eCSSUnit_URL:
    case eCSSUnit_Attr:
    case eCSSUnit_Counter:
    case eCSSUnit_Counters:     aResult.Append(')');    break;
    case eCSSUnit_Integer:      break;
    case eCSSUnit_Enumerated:   break;
    case eCSSUnit_Color:        break;
    case eCSSUnit_Percent:      aResult.Append("%");    break;
    case eCSSUnit_Number:       break;

    case eCSSUnit_Inch:         aResult.Append("in");   break;
    case eCSSUnit_Foot:         aResult.Append("ft");   break;
    case eCSSUnit_Mile:         aResult.Append("mi");   break;
    case eCSSUnit_Millimeter:   aResult.Append("mm");   break;
    case eCSSUnit_Centimeter:   aResult.Append("cm");   break;
    case eCSSUnit_Meter:        aResult.Append("m");    break;
    case eCSSUnit_Kilometer:    aResult.Append("km");   break;
    case eCSSUnit_Point:        aResult.Append("pt");   break;
    case eCSSUnit_Pica:         aResult.Append("pc");   break;
    case eCSSUnit_Didot:        aResult.Append("dt");   break;
    case eCSSUnit_Cicero:       aResult.Append("cc");   break;

    case eCSSUnit_EM:           aResult.Append("em");   break;
    case eCSSUnit_EN:           aResult.Append("en");   break;
    case eCSSUnit_XHeight:      aResult.Append("ex");   break;
    case eCSSUnit_CapHeight:    aResult.Append("cap");  break;

    case eCSSUnit_Pixel:        aResult.Append("px");   break;

    case eCSSUnit_Degree:       aResult.Append("deg");  break;
    case eCSSUnit_Grad:         aResult.Append("grad"); break;
    case eCSSUnit_Radian:       aResult.Append("rad");  break;

    case eCSSUnit_Hertz:        aResult.Append("Hz");   break;
    case eCSSUnit_Kilohertz:    aResult.Append("kHz");  break;

    case eCSSUnit_Seconds:      aResult.Append("s");    break;
    case eCSSUnit_Milliseconds: aResult.Append("ms");   break;
  }

  return PR_TRUE;
}

#define HAS_VALUE(strct,data) \
  ((nsnull != strct) && (eCSSUnit_Null != strct->data.GetUnit()))
#define HAS_RECT(strct,rect)                            \
  ((nsnull != strct) && (nsnull != strct->rect) &&      \
   (eCSSUnit_Null != strct->rect->mTop.GetUnit()) &&    \
   (eCSSUnit_Null != strct->rect->mRight.GetUnit()) &&  \
   (eCSSUnit_Null != strct->rect->mBottom.GetUnit()) && \
   (eCSSUnit_Null != strct->rect->mLeft.GetUnit())) 

NS_IMETHODIMP
CSSDeclarationImpl::GetValue(PRInt32 aProperty, nsString& aValue)
{
  PRBool  isImportant = PR_FALSE;
  GetValueIsImportant(aProperty, isImportant);
  if (PR_TRUE == isImportant) {
    return mImportant->GetValue(aProperty, aValue);
  }

  aValue.Truncate(0);

  // shorthands
  switch (aProperty) {
    case PROP_BACKGROUND:
      if (AppendValueToString(PROP_BACKGROUND_COLOR, aValue)) aValue.Append(' ');
      if (AppendValueToString(PROP_BACKGROUND_IMAGE, aValue)) aValue.Append(' ');
      if (AppendValueToString(PROP_BACKGROUND_REPEAT, aValue)) aValue.Append(' ');
      if (AppendValueToString(PROP_BACKGROUND_ATTACHMENT, aValue)) aValue.Append(' ');
      if (HAS_VALUE(mColor,mBackPositionX) && HAS_VALUE(mColor,mBackPositionY)) {
        AppendValueToString(PROP_BACKGROUND_X_POSITION, aValue);
        aValue.Append(' ');
        AppendValueToString(PROP_BACKGROUND_Y_POSITION, aValue);
      }
      break;
    case PROP_BORDER:
      if (AppendValueToString(PROP_BORDER_TOP_WIDTH, aValue)) aValue.Append(' ');
      if (AppendValueToString(PROP_BORDER_TOP_STYLE, aValue)) aValue.Append(' ');
      AppendValueToString(PROP_BORDER_TOP_COLOR, aValue);
      break;
    case PROP_BORDER_SPACING:
      if (AppendValueToString(PROP_BORDER_X_SPACING, aValue)) {
        aValue.Append(' ');
        AppendValueToString(PROP_BORDER_Y_SPACING, aValue);
      }
      break;
    case PROP_CLIP:
      if (HAS_RECT(mDisplay,mClip)) {
        aValue.Append("rect(");
        AppendValueToString(PROP_CLIP_TOP, aValue);     aValue.Append(' ');
        AppendValueToString(PROP_CLIP_RIGHT, aValue);   aValue.Append(' ');
        AppendValueToString(PROP_CLIP_BOTTOM, aValue);  aValue.Append(' ');
        AppendValueToString(PROP_CLIP_LEFT, aValue);
        aValue.Append(")");
      }
      break;
    case PROP_CUE:
      if (HAS_VALUE(mAural,mCueAfter) && HAS_VALUE(mAural,mCueBefore)) {
        AppendValueToString(PROP_CUE_AFTER, aValue);
        aValue.Append(' ');
        AppendValueToString(PROP_CUE_BEFORE, aValue);
      }
      break;
    case PROP_CURSOR:
      if ((nsnull != mColor) && (nsnull != mColor->mCursor)) {
        nsCSSValueList* cursor = mColor->mCursor;
        do {
          AppendValueToString(PROP_CURSOR, cursor->mValue, aValue);
          cursor = cursor->mNext;
          if (nsnull != cursor) {
            aValue.Append(' ');
          }
        } while (nsnull != cursor);
      }
      break;
    case PROP_FONT:
      if (HAS_VALUE(mFont,mSize)) {
        if (AppendValueToString(PROP_FONT_STYLE, aValue)) aValue.Append(' ');
        if (AppendValueToString(PROP_FONT_VARIANT, aValue)) aValue.Append(' ');
        if (AppendValueToString(PROP_FONT_WEIGHT, aValue)) aValue.Append(' ');
        AppendValueToString(PROP_FONT_SIZE, aValue);
        if (HAS_VALUE(mText,mLineHeight)) {
          aValue.Append('/');
          AppendValueToString(PROP_LINE_HEIGHT, aValue);
        }
        aValue.Append(' ');
        AppendValueToString(PROP_FONT_FAMILY, aValue);
      }
      break;
    case PROP_LIST_STYLE:
      if (AppendValueToString(PROP_LIST_STYLE_TYPE, aValue)) aValue.Append(' ');
      if (AppendValueToString(PROP_LIST_STYLE_POSITION, aValue)) aValue.Append(' ');
      AppendValueToString(PROP_LIST_STYLE_IMAGE, aValue);
      break;
    case PROP_MARGIN:
      if (HAS_RECT(mMargin,mMargin)) {
        AppendValueToString(PROP_MARGIN_TOP, aValue);     aValue.Append(' ');
        AppendValueToString(PROP_MARGIN_RIGHT, aValue);   aValue.Append(' ');
        AppendValueToString(PROP_MARGIN_BOTTOM, aValue);  aValue.Append(' ');
        AppendValueToString(PROP_MARGIN_LEFT, aValue);
      }
      break;
    case PROP_OUTLINE:
      if (AppendValueToString(PROP_OUTLINE_COLOR, aValue)) aValue.Append(' ');
      if (AppendValueToString(PROP_OUTLINE_STYLE, aValue)) aValue.Append(' ');
      AppendValueToString(PROP_OUTLINE_WIDTH, aValue);
      break;
    case PROP_PADDING:
      if (HAS_RECT(mMargin,mPadding)) {
        AppendValueToString(PROP_PADDING_TOP, aValue);    aValue.Append(' ');
        AppendValueToString(PROP_PADDING_RIGHT, aValue);  aValue.Append(' ');
        AppendValueToString(PROP_PADDING_BOTTOM, aValue); aValue.Append(' ');
        AppendValueToString(PROP_PADDING_LEFT, aValue);
      }
      break;
    case PROP_PAUSE:
      if (HAS_VALUE(mAural,mPauseAfter) && HAS_VALUE(mAural,mPauseBefore)) {
        AppendValueToString(PROP_PAUSE_AFTER, aValue);
        aValue.Append(' ');
        AppendValueToString(PROP_PAUSE_BEFORE, aValue);
      }
      break;
    case PROP_SIZE:
      if (HAS_VALUE(mPage,mSizeWidth) && HAS_VALUE(mPage,mSizeHeight)) {
        AppendValueToString(PROP_SIZE_WIDTH, aValue);
        aValue.Append(' ');
        AppendValueToString(PROP_SIZE_HEIGHT, aValue);
      }
      break;
    case PROP_TEXT_SHADOW:
      if ((nsnull != mText) && (nsnull != mText->mTextShadow)) {
        if (mText->mTextShadow->mXOffset.IsLengthUnit()) {
          nsCSSShadow*  shadow = mText->mTextShadow;
          while (nsnull != shadow) {
            if (AppendValueToString(PROP_TEXT_SHADOW_COLOR, shadow->mColor, aValue)) aValue.Append(' ');
            if (AppendValueToString(PROP_TEXT_SHADOW_X, shadow->mXOffset, aValue)) {
              aValue.Append(' ');
              AppendValueToString(PROP_TEXT_SHADOW_Y, shadow->mYOffset, aValue);
              aValue.Append(' ');
            }
            if (AppendValueToString(PROP_TEXT_SHADOW_RADIUS, shadow->mRadius, aValue)) aValue.Append(' ');
            shadow = shadow->mNext;
          }
        }
        else {  // none or inherit
          AppendValueToString(PROP_TEXT_SHADOW_X, aValue);
        }
      }
      break;
    case PROP_BACKGROUND_POSITION:
      if (HAS_VALUE(mColor,mBackPositionX) && HAS_VALUE(mColor,mBackPositionY)) {
        AppendValueToString(PROP_BACKGROUND_X_POSITION, aValue);
        aValue.Append(' ');
        AppendValueToString(PROP_BACKGROUND_Y_POSITION, aValue);
      }
      break;
    case PROP_BORDER_TOP:
      if (AppendValueToString(PROP_BORDER_TOP_WIDTH, aValue)) aValue.Append(' ');
      if (AppendValueToString(PROP_BORDER_TOP_STYLE, aValue)) aValue.Append(' ');
      AppendValueToString(PROP_BORDER_TOP_COLOR, aValue);
      break;
    case PROP_BORDER_RIGHT:
      if (AppendValueToString(PROP_BORDER_RIGHT_WIDTH, aValue)) aValue.Append(' ');
      if (AppendValueToString(PROP_BORDER_RIGHT_STYLE, aValue)) aValue.Append(' ');
      AppendValueToString(PROP_BORDER_RIGHT_COLOR, aValue);
      break;
    case PROP_BORDER_BOTTOM:
      if (AppendValueToString(PROP_BORDER_BOTTOM_WIDTH, aValue)) aValue.Append(' ');
      if (AppendValueToString(PROP_BORDER_BOTTOM_STYLE, aValue)) aValue.Append(' ');
      AppendValueToString(PROP_BORDER_BOTTOM_COLOR, aValue);
      break;
    case PROP_BORDER_LEFT:
      if (AppendValueToString(PROP_BORDER_LEFT_WIDTH, aValue)) aValue.Append(' ');
      if (AppendValueToString(PROP_BORDER_LEFT_STYLE, aValue)) aValue.Append(' ');
      AppendValueToString(PROP_BORDER_LEFT_COLOR, aValue);
      break;
    case PROP_BORDER_COLOR:
      if (HAS_RECT(mMargin,mBorderColor)) {
        AppendValueToString(PROP_BORDER_TOP_COLOR, aValue);     aValue.Append(' ');
        AppendValueToString(PROP_BORDER_RIGHT_COLOR, aValue);   aValue.Append(' ');
        AppendValueToString(PROP_BORDER_BOTTOM_COLOR, aValue);  aValue.Append(' ');
        AppendValueToString(PROP_BORDER_LEFT_COLOR, aValue);
      }
      break;
    case PROP_BORDER_STYLE:
      if (HAS_RECT(mMargin,mBorderStyle)) {
        AppendValueToString(PROP_BORDER_TOP_STYLE, aValue);     aValue.Append(' ');
        AppendValueToString(PROP_BORDER_RIGHT_STYLE, aValue);   aValue.Append(' ');
        AppendValueToString(PROP_BORDER_BOTTOM_STYLE, aValue);  aValue.Append(' ');
        AppendValueToString(PROP_BORDER_LEFT_STYLE, aValue);
      }
      break;
    case PROP_BORDER_WIDTH:
      if (HAS_RECT(mMargin,mBorderWidth)) {
        AppendValueToString(PROP_BORDER_TOP_WIDTH, aValue);     aValue.Append(' ');
        AppendValueToString(PROP_BORDER_RIGHT_WIDTH, aValue);   aValue.Append(' ');
        AppendValueToString(PROP_BORDER_BOTTOM_WIDTH, aValue);  aValue.Append(' ');
        AppendValueToString(PROP_BORDER_LEFT_WIDTH, aValue);
      }
      break;
    case PROP_CONTENT:
      if ((nsnull != mContent) && (nsnull != mContent->mContent)) {
        nsCSSValueList* content = mContent->mContent;
        do {
          AppendValueToString(PROP_CONTENT, content->mValue, aValue);
          content = content->mNext;
          if (nsnull != content) {
            aValue.Append(' ');
          }
        } while (nsnull != content);
      }
      break;
    case PROP_COUNTER_INCREMENT:
      if ((nsnull != mContent) && (nsnull != mContent->mCounterIncrement)) {
        nsCSSCounterData* data = mContent->mCounterIncrement;
        do {
          if (AppendValueToString(PROP_COUNTER_INCREMENT, data->mCounter, aValue)) {
            if (HAS_VALUE(data, mValue)) {
              aValue.Append(' ');
              AppendValueToString(PROP_COUNTER_INCREMENT, data->mValue, aValue);
            }
          }
          data = data->mNext;
          if (nsnull != data) {
            aValue.Append(' ');
          }
        } while (nsnull != data);
      }
      break;
    case PROP_COUNTER_RESET:
      if ((nsnull != mContent) && (nsnull != mContent->mCounterReset)) {
        nsCSSCounterData* data = mContent->mCounterReset;
        do {
          if (AppendValueToString(PROP_COUNTER_RESET, data->mCounter, aValue)) {
            if (HAS_VALUE(data, mValue)) {
              aValue.Append(' ');
              AppendValueToString(PROP_COUNTER_RESET, data->mValue, aValue);
            }
          }
          data = data->mNext;
          if (nsnull != data) {
            aValue.Append(' ');
          }
        } while (nsnull != data);
      }
      break;
    case PROP_PLAY_DURING:
      if (HAS_VALUE(mAural, mPlayDuring)) {
        AppendValueToString(PROP_PLAY_DURING, aValue);
        if (HAS_VALUE(mAural, mPlayDuringFlags)) {
          aValue.Append(' ');
          AppendValueToString(PROP_PLAY_DURING_FLAGS, aValue);
        }
      }
      break;
    case PROP_QUOTES:
      if ((nsnull != mContent) && (nsnull != mContent->mQuotes)) {
        nsCSSQuotes* quotes = mContent->mQuotes;
        do {
          AppendValueToString(PROP_QUOTES_OPEN, quotes->mOpen, aValue);
          aValue.Append(' ');
          AppendValueToString(PROP_QUOTES_CLOSE, quotes->mClose, aValue);
          quotes = quotes->mNext;
          if (nsnull != quotes) {
            aValue.Append(' ');
          }
        } while (nsnull != quotes);
      }
      break;
    default:
      AppendValueToString(aProperty, aValue);
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
CSSDeclarationImpl::GetImportantValues(nsICSSDeclaration*& aResult)
{
  if (nsnull != mImportant) {
    aResult = mImportant;
    NS_ADDREF(aResult);
  }
  else {
    aResult = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
CSSDeclarationImpl::GetValueIsImportant(const nsString& aProperty,
                                        PRBool& aIsImportant)
{
  char prop[50];
  aProperty.ToCString(prop, sizeof(prop));
  PRInt32 propID = nsCSSProps::LookupName(prop);
  return GetValueIsImportant(propID, aIsImportant);
}

NS_IMETHODIMP
CSSDeclarationImpl::GetValueIsImportant(PRInt32 aProperty,
                                        PRBool& aIsImportant)
{
  nsCSSValue val;

  if (nsnull != mImportant) {
    mImportant->GetValue(aProperty, val);
    if (eCSSUnit_Null != val.GetUnit()) {
      aIsImportant = PR_TRUE;
    }
    else {
      aIsImportant = PR_FALSE;
    }
  }
  else {
    aIsImportant = PR_FALSE;
  }

  return NS_OK;
}

NS_IMETHODIMP
CSSDeclarationImpl::ToString(nsString& aString)
{
  if (nsnull != mOrder) {
    PRInt32 count = mOrder->Count();
    PRInt32 index;
    for (index = 0; index < count; index++) {
      PRInt32 property = (PRInt32)mOrder->ElementAt(index);
      if (0 <= property) {
        aString.Append(nsCSSProps::kNameTable[property].name);
        aString.Append(": ");

        nsAutoString value;
        GetValue(property, value);
        aString.Append(value);
        if (index < count) {
          aString.Append("; ");
        }
      }
      else {  // is comment
        aString.Append("/* ");
        nsString* comment = (nsString*)mComments->ElementAt((-1) - property);
        aString.Append(*comment);
        aString.Append(" */ ");
      }
    }
  }
  return NS_OK;
}

void CSSDeclarationImpl::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("{ ", out);

  if (nsnull != mFont) {
    mFont->List(out);
  }
  if (nsnull != mColor) {
    mColor->List(out);
  }
  if (nsnull != mText) {
    mText->List(out);
  }
  if (nsnull != mDisplay) {
    mDisplay->List(out);
  }
  if (nsnull != mMargin) {
    mMargin->List(out);
  }
  if (nsnull != mPosition) {
    mPosition->List(out);
  }
  if (nsnull != mList) {
    mList->List(out);
  }
  if (nsnull != mTable) {
    mTable->List(out);
  }
  if (nsnull != mBreaks) {
    mBreaks->List(out);
  }
  if (nsnull != mPage) {
    mPage->List(out);
  }
  if (nsnull != mContent) {
    mContent->List(out);
  }
  if (nsnull != mAural) {
    mAural->List(out);
  }

  fputs("}", out);

  if (nsnull != mImportant) {
    fputs(" ! important ", out);
    mImportant->List(out, 0);
  }
}

NS_IMETHODIMP
CSSDeclarationImpl::Count(PRUint32* aCount)
{
  if (nsnull != mOrder) {
    *aCount = (PRUint32)mOrder->Count();
  }
  else {
    *aCount = 0;
  }
  
  return NS_OK;
}

NS_IMETHODIMP
CSSDeclarationImpl::GetNthProperty(PRUint32 aIndex, nsString& aReturn)
{
  aReturn.SetLength(0);
  if (nsnull != mOrder) {
    PRInt32 property = (PRInt32)mOrder->ElementAt(aIndex);
    if (0 <= property) {
      aReturn.Append(nsCSSProps::kNameTable[property].name);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
CSSDeclarationImpl::GetStyleImpact(PRInt32* aHint) const
{
  NS_ASSERTION(nsnull != aHint, "null pointer");
  if (nsnull == aHint) {
    return NS_ERROR_NULL_POINTER;
  }
  PRInt32 hint = NS_STYLE_HINT_NONE;
  if (nsnull != mOrder) {
    PRInt32 count = mOrder->Count();
    PRInt32 index;
    for (index = 0; index < count; index++) {
      PRInt32 property = (PRInt32)mOrder->ElementAt(index);
      if (0 <= property) {
        if (hint < nsCSSProps::kHintTable[property]) {
          hint = nsCSSProps::kHintTable[property];
        }
      }
    }
  }
  *aHint = hint;
  return NS_OK;
}


NS_HTML nsresult
  NS_NewCSSDeclaration(nsICSSDeclaration** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  CSSDeclarationImpl  *it;
  NS_NEWXPCOM(it, CSSDeclarationImpl);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kICSSDeclarationIID, (void **) aInstancePtrResult);
}


