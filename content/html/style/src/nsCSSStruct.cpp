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
 *   Daniel Glazman <glazman@netscape.com>
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
#include "nsCSSDeclaration.h"
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

#ifdef MOZ_SVG
static NS_DEFINE_IID(kCSSSVGSID, NS_CSS_SVG_SID);
#endif

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

const nsID& nsCSSFont::GetID(void)
{
  return kCSSFontSID;
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

const nsID& nsCSSColor::GetID(void)
{
  return kCSSColorSID;
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

const nsID& nsCSSText::GetID(void)
{
  return kCSSTextSID;
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


#ifdef DEBUG
void nsCSSRect::List(FILE* out, nsCSSProperty aPropID, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  if (eCSSProperty_UNKNOWN < aPropID) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aPropID).get());
    buffer.Append(NS_LITERAL_STRING(": "));
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
    buffer.Append(NS_LITERAL_STRING(": "));
    mTop.AppendToString(buffer);
  }
  if (eCSSUnit_Null != mRight.GetUnit()) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aTRBL[1]).get());
    buffer.Append(NS_LITERAL_STRING(": "));
    mRight.AppendToString(buffer);
  }
  if (eCSSUnit_Null != mBottom.GetUnit()) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aTRBL[2]).get());
    buffer.Append(NS_LITERAL_STRING(": "));
    mBottom.AppendToString(buffer); 
  }
  if (eCSSUnit_Null != mLeft.GetUnit()) {
    buffer.AppendWithConversion(nsCSSProps::GetStringValue(aTRBL[3]).get());
    buffer.Append(NS_LITERAL_STRING(": "));
    mLeft.AppendToString(buffer);
  }

  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

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
    mOpacity(aCopy.mOpacity),
    // temp fix for bug 24000
    mBreakBefore(aCopy.mBreakBefore),
    mBreakAfter(aCopy.mBreakAfter)
    // end temp
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
  if (nsnull != mClip) {
    mClip->List(out, eCSSProperty_clip);
  }
  buffer.SetLength(0);
  mOverflow.AppendToString(buffer, eCSSProperty_overflow);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

// --- nsCSSMargin -----------------

inline void nsCSSMargin::EnsureBorderColors()
{
  if (!mBorderColors) {
    PRInt32 i;
    mBorderColors = new nsCSSValueList*[4];
    for (i = 0; i < 4; i++)
      mBorderColors[i] = nsnull;
  }
}

nsCSSMargin::nsCSSMargin(void)
  : mMargin(nsnull), mPadding(nsnull), 
    mBorderWidth(nsnull), mBorderColor(nsnull), mBorderColors(nsnull),
    mBorderStyle(nsnull), mBorderRadius(nsnull), mOutlineRadius(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSMargin);
}

nsCSSMargin::nsCSSMargin(const nsCSSMargin& aCopy)
  : mMargin(nsnull), mPadding(nsnull), 
    mBorderWidth(nsnull), mBorderColor(nsnull), mBorderColors(nsnull),
    mBorderStyle(nsnull), mBorderRadius(nsnull),
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
  if (aCopy.mBorderColors) {
    EnsureBorderColors();
    for (PRInt32 i = 0; i < 4; i++)
      CSS_IF_COPY(mBorderColors[i], nsCSSValueList);
  }
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
  if (mBorderColors) {
    for (PRInt32 i = 0; i < 4; i++)
      CSS_IF_DELETE(mBorderColors[i]);
    delete []mBorderColors;
  }
}

const nsID& nsCSSMargin::GetID(void)
{
  return kCSSMarginSID;
}

#ifdef DEBUG
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
  mOutlineWidth.AppendToString(buffer, eCSSProperty__moz_outline_width);
  mOutlineColor.AppendToString(buffer, eCSSProperty__moz_outline_color);
  mOutlineStyle.AppendToString(buffer, eCSSProperty__moz_outline_style);
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
#endif

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
#endif

// --- nsCSSList -----------------

nsCSSList::nsCSSList(void)
:mImageRegion(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSList);
}

nsCSSList::nsCSSList(const nsCSSList& aCopy)
  : mType(aCopy.mType),
    mImage(aCopy.mImage),
    mPosition(aCopy.mPosition),
    mImageRegion(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSList);
  CSS_IF_COPY(mImageRegion, nsCSSRect);
}

nsCSSList::~nsCSSList(void)
{
  MOZ_COUNT_DTOR(nsCSSList);
  CSS_IF_DELETE(mImageRegion);
}

const nsID& nsCSSList::GetID(void)
{
  return kCSSListSID;
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

  if (mImageRegion) {
    static const nsCSSProperty trbl[] = {
      eCSSProperty_top,
      eCSSProperty_right,
      eCSSProperty_bottom,
      eCSSProperty_left
    };
    mImageRegion->List(out, aIndent, trbl);
  }
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

const nsID& nsCSSTable::GetID(void)
{
  return kCSSTableSID;
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

#ifdef DEBUG
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

const nsID& nsCSSPage::GetID(void)
{
  return kCSSPageSID;
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
  nsCSSQuotes*  quotes = mQuotes;
  while (nsnull != quotes) {
    quotes->mOpen.AppendToString(buffer, eCSSProperty_quotes_open);
    quotes->mClose.AppendToString(buffer, eCSSProperty_quotes_close);
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
    mResizer(aCopy.mResizer),
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

const nsID& nsCSSUserInterface::GetID(void)
{
  return kCSSUserInterfaceSID;
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
  mResizer.AppendToString(buffer, eCSSProperty_resizer);
  
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

const nsID& nsCSSAural::GetID(void)
{
  return kCSSAuralSID;
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
#endif

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

#endif // INCLUDE_XUL

#ifdef MOZ_SVG
// --- nsCSSSVG -----------------

nsCSSSVG::nsCSSSVG(void)
{
  MOZ_COUNT_CTOR(nsCSSSVG);
}

nsCSSSVG::nsCSSSVG(const nsCSSSVG& aCopy)
    : mFill(aCopy.mFill),
      mFillOpacity(aCopy.mFillOpacity),
      mFillRule(aCopy.mFillRule),
      mStroke(aCopy.mStroke),
      mStrokeDasharray(aCopy.mStrokeDasharray),
      mStrokeDashoffset(aCopy.mStrokeDashoffset),
      mStrokeLinecap(aCopy.mStrokeLinecap),
      mStrokeLinejoin(aCopy.mStrokeLinejoin),
      mStrokeMiterlimit(aCopy.mStrokeMiterlimit),
      mStrokeOpacity(aCopy.mStrokeOpacity),
      mStrokeWidth(aCopy.mStrokeWidth)
{
  MOZ_COUNT_CTOR(nsCSSSVG);
}

nsCSSSVG::~nsCSSSVG(void)
{
  MOZ_COUNT_DTOR(nsCSSSVG);
}

const nsID& nsCSSSVG::GetID(void)
{
  return kCSSSVGSID;
}

#ifdef DEBUG
void nsCSSSVG::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mFill.AppendToString(buffer, eCSSProperty_fill);
  mFillOpacity.AppendToString(buffer, eCSSProperty_fill_opacity);
  mFillRule.AppendToString(buffer, eCSSProperty_fill_rule);
  mStroke.AppendToString(buffer, eCSSProperty_stroke);
  mStrokeDasharray.AppendToString(buffer, eCSSProperty_stroke_dasharray);
  mStrokeDashoffset.AppendToString(buffer, eCSSProperty_stroke_dashoffset);
  mStrokeLinecap.AppendToString(buffer, eCSSProperty_stroke_linecap);
  mStrokeLinejoin.AppendToString(buffer, eCSSProperty_stroke_linejoin);
  mStrokeMiterlimit.AppendToString(buffer, eCSSProperty_stroke_miterlimit);
  mStrokeOpacity.AppendToString(buffer, eCSSProperty_stroke_opacity);
  mStrokeWidth.AppendToString(buffer, eCSSProperty_stroke_width);
  fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
}
#endif

#endif // MOZ_SVG


#ifdef DEBUG_REFS
static PRInt32 gInstanceCount;
#endif

#define B_BORDER_TOP_STYLE    0x001
#define B_BORDER_LEFT_STYLE   0x002
#define B_BORDER_RIGHT_STYLE  0x004
#define B_BORDER_BOTTOM_STYLE 0x008
#define B_BORDER_TOP_COLOR    0x010
#define B_BORDER_LEFT_COLOR   0x020
#define B_BORDER_RIGHT_COLOR  0x040
#define B_BORDER_BOTTOM_COLOR 0x080
#define B_BORDER_TOP_WIDTH    0x100
#define B_BORDER_LEFT_WIDTH   0x200
#define B_BORDER_RIGHT_WIDTH  0x400
#define B_BORDER_BOTTOM_WIDTH 0x800

#define B_BORDER_STYLE        0x00f
#define B_BORDER_COLOR        0x0f0
#define B_BORDER_WIDTH        0xf00

#define B_BORDER_TOP          0x111
#define B_BORDER_LEFT         0x222
#define B_BORDER_RIGHT        0x444
#define B_BORDER_BOTTOM       0x888

#define B_BORDER              0xfff

NS_IMPL_ZEROING_OPERATOR_NEW(nsCSSDeclaration)

nsCSSDeclaration::nsCSSDeclaration(void) 
{
#ifdef DEBUG_REFS
  ++gInstanceCount;
  fprintf(stdout, "CSSDeclaration Instances (ctor): %ld\n", (long)gInstanceCount);
#endif
}

#define DECL_IF_COPY(type) \
  if (aCopy.mContains.mHas##type) { \
    nsCSS##type& copyMe = *(nsCSS##type*)aCopy.mStructs.ElementAt(CSSDECLIDX_##type(aCopy)); \
    nsCSS##type* newMe = new nsCSS##type(copyMe); \
    if (newMe) { \
      mContains.mHas##type = 1; \
      PRBool insertRes = mStructs.InsertElementAt((void*)newMe, CSSDECLIDX_##type(*this)); \
      NS_ASSERTION(PR_FALSE != insertRes, "Unable to insert!"); \
      if (PR_FALSE == insertRes) { \
        delete newMe; \
        mContains.mHas##type = 0; \
      } \
    } \
  }

#define CSS_NEW_NSVALUEARRAY new nsValueArray((nsCSSProperty)eCSSProperty_COUNT, 8)

nsCSSDeclaration::nsCSSDeclaration(const nsCSSDeclaration& aCopy)
{
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

#ifdef MOZ_SVG
  DECL_IF_COPY(SVG);
#endif
  
#ifdef DEBUG_REFS
  ++gInstanceCount;
  fprintf(stdout, "CSSDeclaration Instances (cp-ctor): %ld\n", (long)gInstanceCount);
#endif

  if (aCopy.mImportant) {
    mImportant = new nsCSSDeclaration(*(aCopy.mImportant));
  }

  if (aCopy.mOrder) {
    // avoid needless reallocation of array should the assignment be larger.
    // mOrder = CSS_NEW_NSVALUEARRAY;
    mOrder = new nsValueArray((nsCSSProperty)eCSSProperty_COUNT,
                              aCopy.mOrder->Count());
    if (mOrder) {
      (*mOrder) = *(aCopy.mOrder);
    }
  }
}

#define CSS_HAS_DELETE(type) \
  if (mContains.mHas##type) { \
    nsCSS##type* delMe = (nsCSS##type*)mStructs.ElementAt(CSSDECLIDX_##type(*this)); \
    delete delMe; \
  }

nsCSSDeclaration::~nsCSSDeclaration(void)
{
  CSS_HAS_DELETE(Font);
  CSS_HAS_DELETE(Color);
  CSS_HAS_DELETE(Text);
  CSS_HAS_DELETE(Margin);
  CSS_HAS_DELETE(Position);
  CSS_HAS_DELETE(List);
  CSS_HAS_DELETE(Display);
  CSS_HAS_DELETE(Table);
  CSS_HAS_DELETE(Breaks);
  CSS_HAS_DELETE(Page);
  CSS_HAS_DELETE(Content);
  CSS_HAS_DELETE(UserInterface);
  CSS_HAS_DELETE(Aural);
#ifdef INCLUDE_XUL
  CSS_HAS_DELETE(XUL);
#endif

#ifdef MOZ_SVG
  CSS_HAS_DELETE(SVG);
#endif

  CSS_IF_DELETE(mImportant);
  CSS_IF_DELETE(mOrder);

#ifdef DEBUG_REFS
  --gInstanceCount;
  fprintf(stdout, "CSSDeclaration Instances (dtor): %ld\n", (long)gInstanceCount);
#endif
}

#define CSS_IF_GET_ELSE(sid,ptr) \
  if (mContains.mHas##ptr && sid.Equals(kCSS##ptr##SID)) { \
    return (nsCSS##ptr*)mStructs.ElementAt(CSSDECLIDX_##ptr(*this)); \
  } \
  else

nsCSSStruct*
nsCSSDeclaration::GetData(const nsID& aSID)
{
  CSS_IF_GET_ELSE(aSID, Font)
  CSS_IF_GET_ELSE(aSID, Color)
  CSS_IF_GET_ELSE(aSID, Display)
  CSS_IF_GET_ELSE(aSID, Text)
  CSS_IF_GET_ELSE(aSID, Margin)
  CSS_IF_GET_ELSE(aSID, Position)
  CSS_IF_GET_ELSE(aSID, List)
  CSS_IF_GET_ELSE(aSID, Table)
  CSS_IF_GET_ELSE(aSID, Breaks)
  CSS_IF_GET_ELSE(aSID, Page)
  CSS_IF_GET_ELSE(aSID, Content)
  CSS_IF_GET_ELSE(aSID, UserInterface)
  CSS_IF_GET_ELSE(aSID, Aural)
#ifdef INCLUDE_XUL
  CSS_IF_GET_ELSE(aSID, XUL)
#endif
#ifdef MOZ_SVG
  CSS_IF_GET_ELSE(aSID, SVG)
#endif
  {
    return nsnull;
  }

  return nsnull;
}

#define CSS_IF_ENSURE_ELSE(sid,type) \
  if (sid.Equals(kCSS##type##SID)) { \
    if (mContains.mHas##type) { \
      return (nsCSS##type*)mStructs.ElementAt(CSSDECLIDX_##type(*this)); \
    }  \
    else { \
      nsCSS##type* newMe = new nsCSS##type(); \
      if (newMe) { \
        mContains.mHas##type = 1; \
        PRBool insertRes = mStructs.InsertElementAt((void*)newMe, CSSDECLIDX_##type(*this)); \
        NS_ASSERTION(PR_FALSE != insertRes, "Unable to insert!"); \
        if (PR_FALSE == insertRes) { \
          delete newMe; \
          newMe = nsnull; \
          mContains.mHas##type = 0; \
        } \
      } \
      return newMe; \
    } \
  } \
  else

nsCSSStruct*
nsCSSDeclaration::EnsureData(const nsID& aSID)
{
  CSS_IF_ENSURE_ELSE(aSID, Font)
  CSS_IF_ENSURE_ELSE(aSID, Color)
  CSS_IF_ENSURE_ELSE(aSID, Display)
  CSS_IF_ENSURE_ELSE(aSID, Text)
  CSS_IF_ENSURE_ELSE(aSID, Margin)
  CSS_IF_ENSURE_ELSE(aSID, Position)
  CSS_IF_ENSURE_ELSE(aSID, List)
  CSS_IF_ENSURE_ELSE(aSID, Table)
  CSS_IF_ENSURE_ELSE(aSID, Breaks)
  CSS_IF_ENSURE_ELSE(aSID, Page)
  CSS_IF_ENSURE_ELSE(aSID, Content)
  CSS_IF_ENSURE_ELSE(aSID, UserInterface)
  CSS_IF_ENSURE_ELSE(aSID, Aural)
  {
    return nsnull;
  }

  return nsnull;
}

#define CSS_ENSURE(type) \
  nsCSS##type* the##type = nsnull; \
  if (mContains.mHas##type) { \
    the##type = (nsCSS##type*)mStructs.ElementAt(CSSDECLIDX_##type(*this)); \
  } \
  else { \
    nsCSS##type* newMe = new nsCSS##type(); \
    if (newMe) { \
      mContains.mHas##type = 1; \
      PRBool insertRes = mStructs.InsertElementAt((void*)newMe, CSSDECLIDX_##type(*this)); \
      NS_ASSERTION(PR_FALSE != insertRes, "Unable to insert!"); \
      if (PR_FALSE == insertRes) { \
        delete newMe; \
        newMe = nsnull; \
        mContains.mHas##type = 0; \
      } \
    } \
    the##type = newMe; \
  } \
  if (nsnull == the##type) { \
    result = NS_ERROR_OUT_OF_MEMORY; \
  } \
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

nsresult
nsCSSDeclaration::AppendValue(nsCSSProperty aProperty, const nsCSSValue& aValue)
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
    case eCSSProperty_font_stretch: {
      CSS_ENSURE(Font) {
        switch (aProperty) {
          case eCSSProperty_font_family:      theFont->mFamily = aValue;      break;
          case eCSSProperty_font_style:       theFont->mStyle = aValue;       break;
          case eCSSProperty_font_variant:     theFont->mVariant = aValue;     break;
          case eCSSProperty_font_weight:      theFont->mWeight = aValue;      break;
          case eCSSProperty_font_size:        theFont->mSize = aValue;        break;
          case eCSSProperty_font_size_adjust: theFont->mSizeAdjust = aValue;  break;
          case eCSSProperty_font_stretch:     theFont->mStretch = aValue;     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

    // nsCSSColor
    case eCSSProperty_color:
    case eCSSProperty_background_color:
    case eCSSProperty_background_image:
    case eCSSProperty_background_repeat:
    case eCSSProperty_background_attachment:
    case eCSSProperty_background_x_position:
    case eCSSProperty_background_y_position:
    case eCSSProperty__moz_background_clip:
    case eCSSProperty__moz_background_origin:
    case eCSSProperty__moz_background_inline_policy: {
      CSS_ENSURE(Color) {
        switch (aProperty) {
          case eCSSProperty_color:                  theColor->mColor = aValue;           break;
          case eCSSProperty_background_color:       theColor->mBackColor = aValue;       break;
          case eCSSProperty_background_image:       theColor->mBackImage = aValue;       break;
          case eCSSProperty_background_repeat:      theColor->mBackRepeat = aValue;      break;
          case eCSSProperty_background_attachment:  theColor->mBackAttachment = aValue;  break;
          case eCSSProperty_background_x_position:  theColor->mBackPositionX = aValue;   break;
          case eCSSProperty_background_y_position:  theColor->mBackPositionY = aValue;   break;
          case eCSSProperty__moz_background_clip:   theColor->mBackClip = aValue;        break;
          case eCSSProperty__moz_background_origin: theColor->mBackOrigin = aValue;      break;
          case eCSSProperty__moz_background_inline_policy: theColor->mBackInlinePolicy = aValue; break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

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
    case eCSSProperty_white_space: {
      CSS_ENSURE(Text) {
        switch (aProperty) {
          case eCSSProperty_word_spacing:     theText->mWordSpacing = aValue;    break;
          case eCSSProperty_letter_spacing:   theText->mLetterSpacing = aValue;  break;
          case eCSSProperty_text_decoration:  theText->mDecoration = aValue;     break;
          case eCSSProperty_vertical_align:   theText->mVerticalAlign = aValue;  break;
          case eCSSProperty_text_transform:   theText->mTextTransform = aValue;  break;
          case eCSSProperty_text_align:       theText->mTextAlign = aValue;      break;
          case eCSSProperty_text_indent:      theText->mTextIndent = aValue;     break;
          case eCSSProperty_unicode_bidi:     theText->mUnicodeBidi = aValue;    break;
          case eCSSProperty_line_height:      theText->mLineHeight = aValue;     break;
          case eCSSProperty_white_space:      theText->mWhiteSpace = aValue;     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

    case eCSSProperty_text_shadow_color:
    case eCSSProperty_text_shadow_radius:
    case eCSSProperty_text_shadow_x:
    case eCSSProperty_text_shadow_y: {
      CSS_ENSURE(Text) {
        CSS_ENSURE_DATA(theText->mTextShadow, nsCSSShadow) {
          switch (aProperty) {
            case eCSSProperty_text_shadow_color:  theText->mTextShadow->mColor = aValue;    break;
            case eCSSProperty_text_shadow_radius: theText->mTextShadow->mRadius = aValue;   break;
            case eCSSProperty_text_shadow_x:      theText->mTextShadow->mXOffset = aValue;  break;
            case eCSSProperty_text_shadow_y:      theText->mTextShadow->mYOffset = aValue;  break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
          CSS_IF_DELETE(theText->mTextShadow->mNext);
        }
      }
      break;
    }

    // nsCSSDisplay
    case eCSSProperty_appearance:
    case eCSSProperty_float:
    case eCSSProperty_clear:
    case eCSSProperty_display:
    case eCSSProperty_binding:
    case eCSSProperty_position:          
    case eCSSProperty_direction:
    case eCSSProperty_visibility:
    case eCSSProperty_opacity:
    case eCSSProperty_overflow: {
      CSS_ENSURE(Display) {
        switch (aProperty) {
          case eCSSProperty_appearance: theDisplay->mAppearance = aValue; break;
          case eCSSProperty_float:      theDisplay->mFloat = aValue;      break;
          case eCSSProperty_clear:      theDisplay->mClear = aValue;      break;
          case eCSSProperty_display:    theDisplay->mDisplay = aValue;    break;
          case eCSSProperty_position:   theDisplay->mPosition = aValue;   break;
          case eCSSProperty_direction:  theDisplay->mDirection = aValue;  break;
          case eCSSProperty_visibility: theDisplay->mVisibility = aValue; break;
          case eCSSProperty_opacity:    theDisplay->mOpacity = aValue;    break;
          case eCSSProperty_overflow:   theDisplay->mOverflow = aValue;   break;
          case eCSSProperty_binding:         
            theDisplay->mBinding = aValue;      
            break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

    case eCSSProperty_clip_top:
    case eCSSProperty_clip_right:
    case eCSSProperty_clip_bottom:
    case eCSSProperty_clip_left: {
      CSS_ENSURE(Display) {
        CSS_ENSURE_RECT(theDisplay->mClip) {
          switch(aProperty) {
            case eCSSProperty_clip_top:     theDisplay->mClip->mTop = aValue;     break;
            case eCSSProperty_clip_right:   theDisplay->mClip->mRight = aValue;   break;
            case eCSSProperty_clip_bottom:  theDisplay->mClip->mBottom = aValue;  break;
            case eCSSProperty_clip_left:    theDisplay->mClip->mLeft = aValue;    break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    // nsCSSMargin
    case eCSSProperty_margin_top:
    case eCSSProperty_margin_right:
    case eCSSProperty_margin_bottom:
    case eCSSProperty_margin_left: {
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(theMargin->mMargin) {
          switch (aProperty) {
            case eCSSProperty_margin_top:     theMargin->mMargin->mTop = aValue;     break;
            case eCSSProperty_margin_right:   theMargin->mMargin->mRight = aValue;   break;
            case eCSSProperty_margin_bottom:  theMargin->mMargin->mBottom = aValue;  break;
            case eCSSProperty_margin_left:    theMargin->mMargin->mLeft = aValue;    break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    case eCSSProperty_padding_top:
    case eCSSProperty_padding_right:
    case eCSSProperty_padding_bottom:
    case eCSSProperty_padding_left: {
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(theMargin->mPadding) {
          switch (aProperty) {
            case eCSSProperty_padding_top:    theMargin->mPadding->mTop = aValue;    break;
            case eCSSProperty_padding_right:  theMargin->mPadding->mRight = aValue;  break;
            case eCSSProperty_padding_bottom: theMargin->mPadding->mBottom = aValue; break;
            case eCSSProperty_padding_left:   theMargin->mPadding->mLeft = aValue;   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    case eCSSProperty_border_top_width:
    case eCSSProperty_border_right_width:
    case eCSSProperty_border_bottom_width:
    case eCSSProperty_border_left_width: {
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(theMargin->mBorderWidth) {
          switch (aProperty) {
            case eCSSProperty_border_top_width:     theMargin->mBorderWidth->mTop = aValue;     break;
            case eCSSProperty_border_right_width:   theMargin->mBorderWidth->mRight = aValue;   break;
            case eCSSProperty_border_bottom_width:  theMargin->mBorderWidth->mBottom = aValue;  break;
            case eCSSProperty_border_left_width:    theMargin->mBorderWidth->mLeft = aValue;    break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    case eCSSProperty_border_top_color:
    case eCSSProperty_border_right_color:
    case eCSSProperty_border_bottom_color:
    case eCSSProperty_border_left_color: {
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(theMargin->mBorderColor) {
          switch (aProperty) {
            case eCSSProperty_border_top_color:     theMargin->mBorderColor->mTop = aValue;    break;
            case eCSSProperty_border_right_color:   theMargin->mBorderColor->mRight = aValue;  break;
            case eCSSProperty_border_bottom_color:  theMargin->mBorderColor->mBottom = aValue; break;
            case eCSSProperty_border_left_color:    theMargin->mBorderColor->mLeft = aValue;   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    case eCSSProperty_border_top_style:
    case eCSSProperty_border_right_style:
    case eCSSProperty_border_bottom_style:
    case eCSSProperty_border_left_style: {
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(theMargin->mBorderStyle) {
          switch (aProperty) {
            case eCSSProperty_border_top_style:     theMargin->mBorderStyle->mTop = aValue;    break;
            case eCSSProperty_border_right_style:   theMargin->mBorderStyle->mRight = aValue;  break;
            case eCSSProperty_border_bottom_style:  theMargin->mBorderStyle->mBottom = aValue; break;
            case eCSSProperty_border_left_style:    theMargin->mBorderStyle->mLeft = aValue;   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    case eCSSProperty__moz_border_radius_topLeft:
    case eCSSProperty__moz_border_radius_topRight:
    case eCSSProperty__moz_border_radius_bottomRight:
    case eCSSProperty__moz_border_radius_bottomLeft: {
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(theMargin->mBorderRadius) {
          switch (aProperty) {
            case eCSSProperty__moz_border_radius_topLeft:			theMargin->mBorderRadius->mTop = aValue;    break;
            case eCSSProperty__moz_border_radius_topRight:		theMargin->mBorderRadius->mRight = aValue;  break;
            case eCSSProperty__moz_border_radius_bottomRight:	theMargin->mBorderRadius->mBottom = aValue; break;
            case eCSSProperty__moz_border_radius_bottomLeft:	theMargin->mBorderRadius->mLeft = aValue;   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    case eCSSProperty__moz_outline_radius_topLeft:
    case eCSSProperty__moz_outline_radius_topRight:
    case eCSSProperty__moz_outline_radius_bottomRight:
    case eCSSProperty__moz_outline_radius_bottomLeft: {
      CSS_ENSURE(Margin) {
        CSS_ENSURE_RECT(theMargin->mOutlineRadius) {
          switch (aProperty) {
            case eCSSProperty__moz_outline_radius_topLeft:			theMargin->mOutlineRadius->mTop = aValue;    break;
            case eCSSProperty__moz_outline_radius_topRight:			theMargin->mOutlineRadius->mRight = aValue;  break;
            case eCSSProperty__moz_outline_radius_bottomRight:	theMargin->mOutlineRadius->mBottom = aValue; break;
            case eCSSProperty__moz_outline_radius_bottomLeft:		theMargin->mOutlineRadius->mLeft = aValue;   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    case eCSSProperty__moz_outline_width:
    case eCSSProperty__moz_outline_color:
    case eCSSProperty__moz_outline_style:
    case eCSSProperty_float_edge: {
      CSS_ENSURE(Margin) {
        switch (aProperty) {
          case eCSSProperty__moz_outline_width: theMargin->mOutlineWidth = aValue;  break;
          case eCSSProperty__moz_outline_color: theMargin->mOutlineColor = aValue;  break;
          case eCSSProperty__moz_outline_style: theMargin->mOutlineStyle = aValue;  break;
          case eCSSProperty_float_edge:         theMargin->mFloatEdge = aValue;     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

    // nsCSSPosition
    case eCSSProperty_width:
    case eCSSProperty_min_width:
    case eCSSProperty_max_width:
    case eCSSProperty_height:
    case eCSSProperty_min_height:
    case eCSSProperty_max_height:
    case eCSSProperty_box_sizing:
    case eCSSProperty_z_index: {
      CSS_ENSURE(Position) {
        switch (aProperty) {
          case eCSSProperty_width:      thePosition->mWidth = aValue;      break;
          case eCSSProperty_min_width:  thePosition->mMinWidth = aValue;   break;
          case eCSSProperty_max_width:  thePosition->mMaxWidth = aValue;   break;
          case eCSSProperty_height:     thePosition->mHeight = aValue;     break;
          case eCSSProperty_min_height: thePosition->mMinHeight = aValue;  break;
          case eCSSProperty_max_height: thePosition->mMaxHeight = aValue;  break;
          case eCSSProperty_box_sizing: thePosition->mBoxSizing = aValue;  break;
          case eCSSProperty_z_index:    thePosition->mZIndex = aValue;     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

    case eCSSProperty_top:
    case eCSSProperty_right:
    case eCSSProperty_bottom:
    case eCSSProperty_left: {
      CSS_ENSURE(Position) {
        CSS_ENSURE_RECT(thePosition->mOffset) {
          switch (aProperty) {
            case eCSSProperty_top:    thePosition->mOffset->mTop = aValue;    break;
            case eCSSProperty_right:  thePosition->mOffset->mRight= aValue;   break;
            case eCSSProperty_bottom: thePosition->mOffset->mBottom = aValue; break;
            case eCSSProperty_left:   thePosition->mOffset->mLeft = aValue;   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

      // nsCSSList
    case eCSSProperty_list_style_type:
    case eCSSProperty_list_style_image:
    case eCSSProperty_list_style_position: {
      CSS_ENSURE(List) {
        switch (aProperty) {
          case eCSSProperty_list_style_type:      theList->mType = aValue;     break;
          case eCSSProperty_list_style_image:     theList->mImage = aValue;    break;
          case eCSSProperty_list_style_position:  theList->mPosition = aValue; break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

    case eCSSProperty_image_region_top:
    case eCSSProperty_image_region_right:
    case eCSSProperty_image_region_bottom:
    case eCSSProperty_image_region_left: {
      CSS_ENSURE(List) {
        CSS_ENSURE_RECT(theList->mImageRegion) {
          switch(aProperty) {
            case eCSSProperty_image_region_top:     theList->mImageRegion->mTop = aValue;     break;
            case eCSSProperty_image_region_right:   theList->mImageRegion->mRight = aValue;   break;
            case eCSSProperty_image_region_bottom:  theList->mImageRegion->mBottom = aValue;  break;
            case eCSSProperty_image_region_left:    theList->mImageRegion->mLeft = aValue;    break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

      // nsCSSTable
    case eCSSProperty_border_collapse:
    case eCSSProperty_border_x_spacing:
    case eCSSProperty_border_y_spacing:
    case eCSSProperty_caption_side:
    case eCSSProperty_empty_cells:
    case eCSSProperty_table_layout: {
      CSS_ENSURE(Table) {
        switch (aProperty) {
          case eCSSProperty_border_collapse:  theTable->mBorderCollapse = aValue; break;
          case eCSSProperty_border_x_spacing: theTable->mBorderSpacingX = aValue; break;
          case eCSSProperty_border_y_spacing: theTable->mBorderSpacingY = aValue; break;
          case eCSSProperty_caption_side:     theTable->mCaptionSide = aValue;    break;
          case eCSSProperty_empty_cells:      theTable->mEmptyCells = aValue;     break;
          case eCSSProperty_table_layout:     theTable->mLayout = aValue;         break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

      // nsCSSBreaks
    case eCSSProperty_orphans:
    case eCSSProperty_widows:
    case eCSSProperty_page: {
      CSS_ENSURE(Breaks) {
        switch (aProperty) {
          case eCSSProperty_orphans:            theBreaks->mOrphans = aValue;         break;
          case eCSSProperty_widows:             theBreaks->mWidows = aValue;          break;
          case eCSSProperty_page:               theBreaks->mPage = aValue;            break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }
    case eCSSProperty_page_break_after:
    case eCSSProperty_page_break_before:
    case eCSSProperty_page_break_inside: {
      // temp fix for bug 24000
      CSS_ENSURE(Display) {
        switch (aProperty) {
          case eCSSProperty_page_break_after:   theDisplay->mBreakAfter  = aValue; break;
          case eCSSProperty_page_break_before:  theDisplay->mBreakBefore = aValue; break;
          case eCSSProperty_page_break_inside:                                     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

      // nsCSSPage
    case eCSSProperty_marks:
    case eCSSProperty_size_width:
    case eCSSProperty_size_height: {
      CSS_ENSURE(Page) {
        switch (aProperty) {
          case eCSSProperty_marks:        thePage->mMarks = aValue; break;
          case eCSSProperty_size_width:   thePage->mSizeWidth = aValue;  break;
          case eCSSProperty_size_height:  thePage->mSizeHeight = aValue;  break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

      // nsCSSContent
    case eCSSProperty_content:
    case eCSSProperty__moz_counter_increment:
    case eCSSProperty__moz_counter_reset:
    case eCSSProperty_marker_offset:
    case eCSSProperty_quotes_open:
    case eCSSProperty_quotes_close: {
      CSS_ENSURE(Content) {
        switch (aProperty) {
          case eCSSProperty_content:
            CSS_ENSURE_DATA(theContent->mContent, nsCSSValueList) {
              theContent->mContent->mValue = aValue;          
              CSS_IF_DELETE(theContent->mContent->mNext);
            }
            break;
          case eCSSProperty__moz_counter_increment:
            CSS_ENSURE_DATA(theContent->mCounterIncrement, nsCSSCounterData) {
              theContent->mCounterIncrement->mCounter = aValue; 
              CSS_IF_DELETE(theContent->mCounterIncrement->mNext);
            }
            break;
          case eCSSProperty__moz_counter_reset:
            CSS_ENSURE_DATA(theContent->mCounterReset, nsCSSCounterData) {
              theContent->mCounterReset->mCounter = aValue;
              CSS_IF_DELETE(theContent->mCounterReset->mNext);
            }
            break;
          case eCSSProperty_marker_offset:
            theContent->mMarkerOffset = aValue;
            break;
          case eCSSProperty_quotes_open:
            CSS_ENSURE_DATA(theContent->mQuotes, nsCSSQuotes) {
              theContent->mQuotes->mOpen = aValue;          
              CSS_IF_DELETE(theContent->mQuotes->mNext);
            }
            break;
          case eCSSProperty_quotes_close:
            CSS_ENSURE_DATA(theContent->mQuotes, nsCSSQuotes) {
              theContent->mQuotes->mClose = aValue;          
              CSS_IF_DELETE(theContent->mQuotes->mNext);
            }
            break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

    // nsCSSUserInterface
    case eCSSProperty_user_input:
    case eCSSProperty_user_modify:
    case eCSSProperty_user_select:
    case eCSSProperty_key_equivalent:
    case eCSSProperty_user_focus:
    case eCSSProperty_resizer:
    case eCSSProperty_cursor:
    case eCSSProperty_force_broken_image_icon: {
      CSS_ENSURE(UserInterface) {
        switch (aProperty) {
          case eCSSProperty_user_input:       theUserInterface->mUserInput = aValue;      break;
          case eCSSProperty_user_modify:      theUserInterface->mUserModify = aValue;     break;
          case eCSSProperty_user_select:      theUserInterface->mUserSelect = aValue;     break;
          case eCSSProperty_key_equivalent: 
            CSS_ENSURE_DATA(theUserInterface->mKeyEquivalent, nsCSSValueList) {
              theUserInterface->mKeyEquivalent->mValue = aValue;
              CSS_IF_DELETE(theUserInterface->mKeyEquivalent->mNext);
            }
            break;
          case eCSSProperty_user_focus:       theUserInterface->mUserFocus = aValue;      break;
          case eCSSProperty_resizer:          theUserInterface->mResizer = aValue;        break;
          case eCSSProperty_cursor:
            CSS_ENSURE_DATA(theUserInterface->mCursor, nsCSSValueList) {
              theUserInterface->mCursor->mValue = aValue;
              CSS_IF_DELETE(theUserInterface->mCursor->mNext);
            }
            break;
          case eCSSProperty_force_broken_image_icon: theUserInterface->mForceBrokenImageIcon = aValue; break;

          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

#ifdef INCLUDE_XUL
    // nsCSSXUL
    case eCSSProperty_box_align:
    case eCSSProperty_box_direction:
    case eCSSProperty_box_flex:
    case eCSSProperty_box_orient:
    case eCSSProperty_box_pack:
    case eCSSProperty_box_ordinal_group: {
      CSS_ENSURE(XUL) {
        switch (aProperty) {
          case eCSSProperty_box_align:         theXUL->mBoxAlign     = aValue;   break;
          case eCSSProperty_box_direction:     theXUL->mBoxDirection = aValue;   break;
          case eCSSProperty_box_flex:          theXUL->mBoxFlex      = aValue;   break;
          case eCSSProperty_box_orient:        theXUL->mBoxOrient    = aValue;   break;
          case eCSSProperty_box_pack:          theXUL->mBoxPack      = aValue;   break;
          case eCSSProperty_box_ordinal_group: theXUL->mBoxOrdinal   = aValue;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }
#endif

#ifdef MOZ_SVG
    // nsCSSSVG
    case eCSSProperty_fill:
    case eCSSProperty_fill_opacity:
    case eCSSProperty_fill_rule:
    case eCSSProperty_stroke:
    case eCSSProperty_stroke_dasharray:
    case eCSSProperty_stroke_dashoffset:
    case eCSSProperty_stroke_linecap:
    case eCSSProperty_stroke_linejoin:
    case eCSSProperty_stroke_miterlimit:
    case eCSSProperty_stroke_opacity:
    case eCSSProperty_stroke_width: {
      CSS_ENSURE(SVG) {
        switch (aProperty) {
          case eCSSProperty_fill:              theSVG->mFill = aValue;            break;
          case eCSSProperty_fill_opacity:      theSVG->mFillOpacity = aValue;     break;
          case eCSSProperty_fill_rule:         theSVG->mFillRule = aValue;        break;
          case eCSSProperty_stroke:            theSVG->mStroke = aValue;          break;
          case eCSSProperty_stroke_dasharray:  theSVG->mStrokeDasharray = aValue; break;
          case eCSSProperty_stroke_dashoffset: theSVG->mStrokeDashoffset = aValue; break;
          case eCSSProperty_stroke_linecap:    theSVG->mStrokeLinecap = aValue;   break;
          case eCSSProperty_stroke_linejoin:   theSVG->mStrokeLinejoin = aValue; break;
          case eCSSProperty_stroke_miterlimit: theSVG->mStrokeMiterlimit = aValue; break;
          case eCSSProperty_stroke_opacity:    theSVG->mStrokeOpacity = aValue;   break;
          case eCSSProperty_stroke_width:      theSVG->mStrokeWidth = aValue;     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }
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
    case eCSSProperty_volume: {
      CSS_ENSURE(Aural) {
        switch (aProperty) {
          case eCSSProperty_azimuth:            theAural->mAzimuth = aValue;          break;
          case eCSSProperty_elevation:          theAural->mElevation = aValue;        break;
          case eCSSProperty_cue_after:          theAural->mCueAfter = aValue;         break;
          case eCSSProperty_cue_before:         theAural->mCueBefore = aValue;        break;
          case eCSSProperty_pause_after:        theAural->mPauseAfter = aValue;       break;
          case eCSSProperty_pause_before:       theAural->mPauseBefore = aValue;      break;
          case eCSSProperty_pitch:              theAural->mPitch = aValue;            break;
          case eCSSProperty_pitch_range:        theAural->mPitchRange = aValue;       break;
          case eCSSProperty_play_during:        theAural->mPlayDuring = aValue;       break;
          case eCSSProperty_play_during_flags:  theAural->mPlayDuringFlags = aValue;  break;
          case eCSSProperty_richness:           theAural->mRichness = aValue;         break;
          case eCSSProperty_speak:              theAural->mSpeak = aValue;            break;
          case eCSSProperty_speak_header:       theAural->mSpeakHeader = aValue;      break;
          case eCSSProperty_speak_numeral:      theAural->mSpeakNumeral = aValue;     break;
          case eCSSProperty_speak_punctuation:  theAural->mSpeakPunctuation = aValue; break;
          case eCSSProperty_speech_rate:        theAural->mSpeechRate = aValue;       break;
          case eCSSProperty_stress:             theAural->mStress = aValue;           break;
          case eCSSProperty_voice_family:       theAural->mVoiceFamily = aValue;      break;
          case eCSSProperty_volume:             theAural->mVolume = aValue;           break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

      // Shorthands
    case eCSSProperty_background:
    case eCSSProperty_border:
    case eCSSProperty_border_spacing:
    case eCSSProperty_clip:
    case eCSSProperty_cue:
    case eCSSProperty_font:
    case eCSSProperty_image_region:
    case eCSSProperty_list_style:
    case eCSSProperty_margin:
    case eCSSProperty__moz_outline:
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
      mOrder = CSS_NEW_NSVALUEARRAY;
    }
    else {
      // order IS important for CSS, so remove and add to the end
      mOrder->RemoveValue(aProperty);
    }
    if (nsnull != mOrder && eCSSUnit_Null != aValue.GetUnit()) {
      mOrder->AppendValue(aProperty);
    }
  }
  return result;
}

nsresult
nsCSSDeclaration::AppendStructValue(nsCSSProperty aProperty, void* aStruct)
{
  NS_ASSERTION(nsnull != aStruct, "must have struct");
  if (nsnull == aStruct) {
    return NS_ERROR_NULL_POINTER;
  }
  nsresult result = NS_OK;
  switch (aProperty) {
    case eCSSProperty_cursor: {
      CSS_ENSURE(UserInterface) {
        CSS_IF_DELETE(theUserInterface->mCursor);
        theUserInterface->mCursor = (nsCSSValueList*)aStruct;
      }
      break;
    }

    case eCSSProperty_text_shadow: {
      CSS_ENSURE(Text) {
        CSS_IF_DELETE(theText->mTextShadow);
        theText->mTextShadow = (nsCSSShadow*)aStruct;
      }
      break;
    }

    case eCSSProperty_content: {
      CSS_ENSURE(Content) {
        CSS_IF_DELETE(theContent->mContent);
        theContent->mContent = (nsCSSValueList*)aStruct;
      }
      break;
    }

    case eCSSProperty__moz_counter_increment: {
      CSS_ENSURE(Content) {
        CSS_IF_DELETE(theContent->mCounterIncrement);
        theContent->mCounterIncrement = (nsCSSCounterData*)aStruct;
      }
      break;
    }

    case eCSSProperty__moz_counter_reset: {
      CSS_ENSURE(Content) {
        CSS_IF_DELETE(theContent->mCounterReset);
        theContent->mCounterReset = (nsCSSCounterData*)aStruct;
      }
      break;
    }

    case eCSSProperty_quotes: {
      CSS_ENSURE(Content) {
        CSS_IF_DELETE(theContent->mQuotes);
        theContent->mQuotes = (nsCSSQuotes*)aStruct;
      }
      break;
    }

    case eCSSProperty_key_equivalent: {
      CSS_ENSURE(UserInterface) {
        CSS_IF_DELETE(theUserInterface->mKeyEquivalent);
        theUserInterface->mKeyEquivalent = (nsCSSValueList*)aStruct;
      }
      break;
    }

    case eCSSProperty_border_top_colors: {
      CSS_ENSURE(Margin) {
        theMargin->EnsureBorderColors();
        CSS_IF_DELETE(theMargin->mBorderColors[0]);
        theMargin->mBorderColors[0] = (nsCSSValueList*)aStruct;
      }
      break;
    }

    case eCSSProperty_border_right_colors: {
      CSS_ENSURE(Margin) {
        theMargin->EnsureBorderColors();
        CSS_IF_DELETE(theMargin->mBorderColors[1]);
        theMargin->mBorderColors[1] = (nsCSSValueList*)aStruct;
      }
      break;
    }

    case eCSSProperty_border_bottom_colors: {
      CSS_ENSURE(Margin) {
        theMargin->EnsureBorderColors();
        CSS_IF_DELETE(theMargin->mBorderColors[2]);
        theMargin->mBorderColors[2] = (nsCSSValueList*)aStruct;
      }
      break;
    }

    case eCSSProperty_border_left_colors: {
      CSS_ENSURE(Margin) {
        theMargin->EnsureBorderColors();
        CSS_IF_DELETE(theMargin->mBorderColors[3]);
        theMargin->mBorderColors[3] = (nsCSSValueList*)aStruct;
      }
      break;
    }

    default:
      NS_ERROR("not a struct property");
      result = NS_ERROR_ILLEGAL_VALUE;
      break;
  }

  if (NS_OK == result) {
    if (nsnull == mOrder) {
      mOrder = CSS_NEW_NSVALUEARRAY;
    }
    else {
      // order IS important for CSS, so remove and add to the end
      mOrder->RemoveValue(aProperty);
    }
    if (nsnull != mOrder) {
      mOrder->AppendValue(aProperty);
    }
  }
  return result;
}


#define CSS_ENSURE_IMPORTANT(data) \
  nsCSS##data* theImportant##data = nsnull; \
  if (0 == mImportant->mContains.mHas##data) { \
    nsCSS##data* newMe = new nsCSS##data(); \
    if (newMe) { \
      mImportant->mContains.mHas##data = 1; \
      PRBool insertRes = mImportant->mStructs.InsertElementAt((void*)newMe, CSSDECLIDX_##data(*mImportant)); \
      NS_ASSERTION(PR_FALSE != insertRes, "Unable to insert!"); \
      if (PR_FALSE == insertRes) { \
        delete newMe; \
        newMe = nsnull; \
        mImportant->mContains.mHas##data = 0; \
      } \
    } \
    theImportant##data = newMe; \
  } \
  else { \
    theImportant##data = (nsCSS##data*)mImportant->mStructs.ElementAt(CSSDECLIDX_##data(*mImportant)); \
  } \
  if (nsnull == theImportant##data) { \
    result = NS_ERROR_OUT_OF_MEMORY; \
  } \
  else

#define CSS_VARONSTACK_GET(type) \
  nsCSS##type* the##type = nsnull; \
  if(mContains.mHas##type) { \
    the##type = (nsCSS##type*)mStructs.ElementAt(CSSDECLIDX_##type(*this)); \
  }

#define CSS_CASE_IMPORTANT(prop,data,member) \
  case prop: \
    theImportant##data->member = the##data->member; \
    the##data->member.Reset(); \
    break

nsresult
nsCSSDeclaration::SetValueImportant(nsCSSProperty aProperty)
{
  nsresult result = NS_OK;

  if (nsnull == mImportant) {
    mImportant = new nsCSSDeclaration();
    if (nsnull == mImportant) {
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
      case eCSSProperty_font_stretch: {
        CSS_VARONSTACK_GET(Font);
        if (nsnull != theFont) {
          CSS_ENSURE_IMPORTANT(Font) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_font_family,      Font, mFamily);
              CSS_CASE_IMPORTANT(eCSSProperty_font_style,       Font, mStyle);
              CSS_CASE_IMPORTANT(eCSSProperty_font_variant,     Font, mVariant);
              CSS_CASE_IMPORTANT(eCSSProperty_font_weight,      Font, mWeight);
              CSS_CASE_IMPORTANT(eCSSProperty_font_size,        Font, mSize);
              CSS_CASE_IMPORTANT(eCSSProperty_font_size_adjust, Font, mSizeAdjust);
              CSS_CASE_IMPORTANT(eCSSProperty_font_stretch,     Font, mStretch);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }

      // nsCSSColor
      case eCSSProperty_color:
      case eCSSProperty_background_color:
      case eCSSProperty_background_image:
      case eCSSProperty_background_repeat:
      case eCSSProperty_background_attachment:
      case eCSSProperty_background_x_position:
      case eCSSProperty_background_y_position:
      case eCSSProperty__moz_background_clip:
      case eCSSProperty__moz_background_origin:
      case eCSSProperty__moz_background_inline_policy: {
        CSS_VARONSTACK_GET(Color);
        if (nsnull != theColor) {
          CSS_ENSURE_IMPORTANT(Color) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_color,                  Color, mColor);
              CSS_CASE_IMPORTANT(eCSSProperty_background_color,       Color, mBackColor);
              CSS_CASE_IMPORTANT(eCSSProperty_background_image,       Color, mBackImage);
              CSS_CASE_IMPORTANT(eCSSProperty_background_repeat,      Color, mBackRepeat);
              CSS_CASE_IMPORTANT(eCSSProperty_background_attachment,  Color, mBackAttachment);
              CSS_CASE_IMPORTANT(eCSSProperty_background_x_position,  Color, mBackPositionX);
              CSS_CASE_IMPORTANT(eCSSProperty_background_y_position,  Color, mBackPositionY);
              CSS_CASE_IMPORTANT(eCSSProperty__moz_background_clip,   Color, mBackClip);
              CSS_CASE_IMPORTANT(eCSSProperty__moz_background_origin, Color, mBackOrigin);
              CSS_CASE_IMPORTANT(eCSSProperty__moz_background_inline_policy, Color, mBackInlinePolicy);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }

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
      case eCSSProperty_white_space: {
        CSS_VARONSTACK_GET(Text);
        if (nsnull != theText) {
          CSS_ENSURE_IMPORTANT(Text) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_word_spacing,     Text, mWordSpacing);
              CSS_CASE_IMPORTANT(eCSSProperty_letter_spacing,   Text, mLetterSpacing);
              CSS_CASE_IMPORTANT(eCSSProperty_text_decoration,  Text, mDecoration);
              CSS_CASE_IMPORTANT(eCSSProperty_vertical_align,   Text, mVerticalAlign);
              CSS_CASE_IMPORTANT(eCSSProperty_text_transform,   Text, mTextTransform);
              CSS_CASE_IMPORTANT(eCSSProperty_text_align,       Text, mTextAlign);
              CSS_CASE_IMPORTANT(eCSSProperty_text_indent,      Text, mTextIndent);
              CSS_CASE_IMPORTANT(eCSSProperty_unicode_bidi,     Text, mUnicodeBidi);
              CSS_CASE_IMPORTANT(eCSSProperty_line_height,      Text, mLineHeight);
              CSS_CASE_IMPORTANT(eCSSProperty_white_space,      Text, mWhiteSpace);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }

      case eCSSProperty_text_shadow: {
        CSS_VARONSTACK_GET(Text);
        if (nsnull != theText) {
          if (nsnull != theText->mTextShadow) {
            CSS_ENSURE_IMPORTANT(Text) {
              CSS_IF_DELETE(theImportantText->mTextShadow);
              theImportantText->mTextShadow = theText->mTextShadow;
              theText->mTextShadow = nsnull;
            }
          }
        }
        break;
      }

      // nsCSSDisplay
      case eCSSProperty_appearance:
      case eCSSProperty_direction:
      case eCSSProperty_display:
      case eCSSProperty_binding:
      case eCSSProperty_float:
      case eCSSProperty_clear:
      case eCSSProperty_overflow:
      case eCSSProperty_visibility:
      case eCSSProperty_opacity:
      case eCSSProperty_position: {
        CSS_VARONSTACK_GET(Display);
        if (nsnull != theDisplay) {
          CSS_ENSURE_IMPORTANT(Display) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_appearance, Display, mAppearance);
              CSS_CASE_IMPORTANT(eCSSProperty_direction,  Display, mDirection);
              CSS_CASE_IMPORTANT(eCSSProperty_display,    Display, mDisplay);
              CSS_CASE_IMPORTANT(eCSSProperty_binding,    Display, mBinding);
              CSS_CASE_IMPORTANT(eCSSProperty_position,   Display, mPosition);
              CSS_CASE_IMPORTANT(eCSSProperty_float,      Display, mFloat);
              CSS_CASE_IMPORTANT(eCSSProperty_clear,      Display, mClear);
              CSS_CASE_IMPORTANT(eCSSProperty_overflow,   Display, mOverflow);
              CSS_CASE_IMPORTANT(eCSSProperty_visibility, Display, mVisibility);
              CSS_CASE_IMPORTANT(eCSSProperty_opacity,    Display, mOpacity);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }

      case eCSSProperty_clip_top:
      case eCSSProperty_clip_right:
      case eCSSProperty_clip_bottom:
      case eCSSProperty_clip_left: {
        CSS_VARONSTACK_GET(Display);
        if (nsnull != theDisplay) {
          if (nsnull != theDisplay->mClip) {
            CSS_ENSURE_IMPORTANT(Display) {
              CSS_ENSURE_RECT(theImportantDisplay->mClip) {
                switch(aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty_clip_top,     Display, mClip->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty_clip_right,   Display, mClip->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty_clip_bottom,  Display, mClip->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty_clip_left,    Display, mClip->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;
      }

      // nsCSSMargin
      case eCSSProperty_margin_top:
      case eCSSProperty_margin_right:
      case eCSSProperty_margin_bottom:
      case eCSSProperty_margin_left: {
        CSS_VARONSTACK_GET(Margin);
        if (nsnull != theMargin) {
          if (nsnull != theMargin->mMargin) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(theImportantMargin->mMargin) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty_margin_top,     Margin, mMargin->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty_margin_right,   Margin, mMargin->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty_margin_bottom,  Margin, mMargin->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty_margin_left,    Margin, mMargin->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;
      }

      case eCSSProperty_padding_top:
      case eCSSProperty_padding_right:
      case eCSSProperty_padding_bottom:
      case eCSSProperty_padding_left: {
        CSS_VARONSTACK_GET(Margin);
        if (nsnull != theMargin) {
          if (nsnull != theMargin->mPadding) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(theImportantMargin->mPadding) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty_padding_top,    Margin, mPadding->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty_padding_right,  Margin, mPadding->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty_padding_bottom, Margin, mPadding->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty_padding_left,   Margin, mPadding->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;
      }

      case eCSSProperty_border_top_width:
      case eCSSProperty_border_right_width:
      case eCSSProperty_border_bottom_width:
      case eCSSProperty_border_left_width: {
        CSS_VARONSTACK_GET(Margin);
        if (nsnull != theMargin) {
          if (nsnull != theMargin->mBorderWidth) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(theImportantMargin->mBorderWidth) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty_border_top_width,     Margin, mBorderWidth->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_right_width,   Margin, mBorderWidth->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_bottom_width,  Margin, mBorderWidth->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_left_width,    Margin, mBorderWidth->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;
      }

      case eCSSProperty_border_top_color:
      case eCSSProperty_border_right_color:
      case eCSSProperty_border_bottom_color:
      case eCSSProperty_border_left_color: {
        CSS_VARONSTACK_GET(Margin);
        if (nsnull != theMargin) {
          if (nsnull != theMargin->mBorderColor) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(theImportantMargin->mBorderColor) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty_border_top_color,     Margin, mBorderColor->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_right_color,   Margin, mBorderColor->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_bottom_color,  Margin, mBorderColor->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_left_color,    Margin, mBorderColor->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;
      }

      case eCSSProperty_border_top_colors: {
        CSS_VARONSTACK_GET(Margin);
        if (nsnull != theMargin && theMargin->mBorderColors) {
          CSS_ENSURE_IMPORTANT(Margin) {
            theImportantMargin->EnsureBorderColors();
            CSS_IF_DELETE(theImportantMargin->mBorderColors[0]);
            theImportantMargin->mBorderColors[0] = theMargin->mBorderColors[0];
            theMargin->mBorderColors[0] = nsnull;
          }
        }
        break;
      }

      case eCSSProperty_border_right_colors: {
        CSS_VARONSTACK_GET(Margin);
        if (nsnull != theMargin && theMargin->mBorderColors) {
          CSS_ENSURE_IMPORTANT(Margin) {
            theImportantMargin->EnsureBorderColors();
            CSS_IF_DELETE(theImportantMargin->mBorderColors[1]);
            theImportantMargin->mBorderColors[1] = theMargin->mBorderColors[1];
            theMargin->mBorderColors[1] = nsnull;
          }
        }
        break;
      }

      case eCSSProperty_border_bottom_colors: {
        CSS_VARONSTACK_GET(Margin);
        if (nsnull != theMargin && theMargin->mBorderColors) {
          CSS_ENSURE_IMPORTANT(Margin) {
            theImportantMargin->EnsureBorderColors();
            CSS_IF_DELETE(theImportantMargin->mBorderColors[2]);
            theImportantMargin->mBorderColors[2] = theMargin->mBorderColors[2];
            theMargin->mBorderColors[2] = nsnull;
          }
        }
        break;
      }

      case eCSSProperty_border_left_colors: {
        CSS_VARONSTACK_GET(Margin);
        if (nsnull != theMargin && theMargin->mBorderColors) {
          CSS_ENSURE_IMPORTANT(Margin) {
            theImportantMargin->EnsureBorderColors();
            CSS_IF_DELETE(theImportantMargin->mBorderColors[3]);
            theImportantMargin->mBorderColors[3] = theMargin->mBorderColors[3];
            theMargin->mBorderColors[3] = nsnull;
          }
        }
        break;
      }

      case eCSSProperty_border_top_style:
      case eCSSProperty_border_right_style:
      case eCSSProperty_border_bottom_style:
      case eCSSProperty_border_left_style: {
        CSS_VARONSTACK_GET(Margin);
        if (nsnull != theMargin) {
          if (nsnull != theMargin->mBorderStyle) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(theImportantMargin->mBorderStyle) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty_border_top_style,     Margin, mBorderStyle->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_right_style,   Margin, mBorderStyle->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_bottom_style,  Margin, mBorderStyle->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty_border_left_style,    Margin, mBorderStyle->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;
      }

      case eCSSProperty__moz_border_radius_topLeft:
      case eCSSProperty__moz_border_radius_topRight:
      case eCSSProperty__moz_border_radius_bottomRight:
      case eCSSProperty__moz_border_radius_bottomLeft: {
        CSS_VARONSTACK_GET(Margin);
        if (nsnull != theMargin) {
          if (nsnull != theMargin->mBorderRadius) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(theImportantMargin->mBorderRadius) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_border_radius_topLeft,			Margin, mBorderRadius->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_border_radius_topRight,		Margin, mBorderRadius->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_border_radius_bottomRight,	Margin, mBorderRadius->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_border_radius_bottomLeft,	Margin, mBorderRadius->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;
      }

      case eCSSProperty__moz_outline_radius_topLeft:
      case eCSSProperty__moz_outline_radius_topRight:
      case eCSSProperty__moz_outline_radius_bottomRight:
      case eCSSProperty__moz_outline_radius_bottomLeft: {
        CSS_VARONSTACK_GET(Margin);
        if (nsnull != theMargin) {
          if (nsnull != theMargin->mOutlineRadius) {
            CSS_ENSURE_IMPORTANT(Margin) {
              CSS_ENSURE_RECT(theImportantMargin->mOutlineRadius) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_outline_radius_topLeft,			Margin, mOutlineRadius->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_outline_radius_topRight,			Margin, mOutlineRadius->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_outline_radius_bottomRight,	Margin, mOutlineRadius->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty__moz_outline_radius_bottomLeft,		Margin, mOutlineRadius->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;
      }

      case eCSSProperty__moz_outline_width:
      case eCSSProperty__moz_outline_color:
      case eCSSProperty__moz_outline_style:
      case eCSSProperty_float_edge: {
        CSS_VARONSTACK_GET(Margin);
        if (nsnull != theMargin) {
          CSS_ENSURE_IMPORTANT(Margin) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty__moz_outline_width, Margin, mOutlineWidth);
              CSS_CASE_IMPORTANT(eCSSProperty__moz_outline_color, Margin, mOutlineColor);
              CSS_CASE_IMPORTANT(eCSSProperty__moz_outline_style, Margin, mOutlineStyle);
              CSS_CASE_IMPORTANT(eCSSProperty_float_edge,         Margin, mFloatEdge);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }

      // nsCSSPosition
      case eCSSProperty_width:
      case eCSSProperty_min_width:
      case eCSSProperty_max_width:
      case eCSSProperty_height:
      case eCSSProperty_min_height:
      case eCSSProperty_max_height:
      case eCSSProperty_box_sizing:
      case eCSSProperty_z_index: {
        CSS_VARONSTACK_GET(Position);
        if (nsnull != thePosition) {
          CSS_ENSURE_IMPORTANT(Position) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_width,      Position, mWidth);
              CSS_CASE_IMPORTANT(eCSSProperty_min_width,  Position, mMinWidth);
              CSS_CASE_IMPORTANT(eCSSProperty_max_width,  Position, mMaxWidth);
              CSS_CASE_IMPORTANT(eCSSProperty_height,     Position, mHeight);
              CSS_CASE_IMPORTANT(eCSSProperty_min_height, Position, mMinHeight);
              CSS_CASE_IMPORTANT(eCSSProperty_max_height, Position, mMaxHeight);
              CSS_CASE_IMPORTANT(eCSSProperty_box_sizing, Position, mBoxSizing);
              CSS_CASE_IMPORTANT(eCSSProperty_z_index,    Position, mZIndex);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }

      case eCSSProperty_top:
      case eCSSProperty_right:
      case eCSSProperty_bottom:
      case eCSSProperty_left: {
        CSS_VARONSTACK_GET(Position);
        if (nsnull != thePosition) {
          if (nsnull != thePosition->mOffset) {
            CSS_ENSURE_IMPORTANT(Position) {
              CSS_ENSURE_RECT(theImportantPosition->mOffset) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty_top,    Position, mOffset->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty_right,  Position, mOffset->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty_bottom, Position, mOffset->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty_left,   Position, mOffset->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;
      }

        // nsCSSList
      case eCSSProperty_list_style_type:
      case eCSSProperty_list_style_image:
      case eCSSProperty_list_style_position: {
        CSS_VARONSTACK_GET(List);
        if (nsnull != theList) {
          CSS_ENSURE_IMPORTANT(List) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_list_style_type,      List, mType);
              CSS_CASE_IMPORTANT(eCSSProperty_list_style_image,     List, mImage);
              CSS_CASE_IMPORTANT(eCSSProperty_list_style_position,  List, mPosition);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }

      case eCSSProperty_image_region_top:
      case eCSSProperty_image_region_right:
      case eCSSProperty_image_region_bottom:
      case eCSSProperty_image_region_left: {
        CSS_VARONSTACK_GET(List);
        if (nsnull != theList) {
          if (theList->mImageRegion) {
            CSS_ENSURE_IMPORTANT(List) {
              CSS_ENSURE_RECT(theImportantList->mImageRegion) {
                switch (aProperty) {
                  CSS_CASE_IMPORTANT(eCSSProperty_image_region_top,    List, mImageRegion->mTop);
                  CSS_CASE_IMPORTANT(eCSSProperty_image_region_right,  List, mImageRegion->mRight);
                  CSS_CASE_IMPORTANT(eCSSProperty_image_region_bottom, List, mImageRegion->mBottom);
                  CSS_CASE_IMPORTANT(eCSSProperty_image_region_left,   List, mImageRegion->mLeft);
                  CSS_BOGUS_DEFAULT; // make compiler happy
                }
              }
            }
          }
        }
        break;
      }

        // nsCSSTable
      case eCSSProperty_border_collapse:
      case eCSSProperty_border_x_spacing:
      case eCSSProperty_border_y_spacing:
      case eCSSProperty_caption_side:
      case eCSSProperty_empty_cells:
      case eCSSProperty_table_layout: {
        CSS_VARONSTACK_GET(Table);
        if (nsnull != theTable) {
          CSS_ENSURE_IMPORTANT(Table) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_border_collapse,  Table, mBorderCollapse);
              CSS_CASE_IMPORTANT(eCSSProperty_border_x_spacing, Table, mBorderSpacingX);
              CSS_CASE_IMPORTANT(eCSSProperty_border_y_spacing, Table, mBorderSpacingY);
              CSS_CASE_IMPORTANT(eCSSProperty_caption_side,     Table, mCaptionSide);
              CSS_CASE_IMPORTANT(eCSSProperty_empty_cells,      Table, mEmptyCells);
              CSS_CASE_IMPORTANT(eCSSProperty_table_layout,     Table, mLayout);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }

        // nsCSSBreaks
      case eCSSProperty_orphans:
      case eCSSProperty_widows:
      case eCSSProperty_page: {
        CSS_VARONSTACK_GET(Breaks);
        if (nsnull != theBreaks) {
          CSS_ENSURE_IMPORTANT(Breaks) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_orphans,            Breaks, mOrphans);
              CSS_CASE_IMPORTANT(eCSSProperty_widows,             Breaks, mWidows);
              CSS_CASE_IMPORTANT(eCSSProperty_page,               Breaks, mPage);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }
      case eCSSProperty_page_break_after:
      case eCSSProperty_page_break_before:
      case eCSSProperty_page_break_inside: {
        // temp fix for bug 24000
        CSS_VARONSTACK_GET(Display);
        if (theDisplay) {
          CSS_ENSURE_IMPORTANT(Display) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_page_break_after,   Display, mBreakAfter);
              CSS_CASE_IMPORTANT(eCSSProperty_page_break_before,  Display, mBreakBefore);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }

        // nsCSSPage
      case eCSSProperty_marks:
      case eCSSProperty_size_width:
      case eCSSProperty_size_height: {
        CSS_VARONSTACK_GET(Page);
        if (nsnull != thePage) {
          CSS_ENSURE_IMPORTANT(Page) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_marks,        Page, mMarks);
              CSS_CASE_IMPORTANT(eCSSProperty_size_width,   Page, mSizeWidth);
              CSS_CASE_IMPORTANT(eCSSProperty_size_height,  Page, mSizeHeight);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }

        // nsCSSContent
      case eCSSProperty_content: {
        CSS_VARONSTACK_GET(Content);
        if (nsnull != theContent) {
          if (nsnull != theContent->mContent) {
            CSS_ENSURE_IMPORTANT(Content) {
              CSS_IF_DELETE(theImportantContent->mContent);
              theImportantContent->mContent = theContent->mContent;
              theContent->mContent = nsnull;
            }
          }
        }
        break;
      }

      case eCSSProperty__moz_counter_increment: {
        CSS_VARONSTACK_GET(Content);
        if (nsnull != theContent) {
          if (nsnull != theContent->mCounterIncrement) {
            CSS_ENSURE_IMPORTANT(Content) {
              CSS_IF_DELETE(theImportantContent->mCounterIncrement);
              theImportantContent->mCounterIncrement = theContent->mCounterIncrement;
              theContent->mCounterIncrement = nsnull;
            }
          }
        }
        break;
      }

      case eCSSProperty__moz_counter_reset: {
        CSS_VARONSTACK_GET(Content);
        if (nsnull != theContent) {
          if (nsnull != theContent->mCounterReset) {
            CSS_ENSURE_IMPORTANT(Content) {
              CSS_IF_DELETE(theImportantContent->mCounterReset);
              theImportantContent->mCounterReset = theContent->mCounterReset;
              theContent->mCounterReset = nsnull;
            }
          }
        }
        break;
      }

      case eCSSProperty_marker_offset: {
        CSS_VARONSTACK_GET(Content);
        if (nsnull != theContent) {
          CSS_ENSURE_IMPORTANT(Content) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_marker_offset,  Content, mMarkerOffset);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }

      case eCSSProperty_quotes: {
        CSS_VARONSTACK_GET(Content);
        if (nsnull != theContent) {
          if (nsnull != theContent->mQuotes) {
            CSS_ENSURE_IMPORTANT(Content) {
              CSS_IF_DELETE(theImportantContent->mQuotes);
              theImportantContent->mQuotes = theContent->mQuotes;
              theContent->mQuotes = nsnull;
            }
          }
        }
        break;
      }

      // nsCSSUserInterface
      case eCSSProperty_user_input:
      case eCSSProperty_user_modify:
      case eCSSProperty_user_select:
      case eCSSProperty_user_focus:
      case eCSSProperty_resizer: {
        CSS_VARONSTACK_GET(UserInterface);
        if (nsnull != theUserInterface) {
          CSS_ENSURE_IMPORTANT(UserInterface) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_user_input,   UserInterface, mUserInput);
              CSS_CASE_IMPORTANT(eCSSProperty_user_modify,  UserInterface, mUserModify);
              CSS_CASE_IMPORTANT(eCSSProperty_user_select,  UserInterface, mUserSelect);
              CSS_CASE_IMPORTANT(eCSSProperty_user_focus,   UserInterface, mUserFocus);
              CSS_CASE_IMPORTANT(eCSSProperty_resizer,      UserInterface, mResizer);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }

      case eCSSProperty_key_equivalent: {
        CSS_VARONSTACK_GET(UserInterface);
        if (nsnull != theUserInterface) {
          if (nsnull != theUserInterface->mKeyEquivalent) {
            CSS_ENSURE_IMPORTANT(UserInterface) {
              CSS_IF_DELETE(theImportantUserInterface->mKeyEquivalent);
              theImportantUserInterface->mKeyEquivalent = theUserInterface->mKeyEquivalent;
              theUserInterface->mKeyEquivalent = nsnull;
            }
          }
        }
        break;
      }

      case eCSSProperty_cursor: {
        CSS_VARONSTACK_GET(UserInterface);
        if (nsnull != theUserInterface) {
          if (nsnull != theUserInterface->mCursor) {
            CSS_ENSURE_IMPORTANT(UserInterface) {
              CSS_IF_DELETE(theImportantUserInterface->mCursor);
              theImportantUserInterface->mCursor = theUserInterface->mCursor;
              theUserInterface->mCursor = nsnull;
            }
          }
        }
        break;
      }

#ifdef INCLUDE_XUL
      // nsCSSXUL
      case eCSSProperty_box_align:
      case eCSSProperty_box_direction:
      case eCSSProperty_box_flex:
      case eCSSProperty_box_orient:
      case eCSSProperty_box_pack:
      case eCSSProperty_box_ordinal_group: {
        CSS_VARONSTACK_GET(XUL);
        if (nsnull != theXUL) {
          CSS_ENSURE_IMPORTANT(XUL) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_box_align,         XUL, mBoxAlign);
              CSS_CASE_IMPORTANT(eCSSProperty_box_direction,     XUL, mBoxDirection);
              CSS_CASE_IMPORTANT(eCSSProperty_box_flex,          XUL, mBoxFlex);
              CSS_CASE_IMPORTANT(eCSSProperty_box_orient,        XUL, mBoxOrient);
              CSS_CASE_IMPORTANT(eCSSProperty_box_pack,          XUL, mBoxPack);
              CSS_CASE_IMPORTANT(eCSSProperty_box_ordinal_group, XUL, mBoxOrdinal);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }
#endif

#ifdef MOZ_SVG
      // nsCSSSVG
      case eCSSProperty_fill:
      case eCSSProperty_fill_opacity:
      case eCSSProperty_fill_rule:
      case eCSSProperty_stroke:
      case eCSSProperty_stroke_dasharray:
      case eCSSProperty_stroke_dashoffset:
      case eCSSProperty_stroke_linecap:
      case eCSSProperty_stroke_linejoin:
      case eCSSProperty_stroke_miterlimit:
      case eCSSProperty_stroke_opacity:
      case eCSSProperty_stroke_width: {
        CSS_VARONSTACK_GET(SVG);
        if (nsnull != theSVG) {
          CSS_ENSURE_IMPORTANT(SVG) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_fill,              SVG, mFill);
              CSS_CASE_IMPORTANT(eCSSProperty_fill_opacity,      SVG, mFillOpacity);
              CSS_CASE_IMPORTANT(eCSSProperty_fill_rule,         SVG, mFillRule);
              CSS_CASE_IMPORTANT(eCSSProperty_stroke,            SVG, mStroke);
              CSS_CASE_IMPORTANT(eCSSProperty_stroke_dasharray,  SVG, mStrokeDasharray);
              CSS_CASE_IMPORTANT(eCSSProperty_stroke_dashoffset, SVG, mStrokeDashoffset);
              CSS_CASE_IMPORTANT(eCSSProperty_stroke_linecap,    SVG, mStrokeLinecap);
              CSS_CASE_IMPORTANT(eCSSProperty_stroke_linejoin,   SVG, mStrokeLinejoin);
              CSS_CASE_IMPORTANT(eCSSProperty_stroke_miterlimit, SVG, mStrokeMiterlimit);
              CSS_CASE_IMPORTANT(eCSSProperty_stroke_opacity,    SVG, mStrokeOpacity);
              CSS_CASE_IMPORTANT(eCSSProperty_stroke_width,      SVG, mStrokeWidth);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }
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
      case eCSSProperty_volume: {
        CSS_VARONSTACK_GET(Aural);
        if (nsnull != theAural) {
          CSS_ENSURE_IMPORTANT(Aural) {
            switch (aProperty) {
              CSS_CASE_IMPORTANT(eCSSProperty_azimuth,            Aural, mAzimuth);
              CSS_CASE_IMPORTANT(eCSSProperty_elevation,          Aural, mElevation);
              CSS_CASE_IMPORTANT(eCSSProperty_cue_after,          Aural, mCueAfter);
              CSS_CASE_IMPORTANT(eCSSProperty_cue_before,         Aural, mCueBefore);
              CSS_CASE_IMPORTANT(eCSSProperty_pause_after,        Aural, mPauseAfter);
              CSS_CASE_IMPORTANT(eCSSProperty_pause_before,       Aural, mPauseBefore);
              CSS_CASE_IMPORTANT(eCSSProperty_pitch,              Aural, mPitch);
              CSS_CASE_IMPORTANT(eCSSProperty_pitch_range,        Aural, mPitchRange);
              CSS_CASE_IMPORTANT(eCSSProperty_richness,           Aural, mRichness);
              CSS_CASE_IMPORTANT(eCSSProperty_speak,              Aural, mSpeak);
              CSS_CASE_IMPORTANT(eCSSProperty_speak_header,       Aural, mSpeakHeader);
              CSS_CASE_IMPORTANT(eCSSProperty_speak_numeral,      Aural, mSpeakNumeral);
              CSS_CASE_IMPORTANT(eCSSProperty_speak_punctuation,  Aural, mSpeakPunctuation);
              CSS_CASE_IMPORTANT(eCSSProperty_speech_rate,        Aural, mSpeechRate);
              CSS_CASE_IMPORTANT(eCSSProperty_stress,             Aural, mStress);
              CSS_CASE_IMPORTANT(eCSSProperty_voice_family,       Aural, mVoiceFamily);
              CSS_CASE_IMPORTANT(eCSSProperty_volume,             Aural, mVolume);
              CSS_BOGUS_DEFAULT; // make compiler happy
            }
          }
        }
        break;
      }

      case eCSSProperty_play_during: {
        CSS_VARONSTACK_GET(Aural);
        if (nsnull != theAural) {
          CSS_ENSURE_IMPORTANT(Aural) {
            theImportantAural->mPlayDuring = theAural->mPlayDuring;
            theAural->mPlayDuring.Reset();
            theImportantAural->mPlayDuringFlags = theAural->mPlayDuringFlags;
            theAural->mPlayDuringFlags.Reset();
          }
        }
        break;
      }

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
      case eCSSProperty_image_region:
        SetValueImportant(eCSSProperty_image_region_top);
        SetValueImportant(eCSSProperty_image_region_right);
        SetValueImportant(eCSSProperty_image_region_bottom);
        SetValueImportant(eCSSProperty_image_region_left);
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
      case eCSSProperty__moz_outline:
        SetValueImportant(eCSSProperty__moz_outline_color);
        SetValueImportant(eCSSProperty__moz_outline_style);
        SetValueImportant(eCSSProperty__moz_outline_width);
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


#define CSS_CHECK(data) \
  CSS_VARONSTACK_GET(data); \
  if (nsnull == the##data) { \
    result = NS_ERROR_NOT_AVAILABLE; \
  } \
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
nsCSSDeclaration::RemoveProperty(nsCSSProperty aProperty)
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
    case eCSSProperty_font_stretch: {
      CSS_CHECK(Font) {
        switch (aProperty) {
          case eCSSProperty_font_family:      theFont->mFamily.Reset();      break;
          case eCSSProperty_font_style:       theFont->mStyle.Reset();       break;
          case eCSSProperty_font_variant:     theFont->mVariant.Reset();     break;
          case eCSSProperty_font_weight:      theFont->mWeight.Reset();      break;
          case eCSSProperty_font_size:        theFont->mSize.Reset();        break;
          case eCSSProperty_font_size_adjust: theFont->mSizeAdjust.Reset();  break;
          case eCSSProperty_font_stretch:     theFont->mStretch.Reset();     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

    // nsCSSColor
    case eCSSProperty_color:
    case eCSSProperty_background_color:
    case eCSSProperty_background_image:
    case eCSSProperty_background_repeat:
    case eCSSProperty_background_attachment:
    case eCSSProperty_background_x_position:
    case eCSSProperty_background_y_position:
    case eCSSProperty__moz_background_clip:
    case eCSSProperty__moz_background_origin:
    case eCSSProperty__moz_background_inline_policy: {
      CSS_CHECK(Color) {
        switch (aProperty) {
          case eCSSProperty_color:                  theColor->mColor.Reset();           break;
          case eCSSProperty_background_color:       theColor->mBackColor.Reset();       break;
          case eCSSProperty_background_image:       theColor->mBackImage.Reset();       break;
          case eCSSProperty_background_repeat:      theColor->mBackRepeat.Reset();      break;
          case eCSSProperty_background_attachment:  theColor->mBackAttachment.Reset();  break;
          case eCSSProperty_background_x_position:  theColor->mBackPositionX.Reset();   break;
          case eCSSProperty_background_y_position:  theColor->mBackPositionY.Reset();   break;
          case eCSSProperty__moz_background_clip:   theColor->mBackClip.Reset();        break;
          case eCSSProperty__moz_background_origin: theColor->mBackOrigin.Reset();      break;
          case eCSSProperty__moz_background_inline_policy: theColor->mBackInlinePolicy.Reset(); break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

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
    case eCSSProperty_white_space: {
      CSS_CHECK(Text) {
        switch (aProperty) {
          case eCSSProperty_word_spacing:     theText->mWordSpacing.Reset();    break;
          case eCSSProperty_letter_spacing:   theText->mLetterSpacing.Reset();  break;
          case eCSSProperty_text_decoration:  theText->mDecoration.Reset();     break;
          case eCSSProperty_vertical_align:   theText->mVerticalAlign.Reset();  break;
          case eCSSProperty_text_transform:   theText->mTextTransform.Reset();  break;
          case eCSSProperty_text_align:       theText->mTextAlign.Reset();      break;
          case eCSSProperty_text_indent:      theText->mTextIndent.Reset();     break;
          case eCSSProperty_unicode_bidi:     theText->mUnicodeBidi.Reset();    break;
          case eCSSProperty_line_height:      theText->mLineHeight.Reset();     break;
          case eCSSProperty_white_space:      theText->mWhiteSpace.Reset();     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

    case eCSSProperty_text_shadow_color:
    case eCSSProperty_text_shadow_radius:
    case eCSSProperty_text_shadow_x:
    case eCSSProperty_text_shadow_y: {
      CSS_CHECK(Text) {
        CSS_CHECK_DATA(theText->mTextShadow, nsCSSShadow) {
          switch (aProperty) {
            case eCSSProperty_text_shadow_color:  theText->mTextShadow->mColor.Reset();    break;
            case eCSSProperty_text_shadow_radius: theText->mTextShadow->mRadius.Reset();   break;
            case eCSSProperty_text_shadow_x:      theText->mTextShadow->mXOffset.Reset();  break;
            case eCSSProperty_text_shadow_y:      theText->mTextShadow->mYOffset.Reset();  break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
          CSS_IF_DELETE(theText->mTextShadow->mNext);
        }
      }
      break;
    }

    // nsCSSDisplay
    case eCSSProperty_appearance:
    case eCSSProperty_float:
    case eCSSProperty_clear:
    case eCSSProperty_display:
    case eCSSProperty_binding:
    case eCSSProperty_position:
    case eCSSProperty_direction:
    case eCSSProperty_visibility:
    case eCSSProperty_opacity:
    case eCSSProperty_overflow: {
      CSS_CHECK(Display) {
        switch (aProperty) {
          case eCSSProperty_appearance: theDisplay->mAppearance.Reset(); break;
          case eCSSProperty_float:      theDisplay->mFloat.Reset();      break;
          case eCSSProperty_clear:      theDisplay->mClear.Reset();      break;
          case eCSSProperty_display:    theDisplay->mDisplay.Reset();    break;
          case eCSSProperty_position:   theDisplay->mPosition.Reset();   break;
          case eCSSProperty_binding:         
            theDisplay->mBinding.Reset();      
            break;
          case eCSSProperty_direction:  theDisplay->mDirection.Reset();  break;
          case eCSSProperty_visibility: theDisplay->mVisibility.Reset(); break;
          case eCSSProperty_opacity:    theDisplay->mOpacity.Reset();    break;
          case eCSSProperty_overflow:   theDisplay->mOverflow.Reset();   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

    case eCSSProperty_clip_top:
    case eCSSProperty_clip_right:
    case eCSSProperty_clip_bottom:
    case eCSSProperty_clip_left: {
      CSS_CHECK(Display) {
        CSS_CHECK_RECT(theDisplay->mClip) {
          switch(aProperty) {
            case eCSSProperty_clip_top:     theDisplay->mClip->mTop.Reset();     break;
            case eCSSProperty_clip_right:   theDisplay->mClip->mRight.Reset();   break;
            case eCSSProperty_clip_bottom:  theDisplay->mClip->mBottom.Reset();  break;
            case eCSSProperty_clip_left:    theDisplay->mClip->mLeft.Reset();    break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    // nsCSSMargin
    case eCSSProperty_margin_top:
    case eCSSProperty_margin_right:
    case eCSSProperty_margin_bottom:
    case eCSSProperty_margin_left: {
      CSS_CHECK(Margin) {
        CSS_CHECK_RECT(theMargin->mMargin) {
          switch (aProperty) {
            case eCSSProperty_margin_top:     theMargin->mMargin->mTop.Reset();     break;
            case eCSSProperty_margin_right:   theMargin->mMargin->mRight.Reset();   break;
            case eCSSProperty_margin_bottom:  theMargin->mMargin->mBottom.Reset();  break;
            case eCSSProperty_margin_left:    theMargin->mMargin->mLeft.Reset();    break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    case eCSSProperty_padding_top:
    case eCSSProperty_padding_right:
    case eCSSProperty_padding_bottom:
    case eCSSProperty_padding_left: {
      CSS_CHECK(Margin) {
        CSS_CHECK_RECT(theMargin->mPadding) {
          switch (aProperty) {
            case eCSSProperty_padding_top:    theMargin->mPadding->mTop.Reset();    break;
            case eCSSProperty_padding_right:  theMargin->mPadding->mRight.Reset();  break;
            case eCSSProperty_padding_bottom: theMargin->mPadding->mBottom.Reset(); break;
            case eCSSProperty_padding_left:   theMargin->mPadding->mLeft.Reset();   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    case eCSSProperty_border_top_width:
    case eCSSProperty_border_right_width:
    case eCSSProperty_border_bottom_width:
    case eCSSProperty_border_left_width: {
      CSS_CHECK(Margin) {
        CSS_CHECK_RECT(theMargin->mBorderWidth) {
          switch (aProperty) {
            case eCSSProperty_border_top_width:     theMargin->mBorderWidth->mTop.Reset();     break;
            case eCSSProperty_border_right_width:   theMargin->mBorderWidth->mRight.Reset();   break;
            case eCSSProperty_border_bottom_width:  theMargin->mBorderWidth->mBottom.Reset();  break;
            case eCSSProperty_border_left_width:    theMargin->mBorderWidth->mLeft.Reset();    break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    case eCSSProperty_border_top_color:
    case eCSSProperty_border_right_color:
    case eCSSProperty_border_bottom_color:
    case eCSSProperty_border_left_color: {
      CSS_CHECK(Margin) {
        CSS_CHECK_RECT(theMargin->mBorderColor) {
          switch (aProperty) {
            case eCSSProperty_border_top_color:     theMargin->mBorderColor->mTop.Reset();    break;
            case eCSSProperty_border_right_color:   theMargin->mBorderColor->mRight.Reset();  break;
            case eCSSProperty_border_bottom_color:  theMargin->mBorderColor->mBottom.Reset(); break;
            case eCSSProperty_border_left_color:    theMargin->mBorderColor->mLeft.Reset();   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    case eCSSProperty_border_top_colors:
    case eCSSProperty_border_right_colors:
    case eCSSProperty_border_bottom_colors:
    case eCSSProperty_border_left_colors: {
      CSS_CHECK(Margin) {
        if (theMargin->mBorderColors) {
          switch (aProperty) {
            case eCSSProperty_border_top_colors:     CSS_IF_DELETE(theMargin->mBorderColors[0]);    break;
            case eCSSProperty_border_right_colors:   CSS_IF_DELETE(theMargin->mBorderColors[1]);  break;
            case eCSSProperty_border_bottom_colors:  CSS_IF_DELETE(theMargin->mBorderColors[2]); break;
            case eCSSProperty_border_left_colors:    CSS_IF_DELETE(theMargin->mBorderColors[3]);   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    case eCSSProperty_border_top_style:
    case eCSSProperty_border_right_style:
    case eCSSProperty_border_bottom_style:
    case eCSSProperty_border_left_style: {
      CSS_CHECK(Margin) {
        CSS_CHECK_RECT(theMargin->mBorderStyle) {
          switch (aProperty) {
            case eCSSProperty_border_top_style:     theMargin->mBorderStyle->mTop.Reset();    break;
            case eCSSProperty_border_right_style:   theMargin->mBorderStyle->mRight.Reset();  break;
            case eCSSProperty_border_bottom_style:  theMargin->mBorderStyle->mBottom.Reset(); break;
            case eCSSProperty_border_left_style:    theMargin->mBorderStyle->mLeft.Reset();   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    case eCSSProperty__moz_border_radius_topLeft:
    case eCSSProperty__moz_border_radius_topRight:
    case eCSSProperty__moz_border_radius_bottomRight:
    case eCSSProperty__moz_border_radius_bottomLeft: {
      CSS_CHECK(Margin) {
        CSS_CHECK_RECT(theMargin->mBorderRadius) {
          switch (aProperty) {
            case eCSSProperty__moz_border_radius_topLeft:			theMargin->mBorderRadius->mTop.Reset();    break;
            case eCSSProperty__moz_border_radius_topRight:		theMargin->mBorderRadius->mRight.Reset();  break;
            case eCSSProperty__moz_border_radius_bottomRight:	theMargin->mBorderRadius->mBottom.Reset(); break;
            case eCSSProperty__moz_border_radius_bottomLeft:	theMargin->mBorderRadius->mLeft.Reset();   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    case eCSSProperty__moz_outline_radius_topLeft:
    case eCSSProperty__moz_outline_radius_topRight:
    case eCSSProperty__moz_outline_radius_bottomRight:
    case eCSSProperty__moz_outline_radius_bottomLeft: {
      CSS_CHECK(Margin) {
        CSS_CHECK_RECT(theMargin->mOutlineRadius) {
          switch (aProperty) {
            case eCSSProperty__moz_outline_radius_topLeft:			theMargin->mOutlineRadius->mTop.Reset();    break;
            case eCSSProperty__moz_outline_radius_topRight:			theMargin->mOutlineRadius->mRight.Reset();  break;
            case eCSSProperty__moz_outline_radius_bottomRight:	theMargin->mOutlineRadius->mBottom.Reset(); break;
            case eCSSProperty__moz_outline_radius_bottomLeft:		theMargin->mOutlineRadius->mLeft.Reset();   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

    case eCSSProperty__moz_outline_width:
    case eCSSProperty__moz_outline_color:
    case eCSSProperty__moz_outline_style:
    case eCSSProperty_float_edge: {
      CSS_CHECK(Margin) {
        switch (aProperty) {
          case eCSSProperty__moz_outline_width: theMargin->mOutlineWidth.Reset();  break;
          case eCSSProperty__moz_outline_color: theMargin->mOutlineColor.Reset();  break;
          case eCSSProperty__moz_outline_style: theMargin->mOutlineStyle.Reset();  break;
          case eCSSProperty_float_edge:         theMargin->mFloatEdge.Reset();     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

    // nsCSSPosition
    case eCSSProperty_width:
    case eCSSProperty_min_width:
    case eCSSProperty_max_width:
    case eCSSProperty_height:
    case eCSSProperty_min_height:
    case eCSSProperty_max_height:
    case eCSSProperty_box_sizing:
    case eCSSProperty_z_index: {
      CSS_CHECK(Position) {
        switch (aProperty) {
          case eCSSProperty_width:      thePosition->mWidth.Reset();      break;
          case eCSSProperty_min_width:  thePosition->mMinWidth.Reset();   break;
          case eCSSProperty_max_width:  thePosition->mMaxWidth.Reset();   break;
          case eCSSProperty_height:     thePosition->mHeight.Reset();     break;
          case eCSSProperty_min_height: thePosition->mMinHeight.Reset();  break;
          case eCSSProperty_max_height: thePosition->mMaxHeight.Reset();  break;
          case eCSSProperty_box_sizing: thePosition->mBoxSizing.Reset();  break;
          case eCSSProperty_z_index:    thePosition->mZIndex.Reset();     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

    case eCSSProperty_top:
    case eCSSProperty_right:
    case eCSSProperty_bottom:
    case eCSSProperty_left: {
      CSS_CHECK(Position) {
        CSS_CHECK_RECT(thePosition->mOffset) {
          switch (aProperty) {
            case eCSSProperty_top:    thePosition->mOffset->mTop.Reset();    break;
            case eCSSProperty_right:  thePosition->mOffset->mRight.Reset();   break;
            case eCSSProperty_bottom: thePosition->mOffset->mBottom.Reset(); break;
            case eCSSProperty_left:   thePosition->mOffset->mLeft.Reset();   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

      // nsCSSList
    case eCSSProperty_list_style_type:
    case eCSSProperty_list_style_image:
    case eCSSProperty_list_style_position: {
      CSS_CHECK(List) {
        switch (aProperty) {
          case eCSSProperty_list_style_type:      theList->mType.Reset();     break;
          case eCSSProperty_list_style_image:     theList->mImage.Reset();    break;
          case eCSSProperty_list_style_position:  theList->mPosition.Reset(); break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

    case eCSSProperty_image_region_top:
    case eCSSProperty_image_region_right:
    case eCSSProperty_image_region_bottom:
    case eCSSProperty_image_region_left: {
      CSS_CHECK(List) {
        CSS_CHECK_RECT(theList->mImageRegion) {
          switch (aProperty) {
            case eCSSProperty_image_region_top:    theList->mImageRegion->mTop.Reset();    break;
            case eCSSProperty_image_region_right:  theList->mImageRegion->mRight.Reset();   break;
            case eCSSProperty_image_region_bottom: theList->mImageRegion->mBottom.Reset(); break;
            case eCSSProperty_image_region_left:   theList->mImageRegion->mLeft.Reset();   break;
            CSS_BOGUS_DEFAULT; // make compiler happy
          }
        }
      }
      break;
    }

      // nsCSSTable
    case eCSSProperty_border_collapse:
    case eCSSProperty_border_x_spacing:
    case eCSSProperty_border_y_spacing:
    case eCSSProperty_caption_side:
    case eCSSProperty_empty_cells:
    case eCSSProperty_table_layout: {
      CSS_CHECK(Table) {
        switch (aProperty) {
          case eCSSProperty_border_collapse:  theTable->mBorderCollapse.Reset(); break;
          case eCSSProperty_border_x_spacing: theTable->mBorderSpacingX.Reset(); break;
          case eCSSProperty_border_y_spacing: theTable->mBorderSpacingY.Reset(); break;
          case eCSSProperty_caption_side:     theTable->mCaptionSide.Reset();    break;
          case eCSSProperty_empty_cells:      theTable->mEmptyCells.Reset();     break;
          case eCSSProperty_table_layout:     theTable->mLayout.Reset();         break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

      // nsCSSBreaks
    case eCSSProperty_orphans:
    case eCSSProperty_widows:
    case eCSSProperty_page: {
      CSS_CHECK(Breaks) {
        switch (aProperty) {
          case eCSSProperty_orphans:            theBreaks->mOrphans.Reset();         break;
          case eCSSProperty_widows:             theBreaks->mWidows.Reset();          break;
          case eCSSProperty_page:               theBreaks->mPage.Reset();            break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }
    case eCSSProperty_page_break_after:
    case eCSSProperty_page_break_before:
    case eCSSProperty_page_break_inside: {
      // temp fix for bug 24000
      CSS_CHECK(Display) {
        switch (aProperty) {
          case eCSSProperty_page_break_after:   theDisplay->mBreakAfter.Reset();  break;
          case eCSSProperty_page_break_before:  theDisplay->mBreakBefore.Reset(); break;
          case eCSSProperty_page_break_inside:                                    break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }
      // nsCSSPage
    case eCSSProperty_marks:
    case eCSSProperty_size_width:
    case eCSSProperty_size_height: {
      CSS_CHECK(Page) {
        switch (aProperty) {
          case eCSSProperty_marks:        thePage->mMarks.Reset(); break;
          case eCSSProperty_size_width:   thePage->mSizeWidth.Reset();  break;
          case eCSSProperty_size_height:  thePage->mSizeHeight.Reset();  break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

      // nsCSSContent
    case eCSSProperty_content:
    case eCSSProperty__moz_counter_increment:
    case eCSSProperty__moz_counter_reset:
    case eCSSProperty_marker_offset:
    case eCSSProperty_quotes_open:
    case eCSSProperty_quotes_close: {
      CSS_CHECK(Content) {
        switch (aProperty) {
          case eCSSProperty_content:
            CSS_CHECK_DATA(theContent->mContent, nsCSSValueList) {
              theContent->mContent->mValue.Reset();          
              CSS_IF_DELETE(theContent->mContent->mNext);
            }
            break;
          case eCSSProperty__moz_counter_increment:
            CSS_CHECK_DATA(theContent->mCounterIncrement, nsCSSCounterData) {
              theContent->mCounterIncrement->mCounter.Reset(); 
              CSS_IF_DELETE(theContent->mCounterIncrement->mNext);
            }
            break;
          case eCSSProperty__moz_counter_reset:
            CSS_CHECK_DATA(theContent->mCounterReset, nsCSSCounterData) {
              theContent->mCounterReset->mCounter.Reset();
              CSS_IF_DELETE(theContent->mCounterReset->mNext);
            }
            break;
          case eCSSProperty_marker_offset:      theContent->mMarkerOffset.Reset();     break;
          case eCSSProperty_quotes_open:
            CSS_CHECK_DATA(theContent->mQuotes, nsCSSQuotes) {
              theContent->mQuotes->mOpen.Reset();          
              CSS_IF_DELETE(theContent->mQuotes->mNext);
            }
            break;
          case eCSSProperty_quotes_close:
            CSS_CHECK_DATA(theContent->mQuotes, nsCSSQuotes) {
              theContent->mQuotes->mClose.Reset();          
              CSS_IF_DELETE(theContent->mQuotes->mNext);
            }
            break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

    // nsCSSUserInterface
    case eCSSProperty_user_input:
    case eCSSProperty_user_modify:
    case eCSSProperty_user_select:
    case eCSSProperty_key_equivalent:
    case eCSSProperty_user_focus:
    case eCSSProperty_resizer:
    case eCSSProperty_cursor: {
      CSS_CHECK(UserInterface) {
        switch (aProperty) {
          case eCSSProperty_user_input:       theUserInterface->mUserInput.Reset();      break;
          case eCSSProperty_user_modify:      theUserInterface->mUserModify.Reset();     break;
          case eCSSProperty_user_select:      theUserInterface->mUserSelect.Reset();     break;
          case eCSSProperty_key_equivalent: 
            CSS_CHECK_DATA(theUserInterface->mKeyEquivalent, nsCSSValueList) {
              theUserInterface->mKeyEquivalent->mValue.Reset();
              CSS_IF_DELETE(theUserInterface->mKeyEquivalent->mNext);
            }
            break;
          case eCSSProperty_user_focus:       theUserInterface->mUserFocus.Reset();      break;
          case eCSSProperty_resizer:          theUserInterface->mResizer.Reset();        break;
          case eCSSProperty_cursor:
            CSS_CHECK_DATA(theUserInterface->mCursor, nsCSSValueList) {
              theUserInterface->mCursor->mValue.Reset();
              CSS_IF_DELETE(theUserInterface->mCursor->mNext);
            }
            break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

#ifdef INCLUDE_XUL
    // nsCSSXUL
    case eCSSProperty_box_align:
    case eCSSProperty_box_direction:
    case eCSSProperty_box_flex:
    case eCSSProperty_box_orient:
    case eCSSProperty_box_pack:
    case eCSSProperty_box_ordinal_group: {
      CSS_CHECK(XUL) {
        switch(aProperty) {
        case eCSSProperty_box_align:          theXUL->mBoxAlign.Reset();         break;
        case eCSSProperty_box_direction:      theXUL->mBoxDirection.Reset();     break;
        case eCSSProperty_box_flex:           theXUL->mBoxFlex.Reset();          break;
        case eCSSProperty_box_orient:         theXUL->mBoxOrient.Reset();        break;
        case eCSSProperty_box_pack:           theXUL->mBoxPack.Reset();          break;
        case eCSSProperty_box_ordinal_group:  theXUL->mBoxOrdinal.Reset();       break;
        CSS_BOGUS_DEFAULT; // Make compiler happy
        }
      }
      break;
    }
#endif

#ifdef MOZ_SVG
    // nsCSSSVG
    case eCSSProperty_fill:
    case eCSSProperty_fill_opacity:
    case eCSSProperty_fill_rule:
    case eCSSProperty_stroke:
    case eCSSProperty_stroke_dasharray:
    case eCSSProperty_stroke_dashoffset:
    case eCSSProperty_stroke_linecap:
    case eCSSProperty_stroke_linejoin:
    case eCSSProperty_stroke_miterlimit:
    case eCSSProperty_stroke_opacity:
    case eCSSProperty_stroke_width: {
      CSS_CHECK(SVG) {
        switch(aProperty) {
          case eCSSProperty_fill:              theSVG->mFill.Reset();             break;
          case eCSSProperty_fill_opacity:      theSVG->mFillOpacity.Reset();      break;
          case eCSSProperty_fill_rule:         theSVG->mFillRule.Reset();         break;
          case eCSSProperty_stroke:            theSVG->mStroke.Reset();           break;
          case eCSSProperty_stroke_dasharray:  theSVG->mStrokeDasharray.Reset();  break;
          case eCSSProperty_stroke_dashoffset: theSVG->mStrokeDashoffset.Reset(); break;
          case eCSSProperty_stroke_linecap:    theSVG->mStrokeLinecap.Reset();    break;
          case eCSSProperty_stroke_linejoin:   theSVG->mStrokeLinejoin.Reset();   break;
          case eCSSProperty_stroke_miterlimit: theSVG->mStrokeMiterlimit.Reset(); break;
          case eCSSProperty_stroke_opacity:    theSVG->mStrokeOpacity.Reset(); break;
          case eCSSProperty_stroke_width:      theSVG->mStrokeWidth.Reset();   break;
       CSS_BOGUS_DEFAULT; // Make compiler happy
        }
      }
      break;
    }
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
    case eCSSProperty_volume: {
      CSS_CHECK(Aural) {
        switch (aProperty) {
          case eCSSProperty_azimuth:            theAural->mAzimuth.Reset();          break;
          case eCSSProperty_elevation:          theAural->mElevation.Reset();        break;
          case eCSSProperty_cue_after:          theAural->mCueAfter.Reset();         break;
          case eCSSProperty_cue_before:         theAural->mCueBefore.Reset();        break;
          case eCSSProperty_pause_after:        theAural->mPauseAfter.Reset();       break;
          case eCSSProperty_pause_before:       theAural->mPauseBefore.Reset();      break;
          case eCSSProperty_pitch:              theAural->mPitch.Reset();            break;
          case eCSSProperty_pitch_range:        theAural->mPitchRange.Reset();       break;
          case eCSSProperty_play_during:        theAural->mPlayDuring.Reset();       break;
          case eCSSProperty_play_during_flags:  theAural->mPlayDuringFlags.Reset();  break;
          case eCSSProperty_richness:           theAural->mRichness.Reset();         break;
          case eCSSProperty_speak:              theAural->mSpeak.Reset();            break;
          case eCSSProperty_speak_header:       theAural->mSpeakHeader.Reset();      break;
          case eCSSProperty_speak_numeral:      theAural->mSpeakNumeral.Reset();     break;
          case eCSSProperty_speak_punctuation:  theAural->mSpeakPunctuation.Reset(); break;
          case eCSSProperty_speech_rate:        theAural->mSpeechRate.Reset();       break;
          case eCSSProperty_stress:             theAural->mStress.Reset();           break;
          case eCSSProperty_voice_family:       theAural->mVoiceFamily.Reset();      break;
          case eCSSProperty_volume:             theAural->mVolume.Reset();           break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

      // Shorthands
    case eCSSProperty_background:
      RemoveProperty(eCSSProperty_background_color);
      RemoveProperty(eCSSProperty_background_image);
      RemoveProperty(eCSSProperty_background_repeat);
      RemoveProperty(eCSSProperty_background_attachment);
      RemoveProperty(eCSSProperty_background_x_position);
      RemoveProperty(eCSSProperty_background_y_position);
      break;
    case eCSSProperty_border: {
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(theMargin->mBorderWidth);
        CSS_IF_DELETE(theMargin->mBorderStyle);
        CSS_IF_DELETE(theMargin->mBorderColor);
      }
      break;
    }
    case eCSSProperty_border_spacing:
      RemoveProperty(eCSSProperty_border_x_spacing);
      RemoveProperty(eCSSProperty_border_y_spacing);
      break;
    case eCSSProperty_clip: {
      CSS_CHECK(Display) {
        CSS_IF_DELETE(theDisplay->mClip);
      }
      break;
    }
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
    case eCSSProperty_image_region: {
      CSS_CHECK(List) {
        CSS_IF_DELETE(theList->mImageRegion);
      }
      break;
    }
    case eCSSProperty_list_style:
      RemoveProperty(eCSSProperty_list_style_type);
      RemoveProperty(eCSSProperty_list_style_image);
      RemoveProperty(eCSSProperty_list_style_position);
      break;
    case eCSSProperty_margin: {
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(theMargin->mMargin);
      }
      break;
    }
    case eCSSProperty__moz_outline:
      RemoveProperty(eCSSProperty__moz_outline_color);
      RemoveProperty(eCSSProperty__moz_outline_style);
      RemoveProperty(eCSSProperty__moz_outline_width);
      break;
    case eCSSProperty_padding: {
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(theMargin->mPadding);
      }
      break;
    }
    case eCSSProperty_pause:
      RemoveProperty(eCSSProperty_pause_after);
      RemoveProperty(eCSSProperty_pause_before);
      break;
    case eCSSProperty_quotes: {
      CSS_CHECK(Content) {
	      CSS_IF_DELETE(theContent->mQuotes);
      }
      break;
    }
    case eCSSProperty_size:
      RemoveProperty(eCSSProperty_size_width);
      RemoveProperty(eCSSProperty_size_height);
      break;
    case eCSSProperty_text_shadow: {
      CSS_CHECK(Text) {
	      CSS_IF_DELETE(theText->mTextShadow);
      }
      break;
    }
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
    case eCSSProperty_border_color: {
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(theMargin->mBorderColor);
      }
      break;
    }
    case eCSSProperty_border_style: {
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(theMargin->mBorderStyle);
      }
      break;
    }
    case eCSSProperty_border_width: {
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(theMargin->mBorderWidth);
      }
      break;
    }
    case eCSSProperty__moz_border_radius: {
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(theMargin->mBorderRadius);
      }
      break;
    }
    case eCSSProperty__moz_outline_radius: {
      CSS_CHECK(Margin) {
        CSS_IF_DELETE(theMargin->mOutlineRadius);
      }
      break;
    }
//    default:  // XXX explicitly removing default case so compiler will help find missed props
    case eCSSProperty_UNKNOWN:
    case eCSSProperty_COUNT:
      result = NS_ERROR_ILLEGAL_VALUE;
      break;
  }

  if (NS_OK == result && nsnull != mOrder) {
    mOrder->RemoveValue(aProperty);
  }
  return result;
}


nsresult
nsCSSDeclaration::RemoveProperty(nsCSSProperty aProperty, nsCSSValue& aValue)
{
  nsresult result = NS_OK;

  PRBool  isImportant = GetValueIsImportant(aProperty);
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

nsresult
nsCSSDeclaration::AppendComment(const nsAString& aComment)
{
  return /* NS_ERROR_NOT_IMPLEMENTED, or not any longer that is */ NS_OK;
}

nsresult
nsCSSDeclaration::GetValueOrImportantValue(nsCSSProperty aProperty, nsCSSValue& aValue)
{
  PRBool  isImportant = GetValueIsImportant(aProperty);
  if (isImportant) {
    return mImportant->GetValue(aProperty, aValue);
  }
  return GetValue(aProperty, aValue);
}

nsresult
nsCSSDeclaration::GetValue(nsCSSProperty aProperty, nsCSSValue& aValue)
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
    case eCSSProperty_font_stretch: {
      CSS_VARONSTACK_GET(Font);
      if (nsnull != theFont) {
        switch (aProperty) {
          case eCSSProperty_font_family:      aValue = theFont->mFamily;      break;
          case eCSSProperty_font_style:       aValue = theFont->mStyle;       break;
          case eCSSProperty_font_variant:     aValue = theFont->mVariant;     break;
          case eCSSProperty_font_weight:      aValue = theFont->mWeight;      break;
          case eCSSProperty_font_size:        aValue = theFont->mSize;        break;
          case eCSSProperty_font_size_adjust: aValue = theFont->mSizeAdjust;  break;
          case eCSSProperty_font_stretch:     aValue = theFont->mStretch;     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

    // nsCSSColor
    case eCSSProperty_color:
    case eCSSProperty_background_color:
    case eCSSProperty_background_image:
    case eCSSProperty_background_repeat:
    case eCSSProperty_background_attachment:
    case eCSSProperty_background_x_position:
    case eCSSProperty_background_y_position:
    case eCSSProperty__moz_background_clip:
    case eCSSProperty__moz_background_origin:
    case eCSSProperty__moz_background_inline_policy: {
      CSS_VARONSTACK_GET(Color);
      if (nsnull != theColor) {
        switch (aProperty) {
          case eCSSProperty_color:                  aValue = theColor->mColor;           break;
          case eCSSProperty_background_color:       aValue = theColor->mBackColor;       break;
          case eCSSProperty_background_image:       aValue = theColor->mBackImage;       break;
          case eCSSProperty_background_repeat:      aValue = theColor->mBackRepeat;      break;
          case eCSSProperty_background_attachment:  aValue = theColor->mBackAttachment;  break;
          case eCSSProperty_background_x_position:  aValue = theColor->mBackPositionX;   break;
          case eCSSProperty_background_y_position:  aValue = theColor->mBackPositionY;   break;
          case eCSSProperty__moz_background_clip:   aValue = theColor->mBackClip;        break;
          case eCSSProperty__moz_background_origin: aValue = theColor->mBackOrigin;      break;
          case eCSSProperty__moz_background_inline_policy: aValue = theColor->mBackInlinePolicy; break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

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
    case eCSSProperty_white_space: {
      CSS_VARONSTACK_GET(Text);
      if (nsnull != theText) {
        switch (aProperty) {
          case eCSSProperty_word_spacing:     aValue = theText->mWordSpacing;    break;
          case eCSSProperty_letter_spacing:   aValue = theText->mLetterSpacing;  break;
          case eCSSProperty_text_decoration:  aValue = theText->mDecoration;     break;
          case eCSSProperty_vertical_align:   aValue = theText->mVerticalAlign;  break;
          case eCSSProperty_text_transform:   aValue = theText->mTextTransform;  break;
          case eCSSProperty_text_align:       aValue = theText->mTextAlign;      break;
          case eCSSProperty_text_indent:      aValue = theText->mTextIndent;     break;
          case eCSSProperty_unicode_bidi:     aValue = theText->mUnicodeBidi;    break;
          case eCSSProperty_line_height:      aValue = theText->mLineHeight;     break;
          case eCSSProperty_white_space:      aValue = theText->mWhiteSpace;     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

    case eCSSProperty_text_shadow_color:
    case eCSSProperty_text_shadow_x:
    case eCSSProperty_text_shadow_y:
    case eCSSProperty_text_shadow_radius: {
      CSS_VARONSTACK_GET(Text);
      if ((nsnull != theText) && (nsnull != theText->mTextShadow)) {
        switch (aProperty) {
          case eCSSProperty_text_shadow_color:  aValue = theText->mTextShadow->mColor;    break;
          case eCSSProperty_text_shadow_x:      aValue = theText->mTextShadow->mXOffset;  break;
          case eCSSProperty_text_shadow_y:      aValue = theText->mTextShadow->mYOffset;  break;
          case eCSSProperty_text_shadow_radius: aValue = theText->mTextShadow->mRadius;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      break;
    }

    // nsCSSDisplay
    case eCSSProperty_appearance:
    case eCSSProperty_float:
    case eCSSProperty_clear:
    case eCSSProperty_display:
    case eCSSProperty_binding:
    case eCSSProperty_position:
    case eCSSProperty_direction:
    case eCSSProperty_visibility:
    case eCSSProperty_opacity:
    case eCSSProperty_overflow: {
      CSS_VARONSTACK_GET(Display);
      if (nsnull != theDisplay) {
        switch (aProperty) {
          case eCSSProperty_appearance: aValue = theDisplay->mAppearance; break;
          case eCSSProperty_float:      aValue = theDisplay->mFloat;      break;
          case eCSSProperty_clear:      aValue = theDisplay->mClear;      break;
          case eCSSProperty_display:    aValue = theDisplay->mDisplay;    break;
          case eCSSProperty_binding:         
            aValue = theDisplay->mBinding;        
            break;
          case eCSSProperty_direction:  aValue = theDisplay->mDirection;  break;
          case eCSSProperty_position:   aValue = theDisplay->mPosition;   break;
          case eCSSProperty_visibility: aValue = theDisplay->mVisibility; break;
          case eCSSProperty_opacity:    aValue = theDisplay->mOpacity;    break;
          case eCSSProperty_overflow:   aValue = theDisplay->mOverflow;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

    case eCSSProperty_clip_top:
    case eCSSProperty_clip_right:
    case eCSSProperty_clip_bottom:
    case eCSSProperty_clip_left: {
      CSS_VARONSTACK_GET(Display);
      if ((nsnull != theDisplay) && (nsnull != theDisplay->mClip)) {
        switch(aProperty) {
          case eCSSProperty_clip_top:     aValue = theDisplay->mClip->mTop;     break;
          case eCSSProperty_clip_right:   aValue = theDisplay->mClip->mRight;   break;
          case eCSSProperty_clip_bottom:  aValue = theDisplay->mClip->mBottom;  break;
          case eCSSProperty_clip_left:    aValue = theDisplay->mClip->mLeft;    break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

    // nsCSSMargin
    case eCSSProperty_margin_top:
    case eCSSProperty_margin_right:
    case eCSSProperty_margin_bottom:
    case eCSSProperty_margin_left: {
      CSS_VARONSTACK_GET(Margin);
      if ((nsnull != theMargin) && (nsnull != theMargin->mMargin)) {
        switch (aProperty) {
          case eCSSProperty_margin_top:     aValue = theMargin->mMargin->mTop;     break;
          case eCSSProperty_margin_right:   aValue = theMargin->mMargin->mRight;   break;
          case eCSSProperty_margin_bottom:  aValue = theMargin->mMargin->mBottom;  break;
          case eCSSProperty_margin_left:    aValue = theMargin->mMargin->mLeft;    break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

    case eCSSProperty_padding_top:
    case eCSSProperty_padding_right:
    case eCSSProperty_padding_bottom:
    case eCSSProperty_padding_left: {
      CSS_VARONSTACK_GET(Margin);
      if ((nsnull != theMargin) && (nsnull != theMargin->mPadding)) {
        switch (aProperty) {
          case eCSSProperty_padding_top:    aValue = theMargin->mPadding->mTop;    break;
          case eCSSProperty_padding_right:  aValue = theMargin->mPadding->mRight;  break;
          case eCSSProperty_padding_bottom: aValue = theMargin->mPadding->mBottom; break;
          case eCSSProperty_padding_left:   aValue = theMargin->mPadding->mLeft;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

    case eCSSProperty_border_top_width:
    case eCSSProperty_border_right_width:
    case eCSSProperty_border_bottom_width:
    case eCSSProperty_border_left_width: {
      CSS_VARONSTACK_GET(Margin);
      if ((nsnull != theMargin) && (nsnull != theMargin->mBorderWidth)) {
        switch (aProperty) {
          case eCSSProperty_border_top_width:     aValue = theMargin->mBorderWidth->mTop;     break;
          case eCSSProperty_border_right_width:   aValue = theMargin->mBorderWidth->mRight;   break;
          case eCSSProperty_border_bottom_width:  aValue = theMargin->mBorderWidth->mBottom;  break;
          case eCSSProperty_border_left_width:    aValue = theMargin->mBorderWidth->mLeft;    break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

    case eCSSProperty_border_top_color:
    case eCSSProperty_border_right_color:
    case eCSSProperty_border_bottom_color:
    case eCSSProperty_border_left_color: {
      CSS_VARONSTACK_GET(Margin);
      if ((nsnull != theMargin) && (nsnull != theMargin->mBorderColor)) {
        switch (aProperty) {
          case eCSSProperty_border_top_color:     aValue = theMargin->mBorderColor->mTop;    break;
          case eCSSProperty_border_right_color:   aValue = theMargin->mBorderColor->mRight;  break;
          case eCSSProperty_border_bottom_color:  aValue = theMargin->mBorderColor->mBottom; break;
          case eCSSProperty_border_left_color:    aValue = theMargin->mBorderColor->mLeft;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

    case eCSSProperty_border_top_style:
    case eCSSProperty_border_right_style:
    case eCSSProperty_border_bottom_style:
    case eCSSProperty_border_left_style: {
      CSS_VARONSTACK_GET(Margin);
      if ((nsnull != theMargin) && (nsnull != theMargin->mBorderStyle)) {
        switch (aProperty) {
          case eCSSProperty_border_top_style:     aValue = theMargin->mBorderStyle->mTop;    break;
          case eCSSProperty_border_right_style:   aValue = theMargin->mBorderStyle->mRight;  break;
          case eCSSProperty_border_bottom_style:  aValue = theMargin->mBorderStyle->mBottom; break;
          case eCSSProperty_border_left_style:    aValue = theMargin->mBorderStyle->mLeft;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

    case eCSSProperty__moz_border_radius_topLeft:
    case eCSSProperty__moz_border_radius_topRight:
    case eCSSProperty__moz_border_radius_bottomRight:
    case eCSSProperty__moz_border_radius_bottomLeft: {
      CSS_VARONSTACK_GET(Margin);
      if ((nsnull != theMargin) && (nsnull != theMargin->mBorderRadius)) {
        switch (aProperty) {
          case eCSSProperty__moz_border_radius_topLeft:			aValue = theMargin->mBorderRadius->mTop;    break;
          case eCSSProperty__moz_border_radius_topRight:		aValue = theMargin->mBorderRadius->mRight;  break;
          case eCSSProperty__moz_border_radius_bottomRight:	aValue = theMargin->mBorderRadius->mBottom; break;
          case eCSSProperty__moz_border_radius_bottomLeft:	aValue = theMargin->mBorderRadius->mLeft;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

    case eCSSProperty__moz_outline_radius_topLeft:
    case eCSSProperty__moz_outline_radius_topRight:
    case eCSSProperty__moz_outline_radius_bottomRight:
    case eCSSProperty__moz_outline_radius_bottomLeft: {
      CSS_VARONSTACK_GET(Margin);
      if ((nsnull != theMargin) && (nsnull != theMargin->mOutlineRadius)) {
        switch (aProperty) {
          case eCSSProperty__moz_outline_radius_topLeft:			aValue = theMargin->mOutlineRadius->mTop;    break;
          case eCSSProperty__moz_outline_radius_topRight:			aValue = theMargin->mOutlineRadius->mRight;  break;
          case eCSSProperty__moz_outline_radius_bottomRight:	aValue = theMargin->mOutlineRadius->mBottom; break;
          case eCSSProperty__moz_outline_radius_bottomLeft:		aValue = theMargin->mOutlineRadius->mLeft;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

    case eCSSProperty__moz_outline_width:
    case eCSSProperty__moz_outline_color:
    case eCSSProperty__moz_outline_style:
    case eCSSProperty_float_edge: {
      CSS_VARONSTACK_GET(Margin);
      if (nsnull != theMargin) {
        switch (aProperty) {
          case eCSSProperty__moz_outline_width: aValue = theMargin->mOutlineWidth; break;
          case eCSSProperty__moz_outline_color: aValue = theMargin->mOutlineColor; break;
          case eCSSProperty__moz_outline_style: aValue = theMargin->mOutlineStyle; break;
          case eCSSProperty_float_edge:         aValue = theMargin->mFloatEdge;    break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

    // nsCSSPosition
    case eCSSProperty_width:
    case eCSSProperty_min_width:
    case eCSSProperty_max_width:
    case eCSSProperty_height:
    case eCSSProperty_min_height:
    case eCSSProperty_max_height:
    case eCSSProperty_box_sizing:
    case eCSSProperty_z_index: {
      CSS_VARONSTACK_GET(Position);
      if (nsnull != thePosition) {
        switch (aProperty) {
          case eCSSProperty_width:      aValue = thePosition->mWidth;      break;
          case eCSSProperty_min_width:  aValue = thePosition->mMinWidth;   break;
          case eCSSProperty_max_width:  aValue = thePosition->mMaxWidth;   break;
          case eCSSProperty_height:     aValue = thePosition->mHeight;     break;
          case eCSSProperty_min_height: aValue = thePosition->mMinHeight;  break;
          case eCSSProperty_max_height: aValue = thePosition->mMaxHeight;  break;
          case eCSSProperty_box_sizing: aValue = thePosition->mBoxSizing;  break;
          case eCSSProperty_z_index:    aValue = thePosition->mZIndex;     break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

    case eCSSProperty_top:
    case eCSSProperty_right:
    case eCSSProperty_bottom:
    case eCSSProperty_left: {
      CSS_VARONSTACK_GET(Position);
      if ((nsnull != thePosition) && (nsnull != thePosition->mOffset)) {
        switch (aProperty) {
          case eCSSProperty_top:    aValue = thePosition->mOffset->mTop;    break;
          case eCSSProperty_right:  aValue = thePosition->mOffset->mRight;  break;
          case eCSSProperty_bottom: aValue = thePosition->mOffset->mBottom; break;
          case eCSSProperty_left:   aValue = thePosition->mOffset->mLeft;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

      // nsCSSList
    case eCSSProperty_list_style_type:
    case eCSSProperty_list_style_image:
    case eCSSProperty_list_style_position: {
      CSS_VARONSTACK_GET(List);
      if (nsnull != theList) {
        switch (aProperty) {
          case eCSSProperty_list_style_type:      aValue = theList->mType;     break;
          case eCSSProperty_list_style_image:     aValue = theList->mImage;    break;
          case eCSSProperty_list_style_position:  aValue = theList->mPosition; break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

    case eCSSProperty_image_region_top:
    case eCSSProperty_image_region_right:
    case eCSSProperty_image_region_bottom:
    case eCSSProperty_image_region_left: {
      CSS_VARONSTACK_GET(List);
      if (theList && theList->mImageRegion) {
        switch (aProperty) {
          case eCSSProperty_image_region_top:    aValue = theList->mImageRegion->mTop;    break;
          case eCSSProperty_image_region_right:  aValue = theList->mImageRegion->mRight;  break;
          case eCSSProperty_image_region_bottom: aValue = theList->mImageRegion->mBottom; break;
          case eCSSProperty_image_region_left:   aValue = theList->mImageRegion->mLeft;   break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

      // nsCSSTable
    case eCSSProperty_border_collapse:
    case eCSSProperty_border_x_spacing:
    case eCSSProperty_border_y_spacing:
    case eCSSProperty_caption_side:
    case eCSSProperty_empty_cells:
    case eCSSProperty_table_layout: {
      CSS_VARONSTACK_GET(Table);
      if (nsnull != theTable) {
        switch (aProperty) {
          case eCSSProperty_border_collapse:  aValue = theTable->mBorderCollapse; break;
          case eCSSProperty_border_x_spacing: aValue = theTable->mBorderSpacingX; break;
          case eCSSProperty_border_y_spacing: aValue = theTable->mBorderSpacingY; break;
          case eCSSProperty_caption_side:     aValue = theTable->mCaptionSide;    break;
          case eCSSProperty_empty_cells:      aValue = theTable->mEmptyCells;     break;
          case eCSSProperty_table_layout:     aValue = theTable->mLayout;         break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

      // nsCSSBreaks
    case eCSSProperty_orphans:
    case eCSSProperty_widows:
    case eCSSProperty_page: {
      CSS_VARONSTACK_GET(Breaks);
      if (nsnull != theBreaks) {
        switch (aProperty) {
          case eCSSProperty_orphans:            aValue = theBreaks->mOrphans;         break;
          case eCSSProperty_widows:             aValue = theBreaks->mWidows;          break;
          case eCSSProperty_page:               aValue = theBreaks->mPage;            break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }
    case eCSSProperty_page_break_after:
    case eCSSProperty_page_break_before:
    case eCSSProperty_page_break_inside: {
      // temp fix for bug 24000
      CSS_VARONSTACK_GET(Display);
      if (theDisplay) {
        switch (aProperty) {
          case eCSSProperty_page_break_inside:  aValue.Reset();                    break;
          case eCSSProperty_page_break_after:   aValue = theDisplay->mBreakAfter;  break;
          case eCSSProperty_page_break_before:  aValue = theDisplay->mBreakBefore; break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

      // nsCSSPage
    case eCSSProperty_marks:
    case eCSSProperty_size_width:
    case eCSSProperty_size_height: {
      CSS_VARONSTACK_GET(Page);
      if (nsnull != thePage) {
        switch (aProperty) {
          case eCSSProperty_marks:        aValue = thePage->mMarks;       break;
          case eCSSProperty_size_width:   aValue = thePage->mSizeWidth;   break;
          case eCSSProperty_size_height:  aValue = thePage->mSizeHeight;  break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

      // nsCSSContent
    case eCSSProperty_content:
    case eCSSProperty__moz_counter_increment:
    case eCSSProperty__moz_counter_reset:
    case eCSSProperty_marker_offset:
    case eCSSProperty_quotes_open:
    case eCSSProperty_quotes_close: {
      CSS_VARONSTACK_GET(Content);
      if (nsnull != theContent) {
        switch (aProperty) {
          case eCSSProperty_content:
            if (nsnull != theContent->mContent) {
              aValue = theContent->mContent->mValue;
            }
            break;
          case eCSSProperty__moz_counter_increment:  
            if (nsnull != theContent->mCounterIncrement) {
              aValue = theContent->mCounterIncrement->mCounter;
            }
            break;
          case eCSSProperty__moz_counter_reset:
            if (nsnull != theContent->mCounterReset) {
              aValue = theContent->mCounterReset->mCounter;
            }
            break;
          case eCSSProperty_marker_offset:
            aValue = theContent->mMarkerOffset;
            break;
          case eCSSProperty_quotes_open:
            if (nsnull != theContent->mQuotes) {
              aValue = theContent->mQuotes->mOpen;
            }
            break;
          case eCSSProperty_quotes_close:
            if (nsnull != theContent->mQuotes) {
              aValue = theContent->mQuotes->mClose;
            }
            break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

    // nsCSSUserInterface
    case eCSSProperty_user_input:
    case eCSSProperty_user_modify:
    case eCSSProperty_user_select:
    case eCSSProperty_key_equivalent:
    case eCSSProperty_user_focus:
    case eCSSProperty_resizer:
    case eCSSProperty_cursor:
    case eCSSProperty_force_broken_image_icon: {
      CSS_VARONSTACK_GET(UserInterface);
      if (nsnull != theUserInterface) {
        switch (aProperty) {
          case eCSSProperty_user_input:       aValue = theUserInterface->mUserInput;       break;
          case eCSSProperty_user_modify:      aValue = theUserInterface->mUserModify;      break;
          case eCSSProperty_user_select:      aValue = theUserInterface->mUserSelect;      break;
          case eCSSProperty_key_equivalent:
            if (nsnull != theUserInterface->mKeyEquivalent) {
              aValue = theUserInterface->mKeyEquivalent->mValue;
            }
            break;
          case eCSSProperty_user_focus:       aValue = theUserInterface->mUserFocus;       break;
          case eCSSProperty_resizer:          aValue = theUserInterface->mResizer;         break;
          case eCSSProperty_cursor:
            if (nsnull != theUserInterface->mCursor) {
              aValue = theUserInterface->mCursor->mValue;
            }
            break;
          case eCSSProperty_force_broken_image_icon: aValue = theUserInterface->mForceBrokenImageIcon; break;

          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }

#ifdef INCLUDE_XUL
    // nsCSSXUL
    case eCSSProperty_box_align:
    case eCSSProperty_box_direction:
    case eCSSProperty_box_flex:
    case eCSSProperty_box_orient:
    case eCSSProperty_box_pack:
    case eCSSProperty_box_ordinal_group: {
      CSS_VARONSTACK_GET(XUL);
      if (nsnull != theXUL) {
        switch (aProperty) {
          case eCSSProperty_box_align:          aValue = theXUL->mBoxAlign;      break;
          case eCSSProperty_box_direction:      aValue = theXUL->mBoxDirection;  break;
          case eCSSProperty_box_flex:           aValue = theXUL->mBoxFlex;       break;
          case eCSSProperty_box_orient:         aValue = theXUL->mBoxOrient;     break;
          case eCSSProperty_box_pack:           aValue = theXUL->mBoxPack;       break;
          case eCSSProperty_box_ordinal_group:  aValue = theXUL->mBoxOrdinal;    break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }
#endif

#ifdef MOZ_SVG
    // nsCSSSVG
    case eCSSProperty_fill:
    case eCSSProperty_fill_opacity:
    case eCSSProperty_fill_rule:
    case eCSSProperty_stroke:
    case eCSSProperty_stroke_dasharray:
    case eCSSProperty_stroke_dashoffset:
    case eCSSProperty_stroke_linecap:
    case eCSSProperty_stroke_linejoin:
    case eCSSProperty_stroke_miterlimit:
    case eCSSProperty_stroke_opacity:
    case eCSSProperty_stroke_width: {
      CSS_VARONSTACK_GET(SVG);
      if (nsnull != theSVG) {
        switch (aProperty) {
          case eCSSProperty_fill:              aValue = theSVG->mFill;             break;
          case eCSSProperty_fill_opacity:      aValue = theSVG->mFillOpacity;      break;
          case eCSSProperty_fill_rule:         aValue = theSVG->mFillRule;         break;
          case eCSSProperty_stroke:            aValue = theSVG->mStroke;           break;
          case eCSSProperty_stroke_dasharray:  aValue = theSVG->mStrokeDasharray;  break;
          case eCSSProperty_stroke_dashoffset: aValue = theSVG->mStrokeDashoffset; break;
          case eCSSProperty_stroke_linecap:    aValue = theSVG->mStrokeLinecap;    break;
          case eCSSProperty_stroke_linejoin:   aValue = theSVG->mStrokeLinejoin;   break;
          case eCSSProperty_stroke_miterlimit: aValue = theSVG->mStrokeMiterlimit; break;
          case eCSSProperty_stroke_opacity:    aValue = theSVG->mStrokeOpacity;    break;
          case eCSSProperty_stroke_width:      aValue = theSVG->mStrokeWidth;      break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }
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
    case eCSSProperty_volume: {
      CSS_VARONSTACK_GET(Aural);
      if (nsnull != theAural) {
        switch (aProperty) {
          case eCSSProperty_azimuth:            aValue = theAural->mAzimuth;          break;
          case eCSSProperty_elevation:          aValue = theAural->mElevation;        break;
          case eCSSProperty_cue_after:          aValue = theAural->mCueAfter;         break;
          case eCSSProperty_cue_before:         aValue = theAural->mCueBefore;        break;
          case eCSSProperty_pause_after:        aValue = theAural->mPauseAfter;       break;
          case eCSSProperty_pause_before:       aValue = theAural->mPauseBefore;      break;
          case eCSSProperty_pitch:              aValue = theAural->mPitch;            break;
          case eCSSProperty_pitch_range:        aValue = theAural->mPitchRange;       break;
          case eCSSProperty_play_during:        aValue = theAural->mPlayDuring;       break;
          case eCSSProperty_play_during_flags:  aValue = theAural->mPlayDuringFlags;  break;
          case eCSSProperty_richness:           aValue = theAural->mRichness;         break;
          case eCSSProperty_speak:              aValue = theAural->mSpeak;            break;
          case eCSSProperty_speak_header:       aValue = theAural->mSpeakHeader;      break;
          case eCSSProperty_speak_numeral:      aValue = theAural->mSpeakNumeral;     break;
          case eCSSProperty_speak_punctuation:  aValue = theAural->mSpeakPunctuation; break;
          case eCSSProperty_speech_rate:        aValue = theAural->mSpeechRate;       break;
          case eCSSProperty_stress:             aValue = theAural->mStress;           break;
          case eCSSProperty_voice_family:       aValue = theAural->mVoiceFamily;      break;
          case eCSSProperty_volume:             aValue = theAural->mVolume;           break;
          CSS_BOGUS_DEFAULT; // make compiler happy
        }
      }
      else {
        aValue.Reset();
      }
      break;
    }


      // Shorthands
    case eCSSProperty_background:
    case eCSSProperty_border:
    case eCSSProperty_border_spacing:
    case eCSSProperty_clip:
    case eCSSProperty_cue:
    case eCSSProperty_font:
    case eCSSProperty_image_region:
    case eCSSProperty_list_style:
    case eCSSProperty_margin:
    case eCSSProperty__moz_outline:
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


nsresult
nsCSSDeclaration::GetValue(const nsAString& aProperty,
                           nsAString& aValue)
{
  nsCSSProperty propID = nsCSSProps::LookupProperty(aProperty);
  return GetValue(propID, aValue);
}

PRBool nsCSSDeclaration::AppendValueToString(nsCSSProperty aProperty, nsAString& aResult)
{
  nsCSSValue  value;
  GetValue(aProperty, value);
  return AppendValueToString(aProperty, value, aResult);
}

PRBool nsCSSDeclaration::AppendValueOrImportantValueToString(nsCSSProperty aProperty, nsAString& aResult)
{
  nsCSSValue  value;
  GetValueOrImportantValue(aProperty, value);
  return AppendValueToString(aProperty, value, aResult);
}

PRBool nsCSSDeclaration::AppendValueToString(nsCSSProperty aProperty, const nsCSSValue& aValue, nsAString& aResult)
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
      case eCSSProperty_background_color:
      case eCSSProperty_border_top_color:
      case eCSSProperty_border_bottom_color:
      case eCSSProperty_border_left_color:
      case eCSSProperty_border_right_color:
      case eCSSProperty__moz_outline_color:
      case eCSSProperty_text_shadow_color: {
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
      const nsAFlatCString& name = nsCSSProps::LookupPropertyValue(aProperty, aValue.GetIntValue());
      aResult.Append(NS_ConvertASCIItoUCS2(name));
    }
  }
  else if (eCSSUnit_Color == unit){
    nsAutoString tmpStr;
    nscolor color = aValue.GetColorValue();

    aResult.Append(NS_LITERAL_STRING("rgb("));

    NS_NAMED_LITERAL_STRING(comma, ", ");

    tmpStr.AppendInt(NS_GET_R(color), 10);
    aResult.Append(tmpStr + comma);

    tmpStr.Truncate();
    tmpStr.AppendInt(NS_GET_G(color), 10);
    aResult.Append(tmpStr + comma);

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

nsresult
nsCSSDeclaration::GetValue(nsCSSProperty aProperty,
                           nsAString& aValue)
{
  PRBool  isImportant = GetValueIsImportant(aProperty);
  if (isImportant) {
    return mImportant->GetValue(aProperty, aValue);
  }

  aValue.Truncate(0);

  // shorthands
  switch (aProperty) {
    case eCSSProperty_background: {
      CSS_VARONSTACK_GET(Color);
      if (AppendValueToString(eCSSProperty_background_color, aValue)) aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty_background_image, aValue)) aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty_background_repeat, aValue)) aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty_background_attachment, aValue)) aValue.Append(PRUnichar(' '));
      if (HAS_VALUE(theColor,mBackPositionX) && HAS_VALUE(theColor,mBackPositionY)) {
        AppendValueToString(eCSSProperty_background_x_position, aValue);
        aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_background_y_position, aValue);
      }
      break;
    }
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
    case eCSSProperty_clip: {
      CSS_VARONSTACK_GET(Display);
      if (HAS_RECT(theDisplay,mClip)) {
        aValue.Append(NS_LITERAL_STRING("rect("));
        AppendValueToString(eCSSProperty_clip_top, aValue);     aValue.Append(NS_LITERAL_STRING(", "));
        AppendValueToString(eCSSProperty_clip_right, aValue);   aValue.Append(NS_LITERAL_STRING(", "));
        AppendValueToString(eCSSProperty_clip_bottom, aValue);  aValue.Append(NS_LITERAL_STRING(", "));
        AppendValueToString(eCSSProperty_clip_left, aValue);
        aValue.Append(PRUnichar(')'));
      }
      break;
    }
    case eCSSProperty_cue: {
      CSS_VARONSTACK_GET(Aural);
      if (HAS_VALUE(theAural,mCueAfter) && HAS_VALUE(theAural,mCueBefore)) {
        AppendValueToString(eCSSProperty_cue_after, aValue);
        aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_cue_before, aValue);
      }
      break;
    }
    case eCSSProperty_cursor: {
      CSS_VARONSTACK_GET(UserInterface);
      if ((nsnull != theUserInterface) && (nsnull != theUserInterface->mCursor)) {
        nsCSSValueList* cursor = theUserInterface->mCursor;
        do {
          AppendValueToString(eCSSProperty_cursor, cursor->mValue, aValue);
          cursor = cursor->mNext;
          if (nsnull != cursor) {
            aValue.Append(PRUnichar(' '));
          }
        } while (nsnull != cursor);
      }
      break;
    }
    case eCSSProperty_font: {
      CSS_VARONSTACK_GET(Font);
      if (HAS_VALUE(theFont,mSize)) {
        if (AppendValueToString(eCSSProperty_font_style, aValue)) aValue.Append(PRUnichar(' '));
        if (AppendValueToString(eCSSProperty_font_variant, aValue)) aValue.Append(PRUnichar(' '));
        if (AppendValueToString(eCSSProperty_font_weight, aValue)) aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_font_size, aValue);
        CSS_VARONSTACK_GET(Text);
        if (HAS_VALUE(theText,mLineHeight)) {
          aValue.Append(PRUnichar('/'));
          AppendValueToString(eCSSProperty_line_height, aValue);
        }
        aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_font_family, aValue);
      }
      break;
    }
    case eCSSProperty_image_region: {
      CSS_VARONSTACK_GET(List);
      if (HAS_RECT(theList,mImageRegion)) {
        aValue.Append(NS_LITERAL_STRING("rect("));
        AppendValueToString(eCSSProperty_image_region_top, aValue);     aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_image_region_right, aValue);   aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_image_region_bottom, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_image_region_left, aValue);
        aValue.Append(PRUnichar(')'));
      }
      break;
    }
    case eCSSProperty_list_style:
      if (AppendValueToString(eCSSProperty_list_style_type, aValue)) aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty_list_style_position, aValue)) aValue.Append(PRUnichar(' '));
      AppendValueToString(eCSSProperty_list_style_image, aValue);
      break;
    case eCSSProperty_margin: {
      CSS_VARONSTACK_GET(Margin);
      if (HAS_RECT(theMargin,mMargin)) {
        AppendValueToString(eCSSProperty_margin_top, aValue);     aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_margin_right, aValue);   aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_margin_bottom, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_margin_left, aValue);
      }
      break;
    }
    case eCSSProperty__moz_outline:
      if (AppendValueToString(eCSSProperty__moz_outline_color, aValue)) aValue.Append(PRUnichar(' '));
      if (AppendValueToString(eCSSProperty__moz_outline_style, aValue)) aValue.Append(PRUnichar(' '));
      AppendValueToString(eCSSProperty__moz_outline_width, aValue);
      break;
    case eCSSProperty_padding: {
      CSS_VARONSTACK_GET(Margin);
      if (HAS_RECT(theMargin,mPadding)) {
        AppendValueToString(eCSSProperty_padding_top, aValue);    aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_padding_right, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_padding_bottom, aValue); aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_padding_left, aValue);
      }
      break;
    }
    case eCSSProperty_pause: {
      CSS_VARONSTACK_GET(Aural);
      if (HAS_VALUE(theAural,mPauseAfter) && HAS_VALUE(theAural,mPauseBefore)) {
        AppendValueToString(eCSSProperty_pause_after, aValue);
        aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_pause_before, aValue);
      }
      break;
    }
    case eCSSProperty_size: {
      CSS_VARONSTACK_GET(Page);
      if (HAS_VALUE(thePage,mSizeWidth) && HAS_VALUE(thePage,mSizeHeight)) {
        AppendValueToString(eCSSProperty_size_width, aValue);
        aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_size_height, aValue);
      }
      break;
    }
    case eCSSProperty_text_shadow: {
      CSS_VARONSTACK_GET(Text);
      if ((nsnull != theText) && (nsnull != theText->mTextShadow)) {
        if (theText->mTextShadow->mXOffset.IsLengthUnit()) {
          nsCSSShadow*  shadow = theText->mTextShadow;
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
    }
    case eCSSProperty_background_position: {
      CSS_VARONSTACK_GET(Color);
      if (HAS_VALUE(theColor,mBackPositionX) && HAS_VALUE(theColor,mBackPositionY)) {
        AppendValueToString(eCSSProperty_background_x_position, aValue);
        aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_background_y_position, aValue);
      }
      break;
    }
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
    case eCSSProperty_border_color: {
      CSS_VARONSTACK_GET(Margin);
      if (HAS_RECT(theMargin,mBorderColor)) {
        AppendValueToString(eCSSProperty_border_top_color, aValue);     aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_right_color, aValue);   aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_bottom_color, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_left_color, aValue);
      }
      break;
    }
    case eCSSProperty_border_style: {
      CSS_VARONSTACK_GET(Margin);
      if (HAS_RECT(theMargin,mBorderStyle)) {
        AppendValueToString(eCSSProperty_border_top_style, aValue);     aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_right_style, aValue);   aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_bottom_style, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_left_style, aValue);
      }
      break;
    }
    case eCSSProperty__moz_border_radius: {
      CSS_VARONSTACK_GET(Margin);
      if (HAS_RECT(theMargin,mBorderRadius)) {
        AppendValueToString(eCSSProperty__moz_border_radius_topLeft, aValue);     aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty__moz_border_radius_topRight, aValue);   aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty__moz_border_radius_bottomRight, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty__moz_border_radius_bottomLeft, aValue);
      }
    	break;
    }
    case eCSSProperty__moz_outline_radius: {
      CSS_VARONSTACK_GET(Margin);
      if (HAS_RECT(theMargin,mOutlineRadius)) {
        AppendValueToString(eCSSProperty__moz_outline_radius_topLeft, aValue);     aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty__moz_outline_radius_topRight, aValue);   aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty__moz_outline_radius_bottomRight, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty__moz_outline_radius_bottomLeft, aValue);
      }
    	break;
    }
    case eCSSProperty_border_width: {
      CSS_VARONSTACK_GET(Margin);
      if (HAS_RECT(theMargin,mBorderWidth)) {
        AppendValueToString(eCSSProperty_border_top_width, aValue);     aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_right_width, aValue);   aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_bottom_width, aValue);  aValue.Append(PRUnichar(' '));
        AppendValueToString(eCSSProperty_border_left_width, aValue);
      }
      break;
    }
    case eCSSProperty_content: {
      CSS_VARONSTACK_GET(Content);
      if ((nsnull != theContent) && (nsnull != theContent->mContent)) {
        nsCSSValueList* content = theContent->mContent;
        do {
          AppendValueToString(eCSSProperty_content, content->mValue, aValue);
          content = content->mNext;
          if (nsnull != content) {
            aValue.Append(PRUnichar(' '));
          }
        } while (nsnull != content);
      }
      break;
    }
    case eCSSProperty__moz_counter_increment: {
      CSS_VARONSTACK_GET(Content);
      if ((nsnull != theContent) && (nsnull != theContent->mCounterIncrement)) {
        nsCSSCounterData* data = theContent->mCounterIncrement;
        do {
          if (AppendValueToString(eCSSProperty__moz_counter_increment, data->mCounter, aValue)) {
            if (HAS_VALUE(data, mValue)) {
              aValue.Append(PRUnichar(' '));
              AppendValueToString(eCSSProperty__moz_counter_increment, data->mValue, aValue);
            }
          }
          data = data->mNext;
          if (nsnull != data) {
            aValue.Append(PRUnichar(' '));
          }
        } while (nsnull != data);
      }
      break;
    }
    case eCSSProperty__moz_counter_reset: {
      CSS_VARONSTACK_GET(Content);
      if ((nsnull != theContent) && (nsnull != theContent->mCounterReset)) {
        nsCSSCounterData* data = theContent->mCounterReset;
        do {
          if (AppendValueToString(eCSSProperty__moz_counter_reset, data->mCounter, aValue)) {
            if (HAS_VALUE(data, mValue)) {
              aValue.Append(PRUnichar(' '));
              AppendValueToString(eCSSProperty__moz_counter_reset, data->mValue, aValue);
            }
          }
          data = data->mNext;
          if (nsnull != data) {
            aValue.Append(PRUnichar(' '));
          }
        } while (nsnull != data);
      }
      break;
    }
    case eCSSProperty_play_during: {
      CSS_VARONSTACK_GET(Aural);
      if (HAS_VALUE(theAural, mPlayDuring)) {
        AppendValueToString(eCSSProperty_play_during, aValue);
        if (HAS_VALUE(theAural, mPlayDuringFlags)) {
          aValue.Append(PRUnichar(' '));
          AppendValueToString(eCSSProperty_play_during_flags, aValue);
        }
      }
      break;
    }
    case eCSSProperty_quotes: {
      CSS_VARONSTACK_GET(Content);
      if ((nsnull != theContent) && (nsnull != theContent->mQuotes)) {
        nsCSSQuotes* quotes = theContent->mQuotes;
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
    }
    case eCSSProperty_key_equivalent: {
      CSS_VARONSTACK_GET(UserInterface);
      if ((nsnull != theUserInterface) && (nsnull != theUserInterface->mKeyEquivalent)) {
        nsCSSValueList* keyEquiv = theUserInterface->mKeyEquivalent;
        do {
          AppendValueToString(eCSSProperty_key_equivalent, keyEquiv->mValue, aValue);
          keyEquiv = keyEquiv->mNext;
          if (nsnull != keyEquiv) {
            aValue.Append(PRUnichar(' '));
          }
        } while (nsnull != keyEquiv);
      }
      break;
    }
    case eCSSProperty_border_top_colors:
    case eCSSProperty_border_right_colors:
    case eCSSProperty_border_bottom_colors:
    case eCSSProperty_border_left_colors: {
      CSS_VARONSTACK_GET(Margin);
      PRUint8 index;
      switch (aProperty) {
        case eCSSProperty_border_top_colors:    index = 0; break;
        case eCSSProperty_border_right_colors:  index = 1; break;
        case eCSSProperty_border_bottom_colors: index = 2; break;
        case eCSSProperty_border_left_colors:   index = 3; break;
        CSS_BOGUS_DEFAULT; // make compiler happy
      }
      if ((nsnull != theMargin) && (nsnull != theMargin->mBorderColors) &&
          (nsnull != theMargin->mBorderColors[index])) {
        nsCSSValueList* borderSideColors = theMargin->mBorderColors[index];
        do {
          AppendValueToString(aProperty, borderSideColors->mValue, aValue);
          borderSideColors = borderSideColors->mNext;
          if (nsnull != borderSideColors) {
            aValue.Append(PRUnichar(' '));
          }
        } while (nsnull != borderSideColors);
      }
      break;
    }
    default:
      AppendValueToString(aProperty, aValue);
      break;
  }
  return NS_OK;
}

nsCSSDeclaration*
nsCSSDeclaration::GetImportantValues()
{
  return mImportant;
}

PRBool
nsCSSDeclaration::GetValueIsImportant(const nsAString& aProperty)
{
  nsCSSProperty propID = nsCSSProps::LookupProperty(aProperty);
  return GetValueIsImportant(propID);
}

PRBool
nsCSSDeclaration::GetValueIsImportant(nsCSSProperty aProperty)
{
  nsCSSValue val;

  if (nsnull != mImportant) {
    mImportant->GetValue(aProperty, val);
    if (eCSSUnit_Null != val.GetUnit()) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

PRBool
nsCSSDeclaration::AllPropertiesSameImportance(PRInt32 aFirst, PRInt32 aSecond,
                                              PRInt32 aThird, PRInt32 aFourth,
                                              PRInt32 aFifth, PRInt32 aSixth,
                                              PRBool & aImportance)
{
  aImportance = GetValueIsImportant((nsCSSProperty)mOrder->ValueAt(aFirst-1));
  if ((aSecond && aImportance != GetValueIsImportant((nsCSSProperty)mOrder->ValueAt(aSecond-1))) ||
      (aThird && aImportance != GetValueIsImportant((nsCSSProperty)mOrder->ValueAt(aThird-1))) ||
      (aFourth && aImportance != GetValueIsImportant((nsCSSProperty)mOrder->ValueAt(aFourth-1))) ||
      (aFifth && aImportance != GetValueIsImportant((nsCSSProperty)mOrder->ValueAt(aFifth-1))) ||
      (aSixth && aImportance != GetValueIsImportant((nsCSSProperty)mOrder->ValueAt(aSixth-1)))) {
    return PR_FALSE;
  }
  return PR_TRUE;
}

PRBool
nsCSSDeclaration::AllPropertiesSameValue(PRInt32 aFirst, PRInt32 aSecond,
                                         PRInt32 aThird, PRInt32 aFourth)
{
  nsCSSValue firstValue, otherValue;
  // TryBorderShorthand does the bounds-checking for us; valid values there
  // are > 0; 0 is a flag for "not set".  We here are passed the actual
  // index, which comes from finding the value in the mOrder property array.
  // Of course, re-getting the mOrder value here is pretty silly.
  GetValueOrImportantValue((nsCSSProperty)mOrder->ValueAt(aFirst-1), firstValue);
  GetValueOrImportantValue((nsCSSProperty)mOrder->ValueAt(aSecond-1), otherValue);
  if (firstValue != otherValue) {
    return PR_FALSE;
  }
  GetValueOrImportantValue((nsCSSProperty)mOrder->ValueAt(aThird-1), otherValue);
  if (firstValue != otherValue) {
    return PR_FALSE;
  }
  GetValueOrImportantValue((nsCSSProperty)mOrder->ValueAt(aFourth-1), otherValue);
  if (firstValue != otherValue) {
    return PR_FALSE;
  }
  return PR_TRUE;
}

void
nsCSSDeclaration::AppendImportanceToString(PRBool aIsImportant, nsAString& aString)
{
  if (aIsImportant) {
   aString.Append(NS_LITERAL_STRING(" ! important"));
  }
}

void
nsCSSDeclaration::AppendPropertyAndValueToString(nsCSSProperty aProperty,
                                                 nsAString& aString)
{
  NS_ASSERTION(aProperty, "null CSS property passed to AppendPropertyAndValueToString.");
  aString.Append(NS_ConvertASCIItoUCS2(nsCSSProps::GetStringValue(aProperty))
                 + NS_LITERAL_STRING(": "));
  AppendValueOrImportantValueToString(aProperty, aString);
  PRBool  isImportant = GetValueIsImportant(aProperty);
  AppendImportanceToString(isImportant, aString);
  aString.Append(NS_LITERAL_STRING("; "));
}

PRBool
nsCSSDeclaration::TryBorderShorthand(nsAString & aString, PRUint32 aPropertiesSet,
                                     PRInt32 aBorderTopWidth,
                                     PRInt32 aBorderTopStyle,
                                     PRInt32 aBorderTopColor,
                                     PRInt32 aBorderBottomWidth,
                                     PRInt32 aBorderBottomStyle,
                                     PRInt32 aBorderBottomColor,
                                     PRInt32 aBorderLeftWidth,
                                     PRInt32 aBorderLeftStyle,
                                     PRInt32 aBorderLeftColor,
                                     PRInt32 aBorderRightWidth,
                                     PRInt32 aBorderRightStyle,
                                     PRInt32 aBorderRightColor)
{
  PRBool border = PR_FALSE, isImportant = PR_FALSE;
  // 0 means not in the mOrder array; otherwise it's index+1
  if (B_BORDER == aPropertiesSet
      && AllPropertiesSameValue(aBorderTopWidth, aBorderBottomWidth,
                                aBorderLeftWidth, aBorderRightWidth)
      && AllPropertiesSameValue(aBorderTopStyle, aBorderBottomStyle,
                                aBorderLeftStyle, aBorderRightStyle)
      && AllPropertiesSameValue(aBorderTopColor, aBorderBottomColor,
                                aBorderLeftColor, aBorderRightColor)) {
    border = PR_TRUE;
  }
  if (border) {
    border = PR_FALSE;
    PRBool  isWidthImportant, isStyleImportant, isColorImportant;
    if (AllPropertiesSameImportance(aBorderTopWidth, aBorderBottomWidth,
                                    aBorderLeftWidth, aBorderRightWidth,
                                    0, 0,
                                    isWidthImportant) &&
        AllPropertiesSameImportance(aBorderTopStyle, aBorderBottomStyle,
                                    aBorderLeftStyle, aBorderRightStyle,
                                    0, 0,
                                    isStyleImportant) &&
        AllPropertiesSameImportance(aBorderTopColor, aBorderBottomColor,
                                    aBorderLeftColor, aBorderRightColor,
                                    0, 0,
                                    isColorImportant)) {
      if (isWidthImportant == isStyleImportant && isWidthImportant == isColorImportant) {
        border = PR_TRUE;
        isImportant = isWidthImportant;
      }
    }
  }
  if (border) {
    aString.Append(NS_ConvertASCIItoUCS2(nsCSSProps::GetStringValue(eCSSProperty_border))
                   + NS_LITERAL_STRING(": "));

    AppendValueOrImportantValueToString(eCSSProperty_border_top_width, aString);
    aString.Append(PRUnichar(' '));

    AppendValueOrImportantValueToString(eCSSProperty_border_top_style, aString);
    aString.Append(PRUnichar(' '));

    nsAutoString valueString;
    AppendValueOrImportantValueToString(eCSSProperty_border_top_color, valueString);
    if (!valueString.Equals(NS_LITERAL_STRING("-moz-use-text-color"))) {
      /* don't output this value, it's proprietary Mozilla and  */
      /* not intended to be exposed ; we can remove it from the */
      /* values of the shorthand since this value represents the */
      /* initial value of border-*-color */
      aString.Append(valueString);
    }
    AppendImportanceToString(isImportant, aString);
    aString.Append(NS_LITERAL_STRING("; "));
  }
  return border;
}

PRBool
nsCSSDeclaration::TryBorderSideShorthand(nsAString & aString,
                                         nsCSSProperty aShorthand,
                                         PRInt32 aBorderWidth,
                                         PRInt32 aBorderStyle,
                                         PRInt32 aBorderColor)
{
  PRBool isImportant;
  if (AllPropertiesSameImportance(aBorderWidth, aBorderStyle, aBorderColor,
                                  0, 0, 0,
                                  isImportant)) {
    aString.Append(NS_ConvertASCIItoUCS2(nsCSSProps::GetStringValue(aShorthand))
                   + NS_LITERAL_STRING(":"));
    aString.Append(PRUnichar(' '));
    AppendValueOrImportantValueToString((nsCSSProperty)mOrder->ValueAt(aBorderWidth-1), aString);

    aString.Append(PRUnichar(' '));
    AppendValueOrImportantValueToString((nsCSSProperty)mOrder->ValueAt(aBorderStyle-1), aString);

    nsAutoString valueString;
    AppendValueOrImportantValueToString((nsCSSProperty)mOrder->ValueAt(aBorderColor-1), valueString);
    if (!valueString.Equals(NS_LITERAL_STRING("-moz-use-text-color"))) {
      aString.Append(NS_LITERAL_STRING(" "));
      aString.Append(valueString);
    }
    AppendImportanceToString(isImportant, aString);
    aString.Append(NS_LITERAL_STRING("; "));
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsCSSDeclaration::TryFourSidesShorthand(nsAString & aString,
                                        nsCSSProperty aShorthand,
                                        PRInt32 & aTop,
                                        PRInt32 & aBottom,
                                        PRInt32 & aLeft,
                                        PRInt32 & aRight,
                                        PRBool aClearIndexes)
{
  // 0 means not in the mOrder array; otherwise it's index+1
  PRBool isImportant;
  if (aTop && aBottom && aLeft && aRight &&
      AllPropertiesSameImportance(aTop, aBottom, aLeft, aRight,
                                  0, 0,
                                  isImportant)) {
    // all 4 properties are set, we can output a shorthand
    aString.Append(NS_ConvertASCIItoUCS2(nsCSSProps::GetStringValue(aShorthand))
                   + NS_LITERAL_STRING(": "));
    nsCSSValue topValue, bottomValue, leftValue, rightValue;
    nsCSSProperty topProp    = (nsCSSProperty)mOrder->ValueAt(aTop-1);
    nsCSSProperty bottomProp = (nsCSSProperty)mOrder->ValueAt(aBottom-1);
    nsCSSProperty leftProp   = (nsCSSProperty)mOrder->ValueAt(aLeft-1);
    nsCSSProperty rightProp  = (nsCSSProperty)mOrder->ValueAt(aRight-1);
    GetValueOrImportantValue(topProp,    topValue);
    GetValueOrImportantValue(bottomProp, bottomValue);
    GetValueOrImportantValue(leftProp,   leftValue);
    GetValueOrImportantValue(rightProp,  rightValue);
    AppendValueToString(topProp, topValue, aString);
    if (topValue != rightValue || topValue != leftValue || topValue != bottomValue) {
      aString.Append(PRUnichar(' '));
      AppendValueToString(rightProp, rightValue, aString);
      if (topValue != bottomValue || rightValue != leftValue) {
        aString.Append(PRUnichar(' '));
        AppendValueToString(bottomProp, bottomValue, aString);
        if (rightValue != leftValue) {
          aString.Append(PRUnichar(' '));
          AppendValueToString(leftProp, leftValue, aString);
        }
      }
    }
    if (aClearIndexes) {
      aTop = 0; aBottom = 0; aLeft = 0; aRight = 0;
    }
    AppendImportanceToString(isImportant, aString);
    aString.Append(NS_LITERAL_STRING("; "));
    return PR_TRUE;
  }
  return PR_FALSE;
}

void nsCSSDeclaration::DoClipShorthand(nsAString & aString,
                                       PRInt32 aTop,
                                       PRInt32 aBottom,
                                       PRInt32 aLeft,
                                       PRInt32 aRight)
{
  NS_ASSERTION((!aTop && !aBottom && !aLeft && !aRight) ||
               (aTop && aBottom && aLeft && aRight),
               "How did we manage to set only some of the clip properties?");
  if (!aTop)
    return;  // No clip set

  PRBool isImportant;
#ifdef DEBUG
  PRBool allSameImportance =
#endif
    AllPropertiesSameImportance(aTop, aBottom, aLeft, aRight, 0, 0, isImportant);
  NS_ASSERTION(allSameImportance, "Why are the clip pseudo-values of different importance??");

  aString.Append(NS_ConvertASCIItoUCS2(nsCSSProps::GetStringValue(eCSSProperty_clip))
                 + NS_LITERAL_STRING(": "));
  nsAutoString clipValue;
  if (isImportant) {
    mImportant->GetValue(eCSSProperty_clip, clipValue);
  } else {
    GetValue(eCSSProperty_clip, clipValue);
  }
  aString.Append(clipValue);
  AppendImportanceToString(isImportant, aString);
  aString.Append(NS_LITERAL_STRING("; "));
}

void
nsCSSDeclaration::TryBackgroundShorthand(nsAString & aString,
                                         PRInt32 & aBgColor,
                                         PRInt32 & aBgImage,
                                         PRInt32 & aBgRepeat,
                                         PRInt32 & aBgAttachment,
                                         PRInt32 & aBgPositionX,
                                         PRInt32 & aBgPositionY)
{
  // 0 means not in the mOrder array; otherwise it's index+1
  // check if we have at least two properties set; otherwise, no need to
  // use a shorthand
  PRBool isImportant;
  if (aBgColor && aBgImage && aBgRepeat && aBgAttachment && aBgPositionX && aBgPositionY &&
      AllPropertiesSameImportance(aBgColor, aBgImage, aBgRepeat, aBgAttachment,
                                  aBgPositionX, aBgPositionY, isImportant)) {
    aString.Append(NS_ConvertASCIItoUCS2(nsCSSProps::GetStringValue(eCSSProperty_background))
                   + NS_LITERAL_STRING(":"));

    aString.Append(PRUnichar(' '));
    AppendValueOrImportantValueToString(eCSSProperty_background_color, aString);
    aBgColor = 0;

    aString.Append(PRUnichar(' '));
    AppendValueOrImportantValueToString(eCSSProperty_background_image, aString);
    aBgImage = 0;

    aString.Append(PRUnichar(' '));
    AppendValueOrImportantValueToString(eCSSProperty_background_repeat, aString);
    aBgRepeat = 0;

    aString.Append(PRUnichar(' '));
    AppendValueOrImportantValueToString(eCSSProperty_background_attachment, aString);
    aBgAttachment = 0;

    aString.Append(PRUnichar(' '));
    UseBackgroundPosition(aString, aBgPositionX, aBgPositionY);
    AppendImportanceToString(isImportant, aString);
    aString.Append(NS_LITERAL_STRING("; "));
  }
}

void
nsCSSDeclaration::UseBackgroundPosition(nsAString & aString,
                                        PRInt32 & aBgPositionX,
                                        PRInt32 & aBgPositionY)
{
  nsAutoString backgroundXValue, backgroundYValue;
  AppendValueOrImportantValueToString(eCSSProperty_background_x_position, backgroundXValue);
  AppendValueOrImportantValueToString(eCSSProperty_background_y_position, backgroundYValue);
  aString.Append(backgroundXValue);
  if (!backgroundXValue.Equals(backgroundYValue, nsCaseInsensitiveStringComparator())) {
    // the two values are different
    aString.Append(PRUnichar(' '));
    aString.Append(backgroundYValue);
  }
  aBgPositionX = 0;
  aBgPositionY = 0;
}

#define NS_CASE_OUTPUT_PROPERTY_VALUE(_prop, _index) \
case _prop: \
          if (_index) { \
            AppendPropertyAndValueToString(property, aString); \
            _index = 0; \
          } \
          break;

#define NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(_condition, _prop, _index) \
case _prop: \
          if ((_condition) && _index) { \
            AppendPropertyAndValueToString(property, aString); \
            _index = 0; \
          } \
          break;

void nsCSSDeclaration::PropertyIsSet(PRInt32 & aPropertyIndex, PRInt32 aIndex, PRUint32 & aSet, PRUint32 aValue)
{
  aPropertyIndex = aIndex + 1;
  aSet |= aValue;
}

nsresult
nsCSSDeclaration::ToString(nsAString& aString)
{
  if (nsnull != mOrder) {
    PRInt32 count = mOrder->Count();
    PRInt32 index;
    // 0 means not in the mOrder array; otherwise it's index+1
    PRInt32 borderTopWidth = 0, borderTopStyle = 0, borderTopColor = 0;
    PRInt32 borderBottomWidth = 0, borderBottomStyle = 0, borderBottomColor = 0;
    PRInt32 borderLeftWidth = 0, borderLeftStyle = 0, borderLeftColor = 0;
    PRInt32 borderRightWidth = 0, borderRightStyle = 0, borderRightColor = 0;
    PRInt32 marginTop = 0,  marginBottom = 0,  marginLeft = 0,  marginRight = 0;
    PRInt32 paddingTop = 0, paddingBottom = 0, paddingLeft = 0, paddingRight = 0;
    PRInt32 bgColor = 0, bgImage = 0, bgRepeat = 0, bgAttachment = 0;
    PRInt32 bgPositionX = 0, bgPositionY = 0;
    PRUint32 borderPropertiesSet = 0, finalBorderPropertiesToSet = 0;
    PRInt32 clipTop = 0, clipBottom = 0, clipLeft = 0, clipRight = 0;
    for (index = 0; index < count; index++) {
      nsCSSProperty property = (nsCSSProperty)mOrder->ValueAt(index);
      switch (property) {
        case eCSSProperty_border_top_width:
          PropertyIsSet(borderTopWidth, index, borderPropertiesSet, B_BORDER_TOP_WIDTH);
          break;
        case eCSSProperty_border_bottom_width:
          PropertyIsSet(borderBottomWidth, index, borderPropertiesSet, B_BORDER_BOTTOM_WIDTH);
          break;
        case eCSSProperty_border_left_width:
          PropertyIsSet(borderLeftWidth, index, borderPropertiesSet, B_BORDER_LEFT_WIDTH);
          break;
        case eCSSProperty_border_right_width:
          PropertyIsSet(borderRightWidth, index, borderPropertiesSet, B_BORDER_RIGHT_WIDTH);
          break;

        case eCSSProperty_border_top_style:
          PropertyIsSet(borderTopStyle, index, borderPropertiesSet, B_BORDER_TOP_STYLE);
          break;
        case eCSSProperty_border_bottom_style:
          PropertyIsSet(borderBottomStyle, index, borderPropertiesSet, B_BORDER_BOTTOM_STYLE);
          break;
        case eCSSProperty_border_left_style:
          PropertyIsSet(borderLeftStyle, index, borderPropertiesSet, B_BORDER_LEFT_STYLE);
          break;
        case eCSSProperty_border_right_style:
          PropertyIsSet(borderRightStyle, index, borderPropertiesSet, B_BORDER_RIGHT_STYLE);
          break;

        case eCSSProperty_border_top_color:
          PropertyIsSet(borderTopColor, index, borderPropertiesSet, B_BORDER_TOP_COLOR);
          break;
        case eCSSProperty_border_bottom_color:
          PropertyIsSet(borderBottomColor, index, borderPropertiesSet, B_BORDER_BOTTOM_COLOR);
          break;
        case eCSSProperty_border_left_color:
          PropertyIsSet(borderLeftColor, index, borderPropertiesSet, B_BORDER_LEFT_COLOR);
          break;
        case eCSSProperty_border_right_color:
          PropertyIsSet(borderRightColor, index, borderPropertiesSet, B_BORDER_RIGHT_COLOR);
          break;

        case eCSSProperty_margin_top:            marginTop         = index+1; break;
        case eCSSProperty_margin_bottom:         marginBottom      = index+1; break;
        case eCSSProperty_margin_left:           marginLeft        = index+1; break;
        case eCSSProperty_margin_right:          marginRight       = index+1; break;

        case eCSSProperty_padding_top:           paddingTop        = index+1; break;
        case eCSSProperty_padding_bottom:        paddingBottom     = index+1; break;
        case eCSSProperty_padding_left:          paddingLeft       = index+1; break;
        case eCSSProperty_padding_right:         paddingRight      = index+1; break;

        case eCSSProperty_background_color:      bgColor           = index+1; break;
        case eCSSProperty_background_image:      bgImage           = index+1; break;
        case eCSSProperty_background_repeat:     bgRepeat          = index+1; break;
        case eCSSProperty_background_attachment: bgAttachment      = index+1; break;
        case eCSSProperty_background_x_position: bgPositionX       = index+1; break;
        case eCSSProperty_background_y_position: bgPositionY       = index+1; break;
        case eCSSProperty_clip_top:              clipTop           = index+1; break;
        case eCSSProperty_clip_bottom:           clipBottom        = index+1; break;
        case eCSSProperty_clip_left:             clipLeft          = index+1; break;
        case eCSSProperty_clip_right:            clipRight         = index+1; break;
        default:                                 ;
      }
    }

    if (!TryBorderShorthand(aString, borderPropertiesSet,
                            borderTopWidth, borderTopStyle, borderTopColor,
                            borderBottomWidth, borderBottomStyle, borderBottomColor,
                            borderLeftWidth, borderLeftStyle, borderLeftColor,
                            borderRightWidth, borderRightStyle, borderRightColor)) {
      PRUint32 borderPropertiesToSet = 0;
      if ((borderPropertiesSet & B_BORDER_STYLE) != B_BORDER_STYLE ||
          !TryFourSidesShorthand(aString, eCSSProperty_border_style,
                                 borderTopStyle, borderBottomStyle,
                                 borderLeftStyle, borderRightStyle,
                                 PR_FALSE)) {
        borderPropertiesToSet |= B_BORDER_STYLE;
      }
      if ((borderPropertiesSet & B_BORDER_COLOR) != B_BORDER_COLOR ||
          !TryFourSidesShorthand(aString, eCSSProperty_border_color,
                                 borderTopColor, borderBottomColor,
                                 borderLeftColor, borderRightColor,
                                 PR_FALSE)) {
        borderPropertiesToSet |= B_BORDER_COLOR;
      }
      if ((borderPropertiesSet & B_BORDER_WIDTH) != B_BORDER_WIDTH ||
          !TryFourSidesShorthand(aString, eCSSProperty_border_width,
                                 borderTopWidth, borderBottomWidth,
                                 borderLeftWidth, borderRightWidth,
                                 PR_FALSE)) {
        borderPropertiesToSet |= B_BORDER_WIDTH;
      }
      borderPropertiesToSet &= borderPropertiesSet;
      if (borderPropertiesToSet) {
        if ((borderPropertiesSet & B_BORDER_TOP) != B_BORDER_TOP ||
            !TryBorderSideShorthand(aString, eCSSProperty_border_top,
                                    borderTopWidth, borderTopStyle, borderTopColor)) {
          finalBorderPropertiesToSet |= B_BORDER_TOP;
        }
        if ((borderPropertiesSet & B_BORDER_LEFT) != B_BORDER_LEFT ||
            !TryBorderSideShorthand(aString, eCSSProperty_border_left,
                                    borderLeftWidth, borderLeftStyle, borderLeftColor)) {
          finalBorderPropertiesToSet |= B_BORDER_LEFT;
        }
        if ((borderPropertiesSet & B_BORDER_RIGHT) != B_BORDER_RIGHT ||
            !TryBorderSideShorthand(aString, eCSSProperty_border_right,
                                    borderRightWidth, borderRightStyle, borderRightColor)) {
          finalBorderPropertiesToSet |= B_BORDER_RIGHT;
        }
        if ((borderPropertiesSet & B_BORDER_BOTTOM) != B_BORDER_BOTTOM ||
            !TryBorderSideShorthand(aString, eCSSProperty_border_bottom,
                                    borderBottomWidth, borderBottomStyle, borderBottomColor)) {
          finalBorderPropertiesToSet |= B_BORDER_BOTTOM;
        }
        finalBorderPropertiesToSet &= borderPropertiesToSet;
      }
    }

    TryFourSidesShorthand(aString, eCSSProperty_margin,
                          marginTop, marginBottom,
                          marginLeft, marginRight,
                          PR_TRUE);
    TryFourSidesShorthand(aString, eCSSProperty_padding,
                          paddingTop, paddingBottom,
                          paddingLeft, paddingRight,
                          PR_TRUE);
    TryBackgroundShorthand(aString,
                           bgColor, bgImage, bgRepeat, bgAttachment,
                           bgPositionX, bgPositionY);

    DoClipShorthand(aString, clipTop, clipBottom, clipLeft, clipRight);

    for (index = 0; index < count; index++) {
      PRBool isImportant;
      nsCSSProperty property = (nsCSSProperty)mOrder->ValueAt(index);
      switch (property) {

        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_TOP_STYLE,
                                                  eCSSProperty_border_top_style, borderTopStyle)
        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_LEFT_STYLE,
                                                  eCSSProperty_border_left_style, borderLeftStyle)
        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_RIGHT_STYLE,
                                                  eCSSProperty_border_right_style, borderRightStyle)
        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_BOTTOM_STYLE,
                                                  eCSSProperty_border_bottom_style, borderBottomStyle)

        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_TOP_COLOR,
                                                  eCSSProperty_border_top_color, borderTopColor)
        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_LEFT_COLOR,
                                                  eCSSProperty_border_left_color, borderLeftColor)
        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_RIGHT_COLOR,
                                                  eCSSProperty_border_right_color, borderRightColor)
        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_BOTTOM_COLOR,
                                                  eCSSProperty_border_bottom_color, borderBottomColor)

        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_TOP_WIDTH,
                                                  eCSSProperty_border_top_width, borderTopWidth)
        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_LEFT_WIDTH,
                                                  eCSSProperty_border_left_width, borderLeftWidth)
        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_RIGHT_WIDTH,
                                                  eCSSProperty_border_right_width, borderRightWidth)
        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(finalBorderPropertiesToSet & B_BORDER_BOTTOM_WIDTH,
                                                  eCSSProperty_border_bottom_width, borderBottomWidth)

        NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_margin_top, marginTop)
        NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_margin_bottom, marginBottom)
        NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_margin_left, marginLeft)
        NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_margin_right, marginRight)

        NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_padding_top, paddingTop)
        NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_padding_bottom, paddingBottom)
        NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_padding_left, paddingLeft)
        NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_padding_right, paddingRight)

        NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_background_color, bgColor)
        NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_background_image, bgImage)
        NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_background_repeat, bgRepeat)
        NS_CASE_OUTPUT_PROPERTY_VALUE(eCSSProperty_background_attachment, bgAttachment)

        // The clip "shorthand" must always consume the "longhand"
        // properties, since those are not real CSS properties in
        // any case.
        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(PR_FALSE,
                                                  eCSSProperty_clip_top,
                                                  clipTop)
        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(PR_FALSE,
                                                  eCSSProperty_clip_right,
                                                  clipRight)
        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(PR_FALSE,
                                                  eCSSProperty_clip_bottom,
                                                  clipBottom)
        NS_CASE_CONDITIONAL_OUTPUT_PROPERTY_VALUE(PR_FALSE,
                                                  eCSSProperty_clip_left,
                                                  clipLeft)
        
        case eCSSProperty_background_x_position:
        case eCSSProperty_background_y_position:
          // 0 means not in the mOrder array; otherwise it's index+1
          if (bgPositionX && bgPositionY &&
              AllPropertiesSameImportance(bgPositionX, bgPositionY,
                                          0, 0, 0, 0, isImportant)) {
            aString.Append(NS_ConvertASCIItoUCS2(nsCSSProps::GetStringValue(eCSSProperty_background_position))
                           + NS_LITERAL_STRING(": "));
            UseBackgroundPosition(aString, bgPositionX, bgPositionY);
            AppendImportanceToString(isImportant, aString);
            aString.Append(NS_LITERAL_STRING("; "));
          }
          else if (eCSSProperty_background_x_position == property && bgPositionX) {
            AppendPropertyAndValueToString(eCSSProperty_background_x_position, aString);
            bgPositionX = 0;
          }
          else if (eCSSProperty_background_y_position == property && bgPositionY) {
            AppendPropertyAndValueToString(eCSSProperty_background_y_position, aString);
            bgPositionY = 0;
          }
          break;

        default:
          if (0 <= property) {
            AppendPropertyAndValueToString(property, aString);
          }
          break;
      }
    }
  }
  if (! aString.IsEmpty()) {
    // if the string is not empty, we have a trailing whitespace we should remove
    aString.Truncate(aString.Length() - 1);
  }
  return NS_OK;
}

#ifdef DEBUG
void nsCSSDeclaration::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("{ ", out);

  CSS_VARONSTACK_GET(Font);
  if (nsnull != theFont) {
    theFont->List(out);
  }
  CSS_VARONSTACK_GET(Color);
  if (nsnull != theColor) {
    theColor->List(out);
  }
  CSS_VARONSTACK_GET(Text);
  if (nsnull != theText) {
    theText->List(out);
  }
  CSS_VARONSTACK_GET(Display);
  if (nsnull != theDisplay) {
    theDisplay->List(out);
  }
  CSS_VARONSTACK_GET(Margin);
  if (nsnull != theMargin) {
    theMargin->List(out);
  }
  CSS_VARONSTACK_GET(Position);
  if (nsnull != thePosition) {
    thePosition->List(out);
  }
  CSS_VARONSTACK_GET(List);
  if (nsnull != theList) {
    theList->List(out);
  }
  CSS_VARONSTACK_GET(Table);
  if (nsnull != theTable) {
    theTable->List(out);
  }
  CSS_VARONSTACK_GET(Breaks);
  if (nsnull != theBreaks) {
    theBreaks->List(out);
  }
  CSS_VARONSTACK_GET(Page);
  if (nsnull != thePage) {
    thePage->List(out);
  }
  CSS_VARONSTACK_GET(Content);
  if (nsnull != theContent) {
    theContent->List(out);
  }
  CSS_VARONSTACK_GET(UserInterface);
  if (nsnull != theUserInterface) {
    theUserInterface->List(out);
  }
  CSS_VARONSTACK_GET(Aural);
  if (nsnull != theAural) {
    theAural->List(out);
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
*  Self (reported as nsCSSDeclaration's size): 
*    1) sizeof(*this) + the sizeof each non-null attribute
*
*  Contained / Aggregated data (not reported as nsCSSDeclaration's size):
*    none
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void nsCSSDeclaration::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);
  if(! uniqueItems->AddItem((void*)this)){
    return;
  }

  // create a tag for this instance
  nsCOMPtr<nsIAtom> tag = do_GetAtom("nsCSSDeclaration");
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);

  // now add in all of the contained objects, checking for duplicates on all of them
  CSS_VARONSTACK_GET(Font);
  if(theFont && uniqueItems->AddItem(theFont)){
    aSize += sizeof(*theFont);
  }
  CSS_VARONSTACK_GET(Color);
  if(theColor && uniqueItems->AddItem(theColor)){
    aSize += sizeof(*theColor);
  }
  CSS_VARONSTACK_GET(Text);
  if(theText && uniqueItems->AddItem(theText)){
    aSize += sizeof(*theText);
  }
  CSS_VARONSTACK_GET(Margin);
  if(theMargin && uniqueItems->AddItem(theMargin)){
    aSize += sizeof(*theMargin);
  }
  CSS_VARONSTACK_GET(Position);
  if(thePosition && uniqueItems->AddItem(thePosition)){
    aSize += sizeof(*thePosition);
  }
  CSS_VARONSTACK_GET(List);
  if(theList && uniqueItems->AddItem(theList)){
    aSize += sizeof(*theList);
  }
  CSS_VARONSTACK_GET(Display);
  if(theDisplay && uniqueItems->AddItem(theDisplay)){
    aSize += sizeof(*theDisplay);
  }
  CSS_VARONSTACK_GET(Table);
  if(theTable && uniqueItems->AddItem(theTable)){
    aSize += sizeof(*theTable);
  }
  CSS_VARONSTACK_GET(Breaks);
  if(theBreaks && uniqueItems->AddItem(theBreaks)){
    aSize += sizeof(*theBreaks);
  }
  CSS_VARONSTACK_GET(Page);
  if(thePage && uniqueItems->AddItem(thePage)){
    aSize += sizeof(*thePage);
  }
  CSS_VARONSTACK_GET(Content);
  if(theContent && uniqueItems->AddItem(theContent)){
    aSize += sizeof(*theContent);
  }
  CSS_VARONSTACK_GET(UserInterface);
  if(theUserInterface && uniqueItems->AddItem(theUserInterface)){
    aSize += sizeof(*theUserInterface);
  }
#ifdef INCLUDE_XUL
  CSS_VARONSTACK_GET(XUL);
  if(theXUL && uniqueItems->AddItem(theXUL)){
    aSize += sizeof(*theXUL);
  }
#endif
#ifdef MOZ_SVG
  CSS_VARONSTACK_GET(SVG);
  if(theSVG && uniqueItems->AddItem(theSVG)){
    aSize += sizeof(*theSVG);
  }
#endif
  CSS_VARONSTACK_GET(Aural);
  if(theAural && uniqueItems->AddItem(theAural)){
    aSize += sizeof(*theAural);
  }
  aSizeOfHandler->AddSize(tag, aSize);
}
#endif

PRUint32
nsCSSDeclaration::Count()
{
  if (nsnull == mOrder) {
    return 0;
  }
  return (PRUint32)mOrder->Count();
}

nsresult
nsCSSDeclaration::GetNthProperty(PRUint32 aIndex, nsAString& aReturn)
{
  aReturn.Truncate();
  if (nsnull != mOrder && aIndex < (PRUint32)mOrder->Count()) {
    nsCSSProperty property = (nsCSSProperty)mOrder->ValueAt(aIndex);
    if (0 <= property) {
      aReturn.Append(NS_ConvertASCIItoUCS2(nsCSSProps::GetStringValue(property)));
    }
  }
  
  return NS_OK;
}

nsChangeHint
nsCSSDeclaration::GetStyleImpact() const
{
  nsChangeHint hint = NS_STYLE_HINT_NONE;
  if (nsnull != mOrder) {
    PRInt32 count = mOrder->Count();
    PRInt32 index;
    for (index = 0; index < count; index++) {
      nsCSSProperty property = (nsCSSProperty)mOrder->ValueAt(index);
      if (eCSSProperty_UNKNOWN < property) {
        NS_UpdateHint(hint, nsCSSProps::kHintTable[property]);
      }
    }
  }
  return hint;
}

nsCSSDeclaration*
nsCSSDeclaration::Clone() const
{
  return new nsCSSDeclaration(*this);
}


NS_EXPORT nsresult
  NS_NewCSSDeclaration(nsCSSDeclaration** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCSSDeclaration *it = new nsCSSDeclaration;

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aInstancePtrResult = it;
  return NS_OK;
}
