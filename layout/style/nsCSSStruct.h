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

#ifndef nsCSSStruct_h___
#define nsCSSStruct_h___

#include "nsCSSValue.h"
#include <stdio.h>

struct nsCSSStruct {
  // EMPTY on purpose.  ABSTRACT with no virtuals (typedef void nsCSSStruct?)
};

// We use the nsCSS* structures for storing nsCSSDeclaration's
// *temporary* data during parsing and modification.  (They are too big
// for permanent storage.)  We also use them for nsRuleData, with some
// additions of things that the style system must cascade, but that
// aren't CSS properties.  Thus we use typedefs and inheritance
// (forwards, when the rule data needs extra data) to make the rule data
// structs from the declaration structs.
// NOTE:  For compilation speed, this typedef also appears in nsRuleNode.h
typedef nsCSSStruct nsRuleDataStruct;


struct nsCSSFont : public nsCSSStruct {
  nsCSSFont(void);
  nsCSSFont(const nsCSSFont& aCopy);
  ~nsCSSFont(void);

  nsCSSValue mFamily;
  nsCSSValue mStyle;
  nsCSSValue mVariant;
  nsCSSValue mWeight;
  nsCSSValue mSize;
  nsCSSValue mSizeAdjust; // NEW
  nsCSSValue mStretch; // NEW
};

struct nsRuleDataFont : public nsCSSFont {
  PRBool mFamilyFromHTML; // Is the family from an HTML FONT element
};

// Prefer nsCSSValue::Array for lists of fixed size.
struct nsCSSValueList {
  nsCSSValueList(void);
  nsCSSValueList(const nsCSSValueList& aCopy);
  ~nsCSSValueList(void);

  static PRBool Equal(nsCSSValueList* aList1, nsCSSValueList* aList2);

  nsCSSValue      mValue;
  nsCSSValueList* mNext;
};

struct nsCSSColor : public nsCSSStruct  {
  nsCSSColor(void);
  nsCSSColor(const nsCSSColor& aCopy);
  ~nsCSSColor(void);

  nsCSSValue      mColor;
  nsCSSValue      mBackColor;
  nsCSSValue      mBackImage;
  nsCSSValue      mBackRepeat;
  nsCSSValue      mBackAttachment;
  nsCSSValue      mBackPositionX;
  nsCSSValue      mBackPositionY;
  nsCSSValue      mBackClip;
  nsCSSValue      mBackOrigin;
  nsCSSValue      mBackInlinePolicy;
};

struct nsRuleDataColor : public nsCSSColor {
};

struct nsCSSText : public nsCSSStruct  {
  nsCSSText(void);
  nsCSSText(const nsCSSText& aCopy);
  ~nsCSSText(void);

  nsCSSValue mWordSpacing;
  nsCSSValue mLetterSpacing;
  nsCSSValue mVerticalAlign;
  nsCSSValue mTextTransform;
  nsCSSValue mTextAlign;
  nsCSSValue mTextIndent;
  nsCSSValue mDecoration;
  nsCSSValueList* mTextShadow; // NEW
  nsCSSValue mUnicodeBidi;  // NEW
  nsCSSValue mLineHeight;
  nsCSSValue mWhiteSpace;
};

struct nsRuleDataText : public nsCSSText {
};

struct nsCSSRect {
  nsCSSRect(void);
  nsCSSRect(const nsCSSRect& aCopy);
  ~nsCSSRect();

  PRBool operator==(const nsCSSRect& aOther) const {
    return mTop == aOther.mTop &&
           mRight == aOther.mRight &&
           mBottom == aOther.mBottom &&
           mLeft == aOther.mLeft;
  }

  PRBool operator!=(const nsCSSRect& aOther) const {
    return mTop != aOther.mTop ||
           mRight != aOther.mRight ||
           mBottom != aOther.mBottom ||
           mLeft != aOther.mLeft;
  }

  void SetAllSidesTo(const nsCSSValue& aValue);

  void Reset() {
    mTop.Reset();
    mRight.Reset();
    mBottom.Reset();
    mLeft.Reset();
  }

  PRBool HasValue() const {
    return
      mTop.GetUnit() != eCSSUnit_Null ||
      mRight.GetUnit() != eCSSUnit_Null ||
      mBottom.GetUnit() != eCSSUnit_Null ||
      mLeft.GetUnit() != eCSSUnit_Null;
  }
  
  nsCSSValue mTop;
  nsCSSValue mRight;
  nsCSSValue mBottom;
  nsCSSValue mLeft;

  typedef nsCSSValue nsCSSRect::*side_type;
  static const side_type sides[4];
};

struct nsCSSValuePair {
  nsCSSValuePair()
  {
    MOZ_COUNT_CTOR(nsCSSValuePair);
  }
  nsCSSValuePair(const nsCSSValuePair& aCopy)
    : mXValue(aCopy.mXValue),
      mYValue(aCopy.mYValue)
  { 
    MOZ_COUNT_CTOR(nsCSSValuePair);
  }
  ~nsCSSValuePair()
  {
    MOZ_COUNT_DTOR(nsCSSValuePair);
  }

  PRBool operator==(const nsCSSValuePair& aOther) const {
    return mXValue == aOther.mXValue &&
           mYValue == aOther.mYValue;
  }

  PRBool operator!=(const nsCSSValuePair& aOther) const {
    return mXValue != aOther.mXValue ||
           mYValue != aOther.mYValue;
  }

  void SetBothValuesTo(const nsCSSValue& aValue) {
    mXValue = aValue;
    mYValue = aValue;
  }

  nsCSSValue mXValue;
  nsCSSValue mYValue;
};

struct nsCSSValueListRect {
  nsCSSValueListRect(void);
  nsCSSValueListRect(const nsCSSValueListRect& aCopy);
  ~nsCSSValueListRect();

  nsCSSValueList* mTop;
  nsCSSValueList* mRight;
  nsCSSValueList* mBottom;
  nsCSSValueList* mLeft;

  typedef nsCSSValueList* nsCSSValueListRect::*side_type;
  static const side_type sides[4];
};

struct nsCSSDisplay : public nsCSSStruct  {
  nsCSSDisplay(void);
  nsCSSDisplay(const nsCSSDisplay& aCopy);
  ~nsCSSDisplay(void);

  nsCSSValue mDirection;
  nsCSSValue mDisplay;
  nsCSSValue mBinding;
  nsCSSValue mAppearance;
  nsCSSValue mPosition;
  nsCSSValue mFloat;
  nsCSSValue mClear;
  nsCSSRect  mClip;
  nsCSSValue mOverflowX;
  nsCSSValue mOverflowY;
  nsCSSValue mVisibility;
  nsCSSValue mOpacity;

  // temp fix for bug 24000 
  nsCSSValue mBreakBefore;
  nsCSSValue mBreakAfter;
  // end temp fix
};

struct nsRuleDataDisplay : public nsCSSDisplay {
  nsCSSValue mLang;
};

struct nsCSSMargin : public nsCSSStruct  {
  nsCSSMargin(void);
  nsCSSMargin(const nsCSSMargin& aCopy);
  ~nsCSSMargin(void);

  nsCSSRect   mMargin;
  nsCSSValue  mMarginStart;
  nsCSSValue  mMarginEnd;
  nsCSSValue  mMarginLeftLTRSource;
  nsCSSValue  mMarginLeftRTLSource;
  nsCSSValue  mMarginRightLTRSource;
  nsCSSValue  mMarginRightRTLSource;
  nsCSSRect   mPadding;
  nsCSSValue  mPaddingStart;
  nsCSSValue  mPaddingEnd;
  nsCSSValue  mPaddingLeftLTRSource;
  nsCSSValue  mPaddingLeftRTLSource;
  nsCSSValue  mPaddingRightLTRSource;
  nsCSSValue  mPaddingRightRTLSource;
  nsCSSRect   mBorderWidth;
  nsCSSRect   mBorderColor;
  nsCSSValueListRect mBorderColors;
  nsCSSRect   mBorderStyle;
  nsCSSRect   mBorderRadius;  // (extension)
  nsCSSValue  mOutlineWidth;
  nsCSSValue  mOutlineColor;
  nsCSSValue  mOutlineStyle;
  nsCSSValue  mOutlineOffset;
  nsCSSRect   mOutlineRadius; // (extension)
  nsCSSValue  mFloatEdge; // NEW
};

struct nsRuleDataMargin : public nsCSSMargin {
};

struct nsCSSPosition : public nsCSSStruct  {
  nsCSSPosition(void);
  nsCSSPosition(const nsCSSPosition& aCopy);
  ~nsCSSPosition(void);

  nsCSSValue  mWidth;
  nsCSSValue  mMinWidth;
  nsCSSValue  mMaxWidth;
  nsCSSValue  mHeight;
  nsCSSValue  mMinHeight;
  nsCSSValue  mMaxHeight;
  nsCSSValue  mBoxSizing; // NEW
  nsCSSRect   mOffset;
  nsCSSValue  mZIndex;
};

struct nsRuleDataPosition : public nsCSSPosition {
};

struct nsCSSList : public nsCSSStruct  {
  nsCSSList(void);
  nsCSSList(const nsCSSList& aCopy);
  ~nsCSSList(void);

  nsCSSValue mType;
  nsCSSValue mImage;
  nsCSSValue mPosition;
  nsCSSRect  mImageRegion;
};

struct nsRuleDataList : public nsCSSList {
};

struct nsCSSTable : public nsCSSStruct  { // NEW
  nsCSSTable(void);
  nsCSSTable(const nsCSSTable& aCopy);
  ~nsCSSTable(void);

  nsCSSValue mBorderCollapse;
  nsCSSValuePair mBorderSpacing;
  nsCSSValue mCaptionSide;
  nsCSSValue mEmptyCells;
  
  nsCSSValue mLayout;
  nsCSSValue mFrame; // Not mappable via CSS, only using HTML4 table attrs.
  nsCSSValue mRules; // Not mappable via CSS, only using HTML4 table attrs.
  nsCSSValue mSpan; // Not mappable via CSS, only using HTML4 table attrs.
  nsCSSValue mCols; // Not mappable via CSS, only using HTML4 table attrs.
};

struct nsRuleDataTable : public nsCSSTable {
};

struct nsCSSBreaks : public nsCSSStruct  { // NEW
  nsCSSBreaks(void);
  nsCSSBreaks(const nsCSSBreaks& aCopy);
  ~nsCSSBreaks(void);

  nsCSSValue mOrphans;
  nsCSSValue mWidows;
  nsCSSValue mPage;
  // temp fix for bug 24000 
  //nsCSSValue mPageBreakAfter;
  //nsCSSValue mPageBreakBefore;
  nsCSSValue mPageBreakInside;
};

struct nsRuleDataBreaks : public nsCSSBreaks {
};

struct nsCSSPage : public nsCSSStruct  { // NEW
  nsCSSPage(void);
  nsCSSPage(const nsCSSPage& aCopy);
  ~nsCSSPage(void);

  nsCSSValue mMarks;
  nsCSSValuePair mSize;
};

struct nsRuleDataPage : public nsCSSPage {
};

// Should be replaced with nsCSSValueList and nsCSSValue::Array.
struct nsCSSCounterData {
  nsCSSCounterData(void);
  nsCSSCounterData(const nsCSSCounterData& aCopy);
  ~nsCSSCounterData(void);

  static PRBool Equal(nsCSSCounterData* aList1, nsCSSCounterData* aList2);

  nsCSSValue        mCounter;
  nsCSSValue        mValue;
  nsCSSCounterData* mNext;
};

// Should be replaced with nsCSSValueList and nsCSSValue::Array.
struct nsCSSQuotes {
  nsCSSQuotes(void);
  nsCSSQuotes(const nsCSSQuotes& aCopy);
  ~nsCSSQuotes(void);

  static PRBool Equal(nsCSSQuotes* aList1, nsCSSQuotes* aList2);

  nsCSSValue    mOpen;
  nsCSSValue    mClose;
  nsCSSQuotes*  mNext;
};

struct nsCSSContent : public nsCSSStruct  {
  nsCSSContent(void);
  nsCSSContent(const nsCSSContent& aCopy);
  ~nsCSSContent(void);

  nsCSSValueList*   mContent;
  nsCSSCounterData* mCounterIncrement;
  nsCSSCounterData* mCounterReset;
  nsCSSValue        mMarkerOffset;
  nsCSSQuotes*      mQuotes;
};

struct nsRuleDataContent : public nsCSSContent {
};

struct nsCSSUserInterface : public nsCSSStruct  { // NEW
  nsCSSUserInterface(void);
  nsCSSUserInterface(const nsCSSUserInterface& aCopy);
  ~nsCSSUserInterface(void);

  nsCSSValue      mUserInput;
  nsCSSValue      mUserModify;
  nsCSSValue      mUserSelect;
  nsCSSValue      mUserFocus;
  
  nsCSSValueList* mCursor;
  nsCSSValue      mForceBrokenImageIcon;
};

struct nsRuleDataUserInterface : public nsCSSUserInterface {
};

struct nsCSSAural : public nsCSSStruct  { // NEW
  nsCSSAural(void);
  nsCSSAural(const nsCSSAural& aCopy);
  ~nsCSSAural(void);

  nsCSSValue mAzimuth;
  nsCSSValue mElevation;
  nsCSSValue mCueAfter;
  nsCSSValue mCueBefore;
  nsCSSValue mPauseAfter;
  nsCSSValue mPauseBefore;
  nsCSSValue mPitch;
  nsCSSValue mPitchRange;
  nsCSSValuePair mPlayDuring; // mXValue is URI, mYValue are flags
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

struct nsRuleDataAural : public nsCSSAural {
};

struct nsCSSXUL : public nsCSSStruct  {
  nsCSSXUL(void);
  nsCSSXUL(const nsCSSXUL& aCopy);
  ~nsCSSXUL(void);

  nsCSSValue  mBoxAlign;
  nsCSSValue  mBoxDirection;
  nsCSSValue  mBoxFlex;
  nsCSSValue  mBoxOrient;
  nsCSSValue  mBoxPack;
  nsCSSValue  mBoxOrdinal;
};

struct nsRuleDataXUL : public nsCSSXUL {
};

struct nsCSSColumn : public nsCSSStruct  {
  nsCSSColumn(void);
  nsCSSColumn(const nsCSSColumn& aCopy);
  ~nsCSSColumn(void);

  nsCSSValue  mColumnCount;
  nsCSSValue  mColumnWidth;
  nsCSSValue  mColumnGap;
};

struct nsRuleDataColumn : public nsCSSColumn {
};

#ifdef MOZ_SVG
struct nsCSSSVG : public nsCSSStruct {
  nsCSSSVG(void);
  nsCSSSVG(const nsCSSSVG& aCopy);
  ~nsCSSSVG(void);

  nsCSSValue mClipPath;
  nsCSSValue mClipRule;
  nsCSSValue mColorInterpolation;
  nsCSSValue mColorInterpolationFilters;
  nsCSSValue mDominantBaseline;
  nsCSSValuePair mFill;
  nsCSSValue mFillOpacity;
  nsCSSValue mFillRule;
  nsCSSValue mFilter;
  nsCSSValue mFloodColor;
  nsCSSValue mFloodOpacity;
  nsCSSValue mMarkerEnd;
  nsCSSValue mMarkerMid;
  nsCSSValue mMarkerStart;
  nsCSSValue mMask;
  nsCSSValue mPointerEvents;
  nsCSSValue mShapeRendering;
  nsCSSValue mStopColor;
  nsCSSValue mStopOpacity;
  nsCSSValuePair mStroke;
  nsCSSValueList *mStrokeDasharray;
  nsCSSValue mStrokeDashoffset;
  nsCSSValue mStrokeLinecap;
  nsCSSValue mStrokeLinejoin;
  nsCSSValue mStrokeMiterlimit;
  nsCSSValue mStrokeOpacity;
  nsCSSValue mStrokeWidth;
  nsCSSValue mTextAnchor;
  nsCSSValue mTextRendering;
};

struct nsRuleDataSVG : public nsCSSSVG {
};
#endif

#endif /* nsCSSStruct_h___ */
