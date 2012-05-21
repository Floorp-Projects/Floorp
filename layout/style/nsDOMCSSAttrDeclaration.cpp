/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object for element.style */

#include "nsDOMCSSAttrDeclaration.h"

#include "mozilla/css/Declaration.h"
#include "mozilla/css/Loader.h"
#include "mozilla/css/StyleRule.h"
#include "mozilla/dom/Element.h"
#include "nsIDocument.h"
#include "nsIDOMMutationEvent.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsNodeUtils.h"
#include "nsGenericElement.h"

namespace css = mozilla::css;
namespace dom = mozilla::dom;

nsDOMCSSAttributeDeclaration::nsDOMCSSAttributeDeclaration(dom::Element* aElement,
                                                           bool aIsSMILOverride)
  : mElement(aElement)
  , mIsSMILOverride(aIsSMILOverride)
{
  MOZ_COUNT_CTOR(nsDOMCSSAttributeDeclaration);

  NS_ASSERTION(aElement, "Inline style for a NULL element?");
}

nsDOMCSSAttributeDeclaration::~nsDOMCSSAttributeDeclaration()
{
  MOZ_COUNT_DTOR(nsDOMCSSAttributeDeclaration);
}

// If nsDOMCSSAttributeDeclaration is changed so that any additional
// fields are traversed by the cycle collector (for instance, if
// wrapper cache handling is changed) then CAN_SKIP must be updated.
NS_IMPL_CYCLE_COLLECTION_1(nsDOMCSSAttributeDeclaration, mElement)

// nsDOMCSSAttributeDeclaration has only one cycle collected field, so
// if mElement is going to be skipped, the attribute declaration can't
// be part of a garbage cycle.
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsDOMCSSAttributeDeclaration)
  return !tmp->mElement || nsGenericElement::CanSkip(tmp->mElement, true);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsDOMCSSAttributeDeclaration)
  return !tmp->mElement || nsGenericElement::CanSkipInCC(tmp->mElement);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

// CanSkipThis returns false to avoid problems with incomplete unlinking.
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsDOMCSSAttributeDeclaration)
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_INTERFACE_MAP_BEGIN(nsDOMCSSAttributeDeclaration)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsDOMCSSAttributeDeclaration)
NS_IMPL_QUERY_TAIL_INHERITING(nsDOMCSSDeclaration)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMCSSAttributeDeclaration)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMCSSAttributeDeclaration)

nsresult
nsDOMCSSAttributeDeclaration::SetCSSDeclaration(css::Declaration* aDecl)
{
  NS_ASSERTION(mElement, "Must have Element to set the declaration!");
  css::StyleRule* oldRule =
    mIsSMILOverride ? mElement->GetSMILOverrideStyleRule() :
    mElement->GetInlineStyleRule();
  NS_ASSERTION(oldRule, "Element must have rule");

  nsRefPtr<css::StyleRule> newRule =
    oldRule->DeclarationChanged(aDecl, false);
  if (!newRule) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return
    mIsSMILOverride ? mElement->SetSMILOverrideStyleRule(newRule, true) :
    mElement->SetInlineStyleRule(newRule, nsnull, true);
}

nsIDocument*
nsDOMCSSAttributeDeclaration::DocToUpdate()
{
  // XXXbz this is a bit of a hack, especially doing it before the
  // BeginUpdate(), but this is a good chokepoint where we know we
  // plan to modify the CSSDeclaration, so need to notify
  // AttributeWillChange if this is inline style.
  if (!mIsSMILOverride) {
    nsNodeUtils::AttributeWillChange(mElement, kNameSpaceID_None,
                                     nsGkAtoms::style,
                                     nsIDOMMutationEvent::MODIFICATION);
  }
 
  // We need OwnerDoc() rather than GetCurrentDoc() because it might
  // be the BeginUpdate call that inserts mElement into the document.
  return mElement->OwnerDoc();
}

css::Declaration*
nsDOMCSSAttributeDeclaration::GetCSSDeclaration(bool aAllocate)
{
  if (!mElement)
    return nsnull;

  css::StyleRule* cssRule;
  if (mIsSMILOverride)
    cssRule = mElement->GetSMILOverrideStyleRule();
  else
    cssRule = mElement->GetInlineStyleRule();

  if (cssRule) {
    return cssRule->GetDeclaration();
  }
  if (!aAllocate) {
    return nsnull;
  }

  // cannot fail
  css::Declaration *decl = new css::Declaration();
  decl->InitializeEmpty();
  nsRefPtr<css::StyleRule> newRule = new css::StyleRule(nsnull, decl);

  // this *can* fail (inside SetAttrAndNotify, at least).
  nsresult rv;
  if (mIsSMILOverride)
    rv = mElement->SetSMILOverrideStyleRule(newRule, false);
  else
    rv = mElement->SetInlineStyleRule(newRule, nsnull, false);

  if (NS_FAILED(rv)) {
    return nsnull; // the decl will be destroyed along with the style rule
  }

  return decl;
}

void
nsDOMCSSAttributeDeclaration::GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv)
{
  NS_ASSERTION(mElement, "Something is severely broken -- there should be an Element here!");

  nsIDocument* doc = mElement->OwnerDoc();
  aCSSParseEnv.mSheetURI = doc->GetDocumentURI();
  aCSSParseEnv.mBaseURI = mElement->GetBaseURI();
  aCSSParseEnv.mPrincipal = mElement->NodePrincipal();
  aCSSParseEnv.mCSSLoader = doc->CSSLoader();
}

NS_IMETHODIMP
nsDOMCSSAttributeDeclaration::GetParentRule(nsIDOMCSSRule **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);

  *aParent = nsnull;
  return NS_OK;
}

/* virtual */ nsINode*
nsDOMCSSAttributeDeclaration::GetParentObject()
{
  return mElement;
}
