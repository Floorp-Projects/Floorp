/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGFECOMPOSITEELEMENT_H_
#define DOM_SVG_SVGFECOMPOSITEELEMENT_H_

#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedNumber.h"
#include "mozilla/dom/SVGFilters.h"

nsresult NS_NewSVGFECompositeElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

using SVGFECompositeElementBase = SVGFilterPrimitiveElement;

class SVGFECompositeElement final : public SVGFECompositeElementBase {
  friend nsresult(::NS_NewSVGFECompositeElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFECompositeElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFECompositeElementBase(std::move(aNodeInfo)) {}
  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

 public:
  FilterPrimitiveDescription GetPrimitiveDescription(
      SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
      const nsTArray<bool>& aInputsAreTainted,
      nsTArray<RefPtr<SourceSurface>>& aInputImages) override;
  bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                 nsAtom* aAttribute) const override;
  SVGAnimatedString& GetResultImageName() override {
    return mStringAttributes[RESULT];
  }
  void GetSourceImageNames(nsTArray<SVGStringInfo>& aSources) override;

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  nsresult BindToTree(BindContext& aCtx, nsINode& aParent) override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedString> In1();
  already_AddRefed<DOMSVGAnimatedString> In2();
  already_AddRefed<DOMSVGAnimatedEnumeration> Operator();
  already_AddRefed<DOMSVGAnimatedNumber> K1();
  already_AddRefed<DOMSVGAnimatedNumber> K2();
  already_AddRefed<DOMSVGAnimatedNumber> K3();
  already_AddRefed<DOMSVGAnimatedNumber> K4();
  void SetK(float k1, float k2, float k3, float k4);

 protected:
  NumberAttributesInfo GetNumberInfo() override;
  EnumAttributesInfo GetEnumInfo() override;
  StringAttributesInfo GetStringInfo() override;

  enum { ATTR_K1, ATTR_K2, ATTR_K3, ATTR_K4 };
  SVGAnimatedNumber mNumberAttributes[4];
  static NumberInfo sNumberInfo[4];

  enum { OPERATOR };
  SVGAnimatedEnumeration mEnumAttributes[1];
  static SVGEnumMapping sOperatorMap[];
  static EnumInfo sEnumInfo[1];

  enum { RESULT, IN1, IN2 };
  SVGAnimatedString mStringAttributes[3];
  static StringInfo sStringInfo[3];
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGFECOMPOSITEELEMENT_H_
