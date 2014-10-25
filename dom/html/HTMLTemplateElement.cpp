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

NS_IMPL_NS_NEW_HTML_ELEMENT(Template)

namespace mozilla {
namespace dom {

HTMLTemplateElement::HTMLTemplateElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  SetHasWeirdParserInsertionMode();

  nsIDocument* contentsOwner = OwnerDoc()->GetTemplateContentsOwner();
  if (!contentsOwner) {
    MOZ_CRASH("There should always be a template contents owner.");
  }

  mContent = contentsOwner->CreateDocumentFragment();
  mContent->SetHost(this);
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

NS_IMPL_ELEMENT_CLONE(HTMLTemplateElement)

JSObject*
HTMLTemplateElement::WrapNode(JSContext *aCx)
{
  return HTMLTemplateElementBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla

