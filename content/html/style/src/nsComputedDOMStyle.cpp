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
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *   Christopher A. Aillon <christopher@aillon.com>
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

#include "nsComputedDOMStyle.h"

#include "nsDOMError.h"
#include "nsDOMString.h"
#include "nsIDOMCSS2Properties.h"
#include "nsIDOMElement.h"
#include "nsStyleContext.h"
#include "nsIScrollableFrame.h"
#include "nsContentUtils.h"
#include "prprf.h"

#include "nsCSSProps.h"
#include "nsCSSKeywords.h"
#include "nsDOMCSSRect.h"
#include "nsLayoutAtoms.h"
#include "nsThemeConstants.h"

#include "nsPresContext.h"
#include "nsIDocument.h"

#include "nsCSSPseudoElements.h"
#include "nsStyleSet.h"
#include "imgIRequest.h"
#include "nsInspectorCSSUtils.h"

#if defined(DEBUG_bzbarsky) || defined(DEBUG_caillon)
#define DEBUG_ComputedDOMStyle
#endif

/*
 * This is the implementation of the readonly CSSStyleDeclaration that is
 * returned by the getComputedStyle() function.
 */

static nsComputedDOMStyle *sCachedComputedDOMStyle;

nsresult
NS_NewComputedDOMStyle(nsIComputedDOMStyle** aComputedStyle)
{
  NS_ENSURE_ARG_POINTER(aComputedStyle);

  if (sCachedComputedDOMStyle) {
    // There's an unused nsComputedDOMStyle cached, use it.
    // But before we use it, re-initialize the object.

    // Oh yeah baby, placement new!
    *aComputedStyle = new (sCachedComputedDOMStyle) nsComputedDOMStyle();

    sCachedComputedDOMStyle = nsnull;
  } else {
    // No nsComputedDOMStyle cached, create a new one.

    *aComputedStyle = new nsComputedDOMStyle();
    NS_ENSURE_TRUE(*aComputedStyle, NS_ERROR_OUT_OF_MEMORY);
  }

  NS_ADDREF(*aComputedStyle);

  return NS_OK;
}

nsComputedDOMStyle::nsComputedDOMStyle()
  : mInner(this), mPresShellWeak(nsnull), mT2P(0.0f)
{
}


nsComputedDOMStyle::~nsComputedDOMStyle()
{
}

void
nsComputedDOMStyle::Shutdown()
{
  // We want to de-allocate without calling the dtor since we
  // already did that manually in doDestroyComputedDOMStyle(),
  // so cast our cached object to something that doesn't know
  // about our dtor.
  delete NS_REINTERPRET_CAST(char*, sCachedComputedDOMStyle);
  sCachedComputedDOMStyle = nsnull;
}


// QueryInterface implementation for nsComputedDOMStyle
NS_INTERFACE_MAP_BEGIN(nsComputedDOMStyle)
  NS_INTERFACE_MAP_ENTRY(nsIComputedDOMStyle)
  NS_INTERFACE_MAP_ENTRY(nsICSSDeclaration)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleDeclaration)
  NS_INTERFACE_MAP_ENTRY_AGGREGATED(nsIDOMCSS2Properties, &mInner)
  NS_INTERFACE_MAP_ENTRY_AGGREGATED(nsIDOMNSCSS2Properties, &mInner)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIComputedDOMStyle)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(ComputedCSSStyleDeclaration)
NS_INTERFACE_MAP_END


static void doDestroyComputedDOMStyle(nsComputedDOMStyle *aComputedStyle)
{
  if (!sCachedComputedDOMStyle) {
    // The cache is empty, store aComputedStyle in the cache.

    sCachedComputedDOMStyle = aComputedStyle;
    sCachedComputedDOMStyle->~nsComputedDOMStyle();
  } else {
    // The cache is full, delete aComputedStyle

    delete aComputedStyle;
  }
}

NS_IMPL_ADDREF(nsComputedDOMStyle)
NS_IMPL_RELEASE_WITH_DESTROY(nsComputedDOMStyle,
                             doDestroyComputedDOMStyle(this))


NS_IMETHODIMP
nsComputedDOMStyle::Init(nsIDOMElement *aElement,
                         const nsAString& aPseudoElt,
                         nsIPresShell *aPresShell)
{
  NS_ENSURE_ARG_POINTER(aElement);
  NS_ENSURE_ARG_POINTER(aPresShell);

  mPresShellWeak = do_GetWeakReference(aPresShell);

  mContent = do_QueryInterface(aElement);
  if (!mContent) {
    // This should not happen, all our elements support nsIContent!

    return NS_ERROR_FAILURE;
  }

  if (!DOMStringIsNull(aPseudoElt) && !aPseudoElt.IsEmpty() &&
      aPseudoElt.First() == PRUnichar(':')) {
    // deal with two-colon forms of aPseudoElt
    nsAString::const_iterator start, end;
    aPseudoElt.BeginReading(start);
    aPseudoElt.EndReading(end);
    NS_ASSERTION(start != end, "aPseudoElt is not empty!");
    ++start;
    PRBool haveTwoColons = PR_TRUE;
    if (start == end || *start != PRUnichar(':')) {
      --start;
      haveTwoColons = PR_FALSE;
    }
    mPseudo = do_GetAtom(Substring(start, end));
    NS_ENSURE_TRUE(mPseudo, NS_ERROR_OUT_OF_MEMORY);

    // There aren't any non-CSS2 pseudo-elements with a single ':'
    if (!haveTwoColons &&
        !nsCSSPseudoElements::IsCSS2PseudoElement(mPseudo)) {
      // XXXbz I'd really rather we threw an exception or something, but
      // the DOM spec sucks.
      mPseudo = nsnull;
    }
  }

  nsCOMPtr<nsPresContext> presCtx;

  aPresShell->GetPresContext(getter_AddRefs(presCtx));

  NS_ENSURE_TRUE(presCtx, NS_ERROR_FAILURE);

  mT2P = presCtx->TwipsToPixels();

  return NS_OK;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyValue(const nsCSSProperty aPropID,
                                     nsAString& aValue)
{
  // This is mostly to avoid code duplication with GetPropertyCSSValue(); if
  // perf ever becomes an issue here (doubtful), we can look into changing
  // this.
  return GetPropertyValue(
    NS_ConvertASCIItoUTF16(nsCSSProps::GetStringValue(aPropID)),
    aValue);
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPropertyValue(const nsCSSProperty aPropID,
                                     const nsAString& aValue)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetCssText(nsAString& aCssText)
{
  aCssText.Truncate();

  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::SetCssText(const nsAString& aCssText)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetLength(PRUint32* aLength)
{
  NS_PRECONDITION(aLength, "Null aLength!  Prepare to die!");

  (void)GetQueryablePropertyMap(aLength);

  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  NS_ENSURE_ARG_POINTER(aParentRule);
  *aParentRule = nsnull;

  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyValue(const nsAString& aPropertyName,
                                     nsAString& aReturn)
{
  nsCOMPtr<nsIDOMCSSValue> val;

  aReturn.Truncate();

  nsresult rv = GetPropertyCSSValue(aPropertyName, getter_AddRefs(val));
  NS_ENSURE_SUCCESS(rv, rv);

  if (val) {
    rv = val->GetCssText(aReturn);
  }

  return rv;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyCSSValue(const nsAString& aPropertyName,
                                        nsIDOMCSSValue** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShellWeak);

  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsIDocument* document = mContent->GetDocument();
  if (document) {
    document->FlushPendingNotifications(Flush_Style);
  }

  nsIFrame *frame = nsnull;
  presShell->GetPrimaryFrameFor(mContent, &frame);

  nsresult rv = NS_OK;

  nsCSSProperty prop = nsCSSProps::LookupProperty(aPropertyName);

  PRUint32 i = 0;
  PRUint32 length = 0;
  const ComputedStyleMapEntry* propMap = GetQueryablePropertyMap(&length);
  for (; i < length; ++i) {
    if (prop == propMap[i].mProperty) {
      // Call our pointer-to-member-function.
      rv = (this->*(propMap[i].mGetter))(frame, aReturn);
      break;
    }
  }

#ifdef DEBUG_ComputedDOMStyle
  if (i == length) {
    NS_WARNING(PromiseFlatCString(NS_ConvertUCS2toUTF8(aPropertyName) + 
                                  NS_LITERAL_CSTRING(" is not queryable!")).get());
  }
#endif

  if (NS_FAILED(rv)) {
    *aReturn = nsnull;
  }

  // Release the current style context for it should be re-resolved
  // whenever a frame is not available.
  mStyleContextHolder = nsnull;

  return rv;
}


NS_IMETHODIMP
nsComputedDOMStyle::RemoveProperty(const nsAString& aPropertyName,
                                   nsAString& aReturn)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyPriority(const nsAString& aPropertyName,
                                        nsAString& aReturn)
{
  aReturn.Truncate();

  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::SetProperty(const nsAString& aPropertyName,
                                const nsAString& aValue,
                                const nsAString& aPriority)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsComputedDOMStyle::Item(PRUint32 aIndex, nsAString& aReturn)
{
  aReturn.Truncate();

  PRUint32 length = 0;
  const ComputedStyleMapEntry* propMap = GetQueryablePropertyMap(&length);
  if (aIndex < length) {
    CopyASCIItoUCS2(nsCSSProps::GetStringValue(propMap[aIndex].mProperty),
                    aReturn);
  }

  return NS_OK;
}


// Property getters...

nsresult
nsComputedDOMStyle::GetBinding(nsIFrame *aFrame,
                               nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = nsnull;

  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display, aFrame);

  if (display && display->mBinding) {
    val->SetURI(display->mBinding);
  } else {
    val->SetIdent(nsLayoutAtoms::none);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetClear(nsIFrame *aFrame,
                             nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay *display = nsnull;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display, aFrame);

  if (display && display->mBreakType != NS_STYLE_CLEAR_NONE) {
    const nsAFlatCString& clear =
      nsCSSProps::SearchKeywordTable(display->mBreakType,
                                     nsCSSProps::kClearKTable);
    val->SetIdent(clear);
  } else {
    val->SetIdent(nsLayoutAtoms::none);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetCssFloat(nsIFrame *aFrame,
                                nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = nsnull;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display, aFrame);

  if (display && display->mFloats != NS_STYLE_FLOAT_NONE) {
    const nsAFlatCString& cssFloat =
      nsCSSProps::SearchKeywordTable(display->mFloats,
                                     nsCSSProps::kFloatKTable);
    val->SetIdent(cssFloat);
  } else {
    val->SetIdent(nsLayoutAtoms::none);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBottom(nsIFrame *aFrame,
                              nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_BOTTOM, aFrame, aValue);
}

nsDOMCSSRGBColor*
nsComputedDOMStyle::GetDOMCSSRGBColor(nscolor aColor)
{
  nsROCSSPrimitiveValue *red   = GetROCSSPrimitiveValue();
  nsROCSSPrimitiveValue *green = GetROCSSPrimitiveValue();
  nsROCSSPrimitiveValue *blue  = GetROCSSPrimitiveValue();

  if (red && green && blue) {
    nsDOMCSSRGBColor *rgbColor = new nsDOMCSSRGBColor(red, green, blue);

    if (rgbColor) {
      red->SetNumber(NS_GET_R(aColor));
      green->SetNumber(NS_GET_G(aColor));
      blue->SetNumber(NS_GET_B(aColor));

      return rgbColor;
    }
  }

  delete red;
  delete green;
  delete blue;

  return nsnull;
}

nsresult
nsComputedDOMStyle::GetColor(nsIFrame *aFrame,
                             nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleColor* color = nsnull;
  GetStyleData(eStyleStruct_Color, (const nsStyleStruct*&)color, aFrame);

  nsDOMCSSRGBColor *rgb = nsnull;

  if (color) {
    rgb = GetDOMCSSRGBColor(color->mColor);
    if (!rgb) {
      delete val;

      return NS_ERROR_OUT_OF_MEMORY;
    }

    val->SetColor(rgb);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOpacity(nsIFrame *aFrame,
                               nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay *display = nsnull;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display, aFrame);

  if (display) {
    val->SetNumber(display->mOpacity);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetColumnCount(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleColumn* column = nsnull;
  GetStyleData(eStyleStruct_Column, (const nsStyleStruct*&)column, aFrame);

  if (column) {
    if (column->mColumnCount == NS_STYLE_COLUMN_COUNT_AUTO) {
      val->SetIdent(nsLayoutAtoms::autoAtom);
    } else {
      val->SetNumber(column->mColumnCount);
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetColumnWidth(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleColumn* column = nsnull;
  GetStyleData(eStyleStruct_Column, (const nsStyleStruct*&)column, aFrame);

  if (column) {
    switch (column->mColumnWidth.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(column->mColumnWidth.GetCoordValue());
        break;
      case eStyleUnit_Auto:
        // XXX fix this. When we actually have a column frame, I think
        // we should return the computed column width.
        val->SetIdent(nsLayoutAtoms::autoAtom);
        break;
      default:
        NS_WARNING("Bad column width unit");
        val->SetTwips(0);
        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetColumnGap(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleColumn* column = nsnull;
  GetStyleData(eStyleStruct_Column, (const nsStyleStruct*&)column, aFrame);

  if (column) {
    switch (column->mColumnGap.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(column->mColumnGap.GetCoordValue());
        break;
      case eStyleUnit_Percent:
        if (aFrame) {
          val->SetTwips(column->mColumnGap.GetPercentValue()*aFrame->GetSize().width);
        } else {
          val->SetPercent(column->mColumnGap.GetPercentValue());
        }
        break;
      default:
        NS_WARNING("Bad column width unit");
        val->SetTwips(0);
        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontFamily(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = nsnull;
  GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font, aFrame);

  if (font) {
    nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShellWeak);
    NS_ASSERTION(presShell, "pres shell is required");
    nsCOMPtr<nsPresContext> presContext;
    presShell->GetPresContext(getter_AddRefs(presContext));
    NS_ASSERTION(presContext, "pres context is required");

    const nsString& fontName = font->mFont.name;
    PRUint8 generic = font->mFlags & NS_STYLE_FONT_FACE_MASK;
    if (generic == kGenericFont_NONE && !font->mFont.systemFont) { 
      const nsFont* defaultFont =
        presContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID);

      PRInt32 lendiff = fontName.Length() - defaultFont->name.Length();
      if (lendiff > 0) {
        val->SetString(Substring(fontName, 0, lendiff-1)); // -1 removes comma
      } else {
        val->SetString(fontName);
      }
    } else {
      val->SetString(fontName);
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontSize(nsIFrame *aFrame,
                                nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = nsnull;
  GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font, aFrame);

  // Note: font->mSize is the 'computed size'; font->mFont.size is the
  // 'actual size'
  val->SetTwips(font? font->mSize:0);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontSizeAdjust(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont *font = nsnull;
  GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font, aFrame);

  if (font && font->mFont.sizeAdjust) {
    val->SetNumber(font->mFont.sizeAdjust);
  } else {
    val->SetIdent(nsLayoutAtoms::none);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontStyle(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = nsnull;
  GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font, aFrame);

  if (font && font->mFont.style != NS_STYLE_FONT_STYLE_NORMAL) {
    const nsAFlatCString& style=
      nsCSSProps::SearchKeywordTable(font->mFont.style,
                                     nsCSSProps::kFontStyleKTable);
    val->SetIdent(style);
  } else {
    val->SetIdent(nsLayoutAtoms::normal);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontWeight(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = nsnull;
  GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font, aFrame);

  if (font) {
    const nsAFlatCString& str_weight=
      nsCSSProps::SearchKeywordTable(font->mFont.weight,
                                     nsCSSProps::kFontWeightKTable);
    if (!str_weight.IsEmpty()) {
      val->SetIdent(str_weight);
    } else {
      val->SetNumber(font->mFont.weight);
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontVariant(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = nsnull;
  GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font, aFrame);

  if (font && font->mFont.variant != NS_STYLE_FONT_VARIANT_NORMAL) {
    const nsAFlatCString& variant=
      nsCSSProps::SearchKeywordTable(font->mFont.variant,
                                     nsCSSProps::kFontVariantKTable);
    val->SetIdent(variant);
  } else {
    val->SetIdent(nsLayoutAtoms::normal);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundAttachment(nsIFrame *aFrame,
                                            nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground *background = nsnull;
  GetStyleData(eStyleStruct_Background, (const nsStyleStruct*&)background,
               aFrame);

  if (background) {
    const nsAFlatCString& backgroundAttachment =
      nsCSSProps::SearchKeywordTable(background->mBackgroundAttachment,
                                     nsCSSProps::kBackgroundAttachmentKTable);
    val->SetIdent(backgroundAttachment);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundClip(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground *background = nsnull;
  GetStyleData(eStyleStruct_Background, (const nsStyleStruct*&)background, aFrame);

  PRUint8 clip = NS_STYLE_BG_CLIP_BORDER;
  if (background) {
    clip = background->mBackgroundClip;
  }

  const nsAFlatCString& backgroundClip =
    nsCSSProps::SearchKeywordTable(clip,
                                   nsCSSProps::kBackgroundClipKTable);

  val->SetIdent(backgroundClip);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundColor(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground* color = nsnull;
  GetStyleData(eStyleStruct_Background, (const nsStyleStruct*&)color, aFrame);

  if (color) {
    if (color->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT) {
      const nsAFlatCString& backgroundColor =
        nsCSSProps::SearchKeywordTable(NS_STYLE_BG_COLOR_TRANSPARENT,
                                       nsCSSProps::kBackgroundColorKTable);
      val->SetIdent(backgroundColor);
    } else {
      nsDOMCSSRGBColor *rgb = nsnull;
      rgb = GetDOMCSSRGBColor(color->mBackgroundColor);
      if (!rgb) {
        delete val;

        return NS_ERROR_OUT_OF_MEMORY;
      }

      val->SetColor(rgb);
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundImage(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground* color = nsnull;
  GetStyleData(eStyleStruct_Background, (const nsStyleStruct*&)color, aFrame);

  if (color) {
    if (color->mBackgroundFlags & NS_STYLE_BG_IMAGE_NONE) {
      val->SetIdent(nsLayoutAtoms::none);
    } else {
      nsCOMPtr<nsIURI> uri;
      if (color->mBackgroundImage) {
        color->mBackgroundImage->GetURI(getter_AddRefs(uri));
      }
      val->SetURI(uri);
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundInlinePolicy(nsIFrame *aFrame,
                                              nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground *background = nsnull;
  GetStyleData(eStyleStruct_Background, (const nsStyleStruct*&)background, aFrame);

  PRUint8 policy = NS_STYLE_BG_INLINE_POLICY_CONTINUOUS;
  if (background) {
    policy = background->mBackgroundInlinePolicy;
  }

  const nsAFlatCString& backgroundPolicy =
      nsCSSProps::SearchKeywordTable(policy,
                                     nsCSSProps::kBackgroundInlinePolicyKTable);

  val->SetIdent(backgroundPolicy);

  return CallQueryInterface(val, aValue);  
}

nsresult
nsComputedDOMStyle::GetBackgroundOrigin(nsIFrame *aFrame,
                                        nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground *background = nsnull;
  GetStyleData(eStyleStruct_Background, (const nsStyleStruct*&)background, aFrame);

  PRUint8 origin = NS_STYLE_BG_ORIGIN_PADDING;
  if (background) {
    origin = background->mBackgroundOrigin;
  }

  const nsAFlatCString& backgroundOrigin =
    nsCSSProps::SearchKeywordTable(origin,
                                   nsCSSProps::kBackgroundOriginKTable);

  val->SetIdent(backgroundOrigin);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundRepeat(nsIFrame *aFrame,
                                        nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground *background = nsnull;
  GetStyleData(eStyleStruct_Background, (const nsStyleStruct*&)background,
               aFrame);

  if (background) {
    const nsAFlatCString& backgroundRepeat =
      nsCSSProps::SearchKeywordTable(background->mBackgroundRepeat,
                                     nsCSSProps::kBackgroundRepeatKTable);
    val->SetIdent(backgroundRepeat);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetPadding(nsIFrame *aFrame,
                               nsIDOMCSSValue** aValue)
{
  // return null per spec.
  aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetPaddingTop(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_TOP, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetPaddingBottom(nsIFrame *aFrame,
                                     nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_BOTTOM, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetPaddingLeft(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetPaddingRight(nsIFrame *aFrame,
                                    nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_RIGHT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderCollapse(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTableBorder* table = nsnull;
  GetStyleData(eStyleStruct_TableBorder, (const nsStyleStruct*&)table, aFrame);

  if (table) {
    const nsAFlatCString& ident=
      nsCSSProps::SearchKeywordTable(table->mBorderCollapse,
                                     nsCSSProps::kBorderCollapseKTable);
    val->SetIdent(ident);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderSpacing(nsIFrame *aFrame,
                                     nsIDOMCSSValue** aValue)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

nsresult
nsComputedDOMStyle::GetCaptionSide(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTableBorder *table = nsnull;
  GetStyleData(eStyleStruct_TableBorder, (const nsStyleStruct*&)table, aFrame);

  if (table) {
    const nsAFlatCString& side =
      nsCSSProps::SearchKeywordTable(table->mCaptionSide,
                                     nsCSSProps::kCaptionSideKTable);
    val->SetIdent(side);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetEmptyCells(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTableBorder *table = nsnull;
  GetStyleData(eStyleStruct_TableBorder, (const nsStyleStruct*&)table, aFrame);

  if (table) {
    const nsAFlatCString& emptyCells =
      nsCSSProps::SearchKeywordTable(table->mEmptyCells,
                                     nsCSSProps::kEmptyCellsKTable);
    val->SetIdent(emptyCells);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTableLayout(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTable *table = nsnull;
  GetStyleData(eStyleStruct_Table, (const nsStyleStruct*&)table, aFrame);

  if (table && table->mLayoutStrategy != NS_STYLE_TABLE_LAYOUT_AUTO) {
    const nsAFlatCString& tableLayout =
      nsCSSProps::SearchKeywordTable(table->mLayoutStrategy,
                                     nsCSSProps::kTableLayoutKTable);
    val->SetIdent(tableLayout);
  } else {
    val->SetIdent(nsLayoutAtoms::autoAtom);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderStyle(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  // return null per spec.
  aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetBorderTopStyle(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_TOP, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderBottomStyle(nsIFrame *aFrame,
                                         nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_BOTTOM, aFrame, aValue);
}
nsresult
nsComputedDOMStyle::GetBorderLeftStyle(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRightStyle(nsIFrame *aFrame,
                                        nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_RIGHT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderBottomColors(nsIFrame *aFrame,
                                          nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_BOTTOM, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderLeftColors(nsIFrame *aFrame,
                                        nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRightColors(nsIFrame *aFrame,
                                         nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_RIGHT, aFrame, aValue);
}


nsresult
nsComputedDOMStyle::GetBorderTopColors(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_TOP, aFrame, aValue);
}


nsresult
nsComputedDOMStyle::GetBorderRadiusBottomLeft(nsIFrame *aFrame,
                                              nsIDOMCSSValue** aValue)
{
  return GetBorderRadiusFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRadiusBottomRight(nsIFrame *aFrame,
                                               nsIDOMCSSValue** aValue)
{
  return GetBorderRadiusFor(NS_SIDE_BOTTOM, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRadiusTopLeft(nsIFrame *aFrame,
                                           nsIDOMCSSValue** aValue)
{
  return GetBorderRadiusFor(NS_SIDE_TOP, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRadiusTopRight(nsIFrame *aFrame,
                                            nsIDOMCSSValue** aValue)
{
  return GetBorderRadiusFor(NS_SIDE_RIGHT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderWidth(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  // return null per spec.
  aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetBorderTopWidth(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_TOP, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderBottomWidth(nsIFrame *aFrame,
                                         nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_BOTTOM, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderLeftWidth(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRightWidth(nsIFrame *aFrame,
                                        nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_RIGHT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderTopColor(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_TOP, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderBottomColor(nsIFrame *aFrame,
                                         nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_BOTTOM, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderLeftColor(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRightColor(nsIFrame *aFrame,
                                        nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_RIGHT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginWidth(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  // return null per spec.
  aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetMarginTopWidth(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_TOP, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginBottomWidth(nsIFrame *aFrame,
                                         nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_BOTTOM, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginLeftWidth(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginRightWidth(nsIFrame *aFrame,
                                        nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_RIGHT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetMarkerOffset(nsIFrame *aFrame,
                                    nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleContent* content = nsnull;
  GetStyleData(eStyleStruct_Content, (const nsStyleStruct*&)content, aFrame);

  if (content) {
    switch (content->mMarkerOffset.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(content->mMarkerOffset.GetCoordValue());
        break;
      case eStyleUnit_Auto:
        val->SetIdent(nsLayoutAtoms::autoAtom);
        break;
      case eStyleUnit_Null:
        val->SetIdent(nsLayoutAtoms::none);
        break;
      default:
        NS_WARNING("Double check the unit");
        val->SetTwips(0);
        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOutline(nsIFrame *aFrame,
                               nsIDOMCSSValue** aValue)
{
  // return null per spec.
  aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetOutlineWidth(nsIFrame *aFrame,
                                    nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleOutline* outline = nsnull;
  GetStyleData(eStyleStruct_Outline, (const nsStyleStruct*&)outline, aFrame);

  if (outline) {
    switch (outline->mOutlineWidth.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(outline->mOutlineWidth.GetCoordValue());
        break;
      case eStyleUnit_Integer:
      case eStyleUnit_Proportional:
      case eStyleUnit_Enumerated:
      case eStyleUnit_Chars:
        {
          const nsAFlatCString& width=
            nsCSSProps::SearchKeywordTable(outline->mOutlineWidth.GetIntValue(),
                                           nsCSSProps::kBorderWidthKTable);
          val->SetIdent(width);
          break;
        }
      default:
        NS_WARNING("Double check the unit");
        val->SetTwips(0);
        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineStyle(nsIFrame *aFrame,
                                    nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleOutline* outline = nsnull;
  GetStyleData(eStyleStruct_Outline, (const nsStyleStruct*&)outline, aFrame);

  if (outline) {
    PRUint8 outlineStyle = outline->GetOutlineStyle();
    if (outlineStyle == NS_STYLE_BORDER_STYLE_NONE) {
      val->SetIdent(nsLayoutAtoms::none);
    } else {
      const nsAFlatCString& style=
        nsCSSProps::SearchKeywordTable(outlineStyle,
                                       nsCSSProps::kBorderStyleKTable);
      val->SetIdent(style);
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineColor(nsIFrame *aFrame,
                                    nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleOutline* outline = nsnull;
  GetStyleData(eStyleStruct_Outline, (const nsStyleStruct*&)outline, aFrame);

  if (outline) {
    nscolor color;
    outline->GetOutlineColor(color);

    nsDOMCSSRGBColor *rgb = nsnull;
    rgb = GetDOMCSSRGBColor(color);
    if (!rgb) {
      delete val;

      return NS_ERROR_OUT_OF_MEMORY;
    }

    val->SetColor(rgb);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetZIndex(nsIFrame *aFrame,
                              nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition* position = nsnull;

  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position, aFrame);
  if (position) {
    switch (position->mZIndex.GetUnit()) {
      case eStyleUnit_Integer:
        val->SetNumber(position->mZIndex.GetIntValue());
        break;
      default:
        NS_WARNING("Double Check the Unit!");
        // fall through
      case eStyleUnit_Auto:
        val->SetIdent(nsLayoutAtoms::autoAtom);
        break;
    }
  }
    
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetListStyleImage(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleList* list = nsnull;
  GetStyleData(eStyleStruct_List, (const nsStyleStruct*&)list, aFrame);

  if (list) {
    if (!list->mListStyleImage) {
      val->SetIdent(nsLayoutAtoms::none);
    } else {
      nsCOMPtr<nsIURI> uri;
      if (list->mListStyleImage) {
        list->mListStyleImage->GetURI(getter_AddRefs(uri));
      }
      val->SetURI(uri);
    }
  }
    
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetListStylePosition(nsIFrame *aFrame,
                                         nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleList *list = nsnull;
  GetStyleData(eStyleStruct_List, (const nsStyleStruct*&)list, aFrame);

  if (list) {
    const nsAFlatCString& style =
      nsCSSProps::SearchKeywordTable(list->mListStylePosition,
                                     nsCSSProps::kListStylePositionKTable);
    val->SetIdent(style);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetListStyleType(nsIFrame *aFrame,
                                     nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleList *list = nsnull;
  GetStyleData(eStyleStruct_List, (const nsStyleStruct*&)list, aFrame);

  if (list) {
    if (list->mListStyleType == NS_STYLE_LIST_STYLE_NONE) {
      val->SetIdent(nsLayoutAtoms::none);
    } else {
      const nsAFlatCString& style =
        nsCSSProps::SearchKeywordTable(list->mListStyleType,
                                       nsCSSProps::kListStyleKTable);
      val->SetIdent(style);
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetImageRegion(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleList* list = nsnull;

  GetStyleData(eStyleStruct_List, (const nsStyleStruct*&)list, aFrame);

  nsresult rv = NS_OK;
  nsROCSSPrimitiveValue *topVal = nsnull;
  nsROCSSPrimitiveValue *rightVal = nsnull;
  nsROCSSPrimitiveValue *bottomVal = nsnull;
  nsROCSSPrimitiveValue *leftVal = nsnull;
  if (list) {
    if (list->mImageRegion.width <= 0 || list->mImageRegion.height <= 0) {
      val->SetIdent(nsLayoutAtoms::autoAtom);
    } else {
      // create the cssvalues for the sides, stick them in the rect object
      topVal = GetROCSSPrimitiveValue();
      rightVal = GetROCSSPrimitiveValue();
      bottomVal = GetROCSSPrimitiveValue();
      leftVal = GetROCSSPrimitiveValue();
      if (topVal && rightVal && bottomVal && leftVal) {
        nsDOMCSSRect * domRect = new nsDOMCSSRect(topVal, rightVal,
                                                  bottomVal, leftVal);
        if (domRect) {
          topVal->SetTwips(list->mImageRegion.y);
          rightVal->SetTwips(list->mImageRegion.width + list->mImageRegion.x);
          bottomVal->SetTwips(list->mImageRegion.height + list->mImageRegion.y);
          leftVal->SetTwips(list->mImageRegion.x);
          val->SetRect(domRect);
        } else {
          rv = NS_ERROR_OUT_OF_MEMORY;
        }
      } else {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  if (NS_FAILED(rv)) {
    delete topVal;
    delete rightVal;
    delete bottomVal;
    delete leftVal;
    delete val;

    return rv;
  }
  
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetLineHeight(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = nsnull;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)text, aFrame);

  nscoord lineHeight;
  nsresult rv = GetLineHeightCoord(aFrame, text, lineHeight);

  if (NS_SUCCEEDED(rv)) {
    val->SetTwips(lineHeight);
  } else if (text) {
    switch (text->mLineHeight.GetUnit()) {
      case eStyleUnit_Percent:
        val->SetPercent(text->mLineHeight.GetPercentValue());
        break;
      case eStyleUnit_Factor:
        val->SetNumber(text->mLineHeight.GetFactorValue());
        break;
      default:
#ifdef DEBUG_ComputedDOMStyle
        NS_WARNING("double check the line-height");
#endif
        val->SetIdent(nsLayoutAtoms::normal);
        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetVerticalAlign(nsIFrame *aFrame,
                                     nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTextReset *text = nsnull;
  GetStyleData(eStyleStruct_TextReset, (const nsStyleStruct*&)text, aFrame);

  if (text) {
    switch (text->mVerticalAlign.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(text->mVerticalAlign.GetCoordValue());
        break;
      case eStyleUnit_Enumerated:
        {
          const nsAFlatCString& align =
            nsCSSProps::SearchKeywordTable(text->mVerticalAlign.GetIntValue(),
                                           nsCSSProps::kVerticalAlignKTable);
          val->SetIdent(align);
          break;
        }
      case eStyleUnit_Percent:
        {
          const nsStyleText *textData = nsnull;
          GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)textData,
                       aFrame);

          nscoord lineHeight = 0;
          nsresult rv = GetLineHeightCoord(aFrame, textData, lineHeight);

          if (NS_SUCCEEDED(rv)) {
            val->SetTwips(lineHeight * text->mVerticalAlign.GetPercentValue());
          } else {
            val->SetPercent(text->mVerticalAlign.GetPercentValue());
          }

          break;
        }
      default:
        NS_ERROR("double check the vertical-align");
        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextAlign(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText* text = nsnull;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)text, aFrame);

  if (text) {
    const nsAFlatCString& align=
      nsCSSProps::SearchKeywordTable(text->mTextAlign,
                                     nsCSSProps::kTextAlignKTable);
    val->SetIdent(align);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextDecoration(nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTextReset* text = nsnull;
  GetStyleData(eStyleStruct_TextReset, (const nsStyleStruct*&)text, aFrame);

  if (text) {
    if (NS_STYLE_TEXT_DECORATION_NONE == text->mTextDecoration) {
      const nsAFlatCString& decoration=
        nsCSSKeywords::GetStringValue(eCSSKeyword_none);
      val->SetIdent(decoration);
    } else {
      nsAutoString decorationString;
      if (text->mTextDecoration & NS_STYLE_TEXT_DECORATION_UNDERLINE) {
        const nsAFlatCString& decoration=
          nsCSSProps::SearchKeywordTable(NS_STYLE_TEXT_DECORATION_UNDERLINE,
                                         nsCSSProps::kTextDecorationKTable);
        decorationString.AppendWithConversion(decoration.get());
      }
      if (text->mTextDecoration & NS_STYLE_TEXT_DECORATION_OVERLINE) {
        if (!decorationString.IsEmpty()) {
          decorationString.Append(PRUnichar(' '));
        }
        const nsAFlatCString& decoration=
          nsCSSProps::SearchKeywordTable(NS_STYLE_TEXT_DECORATION_OVERLINE,
                                         nsCSSProps::kTextDecorationKTable);
        decorationString.AppendWithConversion(decoration.get());
      }
      if (text->mTextDecoration & NS_STYLE_TEXT_DECORATION_LINE_THROUGH) {
        if (!decorationString.IsEmpty()) {
          decorationString.Append(PRUnichar(' '));
        }
        const nsAFlatCString& decoration=
          nsCSSProps::SearchKeywordTable(NS_STYLE_TEXT_DECORATION_LINE_THROUGH,
                                         nsCSSProps::kTextDecorationKTable);
        decorationString.AppendWithConversion(decoration.get());
      }
      if (text->mTextDecoration & NS_STYLE_TEXT_DECORATION_BLINK) {
        if (!decorationString.IsEmpty()) {
          decorationString.Append(PRUnichar(' '));
        }
        const nsAFlatCString& decoration=
          nsCSSProps::SearchKeywordTable(NS_STYLE_TEXT_DECORATION_BLINK,
                                         nsCSSProps::kTextDecorationKTable);
        decorationString.AppendWithConversion(decoration.get());
      }
      val->SetString(decorationString);
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextIndent(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = nsnull;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)text, aFrame);

  FlushPendingReflows();

  if (text) {
    switch (text->mTextIndent.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(text->mTextIndent.GetCoordValue());
        break;
      case eStyleUnit_Percent:
        {
          nsIFrame *container = GetContainingBlock(aFrame);
          if (container) {
            val->SetTwips(container->GetSize().width *
                          text->mTextIndent.GetPercentValue());
          } else {
            // no containing block
            val->SetPercent(text->mTextIndent.GetPercentValue());
          }
          break;
        }
      default:
        val->SetTwips(0);
        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextTransform(nsIFrame *aFrame,
                                     nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = nsnull;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)text, aFrame);

  if (text && text->mTextTransform != NS_STYLE_TEXT_TRANSFORM_NONE) {
    const nsAFlatCString& textTransform =
      nsCSSProps::SearchKeywordTable(text->mTextTransform,
                                     nsCSSProps::kTextTransformKTable);
    val->SetIdent(textTransform);
  } else {
    val->SetIdent(nsLayoutAtoms::none);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetLetterSpacing(nsIFrame *aFrame,
                                     nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = nsnull;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)text, aFrame);

  if (text && text->mLetterSpacing.GetUnit() == eStyleUnit_Coord) {
    val->SetTwips(text->mLetterSpacing.GetCoordValue());
  } else {
    val->SetIdent(nsLayoutAtoms::normal);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetWordSpacing(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = nsnull;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)text, aFrame);

  if (text && text->mWordSpacing.GetUnit() == eStyleUnit_Coord) {
    val->SetTwips(text->mWordSpacing.GetCoordValue());
  } else {
    val->SetIdent(nsLayoutAtoms::normal);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetWhiteSpace(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = nsnull;
  GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&)text, aFrame);

  if (text && text->mWhiteSpace != NS_STYLE_WHITESPACE_NORMAL) {
    const nsAFlatCString& whiteSpace =
      nsCSSProps::SearchKeywordTable(text->mWhiteSpace,
                                     nsCSSProps::kWhitespaceKTable);
    val->SetIdent(whiteSpace);
  } else {
    val->SetIdent(nsLayoutAtoms::normal);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetVisibility(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleVisibility* visibility = nsnull;
  GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&)visibility,
               aFrame);

  if (visibility) {
    const nsAFlatCString& value=
      nsCSSProps::SearchKeywordTable(visibility->mVisible,
                                     nsCSSProps::kVisibilityKTable);
    val->SetIdent(value);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetDirection(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleVisibility *visibility = nsnull;
  GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&)visibility,
               aFrame);

  if (visibility) {
    const nsAFlatCString & direction =
      nsCSSProps::SearchKeywordTable(visibility->mDirection,
                                     nsCSSProps::kDirectionKTable);
    val->SetIdent(direction);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetUnicodeBidi(nsIFrame *aFrame,
                                   nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTextReset *text = nsnull;
  GetStyleData(eStyleStruct_TextReset, (const nsStyleStruct*&)text, aFrame);

  if (text && text->mUnicodeBidi != NS_STYLE_UNICODE_BIDI_NORMAL) {
    const nsAFlatCString& bidi =
      nsCSSProps::SearchKeywordTable(text->mUnicodeBidi,
                                     nsCSSProps::kUnicodeBidiKTable);
    val->SetIdent(bidi);
  } else {
    val->SetIdent(nsLayoutAtoms::normal);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetCursor(nsIFrame *aFrame,
                              nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleUserInterface *ui = nsnull;
  GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)ui, aFrame);

  if (ui) {
    if (ui->mCursor == NS_STYLE_CURSOR_AUTO) {
      val->SetIdent(nsLayoutAtoms::autoAtom);
    } else {
      const nsAFlatCString& cursor =
        nsCSSProps::SearchKeywordTable(ui->mCursor,
                                       nsCSSProps::kCursorKTable);
      val->SetIdent(cursor);
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetAppearance(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay *displayData = nsnull;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)displayData,
               aFrame);

  PRUint8 appearance = NS_THEME_NONE;
  if (displayData) {
    appearance = displayData->mAppearance;
  }

  const nsAFlatCString& appearanceIdent =
    nsCSSProps::SearchKeywordTable(appearance,
                                   nsCSSProps::kAppearanceKTable);
  val->SetIdent(appearanceIdent);

  return CallQueryInterface(val, aValue);
}


nsresult
nsComputedDOMStyle::GetBoxAlign(nsIFrame *aFrame,
                                nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleXUL *xul = nsnull;
  GetStyleData(eStyleStruct_XUL, (const nsStyleStruct*&)xul, aFrame);

  PRUint8 boxAlign = NS_STYLE_BOX_ALIGN_STRETCH;
  if (xul) {
    boxAlign = xul->mBoxAlign;
  }

  const nsAFlatCString& boxAlignIdent =
    nsCSSProps::SearchKeywordTable(boxAlign,
                                   nsCSSProps::kBoxAlignKTable);
  val->SetIdent(boxAlignIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxDirection(nsIFrame *aFrame,
                                    nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleXUL *xul = nsnull;
  GetStyleData(eStyleStruct_XUL, (const nsStyleStruct*&)xul, aFrame);

  PRUint8 boxDirection = NS_STYLE_BOX_DIRECTION_NORMAL;
  if (xul) {
    boxDirection = xul->mBoxDirection;
  }

  const nsAFlatCString& boxDirectionIdent =
    nsCSSProps::SearchKeywordTable(boxDirection,
                                   nsCSSProps::kBoxDirectionKTable);
  val->SetIdent(boxDirectionIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxFlex(nsIFrame *aFrame,
                               nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleXUL *xul = nsnull;
  GetStyleData(eStyleStruct_XUL, (const nsStyleStruct*&)xul, aFrame);

  float boxFlex = 0.0f;
  if (xul) {
    boxFlex = xul->mBoxFlex;
  }

  val->SetNumber(boxFlex);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxOrdinalGroup(nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleXUL *xul = nsnull;
  GetStyleData(eStyleStruct_XUL, (const nsStyleStruct*&)xul, aFrame);

  PRUint32 boxOrdinalGroup = 1;
  if (xul) {
    boxOrdinalGroup = xul->mBoxOrdinal;
  }

  val->SetNumber(boxOrdinalGroup);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxOrient(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleXUL *xul = nsnull;
  GetStyleData(eStyleStruct_XUL, (const nsStyleStruct*&)xul, aFrame);

  PRUint8 boxOrient = NS_STYLE_BOX_ORIENT_HORIZONTAL;
  if (xul) {
    boxOrient = xul->mBoxOrient;
  }

  const nsAFlatCString& boxOrientIdent =
    nsCSSProps::SearchKeywordTable(boxOrient,
                                   nsCSSProps::kBoxOrientKTable);
  val->SetIdent(boxOrientIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxPack(nsIFrame *aFrame,
                               nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleXUL *xul = nsnull;
  GetStyleData(eStyleStruct_XUL, (const nsStyleStruct*&)xul, aFrame);

  PRUint8 boxPack = NS_STYLE_BOX_PACK_START;
  if (xul) {
    boxPack = xul->mBoxPack;
  }

  const nsAFlatCString& boxPackIdent =
    nsCSSProps::SearchKeywordTable(boxPack,
                                   nsCSSProps::kBoxPackKTable);
  val->SetIdent(boxPackIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxSizing(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition *positionData = nsnull;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
               aFrame);

  PRUint8 boxSizing = NS_STYLE_BOX_SIZING_CONTENT;
  if (positionData) {
    boxSizing = positionData->mBoxSizing;
  }

  const nsAFlatCString& boxSizingIdent =
    nsCSSProps::SearchKeywordTable(boxSizing,
                                   nsCSSProps::kBoxSizingKTable);
  val->SetIdent(boxSizingIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFloatEdge(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBorder *borderData = nsnull;
  GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)borderData, aFrame);

  PRUint8 floatEdge = NS_STYLE_FLOAT_EDGE_CONTENT;
  if (borderData) {
    floatEdge = borderData->mFloatEdge;
  }

  const nsAFlatCString& floatEdgeIdent =
    nsCSSProps::SearchKeywordTable(floatEdge,
                                   nsCSSProps::kFloatEdgeKTable);
  val->SetIdent(floatEdgeIdent);

  return CallQueryInterface(val, aValue);
}


nsresult
nsComputedDOMStyle::GetUserFocus(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleUserInterface *uiData = nsnull;
  GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)uiData,
               aFrame);

  if (uiData && uiData->mUserFocus != NS_STYLE_USER_FOCUS_NONE) {
    if (uiData->mUserFocus == NS_STYLE_USER_FOCUS_NORMAL) {
      const nsAFlatCString& userFocusIdent =
              nsCSSKeywords::GetStringValue(eCSSKeyword_normal);
      val->SetIdent(userFocusIdent);
    } else {
      const nsAFlatCString& userFocusIdent =
        nsCSSProps::SearchKeywordTable(uiData->mUserFocus,
                                       nsCSSProps::kUserFocusKTable);
      val->SetIdent(userFocusIdent);
    }
  } else {
    const nsAFlatCString& userFocusIdent =
            nsCSSKeywords::GetStringValue(eCSSKeyword_none);
    val->SetIdent(userFocusIdent);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetUserInput(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleUserInterface *uiData = nsnull;
  GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)uiData,
               aFrame);

  if (uiData && uiData->mUserInput != NS_STYLE_USER_INPUT_AUTO) {
    if (uiData->mUserInput == NS_STYLE_USER_INPUT_NONE) {
      const nsAFlatCString& userInputIdent =
        nsCSSKeywords::GetStringValue(eCSSKeyword_none);
      val->SetIdent(userInputIdent);
    } else {
      const nsAFlatCString& userInputIdent =
        nsCSSProps::SearchKeywordTable(uiData->mUserInput,
                                       nsCSSProps::kUserInputKTable);
      val->SetIdent(userInputIdent);
    }
  } else {
    const nsAFlatCString& userInputIdent =
            nsCSSKeywords::GetStringValue(eCSSKeyword_auto);
    val->SetIdent(userInputIdent);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetUserModify(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleUserInterface *uiData = nsnull;
  GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)uiData,
               aFrame);

  PRUint8 userModify = NS_STYLE_USER_MODIFY_READ_ONLY;
  if (uiData) {
    userModify = uiData->mUserModify;
  }

  const nsAFlatCString& userModifyIdent =
    nsCSSProps::SearchKeywordTable(userModify,
                                   nsCSSProps::kUserModifyKTable);
  val->SetIdent(userModifyIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetUserSelect(nsIFrame *aFrame,
                                  nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleUIReset *uiData = nsnull;
  GetStyleData(eStyleStruct_UIReset, (const nsStyleStruct*&)uiData, aFrame);

  if (uiData && uiData->mUserSelect != NS_STYLE_USER_SELECT_AUTO) {
    if (uiData->mUserSelect == NS_STYLE_USER_SELECT_NONE) {
      const nsAFlatCString& userSelectIdent =
        nsCSSKeywords::GetStringValue(eCSSKeyword_none);
      val->SetIdent(userSelectIdent);
    } else {
      const nsAFlatCString& userSelectIdent =
        nsCSSProps::SearchKeywordTable(uiData->mUserSelect,
                                       nsCSSProps::kUserSelectKTable);
      val->SetIdent(userSelectIdent);
    }
  } else {
    const nsAFlatCString& userSelectIdent =
            nsCSSKeywords::GetStringValue(eCSSKeyword_auto);
    val->SetIdent(userSelectIdent);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetDisplay(nsIFrame *aFrame,
                               nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay *displayData = nsnull;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)displayData,
               aFrame);

  if (displayData) {
    if (displayData->mDisplay == NS_STYLE_DISPLAY_NONE) {
      val->SetIdent(nsLayoutAtoms::none);
    } else {
      const nsAFlatCString& display =
        nsCSSProps::SearchKeywordTable(displayData->mDisplay,
                                       nsCSSProps::kDisplayKTable);
      val->SetIdent(display);
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetPosition(nsIFrame *aFrame,
                                nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = nsnull;

  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display, aFrame);

  if (display) {
    const nsAFlatCString& position =
      nsCSSProps::SearchKeywordTable(display->mPosition,
                                     nsCSSProps::kPositionKTable);
    val->SetIdent(position);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetClip(nsIFrame *aFrame,
                            nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = nsnull;

  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display, aFrame);

  nsresult rv = NS_OK;
  nsROCSSPrimitiveValue *topVal = nsnull;
  nsROCSSPrimitiveValue *rightVal = nsnull;
  nsROCSSPrimitiveValue *bottomVal = nsnull;
  nsROCSSPrimitiveValue *leftVal = nsnull;
  if (display) {
    if (display->mClipFlags == NS_STYLE_CLIP_AUTO ||
        display->mClipFlags == (NS_STYLE_CLIP_TOP_AUTO |
                                NS_STYLE_CLIP_RIGHT_AUTO |
                                NS_STYLE_CLIP_BOTTOM_AUTO |
                                NS_STYLE_CLIP_LEFT_AUTO)) {
      val->SetIdent(nsLayoutAtoms::autoAtom);
    } else {
      // create the cssvalues for the sides, stick them in the rect object
      topVal = GetROCSSPrimitiveValue();
      rightVal = GetROCSSPrimitiveValue();
      bottomVal = GetROCSSPrimitiveValue();
      leftVal = GetROCSSPrimitiveValue();
      if (topVal && rightVal && bottomVal && leftVal) {
        nsDOMCSSRect * domRect = new nsDOMCSSRect(topVal, rightVal,
                                                  bottomVal, leftVal);
        if (domRect) {
          if (display->mClipFlags & NS_STYLE_CLIP_TOP_AUTO) {
            topVal->SetIdent(nsLayoutAtoms::autoAtom);
          } else {
            topVal->SetTwips(display->mClip.y);
          }
        
          if (display->mClipFlags & NS_STYLE_CLIP_RIGHT_AUTO) {
            rightVal->SetIdent(nsLayoutAtoms::autoAtom);
          } else {
            rightVal->SetTwips(display->mClip.width + display->mClip.x);
          }
        
          if (display->mClipFlags & NS_STYLE_CLIP_BOTTOM_AUTO) {
            bottomVal->SetIdent(nsLayoutAtoms::autoAtom);
          } else {
            bottomVal->SetTwips(display->mClip.height + display->mClip.y);
          }
          
          if (display->mClipFlags & NS_STYLE_CLIP_LEFT_AUTO) {
            leftVal->SetIdent(nsLayoutAtoms::autoAtom);
          } else {
            leftVal->SetTwips(display->mClip.x);
          }

          val->SetRect(domRect);
        } else {
          rv = NS_ERROR_OUT_OF_MEMORY;
        }
      } else {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  if (NS_FAILED(rv)) {
    delete topVal;
    delete rightVal;
    delete bottomVal;
    delete leftVal;
    delete val;

    return rv;
  }
  
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOverflow(nsIFrame *aFrame,
                                nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = nsnull;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display, aFrame);

  if (display && display->mOverflow != NS_STYLE_OVERFLOW_AUTO) {
    const nsAFlatCString& overflow =
      nsCSSProps::SearchKeywordTable(display->mOverflow,
                                     nsCSSProps::kOverflowKTable);
    val->SetIdent(overflow);
  } else {
    val->SetIdent(nsLayoutAtoms::autoAtom);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetHeight(nsIFrame *aFrame,
                              nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  PRBool calcHeight = PR_FALSE;
  
  if (aFrame) {
    calcHeight = PR_TRUE;

    FlushPendingReflows();
  
    const nsStyleDisplay* displayData = nsnull;
    GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)displayData,
                 aFrame);
    if (displayData && displayData->mDisplay == NS_STYLE_DISPLAY_INLINE
        && !(aFrame->GetStateBits() & NS_FRAME_REPLACED_ELEMENT)) {
      calcHeight = PR_FALSE;
    }
  }

  if (calcHeight) {
    nsMargin padding;
    nsMargin border;
    nsSize size = aFrame->GetSize();
    const nsStylePadding* paddingData = nsnull;
    GetStyleData(eStyleStruct_Padding, (const nsStyleStruct*&)paddingData,
                 aFrame);
    if (paddingData) {
      paddingData->CalcPaddingFor(aFrame, padding);
    }
    const nsStyleBorder* borderData = nsnull;
    GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)borderData,
                 aFrame);
    if (borderData) {
      borderData->CalcBorderFor(aFrame, border);
    }
  
    val->SetTwips(size.height - padding.top - padding.bottom -
                  border.top - border.bottom);
  } else {
    // Just return the value in the style context
    const nsStylePosition* positionData = nsnull;
    GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
                 aFrame);
    if (positionData) {
      switch (positionData->mHeight.GetUnit()) {
        case eStyleUnit_Coord:
          val->SetTwips(positionData->mHeight.GetCoordValue());
          break;
        case eStyleUnit_Percent:
          val->SetPercent(positionData->mHeight.GetPercentValue());
          break;
        case eStyleUnit_Auto:
          val->SetIdent(nsLayoutAtoms::autoAtom);
          break;
        default:
          NS_WARNING("Double check the unit");
          val->SetTwips(0);
          break;
      }
    }
  }
  
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetWidth(nsIFrame *aFrame,
                             nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  PRBool calcWidth = PR_FALSE;

  if (aFrame) {
    calcWidth = PR_TRUE;

    FlushPendingReflows();

    const nsStyleDisplay *displayData = nsnull;
    GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)displayData,
                 aFrame);
    if (displayData && displayData->mDisplay == NS_STYLE_DISPLAY_INLINE
        && !(aFrame->GetStateBits() & NS_FRAME_REPLACED_ELEMENT)) {
      calcWidth = PR_FALSE;
    }
  }

  if (calcWidth) {
    nsSize size = aFrame->GetSize();
    nsMargin padding;
    nsMargin border;
    const nsStylePadding *paddingData = nsnull;
    GetStyleData(eStyleStruct_Padding, (const nsStyleStruct*&)paddingData,
                 aFrame);
    if (paddingData) {
      paddingData->CalcPaddingFor(aFrame, padding);
    }
    const nsStyleBorder *borderData = nsnull;
    GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)borderData,
                 aFrame);
    if (borderData) {
      borderData->CalcBorderFor(aFrame, border);
    }
    val->SetTwips(size.width - padding.left - padding.right -
                  border.left - border.right);
  } else {
    // Just return the value in the style context
    const nsStylePosition *positionData = nsnull;
    GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
                 aFrame);
    if (positionData) {
      switch (positionData->mWidth.GetUnit()) {
        case eStyleUnit_Coord:
          val->SetTwips(positionData->mWidth.GetCoordValue());
          break;
        case eStyleUnit_Percent:
          val->SetPercent(positionData->mWidth.GetPercentValue());
          break;
        case eStyleUnit_Auto:
          val->SetIdent(nsLayoutAtoms::autoAtom);
          break;
        default:
          NS_WARNING("Double check the unit");
          val->SetTwips(0);
          break;
      }
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMaxHeight(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition *positionData = nsnull;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
               aFrame);

  FlushPendingReflows();

  if (positionData) {
    nsIFrame *container = nsnull;
    nsSize size;
    nscoord minHeight = 0;

    if (positionData->mMinHeight.GetUnit() == eStyleUnit_Percent) {
      container = GetContainingBlock(aFrame);
      if (container) {
        size = container->GetSize();
        minHeight = nscoord(size.height *
                            positionData->mMinHeight.GetPercentValue());
      }
    } else if (positionData->mMinHeight.GetUnit() == eStyleUnit_Coord) {
      minHeight = positionData->mMinHeight.GetCoordValue();
    }

    switch (positionData->mMaxHeight.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(PR_MAX(minHeight,
                             positionData->mMaxHeight.GetCoordValue()));
        break;
      case eStyleUnit_Percent:
        if (!container) {
          container = GetContainingBlock(aFrame);
          if (container) {
            size = container->GetSize();
          } else {
            // no containing block
            val->SetPercent(positionData->mMaxHeight.GetPercentValue());
          }
        }
        if (container) {
          val->SetTwips(PR_MAX(minHeight, size.height *
                               positionData->mMaxHeight.GetPercentValue()));
        }

        break;
      default:
        val->SetIdent(nsLayoutAtoms::none);

        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMaxWidth(nsIFrame *aFrame,
                                nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition *positionData = nsnull;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
               aFrame);

  FlushPendingReflows();

  if (positionData) {
    nsIFrame *container = nsnull;
    nsSize size;
    nscoord minWidth = 0;

    if (positionData->mMinWidth.GetUnit() == eStyleUnit_Percent) {
      container = GetContainingBlock(aFrame);
      if (container) {
        size = container->GetSize();
        minWidth = nscoord(size.width *
                           positionData->mMinWidth.GetPercentValue());
      }
    } else if (positionData->mMinWidth.GetUnit() == eStyleUnit_Coord) {
      minWidth = positionData->mMinWidth.GetCoordValue();
    }

    switch (positionData->mMaxWidth.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(PR_MAX(minWidth,
                             positionData->mMaxWidth.GetCoordValue()));
        break;
      case eStyleUnit_Percent:
        if (!container) {
          container = GetContainingBlock(aFrame);
          if (container) {
            size = container->GetSize();
          } else {
            // no containing block
            val->SetPercent(positionData->mMaxWidth.GetPercentValue());
          }
        }
        if (container) {
          val->SetTwips(PR_MAX(minWidth, size.width *
                               positionData->mMaxWidth.GetPercentValue()));
        }

        break;
      default:
        val->SetIdent(nsLayoutAtoms::none);

        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMinHeight(nsIFrame *aFrame,
                                 nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition *positionData = nsnull;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
               aFrame);

  FlushPendingReflows();

  if (positionData) {
    nsIFrame *container = nsnull;
    switch (positionData->mMinHeight.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(positionData->mMinHeight.GetCoordValue());
        break;
      case eStyleUnit_Percent:
        container = GetContainingBlock(aFrame);
        if (container) {
          val->SetTwips(container->GetSize().height *
                        positionData->mMinHeight.GetPercentValue());
        } else {
          // no containing block
          val->SetPercent(positionData->mMinHeight.GetPercentValue());
        }

        break;
      default:
        val->SetTwips(0);

        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMinWidth(nsIFrame *aFrame,
                                nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition *positionData = nsnull;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
               aFrame);

  FlushPendingReflows();

  if (positionData) {
    nsIFrame *container = nsnull;
    switch (positionData->mMinWidth.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(positionData->mMinWidth.GetCoordValue());
        break;
      case eStyleUnit_Percent:
        container = GetContainingBlock(aFrame);
        if (container) {
          val->SetTwips(container->GetSize().width *
                        positionData->mMinWidth.GetPercentValue());
        } else {
          // no containing block
          val->SetPercent(positionData->mMinWidth.GetPercentValue());
        }
        break;
      default:
        val->SetTwips(0);
        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetLeft(nsIFrame *aFrame, nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_LEFT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetRight(nsIFrame *aFrame, nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_RIGHT, aFrame, aValue);
}

nsresult
nsComputedDOMStyle::GetTop(nsIFrame *aFrame, nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_TOP, aFrame, aValue);
}

nsROCSSPrimitiveValue*
nsComputedDOMStyle::GetROCSSPrimitiveValue()
{
  nsROCSSPrimitiveValue *primitiveValue = new nsROCSSPrimitiveValue(mT2P);

  NS_ASSERTION(primitiveValue != 0, "ran out of memory");

  return primitiveValue;
}

nsDOMCSSValueList*
nsComputedDOMStyle::GetROCSSValueList(PRBool aCommaDelimited)
{
  nsDOMCSSValueList *valueList = new nsDOMCSSValueList(aCommaDelimited,
                                                       PR_TRUE);
  NS_ASSERTION(valueList != 0, "ran out of memory");

  return valueList;
}

nsresult
nsComputedDOMStyle::GetOffsetWidthFor(PRUint8 aSide, nsIFrame* aFrame,
                                      nsIDOMCSSValue** aValue)
{
  const nsStyleDisplay* display = nsnull;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display, aFrame);

  FlushPendingReflows();

  nsresult rv = NS_OK;
  if (display) {
    switch (display->mPosition) {
      case NS_STYLE_POSITION_STATIC:
        rv = GetStaticOffset(aSide, aFrame, aValue);
        break;
      case NS_STYLE_POSITION_RELATIVE:
        rv = GetRelativeOffset(aSide, aFrame, aValue);
        break;
      case NS_STYLE_POSITION_ABSOLUTE:
      case NS_STYLE_POSITION_FIXED:
        rv = GetAbsoluteOffset(aSide, aFrame, aValue);
        break;
      default:
        NS_WARNING("double check the position");
        break;
    }
  }

  return rv;
}

nsresult
nsComputedDOMStyle::GetAbsoluteOffset(PRUint8 aSide, nsIFrame* aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nsIFrame* container = GetContainingBlock(aFrame);
  if (container) {
    nscoord margin = GetMarginWidthCoordFor(aSide, aFrame);
    nscoord border = GetBorderWidthCoordFor(aSide, container);
    nsMargin scrollbarSizes(0, 0, 0, 0);
    nsRect rect = aFrame->GetRect();
    nsRect containerRect = container->GetRect();
      
    if (container->GetType() == nsLayoutAtoms::viewportFrame) {
      // For absolutely positioned frames scrollbars are taken into
      // account by virtue of getting a containing block that does
      // _not_ include the scrollbars.  For fixed positioned frames,
      // the containing block is the viewport, which _does_ include
      // scrollbars.  We have to do some extra work.
      nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShellWeak);
      NS_ASSERTION(presShell, "Must have a presshell!");
      nsCOMPtr<nsPresContext> presContext;
      presShell->GetPresContext(getter_AddRefs(presContext));
      // the first child in the default frame list is what we want
      nsIFrame* scrollingChild = container->GetFirstChild(nsnull);
      nsCOMPtr<nsIScrollableFrame> scrollFrame =
        do_QueryInterface(scrollingChild);
      if (scrollFrame) {
        scrollbarSizes = scrollFrame->GetActualScrollbarSizes();
      }
    }

    nscoord offset = 0;
    switch (aSide) {
      case NS_SIDE_TOP:
        offset = rect.y - margin - border - scrollbarSizes.top;

        break;
      case NS_SIDE_RIGHT:
        offset = containerRect.width - rect.width -
          rect.x - margin - border - scrollbarSizes.right;

        break;
      case NS_SIDE_BOTTOM:
        offset = containerRect.height - rect.height -
          rect.y - margin - border - scrollbarSizes.bottom;

        break;
      case NS_SIDE_LEFT:
        offset = rect.x - margin - border - scrollbarSizes.left;

        break;
      default:
        NS_WARNING("double check the side");

        break;
    }
    val->SetTwips(offset);
  } else {
    // XXX no frame.  This property makes no sense
    val->SetTwips(0);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetRelativeOffset(PRUint8 aSide, nsIFrame* aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition* positionData = nsnull;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
               aFrame);
  if (positionData) {
    nsStyleCoord coord;
    PRInt32 sign = 1;
    switch (aSide) {
      case NS_SIDE_TOP:
        positionData->mOffset.GetTop(coord);
        if (coord.GetUnit() != eStyleUnit_Coord &&
            coord.GetUnit() != eStyleUnit_Percent) {
          positionData->mOffset.GetBottom(coord);
          sign = -1;
        }
        break;
      case NS_SIDE_RIGHT:
        positionData->mOffset.GetRight(coord);
        if (coord.GetUnit() != eStyleUnit_Coord &&
            coord.GetUnit() != eStyleUnit_Percent) {
          positionData->mOffset.GetLeft(coord);
          sign = -1;
        }
        break;
      case NS_SIDE_BOTTOM:
        positionData->mOffset.GetBottom(coord);
        if (coord.GetUnit() != eStyleUnit_Coord &&
            coord.GetUnit() != eStyleUnit_Percent) {
          positionData->mOffset.GetTop(coord);
          sign = -1;
        }
        break;
      case NS_SIDE_LEFT:
        positionData->mOffset.GetLeft(coord);
        if (coord.GetUnit() != eStyleUnit_Coord &&
            coord.GetUnit() != eStyleUnit_Percent) {
          positionData->mOffset.GetRight(coord);
          sign = -1;
        }
        break;
      default:
        NS_WARNING("double check the side");
        break;
    }

    nsIFrame* container = nsnull;
    switch(coord.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(sign * coord.GetCoordValue());
        break;
      case eStyleUnit_Percent:
        container = GetContainingBlock(aFrame);
        if (container) {
          nsMargin border;
          nsMargin padding;
          container->GetStyleBorder()->CalcBorderFor(container, border);
          container->GetStylePadding()->CalcPaddingFor(container, padding);
          nsSize size = container->GetSize();
          if (aSide == NS_SIDE_LEFT || aSide == NS_SIDE_RIGHT) {
            val->SetTwips(sign * coord.GetPercentValue() *
                          (size.width - border.left - border.right -
                           padding.left - padding.right));
          } else {
            val->SetTwips(sign * coord.GetPercentValue() *
                          (size.height - border.top - border.bottom -
                           padding.top - padding.bottom));
          }
        } else {
          // XXX no containing block.
          val->SetTwips(0);
        }
        break;
      default:
        NS_WARNING("double check the unit");
        val->SetTwips(0);
        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetStaticOffset(PRUint8 aSide, nsIFrame* aFrame,
                                    nsIDOMCSSValue** aValue)

{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition* positionData = nsnull;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)positionData,
               aFrame);
  if (positionData) {
    nsStyleCoord coord;
    switch (aSide) {
      case NS_SIDE_TOP:
        positionData->mOffset.GetTop(coord);

        break;
      case NS_SIDE_RIGHT:
        positionData->mOffset.GetRight(coord);

        break;
      case NS_SIDE_BOTTOM:
        positionData->mOffset.GetBottom(coord);

        break;
      case NS_SIDE_LEFT:
        positionData->mOffset.GetLeft(coord);

        break;
      default:
        NS_WARNING("double check the side");

        break;
    }

    switch(coord.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(coord.GetCoordValue());

        break;
      case eStyleUnit_Percent:
        val->SetPercent(coord.GetPercentValue());

        break;
      case eStyleUnit_Auto:
        val->SetIdent(nsLayoutAtoms::autoAtom);

        break;
      default:
        NS_WARNING("double check the unit");
        val->SetTwips(0);

        break;
    }
  }
  
  return CallQueryInterface(val, aValue);
}

void
nsComputedDOMStyle::FlushPendingReflows()
{
  // Flush all pending notifications so that our frames are up to date
  nsIDocument* document = mContent->GetDocument();
  if (document) {
    document->FlushPendingNotifications(Flush_Layout);
  }
}

nsIFrame*
nsComputedDOMStyle::GetContainingBlock(nsIFrame *aFrame)
{
  if (!aFrame) {
    // Tell caller that they have no containing block, seeing as they
    // are not being displayed

    return nsnull;
  }

  nsIFrame* container = aFrame;
  do {
    container = container->GetParent();
  } while (container && !container->IsContainingBlock());

  NS_POSTCONDITION(container, "Frame has no containing block");
  return container;
}

nsresult
nsComputedDOMStyle::GetStyleData(nsStyleStructID aID,
                                 const nsStyleStruct*& aStyleStruct,
                                 nsIFrame* aFrame)
{
  if (aFrame && !mPseudo) {
    aStyleStruct = aFrame->GetStyleData(aID);
  } else if (mStyleContextHolder) {
    aStyleStruct = mStyleContextHolder->GetStyleData(aID);    
  } else {
    nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShellWeak);

    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    mStyleContextHolder =
      nsInspectorCSSUtils::GetStyleContextForContent(mContent,
                                                     mPseudo,
                                                     presShell);
    if (mStyleContextHolder) {
      aStyleStruct = mStyleContextHolder->GetStyleData(aID);
    }
  }
  NS_ASSERTION(aStyleStruct, "Failed to get a style struct");

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetPaddingWidthFor(PRUint8 aSide, nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  FlushPendingReflows();
  
  nscoord width = GetPaddingWidthCoordFor(aSide, aFrame);
  val->SetTwips(width);

  return CallQueryInterface(val, aValue);
}

nscoord
nsComputedDOMStyle::GetPaddingWidthCoordFor(PRUint8 aSide, nsIFrame* aFrame)
{
  const nsStylePadding* paddingData = nsnull;
  GetStyleData(eStyleStruct_Padding, (const nsStyleStruct*&)paddingData,
               aFrame);

  if (paddingData) {
    nsMargin padding;
    paddingData->CalcPaddingFor(aFrame, padding);
    switch(aSide) {
      case NS_SIDE_TOP    :
        return padding.top;
      case NS_SIDE_BOTTOM :
        return padding.bottom;
      case NS_SIDE_LEFT   :
        return padding.left;
      case NS_SIDE_RIGHT  :
        return padding.right;
      default:
        NS_WARNING("double check the side");
        break;
    }
  }

  return 0;
}

nscoord
nsComputedDOMStyle::GetBorderWidthCoordFor(PRUint8 aSide, nsIFrame *aFrame)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBorder* borderData = nsnull;
  GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)borderData, aFrame);

  if (borderData) {
    nsMargin border;
    borderData->CalcBorderFor(aFrame, border);
    switch(aSide) {
      case NS_SIDE_TOP    :
        return border.top;
      case NS_SIDE_BOTTOM :
        return border.bottom;
      case NS_SIDE_LEFT   :
        return border.left;
      case NS_SIDE_RIGHT  :
        return border.right;
      default:
        NS_WARNING("double check the side");
        break;
    }
  }

  return 0;
}

nsresult
nsComputedDOMStyle::GetLineHeightCoord(nsIFrame *aFrame,
                                       const nsStyleText *aText,
                                       nscoord& aCoord)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (aText) {
    const nsStyleFont *font = nsnull;
    GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font, aFrame);
    switch (aText->mLineHeight.GetUnit()) {
      case eStyleUnit_Coord:
        aCoord = aText->mLineHeight.GetCoordValue();
        rv = NS_OK;
        break;
      case eStyleUnit_Percent:
        if (font) {
          aCoord = nscoord(aText->mLineHeight.GetPercentValue() * font->mSize);
          rv = NS_OK;
        }
        break;
      case eStyleUnit_Factor:
        if (font) {
          aCoord = nscoord(aText->mLineHeight.GetFactorValue() * font->mSize);
          rv = NS_OK;
        }
        break;
      default:
        break;
    }
  }

  if (NS_FAILED(rv))
    aCoord = 0;

  return rv;
}

nsresult
nsComputedDOMStyle::GetBorderColorsFor(PRUint8 aSide, nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  const nsStyleBorder *border = nsnull;
  GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)border, aFrame);

  if (border && border->mBorderColors) {
    nsBorderColors* borderColors = nsnull;
    switch (aSide) {
      case NS_SIDE_TOP:
        borderColors = border->mBorderColors[0];
        break;
      case NS_SIDE_RIGHT:
        borderColors = border->mBorderColors[1];
        break;
      case NS_SIDE_BOTTOM:
        borderColors = border->mBorderColors[2];
        break;
      case NS_SIDE_LEFT:
        borderColors = border->mBorderColors[3];
        break;
      default:
        NS_WARNING("double check the side");
        break;
    }

    if (borderColors) {
      nsDOMCSSValueList *valueList = GetROCSSValueList(PR_FALSE);
      NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

      do {
        nsROCSSPrimitiveValue *primitive = GetROCSSPrimitiveValue();
        if (!primitive) {
          delete valueList;

          return NS_ERROR_OUT_OF_MEMORY;
        }
        if (borderColors->mTransparent) {
          primitive->SetIdent(nsLayoutAtoms::transparent);
        } else {
          nsDOMCSSRGBColor *rgb = GetDOMCSSRGBColor(borderColors->mColor);
          if (rgb) {
            primitive->SetColor(rgb);
          } else {
            delete valueList;
            delete primitive;

            return NS_ERROR_OUT_OF_MEMORY;
          }
        }

        PRBool success = valueList->AppendCSSValue(primitive);
        if (!success) {
          delete valueList;
          delete primitive;

          return NS_ERROR_OUT_OF_MEMORY;
        }
        borderColors = borderColors->mNext;
      } while (borderColors);

      return CallQueryInterface(valueList, aValue);
    }
  }

  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsLayoutAtoms::none);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRadiusFor(PRUint8 aSide, nsIFrame *aFrame,
                                       nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBorder *border = nsnull;
  GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)border, aFrame);

  if (border) {
    nsStyleCoord coord;
    switch (aSide) {
      case NS_SIDE_TOP:
        border->mBorderRadius.GetTop(coord);
        break;
      case NS_SIDE_BOTTOM:
        border->mBorderRadius.GetBottom(coord);
        break;
      case NS_SIDE_LEFT:
        border->mBorderRadius.GetLeft(coord);
        break;
      case NS_SIDE_RIGHT:
        border->mBorderRadius.GetRight(coord);
        break;
      default:
        NS_WARNING("double check the side");
        break;
    }

    switch (coord.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(coord.GetCoordValue());
        break;
      case eStyleUnit_Percent:
        if (aFrame) {
          val->SetTwips(coord.GetPercentValue() * aFrame->GetSize().width);
        } else {
          val->SetPercent(coord.GetPercentValue());
        }
        break;
      default:
#ifdef DEBUG_ComputedDOMStyle
        NS_WARNING("double check the border radius");
#endif
        break;
    }
  } else {
    val->SetTwips(0);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderWidthFor(PRUint8 aSide, nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBorder* border = nsnull;
  GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)border, aFrame);

  if (border) {
    nsStyleCoord coord;
    PRUint8 borderStyle = border->GetBorderStyle(aSide);
    if (borderStyle == NS_STYLE_BORDER_STYLE_NONE) {
      coord.SetCoordValue(0);
    } else {
      switch(aSide) {
        case NS_SIDE_TOP:
          border->mBorder.GetTop(coord); break;
        case NS_SIDE_BOTTOM :
          border->mBorder.GetBottom(coord); break;
        case NS_SIDE_LEFT :
          border->mBorder.GetLeft(coord); break;
        case NS_SIDE_RIGHT :
          border->mBorder.GetRight(coord); break;
        default:
          NS_WARNING("double check the side");
          break;
      }
   }

    switch(coord.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(coord.GetCoordValue());  break;
      case eStyleUnit_Integer:
      case eStyleUnit_Proportional:
      case eStyleUnit_Enumerated:
      case eStyleUnit_Chars:
        {
          const nsAFlatCString& width=
            nsCSSProps::SearchKeywordTable(coord.GetIntValue(),
                                           nsCSSProps::kBorderWidthKTable);
          val->SetIdent(width);
          break;
        }
      default:
        NS_WARNING("double check the unit");
        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderColorFor(PRUint8 aSide, nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBorder* border = nsnull;
  GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)border, aFrame);

  if (border) {
    nscolor color; 
    PRBool transparent;
    PRBool foreground;
    border->GetBorderColor(aSide, color, transparent, foreground);
    if (transparent) {
      val->SetIdent(nsLayoutAtoms::transparent);
    } else {
      if (foreground) {
        const nsStyleColor* colorStruct = nsnull;
        GetStyleData(eStyleStruct_Color, (const nsStyleStruct*&)colorStruct,
                     aFrame);
        color = colorStruct->mColor;
      }

      nsDOMCSSRGBColor *rgb = GetDOMCSSRGBColor(color);
      if (!rgb) {
        delete val;

        return NS_ERROR_OUT_OF_MEMORY;
      }

      val->SetColor(rgb);
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginWidthFor(PRUint8 aSide, nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  FlushPendingReflows();

  nscoord width = GetMarginWidthCoordFor(aSide, aFrame);
  val->SetTwips(width);

  return CallQueryInterface(val, aValue);
}

nscoord
nsComputedDOMStyle::GetMarginWidthCoordFor(PRUint8 aSide, nsIFrame *aFrame)
{
  const nsStyleMargin* marginData = nsnull;
  GetStyleData(eStyleStruct_Margin, (const nsStyleStruct*&)marginData, aFrame);
  if (marginData) {
    nsMargin margin;
    marginData->CalcMarginFor(aFrame, margin);
    switch(aSide) {
      case NS_SIDE_TOP    :
        return margin.top;
      case NS_SIDE_BOTTOM :
        return margin.bottom;
      case NS_SIDE_LEFT   :
        return margin.left;
      case NS_SIDE_RIGHT  :
        return margin.right;
      default:
        NS_WARNING("double check the side");
        break;
    }
  }

  return 0;
}

nsresult
nsComputedDOMStyle::GetBorderStyleFor(PRUint8 aSide, nsIFrame *aFrame,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBorder* border = nsnull;
  GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)border, aFrame);

  PRUint8 borderStyle = NS_STYLE_BORDER_STYLE_NONE;

  if (border) {
    borderStyle = border->GetBorderStyle(aSide);
  }

  if (borderStyle != NS_STYLE_BORDER_STYLE_NONE) {
    const nsAFlatCString& style=
      nsCSSProps::SearchKeywordTable(borderStyle,
                                     nsCSSProps::kBorderStyleKTable);
    val->SetIdent(style);
  } else {
    val->SetIdent(nsLayoutAtoms::none);
  }

  return CallQueryInterface(val, aValue);
}


#define COMPUTED_STYLE_MAP_ENTRY(_prop, _method)  \
  { eCSSProperty_##_prop, &nsComputedDOMStyle::Get##_method }

const nsComputedDOMStyle::ComputedStyleMapEntry*
nsComputedDOMStyle::GetQueryablePropertyMap(PRUint32* aLength)
{
  /* ******************************************************************* *\
   * Properties below are listed in alphabetical order.                  *
   * Please keep them that way.                                          *
   *                                                                     *
   * Properties commented out with // are not yet implemented            *
   * Properties commented out with //// are shorthands and not queryable *
  \* ******************************************************************* */
  static
#ifndef XP_MACOSX
    // XXX If this actually fixes the bustage, replace this with an
    // autoconf test.
  const
#endif
  ComputedStyleMapEntry map[] = {
    /* ****************************** *\
     * Implementations of CSS2 styles *
    \* ****************************** */

    // COMPUTED_STYLE_MAP_ENTRY(azimuth,                    Azimuth),
    //// COMPUTED_STYLE_MAP_ENTRY(background,               Background),
    COMPUTED_STYLE_MAP_ENTRY(background_attachment,         BackgroundAttachment),
    COMPUTED_STYLE_MAP_ENTRY(background_color,              BackgroundColor),
    COMPUTED_STYLE_MAP_ENTRY(background_image,              BackgroundImage),
    //// COMPUTED_STYLE_MAP_ENTRY(background_position,      BackgroundPosition),
    COMPUTED_STYLE_MAP_ENTRY(background_repeat,             BackgroundRepeat),
    //// COMPUTED_STYLE_MAP_ENTRY(border,                   Border),
    //// COMPUTED_STYLE_MAP_ENTRY(border_bottom,            BorderBottom),
    COMPUTED_STYLE_MAP_ENTRY(border_bottom_color,           BorderBottomColor),
    COMPUTED_STYLE_MAP_ENTRY(border_bottom_style,           BorderBottomStyle),
    COMPUTED_STYLE_MAP_ENTRY(border_bottom_width,           BorderBottomWidth),
    COMPUTED_STYLE_MAP_ENTRY(border_collapse,               BorderCollapse),
    //// COMPUTED_STYLE_MAP_ENTRY(border_color,             BorderColor),
    //// COMPUTED_STYLE_MAP_ENTRY(border_left,              BorderLeft),
    COMPUTED_STYLE_MAP_ENTRY(border_left_color,             BorderLeftColor),
    COMPUTED_STYLE_MAP_ENTRY(border_left_style,             BorderLeftStyle),
    COMPUTED_STYLE_MAP_ENTRY(border_left_width,             BorderLeftWidth),
    //// COMPUTED_STYLE_MAP_ENTRY(border_right,             BorderRight),
    COMPUTED_STYLE_MAP_ENTRY(border_right_color,            BorderRightColor),
    COMPUTED_STYLE_MAP_ENTRY(border_right_style,            BorderRightStyle),
    COMPUTED_STYLE_MAP_ENTRY(border_right_width,            BorderRightWidth),
    //// COMPUTED_STYLE_MAP_ENTRY(border_spacing,           BorderSpacing),
    //// COMPUTED_STYLE_MAP_ENTRY(border_style,             BorderStyle),
    //// COMPUTED_STYLE_MAP_ENTRY(border_top,               BorderTop),
    COMPUTED_STYLE_MAP_ENTRY(border_top_color,              BorderTopColor),
    COMPUTED_STYLE_MAP_ENTRY(border_top_style,              BorderTopStyle),
    COMPUTED_STYLE_MAP_ENTRY(border_top_width,              BorderTopWidth),
    //// COMPUTED_STYLE_MAP_ENTRY(border_width,             BorderWidth),
    COMPUTED_STYLE_MAP_ENTRY(bottom,                        Bottom),
    COMPUTED_STYLE_MAP_ENTRY(caption_side,                  CaptionSide),
    COMPUTED_STYLE_MAP_ENTRY(clear,                         Clear),
    COMPUTED_STYLE_MAP_ENTRY(clip,                          Clip),
    COMPUTED_STYLE_MAP_ENTRY(color,                         Color),
    // COMPUTED_STYLE_MAP_ENTRY(content,                    Content),
    // COMPUTED_STYLE_MAP_ENTRY(counter_increment,          CounterIncrement),
    // COMPUTED_STYLE_MAP_ENTRY(counter_reset,              CounterReset),
    //// COMPUTED_STYLE_MAP_ENTRY(cue,                      Cue),
    // COMPUTED_STYLE_MAP_ENTRY(cue_after,                  CueAfter),
    // COMPUTED_STYLE_MAP_ENTRY(cue_before,                 CueBefore),
    COMPUTED_STYLE_MAP_ENTRY(cursor,                        Cursor),
    COMPUTED_STYLE_MAP_ENTRY(direction,                     Direction),
    COMPUTED_STYLE_MAP_ENTRY(display,                       Display),
    // COMPUTED_STYLE_MAP_ENTRY(elevation,                  Elevation),
    COMPUTED_STYLE_MAP_ENTRY(empty_cells,                   EmptyCells),
    COMPUTED_STYLE_MAP_ENTRY(float,                         CssFloat),
    //// COMPUTED_STYLE_MAP_ENTRY(font,                     Font),
    COMPUTED_STYLE_MAP_ENTRY(font_family,                   FontFamily),
    COMPUTED_STYLE_MAP_ENTRY(font_size,                     FontSize),
    COMPUTED_STYLE_MAP_ENTRY(font_size_adjust,              FontSizeAdjust),
    // COMPUTED_STYLE_MAP_ENTRY(font_stretch,               FontStretch),
    COMPUTED_STYLE_MAP_ENTRY(font_style,                    FontStyle),
    COMPUTED_STYLE_MAP_ENTRY(font_variant,                  FontVariant),
    COMPUTED_STYLE_MAP_ENTRY(font_weight,                   FontWeight),
    COMPUTED_STYLE_MAP_ENTRY(height,                        Height),
    COMPUTED_STYLE_MAP_ENTRY(left,                          Left),
    COMPUTED_STYLE_MAP_ENTRY(letter_spacing,                LetterSpacing),
    COMPUTED_STYLE_MAP_ENTRY(line_height,                   LineHeight),
    //// COMPUTED_STYLE_MAP_ENTRY(list_style,               ListStyle),
    COMPUTED_STYLE_MAP_ENTRY(list_style_image,              ListStyleImage),
    COMPUTED_STYLE_MAP_ENTRY(list_style_position,           ListStylePosition),
    COMPUTED_STYLE_MAP_ENTRY(list_style_type,               ListStyleType),
    //// COMPUTED_STYLE_MAP_ENTRY(margin,                   Margin),
    COMPUTED_STYLE_MAP_ENTRY(margin_bottom,                 MarginBottomWidth),
    COMPUTED_STYLE_MAP_ENTRY(margin_left,                   MarginLeftWidth),
    COMPUTED_STYLE_MAP_ENTRY(margin_right,                  MarginRightWidth),
    COMPUTED_STYLE_MAP_ENTRY(margin_top,                    MarginTopWidth),
    COMPUTED_STYLE_MAP_ENTRY(marker_offset,                 MarkerOffset),
    // COMPUTED_STYLE_MAP_ENTRY(marks,                      Marks),
    COMPUTED_STYLE_MAP_ENTRY(max_height,                    MaxHeight),
    COMPUTED_STYLE_MAP_ENTRY(max_width,                     MaxWidth),
    COMPUTED_STYLE_MAP_ENTRY(min_height,                    MinHeight),
    COMPUTED_STYLE_MAP_ENTRY(min_width,                     MinWidth),
    // COMPUTED_STYLE_MAP_ENTRY(orphans,                    Orphans),
    //// COMPUTED_STYLE_MAP_ENTRY(outline,                  Outline),
    // COMPUTED_STYLE_MAP_ENTRY(outline_color,              OutlineColor),
    // COMPUTED_STYLE_MAP_ENTRY(outline_style,              OutlineStyle),
    // COMPUTED_STYLE_MAP_ENTRY(outline_width,              OutlineWidth),
    COMPUTED_STYLE_MAP_ENTRY(overflow,                      Overflow),
    //// COMPUTED_STYLE_MAP_ENTRY(padding,                  Padding),
    COMPUTED_STYLE_MAP_ENTRY(padding_bottom,                PaddingBottom),
    COMPUTED_STYLE_MAP_ENTRY(padding_left,                  PaddingLeft),
    COMPUTED_STYLE_MAP_ENTRY(padding_right,                 PaddingRight),
    COMPUTED_STYLE_MAP_ENTRY(padding_top,                   PaddingTop),
    // COMPUTED_STYLE_MAP_ENTRY(page,                       Page),
    // COMPUTED_STYLE_MAP_ENTRY(page_break_after,           PageBreakAfter),
    // COMPUTED_STYLE_MAP_ENTRY(page_break_before,          PageBreakBefore),
    // COMPUTED_STYLE_MAP_ENTRY(page_break_inside,          PageBreakInside),
    //// COMPUTED_STYLE_MAP_ENTRY(pause,                    Pause),
    // COMPUTED_STYLE_MAP_ENTRY(pause_after,                PauseAfter),
    // COMPUTED_STYLE_MAP_ENTRY(pause_before,               PauseBefore),
    // COMPUTED_STYLE_MAP_ENTRY(pitch,                      Pitch),
    // COMPUTED_STYLE_MAP_ENTRY(pitch_range,                PitchRange),
    //// COMPUTED_STYLE_MAP_ENTRY(play_during,              PlayDuring),
    COMPUTED_STYLE_MAP_ENTRY(position,                      Position),
    // COMPUTED_STYLE_MAP_ENTRY(quotes,                     Quotes),
    // COMPUTED_STYLE_MAP_ENTRY(richness,                   Richness),
    COMPUTED_STYLE_MAP_ENTRY(right,                         Right),
    //// COMPUTED_STYLE_MAP_ENTRY(size,                     Size),
    // COMPUTED_STYLE_MAP_ENTRY(speak,                      Speak),
    // COMPUTED_STYLE_MAP_ENTRY(speak_header,               SpeakHeader),
    // COMPUTED_STYLE_MAP_ENTRY(speak_numeral,              SpeakNumeral),
    // COMPUTED_STYLE_MAP_ENTRY(speak_punctuation,          SpeakPunctuation),
    // COMPUTED_STYLE_MAP_ENTRY(speech_rate,                SpeechRate),
    // COMPUTED_STYLE_MAP_ENTRY(stress,                     Stress),
    COMPUTED_STYLE_MAP_ENTRY(table_layout,                  TableLayout),
    COMPUTED_STYLE_MAP_ENTRY(text_align,                    TextAlign),
    COMPUTED_STYLE_MAP_ENTRY(text_decoration,               TextDecoration),
    COMPUTED_STYLE_MAP_ENTRY(text_indent,                   TextIndent),
    // COMPUTED_STYLE_MAP_ENTRY(text_shadow,                TextShadow),
    COMPUTED_STYLE_MAP_ENTRY(text_transform,                TextTransform),
    COMPUTED_STYLE_MAP_ENTRY(top,                           Top),
    COMPUTED_STYLE_MAP_ENTRY(unicode_bidi,                  UnicodeBidi),
    COMPUTED_STYLE_MAP_ENTRY(vertical_align,                VerticalAlign),
    COMPUTED_STYLE_MAP_ENTRY(visibility,                    Visibility),
    // COMPUTED_STYLE_MAP_ENTRY(voice_family,               VoiceFamily),
    // COMPUTED_STYLE_MAP_ENTRY(volume,                     Volume),
    COMPUTED_STYLE_MAP_ENTRY(white_space,                   WhiteSpace),
    // COMPUTED_STYLE_MAP_ENTRY(widows,                     Widows),
    COMPUTED_STYLE_MAP_ENTRY(width,                         Width),
    COMPUTED_STYLE_MAP_ENTRY(word_spacing,                  WordSpacing),
    COMPUTED_STYLE_MAP_ENTRY(z_index,                       ZIndex),

    /* ******************************* *\
     * Implementations of -moz- styles *
    \* ******************************* */

    COMPUTED_STYLE_MAP_ENTRY(appearance,                    Appearance),
    COMPUTED_STYLE_MAP_ENTRY(_moz_background_clip,          BackgroundClip),
    COMPUTED_STYLE_MAP_ENTRY(_moz_background_inline_policy, BackgroundInlinePolicy),
    COMPUTED_STYLE_MAP_ENTRY(_moz_background_origin,        BackgroundOrigin),
    COMPUTED_STYLE_MAP_ENTRY(binding,                       Binding),
    COMPUTED_STYLE_MAP_ENTRY(border_bottom_colors,          BorderBottomColors),
    COMPUTED_STYLE_MAP_ENTRY(border_left_colors,            BorderLeftColors),
    COMPUTED_STYLE_MAP_ENTRY(border_right_colors,           BorderRightColors),
    COMPUTED_STYLE_MAP_ENTRY(border_top_colors,             BorderTopColors),
    COMPUTED_STYLE_MAP_ENTRY(_moz_border_radius_bottomLeft, BorderRadiusBottomLeft),
    COMPUTED_STYLE_MAP_ENTRY(_moz_border_radius_bottomRight,BorderRadiusBottomRight),
    COMPUTED_STYLE_MAP_ENTRY(_moz_border_radius_topLeft,    BorderRadiusTopLeft),
    COMPUTED_STYLE_MAP_ENTRY(_moz_border_radius_topRight,   BorderRadiusTopRight),
    COMPUTED_STYLE_MAP_ENTRY(box_align,                     BoxAlign),
    COMPUTED_STYLE_MAP_ENTRY(box_direction,                 BoxDirection),
    COMPUTED_STYLE_MAP_ENTRY(box_flex,                      BoxFlex),
    COMPUTED_STYLE_MAP_ENTRY(box_ordinal_group,             BoxOrdinalGroup),
    COMPUTED_STYLE_MAP_ENTRY(box_orient,                    BoxOrient),
    COMPUTED_STYLE_MAP_ENTRY(box_pack,                      BoxPack),
    COMPUTED_STYLE_MAP_ENTRY(box_sizing,                    BoxSizing),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_count,             ColumnCount),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_width,             ColumnWidth),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_gap,               ColumnGap),
    COMPUTED_STYLE_MAP_ENTRY(float_edge,                    FloatEdge),
    COMPUTED_STYLE_MAP_ENTRY(image_region,                  ImageRegion),
    COMPUTED_STYLE_MAP_ENTRY(opacity,                       Opacity),
    //// COMPUTED_STYLE_MAP_ENTRY(_moz_outline,             MozOutline),
    COMPUTED_STYLE_MAP_ENTRY(_moz_outline_color,            OutlineColor),
    COMPUTED_STYLE_MAP_ENTRY(_moz_outline_style,            OutlineStyle),
    COMPUTED_STYLE_MAP_ENTRY(_moz_outline_width,            OutlineWidth),
    COMPUTED_STYLE_MAP_ENTRY(user_focus,                    UserFocus),
    COMPUTED_STYLE_MAP_ENTRY(user_input,                    UserInput),
    COMPUTED_STYLE_MAP_ENTRY(user_modify,                   UserModify),
    COMPUTED_STYLE_MAP_ENTRY(user_select,                   UserSelect)
  };

  *aLength = NS_ARRAY_LENGTH(map);

  return map;
}

