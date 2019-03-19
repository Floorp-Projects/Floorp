/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEDistantLightElement.h"
#include "mozilla/dom/SVGFEDistantLightElementBinding.h"
#include "nsSVGFilterInstance.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FEDistantLight)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject* SVGFEDistantLightElement::WrapNode(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return SVGFEDistantLightElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::NumberInfo SVGFEDistantLightElement::sNumberInfo[2] = {
    {nsGkAtoms::azimuth, 0, false}, {nsGkAtoms::elevation, 0, false}};

//----------------------------------------------------------------------
//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEDistantLightElement)

// nsFEUnstyledElement methods

bool SVGFEDistantLightElement::AttributeAffectsRendering(
    int32_t aNameSpaceID, nsAtom* aAttribute) const {
  return aNameSpaceID == kNameSpaceID_None &&
         (aAttribute == nsGkAtoms::azimuth ||
          aAttribute == nsGkAtoms::elevation);
}

LightType SVGFEDistantLightElement::ComputeLightAttributes(
    nsSVGFilterInstance* aInstance, nsTArray<float>& aFloatAttributes) {
  float azimuth, elevation;
  GetAnimatedNumberValues(&azimuth, &elevation, nullptr);

  aFloatAttributes.SetLength(kDistantLightNumAttributes);
  aFloatAttributes[kDistantLightAzimuthIndex] = azimuth;
  aFloatAttributes[kDistantLightElevationIndex] = elevation;
  return LightType::Distant;
}

already_AddRefed<DOMSVGAnimatedNumber> SVGFEDistantLightElement::Azimuth() {
  return mNumberAttributes[AZIMUTH].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber> SVGFEDistantLightElement::Elevation() {
  return mNumberAttributes[ELEVATION].ToDOMAnimatedNumber(this);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::NumberAttributesInfo SVGFEDistantLightElement::GetNumberInfo() {
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

}  // namespace dom
}  // namespace mozilla
