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
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Christian Biesinger <cbiesinger@web.de>
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

/* DOM object returned from element.getComputedStyle() */

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
#include "nsGkAtoms.h"
#include "nsHTMLReflowState.h"
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

static nsIFrame*
GetContainingBlockFor(nsIFrame* aFrame) {
  if (!aFrame) {
    return nsnull;
  }
  return nsHTMLReflowState::GetContainingBlockFor(aFrame);
}

nsComputedDOMStyle::nsComputedDOMStyle()
  : mInner(this), mDocumentWeak(nsnull), mFrame(nsnull), mT2P(0.0f)
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

  mDocumentWeak = do_GetWeakReference(aPresShell->GetDocument());

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

  nsPresContext *presCtx = aPresShell->GetPresContext();
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

  nsCOMPtr<nsIDocument> document = do_QueryReferent(mDocumentWeak);
  NS_ENSURE_TRUE(document, NS_ERROR_NOT_AVAILABLE);

  // Flush _before_ getting the presshell, since that could create a new
  // presshell.  Also note that we want to flush the style on the document
  // we're computing style in, not on the document mContent is in -- the two
  // may be different.
  document->FlushPendingNotifications(Flush_Style);

  nsIPresShell* presShell = document->GetShellAt(0);
  NS_ENSURE_TRUE(presShell, NS_ERROR_NOT_AVAILABLE);

  mFrame = presShell->GetPrimaryFrameFor(mContent);
  if (!mFrame || mPseudo) {
    // Need to resolve a style context
    mStyleContextHolder =
      nsInspectorCSSUtils::GetStyleContextForContent(mContent,
                                                     mPseudo,
                                                     presShell);
    NS_ENSURE_TRUE(mStyleContextHolder, NS_ERROR_OUT_OF_MEMORY);
  }

  nsresult rv = NS_OK;

  nsCSSProperty prop = nsCSSProps::LookupProperty(aPropertyName);

  PRUint32 i = 0;
  PRUint32 length = 0;
  const ComputedStyleMapEntry* propMap = GetQueryablePropertyMap(&length);
  for (; i < length; ++i) {
    if (prop == propMap[i].mProperty) {
      // Call our pointer-to-member-function.
      rv = (this->*(propMap[i].mGetter))(aReturn);
      break;
    }
  }

#ifdef DEBUG_ComputedDOMStyle
  if (i == length) {
    NS_WARNING(PromiseFlatCString(NS_ConvertUTF16toUTF8(aPropertyName) + 
                                  NS_LITERAL_CSTRING(" is not queryable!")).get());
  }
#endif

  if (NS_FAILED(rv)) {
    *aReturn = nsnull;
  }

  mFrame = nsnull;

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
    CopyASCIItoUTF16(nsCSSProps::GetStringValue(propMap[aIndex].mProperty),
                    aReturn);
  }

  return NS_OK;
}


// Property getters...

nsresult
nsComputedDOMStyle::GetBinding(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = GetStyleDisplay();

  if (display->mBinding) {
    val->SetURI(display->mBinding);
  } else {
    val->SetIdent(nsGkAtoms::none);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetClear(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay *display = GetStyleDisplay();

  if (display->mBreakType != NS_STYLE_CLEAR_NONE) {
    const nsAFlatCString& clear =
      nsCSSProps::ValueToKeyword(display->mBreakType,
                                 nsCSSProps::kClearKTable);
    val->SetIdent(clear);
  } else {
    val->SetIdent(nsGkAtoms::none);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetCssFloat(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = GetStyleDisplay();

  if (display->mFloats != NS_STYLE_FLOAT_NONE) {
    const nsAFlatCString& cssFloat =
      nsCSSProps::ValueToKeyword(display->mFloats,
                                 nsCSSProps::kFloatKTable);
    val->SetIdent(cssFloat);
  } else {
    val->SetIdent(nsGkAtoms::none);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBottom(nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_BOTTOM, aValue);
}

nsresult
nsComputedDOMStyle::SetToRGBAColor(nsROCSSPrimitiveValue* aValue,
                                   nscolor aColor)
{
  if (NS_GET_A(aColor) == 0) {
    aValue->SetIdent(nsGkAtoms::transparent);
    return NS_OK;
  }

  nsROCSSPrimitiveValue *red   = GetROCSSPrimitiveValue();
  nsROCSSPrimitiveValue *green = GetROCSSPrimitiveValue();
  nsROCSSPrimitiveValue *blue  = GetROCSSPrimitiveValue();
  nsROCSSPrimitiveValue *alpha  = GetROCSSPrimitiveValue();

  if (red && green && blue && alpha) {
    nsDOMCSSRGBColor *rgbColor =
      new nsDOMCSSRGBColor(red, green, blue, alpha, NS_GET_A(aColor) < 255);

    if (rgbColor) {
      red->SetNumber(NS_GET_R(aColor));
      green->SetNumber(NS_GET_G(aColor));
      blue->SetNumber(NS_GET_B(aColor));
      alpha->SetNumber(float(NS_GET_A(aColor)) / 255.0f);

      aValue->SetColor(rgbColor);
      return NS_OK;
    }
  }

  delete red;
  delete green;
  delete blue;
  delete alpha;

  return NS_ERROR_OUT_OF_MEMORY;
}

nsresult
nsComputedDOMStyle::GetColor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleColor* color = GetStyleColor();

  nsresult rv = SetToRGBAColor(val, color->mColor);
  if (NS_FAILED(rv)) {
    delete val;
    return rv;
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOpacity(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleDisplay()->mOpacity);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetColumnCount(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleColumn* column = GetStyleColumn();

  if (column->mColumnCount == NS_STYLE_COLUMN_COUNT_AUTO) {
    val->SetIdent(nsGkAtoms::_auto);
  } else {
    val->SetNumber(column->mColumnCount);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetColumnWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleColumn* column = GetStyleColumn();

  switch (column->mColumnWidth.GetUnit()) {
    case eStyleUnit_Coord:
      val->SetTwips(column->mColumnWidth.GetCoordValue());
      break;
    case eStyleUnit_Auto:
      // XXX fix this. When we actually have a column frame, I think
      // we should return the computed column width.
      val->SetIdent(nsGkAtoms::_auto);
      break;
    default:
      NS_ERROR("Unexpected column width unit");
      val->SetTwips(0);
      break;
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetColumnGap(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleColumn* column = GetStyleColumn();

  switch (column->mColumnGap.GetUnit()) {
    case eStyleUnit_Coord:
      val->SetTwips(column->mColumnGap.GetCoordValue());
      break;
    case eStyleUnit_Percent:
      if (mFrame) {
        val->SetTwips(column->mColumnGap.GetPercentValue()*mFrame->GetSize().width);
      } else {
        val->SetPercent(column->mColumnGap.GetPercentValue());
      }
      break;
    case eStyleUnit_Normal:
      val->SetTwips(GetStyleFont()->mFont.size);
      break;
    default:
      NS_ERROR("Unexpected column gap unit");
      val->SetTwips(0);
      break;
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetCounterIncrement(nsIDOMCSSValue** aValue)
{
  const nsStyleContent *content = GetStyleContent();

  if (content->CounterIncrementCount() == 0) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
    val->SetIdent(nsGkAtoms::none);
    return CallQueryInterface(val, aValue);
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_FALSE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 i = 0, i_end = content->CounterIncrementCount(); i < i_end; ++i) {
    nsROCSSPrimitiveValue* name = GetROCSSPrimitiveValue();
    if (!name) {
      delete valueList;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!valueList->AppendCSSValue(name)) {
      delete valueList;
      delete name;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    
    nsROCSSPrimitiveValue* value = GetROCSSPrimitiveValue();
    if (!value) {
      delete valueList;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!valueList->AppendCSSValue(value)) {
      delete valueList;
      delete value;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    const nsStyleCounterData *data = content->GetCounterIncrementAt(i);
    name->SetString(data->mCounter);
    value->SetNumber(data->mValue); // XXX This should really be integer
  }

  return CallQueryInterface(valueList, aValue);
}

nsresult
nsComputedDOMStyle::GetCounterReset(nsIDOMCSSValue** aValue)
{
  const nsStyleContent *content = GetStyleContent();

  if (content->CounterResetCount() == 0) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
    val->SetIdent(nsGkAtoms::none);
    return CallQueryInterface(val, aValue);
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_FALSE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 i = 0, i_end = content->CounterResetCount(); i < i_end; ++i) {
    nsROCSSPrimitiveValue* name = GetROCSSPrimitiveValue();
    if (!name) {
      delete valueList;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!valueList->AppendCSSValue(name)) {
      delete valueList;
      delete name;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    
    nsROCSSPrimitiveValue* value = GetROCSSPrimitiveValue();
    if (!value) {
      delete valueList;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!valueList->AppendCSSValue(value)) {
      delete valueList;
      delete value;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    const nsStyleCounterData *data = content->GetCounterResetAt(i);
    name->SetString(data->mCounter);
    value->SetNumber(data->mValue); // XXX This should really be integer
  }

  return CallQueryInterface(valueList, aValue);
}

nsresult
nsComputedDOMStyle::GetFontFamily(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = GetStyleFont();

  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocumentWeak);
  NS_ASSERTION(doc, "document is required");
  nsIPresShell* presShell = doc->GetShellAt(0);
  NS_ASSERTION(presShell, "pres shell is required");
  nsPresContext *presContext = presShell->GetPresContext();
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

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontSize(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  // Note: GetStyleFont()->mSize is the 'computed size';
  // GetStyleFont()->mFont.size is the 'actual size'
  val->SetTwips(GetStyleFont()->mSize);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontSizeAdjust(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont *font = GetStyleFont();

  if (font->mFont.sizeAdjust) {
    val->SetNumber(font->mFont.sizeAdjust);
  } else {
    val->SetIdent(nsGkAtoms::none);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontStyle(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = GetStyleFont();

  if (font->mFont.style != NS_STYLE_FONT_STYLE_NORMAL) {
    const nsAFlatCString& style=
      nsCSSProps::ValueToKeyword(font->mFont.style,
                                 nsCSSProps::kFontStyleKTable);
    val->SetIdent(style);
  } else {
    val->SetIdent(nsGkAtoms::normal);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontWeight(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = GetStyleFont();

  const nsAFlatCString& str_weight=
    nsCSSProps::ValueToKeyword(font->mFont.weight,
                               nsCSSProps::kFontWeightKTable);
  if (!str_weight.IsEmpty()) {
    val->SetIdent(str_weight);
  } else {
    val->SetNumber(font->mFont.weight);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontVariant(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = GetStyleFont();

  if (font->mFont.variant != NS_STYLE_FONT_VARIANT_NORMAL) {
    const nsAFlatCString& variant=
      nsCSSProps::ValueToKeyword(font->mFont.variant,
                                 nsCSSProps::kFontVariantKTable);
    val->SetIdent(variant);
  } else {
    val->SetIdent(nsGkAtoms::normal);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundAttachment(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground *background = GetStyleBackground();

  const nsAFlatCString& backgroundAttachment =
    nsCSSProps::ValueToKeyword(background->mBackgroundAttachment,
                               nsCSSProps::kBackgroundAttachmentKTable);
  val->SetIdent(backgroundAttachment);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundClip(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& backgroundClip =
    nsCSSProps::ValueToKeyword(GetStyleBackground()->mBackgroundClip,
                               nsCSSProps::kBackgroundClipKTable);

  val->SetIdent(backgroundClip);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundColor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground* color = GetStyleBackground();

  if (color->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT) {
    const nsAFlatCString& backgroundColor =
      nsCSSProps::ValueToKeyword(NS_STYLE_BG_COLOR_TRANSPARENT,
                                 nsCSSProps::kBackgroundColorKTable);
    val->SetIdent(backgroundColor);
  } else {
    nsresult rv = SetToRGBAColor(val, color->mBackgroundColor);
    if (NS_FAILED(rv)) {
      delete val;
      return rv;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundImage(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground* color = GetStyleBackground();

  if (color->mBackgroundFlags & NS_STYLE_BG_IMAGE_NONE) {
    val->SetIdent(nsGkAtoms::none);
  } else {
    nsCOMPtr<nsIURI> uri;
    if (color->mBackgroundImage) {
      color->mBackgroundImage->GetURI(getter_AddRefs(uri));
    }
    val->SetURI(uri);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundInlinePolicy(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& backgroundPolicy =
      nsCSSProps::ValueToKeyword(GetStyleBackground()->mBackgroundInlinePolicy,
                                 nsCSSProps::kBackgroundInlinePolicyKTable);

  val->SetIdent(backgroundPolicy);

  return CallQueryInterface(val, aValue);  
}

nsresult
nsComputedDOMStyle::GetBackgroundOrigin(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& backgroundOrigin =
    nsCSSProps::ValueToKeyword(GetStyleBackground()->mBackgroundOrigin,
                               nsCSSProps::kBackgroundOriginKTable);

  val->SetIdent(backgroundOrigin);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundRepeat(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& backgroundRepeat =
    nsCSSProps::ValueToKeyword(GetStyleBackground()->mBackgroundRepeat,
                               nsCSSProps::kBackgroundRepeatKTable);
  val->SetIdent(backgroundRepeat);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetPadding(nsIDOMCSSValue** aValue)
{
  // return null per spec.
  aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetPaddingTop(nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_TOP, aValue);
}

nsresult
nsComputedDOMStyle::GetPaddingBottom(nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_BOTTOM, aValue);
}

nsresult
nsComputedDOMStyle::GetPaddingLeft(nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::GetPaddingRight(nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderCollapse(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& ident=
    nsCSSProps::ValueToKeyword(GetStyleTableBorder()->mBorderCollapse,
                               nsCSSProps::kBorderCollapseKTable);
  val->SetIdent(ident);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderSpacing(nsIDOMCSSValue** aValue)
{
  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_FALSE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  nsROCSSPrimitiveValue* xSpacing = GetROCSSPrimitiveValue();
  if (!xSpacing) {
    delete valueList;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (!valueList->AppendCSSValue(xSpacing)) {
    delete valueList;
    delete xSpacing;
    return NS_ERROR_OUT_OF_MEMORY;
  }
    
  nsROCSSPrimitiveValue* ySpacing = GetROCSSPrimitiveValue();
  if (!ySpacing) {
    delete valueList;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (!valueList->AppendCSSValue(ySpacing)) {
    delete valueList;
    delete ySpacing;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  const nsStyleTableBorder *border = GetStyleTableBorder();
  // border-spacing will always be a coord
  xSpacing->SetTwips(border->mBorderSpacingX.GetCoordValue());
  ySpacing->SetTwips(border->mBorderSpacingY.GetCoordValue());

  return CallQueryInterface(valueList, aValue);
}

nsresult
nsComputedDOMStyle::GetCaptionSide(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& side =
    nsCSSProps::ValueToKeyword(GetStyleTableBorder()->mCaptionSide,
                               nsCSSProps::kCaptionSideKTable);
  val->SetIdent(side);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetEmptyCells(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& emptyCells =
    nsCSSProps::ValueToKeyword(GetStyleTableBorder()->mEmptyCells,
                               nsCSSProps::kEmptyCellsKTable);
  val->SetIdent(emptyCells);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTableLayout(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTable *table = GetStyleTable();

  if (table->mLayoutStrategy != NS_STYLE_TABLE_LAYOUT_AUTO) {
    const nsAFlatCString& tableLayout =
      nsCSSProps::ValueToKeyword(table->mLayoutStrategy,
                                 nsCSSProps::kTableLayoutKTable);
    val->SetIdent(tableLayout);
  } else {
    val->SetIdent(nsGkAtoms::_auto);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderStyle(nsIDOMCSSValue** aValue)
{
  // return null per spec.
  aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetBorderTopStyle(nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_TOP, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderBottomStyle(nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_BOTTOM, aValue);
}
nsresult
nsComputedDOMStyle::GetBorderLeftStyle(nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRightStyle(nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderBottomColors(nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_BOTTOM, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderLeftColors(nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRightColors(nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_RIGHT, aValue);
}


nsresult
nsComputedDOMStyle::GetBorderTopColors(nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_TOP, aValue);
}


nsresult
nsComputedDOMStyle::GetBorderRadiusBottomLeft(nsIDOMCSSValue** aValue)
{
  return GetBorderRadiusFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRadiusBottomRight(nsIDOMCSSValue** aValue)
{
  return GetBorderRadiusFor(NS_SIDE_BOTTOM, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRadiusTopLeft(nsIDOMCSSValue** aValue)
{
  return GetBorderRadiusFor(NS_SIDE_TOP, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRadiusTopRight(nsIDOMCSSValue** aValue)
{
  return GetBorderRadiusFor(NS_SIDE_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderWidth(nsIDOMCSSValue** aValue)
{
  // return null per spec.
  aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetBorderTopWidth(nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_TOP, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderBottomWidth(nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_BOTTOM, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderLeftWidth(nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRightWidth(nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderTopColor(nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_TOP, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderBottomColor(nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_BOTTOM, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderLeftColor(nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRightColor(nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginWidth(nsIDOMCSSValue** aValue)
{
  // return null per spec.
  aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetMarginTopWidth(nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_TOP, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginBottomWidth(nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_BOTTOM, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginLeftWidth(nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginRightWidth(nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::GetMarkerOffset(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleContent* content = GetStyleContent();

  switch (content->mMarkerOffset.GetUnit()) {
    case eStyleUnit_Coord:
      val->SetTwips(content->mMarkerOffset.GetCoordValue());
      break;
    case eStyleUnit_Auto:
      val->SetIdent(nsGkAtoms::_auto);
      break;
    case eStyleUnit_Null: // XXX doesn't seem like a valid unit per CSS2?
      val->SetIdent(nsGkAtoms::none);
      break;
    default:
      NS_ERROR("Unexpected marker offset unit");
      val->SetTwips(0);
      break;
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOutline(nsIDOMCSSValue** aValue)
{
  // return null per spec.
  aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetOutlineWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleOutline* outline = GetStyleOutline();

  nsStyleCoord coord;
  PRUint8 outlineStyle = outline->GetOutlineStyle();
  if (outlineStyle == NS_STYLE_BORDER_STYLE_NONE) {
    coord.SetCoordValue(0);
  } else {
    coord = outline->mOutlineWidth;
  }
  switch (coord.GetUnit()) {
    case eStyleUnit_Coord:
      val->SetTwips(coord.GetCoordValue());
      break;
    case eStyleUnit_Enumerated:
      {
        const nsAFlatCString& width =
          nsCSSProps::ValueToKeyword(coord.GetIntValue(),
                                     nsCSSProps::kBorderWidthKTable);
        val->SetIdent(width);
        break;
      }
    case eStyleUnit_Chars:
      // XXX we need a frame and a rendering context to calculate this, bug 281972, bug 282126.
      val->SetTwips(0);
      break;
    default:
      NS_ERROR("Unexpected outline width unit");
      val->SetTwips(0);
      break;
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineStyle(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  PRUint8 outlineStyle = GetStyleOutline()->GetOutlineStyle();
  switch (outlineStyle) {
  case NS_STYLE_BORDER_STYLE_NONE:
    val->SetIdent(nsGkAtoms::none);
    break;
  case NS_STYLE_BORDER_STYLE_AUTO:
    val->SetIdent(nsGkAtoms::_auto);
    break;
  default:
    const nsAFlatCString& style =
      nsCSSProps::ValueToKeyword(outlineStyle,
                                 nsCSSProps::kOutlineStyleKTable);
    val->SetIdent(style);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineOffset(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleOutline* outline = GetStyleOutline();

  switch (outline->mOutlineOffset.GetUnit()) {
    case eStyleUnit_Coord:
      val->SetTwips(outline->mOutlineOffset.GetCoordValue());
      break;
    case eStyleUnit_Chars:
      // XXX we need a frame and a rendering context to calculate this, bug 281972, bug 282126.
      val->SetTwips(0);
      break;
    default:
      NS_ERROR("Unexpected outline offset unit");
      val->SetTwips(0);
      break;
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineRadiusBottomLeft(nsIDOMCSSValue** aValue)
{
  return GetOutlineRadiusFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineRadiusBottomRight(nsIDOMCSSValue** aValue)
{
  return GetOutlineRadiusFor(NS_SIDE_BOTTOM, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineRadiusTopLeft(nsIDOMCSSValue** aValue)
{
  return GetOutlineRadiusFor(NS_SIDE_TOP, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineRadiusTopRight(nsIDOMCSSValue** aValue)
{
  return GetOutlineRadiusFor(NS_SIDE_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineColor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nscolor color;
  GetStyleOutline()->GetOutlineColor(color);

  nsresult rv = SetToRGBAColor(val, color);
  if (NS_FAILED(rv)) {
    delete val;
    return rv;
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineRadiusFor(PRUint8 aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nsStyleCoord coord;
  GetStyleOutline()->mOutlineRadius.Get(aSide, coord);

  switch (coord.GetUnit()) {
    case eStyleUnit_Coord:
      val->SetTwips(coord.GetCoordValue());
      break;
    case eStyleUnit_Percent:
      if (mFrame) {
        val->SetTwips(coord.GetPercentValue() * mFrame->GetSize().width);
      } else {
        val->SetPercent(coord.GetPercentValue());
      }
      break;
    default:
      NS_ERROR("Unexpected outline radius unit");
      break;
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetZIndex(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition* position = GetStylePosition();

  switch (position->mZIndex.GetUnit()) {
    case eStyleUnit_Integer:
      val->SetNumber(position->mZIndex.GetIntValue());
      break;
    default:
      NS_ERROR("Unexpected z-index unit");
      // fall through
    case eStyleUnit_Auto:
      val->SetIdent(nsGkAtoms::_auto);
      break;
  }
    
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetListStyleImage(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleList* list = GetStyleList();

  if (!list->mListStyleImage) {
    val->SetIdent(nsGkAtoms::none);
  } else {
    nsCOMPtr<nsIURI> uri;
    if (list->mListStyleImage) {
      list->mListStyleImage->GetURI(getter_AddRefs(uri));
    }
    val->SetURI(uri);
  }
    
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetListStylePosition(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& style =
    nsCSSProps::ValueToKeyword(GetStyleList()->mListStylePosition,
                               nsCSSProps::kListStylePositionKTable);
  val->SetIdent(style);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetListStyleType(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleList *list = GetStyleList();

  if (list->mListStyleType == NS_STYLE_LIST_STYLE_NONE) {
    val->SetIdent(nsGkAtoms::none);
  } else {
    const nsAFlatCString& style =
      nsCSSProps::ValueToKeyword(list->mListStyleType,
                                 nsCSSProps::kListStyleKTable);
    val->SetIdent(style);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetImageRegion(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleList* list = GetStyleList();

  nsresult rv = NS_OK;
  nsROCSSPrimitiveValue *topVal = nsnull;
  nsROCSSPrimitiveValue *rightVal = nsnull;
  nsROCSSPrimitiveValue *bottomVal = nsnull;
  nsROCSSPrimitiveValue *leftVal = nsnull;
  if (list->mImageRegion.width <= 0 || list->mImageRegion.height <= 0) {
    val->SetIdent(nsGkAtoms::_auto);
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
nsComputedDOMStyle::GetLineHeight(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = GetStyleText();

  nscoord lineHeight;
  nsresult rv = GetLineHeightCoord(text, lineHeight);

  if (NS_SUCCEEDED(rv)) {
    val->SetTwips(lineHeight);
  } else {
    switch (text->mLineHeight.GetUnit()) {
      case eStyleUnit_Percent:
        val->SetPercent(text->mLineHeight.GetPercentValue());
        break;
      case eStyleUnit_Factor:
        val->SetNumber(text->mLineHeight.GetFactorValue());
        break;
      case eStyleUnit_Normal:
        val->SetIdent(nsGkAtoms::normal);
        break;
      default:
        NS_ERROR("Unexpected line-height unit");
        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetVerticalAlign(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTextReset *text = GetStyleTextReset();

  switch (text->mVerticalAlign.GetUnit()) {
    case eStyleUnit_Coord:
      val->SetTwips(text->mVerticalAlign.GetCoordValue());
      break;
    case eStyleUnit_Enumerated:
      {
        const nsAFlatCString& align =
          nsCSSProps::ValueToKeyword(text->mVerticalAlign.GetIntValue(),
                                     nsCSSProps::kVerticalAlignKTable);
        val->SetIdent(align);
        break;
      }
    case eStyleUnit_Percent:
      {
        const nsStyleText *textData = GetStyleText();

        nscoord lineHeight = 0;
        nsresult rv = GetLineHeightCoord(textData, lineHeight);

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

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextAlign(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& align=
    nsCSSProps::ValueToKeyword(GetStyleText()->mTextAlign,
                               nsCSSProps::kTextAlignKTable);
  val->SetIdent(align);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextDecoration(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTextReset* text = GetStyleTextReset();

  if (NS_STYLE_TEXT_DECORATION_NONE == text->mTextDecoration) {
    const nsAFlatCString& decoration=
      nsCSSKeywords::GetStringValue(eCSSKeyword_none);
    val->SetIdent(decoration);
  } else {
    nsAutoString decorationString;
    if (text->mTextDecoration & NS_STYLE_TEXT_DECORATION_UNDERLINE) {
      const nsAFlatCString& decoration=
        nsCSSProps::ValueToKeyword(NS_STYLE_TEXT_DECORATION_UNDERLINE,
                                   nsCSSProps::kTextDecorationKTable);
      decorationString.AppendWithConversion(decoration.get());
    }
    if (text->mTextDecoration & NS_STYLE_TEXT_DECORATION_OVERLINE) {
      if (!decorationString.IsEmpty()) {
        decorationString.Append(PRUnichar(' '));
      }
      const nsAFlatCString& decoration=
        nsCSSProps::ValueToKeyword(NS_STYLE_TEXT_DECORATION_OVERLINE,
                                   nsCSSProps::kTextDecorationKTable);
      decorationString.AppendWithConversion(decoration.get());
    }
    if (text->mTextDecoration & NS_STYLE_TEXT_DECORATION_LINE_THROUGH) {
      if (!decorationString.IsEmpty()) {
        decorationString.Append(PRUnichar(' '));
      }
      const nsAFlatCString& decoration=
        nsCSSProps::ValueToKeyword(NS_STYLE_TEXT_DECORATION_LINE_THROUGH,
                                   nsCSSProps::kTextDecorationKTable);
      decorationString.AppendWithConversion(decoration.get());
    }
    if (text->mTextDecoration & NS_STYLE_TEXT_DECORATION_BLINK) {
      if (!decorationString.IsEmpty()) {
        decorationString.Append(PRUnichar(' '));
      }
      const nsAFlatCString& decoration=
        nsCSSProps::ValueToKeyword(NS_STYLE_TEXT_DECORATION_BLINK,
                                   nsCSSProps::kTextDecorationKTable);
      decorationString.AppendWithConversion(decoration.get());
    }
    val->SetString(decorationString);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextIndent(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = GetStyleText();

  FlushPendingReflows();

  switch (text->mTextIndent.GetUnit()) {
    case eStyleUnit_Coord:
      val->SetTwips(text->mTextIndent.GetCoordValue());
      break;
    case eStyleUnit_Percent:
      {
        nsIFrame *container = GetContainingBlockFor(mFrame);
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

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextTransform(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = GetStyleText();

  if (text->mTextTransform != NS_STYLE_TEXT_TRANSFORM_NONE) {
    const nsAFlatCString& textTransform =
      nsCSSProps::ValueToKeyword(text->mTextTransform,
                                 nsCSSProps::kTextTransformKTable);
    val->SetIdent(textTransform);
  } else {
    val->SetIdent(nsGkAtoms::none);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetLetterSpacing(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = GetStyleText();

  if (text->mLetterSpacing.GetUnit() == eStyleUnit_Coord) {
    val->SetTwips(text->mLetterSpacing.GetCoordValue());
  } else {
    val->SetIdent(nsGkAtoms::normal);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetWordSpacing(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = GetStyleText();

  if (text->mWordSpacing.GetUnit() == eStyleUnit_Coord) {
    val->SetTwips(text->mWordSpacing.GetCoordValue());
  } else {
    val->SetIdent(nsGkAtoms::normal);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetWhiteSpace(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleText *text = GetStyleText();

  if (text->mWhiteSpace != NS_STYLE_WHITESPACE_NORMAL) {
    const nsAFlatCString& whiteSpace =
      nsCSSProps::ValueToKeyword(text->mWhiteSpace,
                                 nsCSSProps::kWhitespaceKTable);
    val->SetIdent(whiteSpace);
  } else {
    val->SetIdent(nsGkAtoms::normal);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetVisibility(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& value=
    nsCSSProps::ValueToKeyword(GetStyleVisibility()->mVisible,
                               nsCSSProps::kVisibilityKTable);
  val->SetIdent(value);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetDirection(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString & direction =
    nsCSSProps::ValueToKeyword(GetStyleVisibility()->mDirection,
                               nsCSSProps::kDirectionKTable);
  val->SetIdent(direction);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetUnicodeBidi(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleTextReset *text = GetStyleTextReset();

  if (text->mUnicodeBidi != NS_STYLE_UNICODE_BIDI_NORMAL) {
    const nsAFlatCString& bidi =
      nsCSSProps::ValueToKeyword(text->mUnicodeBidi,
                                 nsCSSProps::kUnicodeBidiKTable);
    val->SetIdent(bidi);
  } else {
    val->SetIdent(nsGkAtoms::normal);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetCursor(nsIDOMCSSValue** aValue)
{
  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_TRUE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleUserInterface *ui = GetStyleUserInterface();

  for (nsCursorImage *item = ui->mCursorArray,
         *item_end = ui->mCursorArray + ui->mCursorArrayLength;
       item < item_end; ++item) {
    nsDOMCSSValueList *itemList = GetROCSSValueList(PR_FALSE);
    if (!itemList || !valueList->AppendCSSValue(itemList)) {
      delete itemList;
      delete valueList;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIURI> uri;
    item->mImage->GetURI(getter_AddRefs(uri));

    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    if (!val || !itemList->AppendCSSValue(val)) {
      delete val;
      delete valueList;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    val->SetURI(uri);

    if (item->mHaveHotspot) {
      nsROCSSPrimitiveValue *valX = GetROCSSPrimitiveValue();
      if (!valX || !itemList->AppendCSSValue(valX)) {
        delete valX;
        delete valueList;
        return NS_ERROR_OUT_OF_MEMORY;
      }
      nsROCSSPrimitiveValue *valY = GetROCSSPrimitiveValue();
      if (!valY || !itemList->AppendCSSValue(valY)) {
        delete valY;
        delete valueList;
        return NS_ERROR_OUT_OF_MEMORY;
      }

      valX->SetNumber(item->mHotspotX);
      valY->SetNumber(item->mHotspotY);
    }
  }

  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  if (!val) {
    delete valueList;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (ui->mCursor == NS_STYLE_CURSOR_AUTO) {
    val->SetIdent(nsGkAtoms::_auto);
  } else {
    const nsAFlatCString& cursor =
      nsCSSProps::ValueToKeyword(ui->mCursor,
                                 nsCSSProps::kCursorKTable);
    val->SetIdent(cursor);
  }
  if (!valueList->AppendCSSValue(val)) {
    delete valueList;
    delete val;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(valueList, aValue);
}

nsresult
nsComputedDOMStyle::GetAppearance(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& appearanceIdent =
    nsCSSProps::ValueToKeyword(GetStyleDisplay()->mAppearance,
                               nsCSSProps::kAppearanceKTable);
  val->SetIdent(appearanceIdent);

  return CallQueryInterface(val, aValue);
}


nsresult
nsComputedDOMStyle::GetBoxAlign(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& boxAlignIdent =
    nsCSSProps::ValueToKeyword(GetStyleXUL()->mBoxAlign,
                               nsCSSProps::kBoxAlignKTable);
  val->SetIdent(boxAlignIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxDirection(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& boxDirectionIdent =
    nsCSSProps::ValueToKeyword(GetStyleXUL()->mBoxDirection,
                               nsCSSProps::kBoxDirectionKTable);
  val->SetIdent(boxDirectionIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxFlex(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleXUL()->mBoxFlex);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxOrdinalGroup(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleXUL()->mBoxOrdinal);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxOrient(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& boxOrientIdent =
    nsCSSProps::ValueToKeyword(GetStyleXUL()->mBoxOrient,
                               nsCSSProps::kBoxOrientKTable);
  val->SetIdent(boxOrientIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxPack(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& boxPackIdent =
    nsCSSProps::ValueToKeyword(GetStyleXUL()->mBoxPack,
                               nsCSSProps::kBoxPackKTable);
  val->SetIdent(boxPackIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxSizing(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& boxSizingIdent =
    nsCSSProps::ValueToKeyword(GetStylePosition()->mBoxSizing,
                               nsCSSProps::kBoxSizingKTable);
  val->SetIdent(boxSizingIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFloatEdge(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& floatEdgeIdent =
    nsCSSProps::ValueToKeyword(GetStyleBorder()->mFloatEdge,
                               nsCSSProps::kFloatEdgeKTable);
  val->SetIdent(floatEdgeIdent);

  return CallQueryInterface(val, aValue);
}


nsresult
nsComputedDOMStyle::GetUserFocus(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleUserInterface *uiData = GetStyleUserInterface();

  if (uiData->mUserFocus != NS_STYLE_USER_FOCUS_NONE) {
    if (uiData->mUserFocus == NS_STYLE_USER_FOCUS_NORMAL) {
      const nsAFlatCString& userFocusIdent =
              nsCSSKeywords::GetStringValue(eCSSKeyword_normal);
      val->SetIdent(userFocusIdent);
    } else {
      const nsAFlatCString& userFocusIdent =
        nsCSSProps::ValueToKeyword(uiData->mUserFocus,
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
nsComputedDOMStyle::GetUserInput(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleUserInterface *uiData = GetStyleUserInterface();

  if (uiData->mUserInput != NS_STYLE_USER_INPUT_AUTO) {
    if (uiData->mUserInput == NS_STYLE_USER_INPUT_NONE) {
      const nsAFlatCString& userInputIdent =
        nsCSSKeywords::GetStringValue(eCSSKeyword_none);
      val->SetIdent(userInputIdent);
    } else {
      const nsAFlatCString& userInputIdent =
        nsCSSProps::ValueToKeyword(uiData->mUserInput,
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
nsComputedDOMStyle::GetUserModify(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& userModifyIdent =
    nsCSSProps::ValueToKeyword(GetStyleUserInterface()->mUserModify,
                               nsCSSProps::kUserModifyKTable);
  val->SetIdent(userModifyIdent);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetUserSelect(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleUIReset *uiData = GetStyleUIReset();

  if (uiData->mUserSelect != NS_STYLE_USER_SELECT_AUTO) {
    if (uiData->mUserSelect == NS_STYLE_USER_SELECT_NONE) {
      const nsAFlatCString& userSelectIdent =
        nsCSSKeywords::GetStringValue(eCSSKeyword_none);
      val->SetIdent(userSelectIdent);
    } else {
      const nsAFlatCString& userSelectIdent =
        nsCSSProps::ValueToKeyword(uiData->mUserSelect,
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
nsComputedDOMStyle::GetDisplay(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay *displayData = GetStyleDisplay();

  if (displayData->mDisplay == NS_STYLE_DISPLAY_NONE) {
    val->SetIdent(nsGkAtoms::none);
  } else {
    const nsAFlatCString& display =
      nsCSSProps::ValueToKeyword(displayData->mDisplay,
                                 nsCSSProps::kDisplayKTable);
    val->SetIdent(display);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetPosition(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsAFlatCString& position =
    nsCSSProps::ValueToKeyword(GetStyleDisplay()->mPosition,
                               nsCSSProps::kPositionKTable);
  val->SetIdent(position);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetClip(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = GetStyleDisplay();

  nsresult rv = NS_OK;
  nsROCSSPrimitiveValue *topVal = nsnull;
  nsROCSSPrimitiveValue *rightVal = nsnull;
  nsROCSSPrimitiveValue *bottomVal = nsnull;
  nsROCSSPrimitiveValue *leftVal = nsnull;
  if (display->mClipFlags == NS_STYLE_CLIP_AUTO ||
      display->mClipFlags == (NS_STYLE_CLIP_TOP_AUTO |
                              NS_STYLE_CLIP_RIGHT_AUTO |
                              NS_STYLE_CLIP_BOTTOM_AUTO |
                              NS_STYLE_CLIP_LEFT_AUTO)) {
    val->SetIdent(nsGkAtoms::_auto);
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
          topVal->SetIdent(nsGkAtoms::_auto);
        } else {
          topVal->SetTwips(display->mClip.y);
        }
        
        if (display->mClipFlags & NS_STYLE_CLIP_RIGHT_AUTO) {
          rightVal->SetIdent(nsGkAtoms::_auto);
        } else {
          rightVal->SetTwips(display->mClip.width + display->mClip.x);
        }
        
        if (display->mClipFlags & NS_STYLE_CLIP_BOTTOM_AUTO) {
          bottomVal->SetIdent(nsGkAtoms::_auto);
        } else {
          bottomVal->SetTwips(display->mClip.height + display->mClip.y);
        }
        
        if (display->mClipFlags & NS_STYLE_CLIP_LEFT_AUTO) {
          leftVal->SetIdent(nsGkAtoms::_auto);
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
nsComputedDOMStyle::GetOverflow(nsIDOMCSSValue** aValue)
{
  const nsStyleDisplay* display = GetStyleDisplay();

  if (display->mOverflowX != display->mOverflowY) {
    // No value to return.  We can't express this combination of
    // values as a shorthand.
    *aValue = nsnull;
    return NS_OK;
  }
  
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  if (display->mOverflowX != NS_STYLE_OVERFLOW_AUTO) {
    const nsAFlatCString& overflow =
      nsCSSProps::ValueToKeyword(display->mOverflowX,
                                 nsCSSProps::kOverflowKTable);
    val->SetIdent(overflow);
  } else {
    val->SetIdent(nsGkAtoms::_auto);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOverflowX(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = GetStyleDisplay();

  if (display->mOverflowX != NS_STYLE_OVERFLOW_AUTO) {
    const nsAFlatCString& overflow =
      nsCSSProps::ValueToKeyword(display->mOverflowX,
                                 nsCSSProps::kOverflowSubKTable);
    val->SetIdent(overflow);
  } else {
    val->SetIdent(nsGkAtoms::_auto);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOverflowY(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = GetStyleDisplay();

  if (display->mOverflowY != NS_STYLE_OVERFLOW_AUTO) {
    const nsAFlatCString& overflow =
      nsCSSProps::ValueToKeyword(display->mOverflowY,
                                 nsCSSProps::kOverflowSubKTable);
    val->SetIdent(overflow);
  } else {
    val->SetIdent(nsGkAtoms::_auto);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetHeight(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  PRBool calcHeight = PR_FALSE;
  
  if (mFrame) {
    calcHeight = PR_TRUE;

    FlushPendingReflows();
  
    const nsStyleDisplay* displayData = GetStyleDisplay();
    if (displayData->mDisplay == NS_STYLE_DISPLAY_INLINE &&
        !(mFrame->IsFrameOfType(nsIFrame::eReplaced))) {
      calcHeight = PR_FALSE;
    }
  }

  if (calcHeight) {
    val->SetTwips(mFrame->GetContentRect().height);
  } else {
    // Just return the value in the style context
    const nsStylePosition* positionData = GetStylePosition();
    switch (positionData->mHeight.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(positionData->mHeight.GetCoordValue());
        break;
      case eStyleUnit_Percent:
        val->SetPercent(positionData->mHeight.GetPercentValue());
        break;
      case eStyleUnit_Auto:
        val->SetIdent(nsGkAtoms::_auto);
        break;
      default:
        NS_ERROR("Unexpected height unit");
        val->SetTwips(0);
        break;
    }
  }
  
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  PRBool calcWidth = PR_FALSE;

  if (mFrame) {
    calcWidth = PR_TRUE;

    FlushPendingReflows();

    const nsStyleDisplay *displayData = GetStyleDisplay();
    if (displayData->mDisplay == NS_STYLE_DISPLAY_INLINE &&
        !(mFrame->IsFrameOfType(nsIFrame::eReplaced))) {
      calcWidth = PR_FALSE;
    }
  }

  if (calcWidth) {
    val->SetTwips(mFrame->GetContentRect().width);
  } else {
    // Just return the value in the style context
    const nsStylePosition *positionData = GetStylePosition();
    switch (positionData->mWidth.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(positionData->mWidth.GetCoordValue());
        break;
      case eStyleUnit_Percent:
        val->SetPercent(positionData->mWidth.GetPercentValue());
        break;
      case eStyleUnit_Auto:
        val->SetIdent(nsGkAtoms::_auto);
        break;
      default:
        NS_ERROR("Unexpected width unit");
        val->SetTwips(0);
        break;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMaxHeight(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition *positionData = GetStylePosition();

  FlushPendingReflows();

  nsIFrame *container = nsnull;
  nsSize size;
  nscoord minHeight = 0;

  if (positionData->mMinHeight.GetUnit() == eStyleUnit_Percent) {
    container = GetContainingBlockFor(mFrame);
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
        container = GetContainingBlockFor(mFrame);
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
    val->SetIdent(nsGkAtoms::none);

    break;
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMaxWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition *positionData = GetStylePosition();

  FlushPendingReflows();

  nsIFrame *container = nsnull;
  nsSize size;
  nscoord minWidth = 0;

  if (positionData->mMinWidth.GetUnit() == eStyleUnit_Percent) {
    container = GetContainingBlockFor(mFrame);
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
        container = GetContainingBlockFor(mFrame);
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
      val->SetIdent(nsGkAtoms::none);

      break;
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMinHeight(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition *positionData = GetStylePosition();

  FlushPendingReflows();

  nsIFrame *container = nsnull;
  switch (positionData->mMinHeight.GetUnit()) {
    case eStyleUnit_Coord:
      val->SetTwips(positionData->mMinHeight.GetCoordValue());
      break;
    case eStyleUnit_Percent:
      container = GetContainingBlockFor(mFrame);
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

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMinWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition *positionData = GetStylePosition();

  FlushPendingReflows();

  nsIFrame *container = nsnull;
  switch (positionData->mMinWidth.GetUnit()) {
    case eStyleUnit_Coord:
      val->SetTwips(positionData->mMinWidth.GetCoordValue());
      break;
    case eStyleUnit_Percent:
      container = GetContainingBlockFor(mFrame);
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

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetLeft(nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::GetRight(nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::GetTop(nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_TOP, aValue);
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
nsComputedDOMStyle::GetOffsetWidthFor(PRUint8 aSide, nsIDOMCSSValue** aValue)
{
  const nsStyleDisplay* display = GetStyleDisplay();

  FlushPendingReflows();

  nsresult rv = NS_OK;
  switch (display->mPosition) {
    case NS_STYLE_POSITION_STATIC:
      rv = GetStaticOffset(aSide, aValue);
      break;
    case NS_STYLE_POSITION_RELATIVE:
      rv = GetRelativeOffset(aSide, aValue);
      break;
    case NS_STYLE_POSITION_ABSOLUTE:
    case NS_STYLE_POSITION_FIXED:
      rv = GetAbsoluteOffset(aSide, aValue);
      break;
    default:
      NS_ERROR("Invalid position");
      break;
  }

  return rv;
}

nsresult
nsComputedDOMStyle::GetAbsoluteOffset(PRUint8 aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nsIFrame* container = GetContainingBlockFor(mFrame);
  if (container) {
    nsMargin margin = mFrame->GetUsedMargin();
    nsMargin border = container->GetUsedBorder();
    nsMargin scrollbarSizes(0, 0, 0, 0);
    nsRect rect = mFrame->GetRect();
    nsRect containerRect = container->GetRect();
      
    if (container->GetType() == nsGkAtoms::viewportFrame) {
      // For absolutely positioned frames scrollbars are taken into
      // account by virtue of getting a containing block that does
      // _not_ include the scrollbars.  For fixed positioned frames,
      // the containing block is the viewport, which _does_ include
      // scrollbars.  We have to do some extra work.
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
        offset = rect.y - margin.top - border.top - scrollbarSizes.top;

        break;
      case NS_SIDE_RIGHT:
        offset = containerRect.width - rect.width -
          rect.x - margin.right - border.right - scrollbarSizes.right;

        break;
      case NS_SIDE_BOTTOM:
        offset = containerRect.height - rect.height -
          rect.y - margin.bottom - border.bottom - scrollbarSizes.bottom;

        break;
      case NS_SIDE_LEFT:
        offset = rect.x - margin.left - border.left - scrollbarSizes.left;

        break;
      default:
        NS_ERROR("Invalid side");
        break;
    }
    val->SetTwips(offset);
  } else {
    // XXX no frame.  This property makes no sense
    val->SetTwips(0);
  }

  return CallQueryInterface(val, aValue);
}


#if (NS_SIDE_TOP == 0) && (NS_SIDE_RIGHT == 1) && (NS_SIDE_BOTTOM == 2) && (NS_SIDE_LEFT == 3)
#define NS_OPPOSITE_SIDE(s_) (((s_) + 2) & 3)
#else
#error "Somebody changed the side constants."
#endif

nsresult
nsComputedDOMStyle::GetRelativeOffset(PRUint8 aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition* positionData = GetStylePosition();
  nsStyleCoord coord;
  PRInt32 sign = 1;
  positionData->mOffset.Get(aSide, coord);
  if (coord.GetUnit() != eStyleUnit_Coord &&
      coord.GetUnit() != eStyleUnit_Percent) {
    positionData->mOffset.Get(NS_OPPOSITE_SIDE(aSide), coord);
    sign = -1;
  }
  nsIFrame* container = nsnull;
  switch(coord.GetUnit()) {
    case eStyleUnit_Coord:
      val->SetTwips(sign * coord.GetCoordValue());
      break;
    case eStyleUnit_Percent:
      container = GetContainingBlockFor(mFrame);
      if (container) {
        nsSize size = container->GetContentRect().Size();
        if (aSide == NS_SIDE_LEFT || aSide == NS_SIDE_RIGHT) {
          val->SetTwips(sign * coord.GetPercentValue() * size.width);
        } else {
          val->SetTwips(sign * coord.GetPercentValue() * size.height);
        }
      } else {
        // XXX no containing block.
        val->SetTwips(0);
      }
      break;
    default:
      NS_ERROR("Unexpected left/right/top/bottom unit");
      val->SetTwips(0);
      break;
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetStaticOffset(PRUint8 aSide, nsIDOMCSSValue** aValue)

{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nsStyleCoord coord;
  GetStylePosition()->mOffset.Get(aSide, coord);
  switch(coord.GetUnit()) {
    case eStyleUnit_Coord:
      val->SetTwips(coord.GetCoordValue());

      break;
    case eStyleUnit_Percent:
      val->SetPercent(coord.GetPercentValue());

      break;
    case eStyleUnit_Auto:
      val->SetIdent(nsGkAtoms::_auto);

      break;
    default:
      NS_ERROR("Unexpected left/right/top/bottom unit");
      val->SetTwips(0);

      break;
  }
  
  return CallQueryInterface(val, aValue);
}

void
nsComputedDOMStyle::FlushPendingReflows()
{
  // Flush all pending notifications so that our frames are up to date
  nsCOMPtr<nsIDocument> document = mContent->GetDocument();
  if (document) {
    document->FlushPendingNotifications(Flush_Layout);
  }
}

const nsStyleStruct*
nsComputedDOMStyle::GetStyleData(nsStyleStructID aSID)
{
  if (mFrame && !mPseudo) {
    nsIAtom* type = mFrame->GetType();
    nsIFrame* frame = mFrame;
    if (type == nsGkAtoms::tableOuterFrame) {
      // If the frame is an outer table frame then we should get the style
      // from the inner table frame.
      frame = frame->GetFirstChild(nsnull);
      NS_ASSERTION(frame, "Outer table must have an inner");
      NS_ASSERTION(!frame->GetNextSibling(),
                   "Outer table frames should have just one child, the inner table");
    }
    return frame->GetStyleData(aSID);
  }
  
  NS_ASSERTION(mStyleContextHolder,
               "GetPropertyCSSValue should have resolved a style context");

  return mStyleContextHolder->GetStyleData(aSID);
}

nsresult
nsComputedDOMStyle::GetPaddingWidthFor(PRUint8 aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  FlushPendingReflows();
  
  val->SetTwips(0);

  if (!mFrame) {
    nsStyleCoord c;
    GetStylePadding()->mPadding.Get(aSide, c);
    switch (c.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(c.GetCoordValue());
        break;
      case eStyleUnit_Percent:
        val->SetPercent(c.GetPercentValue());
        break;
      default:
        break;
    }
  } else {
    val->SetTwips(mFrame->GetUsedPadding().side(aSide));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetLineHeightCoord(const nsStyleText *aText,
                                       nscoord& aCoord)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (aText) {
    const nsStyleFont *font = GetStyleFont();
    switch (aText->mLineHeight.GetUnit()) {
      case eStyleUnit_Coord:
        aCoord = aText->mLineHeight.GetCoordValue();
        rv = NS_OK;
        break;
      case eStyleUnit_Percent:
        aCoord = nscoord(aText->mLineHeight.GetPercentValue() * font->mSize);
        rv = NS_OK;
        break;
      case eStyleUnit_Factor:
        aCoord = nscoord(aText->mLineHeight.GetFactorValue() * font->mSize);
        rv = NS_OK;
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
nsComputedDOMStyle::GetBorderColorsFor(PRUint8 aSide, nsIDOMCSSValue** aValue)
{
  const nsStyleBorder *border = GetStyleBorder();

  if (border->mBorderColors) {
    nsBorderColors* borderColors = border->mBorderColors[aSide];
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
          primitive->SetIdent(nsGkAtoms::transparent);
        } else {
          nsresult rv = SetToRGBAColor(primitive, borderColors->mColor);
          if (NS_FAILED(rv)) {
            delete valueList;
            delete primitive;
            return rv;
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

  val->SetIdent(nsGkAtoms::none);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRadiusFor(PRUint8 aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nsStyleCoord coord;
  GetStyleBorder()->mBorderRadius.Get(aSide, coord);
    
  switch (coord.GetUnit()) {
    case eStyleUnit_Coord:
      val->SetTwips(coord.GetCoordValue());
      break;
    case eStyleUnit_Percent:
      if (mFrame) {
        val->SetTwips(coord.GetPercentValue() * mFrame->GetSize().width);
      } else {
        val->SetPercent(coord.GetPercentValue());
      }
      break;
    default:
      NS_ERROR("Unexpected border radius unit");
      break;
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderWidthFor(PRUint8 aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nscoord width;
  const nsStyleDisplay *disp = GetStyleDisplay();
  if (mFrame->IsThemed(disp)) {
    nsMargin result;
    nsPresContext *presContext = mFrame->GetPresContext();
    presContext->GetTheme()->GetWidgetBorder(presContext->DeviceContext(),
                                             mFrame, disp->mAppearance,
                                             &result);
    width = NSIntPixelsToTwips(result.side(aSide),
                               presContext->ScaledPixelsToTwips());
  } else {
    width = GetStyleBorder()->GetComputedBorderWidth(aSide);
  }
  val->SetTwips(width);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderColorFor(PRUint8 aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nscolor color; 
  PRBool transparent;
  PRBool foreground;
  GetStyleBorder()->GetBorderColor(aSide, color, transparent, foreground);
  if (transparent) {
    val->SetIdent(nsGkAtoms::transparent);
  } else {
    if (foreground) {
      const nsStyleColor* colorStruct = GetStyleColor();
      color = colorStruct->mColor;
    }
    // XXX else?

    nsresult rv = SetToRGBAColor(val, color);
    if (NS_FAILED(rv)) {
      delete val;
      return rv;
    }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginWidthFor(PRUint8 aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  FlushPendingReflows();

  val->SetTwips(0);

  if (!mFrame) {
    nsStyleCoord c;
    GetStyleMargin()->mMargin.Get(aSide, c);
    switch (c.GetUnit()) {
      case eStyleUnit_Coord:
        val->SetTwips(c.GetCoordValue());
        break;
      case eStyleUnit_Percent:
        val->SetPercent(c.GetPercentValue());
        break;
      case eStyleUnit_Auto:
        val->SetIdent(nsGkAtoms::_auto);
        break;
      default:
        break;
    }
  } else {
    val->SetTwips(mFrame->GetUsedMargin().side(aSide));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderStyleFor(PRUint8 aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  PRUint8 borderStyle = GetStyleBorder()->GetBorderStyle(aSide);

  if (borderStyle != NS_STYLE_BORDER_STYLE_NONE) {
    const nsAFlatCString& style=
      nsCSSProps::ValueToKeyword(borderStyle,
                                 nsCSSProps::kBorderStyleKTable);
    val->SetIdent(style);
  } else {
    val->SetIdent(nsGkAtoms::none);
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
    COMPUTED_STYLE_MAP_ENTRY(border_spacing,                BorderSpacing),
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
    COMPUTED_STYLE_MAP_ENTRY(counter_increment,             CounterIncrement),
    COMPUTED_STYLE_MAP_ENTRY(counter_reset,                 CounterReset),
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
    COMPUTED_STYLE_MAP_ENTRY(opacity,                       Opacity),
    // COMPUTED_STYLE_MAP_ENTRY(orphans,                    Orphans),
    //// COMPUTED_STYLE_MAP_ENTRY(outline,                  Outline),
    COMPUTED_STYLE_MAP_ENTRY(outline_color,                 OutlineColor),
    COMPUTED_STYLE_MAP_ENTRY(outline_style,                 OutlineStyle),
    COMPUTED_STYLE_MAP_ENTRY(outline_width,                 OutlineWidth),
    COMPUTED_STYLE_MAP_ENTRY(outline_offset,                OutlineOffset),
    COMPUTED_STYLE_MAP_ENTRY(overflow,                      Overflow),
    COMPUTED_STYLE_MAP_ENTRY(overflow_x,                    OverflowX),
    COMPUTED_STYLE_MAP_ENTRY(overflow_y,                    OverflowY),
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
    COMPUTED_STYLE_MAP_ENTRY(_moz_outline_radius_bottomLeft, OutlineRadiusBottomLeft),
    COMPUTED_STYLE_MAP_ENTRY(_moz_outline_radius_bottomRight,OutlineRadiusBottomRight),
    COMPUTED_STYLE_MAP_ENTRY(_moz_outline_radius_topLeft,    OutlineRadiusTopLeft),
    COMPUTED_STYLE_MAP_ENTRY(_moz_outline_radius_topRight,   OutlineRadiusTopRight),
    COMPUTED_STYLE_MAP_ENTRY(user_focus,                    UserFocus),
    COMPUTED_STYLE_MAP_ENTRY(user_input,                    UserInput),
    COMPUTED_STYLE_MAP_ENTRY(user_modify,                   UserModify),
    COMPUTED_STYLE_MAP_ENTRY(user_select,                   UserSelect)
  };

  *aLength = NS_ARRAY_LENGTH(map);

  return map;
}

