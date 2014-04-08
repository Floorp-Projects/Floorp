/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGGElement.h"
#include "mozilla/dom/SVGGElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(G)

namespace mozilla {
namespace dom {

JSObject*
SVGGElement::WrapNode(JSContext *aCx)
{
  return SVGGElementBinding::Wrap(aCx, this);
}

//----------------------------------------------------------------------
// Implementation

SVGGElement::SVGGElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : SVGGraphicsElement(aNodeInfo)
{
}


//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGGElement)


//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGGElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFEFloodMap,
    sFiltersMap,
    sFontSpecificationMap,
    sGradientStopMap,
    sLightingEffectsMap,
    sMarkersMap,
    sTextContentElementsMap,
    sViewportsMap
  };

  return FindAttributeDependence(name, map) ||
    SVGGraphicsElement::IsAttributeMapped(name);
}

} // namespace dom
} // namespace mozilla

