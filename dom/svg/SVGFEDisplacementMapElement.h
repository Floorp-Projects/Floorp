/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGFEDISPLACEMENTMAPELEMENT_H_
#define DOM_SVG_SVGFEDISPLACEMENTMAPELEMENT_H_

#include "SVGAnimatedEnumeration.h"
#include "mozilla/dom/SVGFilters.h"

nsresult NS_NewSVGFEDisplacementMapElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

using SVGFEDisplacementMapElementBase = SVGFE;

class SVGFEDisplacementMapElement : public SVGFEDisplacementMapElementBase {
 protected:
  friend nsresult(::NS_NewSVGFEDisplacementMapElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGFEDisplacementMapElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFEDisplacementMapElementBase(std::move(aNodeInfo)) {}
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  virtual FilterPrimitiveDescription GetPrimitiveDescription(
      SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
      const nsTArray<bool>& aInputsAreTainted,
      nsTArray<RefPtr<SourceSurface>>& aInputImages) override;
  virtual bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                         nsAtom* aAttribute) const override;
  virtual SVGAnimatedString& GetResultImageName() override {
    return mStringAttributes[RESULT];
  }
  virtual void GetSourceImageNames(nsTArray<SVGStringInfo>& aSources) override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  virtual nsresult BindToTree(BindContext& aCtx, nsINode& aParent) override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedString> In1();
  already_AddRefed<DOMSVGAnimatedString> In2();
  already_AddRefed<DOMSVGAnimatedNumber> Scale();
  already_AddRefed<DOMSVGAnimatedEnumeration> XChannelSelector();
  already_AddRefed<DOMSVGAnimatedEnumeration> YChannelSelector();

 protected:
  virtual bool OperatesOnSRGB(int32_t aInputIndex,
                              bool aInputIsAlreadySRGB) override {
    switch (aInputIndex) {
      case 0:
        return aInputIsAlreadySRGB;
      case 1:
        return SVGFEDisplacementMapElementBase::OperatesOnSRGB(
            aInputIndex, aInputIsAlreadySRGB);
      default:
        NS_ERROR("Will not give correct color model");
        return false;
    }
  }

  virtual NumberAttributesInfo GetNumberInfo() override;
  virtual EnumAttributesInfo GetEnumInfo() override;
  virtual StringAttributesInfo GetStringInfo() override;

  enum { SCALE };
  SVGAnimatedNumber mNumberAttributes[1];
  static NumberInfo sNumberInfo[1];

  enum { CHANNEL_X, CHANNEL_Y };
  SVGAnimatedEnumeration mEnumAttributes[2];
  static SVGEnumMapping sChannelMap[];
  static EnumInfo sEnumInfo[2];

  enum { RESULT, IN1, IN2 };
  SVGAnimatedString mStringAttributes[3];
  static StringInfo sStringInfo[3];
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGFEDISPLACEMENTMAPELEMENT_H_
