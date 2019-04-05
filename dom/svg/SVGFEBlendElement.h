/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEBlendElement_h
#define mozilla_dom_SVGFEBlendElement_h

#include "SVGAnimatedEnumeration.h"
#include "SVGFilters.h"

nsresult NS_NewSVGFEBlendElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
namespace mozilla {
namespace dom {

typedef SVGFE SVGFEBlendElementBase;

class SVGFEBlendElement : public SVGFEBlendElementBase {
  friend nsresult(::NS_NewSVGFEBlendElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFEBlendElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFEBlendElementBase(std::move(aNodeInfo)) {}
  virtual JSObject* WrapNode(JSContext* cx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  virtual FilterPrimitiveDescription GetPrimitiveDescription(
      nsSVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
      const nsTArray<bool>& aInputsAreTainted,
      nsTArray<RefPtr<SourceSurface>>& aInputImages) override;
  virtual bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                         nsAtom* aAttribute) const override;
  virtual SVGAnimatedString& GetResultImageName() override {
    return mStringAttributes[RESULT];
  }
  virtual void GetSourceImageNames(nsTArray<SVGStringInfo>& aSources) override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedString> In1();
  already_AddRefed<DOMSVGAnimatedString> In2();
  already_AddRefed<DOMSVGAnimatedEnumeration> Mode();

 protected:
  virtual EnumAttributesInfo GetEnumInfo() override;
  virtual StringAttributesInfo GetStringInfo() override;

  enum { MODE };
  SVGAnimatedEnumeration mEnumAttributes[1];
  static SVGEnumMapping sModeMap[];
  static EnumInfo sEnumInfo[1];

  enum { RESULT, IN1, IN2 };
  SVGAnimatedString mStringAttributes[3];
  static StringInfo sStringInfo[3];
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGFEBlendElement_h
