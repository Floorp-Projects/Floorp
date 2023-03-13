/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGTextElement.h"
#include "mozilla/dom/SVGTextElementBinding.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(Text)

namespace mozilla::dom {

JSObject* SVGTextElement::WrapNode(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return SVGTextElement_Binding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
// Implementation

SVGTextElement::SVGTextElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGTextElementBase(std::move(aNodeInfo)) {}

SVGElement::EnumAttributesInfo SVGTextElement::GetEnumInfo() {
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo, ArrayLength(sEnumInfo));
}

SVGElement::LengthAttributesInfo SVGTextElement::GetLengthInfo() {
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGTextElement)

}  // namespace mozilla::dom
