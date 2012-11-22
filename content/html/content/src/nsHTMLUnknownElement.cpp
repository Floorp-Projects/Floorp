/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLUnknownElement.h"
#include "mozilla/dom/HTMLElementBinding.h"

using namespace mozilla::dom;

NS_IMPL_ADDREF_INHERITED(nsHTMLUnknownElement, Element)
NS_IMPL_RELEASE_INHERITED(nsHTMLUnknownElement, Element)

JSObject*
nsHTMLUnknownElement::WrapNode(JSContext *aCx, JSObject *aScope,
                               bool *aTriedToWrap)
{
  return HTMLUnknownElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

NS_IMPL_NS_NEW_HTML_ELEMENT(Unknown)

DOMCI_NODE_DATA(HTMLUnknownElement, nsHTMLUnknownElement)

// QueryInterface implementation for nsHTMLUnknownElement
NS_INTERFACE_TABLE_HEAD(nsHTMLUnknownElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLUnknownElement,
                                   nsIDOMHTMLUnknownElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLUnknownElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLUnknownElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLUnknownElement)
