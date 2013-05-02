/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocument.h"
#include "HTMLUnknownElement.h"
#include "mozilla/dom/HTMLElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Unknown)

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(HTMLUnknownElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLUnknownElement, Element)

JSObject*
HTMLUnknownElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  JSObject* obj =
    HTMLUnknownElementBinding::Wrap(aCx, aScope, this);
  if (obj && Substring(NodeName(), 0, 2).LowerCaseEqualsLiteral("x-")) {
    // If we have a registered x-tag then we fix the prototype.
    JSAutoCompartment ac(aCx, obj);
    nsDocument* document = static_cast<nsDocument*>(OwnerDoc());
    JSObject* prototype = document->GetCustomPrototype(LocalName());
    if (prototype) {
      NS_ENSURE_TRUE(JS_WrapObject(aCx, &prototype), nullptr);
      NS_ENSURE_TRUE(JS_SetPrototype(aCx, obj, prototype), nullptr);
    }
  }
  return obj;
}

// QueryInterface implementation for HTMLUnknownElement
NS_INTERFACE_TABLE_HEAD(HTMLUnknownElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(HTMLUnknownElement,
                                   nsIDOMHTMLUnknownElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLUnknownElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_MAP_END

NS_IMPL_ELEMENT_CLONE(HTMLUnknownElement)

} // namespace dom
} // namespace mozilla
