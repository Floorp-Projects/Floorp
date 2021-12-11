/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGFESPECULARLIGHTINGELEMENT_H_
#define DOM_SVG_SVGFESPECULARLIGHTINGELEMENT_H_

#include "mozilla/dom/SVGFilters.h"

nsresult NS_NewSVGFESpecularLightingElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

//---------------------SpecularLighting------------------------

using SVGFESpecularLightingElementBase = SVGFELightingElement;

class SVGFESpecularLightingElement : public SVGFESpecularLightingElementBase {
  friend nsresult(::NS_NewSVGFESpecularLightingElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFESpecularLightingElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFESpecularLightingElementBase(std::move(aNodeInfo)) {}
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;

  virtual FilterPrimitiveDescription GetPrimitiveDescription(
      SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
      const nsTArray<bool>& aInputsAreTainted,
      nsTArray<RefPtr<SourceSurface>>& aInputImages) override;
  virtual bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                         nsAtom* aAttribute) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedString> In1();
  already_AddRefed<DOMSVGAnimatedNumber> SurfaceScale();
  already_AddRefed<DOMSVGAnimatedNumber> SpecularConstant();
  already_AddRefed<DOMSVGAnimatedNumber> SpecularExponent();
  already_AddRefed<DOMSVGAnimatedNumber> KernelUnitLengthX();
  already_AddRefed<DOMSVGAnimatedNumber> KernelUnitLengthY();
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGFESPECULARLIGHTINGELEMENT_H_
