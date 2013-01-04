/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSpanElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIAtom.h"
#include "nsRuleData.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Span)
DOMCI_NODE_DATA(HTMLSpanElement, mozilla::dom::HTMLSpanElement)

namespace mozilla {
namespace dom {

HTMLSpanElement::~HTMLSpanElement()
{
}

NS_IMPL_ADDREF_INHERITED(HTMLSpanElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLSpanElement, Element)

// QueryInterface implementation for HTMLSpanElement
NS_INTERFACE_TABLE_HEAD(HTMLSpanElement)
  NS_HTML_CONTENT_INTERFACE_TABLE0(HTMLSpanElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLSpanElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLSpanElement)


NS_IMPL_ELEMENT_CLONE(HTMLSpanElement)

} // namespace dom
} // namespace mozilla
