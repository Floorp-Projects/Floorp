/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGDefsElement.h"
#include "mozilla/dom/SVGDefsElementBinding.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(Defs)

namespace mozilla::dom {

JSObject* SVGDefsElement::WrapNode(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return SVGDefsElement_Binding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
// Implementation

SVGDefsElement::SVGDefsElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGGraphicsElement(std::move(aNodeInfo)) {}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGDefsElement)

}  // namespace mozilla::dom
