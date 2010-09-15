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
 *   Michael Ventnor <m.ventnor@gmail.com>
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>, Collabora Ltd.
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
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
#include "nsPrintfCString.h"
#include "nsIDOMCSS2Properties.h"
#include "nsIDOMElement.h"
#include "nsIDOMCSSPrimitiveValue.h"
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
#include "nsStyleUtil.h"
#include "nsStyleStructInlines.h"

#include "nsPresContext.h"
#include "nsIDocument.h"

#include "nsCSSPseudoElements.h"
#include "nsStyleSet.h"
#include "imgIRequest.h"
#include "nsLayoutUtils.h"
#include "nsFrameManager.h"
#include "prlog.h"
#include "nsCSSKeywords.h"
#include "nsStyleCoord.h"
#include "nsDisplayList.h"
#include "nsDOMCSSDeclaration.h"
#include "mozilla/dom/Element.h"
#include "CSSCalc.h"

using namespace mozilla::dom;
namespace css = mozilla::css;

#if defined(DEBUG_bzbarsky) || defined(DEBUG_caillon)
#define DEBUG_ComputedDOMStyle
#endif

/*
 * This is the implementation of the readonly CSSStyleDeclaration that is
 * returned by the getComputedStyle() function.
 */

static nsComputedDOMStyle *sCachedComputedDOMStyle;

nsresult
NS_NewComputedDOMStyle(nsIDOMElement *aElement, const nsAString &aPseudoElt,
                       nsIPresShell *aPresShell,
                       nsComputedDOMStyle **aComputedStyle)
{
  nsRefPtr<nsComputedDOMStyle> computedStyle;
  if (sCachedComputedDOMStyle) {
    // There's an unused nsComputedDOMStyle cached, use it.
    // But before we use it, re-initialize the object.

    // Oh yeah baby, placement new!
    computedStyle = new (sCachedComputedDOMStyle) nsComputedDOMStyle();

    sCachedComputedDOMStyle = nsnull;
  } else {
    // No nsComputedDOMStyle cached, create a new one.

    computedStyle = new nsComputedDOMStyle();
    NS_ENSURE_TRUE(computedStyle, NS_ERROR_OUT_OF_MEMORY);
  }

  nsresult rv = computedStyle->Init(aElement, aPseudoElt, aPresShell);
  NS_ENSURE_SUCCESS(rv, rv);

  *aComputedStyle = nsnull;
  computedStyle.swap(*aComputedStyle);

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
  : mDocumentWeak(nsnull), mOuterFrame(nsnull),
    mInnerFrame(nsnull), mPresShell(nsnull),
    mExposeVisitedStyle(PR_FALSE)
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
  delete reinterpret_cast<char*>(sCachedComputedDOMStyle);
  sCachedComputedDOMStyle = nsnull;
}


NS_IMPL_CYCLE_COLLECTION_1(nsComputedDOMStyle, mContent)

DOMCI_DATA(ComputedCSSStyleDeclaration, nsComputedDOMStyle)

// QueryInterface implementation for nsComputedDOMStyle
NS_INTERFACE_TABLE_HEAD(nsComputedDOMStyle)
  NS_INTERFACE_TABLE3(nsComputedDOMStyle,
                      nsICSSDeclaration,
                      nsIDOMCSSStyleDeclaration,
                      nsIDOMCSS2Properties)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsComputedDOMStyle)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ComputedCSSStyleDeclaration)
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

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsComputedDOMStyle)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_DESTROY(nsComputedDOMStyle,
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

/* static */
already_AddRefed<nsStyleContext>
nsComputedDOMStyle::GetStyleContextForElement(Element* aElement,
                                              nsIAtom* aPseudo,
                                              nsIPresShell* aPresShell)
{
  // If there's no pres shell, get it from the content
  if (!aPresShell) {
    aPresShell = GetPresShellForContent(aElement);
    if (!aPresShell)
      return nsnull;
  }

  aPresShell->FlushPendingNotifications(Flush_Style);

  return GetStyleContextForElementNoFlush(aElement, aPseudo, aPresShell);
}

/* static */
already_AddRefed<nsStyleContext>
nsComputedDOMStyle::GetStyleContextForElementNoFlush(Element* aElement,
                                                     nsIAtom* aPseudo,
                                                     nsIPresShell* aPresShell)
{
  NS_ABORT_IF_FALSE(aElement, "NULL element");

  // If there's no pres shell, get it from the content
  if (!aPresShell) {
    aPresShell = GetPresShellForContent(aElement);
    if (!aPresShell)
      return nsnull;
  }

  if (!aPseudo) {
    nsIFrame* frame = aElement->GetPrimaryFrame();
    if (frame) {
      nsStyleContext* result =
        nsLayoutUtils::GetStyleFrame(frame)->GetStyleContext();
      // Don't use the style context if it was influenced by
      // pseudo-elements, since then it's not the primary style
      // for this element.
      if (!result->HasPseudoElementData()) {
        // this function returns an addrefed style context
        result->AddRef();
        return result;
      }
    }
  }

  // No frame has been created or we have a pseudo, so resolve the
  // style ourselves
  nsRefPtr<nsStyleContext> parentContext;
  nsIContent* parent = aPseudo ? aElement : aElement->GetParent();
  // Don't resolve parent context for document fragments.
  if (parent && parent->IsElement())
    parentContext = GetStyleContextForElementNoFlush(parent->AsElement(),
                                                     nsnull, aPresShell);

  nsPresContext *presContext = aPresShell->GetPresContext();
  if (!presContext)
    return nsnull;

  nsStyleSet *styleSet = aPresShell->StyleSet();

  if (aPseudo) {
    nsCSSPseudoElements::Type type = nsCSSPseudoElements::GetPseudoType(aPseudo);
    if (type >= nsCSSPseudoElements::ePseudo_PseudoElementCount) {
      return nsnull;
    }
    return styleSet->ResolvePseudoElementStyle(aElement, type, parentContext);
  }

  return styleSet->ResolveStyleFor(aElement, parentContext);
}

/* static */
nsIPresShell*
nsComputedDOMStyle::GetPresShellForContent(nsIContent* aContent)
{
  nsIDocument* currentDoc = aContent->GetCurrentDoc();
  if (!currentDoc)
    return nsnull;

  return currentDoc->GetShell();
}

// nsDOMCSSDeclaration abstract methods which should never be called
// on a nsComputedDOMStyle object, but must be defined to avoid
// compile errors.
css::Declaration*
nsComputedDOMStyle::GetCSSDeclaration(PRBool)
{
  NS_RUNTIMEABORT("called nsComputedDOMStyle::GetCSSDeclaration");
  return nsnull;
}

nsresult
nsComputedDOMStyle::SetCSSDeclaration(css::Declaration*)
{
  NS_RUNTIMEABORT("called nsComputedDOMStyle::SetCSSDeclaration");
  return NS_ERROR_FAILURE;
}

nsIDocument*
nsComputedDOMStyle::DocToUpdate()
{
  NS_RUNTIMEABORT("called nsComputedDOMStyle::DocToUpdate");
  return nsnull;
}

nsresult
nsComputedDOMStyle::GetCSSParsingEnvironment(nsIURI**, nsIURI**, nsIPrincipal**,
                                             css::Loader**)
{
  NS_RUNTIMEABORT("called nsComputedDOMStyle::GetCSSParsingEnvironment");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyCSSValue(const nsAString& aPropertyName,
                                        nsIDOMCSSValue** aReturn)
{
  NS_ASSERTION(!mStyleContextHolder, "bad state");

  *aReturn = nsnull;

  nsCOMPtr<nsIDocument> document = do_QueryReferent(mDocumentWeak);
  NS_ENSURE_TRUE(document, NS_ERROR_NOT_AVAILABLE);

  nsCSSProperty prop = nsCSSProps::LookupProperty(aPropertyName);

  const ComputedStyleMapEntry* propEntry = nsnull;
  {
    PRUint32 length = 0;
    const ComputedStyleMapEntry* propMap = GetQueryablePropertyMap(&length);
    for (PRUint32 i = 0; i < length; ++i) {
      if (prop == propMap[i].mProperty) {
        propEntry = &propMap[i];
        break;
      }
    }
  }
  if (!propEntry) {
#ifdef DEBUG_ComputedDOMStyle
    NS_WARNING(PromiseFlatCString(NS_ConvertUTF16toUTF8(aPropertyName) +
                                  NS_LITERAL_CSTRING(" is not queryable!")).get());
#endif

    // NOTE:  For branches, we should flush here for compatibility!
    return NS_OK;
  }

  // Flush _before_ getting the presshell, since that could create a new
  // presshell.  Also note that we want to flush the style on the document
  // we're computing style in, not on the document mContent is in -- the two
  // may be different.
  document->FlushPendingNotifications(
    propEntry->mNeedsLayoutFlush ? Flush_Layout : Flush_Style);
#ifdef DEBUG
  mFlushedPendingReflows = propEntry->mNeedsLayoutFlush;
#endif

  mPresShell = document->GetShell();
  NS_ENSURE_TRUE(mPresShell && mPresShell->GetPresContext(),
                 NS_ERROR_NOT_AVAILABLE);

  mOuterFrame = mContent->GetPrimaryFrame();
  mInnerFrame = mOuterFrame;
  if (mOuterFrame && !mPseudo) {
    nsIAtom* type = mOuterFrame->GetType();
    if (type == nsGkAtoms::tableOuterFrame) {
      // If the frame is an outer table frame then we should get the style
      // from the inner table frame.
      mInnerFrame = mOuterFrame->GetFirstChild(nsnull);
      NS_ASSERTION(mInnerFrame, "Outer table must have an inner");
      NS_ASSERTION(!mInnerFrame->GetNextSibling(),
                   "Outer table frames should have just one child, the inner "
                   "table");
    }

    mStyleContextHolder = mInnerFrame->GetStyleContext();
    NS_ASSERTION(mStyleContextHolder, "Frame without style context?");
  }

  if (!mStyleContextHolder || mStyleContextHolder->HasPseudoElementData()) {
#ifdef DEBUG
    if (mStyleContextHolder) {
      // We want to check that going through this path because of
      // HasPseudoElementData is rare, because it slows us down a good
      // bit.  So check that we're really inside something associated
      // with a pseudo-element that contains elements.
      nsStyleContext *topWithPseudoElementData = mStyleContextHolder;
      while (topWithPseudoElementData->GetParent()->HasPseudoElementData()) {
        topWithPseudoElementData = topWithPseudoElementData->GetParent();
      }
      NS_ASSERTION(nsCSSPseudoElements::PseudoElementContainsElements(
                     topWithPseudoElementData->GetPseudo()),
                   "we should be in a pseudo-element that is expected to "
                   "contain elements");
    }
#endif
    // Need to resolve a style context
    mStyleContextHolder =
      nsComputedDOMStyle::GetStyleContextForElement(mContent->AsElement(),
                                                    mPseudo,
                                                    mPresShell);
    NS_ENSURE_TRUE(mStyleContextHolder, NS_ERROR_OUT_OF_MEMORY);
    NS_ASSERTION(mPseudo || !mStyleContextHolder->HasPseudoElementData(),
                 "should not have pseudo-element data");
  }

  // mExposeVisitedStyle is set to true only by testing APIs that
  // require UniversalXPConnect.
  NS_ABORT_IF_FALSE(!mExposeVisitedStyle ||
                    nsContentUtils::IsCallerTrustedForCapability(
                                      "UniversalXPConnect"),
                    "mExposeVisitedStyle set incorrectly");
  if (mExposeVisitedStyle && mStyleContextHolder->RelevantLinkVisited()) {
    nsStyleContext *styleIfVisited = mStyleContextHolder->GetStyleIfVisited();
    if (styleIfVisited) {
      mStyleContextHolder = styleIfVisited;
    }
  }

  // Call our pointer-to-member-function.
  nsresult rv = (this->*(propEntry->mGetter))(aReturn);

  if (NS_FAILED(rv)) {
    *aReturn = nsnull;
  }

  mOuterFrame = nsnull;
  mInnerFrame = nsnull;
  mPresShell = nsnull;

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
nsComputedDOMStyle::DoGetBinding(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = GetStyleDisplay();

  if (display->mBinding) {
    val->SetURI(display->mBinding->mURI);
  } else {
    val->SetIdent(eCSSKeyword_none);
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetClear(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleDisplay()->mBreakType,
                                               nsCSSProps::kClearKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetCssFloat(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleDisplay()->mFloats,
                                               nsCSSProps::kFloatKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBottom(nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_BOTTOM, aValue);
}

nsresult
nsComputedDOMStyle::DoGetStackSizing(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(GetStyleXUL()->mStretchStack ? eCSSKeyword_stretch_to_fit :
                eCSSKeyword_ignore);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::SetToRGBAColor(nsROCSSPrimitiveValue* aValue,
                                   nscolor aColor)
{
  if (NS_GET_A(aColor) == 0) {
    aValue->SetIdent(eCSSKeyword_transparent);
    return NS_OK;
  }

  nsROCSSPrimitiveValue *red   = GetROCSSPrimitiveValue();
  nsROCSSPrimitiveValue *green = GetROCSSPrimitiveValue();
  nsROCSSPrimitiveValue *blue  = GetROCSSPrimitiveValue();
  nsROCSSPrimitiveValue *alpha  = GetROCSSPrimitiveValue();

  if (red && green && blue && alpha) {
    PRUint8 a = NS_GET_A(aColor);
    nsDOMCSSRGBColor *rgbColor =
      new nsDOMCSSRGBColor(red, green, blue, alpha, a < 255);

    if (rgbColor) {
      red->SetNumber(NS_GET_R(aColor));
      green->SetNumber(NS_GET_G(aColor));
      blue->SetNumber(NS_GET_B(aColor));
      alpha->SetNumber(nsStyleUtil::ColorComponentToFloat(a));

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
nsComputedDOMStyle::DoGetColor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleColor* color = GetStyleColor();

  nsresult rv = SetToRGBAColor(val, color->mColor);
  if (NS_FAILED(rv)) {
    delete val;
    return rv;
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetOpacity(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleDisplay()->mOpacity);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetColumnCount(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleColumn* column = GetStyleColumn();

  if (column->mColumnCount == NS_STYLE_COLUMN_COUNT_AUTO) {
    val->SetIdent(eCSSKeyword_auto);
  } else {
    val->SetNumber(column->mColumnCount);
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetColumnWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  // XXX fix the auto case. When we actually have a column frame, I think
  // we should return the computed column width.
  SetValueToCoord(val, GetStyleColumn()->mColumnWidth, PR_TRUE);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetColumnGap(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleColumn* column = GetStyleColumn();
  if (column->mColumnGap.GetUnit() == eStyleUnit_Normal) {
    val->SetAppUnits(GetStyleFont()->mFont.size);
  } else {
    SetValueToCoord(val, GetStyleColumn()->mColumnGap, PR_TRUE);
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetColumnRuleWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  if (!val)
    return NS_ERROR_OUT_OF_MEMORY;

  val->SetAppUnits(GetStyleColumn()->GetComputedColumnRuleWidth());
  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetColumnRuleStyle(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  if (!val)
    return NS_ERROR_OUT_OF_MEMORY;

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleColumn()->mColumnRuleStyle,
                                   nsCSSProps::kBorderStyleKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetColumnRuleColor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  if (!val)
    return NS_ERROR_OUT_OF_MEMORY;

  const nsStyleColumn* column = GetStyleColumn();
  nscolor ruleColor;
  if (column->mColumnRuleColorIsForeground) {
    ruleColor = GetStyleColor()->mColor;
  } else {
    ruleColor = column->mColumnRuleColor;
  }

  SetToRGBAColor(val, ruleColor);
  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetContent(nsIDOMCSSValue** aValue)
{
  const nsStyleContent *content = GetStyleContent();

  if (content->ContentCount() == 0) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
    val->SetIdent(eCSSKeyword_none);
    NS_ADDREF(*aValue = val);
    return NS_OK;
  }

  if (content->ContentCount() == 1 &&
      content->ContentAt(0).mType == eStyleContentType_AltContent) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
    val->SetIdent(eCSSKeyword__moz_alt_content);
    NS_ADDREF(*aValue = val);
    return NS_OK;
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_FALSE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 i = 0, i_end = content->ContentCount(); i < i_end; ++i) {
    nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
    if (!val || !valueList->AppendCSSValue(val)) {
      delete valueList;
      delete val;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    const nsStyleContentData &data = content->ContentAt(i);
    switch (data.mType) {
      case eStyleContentType_String:
        {
          nsString str;
          nsStyleUtil::AppendEscapedCSSString(
            nsDependentString(data.mContent.mString), str);
          val->SetString(str);
        }
        break;
      case eStyleContentType_Image:
        {
          nsCOMPtr<nsIURI> uri;
          if (data.mContent.mImage) {
            data.mContent.mImage->GetURI(getter_AddRefs(uri));
          }
          val->SetURI(uri);
        }
        break;
      case eStyleContentType_Attr:
        {
          nsAutoString str;
          nsStyleUtil::AppendEscapedCSSIdent(
            nsDependentString(data.mContent.mString), str);
          val->SetString(str, nsIDOMCSSPrimitiveValue::CSS_ATTR);
        }
        break;
      case eStyleContentType_Counter:
      case eStyleContentType_Counters:
        {
          /* FIXME: counters should really use an object */
          nsAutoString str;
          if (data.mType == eStyleContentType_Counter) {
            str.AppendLiteral("counter(");
          }
          else {
            str.AppendLiteral("counters(");
          }
          // WRITE ME
          nsCSSValue::Array *a = data.mContent.mCounters;

          nsStyleUtil::AppendEscapedCSSIdent(
            nsDependentString(a->Item(0).GetStringBufferValue()), str);
          PRInt32 typeItem = 1;
          if (data.mType == eStyleContentType_Counters) {
            typeItem = 2;
            str.AppendLiteral(", ");
            nsStyleUtil::AppendEscapedCSSString(
              nsDependentString(a->Item(1).GetStringBufferValue()), str);
          }
          NS_ABORT_IF_FALSE(eCSSUnit_None != a->Item(typeItem).GetUnit(),
                            "'none' should be handled  as enumerated value");
          PRInt32 type = a->Item(typeItem).GetIntValue();
          if (type != NS_STYLE_LIST_STYLE_DECIMAL) {
            str.AppendLiteral(", ");
            AppendASCIItoUTF16(
              nsCSSProps::ValueToKeyword(type, nsCSSProps::kListStyleKTable),
              str);
          }

          str.Append(PRUnichar(')'));
          val->SetString(str, nsIDOMCSSPrimitiveValue::CSS_COUNTER);
        }
        break;
      case eStyleContentType_OpenQuote:
        val->SetIdent(eCSSKeyword_open_quote);
        break;
      case eStyleContentType_CloseQuote:
        val->SetIdent(eCSSKeyword_close_quote);
        break;
      case eStyleContentType_NoOpenQuote:
        val->SetIdent(eCSSKeyword_no_open_quote);
        break;
      case eStyleContentType_NoCloseQuote:
        val->SetIdent(eCSSKeyword_no_close_quote);
        break;
      case eStyleContentType_AltContent:
      default:
        NS_NOTREACHED("unexpected type");
        break;
    }
  }

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetCounterIncrement(nsIDOMCSSValue** aValue)
{
  const nsStyleContent *content = GetStyleContent();

  if (content->CounterIncrementCount() == 0) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
    val->SetIdent(eCSSKeyword_none);
    NS_ADDREF(*aValue = val);
    return NS_OK;
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_FALSE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 i = 0, i_end = content->CounterIncrementCount(); i < i_end; ++i) {
    nsROCSSPrimitiveValue* name = GetROCSSPrimitiveValue();
    if (!name || !valueList->AppendCSSValue(name)) {
      delete valueList;
      delete name;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsROCSSPrimitiveValue* value = GetROCSSPrimitiveValue();
    if (!value || !valueList->AppendCSSValue(value)) {
      delete valueList;
      delete value;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    const nsStyleCounterData *data = content->GetCounterIncrementAt(i);
    nsAutoString escaped;
    nsStyleUtil::AppendEscapedCSSIdent(data->mCounter, escaped);
    name->SetString(escaped);
    value->SetNumber(data->mValue); // XXX This should really be integer
  }

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

/* Convert the stored representation into a list of two values and then hand
 * it back.
 */
nsresult
nsComputedDOMStyle::DoGetMozTransformOrigin(nsIDOMCSSValue **aValue)
{
  /* We need to build up a list of two values.  We'll call them
   * width and height.
   */
  nsRefPtr<nsROCSSPrimitiveValue> width = GetROCSSPrimitiveValue();
  nsRefPtr<nsROCSSPrimitiveValue> height = GetROCSSPrimitiveValue();
  if (!width || !height)
    return NS_ERROR_OUT_OF_MEMORY;

  /* Now, get the values. */
  const nsStyleDisplay* display = GetStyleDisplay();
  SetValueToCoord(width, display->mTransformOrigin[0], PR_FALSE,
                  &nsComputedDOMStyle::GetFrameBoundsWidthForTransform);
  SetValueToCoord(height, display->mTransformOrigin[1], PR_FALSE,
                  &nsComputedDOMStyle::GetFrameBoundsHeightForTransform);

  /* Store things as a value list, fail if we can't get one. */
  nsRefPtr<nsDOMCSSValueList> valueList = GetROCSSValueList(PR_FALSE);
  if (!valueList)
    return NS_ERROR_OUT_OF_MEMORY;

  /* Chain on width and height, fail if we can't. */
  if (!valueList->AppendCSSValue(width))
    return NS_ERROR_OUT_OF_MEMORY;
  if (!valueList->AppendCSSValue(height))
    return NS_ERROR_OUT_OF_MEMORY;

  valueList.forget(aValue);
  return NS_OK;
}

/* If the property is "none", hand back "none" wrapped in a value.
 * Otherwise, compute the aggregate transform matrix and hands it back in a
 * "matrix" wrapper.
 */
nsresult
nsComputedDOMStyle::DoGetMozTransform(nsIDOMCSSValue **aValue)
{
  static const PRInt32 NUM_FLOATS = 4;

  /* First, get the display data.  We'll need it. */
  const nsStyleDisplay* display = GetStyleDisplay();

  /* If the "no transforms" flag is set, then we should construct a
   * single-element entry and hand it back.
   */
  if (!display->HasTransform()) {
    nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
    if (!val)
      return NS_ERROR_OUT_OF_MEMORY;

    /* Set it to "none." */
    val->SetIdent(eCSSKeyword_none);
    NS_ADDREF(*aValue = val);
    return NS_OK;
  }

  /* Otherwise, we need to compute the current value of the transform matrix,
   * store it in a string, and hand it back to the caller.
   */
  nsAutoString resultString(NS_LITERAL_STRING("matrix("));

  /* Now, we need to convert the matrix into a string.  We'll start by taking
   * the first four entries and converting them directly to floating-point
   * values.
   */
  for (PRInt32 index = 0; index < NUM_FLOATS; ++index) {
    resultString.AppendFloat(display->mTransform.GetMainMatrixEntry(index));
    resultString.Append(NS_LITERAL_STRING(", "));
  }

  /* Use the inner frame for width and height.  If we fail, assume zero.
   * TODO: There is no good way for us to represent the case where there's no
   * frame, which is problematic.  The reason is that when we have percentage
   * transforms, there are a total of four stored matrix entries that influence
   * the transform based on the size of the element.  However, this poses a
   * problem, because only two of these values can be explicitly referenced
   * using the named transforms.  Until a real solution is found, we'll just
   * use this approach.
   */
  nsRect bounds =
    (mInnerFrame ? nsDisplayTransform::GetFrameBoundsForTransform(mInnerFrame) :
     nsRect(0, 0, 0, 0));

  /* Now, compute the dX and dY components by adding the stored coord value
   * (in CSS pixels) to the translate values.
   */

  float deltaX = nsPresContext::AppUnitsToFloatCSSPixels
    (display->mTransform.GetXTranslation(bounds));
  float deltaY = nsPresContext::AppUnitsToFloatCSSPixels
    (display->mTransform.GetYTranslation(bounds));


  /* Append these values! */
  resultString.AppendFloat(deltaX);
  resultString.Append(NS_LITERAL_STRING("px, "));
  resultString.AppendFloat(deltaY);
  resultString.Append(NS_LITERAL_STRING("px)"));

  /* Create a value to hold our result. */
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();

  if (!val)
    return NS_ERROR_OUT_OF_MEMORY;

  val->SetString(resultString);
  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetCounterReset(nsIDOMCSSValue** aValue)
{
  const nsStyleContent *content = GetStyleContent();

  if (content->CounterResetCount() == 0) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
    val->SetIdent(eCSSKeyword_none);
    NS_ADDREF(*aValue = val);
    return NS_OK;
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_FALSE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 i = 0, i_end = content->CounterResetCount(); i < i_end; ++i) {
    nsROCSSPrimitiveValue* name = GetROCSSPrimitiveValue();
    if (!name || !valueList->AppendCSSValue(name)) {
      delete valueList;
      delete name;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsROCSSPrimitiveValue* value = GetROCSSPrimitiveValue();
    if (!value || !valueList->AppendCSSValue(value)) {
      delete valueList;
      delete value;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    const nsStyleCounterData *data = content->GetCounterResetAt(i);
    nsAutoString escaped;
    nsStyleUtil::AppendEscapedCSSIdent(data->mCounter, escaped);
    name->SetString(escaped);
    value->SetNumber(data->mValue); // XXX This should really be integer
  }

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetQuotes(nsIDOMCSSValue** aValue)
{
  const nsStyleQuotes *quotes = GetStyleQuotes();

  if (quotes->QuotesCount() == 0) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
    val->SetIdent(eCSSKeyword_none);
    NS_ADDREF(*aValue = val);
    return NS_OK;
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_FALSE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 i = 0, i_end = quotes->QuotesCount(); i < i_end; ++i) {
    nsROCSSPrimitiveValue* openVal = GetROCSSPrimitiveValue();
    if (!openVal || !valueList->AppendCSSValue(openVal)) {
      delete valueList;
      delete openVal;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsROCSSPrimitiveValue* closeVal = GetROCSSPrimitiveValue();
    if (!closeVal || !valueList->AppendCSSValue(closeVal)) {
      delete valueList;
      delete closeVal;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsString s;
    nsStyleUtil::AppendEscapedCSSString(*quotes->OpenQuoteAt(i), s);
    openVal->SetString(s);
    s.Truncate();
    nsStyleUtil::AppendEscapedCSSString(*quotes->CloseQuoteAt(i), s);
    closeVal->SetString(s);
  }

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetFontFamily(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = GetStyleFont();

  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocumentWeak);
  NS_ASSERTION(doc, "document is required");
  nsIPresShell* presShell = doc->GetShell();
  NS_ASSERTION(presShell, "pres shell is required");
  nsPresContext *presContext = presShell->GetPresContext();
  NS_ASSERTION(presContext, "pres context is required");

  const nsString& fontName = font->mFont.name;
  if (font->mGenericID == kGenericFont_NONE && !font->mFont.systemFont) {
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

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetFontSize(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  // Note: GetStyleFont()->mSize is the 'computed size';
  // GetStyleFont()->mFont.size is the 'actual size'
  val->SetAppUnits(GetStyleFont()->mSize);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetFontSizeAdjust(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont *font = GetStyleFont();

  if (font->mFont.sizeAdjust) {
    val->SetNumber(font->mFont.sizeAdjust);
  } else {
    val->SetIdent(eCSSKeyword_none);
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetFontStretch(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = GetStyleFont();

  // The computed value space isn't actually representable in string
  // form, so just represent anything with widers or narrowers in it as
  // 'wider' or 'narrower'.
  PR_STATIC_ASSERT(NS_FONT_STRETCH_NARROWER % 2 == 0);
  PR_STATIC_ASSERT(NS_FONT_STRETCH_WIDER % 2 == 0);
  PR_STATIC_ASSERT(NS_FONT_STRETCH_NARROWER + NS_FONT_STRETCH_WIDER == 0);
  PR_STATIC_ASSERT(NS_FONT_STRETCH_NARROWER < 0);
  PRInt16 stretch = font->mFont.stretch;
  if (stretch <= NS_FONT_STRETCH_NARROWER / 2) {
    val->SetIdent(eCSSKeyword_narrower);
  } else if (stretch >= NS_FONT_STRETCH_WIDER / 2) {
    val->SetIdent(eCSSKeyword_wider);
  } else {
    val->SetIdent(
      nsCSSProps::ValueToKeywordEnum(stretch, nsCSSProps::kFontStretchKTable));
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetFontStyle(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleFont()->mFont.style,
                                               nsCSSProps::kFontStyleKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetFontWeight(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = GetStyleFont();

  PRUint16 weight = font->mFont.weight;
  if (weight % 100 == 0) {
    val->SetNumber(font->mFont.weight);
  } else if (weight % 100 > 50) {
    // FIXME: This doesn't represent the full range of computed values,
    // but at least it's legal CSS.
    val->SetIdent(eCSSKeyword_lighter);
  } else {
    // FIXME: This doesn't represent the full range of computed values,
    // but at least it's legal CSS.
    val->SetIdent(eCSSKeyword_bolder);
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetFontVariant(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleFont()->mFont.variant,
                                   nsCSSProps::kFontVariantKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetMozFontFeatureSettings(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = GetStyleFont();
  if (font->mFont.featureSettings.IsEmpty()) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    nsString str;
    nsStyleUtil::AppendEscapedCSSString(font->mFont.featureSettings, str);
    val->SetString(str);
  }
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::DoGetMozFontLanguageOverride(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleFont* font = GetStyleFont();
  if (font->mFont.languageOverride.IsEmpty()) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    nsString str;
    nsStyleUtil::AppendEscapedCSSString(font->mFont.languageOverride, str);
    val->SetString(str);
  }
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundList(PRUint8 nsStyleBackground::Layer::* aMember,
                                      PRUint32 nsStyleBackground::* aCount,
                                      const PRInt32 aTable[],
                                      nsIDOMCSSValue** aResult)
{
  const nsStyleBackground* bg = GetStyleBackground();

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_TRUE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 i = 0, i_end = bg->*aCount; i < i_end; ++i) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    if (!val || !valueList->AppendCSSValue(val)) {
      delete val;
      delete valueList;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    val->SetIdent(nsCSSProps::ValueToKeywordEnum(bg->mLayers[i].*aMember,
                                                 aTable));
  }

  NS_ADDREF(*aResult = valueList);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBackgroundAttachment(nsIDOMCSSValue** aValue)
{
  return GetBackgroundList(&nsStyleBackground::Layer::mAttachment,
                           &nsStyleBackground::mAttachmentCount,
                           nsCSSProps::kBackgroundAttachmentKTable,
                           aValue);
}

nsresult
nsComputedDOMStyle::DoGetBackgroundClip(nsIDOMCSSValue** aValue)
{
  return GetBackgroundList(&nsStyleBackground::Layer::mClip,
                           &nsStyleBackground::mClipCount,
                           nsCSSProps::kBackgroundOriginKTable,
                           aValue);
}

nsresult
nsComputedDOMStyle::DoGetBackgroundColor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground* color = GetStyleBackground();
  nsresult rv = SetToRGBAColor(val, color->mBackgroundColor);
  if (NS_FAILED(rv)) {
    delete val;
    return rv;
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}


static void
SetValueToCalc(nsStyleCoord::Calc *aCalc, nsROCSSPrimitiveValue *aValue)
{
  nsRefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue();
  nsAutoString tmp, result;

  result.AppendLiteral("-moz-calc(");

  val->SetAppUnits(aCalc->mLength);
  val->GetCssText(tmp);
  result.Append(tmp);

  if (aCalc->mHasPercent) {
    result.AppendLiteral(" + ");

    val->SetPercent(aCalc->mPercent);
    val->GetCssText(tmp);
    result.Append(tmp);
  }

  result.AppendLiteral(")");

  aValue->SetString(result); // not really SetString
}

static void
AppendCSSGradientLength(const nsStyleCoord& aValue,
                        nsROCSSPrimitiveValue* aPrimitive,
                        nsAString& aString)
{
  nsAutoString tokenString;
  if (aValue.IsCalcUnit())
    SetValueToCalc(aValue.GetCalcValue(), aPrimitive);
  else if (aValue.GetUnit() == eStyleUnit_Coord)
    aPrimitive->SetAppUnits(aValue.GetCoordValue());
  else
    aPrimitive->SetPercent(aValue.GetPercentValue());
  aPrimitive->GetCssText(tokenString);
  aString.Append(tokenString);
}

nsresult
nsComputedDOMStyle::GetCSSGradientString(const nsStyleGradient* aGradient,
                                         nsAString& aString)
{
  if (aGradient->mRepeating) {
    if (aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_LINEAR)
      aString.AssignLiteral("-moz-repeating-linear-gradient(");
    else
      aString.AssignLiteral("-moz-repeating-radial-gradient(");
  } else {
    if (aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_LINEAR)
      aString.AssignLiteral("-moz-linear-gradient(");
    else
      aString.AssignLiteral("-moz-radial-gradient(");
  }

  PRBool needSep = PR_FALSE;
  nsAutoString tokenString;
  nsROCSSPrimitiveValue *tmpVal = GetROCSSPrimitiveValue();
  if (!tmpVal)
    return NS_ERROR_OUT_OF_MEMORY;

  if (aGradient->mBgPosX.mUnit != eStyleUnit_None) {
    AppendCSSGradientLength(aGradient->mBgPosX, tmpVal, aString);
    needSep = PR_TRUE;
  }
  if (aGradient->mBgPosY.mUnit != eStyleUnit_None) {
    if (needSep) {
      aString.AppendLiteral(" ");
    }
    AppendCSSGradientLength(aGradient->mBgPosY, tmpVal, aString);
    needSep = PR_TRUE;
  }
  if (aGradient->mAngle.mUnit != eStyleUnit_None) {
    if (needSep) {
      aString.AppendLiteral(" ");
    }
    tmpVal->SetNumber(aGradient->mAngle.GetAngleValue());
    tmpVal->GetCssText(tokenString);
    aString.Append(tokenString);
    switch (aGradient->mAngle.mUnit) {
    case eStyleUnit_Degree: aString.AppendLiteral("deg"); break;
    case eStyleUnit_Grad: aString.AppendLiteral("grad"); break;
    case eStyleUnit_Radian: aString.AppendLiteral("rad"); break;
    default: NS_NOTREACHED("unrecognized angle unit");
    }
    needSep = PR_TRUE;
  }

  if (aGradient->mShape != NS_STYLE_GRADIENT_SHAPE_LINEAR) {
    if (needSep) {
      aString.AppendLiteral(", ");
    }
    AppendASCIItoUTF16(nsCSSProps::
                       ValueToKeyword(aGradient->mShape,
                                      nsCSSProps::kRadialGradientShapeKTable),
                       aString);
    if (aGradient->mSize != NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER) {
      aString.AppendLiteral(" ");
      AppendASCIItoUTF16(nsCSSProps::
                         ValueToKeyword(aGradient->mSize,
                                        nsCSSProps::kRadialGradientSizeKTable),
                         aString);
    }
    needSep = PR_TRUE;
  }


  // color stops
  for (PRUint32 i = 0; i < aGradient->mStops.Length(); ++i) {
    if (needSep) {
      aString.AppendLiteral(", ");
    }
    nsresult rv = SetToRGBAColor(tmpVal, aGradient->mStops[i].mColor);
    if (NS_FAILED(rv)) {
      delete tmpVal;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    tmpVal->GetCssText(tokenString);
    aString.Append(tokenString);

    if (aGradient->mStops[i].mLocation.mUnit != eStyleUnit_None) {
      aString.AppendLiteral(" ");
      AppendCSSGradientLength(aGradient->mStops[i].mLocation, tmpVal, aString);
    }
    needSep = PR_TRUE;
  }

  delete tmpVal;
  aString.AppendLiteral(")");
  return NS_OK;
}

// -moz-image-rect(<uri>, <top>, <right>, <bottom>, <left>)
nsresult
nsComputedDOMStyle::GetImageRectString(nsIURI* aURI,
                                       const nsStyleSides& aCropRect,
                                       nsString& aString)
{
  nsDOMCSSValueList* valueList = GetROCSSValueList(PR_TRUE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  // <uri>
  nsROCSSPrimitiveValue *valURI = GetROCSSPrimitiveValue();
  if (!valURI || !valueList->AppendCSSValue(valURI)) {
    delete valURI;
    delete valueList;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  valURI->SetURI(aURI);

  // <top>, <right>, <bottom>, <left>
  NS_FOR_CSS_SIDES(side) {
    nsROCSSPrimitiveValue *valSide = GetROCSSPrimitiveValue();
    if (!valSide || !valueList->AppendCSSValue(valSide)) {
      delete valSide;
      delete valueList;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    SetValueToCoord(valSide, aCropRect.Get(side), PR_FALSE);
  }

  nsAutoString argumentString;
  valueList->GetCssText(argumentString);
  delete valueList;

  aString = NS_LITERAL_STRING("-moz-image-rect(") +
            argumentString +
            NS_LITERAL_STRING(")");
  return NS_OK;
}

nsresult
nsComputedDOMStyle::SetValueToStyleImage(const nsStyleImage& aStyleImage,
                                         nsROCSSPrimitiveValue* aValue)
{
  switch (aStyleImage.GetType()) {
    case eStyleImageType_Image:
    {
      imgIRequest *req = aStyleImage.GetImageData();
      nsCOMPtr<nsIURI> uri;
      req->GetURI(getter_AddRefs(uri));

      const nsStyleSides* cropRect = aStyleImage.GetCropRect();
      if (cropRect) {
        nsAutoString imageRectString;
        nsresult rv = GetImageRectString(uri, *cropRect, imageRectString);
        NS_ENSURE_SUCCESS(rv, rv);
        aValue->SetString(imageRectString);
      } else {
        aValue->SetURI(uri);
      }
      break;
    }
    case eStyleImageType_Gradient:
    {
      nsAutoString gradientString;
      nsresult rv = GetCSSGradientString(aStyleImage.GetGradientData(),
                                         gradientString);
      NS_ENSURE_SUCCESS(rv, rv);
      aValue->SetString(gradientString);
      break;
    }
    case eStyleImageType_Element:
    {
      nsAutoString elementId;
      nsStyleUtil::AppendEscapedCSSIdent(
        nsDependentString(aStyleImage.GetElementId()), elementId);
      nsAutoString elementString = NS_LITERAL_STRING("-moz-element(#") +
                                   elementId +
                                   NS_LITERAL_STRING(")");
      aValue->SetString(elementString);
      break;
    }
    case eStyleImageType_Null:
      aValue->SetIdent(eCSSKeyword_none);
      break;
    default:
      NS_NOTREACHED("unexpected image type");
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBackgroundImage(nsIDOMCSSValue** aValue)
{
  const nsStyleBackground* bg = GetStyleBackground();

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_TRUE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 i = 0, i_end = bg->mImageCount; i < i_end; ++i) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    if (!val || !valueList->AppendCSSValue(val)) {
      delete val;
      delete valueList;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    const nsStyleImage& image = bg->mLayers[i].mImage;
    nsresult rv = SetValueToStyleImage(image, val);
    if (NS_FAILED(rv)) {
      delete valueList;
      return rv;
    }
  }

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBackgroundInlinePolicy(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(
                  GetStyleBackground()->mBackgroundInlinePolicy,
                  nsCSSProps::kBackgroundInlinePolicyKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBackgroundOrigin(nsIDOMCSSValue** aValue)
{
  return GetBackgroundList(&nsStyleBackground::Layer::mOrigin,
                           &nsStyleBackground::mOriginCount,
                           nsCSSProps::kBackgroundOriginKTable,
                           aValue);
}

nsresult
nsComputedDOMStyle::DoGetBackgroundPosition(nsIDOMCSSValue** aValue)
{
  const nsStyleBackground* bg = GetStyleBackground();

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_TRUE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 i = 0, i_end = bg->mPositionCount; i < i_end; ++i) {
    nsDOMCSSValueList *itemList = GetROCSSValueList(PR_FALSE);
    if (!itemList || !valueList->AppendCSSValue(itemList)) {
      delete valueList;
      delete itemList;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsROCSSPrimitiveValue *valX = GetROCSSPrimitiveValue();
    if (!valX || !itemList->AppendCSSValue(valX)) {
      delete valueList;
      delete valX;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsROCSSPrimitiveValue *valY = GetROCSSPrimitiveValue();
    if (!valY || !itemList->AppendCSSValue(valY)) {
      delete valueList;
      delete valY;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    const nsStyleBackground::Position &pos = bg->mLayers[i].mPosition;

    if (pos.mXPosition.mLength == 0) {
      valX->SetPercent(pos.mXPosition.mPercent);
    } else if (pos.mXPosition.mPercent == 0.0f) {
      valX->SetAppUnits(pos.mXPosition.mLength);
    } else {
      nsStyleCoord::Calc calc;
      calc.mPercent = pos.mXPosition.mPercent;
      calc.mLength  = pos.mXPosition.mLength;
      calc.mHasPercent = PR_TRUE;
      SetValueToCalc(&calc, valX);
    }

    if (pos.mYPosition.mLength == 0) {
      valY->SetPercent(pos.mYPosition.mPercent);
    } else if (pos.mYPosition.mPercent == 0.0f) {
      valY->SetAppUnits(pos.mYPosition.mLength);
    } else {
      nsStyleCoord::Calc calc;
      calc.mPercent = pos.mYPosition.mPercent;
      calc.mLength  = pos.mYPosition.mLength;
      calc.mHasPercent = PR_TRUE;
      SetValueToCalc(&calc, valY);
    }
  }

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBackgroundRepeat(nsIDOMCSSValue** aValue)
{
  return GetBackgroundList(&nsStyleBackground::Layer::mRepeat,
                           &nsStyleBackground::mRepeatCount,
                           nsCSSProps::kBackgroundRepeatKTable,
                           aValue);
}

nsresult
nsComputedDOMStyle::DoGetMozBackgroundSize(nsIDOMCSSValue** aValue)
{
  const nsStyleBackground* bg = GetStyleBackground();

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_TRUE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 i = 0, i_end = bg->mSizeCount; i < i_end; ++i) {
    const nsStyleBackground::Size &size = bg->mLayers[i].mSize;

    switch (size.mWidthType) {
      case nsStyleBackground::Size::eContain:
      case nsStyleBackground::Size::eCover: {
        NS_ABORT_IF_FALSE(size.mWidthType == size.mHeightType,
                          "unsynced types");
        nsCSSKeyword keyword = size.mWidthType == nsStyleBackground::Size::eContain
                             ? eCSSKeyword_contain
                             : eCSSKeyword_cover;
        nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
        if (!val || !valueList->AppendCSSValue(val)) {
          delete valueList;
          delete val;
          return NS_ERROR_OUT_OF_MEMORY;
        }
        val->SetIdent(keyword);
        break;
      }
      default: {
        nsDOMCSSValueList *itemList = GetROCSSValueList(PR_FALSE);
        if (!itemList || !valueList->AppendCSSValue(itemList)) {
          delete valueList;
          delete itemList;
          return NS_ERROR_OUT_OF_MEMORY;
        }

        nsROCSSPrimitiveValue* valX = GetROCSSPrimitiveValue();
        nsROCSSPrimitiveValue* valY = GetROCSSPrimitiveValue();
        if (!valX || !itemList->AppendCSSValue(valX)) {
          delete valueList;
          delete valX;
          return NS_ERROR_OUT_OF_MEMORY;
        }
        if (!valY || !itemList->AppendCSSValue(valY)) {
          delete valueList;
          delete valY;
          return NS_ERROR_OUT_OF_MEMORY;
        }

        if (size.mWidthType == nsStyleBackground::Size::eAuto) {
          valX->SetIdent(eCSSKeyword_auto);
        } else {
          NS_ABORT_IF_FALSE(size.mWidthType ==
                              nsStyleBackground::Size::eLengthPercentage,
                            "bad mWidthType");
          if (size.mWidth.mLength == 0 &&
              // negative values must have come from calc()
              size.mWidth.mPercent > 0.0f) {
            valX->SetPercent(size.mWidth.mPercent);
          } else if (size.mWidth.mPercent == 0.0f &&
                     // negative values must have come from calc()
                     size.mWidth.mLength > 0) {
            valX->SetAppUnits(size.mWidth.mLength);
          } else {
            nsStyleCoord::Calc calc;
            calc.mPercent = size.mWidth.mPercent;
            calc.mLength  = size.mWidth.mLength;
            calc.mHasPercent = PR_TRUE;
            SetValueToCalc(&calc, valX);
          }
        }

        if (size.mHeightType == nsStyleBackground::Size::eAuto) {
          valY->SetIdent(eCSSKeyword_auto);
        } else {
          NS_ABORT_IF_FALSE(size.mHeightType ==
                              nsStyleBackground::Size::eLengthPercentage,
                            "bad mHeightType");
          if (size.mHeight.mLength == 0 &&
              // negative values must have come from calc()
              size.mHeight.mPercent > 0.0f) {
            valY->SetPercent(size.mHeight.mPercent);
          } else if (size.mHeight.mPercent == 0.0f &&
                     // negative values must have come from calc()
                     size.mHeight.mLength > 0) {
            valY->SetAppUnits(size.mHeight.mLength);
          } else {
            nsStyleCoord::Calc calc;
            calc.mPercent = size.mHeight.mPercent;
            calc.mLength  = size.mHeight.mLength;
            calc.mHasPercent = PR_TRUE;
            SetValueToCalc(&calc, valY);
          }
        }
        break;
      }
    }
  }

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetPadding(nsIDOMCSSValue** aValue)
{
  // return null per spec.
  *aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetPaddingTop(nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_TOP, aValue);
}

nsresult
nsComputedDOMStyle::DoGetPaddingBottom(nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_BOTTOM, aValue);
}

nsresult
nsComputedDOMStyle::DoGetPaddingLeft(nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetPaddingRight(nsIDOMCSSValue** aValue)
{
  return GetPaddingWidthFor(NS_SIDE_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderCollapse(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleTableBorder()->mBorderCollapse,
                                   nsCSSProps::kBorderCollapseKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBorderSpacing(nsIDOMCSSValue** aValue)
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
  xSpacing->SetAppUnits(border->mBorderSpacingX);
  ySpacing->SetAppUnits(border->mBorderSpacingY);

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetCaptionSide(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleTableBorder()->mCaptionSide,
                                   nsCSSProps::kCaptionSideKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetEmptyCells(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleTableBorder()->mEmptyCells,
                                   nsCSSProps::kEmptyCellsKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetTableLayout(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleTable()->mLayoutStrategy,
                                   nsCSSProps::kTableLayoutKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBorderStyle(nsIDOMCSSValue** aValue)
{
  // return null per spec.
  *aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBorderTopStyle(nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_TOP, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderBottomStyle(nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_BOTTOM, aValue);
}
nsresult
nsComputedDOMStyle::DoGetBorderLeftStyle(nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderRightStyle(nsIDOMCSSValue** aValue)
{
  return GetBorderStyleFor(NS_SIDE_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderBottomColors(nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_BOTTOM, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderLeftColors(nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderRightColors(nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_RIGHT, aValue);
}


nsresult
nsComputedDOMStyle::DoGetBorderTopColors(nsIDOMCSSValue** aValue)
{
  return GetBorderColorsFor(NS_SIDE_TOP, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderBottomLeftRadius(nsIDOMCSSValue** aValue)
{
  return GetEllipseRadii(GetStyleBorder()->mBorderRadius,
                         NS_CORNER_BOTTOM_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderBottomRightRadius(nsIDOMCSSValue** aValue)
{
  return GetEllipseRadii(GetStyleBorder()->mBorderRadius,
                         NS_CORNER_BOTTOM_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderTopLeftRadius(nsIDOMCSSValue** aValue)
{
  return GetEllipseRadii(GetStyleBorder()->mBorderRadius,
                         NS_CORNER_TOP_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderTopRightRadius(nsIDOMCSSValue** aValue)
{
  return GetEllipseRadii(GetStyleBorder()->mBorderRadius,
                         NS_CORNER_TOP_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderWidth(nsIDOMCSSValue** aValue)
{
  // return null per spec.
  *aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBorderTopWidth(nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_TOP, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderBottomWidth(nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_BOTTOM, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderLeftWidth(nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderRightWidth(nsIDOMCSSValue** aValue)
{
  return GetBorderWidthFor(NS_SIDE_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderTopColor(nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_TOP, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderBottomColor(nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_BOTTOM, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderLeftColor(nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetBorderRightColor(nsIDOMCSSValue** aValue)
{
  return GetBorderColorFor(NS_SIDE_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetMarginWidth(nsIDOMCSSValue** aValue)
{
  // return null per spec.
  *aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetMarginTopWidth(nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_TOP, aValue);
}

nsresult
nsComputedDOMStyle::DoGetMarginBottomWidth(nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_BOTTOM, aValue);
}

nsresult
nsComputedDOMStyle::DoGetMarginLeftWidth(nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetMarginRightWidth(nsIDOMCSSValue** aValue)
{
  return GetMarginWidthFor(NS_SIDE_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetMarkerOffset(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStyleContent()->mMarkerOffset, PR_FALSE);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetOutline(nsIDOMCSSValue** aValue)
{
  // return null per spec.
  *aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetOutlineWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleOutline* outline = GetStyleOutline();

  nscoord width;
  if (outline->GetOutlineStyle() == NS_STYLE_BORDER_STYLE_NONE) {
    NS_ASSERTION(outline->GetOutlineWidth(width) && width == 0,
                 "unexpected width");
    width = 0;
  } else {
#ifdef DEBUG
    PRBool res =
#endif
      outline->GetOutlineWidth(width);
    NS_ASSERTION(res, "percent outline doesn't exist");
  }
  val->SetAppUnits(width);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetOutlineStyle(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleOutline()->GetOutlineStyle(),
                                   nsCSSProps::kOutlineStyleKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetOutlineOffset(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetAppUnits(GetStyleOutline()->mOutlineOffset);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetOutlineRadiusBottomLeft(nsIDOMCSSValue** aValue)
{
  return GetEllipseRadii(GetStyleOutline()->mOutlineRadius,
                         NS_CORNER_BOTTOM_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetOutlineRadiusBottomRight(nsIDOMCSSValue** aValue)
{
  return GetEllipseRadii(GetStyleOutline()->mOutlineRadius,
                         NS_CORNER_BOTTOM_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetOutlineRadiusTopLeft(nsIDOMCSSValue** aValue)
{
  return GetEllipseRadii(GetStyleOutline()->mOutlineRadius,
                         NS_CORNER_TOP_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetOutlineRadiusTopRight(nsIDOMCSSValue** aValue)
{
  return GetEllipseRadii(GetStyleOutline()->mOutlineRadius,
                         NS_CORNER_TOP_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetOutlineColor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nscolor color;
#ifdef GFX_HAS_INVERT
  GetStyleOutline()->GetOutlineColor(color);
#else
  if (!GetStyleOutline()->GetOutlineColor(color))
    color = GetStyleColor()->mColor;
#endif

  nsresult rv = SetToRGBAColor(val, color);
  if (NS_FAILED(rv)) {
    delete val;
    return rv;
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetEllipseRadii(const nsStyleCorners& aRadius,
                                    PRUint8 aFullCorner,
                                    nsIDOMCSSValue** aValue)
{
  const nsStyleCoord& radiusX
    = aRadius.Get(NS_FULL_TO_HALF_CORNER(aFullCorner, PR_FALSE));
  const nsStyleCoord& radiusY
    = aRadius.Get(NS_FULL_TO_HALF_CORNER(aFullCorner, PR_TRUE));

  // for compatibility, return a single value if X and Y are equal
  if (radiusX == radiusY) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

    SetValueToCoord(val, radiusX, PR_TRUE,
                    &nsComputedDOMStyle::GetFrameBorderRectWidth);

    NS_ADDREF(*aValue = val);
    return NS_OK;
  } else {
    nsDOMCSSValueList *valueList = GetROCSSValueList(PR_FALSE);
    NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

    nsROCSSPrimitiveValue *valX = GetROCSSPrimitiveValue();
    if (!valX || !valueList->AppendCSSValue(valX)) {
      delete valX;
      delete valueList;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsROCSSPrimitiveValue *valY = GetROCSSPrimitiveValue();
    if (!valY || !valueList->AppendCSSValue(valY)) {
      delete valY;
      // valX deleted by valueList destructor
      delete valueList;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    SetValueToCoord(valX, radiusX, PR_TRUE,
                    &nsComputedDOMStyle::GetFrameBorderRectWidth);
    SetValueToCoord(valY, radiusY, PR_TRUE,
                    &nsComputedDOMStyle::GetFrameBorderRectWidth);

    NS_ADDREF(*aValue = valueList);
    return NS_OK;
  }
}

nsresult
nsComputedDOMStyle::GetCSSShadowArray(nsCSSShadowArray* aArray,
                                      const nscolor& aDefaultColor,
                                      PRBool aIsBoxShadow,
                                      nsIDOMCSSValue** aValue)
{
  if (!aArray) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    val->SetIdent(eCSSKeyword_none);
    NS_ADDREF(*aValue = val);
    return NS_OK;
  }

  static nscoord nsCSSShadowItem::* const shadowValuesNoSpread[] = {
    &nsCSSShadowItem::mXOffset,
    &nsCSSShadowItem::mYOffset,
    &nsCSSShadowItem::mRadius
  };

  static nscoord nsCSSShadowItem::* const shadowValuesWithSpread[] = {
    &nsCSSShadowItem::mXOffset,
    &nsCSSShadowItem::mYOffset,
    &nsCSSShadowItem::mRadius,
    &nsCSSShadowItem::mSpread
  };

  nscoord nsCSSShadowItem::* const * shadowValues;
  PRUint32 shadowValuesLength;
  if (aIsBoxShadow) {
    shadowValues = shadowValuesWithSpread;
    shadowValuesLength = NS_ARRAY_LENGTH(shadowValuesWithSpread);
  } else {
    shadowValues = shadowValuesNoSpread;
    shadowValuesLength = NS_ARRAY_LENGTH(shadowValuesNoSpread);
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_TRUE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  for (nsCSSShadowItem *item = aArray->ShadowAt(0),
                   *item_end = item + aArray->Length();
       item < item_end; ++item) {
    nsDOMCSSValueList *itemList = GetROCSSValueList(PR_FALSE);
    if (!itemList || !valueList->AppendCSSValue(itemList)) {
      delete itemList;
      delete valueList;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Color is either the specified shadow color or the foreground color
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    if (!val || !itemList->AppendCSSValue(val)) {
      delete val;
      delete valueList;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    nscolor shadowColor;
    if (item->mHasColor) {
      shadowColor = item->mColor;
    } else {
      shadowColor = aDefaultColor;
    }
    SetToRGBAColor(val, shadowColor);

    // Set the offsets, blur radius, and spread if available
    for (PRUint32 i = 0; i < shadowValuesLength; ++i) {
      val = GetROCSSPrimitiveValue();
      if (!val || !itemList->AppendCSSValue(val)) {
        delete val;
        delete valueList;
        return NS_ERROR_OUT_OF_MEMORY;
      }
      val->SetAppUnits(item->*(shadowValues[i]));
    }

    if (item->mInset && aIsBoxShadow) {
      // This is an inset box-shadow
      val = GetROCSSPrimitiveValue();
      if (!val || !itemList->AppendCSSValue(val)) {
        delete val;
        delete valueList;
        return NS_ERROR_OUT_OF_MEMORY;
      }
      val->SetIdent(
        nsCSSProps::ValueToKeywordEnum(NS_STYLE_BOX_SHADOW_INSET,
                                       nsCSSProps::kBoxShadowTypeKTable));
    }
  }

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBoxShadow(nsIDOMCSSValue** aValue)
{
  return GetCSSShadowArray(GetStyleBorder()->mBoxShadow,
                           GetStyleColor()->mColor,
                           PR_TRUE, aValue);
}

nsresult
nsComputedDOMStyle::DoGetZIndex(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStylePosition()->mZIndex, PR_FALSE);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetListStyleImage(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleList* list = GetStyleList();

  if (!list->GetListStyleImage()) {
    val->SetIdent(eCSSKeyword_none);
  } else {
    nsCOMPtr<nsIURI> uri;
    if (list->GetListStyleImage()) {
      list->GetListStyleImage()->GetURI(getter_AddRefs(uri));
    }
    val->SetURI(uri);
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetListStylePosition(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleList()->mListStylePosition,
                                   nsCSSProps::kListStylePositionKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetListStyleType(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleList()->mListStyleType,
                                   nsCSSProps::kListStyleKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetImageRegion(nsIDOMCSSValue** aValue)
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
    val->SetIdent(eCSSKeyword_auto);
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
        topVal->SetAppUnits(list->mImageRegion.y);
        rightVal->SetAppUnits(list->mImageRegion.width + list->mImageRegion.x);
        bottomVal->SetAppUnits(list->mImageRegion.height + list->mImageRegion.y);
        leftVal->SetAppUnits(list->mImageRegion.x);
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

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetLineHeight(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nscoord lineHeight;
  if (GetLineHeightCoord(lineHeight)) {
    val->SetAppUnits(lineHeight);
  } else {
    SetValueToCoord(val, GetStyleText()->mLineHeight, PR_TRUE,
                    nsnull, nsCSSProps::kLineHeightKTable);
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetVerticalAlign(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStyleTextReset()->mVerticalAlign, PR_FALSE,
                  &nsComputedDOMStyle::GetLineHeightCoord,
                  nsCSSProps::kVerticalAlignKTable);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetTextAlign(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleText()->mTextAlign,
                                   nsCSSProps::kTextAlignKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetTextDecoration(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  PRInt32 intValue = GetStyleTextReset()->mTextDecoration;

  if (NS_STYLE_TEXT_DECORATION_NONE == intValue) {
    val->SetIdent(eCSSKeyword_none);
  } else {
    nsAutoString decorationString;
    // Clear the -moz-anchor-decoration bit and the OVERRIDE_ALL bits -- we
    // don't want these to appear in the computed style.
    intValue &= ~(NS_STYLE_TEXT_DECORATION_PREF_ANCHORS |
                  NS_STYLE_TEXT_DECORATION_OVERRIDE_ALL);
    nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_text_decoration, intValue,
                                       NS_STYLE_TEXT_DECORATION_UNDERLINE,
                                       NS_STYLE_TEXT_DECORATION_BLINK,
                                       decorationString);
    val->SetString(decorationString);
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetTextIndent(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStyleText()->mTextIndent, PR_FALSE,
                  &nsComputedDOMStyle::GetCBContentWidth);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetTextShadow(nsIDOMCSSValue** aValue)
{
  return GetCSSShadowArray(GetStyleText()->mTextShadow,
                           GetStyleColor()->mColor,
                           PR_FALSE, aValue);
}

nsresult
nsComputedDOMStyle::DoGetTextTransform(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleText()->mTextTransform,
                                   nsCSSProps::kTextTransformKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetMozTabSize(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleText()->mTabSize);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetLetterSpacing(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStyleText()->mLetterSpacing, PR_FALSE);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetWordSpacing(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetAppUnits(GetStyleText()->mWordSpacing);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetWhiteSpace(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleText()->mWhiteSpace,
                                   nsCSSProps::kWhitespaceKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetWindowShadow(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleUIReset()->mWindowShadow,
                                   nsCSSProps::kWindowShadowKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}


nsresult
nsComputedDOMStyle::DoGetWordWrap(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleText()->mWordWrap,
                                   nsCSSProps::kWordwrapKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetPointerEvents(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleVisibility()->mPointerEvents,
                                   nsCSSProps::kPointerEventsKTable));
  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetVisibility(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleVisibility()->mVisible,
                                               nsCSSProps::kVisibilityKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetDirection(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleVisibility()->mDirection,
                                   nsCSSProps::kDirectionKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetUnicodeBidi(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleTextReset()->mUnicodeBidi,
                                   nsCSSProps::kUnicodeBidiKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetCursor(nsIDOMCSSValue** aValue)
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
    item->GetImage()->GetURI(getter_AddRefs(uri));

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

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(ui->mCursor,
                                               nsCSSProps::kCursorKTable));
  if (!valueList->AppendCSSValue(val)) {
    delete valueList;
    delete val;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetAppearance(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleDisplay()->mAppearance,
                                               nsCSSProps::kAppearanceKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}


nsresult
nsComputedDOMStyle::DoGetBoxAlign(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleXUL()->mBoxAlign,
                                               nsCSSProps::kBoxAlignKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBoxDirection(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleXUL()->mBoxDirection,
                                   nsCSSProps::kBoxDirectionKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBoxFlex(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleXUL()->mBoxFlex);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBoxOrdinalGroup(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleXUL()->mBoxOrdinal);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBoxOrient(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleXUL()->mBoxOrient,
                                   nsCSSProps::kBoxOrientKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBoxPack(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleXUL()->mBoxPack,
                                               nsCSSProps::kBoxPackKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBoxSizing(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStylePosition()->mBoxSizing,
                                   nsCSSProps::kBoxSizingKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetBorderImage(nsIDOMCSSValue** aValue)
{
  const nsStyleBorder* border = GetStyleBorder();

  // none
  if (!border->GetBorderImage()) {
    nsROCSSPrimitiveValue *valNone = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(valNone, NS_ERROR_OUT_OF_MEMORY);
    valNone->SetIdent(eCSSKeyword_none);
    NS_ADDREF(*aValue = valNone);
    return NS_OK;
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_FALSE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  // uri
  nsROCSSPrimitiveValue *valURI = GetROCSSPrimitiveValue();
  if (!valURI || !valueList->AppendCSSValue(valURI)) {
    delete valURI;
    delete valueList;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIURI> uri;
  border->GetBorderImage()->GetURI(getter_AddRefs(uri));
  valURI->SetURI(uri);

  // four split numbers
  NS_FOR_CSS_SIDES(side) {
    nsROCSSPrimitiveValue *valSplit = GetROCSSPrimitiveValue();
    if (!valSplit || !valueList->AppendCSSValue(valSplit)) {
      delete valSplit;
      delete valueList;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    SetValueToCoord(valSplit, border->mBorderImageSplit.Get(side), PR_TRUE);
  }

  // copy of border-width
  if (border->mHaveBorderImageWidth) {
    nsROCSSPrimitiveValue *slash = GetROCSSPrimitiveValue();
    if (!slash || !valueList->AppendCSSValue(slash)) {
      delete slash;
      delete valueList;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    slash->SetString(NS_LITERAL_STRING("/"));
    NS_FOR_CSS_SIDES(side) {
      nsROCSSPrimitiveValue *borderWidth = GetROCSSPrimitiveValue();
      if (!borderWidth || !valueList->AppendCSSValue(borderWidth)) {
        delete borderWidth;
        delete valueList;
        return NS_ERROR_OUT_OF_MEMORY;
      }
      nscoord width = GetStyleBorder()->mBorderImageWidth.side(side);
      borderWidth->SetAppUnits(width);
    }
  }

  // first keyword
  nsROCSSPrimitiveValue *keyword = GetROCSSPrimitiveValue();
  if (!keyword || !valueList->AppendCSSValue(keyword)) {
    delete keyword;
    delete valueList;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  keyword->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleBorder()->mBorderImageHFill,
                                   nsCSSProps::kBorderImageKTable));

  // second keyword
  nsROCSSPrimitiveValue *keyword2 = GetROCSSPrimitiveValue();
  if (!keyword2 || !valueList->AppendCSSValue(keyword2)) {
    delete keyword2;
    delete valueList;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  keyword2->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleBorder()->mBorderImageVFill,
                                   nsCSSProps::kBorderImageKTable));

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetFloatEdge(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleBorder()->mFloatEdge,
                                   nsCSSProps::kFloatEdgeKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetForceBrokenImageIcon(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleUIReset()->mForceBrokenImageIcon);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetIMEMode(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleUIReset()->mIMEMode,
                                   nsCSSProps::kIMEModeKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetUserFocus(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleUserInterface()->mUserFocus,
                                   nsCSSProps::kUserFocusKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetUserInput(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleUserInterface()->mUserInput,
                                   nsCSSProps::kUserInputKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetUserModify(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleUserInterface()->mUserModify,
                                   nsCSSProps::kUserModifyKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetUserSelect(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleUIReset()->mUserSelect,
                                   nsCSSProps::kUserSelectKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetDisplay(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleDisplay()->mDisplay,
                                               nsCSSProps::kDisplayKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetPosition(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleDisplay()->mPosition,
                                               nsCSSProps::kPositionKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetClip(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = GetStyleDisplay();

  nsresult rv = NS_OK;
  nsROCSSPrimitiveValue *topVal = nsnull;
  nsROCSSPrimitiveValue *rightVal = nsnull;
  nsROCSSPrimitiveValue *bottomVal = nsnull;
  nsROCSSPrimitiveValue *leftVal = nsnull;
  if (display->mClipFlags == NS_STYLE_CLIP_AUTO) {
    val->SetIdent(eCSSKeyword_auto);
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
          topVal->SetIdent(eCSSKeyword_auto);
        } else {
          topVal->SetAppUnits(display->mClip.y);
        }

        if (display->mClipFlags & NS_STYLE_CLIP_RIGHT_AUTO) {
          rightVal->SetIdent(eCSSKeyword_auto);
        } else {
          rightVal->SetAppUnits(display->mClip.width + display->mClip.x);
        }

        if (display->mClipFlags & NS_STYLE_CLIP_BOTTOM_AUTO) {
          bottomVal->SetIdent(eCSSKeyword_auto);
        } else {
          bottomVal->SetAppUnits(display->mClip.height + display->mClip.y);
        }

        if (display->mClipFlags & NS_STYLE_CLIP_LEFT_AUTO) {
          leftVal->SetIdent(eCSSKeyword_auto);
        } else {
          leftVal->SetAppUnits(display->mClip.x);
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

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetOverflow(nsIDOMCSSValue** aValue)
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

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(display->mOverflowX,
                                               nsCSSProps::kOverflowKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetOverflowX(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleDisplay()->mOverflowX,
                                   nsCSSProps::kOverflowSubKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetOverflowY(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleDisplay()->mOverflowY,
                                   nsCSSProps::kOverflowSubKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetResize(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleDisplay()->mResize,
                                               nsCSSProps::kResizeKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}


nsresult
nsComputedDOMStyle::DoGetPageBreakAfter(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay *display = GetStyleDisplay();

  if (display->mBreakAfter) {
    val->SetIdent(eCSSKeyword_always);
  } else {
    val->SetIdent(eCSSKeyword_auto);
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetPageBreakBefore(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay *display = GetStyleDisplay();

  if (display->mBreakBefore) {
    val->SetIdent(eCSSKeyword_always);
  } else {
    val->SetIdent(eCSSKeyword_auto);
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetHeight(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  PRBool calcHeight = PR_FALSE;

  if (mInnerFrame) {
    calcHeight = PR_TRUE;

    const nsStyleDisplay* displayData = GetStyleDisplay();
    if (displayData->mDisplay == NS_STYLE_DISPLAY_INLINE &&
        !(mInnerFrame->IsFrameOfType(nsIFrame::eReplaced))) {
      calcHeight = PR_FALSE;
    }
  }

  if (calcHeight) {
    AssertFlushedPendingReflows();

    val->SetAppUnits(mInnerFrame->GetContentRect().height);
  } else {
    const nsStylePosition *positionData = GetStylePosition();

    nscoord minHeight =
      StyleCoordToNSCoord(positionData->mMinHeight,
                          &nsComputedDOMStyle::GetCBContentHeight, 0, PR_TRUE);

    nscoord maxHeight =
      StyleCoordToNSCoord(positionData->mMaxHeight,
                          &nsComputedDOMStyle::GetCBContentHeight,
                          nscoord_MAX, PR_TRUE);

    SetValueToCoord(val, positionData->mHeight, PR_TRUE, nsnull, nsnull,
                    minHeight, maxHeight);
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  PRBool calcWidth = PR_FALSE;

  if (mInnerFrame) {
    calcWidth = PR_TRUE;

    const nsStyleDisplay *displayData = GetStyleDisplay();
    if (displayData->mDisplay == NS_STYLE_DISPLAY_INLINE &&
        !(mInnerFrame->IsFrameOfType(nsIFrame::eReplaced))) {
      calcWidth = PR_FALSE;
    }
  }

  if (calcWidth) {
    AssertFlushedPendingReflows();

    val->SetAppUnits(mInnerFrame->GetContentRect().width);
  } else {
    const nsStylePosition *positionData = GetStylePosition();

    nscoord minWidth =
      StyleCoordToNSCoord(positionData->mMinWidth,
                          &nsComputedDOMStyle::GetCBContentWidth, 0, PR_TRUE);

    nscoord maxWidth =
      StyleCoordToNSCoord(positionData->mMaxWidth,
                          &nsComputedDOMStyle::GetCBContentWidth,
                          nscoord_MAX, PR_TRUE);

    SetValueToCoord(val, positionData->mWidth, PR_TRUE, nsnull,
                    nsCSSProps::kWidthKTable, minWidth, maxWidth);
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetMaxHeight(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStylePosition()->mMaxHeight, PR_TRUE,
                  &nsComputedDOMStyle::GetCBContentHeight);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetMaxWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStylePosition()->mMaxWidth, PR_TRUE,
                  &nsComputedDOMStyle::GetCBContentWidth,
                  nsCSSProps::kWidthKTable);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetMinHeight(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStylePosition()->mMinHeight, PR_TRUE,
                  &nsComputedDOMStyle::GetCBContentHeight);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetMinWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStylePosition()->mMinWidth, PR_TRUE,
                  &nsComputedDOMStyle::GetCBContentWidth,
                  nsCSSProps::kWidthKTable);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetLeft(nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetRight(nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::DoGetTop(nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_TOP, aValue);
}

nsROCSSPrimitiveValue*
nsComputedDOMStyle::GetROCSSPrimitiveValue()
{
  nsROCSSPrimitiveValue *primitiveValue = new nsROCSSPrimitiveValue();

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
nsComputedDOMStyle::GetOffsetWidthFor(mozilla::css::Side aSide,
                                      nsIDOMCSSValue** aValue)
{
  const nsStyleDisplay* display = GetStyleDisplay();

  AssertFlushedPendingReflows();

  PRUint8 position = display->mPosition;
  if (!mOuterFrame) {
    // GetRelativeOffset and GetAbsoluteOffset don't handle elements
    // without frames in any sensible way.  GetStaticOffset, however,
    // is perfect for that case.
    position = NS_STYLE_POSITION_STATIC;
  }

  nsresult rv = NS_OK;
  switch (position) {
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
nsComputedDOMStyle::GetAbsoluteOffset(mozilla::css::Side aSide,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nsIFrame* container = GetContainingBlockFor(mOuterFrame);
  if (container) {
    nsMargin margin = mOuterFrame->GetUsedMargin();
    nsMargin border = container->GetUsedBorder();
    nsMargin scrollbarSizes(0, 0, 0, 0);
    nsRect rect = mOuterFrame->GetRect();
    nsRect containerRect = container->GetRect();

    if (container->GetType() == nsGkAtoms::viewportFrame) {
      // For absolutely positioned frames scrollbars are taken into
      // account by virtue of getting a containing block that does
      // _not_ include the scrollbars.  For fixed positioned frames,
      // the containing block is the viewport, which _does_ include
      // scrollbars.  We have to do some extra work.
      // the first child in the default frame list is what we want
      nsIFrame* scrollingChild = container->GetFirstChild(nsnull);
      nsIScrollableFrame *scrollFrame = do_QueryFrame(scrollingChild);
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
    val->SetAppUnits(offset);
  } else {
    // XXX no frame.  This property makes no sense
    val->SetAppUnits(0);
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

PR_STATIC_ASSERT((NS_SIDE_TOP == 0) && (NS_SIDE_RIGHT == 1) && (NS_SIDE_BOTTOM == 2) && (NS_SIDE_LEFT == 3));
#define NS_OPPOSITE_SIDE(s_) mozilla::css::Side(((s_) + 2) & 3)

nsresult
nsComputedDOMStyle::GetRelativeOffset(mozilla::css::Side aSide,
                                      nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStylePosition* positionData = GetStylePosition();
  PRInt32 sign = 1;
  nsStyleCoord coord = positionData->mOffset.Get(aSide);

  NS_ASSERTION(coord.GetUnit() == eStyleUnit_Coord ||
               coord.GetUnit() == eStyleUnit_Percent ||
               coord.GetUnit() == eStyleUnit_Auto ||
               coord.IsCalcUnit(),
               "Unexpected unit");

  if (coord.GetUnit() == eStyleUnit_Auto) {
    coord = positionData->mOffset.Get(NS_OPPOSITE_SIDE(aSide));
    sign = -1;
  }
  PercentageBaseGetter baseGetter;
  if (aSide == NS_SIDE_LEFT || aSide == NS_SIDE_RIGHT) {
    baseGetter = &nsComputedDOMStyle::GetCBContentWidth;
  } else {
    baseGetter = &nsComputedDOMStyle::GetCBContentHeight;
  }

  val->SetAppUnits(sign * StyleCoordToNSCoord(coord, baseGetter, 0, PR_FALSE));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetStaticOffset(mozilla::css::Side aSide, nsIDOMCSSValue** aValue)

{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStylePosition()->mOffset.Get(aSide), PR_FALSE);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetPaddingWidthFor(mozilla::css::Side aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  if (!mInnerFrame) {
    SetValueToCoord(val, GetStylePadding()->mPadding.Get(aSide), PR_TRUE);
  } else {
    AssertFlushedPendingReflows();

    val->SetAppUnits(mInnerFrame->GetUsedPadding().side(aSide));
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

PRBool
nsComputedDOMStyle::GetLineHeightCoord(nscoord& aCoord)
{
  AssertFlushedPendingReflows();

  nscoord blockHeight = NS_AUTOHEIGHT;
  if (GetStyleText()->mLineHeight.GetUnit() == eStyleUnit_Enumerated) {
    if (!mInnerFrame)
      return PR_FALSE;

    if (mInnerFrame->IsContainingBlock()) {
      blockHeight = mInnerFrame->GetContentRect().height;
    } else {
      GetCBContentHeight(blockHeight);
    }
  }

  aCoord = nsHTMLReflowState::CalcLineHeight(mStyleContextHolder,
                                             blockHeight);

  // CalcLineHeight uses font->mFont.size, but we want to use
  // font->mSize as the font size.  Adjust for that.  Also adjust for
  // the text zoom, if any.
  const nsStyleFont* font = GetStyleFont();
  aCoord = NSToCoordRound((float(aCoord) *
                           (float(font->mSize) / float(font->mFont.size))) /
                          mPresShell->GetPresContext()->TextZoom());

  return PR_TRUE;
}

nsresult
nsComputedDOMStyle::GetBorderColorsFor(mozilla::css::Side aSide,
                                       nsIDOMCSSValue** aValue)
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
        nsresult rv = SetToRGBAColor(primitive, borderColors->mColor);
        if (NS_FAILED(rv)) {
          delete valueList;
          delete primitive;
          return rv;
        }

        PRBool success = valueList->AppendCSSValue(primitive);
        if (!success) {
          delete valueList;
          delete primitive;

          return NS_ERROR_OUT_OF_MEMORY;
        }
        borderColors = borderColors->mNext;
      } while (borderColors);

      NS_ADDREF(*aValue = valueList);
      return NS_OK;
    }
  }

  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(eCSSKeyword_none);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetBorderWidthFor(mozilla::css::Side aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nscoord width;
  if (mInnerFrame) {
    AssertFlushedPendingReflows();
    width = mInnerFrame->GetUsedBorder().side(aSide);
  } else {
    width = GetStyleBorder()->GetActualBorderWidth(aSide);
  }
  val->SetAppUnits(width);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetBorderColorFor(mozilla::css::Side aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nscolor color;
  PRBool foreground;
  GetStyleBorder()->GetBorderColor(aSide, color, foreground);
  if (foreground) {
    color = GetStyleColor()->mColor;
  }

  nsresult rv = SetToRGBAColor(val, color);
  if (NS_FAILED(rv)) {
    delete val;
    return rv;
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetMarginWidthFor(mozilla::css::Side aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  if (!mInnerFrame) {
    SetValueToCoord(val, GetStyleMargin()->mMargin.Get(aSide), PR_FALSE);
  } else {
    AssertFlushedPendingReflows();

    val->SetAppUnits(mInnerFrame->GetUsedMargin().side(aSide));
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetBorderStyleFor(mozilla::css::Side aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleBorder()->GetBorderStyle(aSide),
                                   nsCSSProps::kBorderStyleKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

void
nsComputedDOMStyle::SetValueToCoord(nsROCSSPrimitiveValue* aValue,
                                    const nsStyleCoord& aCoord,
                                    PRBool aClampNegativeCalc,
                                    PercentageBaseGetter aPercentageBaseGetter,
                                    const PRInt32 aTable[],
                                    nscoord aMinAppUnits,
                                    nscoord aMaxAppUnits)
{
  NS_PRECONDITION(aValue, "Must have a value to work with");

  switch (aCoord.GetUnit()) {
    case eStyleUnit_Normal:
      aValue->SetIdent(eCSSKeyword_normal);
      break;

    case eStyleUnit_Auto:
      aValue->SetIdent(eCSSKeyword_auto);
      break;

    case eStyleUnit_Percent:
      {
        nscoord percentageBase;
        if (aPercentageBaseGetter &&
            (this->*aPercentageBaseGetter)(percentageBase)) {
          nscoord val = NSCoordSaturatingMultiply(percentageBase,
                                                  aCoord.GetPercentValue());
          aValue->SetAppUnits(NS_MAX(aMinAppUnits, NS_MIN(val, aMaxAppUnits)));
        } else {
          aValue->SetPercent(aCoord.GetPercentValue());
        }
      }
      break;

    case eStyleUnit_Factor:
      aValue->SetNumber(aCoord.GetFactorValue());
      break;

    case eStyleUnit_Coord:
      {
        nscoord val = aCoord.GetCoordValue();
        aValue->SetAppUnits(NS_MAX(aMinAppUnits, NS_MIN(val, aMaxAppUnits)));
      }
      break;

    case eStyleUnit_Integer:
      aValue->SetNumber(aCoord.GetIntValue());
      break;

    case eStyleUnit_Enumerated:
      NS_ASSERTION(aTable, "Must have table to handle this case");
      aValue->SetIdent(nsCSSProps::ValueToKeywordEnum(aCoord.GetIntValue(),
                                                      aTable));
      break;

    case eStyleUnit_None:
      aValue->SetIdent(eCSSKeyword_none);
      break;

    case eStyleUnit_Calc:
      nscoord percentageBase;
      if (!aCoord.CalcHasPercent()) {
        nscoord val = nsRuleNode::ComputeCoordPercentCalc(aCoord, 0);
        if (aClampNegativeCalc && val < 0) {
          NS_ABORT_IF_FALSE(aCoord.IsCalcUnit(),
                            "parser should have rejected value");
          val = 0;
        }
        aValue->SetAppUnits(NS_MAX(aMinAppUnits, NS_MIN(val, aMaxAppUnits)));
      } else if (aPercentageBaseGetter &&
                 (this->*aPercentageBaseGetter)(percentageBase)) {
        nscoord val =
          nsRuleNode::ComputeCoordPercentCalc(aCoord, percentageBase);
        if (aClampNegativeCalc && val < 0) {
          NS_ABORT_IF_FALSE(aCoord.IsCalcUnit(),
                            "parser should have rejected value");
          val = 0;
        }
        aValue->SetAppUnits(NS_MAX(aMinAppUnits, NS_MIN(val, aMaxAppUnits)));
      } else {
        nsStyleCoord::Calc *calc = aCoord.GetCalcValue();
        SetValueToCalc(calc, aValue);
      }
      break;
    default:
      NS_ERROR("Can't handle this unit");
      break;
  }
}

nscoord
nsComputedDOMStyle::StyleCoordToNSCoord(const nsStyleCoord& aCoord,
                                        PercentageBaseGetter aPercentageBaseGetter,
                                        nscoord aDefaultValue,
                                        PRBool aClampNegativeCalc)
{
  NS_PRECONDITION(aPercentageBaseGetter, "Must have a percentage base getter");
  if (aCoord.GetUnit() == eStyleUnit_Coord) {
    return aCoord.GetCoordValue();
  }
  if (aCoord.GetUnit() == eStyleUnit_Percent || aCoord.IsCalcUnit()) {
    nscoord percentageBase;
    if ((this->*aPercentageBaseGetter)(percentageBase)) {
      nscoord result =
        nsRuleNode::ComputeCoordPercentCalc(aCoord, percentageBase);
      if (aClampNegativeCalc && result < 0) {
        NS_ABORT_IF_FALSE(aCoord.IsCalcUnit(),
                          "parser should have rejected value");
        result = 0;
      }
      return result;
    }
    // Fall through to returning aDefaultValue if we have no percentage base.
  }

  return aDefaultValue;
}

PRBool
nsComputedDOMStyle::GetCBContentWidth(nscoord& aWidth)
{
  if (!mOuterFrame) {
    return PR_FALSE;
  }

  nsIFrame* container = GetContainingBlockFor(mOuterFrame);
  if (!container) {
    return PR_FALSE;
  }

  AssertFlushedPendingReflows();

  aWidth = container->GetContentRect().width;
  return PR_TRUE;
}

PRBool
nsComputedDOMStyle::GetCBContentHeight(nscoord& aHeight)
{
  if (!mOuterFrame) {
    return PR_FALSE;
  }

  nsIFrame* container = GetContainingBlockFor(mOuterFrame);
  if (!container) {
    return PR_FALSE;
  }

  AssertFlushedPendingReflows();

  aHeight = container->GetContentRect().height;
  return PR_TRUE;
}

PRBool
nsComputedDOMStyle::GetFrameBorderRectWidth(nscoord& aWidth)
{
  if (!mInnerFrame) {
    return PR_FALSE;
  }

  AssertFlushedPendingReflows();

  aWidth = mInnerFrame->GetSize().width;
  return PR_TRUE;
}

PRBool
nsComputedDOMStyle::GetFrameBoundsWidthForTransform(nscoord& aWidth)
{
  // We need a frame to work with.
  if (!mInnerFrame) {
    return PR_FALSE;
  }

  AssertFlushedPendingReflows();

  // Check to see that we're transformed.
  if (!mInnerFrame->GetStyleDisplay()->HasTransform())
    return PR_FALSE;

  aWidth = nsDisplayTransform::GetFrameBoundsForTransform(mInnerFrame).width;
  return PR_TRUE;
}

PRBool
nsComputedDOMStyle::GetFrameBoundsHeightForTransform(nscoord& aHeight)
{
  // We need a frame to work with.
  if (!mInnerFrame) {
    return PR_FALSE;
  }

  AssertFlushedPendingReflows();

  // Check to see that we're transformed.
  if (!mInnerFrame->GetStyleDisplay()->HasTransform())
    return PR_FALSE;

  aHeight = nsDisplayTransform::GetFrameBoundsForTransform(mInnerFrame).height;
  return PR_TRUE;
}

nsresult
nsComputedDOMStyle::GetSVGPaintFor(PRBool aFill,
                                   nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleSVG* svg = GetStyleSVG();
  const nsStyleSVGPaint* paint = nsnull;

  if (aFill)
    paint = &svg->mFill;
  else
    paint = &svg->mStroke;

  nsAutoString paintString;

  switch (paint->mType) {
  case eStyleSVGPaintType_None:
  {
    val->SetIdent(eCSSKeyword_none);
    break;
  }
  case eStyleSVGPaintType_Color:
  {
    nsresult rv = SetToRGBAColor(val, paint->mPaint.mColor);
    if (NS_FAILED(rv)) {
      delete val;
      return rv;
    }
    break;
  }
  case eStyleSVGPaintType_Server:
  {
    nsDOMCSSValueList *valueList = GetROCSSValueList(PR_FALSE);
    NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

    if (!valueList->AppendCSSValue(val)) {
      delete valueList;
      delete val;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsROCSSPrimitiveValue* fallback = GetROCSSPrimitiveValue();
    if (!fallback || !valueList->AppendCSSValue(fallback)) {
      delete valueList;
      delete fallback;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    val->SetURI(paint->mPaint.mPaintServer);
    nsresult rv = SetToRGBAColor(fallback, paint->mFallbackColor);
    if (NS_FAILED(rv)) {
      delete valueList;
      return rv;
    }

    NS_ADDREF(*aValue = valueList);
    return NS_OK;
  }
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetFill(nsIDOMCSSValue** aValue)
{
  return GetSVGPaintFor(PR_TRUE, aValue);
}

nsresult
nsComputedDOMStyle::DoGetStroke(nsIDOMCSSValue** aValue)
{
  return GetSVGPaintFor(PR_FALSE, aValue);
}

nsresult
nsComputedDOMStyle::DoGetMarkerEnd(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleSVG* svg = GetStyleSVG();

  if (svg->mMarkerEnd)
    val->SetURI(svg->mMarkerEnd);
  else
    val->SetIdent(eCSSKeyword_none);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetMarkerMid(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleSVG* svg = GetStyleSVG();

  if (svg->mMarkerMid)
    val->SetURI(svg->mMarkerMid);
  else
    val->SetIdent(eCSSKeyword_none);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetMarkerStart(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleSVG* svg = GetStyleSVG();

  if (svg->mMarkerStart)
    val->SetURI(svg->mMarkerStart);
  else
    val->SetIdent(eCSSKeyword_none);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetStrokeDasharray(nsIDOMCSSValue** aValue)
{
  const nsStyleSVG* svg = GetStyleSVG();

  if (!svg->mStrokeDasharrayLength || !svg->mStrokeDasharray) {
    nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
    val->SetIdent(eCSSKeyword_none);
    NS_ADDREF(*aValue = val);
    return NS_OK;
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_TRUE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 i = 0; i < svg->mStrokeDasharrayLength; i++) {
    nsROCSSPrimitiveValue* dash = GetROCSSPrimitiveValue();
    if (!dash || !valueList->AppendCSSValue(dash)) {
      delete valueList;
      delete dash;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    SetValueToCoord(dash, svg->mStrokeDasharray[i], PR_TRUE);
  }

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetStrokeDashoffset(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStyleSVG()->mStrokeDashoffset, PR_FALSE);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetStrokeWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStyleSVG()->mStrokeWidth, PR_TRUE);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetFillOpacity(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleSVG()->mFillOpacity);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetFloodOpacity(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleSVGReset()->mFloodOpacity);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetStopOpacity(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleSVGReset()->mStopOpacity);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetStrokeMiterlimit(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleSVG()->mStrokeMiterlimit);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetStrokeOpacity(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleSVG()->mStrokeOpacity);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetClipRule(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(
                  GetStyleSVG()->mClipRule, nsCSSProps::kFillRuleKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetFillRule(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(
                  GetStyleSVG()->mFillRule, nsCSSProps::kFillRuleKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetStrokeLinecap(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mStrokeLinecap,
                                   nsCSSProps::kStrokeLinecapKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetStrokeLinejoin(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mStrokeLinejoin,
                                   nsCSSProps::kStrokeLinejoinKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetTextAnchor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mTextAnchor,
                                   nsCSSProps::kTextAnchorKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetColorInterpolation(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mColorInterpolation,
                                   nsCSSProps::kColorInterpolationKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetColorInterpolationFilters(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mColorInterpolationFilters,
                                   nsCSSProps::kColorInterpolationKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetDominantBaseline(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVGReset()->mDominantBaseline,
                                   nsCSSProps::kDominantBaselineKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetImageRendering(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mImageRendering,
                                   nsCSSProps::kImageRenderingKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetShapeRendering(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mShapeRendering,
                                   nsCSSProps::kShapeRenderingKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetTextRendering(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mTextRendering,
                                   nsCSSProps::kTextRenderingKTable));

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetFloodColor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = SetToRGBAColor(val, GetStyleSVGReset()->mFloodColor);
  if (NS_FAILED(rv)) {
    delete val;
    return rv;
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetLightingColor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = SetToRGBAColor(val, GetStyleSVGReset()->mLightingColor);
  if (NS_FAILED(rv)) {
    delete val;
    return rv;
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetStopColor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = SetToRGBAColor(val, GetStyleSVGReset()->mStopColor);
  if (NS_FAILED(rv)) {
    delete val;
    return rv;
  }

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetClipPath(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleSVGReset* svg = GetStyleSVGReset();

  if (svg->mClipPath)
    val->SetURI(svg->mClipPath);
  else
    val->SetIdent(eCSSKeyword_none);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetFilter(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleSVGReset* svg = GetStyleSVGReset();

  if (svg->mFilter)
    val->SetURI(svg->mFilter);
  else
    val->SetIdent(eCSSKeyword_none);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetMask(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleSVGReset* svg = GetStyleSVGReset();

  if (svg->mMask)
    val->SetURI(svg->mMask);
  else
    val->SetIdent(eCSSKeyword_none);

  NS_ADDREF(*aValue = val);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetTransitionDelay(nsIDOMCSSValue** aValue)
{
  const nsStyleDisplay* display = GetStyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_TRUE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  NS_ABORT_IF_FALSE(display->mTransitionDelayCount > 0,
                    "first item must be explicit");
  PRUint32 i = 0;
  do {
    const nsTransition *transition = &display->mTransitions[i];
    nsROCSSPrimitiveValue* delay = GetROCSSPrimitiveValue();
    if (!delay || !valueList->AppendCSSValue(delay)) {
      delete valueList;
      delete delay;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    delay->SetTime((float)transition->GetDelay() / (float)PR_MSEC_PER_SEC);
  } while (++i < display->mTransitionDelayCount);

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetTransitionDuration(nsIDOMCSSValue** aValue)
{
  const nsStyleDisplay* display = GetStyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_TRUE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  NS_ABORT_IF_FALSE(display->mTransitionDurationCount > 0,
                    "first item must be explicit");
  PRUint32 i = 0;
  do {
    const nsTransition *transition = &display->mTransitions[i];
    nsROCSSPrimitiveValue* duration = GetROCSSPrimitiveValue();
    if (!duration || !valueList->AppendCSSValue(duration)) {
      delete valueList;
      delete duration;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    duration->SetTime((float)transition->GetDuration() / (float)PR_MSEC_PER_SEC);
  } while (++i < display->mTransitionDurationCount);

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetTransitionProperty(nsIDOMCSSValue** aValue)
{
  const nsStyleDisplay* display = GetStyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_TRUE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  NS_ABORT_IF_FALSE(display->mTransitionPropertyCount > 0,
                    "first item must be explicit");
  PRUint32 i = 0;
  do {
    const nsTransition *transition = &display->mTransitions[i];
    nsROCSSPrimitiveValue* property = GetROCSSPrimitiveValue();
    if (!property || !valueList->AppendCSSValue(property)) {
      delete valueList;
      delete property;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    nsCSSProperty cssprop = transition->GetProperty();
    if (cssprop == eCSSPropertyExtra_all_properties)
      property->SetIdent(eCSSKeyword_all);
    else if (cssprop == eCSSPropertyExtra_no_properties)
      property->SetIdent(eCSSKeyword_none);
    else if (cssprop == eCSSProperty_UNKNOWN)
    {
      nsAutoString escaped;
      nsStyleUtil::AppendEscapedCSSIdent(
        nsDependentAtomString(transition->GetUnknownProperty()), escaped);
      property->SetString(escaped); // really want SetIdent
    }
    else
      property->SetString(nsCSSProps::GetStringValue(cssprop));
  } while (++i < display->mTransitionPropertyCount);

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

nsresult
nsComputedDOMStyle::DoGetTransitionTimingFunction(nsIDOMCSSValue** aValue)
{
  const nsStyleDisplay* display = GetStyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(PR_TRUE);
  NS_ENSURE_TRUE(valueList, NS_ERROR_OUT_OF_MEMORY);

  NS_ABORT_IF_FALSE(display->mTransitionTimingFunctionCount > 0,
                    "first item must be explicit");
  PRUint32 i = 0;
  do {
    const nsTransition *transition = &display->mTransitions[i];
    nsROCSSPrimitiveValue* timingFunction = GetROCSSPrimitiveValue();
    if (!timingFunction || !valueList->AppendCSSValue(timingFunction)) {
      delete valueList;
      delete timingFunction;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // set the value from the cubic-bezier control points
    // (We could try to regenerate the keywords if we want.)
    const nsTimingFunction& tf = transition->GetTimingFunction();
    timingFunction->SetString(
      nsPrintfCString(64, "cubic-bezier(%f, %f, %f, %f)",
                          tf.mX1, tf.mY1, tf.mX2, tf.mY2));
  } while (++i < display->mTransitionTimingFunctionCount);

  NS_ADDREF(*aValue = valueList);
  return NS_OK;
}

#define COMPUTED_STYLE_MAP_ENTRY(_prop, _method)              \
  { eCSSProperty_##_prop, &nsComputedDOMStyle::DoGet##_method, PR_FALSE }
#define COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_prop, _method)       \
  { eCSSProperty_##_prop, &nsComputedDOMStyle::DoGet##_method, PR_TRUE }

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
    COMPUTED_STYLE_MAP_ENTRY(background_position,           BackgroundPosition),
    COMPUTED_STYLE_MAP_ENTRY(background_repeat,             BackgroundRepeat),
    //// COMPUTED_STYLE_MAP_ENTRY(border,                   Border),
    //// COMPUTED_STYLE_MAP_ENTRY(border_bottom,            BorderBottom),
    COMPUTED_STYLE_MAP_ENTRY(border_bottom_color,           BorderBottomColor),
    COMPUTED_STYLE_MAP_ENTRY(border_bottom_style,           BorderBottomStyle),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(border_bottom_width,    BorderBottomWidth),
    COMPUTED_STYLE_MAP_ENTRY(border_collapse,               BorderCollapse),
    //// COMPUTED_STYLE_MAP_ENTRY(border_color,             BorderColor),
    //// COMPUTED_STYLE_MAP_ENTRY(border_left,              BorderLeft),
    COMPUTED_STYLE_MAP_ENTRY(border_left_color,             BorderLeftColor),
    COMPUTED_STYLE_MAP_ENTRY(border_left_style,             BorderLeftStyle),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(border_left_width,      BorderLeftWidth),
    //// COMPUTED_STYLE_MAP_ENTRY(border_right,             BorderRight),
    COMPUTED_STYLE_MAP_ENTRY(border_right_color,            BorderRightColor),
    COMPUTED_STYLE_MAP_ENTRY(border_right_style,            BorderRightStyle),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(border_right_width,     BorderRightWidth),
    COMPUTED_STYLE_MAP_ENTRY(border_spacing,                BorderSpacing),
    //// COMPUTED_STYLE_MAP_ENTRY(border_style,             BorderStyle),
    //// COMPUTED_STYLE_MAP_ENTRY(border_top,               BorderTop),
    COMPUTED_STYLE_MAP_ENTRY(border_top_color,              BorderTopColor),
    COMPUTED_STYLE_MAP_ENTRY(border_top_style,              BorderTopStyle),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(border_top_width,       BorderTopWidth),
    //// COMPUTED_STYLE_MAP_ENTRY(border_width,             BorderWidth),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(bottom,                 Bottom),
    COMPUTED_STYLE_MAP_ENTRY(caption_side,                  CaptionSide),
    COMPUTED_STYLE_MAP_ENTRY(clear,                         Clear),
    COMPUTED_STYLE_MAP_ENTRY(clip,                          Clip),
    COMPUTED_STYLE_MAP_ENTRY(color,                         Color),
    COMPUTED_STYLE_MAP_ENTRY(content,                       Content),
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
    COMPUTED_STYLE_MAP_ENTRY(font_stretch,                  FontStretch),
    COMPUTED_STYLE_MAP_ENTRY(font_style,                    FontStyle),
    COMPUTED_STYLE_MAP_ENTRY(font_variant,                  FontVariant),
    COMPUTED_STYLE_MAP_ENTRY(font_weight,                   FontWeight),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(height,                 Height),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(left,                   Left),
    COMPUTED_STYLE_MAP_ENTRY(letter_spacing,                LetterSpacing),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(line_height,            LineHeight),
    //// COMPUTED_STYLE_MAP_ENTRY(list_style,               ListStyle),
    COMPUTED_STYLE_MAP_ENTRY(list_style_image,              ListStyleImage),
    COMPUTED_STYLE_MAP_ENTRY(list_style_position,           ListStylePosition),
    COMPUTED_STYLE_MAP_ENTRY(list_style_type,               ListStyleType),
    //// COMPUTED_STYLE_MAP_ENTRY(margin,                   Margin),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(margin_bottom,          MarginBottomWidth),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(margin_left,            MarginLeftWidth),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(margin_right,           MarginRightWidth),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(margin_top,             MarginTopWidth),
    COMPUTED_STYLE_MAP_ENTRY(marker_offset,                 MarkerOffset),
    // COMPUTED_STYLE_MAP_ENTRY(marks,                      Marks),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(max_height,             MaxHeight),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(max_width,              MaxWidth),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(min_height,             MinHeight),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(min_width,              MinWidth),
    COMPUTED_STYLE_MAP_ENTRY(ime_mode,                      IMEMode),
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
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(padding_bottom,         PaddingBottom),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(padding_left,           PaddingLeft),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(padding_right,          PaddingRight),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(padding_top,            PaddingTop),
    // COMPUTED_STYLE_MAP_ENTRY(page,                       Page),
    COMPUTED_STYLE_MAP_ENTRY(page_break_after,              PageBreakAfter),
    COMPUTED_STYLE_MAP_ENTRY(page_break_before,             PageBreakBefore),
    // COMPUTED_STYLE_MAP_ENTRY(page_break_inside,          PageBreakInside),
    //// COMPUTED_STYLE_MAP_ENTRY(pause,                    Pause),
    // COMPUTED_STYLE_MAP_ENTRY(pause_after,                PauseAfter),
    // COMPUTED_STYLE_MAP_ENTRY(pause_before,               PauseBefore),
    // COMPUTED_STYLE_MAP_ENTRY(pitch,                      Pitch),
    // COMPUTED_STYLE_MAP_ENTRY(pitch_range,                PitchRange),
    COMPUTED_STYLE_MAP_ENTRY(pointer_events,                PointerEvents),
    COMPUTED_STYLE_MAP_ENTRY(position,                      Position),
    COMPUTED_STYLE_MAP_ENTRY(quotes,                        Quotes),
    // COMPUTED_STYLE_MAP_ENTRY(richness,                   Richness),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(right,                  Right),
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
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(text_indent,            TextIndent),
    COMPUTED_STYLE_MAP_ENTRY(text_shadow,                   TextShadow),
    COMPUTED_STYLE_MAP_ENTRY(text_transform,                TextTransform),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(top,                    Top),
    COMPUTED_STYLE_MAP_ENTRY(unicode_bidi,                  UnicodeBidi),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(vertical_align,         VerticalAlign),
    COMPUTED_STYLE_MAP_ENTRY(visibility,                    Visibility),
    // COMPUTED_STYLE_MAP_ENTRY(voice_family,               VoiceFamily),
    // COMPUTED_STYLE_MAP_ENTRY(volume,                     Volume),
    COMPUTED_STYLE_MAP_ENTRY(white_space,                   WhiteSpace),
    // COMPUTED_STYLE_MAP_ENTRY(widows,                     Widows),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(width,                  Width),
    COMPUTED_STYLE_MAP_ENTRY(word_spacing,                  WordSpacing),
    COMPUTED_STYLE_MAP_ENTRY(z_index,                       ZIndex),

    /* ******************************* *\
     * Implementations of -moz- styles *
    \* ******************************* */

    COMPUTED_STYLE_MAP_ENTRY(appearance,                    Appearance),
    COMPUTED_STYLE_MAP_ENTRY(background_clip,               BackgroundClip),
    COMPUTED_STYLE_MAP_ENTRY(_moz_background_inline_policy, BackgroundInlinePolicy),
    COMPUTED_STYLE_MAP_ENTRY(background_origin,             BackgroundOrigin),
    COMPUTED_STYLE_MAP_ENTRY(background_size,               MozBackgroundSize),
    COMPUTED_STYLE_MAP_ENTRY(binding,                       Binding),
    COMPUTED_STYLE_MAP_ENTRY(border_bottom_colors,          BorderBottomColors),
    COMPUTED_STYLE_MAP_ENTRY(border_image,                  BorderImage),
    COMPUTED_STYLE_MAP_ENTRY(border_left_colors,            BorderLeftColors),
    COMPUTED_STYLE_MAP_ENTRY(border_right_colors,           BorderRightColors),
    COMPUTED_STYLE_MAP_ENTRY(border_top_colors,             BorderTopColors),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(border_bottom_left_radius, BorderBottomLeftRadius),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(border_bottom_right_radius,BorderBottomRightRadius),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(border_top_left_radius,    BorderTopLeftRadius),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(border_top_right_radius,   BorderTopRightRadius),
    COMPUTED_STYLE_MAP_ENTRY(box_align,                     BoxAlign),
    COMPUTED_STYLE_MAP_ENTRY(box_direction,                 BoxDirection),
    COMPUTED_STYLE_MAP_ENTRY(box_flex,                      BoxFlex),
    COMPUTED_STYLE_MAP_ENTRY(box_ordinal_group,             BoxOrdinalGroup),
    COMPUTED_STYLE_MAP_ENTRY(box_orient,                    BoxOrient),
    COMPUTED_STYLE_MAP_ENTRY(box_pack,                      BoxPack),
    COMPUTED_STYLE_MAP_ENTRY(box_shadow,                    BoxShadow),
    COMPUTED_STYLE_MAP_ENTRY(box_sizing,                    BoxSizing),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_count,             ColumnCount),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_width,             ColumnWidth),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_gap,               ColumnGap),
    //// COMPUTED_STYLE_MAP_ENTRY(_moz_column_rule,         ColumnRule),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_rule_color,        ColumnRuleColor),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_rule_width,        ColumnRuleWidth),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_rule_style,        ColumnRuleStyle),
    COMPUTED_STYLE_MAP_ENTRY(float_edge,                    FloatEdge),
    COMPUTED_STYLE_MAP_ENTRY(font_feature_settings,         MozFontFeatureSettings),
    COMPUTED_STYLE_MAP_ENTRY(font_language_override,        MozFontLanguageOverride),
    COMPUTED_STYLE_MAP_ENTRY(force_broken_image_icon,  ForceBrokenImageIcon),
    COMPUTED_STYLE_MAP_ENTRY(image_region,                  ImageRegion),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_outline_radius_bottomLeft, OutlineRadiusBottomLeft),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_outline_radius_bottomRight,OutlineRadiusBottomRight),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_outline_radius_topLeft,    OutlineRadiusTopLeft),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_outline_radius_topRight,   OutlineRadiusTopRight),
    COMPUTED_STYLE_MAP_ENTRY(resize,                        Resize),
    COMPUTED_STYLE_MAP_ENTRY(stack_sizing,                  StackSizing),
    COMPUTED_STYLE_MAP_ENTRY(_moz_tab_size,                 MozTabSize),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_transform,         MozTransform),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_transform_origin,  MozTransformOrigin),
    COMPUTED_STYLE_MAP_ENTRY(user_focus,                    UserFocus),
    COMPUTED_STYLE_MAP_ENTRY(user_input,                    UserInput),
    COMPUTED_STYLE_MAP_ENTRY(user_modify,                   UserModify),
    COMPUTED_STYLE_MAP_ENTRY(user_select,                   UserSelect),
    COMPUTED_STYLE_MAP_ENTRY(transition_delay,              TransitionDelay),
    COMPUTED_STYLE_MAP_ENTRY(transition_duration,           TransitionDuration),
    COMPUTED_STYLE_MAP_ENTRY(transition_property,           TransitionProperty),
    COMPUTED_STYLE_MAP_ENTRY(transition_timing_function,    TransitionTimingFunction),
    COMPUTED_STYLE_MAP_ENTRY(_moz_window_shadow,            WindowShadow),
    COMPUTED_STYLE_MAP_ENTRY(word_wrap,                     WordWrap),
    COMPUTED_STYLE_MAP_ENTRY(clip_path,                     ClipPath),
    COMPUTED_STYLE_MAP_ENTRY(clip_rule,                     ClipRule),
    COMPUTED_STYLE_MAP_ENTRY(color_interpolation,           ColorInterpolation),
    COMPUTED_STYLE_MAP_ENTRY(color_interpolation_filters,   ColorInterpolationFilters),
    COMPUTED_STYLE_MAP_ENTRY(dominant_baseline,             DominantBaseline),
    COMPUTED_STYLE_MAP_ENTRY(fill,                          Fill),
    COMPUTED_STYLE_MAP_ENTRY(fill_opacity,                  FillOpacity),
    COMPUTED_STYLE_MAP_ENTRY(fill_rule,                     FillRule),
    COMPUTED_STYLE_MAP_ENTRY(filter,                        Filter),
    COMPUTED_STYLE_MAP_ENTRY(flood_color,                   FloodColor),
    COMPUTED_STYLE_MAP_ENTRY(flood_opacity,                 FloodOpacity),
    COMPUTED_STYLE_MAP_ENTRY(lighting_color,                LightingColor),
    COMPUTED_STYLE_MAP_ENTRY(image_rendering,               ImageRendering),
    COMPUTED_STYLE_MAP_ENTRY(mask,                          Mask),
    COMPUTED_STYLE_MAP_ENTRY(marker_end,                    MarkerEnd),
    COMPUTED_STYLE_MAP_ENTRY(marker_mid,                    MarkerMid),
    COMPUTED_STYLE_MAP_ENTRY(marker_start,                  MarkerStart),
    COMPUTED_STYLE_MAP_ENTRY(shape_rendering,               ShapeRendering),
    COMPUTED_STYLE_MAP_ENTRY(stop_color,                    StopColor),
    COMPUTED_STYLE_MAP_ENTRY(stop_opacity,                  StopOpacity),
    COMPUTED_STYLE_MAP_ENTRY(stroke,                        Stroke),
    COMPUTED_STYLE_MAP_ENTRY(stroke_dasharray,              StrokeDasharray),
    COMPUTED_STYLE_MAP_ENTRY(stroke_dashoffset,             StrokeDashoffset),
    COMPUTED_STYLE_MAP_ENTRY(stroke_linecap,                StrokeLinecap),
    COMPUTED_STYLE_MAP_ENTRY(stroke_linejoin,               StrokeLinejoin),
    COMPUTED_STYLE_MAP_ENTRY(stroke_miterlimit,             StrokeMiterlimit),
    COMPUTED_STYLE_MAP_ENTRY(stroke_opacity,                StrokeOpacity),
    COMPUTED_STYLE_MAP_ENTRY(stroke_width,                  StrokeWidth),
    COMPUTED_STYLE_MAP_ENTRY(text_anchor,                   TextAnchor),
    COMPUTED_STYLE_MAP_ENTRY(text_rendering,                TextRendering)

  };

  *aLength = NS_ARRAY_LENGTH(map);

  return map;
}

