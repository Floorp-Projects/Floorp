/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGTSpanElement.h"
#include "mozilla/dom/SVGTSpanElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(TSpan)

namespace mozilla {
namespace dom {

JSObject*
SVGTSpanElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGTSpanElement_Binding::Wrap(aCx, this, aGivenProto);
}


//----------------------------------------------------------------------
// Implementation

SVGTSpanElement::SVGTSpanElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGTSpanElementBase(aNodeInfo)
{
}

nsSVGElement::EnumAttributesInfo
SVGTSpanElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::LengthAttributesInfo
SVGTSpanElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// nsINode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGTSpanElement)

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGTSpanElement::IsAttributeMapped(const nsAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFillStrokeMap,
    sFontSpecificationMap,
    sGraphicsMap,
    sTextContentElementsMap
  };

  return FindAttributeDependence(name, map) ||
    SVGTSpanElementBase::IsAttributeMapped(name);
}

} // namespace dom
} // namespace mozilla
