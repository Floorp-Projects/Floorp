/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=78 expandtab softtabstop=2 ts=2 sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object returned from element.getComputedStyle() */

#include "mozilla/Util.h"

#include "nsComputedDOMStyle.h"

#include "nsError.h"
#include "nsDOMString.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsStyleContext.h"
#include "nsIScrollableFrame.h"
#include "nsContentUtils.h"
#include "nsIContent.h"

#include "nsCSSProps.h"
#include "nsDOMCSSRect.h"
#include "nsDOMCSSRGBColor.h"
#include "nsDOMCSSValueList.h"
#include "nsGkAtoms.h"
#include "nsHTMLReflowState.h"
#include "nsStyleUtil.h"
#include "nsStyleStructInlines.h"
#include "nsROCSSPrimitiveValue.h"

#include "nsPresContext.h"
#include "nsIDocument.h"

#include "nsCSSPseudoElements.h"
#include "nsStyleSet.h"
#include "imgIRequest.h"
#include "nsLayoutUtils.h"
#include "nsCSSKeywords.h"
#include "nsStyleCoord.h"
#include "nsDisplayList.h"
#include "nsDOMCSSDeclaration.h"
#include "nsStyleTransformMatrix.h"
#include "mozilla/dom/Element.h"
#include "nsWrapperCacheInlines.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

#if defined(DEBUG_bzbarsky) || defined(DEBUG_caillon)
#define DEBUG_ComputedDOMStyle
#endif

/*
 * This is the implementation of the readonly CSSStyleDeclaration that is
 * returned by the getComputedStyle() function.
 */

static nsComputedDOMStyle *sCachedComputedDOMStyle;

already_AddRefed<nsComputedDOMStyle>
NS_NewComputedDOMStyle(dom::Element* aElement, const nsAString& aPseudoElt,
                       nsIPresShell* aPresShell,
                       nsComputedDOMStyle::StyleType aStyleType)
{
  nsRefPtr<nsComputedDOMStyle> computedStyle;
  if (sCachedComputedDOMStyle) {
    // There's an unused nsComputedDOMStyle cached, use it.
    // But before we use it, re-initialize the object.

    // Oh yeah baby, placement new!
    computedStyle = new (sCachedComputedDOMStyle)
      nsComputedDOMStyle(aElement, aPseudoElt, aPresShell, aStyleType);

    sCachedComputedDOMStyle = nullptr;
  } else {
    // No nsComputedDOMStyle cached, create a new one.

    computedStyle = new nsComputedDOMStyle(aElement, aPseudoElt, aPresShell,
                                           aStyleType);
  }

  return computedStyle.forget();
}

nsComputedDOMStyle::nsComputedDOMStyle(dom::Element* aElement,
                                       const nsAString& aPseudoElt,
                                       nsIPresShell* aPresShell,
                                       StyleType aStyleType)
  : mDocumentWeak(nullptr), mOuterFrame(nullptr),
    mInnerFrame(nullptr), mPresShell(nullptr),
    mStyleType(aStyleType),
    mExposeVisitedStyle(false)
{
  MOZ_ASSERT(aElement && aPresShell);

  mDocumentWeak = do_GetWeakReference(aPresShell->GetDocument());

  mContent = aElement;

  if (!DOMStringIsNull(aPseudoElt) && !aPseudoElt.IsEmpty() &&
      aPseudoElt.First() == PRUnichar(':')) {
    // deal with two-colon forms of aPseudoElt
    nsAString::const_iterator start, end;
    aPseudoElt.BeginReading(start);
    aPseudoElt.EndReading(end);
    NS_ASSERTION(start != end, "aPseudoElt is not empty!");
    ++start;
    bool haveTwoColons = true;
    if (start == end || *start != PRUnichar(':')) {
      --start;
      haveTwoColons = false;
    }
    mPseudo = do_GetAtom(Substring(start, end));
    MOZ_ASSERT(mPseudo);

    // There aren't any non-CSS2 pseudo-elements with a single ':'
    if (!haveTwoColons &&
        (!nsCSSPseudoElements::IsPseudoElement(mPseudo) ||
         !nsCSSPseudoElements::IsCSS2PseudoElement(mPseudo))) {
      // XXXbz I'd really rather we threw an exception or something, but
      // the DOM spec sucks.
      mPseudo = nullptr;
    }
  }

  MOZ_ASSERT(aPresShell->GetPresContext());
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
  sCachedComputedDOMStyle = nullptr;
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsComputedDOMStyle, mContent)

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsComputedDOMStyle)
  return tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsComputedDOMStyle)
  return tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsComputedDOMStyle)
  return tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

// QueryInterface implementation for nsComputedDOMStyle
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsComputedDOMStyle)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(nsDOMCSSDeclaration)


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
nsComputedDOMStyle::GetLength(uint32_t* aLength)
{
  NS_PRECONDITION(aLength, "Null aLength!  Prepare to die!");

  (void)GetQueryablePropertyMap(aLength);

  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  *aParentRule = nullptr;

  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyValue(const nsAString& aPropertyName,
                                     nsAString& aReturn)
{
  aReturn.Truncate();

  ErrorResult error;
  nsRefPtr<CSSValue> val = GetPropertyCSSValue(aPropertyName, error);
  if (error.Failed()) {
    return error.ErrorCode();
  }

  if (val) {
    nsString text;
    val->GetCssText(text, error);
    aReturn.Assign(text);
    return error.ErrorCode();
  }

  return NS_OK;
}

/* static */
already_AddRefed<nsStyleContext>
nsComputedDOMStyle::GetStyleContextForElement(Element* aElement,
                                              nsIAtom* aPseudo,
                                              nsIPresShell* aPresShell,
                                              StyleType aStyleType)
{
  // If the content has a pres shell, we must use it.  Otherwise we'd
  // potentially mix rule trees by using the wrong pres shell's style
  // set.  Using the pres shell from the content also means that any
  // content that's actually *in* a document will get the style from the
  // correct document.
  nsIPresShell *presShell = GetPresShellForContent(aElement);
  if (!presShell) {
    presShell = aPresShell;
    if (!presShell)
      return nullptr;
  }

  presShell->FlushPendingNotifications(Flush_Style);

  return GetStyleContextForElementNoFlush(aElement, aPseudo, presShell,
                                          aStyleType);
}

/* static */
already_AddRefed<nsStyleContext>
nsComputedDOMStyle::GetStyleContextForElementNoFlush(Element* aElement,
                                                     nsIAtom* aPseudo,
                                                     nsIPresShell* aPresShell,
                                                     StyleType aStyleType)
{
  NS_ABORT_IF_FALSE(aElement, "NULL element");
  // If the content has a pres shell, we must use it.  Otherwise we'd
  // potentially mix rule trees by using the wrong pres shell's style
  // set.  Using the pres shell from the content also means that any
  // content that's actually *in* a document will get the style from the
  // correct document.
  nsIPresShell *presShell = GetPresShellForContent(aElement);
  if (!presShell) {
    presShell = aPresShell;
    if (!presShell)
      return nullptr;
  }

  if (!aPseudo && aStyleType == eAll) {
    nsIFrame* frame = nsLayoutUtils::GetStyleFrame(aElement);
    if (frame) {
      nsStyleContext* result = frame->StyleContext();
      // Don't use the style context if it was influenced by
      // pseudo-elements, since then it's not the primary style
      // for this element.
      if (!result->HasPseudoElementData()) {
        // this function returns an addrefed style context
        nsRefPtr<nsStyleContext> ret = result;
        return ret.forget();
      }
    }
  }

  // No frame has been created, or we have a pseudo, or we're looking
  // for the default style, so resolve the style ourselves.
  nsRefPtr<nsStyleContext> parentContext;
  nsIContent* parent = aPseudo ? aElement : aElement->GetParent();
  // Don't resolve parent context for document fragments.
  if (parent && parent->IsElement())
    parentContext = GetStyleContextForElementNoFlush(parent->AsElement(),
                                                     nullptr, presShell,
                                                     aStyleType);

  nsPresContext *presContext = presShell->GetPresContext();
  if (!presContext)
    return nullptr;

  nsStyleSet *styleSet = presShell->StyleSet();

  nsRefPtr<nsStyleContext> sc;
  if (aPseudo) {
    nsCSSPseudoElements::Type type = nsCSSPseudoElements::GetPseudoType(aPseudo);
    if (type >= nsCSSPseudoElements::ePseudo_PseudoElementCount) {
      return nullptr;
    }
    sc = styleSet->ResolvePseudoElementStyle(aElement, type, parentContext);
  } else {
    sc = styleSet->ResolveStyleFor(aElement, parentContext);
  }

  if (aStyleType == eDefaultOnly) {
    // We really only want the user and UA rules.  Filter out the other ones.
    nsTArray< nsCOMPtr<nsIStyleRule> > rules;
    for (nsRuleNode* ruleNode = sc->RuleNode();
         !ruleNode->IsRoot();
         ruleNode = ruleNode->GetParent()) {
      if (ruleNode->GetLevel() == nsStyleSet::eAgentSheet ||
          ruleNode->GetLevel() == nsStyleSet::eUserSheet) {
        rules.AppendElement(ruleNode->GetRule());
      }
    }

    // We want to build a list of user/ua rules that is in order from least to
    // most important, so we have to reverse the list.
    // Integer division to get "stop" is purposeful here: if length is odd, we
    // don't have to do anything with the middle element of the array.
    for (uint32_t i = 0, length = rules.Length(), stop = length / 2;
         i < stop; ++i) {
      rules[i].swap(rules[length - i - 1]);
    }
    
    sc = styleSet->ResolveStyleForRules(parentContext, rules);
  }

  return sc.forget();
}

nsMargin
nsComputedDOMStyle::GetAdjustedValuesForBoxSizing()
{
  // We want the width/height of whatever parts 'width' or 'height' controls,
  // which can be different depending on the value of the 'box-sizing' property.
  const nsStylePosition* stylePos = StylePosition();

  nsMargin adjustment;
  switch(stylePos->mBoxSizing) {
    case NS_STYLE_BOX_SIZING_BORDER:
      adjustment += mInnerFrame->GetUsedBorder();
      // fall through

    case NS_STYLE_BOX_SIZING_PADDING:
      adjustment += mInnerFrame->GetUsedPadding();
  }

  return adjustment;
}

/* static */
nsIPresShell*
nsComputedDOMStyle::GetPresShellForContent(nsIContent* aContent)
{
  nsIDocument* currentDoc = aContent->GetCurrentDoc();
  if (!currentDoc)
    return nullptr;

  return currentDoc->GetShell();
}

// nsDOMCSSDeclaration abstract methods which should never be called
// on a nsComputedDOMStyle object, but must be defined to avoid
// compile errors.
css::Declaration*
nsComputedDOMStyle::GetCSSDeclaration(bool)
{
  NS_RUNTIMEABORT("called nsComputedDOMStyle::GetCSSDeclaration");
  return nullptr;
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
  return nullptr;
}

void
nsComputedDOMStyle::GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv)
{
  NS_RUNTIMEABORT("called nsComputedDOMStyle::GetCSSParsingEnvironment");
  // Just in case NS_RUNTIMEABORT ever stops killing us for some reason
  aCSSParseEnv.mPrincipal = nullptr;
}

already_AddRefed<CSSValue>
nsComputedDOMStyle::GetPropertyCSSValue(const nsAString& aPropertyName, ErrorResult& aRv)
{
  NS_ASSERTION(!mStyleContextHolder, "bad state");

  nsCOMPtr<nsIDocument> document = do_QueryReferent(mDocumentWeak);
  if (!document) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  document->FlushPendingLinkUpdates();

  nsCSSProperty prop = nsCSSProps::LookupProperty(aPropertyName,
                                                  nsCSSProps::eEnabled);

  // We don't (for now, anyway, though it may make sense to change it
  // for all aliases, including those in nsCSSPropAliasList) want
  // aliases to be enumerable (via GetLength and IndexedGetter), so
  // handle them here rather than adding entries to
  // GetQueryablePropertyMap.
  if (prop != eCSSProperty_UNKNOWN &&
      nsCSSProps::PropHasFlags(prop, CSS_PROPERTY_IS_ALIAS)) {
    const nsCSSProperty* subprops = nsCSSProps::SubpropertyEntryFor(prop);
    NS_ABORT_IF_FALSE(subprops[1] == eCSSProperty_UNKNOWN,
                      "must have list of length 1");
    prop = subprops[0];
  }

  const ComputedStyleMapEntry* propEntry = nullptr;
  {
    uint32_t length = 0;
    const ComputedStyleMapEntry* propMap = GetQueryablePropertyMap(&length);
    for (uint32_t i = 0; i < length; ++i) {
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
    return nullptr;
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
  if (!mPresShell || !mPresShell->GetPresContext()) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return nullptr;
  }

  if (!mPseudo && mStyleType == eAll) {
    mOuterFrame = mContent->GetPrimaryFrame();
    mInnerFrame = mOuterFrame;
    if (mOuterFrame) {
      nsIAtom* type = mOuterFrame->GetType();
      if (type == nsGkAtoms::tableOuterFrame) {
        // If the frame is an outer table frame then we should get the style
        // from the inner table frame.
        mInnerFrame = mOuterFrame->GetFirstPrincipalChild();
        NS_ASSERTION(mInnerFrame, "Outer table must have an inner");
        NS_ASSERTION(!mInnerFrame->GetNextSibling(),
                     "Outer table frames should have just one child, "
                     "the inner table");
      }

      mStyleContextHolder = mInnerFrame->StyleContext();
      NS_ASSERTION(mStyleContextHolder, "Frame without style context?");
    }
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
                                                    mPresShell,
                                                    mStyleType);
    if (!mStyleContextHolder) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    NS_ASSERTION(mPseudo || !mStyleContextHolder->HasPseudoElementData(),
                 "should not have pseudo-element data");
  }

  // mExposeVisitedStyle is set to true only by testing APIs that
  // require chrome privilege.
  NS_ABORT_IF_FALSE(!mExposeVisitedStyle ||
                    nsContentUtils::IsCallerChrome(),
                    "mExposeVisitedStyle set incorrectly");
  if (mExposeVisitedStyle && mStyleContextHolder->RelevantLinkVisited()) {
    nsStyleContext *styleIfVisited = mStyleContextHolder->GetStyleIfVisited();
    if (styleIfVisited) {
      mStyleContextHolder = styleIfVisited;
    }
  }

  // Call our pointer-to-member-function.
  nsRefPtr<CSSValue> val = (this->*(propEntry->mGetter))();

  mOuterFrame = nullptr;
  mInnerFrame = nullptr;
  mPresShell = nullptr;

  // Release the current style context for it should be re-resolved
  // whenever a frame is not available.
  mStyleContextHolder = nullptr;

  return val.forget();
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
nsComputedDOMStyle::Item(uint32_t aIndex, nsAString& aReturn)
{
  return nsDOMCSSDeclaration::Item(aIndex, aReturn);
}

void
nsComputedDOMStyle::IndexedGetter(uint32_t aIndex, bool& aFound,
                                  nsAString& aPropName)
{
  uint32_t length = 0;
  const ComputedStyleMapEntry* propMap = GetQueryablePropertyMap(&length);
  aFound = aIndex < length;
  if (aFound) {
    CopyASCIItoUTF16(nsCSSProps::GetStringValue(propMap[aIndex].mProperty),
                     aPropName);
  }
}

// Property getters...

CSSValue*
nsComputedDOMStyle::DoGetBinding()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;

  const nsStyleDisplay* display = StyleDisplay();

  if (display->mBinding) {
    val->SetURI(display->mBinding->GetURI());
  } else {
    val->SetIdent(eCSSKeyword_none);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetClear()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mBreakType,
                                               nsCSSProps::kClearKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFloat()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mFloats,
                                               nsCSSProps::kFloatKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetBottom()
{
  return GetOffsetWidthFor(NS_SIDE_BOTTOM);
}

CSSValue*
nsComputedDOMStyle::DoGetStackSizing()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(StyleXUL()->mStretchStack ? eCSSKeyword_stretch_to_fit :
                eCSSKeyword_ignore);
  return val;
}

void
nsComputedDOMStyle::SetToRGBAColor(nsROCSSPrimitiveValue* aValue,
                                   nscolor aColor)
{
  if (NS_GET_A(aColor) == 0) {
    aValue->SetIdent(eCSSKeyword_transparent);
    return;
  }

  nsROCSSPrimitiveValue *red   = new nsROCSSPrimitiveValue;
  nsROCSSPrimitiveValue *green = new nsROCSSPrimitiveValue;
  nsROCSSPrimitiveValue *blue  = new nsROCSSPrimitiveValue;
  nsROCSSPrimitiveValue *alpha  = new nsROCSSPrimitiveValue;

  uint8_t a = NS_GET_A(aColor);
  nsDOMCSSRGBColor *rgbColor =
    new nsDOMCSSRGBColor(red, green, blue, alpha, a < 255);

  red->SetNumber(NS_GET_R(aColor));
  green->SetNumber(NS_GET_G(aColor));
  blue->SetNumber(NS_GET_B(aColor));
  alpha->SetNumber(nsStyleUtil::ColorComponentToFloat(a));

  aValue->SetColor(rgbColor);
}

CSSValue*
nsComputedDOMStyle::DoGetColor()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  SetToRGBAColor(val, StyleColor()->mColor);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetOpacity()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleDisplay()->mOpacity);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetColumnCount()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;

  const nsStyleColumn* column = StyleColumn();

  if (column->mColumnCount == NS_STYLE_COLUMN_COUNT_AUTO) {
    val->SetIdent(eCSSKeyword_auto);
  } else {
    val->SetNumber(column->mColumnCount);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetColumnWidth()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;

  // XXX fix the auto case. When we actually have a column frame, I think
  // we should return the computed column width.
  SetValueToCoord(val, StyleColumn()->mColumnWidth, true);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetColumnGap()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;

  const nsStyleColumn* column = StyleColumn();
  if (column->mColumnGap.GetUnit() == eStyleUnit_Normal) {
    val->SetAppUnits(StyleFont()->mFont.size);
  } else {
    SetValueToCoord(val, StyleColumn()->mColumnGap, true);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetColumnFill()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleColumn()->mColumnFill,
                                   nsCSSProps::kColumnFillKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetColumnRuleWidth()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetAppUnits(StyleColumn()->GetComputedColumnRuleWidth());
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetColumnRuleStyle()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleColumn()->mColumnRuleStyle,
                                   nsCSSProps::kBorderStyleKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetColumnRuleColor()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;

  const nsStyleColumn* column = StyleColumn();
  nscolor ruleColor;
  if (column->mColumnRuleColorIsForeground) {
    ruleColor = StyleColor()->mColor;
  } else {
    ruleColor = column->mColumnRuleColor;
  }

  SetToRGBAColor(val, ruleColor);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetContent()
{
  const nsStyleContent *content = StyleContent();

  if (content->ContentCount() == 0) {
    nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val;
  }

  if (content->ContentCount() == 1 &&
      content->ContentAt(0).mType == eStyleContentType_AltContent) {
    nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword__moz_alt_content);
    return val;
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(false);

  for (uint32_t i = 0, i_end = content->ContentCount(); i < i_end; ++i) {
    nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(val);

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
          int32_t typeItem = 1;
          if (data.mType == eStyleContentType_Counters) {
            typeItem = 2;
            str.AppendLiteral(", ");
            nsStyleUtil::AppendEscapedCSSString(
              nsDependentString(a->Item(1).GetStringBufferValue()), str);
          }
          NS_ABORT_IF_FALSE(eCSSUnit_None != a->Item(typeItem).GetUnit(),
                            "'none' should be handled  as enumerated value");
          int32_t type = a->Item(typeItem).GetIntValue();
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

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetCounterIncrement()
{
  const nsStyleContent *content = StyleContent();

  if (content->CounterIncrementCount() == 0) {
    nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val;
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(false);

  for (uint32_t i = 0, i_end = content->CounterIncrementCount(); i < i_end; ++i) {
    nsROCSSPrimitiveValue* name = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(name);

    nsROCSSPrimitiveValue* value = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(value);

    const nsStyleCounterData *data = content->GetCounterIncrementAt(i);
    nsAutoString escaped;
    nsStyleUtil::AppendEscapedCSSIdent(data->mCounter, escaped);
    name->SetString(escaped);
    value->SetNumber(data->mValue); // XXX This should really be integer
  }

  return valueList;
}

/* Convert the stored representation into a list of two values and then hand
 * it back.
 */
CSSValue*
nsComputedDOMStyle::DoGetTransformOrigin()
{
  /* We need to build up a list of two values.  We'll call them
   * width and height.
   */

  /* Store things as a value list */
  nsDOMCSSValueList* valueList = GetROCSSValueList(false);

  /* Now, get the values. */
  const nsStyleDisplay* display = StyleDisplay();

  nsROCSSPrimitiveValue* width = new nsROCSSPrimitiveValue;
  SetValueToCoord(width, display->mTransformOrigin[0], false,
                  &nsComputedDOMStyle::GetFrameBoundsWidthForTransform);
  valueList->AppendCSSValue(width);

  nsROCSSPrimitiveValue* height = new nsROCSSPrimitiveValue;
  SetValueToCoord(height, display->mTransformOrigin[1], false,
                  &nsComputedDOMStyle::GetFrameBoundsHeightForTransform);
  valueList->AppendCSSValue(height);

  if (display->mTransformOrigin[2].GetUnit() != eStyleUnit_Coord ||
      display->mTransformOrigin[2].GetCoordValue() != 0) {
    nsROCSSPrimitiveValue* depth = new nsROCSSPrimitiveValue;
    SetValueToCoord(depth, display->mTransformOrigin[2], false,
                    nullptr);
    valueList->AppendCSSValue(depth);
  }

  return valueList;
}

/* Convert the stored representation into a list of two values and then hand
 * it back.
 */
CSSValue*
nsComputedDOMStyle::DoGetPerspectiveOrigin()
{
  /* We need to build up a list of two values.  We'll call them
   * width and height.
   */

  /* Store things as a value list */
  nsDOMCSSValueList* valueList = GetROCSSValueList(false);

  /* Now, get the values. */
  const nsStyleDisplay* display = StyleDisplay();

  nsROCSSPrimitiveValue* width = new nsROCSSPrimitiveValue;
  SetValueToCoord(width, display->mPerspectiveOrigin[0], false,
                  &nsComputedDOMStyle::GetFrameBoundsWidthForTransform);
  valueList->AppendCSSValue(width);

  nsROCSSPrimitiveValue* height = new nsROCSSPrimitiveValue;
  SetValueToCoord(height, display->mPerspectiveOrigin[1], false,
                  &nsComputedDOMStyle::GetFrameBoundsHeightForTransform);
  valueList->AppendCSSValue(height);

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetPerspective()
{
    nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
    if (StyleDisplay()->mChildPerspective.GetUnit() == eStyleUnit_Coord &&
        StyleDisplay()->mChildPerspective.GetCoordValue() == 0.0) {
        val->SetIdent(eCSSKeyword_none);
    } else {
        SetValueToCoord(val, StyleDisplay()->mChildPerspective, false);
    }
    return val;
}

CSSValue*
nsComputedDOMStyle::DoGetBackfaceVisibility()
{
    nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
    val->SetIdent(
        nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mBackfaceVisibility,
                                       nsCSSProps::kBackfaceVisibilityKTable));
    return val;
}

CSSValue*
nsComputedDOMStyle::DoGetTransformStyle()
{
    nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
    val->SetIdent(
        nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mTransformStyle,
                                       nsCSSProps::kTransformStyleKTable));
    return val;
}

/* If the property is "none", hand back "none" wrapped in a value.
 * Otherwise, compute the aggregate transform matrix and hands it back in a
 * "matrix" wrapper.
 */
CSSValue*
nsComputedDOMStyle::DoGetTransform()
{
  /* First, get the display data.  We'll need it. */
  const nsStyleDisplay* display = StyleDisplay();

  /* If there are no transforms, then we should construct a single-element
   * entry and hand it back.
   */
  if (!display->mSpecifiedTransform) {
    nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

    /* Set it to "none." */
    val->SetIdent(eCSSKeyword_none);
    return val;
  }

  /* Otherwise, we need to compute the current value of the transform matrix,
   * store it in a string, and hand it back to the caller.
   */

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

   bool dummy;
   gfx3DMatrix matrix =
     nsStyleTransformMatrix::ReadTransforms(display->mSpecifiedTransform,
                                            mStyleContextHolder,
                                            mStyleContextHolder->PresContext(),
                                            dummy,
                                            bounds,
                                            float(nsDeviceContext::AppUnitsPerCSSPixel()));

  return MatrixToCSSValue(matrix);
}

/* static */ nsROCSSPrimitiveValue*
nsComputedDOMStyle::MatrixToCSSValue(gfx3DMatrix& matrix)
{
  bool is3D = !matrix.Is2D();

  nsAutoString resultString(NS_LITERAL_STRING("matrix"));
  if (is3D) {
    resultString.Append(NS_LITERAL_STRING("3d"));
  }

  resultString.Append(NS_LITERAL_STRING("("));
  resultString.AppendFloat(matrix._11);
  resultString.Append(NS_LITERAL_STRING(", "));
  resultString.AppendFloat(matrix._12);
  resultString.Append(NS_LITERAL_STRING(", "));
  if (is3D) {
    resultString.AppendFloat(matrix._13);
    resultString.Append(NS_LITERAL_STRING(", "));
    resultString.AppendFloat(matrix._14);
    resultString.Append(NS_LITERAL_STRING(", "));
  }
  resultString.AppendFloat(matrix._21);
  resultString.Append(NS_LITERAL_STRING(", "));
  resultString.AppendFloat(matrix._22);
  resultString.Append(NS_LITERAL_STRING(", "));
  if (is3D) {
    resultString.AppendFloat(matrix._23);
    resultString.Append(NS_LITERAL_STRING(", "));
    resultString.AppendFloat(matrix._24);
    resultString.Append(NS_LITERAL_STRING(", "));
    resultString.AppendFloat(matrix._31);
    resultString.Append(NS_LITERAL_STRING(", "));
    resultString.AppendFloat(matrix._32);
    resultString.Append(NS_LITERAL_STRING(", "));
    resultString.AppendFloat(matrix._33);
    resultString.Append(NS_LITERAL_STRING(", "));
    resultString.AppendFloat(matrix._34);
    resultString.Append(NS_LITERAL_STRING(", "));
  }
  resultString.AppendFloat(matrix._41);
  resultString.Append(NS_LITERAL_STRING(", "));
  resultString.AppendFloat(matrix._42);
  if (is3D) {
    resultString.Append(NS_LITERAL_STRING(", "));
    resultString.AppendFloat(matrix._43);
    resultString.Append(NS_LITERAL_STRING(", "));
    resultString.AppendFloat(matrix._44);
  }
  resultString.Append(NS_LITERAL_STRING(")"));

  /* Create a value to hold our result. */
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  val->SetString(resultString);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetCounterReset()
{
  const nsStyleContent *content = StyleContent();

  if (content->CounterResetCount() == 0) {
    nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val;
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(false);

  for (uint32_t i = 0, i_end = content->CounterResetCount(); i < i_end; ++i) {
    nsROCSSPrimitiveValue* name = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(name);

    nsROCSSPrimitiveValue* value = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(value);

    const nsStyleCounterData *data = content->GetCounterResetAt(i);
    nsAutoString escaped;
    nsStyleUtil::AppendEscapedCSSIdent(data->mCounter, escaped);
    name->SetString(escaped);
    value->SetNumber(data->mValue); // XXX This should really be integer
  }

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetQuotes()
{
  const nsStyleQuotes *quotes = StyleQuotes();

  if (quotes->QuotesCount() == 0) {
    nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val;
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(false);

  for (uint32_t i = 0, i_end = quotes->QuotesCount(); i < i_end; ++i) {
    nsROCSSPrimitiveValue* openVal = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(openVal);

    nsROCSSPrimitiveValue* closeVal = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(closeVal);

    nsString s;
    nsStyleUtil::AppendEscapedCSSString(*quotes->OpenQuoteAt(i), s);
    openVal->SetString(s);
    s.Truncate();
    nsStyleUtil::AppendEscapedCSSString(*quotes->CloseQuoteAt(i), s);
    closeVal->SetString(s);
  }

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetFontFamily()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  const nsStyleFont* font = StyleFont();

  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocumentWeak);
  NS_ASSERTION(doc, "document is required");
  nsIPresShell* presShell = doc->GetShell();
  NS_ASSERTION(presShell, "pres shell is required");
  nsPresContext *presContext = presShell->GetPresContext();
  NS_ASSERTION(presContext, "pres context is required");

  const nsString& fontName = font->mFont.name;
  if (font->mGenericID == kGenericFont_NONE && !font->mFont.systemFont) {
    const nsFont* defaultFont =
      presContext->GetDefaultFont(kPresContext_DefaultVariableFont_ID,
                                  font->mLanguage);

    int32_t lendiff = fontName.Length() - defaultFont->name.Length();
    if (lendiff > 0) {
      val->SetString(Substring(fontName, 0, lendiff-1)); // -1 removes comma
    } else {
      val->SetString(fontName);
    }
  } else {
    val->SetString(fontName);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFontSize()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  // Note: StyleFont()->mSize is the 'computed size';
  // StyleFont()->mFont.size is the 'actual size'
  val->SetAppUnits(StyleFont()->mSize);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFontSizeAdjust()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;

  const nsStyleFont *font = StyleFont();

  if (font->mFont.sizeAdjust) {
    val->SetNumber(font->mFont.sizeAdjust);
  } else {
    val->SetIdent(eCSSKeyword_none);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetOSXFontSmoothing()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleFont()->mFont.smoothing,
                                               nsCSSProps::kFontSmoothingKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFontStretch()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleFont()->mFont.stretch,
                                               nsCSSProps::kFontStretchKTable));

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFontStyle()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleFont()->mFont.style,
                                               nsCSSProps::kFontStyleKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFontWeight()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  const nsStyleFont* font = StyleFont();

  uint16_t weight = font->mFont.weight;
  NS_ASSERTION(weight % 100 == 0, "unexpected value of font-weight");
  val->SetNumber(weight);

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFontVariant()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleFont()->mFont.variant,
                                   nsCSSProps::kFontVariantKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFontFeatureSettings()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  const nsStyleFont* font = StyleFont();
  if (font->mFont.fontFeatureSettings.IsEmpty()) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    nsAutoString result;
    nsStyleUtil::AppendFontFeatureSettings(font->mFont.fontFeatureSettings,
                                           result);
    val->SetString(result);
  }
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFontKerning()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleFont()->mFont.kerning,
                                   nsCSSProps::kFontKerningKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFontLanguageOverride()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  const nsStyleFont* font = StyleFont();
  if (font->mFont.languageOverride.IsEmpty()) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    nsString str;
    nsStyleUtil::AppendEscapedCSSString(font->mFont.languageOverride, str);
    val->SetString(str);
  }
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFontSynthesis()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleFont()->mFont.synthesis;

  if (0 == intValue) {
    val->SetIdent(eCSSKeyword_none);
  } else {
    nsAutoString valueStr;

    nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_font_synthesis,
      intValue, NS_FONT_SYNTHESIS_WEIGHT,
      NS_FONT_SYNTHESIS_STYLE, valueStr);
    val->SetString(valueStr);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFontVariantAlternates()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleFont()->mFont.variantAlternates;

  if (0 == intValue) {
    val->SetIdent(eCSSKeyword_normal);
    return val;
  }

  // first, include enumerated values
  nsAutoString valueStr;

  nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_font_variant_alternates,
    intValue & NS_FONT_VARIANT_ALTERNATES_ENUMERATED_MASK,
    NS_FONT_VARIANT_ALTERNATES_HISTORICAL,
    NS_FONT_VARIANT_ALTERNATES_HISTORICAL, valueStr);

  // next, include functional values if present
  if (intValue & NS_FONT_VARIANT_ALTERNATES_FUNCTIONAL_MASK) {
    nsStyleUtil::SerializeFunctionalAlternates(StyleFont()->mFont.alternateValues,
                                               valueStr);
  }

  val->SetString(valueStr);
  return val;
}


CSSValue*
nsComputedDOMStyle::DoGetFontVariantCaps()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleFont()->mFont.variantCaps;

  if (0 == intValue) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    val->SetIdent(
      nsCSSProps::ValueToKeywordEnum(intValue,
                                     nsCSSProps::kFontVariantCapsKTable));
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFontVariantEastAsian()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleFont()->mFont.variantEastAsian;

  if (0 == intValue) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    nsAutoString valueStr;

    nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_font_variant_east_asian,
      intValue, NS_FONT_VARIANT_EAST_ASIAN_JIS78,
      NS_FONT_VARIANT_EAST_ASIAN_RUBY, valueStr);
    val->SetString(valueStr);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFontVariantLigatures()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleFont()->mFont.variantLigatures;

  if (0 == intValue) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    nsAutoString valueStr;

    nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_font_variant_ligatures,
      intValue, NS_FONT_VARIANT_LIGATURES_COMMON,
      NS_FONT_VARIANT_LIGATURES_NO_CONTEXTUAL, valueStr);
    val->SetString(valueStr);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFontVariantNumeric()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleFont()->mFont.variantNumeric;

  if (0 == intValue) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    nsAutoString valueStr;

    nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_font_variant_numeric,
      intValue, NS_FONT_VARIANT_NUMERIC_LINING,
      NS_FONT_VARIANT_NUMERIC_ORDINAL, valueStr);
    val->SetString(valueStr);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFontVariantPosition()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleFont()->mFont.variantPosition;

  if (0 == intValue) {
    val->SetIdent(eCSSKeyword_normal);
  } else {
    val->SetIdent(
      nsCSSProps::ValueToKeywordEnum(intValue,
                                     nsCSSProps::kFontVariantPositionKTable));
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::GetBackgroundList(uint8_t nsStyleBackground::Layer::* aMember,
                                      uint32_t nsStyleBackground::* aCount,
                                      const int32_t aTable[])
{
  const nsStyleBackground* bg = StyleBackground();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  for (uint32_t i = 0, i_end = bg->*aCount; i < i_end; ++i) {
    nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(val);
    val->SetIdent(nsCSSProps::ValueToKeywordEnum(bg->mLayers[i].*aMember,
                                                 aTable));
  }

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetBackgroundAttachment()
{
  return GetBackgroundList(&nsStyleBackground::Layer::mAttachment,
                           &nsStyleBackground::mAttachmentCount,
                           nsCSSProps::kBackgroundAttachmentKTable);
}

CSSValue*
nsComputedDOMStyle::DoGetBackgroundClip()
{
  return GetBackgroundList(&nsStyleBackground::Layer::mClip,
                           &nsStyleBackground::mClipCount,
                           nsCSSProps::kBackgroundOriginKTable);
}

CSSValue*
nsComputedDOMStyle::DoGetBackgroundColor()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  SetToRGBAColor(val, StyleBackground()->mBackgroundColor);
  return val;
}


static void
SetValueToCalc(const nsStyleCoord::Calc *aCalc, nsROCSSPrimitiveValue *aValue)
{
  nsRefPtr<nsROCSSPrimitiveValue> val = new nsROCSSPrimitiveValue;
  nsAutoString tmp, result;

  result.AppendLiteral("calc(");

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

static void
AppendCSSGradientToBoxPosition(const nsStyleGradient* aGradient,
                               nsAString& aString,
                               bool& aNeedSep)
{
  float xValue = aGradient->mBgPosX.GetPercentValue();
  float yValue = aGradient->mBgPosY.GetPercentValue();

  if (yValue == 1.0f && xValue == 0.5f) {
    // omit "to bottom"
    return;
  }
  NS_ASSERTION(yValue != 0.5f || xValue != 0.5f, "invalid box position");

  aString.AppendLiteral("to");

  if (yValue == 0.0f) {
    aString.AppendLiteral(" top");
  } else if (yValue == 1.0f) {
    aString.AppendLiteral(" bottom");
  } else if (yValue != 0.5f) { // do not write "center" keyword
    NS_NOTREACHED("invalid box position");
  }

  if (xValue == 0.0f) {
    aString.AppendLiteral(" left");
  } else if (xValue == 1.0f) {
    aString.AppendLiteral(" right");
  } else if (xValue != 0.5f) { // do not write "center" keyword
    NS_NOTREACHED("invalid box position");
  }

  aNeedSep = true;
}

void
nsComputedDOMStyle::GetCSSGradientString(const nsStyleGradient* aGradient,
                                         nsAString& aString)
{
  if (!aGradient->mLegacySyntax) {
    aString.Truncate();
  } else {
    aString.AssignLiteral("-moz-");
  }
  if (aGradient->mRepeating) {
    aString.AppendLiteral("repeating-");
  }
  bool isRadial = aGradient->mShape != NS_STYLE_GRADIENT_SHAPE_LINEAR;
  if (isRadial) {
    aString.AppendLiteral("radial-gradient(");
  } else {
    aString.AppendLiteral("linear-gradient(");
  }

  bool needSep = false;
  nsAutoString tokenString;
  nsROCSSPrimitiveValue *tmpVal = new nsROCSSPrimitiveValue;

  if (isRadial && !aGradient->mLegacySyntax) {
    if (aGradient->mSize != NS_STYLE_GRADIENT_SIZE_EXPLICIT_SIZE) {
      if (aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_CIRCULAR) {
        aString.AppendLiteral("circle");
        needSep = true;
      }
      if (aGradient->mSize != NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER) {
        if (needSep) {
          aString.AppendLiteral(" ");
        }
        AppendASCIItoUTF16(nsCSSProps::
                           ValueToKeyword(aGradient->mSize,
                                          nsCSSProps::kRadialGradientSizeKTable),
                           aString);
        needSep = true;
      }
    } else {
      AppendCSSGradientLength(aGradient->mRadiusX, tmpVal, aString);
      if (aGradient->mShape != NS_STYLE_GRADIENT_SHAPE_CIRCULAR) {
        aString.AppendLiteral(" ");
        AppendCSSGradientLength(aGradient->mRadiusY, tmpVal, aString);
      }
      needSep = true;
    }
  }
  if (aGradient->mBgPosX.GetUnit() != eStyleUnit_None) {
    MOZ_ASSERT(aGradient->mBgPosY.GetUnit() != eStyleUnit_None);
    if (!isRadial && !aGradient->mLegacySyntax) {
      AppendCSSGradientToBoxPosition(aGradient, aString, needSep);
    } else if (aGradient->mBgPosX.GetUnit() != eStyleUnit_Percent ||
               aGradient->mBgPosX.GetPercentValue() != 0.5f ||
               aGradient->mBgPosY.GetUnit() != eStyleUnit_Percent ||
               aGradient->mBgPosY.GetPercentValue() != (isRadial ? 0.5f : 1.0f)) {
      if (isRadial && !aGradient->mLegacySyntax) {
        if (needSep) {
          aString.AppendLiteral(" ");
        }
        aString.AppendLiteral("at ");
        needSep = false;
      }
      AppendCSSGradientLength(aGradient->mBgPosX, tmpVal, aString);
      if (aGradient->mBgPosY.GetUnit() != eStyleUnit_None) {
        aString.AppendLiteral(" ");
        AppendCSSGradientLength(aGradient->mBgPosY, tmpVal, aString);
      }
      needSep = true;
    }
  }
  if (aGradient->mAngle.GetUnit() != eStyleUnit_None) {
    MOZ_ASSERT(!isRadial || aGradient->mLegacySyntax);
    if (needSep) {
      aString.AppendLiteral(" ");
    }
    SetCssTextToCoord(tokenString, aGradient->mAngle);
    aString.Append(tokenString);
    needSep = true;
  }

  if (isRadial && aGradient->mLegacySyntax &&
      (aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_CIRCULAR ||
       aGradient->mSize != NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER)) {
    MOZ_ASSERT(aGradient->mSize != NS_STYLE_GRADIENT_SIZE_EXPLICIT_SIZE);
    if (needSep) {
      aString.AppendLiteral(", ");
      needSep = false;
    }
    if (aGradient->mShape == NS_STYLE_GRADIENT_SHAPE_CIRCULAR) {
      aString.AppendLiteral("circle");
      needSep = true;
    }
    if (aGradient->mSize != NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER) {
      if (needSep) {
        aString.AppendLiteral(" ");
      }
      AppendASCIItoUTF16(nsCSSProps::
                         ValueToKeyword(aGradient->mSize,
                                        nsCSSProps::kRadialGradientSizeKTable),
                         aString);
    }
    needSep = true;
  }


  // color stops
  for (uint32_t i = 0; i < aGradient->mStops.Length(); ++i) {
    if (needSep) {
      aString.AppendLiteral(", ");
    }
    SetToRGBAColor(tmpVal, aGradient->mStops[i].mColor);
    tmpVal->GetCssText(tokenString);
    aString.Append(tokenString);

    if (aGradient->mStops[i].mLocation.GetUnit() != eStyleUnit_None) {
      aString.AppendLiteral(" ");
      AppendCSSGradientLength(aGradient->mStops[i].mLocation, tmpVal, aString);
    }
    needSep = true;
  }

  delete tmpVal;
  aString.AppendLiteral(")");
}

// -moz-image-rect(<uri>, <top>, <right>, <bottom>, <left>)
void
nsComputedDOMStyle::GetImageRectString(nsIURI* aURI,
                                       const nsStyleSides& aCropRect,
                                       nsString& aString)
{
  nsDOMCSSValueList* valueList = GetROCSSValueList(true);

  // <uri>
  nsROCSSPrimitiveValue *valURI = new nsROCSSPrimitiveValue;
  valueList->AppendCSSValue(valURI);
  valURI->SetURI(aURI);

  // <top>, <right>, <bottom>, <left>
  NS_FOR_CSS_SIDES(side) {
    nsROCSSPrimitiveValue *valSide = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(valSide);
    SetValueToCoord(valSide, aCropRect.Get(side), false);
  }

  nsAutoString argumentString;
  valueList->GetCssText(argumentString);
  delete valueList;

  aString = NS_LITERAL_STRING("-moz-image-rect(") +
            argumentString +
            NS_LITERAL_STRING(")");
}

void
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
        GetImageRectString(uri, *cropRect, imageRectString);
        aValue->SetString(imageRectString);
      } else {
        aValue->SetURI(uri);
      }
      break;
    }
    case eStyleImageType_Gradient:
    {
      nsAutoString gradientString;
      GetCSSGradientString(aStyleImage.GetGradientData(),
                           gradientString);
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
      break;
  }
}

CSSValue*
nsComputedDOMStyle::DoGetBackgroundImage()
{
  const nsStyleBackground* bg = StyleBackground();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  for (uint32_t i = 0, i_end = bg->mImageCount; i < i_end; ++i) {
    nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(val);

    const nsStyleImage& image = bg->mLayers[i].mImage;
    SetValueToStyleImage(image, val);
  }

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetBackgroundInlinePolicy()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(
                  StyleBackground()->mBackgroundInlinePolicy,
                  nsCSSProps::kBackgroundInlinePolicyKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetBackgroundOrigin()
{
  return GetBackgroundList(&nsStyleBackground::Layer::mOrigin,
                           &nsStyleBackground::mOriginCount,
                           nsCSSProps::kBackgroundOriginKTable);
}

CSSValue*
nsComputedDOMStyle::DoGetBackgroundPosition()
{
  const nsStyleBackground* bg = StyleBackground();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  for (uint32_t i = 0, i_end = bg->mPositionCount; i < i_end; ++i) {
    nsDOMCSSValueList *itemList = GetROCSSValueList(false);
    valueList->AppendCSSValue(itemList);

    nsROCSSPrimitiveValue *valX = new nsROCSSPrimitiveValue;
    itemList->AppendCSSValue(valX);

    nsROCSSPrimitiveValue *valY = new nsROCSSPrimitiveValue;
    itemList->AppendCSSValue(valY);

    const nsStyleBackground::Position &pos = bg->mLayers[i].mPosition;

    if (!pos.mXPosition.mHasPercent) {
      NS_ABORT_IF_FALSE(pos.mXPosition.mPercent == 0.0f,
                        "Shouldn't have mPercent!");
      valX->SetAppUnits(pos.mXPosition.mLength);
    } else if (pos.mXPosition.mLength == 0) {
      valX->SetPercent(pos.mXPosition.mPercent);
    } else {
      SetValueToCalc(&pos.mXPosition, valX);
    }

    if (!pos.mYPosition.mHasPercent) {
      NS_ABORT_IF_FALSE(pos.mYPosition.mPercent == 0.0f,
                        "Shouldn't have mPercent!");
      valY->SetAppUnits(pos.mYPosition.mLength);
    } else if (pos.mYPosition.mLength == 0) {
      valY->SetPercent(pos.mYPosition.mPercent);
    } else {
      SetValueToCalc(&pos.mYPosition, valY);
    }
  }

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetBackgroundRepeat()
{
  const nsStyleBackground* bg = StyleBackground();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  for (uint32_t i = 0, i_end = bg->mRepeatCount; i < i_end; ++i) {
    nsDOMCSSValueList *itemList = GetROCSSValueList(false);
    valueList->AppendCSSValue(itemList);

    nsROCSSPrimitiveValue *valX = new nsROCSSPrimitiveValue;
    itemList->AppendCSSValue(valX);

    const uint8_t& xRepeat = bg->mLayers[i].mRepeat.mXRepeat;
    const uint8_t& yRepeat = bg->mLayers[i].mRepeat.mYRepeat;

    bool hasContraction = true;
    unsigned contraction;
    if (xRepeat == yRepeat) {
      contraction = xRepeat;
    } else if (xRepeat == NS_STYLE_BG_REPEAT_REPEAT &&
               yRepeat == NS_STYLE_BG_REPEAT_NO_REPEAT) {
      contraction = NS_STYLE_BG_REPEAT_REPEAT_X;
    } else if (xRepeat == NS_STYLE_BG_REPEAT_NO_REPEAT &&
               yRepeat == NS_STYLE_BG_REPEAT_REPEAT) {
      contraction = NS_STYLE_BG_REPEAT_REPEAT_Y;
    } else {
      hasContraction = false;
    }

    if (hasContraction) {
      valX->SetIdent(nsCSSProps::ValueToKeywordEnum(contraction,
                                         nsCSSProps::kBackgroundRepeatKTable));
    } else {
      nsROCSSPrimitiveValue *valY = new nsROCSSPrimitiveValue;
      itemList->AppendCSSValue(valY);
      
      valX->SetIdent(nsCSSProps::ValueToKeywordEnum(xRepeat,
                                          nsCSSProps::kBackgroundRepeatKTable));
      valY->SetIdent(nsCSSProps::ValueToKeywordEnum(yRepeat,
                                          nsCSSProps::kBackgroundRepeatKTable));
    }
  }

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetBackgroundSize()
{
  const nsStyleBackground* bg = StyleBackground();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  for (uint32_t i = 0, i_end = bg->mSizeCount; i < i_end; ++i) {
    const nsStyleBackground::Size &size = bg->mLayers[i].mSize;

    switch (size.mWidthType) {
      case nsStyleBackground::Size::eContain:
      case nsStyleBackground::Size::eCover: {
        NS_ABORT_IF_FALSE(size.mWidthType == size.mHeightType,
                          "unsynced types");
        nsCSSKeyword keyword = size.mWidthType == nsStyleBackground::Size::eContain
                             ? eCSSKeyword_contain
                             : eCSSKeyword_cover;
        nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
        valueList->AppendCSSValue(val);
        val->SetIdent(keyword);
        break;
      }
      default: {
        nsDOMCSSValueList *itemList = GetROCSSValueList(false);
        valueList->AppendCSSValue(itemList);

        nsROCSSPrimitiveValue* valX = new nsROCSSPrimitiveValue;
        itemList->AppendCSSValue(valX);
        nsROCSSPrimitiveValue* valY = new nsROCSSPrimitiveValue;
        itemList->AppendCSSValue(valY);

        if (size.mWidthType == nsStyleBackground::Size::eAuto) {
          valX->SetIdent(eCSSKeyword_auto);
        } else {
          NS_ABORT_IF_FALSE(size.mWidthType ==
                              nsStyleBackground::Size::eLengthPercentage,
                            "bad mWidthType");
          if (!size.mWidth.mHasPercent &&
              // negative values must have come from calc()
              size.mWidth.mLength >= 0) {
            NS_ABORT_IF_FALSE(size.mWidth.mPercent == 0.0f,
                              "Shouldn't have mPercent");
            valX->SetAppUnits(size.mWidth.mLength);
          } else if (size.mWidth.mLength == 0 &&
                     // negative values must have come from calc()
                     size.mWidth.mPercent >= 0.0f) {
            valX->SetPercent(size.mWidth.mPercent);
          } else {
            SetValueToCalc(&size.mWidth, valX);
          }
        }

        if (size.mHeightType == nsStyleBackground::Size::eAuto) {
          valY->SetIdent(eCSSKeyword_auto);
        } else {
          NS_ABORT_IF_FALSE(size.mHeightType ==
                              nsStyleBackground::Size::eLengthPercentage,
                            "bad mHeightType");
          if (!size.mHeight.mHasPercent &&
              // negative values must have come from calc()
              size.mHeight.mLength >= 0) {
            NS_ABORT_IF_FALSE(size.mHeight.mPercent == 0.0f,
                              "Shouldn't have mPercent");
            valY->SetAppUnits(size.mHeight.mLength);
          } else if (size.mHeight.mLength == 0 &&
                     // negative values must have come from calc()
                     size.mHeight.mPercent >= 0.0f) {
            valY->SetPercent(size.mHeight.mPercent);
          } else {
            SetValueToCalc(&size.mHeight, valY);
          }
        }
        break;
      }
    }
  }

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetPaddingTop()
{
  return GetPaddingWidthFor(NS_SIDE_TOP);
}

CSSValue*
nsComputedDOMStyle::DoGetPaddingBottom()
{
  return GetPaddingWidthFor(NS_SIDE_BOTTOM);
}

CSSValue*
nsComputedDOMStyle::DoGetPaddingLeft()
{
  return GetPaddingWidthFor(NS_SIDE_LEFT);
}

CSSValue*
nsComputedDOMStyle::DoGetPaddingRight()
{
  return GetPaddingWidthFor(NS_SIDE_RIGHT);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderCollapse()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleTableBorder()->mBorderCollapse,
                                   nsCSSProps::kBorderCollapseKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetBorderSpacing()
{
  nsDOMCSSValueList *valueList = GetROCSSValueList(false);

  nsROCSSPrimitiveValue* xSpacing = new nsROCSSPrimitiveValue;
  valueList->AppendCSSValue(xSpacing);

  nsROCSSPrimitiveValue* ySpacing = new nsROCSSPrimitiveValue;
  valueList->AppendCSSValue(ySpacing);

  const nsStyleTableBorder *border = StyleTableBorder();
  xSpacing->SetAppUnits(border->mBorderSpacingX);
  ySpacing->SetAppUnits(border->mBorderSpacingY);

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetCaptionSide()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleTableBorder()->mCaptionSide,
                                   nsCSSProps::kCaptionSideKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetEmptyCells()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleTableBorder()->mEmptyCells,
                                   nsCSSProps::kEmptyCellsKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetTableLayout()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleTable()->mLayoutStrategy,
                                   nsCSSProps::kTableLayoutKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetBorderTopStyle()
{
  return GetBorderStyleFor(NS_SIDE_TOP);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderBottomStyle()
{
  return GetBorderStyleFor(NS_SIDE_BOTTOM);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderLeftStyle()
{
  return GetBorderStyleFor(NS_SIDE_LEFT);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderRightStyle()
{
  return GetBorderStyleFor(NS_SIDE_RIGHT);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderBottomColors()
{
  return GetBorderColorsFor(NS_SIDE_BOTTOM);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderLeftColors()
{
  return GetBorderColorsFor(NS_SIDE_LEFT);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderRightColors()
{
  return GetBorderColorsFor(NS_SIDE_RIGHT);
}


CSSValue*
nsComputedDOMStyle::DoGetBorderTopColors()
{
  return GetBorderColorsFor(NS_SIDE_TOP);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderBottomLeftRadius()
{
  return GetEllipseRadii(StyleBorder()->mBorderRadius,
                         NS_CORNER_BOTTOM_LEFT, true);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderBottomRightRadius()
{
  return GetEllipseRadii(StyleBorder()->mBorderRadius,
                         NS_CORNER_BOTTOM_RIGHT, true);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderTopLeftRadius()
{
  return GetEllipseRadii(StyleBorder()->mBorderRadius,
                         NS_CORNER_TOP_LEFT, true);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderTopRightRadius()
{
  return GetEllipseRadii(StyleBorder()->mBorderRadius,
                         NS_CORNER_TOP_RIGHT, true);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderTopWidth()
{
  return GetBorderWidthFor(NS_SIDE_TOP);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderBottomWidth()
{
  return GetBorderWidthFor(NS_SIDE_BOTTOM);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderLeftWidth()
{
  return GetBorderWidthFor(NS_SIDE_LEFT);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderRightWidth()
{
  return GetBorderWidthFor(NS_SIDE_RIGHT);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderTopColor()
{
  return GetBorderColorFor(NS_SIDE_TOP);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderBottomColor()
{
  return GetBorderColorFor(NS_SIDE_BOTTOM);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderLeftColor()
{
  return GetBorderColorFor(NS_SIDE_LEFT);
}

CSSValue*
nsComputedDOMStyle::DoGetBorderRightColor()
{
  return GetBorderColorFor(NS_SIDE_RIGHT);
}

CSSValue*
nsComputedDOMStyle::DoGetMarginTopWidth()
{
  return GetMarginWidthFor(NS_SIDE_TOP);
}

CSSValue*
nsComputedDOMStyle::DoGetMarginBottomWidth()
{
  return GetMarginWidthFor(NS_SIDE_BOTTOM);
}

CSSValue*
nsComputedDOMStyle::DoGetMarginLeftWidth()
{
  return GetMarginWidthFor(NS_SIDE_LEFT);
}

CSSValue*
nsComputedDOMStyle::DoGetMarginRightWidth()
{
  return GetMarginWidthFor(NS_SIDE_RIGHT);
}

CSSValue*
nsComputedDOMStyle::DoGetMarkerOffset()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StyleContent()->mMarkerOffset, false);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetOrient()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mOrient,
                                   nsCSSProps::kOrientKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetOutlineWidth()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  const nsStyleOutline* outline = StyleOutline();

  nscoord width;
  if (outline->GetOutlineStyle() == NS_STYLE_BORDER_STYLE_NONE) {
    NS_ASSERTION(outline->GetOutlineWidth(width) && width == 0,
                 "unexpected width");
    width = 0;
  } else {
#ifdef DEBUG
    bool res =
#endif
      outline->GetOutlineWidth(width);
    NS_ASSERTION(res, "percent outline doesn't exist");
  }
  val->SetAppUnits(width);

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetOutlineStyle()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleOutline()->GetOutlineStyle(),
                                   nsCSSProps::kOutlineStyleKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetOutlineOffset()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetAppUnits(StyleOutline()->mOutlineOffset);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetOutlineRadiusBottomLeft()
{
  return GetEllipseRadii(StyleOutline()->mOutlineRadius,
                         NS_CORNER_BOTTOM_LEFT, false);
}

CSSValue*
nsComputedDOMStyle::DoGetOutlineRadiusBottomRight()
{
  return GetEllipseRadii(StyleOutline()->mOutlineRadius,
                         NS_CORNER_BOTTOM_RIGHT, false);
}

CSSValue*
nsComputedDOMStyle::DoGetOutlineRadiusTopLeft()
{
  return GetEllipseRadii(StyleOutline()->mOutlineRadius,
                         NS_CORNER_TOP_LEFT, false);
}

CSSValue*
nsComputedDOMStyle::DoGetOutlineRadiusTopRight()
{
  return GetEllipseRadii(StyleOutline()->mOutlineRadius,
                         NS_CORNER_TOP_RIGHT, false);
}

CSSValue*
nsComputedDOMStyle::DoGetOutlineColor()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  nscolor color;
  if (!StyleOutline()->GetOutlineColor(color))
    color = StyleColor()->mColor;

  SetToRGBAColor(val, color);
  return val;
}

CSSValue*
nsComputedDOMStyle::GetEllipseRadii(const nsStyleCorners& aRadius,
                                    uint8_t aFullCorner,
                                    bool aIsBorder) // else outline
{
  nsStyleCoord radiusX, radiusY;
  if (mInnerFrame && aIsBorder) {
    nscoord radii[8];
    mInnerFrame->GetBorderRadii(radii);
    radiusX.SetCoordValue(radii[NS_FULL_TO_HALF_CORNER(aFullCorner, false)]);
    radiusY.SetCoordValue(radii[NS_FULL_TO_HALF_CORNER(aFullCorner, true)]);
  } else {
    radiusX = aRadius.Get(NS_FULL_TO_HALF_CORNER(aFullCorner, false));
    radiusY = aRadius.Get(NS_FULL_TO_HALF_CORNER(aFullCorner, true));

    if (mInnerFrame) {
      // We need to convert to absolute coordinates before doing the
      // equality check below.
      nscoord v;

      v = StyleCoordToNSCoord(radiusX,
                              &nsComputedDOMStyle::GetFrameBorderRectWidth,
                              0, true);
      radiusX.SetCoordValue(v);

      v = StyleCoordToNSCoord(radiusY,
                              &nsComputedDOMStyle::GetFrameBorderRectHeight,
                              0, true);
      radiusY.SetCoordValue(v);
    }
  }

  // for compatibility, return a single value if X and Y are equal
  if (radiusX == radiusY) {
    nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;

    SetValueToCoord(val, radiusX, true);

    return val;
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(false);

  nsROCSSPrimitiveValue *valX = new nsROCSSPrimitiveValue;
  valueList->AppendCSSValue(valX);

  nsROCSSPrimitiveValue *valY = new nsROCSSPrimitiveValue;
  valueList->AppendCSSValue(valY);

  SetValueToCoord(valX, radiusX, true);
  SetValueToCoord(valY, radiusY, true);

  return valueList;
}

CSSValue*
nsComputedDOMStyle::GetCSSShadowArray(nsCSSShadowArray* aArray,
                                      const nscolor& aDefaultColor,
                                      bool aIsBoxShadow)
{
  if (!aArray) {
    nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val;
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
  uint32_t shadowValuesLength;
  if (aIsBoxShadow) {
    shadowValues = shadowValuesWithSpread;
    shadowValuesLength = ArrayLength(shadowValuesWithSpread);
  } else {
    shadowValues = shadowValuesNoSpread;
    shadowValuesLength = ArrayLength(shadowValuesNoSpread);
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  for (nsCSSShadowItem *item = aArray->ShadowAt(0),
                   *item_end = item + aArray->Length();
       item < item_end; ++item) {
    nsDOMCSSValueList *itemList = GetROCSSValueList(false);
    valueList->AppendCSSValue(itemList);

    // Color is either the specified shadow color or the foreground color
    nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
    itemList->AppendCSSValue(val);
    nscolor shadowColor;
    if (item->mHasColor) {
      shadowColor = item->mColor;
    } else {
      shadowColor = aDefaultColor;
    }
    SetToRGBAColor(val, shadowColor);

    // Set the offsets, blur radius, and spread if available
    for (uint32_t i = 0; i < shadowValuesLength; ++i) {
      val = new nsROCSSPrimitiveValue;
      itemList->AppendCSSValue(val);
      val->SetAppUnits(item->*(shadowValues[i]));
    }

    if (item->mInset && aIsBoxShadow) {
      // This is an inset box-shadow
      val = new nsROCSSPrimitiveValue;
      itemList->AppendCSSValue(val);
      val->SetIdent(
        nsCSSProps::ValueToKeywordEnum(NS_STYLE_BOX_SHADOW_INSET,
                                       nsCSSProps::kBoxShadowTypeKTable));
    }
  }

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetBoxShadow()
{
  return GetCSSShadowArray(StyleBorder()->mBoxShadow,
                           StyleColor()->mColor,
                           true);
}

CSSValue*
nsComputedDOMStyle::DoGetZIndex()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StylePosition()->mZIndex, false);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetListStyleImage()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  const nsStyleList* list = StyleList();

  if (!list->GetListStyleImage()) {
    val->SetIdent(eCSSKeyword_none);
  } else {
    nsCOMPtr<nsIURI> uri;
    if (list->GetListStyleImage()) {
      list->GetListStyleImage()->GetURI(getter_AddRefs(uri));
    }
    val->SetURI(uri);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetListStylePosition()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleList()->mListStylePosition,
                                   nsCSSProps::kListStylePositionKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetListStyleType()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleList()->mListStyleType,
                                   nsCSSProps::kListStyleKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetImageRegion()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;

  const nsStyleList* list = StyleList();

  if (list->mImageRegion.width <= 0 || list->mImageRegion.height <= 0) {
    val->SetIdent(eCSSKeyword_auto);
  } else {
    // create the cssvalues for the sides, stick them in the rect object
    nsROCSSPrimitiveValue *topVal    = new nsROCSSPrimitiveValue;
    nsROCSSPrimitiveValue *rightVal  = new nsROCSSPrimitiveValue;
    nsROCSSPrimitiveValue *bottomVal = new nsROCSSPrimitiveValue;
    nsROCSSPrimitiveValue *leftVal   = new nsROCSSPrimitiveValue;
    nsDOMCSSRect * domRect = new nsDOMCSSRect(topVal, rightVal,
                                              bottomVal, leftVal);
    topVal->SetAppUnits(list->mImageRegion.y);
    rightVal->SetAppUnits(list->mImageRegion.width + list->mImageRegion.x);
    bottomVal->SetAppUnits(list->mImageRegion.height + list->mImageRegion.y);
    leftVal->SetAppUnits(list->mImageRegion.x);
    val->SetRect(domRect);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetLineHeight()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;

  nscoord lineHeight;
  if (GetLineHeightCoord(lineHeight)) {
    val->SetAppUnits(lineHeight);
  } else {
    SetValueToCoord(val, StyleText()->mLineHeight, true,
                    nullptr, nsCSSProps::kLineHeightKTable);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetVerticalAlign()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StyleTextReset()->mVerticalAlign, false,
                  &nsComputedDOMStyle::GetLineHeightCoord,
                  nsCSSProps::kVerticalAlignKTable);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetTextAlign()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleText()->mTextAlign,
                                   nsCSSProps::kTextAlignKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetTextAlignLast()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleText()->mTextAlignLast,
                                   nsCSSProps::kTextAlignLastKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetMozTextBlink()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleTextReset()->mTextBlink,
                                   nsCSSProps::kTextBlinkKTable));

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetTextDecoration()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  const nsStyleTextReset* textReset = StyleTextReset();

  // If decoration style or color wasn't initial value, the author knew the
  // text-decoration is a shorthand property in CSS 3.
  // Return NULL in such cases.
  if (textReset->GetDecorationStyle() != NS_STYLE_TEXT_DECORATION_STYLE_SOLID) {
    return nullptr;
  }

  nscolor color;
  bool isForegroundColor;
  textReset->GetDecorationColor(color, isForegroundColor);
  if (!isForegroundColor) {
    return nullptr;
  }

  // Otherwise, the web pages may have been written for CSS 2.1 or earlier,
  // i.e., text-decoration was assumed as a longhand property.  In that case,
  // we should return computed value same as CSS 2.1 for backward compatibility.

  uint8_t line = textReset->mTextDecorationLine;
  // Clear the -moz-anchor-decoration bit and the OVERRIDE_ALL bits -- we
  // don't want these to appear in the computed style.
  line &= ~(NS_STYLE_TEXT_DECORATION_LINE_PREF_ANCHORS |
            NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL);
  uint8_t blink = textReset->mTextBlink;

  if (blink == NS_STYLE_TEXT_BLINK_NONE &&
      line == NS_STYLE_TEXT_DECORATION_LINE_NONE) {
    val->SetIdent(eCSSKeyword_none);
  } else {
    nsAutoString str;
    if (line != NS_STYLE_TEXT_DECORATION_LINE_NONE) {
      nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_text_decoration_line,
        line, NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE,
        NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH, str);
    }
    if (blink != NS_STYLE_TEXT_BLINK_NONE) {
      if (!str.IsEmpty()) {
        str.Append(PRUnichar(' '));
      }
      nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_text_blink, blink,
        NS_STYLE_TEXT_BLINK_BLINK, NS_STYLE_TEXT_BLINK_BLINK, str);
    }
    val->SetString(str);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetTextDecorationColor()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  nscolor color;
  bool isForeground;
  StyleTextReset()->GetDecorationColor(color, isForeground);
  if (isForeground) {
    color = StyleColor()->mColor;
  }

  SetToRGBAColor(val, color);

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetTextDecorationLine()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  int32_t intValue = StyleTextReset()->mTextDecorationLine;

  if (NS_STYLE_TEXT_DECORATION_LINE_NONE == intValue) {
    val->SetIdent(eCSSKeyword_none);
  } else {
    nsAutoString decorationLineString;
    // Clear the -moz-anchor-decoration bit and the OVERRIDE_ALL bits -- we
    // don't want these to appear in the computed style.
    intValue &= ~(NS_STYLE_TEXT_DECORATION_LINE_PREF_ANCHORS |
                  NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL);
    nsStyleUtil::AppendBitmaskCSSValue(eCSSProperty_text_decoration_line,
      intValue, NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE,
      NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH, decorationLineString);
    val->SetString(decorationLineString);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetTextDecorationStyle()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleTextReset()->GetDecorationStyle(),
                                   nsCSSProps::kTextDecorationStyleKTable));

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetTextIndent()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StyleText()->mTextIndent, false,
                  &nsComputedDOMStyle::GetCBContentWidth);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetTextOverflow()
{
  const nsStyleTextReset *style = StyleTextReset();
  nsROCSSPrimitiveValue *first = new nsROCSSPrimitiveValue;
  const nsStyleTextOverflowSide *side = style->mTextOverflow.GetFirstValue();
  if (side->mType == NS_STYLE_TEXT_OVERFLOW_STRING) {
    nsString str;
    nsStyleUtil::AppendEscapedCSSString(side->mString, str);
    first->SetString(str);
  } else {
    first->SetIdent(
      nsCSSProps::ValueToKeywordEnum(side->mType,
                                     nsCSSProps::kTextOverflowKTable));
  }
  side = style->mTextOverflow.GetSecondValue();
  if (!side) {
    return first;
  }
  nsROCSSPrimitiveValue *second = new nsROCSSPrimitiveValue;
  if (side->mType == NS_STYLE_TEXT_OVERFLOW_STRING) {
    nsString str;
    nsStyleUtil::AppendEscapedCSSString(side->mString, str);
    second->SetString(str);
  } else {
    second->SetIdent(
      nsCSSProps::ValueToKeywordEnum(side->mType,
                                     nsCSSProps::kTextOverflowKTable));
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(false);
  valueList->AppendCSSValue(first);
  valueList->AppendCSSValue(second);
  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetTextShadow()
{
  return GetCSSShadowArray(StyleText()->mTextShadow,
                           StyleColor()->mColor,
                           false);
}

CSSValue*
nsComputedDOMStyle::DoGetTextTransform()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleText()->mTextTransform,
                                   nsCSSProps::kTextTransformKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetTabSize()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleText()->mTabSize);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetLetterSpacing()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StyleText()->mLetterSpacing, false);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetWordSpacing()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetAppUnits(StyleText()->mWordSpacing);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetWhiteSpace()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleText()->mWhiteSpace,
                                   nsCSSProps::kWhitespaceKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetWindowShadow()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleUIReset()->mWindowShadow,
                                   nsCSSProps::kWindowShadowKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetWordBreak()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleText()->mWordBreak,
                                   nsCSSProps::kWordBreakKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetWordWrap()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleText()->mWordWrap,
                                   nsCSSProps::kWordWrapKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetHyphens()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleText()->mHyphens,
                                   nsCSSProps::kHyphensKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetTextSizeAdjust()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  switch (StyleText()->mTextSizeAdjust) {
    default:
      NS_NOTREACHED("unexpected value");
      // fall through
    case NS_STYLE_TEXT_SIZE_ADJUST_AUTO:
      val->SetIdent(eCSSKeyword_auto);
      break;
    case NS_STYLE_TEXT_SIZE_ADJUST_NONE:
      val->SetIdent(eCSSKeyword_none);
      break;
  }
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetPointerEvents()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleVisibility()->mPointerEvents,
                                   nsCSSProps::kPointerEventsKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetVisibility()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleVisibility()->mVisible,
                                               nsCSSProps::kVisibilityKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetWritingMode()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleVisibility()->mWritingMode,
                                   nsCSSProps::kWritingModeKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetDirection()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleVisibility()->mDirection,
                                   nsCSSProps::kDirectionKTable));
  return val;
}

MOZ_STATIC_ASSERT(NS_STYLE_UNICODE_BIDI_NORMAL == 0,
                  "unicode-bidi style constants not as expected");

CSSValue*
nsComputedDOMStyle::DoGetUnicodeBidi()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleTextReset()->mUnicodeBidi,
                                   nsCSSProps::kUnicodeBidiKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetCursor()
{
  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  const nsStyleUserInterface *ui = StyleUserInterface();

  for (nsCursorImage *item = ui->mCursorArray,
         *item_end = ui->mCursorArray + ui->mCursorArrayLength;
       item < item_end; ++item) {
    nsDOMCSSValueList *itemList = GetROCSSValueList(false);
    valueList->AppendCSSValue(itemList);

    nsCOMPtr<nsIURI> uri;
    item->GetImage()->GetURI(getter_AddRefs(uri));

    nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
    itemList->AppendCSSValue(val);
    val->SetURI(uri);

    if (item->mHaveHotspot) {
      nsROCSSPrimitiveValue *valX = new nsROCSSPrimitiveValue;
      itemList->AppendCSSValue(valX);
      nsROCSSPrimitiveValue *valY = new nsROCSSPrimitiveValue;
      itemList->AppendCSSValue(valY);

      valX->SetNumber(item->mHotspotX);
      valY->SetNumber(item->mHotspotY);
    }
  }

  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(ui->mCursor,
                                               nsCSSProps::kCursorKTable));
  valueList->AppendCSSValue(val);
  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetAppearance()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mAppearance,
                                               nsCSSProps::kAppearanceKTable));
  return val;
}


CSSValue*
nsComputedDOMStyle::DoGetBoxAlign()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleXUL()->mBoxAlign,
                                               nsCSSProps::kBoxAlignKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetBoxDirection()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleXUL()->mBoxDirection,
                                   nsCSSProps::kBoxDirectionKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetBoxFlex()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleXUL()->mBoxFlex);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetBoxOrdinalGroup()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleXUL()->mBoxOrdinal);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetBoxOrient()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleXUL()->mBoxOrient,
                                   nsCSSProps::kBoxOrientKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetBoxPack()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleXUL()->mBoxPack,
                                               nsCSSProps::kBoxPackKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetBoxSizing()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StylePosition()->mBoxSizing,
                                   nsCSSProps::kBoxSizingKTable));
  return val;
}

/* Border image properties */

CSSValue*
nsComputedDOMStyle::DoGetBorderImageSource()
{
  const nsStyleBorder* border = StyleBorder();

  imgIRequest* imgSrc = border->GetBorderImage();
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  if (imgSrc) {
    nsCOMPtr<nsIURI> uri;
    imgSrc->GetURI(getter_AddRefs(uri));
    val->SetURI(uri);
  } else {
    val->SetIdent(eCSSKeyword_none);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetBorderImageSlice()
{
  nsDOMCSSValueList* valueList = GetROCSSValueList(false);

  const nsStyleBorder* border = StyleBorder();
  // Four slice numbers.
  NS_FOR_CSS_SIDES (side) {
    nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(val);
    SetValueToCoord(val, border->mBorderImageSlice.Get(side), true, nullptr);
  }

  // Fill keyword.
  if (NS_STYLE_BORDER_IMAGE_SLICE_FILL == border->mBorderImageFill) {
    nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(val);
    val->SetIdent(eCSSKeyword_fill);
  }

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetBorderImageWidth()
{
  const nsStyleBorder* border = StyleBorder();
  nsDOMCSSValueList* valueList = GetROCSSValueList(false);
  NS_FOR_CSS_SIDES (side) {
    nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(val);
    SetValueToCoord(val, border->mBorderImageWidth.Get(side),
                    true, nullptr);
  }

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetBorderImageOutset()
{
  nsDOMCSSValueList *valueList = GetROCSSValueList(false);

  const nsStyleBorder* border = StyleBorder();
  // four slice numbers
  NS_FOR_CSS_SIDES (side) {
    nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(val);
    SetValueToCoord(val, border->mBorderImageOutset.Get(side),
                    true, nullptr);
  }

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetBorderImageRepeat()
{
  nsDOMCSSValueList* valueList = GetROCSSValueList(false);

  const nsStyleBorder* border = StyleBorder();

  // horizontal repeat
  nsROCSSPrimitiveValue* valX = new nsROCSSPrimitiveValue;
  valueList->AppendCSSValue(valX);
  valX->SetIdent(
    nsCSSProps::ValueToKeywordEnum(border->mBorderImageRepeatH,
                                   nsCSSProps::kBorderImageRepeatKTable));

  // vertical repeat
  nsROCSSPrimitiveValue* valY = new nsROCSSPrimitiveValue;
  valueList->AppendCSSValue(valY);
  valY->SetIdent(
    nsCSSProps::ValueToKeywordEnum(border->mBorderImageRepeatV,
                                   nsCSSProps::kBorderImageRepeatKTable));
  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetAlignItems()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StylePosition()->mAlignItems,
                                   nsCSSProps::kAlignItemsKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetAlignSelf()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  uint8_t computedAlignSelf = StylePosition()->mAlignSelf;

  if (computedAlignSelf == NS_STYLE_ALIGN_SELF_AUTO) {
    // "align-self: auto" needs to compute to parent's align-items value.
    nsStyleContext* parentStyleContext = mStyleContextHolder->GetParent();
    if (parentStyleContext) {
      computedAlignSelf =
        parentStyleContext->StylePosition()->mAlignItems;
    } else {
      // No parent --> use default.
      computedAlignSelf = NS_STYLE_ALIGN_ITEMS_INITIAL_VALUE;
    }
  }

  NS_ABORT_IF_FALSE(computedAlignSelf != NS_STYLE_ALIGN_SELF_AUTO,
                    "Should have swapped out 'auto' for something non-auto");
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(computedAlignSelf,
                                   nsCSSProps::kAlignSelfKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFlexBasis()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  // XXXdholbert We could make this more automagic and resolve percentages
  // if we wanted, by passing in a PercentageBaseGetter instead of nullptr
  // below.  Logic would go like this:
  //   if (i'm a flex item) {
  //     if (my flex container is horizontal) {
  //       percentageBaseGetter = &nsComputedDOMStyle::GetCBContentWidth;
  //     } else {
  //       percentageBaseGetter = &nsComputedDOMStyle::GetCBContentHeight;
  //     }
  //   }

  SetValueToCoord(val, StylePosition()->mFlexBasis, true,
                  nullptr, nsCSSProps::kWidthKTable);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFlexDirection()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StylePosition()->mFlexDirection,
                                   nsCSSProps::kFlexDirectionKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFlexGrow()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetNumber(StylePosition()->mFlexGrow);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFlexShrink()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetNumber(StylePosition()->mFlexShrink);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetOrder()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetNumber(StylePosition()->mOrder);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetJustifyContent()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StylePosition()->mJustifyContent,
                                   nsCSSProps::kJustifyContentKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFloatEdge()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleBorder()->mFloatEdge,
                                   nsCSSProps::kFloatEdgeKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetForceBrokenImageIcon()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleUIReset()->mForceBrokenImageIcon);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetIMEMode()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleUIReset()->mIMEMode,
                                   nsCSSProps::kIMEModeKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetUserFocus()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleUserInterface()->mUserFocus,
                                   nsCSSProps::kUserFocusKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetUserInput()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleUserInterface()->mUserInput,
                                   nsCSSProps::kUserInputKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetUserModify()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleUserInterface()->mUserModify,
                                   nsCSSProps::kUserModifyKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetUserSelect()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleUIReset()->mUserSelect,
                                   nsCSSProps::kUserSelectKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetDisplay()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mDisplay,
                                               nsCSSProps::kDisplayKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetPosition()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mPosition,
                                               nsCSSProps::kPositionKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetClip()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;

  const nsStyleDisplay* display = StyleDisplay();

  if (display->mClipFlags == NS_STYLE_CLIP_AUTO) {
    val->SetIdent(eCSSKeyword_auto);
  } else {
    // create the cssvalues for the sides, stick them in the rect object
    nsROCSSPrimitiveValue *topVal    = new nsROCSSPrimitiveValue;
    nsROCSSPrimitiveValue *rightVal  = new nsROCSSPrimitiveValue;
    nsROCSSPrimitiveValue *bottomVal = new nsROCSSPrimitiveValue;
    nsROCSSPrimitiveValue *leftVal   = new nsROCSSPrimitiveValue;
    nsDOMCSSRect * domRect = new nsDOMCSSRect(topVal, rightVal,
                                              bottomVal, leftVal);
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
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetOverflow()
{
  const nsStyleDisplay* display = StyleDisplay();

  if (display->mOverflowX != display->mOverflowY) {
    // No value to return.  We can't express this combination of
    // values as a shorthand.
    return nullptr;
  }

  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(display->mOverflowX,
                                               nsCSSProps::kOverflowKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetOverflowX()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mOverflowX,
                                   nsCSSProps::kOverflowSubKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetOverflowY()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mOverflowY,
                                   nsCSSProps::kOverflowSubKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetResize()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mResize,
                                               nsCSSProps::kResizeKTable));
  return val;
}


CSSValue*
nsComputedDOMStyle::DoGetPageBreakAfter()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;

  const nsStyleDisplay *display = StyleDisplay();

  if (display->mBreakAfter) {
    val->SetIdent(eCSSKeyword_always);
  } else {
    val->SetIdent(eCSSKeyword_auto);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetPageBreakBefore()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;

  const nsStyleDisplay *display = StyleDisplay();

  if (display->mBreakBefore) {
    val->SetIdent(eCSSKeyword_always);
  } else {
    val->SetIdent(eCSSKeyword_auto);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetPageBreakInside()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleDisplay()->mBreakInside,
                                               nsCSSProps::kPageBreakInsideKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetHeight()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;

  bool calcHeight = false;

  if (mInnerFrame) {
    calcHeight = true;

    const nsStyleDisplay* displayData = StyleDisplay();
    if (displayData->mDisplay == NS_STYLE_DISPLAY_INLINE &&
        !(mInnerFrame->IsFrameOfType(nsIFrame::eReplaced))) {
      calcHeight = false;
    }
  }

  if (calcHeight) {
    AssertFlushedPendingReflows();
    nsMargin adjustedValues = GetAdjustedValuesForBoxSizing();
    val->SetAppUnits(mInnerFrame->GetContentRect().height +
      adjustedValues.TopBottom());
  } else {
    const nsStylePosition *positionData = StylePosition();

    nscoord minHeight =
      StyleCoordToNSCoord(positionData->mMinHeight,
                          &nsComputedDOMStyle::GetCBContentHeight, 0, true);

    nscoord maxHeight =
      StyleCoordToNSCoord(positionData->mMaxHeight,
                          &nsComputedDOMStyle::GetCBContentHeight,
                          nscoord_MAX, true);

    SetValueToCoord(val, positionData->mHeight, true, nullptr, nullptr,
                    minHeight, maxHeight);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetWidth()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;

  bool calcWidth = false;

  if (mInnerFrame) {
    calcWidth = true;

    const nsStyleDisplay *displayData = StyleDisplay();
    if (displayData->mDisplay == NS_STYLE_DISPLAY_INLINE &&
        !(mInnerFrame->IsFrameOfType(nsIFrame::eReplaced))) {
      calcWidth = false;
    }
  }

  if (calcWidth) {
    AssertFlushedPendingReflows();
    nsMargin adjustedValues = GetAdjustedValuesForBoxSizing();
    val->SetAppUnits(mInnerFrame->GetContentRect().width +
      adjustedValues.LeftRight());
  } else {
    const nsStylePosition *positionData = StylePosition();

    nscoord minWidth =
      StyleCoordToNSCoord(positionData->mMinWidth,
                          &nsComputedDOMStyle::GetCBContentWidth, 0, true);

    nscoord maxWidth =
      StyleCoordToNSCoord(positionData->mMaxWidth,
                          &nsComputedDOMStyle::GetCBContentWidth,
                          nscoord_MAX, true);

    SetValueToCoord(val, positionData->mWidth, true, nullptr,
                    nsCSSProps::kWidthKTable, minWidth, maxWidth);
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetMaxHeight()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StylePosition()->mMaxHeight, true,
                  &nsComputedDOMStyle::GetCBContentHeight);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetMaxWidth()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StylePosition()->mMaxWidth, true,
                  &nsComputedDOMStyle::GetCBContentWidth,
                  nsCSSProps::kWidthKTable);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetMinHeight()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StylePosition()->mMinHeight, true,
                  &nsComputedDOMStyle::GetCBContentHeight);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetMinWidth()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StylePosition()->mMinWidth, true,
                  &nsComputedDOMStyle::GetCBContentWidth,
                  nsCSSProps::kWidthKTable);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetLeft()
{
  return GetOffsetWidthFor(NS_SIDE_LEFT);
}

CSSValue*
nsComputedDOMStyle::DoGetRight()
{
  return GetOffsetWidthFor(NS_SIDE_RIGHT);
}

CSSValue*
nsComputedDOMStyle::DoGetTop()
{
  return GetOffsetWidthFor(NS_SIDE_TOP);
}

nsDOMCSSValueList*
nsComputedDOMStyle::GetROCSSValueList(bool aCommaDelimited)
{
  nsDOMCSSValueList *valueList = new nsDOMCSSValueList(aCommaDelimited, true);
  NS_ASSERTION(valueList != 0, "ran out of memory");

  return valueList;
}

CSSValue*
nsComputedDOMStyle::GetOffsetWidthFor(mozilla::css::Side aSide)
{
  const nsStyleDisplay* display = StyleDisplay();

  AssertFlushedPendingReflows();

  uint8_t position = display->mPosition;
  if (!mOuterFrame) {
    // GetRelativeOffset and GetAbsoluteOffset don't handle elements
    // without frames in any sensible way.  GetStaticOffset, however,
    // is perfect for that case.
    position = NS_STYLE_POSITION_STATIC;
  }

  switch (position) {
    case NS_STYLE_POSITION_STATIC:
      return GetStaticOffset(aSide);
    case NS_STYLE_POSITION_RELATIVE:
      return GetRelativeOffset(aSide);
    case NS_STYLE_POSITION_ABSOLUTE:
    case NS_STYLE_POSITION_FIXED:
      return GetAbsoluteOffset(aSide);
    default:
      NS_ERROR("Invalid position");
      return nullptr;
  }
}

CSSValue*
nsComputedDOMStyle::GetAbsoluteOffset(mozilla::css::Side aSide)
{
  MOZ_ASSERT(mOuterFrame, "need a frame, so we can call GetContainingBlock()");

  nsIFrame* container = mOuterFrame->GetContainingBlock();
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
    nsIFrame* scrollingChild = container->GetFirstPrincipalChild();
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

  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetAppUnits(offset);
  return val;
}

MOZ_STATIC_ASSERT(NS_SIDE_TOP == 0 && NS_SIDE_RIGHT == 1 &&
                  NS_SIDE_BOTTOM == 2 && NS_SIDE_LEFT == 3,
                  "box side constants not as expected for NS_OPPOSITE_SIDE");
#define NS_OPPOSITE_SIDE(s_) mozilla::css::Side(((s_) + 2) & 3)

CSSValue*
nsComputedDOMStyle::GetRelativeOffset(mozilla::css::Side aSide)
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;

  const nsStylePosition* positionData = StylePosition();
  int32_t sign = 1;
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

  val->SetAppUnits(sign * StyleCoordToNSCoord(coord, baseGetter, 0, false));
  return val;
}

CSSValue*
nsComputedDOMStyle::GetStaticOffset(mozilla::css::Side aSide)

{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StylePosition()->mOffset.Get(aSide), false);
  return val;
}

CSSValue*
nsComputedDOMStyle::GetPaddingWidthFor(mozilla::css::Side aSide)
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  if (!mInnerFrame) {
    SetValueToCoord(val, StylePadding()->mPadding.Get(aSide), true);
  } else {
    AssertFlushedPendingReflows();

    val->SetAppUnits(mInnerFrame->GetUsedPadding().Side(aSide));
  }

  return val;
}

bool
nsComputedDOMStyle::GetLineHeightCoord(nscoord& aCoord)
{
  AssertFlushedPendingReflows();

  nscoord blockHeight = NS_AUTOHEIGHT;
  if (StyleText()->mLineHeight.GetUnit() == eStyleUnit_Enumerated) {
    if (!mInnerFrame)
      return false;

    if (nsLayoutUtils::IsNonWrapperBlock(mInnerFrame)) {
      blockHeight = mInnerFrame->GetContentRect().height;
    } else {
      GetCBContentHeight(blockHeight);
    }
  }

  // lie about font size inflation since we lie about font size (since
  // the inflation only applies to text)
  aCoord = nsHTMLReflowState::CalcLineHeight(mStyleContextHolder,
                                             blockHeight, 1.0f);

  // CalcLineHeight uses font->mFont.size, but we want to use
  // font->mSize as the font size.  Adjust for that.  Also adjust for
  // the text zoom, if any.
  const nsStyleFont* font = StyleFont();
  float fCoord = float(aCoord);
  if (font->mAllowZoom) {
    fCoord /= mPresShell->GetPresContext()->TextZoom();
  }
  if (font->mFont.size != font->mSize) {
    fCoord = fCoord * (float(font->mSize) / float(font->mFont.size));
  }
  aCoord = NSToCoordRound(fCoord);

  return true;
}

CSSValue*
nsComputedDOMStyle::GetBorderColorsFor(mozilla::css::Side aSide)
{
  const nsStyleBorder *border = StyleBorder();

  if (border->mBorderColors) {
    nsBorderColors* borderColors = border->mBorderColors[aSide];
    if (borderColors) {
      nsDOMCSSValueList *valueList = GetROCSSValueList(false);

      do {
        nsROCSSPrimitiveValue *primitive = new nsROCSSPrimitiveValue;

        SetToRGBAColor(primitive, borderColors->mColor);

        valueList->AppendCSSValue(primitive);
        borderColors = borderColors->mNext;
      } while (borderColors);

      return valueList;
    }
  }

  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(eCSSKeyword_none);
  return val;
}

CSSValue*
nsComputedDOMStyle::GetBorderWidthFor(mozilla::css::Side aSide)
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  nscoord width;
  if (mInnerFrame) {
    AssertFlushedPendingReflows();
    width = mInnerFrame->GetUsedBorder().Side(aSide);
  } else {
    width = StyleBorder()->GetComputedBorderWidth(aSide);
  }
  val->SetAppUnits(width);

  return val;
}

CSSValue*
nsComputedDOMStyle::GetBorderColorFor(mozilla::css::Side aSide)
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  nscolor color;
  bool foreground;
  StyleBorder()->GetBorderColor(aSide, color, foreground);
  if (foreground) {
    color = StyleColor()->mColor;
  }

  SetToRGBAColor(val, color);
  return val;
}

CSSValue*
nsComputedDOMStyle::GetMarginWidthFor(mozilla::css::Side aSide)
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  if (!mInnerFrame) {
    SetValueToCoord(val, StyleMargin()->mMargin.Get(aSide), false);
  } else {
    AssertFlushedPendingReflows();

    // For tables, GetUsedMargin always returns an empty margin, so we
    // should read the margin from the outer table frame instead.
    val->SetAppUnits(mOuterFrame->GetUsedMargin().Side(aSide));
    NS_ASSERTION(mOuterFrame == mInnerFrame ||
                 mInnerFrame->GetUsedMargin() == nsMargin(0, 0, 0, 0),
                 "Inner tables must have zero margins");
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::GetBorderStyleFor(mozilla::css::Side aSide)
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleBorder()->GetBorderStyle(aSide),
                                   nsCSSProps::kBorderStyleKTable));
  return val;
}

void
nsComputedDOMStyle::SetValueToCoord(nsROCSSPrimitiveValue* aValue,
                                    const nsStyleCoord& aCoord,
                                    bool aClampNegativeCalc,
                                    PercentageBaseGetter aPercentageBaseGetter,
                                    const int32_t aTable[],
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
          aValue->SetAppUnits(std::max(aMinAppUnits, std::min(val, aMaxAppUnits)));
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
        aValue->SetAppUnits(std::max(aMinAppUnits, std::min(val, aMaxAppUnits)));
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
        aValue->SetAppUnits(std::max(aMinAppUnits, std::min(val, aMaxAppUnits)));
      } else if (aPercentageBaseGetter &&
                 (this->*aPercentageBaseGetter)(percentageBase)) {
        nscoord val =
          nsRuleNode::ComputeCoordPercentCalc(aCoord, percentageBase);
        if (aClampNegativeCalc && val < 0) {
          NS_ABORT_IF_FALSE(aCoord.IsCalcUnit(),
                            "parser should have rejected value");
          val = 0;
        }
        aValue->SetAppUnits(std::max(aMinAppUnits, std::min(val, aMaxAppUnits)));
      } else {
        nsStyleCoord::Calc *calc = aCoord.GetCalcValue();
        SetValueToCalc(calc, aValue);
      }
      break;

    case eStyleUnit_Degree:
      aValue->SetDegree(aCoord.GetAngleValue());
      break;

    case eStyleUnit_Grad:
      aValue->SetGrad(aCoord.GetAngleValue());
      break;

    case eStyleUnit_Radian:
      aValue->SetRadian(aCoord.GetAngleValue());
      break;

    case eStyleUnit_Turn:
      aValue->SetTurn(aCoord.GetAngleValue());
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
                                        bool aClampNegativeCalc)
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

bool
nsComputedDOMStyle::GetCBContentWidth(nscoord& aWidth)
{
  if (!mOuterFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  nsIFrame* container = mOuterFrame->GetContainingBlock();
  aWidth = container->GetContentRect().width;
  return true;
}

bool
nsComputedDOMStyle::GetCBContentHeight(nscoord& aHeight)
{
  if (!mOuterFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  nsIFrame* container = mOuterFrame->GetContainingBlock();
  aHeight = container->GetContentRect().height;
  return true;
}

bool
nsComputedDOMStyle::GetFrameBorderRectWidth(nscoord& aWidth)
{
  if (!mInnerFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  aWidth = mInnerFrame->GetSize().width;
  return true;
}

bool
nsComputedDOMStyle::GetFrameBorderRectHeight(nscoord& aHeight)
{
  if (!mInnerFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  aHeight = mInnerFrame->GetSize().height;
  return true;
}

bool
nsComputedDOMStyle::GetFrameBoundsWidthForTransform(nscoord& aWidth)
{
  // We need a frame to work with.
  if (!mInnerFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  aWidth = nsDisplayTransform::GetFrameBoundsForTransform(mInnerFrame).width;
  return true;
}

bool
nsComputedDOMStyle::GetFrameBoundsHeightForTransform(nscoord& aHeight)
{
  // We need a frame to work with.
  if (!mInnerFrame) {
    return false;
  }

  AssertFlushedPendingReflows();

  aHeight = nsDisplayTransform::GetFrameBoundsForTransform(mInnerFrame).height;
  return true;
}

CSSValue*
nsComputedDOMStyle::GetSVGPaintFor(bool aFill)
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  const nsStyleSVG* svg = StyleSVG();
  const nsStyleSVGPaint* paint = nullptr;

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
      SetToRGBAColor(val, paint->mPaint.mColor);
      break;
    }
    case eStyleSVGPaintType_Server:
    {
      nsDOMCSSValueList *valueList = GetROCSSValueList(false);
      valueList->AppendCSSValue(val);

      nsROCSSPrimitiveValue* fallback = new nsROCSSPrimitiveValue;
      valueList->AppendCSSValue(fallback);

      val->SetURI(paint->mPaint.mPaintServer);
      SetToRGBAColor(fallback, paint->mFallbackColor);
      return valueList;
    }
    case eStyleSVGPaintType_ObjectFill:
    {
      val->SetIdent(eCSSKeyword__moz_objectfill);
      break;
    }
    case eStyleSVGPaintType_ObjectStroke:
    {
      val->SetIdent(eCSSKeyword__moz_objectstroke);
      break;
    }
  }

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFill()
{
  return GetSVGPaintFor(true);
}

CSSValue*
nsComputedDOMStyle::DoGetStroke()
{
  return GetSVGPaintFor(false);
}

CSSValue*
nsComputedDOMStyle::DoGetMarkerEnd()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  const nsStyleSVG* svg = StyleSVG();

  if (svg->mMarkerEnd)
    val->SetURI(svg->mMarkerEnd);
  else
    val->SetIdent(eCSSKeyword_none);

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetMarkerMid()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  const nsStyleSVG* svg = StyleSVG();

  if (svg->mMarkerMid)
    val->SetURI(svg->mMarkerMid);
  else
    val->SetIdent(eCSSKeyword_none);

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetMarkerStart()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  const nsStyleSVG* svg = StyleSVG();

  if (svg->mMarkerStart)
    val->SetURI(svg->mMarkerStart);
  else
    val->SetIdent(eCSSKeyword_none);

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetStrokeDasharray()
{
  const nsStyleSVG* svg = StyleSVG();

  if (!svg->mStrokeDasharrayLength || !svg->mStrokeDasharray) {
    nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
    val->SetIdent(eCSSKeyword_none);
    return val;
  }

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  for (uint32_t i = 0; i < svg->mStrokeDasharrayLength; i++) {
    nsROCSSPrimitiveValue* dash = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(dash);

    SetValueToCoord(dash, svg->mStrokeDasharray[i], true);
  }

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetStrokeDashoffset()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StyleSVG()->mStrokeDashoffset, false);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetStrokeWidth()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;
  SetValueToCoord(val, StyleSVG()->mStrokeWidth, true);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetVectorEffect()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(StyleSVGReset()->mVectorEffect,
                                               nsCSSProps::kVectorEffectKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFillOpacity()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleSVG()->mFillOpacity);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFloodOpacity()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleSVGReset()->mFloodOpacity);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetStopOpacity()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleSVGReset()->mStopOpacity);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetStrokeMiterlimit()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleSVG()->mStrokeMiterlimit);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetStrokeOpacity()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetNumber(StyleSVG()->mStrokeOpacity);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetClipRule()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(
                  StyleSVG()->mClipRule, nsCSSProps::kFillRuleKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFillRule()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(nsCSSProps::ValueToKeywordEnum(
                  StyleSVG()->mFillRule, nsCSSProps::kFillRuleKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetStrokeLinecap()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVG()->mStrokeLinecap,
                                   nsCSSProps::kStrokeLinecapKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetStrokeLinejoin()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVG()->mStrokeLinejoin,
                                   nsCSSProps::kStrokeLinejoinKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetTextAnchor()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVG()->mTextAnchor,
                                   nsCSSProps::kTextAnchorKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetColorInterpolation()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVG()->mColorInterpolation,
                                   nsCSSProps::kColorInterpolationKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetColorInterpolationFilters()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVG()->mColorInterpolationFilters,
                                   nsCSSProps::kColorInterpolationKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetDominantBaseline()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVGReset()->mDominantBaseline,
                                   nsCSSProps::kDominantBaselineKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetImageRendering()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVG()->mImageRendering,
                                   nsCSSProps::kImageRenderingKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetShapeRendering()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVG()->mShapeRendering,
                                   nsCSSProps::kShapeRenderingKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetTextRendering()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVG()->mTextRendering,
                                   nsCSSProps::kTextRenderingKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetFloodColor()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  SetToRGBAColor(val, StyleSVGReset()->mFloodColor);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetLightingColor()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  SetToRGBAColor(val, StyleSVGReset()->mLightingColor);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetStopColor()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  SetToRGBAColor(val, StyleSVGReset()->mStopColor);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetClipPath()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  const nsStyleSVGReset* svg = StyleSVGReset();

  if (svg->mClipPath)
    val->SetURI(svg->mClipPath);
  else
    val->SetIdent(eCSSKeyword_none);

  return val;
}

void
nsComputedDOMStyle::SetCssTextToCoord(nsAString& aCssText,
                                      const nsStyleCoord& aCoord)
{
  nsROCSSPrimitiveValue* value = new nsROCSSPrimitiveValue;
  bool clampNegativeCalc = true;
  SetValueToCoord(value, aCoord, clampNegativeCalc);
  value->GetCssText(aCssText);
  delete value;
}

static void
GetFilterFunctionName(nsAString& aString, nsStyleFilter::Type mType)
{
  switch (mType) {
    case nsStyleFilter::Type::eBlur:
      aString.AssignLiteral("blur(");
      break;
    case nsStyleFilter::Type::eBrightness:
      aString.AssignLiteral("brightness(");
      break;
    case nsStyleFilter::Type::eContrast:
      aString.AssignLiteral("contrast(");
      break;
    case nsStyleFilter::Type::eGrayscale:
      aString.AssignLiteral("grayscale(");
      break;
    case nsStyleFilter::Type::eHueRotate:
      aString.AssignLiteral("hue-rotate(");
      break;
    case nsStyleFilter::Type::eInvert:
      aString.AssignLiteral("invert(");
      break;
    case nsStyleFilter::Type::eOpacity:
      aString.AssignLiteral("opacity(");
      break;
    case nsStyleFilter::Type::eSaturate:
      aString.AssignLiteral("saturate(");
      break;
    case nsStyleFilter::Type::eSepia:
      aString.AssignLiteral("sepia(");
      break;
    default:
      NS_NOTREACHED("unrecognized filter type");
  }
}

nsROCSSPrimitiveValue*
nsComputedDOMStyle::CreatePrimitiveValueForStyleFilter(
  const nsStyleFilter& aStyleFilter)
{
  nsROCSSPrimitiveValue* value = new nsROCSSPrimitiveValue;

  // Handle url().
  if (nsStyleFilter::Type::eURL == aStyleFilter.mType) {
    value->SetURI(aStyleFilter.mURL);
    return value;
  }

  // Filter function name and opening parenthesis.
  nsAutoString filterFunctionString;
  GetFilterFunctionName(filterFunctionString, aStyleFilter.mType);

  // Filter function argument.
  nsAutoString argumentString;
  SetCssTextToCoord(argumentString, aStyleFilter.mFilterParameter);
  filterFunctionString.Append(argumentString);

  // Filter function closing parenthesis.
  filterFunctionString.AppendLiteral(")");

  value->SetString(filterFunctionString);
  return value;
}

CSSValue*
nsComputedDOMStyle::DoGetFilter()
{
  const nsTArray<nsStyleFilter>& filters = StyleSVGReset()->mFilters;

  if (filters.IsEmpty()) {
    nsROCSSPrimitiveValue* value = new nsROCSSPrimitiveValue;
    value->SetIdent(eCSSKeyword_none);
    return value;
  }

  nsDOMCSSValueList* valueList = GetROCSSValueList(false);
  for(uint32_t i = 0; i < filters.Length(); i++) {
    nsROCSSPrimitiveValue* value =
      CreatePrimitiveValueForStyleFilter(filters[i]);
    valueList->AppendCSSValue(value);
  }
  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetMask()
{
  nsROCSSPrimitiveValue* val = new nsROCSSPrimitiveValue;

  const nsStyleSVGReset* svg = StyleSVGReset();

  if (svg->mMask)
    val->SetURI(svg->mMask);
  else
    val->SetIdent(eCSSKeyword_none);

  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetMaskType()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  val->SetIdent(
    nsCSSProps::ValueToKeywordEnum(StyleSVGReset()->mMaskType,
                                   nsCSSProps::kMaskTypeKTable));
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetPaintOrder()
{
  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue;
  nsAutoString string;
  uint8_t paintOrder = StyleSVG()->mPaintOrder;
  nsStyleUtil::AppendPaintOrderValue(paintOrder, string);
  val->SetString(string);
  return val;
}

CSSValue*
nsComputedDOMStyle::DoGetTransitionDelay()
{
  const nsStyleDisplay* display = StyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  NS_ABORT_IF_FALSE(display->mTransitionDelayCount > 0,
                    "first item must be explicit");
  uint32_t i = 0;
  do {
    const nsTransition *transition = &display->mTransitions[i];
    nsROCSSPrimitiveValue* delay = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(delay);
    delay->SetTime((float)transition->GetDelay() / (float)PR_MSEC_PER_SEC);
  } while (++i < display->mTransitionDelayCount);

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetTransitionDuration()
{
  const nsStyleDisplay* display = StyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  NS_ABORT_IF_FALSE(display->mTransitionDurationCount > 0,
                    "first item must be explicit");
  uint32_t i = 0;
  do {
    const nsTransition *transition = &display->mTransitions[i];
    nsROCSSPrimitiveValue* duration = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(duration);

    duration->SetTime((float)transition->GetDuration() / (float)PR_MSEC_PER_SEC);
  } while (++i < display->mTransitionDurationCount);

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetTransitionProperty()
{
  const nsStyleDisplay* display = StyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  NS_ABORT_IF_FALSE(display->mTransitionPropertyCount > 0,
                    "first item must be explicit");
  uint32_t i = 0;
  do {
    const nsTransition *transition = &display->mTransitions[i];
    nsROCSSPrimitiveValue* property = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(property);
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

  return valueList;
}

void
nsComputedDOMStyle::AppendTimingFunction(nsDOMCSSValueList *aValueList,
                                         const nsTimingFunction& aTimingFunction)
{
  nsROCSSPrimitiveValue* timingFunction = new nsROCSSPrimitiveValue;
  aValueList->AppendCSSValue(timingFunction);

  nsAutoString tmp;

  if (aTimingFunction.mType == nsTimingFunction::Function) {
    // set the value from the cubic-bezier control points
    // (We could try to regenerate the keywords if we want.)
    tmp.AppendLiteral("cubic-bezier(");
    tmp.AppendFloat(aTimingFunction.mFunc.mX1);
    tmp.AppendLiteral(", ");
    tmp.AppendFloat(aTimingFunction.mFunc.mY1);
    tmp.AppendLiteral(", ");
    tmp.AppendFloat(aTimingFunction.mFunc.mX2);
    tmp.AppendLiteral(", ");
    tmp.AppendFloat(aTimingFunction.mFunc.mY2);
    tmp.AppendLiteral(")");
  } else {
    tmp.AppendLiteral("steps(");
    tmp.AppendInt(aTimingFunction.mSteps);
    if (aTimingFunction.mType == nsTimingFunction::StepStart) {
      tmp.AppendLiteral(", start)");
    } else {
      tmp.AppendLiteral(", end)");
    }
  }
  timingFunction->SetString(tmp);
}

CSSValue*
nsComputedDOMStyle::DoGetTransitionTimingFunction()
{
  const nsStyleDisplay* display = StyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  NS_ABORT_IF_FALSE(display->mTransitionTimingFunctionCount > 0,
                    "first item must be explicit");
  uint32_t i = 0;
  do {
    AppendTimingFunction(valueList,
                         display->mTransitions[i].GetTimingFunction());
  } while (++i < display->mTransitionTimingFunctionCount);

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetAnimationName()
{
  const nsStyleDisplay* display = StyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  NS_ABORT_IF_FALSE(display->mAnimationNameCount > 0,
                    "first item must be explicit");
  uint32_t i = 0;
  do {
    const nsAnimation *animation = &display->mAnimations[i];
    nsROCSSPrimitiveValue* property = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(property);

    const nsString& name = animation->GetName();
    if (name.IsEmpty()) {
      property->SetIdent(eCSSKeyword_none);
    } else {
      nsAutoString escaped;
      nsStyleUtil::AppendEscapedCSSIdent(animation->GetName(), escaped);
      property->SetString(escaped); // really want SetIdent
    }
  } while (++i < display->mAnimationNameCount);

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetAnimationDelay()
{
  const nsStyleDisplay* display = StyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  NS_ABORT_IF_FALSE(display->mAnimationDelayCount > 0,
                    "first item must be explicit");
  uint32_t i = 0;
  do {
    const nsAnimation *animation = &display->mAnimations[i];
    nsROCSSPrimitiveValue* delay = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(delay);
    delay->SetTime((float)animation->GetDelay() / (float)PR_MSEC_PER_SEC);
  } while (++i < display->mAnimationDelayCount);

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetAnimationDuration()
{
  const nsStyleDisplay* display = StyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  NS_ABORT_IF_FALSE(display->mAnimationDurationCount > 0,
                    "first item must be explicit");
  uint32_t i = 0;
  do {
    const nsAnimation *animation = &display->mAnimations[i];
    nsROCSSPrimitiveValue* duration = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(duration);

    duration->SetTime((float)animation->GetDuration() / (float)PR_MSEC_PER_SEC);
  } while (++i < display->mAnimationDurationCount);

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetAnimationTimingFunction()
{
  const nsStyleDisplay* display = StyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  NS_ABORT_IF_FALSE(display->mAnimationTimingFunctionCount > 0,
                    "first item must be explicit");
  uint32_t i = 0;
  do {
    AppendTimingFunction(valueList,
                         display->mAnimations[i].GetTimingFunction());
  } while (++i < display->mAnimationTimingFunctionCount);

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetAnimationDirection()
{
  const nsStyleDisplay* display = StyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  NS_ABORT_IF_FALSE(display->mAnimationDirectionCount > 0,
                    "first item must be explicit");
  uint32_t i = 0;
  do {
    const nsAnimation *animation = &display->mAnimations[i];
    nsROCSSPrimitiveValue* direction = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(direction);
    direction->SetIdent(
      nsCSSProps::ValueToKeywordEnum(animation->GetDirection(),
                                     nsCSSProps::kAnimationDirectionKTable));
  } while (++i < display->mAnimationDirectionCount);

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetAnimationFillMode()
{
  const nsStyleDisplay* display = StyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  NS_ABORT_IF_FALSE(display->mAnimationFillModeCount > 0,
                    "first item must be explicit");
  uint32_t i = 0;
  do {
    const nsAnimation *animation = &display->mAnimations[i];
    nsROCSSPrimitiveValue* fillMode = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(fillMode);
    fillMode->SetIdent(
      nsCSSProps::ValueToKeywordEnum(animation->GetFillMode(),
                                     nsCSSProps::kAnimationFillModeKTable));
  } while (++i < display->mAnimationFillModeCount);

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetAnimationIterationCount()
{
  const nsStyleDisplay* display = StyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  NS_ABORT_IF_FALSE(display->mAnimationIterationCountCount > 0,
                    "first item must be explicit");
  uint32_t i = 0;
  do {
    const nsAnimation *animation = &display->mAnimations[i];
    nsROCSSPrimitiveValue* iterationCount = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(iterationCount);

    float f = animation->GetIterationCount();
    /* Need a nasty hack here to work around an optimizer bug in gcc
       4.2 on Mac, which somehow gets confused when directly comparing
       a float to the return value of NS_IEEEPositiveInfinity when
       building 32-bit builds. */
#ifdef XP_MACOSX
    volatile
#endif
      float inf = NS_IEEEPositiveInfinity();
    if (f == inf) {
      iterationCount->SetIdent(eCSSKeyword_infinite);
    } else {
      iterationCount->SetNumber(f);
    }
  } while (++i < display->mAnimationIterationCountCount);

  return valueList;
}

CSSValue*
nsComputedDOMStyle::DoGetAnimationPlayState()
{
  const nsStyleDisplay* display = StyleDisplay();

  nsDOMCSSValueList *valueList = GetROCSSValueList(true);

  NS_ABORT_IF_FALSE(display->mAnimationPlayStateCount > 0,
                    "first item must be explicit");
  uint32_t i = 0;
  do {
    const nsAnimation *animation = &display->mAnimations[i];
    nsROCSSPrimitiveValue* playState = new nsROCSSPrimitiveValue;
    valueList->AppendCSSValue(playState);
    playState->SetIdent(
      nsCSSProps::ValueToKeywordEnum(animation->GetPlayState(),
                                     nsCSSProps::kAnimationPlayStateKTable));
  } while (++i < display->mAnimationPlayStateCount);

  return valueList;
}

#define COMPUTED_STYLE_MAP_ENTRY(_prop, _method)              \
  { eCSSProperty_##_prop, &nsComputedDOMStyle::DoGet##_method, false }
#define COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_prop, _method)       \
  { eCSSProperty_##_prop, &nsComputedDOMStyle::DoGet##_method, true }

const nsComputedDOMStyle::ComputedStyleMapEntry*
nsComputedDOMStyle::GetQueryablePropertyMap(uint32_t* aLength)
{
  /* ******************************************************************* *\
   * Properties below are listed in alphabetical order.                  *
   * Please keep them that way.                                          *
   *                                                                     *
   * Properties commented out with // are not yet implemented            *
   * Properties commented out with //// are shorthands and not queryable *
  \* ******************************************************************* */
  static const ComputedStyleMapEntry map[] = {
    /* ***************************** *\
     * Implementations of CSS styles *
    \* ***************************** */

    COMPUTED_STYLE_MAP_ENTRY(align_items,                   AlignItems),
    COMPUTED_STYLE_MAP_ENTRY(align_self,                    AlignSelf),
    //// COMPUTED_STYLE_MAP_ENTRY(animation,                Animation),
    COMPUTED_STYLE_MAP_ENTRY(animation_delay,               AnimationDelay),
    COMPUTED_STYLE_MAP_ENTRY(animation_direction,           AnimationDirection),
    COMPUTED_STYLE_MAP_ENTRY(animation_duration,            AnimationDuration),
    COMPUTED_STYLE_MAP_ENTRY(animation_fill_mode,           AnimationFillMode),
    COMPUTED_STYLE_MAP_ENTRY(animation_iteration_count,     AnimationIterationCount),
    COMPUTED_STYLE_MAP_ENTRY(animation_name,                AnimationName),
    COMPUTED_STYLE_MAP_ENTRY(animation_play_state,          AnimationPlayState),
    COMPUTED_STYLE_MAP_ENTRY(animation_timing_function,     AnimationTimingFunction),
    COMPUTED_STYLE_MAP_ENTRY(backface_visibility,           BackfaceVisibility),
    //// COMPUTED_STYLE_MAP_ENTRY(background,               Background),
    COMPUTED_STYLE_MAP_ENTRY(background_attachment,         BackgroundAttachment),
    COMPUTED_STYLE_MAP_ENTRY(background_clip,               BackgroundClip),
    COMPUTED_STYLE_MAP_ENTRY(background_color,              BackgroundColor),
    COMPUTED_STYLE_MAP_ENTRY(background_image,              BackgroundImage),
    COMPUTED_STYLE_MAP_ENTRY(background_origin,             BackgroundOrigin),
    COMPUTED_STYLE_MAP_ENTRY(background_position,           BackgroundPosition),
    COMPUTED_STYLE_MAP_ENTRY(background_repeat,             BackgroundRepeat),
    COMPUTED_STYLE_MAP_ENTRY(background_size,               BackgroundSize),
    //// COMPUTED_STYLE_MAP_ENTRY(border,                   Border),
    //// COMPUTED_STYLE_MAP_ENTRY(border_bottom,            BorderBottom),
    COMPUTED_STYLE_MAP_ENTRY(border_bottom_color,           BorderBottomColor),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(border_bottom_left_radius, BorderBottomLeftRadius),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(border_bottom_right_radius,BorderBottomRightRadius),
    COMPUTED_STYLE_MAP_ENTRY(border_bottom_style,           BorderBottomStyle),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(border_bottom_width,    BorderBottomWidth),
    COMPUTED_STYLE_MAP_ENTRY(border_collapse,               BorderCollapse),
    //// COMPUTED_STYLE_MAP_ENTRY(border_color,             BorderColor),
    //// COMPUTED_STYLE_MAP_ENTRY(border_image,             BorderImage),
    COMPUTED_STYLE_MAP_ENTRY(border_image_outset,           BorderImageOutset),
    COMPUTED_STYLE_MAP_ENTRY(border_image_repeat,           BorderImageRepeat),
    COMPUTED_STYLE_MAP_ENTRY(border_image_slice,            BorderImageSlice),
    COMPUTED_STYLE_MAP_ENTRY(border_image_source,           BorderImageSource),
    COMPUTED_STYLE_MAP_ENTRY(border_image_width,            BorderImageWidth),
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
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(border_top_left_radius,    BorderTopLeftRadius),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(border_top_right_radius,   BorderTopRightRadius),
    COMPUTED_STYLE_MAP_ENTRY(border_top_style,              BorderTopStyle),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(border_top_width,       BorderTopWidth),
    //// COMPUTED_STYLE_MAP_ENTRY(border_width,             BorderWidth),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(bottom,                 Bottom),
    COMPUTED_STYLE_MAP_ENTRY(box_shadow,                    BoxShadow),
    COMPUTED_STYLE_MAP_ENTRY(caption_side,                  CaptionSide),
    COMPUTED_STYLE_MAP_ENTRY(clear,                         Clear),
    COMPUTED_STYLE_MAP_ENTRY(clip,                          Clip),
    COMPUTED_STYLE_MAP_ENTRY(color,                         Color),
    COMPUTED_STYLE_MAP_ENTRY(content,                       Content),
    COMPUTED_STYLE_MAP_ENTRY(counter_increment,             CounterIncrement),
    COMPUTED_STYLE_MAP_ENTRY(counter_reset,                 CounterReset),
    COMPUTED_STYLE_MAP_ENTRY(cursor,                        Cursor),
    COMPUTED_STYLE_MAP_ENTRY(direction,                     Direction),
    COMPUTED_STYLE_MAP_ENTRY(display,                       Display),
    COMPUTED_STYLE_MAP_ENTRY(empty_cells,                   EmptyCells),
    COMPUTED_STYLE_MAP_ENTRY(flex_basis,                    FlexBasis),
    COMPUTED_STYLE_MAP_ENTRY(flex_direction,                FlexDirection),
    COMPUTED_STYLE_MAP_ENTRY(flex_grow,                     FlexGrow),
    COMPUTED_STYLE_MAP_ENTRY(flex_shrink,                   FlexShrink),
    COMPUTED_STYLE_MAP_ENTRY(float,                         Float),
    //// COMPUTED_STYLE_MAP_ENTRY(font,                     Font),
    COMPUTED_STYLE_MAP_ENTRY(font_family,                   FontFamily),
    COMPUTED_STYLE_MAP_ENTRY(font_kerning,                  FontKerning),
    COMPUTED_STYLE_MAP_ENTRY(font_size,                     FontSize),
    COMPUTED_STYLE_MAP_ENTRY(font_size_adjust,              FontSizeAdjust),
    COMPUTED_STYLE_MAP_ENTRY(font_stretch,                  FontStretch),
    COMPUTED_STYLE_MAP_ENTRY(font_style,                    FontStyle),
    COMPUTED_STYLE_MAP_ENTRY(font_synthesis,                FontSynthesis),
    COMPUTED_STYLE_MAP_ENTRY(font_variant,                  FontVariant),
    COMPUTED_STYLE_MAP_ENTRY(font_variant_alternates,       FontVariantAlternates),
    COMPUTED_STYLE_MAP_ENTRY(font_variant_caps,             FontVariantCaps),
    COMPUTED_STYLE_MAP_ENTRY(font_variant_east_asian,       FontVariantEastAsian),
    COMPUTED_STYLE_MAP_ENTRY(font_variant_ligatures,        FontVariantLigatures),
    COMPUTED_STYLE_MAP_ENTRY(font_variant_numeric,          FontVariantNumeric),
    COMPUTED_STYLE_MAP_ENTRY(font_variant_position,         FontVariantPosition),
    COMPUTED_STYLE_MAP_ENTRY(font_weight,                   FontWeight),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(height,                 Height),
    COMPUTED_STYLE_MAP_ENTRY(ime_mode,                      IMEMode),
    COMPUTED_STYLE_MAP_ENTRY(justify_content,               JustifyContent),
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
    COMPUTED_STYLE_MAP_ENTRY(opacity,                       Opacity),
    // COMPUTED_STYLE_MAP_ENTRY(orphans,                    Orphans),
    //// COMPUTED_STYLE_MAP_ENTRY(outline,                  Outline),
    COMPUTED_STYLE_MAP_ENTRY(order,                         Order),
    COMPUTED_STYLE_MAP_ENTRY(outline_color,                 OutlineColor),
    COMPUTED_STYLE_MAP_ENTRY(outline_offset,                OutlineOffset),
    COMPUTED_STYLE_MAP_ENTRY(outline_style,                 OutlineStyle),
    COMPUTED_STYLE_MAP_ENTRY(outline_width,                 OutlineWidth),
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
    COMPUTED_STYLE_MAP_ENTRY(page_break_inside,             PageBreakInside),
    COMPUTED_STYLE_MAP_ENTRY(perspective,                   Perspective),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(perspective_origin,     PerspectiveOrigin),
    COMPUTED_STYLE_MAP_ENTRY(pointer_events,                PointerEvents),
    COMPUTED_STYLE_MAP_ENTRY(position,                      Position),
    COMPUTED_STYLE_MAP_ENTRY(quotes,                        Quotes),
    COMPUTED_STYLE_MAP_ENTRY(resize,                        Resize),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(right,                  Right),
    //// COMPUTED_STYLE_MAP_ENTRY(size,                     Size),
    COMPUTED_STYLE_MAP_ENTRY(table_layout,                  TableLayout),
    COMPUTED_STYLE_MAP_ENTRY(text_align,                    TextAlign),
    COMPUTED_STYLE_MAP_ENTRY(text_decoration,               TextDecoration),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(text_indent,            TextIndent),
    COMPUTED_STYLE_MAP_ENTRY(text_overflow,                 TextOverflow),
    COMPUTED_STYLE_MAP_ENTRY(text_shadow,                   TextShadow),
    COMPUTED_STYLE_MAP_ENTRY(text_transform,                TextTransform),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(top,                    Top),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(transform,              Transform),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(transform_origin,       TransformOrigin),
    COMPUTED_STYLE_MAP_ENTRY(transform_style,               TransformStyle),
    //// COMPUTED_STYLE_MAP_ENTRY(transition,               Transition),
    COMPUTED_STYLE_MAP_ENTRY(transition_delay,              TransitionDelay),
    COMPUTED_STYLE_MAP_ENTRY(transition_duration,           TransitionDuration),
    COMPUTED_STYLE_MAP_ENTRY(transition_property,           TransitionProperty),
    COMPUTED_STYLE_MAP_ENTRY(transition_timing_function,    TransitionTimingFunction),
    COMPUTED_STYLE_MAP_ENTRY(unicode_bidi,                  UnicodeBidi),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(vertical_align,         VerticalAlign),
    COMPUTED_STYLE_MAP_ENTRY(visibility,                    Visibility),
    COMPUTED_STYLE_MAP_ENTRY(white_space,                   WhiteSpace),
    // COMPUTED_STYLE_MAP_ENTRY(widows,                     Widows),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(width,                  Width),
    COMPUTED_STYLE_MAP_ENTRY(word_break,                    WordBreak),
    COMPUTED_STYLE_MAP_ENTRY(word_spacing,                  WordSpacing),
    COMPUTED_STYLE_MAP_ENTRY(word_wrap,                     WordWrap),
    COMPUTED_STYLE_MAP_ENTRY(writing_mode,                  WritingMode),
    COMPUTED_STYLE_MAP_ENTRY(z_index,                       ZIndex),

    /* ******************************* *\
     * Implementations of -moz- styles *
    \* ******************************* */

    COMPUTED_STYLE_MAP_ENTRY(appearance,                    Appearance),
    COMPUTED_STYLE_MAP_ENTRY(_moz_background_inline_policy, BackgroundInlinePolicy),
    COMPUTED_STYLE_MAP_ENTRY(binding,                       Binding),
    COMPUTED_STYLE_MAP_ENTRY(border_bottom_colors,          BorderBottomColors),
    COMPUTED_STYLE_MAP_ENTRY(border_left_colors,            BorderLeftColors),
    COMPUTED_STYLE_MAP_ENTRY(border_right_colors,           BorderRightColors),
    COMPUTED_STYLE_MAP_ENTRY(border_top_colors,             BorderTopColors),
    COMPUTED_STYLE_MAP_ENTRY(box_align,                     BoxAlign),
    COMPUTED_STYLE_MAP_ENTRY(box_direction,                 BoxDirection),
    COMPUTED_STYLE_MAP_ENTRY(box_flex,                      BoxFlex),
    COMPUTED_STYLE_MAP_ENTRY(box_ordinal_group,             BoxOrdinalGroup),
    COMPUTED_STYLE_MAP_ENTRY(box_orient,                    BoxOrient),
    COMPUTED_STYLE_MAP_ENTRY(box_pack,                      BoxPack),
    COMPUTED_STYLE_MAP_ENTRY(box_sizing,                    BoxSizing),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_count,             ColumnCount),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_fill,              ColumnFill),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_gap,               ColumnGap),
    //// COMPUTED_STYLE_MAP_ENTRY(_moz_column_rule,         ColumnRule),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_rule_color,        ColumnRuleColor),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_rule_style,        ColumnRuleStyle),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_rule_width,        ColumnRuleWidth),
    COMPUTED_STYLE_MAP_ENTRY(_moz_column_width,             ColumnWidth),
    COMPUTED_STYLE_MAP_ENTRY(float_edge,                    FloatEdge),
    COMPUTED_STYLE_MAP_ENTRY(font_feature_settings,         FontFeatureSettings),
    COMPUTED_STYLE_MAP_ENTRY(font_language_override,        FontLanguageOverride),
    COMPUTED_STYLE_MAP_ENTRY(force_broken_image_icon,       ForceBrokenImageIcon),
    COMPUTED_STYLE_MAP_ENTRY(hyphens,                       Hyphens),
    COMPUTED_STYLE_MAP_ENTRY(image_region,                  ImageRegion),
    COMPUTED_STYLE_MAP_ENTRY(orient,                        Orient),
    COMPUTED_STYLE_MAP_ENTRY(osx_font_smoothing,            OSXFontSmoothing),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_outline_radius_bottomLeft, OutlineRadiusBottomLeft),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_outline_radius_bottomRight,OutlineRadiusBottomRight),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_outline_radius_topLeft,    OutlineRadiusTopLeft),
    COMPUTED_STYLE_MAP_ENTRY_LAYOUT(_moz_outline_radius_topRight,   OutlineRadiusTopRight),
    COMPUTED_STYLE_MAP_ENTRY(stack_sizing,                  StackSizing),
    COMPUTED_STYLE_MAP_ENTRY(_moz_tab_size,                 TabSize),
    COMPUTED_STYLE_MAP_ENTRY(text_align_last,               TextAlignLast),
    COMPUTED_STYLE_MAP_ENTRY(text_blink,                    MozTextBlink),
    COMPUTED_STYLE_MAP_ENTRY(text_decoration_color,         TextDecorationColor),
    COMPUTED_STYLE_MAP_ENTRY(text_decoration_line,          TextDecorationLine),
    COMPUTED_STYLE_MAP_ENTRY(text_decoration_style,         TextDecorationStyle),
    COMPUTED_STYLE_MAP_ENTRY(text_size_adjust,              TextSizeAdjust),
    COMPUTED_STYLE_MAP_ENTRY(user_focus,                    UserFocus),
    COMPUTED_STYLE_MAP_ENTRY(user_input,                    UserInput),
    COMPUTED_STYLE_MAP_ENTRY(user_modify,                   UserModify),
    COMPUTED_STYLE_MAP_ENTRY(user_select,                   UserSelect),
    COMPUTED_STYLE_MAP_ENTRY(_moz_window_shadow,            WindowShadow),

    /* ***************************** *\
     * Implementations of SVG styles *
    \* ***************************** */

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
    COMPUTED_STYLE_MAP_ENTRY(image_rendering,               ImageRendering),
    COMPUTED_STYLE_MAP_ENTRY(lighting_color,                LightingColor),
    COMPUTED_STYLE_MAP_ENTRY(marker_end,                    MarkerEnd),
    COMPUTED_STYLE_MAP_ENTRY(marker_mid,                    MarkerMid),
    COMPUTED_STYLE_MAP_ENTRY(marker_start,                  MarkerStart),
    COMPUTED_STYLE_MAP_ENTRY(mask,                          Mask),
    COMPUTED_STYLE_MAP_ENTRY(mask_type,                     MaskType),
    COMPUTED_STYLE_MAP_ENTRY(paint_order,                   PaintOrder),
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
    COMPUTED_STYLE_MAP_ENTRY(text_rendering,                TextRendering),
    COMPUTED_STYLE_MAP_ENTRY(vector_effect,                 VectorEffect)

  };

  *aLength = ArrayLength(map);

  return map;
}

