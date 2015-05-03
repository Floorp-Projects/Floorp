/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFESpotLightElement.h"
#include "mozilla/dom/SVGFESpotLightElementBinding.h"
#include "nsSVGFilterInstance.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FESpotLight)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGFESpotLightElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGFESpotLightElementBinding::Wrap(aCx, this, aGivenProto);
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

AttributeMap
SVGFESpotLightElement::ComputeLightAttributes(nsSVGFilterInstance* aInstance)
{
  Point3D lightPos, pointsAt;
  float specularExponent, limitingConeAngle;
  GetAnimatedNumberValues(&lightPos.x, &lightPos.y, &lightPos.z,
                          &pointsAt.x, &pointsAt.y, &pointsAt.z,
                          &specularExponent, &limitingConeAngle,
                          nullptr);
  if (!mNumberAttributes[SVGFESpotLightElement::LIMITING_CONE_ANGLE].IsExplicitlySet()) {
    limitingConeAngle = 90;
  }

  AttributeMap map;
  map.Set(eLightType, (uint32_t)eLightTypeSpot);
  map.Set(eSpotLightPosition, aInstance->ConvertLocation(lightPos));
  map.Set(eSpotLightPointsAt, aInstance->ConvertLocation(pointsAt));
  map.Set(eSpotLightFocus, specularExponent);
  map.Set(eSpotLightLimitingConeAngle, limitingConeAngle);
  return map;
}

already_AddRefed<SVGAnimatedNumber>
SVGFESpotLightElement::X()
{
  return mNumberAttributes[ATTR_X].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFESpotLightElement::Y()
{
  return mNumberAttributes[ATTR_Y].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFESpotLightElement::Z()
{
  return mNumberAttributes[ATTR_Z].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFESpotLightElement::PointsAtX()
{
  return mNumberAttributes[POINTS_AT_X].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFESpotLightElement::PointsAtY()
{
  return mNumberAttributes[POINTS_AT_Y].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFESpotLightElement::PointsAtZ()
{
  return mNumberAttributes[POINTS_AT_Z].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFESpotLightElement::SpecularExponent()
{
  return mNumberAttributes[SPECULAR_EXPONENT].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
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
