/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGFEBLENDELEMENT_H_
#define DOM_SVG_SVGFEBLENDELEMENT_H_

#include "SVGAnimatedEnumeration.h"
#include "mozilla/dom/SVGFilters.h"

nsresult NS_NewSVGFEBlendElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
namespace mozilla::dom {

using SVGFEBlendElementBase = SVGFilterPrimitiveElement;

class SVGFEBlendElement final : public SVGFEBlendElementBase {
  friend nsresult(::NS_NewSVGFEBlendElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFEBlendElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFEBlendElementBase(std::move(aNodeInfo)) {}
  JSObject* WrapNode(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

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

  nsresult BindToTree(BindContext&, nsINode& aParent) override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedString> In1();
  already_AddRefed<DOMSVGAnimatedString> In2();
  already_AddRefed<DOMSVGAnimatedEnumeration> Mode();

 protected:
  EnumAttributesInfo GetEnumInfo() override;
  StringAttributesInfo GetStringInfo() override;

  enum { MODE };
  SVGAnimatedEnumeration mEnumAttributes[1];
  static SVGEnumMapping sModeMap[];
  static EnumInfo sEnumInfo[1];

  enum { RESULT, IN1, IN2 };
  SVGAnimatedString mStringAttributes[3];
  static StringInfo sStringInfo[3];
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGFEBLENDELEMENT_H_
