/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTemplateElement.h"
#include "mozilla/dom/HTMLTemplateElementBinding.h"

#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIAtom.h"
#include "nsRuleData.h"

using namespace mozilla::dom;

nsGenericHTMLElement*
NS_NewHTMLTemplateElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                          FromParser aFromParser)
{
  HTMLTemplateElement* it = new HTMLTemplateElement(aNodeInfo);
  nsresult rv = it->Init();
  if (NS_FAILED(rv)) {
    delete it;
    return nullptr;
  }

  return it;
}

namespace mozilla {
namespace dom {

HTMLTemplateElement::HTMLTemplateElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsresult
HTMLTemplateElement::Init()
{
  nsIDocument* doc = OwnerDoc();
  nsIDocument* contentsOwner = doc;

  // Used to test if the document "has a browsing context".
  nsCOMPtr<nsISupports> container = doc->GetContainer();
  if (container) {
    // GetTemplateContentsOwner lazily creates a document.
    contentsOwner = doc->GetTemplateContentsOwner();
    NS_ENSURE_TRUE(contentsOwner, NS_ERROR_UNEXPECTED);
  }

  mContent = contentsOwner->CreateDocumentFragment();
  mContent->SetHost(this);

  return NS_OK;
}

HTMLTemplateElement::~HTMLTemplateElement()
{
  if (mContent) {
    mContent->SetHost(nullptr);
  }
}

NS_IMPL_ADDREF_INHERITED(HTMLTemplateElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLTemplateElement, Element)

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(HTMLTemplateElement,
                                     nsGenericHTMLElement,
                                     mContent)

// QueryInterface implementation for HTMLTemplateElement
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(HTMLTemplateElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE_WITH_INIT(HTMLTemplateElement)

JSObject*
HTMLTemplateElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLTemplateElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla

