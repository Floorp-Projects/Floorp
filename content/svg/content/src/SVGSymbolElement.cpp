/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGSymbolElement.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Symbol)

DOMCI_NODE_DATA(SVGSymbolElement, mozilla::dom::SVGSymbolElement)

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGSymbolElement,SVGSymbolElementBase)
NS_IMPL_RELEASE_INHERITED(SVGSymbolElement,SVGSymbolElementBase)

NS_INTERFACE_TABLE_HEAD(SVGSymbolElement)
  NS_NODE_INTERFACE_TABLE5(SVGSymbolElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGFitToViewBox,
                           nsIDOMSVGSymbolElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGSymbolElement)
NS_INTERFACE_MAP_END_INHERITING(SVGSymbolElementBase)

//----------------------------------------------------------------------
// Implementation

SVGSymbolElement::SVGSymbolElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGSymbolElementBase(aNodeInfo)
{
}


//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGSymbolElement)

//----------------------------------------------------------------------
// nsIDOMSVGFitToViewBox methods

/* readonly attribute nsIDOMSVGAnimatedRect viewBox; */
NS_IMETHODIMP SVGSymbolElement::GetViewBox(nsIDOMSVGAnimatedRect * *aViewBox)
{
  return mViewBox.ToDOMAnimatedRect(aViewBox, this);
}

/* readonly attribute SVGPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
SVGSymbolElement::GetPreserveAspectRatio(nsISupports
                                         **aPreserveAspectRatio)
{
  nsRefPtr<DOMSVGAnimatedPreserveAspectRatio> ratio;
  mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(getter_AddRefs(ratio), this);
  ratio.forget(aPreserveAspectRatio);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGSymbolElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFEFloodMap,
    sFillStrokeMap,
    sFiltersMap,
    sFontSpecificationMap,
    sGradientStopMap,
    sGraphicsMap,
    sLightingEffectsMap,
    sMarkersMap,
    sTextContentElementsMap,
    sViewportsMap
   };

  return FindAttributeDependence(name, map) ||
    SVGSymbolElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGViewBox *
SVGSymbolElement::GetViewBox()
{
  return &mViewBox;
}

SVGAnimatedPreserveAspectRatio *
SVGSymbolElement::GetPreserveAspectRatio()
{
  return &mPreserveAspectRatio;
}

} // namespace dom
} // namespace mozilla
