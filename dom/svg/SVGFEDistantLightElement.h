/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEDistantLightElement_h
#define mozilla_dom_SVGFEDistantLightElement_h

#include "SVGAnimatedNumber.h"
#include "SVGFilters.h"

nsresult NS_NewSVGFEDistantLightElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGFELightElement SVGFEDistantLightElementBase;

class SVGFEDistantLightElement : public SVGFEDistantLightElementBase {
  friend nsresult(::NS_NewSVGFEDistantLightElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFEDistantLightElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFEDistantLightElementBase(std::move(aNodeInfo)) {}
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
  already_AddRefed<DOMSVGAnimatedNumber> Azimuth();
  already_AddRefed<DOMSVGAnimatedNumber> Elevation();

 protected:
  virtual NumberAttributesInfo GetNumberInfo() override;

  enum { AZIMUTH, ELEVATION };
  SVGAnimatedNumber mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGFEDistantLightElement_h
