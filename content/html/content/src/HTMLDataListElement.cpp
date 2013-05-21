/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLDataListElement.h"
#include "mozilla/dom/HTMLDataListElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(DataList)

namespace mozilla {
namespace dom {

HTMLDataListElement::~HTMLDataListElement()
{
}

JSObject*
HTMLDataListElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLDataListElementBinding::Wrap(aCx, aScope, this);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(HTMLDataListElement, nsGenericHTMLElement,
                                     mOptions)

NS_IMPL_ADDREF_INHERITED(HTMLDataListElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLDataListElement, Element)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLDataListElement)
  NS_HTML_CONTENT_INTERFACES(nsGenericHTMLElement)
  NS_INTERFACE_TABLE_INHERITED1(HTMLDataListElement,
                                nsIDOMHTMLDataListElement)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_ELEMENT_CLONE(HTMLDataListElement)

bool
HTMLDataListElement::MatchOptions(nsIContent* aContent, int32_t aNamespaceID,
                                  nsIAtom* aAtom, void* aData)
{
  return aContent->NodeInfo()->Equals(nsGkAtoms::option, kNameSpaceID_XHTML) &&
         !aContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled);
}

NS_IMETHODIMP
HTMLDataListElement::GetOptions(nsIDOMHTMLCollection** aOptions)
{
  NS_ADDREF(*aOptions = Options());

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
