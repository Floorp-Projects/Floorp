/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFESpotLightElement.h"
#include "mozilla/dom/SVGFESpotLightElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FESpotLight)

namespace mozilla {
namespace dom {

JSObject*
SVGFESpotLightElement::WrapNode(JSContext* aCx, JSObject* aScope)
{
  return SVGFESpotLightElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::NumberInfo SVGFESpotLightElement::sNumberInfo[8] =
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

NS_IMPL_ADDREF_INHERITED(SVGFESpotLightElement,SVGFESpotLightElementBase)
NS_IMPL_RELEASE_INHERITED(SVGFESpotLightElement,SVGFESpotLightElementBase)

NS_INTERFACE_TABLE_HEAD(SVGFESpotLightElement)
  NS_NODE_INTERFACE_TABLE3(SVGFESpotLightElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement)
NS_INTERFACE_MAP_END_INHERITING(SVGFESpotLightElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFESpotLightElement)

//----------------------------------------------------------------------
// nsFEUnstyledElement methods

bool
SVGFESpotLightElement::AttributeAffectsRendering(int32_t aNameSpaceID,
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

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFESpotLightElement::X()
{
  return mNumberAttributes[ATTR_X].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFESpotLightElement::Y()
{
  return mNumberAttributes[ATTR_Y].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFESpotLightElement::Z()
{
  return mNumberAttributes[ATTR_Z].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFESpotLightElement::PointsAtX()
{
  return mNumberAttributes[POINTS_AT_X].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFESpotLightElement::PointsAtY()
{
  return mNumberAttributes[POINTS_AT_Y].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFESpotLightElement::PointsAtZ()
{
  return mNumberAttributes[POINTS_AT_Z].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFESpotLightElement::SpecularExponent()
{
  return mNumberAttributes[SPECULAR_EXPONENT].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFESpotLightElement::LimitingConeAngle()
{
  return mNumberAttributes[LIMITING_CONE_ANGLE].ToDOMAnimatedNumber(this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
SVGFESpotLightElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

} // namespace dom
} // namespace mozilla
