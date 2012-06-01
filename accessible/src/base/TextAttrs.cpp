/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextAttrs.h"

#include "HyperTextAccessibleWrap.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "StyleInfo.h"

#include "gfxFont.h"
#include "gfxUserFontSet.h"
#include "nsFontMetrics.h"
#include "nsLayoutUtils.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// TextAttrsMgr
////////////////////////////////////////////////////////////////////////////////

void
TextAttrsMgr::GetAttributes(nsIPersistentProperties* aAttributes,
                            PRInt32* aStartHTOffset,
                            PRInt32* aEndHTOffset)
{
  // 1. Hyper text accessible must be specified always.
  // 2. Offset accessible and result hyper text offsets must be specified in
  // the case of text attributes.
  // 3. Offset accessible and result hyper text offsets must not be specified
  // but include default text attributes flag and attributes list must be
  // specified in the case of default text attributes.
  NS_PRECONDITION(mHyperTextAcc &&
                  ((mOffsetAcc && mOffsetAccIdx != -1 &&
                    aStartHTOffset && aEndHTOffset) ||
                  (!mOffsetAcc && mOffsetAccIdx == -1 &&
                    !aStartHTOffset && !aEndHTOffset &&
                   mIncludeDefAttrs && aAttributes)),
                  "Wrong usage of TextAttrsMgr!");

  // Embedded objects are combined into own range with empty attributes set.
  if (mOffsetAcc && nsAccUtils::IsEmbeddedObject(mOffsetAcc)) {
    for (PRInt32 childIdx = mOffsetAccIdx - 1; childIdx >= 0; childIdx--) {
      Accessible* currAcc = mHyperTextAcc->GetChildAt(childIdx);
      if (!nsAccUtils::IsEmbeddedObject(currAcc))
        break;

      (*aStartHTOffset)--;
    }

    PRUint32 childCount = mHyperTextAcc->ChildCount();
    for (PRUint32 childIdx = mOffsetAccIdx + 1; childIdx < childCount;
         childIdx++) {
      Accessible* currAcc = mHyperTextAcc->GetChildAt(childIdx);
      if (!nsAccUtils::IsEmbeddedObject(currAcc))
        break;

      (*aEndHTOffset)++;
    }

    return;
  }

  // Get the content and frame of the accessible. In the case of document
  // accessible it's role content and root frame.
  nsIContent *hyperTextElm = mHyperTextAcc->GetContent();
  nsIFrame *rootFrame = mHyperTextAcc->GetFrame();
  NS_ASSERTION(rootFrame, "No frame for accessible!");
  if (!rootFrame)
    return;

  nsIContent *offsetNode = nsnull, *offsetElm = nsnull;
  nsIFrame *frame = nsnull;
  if (mOffsetAcc) {
    offsetNode = mOffsetAcc->GetContent();
    offsetElm = nsCoreUtils::GetDOMElementFor(offsetNode);
    frame = offsetElm->GetPrimaryFrame();
  }

  // "language" text attribute
  LangTextAttr langTextAttr(mHyperTextAcc, hyperTextElm, offsetNode);

  // "background-color" text attribute
  BGColorTextAttr bgColorTextAttr(rootFrame, frame);

  // "color" text attribute
  ColorTextAttr colorTextAttr(rootFrame, frame);

  // "font-family" text attribute
  FontFamilyTextAttr fontFamilyTextAttr(rootFrame, frame);

  // "font-size" text attribute
  FontSizeTextAttr fontSizeTextAttr(rootFrame, frame);

  // "font-style" text attribute
  FontStyleTextAttr fontStyleTextAttr(rootFrame, frame);

  // "font-weight" text attribute
  FontWeightTextAttr fontWeightTextAttr(rootFrame, frame);

  // "text-underline(line-through)-style(color)" text attributes
  TextDecorTextAttr textDecorTextAttr(rootFrame, frame);

  // "text-position" text attribute
  TextPosTextAttr textPosTextAttr(rootFrame, frame);

  TextAttr* attrArray[] =
  {
    &langTextAttr,
    &bgColorTextAttr,
    &colorTextAttr,
    &fontFamilyTextAttr,
    &fontSizeTextAttr,
    &fontStyleTextAttr,
    &fontWeightTextAttr,
    &textDecorTextAttr,
    &textPosTextAttr
  };

  // Expose text attributes if applicable.
  if (aAttributes) {
    for (PRUint32 idx = 0; idx < ArrayLength(attrArray); idx++)
      attrArray[idx]->Expose(aAttributes, mIncludeDefAttrs);
  }

  // Expose text attributes range where they are applied if applicable.
  if (mOffsetAcc)
    GetRange(attrArray, ArrayLength(attrArray), aStartHTOffset, aEndHTOffset);
}

void
TextAttrsMgr::GetRange(TextAttr* aAttrArray[], PRUint32 aAttrArrayLen,
                       PRInt32* aStartHTOffset, PRInt32* aEndHTOffset)
{
  // Navigate backward from anchor accessible to find start offset.
  for (PRInt32 childIdx = mOffsetAccIdx - 1; childIdx >= 0; childIdx--) {
    Accessible* currAcc = mHyperTextAcc->GetChildAt(childIdx);

    // Stop on embedded accessible since embedded accessibles are combined into
    // own range.
    if (nsAccUtils::IsEmbeddedObject(currAcc))
      break;

    nsIContent* currElm = nsCoreUtils::GetDOMElementFor(currAcc->GetContent());
    if (!currElm)
      return;

    bool offsetFound = false;
    for (PRUint32 attrIdx = 0; attrIdx < aAttrArrayLen; attrIdx++) {
      TextAttr* textAttr = aAttrArray[attrIdx];
      if (!textAttr->Equal(currElm)) {
        offsetFound = true;
        break;
      }
    }

    if (offsetFound)
      break;

    *(aStartHTOffset) -= nsAccUtils::TextLength(currAcc);
  }

  // Navigate forward from anchor accessible to find end offset.
  PRUint32 childLen = mHyperTextAcc->ChildCount();
  for (PRUint32 childIdx = mOffsetAccIdx + 1; childIdx < childLen; childIdx++) {
    Accessible* currAcc = mHyperTextAcc->GetChildAt(childIdx);
    if (nsAccUtils::IsEmbeddedObject(currAcc))
      break;

    nsIContent* currElm = nsCoreUtils::GetDOMElementFor(currAcc->GetContent());
    if (!currElm)
      return;

    bool offsetFound = false;
    for (PRUint32 attrIdx = 0; attrIdx < aAttrArrayLen; attrIdx++) {
      TextAttr* textAttr = aAttrArray[attrIdx];

      // Alter the end offset when text attribute changes its value and stop
      // the search.
      if (!textAttr->Equal(currElm)) {
        offsetFound = true;
        break;
      }
    }

    if (offsetFound)
      break;

    (*aEndHTOffset) += nsAccUtils::TextLength(currAcc);
  }
}


////////////////////////////////////////////////////////////////////////////////
// LangTextAttr
////////////////////////////////////////////////////////////////////////////////

TextAttrsMgr::LangTextAttr::
  LangTextAttr(HyperTextAccessible* aRoot,
               nsIContent* aRootElm, nsIContent* aElm) :
  TTextAttr<nsString>(!aElm), mRootContent(aRootElm)
{
  aRoot->Language(mRootNativeValue);
  mIsRootDefined =  !mRootNativeValue.IsEmpty();

  if (aElm)
    mIsDefined = GetLang(aElm, mNativeValue);
}

bool
TextAttrsMgr::LangTextAttr::
  GetValueFor(nsIContent* aElm, nsString* aValue)
{
  return GetLang(aElm, *aValue);
}

void
TextAttrsMgr::LangTextAttr::
  ExposeValue(nsIPersistentProperties* aAttributes, const nsString& aValue)
{
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::language, aValue);
}

bool
TextAttrsMgr::LangTextAttr::
  GetLang(nsIContent* aElm, nsAString& aLang)
{
  nsCoreUtils::GetLanguageFor(aElm, mRootContent, aLang);
  return !aLang.IsEmpty();
}


////////////////////////////////////////////////////////////////////////////////
// BGColorTextAttr
////////////////////////////////////////////////////////////////////////////////

TextAttrsMgr::BGColorTextAttr::
  BGColorTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame) :
  TTextAttr<nscolor>(!aFrame), mRootFrame(aRootFrame)
{
  mIsRootDefined = GetColor(mRootFrame, &mRootNativeValue);
  if (aFrame)
    mIsDefined = GetColor(aFrame, &mNativeValue);
}

bool
TextAttrsMgr::BGColorTextAttr::
  GetValueFor(nsIContent* aElm, nscolor* aValue)
{
  nsIFrame* frame = aElm->GetPrimaryFrame();
  return frame ? GetColor(frame, aValue) : false;
}

void
TextAttrsMgr::BGColorTextAttr::
  ExposeValue(nsIPersistentProperties* aAttributes, const nscolor& aValue)
{
  nsAutoString formattedValue;
  StyleInfo::FormatColor(aValue, formattedValue);
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::backgroundColor,
                         formattedValue);
}

bool
TextAttrsMgr::BGColorTextAttr::
  GetColor(nsIFrame* aFrame, nscolor* aColor)
{
  const nsStyleBackground* styleBackground = aFrame->GetStyleBackground();

  if (NS_GET_A(styleBackground->mBackgroundColor) > 0) {
    *aColor = styleBackground->mBackgroundColor;
    return true;
  }

  nsIFrame *parentFrame = aFrame->GetParent();
  if (!parentFrame) {
    *aColor = aFrame->PresContext()->DefaultBackgroundColor();
    return true;
  }

  // Each frame of parents chain for the initially passed 'aFrame' has
  // transparent background color. So background color isn't changed from
  // 'mRootFrame' to initially passed 'aFrame'.
  if (parentFrame == mRootFrame)
    return false;

  return GetColor(parentFrame, aColor);
}


////////////////////////////////////////////////////////////////////////////////
// ColorTextAttr
////////////////////////////////////////////////////////////////////////////////

TextAttrsMgr::ColorTextAttr::
  ColorTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame) :
  TTextAttr<nscolor>(!aFrame)
{
  mRootNativeValue = aRootFrame->GetStyleColor()->mColor;
  mIsRootDefined = true;

  if (aFrame) {
    mNativeValue = aFrame->GetStyleColor()->mColor;
    mIsDefined = true;
  }
}

bool
TextAttrsMgr::ColorTextAttr::
  GetValueFor(nsIContent* aElm, nscolor* aValue)
{
  nsIFrame* frame = aElm->GetPrimaryFrame();
  if (frame) {
    *aValue = frame->GetStyleColor()->mColor;
    return true;
  }

  return false;
}

void
TextAttrsMgr::ColorTextAttr::
  ExposeValue(nsIPersistentProperties* aAttributes, const nscolor& aValue)
{
  nsAutoString formattedValue;
  StyleInfo::FormatColor(aValue, formattedValue);
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::color, formattedValue);
}


////////////////////////////////////////////////////////////////////////////////
// FontFamilyTextAttr
////////////////////////////////////////////////////////////////////////////////

TextAttrsMgr::FontFamilyTextAttr::
  FontFamilyTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame) :
  TTextAttr<nsString>(!aFrame)
{
  mIsRootDefined = GetFontFamily(aRootFrame, mRootNativeValue);

  if (aFrame)
    mIsDefined = GetFontFamily(aFrame, mNativeValue);
}

bool
TextAttrsMgr::FontFamilyTextAttr::
  GetValueFor(nsIContent* aElm, nsString* aValue)
{
  nsIFrame* frame = aElm->GetPrimaryFrame();
  return frame ? GetFontFamily(frame, *aValue) : false;
}

void
TextAttrsMgr::FontFamilyTextAttr::
  ExposeValue(nsIPersistentProperties* aAttributes, const nsString& aValue)
{
  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::font_family, aValue);
}

bool
TextAttrsMgr::FontFamilyTextAttr::
  GetFontFamily(nsIFrame* aFrame, nsString& aFamily)
{
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(aFrame, getter_AddRefs(fm));

  gfxFontGroup* fontGroup = fm->GetThebesFontGroup();
  gfxFont* font = fontGroup->GetFontAt(0);
  gfxFontEntry* fontEntry = font->GetFontEntry();
  aFamily = fontEntry->FamilyName();
  return true;
}


////////////////////////////////////////////////////////////////////////////////
// FontSizeTextAttr
////////////////////////////////////////////////////////////////////////////////

TextAttrsMgr::FontSizeTextAttr::
  FontSizeTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame) :
  TTextAttr<nscoord>(!aFrame)
{
  mDC = aRootFrame->PresContext()->DeviceContext();

  mRootNativeValue = aRootFrame->GetStyleFont()->mSize;
  mIsRootDefined = true;

  if (aFrame) {
    mNativeValue = aFrame->GetStyleFont()->mSize;
    mIsDefined = true;
  }
}

bool
TextAttrsMgr::FontSizeTextAttr::
  GetValueFor(nsIContent* aElm, nscoord* aValue)
{
  nsIFrame* frame = aElm->GetPrimaryFrame();
  if (frame) {
    *aValue = frame->GetStyleFont()->mSize;
    return true;
  }

  return false;
}

void
TextAttrsMgr::FontSizeTextAttr::
  ExposeValue(nsIPersistentProperties* aAttributes, const nscoord& aValue)
{
  // Convert from nscoord to pt.
  //
  // Note: according to IA2, "The conversion doesn't have to be exact.
  // The intent is to give the user a feel for the size of the text."
  //
  // ATK does not specify a unit and will likely follow IA2 here.
  //
  // XXX todo: consider sharing this code with layout module? (bug 474621)
  float px =
    NSAppUnitsToFloatPixels(aValue, nsDeviceContext::AppUnitsPerCSSPixel());
  // Each pt is 4/3 of a CSS pixel.
  int pts = NS_lround(px*3/4);

  nsAutoString value;
  value.AppendInt(pts);
  value.Append(NS_LITERAL_STRING("pt"));

  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::font_size, value);
}


////////////////////////////////////////////////////////////////////////////////
// FontStyleTextAttr
////////////////////////////////////////////////////////////////////////////////

TextAttrsMgr::FontStyleTextAttr::
  FontStyleTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame) :
  TTextAttr<nscoord>(!aFrame)
{
  mRootNativeValue = aRootFrame->GetStyleFont()->mFont.style;
  mIsRootDefined = true;

  if (aFrame) {
    mNativeValue = aFrame->GetStyleFont()->mFont.style;
    mIsDefined = true;
  }
}

bool
TextAttrsMgr::FontStyleTextAttr::
  GetValueFor(nsIContent* aContent, nscoord* aValue)
{
  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (frame) {
    *aValue = frame->GetStyleFont()->mFont.style;
    return true;
  }

  return false;
}

void
TextAttrsMgr::FontStyleTextAttr::
  ExposeValue(nsIPersistentProperties* aAttributes, const nscoord& aValue)
{
  nsAutoString formattedValue;
  StyleInfo::FormatFontStyle(aValue, formattedValue);

  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::font_style, formattedValue);
}


////////////////////////////////////////////////////////////////////////////////
// FontWeightTextAttr
////////////////////////////////////////////////////////////////////////////////

TextAttrsMgr::FontWeightTextAttr::
  FontWeightTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame) :
  TTextAttr<PRInt32>(!aFrame)
{
  mRootNativeValue = GetFontWeight(aRootFrame);
  mIsRootDefined = true;

  if (aFrame) {
    mNativeValue = GetFontWeight(aFrame);
    mIsDefined = true;
  }
}

bool
TextAttrsMgr::FontWeightTextAttr::
  GetValueFor(nsIContent* aElm, PRInt32* aValue)
{
  nsIFrame* frame = aElm->GetPrimaryFrame();
  if (frame) {
    *aValue = GetFontWeight(frame);
    return true;
  }

  return false;
}

void
TextAttrsMgr::FontWeightTextAttr::
  ExposeValue(nsIPersistentProperties* aAttributes, const PRInt32& aValue)
{
  nsAutoString formattedValue;
  formattedValue.AppendInt(aValue);

  nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::fontWeight, formattedValue);
}

PRInt32
TextAttrsMgr::FontWeightTextAttr::
  GetFontWeight(nsIFrame* aFrame)
{
  // nsFont::width isn't suitable here because it's necessary to expose real
  // value of font weight (used font might not have some font weight values).
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(aFrame, getter_AddRefs(fm));

  gfxFontGroup *fontGroup = fm->GetThebesFontGroup();
  gfxFont *font = fontGroup->GetFontAt(0);

  // When there doesn't exist a bold font in the family and so the rendering of
  // a non-bold font face is changed so that the user sees what looks like a
  // bold font, i.e. synthetic bolding is used. IsSyntheticBold method is only
  // needed on Mac, but it is "safe" to use on all platforms.  (For non-Mac
  // platforms it always return false.)
  if (font->IsSyntheticBold())
    return 700;

#ifdef MOZ_PANGO
  // On Linux, font->GetStyle()->weight will give the absolute weight requested
  // of the font face. The Linux code uses the gfxFontEntry constructor which
  // doesn't initialize the weight field.
  return font->GetStyle()->weight;
#else
  // On Windows, font->GetStyle()->weight will give the same weight as
  // fontEntry->Weight(), the weight of the first font in the font group, which
  // may not be the weight of the font face used to render the characters.
  // On Mac, font->GetStyle()->weight will just give the same number as
  // getComputedStyle(). fontEntry->Weight() will give the weight of the font
  // face used.
  gfxFontEntry *fontEntry = font->GetFontEntry();
  return fontEntry->Weight();
#endif
}


////////////////////////////////////////////////////////////////////////////////
// TextDecorTextAttr
////////////////////////////////////////////////////////////////////////////////

TextAttrsMgr::TextDecorValue::
  TextDecorValue(nsIFrame* aFrame)
{
  const nsStyleTextReset* textReset = aFrame->GetStyleTextReset();
  mStyle = textReset->GetDecorationStyle();

  bool isForegroundColor = false;
  textReset->GetDecorationColor(mColor, isForegroundColor);
  if (isForegroundColor)
    mColor = aFrame->GetStyleColor()->mColor;

  mLine = textReset->mTextDecorationLine &
    (NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE |
     NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH);
}

TextAttrsMgr::TextDecorTextAttr::
  TextDecorTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame) :
  TTextAttr<TextDecorValue>(!aFrame)
{
  mRootNativeValue = TextDecorValue(aRootFrame);
  mIsRootDefined = mRootNativeValue.IsDefined();

  if (aFrame) {
    mNativeValue = TextDecorValue(aFrame);
    mIsDefined = mNativeValue.IsDefined();
  }
}

bool
TextAttrsMgr::TextDecorTextAttr::
  GetValueFor(nsIContent* aContent, TextDecorValue* aValue)
{
  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (frame) {
    *aValue = TextDecorValue(frame);
    return aValue->IsDefined();
  }

  return false;
}

void
TextAttrsMgr::TextDecorTextAttr::
  ExposeValue(nsIPersistentProperties* aAttributes, const TextDecorValue& aValue)
{
  if (aValue.IsUnderline()) {
    nsAutoString formattedStyle;
    StyleInfo::FormatTextDecorationStyle(aValue.Style(), formattedStyle);
    nsAccUtils::SetAccAttr(aAttributes,
                           nsGkAtoms::textUnderlineStyle,
                           formattedStyle);

    nsAutoString formattedColor;
    StyleInfo::FormatColor(aValue.Color(), formattedColor);
    nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::textUnderlineColor,
                           formattedColor);
    return;
  }

  if (aValue.IsLineThrough()) {
    nsAutoString formattedStyle;
    StyleInfo::FormatTextDecorationStyle(aValue.Style(), formattedStyle);
    nsAccUtils::SetAccAttr(aAttributes,
                           nsGkAtoms::textLineThroughStyle,
                           formattedStyle);

    nsAutoString formattedColor;
    StyleInfo::FormatColor(aValue.Color(), formattedColor);
    nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::textLineThroughColor,
                           formattedColor);
  }
}

////////////////////////////////////////////////////////////////////////////////
// TextPosTextAttr
////////////////////////////////////////////////////////////////////////////////

TextAttrsMgr::TextPosTextAttr::
  TextPosTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame) :
  TTextAttr<TextPosValue>(!aFrame)
{
  mRootNativeValue = GetTextPosValue(aRootFrame);
  mIsRootDefined = mRootNativeValue != eTextPosNone;

  if (aFrame) {
    mNativeValue = GetTextPosValue(aFrame);
    mIsDefined = mNativeValue != eTextPosNone;
  }
}

bool
TextAttrsMgr::TextPosTextAttr::
  GetValueFor(nsIContent* aContent, TextPosValue* aValue)
{
  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (frame) {
    *aValue = GetTextPosValue(frame);
    return *aValue != eTextPosNone;
  }

  return false;
}

void
TextAttrsMgr::TextPosTextAttr::
  ExposeValue(nsIPersistentProperties* aAttributes, const TextPosValue& aValue)
{
  switch (aValue) {
    case eTextPosBaseline:
      nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::textPosition,
                             NS_LITERAL_STRING("baseline"));
      break;

    case eTextPosSub:
      nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::textPosition,
                             NS_LITERAL_STRING("sub"));
      break;

    case eTextPosSuper:
      nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::textPosition,
                             NS_LITERAL_STRING("super"));
      break;
  }
}

TextAttrsMgr::TextPosValue
TextAttrsMgr::TextPosTextAttr::
  GetTextPosValue(nsIFrame* aFrame) const
{
  const nsStyleCoord& styleCoord = aFrame->GetStyleTextReset()->mVerticalAlign;
  switch (styleCoord.GetUnit()) {
    case eStyleUnit_Enumerated:
      switch (styleCoord.GetIntValue()) {
        case NS_STYLE_VERTICAL_ALIGN_BASELINE:
          return eTextPosBaseline;
        case NS_STYLE_VERTICAL_ALIGN_SUB:
          return eTextPosSub;
        case NS_STYLE_VERTICAL_ALIGN_SUPER:
          return eTextPosSuper;

        // No good guess for these:
        //   NS_STYLE_VERTICAL_ALIGN_TOP
        //   NS_STYLE_VERTICAL_ALIGN_TEXT_TOP
        //   NS_STYLE_VERTICAL_ALIGN_MIDDLE
        //   NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM
        //   NS_STYLE_VERTICAL_ALIGN_BOTTOM
        //   NS_STYLE_VERTICAL_ALIGN_MIDDLE_WITH_BASELINE
        // Do not expose value of text-position attribute.

        default:
          break;
      }
      return eTextPosNone;

    case eStyleUnit_Percent:
    {
      float percentValue = styleCoord.GetPercentValue();
      return percentValue > 0 ?
        eTextPosSuper :
        (percentValue < 0 ? eTextPosSub : eTextPosBaseline);
    }

    case eStyleUnit_Coord:
    {
       nscoord coordValue = styleCoord.GetCoordValue();
       return coordValue > 0 ?
         eTextPosSuper :
         (coordValue < 0 ? eTextPosSub : eTextPosBaseline);
    }
  }

  const nsIContent* content = aFrame->GetContent();
  if (content && content->IsHTML()) {
    const nsIAtom* tagName = content->Tag();
    if (tagName == nsGkAtoms::sup) 
      return eTextPosSuper;
    if (tagName == nsGkAtoms::sub) 
      return eTextPosSub;
  }

  return eTextPosNone;
}
