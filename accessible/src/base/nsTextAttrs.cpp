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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#include "nsTextAttrs.h"

#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsHyperTextAccessibleWrap.h"

#include "gfxFont.h"
#include "gfxUserFontSet.h"
#include "nsFontMetrics.h"
#include "nsLayoutUtils.h"

////////////////////////////////////////////////////////////////////////////////
// Constants and structures

/**
 * Item of the gCSSTextAttrsMap map.
 */
struct nsCSSTextAttrMapItem
{
  const char* mCSSName;
  const char* mCSSValue;
  nsIAtom** mAttrName;
  const char* mAttrValue;
};

/**
 * The map of CSS properties to text attributes.
 */
const char* const kAnyValue = nsnull;
const char* const kCopyValue = nsnull;

static nsCSSTextAttrMapItem gCSSTextAttrsMap[] =
{
  // CSS name            CSS value        Attribute name                                Attribute value
  { "color",             kAnyValue,       &nsGkAtoms::color,                 kCopyValue },
  { "font-family",       kAnyValue,       &nsGkAtoms::font_family,            kCopyValue },
  { "font-style",        kAnyValue,       &nsGkAtoms::font_style,             kCopyValue },
  { "text-decoration",   "line-through",  &nsGkAtoms::textLineThroughStyle,  "solid" },
  { "text-decoration",   "underline",     &nsGkAtoms::textUnderlineStyle,    "solid" },
  { "vertical-align",    kAnyValue,       &nsGkAtoms::textPosition,          kCopyValue }
};

////////////////////////////////////////////////////////////////////////////////
// nsTextAttrs

nsTextAttrsMgr::nsTextAttrsMgr(nsHyperTextAccessible *aHyperTextAcc,
                               PRBool aIncludeDefAttrs,
                               nsAccessible *aOffsetAcc,
                               PRInt32 aOffsetAccIdx) :
  mHyperTextAcc(aHyperTextAcc), mIncludeDefAttrs(aIncludeDefAttrs),
  mOffsetAcc(aOffsetAcc), mOffsetAccIdx(aOffsetAccIdx)
{
}

nsresult
nsTextAttrsMgr::GetAttributes(nsIPersistentProperties *aAttributes,
                              PRInt32 *aStartHTOffset,
                              PRInt32 *aEndHTOffset)
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
                  "Wrong usage of nsTextAttrsMgr!");

  // Embedded objects are combined into own range with empty attributes set.
  if (mOffsetAcc && nsAccUtils::IsEmbeddedObject(mOffsetAcc)) {
    for (PRInt32 childIdx = mOffsetAccIdx - 1; childIdx >= 0; childIdx--) {
      nsAccessible *currAcc = mHyperTextAcc->GetChildAt(childIdx);
      if (!nsAccUtils::IsEmbeddedObject(currAcc))
        break;

      (*aStartHTOffset)--;
    }

    PRInt32 childCount = mHyperTextAcc->GetChildCount();
    for (PRInt32 childIdx = mOffsetAccIdx + 1; childIdx < childCount;
         childIdx++) {
      nsAccessible *currAcc = mHyperTextAcc->GetChildAt(childIdx);
      if (!nsAccUtils::IsEmbeddedObject(currAcc))
        break;

      (*aEndHTOffset)++;
    }

    return NS_OK;
  }

  // Get the content and frame of the accessible. In the case of document
  // accessible it's role content and root frame.
  nsIContent *hyperTextElm = mHyperTextAcc->GetContent();
  nsIFrame *rootFrame = mHyperTextAcc->GetFrame();
  NS_ASSERTION(rootFrame, "No frame for accessible!");
  if (!rootFrame)
    return NS_OK;

  nsIContent *offsetNode = nsnull, *offsetElm = nsnull;
  nsIFrame *frame = nsnull;
  if (mOffsetAcc) {
    offsetNode = mOffsetAcc->GetContent();
    offsetElm = nsCoreUtils::GetDOMElementFor(offsetNode);
    frame = offsetElm->GetPrimaryFrame();
  }

  nsTArray<nsITextAttr*> textAttrArray(10);

  // "language" text attribute
  nsLangTextAttr langTextAttr(mHyperTextAcc, hyperTextElm, offsetNode);
  textAttrArray.AppendElement(static_cast<nsITextAttr*>(&langTextAttr));

  // "color" text attribute
  nsCSSTextAttr colorTextAttr(0, hyperTextElm, offsetElm);
  textAttrArray.AppendElement(static_cast<nsITextAttr*>(&colorTextAttr));

  // "font-family" text attribute
  nsCSSTextAttr fontFamilyTextAttr(1, hyperTextElm, offsetElm);
  textAttrArray.AppendElement(static_cast<nsITextAttr*>(&fontFamilyTextAttr));

  // "font-style" text attribute
  nsCSSTextAttr fontStyleTextAttr(2, hyperTextElm, offsetElm);
  textAttrArray.AppendElement(static_cast<nsITextAttr*>(&fontStyleTextAttr));

  // "text-line-through-style" text attribute
  nsCSSTextAttr lineThroughTextAttr(3, hyperTextElm, offsetElm);
  textAttrArray.AppendElement(static_cast<nsITextAttr*>(&lineThroughTextAttr));

  // "text-underline-style" text attribute
  nsCSSTextAttr underlineTextAttr(4, hyperTextElm, offsetElm);
  textAttrArray.AppendElement(static_cast<nsITextAttr*>(&underlineTextAttr));

  // "text-position" text attribute
  nsCSSTextAttr posTextAttr(5, hyperTextElm, offsetElm);
  textAttrArray.AppendElement(static_cast<nsITextAttr*>(&posTextAttr));

  // "background-color" text attribute
  nsBGColorTextAttr bgColorTextAttr(rootFrame, frame);
  textAttrArray.AppendElement(static_cast<nsITextAttr*>(&bgColorTextAttr));

  // "font-size" text attribute
  nsFontSizeTextAttr fontSizeTextAttr(rootFrame, frame);
  textAttrArray.AppendElement(static_cast<nsITextAttr*>(&fontSizeTextAttr));

  // "font-weight" text attribute
  nsFontWeightTextAttr fontWeightTextAttr(rootFrame, frame);
  textAttrArray.AppendElement(static_cast<nsITextAttr*>(&fontWeightTextAttr));

  // Expose text attributes if applicable.
  if (aAttributes) {
    PRUint32 len = textAttrArray.Length();
    for (PRUint32 idx = 0; idx < len; idx++) {
      nsITextAttr *textAttr = textAttrArray[idx];

      nsAutoString value;
      if (textAttr->GetValue(value, mIncludeDefAttrs))
        nsAccUtils::SetAccAttr(aAttributes, textAttr->GetName(), value);
    }
  }

  nsresult rv = NS_OK;

  // Expose text attributes range where they are applied if applicable.
  if (mOffsetAcc)
    rv = GetRange(textAttrArray, aStartHTOffset, aEndHTOffset);

  textAttrArray.Clear();
  return rv;
}

nsresult
nsTextAttrsMgr::GetRange(const nsTArray<nsITextAttr*>& aTextAttrArray,
                         PRInt32 *aStartHTOffset, PRInt32 *aEndHTOffset)
{
  PRUint32 attrLen = aTextAttrArray.Length();

  // Navigate backward from anchor accessible to find start offset.
  for (PRInt32 childIdx = mOffsetAccIdx - 1; childIdx >= 0; childIdx--) {
    nsAccessible *currAcc = mHyperTextAcc->GetChildAt(childIdx);

    // Stop on embedded accessible since embedded accessibles are combined into
    // own range.
    if (nsAccUtils::IsEmbeddedObject(currAcc))
      break;

    nsIContent *currElm = nsCoreUtils::GetDOMElementFor(currAcc->GetContent());
    NS_ENSURE_STATE(currElm);

    PRBool offsetFound = PR_FALSE;
    for (PRUint32 attrIdx = 0; attrIdx < attrLen; attrIdx++) {
      nsITextAttr *textAttr = aTextAttrArray[attrIdx];
      if (!textAttr->Equal(currElm)) {
        offsetFound = PR_TRUE;
        break;
      }
    }

    if (offsetFound)
      break;

    *(aStartHTOffset) -= nsAccUtils::TextLength(currAcc);
  }

  // Navigate forward from anchor accessible to find end offset.
  PRInt32 childLen = mHyperTextAcc->GetChildCount();
  for (PRInt32 childIdx = mOffsetAccIdx + 1; childIdx < childLen; childIdx++) {
    nsAccessible *currAcc = mHyperTextAcc->GetChildAt(childIdx);
    if (nsAccUtils::IsEmbeddedObject(currAcc))
      break;

    nsIContent *currElm = nsCoreUtils::GetDOMElementFor(currAcc->GetContent());
    NS_ENSURE_STATE(currElm);

    PRBool offsetFound = PR_FALSE;
    for (PRUint32 attrIdx = 0; attrIdx < attrLen; attrIdx++) {
      nsITextAttr *textAttr = aTextAttrArray[attrIdx];

      // Alter the end offset when text attribute changes its value and stop
      // the search.
      if (!textAttr->Equal(currElm)) {
        offsetFound = PR_TRUE;
        break;
      }
    }

    if (offsetFound)
      break;

    (*aEndHTOffset) += nsAccUtils::TextLength(currAcc);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsLangTextAttr

nsLangTextAttr::nsLangTextAttr(nsHyperTextAccessible *aRootAcc, 
                               nsIContent *aRootContent, nsIContent *aContent) :
  nsTextAttr<nsAutoString>(aContent == nsnull), mRootContent(aRootContent)
{
  nsresult rv = aRootAcc->GetLanguage(mRootNativeValue);
  mIsRootDefined = NS_SUCCEEDED(rv) && !mRootNativeValue.IsEmpty();

  if (aContent)
    mIsDefined = GetLang(aContent, mNativeValue);
}

PRBool
nsLangTextAttr::GetValueFor(nsIContent *aElm, nsAutoString *aValue)
{
  return GetLang(aElm, *aValue);
}

void
nsLangTextAttr::Format(const nsAutoString& aValue, nsAString& aFormattedValue)
{
  aFormattedValue = aValue;
}

PRBool
nsLangTextAttr::GetLang(nsIContent *aContent, nsAString& aLang)
{
  nsCoreUtils::GetLanguageFor(aContent, mRootContent, aLang);
  return !aLang.IsEmpty();
}


////////////////////////////////////////////////////////////////////////////////
// nsCSSTextAttr
////////////////////////////////////////////////////////////////////////////////

nsCSSTextAttr::nsCSSTextAttr(PRUint32 aIndex, nsIContent *aRootContent,
                             nsIContent *aContent) :
  nsTextAttr<nsAutoString>(aContent == nsnull), mIndex(aIndex)
{
  mIsRootDefined = GetValueFor(aRootContent, &mRootNativeValue);

  if (aContent)
    mIsDefined = GetValueFor(aContent, &mNativeValue);
}

nsIAtom*
nsCSSTextAttr::GetName() const
{
  return *gCSSTextAttrsMap[mIndex].mAttrName;
}

PRBool
nsCSSTextAttr::GetValueFor(nsIContent *aContent, nsAutoString *aValue)
{
  nsCOMPtr<nsIDOMCSSStyleDeclaration> currStyleDecl =
    nsCoreUtils::GetComputedStyleDeclaration(EmptyString(), aContent);
  if (!currStyleDecl)
    return PR_FALSE;

  NS_ConvertASCIItoUTF16 cssName(gCSSTextAttrsMap[mIndex].mCSSName);

  nsresult rv = currStyleDecl->GetPropertyValue(cssName, *aValue);
  if (NS_FAILED(rv))
    return PR_TRUE;

  const char *cssValue = gCSSTextAttrsMap[mIndex].mCSSValue;
  if (cssValue != kAnyValue && !aValue->EqualsASCII(cssValue))
    return PR_FALSE;

  return PR_TRUE;
}

void
nsCSSTextAttr::Format(const nsAutoString& aValue, nsAString& aFormattedValue)
{
  const char *attrValue = gCSSTextAttrsMap[mIndex].mAttrValue;
  if (attrValue != kCopyValue)
    AppendASCIItoUTF16(attrValue, aFormattedValue);
  else
    aFormattedValue = aValue;
}


////////////////////////////////////////////////////////////////////////////////
// nsBackgroundTextAttr
////////////////////////////////////////////////////////////////////////////////

nsBGColorTextAttr::nsBGColorTextAttr(nsIFrame *aRootFrame, nsIFrame *aFrame) :
  nsTextAttr<nscolor>(aFrame == nsnull), mRootFrame(aRootFrame)
{
  mIsRootDefined = GetColor(mRootFrame, &mRootNativeValue);
  if (aFrame)
    mIsDefined = GetColor(aFrame, &mNativeValue);
}

PRBool
nsBGColorTextAttr::GetValueFor(nsIContent *aContent, nscolor *aValue)
{
  nsIFrame *frame = aContent->GetPrimaryFrame();
  if (!frame)
    return PR_FALSE;

  return GetColor(frame, aValue);
}

void
nsBGColorTextAttr::Format(const nscolor& aValue, nsAString& aFormattedValue)
{
  // Combine the string like rgb(R, G, B) from nscolor.
  nsAutoString value;
  value.AppendLiteral("rgb(");
  value.AppendInt(NS_GET_R(aValue));
  value.AppendLiteral(", ");
  value.AppendInt(NS_GET_G(aValue));
  value.AppendLiteral(", ");
  value.AppendInt(NS_GET_B(aValue));
  value.Append(')');

  aFormattedValue = value;
}

PRBool
nsBGColorTextAttr::GetColor(nsIFrame *aFrame, nscolor *aColor)
{
  const nsStyleBackground *styleBackground = aFrame->GetStyleBackground();

  if (NS_GET_A(styleBackground->mBackgroundColor) > 0) {
    *aColor = styleBackground->mBackgroundColor;
    return PR_TRUE;
  }

  nsIFrame *parentFrame = aFrame->GetParent();
  if (!parentFrame) {
    *aColor = aFrame->PresContext()->DefaultBackgroundColor();
    return PR_TRUE;
  }

  // Each frame of parents chain for the initially passed 'aFrame' has
  // transparent background color. So background color isn't changed from
  // 'mRootFrame' to initially passed 'aFrame'.
  if (parentFrame == mRootFrame)
    return PR_FALSE;

  return GetColor(parentFrame, aColor);
}


////////////////////////////////////////////////////////////////////////////////
// nsFontSizeTextAttr
////////////////////////////////////////////////////////////////////////////////

nsFontSizeTextAttr::nsFontSizeTextAttr(nsIFrame *aRootFrame, nsIFrame *aFrame) :
  nsTextAttr<nscoord>(aFrame == nsnull)
{
  mDC = aRootFrame->PresContext()->DeviceContext();

  mRootNativeValue = GetFontSize(aRootFrame);
  mIsRootDefined = PR_TRUE;

  if (aFrame) {
    mNativeValue = GetFontSize(aFrame);
    mIsDefined = PR_TRUE;
  }
}

PRBool
nsFontSizeTextAttr::GetValueFor(nsIContent *aContent, nscoord *aValue)
{
  nsIFrame *frame = aContent->GetPrimaryFrame();
  if (!frame)
    return PR_FALSE;
  
  *aValue = GetFontSize(frame);
  return PR_TRUE;
}

void
nsFontSizeTextAttr::Format(const nscoord& aValue, nsAString& aFormattedValue)
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
  aFormattedValue = value;
}

nscoord
nsFontSizeTextAttr::GetFontSize(nsIFrame *aFrame)
{
  return aFrame->GetStyleFont()->mSize;
}


////////////////////////////////////////////////////////////////////////////////
// nsFontWeightTextAttr
////////////////////////////////////////////////////////////////////////////////

nsFontWeightTextAttr::nsFontWeightTextAttr(nsIFrame *aRootFrame,
                                           nsIFrame *aFrame) :
  nsTextAttr<PRInt32>(aFrame == nsnull)
{
  mRootNativeValue = GetFontWeight(aRootFrame);
  mIsRootDefined = PR_TRUE;

  if (aFrame) {
    mNativeValue = GetFontWeight(aFrame);
    mIsDefined = PR_TRUE;
  }
}

PRBool
nsFontWeightTextAttr::GetValueFor(nsIContent *aContent, PRInt32 *aValue)
{
  nsIFrame *frame = aContent->GetPrimaryFrame();
  if (!frame)
    return PR_FALSE;

  *aValue = GetFontWeight(frame);
  return PR_TRUE;
}

void
nsFontWeightTextAttr::Format(const PRInt32& aValue, nsAString& aFormattedValue)
{
  nsAutoString value;
  value.AppendInt(aValue);
  aFormattedValue = value;
}

PRInt32
nsFontWeightTextAttr::GetFontWeight(nsIFrame *aFrame)
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
