/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLModElement.h"
#include "mozilla/dom/HTMLModElementBinding.h"
#include "nsStyleConsts.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Mod)

namespace mozilla {
namespace dom {

HTMLModElement::HTMLModElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

HTMLModElement::~HTMLModElement()
{
}


NS_IMPL_ADDREF_INHERITED(HTMLModElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLModElement, Element)

// QueryInterface implementation for HTMLModElement
NS_INTERFACE_TABLE_HEAD(HTMLModElement)
  NS_HTML_CONTENT_INTERFACES(nsGenericHTMLElement)
  NS_INTERFACE_TABLE_INHERITED1(HTMLModElement,
                                nsIDOMHTMLModElement)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
NS_ELEMENT_INTERFACE_MAP_END


NS_IMPL_ELEMENT_CLONE(HTMLModElement)


NS_IMPL_URI_ATTR(HTMLModElement, Cite, cite)
NS_IMPL_STRING_ATTR(HTMLModElement, DateTime, datetime)

JSObject*
HTMLModElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLModElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
