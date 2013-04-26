/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGSymbolElement.h"
#include "mozilla/dom/SVGSymbolElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Symbol)

namespace mozilla {
namespace dom {

JSObject*
SVGSymbolElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return SVGSymbolElementBinding::Wrap(aCx, aScope, this);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS_INHERITED4(SVGSymbolElement, SVGSymbolElementBase,
                             nsIDOMNode, nsIDOMElement,
                             nsIDOMSVGElement, mozilla::dom::SVGTests)

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

already_AddRefed<nsIDOMSVGAnimatedRect>
SVGSymbolElement::ViewBox()
{
  nsCOMPtr<nsIDOMSVGAnimatedRect> rect;
  mViewBox.ToDOMAnimatedRect(getter_AddRefs(rect), this);
  return rect.forget();
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
