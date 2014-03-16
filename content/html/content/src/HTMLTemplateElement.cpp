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
NS_NewHTMLTemplateElement(already_AddRefed<nsINodeInfo>&& aNodeInfo,
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

HTMLTemplateElement::HTMLTemplateElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  SetHasWeirdParserInsertionMode();
}

nsresult
HTMLTemplateElement::Init()
{
  nsIDocument* contentsOwner = OwnerDoc()->GetTemplateContentsOwner();
  NS_ENSURE_TRUE(contentsOwner, NS_ERROR_UNEXPECTED);

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

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLTemplateElement)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLTemplateElement,
                                                nsGenericHTMLElement)
  if (tmp->mContent) {
    tmp->mContent->SetHost(nullptr);
    tmp->mContent = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLTemplateElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

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

