/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLMenuElement.h"

#include "mozilla/dom/HTMLMenuElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Menu)

namespace mozilla::dom {

HTMLMenuElement::HTMLMenuElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)) {}

HTMLMenuElement::~HTMLMenuElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLMenuElement)

JSObject* HTMLMenuElement::WrapNode(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return HTMLMenuElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
