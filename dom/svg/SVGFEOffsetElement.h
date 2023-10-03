/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGFEOFFSETELEMENT_H_
#define DOM_SVG_SVGFEOFFSETELEMENT_H_

#include "SVGAnimatedNumber.h"
#include "SVGAnimatedString.h"
#include "mozilla/dom/SVGFilters.h"

nsresult NS_NewSVGFEOffsetElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla::dom {

using SVGFEOffsetElementBase = SVGFilterPrimitiveElement;

class SVGFEOffsetElement final : public SVGFEOffsetElementBase {
  friend nsresult(::NS_NewSVGFEOffsetElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFEOffsetElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFEOffsetElementBase(std::move(aNodeInfo)) {}
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

  nsresult BindToTree(BindContext&, nsINode& aParent) override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedString> In1();
  already_AddRefed<DOMSVGAnimatedNumber> Dx();
  already_AddRefed<DOMSVGAnimatedNumber> Dy();

 protected:
  NumberAttributesInfo GetNumberInfo() override;
  StringAttributesInfo GetStringInfo() override;

  enum { DX, DY };
  SVGAnimatedNumber mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];

  enum { RESULT, IN1 };
  SVGAnimatedString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGFEOFFSETELEMENT_H_
