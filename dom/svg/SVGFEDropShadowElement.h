/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGFEDROPSHADOWELEMENT_H_
#define DOM_SVG_SVGFEDROPSHADOWELEMENT_H_

#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberPair.h"
#include "SVGAnimatedString.h"
#include "mozilla/dom/SVGFilters.h"

nsresult NS_NewSVGFEDropShadowElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

using SVGFEDropShadowElementBase = SVGFilterPrimitiveElement;

class SVGFEDropShadowElement final : public SVGFEDropShadowElementBase {
  friend nsresult(::NS_NewSVGFEDropShadowElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFEDropShadowElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFEDropShadowElementBase(std::move(aNodeInfo)) {}
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

  bool OutputIsTainted(const nsTArray<bool>& aInputsAreTainted,
                       nsIPrincipal* aReferencePrincipal) override;

  void GetSourceImageNames(nsTArray<SVGStringInfo>& aSources) override;

  // nsIContent interface
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedString> In1();
  already_AddRefed<DOMSVGAnimatedNumber> Dx();
  already_AddRefed<DOMSVGAnimatedNumber> Dy();
  already_AddRefed<DOMSVGAnimatedNumber> StdDeviationX();
  already_AddRefed<DOMSVGAnimatedNumber> StdDeviationY();
  void SetStdDeviation(float stdDeviationX, float stdDeviationY);

 protected:
  NumberAttributesInfo GetNumberInfo() override;
  NumberPairAttributesInfo GetNumberPairInfo() override;
  StringAttributesInfo GetStringInfo() override;

  enum { DX, DY };
  SVGAnimatedNumber mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];

  enum { STD_DEV };
  SVGAnimatedNumberPair mNumberPairAttributes[1];
  static NumberPairInfo sNumberPairInfo[1];

  enum { RESULT, IN1 };
  SVGAnimatedString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGFEDROPSHADOWELEMENT_H_
