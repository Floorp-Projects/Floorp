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
 *   Daniel Glazman <glazman@netscape.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

/*
 * temporary (expanded) representation of the property-value pairs
 * within a CSS declaration using during parsing and mutation, and
 * representation of complex values for CSS properties
 */

#include "nscore.h"
#include "nsCSSStruct.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsCSSProps.h"
#include "nsUnitConversion.h"
#include "nsFont.h"

#include "nsStyleConsts.h"

#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsPrintfCString.h"

#define CSS_IF_DELETE(ptr)  if (nsnull != ptr)  { delete ptr; ptr = nsnull; }

// --- nsCSSFont -----------------

nsCSSFont::nsCSSFont(void)
{
  MOZ_COUNT_CTOR(nsCSSFont);
}

nsCSSFont::nsCSSFont(const nsCSSFont& aCopy)
  : mFamily(aCopy.mFamily),
    mStyle(aCopy.mStyle),
    mVariant(aCopy.mVariant),
    mWeight(aCopy.mWeight),
    mSize(aCopy.mSize),
    mSizeAdjust(aCopy.mSizeAdjust),
    mStretch(aCopy.mStretch)
{
  MOZ_COUNT_CTOR(nsCSSFont);
}

nsCSSFont::~nsCSSFont(void)
{
  MOZ_COUNT_DTOR(nsCSSFont);
}

// --- support -----------------

#define CSS_IF_COPY(val, type) \
  if (aCopy.val) (val) = new type(*(aCopy.val));

nsCSSValueList::nsCSSValueList(void)
  : mValue(),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSValueList);
}

nsCSSValueList::nsCSSValueList(const nsCSSValueList& aCopy)
  : mValue(aCopy.mValue),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSValueList);
  CSS_IF_COPY(mNext, nsCSSValueList);
}

nsCSSValueList::~nsCSSValueList(void)
{
  MOZ_COUNT_DTOR(nsCSSValueList);
  CSS_IF_DELETE(mNext);
}

/* static */ PRBool
nsCSSValueList::Equal(nsCSSValueList* aList1, nsCSSValueList* aList2)
{
  if (aList1 == aList2)
    return PR_TRUE;
    
  nsCSSValueList *p1 = aList1, *p2 = aList2;
  for ( ; p1 && p2; p1 = p1->mNext, p2 = p2->mNext) {
    if (p1->mValue != p2->mValue)
      return PR_FALSE;
  }
  return !p1 && !p2; // true if same length, false otherwise
}

// --- nsCSSColor -----------------

nsCSSColor::nsCSSColor(void)
{
  MOZ_COUNT_CTOR(nsCSSColor);
}

nsCSSColor::nsCSSColor(const nsCSSColor& aCopy)
  : mColor(aCopy.mColor),
    mBackColor(aCopy.mBackColor),
    mBackImage(aCopy.mBackImage),
    mBackRepeat(aCopy.mBackRepeat),
    mBackAttachment(aCopy.mBackAttachment),
    mBackPosition(aCopy.mBackPosition),
    mBackClip(aCopy.mBackClip),
    mBackOrigin(aCopy.mBackOrigin),
    mBackInlinePolicy(aCopy.mBackInlinePolicy)

{
  MOZ_COUNT_CTOR(nsCSSColor);
}

nsCSSColor::~nsCSSColor(void)
{
  MOZ_COUNT_DTOR(nsCSSColor);
}

// --- nsCSSText -----------------

nsCSSText::nsCSSText(void)
  : mTextShadow(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSText);
}

nsCSSText::nsCSSText(const nsCSSText& aCopy)
  : mWordSpacing(aCopy.mWordSpacing),
    mLetterSpacing(aCopy.mLetterSpacing),
    mVerticalAlign(aCopy.mVerticalAlign),
    mTextTransform(aCopy.mTextTransform),
    mTextAlign(aCopy.mTextAlign),
    mTextIndent(aCopy.mTextIndent),
    mDecoration(aCopy.mDecoration),
    mTextShadow(nsnull),
    mUnicodeBidi(aCopy.mUnicodeBidi),
    mLineHeight(aCopy.mLineHeight),
    mWhiteSpace(aCopy.mWhiteSpace)
{
  MOZ_COUNT_CTOR(nsCSSText);
  CSS_IF_COPY(mTextShadow, nsCSSValueList);
}

nsCSSText::~nsCSSText(void)
{
  MOZ_COUNT_DTOR(nsCSSText);
  CSS_IF_DELETE(mTextShadow);
}

// --- nsCSSRect -----------------

nsCSSRect::nsCSSRect(void)
{
  MOZ_COUNT_CTOR(nsCSSRect);
}

nsCSSRect::nsCSSRect(const nsCSSRect& aCopy)
  : mTop(aCopy.mTop),
    mRight(aCopy.mRight),
    mBottom(aCopy.mBottom),
    mLeft(aCopy.mLeft)
{
  MOZ_COUNT_CTOR(nsCSSRect);
}

nsCSSRect::~nsCSSRect()
{
  MOZ_COUNT_DTOR(nsCSSRect);
}

void nsCSSRect::SetAllSidesTo(const nsCSSValue& aValue)
{
  mTop = aValue;
  mRight = aValue;
  mBottom = aValue;
  mLeft = aValue;
}

#if (NS_SIDE_TOP != 0) || (NS_SIDE_RIGHT != 1) || (NS_SIDE_BOTTOM != 2) || (NS_SIDE_LEFT != 3)
#error "Somebody changed the side constants."
#endif

/* static */ const nsCSSRect::side_type nsCSSRect::sides[4] = {
  &nsCSSRect::mTop,
  &nsCSSRect::mRight,
  &nsCSSRect::mBottom,
  &nsCSSRect::mLeft,
};

// --- nsCSSValueListRect -----------------

nsCSSValueListRect::nsCSSValueListRect(void)
  : mTop(nsnull),
    mRight(nsnull),
    mBottom(nsnull),
    mLeft(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSValueListRect);
}

nsCSSValueListRect::nsCSSValueListRect(const nsCSSValueListRect& aCopy)
  : mTop(aCopy.mTop),
    mRight(aCopy.mRight),
    mBottom(aCopy.mBottom),
    mLeft(aCopy.mLeft)
{
  MOZ_COUNT_CTOR(nsCSSValueListRect);
}

nsCSSValueListRect::~nsCSSValueListRect()
{
  MOZ_COUNT_DTOR(nsCSSValueListRect);
}

/* static */ const nsCSSValueListRect::side_type
nsCSSValueListRect::sides[4] = {
  &nsCSSValueListRect::mTop,
  &nsCSSValueListRect::mRight,
  &nsCSSValueListRect::mBottom,
  &nsCSSValueListRect::mLeft,
};

// --- nsCSSDisplay -----------------

nsCSSDisplay::nsCSSDisplay(void)
{
  MOZ_COUNT_CTOR(nsCSSDisplay);
}

nsCSSDisplay::nsCSSDisplay(const nsCSSDisplay& aCopy)
  : mDirection(aCopy.mDirection),
    mDisplay(aCopy.mDisplay),
    mBinding(aCopy.mBinding),
    mAppearance(aCopy.mAppearance),
    mPosition(aCopy.mPosition),
    mFloat(aCopy.mFloat),
    mClear(aCopy.mClear),
    mClip(aCopy.mClip),
    mOverflowX(aCopy.mOverflowX),
    mOverflowY(aCopy.mOverflowY),
    mVisibility(aCopy.mVisibility),
    mOpacity(aCopy.mOpacity),
    // temp fix for bug 24000
    mBreakBefore(aCopy.mBreakBefore),
    mBreakAfter(aCopy.mBreakAfter)
    // end temp
{
  MOZ_COUNT_CTOR(nsCSSDisplay);
}

nsCSSDisplay::~nsCSSDisplay(void)
{
  MOZ_COUNT_DTOR(nsCSSDisplay);
}

// --- nsCSSMargin -----------------

nsCSSMargin::nsCSSMargin(void)
{
  MOZ_COUNT_CTOR(nsCSSMargin);
}

nsCSSMargin::nsCSSMargin(const nsCSSMargin& aCopy)
  : mMargin(aCopy.mMargin),
    mMarginStart(aCopy.mMarginStart),
    mMarginEnd(aCopy.mMarginEnd),
    mMarginLeftLTRSource(aCopy.mMarginLeftLTRSource),
    mMarginLeftRTLSource(aCopy.mMarginLeftRTLSource),
    mMarginRightLTRSource(aCopy.mMarginRightLTRSource),
    mMarginRightRTLSource(aCopy.mMarginRightRTLSource),
    mPadding(aCopy.mPadding), 
    mPaddingStart(aCopy.mPaddingStart),
    mPaddingEnd(aCopy.mPaddingEnd),
    mPaddingLeftLTRSource(aCopy.mPaddingLeftLTRSource),
    mPaddingLeftRTLSource(aCopy.mPaddingLeftRTLSource),
    mPaddingRightLTRSource(aCopy.mPaddingRightLTRSource),
    mPaddingRightRTLSource(aCopy.mPaddingRightRTLSource),
    mBorderWidth(aCopy.mBorderWidth),
    mBorderColor(aCopy.mBorderColor),
    mBorderColors(aCopy.mBorderColors),
    mBorderStyle(aCopy.mBorderStyle),
    mBorderRadius(aCopy.mBorderRadius),
    mOutlineWidth(aCopy.mOutlineWidth),
    mOutlineColor(aCopy.mOutlineColor),
    mOutlineStyle(aCopy.mOutlineStyle),
    mOutlineOffset(aCopy.mOutlineOffset),
    mOutlineRadius(aCopy.mOutlineRadius),
    mFloatEdge(aCopy.mFloatEdge)
{
  MOZ_COUNT_CTOR(nsCSSMargin);
}

nsCSSMargin::~nsCSSMargin(void)
{
  MOZ_COUNT_DTOR(nsCSSMargin);
}

// --- nsCSSPosition -----------------

nsCSSPosition::nsCSSPosition(void)
{
  MOZ_COUNT_CTOR(nsCSSPosition);
}

nsCSSPosition::nsCSSPosition(const nsCSSPosition& aCopy)
  : mWidth(aCopy.mWidth),
    mMinWidth(aCopy.mMinWidth),
    mMaxWidth(aCopy.mMaxWidth),
    mHeight(aCopy.mHeight),
    mMinHeight(aCopy.mMinHeight),
    mMaxHeight(aCopy.mMaxHeight),
    mBoxSizing(aCopy.mBoxSizing),
    mOffset(aCopy.mOffset),
    mZIndex(aCopy.mZIndex)
{
  MOZ_COUNT_CTOR(nsCSSPosition);
}

nsCSSPosition::~nsCSSPosition(void)
{
  MOZ_COUNT_DTOR(nsCSSPosition);
}

// --- nsCSSList -----------------

nsCSSList::nsCSSList(void)
{
  MOZ_COUNT_CTOR(nsCSSList);
}

nsCSSList::nsCSSList(const nsCSSList& aCopy)
  : mType(aCopy.mType),
    mImage(aCopy.mImage),
    mPosition(aCopy.mPosition),
    mImageRegion(aCopy.mImageRegion)
{
  MOZ_COUNT_CTOR(nsCSSList);
}

nsCSSList::~nsCSSList(void)
{
  MOZ_COUNT_DTOR(nsCSSList);
}

// --- nsCSSTable -----------------

nsCSSTable::nsCSSTable(void)
{
  MOZ_COUNT_CTOR(nsCSSTable);
}

nsCSSTable::nsCSSTable(const nsCSSTable& aCopy)
  : mBorderCollapse(aCopy.mBorderCollapse),
    mBorderSpacing(aCopy.mBorderSpacing),
    mCaptionSide(aCopy.mCaptionSide),
    mEmptyCells(aCopy.mEmptyCells),
    mLayout(aCopy.mLayout)
{
  MOZ_COUNT_CTOR(nsCSSTable);
}

nsCSSTable::~nsCSSTable(void)
{
  MOZ_COUNT_DTOR(nsCSSTable);
}

// --- nsCSSBreaks -----------------

nsCSSBreaks::nsCSSBreaks(void)
{
  MOZ_COUNT_CTOR(nsCSSBreaks);
}

nsCSSBreaks::nsCSSBreaks(const nsCSSBreaks& aCopy)
  : mOrphans(aCopy.mOrphans),
    mWidows(aCopy.mWidows),
    mPage(aCopy.mPage),
    // temp fix for bug 24000
    //mPageBreakAfter(aCopy.mPageBreakAfter),
    //mPageBreakBefore(aCopy.mPageBreakBefore),
    mPageBreakInside(aCopy.mPageBreakInside)
{
  MOZ_COUNT_CTOR(nsCSSBreaks);
}

nsCSSBreaks::~nsCSSBreaks(void)
{
  MOZ_COUNT_DTOR(nsCSSBreaks);
}

// --- nsCSSPage -----------------

nsCSSPage::nsCSSPage(void)
{
  MOZ_COUNT_CTOR(nsCSSPage);
}

nsCSSPage::nsCSSPage(const nsCSSPage& aCopy)
  : mMarks(aCopy.mMarks),
    mSize(aCopy.mSize)
{
  MOZ_COUNT_CTOR(nsCSSPage);
}

nsCSSPage::~nsCSSPage(void)
{
  MOZ_COUNT_DTOR(nsCSSPage);
}

// --- nsCSSContent support -----------------

nsCSSCounterData::nsCSSCounterData(void)
  : mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSCounterData);
}

nsCSSCounterData::nsCSSCounterData(const nsCSSCounterData& aCopy)
  : mCounter(aCopy.mCounter),
    mValue(aCopy.mValue),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSCounterData);
  CSS_IF_COPY(mNext, nsCSSCounterData);
}

nsCSSCounterData::~nsCSSCounterData(void)
{
  MOZ_COUNT_DTOR(nsCSSCounterData);
  CSS_IF_DELETE(mNext);
}

/* static */ PRBool
nsCSSCounterData::Equal(nsCSSCounterData* aList1, nsCSSCounterData* aList2)
{
  if (aList1 == aList2)
    return PR_TRUE;

  nsCSSCounterData *p1 = aList1, *p2 = aList2;
  for ( ; p1 && p2; p1 = p1->mNext, p2 = p2->mNext) {
    if (p1->mCounter != p2->mCounter ||
        p1->mValue != p2->mValue)
      return PR_FALSE;
  }
  return !p1 && !p2; // true if same length, false otherwise
}

nsCSSQuotes::nsCSSQuotes(void)
  : mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSQuotes);
}

nsCSSQuotes::nsCSSQuotes(const nsCSSQuotes& aCopy)
  : mOpen(aCopy.mOpen),
    mClose(aCopy.mClose),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSQuotes);
  CSS_IF_COPY(mNext, nsCSSQuotes);
}

nsCSSQuotes::~nsCSSQuotes(void)
{
  MOZ_COUNT_DTOR(nsCSSQuotes);
  CSS_IF_DELETE(mNext);
}

/* static */ PRBool
nsCSSQuotes::Equal(nsCSSQuotes* aList1, nsCSSQuotes* aList2)
{
  if (aList1 == aList2)
    return PR_TRUE;

  nsCSSQuotes *p1 = aList1, *p2 = aList2;
  for ( ; p1 && p2; p1 = p1->mNext, p2 = p2->mNext) {
    if (p1->mOpen != p2->mOpen ||
        p1->mClose != p2->mClose)
      return PR_FALSE;
  }
  return !p1 && !p2; // true if same length, false otherwise
}

// --- nsCSSContent -----------------

nsCSSContent::nsCSSContent(void)
  : mContent(nsnull),
    mCounterIncrement(nsnull),
    mCounterReset(nsnull),
    mQuotes(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSContent);
}

nsCSSContent::nsCSSContent(const nsCSSContent& aCopy)
  : mContent(nsnull),
    mCounterIncrement(nsnull),
    mCounterReset(nsnull),
    mMarkerOffset(aCopy.mMarkerOffset),
    mQuotes(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSContent);
  CSS_IF_COPY(mContent, nsCSSValueList);
  CSS_IF_COPY(mCounterIncrement, nsCSSCounterData);
  CSS_IF_COPY(mCounterReset, nsCSSCounterData);
  CSS_IF_COPY(mQuotes, nsCSSQuotes);
}

nsCSSContent::~nsCSSContent(void)
{
  MOZ_COUNT_DTOR(nsCSSContent);
  CSS_IF_DELETE(mContent);
  CSS_IF_DELETE(mCounterIncrement);
  CSS_IF_DELETE(mCounterReset);
  CSS_IF_DELETE(mQuotes);
}

// --- nsCSSUserInterface -----------------

nsCSSUserInterface::nsCSSUserInterface(void)
  : mCursor(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSUserInterface);
}

nsCSSUserInterface::nsCSSUserInterface(const nsCSSUserInterface& aCopy)
  : mUserInput(aCopy.mUserInput),
    mUserModify(aCopy.mUserModify),
    mUserSelect(aCopy.mUserSelect),
    mUserFocus(aCopy.mUserFocus),
    mCursor(nsnull),
    mForceBrokenImageIcon(aCopy.mForceBrokenImageIcon)
{
  MOZ_COUNT_CTOR(nsCSSUserInterface);
  CSS_IF_COPY(mCursor, nsCSSValueList);
}

nsCSSUserInterface::~nsCSSUserInterface(void)
{
  MOZ_COUNT_DTOR(nsCSSUserInterface);
  CSS_IF_DELETE(mCursor);
}

// --- nsCSSAural -----------------

nsCSSAural::nsCSSAural(void)
{
  MOZ_COUNT_CTOR(nsCSSAural);
}

nsCSSAural::nsCSSAural(const nsCSSAural& aCopy)
  : mAzimuth(aCopy.mAzimuth),
    mElevation(aCopy.mElevation),
    mCueAfter(aCopy.mCueAfter),
    mCueBefore(aCopy.mCueBefore),
    mPauseAfter(aCopy.mPauseAfter),
    mPauseBefore(aCopy.mPauseBefore),
    mPitch(aCopy.mPitch),
    mPitchRange(aCopy.mPitchRange),
    mRichness(aCopy.mRichness),
    mSpeak(aCopy.mSpeak),
    mSpeakHeader(aCopy.mSpeakHeader),
    mSpeakNumeral(aCopy.mSpeakNumeral),
    mSpeakPunctuation(aCopy.mSpeakPunctuation),
    mSpeechRate(aCopy.mSpeechRate),
    mStress(aCopy.mStress),
    mVoiceFamily(aCopy.mVoiceFamily),
    mVolume(aCopy.mVolume)
{
  MOZ_COUNT_CTOR(nsCSSAural);
}

nsCSSAural::~nsCSSAural(void)
{
  MOZ_COUNT_DTOR(nsCSSAural);
}

// --- nsCSSXUL -----------------

nsCSSXUL::nsCSSXUL(void)
{
  MOZ_COUNT_CTOR(nsCSSXUL);
}

nsCSSXUL::nsCSSXUL(const nsCSSXUL& aCopy)
  : mBoxAlign(aCopy.mBoxAlign), mBoxDirection(aCopy.mBoxDirection),
    mBoxFlex(aCopy.mBoxFlex), mBoxOrient(aCopy.mBoxOrient),
    mBoxPack(aCopy.mBoxPack), mBoxOrdinal(aCopy.mBoxOrdinal)
{
  MOZ_COUNT_CTOR(nsCSSXUL);
}

nsCSSXUL::~nsCSSXUL(void)
{
  MOZ_COUNT_DTOR(nsCSSXUL);
}

// --- nsCSSColumn -----------------

nsCSSColumn::nsCSSColumn(void)
{
  MOZ_COUNT_CTOR(nsCSSColumn);
}

nsCSSColumn::nsCSSColumn(const nsCSSColumn& aCopy)
  : mColumnCount(aCopy.mColumnCount), mColumnWidth(aCopy.mColumnWidth),
    mColumnGap(aCopy.mColumnGap)
{
  MOZ_COUNT_CTOR(nsCSSColumn);
}

nsCSSColumn::~nsCSSColumn(void)
{
  MOZ_COUNT_DTOR(nsCSSColumn);
}

#ifdef MOZ_SVG
// --- nsCSSSVG -----------------

nsCSSSVG::nsCSSSVG(void) : mStrokeDasharray(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSSVG);
}

nsCSSSVG::nsCSSSVG(const nsCSSSVG& aCopy)
    : mClipPath(aCopy.mClipPath),
      mClipRule(aCopy.mClipRule),
      mDominantBaseline(aCopy.mDominantBaseline),
      mFill(aCopy.mFill),
      mFillOpacity(aCopy.mFillOpacity),
      mFillRule(aCopy.mFillRule),
      mFilter(aCopy.mFilter),
      mFloodColor(aCopy.mFloodColor),
      mFloodOpacity(aCopy.mFloodOpacity),
      mMarkerEnd(aCopy.mMarkerEnd),
      mMarkerMid(aCopy.mMarkerMid),
      mMarkerStart(aCopy.mMarkerStart),
      mMask(aCopy.mMask),
      mPointerEvents(aCopy.mPointerEvents),
      mShapeRendering(aCopy.mShapeRendering),
      mStopColor(aCopy.mStopColor),
      mStopOpacity(aCopy.mStopOpacity),
      mStroke(aCopy.mStroke),
      mStrokeDasharray(nsnull),
      mStrokeDashoffset(aCopy.mStrokeDashoffset),
      mStrokeLinecap(aCopy.mStrokeLinecap),
      mStrokeLinejoin(aCopy.mStrokeLinejoin),
      mStrokeMiterlimit(aCopy.mStrokeMiterlimit),
      mStrokeOpacity(aCopy.mStrokeOpacity),
      mStrokeWidth(aCopy.mStrokeWidth),
      mTextAnchor(aCopy.mTextAnchor),
      mTextRendering(aCopy.mTextRendering)
{
  MOZ_COUNT_CTOR(nsCSSSVG);
  CSS_IF_COPY(mStrokeDasharray, nsCSSValueList);
}

nsCSSSVG::~nsCSSSVG(void)
{
  MOZ_COUNT_DTOR(nsCSSSVG);
  CSS_IF_DELETE(mStrokeDasharray);
}

#endif // MOZ_SVG
