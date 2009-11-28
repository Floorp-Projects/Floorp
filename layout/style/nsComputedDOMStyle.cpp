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
#include "nsIDOMNSCSS2Properties.h"
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
    mInnerFrame(nsnull), mPresShell(nsnull), mAppUnitsPerInch(0)
{
}


nsComputedDOMStyle::~nsComputedDOMStyle()
{
  ClearWrapper();
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


NS_IMPL_CYCLE_COLLECTION_CLASS(nsComputedDOMStyle)
NS_IMPL_CYCLE_COLLECTION_ROOT_BEGIN(nsComputedDOMStyle)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_ROOT_END
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsComputedDOMStyle)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mContent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsComputedDOMStyle)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsComputedDOMStyle)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mContent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// QueryInterface implementation for nsComputedDOMStyle
NS_INTERFACE_TABLE_HEAD(nsComputedDOMStyle)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_OFFSET_AND_INTERFACE_TABLE_BEGIN(nsComputedDOMStyle)
    NS_INTERFACE_TABLE_ENTRY(nsComputedDOMStyle, nsICSSDeclaration)
    NS_INTERFACE_TABLE_ENTRY(nsComputedDOMStyle,
                             nsIDOMCSSStyleDeclaration)
    NS_INTERFACE_TABLE_ENTRY(nsComputedDOMStyle, nsISupports)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsComputedDOMStyle)
  NS_INTERFACE_MAP_ENTRY_AGGREGATED(nsIDOMCSS2Properties,
                                    new CSS2PropertiesTearoff(this))
  NS_INTERFACE_MAP_ENTRY_AGGREGATED(nsIDOMNSCSS2Properties,
                                    new CSS2PropertiesTearoff(this))
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

  mAppUnitsPerInch = presCtx->AppUnitsPerInch();

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

static nsStyleContext*
GetStyleContextForFrame(nsIFrame* aFrame)
{
  nsStyleContext* styleContext = aFrame->GetStyleContext();

  /* For tables the primary frame is the "outer frame" but the style
   * rules are applied to the "inner frame".  Luckily, the "outer
   * frame" actually inherits style from the "inner frame" so we can
   * just move one level up in the style context hierarchy....
   */
  if (aFrame->GetType() == nsGkAtoms::tableOuterFrame)
    return styleContext->GetParent();

  return styleContext;
}    

/* static */
already_AddRefed<nsStyleContext>
nsComputedDOMStyle::GetStyleContextForContent(nsIContent* aContent,
                                              nsIAtom* aPseudo,
                                              nsIPresShell* aPresShell)
{
  NS_ASSERTION(aContent->IsNodeOfType(nsINode::eELEMENT),
               "aContent must be an element");
  if (!aPseudo) {
    // If there's no pres shell, get it from the content
    if (!aPresShell) {
      aPresShell = GetPresShellForContent(aContent);
      if (!aPresShell)
        return nsnull;
    }

    aPresShell->FlushPendingNotifications(Flush_Style);
  }

  return GetStyleContextForContentNoFlush(aContent, aPseudo, aPresShell);
}

/* static */
already_AddRefed<nsStyleContext>
nsComputedDOMStyle::GetStyleContextForContentNoFlush(nsIContent* aContent,
                                                     nsIAtom* aPseudo,
                                                     nsIPresShell* aPresShell)
{
  NS_ABORT_IF_FALSE(aContent, "NULL content node");

  // If there's no pres shell, get it from the content
  if (!aPresShell) {
    aPresShell = GetPresShellForContent(aContent);
    if (!aPresShell)
      return nsnull;
  }

  if (!aPseudo) {
    nsIFrame* frame = aPresShell->GetPrimaryFrameFor(aContent);
    if (frame) {
      nsStyleContext* result = GetStyleContextForFrame(frame);
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
  nsIContent* parent = aPseudo ? aContent : aContent->GetParent();
  // Don't resolve parent context for document fragments.
  if (parent && parent->IsNodeOfType(nsINode::eELEMENT))
    parentContext = GetStyleContextForContent(parent, nsnull, aPresShell);

  nsPresContext *presContext = aPresShell->GetPresContext();
  if (!presContext)
    return nsnull;

  nsStyleSet *styleSet = aPresShell->StyleSet();

  if (aPseudo) {
    return styleSet->ResolvePseudoStyleFor(aContent, aPseudo, parentContext);
  }

  return styleSet->ResolveStyleFor(aContent, parentContext);
}

/* static */
nsIPresShell*
nsComputedDOMStyle::GetPresShellForContent(nsIContent* aContent)
{
  nsIDocument* currentDoc = aContent->GetCurrentDoc();
  if (!currentDoc)
    return nsnull;

  return currentDoc->GetPrimaryShell();
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

  mPresShell = document->GetPrimaryShell();
  NS_ENSURE_TRUE(mPresShell && mPresShell->GetPresContext(),
                 NS_ERROR_NOT_AVAILABLE);

  mOuterFrame = mPresShell->GetPrimaryFrameFor(mContent);
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
      nsComputedDOMStyle::GetStyleContextForContent(mContent,
                                                    mPseudo,
                                                    mPresShell);
    NS_ENSURE_TRUE(mStyleContextHolder, NS_ERROR_OUT_OF_MEMORY);
    NS_ASSERTION(mPseudo || !mStyleContextHolder->HasPseudoElementData(),
                 "should not have pseudo-element data");
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
nsComputedDOMStyle::GetBinding(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display = GetStyleDisplay();

  if (display->mBinding) {
    val->SetURI(display->mBinding->mURI);
  } else {
    val->SetIdent(eCSSKeyword_none);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetClear(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleDisplay()->mBreakType,
                                               nsCSSProps::kClearKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetCssFloat(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleDisplay()->mFloats,
                                               nsCSSProps::kFloatKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBottom(nsIDOMCSSValue** aValue)
{
  return GetOffsetWidthFor(NS_SIDE_BOTTOM, aValue);
}

nsresult
nsComputedDOMStyle::GetStackSizing(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(GetStyleXUL()->mStretchStack ? eCSSKeyword_stretch_to_fit :
                eCSSKeyword_ignore);

  return CallQueryInterface(val, aValue);
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
    val->SetIdent(eCSSKeyword_auto);
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

  // XXX fix the auto case. When we actually have a column frame, I think
  // we should return the computed column width.
  SetValueToCoord(val, GetStyleColumn()->mColumnWidth);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetColumnGap(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleColumn* column = GetStyleColumn();
  if (column->mColumnGap.GetUnit() == eStyleUnit_Normal) {
    val->SetAppUnits(GetStyleFont()->mFont.size);
  } else {
    SetValueToCoord(val, GetStyleColumn()->mColumnGap);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetColumnRuleWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  if (!val)
    return NS_ERROR_OUT_OF_MEMORY;

  val->SetAppUnits(GetStyleColumn()->GetComputedColumnRuleWidth());
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetColumnRuleStyle(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  if (!val)
    return NS_ERROR_OUT_OF_MEMORY;
  
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleColumn()->mColumnRuleStyle,
                                   nsCSSProps::kBorderStyleKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetColumnRuleColor(nsIDOMCSSValue** aValue)
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
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetContent(nsIDOMCSSValue** aValue)
{
  const nsStyleContent *content = GetStyleContent();

  if (content->ContentCount() == 0) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
    val->SetIdent(eCSSKeyword_none);
    return CallQueryInterface(val, aValue);
  }

  if (content->ContentCount() == 1 &&
      content->ContentAt(0).mType == eStyleContentType_AltContent) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
    val->SetIdent(eCSSKeyword__moz_alt_content);
    return CallQueryInterface(val, aValue);
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
        val->SetString(nsDependentString(data.mContent.mString),
                       nsIDOMCSSPrimitiveValue::CSS_ATTR);
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

          str.Append(a->Item(0).GetStringBufferValue());
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

  return CallQueryInterface(valueList, aValue);
}

nsresult
nsComputedDOMStyle::GetCounterIncrement(nsIDOMCSSValue** aValue)
{
  const nsStyleContent *content = GetStyleContent();

  if (content->CounterIncrementCount() == 0) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
    val->SetIdent(eCSSKeyword_none);
    return CallQueryInterface(val, aValue);
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
    name->SetString(data->mCounter);
    value->SetNumber(data->mValue); // XXX This should really be integer
  }

  return CallQueryInterface(valueList, aValue);
}

/* Convert the stored representation into a list of two values and then hand
 * it back.
 */
nsresult nsComputedDOMStyle::GetMozTransformOrigin(nsIDOMCSSValue **aValue)
{
  /* We need to build up a list of two values.  We'll call them
   * width and height.
   */
  nsAutoPtr<nsROCSSPrimitiveValue> width(GetROCSSPrimitiveValue());
  nsAutoPtr<nsROCSSPrimitiveValue> height(GetROCSSPrimitiveValue());
  if (!width || !height)
    return NS_ERROR_OUT_OF_MEMORY;

  /* Now, get the values. */
  const nsStyleDisplay* display = GetStyleDisplay();
  SetValueToCoord(width, display->mTransformOrigin[0],
                  &nsComputedDOMStyle::GetFrameBoundsWidthForTransform);
  SetValueToCoord(height, display->mTransformOrigin[1],
                  &nsComputedDOMStyle::GetFrameBoundsHeightForTransform);

  /* Store things as a value list, fail if we can't get one. */
  nsAutoPtr<nsDOMCSSValueList> valueList(GetROCSSValueList(PR_FALSE));
  if (!valueList)
    return NS_ERROR_OUT_OF_MEMORY;

  /* Chain on width and height, fail if we can't. */
  if (!valueList->AppendCSSValue(width))
    return NS_ERROR_OUT_OF_MEMORY;
  width.forget();
  if (!valueList->AppendCSSValue(height))
    return NS_ERROR_OUT_OF_MEMORY;
  height.forget();

  /* Release the pointer and call query interface!  We're done. */
  return CallQueryInterface(valueList.forget(), aValue);
}

/* If the property is "none", hand back "none" wrapped in a value.
 * Otherwise, compute the aggregate transform matrix and hands it back in a
 * "matrix" wrapper.
 */
nsresult nsComputedDOMStyle::GetMozTransform(nsIDOMCSSValue **aValue)
{
  static const PRInt32 NUM_FLOATS = 4;
  
  /* First, get the display data.  We'll need it. */
  const nsStyleDisplay* display = GetStyleDisplay();
  
  /* If the "no transforms" flag is set, then we should construct a
   * single-element entry and hand it back.
   */
  if (!display->mTransformPresent) {
    nsROCSSPrimitiveValue *val(GetROCSSPrimitiveValue());
    if (!val)
      return NS_ERROR_OUT_OF_MEMORY;
    
    /* Set it to "none." */
    val->SetIdent(eCSSKeyword_none);
    return CallQueryInterface(val, aValue);
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
  nsROCSSPrimitiveValue* rv(GetROCSSPrimitiveValue());

  if (!rv)
    return NS_ERROR_OUT_OF_MEMORY;

  rv->SetString(resultString);
  return CallQueryInterface(rv, aValue);
}

nsresult
nsComputedDOMStyle::GetCounterReset(nsIDOMCSSValue** aValue)
{
  const nsStyleContent *content = GetStyleContent();

  if (content->CounterResetCount() == 0) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
    val->SetIdent(eCSSKeyword_none);
    return CallQueryInterface(val, aValue);
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
    name->SetString(data->mCounter);
    value->SetNumber(data->mValue); // XXX This should really be integer
  }

  return CallQueryInterface(valueList, aValue);
}

nsresult
nsComputedDOMStyle::GetQuotes(nsIDOMCSSValue** aValue)
{
  const nsStyleQuotes *quotes = GetStyleQuotes();

  if (quotes->QuotesCount() == 0) {
    nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
    val->SetIdent(eCSSKeyword_none);
    return CallQueryInterface(val, aValue);
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
  nsIPresShell* presShell = doc->GetPrimaryShell();
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

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontSize(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  // Note: GetStyleFont()->mSize is the 'computed size';
  // GetStyleFont()->mFont.size is the 'actual size'
  val->SetAppUnits(GetStyleFont()->mSize);

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
    val->SetIdent(eCSSKeyword_none);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontStretch(nsIDOMCSSValue** aValue)
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

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontStyle(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleFont()->mFont.style,
                                               nsCSSProps::kFontStyleKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontWeight(nsIDOMCSSValue** aValue)
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

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFontVariant(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleFont()->mFont.variant,
                                   nsCSSProps::kFontVariantKTable));

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

  return CallQueryInterface(valueList, aResult);
}

nsresult
nsComputedDOMStyle::GetBackgroundAttachment(nsIDOMCSSValue** aValue)
{
  return GetBackgroundList(&nsStyleBackground::Layer::mAttachment,
                           &nsStyleBackground::mAttachmentCount,
                           nsCSSProps::kBackgroundAttachmentKTable,
                           aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundClip(nsIDOMCSSValue** aValue)
{
  return GetBackgroundList(&nsStyleBackground::Layer::mClip,
                           &nsStyleBackground::mClipCount,
                           nsCSSProps::kBackgroundClipKTable,
                           aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundColor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleBackground* color = GetStyleBackground();
  nsresult rv = SetToRGBAColor(val, color->mBackgroundColor);
  if (NS_FAILED(rv)) {
    delete val;
    return rv;
  }

  return CallQueryInterface(val, aValue);
}

static void
AppendCSSGradientLength(const nsStyleCoord& aValue,
                        nsROCSSPrimitiveValue* aPrimitive,
                        nsAString& aString)
{
  nsAutoString tokenString;
  if (aValue.GetUnit() == eStyleUnit_Coord)
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
    SetValueToCoord(valSide, aCropRect.Get(side));
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
nsComputedDOMStyle::GetBackgroundImage(nsIDOMCSSValue** aValue)
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

  return CallQueryInterface(valueList, aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundInlinePolicy(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(
                  GetStyleBackground()->mBackgroundInlinePolicy,
                  nsCSSProps::kBackgroundInlinePolicyKTable));

  return CallQueryInterface(val, aValue);  
}

nsresult
nsComputedDOMStyle::GetBackgroundOrigin(nsIDOMCSSValue** aValue)
{
  return GetBackgroundList(&nsStyleBackground::Layer::mOrigin,
                           &nsStyleBackground::mOriginCount,
                           nsCSSProps::kBackgroundOriginKTable,
                           aValue);
}

nsresult
nsComputedDOMStyle::GetBackgroundPosition(nsIDOMCSSValue** aValue)
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

    if (pos.mXIsPercent) {
      valX->SetPercent(pos.mXPosition.mFloat);
    } else {
      valX->SetAppUnits(pos.mXPosition.mCoord);
    }

    if (pos.mYIsPercent) {
      valY->SetPercent(pos.mYPosition.mFloat);
    } else {
      valY->SetAppUnits(pos.mYPosition.mCoord);
    }
  }

  return CallQueryInterface(valueList, aValue);  
}

nsresult
nsComputedDOMStyle::GetBackgroundRepeat(nsIDOMCSSValue** aValue)
{
  return GetBackgroundList(&nsStyleBackground::Layer::mRepeat,
                           &nsStyleBackground::mRepeatCount,
                           nsCSSProps::kBackgroundRepeatKTable,
                           aValue);
}

nsresult
nsComputedDOMStyle::GetMozBackgroundSize(nsIDOMCSSValue** aValue)
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
        } else if (size.mWidthType == nsStyleBackground::Size::ePercentage) {
          valX->SetPercent(size.mWidth.mFloat);
        } else {
          NS_ABORT_IF_FALSE(size.mWidthType == nsStyleBackground::Size::eLength,
                            "bad mWidthType");
          valX->SetAppUnits(size.mWidth.mCoord);
        }

        if (size.mHeightType == nsStyleBackground::Size::eAuto) {
          valY->SetIdent(eCSSKeyword_auto);
        } else if (size.mHeightType == nsStyleBackground::Size::ePercentage) {
          valY->SetPercent(size.mHeight.mFloat);
        } else {
          NS_ABORT_IF_FALSE(size.mHeightType == nsStyleBackground::Size::eLength,
                            "bad mHeightType");
          valY->SetAppUnits(size.mHeight.mCoord);
        }
        break;
      }
    }
  }

  return CallQueryInterface(valueList, aValue);
}

nsresult
nsComputedDOMStyle::GetPadding(nsIDOMCSSValue** aValue)
{
  // return null per spec.
  *aValue = nsnull;

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

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleTableBorder()->mBorderCollapse,
                                   nsCSSProps::kBorderCollapseKTable));

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
  xSpacing->SetAppUnits(border->mBorderSpacingX);
  ySpacing->SetAppUnits(border->mBorderSpacingY);

  return CallQueryInterface(valueList, aValue);
}

nsresult
nsComputedDOMStyle::GetCaptionSide(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleTableBorder()->mCaptionSide,
                                   nsCSSProps::kCaptionSideKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetEmptyCells(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleTableBorder()->mEmptyCells,
                                   nsCSSProps::kEmptyCellsKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTableLayout(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleTable()->mLayoutStrategy,
                                   nsCSSProps::kTableLayoutKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderStyle(nsIDOMCSSValue** aValue)
{
  // return null per spec.
  *aValue = nsnull;

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
  return GetEllipseRadii(GetStyleBorder()->mBorderRadius,
                         NS_CORNER_BOTTOM_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRadiusBottomRight(nsIDOMCSSValue** aValue)
{
  return GetEllipseRadii(GetStyleBorder()->mBorderRadius,
                         NS_CORNER_BOTTOM_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRadiusTopLeft(nsIDOMCSSValue** aValue)
{
  return GetEllipseRadii(GetStyleBorder()->mBorderRadius,
                         NS_CORNER_TOP_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderRadiusTopRight(nsIDOMCSSValue** aValue)
{
  return GetEllipseRadii(GetStyleBorder()->mBorderRadius,
                         NS_CORNER_TOP_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderWidth(nsIDOMCSSValue** aValue)
{
  // return null per spec.
  *aValue = nsnull;

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
  *aValue = nsnull;

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

  SetValueToCoord(val, GetStyleContent()->mMarkerOffset);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOutline(nsIDOMCSSValue** aValue)
{
  // return null per spec.
  *aValue = nsnull;

  return NS_OK;
}

nsresult
nsComputedDOMStyle::GetOutlineWidth(nsIDOMCSSValue** aValue)
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

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineStyle(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleOutline()->GetOutlineStyle(),
                                   nsCSSProps::kOutlineStyleKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineOffset(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetAppUnits(GetStyleOutline()->mOutlineOffset);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineRadiusBottomLeft(nsIDOMCSSValue** aValue)
{
  return GetEllipseRadii(GetStyleOutline()->mOutlineRadius,
                         NS_CORNER_BOTTOM_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineRadiusBottomRight(nsIDOMCSSValue** aValue)
{
  return GetEllipseRadii(GetStyleOutline()->mOutlineRadius,
                         NS_CORNER_BOTTOM_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineRadiusTopLeft(nsIDOMCSSValue** aValue)
{
  return GetEllipseRadii(GetStyleOutline()->mOutlineRadius,
                         NS_CORNER_TOP_LEFT, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineRadiusTopRight(nsIDOMCSSValue** aValue)
{
  return GetEllipseRadii(GetStyleOutline()->mOutlineRadius,
                         NS_CORNER_TOP_RIGHT, aValue);
}

nsresult
nsComputedDOMStyle::GetOutlineColor(nsIDOMCSSValue** aValue)
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

  return CallQueryInterface(val, aValue);
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

    SetValueToCoord(val, radiusX,
                    &nsComputedDOMStyle::GetFrameBorderRectWidth);

    return CallQueryInterface(val, aValue);
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

    SetValueToCoord(valX, radiusX,
                    &nsComputedDOMStyle::GetFrameBorderRectWidth);
    SetValueToCoord(valY, radiusY,
                    &nsComputedDOMStyle::GetFrameBorderRectWidth);

    return CallQueryInterface(valueList, aValue);
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
    return CallQueryInterface(val, aValue);
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

  return CallQueryInterface(valueList, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxShadow(nsIDOMCSSValue** aValue)
{
  return GetCSSShadowArray(GetStyleBorder()->mBoxShadow,
                           GetStyleColor()->mColor,
                           PR_TRUE, aValue);
}

nsresult
nsComputedDOMStyle::GetZIndex(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStylePosition()->mZIndex);
    
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetListStyleImage(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleList* list = GetStyleList();

  if (!list->mListStyleImage) {
    val->SetIdent(eCSSKeyword_none);
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

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleList()->mListStylePosition,
                                   nsCSSProps::kListStylePositionKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetListStyleType(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleList()->mListStyleType,
                                   nsCSSProps::kListStyleKTable));

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
  
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetLineHeight(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nscoord lineHeight;
  if (GetLineHeightCoord(lineHeight)) {
    val->SetAppUnits(lineHeight);
  } else {
    SetValueToCoord(val, GetStyleText()->mLineHeight,
                    nsnull, nsCSSProps::kLineHeightKTable);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetVerticalAlign(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStyleTextReset()->mVerticalAlign,
                  &nsComputedDOMStyle::GetLineHeightCoord,
                  nsCSSProps::kVerticalAlignKTable);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextAlign(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleText()->mTextAlign,
                                   nsCSSProps::kTextAlignKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextDecoration(nsIDOMCSSValue** aValue)
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

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextIndent(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStyleText()->mTextIndent,
                  &nsComputedDOMStyle::GetCBContentWidth);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextShadow(nsIDOMCSSValue** aValue)
{
  return GetCSSShadowArray(GetStyleText()->mTextShadow,
                           GetStyleColor()->mColor,
                           PR_FALSE, aValue);
}

nsresult
nsComputedDOMStyle::GetTextTransform(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleText()->mTextTransform,
                                   nsCSSProps::kTextTransformKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMozTabSize(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleText()->mTabSize);
    
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetLetterSpacing(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStyleText()->mLetterSpacing);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetWordSpacing(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetAppUnits(GetStyleText()->mWordSpacing);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetWhiteSpace(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleText()->mWhiteSpace,
                                   nsCSSProps::kWhitespaceKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetWindowShadow(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleUIReset()->mWindowShadow,
                                   nsCSSProps::kWindowShadowKTable));

  return CallQueryInterface(val, aValue);
}


nsresult
nsComputedDOMStyle::GetWordWrap(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleText()->mWordWrap,
                                   nsCSSProps::kWordwrapKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetPointerEvents(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
  
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleVisibility()->mPointerEvents,
                                   nsCSSProps::kPointerEventsKTable));
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetVisibility(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleVisibility()->mVisible,
                                               nsCSSProps::kVisibilityKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetDirection(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleVisibility()->mDirection,
                                   nsCSSProps::kDirectionKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetUnicodeBidi(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleTextReset()->mUnicodeBidi,
                                   nsCSSProps::kUnicodeBidiKTable));

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

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(ui->mCursor,
                                               nsCSSProps::kCursorKTable));
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

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleDisplay()->mAppearance,
                                               nsCSSProps::kAppearanceKTable));

  return CallQueryInterface(val, aValue);
}


nsresult
nsComputedDOMStyle::GetBoxAlign(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleXUL()->mBoxAlign,
                                               nsCSSProps::kBoxAlignKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxDirection(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleXUL()->mBoxDirection,
                                   nsCSSProps::kBoxDirectionKTable));

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

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleXUL()->mBoxOrient,
                                   nsCSSProps::kBoxOrientKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxPack(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleXUL()->mBoxPack,
                                               nsCSSProps::kBoxPackKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBoxSizing(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStylePosition()->mBoxSizing,
                                   nsCSSProps::kBoxSizingKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderImage(nsIDOMCSSValue** aValue)
{
  const nsStyleBorder* border = GetStyleBorder();
  
  // none
  if (!border->GetBorderImage()) {
    nsROCSSPrimitiveValue *valNone = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(valNone, NS_ERROR_OUT_OF_MEMORY);
    valNone->SetIdent(eCSSKeyword_none);
    return CallQueryInterface(valNone, aValue);
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
    SetValueToCoord(valSplit, border->mBorderImageSplit.Get(side), nsnull,
                    nsnull);
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
  
  return CallQueryInterface(valueList, aValue);
}

nsresult
nsComputedDOMStyle::GetFloatEdge(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleBorder()->mFloatEdge,
                                   nsCSSProps::kFloatEdgeKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetForceBrokenImageIcon(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleUIReset()->mForceBrokenImageIcon);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetIMEMode(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleUIReset()->mIMEMode,
                                   nsCSSProps::kIMEModeKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetUserFocus(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleUserInterface()->mUserFocus,
                                   nsCSSProps::kUserFocusKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetUserInput(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleUserInterface()->mUserInput,
                                   nsCSSProps::kUserInputKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetUserModify(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleUserInterface()->mUserModify,
                                   nsCSSProps::kUserModifyKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetUserSelect(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleUIReset()->mUserSelect,
                                   nsCSSProps::kUserSelectKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetDisplay(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleDisplay()->mDisplay,
                                               nsCSSProps::kDisplayKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetPosition(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(GetStyleDisplay()->mPosition,
                                               nsCSSProps::kPositionKTable));

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

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(display->mOverflowX,
                                               nsCSSProps::kOverflowKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOverflowX(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleDisplay()->mOverflowX,
                                   nsCSSProps::kOverflowSubKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetOverflowY(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleDisplay()->mOverflowY,
                                   nsCSSProps::kOverflowSubKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetPageBreakAfter(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay *display = GetStyleDisplay();

  if (display->mBreakAfter) {
    val->SetIdent(eCSSKeyword_always);
  } else {
    val->SetIdent(eCSSKeyword_auto);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetPageBreakBefore(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay *display = GetStyleDisplay();

  if (display->mBreakBefore) {
    val->SetIdent(eCSSKeyword_always);
  } else {
    val->SetIdent(eCSSKeyword_auto);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetHeight(nsIDOMCSSValue** aValue)
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
                          &nsComputedDOMStyle::GetCBContentHeight, 0);

    nscoord maxHeight =
      StyleCoordToNSCoord(positionData->mMaxHeight,
                          &nsComputedDOMStyle::GetCBContentHeight,
                          nscoord_MAX);
    
    SetValueToCoord(val, positionData->mHeight, nsnull, nsnull,
                    minHeight, maxHeight);
  }
  
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetWidth(nsIDOMCSSValue** aValue)
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
                          &nsComputedDOMStyle::GetCBContentWidth, 0);

    nscoord maxWidth =
      StyleCoordToNSCoord(positionData->mMaxWidth,
                          &nsComputedDOMStyle::GetCBContentWidth,
                          nscoord_MAX);
    
    SetValueToCoord(val, positionData->mWidth, nsnull,
                    nsCSSProps::kWidthKTable, minWidth, maxWidth);
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMaxHeight(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStylePosition()->mMaxHeight,
                  &nsComputedDOMStyle::GetCBContentHeight);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMaxWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStylePosition()->mMaxWidth,
                  &nsComputedDOMStyle::GetCBContentWidth,
                  nsCSSProps::kWidthKTable);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMinHeight(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStylePosition()->mMinHeight,
                  &nsComputedDOMStyle::GetCBContentHeight);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMinWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStylePosition()->mMinWidth,
                  &nsComputedDOMStyle::GetCBContentWidth,
                  nsCSSProps::kWidthKTable);

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
  nsROCSSPrimitiveValue *primitiveValue = new nsROCSSPrimitiveValue(mAppUnitsPerInch);

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

  AssertFlushedPendingReflows();

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
  PRInt32 sign = 1;
  nsStyleCoord coord = positionData->mOffset.Get(aSide);

  NS_ASSERTION(coord.GetUnit() == eStyleUnit_Coord ||
               coord.GetUnit() == eStyleUnit_Percent ||
               coord.GetUnit() == eStyleUnit_Auto,
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

  val->SetAppUnits(sign * StyleCoordToNSCoord(coord, baseGetter, 0));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetStaticOffset(PRUint8 aSide, nsIDOMCSSValue** aValue)

{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStylePosition()->mOffset.Get(aSide));
  
  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetPaddingWidthFor(PRUint8 aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  if (!mInnerFrame) {
    SetValueToCoord(val, GetStylePadding()->mPadding.Get(aSide));
  } else {
    AssertFlushedPendingReflows();
  
    val->SetAppUnits(mInnerFrame->GetUsedPadding().side(aSide));
  }

  return CallQueryInterface(val, aValue);
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

      return CallQueryInterface(valueList, aValue);
    }
  }

  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(eCSSKeyword_none);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderWidthFor(PRUint8 aSide, nsIDOMCSSValue** aValue)
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

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderColorFor(PRUint8 aSide, nsIDOMCSSValue** aValue)
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

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMarginWidthFor(PRUint8 aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  if (!mInnerFrame) {
    SetValueToCoord(val, GetStyleMargin()->mMargin.Get(aSide));
  } else {
    AssertFlushedPendingReflows();

    val->SetAppUnits(mInnerFrame->GetUsedMargin().side(aSide));
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetBorderStyleFor(PRUint8 aSide, nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleBorder()->GetBorderStyle(aSide),
                                   nsCSSProps::kBorderStyleKTable));

  return CallQueryInterface(val, aValue);
}

void
nsComputedDOMStyle::SetValueToCoord(nsROCSSPrimitiveValue* aValue,
                                    const nsStyleCoord& aCoord,
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
          nscoord val = nscoord(aCoord.GetPercentValue() * percentageBase);
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
      
    default:
      NS_ERROR("Can't handle this unit");
      break;
  }
}

nscoord
nsComputedDOMStyle::StyleCoordToNSCoord(const nsStyleCoord& aCoord,
                                        PercentageBaseGetter aPercentageBaseGetter,
                                        nscoord aDefaultValue)
{
  NS_PRECONDITION(aPercentageBaseGetter, "Must have a percentage base getter");
  switch (aCoord.GetUnit()) {
    case eStyleUnit_Coord:
      return aCoord.GetCoordValue();
    case eStyleUnit_Percent:
      {
        nscoord percentageBase;
        if ((this->*aPercentageBaseGetter)(percentageBase)) {
          return nscoord(aCoord.GetPercentValue() * percentageBase);
        }
      }
      // Fall through to returning aDefaultValue if we have no percentage base.
    default:
      break;
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

#ifdef MOZ_SVG

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

    return CallQueryInterface(valueList, aValue);
  }
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFill(nsIDOMCSSValue** aValue)
{
  return GetSVGPaintFor(PR_TRUE, aValue);
}

nsresult
nsComputedDOMStyle::GetStroke(nsIDOMCSSValue** aValue)
{
  return GetSVGPaintFor(PR_FALSE, aValue);
}

nsresult
nsComputedDOMStyle::GetMarkerEnd(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleSVG* svg = GetStyleSVG();

  if (svg->mMarkerEnd)
    val->SetURI(svg->mMarkerEnd);
  else
    val->SetIdent(eCSSKeyword_none);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMarkerMid(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleSVG* svg = GetStyleSVG();

  if (svg->mMarkerMid)
    val->SetURI(svg->mMarkerMid);
  else
    val->SetIdent(eCSSKeyword_none);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMarkerStart(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleSVG* svg = GetStyleSVG();

  if (svg->mMarkerStart)
    val->SetURI(svg->mMarkerStart);
  else
    val->SetIdent(eCSSKeyword_none);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetStrokeDasharray(nsIDOMCSSValue** aValue)
{
  const nsStyleSVG* svg = GetStyleSVG();

  if (!svg->mStrokeDasharrayLength || !svg->mStrokeDasharray) {
    nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
    NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);
    val->SetIdent(eCSSKeyword_none);
    return CallQueryInterface(val, aValue);
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

    SetValueToCoord(dash, svg->mStrokeDasharray[i]);
  }

  return CallQueryInterface(valueList, aValue);
}

nsresult
nsComputedDOMStyle::GetStrokeDashoffset(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStyleSVG()->mStrokeDashoffset);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetStrokeWidth(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  SetValueToCoord(val, GetStyleSVG()->mStrokeWidth);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFillOpacity(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleSVG()->mFillOpacity);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFloodOpacity(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleSVGReset()->mFloodOpacity);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetStopOpacity(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleSVGReset()->mStopOpacity);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetStrokeMiterlimit(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleSVG()->mStrokeMiterlimit);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetStrokeOpacity(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetNumber(GetStyleSVG()->mStrokeOpacity);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetClipRule(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(
                  GetStyleSVG()->mClipRule, nsCSSProps::kFillRuleKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFillRule(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(
                  GetStyleSVG()->mFillRule, nsCSSProps::kFillRuleKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetStrokeLinecap(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mStrokeLinecap,
                                   nsCSSProps::kStrokeLinecapKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetStrokeLinejoin(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mStrokeLinejoin,
                                   nsCSSProps::kStrokeLinejoinKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextAnchor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mTextAnchor,
                                   nsCSSProps::kTextAnchorKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetColorInterpolation(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mColorInterpolation,
                                   nsCSSProps::kColorInterpolationKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetColorInterpolationFilters(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mColorInterpolationFilters,
                                   nsCSSProps::kColorInterpolationKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetDominantBaseline(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVGReset()->mDominantBaseline,
                                   nsCSSProps::kDominantBaselineKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetImageRendering(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mImageRendering,
                                   nsCSSProps::kImageRenderingKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetShapeRendering(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mShapeRendering,
                                   nsCSSProps::kShapeRenderingKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetTextRendering(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(GetStyleSVG()->mTextRendering,
                                   nsCSSProps::kTextRenderingKTable));

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFloodColor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = SetToRGBAColor(val, GetStyleSVGReset()->mFloodColor);
  if (NS_FAILED(rv)) {
    delete val;
    return rv;
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetLightingColor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = SetToRGBAColor(val, GetStyleSVGReset()->mLightingColor);
  if (NS_FAILED(rv)) {
    delete val;
    return rv;
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetStopColor(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue *val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = SetToRGBAColor(val, GetStyleSVGReset()->mStopColor);
  if (NS_FAILED(rv)) {
    delete val;
    return rv;
  }

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetClipPath(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleSVGReset* svg = GetStyleSVGReset();

  if (svg->mClipPath)
    val->SetURI(svg->mClipPath);
  else
    val->SetIdent(eCSSKeyword_none);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetFilter(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleSVGReset* svg = GetStyleSVGReset();

  if (svg->mFilter)
    val->SetURI(svg->mFilter);
  else
    val->SetIdent(eCSSKeyword_none);

  return CallQueryInterface(val, aValue);
}

nsresult
nsComputedDOMStyle::GetMask(nsIDOMCSSValue** aValue)
{
  nsROCSSPrimitiveValue* val = GetROCSSPrimitiveValue();
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleSVGReset* svg = GetStyleSVGReset();

  if (svg->mMask)
    val->SetURI(svg->mMask);
  else
    val->SetIdent(eCSSKeyword_none);

  return CallQueryInterface(val, aValue);
}

#endif // MOZ_SVG

nsresult
nsComputedDOMStyle::GetTransitionDelay(nsIDOMCSSValue** aValue)
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

  return CallQueryInterface(valueList, aValue);
}

nsresult
nsComputedDOMStyle::GetTransitionDuration(nsIDOMCSSValue** aValue)
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

  return CallQueryInterface(valueList, aValue);
}

nsresult
nsComputedDOMStyle::GetTransitionProperty(nsIDOMCSSValue** aValue)
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
      const char *str;
      transition->GetUnknownProperty()->GetUTF8String(&str);
      property->SetString(nsDependentCString(str)); // really want SetIdent
    }
    else
      property->SetString(nsCSSProps::GetStringValue(cssprop));
  } while (++i < display->mTransitionPropertyCount);

  return CallQueryInterface(valueList, aValue);
}

nsresult
nsComputedDOMStyle::GetTransitionTimingFunction(nsIDOMCSSValue** aValue)
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

  return CallQueryInterface(valueList, aValue);
}

#define COMPUTED_STYLE_MAP_ENTRY(_prop, _method)              \
  { eCSSProperty_##_prop, &nsComputedDOMStyle::Get##_method, PR_FALSE }
#define COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_prop, _method)       \
  { eCSSProperty_##_prop, &nsComputedDOMStyle::Get##_method, PR_TRUE }

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
    COMPUTED_STYLE_MAP_ENTRY(_moz_background_clip,          BackgroundClip),
    COMPUTED_STYLE_MAP_ENTRY(_moz_background_inline_policy, BackgroundInlinePolicy),
    COMPUTED_STYLE_MAP_ENTRY(_moz_background_origin,        BackgroundOrigin),
    COMPUTED_STYLE_MAP_ENTRY(_moz_background_size,          MozBackgroundSize),
    COMPUTED_STYLE_MAP_ENTRY(binding,                       Binding),
    COMPUTED_STYLE_MAP_ENTRY(border_bottom_colors,          BorderBottomColors),
    COMPUTED_STYLE_MAP_ENTRY(border_image,                  BorderImage),
    COMPUTED_STYLE_MAP_ENTRY(border_left_colors,            BorderLeftColors),
    COMPUTED_STYLE_MAP_ENTRY(border_right_colors,           BorderRightColors),
    COMPUTED_STYLE_MAP_ENTRY(border_top_colors,             BorderTopColors),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_border_radius_bottomLeft, BorderRadiusBottomLeft),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_border_radius_bottomRight,BorderRadiusBottomRight),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_border_radius_topLeft,    BorderRadiusTopLeft),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_border_radius_topRight,   BorderRadiusTopRight),
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
    COMPUTED_STYLE_MAP_ENTRY(force_broken_image_icon,  ForceBrokenImageIcon),
    COMPUTED_STYLE_MAP_ENTRY(image_region,                  ImageRegion),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_outline_radius_bottomLeft, OutlineRadiusBottomLeft),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_outline_radius_bottomRight,OutlineRadiusBottomRight),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_outline_radius_topLeft,    OutlineRadiusTopLeft),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_outline_radius_topRight,   OutlineRadiusTopRight),
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
    COMPUTED_STYLE_MAP_ENTRY(word_wrap,                     WordWrap)

#ifdef MOZ_SVG
    ,
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
#endif

  };

  *aLength = NS_ARRAY_LENGTH(map);

  return map;
}

