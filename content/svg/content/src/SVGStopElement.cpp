/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGStopElement.h"
#include "mozilla/dom/SVGStopElementBinding.h"

DOMCI_NODE_DATA(SVGStopElement, mozilla::dom::SVGStopElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Stop)

namespace mozilla {
namespace dom {

JSObject*
SVGStopElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGStopElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

nsSVGElement::NumberInfo SVGStopElement::sNumberInfo =
{ &nsGkAtoms::offset, 0, true };

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGStopElement, SVGStopElementBase)
NS_IMPL_RELEASE_INHERITED(SVGStopElement, SVGStopElementBase)

NS_INTERFACE_TABLE_HEAD(SVGStopElement)
  NS_NODE_INTERFACE_TABLE4(SVGStopElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGStopElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGStopElement)
NS_INTERFACE_MAP_END_INHERITING(SVGStopElementBase)

//----------------------------------------------------------------------
// Implementation

SVGStopElement::SVGStopElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGStopElementBase(aNodeInfo)
{
  SetIsDOMBinding();
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGStopElement)

//----------------------------------------------------------------------
// nsIDOMSVGStopElement methods

/* readonly attribute nsIDOMSVGAnimatedNumber offset; */
NS_IMETHODIMP SVGStopElement::GetOffset(nsIDOMSVGAnimatedNumber * *aOffset)
{
  return mOffset.ToDOMAnimatedNumber(aOffset,this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGStopElement::Offset()
{
  nsCOMPtr<nsIDOMSVGAnimatedNumber> offset;
  mOffset.ToDOMAnimatedNumber(getter_AddRefs(offset), this);
  return offset.forget();
}

//----------------------------------------------------------------------
// sSVGElement methods

nsSVGElement::NumberAttributesInfo
SVGStopElement::GetNumberInfo()
{
  return NumberAttributesInfo(&mOffset, &sNumberInfo, 1);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGStopElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sGradientStopMap
  };

  return FindAttributeDependence(name, map) ||
    SVGStopElementBase::IsAttributeMapped(name);
}

} // namespace dom
} // namespace mozilla

