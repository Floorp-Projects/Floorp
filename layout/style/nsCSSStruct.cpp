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
#include "nscore.h"
#include "nsCSSStruct.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsCSSProps.h"
#include "nsUnitConversion.h"
#include "nsVoidArray.h"
#include "nsFont.h"

#include "nsStyleConsts.h"

#include "nsCOMPtr.h"

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

#ifdef DEBUG
void nsCSSFont::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mFamily.AppendToString(buffer, eCSSProperty_font_family);
  mStyle.AppendToString(buffer, eCSSProperty_font_style);
  mVariant.AppendToString(buffer, eCSSProperty_font_variant);
  mWeight.AppendToString(buffer, eCSSProperty_font_weight);
  mSize.AppendToString(buffer, eCSSProperty_font_size);
  mSizeAdjust.AppendToString(buffer, eCSSProperty_font_size_adjust);
  mStretch.AppendToString(buffer, eCSSProperty_font_stretch);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

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
    mBackPositionX(aCopy.mBackPositionX),
    mBackPositionY(aCopy.mBackPositionY),
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

#ifdef DEBUG
void nsCSSColor::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mColor.AppendToString(buffer, eCSSProperty_color);
  mBackColor.AppendToString(buffer, eCSSProperty_background_color);
  mBackImage.AppendToString(buffer, eCSSProperty_background_image);
  mBackRepeat.AppendToString(buffer, eCSSProperty_background_repeat);
  mBackAttachment.AppendToString(buffer, eCSSProperty_background_attachment);
  mBackPositionX.AppendToString(buffer, eCSSProperty_background_x_position);
  mBackPositionY.AppendToString(buffer, eCSSProperty_background_y_position);
  mBackClip.AppendToString(buffer, eCSSProperty__moz_background_clip);
  mBackOrigin.AppendToString(buffer, eCSSProperty__moz_background_origin);
  mBackInlinePolicy.AppendToString(buffer, eCSSProperty__moz_background_inline_policy);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

// --- nsCSSText support -----------------

nsCSSShadow::nsCSSShadow(void)
  : mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSShadow);
}

nsCSSShadow::nsCSSShadow(const nsCSSShadow& aCopy)
  : mColor(aCopy.mColor),
    mXOffset(aCopy.mXOffset),
    mYOffset(aCopy.mYOffset),
    mRadius(aCopy.mRadius),
    mNext(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSShadow);
  CSS_IF_COPY(mNext, nsCSSShadow);
}

nsCSSShadow::~nsCSSShadow(void)
{
  MOZ_COUNT_DTOR(nsCSSShadow);
  CSS_IF_DELETE(mNext);
}

/* static */ PRBool
nsCSSShadow::Equal(nsCSSShadow* aList1, nsCSSShadow* aList2)
{
  if (aList1 == aList2)
    return PR_TRUE;

  nsCSSShadow *p1 = aList1, *p2 = aList2;
  for ( ; p1 && p2; p1 = p1->mNext, p2 = p2->mNext) {
    if (p1->mColor != p2->mColor ||
        p1->mXOffset != p2->mXOffset ||
        p1->mYOffset != p2->mYOffset ||
        p1->mRadius != p2->mRadius)
      return PR_FALSE;
  }
  return !p1 && !p2; // true if same length, false otherwise
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
}

nsCSSText::~nsCSSText(void)
{
  MOZ_COUNT_DTOR(nsCSSText);
  CSS_IF_DELETE(mTextShadow);
}

#ifdef DEBUG
void nsCSSText::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mWordSpacing.AppendToString(buffer, eCSSProperty_word_spacing);
  mLetterSpacing.AppendToString(buffer, eCSSProperty_letter_spacing);
  mDecoration.AppendToString(buffer, eCSSProperty_text_decoration);
  mVerticalAlign.AppendToString(buffer, eCSSProperty_vertical_align);
  mTextTransform.AppendToString(buffer, eCSSProperty_text_transform);
  mTextAlign.AppendToString(buffer, eCSSProperty_text_align);
  mTextIndent.AppendToString(buffer, eCSSProperty_text_indent);
  if (nsnull != mTextShadow) {
    if (mTextShadow->mXOffset.IsLengthUnit()) {
      nsCSSShadow*  shadow = mTextShadow;
      // XXX This prints the property name many times, but nobody cares.
      while (nsnull != shadow) {
        shadow->mColor.AppendToString(buffer, eCSSProperty_text_shadow);
        shadow->mXOffset.AppendToString(buffer, eCSSProperty_text_shadow);
        shadow->mYOffset.AppendToString(buffer, eCSSProperty_text_shadow);
        shadow->mRadius.AppendToString(buffer, eCSSProperty_text_shadow);
        shadow = shadow->mNext;
      }
    }
    else {
      mTextShadow->mXOffset.AppendToString(buffer, eCSSProperty_text_shadow);
    }
  }
  mUnicodeBidi.AppendToString(buffer, eCSSProperty_unicode_bidi);
  mLineHeight.AppendToString(buffer, eCSSProperty_line_height);
  mWhiteSpace.AppendToString(buffer, eCSSProperty_white_space);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

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

#ifdef DEBUG
void nsCSSRect::List(FILE* out, nsCSSProperty aPropID, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  if (eCSSProperty_UNKNOWN < aPropID) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aPropID).get());
    buffer.AppendLiteral(": ");
  }

  mTop.AppendToString(buffer);
  mRight.AppendToString(buffer);
  mBottom.AppendToString(buffer); 
  mLeft.AppendToString(buffer);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}

void nsCSSRect::List(FILE* out, PRInt32 aIndent, const nsCSSProperty aTRBL[]) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  if (eCSSUnit_Null != mTop.GetUnit()) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aTRBL[0]).get());
    buffer.AppendLiteral(": ");
    mTop.AppendToString(buffer);
  }
  if (eCSSUnit_Null != mRight.GetUnit()) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aTRBL[1]).get());
    buffer.AppendLiteral(": ");
    mRight.AppendToString(buffer);
  }
  if (eCSSUnit_Null != mBottom.GetUnit()) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aTRBL[2]).get());
    buffer.AppendLiteral(": ");
    mBottom.AppendToString(buffer); 
  }
  if (eCSSUnit_Null != mLeft.GetUnit()) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aTRBL[3]).get());
    buffer.AppendLiteral(": ");
    mLeft.AppendToString(buffer);
  }

  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

// --- nsCSSValueListRect -----------------

MOZ_DECL_CTOR_COUNTER(nsCSSValueListRect)

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

#ifdef DEBUG
void nsCSSValueListRect::List(FILE* out, nsCSSProperty aPropID, PRInt32 aIndent) const
{
#if 0
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  if (eCSSProperty_UNKNOWN < aPropID) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aPropID).get());
    buffer.AppendLiteral(": ");
  }

  mTop.AppendToString(buffer);
  mRight.AppendToString(buffer);
  mBottom.AppendToString(buffer); 
  mLeft.AppendToString(buffer);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
#endif
}

void nsCSSValueListRect::List(FILE* out, PRInt32 aIndent, const nsCSSProperty aTRBL[]) const
{
#if 0
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  if (eCSSUnit_Null != mTop.GetUnit()) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aTRBL[0]).get());
    buffer.AppendLiteral(": ");
    mTop.AppendToString(buffer);
  }
  if (eCSSUnit_Null != mRight.GetUnit()) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aTRBL[1]).get());
    buffer.AppendLiteral(": ");
    mRight.AppendToString(buffer);
  }
  if (eCSSUnit_Null != mBottom.GetUnit()) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aTRBL[2]).get());
    buffer.AppendLiteral(": ");
    mBottom.AppendToString(buffer); 
  }
  if (eCSSUnit_Null != mLeft.GetUnit()) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aTRBL[3]).get());
    buffer.AppendLiteral(": ");
    mLeft.AppendToString(buffer);
  }

  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
#endif
}
#endif

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
    mOverflow(aCopy.mOverflow),
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

#ifdef DEBUG
void nsCSSDisplay::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mAppearance.AppendToString(buffer, eCSSProperty_appearance);
  mDirection.AppendToString(buffer, eCSSProperty_direction);
  mDisplay.AppendToString(buffer, eCSSProperty_display);
  mBinding.AppendToString(buffer, eCSSProperty_binding);
  mPosition.AppendToString(buffer, eCSSProperty_position);
  mFloat.AppendToString(buffer, eCSSProperty_float);
  mClear.AppendToString(buffer, eCSSProperty_clear);
  mVisibility.AppendToString(buffer, eCSSProperty_visibility);
  mOpacity.AppendToString(buffer, eCSSProperty_opacity);

  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
  mClip.List(out, eCSSProperty_clip);
  buffer.SetLength(0);
  mOverflow.AppendToString(buffer, eCSSProperty_overflow);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

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
    mOutlineRadius(aCopy.mOutlineRadius),
    mFloatEdge(aCopy.mFloatEdge)
{
  MOZ_COUNT_CTOR(nsCSSMargin);
}

nsCSSMargin::~nsCSSMargin(void)
{
  MOZ_COUNT_DTOR(nsCSSMargin);
}

#ifdef DEBUG
void nsCSSMargin::List(FILE* out, PRInt32 aIndent) const
{
  {
    static const nsCSSProperty trbl[] = {
      eCSSProperty_margin_top,
      eCSSProperty_margin_right,
      eCSSProperty_margin_bottom,
      eCSSProperty_margin_left
    };
    mMargin.List(out, aIndent, trbl);
  }
  {
    static const nsCSSProperty trbl[] = {
      eCSSProperty_padding_top,
      eCSSProperty_padding_right,
      eCSSProperty_padding_bottom,
      eCSSProperty_padding_left
    };
    mPadding.List(out, aIndent, trbl);
  }
  {
    static const nsCSSProperty trbl[] = {
      eCSSProperty_border_top_width,
      eCSSProperty_border_right_width,
      eCSSProperty_border_bottom_width,
      eCSSProperty_border_left_width
    };
    mBorderWidth.List(out, aIndent, trbl);
  }
  mBorderColor.List(out, eCSSProperty_border_color, aIndent);
  mBorderStyle.List(out, eCSSProperty_border_style, aIndent);
  {
    static const nsCSSProperty trbl[] = {
      eCSSProperty__moz_border_radius_topLeft,
      eCSSProperty__moz_border_radius_topRight,
      eCSSProperty__moz_border_radius_bottomRight,
      eCSSProperty__moz_border_radius_bottomLeft
    };
    mBorderRadius.List(out, aIndent, trbl);
  }

  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);
 
  nsAutoString  buffer;
  mOutlineWidth.AppendToString(buffer, eCSSProperty__moz_outline_width);
  mOutlineColor.AppendToString(buffer, eCSSProperty__moz_outline_color);
  mOutlineStyle.AppendToString(buffer, eCSSProperty__moz_outline_style);
  {
    static const nsCSSProperty trbl[] = {
      eCSSProperty__moz_outline_radius_topLeft,
      eCSSProperty__moz_outline_radius_topRight,
      eCSSProperty__moz_outline_radius_bottomRight,
      eCSSProperty__moz_outline_radius_bottomLeft
    };
    mOutlineRadius.List(out, aIndent, trbl);
  }
  mFloatEdge.AppendToString(buffer, eCSSProperty_float_edge);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

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

#ifdef DEBUG
void nsCSSPosition::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mWidth.AppendToString(buffer, eCSSProperty_width);
  mMinWidth.AppendToString(buffer, eCSSProperty_min_width);
  mMaxWidth.AppendToString(buffer, eCSSProperty_max_width);
  mHeight.AppendToString(buffer, eCSSProperty_height);
  mMinHeight.AppendToString(buffer, eCSSProperty_min_height);
  mMaxHeight.AppendToString(buffer, eCSSProperty_max_height);
  mBoxSizing.AppendToString(buffer, eCSSProperty_box_sizing);
  mZIndex.AppendToString(buffer, eCSSProperty_z_index);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);

  static const nsCSSProperty trbl[] = {
    eCSSProperty_top,
    eCSSProperty_right,
    eCSSProperty_bottom,
    eCSSProperty_left
  };
  mOffset.List(out, aIndent, trbl);
}
#endif

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

#ifdef DEBUG
void nsCSSList::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mType.AppendToString(buffer, eCSSProperty_list_style_type);
  mImage.AppendToString(buffer, eCSSProperty_list_style_image);
  mPosition.AppendToString(buffer, eCSSProperty_list_style_position);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);

  static const nsCSSProperty trbl[] = {
    eCSSProperty_top,
    eCSSProperty_right,
    eCSSProperty_bottom,
    eCSSProperty_left
  };
  mImageRegion.List(out, aIndent, trbl);
}
#endif

// --- nsCSSTable -----------------

nsCSSTable::nsCSSTable(void)
{
  MOZ_COUNT_CTOR(nsCSSTable);
}

nsCSSTable::nsCSSTable(const nsCSSTable& aCopy)
  : mBorderCollapse(aCopy.mBorderCollapse),
    mBorderSpacingX(aCopy.mBorderSpacingX),
    mBorderSpacingY(aCopy.mBorderSpacingY),
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

#ifdef DEBUG
void nsCSSTable::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mBorderCollapse.AppendToString(buffer, eCSSProperty_border_collapse);
  mBorderSpacingX.AppendToString(buffer, eCSSProperty_border_x_spacing);
  mBorderSpacingY.AppendToString(buffer, eCSSProperty_border_y_spacing);
  mCaptionSide.AppendToString(buffer, eCSSProperty_caption_side);
  mEmptyCells.AppendToString(buffer, eCSSProperty_empty_cells);
  mLayout.AppendToString(buffer, eCSSProperty_table_layout);

  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

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

#ifdef DEBUG
void nsCSSBreaks::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mOrphans.AppendToString(buffer, eCSSProperty_orphans);
  mWidows.AppendToString(buffer, eCSSProperty_widows);
  mPage.AppendToString(buffer, eCSSProperty_page);
  // temp fix for bug 24000
  //mPageBreakAfter.AppendToString(buffer, eCSSProperty_page_break_after);
  //mPageBreakBefore.AppendToString(buffer, eCSSProperty_page_break_before);
  mPageBreakInside.AppendToString(buffer, eCSSProperty_page_break_inside);

  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

// --- nsCSSPage -----------------

nsCSSPage::nsCSSPage(void)
{
  MOZ_COUNT_CTOR(nsCSSPage);
}

nsCSSPage::nsCSSPage(const nsCSSPage& aCopy)
  : mMarks(aCopy.mMarks),
    mSizeWidth(aCopy.mSizeWidth),
    mSizeHeight(aCopy.mSizeHeight)
{
  MOZ_COUNT_CTOR(nsCSSPage);
}

nsCSSPage::~nsCSSPage(void)
{
  MOZ_COUNT_DTOR(nsCSSPage);
}

#ifdef DEBUG
void nsCSSPage::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mMarks.AppendToString(buffer, eCSSProperty_marks);
  mSizeWidth.AppendToString(buffer, eCSSProperty_size_width);
  mSizeHeight.AppendToString(buffer, eCSSProperty_size_height);

  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

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

#ifdef DEBUG
void nsCSSContent::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  nsCSSValueList*  content = mContent;
  while (nsnull != content) {
    content->mValue.AppendToString(buffer, eCSSProperty_content);
    content = content->mNext;
  }
  nsCSSCounterData* counter = mCounterIncrement;
  while (nsnull != counter) {
    counter->mCounter.AppendToString(buffer, eCSSProperty__moz_counter_increment);
    counter->mValue.AppendToString(buffer, eCSSProperty_UNKNOWN);
    counter = counter->mNext;
  }
  counter = mCounterReset;
  while (nsnull != counter) {
    counter->mCounter.AppendToString(buffer, eCSSProperty__moz_counter_reset);
    counter->mValue.AppendToString(buffer, eCSSProperty_UNKNOWN);
    counter = counter->mNext;
  }
  mMarkerOffset.AppendToString(buffer, eCSSProperty_marker_offset);
  // XXX This prints the property name many times, but nobody cares.
  nsCSSQuotes*  quotes = mQuotes;
  while (nsnull != quotes) {
    quotes->mOpen.AppendToString(buffer, eCSSProperty_quotes);
    quotes->mClose.AppendToString(buffer, eCSSProperty_quotes);
    quotes = quotes->mNext;
  }

  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

// --- nsCSSUserInterface -----------------

nsCSSUserInterface::nsCSSUserInterface(void)
  : mKeyEquivalent(nsnull), mCursor(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSUserInterface);
}

nsCSSUserInterface::nsCSSUserInterface(const nsCSSUserInterface& aCopy)
  : mUserInput(aCopy.mUserInput),
    mUserModify(aCopy.mUserModify),
    mUserSelect(aCopy.mUserSelect),
    mKeyEquivalent(nsnull),
    mUserFocus(aCopy.mUserFocus),
    mCursor(nsnull),
    mForceBrokenImageIcon(aCopy.mForceBrokenImageIcon)
{
  MOZ_COUNT_CTOR(nsCSSUserInterface);
  CSS_IF_COPY(mCursor, nsCSSValueList);
  CSS_IF_COPY(mKeyEquivalent, nsCSSValueList);
}

nsCSSUserInterface::~nsCSSUserInterface(void)
{
  MOZ_COUNT_DTOR(nsCSSUserInterface);
  CSS_IF_DELETE(mKeyEquivalent);
  CSS_IF_DELETE(mCursor);
}

#ifdef DEBUG
void nsCSSUserInterface::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mUserInput.AppendToString(buffer, eCSSProperty_user_input);
  mUserModify.AppendToString(buffer, eCSSProperty_user_modify);
  mUserSelect.AppendToString(buffer, eCSSProperty_user_select);
  nsCSSValueList*  keyEquiv = mKeyEquivalent;
  while (nsnull != keyEquiv) {
    keyEquiv->mValue.AppendToString(buffer, eCSSProperty_key_equivalent);
    keyEquiv= keyEquiv->mNext;
  }
  mUserFocus.AppendToString(buffer, eCSSProperty_user_focus);
  
  nsCSSValueList*  cursor = mCursor;
  while (nsnull != cursor) {
    cursor->mValue.AppendToString(buffer, eCSSProperty_cursor);
    cursor = cursor->mNext;
  }

  mForceBrokenImageIcon.AppendToString(buffer,eCSSProperty_force_broken_image_icon);

  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

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
    mPlayDuring(aCopy.mPlayDuring),
    mPlayDuringFlags(aCopy.mPlayDuringFlags),
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

#ifdef DEBUG
void nsCSSAural::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mAzimuth.AppendToString(buffer, eCSSProperty_azimuth);
  mElevation.AppendToString(buffer, eCSSProperty_elevation);
  mCueAfter.AppendToString(buffer, eCSSProperty_cue_after);
  mCueBefore.AppendToString(buffer, eCSSProperty_cue_before);
  mPauseAfter.AppendToString(buffer, eCSSProperty_pause_after);
  mPauseBefore.AppendToString(buffer, eCSSProperty_pause_before);
  mPitch.AppendToString(buffer, eCSSProperty_pitch);
  mPitchRange.AppendToString(buffer, eCSSProperty_pitch_range);
  mPlayDuring.AppendToString(buffer, eCSSProperty_play_during_uri);
  mPlayDuringFlags.AppendToString(buffer, eCSSProperty_play_during_flags);
  mRichness.AppendToString(buffer, eCSSProperty_richness);
  mSpeak.AppendToString(buffer, eCSSProperty_speak);
  mSpeakHeader.AppendToString(buffer, eCSSProperty_speak_header);
  mSpeakNumeral.AppendToString(buffer, eCSSProperty_speak_numeral);
  mSpeakPunctuation.AppendToString(buffer, eCSSProperty_speak_punctuation);
  mSpeechRate.AppendToString(buffer, eCSSProperty_speech_rate);
  mStress.AppendToString(buffer, eCSSProperty_stress);
  mVoiceFamily.AppendToString(buffer, eCSSProperty_voice_family);
  mVolume.AppendToString(buffer, eCSSProperty_volume);

  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

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

#ifdef DEBUG
void nsCSSXUL::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mBoxAlign.AppendToString(buffer, eCSSProperty_box_align);
  mBoxDirection.AppendToString(buffer, eCSSProperty_box_direction);
  mBoxFlex.AppendToString(buffer, eCSSProperty_box_flex);
  mBoxOrient.AppendToString(buffer, eCSSProperty_box_orient);
  mBoxPack.AppendToString(buffer, eCSSProperty_box_pack);
  mBoxOrdinal.AppendToString(buffer, eCSSProperty_box_ordinal_group);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

#ifdef MOZ_SVG
// --- nsCSSSVG -----------------

nsCSSSVG::nsCSSSVG(void)
{
  MOZ_COUNT_CTOR(nsCSSSVG);
}

nsCSSSVG::nsCSSSVG(const nsCSSSVG& aCopy)
    : mDominantBaseline(aCopy.mDominantBaseline),
      mFill(aCopy.mFill),
      mFillOpacity(aCopy.mFillOpacity),
      mFillRule(aCopy.mFillRule),
      mPointerEvents(aCopy.mPointerEvents),
      mShapeRendering(aCopy.mShapeRendering),
      mStroke(aCopy.mStroke),
      mStrokeDasharray(aCopy.mStrokeDasharray),
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
}

nsCSSSVG::~nsCSSSVG(void)
{
  MOZ_COUNT_DTOR(nsCSSSVG);
}

#ifdef DEBUG
void nsCSSSVG::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mDominantBaseline.AppendToString(buffer, eCSSProperty_dominant_baseline);
  mFill.AppendToString(buffer, eCSSProperty_fill);
  mFillOpacity.AppendToString(buffer, eCSSProperty_fill_opacity);
  mFillRule.AppendToString(buffer, eCSSProperty_fill_rule);
  mPointerEvents.AppendToString(buffer, eCSSProperty_pointer_events);
  mShapeRendering.AppendToString(buffer, eCSSProperty_shape_rendering);
  mStroke.AppendToString(buffer, eCSSProperty_stroke);
  mStrokeDasharray.AppendToString(buffer, eCSSProperty_stroke_dasharray);
  mStrokeDashoffset.AppendToString(buffer, eCSSProperty_stroke_dashoffset);
  mStrokeLinecap.AppendToString(buffer, eCSSProperty_stroke_linecap);
  mStrokeLinejoin.AppendToString(buffer, eCSSProperty_stroke_linejoin);
  mStrokeMiterlimit.AppendToString(buffer, eCSSProperty_stroke_miterlimit);
  mStrokeOpacity.AppendToString(buffer, eCSSProperty_stroke_opacity);
  mStrokeWidth.AppendToString(buffer, eCSSProperty_stroke_width);
  mTextAnchor.AppendToString(buffer, eCSSProperty_text_anchor);
  mTextRendering.AppendToString(buffer, eCSSProperty_text_rendering);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

#endif // MOZ_SVG
