/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGLineElement.h"
#include "mozilla/dom/SVGLineElementBinding.h"
#include "gfxContext.h"

DOMCI_NODE_DATA(SVGLineElement, mozilla::dom::SVGLineElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Line)

namespace mozilla {
namespace dom {

JSObject*
SVGLineElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGLineElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

nsSVGElement::LengthInfo SVGLineElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x1, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::y1, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::x2, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::y2, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGLineElement,SVGLineElementBase)
NS_IMPL_RELEASE_INHERITED(SVGLineElement,SVGLineElementBase)

NS_INTERFACE_TABLE_HEAD(SVGLineElement)
  NS_NODE_INTERFACE_TABLE4(SVGLineElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGLineElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGLineElement)
NS_INTERFACE_MAP_END_INHERITING(SVGLineElementBase)

//----------------------------------------------------------------------
// Implementation

SVGLineElement::SVGLineElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGLineElementBase(aNodeInfo)
{
  SetIsDOMBinding();
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGLineElement)

//----------------------------------------------------------------------
// nsIDOMSVGLineElement methods

/* readonly attribute nsIDOMSVGAnimatedLength cx; */
NS_IMETHODIMP SVGLineElement::GetX1(nsIDOMSVGAnimatedLength * *aX1)
{
  *aX1 = X1().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGLineElement::X1()
{
  return mLengthAttributes[ATTR_X1].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength cy; */
NS_IMETHODIMP SVGLineElement::GetY1(nsIDOMSVGAnimatedLength * *aY1)
{
  *aY1 = Y1().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGLineElement::Y1()
{
  return mLengthAttributes[ATTR_Y1].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength rx; */
NS_IMETHODIMP SVGLineElement::GetX2(nsIDOMSVGAnimatedLength * *aX2)
{
  *aX2 = X2().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGLineElement::X2()
{
  return mLengthAttributes[ATTR_X2].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength ry; */
NS_IMETHODIMP SVGLineElement::GetY2(nsIDOMSVGAnimatedLength * *aY2)
{
  *aY2 = Y2().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGLineElement::Y2()
{
  return mLengthAttributes[ATTR_Y2].ToDOMAnimatedLength(this);
}


//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGLineElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sMarkersMap
  };

  return FindAttributeDependence(name, map) ||
    SVGLineElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthAttributesInfo
SVGLineElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

void
SVGLineElement::GetMarkPoints(nsTArray<nsSVGMark> *aMarks) {
  float x1, y1, x2, y2;

  GetAnimatedLengthValues(&x1, &y1, &x2, &y2, nullptr);

  float angle = atan2(y2 - y1, x2 - x1);

  aMarks->AppendElement(nsSVGMark(x1, y1, angle));
  aMarks->AppendElement(nsSVGMark(x2, y2, angle));
}

void
SVGLineElement::ConstructPath(gfxContext *aCtx)
{
  float x1, y1, x2, y2;

  GetAnimatedLengthValues(&x1, &y1, &x2, &y2, nullptr);

  aCtx->MoveTo(gfxPoint(x1, y1));
  aCtx->LineTo(gfxPoint(x2, y2));
}

} // namespace dom
} // namespace mozilla
