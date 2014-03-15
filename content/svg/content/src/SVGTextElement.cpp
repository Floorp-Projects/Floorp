/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGTextElement.h"
#include "mozilla/dom/SVGTextElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Text)

namespace mozilla {
namespace dom {

JSObject*
SVGTextElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return SVGTextElementBinding::Wrap(aCx, aScope, this);
}

//----------------------------------------------------------------------
// Implementation

SVGTextElement::SVGTextElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : SVGTextElementBase(aNodeInfo)
{
}

nsSVGElement::EnumAttributesInfo
SVGTextElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::LengthAttributesInfo
SVGTextElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGTextElement)


//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGTextElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sTextContentElementsMap,
    sFontSpecificationMap
  };

  return FindAttributeDependence(name, map) ||
    SVGTextElementBase::IsAttributeMapped(name);
}

} // namespace dom
} // namespace mozilla
