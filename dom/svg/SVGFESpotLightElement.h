/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFESpotLightElement_h
#define mozilla_dom_SVGFESpotLightElement_h

#include "SVGAnimatedNumber.h"
#include "SVGFilters.h"

nsresult NS_NewSVGFESpotLightElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGFELightElement SVGFESpotLightElementBase;

class SVGFESpotLightElement : public SVGFESpotLightElementBase {
  friend nsresult(::NS_NewSVGFESpotLightElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  friend class SVGFELightingElement;

 protected:
  explicit SVGFESpotLightElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFESpotLightElementBase(std::move(aNodeInfo)) {}
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  virtual mozilla::gfx::LightType ComputeLightAttributes(
      nsSVGFilterInstance* aInstance,
      nsTArray<float>& aFloatAttributes) override;
  virtual bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                         nsAtom* aAttribute) const override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedNumber> X();
  already_AddRefed<DOMSVGAnimatedNumber> Y();
  already_AddRefed<DOMSVGAnimatedNumber> Z();
  already_AddRefed<DOMSVGAnimatedNumber> PointsAtX();
  already_AddRefed<DOMSVGAnimatedNumber> PointsAtY();
  already_AddRefed<DOMSVGAnimatedNumber> PointsAtZ();
  already_AddRefed<DOMSVGAnimatedNumber> SpecularExponent();
  already_AddRefed<DOMSVGAnimatedNumber> LimitingConeAngle();

 protected:
  virtual NumberAttributesInfo GetNumberInfo() override;

  enum {
    ATTR_X,
    ATTR_Y,
    ATTR_Z,
    POINTS_AT_X,
    POINTS_AT_Y,
    POINTS_AT_Z,
    SPECULAR_EXPONENT,
    LIMITING_CONE_ANGLE
  };
  SVGAnimatedNumber mNumberAttributes[8];
  static NumberInfo sNumberInfo[8];
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGFESpotLightElement_h
