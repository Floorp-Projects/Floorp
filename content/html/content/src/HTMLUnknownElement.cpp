/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDocument.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "mozilla/dom/HTMLElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Unknown)

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(HTMLUnknownElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLUnknownElement, Element)

JSObject*
HTMLUnknownElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  JS::Rooted<JSObject*> obj(aCx,
    HTMLUnknownElementBinding::Wrap(aCx, aScope, this));
  if (obj && Substring(NodeName(), 0, 2).LowerCaseEqualsLiteral("x-")) {
    // If we have a registered x-tag then we fix the prototype.
    JSAutoCompartment ac(aCx, obj);
    nsDocument* document = static_cast<nsDocument*>(OwnerDoc());
    JS::Rooted<JSObject*> prototype(aCx);
    document->GetCustomPrototype(LocalName(), &prototype);
    if (prototype) {
      NS_ENSURE_TRUE(JS_WrapObject(aCx, prototype.address()), nullptr);
      NS_ENSURE_TRUE(JS_SetPrototype(aCx, obj, prototype), nullptr);
    }
  }
  return obj;
}

// QueryInterface implementation for HTMLUnknownElement
NS_INTERFACE_MAP_BEGIN(HTMLUnknownElement)
  NS_HTML_CONTENT_INTERFACES(nsGenericHTMLElement)
NS_ELEMENT_INTERFACE_MAP_END

NS_IMPL_ELEMENT_CLONE(HTMLUnknownElement)

} // namespace dom
} // namespace mozilla
