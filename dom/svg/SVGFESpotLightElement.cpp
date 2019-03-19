/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFESpotLightElement.h"
#include "mozilla/dom/SVGFESpotLightElementBinding.h"
#include "nsSVGFilterInstance.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FESpotLight)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject* SVGFESpotLightElement::WrapNode(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return SVGFESpotLightElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::NumberInfo SVGFESpotLightElement::sNumberInfo[8] = {
    {nsGkAtoms::x, 0, false},
    {nsGkAtoms::y, 0, false},
    {nsGkAtoms::z, 0, false},
    {nsGkAtoms::pointsAtX, 0, false},
    {nsGkAtoms::pointsAtY, 0, false},
    {nsGkAtoms::pointsAtZ, 0, false},
    {nsGkAtoms::specularExponent, 1, false},
    {nsGkAtoms::limitingConeAngle, 0, false}};

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFESpotLightElement)

//----------------------------------------------------------------------
// nsFEUnstyledElement methods

bool SVGFESpotLightElement::AttributeAffectsRendering(
    int32_t aNameSpaceID, nsAtom* aAttribute) const {
  return aNameSpaceID == kNameSpaceID_None &&
         (aAttribute == nsGkAtoms::x || aAttribute == nsGkAtoms::y ||
          aAttribute == nsGkAtoms::z || aAttribute == nsGkAtoms::pointsAtX ||
          aAttribute == nsGkAtoms::pointsAtY ||
          aAttribute == nsGkAtoms::pointsAtZ ||
          aAttribute == nsGkAtoms::specularExponent ||
          aAttribute == nsGkAtoms::limitingConeAngle);
}

//----------------------------------------------------------------------

LightType SVGFESpotLightElement::ComputeLightAttributes(
    nsSVGFilterInstance* aInstance, nsTArray<float>& aFloatAttributes) {
  aFloatAttributes.SetLength(kSpotLightNumAttributes);
  GetAnimatedNumberValues(&aFloatAttributes[kSpotLightPositionXIndex],
                          &aFloatAttributes[kSpotLightPositionYIndex],
                          &aFloatAttributes[kSpotLightPositionZIndex],
                          &aFloatAttributes[kSpotLightPointsAtXIndex],
                          &aFloatAttributes[kSpotLightPointsAtYIndex],
                          &aFloatAttributes[kSpotLightPointsAtZIndex],
                          &aFloatAttributes[kSpotLightFocusIndex],
                          &aFloatAttributes[kSpotLightLimitingConeAngleIndex],
                          nullptr);
  if (!mNumberAttributes[SVGFESpotLightElement::LIMITING_CONE_ANGLE]
           .IsExplicitlySet()) {
    aFloatAttributes[kSpotLightLimitingConeAngleIndex] = 90;
  }

  return LightType::Spot;
}

already_AddRefed<DOMSVGAnimatedNumber> SVGFESpotLightElement::X() {
  return mNumberAttributes[ATTR_X].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber> SVGFESpotLightElement::Y() {
  return mNumberAttributes[ATTR_Y].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber> SVGFESpotLightElement::Z() {
  return mNumberAttributes[ATTR_Z].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber> SVGFESpotLightElement::PointsAtX() {
  return mNumberAttributes[POINTS_AT_X].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber> SVGFESpotLightElement::PointsAtY() {
  return mNumberAttributes[POINTS_AT_Y].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber> SVGFESpotLightElement::PointsAtZ() {
  return mNumberAttributes[POINTS_AT_Z].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGFESpotLightElement::SpecularExponent() {
  return mNumberAttributes[SPECULAR_EXPONENT].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGFESpotLightElement::LimitingConeAngle() {
  return mNumberAttributes[LIMITING_CONE_ANGLE].ToDOMAnimatedNumber(this);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::NumberAttributesInfo SVGFESpotLightElement::GetNumberInfo() {
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

}  // namespace dom
}  // namespace mozilla
