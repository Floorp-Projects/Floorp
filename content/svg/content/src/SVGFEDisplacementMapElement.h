/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEDisplacementMapElement_h
#define mozilla_dom_SVGFEDisplacementMapElement_h

#include "nsSVGEnum.h"
#include "nsSVGFilters.h"

nsresult NS_NewSVGFEDisplacementMapElement(nsIContent **aResult,
                                           already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGFE SVGFEDisplacementMapElementBase;

class SVGFEDisplacementMapElement : public SVGFEDisplacementMapElementBase
{
protected:
  friend nsresult (::NS_NewSVGFEDisplacementMapElement(nsIContent **aResult,
                                                       already_AddRefed<nsINodeInfo> aNodeInfo));
  SVGFEDisplacementMapElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGFEDisplacementMapElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;

public:
  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources);
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);
  virtual void ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance);
  virtual nsIntRect ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
          const nsSVGFilterInstance& aInstance);


  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedString> In1();
  already_AddRefed<nsIDOMSVGAnimatedString> In2();
  already_AddRefed<nsIDOMSVGAnimatedNumber> Scale();
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> XChannelSelector();
  already_AddRefed<nsIDOMSVGAnimatedEnumeration> YChannelSelector();

protected:
  virtual bool OperatesOnSRGB(nsSVGFilterInstance* aInstance,
                              int32_t aInput, Image* aImage) {
    switch (aInput) {
    case 0:
      return aImage->mColorModel.mColorSpace == ColorModel::SRGB;
    case 1:
      return SVGFEDisplacementMapElementBase::OperatesOnSRGB(aInstance,
                                                             aInput, aImage);
    default:
      NS_ERROR("Will not give correct output color model");
      return false;
    }
  }
  virtual bool OperatesOnPremultipledAlpha(int32_t aInput) {
    return !(aInput == 1);
  }

  virtual NumberAttributesInfo GetNumberInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();

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
