/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
HTMLDataListElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLDataListElement_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLDataListElement,
                                   nsGenericHTMLElement,
                                   mOptions)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(HTMLDataListElement,
                                               nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLDataListElement)

bool
HTMLDataListElement::MatchOptions(Element* aElement, int32_t aNamespaceID,
                                  nsAtom* aAtom, void* aData)
{
  return aElement->NodeInfo()->Equals(nsGkAtoms::option, kNameSpaceID_XHTML) &&
         !aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::disabled);
}

} // namespace dom
} // namespace mozilla
