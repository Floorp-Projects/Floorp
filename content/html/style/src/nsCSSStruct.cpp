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
#include "nscore.h"
#include "nsICSSDeclaration.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsCSSProps.h"
#include "nsUnitConversion.h"
#include "nsVoidArray.h"

#include "nsStyleConsts.h"

#include "nsCOMPtr.h"
#include "nsIStyleSet.h"
#include "nsISizeOfHandler.h"

// #define DEBUG_REFS


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
static NS_DEFINE_IID(kCSSUserInterfaceSID, NS_CSS_USER_INTERFACE_SID);
static NS_DEFINE_IID(kCSSAuralSID, NS_CSS_AURAL_SID);
#ifdef INCLUDE_XUL
static NS_DEFINE_IID(kCSSXULSID, NS_CSS_XUL_SID);
#endif

#define CSS_IF_DELETE(ptr)  if (nsnull != ptr)  { delete ptr; ptr = nsnull; }

nsCSSStruct::~nsCSSStruct()
{
}

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

const nsID& nsCSSFont::GetID(void)
{
  return kCSSFontSID;
}

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
    mBackPositionY(aCopy.mBackPositionY)
{
  MOZ_COUNT_CTOR(nsCSSColor);
}

nsCSSColor::~nsCSSColor(void)
{
  MOZ_COUNT_DTOR(nsCSSColor);
}

const nsID& nsCSSColor::GetID(void)
{
  return kCSSColorSID;
}

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
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}

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

// --- nsCSSText -----------------

nsCSSText::nsCSSText(void)
  : mTextShadow(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSText);
}

nsCSSText::nsCSSText(const nsCSSText& aCopy)
  : mWordSpacing(aCopy.mWordSpacing),
    mLetterSpacing(aCopy.mLetterSpacing),
    mDecoration(aCopy.mDecoration),
    mVerticalAlign(aCopy.mVerticalAlign),
    mTextTransform(aCopy.mTextTransform),
    mTextAlign(aCopy.mTextAlign),
    mTextIndent(aCopy.mTextIndent),
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

const nsID& nsCSSText::GetID(void)
{
  return kCSSTextSID;
}

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
      while (nsnull != shadow) {
        shadow->mColor.AppendToString(buffer, eCSSProperty_text_shadow_color);
        shadow->mXOffset.AppendToString(buffer, eCSSProperty_text_shadow_x);
        shadow->mYOffset.AppendToString(buffer, eCSSProperty_text_shadow_y);
        shadow->mRadius.AppendToString(buffer, eCSSProperty_text_shadow_radius);
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


void nsCSSRect::List(FILE* out, nsCSSProperty aPropID, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  if (eCSSProperty_UNKNOWN < aPropID) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aPropID));
    buffer.AppendWithConversion(": ");
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
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aTRBL[0]));
    buffer.AppendWithConversion(": ");
    mTop.AppendToString(buffer);
  }
  if (eCSSUnit_Null != mRight.GetUnit()) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aTRBL[1]));
    buffer.AppendWithConversion(": ");
    mRight.AppendToString(buffer);
  }
  if (eCSSUnit_Null != mBottom.GetUnit()) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aTRBL[2]));
    buffer.AppendWithConversion(": ");
    mBottom.AppendToString(buffer); 
  }
  if (eCSSUnit_Null != mLeft.GetUnit()) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aTRBL[3]));
    buffer.AppendWithConversion(": ");
    mLeft.AppendToString(buffer);
  }

  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}

// --- nsCSSDisplay -----------------

nsCSSDisplay::nsCSSDisplay(void)
  : mClip(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSDisplay);
}

nsCSSDisplay::nsCSSDisplay(const nsCSSDisplay& aCopy)
  : mDirection(aCopy.mDirection),
    mDisplay(aCopy.mDisplay),
    mBinding(aCopy.mBinding),
    mPosition(aCopy.mPosition),
    mFloat(aCopy.mFloat),
    mClear(aCopy.mClear),
    mClip(nsnull),
    mOverflow(aCopy.mOverflow),
    mVisibility(aCopy.mVisibility),
    mOpacity(aCopy.mOpacity)
{
  MOZ_COUNT_CTOR(nsCSSDisplay);
  CSS_IF_COPY(mClip, nsCSSRect);
}

nsCSSDisplay::~nsCSSDisplay(void)
{
  MOZ_COUNT_DTOR(nsCSSDisplay);
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

  mDirection.AppendToString(buffer, eCSSProperty_direction);
  mDisplay.AppendToString(buffer, eCSSProperty_display);
  mBinding.AppendToString(buffer, eCSSProperty_binding);
  mPosition.AppendToString(buffer, eCSSProperty_position);
  mFloat.AppendToString(buffer, eCSSProperty_float);
  mClear.AppendToString(buffer, eCSSProperty_clear);
  mVisibility.AppendToString(buffer, eCSSProperty_visibility);
  mOpacity.AppendToString(buffer, eCSSProperty_opacity);

  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
  if (nsnull != mClip) {
    mClip->List(out, eCSSProperty_clip);
  }
  buffer.SetLength(0);
  mOverflow.AppendToString(buffer, eCSSProperty_overflow);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}

// --- nsCSSMargin -----------------

nsCSSMargin::nsCSSMargin(void)
  : mMargin(nsnull), mPadding(nsnull), 
    mBorderWidth(nsnull), mBorderColor(nsnull), mBorderStyle(nsnull), mBorderRadius(nsnull), mOutlineRadius(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSMargin);
}

nsCSSMargin::nsCSSMargin(const nsCSSMargin& aCopy)
  : mMargin(nsnull), mPadding(nsnull), 
    mBorderWidth(nsnull), mBorderColor(nsnull), mBorderStyle(nsnull), mBorderRadius(nsnull),
    mOutlineWidth(aCopy.mOutlineWidth),
    mOutlineColor(aCopy.mOutlineColor),
    mOutlineStyle(aCopy.mOutlineStyle),
    mOutlineRadius(nsnull),
    mFloatEdge(aCopy.mFloatEdge)
{
  MOZ_COUNT_CTOR(nsCSSMargin);
  CSS_IF_COPY(mMargin, nsCSSRect);
  CSS_IF_COPY(mPadding, nsCSSRect);
  CSS_IF_COPY(mBorderWidth, nsCSSRect);
  CSS_IF_COPY(mBorderColor, nsCSSRect);
  CSS_IF_COPY(mBorderStyle, nsCSSRect);
  CSS_IF_COPY(mBorderRadius, nsCSSRect);
  CSS_IF_COPY(mOutlineRadius, nsCSSRect);
}

nsCSSMargin::~nsCSSMargin(void)
{
  MOZ_COUNT_DTOR(nsCSSMargin);
  CSS_IF_DELETE(mMargin);
  CSS_IF_DELETE(mPadding);
  CSS_IF_DELETE(mBorderWidth);
  CSS_IF_DELETE(mBorderColor);
  CSS_IF_DELETE(mBorderStyle);
  CSS_IF_DELETE(mBorderRadius);
  CSS_IF_DELETE(mOutlineRadius);
}

const nsID& nsCSSMargin::GetID(void)
{
  return kCSSMarginSID;
}

void nsCSSMargin::List(FILE* out, PRInt32 aIndent) const
{
  if (nsnull != mMargin) {
    static const nsCSSProperty trbl[] = {
      eCSSProperty_margin_top,
      eCSSProperty_margin_right,
      eCSSProperty_margin_bottom,
      eCSSProperty_margin_left
    };
    mMargin->List(out, aIndent, trbl);
  }
  if (nsnull != mPadding) {
    static const nsCSSProperty trbl[] = {
      eCSSProperty_padding_top,
      eCSSProperty_padding_right,
      eCSSProperty_padding_bottom,
      eCSSProperty_padding_left
    };
    mPadding->List(out, aIndent, trbl);
  }
  if (nsnull != mBorderWidth) {
    static const nsCSSProperty trbl[] = {
      eCSSProperty_border_top_width,
      eCSSProperty_border_right_width,
      eCSSProperty_border_bottom_width,
      eCSSProperty_border_left_width
    };
    mBorderWidth->List(out, aIndent, trbl);
  }
  if (nsnull != mBorderColor) {
    mBorderColor->List(out, eCSSProperty_border_color, aIndent);
  }
  if (nsnull != mBorderStyle) {
    mBorderStyle->List(out, eCSSProperty_border_style, aIndent);
  }
  if (nsnull != mBorderRadius) {
    static const nsCSSProperty trbl[] = {
      eCSSProperty__moz_border_radius_topLeft,
      eCSSProperty__moz_border_radius_topRight,
      eCSSProperty__moz_border_radius_bottomRight,
      eCSSProperty__moz_border_radius_bottomLeft
    };
    mBorderRadius->List(out, aIndent, trbl);
  }

  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);
 
  nsAutoString  buffer;
  mOutlineWidth.AppendToString(buffer, eCSSProperty_outline_width);
  mOutlineColor.AppendToString(buffer, eCSSProperty_outline_color);
  mOutlineStyle.AppendToString(buffer, eCSSProperty_outline_style);
  if (nsnull != mOutlineRadius) {
    static const nsCSSProperty trbl[] = {
      eCSSProperty__moz_outline_radius_topLeft,
      eCSSProperty__moz_outline_radius_topRight,
      eCSSProperty__moz_outline_radius_bottomRight,
      eCSSProperty__moz_outline_radius_bottomLeft
    };
    mOutlineRadius->List(out, aIndent, trbl);
  }
  mFloatEdge.AppendToString(buffer, eCSSProperty_float_edge);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}

// --- nsCSSPosition -----------------

nsCSSPosition::nsCSSPosition(void)
  : mOffset(nsnull)
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
    mOffset(nsnull),
    mZIndex(aCopy.mZIndex)
{
  MOZ_COUNT_CTOR(nsCSSPosition);
  CSS_IF_COPY(mOffset, nsCSSRect);
}

nsCSSPosition::~nsCSSPosition(void)
{
  MOZ_COUNT_DTOR(nsCSSPosition);
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

  mWidth.AppendToString(buffer, eCSSProperty_width);
  mMinWidth.AppendToString(buffer, eCSSProperty_min_width);
  mMaxWidth.AppendToString(buffer, eCSSProperty_max_width);
  mHeight.AppendToString(buffer, eCSSProperty_height);
  mMinHeight.AppendToString(buffer, eCSSProperty_min_height);
  mMaxHeight.AppendToString(buffer, eCSSProperty_max_height);
  mBoxSizing.AppendToString(buffer, eCSSProperty_box_sizing);
  mZIndex.AppendToString(buffer, eCSSProperty_z_index);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);

  if (nsnull != mOffset) {
    static const nsCSSProperty trbl[] = {
      eCSSProperty_top,
      eCSSProperty_right,
      eCSSProperty_bottom,
      eCSSProperty_left
    };
    mOffset->List(out, aIndent, trbl);
  }
}

// --- nsCSSList -----------------

nsCSSList::nsCSSList(void)
{
  MOZ_COUNT_CTOR(nsCSSList);
}

nsCSSList::nsCSSList(const nsCSSList& aCopy)
  : mType(aCopy.mType),
    mImage(aCopy.mImage),
    mPosition(aCopy.mPosition)
{
  MOZ_COUNT_CTOR(nsCSSList);
}

nsCSSList::~nsCSSList(void)
{
  MOZ_COUNT_DTOR(nsCSSList);
}

const nsID& nsCSSList::GetID(void)
{
  return kCSSListSID;
}

void nsCSSList::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mType.AppendToString(buffer, eCSSProperty_list_style_type);
  mImage.AppendToString(buffer, eCSSProperty_list_style_image);
  mPosition.AppendToString(buffer, eCSSProperty_list_style_position);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}

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

const nsID& nsCSSTable::GetID(void)
{
  return kCSSTableSID;
}

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

// --- nsCSSBreaks -----------------

nsCSSBreaks::nsCSSBreaks(void)
{
  MOZ_COUNT_CTOR(nsCSSBreaks);
}

nsCSSBreaks::nsCSSBreaks(const nsCSSBreaks& aCopy)
  : mOrphans(aCopy.mOrphans),
    mWidows(aCopy.mWidows),
    mPage(aCopy.mPage),
    mPageBreakAfter(aCopy.mPageBreakAfter),
    mPageBreakBefore(aCopy.mPageBreakBefore),
    mPageBreakInside(aCopy.mPageBreakInside)
{
  MOZ_COUNT_CTOR(nsCSSBreaks);
}

nsCSSBreaks::~nsCSSBreaks(void)
{
  MOZ_COUNT_DTOR(nsCSSBreaks);
}

const nsID& nsCSSBreaks::GetID(void)
{
  return kCSSBreaksSID;
}

void nsCSSBreaks::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mOrphans.AppendToString(buffer, eCSSProperty_orphans);
  mWidows.AppendToString(buffer, eCSSProperty_widows);
  mPage.AppendToString(buffer, eCSSProperty_page);
  mPageBreakAfter.AppendToString(buffer, eCSSProperty_page_break_after);
  mPageBreakBefore.AppendToString(buffer, eCSSProperty_page_break_before);
  mPageBreakInside.AppendToString(buffer, eCSSProperty_page_break_inside);

  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}

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

const nsID& nsCSSPage::GetID(void)
{
  return kCSSPageSID;
}

void nsCSSPage::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mMarks.AppendToString(buffer, eCSSProperty_marks);
  mSizeWidth.AppendToString(buffer, eCSSProperty_size_width);
  mSizeHeight.AppendToString(buffer, eCSSProperty_size_height);

  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
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
    content->mValue.AppendToString(buffer, eCSSProperty_content);
    content = content->mNext;
  }
  nsCSSCounterData* counter = mCounterIncrement;
  while (nsnull != counter) {
    counter->mCounter.AppendToString(buffer, eCSSProperty_counter_increment);
    counter->mValue.AppendToString(buffer, eCSSProperty_UNKNOWN);
    counter = counter->mNext;
  }
  counter = mCounterReset;
  while (nsnull != counter) {
    counter->mCounter.AppendToString(buffer, eCSSProperty_counter_reset);
    counter->mValue.AppendToString(buffer, eCSSProperty_UNKNOWN);
    counter = counter->mNext;
  }
  mMarkerOffset.AppendToString(buffer, eCSSProperty_marker_offset);
  nsCSSQuotes*  quotes = mQuotes;
  while (nsnull != quotes) {
    quotes->mOpen.AppendToString(buffer, eCSSProperty_quotes_open);
    quotes->mClose.AppendToString(buffer, eCSSProperty_quotes_close);
    quotes = quotes->mNext;
  }

  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}

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
    mResizer(aCopy.mResizer),
    mCursor(nsnull)
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

const nsID& nsCSSUserInterface::GetID(void)
{
  return kCSSUserInterfaceSID;
}

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
  mResizer.AppendToString(buffer, eCSSProperty_resizer);
  
  nsCSSValueList*  cursor = mCursor;
  while (nsnull != cursor) {
    cursor->mValue.AppendToString(buffer, eCSSProperty_cursor);
    cursor = cursor->mNext;
  }
  
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
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

const nsID& nsCSSAural::GetID(void)
{
  return kCSSAuralSID;
}

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
  mPlayDuring.AppendToString(buffer, eCSSProperty_play_during);
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

#ifdef INCLUDE_XUL
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

const nsID& nsCSSXUL::GetID(void)
{
  return kCSSXULSID;
}

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

#endif // INCLUDE_XUL


// --- nsCSSDeclaration -----------------


class CSSDeclarationImpl : public nsICSSDeclaration {
public:
  CSSDeclarationImpl(void);
  CSSDeclarationImpl(const CSSDeclarationImpl& aCopy);
  virtual ~CSSDeclarationImpl(void);

  NS_DECL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetData(const nsID& aSID, nsCSSStruct** aData);
  NS_IMETHOD EnsureData(const nsID& aSID, nsCSSStruct** aData);

  NS_IMETHOD AppendValue(nsCSSProperty aProperty, const nsCSSValue& aValue);
  NS_IMETHOD AppendStructValue(nsCSSProperty aProperty, void* aStruct);
  NS_IMETHOD SetValueImportant(nsCSSProperty aProperty);
  NS_IMETHOD AppendComment(const nsAReadableString& aComment);
  NS_IMETHOD RemoveProperty(nsCSSProperty aProperty, nsCSSValue& aValue);

  NS_IMETHOD GetValue(nsCSSProperty aProperty, nsCSSValue& aValue);
  NS_IMETHOD GetValue(nsCSSProperty aProperty, nsAWritableString& aValue);
  NS_IMETHOD GetValue(const nsAReadableString& aProperty, nsAWritableString& aValue);

  NS_IMETHOD GetImportantValues(nsICSSDeclaration*& aResult);
  NS_IMETHOD GetValueIsImportant(nsCSSProperty aProperty, PRBool& aIsImportant);
  NS_IMETHOD GetValueIsImportant(const nsAReadableString& aProperty, PRBool& aIsImportant);

  PRBool   AppendValueToString(nsCSSProperty aProperty, nsAWritableString& aResult);
  PRBool   AppendValueToString(nsCSSProperty aProperty, const nsCSSValue& aValue, nsAWritableString& aResult);

  NS_IMETHOD ToString(nsAWritableString& aString);

  NS_IMETHOD Clone(nsICSSDeclaration*& aClone) const;

#ifdef DEBUG
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);
#endif
  
  NS_IMETHOD Count(PRUint32* aCount);
  NS_IMETHOD GetNthProperty(PRUint32 aIndex, nsAWritableString& aReturn);

  NS_IMETHOD GetStyleImpact(PRInt32* aHint) const;

protected:
  nsresult RemoveProperty(nsCSSProperty aProperty);

private:
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
  nsCSSUserInterface* mUserInterface;
  nsCSSAural*     mAural;
#ifdef INCLUDE_XUL
  nsCSSXUL*       mXUL;
#endif

  CSSDeclarationImpl* mImportant;

  nsVoidArray*    mOrder;
  nsStringArray*  mComments;
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
  fprintf(stdout, "CSSDeclaration Instances (ctor): %ld\n", (long)gInstanceCount);
#endif
}

#define DECL_IF_COPY(type) \
  if (aCopy.m##type)  m##type = new nsCSS##type(*(aCopy.m##type))

CSSDeclarationImpl::CSSDeclarationImpl(const CSSDeclarationImpl& aCopy)
{
  NS_INIT_REFCNT();
  DECL_IF_COPY(Font);
  DECL_IF_COPY(Color);
  DECL_IF_COPY(Text);
  DECL_IF_COPY(Margin);
  DECL_IF_COPY(Position);
  DECL_IF_COPY(List);
  DECL_IF_COPY(Display);
  DECL_IF_COPY(Table);
  DECL_IF_COPY(Breaks);
  DECL_IF_COPY(Page);
  DECL_IF_COPY(Content);
  DECL_IF_COPY(UserInterface);
  DECL_IF_COPY(Aural);
#ifdef INCLUDE_XUL
  DECL_IF_COPY(XUL);
#endif

#ifdef DEBUG_REFS
  ++gInstanceCount;
  fprintf(stdout, "CSSDeclaration Instances (cp-ctor): %ld\n", (long)gInstanceCount);
#endif

  if (aCopy.mImportant) {
    mImportant = new CSSDeclarationImpl(*(aCopy.mImportant));
    NS_IF_ADDREF(mImportant);
  }

  if (aCopy.mOrder) {
    mOrder = new nsAutoVoidArray();
    if (mOrder) {
      (*mOrder) = *(aCopy.mOrder);
    }
  }

  if (aCopy.mComments) {
    mComments = new nsStringArray();
    if (mComments) {
      (*mComments) = *(aCopy.mComments);
    }
  }
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
  CSS_IF_DELETE(mUserInterface);
  CSS_IF_DELETE(mAural);
#ifdef INCLUDE_XUL
  CSS_IF_DELETE(mXUL);
#endif

  NS_IF_RELEASE(mImportant);
  CSS_IF_DELETE(mOrder);
  CSS_IF_DELETE(mComments);

#ifdef DEBUG_REFS
  --gInstanceCount;
  fprintf(stdout, "CSSDeclaration Instances (dtor): %ld\n", (long)gInstanceCount);
#endif
}

NS_IMPL_ISUPPORTS1(CSSDeclarationImpl, nsICSSDeclaration)

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
  CSS_IF_GET_ELSE(aSID, UserInterface, aDataPtr)
  CSS_IF_GET_ELSE(aSID, Aural, aDataPtr)
#ifdef INCLUDE_XUL
  CSS_IF_GET_ELSE(aSID, XUL, aDataPtr)
#endif
  {
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
  CSS_IF_ENSURE_ELSE(aSID, UserInterface, aDataPtr)
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

#define CSS_BOGUS_DEFAULT   default: NS_ERROR("should never happen"); break;

NS_IMETHODIMP
CSSDeclarationImpl::AppendValue(nsCSSProperty aProperty, const nsCSSValue& aValue)
{
  nsresult result = NS_OK;

  switch (aProperty) {
    // nsCSSFont
    case eCSSProperty_font_family:
    case eCSSProperty_font_style:
    case eCSSProperty_font_variant:
    case eCSSProperty_font_weight:
    case eCSSProperty_font_size:
    case eCSSProperty_font_size_adjust:
    case eCSSProperty_font_stretch:
      CSS_ENSURE(Font) {
        switch (aProperty) {
          case eCSSProperty_font_family:      mFont->mFamily = aValue;      break;
          case eCSSProperty_font_style:       mFont->mStyle = aValue;       break;
          case eCSSProperty_font_variant:     mFont->mVariant = aValue;     break;
          case eCSSProperty_font_weight:      mFont->mWeight = aValue;      break;
          case eCSSProperty_font_size:        mFont->mSize = aValue;        break;
          case eCSSProperty_font_size_adjust: mFont->mSizeAdjust = aValue;  break;
          case eCSSProperty_font_stretch:     mFont->mStretch = aValue;     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

    // nsCSSColor
    case eCSSProperty_color:
    case eCSSProperty_background_color:
    case eCSSProperty_background_image:
    case eCSSProperty_background_repeat:
    case eCSSProperty_background_attachment:
    case eCSSProperty_background_x_position:
    case eCSSProperty_background_y_position:
      CSS_ENSURE(Color) {
        switch (aProperty) {
          case eCSSProperty_color:                  mColor->mColor = aValue;           break;
          case eCSSProperty_background_color:       mColor->mBackColor = aValue;       break;
          case eCSSProperty_background_image:       mColor->mBackImage = aValue;       break;
          case eCSSProperty_background_repeat:      mColor->mBackRepeat = aValue;      break;
          case eCSSProperty_background_attachment:  mColor->mBackAttachment = aValue;  break;
          case eCSSProperty_background_x_position:  mColor->mBackPositionX = aValue;   break;
          case eCSSProperty_background_y_position:  mColor->mBackPositionY = aValue;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

    // nsCSSText
    case eCSSProperty_word_spacing:
    case eCSSProperty_letter_spacing:
    case eCSSProperty_text_decoration:
    case eCSSProperty_vertical_align:
    case eCSSProperty_text_transform:
    case eCSSProperty_text_align:
    case eCSSProperty_text_indent:
    case eCSSProperty_unicode_bidi:
    case eCSSProperty_line_height:
    case eCSSProperty_white_space:
      CSS_ENSURE(Text) {
        switch (aProperty) {
          case eCSSProperty_word_spacing:     mText->mWordSpacing = aValue;    break;
          case eCSSProperty_letter_spacing:   mText->mLetterSpacing = aValue;  break;
          case eCSSProperty_text_decoration:  mText->mDecoration = aValue;     break;
          case eCSSProperty_vertical_align:   mText->mVerticalAlign = aValue;  break;
          case eCSSProperty_text_transform:   mText->mTextTransform = aValue;  break;
          case eCSSProperty_text_align:       mText->mTextAlign = aValue;      break;
          case eCSSProperty_text_indent:      mText->mTextIndent = aValue;     break;
          case eCSSProperty_unicode_bidi:     mText->mUnicodeBidi = aValue;    break;
          case eCSSProperty_line_height:      mText->mLineHeight = aValue;     break;
          case eCSSProperty_white_space:      mText->mWhiteSpace = aValue;     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

    case eCSSProperty_text_shadow_color:
    case eCSSProperty_text_shadow_radius:
    case eCSSProperty_text_shadow_x:
    case eCSSProperty_text_shadow_y:
      CSS_ENSURE(Text) {
        CSS_ENSURE_DATA(mText->mTextShadow, nsCSSShadow) {
          switch (aProperty) {
            case eCSSProperty_text_shadow_color:  mText->mTextShadow->mColor = aValue;    break;
            case eCSSProperty_text_shadow_radius: mText->mTextShadow->mRadius = aValue;   break;
            case eCSSProperty_text_shadow_x:      mText->mTextShadow->mXOffset = aValue;  break;
            case eCSSProperty_text_shadow_y:      mText->mTextShadow->mYOffset = aValue;  break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
          CSS_IF_DELETE(mText->mTextShadow->mNext);
        }
      }
      break;

      // nsCSSDisplay
    case eCSSProperty_float:
    case eCSSProperty_clear:
    case eCSSProperty_display:
    case eCSSProperty_binding:
    case eCSSProperty_position:          
    case eCSSProperty_direction:
    case eCSSProperty_visibility:
    case eCSSProperty_opacity:
    case eCSSProperty_overflow:
      CSS_ENSURE(Display) {
        switch (aProperty) {
          case eCSSProperty_float:      mDisplay->mFloat = aValue;      break;
          case eCSSProperty_clear:      mDisplay->mClear = aValue;      break;
          case eCSSProperty_display:    mDisplay->mDisplay = aValue;    break;
          case eCSSProperty_position:   mDisplay->mPosition = aValue;   break;
          case eCSSProperty_direction:  mDisplay->mDirection = aValue;  break;
          case eCSSProperty_visibility: mDisplay->mVisibility = aValue; break;
          case eCSSProperty_opacity:    mDisplay->mOpacity = aValue;    break;
          case eCSSProperty_overflow:   mDisplay->mOverflow = aValue;   break;
          case eCSSProperty_binding:         
            mDisplay->mBinding = aValue;      
            break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

    case eCSSProperty_clip_top:
    case eCSSProperty_clip_right:
    case eCSSProperty_clip_bottom:
    case eCSSProperty_clip_left:
      CSS_ENSURE(Display) {
        CSS_ENSURE_RECT(mDisplay->mClip) {
          switch(aProperty) {
            case eCSSProperty_clip_top:     mDisplay->mClip->mTop = aValue;     break;
            case eCSSProperty_clip_right:   mDisplay->mClip->mRight = aValue;   break;
            case eCSSProperty_clip_bottom:  mDisplay->mClip->mBottom = aValue;  break;
            case eCSSProperty_clip_left:    mDisplay->mClip->mLeft = aValue;    break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    // nsCSSMargin
    case eCSSProperty_margin_top:
    case eCSSProperty_margin_right:
    case eCSSProperty_margin_bottom:
    case eCSSProperty_margin_left:
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(mMargin->mMargin) {
          switch (aProperty) {
            case eCSSProperty_margin_top:     mMargin->mMargin->mTop = aValue;     break;
            case eCSSProperty_margin_right:   mMargin->mMargin->mRight = aValue;   break;
            case eCSSProperty_margin_bottom:  mMargin->mMargin->mBottom = aValue;  break;
            case eCSSProperty_margin_left:    mMargin->mMargin->mLeft = aValue;    break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    case eCSSProperty_padding_top:
    case eCSSProperty_padding_right:
    case eCSSProperty_padding_bottom:
    case eCSSProperty_padding_left:
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(mMargin->mPadding) {
          switch (aProperty) {
            case eCSSProperty_padding_top:    mMargin->mPadding->mTop = aValue;    break;
            case eCSSProperty_padding_right:  mMargin->mPadding->mRight = aValue;  break;
            case eCSSProperty_padding_bottom: mMargin->mPadding->mBottom = aValue; break;
            case eCSSProperty_padding_left:   mMargin->mPadding->mLeft = aValue;   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    case eCSSProperty_border_top_width:
    case eCSSProperty_border_right_width:
    case eCSSProperty_border_bottom_width:
    case eCSSProperty_border_left_width:
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(mMargin->mBorderWidth) {
          switch (aProperty) {
            case eCSSProperty_border_top_width:     mMargin->mBorderWidth->mTop = aValue;     break;
            case eCSSProperty_border_right_width:   mMargin->mBorderWidth->mRight = aValue;   break;
            case eCSSProperty_border_bottom_width:  mMargin->mBorderWidth->mBottom = aValue;  break;
            case eCSSProperty_border_left_width:    mMargin->mBorderWidth->mLeft = aValue;    break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    case eCSSProperty_border_top_color:
    case eCSSProperty_border_right_color:
    case eCSSProperty_border_bottom_color:
    case eCSSProperty_border_left_color:
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(mMargin->mBorderColor) {
          switch (aProperty) {
            case eCSSProperty_border_top_color:     mMargin->mBorderColor->mTop = aValue;    break;
            case eCSSProperty_border_right_color:   mMargin->mBorderColor->mRight = aValue;  break;
            case eCSSProperty_border_bottom_color:  mMargin->mBorderColor->mBottom = aValue; break;
            case eCSSProperty_border_left_color:    mMargin->mBorderColor->mLeft = aValue;   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    case eCSSProperty_border_top_style:
    case eCSSProperty_border_right_style:
    case eCSSProperty_border_bottom_style:
    case eCSSProperty_border_left_style:
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(mMargin->mBorderStyle) {
          switch (aProperty) {
            case eCSSProperty_border_top_style:     mMargin->mBorderStyle->mTop = aValue;    break;
            case eCSSProperty_border_right_style:   mMargin->mBorderStyle->mRight = aValue;  break;
            case eCSSProperty_border_bottom_style:  mMargin->mBorderStyle->mBottom = aValue; break;
            case eCSSProperty_border_left_style:    mMargin->mBorderStyle->mLeft = aValue;   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    case eCSSProperty__moz_border_radius_topLeft:
    case eCSSProperty__moz_border_radius_topRight:
    case eCSSProperty__moz_border_radius_bottomRight:
    case eCSSProperty__moz_border_radius_bottomLeft:
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(mMargin->mBorderRadius) {
          switch (aProperty) {
            case eCSSProperty__moz_border_radius_topLeft:			mMargin->mBorderRadius->mTop = aValue;    break;
            case eCSSProperty__moz_border_radius_topRight:		mMargin->mBorderRadius->mRight = aValue;  break;
            case eCSSProperty__moz_border_radius_bottomRight:	mMargin->mBorderRadius->mBottom = aValue; break;
            case eCSSProperty__moz_border_radius_bottomLeft:	mMargin->mBorderRadius->mLeft = aValue;   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    case eCSSProperty__moz_outline_radius_topLeft:
    case eCSSProperty__moz_outline_radius_topRight:
    case eCSSProperty__moz_outline_radius_bottomRight:
    case eCSSProperty__moz_outline_radius_bottomLeft:
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(mMargin->mOutlineRadius) {
          switch (aProperty) {
            case eCSSProperty__moz_outline_radius_topLeft:			mMargin->mOutlineRadius->mTop = aValue;    break;
            case eCSSProperty__moz_outline_radius_topRight:			mMargin->mOutlineRadius->mRight = aValue;  break;
            case eCSSProperty__moz_outline_radius_bottomRight:	mMargin->mOutlineRadius->mBottom = aValue; break;
            case eCSSProperty__moz_outline_radius_bottomLeft:		mMargin->mOutlineRadius->mLeft = aValue;   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    case eCSSProperty_outline_width:
    case eCSSProperty_outline_color:
    case eCSSProperty_outline_style:
    case eCSSProperty_float_edge:
      CSS_ENSURE(Margin) {
        switch (aProperty) {
          case eCSSProperty_outline_width:      mMargin->mOutlineWidth = aValue;  break;
          case eCSSProperty_outline_color:      mMargin->mOutlineColor = aValue;  break;
          case eCSSProperty_outline_style:      mMargin->mOutlineStyle = aValue;  break;
          case eCSSProperty_float_edge:         mMargin->mFloatEdge = aValue;     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

    // nsCSSPosition
    case eCSSProperty_width:
    case eCSSProperty_min_width:
    case eCSSProperty_max_width:
    case eCSSProperty_height:
    case eCSSProperty_min_height:
    case eCSSProperty_max_height:
    case eCSSProperty_box_sizing:
    case eCSSProperty_z_index:
      CSS_ENSURE(Position) {
        switch (aProperty) {
          case eCSSProperty_width:      mPosition->mWidth = aValue;      break;
          case eCSSProperty_min_width:  mPosition->mMinWidth = aValue;   break;
          case eCSSProperty_max_width:  mPosition->mMaxWidth = aValue;   break;
          case eCSSProperty_height:     mPosition->mHeight = aValue;     break;
          case eCSSProperty_min_height: mPosition->mMinHeight = aValue;  break;
          case eCSSProperty_max_height: mPosition->mMaxHeight = aValue;  break;
          case eCSSProperty_box_sizing: mPosition->mBoxSizing = aValue;  break;
          case eCSSProperty_z_index:    mPosition->mZIndex = aValue;     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

    case eCSSProperty_top:
    case eCSSProperty_right:
    case eCSSProperty_bottom:
    case eCSSProperty_left:
      CSS_ENSURE(Position) {
        CSS_ENSURE_RECT(mPosition->mOffset) {
          switch (aProperty) {
            case eCSSProperty_top:    mPosition->mOffset->mTop = aValue;    break;
            case eCSSProperty_right:  mPosition->mOffset->mRight= aValue;   break;
            case eCSSProperty_bottom: mPosition->mOffset->mBottom = aValue; break;
            case eCSSProperty_left:   mPosition->mOffset->mLeft = aValue;   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

      // nsCSSList
    case eCSSProperty_list_style_type:
    case eCSSProperty_list_style_image:
    case eCSSProperty_list_style_position:
      CSS_ENSURE(List) {
        switch (aProperty) {
          case eCSSProperty_list_style_type:      mList->mType = aValue;     break;
          case eCSSProperty_list_style_image:     mList->mImage = aValue;    break;
          case eCSSProperty_list_style_position:  mList->mPosition = aValue; break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

      // nsCSSTable
    case eCSSProperty_border_collapse:
    case eCSSProperty_border_x_spacing:
    case eCSSProperty_border_y_spacing:
    case eCSSProperty_caption_side:
    case eCSSProperty_empty_cells:
    case eCSSProperty_table_layout:
      CSS_ENSURE(Table) {
        switch (aProperty) {
          case eCSSProperty_border_collapse:  mTable->mBorderCollapse = aValue; break;
          case eCSSProperty_border_x_spacing: mTable->mBorderSpacingX = aValue; break;
          case eCSSProperty_border_y_spacing: mTable->mBorderSpacingY = aValue; break;
          case eCSSProperty_caption_side:     mTable->mCaptionSide = aValue;    break;
          case eCSSProperty_empty_cells:      mTable->mEmptyCells = aValue;     break;
          case eCSSProperty_table_layout:     mTable->mLayout = aValue;         break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

      // nsCSSBreaks
    case eCSSProperty_orphans:
    case eCSSProperty_widows:
    case eCSSProperty_page:
    case eCSSProperty_page_break_after:
    case eCSSProperty_page_break_before:
    case eCSSProperty_page_break_inside:
      CSS_ENSURE(Breaks) {
        switch (aProperty) {
          case eCSSProperty_orphans:            mBreaks->mOrphans = aValue;         break;
          case eCSSProperty_widows:             mBreaks->mWidows = aValue;          break;
          case eCSSProperty_page:               mBreaks->mPage = aValue;            break;
          case eCSSProperty_page_break_after:   mBreaks->mPageBreakAfter = aValue;  break;
          case eCSSProperty_page_break_before:  mBreaks->mPageBreakBefore = aValue; break;
          case eCSSProperty_page_break_inside:  mBreaks->mPageBreakInside = aValue; break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

      // nsCSSPage
    case eCSSProperty_marks:
    case eCSSProperty_size_width:
    case eCSSProperty_size_height:
      CSS_ENSURE(Page) {
        switch (aProperty) {
          case eCSSProperty_marks:        mPage->mMarks = aValue; break;
          case eCSSProperty_size_width:   mPage->mSizeWidth = aValue;  break;
          case eCSSProperty_size_height:  mPage->mSizeHeight = aValue;  break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

      // nsCSSContent
    case eCSSProperty_content:
    case eCSSProperty_counter_increment:
    case eCSSProperty_counter_reset:
    case eCSSProperty_marker_offset:
    case eCSSProperty_quotes_open:
    case eCSSProperty_quotes_close:
      CSS_ENSURE(Content) {
        switch (aProperty) {
          case eCSSProperty_content:
            CSS_ENSURE_DATA(mContent->mContent, nsCSSValueList) {
              mContent->mContent->mValue = aValue;          
              CSS_IF_DELETE(mContent->mContent->mNext);
            }
            break;
          case eCSSProperty_counter_increment:
            CSS_ENSURE_DATA(mContent->mCounterIncrement, nsCSSCounterData) {
              mContent->mCounterIncrement->mCounter = aValue; 
              CSS_IF_DELETE(mContent->mCounterIncrement->mNext);
            }
            break;
          case eCSSProperty_counter_reset:
            CSS_ENSURE_DATA(mContent->mCounterReset, nsCSSCounterData) {
              mContent->mCounterReset->mCounter = aValue;
              CSS_IF_DELETE(mContent->mCounterReset->mNext);
            }
            break;
          case eCSSProperty_marker_offset:      mContent->mMarkerOffset = aValue;     break;
          case eCSSProperty_quotes_open:
            CSS_ENSURE_DATA(mContent->mQuotes, nsCSSQuotes) {
              mContent->mQuotes->mOpen = aValue;          
              CSS_IF_DELETE(mContent->mQuotes->mNext);
            }
            break;
          case eCSSProperty_quotes_close:
            CSS_ENSURE_DATA(mContent->mQuotes, nsCSSQuotes) {
              mContent->mQuotes->mClose = aValue;          
              CSS_IF_DELETE(mContent->mQuotes->mNext);
            }
            break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

    // nsCSSUserInterface
    case eCSSProperty_user_input:
    case eCSSProperty_user_modify:
    case eCSSProperty_user_select:
    case eCSSProperty_key_equivalent:
    case eCSSProperty_user_focus:
    case eCSSProperty_resizer:
    case eCSSProperty_cursor:
      CSS_ENSURE(UserInterface) {
        switch (aProperty) {
          case eCSSProperty_user_input:       mUserInterface->mUserInput = aValue;      break;
          case eCSSProperty_user_modify:      mUserInterface->mUserModify = aValue;     break;
          case eCSSProperty_user_select:      mUserInterface->mUserSelect = aValue;     break;
          case eCSSProperty_key_equivalent: 
            CSS_ENSURE_DATA(mUserInterface->mKeyEquivalent, nsCSSValueList) {
              mUserInterface->mKeyEquivalent->mValue = aValue;
              CSS_IF_DELETE(mUserInterface->mKeyEquivalent->mNext);
            }
            break;
          case eCSSProperty_user_focus:       mUserInterface->mUserFocus = aValue;      break;
          case eCSSProperty_resizer:          mUserInterface->mResizer = aValue;        break;
          case eCSSProperty_cursor:
            CSS_ENSURE_DATA(mUserInterface->mCursor, nsCSSValueList) {
              mUserInterface->mCursor->mValue = aValue;
              CSS_IF_DELETE(mUserInterface->mCursor->mNext);
            }
            break;
          
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

#ifdef INCLUDE_XUL
    // nsCSSXUL
    case eCSSProperty_box_align:
    case eCSSProperty_box_direction:
    case eCSSProperty_box_flex:
    case eCSSProperty_box_orient:
    case eCSSProperty_box_pack:
    case eCSSProperty_box_ordinal_group:
      CSS_ENSURE(XUL) {
        switch (aProperty) {
          case eCSSProperty_box_align:         mXUL->mBoxAlign     = aValue;   break;
          case eCSSProperty_box_direction:     mXUL->mBoxDirection = aValue;   break;
          case eCSSProperty_box_flex:          mXUL->mBoxFlex      = aValue;   break;
          case eCSSProperty_box_orient:        mXUL->mBoxOrient    = aValue;   break;
          case eCSSProperty_box_pack:          mXUL->mBoxPack      = aValue;   break;
          case eCSSProperty_box_ordinal_group: mXUL->mBoxOrdinal   = aValue;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
#endif

      // nsCSSAural
    case eCSSProperty_azimuth:
    case eCSSProperty_elevation:
    case eCSSProperty_cue_after:
    case eCSSProperty_cue_before:
    case eCSSProperty_pause_after:
    case eCSSProperty_pause_before:
    case eCSSProperty_pitch:
    case eCSSProperty_pitch_range:
    case eCSSProperty_play_during:
    case eCSSProperty_play_during_flags:
    case eCSSProperty_richness:
    case eCSSProperty_speak:
    case eCSSProperty_speak_header:
    case eCSSProperty_speak_numeral:
    case eCSSProperty_speak_punctuation:
    case eCSSProperty_speech_rate:
    case eCSSProperty_stress:
    case eCSSProperty_voice_family:
    case eCSSProperty_volume:
      CSS_ENSURE(Aural) {
        switch (aProperty) {
          case eCSSProperty_azimuth:            mAural->mAzimuth = aValue;          break;
          case eCSSProperty_elevation:          mAural->mElevation = aValue;        break;
          case eCSSProperty_cue_after:          mAural->mCueAfter = aValue;         break;
          case eCSSProperty_cue_before:         mAural->mCueBefore = aValue;        break;
          case eCSSProperty_pause_after:        mAural->mPauseAfter = aValue;       break;
          case eCSSProperty_pause_before:       mAural->mPauseBefore = aValue;      break;
          case eCSSProperty_pitch:              mAural->mPitch = aValue;            break;
          case eCSSProperty_pitch_range:        mAural->mPitchRange = aValue;       break;
          case eCSSProperty_play_during:        mAural->mPlayDuring = aValue;       break;
          case eCSSProperty_play_during_flags:  mAural->mPlayDuringFlags = aValue;  break;
          case eCSSProperty_richness:           mAural->mRichness = aValue;         break;
          case eCSSProperty_speak:              mAural->mSpeak = aValue;            break;
          case eCSSProperty_speak_header:       mAural->mSpeakHeader = aValue;      break;
          case eCSSProperty_speak_numeral:      mAural->mSpeakNumeral = aValue;     break;
          case eCSSProperty_speak_punctuation:  mAural->mSpeakPunctuation = aValue; break;
          case eCSSProperty_speech_rate:        mAural->mSpeechRate = aValue;       break;
          case eCSSProperty_stress:             mAural->mStress = aValue;           break;
          case eCSSProperty_voice_family:       mAural->mVoiceFamily = aValue;      break;
          case eCSSProperty_volume:             mAural->mVolume = aValue;           break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

      // Shorthands
    case eCSSProperty_background:
    case eCSSProperty_border:
    case eCSSProperty_border_spacing:
    case eCSSProperty_clip:
    case eCSSProperty_cue:
    case eCSSProperty_font:
    case eCSSProperty_list_style:
    case eCSSProperty_margin:
    case eCSSProperty_outline:
    case eCSSProperty_padding:
    case eCSSProperty_pause:
    case eCSSProperty_quotes:
    case eCSSProperty_size:
    case eCSSProperty_text_shadow:
    case eCSSProperty_background_position:
    case eCSSProperty_border_top:
    case eCSSProperty_border_right:
    case eCSSProperty_border_bottom:
    case eCSSProperty_border_left:
    case eCSSProperty_border_color:
    case eCSSProperty_border_style:
    case eCSSProperty_border_width:
    case eCSSProperty__moz_border_radius:
    case eCSSProperty__moz_outline_radius:
      NS_ERROR("can't append shorthand properties");
//    default:  // XXX explicitly removing default case so compiler will help find missed props
    case eCSSProperty_UNKNOWN:
    case eCSSProperty_COUNT:
      result = NS_ERROR_ILLEGAL_VALUE;
      break;
  }

  if (NS_OK == result) {
    if (nsnull == mOrder) {
      mOrder = new nsAutoVoidArray();
    }
    else {
      // order IS important for CSS, so remove and add to the end
      PRInt32 index = mOrder->IndexOf((void*)aProperty);
      if (-1 != index) {
        mOrder->RemoveElementAt(index);
      }
    }
    if (nsnull != mOrder) {
      if (eCSSUnit_Null != aValue.GetUnit()) {
        mOrder->AppendElement((void*)(PRInt32)aProperty);
      }
    }
  }
  return result;
}

NS_IMETHODIMP
CSSDeclarationImpl::AppendStructValue(nsCSSProperty aProperty, void* aStruct)
{
  NS_ASSERTION(nsnull != aStruct, "must have struct");
  if (nsnull == aStruct) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult result = NS_OK;
  switch (aProperty) {
    case eCSSProperty_cursor:
      CSS_ENSURE(UserInterface) {
        CSS_IF_DELETE(mUserInterface->mCursor);
        mUserInterface->mCursor = (nsCSSValueList*)aStruct;
      }
      break;

    case eCSSProperty_text_shadow:
      CSS_ENSURE(Text) {
        CSS_IF_DELETE(mText->mTextShadow);
        mText->mTextShadow = (nsCSSShadow*)aStruct;
      }
      break;

    case eCSSProperty_content:
      CSS_ENSURE(Content) {
        CSS_IF_DELETE(mContent->mContent);
        mContent->mContent = (nsCSSValueList*)aStruct;
      }
      break;

    case eCSSProperty_counter_increment:
      CSS_ENSURE(Content) {
        CSS_IF_DELETE(mContent->mCounterIncrement);
        mContent->mCounterIncrement = (nsCSSCounterData*)aStruct;
      }
      break;

    case eCSSProperty_counter_reset:
      CSS_ENSURE(Content) {
        CSS_IF_DELETE(mContent->mCounterReset);
        mContent->mCounterReset = (nsCSSCounterData*)aStruct;
      }
      break;

    case eCSSProperty_quotes:
      CSS_ENSURE(Content) {
        CSS_IF_DELETE(mContent->mQuotes);
        mContent->mQuotes = (nsCSSQuotes*)aStruct;
      }
      break;

    case eCSSProperty_key_equivalent:
      CSS_ENSURE(UserInterface) {
        CSS_IF_DELETE(mUserInterface->mKeyEquivalent);
        mUserInterface->mKeyEquivalent = (nsCSSValueList*)aStruct;
      }
      break;

    default:
      NS_ERROR("not a struct property");
      result = NS_ERROR_ILLEGAL_VALUE;
      break;
  }

  if (NS_OK == result) {
    if (nsnull == mOrder) {
      mOrder = new nsAutoVoidArray();
    }
    else {
      // order IS important for CSS, so remove and add to the end
      PRInt32 index = mOrder->IndexOf((void*)(PRInt32)aProperty);
      if (-1 != index) {
        mOrder->RemoveElementAt(index);
      }
    }
    if (nsnull != mOrder) {
      mOrder->AppendElement((void*)(PRInt32)aProperty);
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
CSSDeclarationImpl::SetValueImportant(nsCSSProperty aProperty)
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
      case eCSSProperty_font_family:
      case eCSSProperty_font_style:
      case eCSSProperty_font_variant:
      case eCSSProperty_font_weight:
      case eCSSProperty_font_size:
      case eCSSProperty_font_size_adjust:
      case eCSSProperty_font_stretch:
        if (nsnull != mFont) {
          CSS_ENSURE_IMPORTANT(Font) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_font_family, mFont->mFamily);
              CSS_CASE_IMPORTANT(eCSSProperty_font_style, mFont->mStyle);
              CSS_CASE_IMPORTANT(eCSSProperty_font_variant, mFont->mVariant);
              CSS_CASE_IMPORTANT(eCSSProperty_font_weight, mFont->mWeight);
              CSS_CASE_IMPORTANT(eCSSProperty_font_size, mFont->mSize);
              CSS_CASE_IMPORTANT(eCSSProperty_font_size_adjust, mFont->mSizeAdjust);
              CSS_CASE_IMPORTANT(eCSSProperty_font_stretch, mFont->mStretch);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;

      // nsCSSColor
      case eCSSProperty_color:
      case eCSSProperty_background_color:
      case eCSSProperty_background_image:
      case eCSSProperty_background_repeat:
      case eCSSProperty_background_attachment:
      case eCSSProperty_background_x_position:
      case eCSSProperty_background_y_position:
        if (nsnull != mColor) {
          CSS_ENSURE_IMPORTANT(Color) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_color,                  mColor->mColor);
              CSS_CASE_IMPORTANT(eCSSProperty_background_color,       mColor->mBackColor);
              CSS_CASE_IMPORTANT(eCSSProperty_background_image,       mColor->mBackImage);
              CSS_CASE_IMPORTANT(eCSSProperty_background_repeat,      mColor->mBackRepeat);
              CSS_CASE_IMPORTANT(eCSSProperty_background_attachment,  mColor->mBackAttachment);
              CSS_CASE_IMPORTANT(eCSSProperty_background_x_position,  mColor->mBackPositionX);
              CSS_CASE_IMPORTANT(eCSSProperty_background_y_position,  mColor->mBackPositionY);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;

      // nsCSSText
      case eCSSProperty_word_spacing:
      case eCSSProperty_letter_spacing:
      case eCSSProperty_text_decoration:
      case eCSSProperty_vertical_align:
      case eCSSProperty_text_transform:
      case eCSSProperty_text_align:
      case eCSSProperty_text_indent:
      case eCSSProperty_unicode_bidi:
      case eCSSProperty_line_height:
      case eCSSProperty_white_space:
        if (nsnull != mText) {
          CSS_ENSURE_IMPORTANT(Text) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_word_spacing,     mText->mWordSpacing);
              CSS_CASE_IMPORTANT(eCSSProperty_letter_spacing,   mText->mLetterSpacing);
              CSS_CASE_IMPORTANT(eCSSProperty_text_decoration,  mText->mDecoration);
              CSS_CASE_IMPORTANT(eCSSProperty_vertical_align,   mText->mVerticalAlign);
              CSS_CASE_IMPORTANT(eCSSProperty_text_transform,   mText->mTextTransform);
              CSS_CASE_IMPORTANT(eCSSProperty_text_align,       mText->mTextAlign);
              CSS_CASE_IMPORTANT(eCSSProperty_text_indent,      mText->mTextIndent);
              CSS_CASE_IMPORTANT(eCSSProperty_unicode_bidi,     mText->mUnicodeBidi);
              CSS_CASE_IMPORTANT(eCSSProperty_line_height,      mText->mLineHeight);
              CSS_CASE_IMPORTANT(eCSSProperty_white_space,      mText->mWhiteSpace);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;

      case eCSSProperty_text_shadow:
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
      case eCSSProperty_direction:
      case eCSSProperty_display:
      case eCSSProperty_binding:
      case eCSSProperty_float:
      case eCSSProperty_clear:
      case eCSSProperty_overflow:
      case eCSSProperty_visibility:
      case eCSSProperty_opacity:
      case eCSSProperty_position:
        if (nsnull != mDisplay) {
          CSS_ENSURE_IMPORTANT(Display) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_direction,  mDisplay->mDirection);
              CSS_CASE_IMPORTANT(eCSSProperty_display,    mDisplay->mDisplay);
              CSS_CASE_IMPORTANT(eCSSProperty_binding,    mDisplay->mBinding);
              CSS_CASE_IMPORTANT(eCSSProperty_position,   mDisplay->mPosition);
              CSS_CASE_IMPORTANT(eCSSProperty_float,      mDisplay->mFloat);
              CSS_CASE_IMPORTANT(eCSSProperty_clear,      mDisplay->mClear);
              CSS_CASE_IMPORTANT(eCSSProperty_overflow,   mDisplay->mOverflow);
              CSS_CASE_IMPORTANT(eCSSProperty_visibility, mDisplay->mVisibility);
              CSS_CASE_IMPORTANT(eCSSProperty_opacity,    mDisplay->mOpacity);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;

      case eCSSProperty_clip_top:
      case eCSSProperty_clip_right:
      case eCSSProperty_clip_bottom:
      case eCSSProperty_clip_left:
        if (nsnull != mDisplay) {
          if (nsnull != mDisplay->mClip) {
            CSS_ENSURE_IMPORTANT(Display) {
              CSS_ENSURE_RECT(mImportant->mDisplay->mClip) {
                switch(aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty_clip_top,     mDisplay->mClip->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty_clip_right,   mDisplay->mClip->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty_clip_bottom,  mDisplay->mClip->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty_clip_left,    mDisplay->mClip->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;

      // nsCSSMargin
      case eCSSProperty_margin_top:
      case eCSSProperty_margin_right:
      case eCSSProperty_margin_bottom:
      case eCSSProperty_margin_left:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mMargin) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(mImportant->mMargin->mMargin) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty_margin_top,     mMargin->mMargin->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty_margin_right,   mMargin->mMargin->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty_margin_bottom,  mMargin->mMargin->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty_margin_left,    mMargin->mMargin->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;

      case eCSSProperty_padding_top:
      case eCSSProperty_padding_right:
      case eCSSProperty_padding_bottom:
      case eCSSProperty_padding_left:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mPadding) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(mImportant->mMargin->mPadding) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty_padding_top,    mMargin->mPadding->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty_padding_right,  mMargin->mPadding->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty_padding_bottom, mMargin->mPadding->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty_padding_left,   mMargin->mPadding->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;

      case eCSSProperty_border_top_width:
      case eCSSProperty_border_right_width:
      case eCSSProperty_border_bottom_width:
      case eCSSProperty_border_left_width:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mBorderWidth) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(mImportant->mMargin->mBorderWidth) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty_border_top_width,     mMargin->mBorderWidth->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_right_width,   mMargin->mBorderWidth->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_bottom_width,  mMargin->mBorderWidth->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_left_width,    mMargin->mBorderWidth->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;

      case eCSSProperty_border_top_color:
      case eCSSProperty_border_right_color:
      case eCSSProperty_border_bottom_color:
      case eCSSProperty_border_left_color:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mBorderColor) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(mImportant->mMargin->mBorderColor) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty_border_top_color,     mMargin->mBorderColor->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_right_color,   mMargin->mBorderColor->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_bottom_color,  mMargin->mBorderColor->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_left_color,    mMargin->mBorderColor->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;

      case eCSSProperty_border_top_style:
      case eCSSProperty_border_right_style:
      case eCSSProperty_border_bottom_style:
      case eCSSProperty_border_left_style:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mBorderStyle) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(mImportant->mMargin->mBorderStyle) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty_border_top_style,     mMargin->mBorderStyle->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_right_style,   mMargin->mBorderStyle->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_bottom_style,  mMargin->mBorderStyle->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_left_style,    mMargin->mBorderStyle->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;

      case eCSSProperty__moz_border_radius_topLeft:
      case eCSSProperty__moz_border_radius_topRight:
      case eCSSProperty__moz_border_radius_bottomRight:
      case eCSSProperty__moz_border_radius_bottomLeft:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mBorderRadius) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(mImportant->mMargin->mBorderRadius) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_border_radius_topLeft,			mMargin->mBorderRadius->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_border_radius_topRight,		mMargin->mBorderRadius->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_border_radius_bottomRight,	mMargin->mBorderRadius->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_border_radius_bottomLeft,	mMargin->mBorderRadius->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;

      case eCSSProperty__moz_outline_radius_topLeft:
      case eCSSProperty__moz_outline_radius_topRight:
      case eCSSProperty__moz_outline_radius_bottomRight:
      case eCSSProperty__moz_outline_radius_bottomLeft:
        if (nsnull != mMargin) {
          if (nsnull != mMargin->mOutlineRadius) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(mImportant->mMargin->mOutlineRadius) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_outline_radius_topLeft,			mMargin->mOutlineRadius->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_outline_radius_topRight,			mMargin->mOutlineRadius->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_outline_radius_bottomRight,	mMargin->mOutlineRadius->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_outline_radius_bottomLeft,		mMargin->mOutlineRadius->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;

      case eCSSProperty_outline_width:
      case eCSSProperty_outline_color:
      case eCSSProperty_outline_style:
      case eCSSProperty_float_edge:
        if (nsnull != mMargin) {
          CSS_ENSURE_IMPORTANT(Margin) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_outline_width,      mMargin->mOutlineWidth);
              CSS_CASE_IMPORTANT(eCSSProperty_outline_color,      mMargin->mOutlineColor);
              CSS_CASE_IMPORTANT(eCSSProperty_outline_style,      mMargin->mOutlineStyle);
              CSS_CASE_IMPORTANT(eCSSProperty_float_edge,         mMargin->mFloatEdge);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;

      // nsCSSPosition
      case eCSSProperty_width:
      case eCSSProperty_min_width:
      case eCSSProperty_max_width:
      case eCSSProperty_height:
      case eCSSProperty_min_height:
      case eCSSProperty_max_height:
      case eCSSProperty_box_sizing:
      case eCSSProperty_z_index:
        if (nsnull != mPosition) {
          CSS_ENSURE_IMPORTANT(Position) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_width,      mPosition->mWidth);
              CSS_CASE_IMPORTANT(eCSSProperty_min_width,  mPosition->mMinWidth);
              CSS_CASE_IMPORTANT(eCSSProperty_max_width,  mPosition->mMaxWidth);
              CSS_CASE_IMPORTANT(eCSSProperty_height,     mPosition->mHeight);
              CSS_CASE_IMPORTANT(eCSSProperty_min_height, mPosition->mMinHeight);
              CSS_CASE_IMPORTANT(eCSSProperty_max_height, mPosition->mMaxHeight);
              CSS_CASE_IMPORTANT(eCSSProperty_box_sizing, mPosition->mBoxSizing);
              CSS_CASE_IMPORTANT(eCSSProperty_z_index,    mPosition->mZIndex);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;

      case eCSSProperty_top:
      case eCSSProperty_right:
      case eCSSProperty_bottom:
      case eCSSProperty_left:
        if (nsnull != mPosition) {
          if (nsnull != mPosition->mOffset) {
            CSS_ENSURE_IMPORTANT(Position) {
              CSS_ENSURE_RECT(mImportant->mPosition->mOffset) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty_top,    mPosition->mOffset->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty_right,  mPosition->mOffset->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty_bottom, mPosition->mOffset->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty_left,   mPosition->mOffset->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;

        // nsCSSList
      case eCSSProperty_list_style_type:
      case eCSSProperty_list_style_image:
      case eCSSProperty_list_style_position:
        if (nsnull != mList) {
          CSS_ENSURE_IMPORTANT(List) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_list_style_type,      mList->mType);
              CSS_CASE_IMPORTANT(eCSSProperty_list_style_image,     mList->mImage);
              CSS_CASE_IMPORTANT(eCSSProperty_list_style_position,  mList->mPosition);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;

        // nsCSSTable
      case eCSSProperty_border_collapse:
      case eCSSProperty_border_x_spacing:
      case eCSSProperty_border_y_spacing:
      case eCSSProperty_caption_side:
      case eCSSProperty_empty_cells:
      case eCSSProperty_table_layout:
        if (nsnull != mTable) {
          CSS_ENSURE_IMPORTANT(Table) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_border_collapse,  mTable->mBorderCollapse);
              CSS_CASE_IMPORTANT(eCSSProperty_border_x_spacing, mTable->mBorderSpacingX);
              CSS_CASE_IMPORTANT(eCSSProperty_border_y_spacing, mTable->mBorderSpacingY);
              CSS_CASE_IMPORTANT(eCSSProperty_caption_side,     mTable->mCaptionSide);
              CSS_CASE_IMPORTANT(eCSSProperty_empty_cells,      mTable->mEmptyCells);
              CSS_CASE_IMPORTANT(eCSSProperty_table_layout,     mTable->mLayout);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;

        // nsCSSBreaks
      case eCSSProperty_orphans:
      case eCSSProperty_widows:
      case eCSSProperty_page:
      case eCSSProperty_page_break_after:
      case eCSSProperty_page_break_before:
      case eCSSProperty_page_break_inside:
        if (nsnull != mBreaks) {
          CSS_ENSURE_IMPORTANT(Breaks) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_orphans,            mBreaks->mOrphans);
              CSS_CASE_IMPORTANT(eCSSProperty_widows,             mBreaks->mWidows);
              CSS_CASE_IMPORTANT(eCSSProperty_page,               mBreaks->mPage);
              CSS_CASE_IMPORTANT(eCSSProperty_page_break_after,   mBreaks->mPageBreakAfter);
              CSS_CASE_IMPORTANT(eCSSProperty_page_break_before,  mBreaks->mPageBreakBefore);
              CSS_CASE_IMPORTANT(eCSSProperty_page_break_inside,  mBreaks->mPageBreakInside);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;

        // nsCSSPage
      case eCSSProperty_marks:
      case eCSSProperty_size_width:
      case eCSSProperty_size_height:
        if (nsnull != mPage) {
          CSS_ENSURE_IMPORTANT(Page) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_marks,        mPage->mMarks);
              CSS_CASE_IMPORTANT(eCSSProperty_size_width,   mPage->mSizeWidth);
              CSS_CASE_IMPORTANT(eCSSProperty_size_height,  mPage->mSizeHeight);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;

        // nsCSSContent
      case eCSSProperty_content:
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

      case eCSSProperty_counter_increment:
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

      case eCSSProperty_counter_reset:
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

      case eCSSProperty_marker_offset:
        if (nsnull != mContent) {
          CSS_ENSURE_IMPORTANT(Content) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_marker_offset,  mContent->mMarkerOffset);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;

      case eCSSProperty_quotes:
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

      // nsCSSUserInterface
      case eCSSProperty_user_input:
      case eCSSProperty_user_modify:
      case eCSSProperty_user_select:
      case eCSSProperty_user_focus:
      case eCSSProperty_resizer:
        if (nsnull != mUserInterface) {
          CSS_ENSURE_IMPORTANT(UserInterface) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_user_input,   mUserInterface->mUserInput);
              CSS_CASE_IMPORTANT(eCSSProperty_user_modify,  mUserInterface->mUserModify);
              CSS_CASE_IMPORTANT(eCSSProperty_user_select,  mUserInterface->mUserSelect);
              CSS_CASE_IMPORTANT(eCSSProperty_user_focus,   mUserInterface->mUserFocus);
              CSS_CASE_IMPORTANT(eCSSProperty_resizer,      mUserInterface->mResizer);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;

      case eCSSProperty_key_equivalent:
        if (nsnull != mUserInterface) {
          if (nsnull != mUserInterface->mKeyEquivalent) {
            CSS_ENSURE_IMPORTANT(UserInterface) {
              CSS_IF_DELETE(mImportant->mUserInterface->mKeyEquivalent);
              mImportant->mUserInterface->mKeyEquivalent = mUserInterface->mKeyEquivalent;
              mUserInterface->mKeyEquivalent = nsnull;
            }
          }
        }
        break;

      case eCSSProperty_cursor:
        if (nsnull != mUserInterface) {
          if (nsnull != mUserInterface->mCursor) {
            CSS_ENSURE_IMPORTANT(UserInterface) {
              CSS_IF_DELETE(mImportant->mUserInterface->mCursor);
              mImportant->mUserInterface->mCursor = mUserInterface->mCursor;
              mUserInterface->mCursor = nsnull;
            }
          }
        }
        break;

#ifdef INCLUDE_XUL
      // nsCSSXUL
      case eCSSProperty_box_align:
      case eCSSProperty_box_direction:
      case eCSSProperty_box_flex:
      case eCSSProperty_box_orient:
      case eCSSProperty_box_pack:
      case eCSSProperty_box_ordinal_group:
        if (nsnull != mXUL) {
          CSS_ENSURE_IMPORTANT(XUL) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_box_align,         mXUL->mBoxAlign);
              CSS_CASE_IMPORTANT(eCSSProperty_box_direction,     mXUL->mBoxDirection);
              CSS_CASE_IMPORTANT(eCSSProperty_box_flex,          mXUL->mBoxFlex);
              CSS_CASE_IMPORTANT(eCSSProperty_box_orient,        mXUL->mBoxOrient);
              CSS_CASE_IMPORTANT(eCSSProperty_box_pack,          mXUL->mBoxPack);
              CSS_CASE_IMPORTANT(eCSSProperty_box_ordinal_group, mXUL->mBoxOrdinal);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
#endif

        // nsCSSAural
      case eCSSProperty_azimuth:
      case eCSSProperty_elevation:
      case eCSSProperty_cue_after:
      case eCSSProperty_cue_before:
      case eCSSProperty_pause_after:
      case eCSSProperty_pause_before:
      case eCSSProperty_pitch:
      case eCSSProperty_pitch_range:
      case eCSSProperty_richness:
      case eCSSProperty_speak:
      case eCSSProperty_speak_header:
      case eCSSProperty_speak_numeral:
      case eCSSProperty_speak_punctuation:
      case eCSSProperty_speech_rate:
      case eCSSProperty_stress:
      case eCSSProperty_voice_family:
      case eCSSProperty_volume:
        if (nsnull != mAural) {
          CSS_ENSURE_IMPORTANT(Aural) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_azimuth,            mAural->mAzimuth);
              CSS_CASE_IMPORTANT(eCSSProperty_elevation,          mAural->mElevation);
              CSS_CASE_IMPORTANT(eCSSProperty_cue_after,          mAural->mCueAfter);
              CSS_CASE_IMPORTANT(eCSSProperty_cue_before,         mAural->mCueBefore);
              CSS_CASE_IMPORTANT(eCSSProperty_pause_after,        mAural->mPauseAfter);
              CSS_CASE_IMPORTANT(eCSSProperty_pause_before,       mAural->mPauseBefore);
              CSS_CASE_IMPORTANT(eCSSProperty_pitch,              mAural->mPitch);
              CSS_CASE_IMPORTANT(eCSSProperty_pitch_range,        mAural->mPitchRange);
              CSS_CASE_IMPORTANT(eCSSProperty_richness,           mAural->mRichness);
              CSS_CASE_IMPORTANT(eCSSProperty_speak,              mAural->mSpeak);
              CSS_CASE_IMPORTANT(eCSSProperty_speak_header,       mAural->mSpeakHeader);
              CSS_CASE_IMPORTANT(eCSSProperty_speak_numeral,      mAural->mSpeakNumeral);
              CSS_CASE_IMPORTANT(eCSSProperty_speak_punctuation,  mAural->mSpeakPunctuation);
              CSS_CASE_IMPORTANT(eCSSProperty_speech_rate,        mAural->mSpeechRate);
              CSS_CASE_IMPORTANT(eCSSProperty_stress,             mAural->mStress);
              CSS_CASE_IMPORTANT(eCSSProperty_voice_family,       mAural->mVoiceFamily);
              CSS_CASE_IMPORTANT(eCSSProperty_volume,             mAural->mVolume);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;

      case eCSSProperty_play_during:
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
      case eCSSProperty_background:
        SetValueImportant(eCSSProperty_background_color);
        SetValueImportant(eCSSProperty_background_image);
        SetValueImportant(eCSSProperty_background_repeat);
        SetValueImportant(eCSSProperty_background_attachment);
        SetValueImportant(eCSSProperty_background_x_position);
        SetValueImportant(eCSSProperty_background_y_position);
        break;
      case eCSSProperty_border:
        SetValueImportant(eCSSProperty_border_top_width);
        SetValueImportant(eCSSProperty_border_right_width);
        SetValueImportant(eCSSProperty_border_bottom_width);
        SetValueImportant(eCSSProperty_border_left_width);
        SetValueImportant(eCSSProperty_border_top_style);
        SetValueImportant(eCSSProperty_border_right_style);
        SetValueImportant(eCSSProperty_border_bottom_style);
        SetValueImportant(eCSSProperty_border_left_style);
        SetValueImportant(eCSSProperty_border_top_color);
        SetValueImportant(eCSSProperty_border_right_color);
        SetValueImportant(eCSSProperty_border_bottom_color);
        SetValueImportant(eCSSProperty_border_left_color);
        break;
      case eCSSProperty_border_spacing:
        SetValueImportant(eCSSProperty_border_x_spacing);
        SetValueImportant(eCSSProperty_border_y_spacing);
        break;
      case eCSSProperty_clip:
        SetValueImportant(eCSSProperty_clip_top);
        SetValueImportant(eCSSProperty_clip_right);
        SetValueImportant(eCSSProperty_clip_bottom);
        SetValueImportant(eCSSProperty_clip_left);
        break;
      case eCSSProperty_cue:
        SetValueImportant(eCSSProperty_cue_after);
        SetValueImportant(eCSSProperty_cue_before);
        break;
      case eCSSProperty_font:
        SetValueImportant(eCSSProperty_font_family);
        SetValueImportant(eCSSProperty_font_style);
        SetValueImportant(eCSSProperty_font_variant);
        SetValueImportant(eCSSProperty_font_weight);
        SetValueImportant(eCSSProperty_font_size);
        SetValueImportant(eCSSProperty_line_height);
        break;
      case eCSSProperty_list_style:
        SetValueImportant(eCSSProperty_list_style_type);
        SetValueImportant(eCSSProperty_list_style_image);
        SetValueImportant(eCSSProperty_list_style_position);
        break;
      case eCSSProperty_margin:
        SetValueImportant(eCSSProperty_margin_top);
        SetValueImportant(eCSSProperty_margin_right);
        SetValueImportant(eCSSProperty_margin_bottom);
        SetValueImportant(eCSSProperty_margin_left);
        break;
      case eCSSProperty_outline:
        SetValueImportant(eCSSProperty_outline_color);
        SetValueImportant(eCSSProperty_outline_style);
        SetValueImportant(eCSSProperty_outline_width);
        break;
      case eCSSProperty_padding:
        SetValueImportant(eCSSProperty_padding_top);
        SetValueImportant(eCSSProperty_padding_right);
        SetValueImportant(eCSSProperty_padding_bottom);
        SetValueImportant(eCSSProperty_padding_left);
        break;
      case eCSSProperty_pause:
        SetValueImportant(eCSSProperty_pause_after);
        SetValueImportant(eCSSProperty_pause_before);
        break;
      case eCSSProperty_size:
        SetValueImportant(eCSSProperty_size_width);
        SetValueImportant(eCSSProperty_size_height);
        break;
      case eCSSProperty_background_position:
        SetValueImportant(eCSSProperty_background_x_position);
        SetValueImportant(eCSSProperty_background_y_position);
        break;
      case eCSSProperty_border_top:
        SetValueImportant(eCSSProperty_border_top_width);
        SetValueImportant(eCSSProperty_border_top_style);
        SetValueImportant(eCSSProperty_border_top_color);
        break;
      case eCSSProperty_border_right:
        SetValueImportant(eCSSProperty_border_right_width);
        SetValueImportant(eCSSProperty_border_right_style);
        SetValueImportant(eCSSProperty_border_right_color);
        break;
      case eCSSProperty_border_bottom:
        SetValueImportant(eCSSProperty_border_bottom_width);
        SetValueImportant(eCSSProperty_border_bottom_style);
        SetValueImportant(eCSSProperty_border_bottom_color);
        break;
      case eCSSProperty_border_left:
        SetValueImportant(eCSSProperty_border_left_width);
        SetValueImportant(eCSSProperty_border_left_style);
        SetValueImportant(eCSSProperty_border_left_color);
        break;
      case eCSSProperty_border_color:
        SetValueImportant(eCSSProperty_border_top_color);
        SetValueImportant(eCSSProperty_border_right_color);
        SetValueImportant(eCSSProperty_border_bottom_color);
        SetValueImportant(eCSSProperty_border_left_color);
        break;
      case eCSSProperty_border_style:
        SetValueImportant(eCSSProperty_border_top_style);
        SetValueImportant(eCSSProperty_border_right_style);
        SetValueImportant(eCSSProperty_border_bottom_style);
        SetValueImportant(eCSSProperty_border_left_style);
        break;
      case eCSSProperty_border_width:
        SetValueImportant(eCSSProperty_border_top_width);
        SetValueImportant(eCSSProperty_border_right_width);
        SetValueImportant(eCSSProperty_border_bottom_width);
        SetValueImportant(eCSSProperty_border_left_width);
        break;
      case eCSSProperty__moz_border_radius:
        SetValueImportant(eCSSProperty__moz_border_radius_topLeft);
        SetValueImportant(eCSSProperty__moz_border_radius_topRight);
        SetValueImportant(eCSSProperty__moz_border_radius_bottomRight);
        SetValueImportant(eCSSProperty__moz_border_radius_bottomLeft);
      	break;
      case eCSSProperty__moz_outline_radius:
        SetValueImportant(eCSSProperty__moz_outline_radius_topLeft);
        SetValueImportant(eCSSProperty__moz_outline_radius_topRight);
        SetValueImportant(eCSSProperty__moz_outline_radius_bottomRight);
        SetValueImportant(eCSSProperty__moz_outline_radius_bottomLeft);
      	break;
      default:
        result = NS_ERROR_ILLEGAL_VALUE;
        break;
    }
  }
  return result;
}


#define CSS_CHECK(data)              \
  if (nsnull == m##data) {            \
    result = NS_ERROR_NOT_AVAILABLE;  \
  }                                   \
  else

#define CSS_CHECK_RECT(data)         \
  if (nsnull == data) {               \
    result = NS_ERROR_NOT_AVAILABLE;  \
  }                                   \
  else

#define CSS_CHECK_DATA(data,type)    \
  if (nsnull == data) {               \
    result = NS_ERROR_NOT_AVAILABLE;  \
  }                                   \
  else


nsresult
CSSDeclarationImpl::RemoveProperty(nsCSSProperty aProperty)
{
  nsresult result = NS_OK;

  switch (aProperty) {
    // nsCSSFont
    case eCSSProperty_font_family:
    case eCSSProperty_font_style:
    case eCSSProperty_font_variant:
    case eCSSProperty_font_weight:
    case eCSSProperty_font_size:
    case eCSSProperty_font_size_adjust:
    case eCSSProperty_font_stretch:
      CSS_CHECK(Font) {
        switch (aProperty) {
          case eCSSProperty_font_family:      mFont->mFamily.Reset();      break;
          case eCSSProperty_font_style:       mFont->mStyle.Reset();       break;
          case eCSSProperty_font_variant:     mFont->mVariant.Reset();     break;
          case eCSSProperty_font_weight:      mFont->mWeight.Reset();      break;
          case eCSSProperty_font_size:        mFont->mSize.Reset();        break;
          case eCSSProperty_font_size_adjust: mFont->mSizeAdjust.Reset();  break;
          case eCSSProperty_font_stretch:     mFont->mStretch.Reset();     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

    // nsCSSColor
    case eCSSProperty_color:
    case eCSSProperty_background_color:
    case eCSSProperty_background_image:
    case eCSSProperty_background_repeat:
    case eCSSProperty_background_attachment:
    case eCSSProperty_background_x_position:
    case eCSSProperty_background_y_position:
      CSS_CHECK(Color) {
        switch (aProperty) {
          case eCSSProperty_color:                  mColor->mColor.Reset();           break;
          case eCSSProperty_background_color:       mColor->mBackColor.Reset();       break;
          case eCSSProperty_background_image:       mColor->mBackImage.Reset();       break;
          case eCSSProperty_background_repeat:      mColor->mBackRepeat.Reset();      break;
          case eCSSProperty_background_attachment:  mColor->mBackAttachment.Reset();  break;
          case eCSSProperty_background_x_position:  mColor->mBackPositionX.Reset();   break;
          case eCSSProperty_background_y_position:  mColor->mBackPositionY.Reset();   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

    // nsCSSText
    case eCSSProperty_word_spacing:
    case eCSSProperty_letter_spacing:
    case eCSSProperty_text_decoration:
    case eCSSProperty_vertical_align:
    case eCSSProperty_text_transform:
    case eCSSProperty_text_align:
    case eCSSProperty_text_indent:
    case eCSSProperty_unicode_bidi:
    case eCSSProperty_line_height:
    case eCSSProperty_white_space:
      CSS_CHECK(Text) {
        switch (aProperty) {
          case eCSSProperty_word_spacing:     mText->mWordSpacing.Reset();    break;
          case eCSSProperty_letter_spacing:   mText->mLetterSpacing.Reset();  break;
          case eCSSProperty_text_decoration:  mText->mDecoration.Reset();     break;
          case eCSSProperty_vertical_align:   mText->mVerticalAlign.Reset();  break;
          case eCSSProperty_text_transform:   mText->mTextTransform.Reset();  break;
          case eCSSProperty_text_align:       mText->mTextAlign.Reset();      break;
          case eCSSProperty_text_indent:      mText->mTextIndent.Reset();     break;
          case eCSSProperty_unicode_bidi:     mText->mUnicodeBidi.Reset();    break;
          case eCSSProperty_line_height:      mText->mLineHeight.Reset();     break;
          case eCSSProperty_white_space:      mText->mWhiteSpace.Reset();     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

    case eCSSProperty_text_shadow_color:
    case eCSSProperty_text_shadow_radius:
    case eCSSProperty_text_shadow_x:
    case eCSSProperty_text_shadow_y:
      CSS_CHECK(Text) {
        CSS_CHECK_DATA(mText->mTextShadow, nsCSSShadow) {
          switch (aProperty) {
            case eCSSProperty_text_shadow_color:  mText->mTextShadow->mColor.Reset();    break;
            case eCSSProperty_text_shadow_radius: mText->mTextShadow->mRadius.Reset();   break;
            case eCSSProperty_text_shadow_x:      mText->mTextShadow->mXOffset.Reset();  break;
            case eCSSProperty_text_shadow_y:      mText->mTextShadow->mYOffset.Reset();  break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
          CSS_IF_DELETE(mText->mTextShadow->mNext);
        }
      }
      break;

      // nsCSSDisplay
    case eCSSProperty_float:
    case eCSSProperty_clear:
    case eCSSProperty_display:
    case eCSSProperty_binding:
    case eCSSProperty_position:
    case eCSSProperty_direction:
    case eCSSProperty_visibility:
    case eCSSProperty_opacity:
    case eCSSProperty_overflow:
      CSS_CHECK(Display) {
        switch (aProperty) {
          case eCSSProperty_float:      mDisplay->mFloat.Reset();      break;
          case eCSSProperty_clear:      mDisplay->mClear.Reset();      break;
          case eCSSProperty_display:    mDisplay->mDisplay.Reset();    break;
          case eCSSProperty_position:   mDisplay->mPosition.Reset();   break;
          case eCSSProperty_binding:         
            mDisplay->mBinding.Reset();      
            break;
          case eCSSProperty_direction:  mDisplay->mDirection.Reset();  break;
          case eCSSProperty_visibility: mDisplay->mVisibility.Reset(); break;
          case eCSSProperty_opacity:    mDisplay->mOpacity.Reset();    break;
          case eCSSProperty_overflow:   mDisplay->mOverflow.Reset();   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

    case eCSSProperty_clip_top:
    case eCSSProperty_clip_right:
    case eCSSProperty_clip_bottom:
    case eCSSProperty_clip_left:
      CSS_CHECK(Display) {
        CSS_CHECK_RECT(mDisplay->mClip) {
          switch(aProperty) {
            case eCSSProperty_clip_top:     mDisplay->mClip->mTop.Reset();     break;
            case eCSSProperty_clip_right:   mDisplay->mClip->mRight.Reset();   break;
            case eCSSProperty_clip_bottom:  mDisplay->mClip->mBottom.Reset();  break;
            case eCSSProperty_clip_left:    mDisplay->mClip->mLeft.Reset();    break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    // nsCSSMargin
    case eCSSProperty_margin_top:
    case eCSSProperty_margin_right:
    case eCSSProperty_margin_bottom:
    case eCSSProperty_margin_left:
      CSS_CHECK(Margin) {
        CSS_CHECK_RECT(mMargin->mMargin) {
          switch (aProperty) {
            case eCSSProperty_margin_top:     mMargin->mMargin->mTop.Reset();     break;
            case eCSSProperty_margin_right:   mMargin->mMargin->mRight.Reset();   break;
            case eCSSProperty_margin_bottom:  mMargin->mMargin->mBottom.Reset();  break;
            case eCSSProperty_margin_left:    mMargin->mMargin->mLeft.Reset();    break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    case eCSSProperty_padding_top:
    case eCSSProperty_padding_right:
    case eCSSProperty_padding_bottom:
    case eCSSProperty_padding_left:
      CSS_CHECK(Margin) {
        CSS_CHECK_RECT(mMargin->mPadding) {
          switch (aProperty) {
            case eCSSProperty_padding_top:    mMargin->mPadding->mTop.Reset();    break;
            case eCSSProperty_padding_right:  mMargin->mPadding->mRight.Reset();  break;
            case eCSSProperty_padding_bottom: mMargin->mPadding->mBottom.Reset(); break;
            case eCSSProperty_padding_left:   mMargin->mPadding->mLeft.Reset();   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    case eCSSProperty_border_top_width:
    case eCSSProperty_border_right_width:
    case eCSSProperty_border_bottom_width:
    case eCSSProperty_border_left_width:
      CSS_CHECK(Margin) {
        CSS_CHECK_RECT(mMargin->mBorderWidth) {
          switch (aProperty) {
            case eCSSProperty_border_top_width:     mMargin->mBorderWidth->mTop.Reset();     break;
            case eCSSProperty_border_right_width:   mMargin->mBorderWidth->mRight.Reset();   break;
            case eCSSProperty_border_bottom_width:  mMargin->mBorderWidth->mBottom.Reset();  break;
            case eCSSProperty_border_left_width:    mMargin->mBorderWidth->mLeft.Reset();    break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    case eCSSProperty_border_top_color:
    case eCSSProperty_border_right_color:
    case eCSSProperty_border_bottom_color:
    case eCSSProperty_border_left_color:
      CSS_CHECK(Margin) {
        CSS_CHECK_RECT(mMargin->mBorderColor) {
          switch (aProperty) {
            case eCSSProperty_border_top_color:     mMargin->mBorderColor->mTop.Reset();    break;
            case eCSSProperty_border_right_color:   mMargin->mBorderColor->mRight.Reset();  break;
            case eCSSProperty_border_bottom_color:  mMargin->mBorderColor->mBottom.Reset(); break;
            case eCSSProperty_border_left_color:    mMargin->mBorderColor->mLeft.Reset();   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    case eCSSProperty_border_top_style:
    case eCSSProperty_border_right_style:
    case eCSSProperty_border_bottom_style:
    case eCSSProperty_border_left_style:
      CSS_CHECK(Margin) {
        CSS_CHECK_RECT(mMargin->mBorderStyle) {
          switch (aProperty) {
            case eCSSProperty_border_top_style:     mMargin->mBorderStyle->mTop.Reset();    break;
            case eCSSProperty_border_right_style:   mMargin->mBorderStyle->mRight.Reset();  break;
            case eCSSProperty_border_bottom_style:  mMargin->mBorderStyle->mBottom.Reset(); break;
            case eCSSProperty_border_left_style:    mMargin->mBorderStyle->mLeft.Reset();   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    case eCSSProperty__moz_border_radius_topLeft:
    case eCSSProperty__moz_border_radius_topRight:
    case eCSSProperty__moz_border_radius_bottomRight:
    case eCSSProperty__moz_border_radius_bottomLeft:
      CSS_CHECK(Margin) {
        CSS_CHECK_RECT(mMargin->mBorderRadius) {
          switch (aProperty) {
            case eCSSProperty__moz_border_radius_topLeft:			mMargin->mBorderRadius->mTop.Reset();    break;
            case eCSSProperty__moz_border_radius_topRight:		mMargin->mBorderRadius->mRight.Reset();  break;
            case eCSSProperty__moz_border_radius_bottomRight:	mMargin->mBorderRadius->mBottom.Reset(); break;
            case eCSSProperty__moz_border_radius_bottomLeft:	mMargin->mBorderRadius->mLeft.Reset();   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    case eCSSProperty__moz_outline_radius_topLeft:
    case eCSSProperty__moz_outline_radius_topRight:
    case eCSSProperty__moz_outline_radius_bottomRight:
    case eCSSProperty__moz_outline_radius_bottomLeft:
      CSS_CHECK(Margin) {
        CSS_CHECK_RECT(mMargin->mOutlineRadius) {
          switch (aProperty) {
            case eCSSProperty__moz_outline_radius_topLeft:			mMargin->mOutlineRadius->mTop.Reset();    break;
            case eCSSProperty__moz_outline_radius_topRight:			mMargin->mOutlineRadius->mRight.Reset();  break;
            case eCSSProperty__moz_outline_radius_bottomRight:	mMargin->mOutlineRadius->mBottom.Reset(); break;
            case eCSSProperty__moz_outline_radius_bottomLeft:		mMargin->mOutlineRadius->mLeft.Reset();   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

    case eCSSProperty_outline_width:
    case eCSSProperty_outline_color:
    case eCSSProperty_outline_style:
    case eCSSProperty_float_edge:
      CSS_CHECK(Margin) {
        switch (aProperty) {
          case eCSSProperty_outline_width:      mMargin->mOutlineWidth.Reset();  break;
          case eCSSProperty_outline_color:      mMargin->mOutlineColor.Reset();  break;
          case eCSSProperty_outline_style:      mMargin->mOutlineStyle.Reset();  break;
          case eCSSProperty_float_edge:         mMargin->mFloatEdge.Reset();     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

    // nsCSSPosition
    case eCSSProperty_width:
    case eCSSProperty_min_width:
    case eCSSProperty_max_width:
    case eCSSProperty_height:
    case eCSSProperty_min_height:
    case eCSSProperty_max_height:
    case eCSSProperty_box_sizing:
    case eCSSProperty_z_index:
      CSS_CHECK(Position) {
        switch (aProperty) {
          case eCSSProperty_width:      mPosition->mWidth.Reset();      break;
          case eCSSProperty_min_width:  mPosition->mMinWidth.Reset();   break;
          case eCSSProperty_max_width:  mPosition->mMaxWidth.Reset();   break;
          case eCSSProperty_height:     mPosition->mHeight.Reset();     break;
          case eCSSProperty_min_height: mPosition->mMinHeight.Reset();  break;
          case eCSSProperty_max_height: mPosition->mMaxHeight.Reset();  break;
          case eCSSProperty_box_sizing: mPosition->mBoxSizing.Reset();  break;
          case eCSSProperty_z_index:    mPosition->mZIndex.Reset();     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

    case eCSSProperty_top:
    case eCSSProperty_right:
    case eCSSProperty_bottom:
    case eCSSProperty_left:
      CSS_CHECK(Position) {
        CSS_CHECK_RECT(mPosition->mOffset) {
          switch (aProperty) {
            case eCSSProperty_top:    mPosition->mOffset->mTop.Reset();    break;
            case eCSSProperty_right:  mPosition->mOffset->mRight.Reset();   break;
            case eCSSProperty_bottom: mPosition->mOffset->mBottom.Reset(); break;
            case eCSSProperty_left:   mPosition->mOffset->mLeft.Reset();   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;

      // nsCSSList
    case eCSSProperty_list_style_type:
    case eCSSProperty_list_style_image:
    case eCSSProperty_list_style_position:
      CSS_CHECK(List) {
        switch (aProperty) {
          case eCSSProperty_list_style_type:      mList->mType.Reset();     break;
          case eCSSProperty_list_style_image:     mList->mImage.Reset();    break;
          case eCSSProperty_list_style_position:  mList->mPosition.Reset(); break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

      // nsCSSTable
    case eCSSProperty_border_collapse:
    case eCSSProperty_border_x_spacing:
    case eCSSProperty_border_y_spacing:
    case eCSSProperty_caption_side:
    case eCSSProperty_empty_cells:
    case eCSSProperty_table_layout:
      CSS_CHECK(Table) {
        switch (aProperty) {
          case eCSSProperty_border_collapse:  mTable->mBorderCollapse.Reset(); break;
          case eCSSProperty_border_x_spacing: mTable->mBorderSpacingX.Reset(); break;
          case eCSSProperty_border_y_spacing: mTable->mBorderSpacingY.Reset(); break;
          case eCSSProperty_caption_side:     mTable->mCaptionSide.Reset();    break;
          case eCSSProperty_empty_cells:      mTable->mEmptyCells.Reset();     break;
          case eCSSProperty_table_layout:     mTable->mLayout.Reset();         break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

      // nsCSSBreaks
    case eCSSProperty_orphans:
    case eCSSProperty_widows:
    case eCSSProperty_page:
    case eCSSProperty_page_break_after:
    case eCSSProperty_page_break_before:
    case eCSSProperty_page_break_inside:
      CSS_CHECK(Breaks) {
        switch (aProperty) {
          case eCSSProperty_orphans:            mBreaks->mOrphans.Reset();         break;
          case eCSSProperty_widows:             mBreaks->mWidows.Reset();          break;
          case eCSSProperty_page:               mBreaks->mPage.Reset();            break;
          case eCSSProperty_page_break_after:   mBreaks->mPageBreakAfter.Reset();  break;
          case eCSSProperty_page_break_before:  mBreaks->mPageBreakBefore.Reset(); break;
          case eCSSProperty_page_break_inside:  mBreaks->mPageBreakInside.Reset(); break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

      // nsCSSPage
    case eCSSProperty_marks:
    case eCSSProperty_size_width:
    case eCSSProperty_size_height:
      CSS_CHECK(Page) {
        switch (aProperty) {
          case eCSSProperty_marks:        mPage->mMarks.Reset(); break;
          case eCSSProperty_size_width:   mPage->mSizeWidth.Reset();  break;
          case eCSSProperty_size_height:  mPage->mSizeHeight.Reset();  break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

      // nsCSSContent
    case eCSSProperty_content:
    case eCSSProperty_counter_increment:
    case eCSSProperty_counter_reset:
    case eCSSProperty_marker_offset:
    case eCSSProperty_quotes_open:
    case eCSSProperty_quotes_close:
      CSS_CHECK(Content) {
        switch (aProperty) {
          case eCSSProperty_content:
            CSS_CHECK_DATA(mContent->mContent, nsCSSValueList) {
              mContent->mContent->mValue.Reset();          
              CSS_IF_DELETE(mContent->mContent->mNext);
            }
            break;
          case eCSSProperty_counter_increment:
            CSS_CHECK_DATA(mContent->mCounterIncrement, nsCSSCounterData) {
              mContent->mCounterIncrement->mCounter.Reset(); 
              CSS_IF_DELETE(mContent->mCounterIncrement->mNext);
            }
            break;
          case eCSSProperty_counter_reset:
            CSS_CHECK_DATA(mContent->mCounterReset, nsCSSCounterData) {
              mContent->mCounterReset->mCounter.Reset();
              CSS_IF_DELETE(mContent->mCounterReset->mNext);
            }
            break;
          case eCSSProperty_marker_offset:      mContent->mMarkerOffset.Reset();     break;
          case eCSSProperty_quotes_open:
            CSS_CHECK_DATA(mContent->mQuotes, nsCSSQuotes) {
              mContent->mQuotes->mOpen.Reset();          
              CSS_IF_DELETE(mContent->mQuotes->mNext);
            }
            break;
          case eCSSProperty_quotes_close:
            CSS_CHECK_DATA(mContent->mQuotes, nsCSSQuotes) {
              mContent->mQuotes->mClose.Reset();          
              CSS_IF_DELETE(mContent->mQuotes->mNext);
            }
            break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

    // nsCSSUserInterface
    case eCSSProperty_user_input:
    case eCSSProperty_user_modify:
    case eCSSProperty_user_select:
    case eCSSProperty_key_equivalent:
    case eCSSProperty_user_focus:
    case eCSSProperty_resizer:
    case eCSSProperty_cursor:
      CSS_CHECK(UserInterface) {
        switch (aProperty) {
          case eCSSProperty_user_input:       mUserInterface->mUserInput.Reset();      break;
          case eCSSProperty_user_modify:      mUserInterface->mUserModify.Reset();     break;
          case eCSSProperty_user_select:      mUserInterface->mUserSelect.Reset();     break;
          case eCSSProperty_key_equivalent: 
            CSS_CHECK_DATA(mUserInterface->mKeyEquivalent, nsCSSValueList) {
              mUserInterface->mKeyEquivalent->mValue.Reset();
              CSS_IF_DELETE(mUserInterface->mKeyEquivalent->mNext);
            }
            break;
          case eCSSProperty_user_focus:       mUserInterface->mUserFocus.Reset();      break;
          case eCSSProperty_resizer:          mUserInterface->mResizer.Reset();        break;
          case eCSSProperty_cursor:
            CSS_CHECK_DATA(mUserInterface->mCursor, nsCSSValueList) {
              mUserInterface->mCursor->mValue.Reset();
              CSS_IF_DELETE(mUserInterface->mCursor->mNext);
            }
            break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

#ifdef INCLUDE_XUL
    // nsCSSXUL
    case eCSSProperty_box_align:
    case eCSSProperty_box_direction:
    case eCSSProperty_box_flex:
    case eCSSProperty_box_orient:
    case eCSSProperty_box_pack:
    case eCSSProperty_box_ordinal_group:
      CSS_CHECK(XUL) {
        switch(aProperty) {
        case eCSSProperty_box_align:          mXUL->mBoxAlign.Reset();         break;
        case eCSSProperty_box_direction:      mXUL->mBoxDirection.Reset();     break;
        case eCSSProperty_box_flex:           mXUL->mBoxFlex.Reset();          break;
        case eCSSProperty_box_orient:         mXUL->mBoxOrient.Reset();        break;
        case eCSSProperty_box_pack:           mXUL->mBoxPack.Reset();          break;
        case eCSSProperty_box_ordinal_group:  mXUL->mBoxOrdinal.Reset();       break;
        CSS_BOGUS_DEFAULT; // Make compiler happy
        }
      }
      break;
#endif

      // nsCSSAural
    case eCSSProperty_azimuth:
    case eCSSProperty_elevation:
    case eCSSProperty_cue_after:
    case eCSSProperty_cue_before:
    case eCSSProperty_pause_after:
    case eCSSProperty_pause_before:
    case eCSSProperty_pitch:
    case eCSSProperty_pitch_range:
    case eCSSProperty_play_during:
    case eCSSProperty_play_during_flags:
    case eCSSProperty_richness:
    case eCSSProperty_speak:
    case eCSSProperty_speak_header:
    case eCSSProperty_speak_numeral:
    case eCSSProperty_speak_punctuation:
    case eCSSProperty_speech_rate:
    case eCSSProperty_stress:
    case eCSSProperty_voice_family:
    case eCSSProperty_volume:
      CSS_CHECK(Aural) {
        switch (aProperty) {
          case eCSSProperty_azimuth:            mAural->mAzimuth.Reset();          break;
          case eCSSProperty_elevation:          mAural->mElevation.Reset();        break;
          case eCSSProperty_cue_after:          mAural->mCueAfter.Reset();         break;
          case eCSSProperty_cue_before:         mAural->mCueBefore.Reset();        break;
          case eCSSProperty_pause_after:        mAural->mPauseAfter.Reset();       break;
          case eCSSProperty_pause_before:       mAural->mPauseBefore.Reset();      break;
          case eCSSProperty_pitch:              mAural->mPitch.Reset();            break;
          case eCSSProperty_pitch_range:        mAural->mPitchRange.Reset();       break;
          case eCSSProperty_play_during:        mAural->mPlayDuring.Reset();       break;
          case eCSSProperty_play_during_flags:  mAural->mPlayDuringFlags.Reset();  break;
          case eCSSProperty_richness:           mAural->mRichness.Reset();         break;
          case eCSSProperty_speak:              mAural->mSpeak.Reset();            break;
          case eCSSProperty_speak_header:       mAural->mSpeakHeader.Reset();      break;
          case eCSSProperty_speak_numeral:      mAural->mSpeakNumeral.Reset();     break;
          case eCSSProperty_speak_punctuation:  mAural->mSpeakPunctuation.Reset(); break;
          case eCSSProperty_speech_rate:        mAural->mSpeechRate.Reset();       break;
          case eCSSProperty_stress:             mAural->mStress.Reset();           break;
          case eCSSProperty_voice_family:       mAural->mVoiceFamily.Reset();      break;
          case eCSSProperty_volume:             mAural->mVolume.Reset();           break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

      // Shorthands
    case eCSSProperty_background:
      RemoveProperty(eCSSProperty_background_color);
      RemoveProperty(eCSSProperty_background_image);
      RemoveProperty(eCSSProperty_background_repeat);
      RemoveProperty(eCSSProperty_background_attachment);
      RemoveProperty(eCSSProperty_background_x_position);
      RemoveProperty(eCSSProperty_background_y_position);
      break;
    case eCSSProperty_border:
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(mMargin->mBorderWidth);
        CSS_IF_DELETE(mMargin->mBorderStyle);
        CSS_IF_DELETE(mMargin->mBorderColor);
      }
      break;
    case eCSSProperty_border_spacing:
      RemoveProperty(eCSSProperty_border_x_spacing);
      RemoveProperty(eCSSProperty_border_y_spacing);
      break;
    case eCSSProperty_clip:
      CSS_CHECK(Display) {
        CSS_IF_DELETE(mDisplay->mClip);
      }
      break;
    case eCSSProperty_cue:
      RemoveProperty(eCSSProperty_cue_after);
      RemoveProperty(eCSSProperty_cue_before);
      break;
    case eCSSProperty_font:
      RemoveProperty(eCSSProperty_font_family);
      RemoveProperty(eCSSProperty_font_style);
      RemoveProperty(eCSSProperty_font_variant);
      RemoveProperty(eCSSProperty_font_weight);
      RemoveProperty(eCSSProperty_font_size);
      RemoveProperty(eCSSProperty_line_height);
      break;
    case eCSSProperty_list_style:
      RemoveProperty(eCSSProperty_list_style_type);
      RemoveProperty(eCSSProperty_list_style_image);
      RemoveProperty(eCSSProperty_list_style_position);
      break;
    case eCSSProperty_margin:
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(mMargin->mMargin);
      }
      break;
    case eCSSProperty_outline:
      RemoveProperty(eCSSProperty_outline_color);
      RemoveProperty(eCSSProperty_outline_style);
      RemoveProperty(eCSSProperty_outline_width);
      break;
    case eCSSProperty_padding:
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(mMargin->mPadding);
      }
      break;
    case eCSSProperty_pause:
      RemoveProperty(eCSSProperty_pause_after);
      RemoveProperty(eCSSProperty_pause_before);
      break;
    case eCSSProperty_quotes:
      CSS_CHECK(Content) {
	      CSS_IF_DELETE(mContent->mQuotes);
      }
      break;
    case eCSSProperty_size:
      RemoveProperty(eCSSProperty_size_width);
      RemoveProperty(eCSSProperty_size_height);
      break;
    case eCSSProperty_text_shadow:
      CSS_CHECK(Text) {
	      CSS_IF_DELETE(mText->mTextShadow);
      }
      break;
    case eCSSProperty_background_position:
      RemoveProperty(eCSSProperty_background_x_position);
      RemoveProperty(eCSSProperty_background_y_position);
      break;
    case eCSSProperty_border_top:
      RemoveProperty(eCSSProperty_border_top_width);
      RemoveProperty(eCSSProperty_border_top_style);
      RemoveProperty(eCSSProperty_border_top_color);
      break;
    case eCSSProperty_border_right:
      RemoveProperty(eCSSProperty_border_right_width);
      RemoveProperty(eCSSProperty_border_right_style);
      RemoveProperty(eCSSProperty_border_right_color);
      break;
    case eCSSProperty_border_bottom:
      RemoveProperty(eCSSProperty_border_bottom_width);
      RemoveProperty(eCSSProperty_border_bottom_style);
      RemoveProperty(eCSSProperty_border_bottom_color);
      break;
    case eCSSProperty_border_left:
      RemoveProperty(eCSSProperty_border_left_width);
      RemoveProperty(eCSSProperty_border_left_style);
      RemoveProperty(eCSSProperty_border_left_color);
      break;
    case eCSSProperty_border_color:
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(mMargin->mBorderColor);
      }
      break;
    case eCSSProperty_border_style:
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(mMargin->mBorderStyle);
      }
      break;
    case eCSSProperty_border_width:
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(mMargin->mBorderWidth);
      }
      break;
    case eCSSProperty__moz_border_radius:
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(mMargin->mBorderRadius);
      }
      break;
    case eCSSProperty__moz_outline_radius:
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(mMargin->mOutlineRadius);
      }
      break;
//    default:  // XXX explicitly removing default case so compiler will help find missed props
    case eCSSProperty_UNKNOWN:
    case eCSSProperty_COUNT:
      result = NS_ERROR_ILLEGAL_VALUE;
      break;
  }

  if (NS_OK == result) {
    if (nsnull != mOrder) {
      PRInt32 index = mOrder->IndexOf((void*)aProperty);
      if (-1 != index) {
        mOrder->RemoveElementAt(index);
      }
    }
  }
  return result;
}


NS_IMETHODIMP
CSSDeclarationImpl::RemoveProperty(nsCSSProperty aProperty, nsCSSValue& aValue)
{
  nsresult result = NS_OK;

  PRBool  isImportant = PR_FALSE;
  GetValueIsImportant(aProperty, isImportant);
  if (isImportant) {
    result = mImportant->GetValue(aProperty, aValue);
    if (NS_SUCCEEDED(result)) {
      result = mImportant->RemoveProperty(aProperty);
    }
  } else {
    result = GetValue(aProperty, aValue);
    if (NS_SUCCEEDED(result)) {
      result = RemoveProperty(aProperty);
    }
  }
  return result;
}

NS_IMETHODIMP
CSSDeclarationImpl::AppendComment(const nsAReadableString& aComment)
{
  nsresult result = NS_ERROR_OUT_OF_MEMORY;

  if (nsnull == mOrder) {
    mOrder = new nsAutoVoidArray();
  }
  if (nsnull == mComments) {
    mComments = new nsStringArray();
  }
  if ((nsnull != mComments) && (nsnull != mOrder)) {
    mComments->AppendString(aComment);
    mOrder->AppendElement((void*)-mComments->Count());
    result = NS_OK;
  }
  return result;
}

NS_IMETHODIMP
CSSDeclarationImpl::GetValue(nsCSSProperty aProperty, nsCSSValue& aValue)
{
  nsresult result = NS_OK;

  switch (aProperty) {
    // nsCSSFont
    case eCSSProperty_font_family:
    case eCSSProperty_font_style:
    case eCSSProperty_font_variant:
    case eCSSProperty_font_weight:
    case eCSSProperty_font_size:
    case eCSSProperty_font_size_adjust:
    case eCSSProperty_font_stretch:
      if (nsnull != mFont) {
        switch (aProperty) {
          case eCSSProperty_font_family:      aValue = mFont->mFamily;      break;
          case eCSSProperty_font_style:       aValue = mFont->mStyle;       break;
          case eCSSProperty_font_variant:     aValue = mFont->mVariant;     break;
          case eCSSProperty_font_weight:      aValue = mFont->mWeight;      break;
          case eCSSProperty_font_size:        aValue = mFont->mSize;        break;
          case eCSSProperty_font_size_adjust: aValue = mFont->mSizeAdjust;  break;
          case eCSSProperty_font_stretch:     aValue = mFont->mStretch;     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSColor
    case eCSSProperty_color:
    case eCSSProperty_background_color:
    case eCSSProperty_background_image:
    case eCSSProperty_background_repeat:
    case eCSSProperty_background_attachment:
    case eCSSProperty_background_x_position:
    case eCSSProperty_background_y_position:
      if (nsnull != mColor) {
        switch (aProperty) {
          case eCSSProperty_color:                  aValue = mColor->mColor;           break;
          case eCSSProperty_background_color:       aValue = mColor->mBackColor;       break;
          case eCSSProperty_background_image:       aValue = mColor->mBackImage;       break;
          case eCSSProperty_background_repeat:      aValue = mColor->mBackRepeat;      break;
          case eCSSProperty_background_attachment:  aValue = mColor->mBackAttachment;  break;
          case eCSSProperty_background_x_position:  aValue = mColor->mBackPositionX;   break;
          case eCSSProperty_background_y_position:  aValue = mColor->mBackPositionY;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSText
    case eCSSProperty_word_spacing:
    case eCSSProperty_letter_spacing:
    case eCSSProperty_text_decoration:
    case eCSSProperty_vertical_align:
    case eCSSProperty_text_transform:
    case eCSSProperty_text_align:
    case eCSSProperty_text_indent:
    case eCSSProperty_unicode_bidi:
    case eCSSProperty_line_height:
    case eCSSProperty_white_space:
      if (nsnull != mText) {
        switch (aProperty) {
          case eCSSProperty_word_spacing:     aValue = mText->mWordSpacing;    break;
          case eCSSProperty_letter_spacing:   aValue = mText->mLetterSpacing;  break;
          case eCSSProperty_text_decoration:  aValue = mText->mDecoration;     break;
          case eCSSProperty_vertical_align:   aValue = mText->mVerticalAlign;  break;
          case eCSSProperty_text_transform:   aValue = mText->mTextTransform;  break;
          case eCSSProperty_text_align:       aValue = mText->mTextAlign;      break;
          case eCSSProperty_text_indent:      aValue = mText->mTextIndent;     break;
          case eCSSProperty_unicode_bidi:     aValue = mText->mUnicodeBidi;    break;
          case eCSSProperty_line_height:      aValue = mText->mLineHeight;     break;
          case eCSSProperty_white_space:      aValue = mText->mWhiteSpace;     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case eCSSProperty_text_shadow_color:
    case eCSSProperty_text_shadow_x:
    case eCSSProperty_text_shadow_y:
    case eCSSProperty_text_shadow_radius:
      if ((nsnull != mText) && (nsnull != mText->mTextShadow)) {
        switch (aProperty) {
          case eCSSProperty_text_shadow_color:  aValue = mText->mTextShadow->mColor;    break;
          case eCSSProperty_text_shadow_x:      aValue = mText->mTextShadow->mXOffset;  break;
          case eCSSProperty_text_shadow_y:      aValue = mText->mTextShadow->mYOffset;  break;
          case eCSSProperty_text_shadow_radius: aValue = mText->mTextShadow->mRadius;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;

      // nsCSSDisplay
    case eCSSProperty_float:
    case eCSSProperty_clear:
    case eCSSProperty_display:
    case eCSSProperty_binding:
    case eCSSProperty_position:
    case eCSSProperty_direction:
    case eCSSProperty_visibility:
    case eCSSProperty_opacity:
    case eCSSProperty_overflow:
      if (nsnull != mDisplay) {
        switch (aProperty) {
          case eCSSProperty_float:      aValue = mDisplay->mFloat;      break;
          case eCSSProperty_clear:      aValue = mDisplay->mClear;      break;
          case eCSSProperty_display:    aValue = mDisplay->mDisplay;    break;
          case eCSSProperty_binding:         
            aValue = mDisplay->mBinding;        
            break;
          case eCSSProperty_direction:  aValue = mDisplay->mDirection;  break;
          case eCSSProperty_position:   aValue = mDisplay->mPosition;   break;
          case eCSSProperty_visibility: aValue = mDisplay->mVisibility; break;
          case eCSSProperty_opacity:    aValue = mDisplay->mOpacity;    break;
          case eCSSProperty_overflow:   aValue = mDisplay->mOverflow;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case eCSSProperty_clip_top:
    case eCSSProperty_clip_right:
    case eCSSProperty_clip_bottom:
    case eCSSProperty_clip_left:
      if ((nsnull != mDisplay) && (nsnull != mDisplay->mClip)) {
        switch(aProperty) {
          case eCSSProperty_clip_top:     aValue = mDisplay->mClip->mTop;     break;
          case eCSSProperty_clip_right:   aValue = mDisplay->mClip->mRight;   break;
          case eCSSProperty_clip_bottom:  aValue = mDisplay->mClip->mBottom;  break;
          case eCSSProperty_clip_left:    aValue = mDisplay->mClip->mLeft;    break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSMargin
    case eCSSProperty_margin_top:
    case eCSSProperty_margin_right:
    case eCSSProperty_margin_bottom:
    case eCSSProperty_margin_left:
      if ((nsnull != mMargin) && (nsnull != mMargin->mMargin)) {
        switch (aProperty) {
          case eCSSProperty_margin_top:     aValue = mMargin->mMargin->mTop;     break;
          case eCSSProperty_margin_right:   aValue = mMargin->mMargin->mRight;   break;
          case eCSSProperty_margin_bottom:  aValue = mMargin->mMargin->mBottom;  break;
          case eCSSProperty_margin_left:    aValue = mMargin->mMargin->mLeft;    break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case eCSSProperty_padding_top:
    case eCSSProperty_padding_right:
    case eCSSProperty_padding_bottom:
    case eCSSProperty_padding_left:
      if ((nsnull != mMargin) && (nsnull != mMargin->mPadding)) {
        switch (aProperty) {
          case eCSSProperty_padding_top:    aValue = mMargin->mPadding->mTop;    break;
          case eCSSProperty_padding_right:  aValue = mMargin->mPadding->mRight;  break;
          case eCSSProperty_padding_bottom: aValue = mMargin->mPadding->mBottom; break;
          case eCSSProperty_padding_left:   aValue = mMargin->mPadding->mLeft;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case eCSSProperty_border_top_width:
    case eCSSProperty_border_right_width:
    case eCSSProperty_border_bottom_width:
    case eCSSProperty_border_left_width:
      if ((nsnull != mMargin) && (nsnull != mMargin->mBorderWidth)) {
        switch (aProperty) {
          case eCSSProperty_border_top_width:     aValue = mMargin->mBorderWidth->mTop;     break;
          case eCSSProperty_border_right_width:   aValue = mMargin->mBorderWidth->mRight;   break;
          case eCSSProperty_border_bottom_width:  aValue = mMargin->mBorderWidth->mBottom;  break;
          case eCSSProperty_border_left_width:    aValue = mMargin->mBorderWidth->mLeft;    break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case eCSSProperty_border_top_color:
    case eCSSProperty_border_right_color:
    case eCSSProperty_border_bottom_color:
    case eCSSProperty_border_left_color:
      if ((nsnull != mMargin) && (nsnull != mMargin->mBorderColor)) {
        switch (aProperty) {
          case eCSSProperty_border_top_color:     aValue = mMargin->mBorderColor->mTop;    break;
          case eCSSProperty_border_right_color:   aValue = mMargin->mBorderColor->mRight;  break;
          case eCSSProperty_border_bottom_color:  aValue = mMargin->mBorderColor->mBottom; break;
          case eCSSProperty_border_left_color:    aValue = mMargin->mBorderColor->mLeft;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case eCSSProperty_border_top_style:
    case eCSSProperty_border_right_style:
    case eCSSProperty_border_bottom_style:
    case eCSSProperty_border_left_style:
      if ((nsnull != mMargin) && (nsnull != mMargin->mBorderStyle)) {
        switch (aProperty) {
          case eCSSProperty_border_top_style:     aValue = mMargin->mBorderStyle->mTop;    break;
          case eCSSProperty_border_right_style:   aValue = mMargin->mBorderStyle->mRight;  break;
          case eCSSProperty_border_bottom_style:  aValue = mMargin->mBorderStyle->mBottom; break;
          case eCSSProperty_border_left_style:    aValue = mMargin->mBorderStyle->mLeft;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case eCSSProperty__moz_border_radius_topLeft:
    case eCSSProperty__moz_border_radius_topRight:
    case eCSSProperty__moz_border_radius_bottomRight:
    case eCSSProperty__moz_border_radius_bottomLeft:
      if ((nsnull != mMargin) && (nsnull != mMargin->mBorderRadius)) {
        switch (aProperty) {
          case eCSSProperty__moz_border_radius_topLeft:			aValue = mMargin->mBorderRadius->mTop;    break;
          case eCSSProperty__moz_border_radius_topRight:		aValue = mMargin->mBorderRadius->mRight;  break;
          case eCSSProperty__moz_border_radius_bottomRight:	aValue = mMargin->mBorderRadius->mBottom; break;
          case eCSSProperty__moz_border_radius_bottomLeft:	aValue = mMargin->mBorderRadius->mLeft;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case eCSSProperty__moz_outline_radius_topLeft:
    case eCSSProperty__moz_outline_radius_topRight:
    case eCSSProperty__moz_outline_radius_bottomRight:
    case eCSSProperty__moz_outline_radius_bottomLeft:
      if ((nsnull != mMargin) && (nsnull != mMargin->mOutlineRadius)) {
        switch (aProperty) {
          case eCSSProperty__moz_outline_radius_topLeft:			aValue = mMargin->mOutlineRadius->mTop;    break;
          case eCSSProperty__moz_outline_radius_topRight:			aValue = mMargin->mOutlineRadius->mRight;  break;
          case eCSSProperty__moz_outline_radius_bottomRight:	aValue = mMargin->mOutlineRadius->mBottom; break;
          case eCSSProperty__moz_outline_radius_bottomLeft:		aValue = mMargin->mOutlineRadius->mLeft;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case eCSSProperty_outline_width:
    case eCSSProperty_outline_color:
    case eCSSProperty_outline_style:
    case eCSSProperty_float_edge:
      if (nsnull != mMargin) {
        switch (aProperty) {
          case eCSSProperty_outline_width:      aValue = mMargin->mOutlineWidth; break;
          case eCSSProperty_outline_color:      aValue = mMargin->mOutlineColor; break;
          case eCSSProperty_outline_style:      aValue = mMargin->mOutlineStyle; break;
          case eCSSProperty_float_edge:         aValue = mMargin->mFloatEdge;    break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSPosition
    case eCSSProperty_width:
    case eCSSProperty_min_width:
    case eCSSProperty_max_width:
    case eCSSProperty_height:
    case eCSSProperty_min_height:
    case eCSSProperty_max_height:
    case eCSSProperty_box_sizing:
    case eCSSProperty_z_index:
      if (nsnull != mPosition) {
        switch (aProperty) {
          case eCSSProperty_width:      aValue = mPosition->mWidth;      break;
          case eCSSProperty_min_width:  aValue = mPosition->mMinWidth;   break;
          case eCSSProperty_max_width:  aValue = mPosition->mMaxWidth;   break;
          case eCSSProperty_height:     aValue = mPosition->mHeight;     break;
          case eCSSProperty_min_height: aValue = mPosition->mMinHeight;  break;
          case eCSSProperty_max_height: aValue = mPosition->mMaxHeight;  break;
          case eCSSProperty_box_sizing: aValue = mPosition->mBoxSizing;  break;
          case eCSSProperty_z_index:    aValue = mPosition->mZIndex;     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case eCSSProperty_top:
    case eCSSProperty_right:
    case eCSSProperty_bottom:
    case eCSSProperty_left:
      if ((nsnull != mPosition) && (nsnull != mPosition->mOffset)) {
        switch (aProperty) {
          case eCSSProperty_top:    aValue = mPosition->mOffset->mTop;    break;
          case eCSSProperty_right:  aValue = mPosition->mOffset->mRight;  break;
          case eCSSProperty_bottom: aValue = mPosition->mOffset->mBottom; break;
          case eCSSProperty_left:   aValue = mPosition->mOffset->mLeft;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

      // nsCSSList
    case eCSSProperty_list_style_type:
    case eCSSProperty_list_style_image:
    case eCSSProperty_list_style_position:
      if (nsnull != mList) {
        switch (aProperty) {
          case eCSSProperty_list_style_type:      aValue = mList->mType;     break;
          case eCSSProperty_list_style_image:     aValue = mList->mImage;    break;
          case eCSSProperty_list_style_position:  aValue = mList->mPosition; break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

      // nsCSSTable
    case eCSSProperty_border_collapse:
    case eCSSProperty_border_x_spacing:
    case eCSSProperty_border_y_spacing:
    case eCSSProperty_caption_side:
    case eCSSProperty_empty_cells:
    case eCSSProperty_table_layout:
      if (nsnull != mTable) {
        switch (aProperty) {
          case eCSSProperty_border_collapse:  aValue = mTable->mBorderCollapse; break;
          case eCSSProperty_border_x_spacing: aValue = mTable->mBorderSpacingX; break;
          case eCSSProperty_border_y_spacing: aValue = mTable->mBorderSpacingY; break;
          case eCSSProperty_caption_side:     aValue = mTable->mCaptionSide;    break;
          case eCSSProperty_empty_cells:      aValue = mTable->mEmptyCells;     break;
          case eCSSProperty_table_layout:     aValue = mTable->mLayout;         break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

      // nsCSSBreaks
    case eCSSProperty_orphans:
    case eCSSProperty_widows:
    case eCSSProperty_page:
    case eCSSProperty_page_break_after:
    case eCSSProperty_page_break_before:
    case eCSSProperty_page_break_inside:
      if (nsnull != mBreaks) {
        switch (aProperty) {
          case eCSSProperty_orphans:            aValue = mBreaks->mOrphans;         break;
          case eCSSProperty_widows:             aValue = mBreaks->mWidows;          break;
          case eCSSProperty_page:               aValue = mBreaks->mPage;            break;
          case eCSSProperty_page_break_after:   aValue = mBreaks->mPageBreakAfter;  break;
          case eCSSProperty_page_break_before:  aValue = mBreaks->mPageBreakBefore; break;
          case eCSSProperty_page_break_inside:  aValue = mBreaks->mPageBreakInside; break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

      // nsCSSPage
    case eCSSProperty_marks:
    case eCSSProperty_size_width:
    case eCSSProperty_size_height:
      if (nsnull != mPage) {
        switch (aProperty) {
          case eCSSProperty_marks:        aValue = mPage->mMarks;       break;
          case eCSSProperty_size_width:   aValue = mPage->mSizeWidth;   break;
          case eCSSProperty_size_height:  aValue = mPage->mSizeHeight;  break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

      // nsCSSContent
    case eCSSProperty_content:
    case eCSSProperty_counter_increment:
    case eCSSProperty_counter_reset:
    case eCSSProperty_marker_offset:
    case eCSSProperty_quotes_open:
    case eCSSProperty_quotes_close:
      if (nsnull != mContent) {
        switch (aProperty) {
          case eCSSProperty_content:
            if (nsnull != mContent->mContent) {
              aValue = mContent->mContent->mValue;
            }
            break;
          case eCSSProperty_counter_increment:  
            if (nsnull != mContent->mCounterIncrement) {
              aValue = mContent->mCounterIncrement->mCounter;
            }
            break;
          case eCSSProperty_counter_reset:
            if (nsnull != mContent->mCounterReset) {
              aValue = mContent->mCounterReset->mCounter;
            }
            break;
          case eCSSProperty_marker_offset:      aValue = mContent->mMarkerOffset;     break;
          case eCSSProperty_quotes_open:
            if (nsnull != mContent->mQuotes) {
              aValue = mContent->mQuotes->mOpen;
            }
            break;
          case eCSSProperty_quotes_close:
            if (nsnull != mContent->mQuotes) {
              aValue = mContent->mQuotes->mClose;
            }
            break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSUserInterface
    case eCSSProperty_user_input:
    case eCSSProperty_user_modify:
    case eCSSProperty_user_select:
    case eCSSProperty_key_equivalent:
    case eCSSProperty_user_focus:
    case eCSSProperty_resizer:
    case eCSSProperty_cursor:
      if (nsnull != mUserInterface) {
        switch (aProperty) {
          case eCSSProperty_user_input:       aValue = mUserInterface->mUserInput;       break;
          case eCSSProperty_user_modify:      aValue = mUserInterface->mUserModify;      break;
          case eCSSProperty_user_select:      aValue = mUserInterface->mUserSelect;      break;
          case eCSSProperty_key_equivalent:
            if (nsnull != mUserInterface->mKeyEquivalent) {
              aValue = mUserInterface->mKeyEquivalent->mValue;
            }
            break;
          case eCSSProperty_user_focus:       aValue = mUserInterface->mUserFocus;       break;
          case eCSSProperty_resizer:          aValue = mUserInterface->mResizer;         break;
          case eCSSProperty_cursor:
            if (nsnull != mUserInterface->mCursor) {
              aValue = mUserInterface->mCursor->mValue;
            }
            break;
          
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;

#ifdef INCLUDE_XUL
    // nsCSSXUL
    case eCSSProperty_box_align:
    case eCSSProperty_box_direction:
    case eCSSProperty_box_flex:
    case eCSSProperty_box_orient:
    case eCSSProperty_box_pack:
    case eCSSProperty_box_ordinal_group:
      if (nsnull != mXUL) {
        switch (aProperty) {
          case eCSSProperty_box_align:          aValue = mXUL->mBoxAlign;      break;
          case eCSSProperty_box_direction:      aValue = mXUL->mBoxDirection;  break;
          case eCSSProperty_box_flex:           aValue = mXUL->mBoxFlex;       break;
          case eCSSProperty_box_orient:         aValue = mXUL->mBoxOrient;     break;
          case eCSSProperty_box_pack:           aValue = mXUL->mBoxPack;       break;
          case eCSSProperty_box_ordinal_group:  aValue = mXUL->mBoxOrdinal;    break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
#endif

      // nsCSSAural
    case eCSSProperty_azimuth:
    case eCSSProperty_elevation:
    case eCSSProperty_cue_after:
    case eCSSProperty_cue_before:
    case eCSSProperty_pause_after:
    case eCSSProperty_pause_before:
    case eCSSProperty_pitch:
    case eCSSProperty_pitch_range:
    case eCSSProperty_play_during:
    case eCSSProperty_play_during_flags:
    case eCSSProperty_richness:
    case eCSSProperty_speak:
    case eCSSProperty_speak_header:
    case eCSSProperty_speak_numeral:
    case eCSSProperty_speak_punctuation:
    case eCSSProperty_speech_rate:
    case eCSSProperty_stress:
    case eCSSProperty_voice_family:
    case eCSSProperty_volume:
      if (nsnull != mAural) {
        switch (aProperty) {
          case eCSSProperty_azimuth:            aValue = mAural->mAzimuth;          break;
          case eCSSProperty_elevation:          aValue = mAural->mElevation;        break;
          case eCSSProperty_cue_after:          aValue = mAural->mCueAfter;         break;
          case eCSSProperty_cue_before:         aValue = mAural->mCueBefore;        break;
          case eCSSProperty_pause_after:        aValue = mAural->mPauseAfter;       break;
          case eCSSProperty_pause_before:       aValue = mAural->mPauseBefore;      break;
          case eCSSProperty_pitch:              aValue = mAural->mPitch;            break;
          case eCSSProperty_pitch_range:        aValue = mAural->mPitchRange;       break;
          case eCSSProperty_play_during:        aValue = mAural->mPlayDuring;       break;
          case eCSSProperty_play_during_flags:  aValue = mAural->mPlayDuringFlags;  break;
          case eCSSProperty_richness:           aValue = mAural->mRichness;         break;
          case eCSSProperty_speak:              aValue = mAural->mSpeak;            break;
          case eCSSProperty_speak_header:       aValue = mAural->mSpeakHeader;      break;
          case eCSSProperty_speak_numeral:      aValue = mAural->mSpeakNumeral;     break;
          case eCSSProperty_speak_punctuation:  aValue = mAural->mSpeakPunctuation; break;
          case eCSSProperty_speech_rate:        aValue = mAural->mSpeechRate;       break;
          case eCSSProperty_stress:             aValue = mAural->mStress;           break;
          case eCSSProperty_voice_family:       aValue = mAural->mVoiceFamily;      break;
          case eCSSProperty_volume:             aValue = mAural->mVolume;           break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;


      // Shorthands
    case eCSSProperty_background:
    case eCSSProperty_border:
    case eCSSProperty_border_spacing:
    case eCSSProperty_clip:
    case eCSSProperty_cue:
    case eCSSProperty_font:
    case eCSSProperty_list_style:
    case eCSSProperty_margin:
    case eCSSProperty_outline:
    case eCSSProperty_padding:
    case eCSSProperty_pause:
    case eCSSProperty_quotes:
    case eCSSProperty_size:
    case eCSSProperty_text_shadow:
    case eCSSProperty_background_position:
    case eCSSProperty_border_top:
    case eCSSProperty_border_right:
    case eCSSProperty_border_bottom:
    case eCSSProperty_border_left:
    case eCSSProperty_border_color:
    case eCSSProperty_border_style:
    case eCSSProperty_border_width:
    case eCSSProperty__moz_border_radius:
      NS_ERROR("can't query for shorthand properties");
    default:
      result = NS_ERROR_ILLEGAL_VALUE;
      break;
  }
  return result;
}


NS_IMETHODIMP
CSSDeclarationImpl::GetValue(const nsAReadableString& aProperty,
                             nsAWritableString& aValue)
{
  nsCSSProperty propID = nsCSSProps::LookupProperty(aProperty);
  return GetValue(propID, aValue);
}

PRBool CSSDeclarationImpl::AppendValueToString(nsCSSProperty aProperty, nsAWritableString& aResult)
{
  nsCSSValue  value;
  GetValue(aProperty, value);
  return AppendValueToString(aProperty, value, aResult);
}

PRBool CSSDeclarationImpl::AppendValueToString(nsCSSProperty aProperty, const nsCSSValue& aValue, nsAWritableString& aResult)
{
  nsCSSUnit unit = aValue.GetUnit();

  if (eCSSUnit_Null == unit) {
    return PR_FALSE;
  }

  if ((eCSSUnit_String <= unit) && (unit <= eCSSUnit_Counters)) {
    switch (unit) {
      case eCSSUnit_URL:      aResult.Append(NS_LITERAL_STRING("url("));
        break;
      case eCSSUnit_Attr:     aResult.Append(NS_LITERAL_STRING("attr("));
        break;
      case eCSSUnit_Counter:  aResult.Append(NS_LITERAL_STRING("counter("));
        break;
      case eCSSUnit_Counters: aResult.Append(NS_LITERAL_STRING("counters("));
        break;
      default:  break;
    }
    nsAutoString  buffer;
    aValue.GetStringValue(buffer);
    aResult.Append(buffer);
  }
  else if (eCSSUnit_Integer == unit) {
    switch (aProperty) {
      case eCSSProperty_color:
      case eCSSProperty_background_color: {
        // we can lookup the property in the ColorTable and then
        // get a string mapping the name
        nsAutoString tmpStr;
        nsCAutoString str;
        if (nsCSSProps::GetColorName(aValue.GetIntValue(), str)){
          aResult.Append(NS_ConvertASCIItoUCS2(str));
        } else {
          tmpStr.AppendInt(aValue.GetIntValue(), 10);
          aResult.Append(tmpStr);
        }
      }
      break;

      default:
        {
          nsAutoString tmpStr;
          tmpStr.AppendInt(aValue.GetIntValue(), 10);
          aResult.Append(tmpStr);
        }
    }
  }
  else if (eCSSUnit_Enumerated == unit) {
    if (eCSSProperty_text_decoration == aProperty) {
      PRInt32 intValue = aValue.GetIntValue();
      if (NS_STYLE_TEXT_DECORATION_NONE != intValue) {
        PRInt32 mask;
        for (mask = NS_STYLE_TEXT_DECORATION_UNDERLINE; 
             mask <= NS_STYLE_TEXT_DECORATION_BLINK; 
             mask <<= 1) {
          if ((mask & intValue) == mask) {
            aResult.Append(NS_ConvertASCIItoUCS2(nsCSSProps::LookupPropertyValue(aProperty, mask)));
            intValue &= ~mask;
            if (0 != intValue) { // more left
              aResult.Append(PRUnichar(' '));
            }
          }
        }
      }
      else {
        aResult.Append(NS_ConvertASCIItoUCS2(nsCSSProps::LookupPropertyValue(aProperty, NS_STYLE_TEXT_DECORATION_NONE)));
      }
    }
    else if (eCSSProperty_azimuth == aProperty) {
      PRInt32 intValue = aValue.GetIntValue();
      aResult.Append(NS_ConvertASCIItoUCS2(nsCSSProps::LookupPropertyValue(aProperty, (intValue & ~NS_STYLE_AZIMUTH_BEHIND))));
      if ((NS_STYLE_AZIMUTH_BEHIND & intValue) != 0) {
        aResult.Append(PRUnichar(' '));
        aResult.Append(NS_ConvertASCIItoUCS2(nsCSSProps::LookupPropertyValue(aProperty, NS_STYLE_AZIMUTH_BEHIND)));
      }
    }
    else if (eCSSProperty_play_during_flags == aProperty) {
      PRInt32 intValue = aValue.GetIntValue();
      if ((NS_STYLE_PLAY_DURING_MIX & intValue) != 0) {
        aResult.Append(NS_ConvertASCIItoUCS2(nsCSSProps::LookupPropertyValue(aProperty, NS_STYLE_PLAY_DURING_MIX)));
      }
      if ((NS_STYLE_PLAY_DURING_REPEAT & intValue) != 0) {
        if (NS_STYLE_PLAY_DURING_REPEAT != intValue) {
          aResult.Append(PRUnichar(' '));
        }
        aResult.Append(NS_ConvertASCIItoUCS2(nsCSSProps::LookupPropertyValue(aProperty, NS_STYLE_PLAY_DURING_REPEAT)));
      }
    }
    else if (eCSSProperty_marks == aProperty) {
      PRInt32 intValue = aValue.GetIntValue();
      if ((NS_STYLE_PAGE_MARKS_CROP & intValue) != 0) {
        aResult.Append(NS_ConvertASCIItoUCS2(nsCSSProps::LookupPropertyValue(aProperty, NS_STYLE_PAGE_MARKS_CROP)));
      }
      if ((NS_STYLE_PAGE_MARKS_REGISTER & intValue) != 0) {
        if ((NS_STYLE_PAGE_MARKS_CROP & intValue) != 0) {
          aResult.Append(PRUnichar(' '));
        }
        aResult.Append(NS_ConvertASCIItoUCS2(nsCSSProps::LookupPropertyValue(aProperty, NS_STYLE_PAGE_MARKS_REGISTER)));
      }
    }
    else {
      const nsCString& name = nsCSSProps::LookupPropertyValue(aProperty, aValue.GetIntValue());
      aResult.Append(NS_ConvertASCIItoUCS2(name));
    }
  }
  else if (eCSSUnit_Color == unit){
    nsAutoString tmpStr;
    nscolor color = aValue.GetColorValue();

    aResult.Append(NS_LITERAL_STRING("rgb("));

    tmpStr.AppendInt(NS_GET_R(color), 10);
    aResult.Append(tmpStr);

    aResult.Append(PRUnichar(','));

    tmpStr.Truncate();
    tmpStr.AppendInt(NS_GET_G(color), 10);
    aResult.Append(tmpStr);

    aResult.Append(PRUnichar(','));

    tmpStr.Truncate();
    tmpStr.AppendInt(NS_GET_B(color), 10);
    aResult.Append(tmpStr);

    aResult.Append(PRUnichar(')'));
  }
  else if (eCSSUnit_Percent == unit) {
    nsAutoString tmpStr;
    tmpStr.AppendFloat(aValue.GetPercentValue() * 100.0f);
    aResult.Append(tmpStr);
  }
  else if (eCSSUnit_Percent < unit) {  // length unit
    nsAutoString tmpStr;
    tmpStr.AppendFloat(aValue.GetFloatValue());
    aResult.Append(tmpStr);
  }

  switch (unit) {
    case eCSSUnit_Null:         break;
    case eCSSUnit_Auto:         aResult.Append(NS_LITERAL_STRING("auto"));     break;
    case eCSSUnit_Inherit:      aResult.Append(NS_LITERAL_STRING("inherit"));  break;
    case eCSSUnit_Initial:      aResult.Append(NS_LITERAL_STRING("initial"));  break;
    case eCSSUnit_None:         aResult.Append(NS_LITERAL_STRING("none"));     break;
    case eCSSUnit_Normal:       aResult.Append(NS_LITERAL_STRING("normal"));   break;

    case eCSSUnit_String:       break;
    case eCSSUnit_URL:
    case eCSSUnit_Attr:
    case eCSSUnit_Counter:
    case eCSSUnit_Counters:     aResult.Append(PRUnichar(')'));    break;
    case eCSSUnit_Integer:      break;
    case eCSSUnit_Enumerated:   break;
    case eCSSUnit_Color:        break;
    case eCSSUnit_Percent:      aResult.Append(PRUnichar('%'));    break;
    case eCSSUnit_Number:       break;

    case eCSSUnit_Inch:         aResult.Append(NS_LITERAL_STRING("in"));   break;
    case eCSSUnit_Foot:         aResult.Append(NS_LITERAL_STRING("ft"));   break;
    case eCSSUnit_Mile:         aResult.Append(NS_LITERAL_STRING("mi"));   break;
    case eCSSUnit_Millimeter:   aResult.Append(NS_LITERAL_STRING("mm"));   break;
    case eCSSUnit_Centimeter:   aResult.Append(NS_LITERAL_STRING("cm"));   break;
    case eCSSUnit_Meter:        aResult.Append(NS_LITERAL_STRING("m"));    break;
    case eCSSUnit_Kilometer:    aResult.Append(NS_LITERAL_STRING("km"));   break;
    case eCSSUnit_Point:        aResult.Append(NS_LITERAL_STRING("pt"));   break;
    case eCSSUnit_Pica:         aResult.Append(NS_LITERAL_STRING("pc"));   break;
    case eCSSUnit_Didot:        aResult.Append(NS_LITERAL_STRING("dt"));   break;
    case eCSSUnit_Cicero:       aResult.Append(NS_LITERAL_STRING("cc"));   break;

    case eCSSUnit_EM:           aResult.Append(NS_LITERAL_STRING("em"));   break;
    case eCSSUnit_EN:           aResult.Append(NS_LITERAL_STRING("en"));   break;
    case eCSSUnit_XHeight:      aResult.Append(NS_LITERAL_STRING("ex"));   break;
    case eCSSUnit_CapHeight:    aResult.Append(NS_LITERAL_STRING("cap"));  break;
    case eCSSUnit_Char:         aResult.Append(NS_LITERAL_STRING("ch"));   break;

    case eCSSUnit_Pixel:        aResult.Append(NS_LITERAL_STRING("px"));   break;

    case eCSSUnit_Proportional: aResult.Append(NS_LITERAL_STRING("*"));   break;

    case eCSSUnit_Degree:       aResult.Append(NS_LITERAL_STRING("deg"));  break;
    case eCSSUnit_Grad:         aResult.Append(NS_LITERAL_STRING("grad")); break;
    case eCSSUnit_Radian:       aResult.Append(NS_LITERAL_STRING("rad"));  break;

    case eCSSUnit_Hertz:        aResult.Append(NS_LITERAL_STRING("Hz"));   break;
    case eCSSUnit_Kilohertz:    aResult.Append(NS_LITERAL_STRING("kHz"));  break;

    case eCSSUnit_Seconds:      aResult.Append(PRUnichar('s'));    break;
    case eCSSUnit_Milliseconds: aResult.Append(NS_LITERAL_STRING("ms"));   break;
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
CSSDeclarationImpl::GetValue(nsCSSProperty aProperty,
                             nsAWritableString& aValue)
{
  PRBool  isImportant = PR_FALSE;
  GetValueIsImportant(aProperty, isImportant);
  if (PR_TRUE == isImportant) {
    return mImportant->GetValue(aProperty, aValue);
  }

  aValue.Truncate(0);

  // shorthands
  switch (aProperty) {
    case eCSSProperty_background:
      if (AppendValueToString(eCSSProperty_background_color, aValue)) aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty_background_image, aValue)) aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty_background_repeat, aValue)) aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty_background_attachment, aValue)) aValue.Append(PRUnichar(' '));
      if (HAS_VALUE(mColor,mBackPositionX) && HAS_VALUE(mColor,mBackPositionY)) {
        AppendValueToString(eCSSProperty_background_x_position, aValue);
        aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_background_y_position, aValue);
      }
      break;
    case eCSSProperty_border:
      if (AppendValueToString(eCSSProperty_border_top_width, aValue)) aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty_border_top_style, aValue)) aValue.Append(PRUnichar(' '));
      AppendValueToString(eCSSProperty_border_top_color, aValue);
      break;
    case eCSSProperty_border_spacing:
      if (AppendValueToString(eCSSProperty_border_x_spacing, aValue)) {
        aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_y_spacing, aValue);
      }
      break;
    case eCSSProperty_clip:
      if (HAS_RECT(mDisplay,mClip)) {
        aValue.Append(NS_LITERAL_STRING("rect("));
        AppendValueToString(eCSSProperty_clip_top, aValue);     aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_clip_right, aValue);   aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_clip_bottom, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_clip_left, aValue);
        aValue.Append(PRUnichar(')'));
      }
      break;
    case eCSSProperty_cue:
      if (HAS_VALUE(mAural,mCueAfter) && HAS_VALUE(mAural,mCueBefore)) {
        AppendValueToString(eCSSProperty_cue_after, aValue);
        aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_cue_before, aValue);
      }
      break;
    case eCSSProperty_cursor:
      if ((nsnull != mUserInterface) && (nsnull != mUserInterface->mCursor)) {
        nsCSSValueList* cursor = mUserInterface->mCursor;
        do {
          AppendValueToString(eCSSProperty_cursor, cursor->mValue, aValue);
          cursor = cursor->mNext;
          if (nsnull != cursor) {
            aValue.Append(PRUnichar(' '));
          }
        } while (nsnull != cursor);
      }
      break;
    case eCSSProperty_font:
      if (HAS_VALUE(mFont,mSize)) {
        if (AppendValueToString(eCSSProperty_font_style, aValue)) aValue.Append(PRUnichar(' '));
        if (AppendValueToString(eCSSProperty_font_variant, aValue)) aValue.Append(PRUnichar(' '));
        if (AppendValueToString(eCSSProperty_font_weight, aValue)) aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_font_size, aValue);
        if (HAS_VALUE(mText,mLineHeight)) {
          aValue.Append(PRUnichar('/'));
          AppendValueToString(eCSSProperty_line_height, aValue);
        }
        aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_font_family, aValue);
      }
      break;
    case eCSSProperty_list_style:
      if (AppendValueToString(eCSSProperty_list_style_type, aValue)) aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty_list_style_position, aValue)) aValue.Append(PRUnichar(' '));
      AppendValueToString(eCSSProperty_list_style_image, aValue);
      break;
    case eCSSProperty_margin:
      if (HAS_RECT(mMargin,mMargin)) {
        AppendValueToString(eCSSProperty_margin_top, aValue);     aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_margin_right, aValue);   aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_margin_bottom, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_margin_left, aValue);
      }
      break;
    case eCSSProperty_outline:
      if (AppendValueToString(eCSSProperty_outline_color, aValue)) aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty_outline_style, aValue)) aValue.Append(PRUnichar(' '));
      AppendValueToString(eCSSProperty_outline_width, aValue);
      break;
    case eCSSProperty_padding:
      if (HAS_RECT(mMargin,mPadding)) {
        AppendValueToString(eCSSProperty_padding_top, aValue);    aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_padding_right, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_padding_bottom, aValue); aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_padding_left, aValue);
      }
      break;
    case eCSSProperty_pause:
      if (HAS_VALUE(mAural,mPauseAfter) && HAS_VALUE(mAural,mPauseBefore)) {
        AppendValueToString(eCSSProperty_pause_after, aValue);
        aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_pause_before, aValue);
      }
      break;
    case eCSSProperty_size:
      if (HAS_VALUE(mPage,mSizeWidth) && HAS_VALUE(mPage,mSizeHeight)) {
        AppendValueToString(eCSSProperty_size_width, aValue);
        aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_size_height, aValue);
      }
      break;
    case eCSSProperty_text_shadow:
      if ((nsnull != mText) && (nsnull != mText->mTextShadow)) {
        if (mText->mTextShadow->mXOffset.IsLengthUnit()) {
          nsCSSShadow*  shadow = mText->mTextShadow;
          while (nsnull != shadow) {
            if (AppendValueToString(eCSSProperty_text_shadow_color, shadow->mColor, aValue)) aValue.Append(PRUnichar(' '));
            if (AppendValueToString(eCSSProperty_text_shadow_x, shadow->mXOffset, aValue)) {
              aValue.Append(PRUnichar(' '));
              AppendValueToString(eCSSProperty_text_shadow_y, shadow->mYOffset, aValue);
              aValue.Append(PRUnichar(' '));
            }
            if (AppendValueToString(eCSSProperty_text_shadow_radius, shadow->mRadius, aValue)) aValue.Append(PRUnichar(' '));
            shadow = shadow->mNext;
          }
        }
        else {  // none or inherit
          AppendValueToString(eCSSProperty_text_shadow_x, aValue);
        }
      }
      break;
    case eCSSProperty_background_position:
      if (HAS_VALUE(mColor,mBackPositionX) && HAS_VALUE(mColor,mBackPositionY)) {
        AppendValueToString(eCSSProperty_background_x_position, aValue);
        aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_background_y_position, aValue);
      }
      break;
    case eCSSProperty_border_top:
      if (AppendValueToString(eCSSProperty_border_top_width, aValue)) aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty_border_top_style, aValue)) aValue.Append(PRUnichar(' '));
      AppendValueToString(eCSSProperty_border_top_color, aValue);
      break;
    case eCSSProperty_border_right:
      if (AppendValueToString(eCSSProperty_border_right_width, aValue)) aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty_border_right_style, aValue)) aValue.Append(PRUnichar(' '));
      AppendValueToString(eCSSProperty_border_right_color, aValue);
      break;
    case eCSSProperty_border_bottom:
      if (AppendValueToString(eCSSProperty_border_bottom_width, aValue)) aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty_border_bottom_style, aValue)) aValue.Append(PRUnichar(' '));
      AppendValueToString(eCSSProperty_border_bottom_color, aValue);
      break;
    case eCSSProperty_border_left:
      if (AppendValueToString(eCSSProperty_border_left_width, aValue)) aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty_border_left_style, aValue)) aValue.Append(PRUnichar(' '));
      AppendValueToString(eCSSProperty_border_left_color, aValue);
      break;
    case eCSSProperty_border_color:
      if (HAS_RECT(mMargin,mBorderColor)) {
        AppendValueToString(eCSSProperty_border_top_color, aValue);     aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_right_color, aValue);   aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_bottom_color, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_left_color, aValue);
      }
      break;
    case eCSSProperty_border_style:
      if (HAS_RECT(mMargin,mBorderStyle)) {
        AppendValueToString(eCSSProperty_border_top_style, aValue);     aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_right_style, aValue);   aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_bottom_style, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_left_style, aValue);
      }
      break;
    case eCSSProperty__moz_border_radius:
      if (HAS_RECT(mMargin,mBorderRadius)) {
        AppendValueToString(eCSSProperty__moz_border_radius_topLeft, aValue);     aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty__moz_border_radius_topRight, aValue);   aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty__moz_border_radius_bottomRight, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty__moz_border_radius_bottomLeft, aValue);
      }
    	break;
    case eCSSProperty__moz_outline_radius:
      if (HAS_RECT(mMargin,mOutlineRadius)) {
        AppendValueToString(eCSSProperty__moz_outline_radius_topLeft, aValue);     aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty__moz_outline_radius_topRight, aValue);   aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty__moz_outline_radius_bottomRight, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty__moz_outline_radius_bottomLeft, aValue);
      }
    	break;
    case eCSSProperty_border_width:
      if (HAS_RECT(mMargin,mBorderWidth)) {
        AppendValueToString(eCSSProperty_border_top_width, aValue);     aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_right_width, aValue);   aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_bottom_width, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_left_width, aValue);
      }
      break;
    case eCSSProperty_content:
      if ((nsnull != mContent) && (nsnull != mContent->mContent)) {
        nsCSSValueList* content = mContent->mContent;
        do {
          AppendValueToString(eCSSProperty_content, content->mValue, aValue);
          content = content->mNext;
          if (nsnull != content) {
            aValue.Append(PRUnichar(' '));
          }
        } while (nsnull != content);
      }
      break;
    case eCSSProperty_counter_increment:
      if ((nsnull != mContent) && (nsnull != mContent->mCounterIncrement)) {
        nsCSSCounterData* data = mContent->mCounterIncrement;
        do {
          if (AppendValueToString(eCSSProperty_counter_increment, data->mCounter, aValue)) {
            if (HAS_VALUE(data, mValue)) {
              aValue.Append(PRUnichar(' '));
              AppendValueToString(eCSSProperty_counter_increment, data->mValue, aValue);
            }
          }
          data = data->mNext;
          if (nsnull != data) {
            aValue.Append(PRUnichar(' '));
          }
        } while (nsnull != data);
      }
      break;
    case eCSSProperty_counter_reset:
      if ((nsnull != mContent) && (nsnull != mContent->mCounterReset)) {
        nsCSSCounterData* data = mContent->mCounterReset;
        do {
          if (AppendValueToString(eCSSProperty_counter_reset, data->mCounter, aValue)) {
            if (HAS_VALUE(data, mValue)) {
              aValue.Append(PRUnichar(' '));
              AppendValueToString(eCSSProperty_counter_reset, data->mValue, aValue);
            }
          }
          data = data->mNext;
          if (nsnull != data) {
            aValue.Append(PRUnichar(' '));
          }
        } while (nsnull != data);
      }
      break;
    case eCSSProperty_play_during:
      if (HAS_VALUE(mAural, mPlayDuring)) {
        AppendValueToString(eCSSProperty_play_during, aValue);
        if (HAS_VALUE(mAural, mPlayDuringFlags)) {
          aValue.Append(PRUnichar(' '));
          AppendValueToString(eCSSProperty_play_during_flags, aValue);
        }
      }
      break;
    case eCSSProperty_quotes:
      if ((nsnull != mContent) && (nsnull != mContent->mQuotes)) {
        nsCSSQuotes* quotes = mContent->mQuotes;
        do {
          AppendValueToString(eCSSProperty_quotes_open, quotes->mOpen, aValue);
          aValue.Append(PRUnichar(' '));
          AppendValueToString(eCSSProperty_quotes_close, quotes->mClose, aValue);
          quotes = quotes->mNext;
          if (nsnull != quotes) {
            aValue.Append(PRUnichar(' '));
          }
        } while (nsnull != quotes);
      }
      break;
    case eCSSProperty_key_equivalent:
      if ((nsnull != mUserInterface) && (nsnull != mUserInterface->mKeyEquivalent)) {
        nsCSSValueList* keyEquiv = mUserInterface->mKeyEquivalent;
        do {
          AppendValueToString(eCSSProperty_key_equivalent, keyEquiv->mValue, aValue);
          keyEquiv = keyEquiv->mNext;
          if (nsnull != keyEquiv) {
            aValue.Append(PRUnichar(' '));
          }
        } while (nsnull != keyEquiv);
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
CSSDeclarationImpl::GetValueIsImportant(const nsAReadableString& aProperty,
                                        PRBool& aIsImportant)
{
  nsCSSProperty propID = nsCSSProps::LookupProperty(aProperty);
  return GetValueIsImportant(propID, aIsImportant);
}

NS_IMETHODIMP
CSSDeclarationImpl::GetValueIsImportant(nsCSSProperty aProperty,
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
CSSDeclarationImpl::ToString(nsAWritableString& aString)
{
  if (nsnull != mOrder) {
    PRInt32 count = mOrder->Count();
    PRInt32 index;
    for (index = 0; index < count; index++) {
      nsCSSProperty property = (nsCSSProperty)NS_PTR_TO_INT32(mOrder->ElementAt(index));
      if (0 <= property) {
        aString.Append(NS_ConvertASCIItoUCS2(nsCSSProps::GetStringValue(property)));
        aString.Append(NS_LITERAL_STRING(": "));

        nsAutoString value;
        GetValue(property, value);
        aString.Append(value);
        if (index < count) {
          aString.Append(NS_LITERAL_STRING("; "));
        }
      }
      else {  // is comment
        aString.Append(NS_LITERAL_STRING("/* "));
        nsString* comment = mComments->StringAt((-1) - property);
        aString.Append(*comment);
        aString.Append(NS_LITERAL_STRING(" */ "));
      }
    }
  }
  return NS_OK;
}

#ifdef DEBUG
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
  if (nsnull != mUserInterface) {
    mUserInterface->List(out);
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

/******************************************************************************
* SizeOf method:
*
*  Self (reported as CSSDeclarationImpl's size): 
*    1) sizeof(*this) + the sizeof each non-null attribute
*
*  Contained / Aggregated data (not reported as CSSDeclarationImpl's size):
*    none
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void CSSDeclarationImpl::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);
  if(! uniqueItems->AddItem((void*)this)){
    return;
  }

  // create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("CSSDeclarationImpl"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);

  // now add in all of the contained objects, checking for duplicates on all of them
  if(mFont && uniqueItems->AddItem(mFont)){
    aSize += sizeof(*mFont);
  }
  if(mColor && uniqueItems->AddItem(mColor)){
    aSize += sizeof(*mColor);
  }
  if(mText && uniqueItems->AddItem(mText)){
    aSize += sizeof(*mText);
  }
  if(mMargin && uniqueItems->AddItem(mMargin)){
    aSize += sizeof(*mMargin);
  }
  if(mPosition && uniqueItems->AddItem(mPosition)){
    aSize += sizeof(*mPosition);
  }
  if(mList && uniqueItems->AddItem(mList)){
    aSize += sizeof(*mList);
  }
  if(mDisplay && uniqueItems->AddItem(mDisplay)){
    aSize += sizeof(*mDisplay);
  }
  if(mTable && uniqueItems->AddItem(mTable)){
    aSize += sizeof(*mTable);
  }
  if(mBreaks && uniqueItems->AddItem(mBreaks)){
    aSize += sizeof(*mBreaks);
  }
  if(mPage && uniqueItems->AddItem(mPage)){
    aSize += sizeof(*mPage);
  }
  if(mContent && uniqueItems->AddItem(mContent)){
    aSize += sizeof(*mContent);
  }
  if(mUserInterface && uniqueItems->AddItem(mUserInterface)){
    aSize += sizeof(*mUserInterface);
  }
#ifdef INCLUDE_XUL
  if(mXUL && uniqueItems->AddItem(mXUL)){
    aSize += sizeof(*mXUL);
  }
#endif
  if(mAural && uniqueItems->AddItem(mAural)){
    aSize += sizeof(*mAural);
  }
  aSizeOfHandler->AddSize(tag, aSize);
}
#endif

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
CSSDeclarationImpl::GetNthProperty(PRUint32 aIndex, nsAWritableString& aReturn)
{
  aReturn.Truncate();
  if (nsnull != mOrder) {
    nsCSSProperty property = (nsCSSProperty)NS_PTR_TO_INT32(mOrder->ElementAt(aIndex));
    if (0 <= property) {
      aReturn.Append(NS_ConvertASCIItoUCS2(nsCSSProps::GetStringValue(property)));
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
      nsCSSProperty property = (nsCSSProperty)NS_PTR_TO_INT32(mOrder->ElementAt(index));
      if (eCSSProperty_UNKNOWN < property) {
        if (hint < nsCSSProps::kHintTable[property]) {
          hint = nsCSSProps::kHintTable[property];
        }
      }
    }
  }
  *aHint = hint;
  return NS_OK;
}

NS_IMETHODIMP
CSSDeclarationImpl::Clone(nsICSSDeclaration*& aClone) const
{
  CSSDeclarationImpl* clone = new CSSDeclarationImpl(*this);
  if (clone) {
    return clone->QueryInterface(NS_GET_IID(nsICSSDeclaration), (void**)&aClone);
  }
  aClone = nsnull;
  return NS_ERROR_OUT_OF_MEMORY;
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

  return it->QueryInterface(NS_GET_IID(nsICSSDeclaration), (void **) aInstancePtrResult);
}


