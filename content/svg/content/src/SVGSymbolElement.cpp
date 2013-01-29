/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGSymbolElement.h"
#include "mozilla/dom/SVGSymbolElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Symbol)

DOMCI_NODE_DATA(SVGSymbolElement, mozilla::dom::SVGSymbolElement)

namespace mozilla {
namespace dom {

JSObject*
SVGSymbolElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGSymbolElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGSymbolElement,SVGSymbolElementBase)
NS_IMPL_RELEASE_INHERITED(SVGSymbolElement,SVGSymbolElementBase)

NS_INTERFACE_TABLE_HEAD(SVGSymbolElement)
  NS_NODE_INTERFACE_TABLE6(SVGSymbolElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGFitToViewBox,
                           nsIDOMSVGSymbolElement, nsIDOMSVGTests)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGSymbolElement)
NS_INTERFACE_MAP_END_INHERITING(SVGSymbolElementBase)

//----------------------------------------------------------------------
// Implementation

SVGSymbolElement::SVGSymbolElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGSymbolElementBase(aNodeInfo)
{
  SetIsDOMBinding();
}


//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGSymbolElement)

//----------------------------------------------------------------------
// nsIDOMSVGFitToViewBox methods

/* readonly attribute nsIDOMSVGAnimatedRect viewBox; */
NS_IMETHODIMP SVGSymbolElement::GetViewBox(nsIDOMSVGAnimatedRect * *aViewBox)
{
  *aViewBox = ViewBox().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedRect>
SVGSymbolElement::ViewBox()
{
  nsCOMPtr<nsIDOMSVGAnimatedRect> rect;
  mViewBox.ToDOMAnimatedRect(getter_AddRefs(rect), this);
  return rect.forget();
}

/* readonly attribute SVGPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
SVGSymbolElement::GetPreserveAspectRatio(nsISupports
                                         **aPreserveAspectRatio)
{
  *aPreserveAspectRatio = PreserveAspectRatio().get();
  return NS_OK;
}

already_AddRefed<DOMSVGAnimatedPreserveAspectRatio>
SVGSymbolElement::PreserveAspectRatio()
{
  nsRefPtr<DOMSVGAnimatedPreserveAspectRatio> ratio;
  mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(getter_AddRefs(ratio), this);
  return ratio.forget();
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
