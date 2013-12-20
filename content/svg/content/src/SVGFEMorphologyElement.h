/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEMorphologyElement_h
#define mozilla_dom_SVGFEMorphologyElement_h

#include "nsSVGEnum.h"
#include "nsSVGFilters.h"
#include "nsSVGNumberPair.h"
#include "nsSVGString.h"

nsresult NS_NewSVGFEMorphologyElement(nsIContent **aResult,
                                      already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGFE SVGFEMorphologyElementBase;

class SVGFEMorphologyElement : public SVGFEMorphologyElementBase
{
  friend nsresult (::NS_NewSVGFEMorphologyElement(nsIContent **aResult,
                                                  already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEMorphologyElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGFEMorphologyElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

public:
  virtual FilterPrimitiveDescription
    GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                            const IntRect& aFilterSubregion,
                            nsTArray<mozilla::RefPtr<SourceSurface>>& aInputImages) MOZ_OVERRIDE;
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const MOZ_OVERRIDE;
  virtual nsSVGString& GetResultImageName() MOZ_OVERRIDE { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources) MOZ_OVERRIDE;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<SVGAnimatedString> In1();
  already_AddRefed<SVGAnimatedEnumeration> Operator();
  already_AddRefed<SVGAnimatedNumber> RadiusX();
  already_AddRefed<SVGAnimatedNumber> RadiusY();
  void SetRadius(float rx, float ry);

protected:
  void GetRXY(int32_t *aRX, int32_t *aRY, const nsSVGFilterInstance& aInstance);

  virtual NumberPairAttributesInfo GetNumberPairInfo() MOZ_OVERRIDE;
  virtual EnumAttributesInfo GetEnumInfo() MOZ_OVERRIDE;
  virtual StringAttributesInfo GetStringInfo() MOZ_OVERRIDE;

  enum { RADIUS };
  nsSVGNumberPair mNumberPairAttributes[1];
  static NumberPairInfo sNumberPairInfo[1];

  enum { OPERATOR };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sOperatorMap[];
  static EnumInfo sEnumInfo[1];

  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEMorphologyElement_h
