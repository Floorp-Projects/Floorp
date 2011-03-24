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
#include "nsStyleConsts.h"

struct nsCSSCornerSizes {
  nsCSSCornerSizes(void);
  nsCSSCornerSizes(const nsCSSCornerSizes& aCopy);
  ~nsCSSCornerSizes();

  // argument is a "full corner" constant from nsStyleConsts.h
  nsCSSValue const & GetCorner(PRUint32 aCorner) const {
    return this->*corners[aCorner];
  }
  nsCSSValue & GetCorner(PRUint32 aCorner) {
    return this->*corners[aCorner];
  }

  PRBool operator==(const nsCSSCornerSizes& aOther) const {
    NS_FOR_CSS_FULL_CORNERS(corner) {
      if (this->GetCorner(corner) != aOther.GetCorner(corner))
        return PR_FALSE;
    }
    return PR_TRUE;
  }

  PRBool operator!=(const nsCSSCornerSizes& aOther) const {
    NS_FOR_CSS_FULL_CORNERS(corner) {
      if (this->GetCorner(corner) != aOther.GetCorner(corner))
        return PR_TRUE;
    }
    return PR_FALSE;
  }

  PRBool HasValue() const {
    NS_FOR_CSS_FULL_CORNERS(corner) {
      if (this->GetCorner(corner).GetUnit() != eCSSUnit_Null)
        return PR_TRUE;
    }
    return PR_FALSE;
  }

  void Reset();

  nsCSSValue mTopLeft;
  nsCSSValue mTopRight;
  nsCSSValue mBottomRight;
  nsCSSValue mBottomLeft;

protected:
  typedef nsCSSValue nsCSSCornerSizes::*corner_type;
  static const corner_type corners[4];
};

/****************************************************************************/

struct nsCSSStruct {
  // EMPTY on purpose.  ABSTRACT with no virtuals (typedef void nsCSSStruct?)
};

// We use the nsCSS* structures for storing css::Declaration's
// *temporary* data during parsing and modification.

struct nsCSSFont : public nsCSSStruct {
  nsCSSFont(void);
  ~nsCSSFont(void);

  nsCSSValue mSystemFont;
  nsCSSValue mFamily;
  nsCSSValue mStyle;
  nsCSSValue mVariant;
  nsCSSValue mWeight;
  nsCSSValue mSize;
  nsCSSValue mSizeAdjust; // NEW
  nsCSSValue mStretch; // NEW
  nsCSSValue mFontFeatureSettings;
  nsCSSValue mFontLanguageOverride;

#ifdef MOZ_MATHML
  nsCSSValue mScriptLevel; // Integer values mean "relative", Number values mean "absolute" 
  nsCSSValue mScriptSizeMultiplier;
  nsCSSValue mScriptMinSize;
#endif

private:
  nsCSSFont(const nsCSSFont& aOther); // NOT IMPLEMENTED
};

struct nsCSSColor : public nsCSSStruct  {
  nsCSSColor(void);
  ~nsCSSColor(void);

  nsCSSValue mColor;
  nsCSSValue mBackColor;
  nsCSSValue mBackImage;
  nsCSSValue mBackRepeat;
  nsCSSValue mBackAttachment;
  nsCSSValue mBackPosition;
  nsCSSValue mBackSize;
  nsCSSValue mBackClip;
  nsCSSValue mBackOrigin;
  nsCSSValue mBackInlinePolicy;
private:
  nsCSSColor(const nsCSSColor& aOther); // NOT IMPLEMENTED
};

struct nsCSSText : public nsCSSStruct  {
  nsCSSText(void);
  ~nsCSSText(void);

  nsCSSValue mTabSize;
  nsCSSValue mWordSpacing;
  nsCSSValue mLetterSpacing;
  nsCSSValue mVerticalAlign;
  nsCSSValue mTextTransform;
  nsCSSValue mTextAlign;
  nsCSSValue mTextIndent;
  nsCSSValue mDecoration;
  nsCSSValue mTextShadow; // NEW
  nsCSSValue mUnicodeBidi;  // NEW
  nsCSSValue mLineHeight;
  nsCSSValue mWhiteSpace;
  nsCSSValue mWordWrap;
private:
  nsCSSText(const nsCSSText& aOther); // NOT IMPLEMENTED
};

struct nsCSSDisplay : public nsCSSStruct  {
  nsCSSDisplay(void);
  ~nsCSSDisplay(void);

  nsCSSValue mDirection;
  nsCSSValue mDisplay;
  nsCSSValue mBinding;
  nsCSSValue mAppearance;
  nsCSSValue mPosition;
  nsCSSValue mFloat;
  nsCSSValue mClear;
  nsCSSValue mClip;
  nsCSSValue mOverflowX;
  nsCSSValue mOverflowY;
  nsCSSValue mResize;
  nsCSSValue mPointerEvents;
  nsCSSValue mVisibility;
  nsCSSValue mOpacity;
  nsCSSValue mTransform; // List of Arrays containing transform information
  nsCSSValue mTransformOrigin;
  nsCSSValue mTransitionProperty;
  nsCSSValue mTransitionDuration;
  nsCSSValue mTransitionTimingFunction;
  nsCSSValue mTransitionDelay;

  // temp fix for bug 24000 
  nsCSSValue mBreakBefore;
  nsCSSValue mBreakAfter;
  // end temp fix
private:
  nsCSSDisplay(const nsCSSDisplay& aOther); // NOT IMPLEMENTED
};

struct nsCSSMargin : public nsCSSStruct  {
  nsCSSMargin(void);
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
  nsCSSValue  mBorderStartWidth;
  nsCSSValue  mBorderEndWidth;
  nsCSSValue  mBorderLeftWidthLTRSource;
  nsCSSValue  mBorderLeftWidthRTLSource;
  nsCSSValue  mBorderRightWidthLTRSource;
  nsCSSValue  mBorderRightWidthRTLSource;
  nsCSSRect   mBorderColor;
  nsCSSValue  mBorderStartColor;
  nsCSSValue  mBorderEndColor;
  nsCSSValue  mBorderLeftColorLTRSource;
  nsCSSValue  mBorderLeftColorRTLSource;
  nsCSSValue  mBorderRightColorLTRSource;
  nsCSSValue  mBorderRightColorRTLSource;
  nsCSSRect   mBorderColors;
  nsCSSRect   mBorderStyle;
  nsCSSValue  mBorderStartStyle;
  nsCSSValue  mBorderEndStyle;
  nsCSSValue  mBorderLeftStyleLTRSource;
  nsCSSValue  mBorderLeftStyleRTLSource;
  nsCSSValue  mBorderRightStyleLTRSource;
  nsCSSValue  mBorderRightStyleRTLSource;
  nsCSSCornerSizes mBorderRadius;
  nsCSSValue  mOutlineWidth;
  nsCSSValue  mOutlineColor;
  nsCSSValue  mOutlineStyle;
  nsCSSValue  mOutlineOffset;
  nsCSSCornerSizes mOutlineRadius;
  nsCSSValue  mFloatEdge; // NEW
  nsCSSValue  mBorderImage;
  nsCSSValue  mBoxShadow;
private:
  nsCSSMargin(const nsCSSMargin& aOther); // NOT IMPLEMENTED
};

struct nsCSSPosition : public nsCSSStruct  {
  nsCSSPosition(void);
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
private:
  nsCSSPosition(const nsCSSPosition& aOther); // NOT IMPLEMENTED
};

struct nsCSSList : public nsCSSStruct  {
  nsCSSList(void);
  ~nsCSSList(void);

  nsCSSValue mType;
  nsCSSValue mImage;
  nsCSSValue mPosition;
  nsCSSValue mImageRegion;
private:
  nsCSSList(const nsCSSList& aOther); // NOT IMPLEMENTED
};

struct nsCSSTable : public nsCSSStruct  { // NEW
  nsCSSTable(void);
  ~nsCSSTable(void);

  nsCSSValue mBorderCollapse;
  nsCSSValue mBorderSpacing;
  nsCSSValue mCaptionSide;
  nsCSSValue mEmptyCells;

  nsCSSValue mLayout;
private:
  nsCSSTable(const nsCSSTable& aOther); // NOT IMPLEMENTED
};

struct nsCSSBreaks : public nsCSSStruct  { // NEW
  nsCSSBreaks(void);
  ~nsCSSBreaks(void);

  nsCSSValue mOrphans;
  nsCSSValue mWidows;
  nsCSSValue mPage;
  // temp fix for bug 24000 
  //nsCSSValue mPageBreakAfter;
  //nsCSSValue mPageBreakBefore;
  nsCSSValue mPageBreakInside;
private:
  nsCSSBreaks(const nsCSSBreaks& aOther); // NOT IMPLEMENTED
};

struct nsCSSPage : public nsCSSStruct  { // NEW
  nsCSSPage(void);
  ~nsCSSPage(void);

  nsCSSValue mMarks;
  nsCSSValue mSize;
private:
  nsCSSPage(const nsCSSPage& aOther); // NOT IMPLEMENTED
};

struct nsCSSContent : public nsCSSStruct  {
  nsCSSContent(void);
  ~nsCSSContent(void);

  nsCSSValue mContent;
  nsCSSValue mCounterIncrement;
  nsCSSValue mCounterReset;
  nsCSSValue mMarkerOffset;
  nsCSSValue mQuotes;
private:
  nsCSSContent(const nsCSSContent& aOther); // NOT IMPLEMENTED
};

struct nsCSSUserInterface : public nsCSSStruct  { // NEW
  nsCSSUserInterface(void);
  ~nsCSSUserInterface(void);

  nsCSSValue mUserInput;
  nsCSSValue mUserModify;
  nsCSSValue mUserSelect;
  nsCSSValue mUserFocus;

  nsCSSValue mCursor;
  nsCSSValue mForceBrokenImageIcon;
  nsCSSValue mIMEMode;
  nsCSSValue mWindowShadow;
private:
  nsCSSUserInterface(const nsCSSUserInterface& aOther); // NOT IMPLEMENTED
};

struct nsCSSAural : public nsCSSStruct  { // NEW
  nsCSSAural(void);
  ~nsCSSAural(void);

  nsCSSValue mAzimuth;
  nsCSSValue mElevation;
  nsCSSValue mCueAfter;
  nsCSSValue mCueBefore;
  nsCSSValue mPauseAfter;
  nsCSSValue mPauseBefore;
  nsCSSValue mPitch;
  nsCSSValue mPitchRange;
  nsCSSValue mRichness;
  nsCSSValue mSpeak;
  nsCSSValue mSpeakHeader;
  nsCSSValue mSpeakNumeral;
  nsCSSValue mSpeakPunctuation;
  nsCSSValue mSpeechRate;
  nsCSSValue mStress;
  nsCSSValue mVoiceFamily;
  nsCSSValue mVolume;
private:
  nsCSSAural(const nsCSSAural& aOther); // NOT IMPLEMENTED
};

struct nsCSSXUL : public nsCSSStruct  {
  nsCSSXUL(void);
  ~nsCSSXUL(void);

  nsCSSValue  mBoxAlign;
  nsCSSValue  mBoxDirection;
  nsCSSValue  mBoxFlex;
  nsCSSValue  mBoxOrient;
  nsCSSValue  mBoxPack;
  nsCSSValue  mBoxOrdinal;
  nsCSSValue  mStackSizing;
private:
  nsCSSXUL(const nsCSSXUL& aOther); // NOT IMPLEMENTED
};

struct nsCSSColumn : public nsCSSStruct  {
  nsCSSColumn(void);
  ~nsCSSColumn(void);

  nsCSSValue  mColumnCount;
  nsCSSValue  mColumnWidth;
  nsCSSValue  mColumnGap;
  nsCSSValue  mColumnRuleColor;
  nsCSSValue  mColumnRuleWidth;
  nsCSSValue  mColumnRuleStyle;
private:
  nsCSSColumn(const nsCSSColumn& aOther); // NOT IMPLEMENTED
};

struct nsCSSSVG : public nsCSSStruct {
  nsCSSSVG(void);
  ~nsCSSSVG(void);

  nsCSSValue mClipPath;
  nsCSSValue mClipRule;
  nsCSSValue mColorInterpolation;
  nsCSSValue mColorInterpolationFilters;
  nsCSSValue mDominantBaseline;
  nsCSSValue mFill;
  nsCSSValue mFillOpacity;
  nsCSSValue mFillRule;
  nsCSSValue mFilter;
  nsCSSValue mFloodColor;
  nsCSSValue mFloodOpacity;
  nsCSSValue mImageRendering;
  nsCSSValue mLightingColor;
  nsCSSValue mMarkerEnd;
  nsCSSValue mMarkerMid;
  nsCSSValue mMarkerStart;
  nsCSSValue mMask;
  nsCSSValue mShapeRendering;
  nsCSSValue mStopColor;
  nsCSSValue mStopOpacity;
  nsCSSValue mStroke;
  nsCSSValue mStrokeDasharray;
  nsCSSValue mStrokeDashoffset;
  nsCSSValue mStrokeLinecap;
  nsCSSValue mStrokeLinejoin;
  nsCSSValue mStrokeMiterlimit;
  nsCSSValue mStrokeOpacity;
  nsCSSValue mStrokeWidth;
  nsCSSValue mTextAnchor;
  nsCSSValue mTextRendering;
private:
  nsCSSSVG(const nsCSSSVG& aOther); // NOT IMPLEMENTED
};

#endif /* nsCSSStruct_h___ */
