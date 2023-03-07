/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGFEPOINTLIGHTELEMENT_H_
#define DOM_SVG_SVGFEPOINTLIGHTELEMENT_H_

#include "SVGAnimatedNumber.h"
#include "mozilla/dom/SVGFilters.h"

nsresult NS_NewSVGFEPointLightElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

using SVGFEPointLightElementBase = SVGFELightElement;

class SVGFEPointLightElement final : public SVGFEPointLightElementBase {
  friend nsresult(::NS_NewSVGFEPointLightElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFEPointLightElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFEPointLightElementBase(std::move(aNodeInfo)) {}
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

 public:
  mozilla::gfx::LightType ComputeLightAttributes(
      SVGFilterInstance* aInstance, nsTArray<float>& aFloatAttributes) override;
  bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                 nsAtom* aAttribute) const override;

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedNumber> X();
  already_AddRefed<DOMSVGAnimatedNumber> Y();
  already_AddRefed<DOMSVGAnimatedNumber> Z();

 protected:
  NumberAttributesInfo GetNumberInfo() override;

  enum { ATTR_X, ATTR_Y, ATTR_Z };
  SVGAnimatedNumber mNumberAttributes[3];
  static NumberInfo sNumberInfo[3];
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGFEPOINTLIGHTELEMENT_H_
