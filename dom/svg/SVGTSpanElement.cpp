/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGTSpanElement.h"
#include "mozilla/dom/SVGTSpanElementBinding.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(TSpan)

namespace mozilla::dom {

JSObject* SVGTSpanElement::WrapNode(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return SVGTSpanElement_Binding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
// Implementation

SVGTSpanElement::SVGTSpanElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGTSpanElementBase(std::move(aNodeInfo)) {}

SVGElement::EnumAttributesInfo SVGTSpanElement::GetEnumInfo() {
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo, ArrayLength(sEnumInfo));
}

SVGElement::LengthAttributesInfo SVGTSpanElement::GetLengthInfo() {
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGTSpanElement)

}  // namespace mozilla::dom
