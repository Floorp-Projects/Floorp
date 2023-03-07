/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGFEDISTANTLIGHTELEMENT_H_
#define DOM_SVG_SVGFEDISTANTLIGHTELEMENT_H_

#include "SVGAnimatedNumber.h"
#include "mozilla/dom/SVGFilters.h"

nsresult NS_NewSVGFEDistantLightElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

using SVGFEDistantLightElementBase = SVGFELightElement;

class SVGFEDistantLightElement final : public SVGFEDistantLightElementBase {
  friend nsresult(::NS_NewSVGFEDistantLightElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFEDistantLightElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFEDistantLightElementBase(std::move(aNodeInfo)) {}
  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

 public:
  mozilla::gfx::LightType ComputeLightAttributes(
      SVGFilterInstance* aInstance, nsTArray<float>& aFloatAttributes) override;
  bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                 nsAtom* aAttribute) const override;

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedNumber> Azimuth();
  already_AddRefed<DOMSVGAnimatedNumber> Elevation();

 protected:
  NumberAttributesInfo GetNumberInfo() override;

  enum { AZIMUTH, ELEVATION };
  SVGAnimatedNumber mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGFEDISTANTLIGHTELEMENT_H_
