/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGRectElement.h"
#include "nsGkAtoms.h"
#include "gfxContext.h"
#include "mozilla/dom/SVGRectElementBinding.h"
#include <algorithm>

DOMCI_NODE_DATA(SVGRectElement, mozilla::dom::SVGRectElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Rect)

namespace mozilla {
namespace dom {

JSObject*
SVGRectElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGRectElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

nsSVGElement::LengthInfo SVGRectElement::sLengthInfo[6] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::width, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::height, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::rx, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::ry, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGRectElement,SVGRectElementBase)
NS_IMPL_RELEASE_INHERITED(SVGRectElement,SVGRectElementBase)

NS_INTERFACE_TABLE_HEAD(SVGRectElement)
  NS_NODE_INTERFACE_TABLE4(SVGRectElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGRectElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGRectElement)
NS_INTERFACE_MAP_END_INHERITING(SVGRectElementBase)

//----------------------------------------------------------------------
// Implementation

SVGRectElement::SVGRectElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGRectElementBase(aNodeInfo)
{
  SetIsDOMBinding();
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGRectElement)

//----------------------------------------------------------------------
// nsIDOMSVGRectElement methods

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP SVGRectElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  *aX = X().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGRectElement::X()
{
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP SVGRectElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  *aY = Y().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGRectElement::Y()
{
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP SVGRectElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  *aWidth = Width().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGRectElement::Width()
{
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP SVGRectElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  *aHeight = Height().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGRectElement::Height()
{
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength rx; */
NS_IMETHODIMP SVGRectElement::GetRx(nsIDOMSVGAnimatedLength * *aRx)
{
  *aRx = Rx().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGRectElement::Rx()
{
  return mLengthAttributes[ATTR_RX].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength ry; */
NS_IMETHODIMP SVGRectElement::GetRy(nsIDOMSVGAnimatedLength * *aRy)
{
  *aRy = Ry().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGRectElement::Ry()
{
  return mLengthAttributes[ATTR_RY].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ bool
SVGRectElement::HasValidDimensions() const
{
  return mLengthAttributes[ATTR_WIDTH].IsExplicitlySet() &&
         mLengthAttributes[ATTR_WIDTH].GetAnimValInSpecifiedUnits() > 0 &&
         mLengthAttributes[ATTR_HEIGHT].IsExplicitlySet() &&
         mLengthAttributes[ATTR_HEIGHT].GetAnimValInSpecifiedUnits() > 0;
}

nsSVGElement::LengthAttributesInfo
SVGRectElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

void
SVGRectElement::ConstructPath(gfxContext *aCtx)
{
  float x, y, width, height, rx, ry;

  GetAnimatedLengthValues(&x, &y, &width, &height, &rx, &ry, nullptr);

  /* In a perfect world, this would be handled by the DOM, and
     return a DOM exception. */
  if (width <= 0 || height <= 0)
    return;

  rx = std::max(rx, 0.0f);
  ry = std::max(ry, 0.0f);

  /* optimize the no rounded corners case */
  if (rx == 0 && ry == 0) {
    aCtx->Rectangle(gfxRect(x, y, width, height));
    return;
  }

  /* If either the 'rx' or the 'ry' attribute isn't set, then we
     have to set it to the value of the other. */
  bool hasRx = mLengthAttributes[ATTR_RX].IsExplicitlySet();
  bool hasRy = mLengthAttributes[ATTR_RY].IsExplicitlySet();
  if (hasRx && !hasRy)
    ry = rx;
  else if (hasRy && !hasRx)
    rx = ry;

  /* Clamp rx and ry to half the rect's width and height respectively. */
  float halfWidth  = width/2;
  float halfHeight = height/2;
  if (rx > halfWidth)
    rx = halfWidth;
  if (ry > halfHeight)
    ry = halfHeight;

  gfxSize corner(rx, ry);
  aCtx->RoundedRectangle(gfxRect(x, y, width, height),
                         gfxCornerSizes(corner, corner, corner, corner));
}

} // namespace dom
} // namespace mozilla
