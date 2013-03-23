/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFESpotLightElement.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FESpotLight)

namespace mozilla {
namespace dom {

nsSVGElement::NumberInfo nsSVGFESpotLightElement::sNumberInfo[8] =
{
  { &nsGkAtoms::x, 0, false },
  { &nsGkAtoms::y, 0, false },
  { &nsGkAtoms::z, 0, false },
  { &nsGkAtoms::pointsAtX, 0, false },
  { &nsGkAtoms::pointsAtY, 0, false },
  { &nsGkAtoms::pointsAtZ, 0, false },
  { &nsGkAtoms::specularExponent, 1, false },
  { &nsGkAtoms::limitingConeAngle, 0, false }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFESpotLightElement,nsSVGFESpotLightElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFESpotLightElement,nsSVGFESpotLightElementBase)

DOMCI_NODE_DATA(SVGFESpotLightElement, nsSVGFESpotLightElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFESpotLightElement)
  NS_NODE_INTERFACE_TABLE4(nsSVGFESpotLightElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGFESpotLightElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFESpotLightElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFESpotLightElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFESpotLightElement)

//----------------------------------------------------------------------
// nsFEUnstyledElement methods

bool
nsSVGFESpotLightElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                   nsIAtom* aAttribute) const
{
  return aNameSpaceID == kNameSpaceID_None &&
         (aAttribute == nsGkAtoms::x ||
          aAttribute == nsGkAtoms::y ||
          aAttribute == nsGkAtoms::z ||
          aAttribute == nsGkAtoms::pointsAtX ||
          aAttribute == nsGkAtoms::pointsAtY ||
          aAttribute == nsGkAtoms::pointsAtZ ||
          aAttribute == nsGkAtoms::specularExponent ||
          aAttribute == nsGkAtoms::limitingConeAngle);
}

//----------------------------------------------------------------------
// nsIDOMSVGFESpotLightElement methods

NS_IMETHODIMP
nsSVGFESpotLightElement::GetX(nsIDOMSVGAnimatedNumber **aX)
{
  return mNumberAttributes[X].ToDOMAnimatedNumber(aX, this);
}

NS_IMETHODIMP
nsSVGFESpotLightElement::GetY(nsIDOMSVGAnimatedNumber **aY)
{
  return mNumberAttributes[Y].ToDOMAnimatedNumber(aY, this);
}

NS_IMETHODIMP
nsSVGFESpotLightElement::GetZ(nsIDOMSVGAnimatedNumber **aZ)
{
  return mNumberAttributes[Z].ToDOMAnimatedNumber(aZ, this);
}

NS_IMETHODIMP
nsSVGFESpotLightElement::GetPointsAtX(nsIDOMSVGAnimatedNumber **aX)
{
  return mNumberAttributes[POINTS_AT_X].ToDOMAnimatedNumber(aX, this);
}

NS_IMETHODIMP
nsSVGFESpotLightElement::GetPointsAtY(nsIDOMSVGAnimatedNumber **aY)
{
  return mNumberAttributes[POINTS_AT_Y].ToDOMAnimatedNumber(aY, this);
}

NS_IMETHODIMP
nsSVGFESpotLightElement::GetPointsAtZ(nsIDOMSVGAnimatedNumber **aZ)
{
  return mNumberAttributes[POINTS_AT_Z].ToDOMAnimatedNumber(aZ, this);
}

NS_IMETHODIMP
nsSVGFESpotLightElement::GetSpecularExponent(nsIDOMSVGAnimatedNumber **aExponent)
{
  return mNumberAttributes[SPECULAR_EXPONENT].ToDOMAnimatedNumber(aExponent,
                                                                  this);
}

NS_IMETHODIMP
nsSVGFESpotLightElement::GetLimitingConeAngle(nsIDOMSVGAnimatedNumber **aAngle)
{
  return mNumberAttributes[LIMITING_CONE_ANGLE].ToDOMAnimatedNumber(aAngle,
                                                                    this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFESpotLightElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

} // namespace dom
} // namespace mozilla
