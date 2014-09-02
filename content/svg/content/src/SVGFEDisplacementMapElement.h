/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEDisplacementMapElement_h
#define mozilla_dom_SVGFEDisplacementMapElement_h

#include "nsSVGEnum.h"
#include "nsSVGFilters.h"

nsresult NS_NewSVGFEDisplacementMapElement(nsIContent **aResult,
                                           already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGFE SVGFEDisplacementMapElementBase;

class SVGFEDisplacementMapElement : public SVGFEDisplacementMapElementBase
{
protected:
  friend nsresult (::NS_NewSVGFEDisplacementMapElement(nsIContent **aResult,
                                                       already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGFEDisplacementMapElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : SVGFEDisplacementMapElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext* aCx) MOZ_OVERRIDE;

public:
  virtual FilterPrimitiveDescription
    GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                            const IntRect& aFilterSubregion,
                            const nsTArray<bool>& aInputsAreTainted,
                            nsTArray<mozilla::RefPtr<SourceSurface>>& aInputImages) MOZ_OVERRIDE;
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const MOZ_OVERRIDE;
  virtual nsSVGString& GetResultImageName() MOZ_OVERRIDE { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources) MOZ_OVERRIDE;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<SVGAnimatedString> In1();
  already_AddRefed<SVGAnimatedString> In2();
  already_AddRefed<SVGAnimatedNumber> Scale();
  already_AddRefed<SVGAnimatedEnumeration> XChannelSelector();
  already_AddRefed<SVGAnimatedEnumeration> YChannelSelector();

protected:
  virtual bool OperatesOnSRGB(int32_t aInputIndex,
                              bool aInputIsAlreadySRGB) MOZ_OVERRIDE {
    switch (aInputIndex) {
    case 0:
      return aInputIsAlreadySRGB;
    case 1:
      return SVGFEDisplacementMapElementBase::OperatesOnSRGB(aInputIndex, aInputIsAlreadySRGB);
    default:
      NS_ERROR("Will not give correct color model");
      return false;
    }
  }

  virtual NumberAttributesInfo GetNumberInfo() MOZ_OVERRIDE;
  virtual EnumAttributesInfo GetEnumInfo() MOZ_OVERRIDE;
  virtual StringAttributesInfo GetStringInfo() MOZ_OVERRIDE;

  enum { SCALE };
  nsSVGNumber2 mNumberAttributes[1];
  static NumberInfo sNumberInfo[1];

  enum { CHANNEL_X, CHANNEL_Y };
  nsSVGEnum mEnumAttributes[2];
  static nsSVGEnumMapping sChannelMap[];
  static EnumInfo sEnumInfo[2];

  enum { RESULT, IN1, IN2 };
  nsSVGString mStringAttributes[3];
  static StringInfo sStringInfo[3];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEDisplacementMapElement_h
