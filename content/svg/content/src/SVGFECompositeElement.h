/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFECompositeElement_h
#define mozilla_dom_SVGFECompositeElement_h

#include "nsSVGEnum.h"
#include "nsSVGFilters.h"
#include "nsSVGNumber2.h"

nsresult NS_NewSVGFECompositeElement(nsIContent **aResult,
                                     already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGFE SVGFECompositeElementBase;

class SVGFECompositeElement : public SVGFECompositeElementBase
{
  friend nsresult (::NS_NewSVGFECompositeElement(nsIContent **aResult,
                                                 already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFECompositeElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGFECompositeElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

public:
  virtual FilterPrimitiveDescription
    GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                            const IntRect& aFilterSubregion,
                            nsTArray<nsRefPtr<gfxASurface> >& aInputImages) MOZ_OVERRIDE;
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const MOZ_OVERRIDE;
  virtual nsSVGString& GetResultImageName() MOZ_OVERRIDE { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources) MOZ_OVERRIDE;


  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<SVGAnimatedString> In1();
  already_AddRefed<SVGAnimatedString> In2();
  already_AddRefed<SVGAnimatedEnumeration> Operator();
  already_AddRefed<SVGAnimatedNumber> K1();
  already_AddRefed<SVGAnimatedNumber> K2();
  already_AddRefed<SVGAnimatedNumber> K3();
  already_AddRefed<SVGAnimatedNumber> K4();
  void SetK(float k1, float k2, float k3, float k4);

protected:
  virtual NumberAttributesInfo GetNumberInfo() MOZ_OVERRIDE;
  virtual EnumAttributesInfo GetEnumInfo() MOZ_OVERRIDE;
  virtual StringAttributesInfo GetStringInfo() MOZ_OVERRIDE;

  enum { ATTR_K1, ATTR_K2, ATTR_K3, ATTR_K4 };
  nsSVGNumber2 mNumberAttributes[4];
  static NumberInfo sNumberInfo[4];

  enum { OPERATOR };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sOperatorMap[];
  static EnumInfo sEnumInfo[1];

  enum { RESULT, IN1, IN2 };
  nsSVGString mStringAttributes[3];
  static StringInfo sStringInfo[3];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFECompositeElement_h
