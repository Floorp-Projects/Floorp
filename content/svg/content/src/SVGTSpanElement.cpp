/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGTSpanElement.h"
#include "mozilla/dom/SVGTSpanElementBinding.h"

DOMCI_NODE_DATA(SVGTSpanElement, mozilla::dom::SVGTSpanElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(TSpan)

namespace mozilla {
namespace dom {

JSObject*
SVGTSpanElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGTSpanElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGTSpanElement,SVGTSpanElementBase)
NS_IMPL_RELEASE_INHERITED(SVGTSpanElement,SVGTSpanElementBase)

NS_INTERFACE_TABLE_HEAD(SVGTSpanElement)
  NS_NODE_INTERFACE_TABLE6(SVGTSpanElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTSpanElement,
                           nsIDOMSVGTextPositioningElement,
                           nsIDOMSVGTextContentElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGTSpanElement)
NS_INTERFACE_MAP_END_INHERITING(SVGTSpanElementBase)

//----------------------------------------------------------------------
// Implementation

SVGTSpanElement::SVGTSpanElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGTSpanElementBase(aNodeInfo)
{
  SetIsDOMBinding();
}


//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGTSpanElement)


//----------------------------------------------------------------------
// nsIDOMSVGTSpanElement methods

// - no methods -

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGTSpanElement::IsAttributeMapped(const nsIAtom* name) const
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

//----------------------------------------------------------------------
// nsSVGElement overrides

bool
SVGTSpanElement::IsEventName(nsIAtom* aName)
{
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_SVGGraphic);
}

} // namespace dom
} // namespace mozilla
