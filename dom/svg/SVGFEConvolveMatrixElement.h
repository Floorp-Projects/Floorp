/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEConvolveMatrixElement_h
#define mozilla_dom_SVGFEConvolveMatrixElement_h

#include "nsSVGBoolean.h"
#include "nsSVGEnum.h"
#include "nsSVGFilters.h"
#include "nsSVGInteger.h"
#include "nsSVGIntegerPair.h"
#include "nsSVGNumber2.h"
#include "nsSVGString.h"
#include "SVGAnimatedNumberList.h"

nsresult NS_NewSVGFEConvolveMatrixElement(nsIContent **aResult,
                                          already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
class DOMSVGAnimatedNumberList;

namespace dom {
class SVGAnimatedBoolean;

typedef nsSVGFE SVGFEConvolveMatrixElementBase;

class SVGFEConvolveMatrixElement : public SVGFEConvolveMatrixElementBase
{
  friend nsresult (::NS_NewSVGFEConvolveMatrixElement(nsIContent **aResult,
                                                      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
protected:
  explicit SVGFEConvolveMatrixElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : SVGFEConvolveMatrixElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

public:
  virtual FilterPrimitiveDescription
    GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                            const IntRect& aFilterSubregion,
                            const nsTArray<bool>& aInputsAreTainted,
                            nsTArray<RefPtr<SourceSurface>>& aInputImages) override;
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const override;
  virtual nsSVGString& GetResultImageName() override { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources) override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  // WebIDL
  already_AddRefed<SVGAnimatedString> In1();
  already_AddRefed<SVGAnimatedInteger> OrderX();
  already_AddRefed<SVGAnimatedInteger> OrderY();
  already_AddRefed<DOMSVGAnimatedNumberList> KernelMatrix();
  already_AddRefed<SVGAnimatedInteger> TargetX();
  already_AddRefed<SVGAnimatedInteger> TargetY();
  already_AddRefed<SVGAnimatedEnumeration> EdgeMode();
  already_AddRefed<SVGAnimatedBoolean> PreserveAlpha();
  already_AddRefed<SVGAnimatedNumber> Divisor();
  already_AddRefed<SVGAnimatedNumber> Bias();
  already_AddRefed<SVGAnimatedNumber> KernelUnitLengthX();
  already_AddRefed<SVGAnimatedNumber> KernelUnitLengthY();

protected:
  virtual NumberAttributesInfo GetNumberInfo() override;
  virtual NumberPairAttributesInfo GetNumberPairInfo() override;
  virtual IntegerAttributesInfo GetIntegerInfo() override;
  virtual IntegerPairAttributesInfo GetIntegerPairInfo() override;
  virtual BooleanAttributesInfo GetBooleanInfo() override;
  virtual EnumAttributesInfo GetEnumInfo() override;
  virtual StringAttributesInfo GetStringInfo() override;
  virtual NumberListAttributesInfo GetNumberListInfo() override;

  enum { DIVISOR, BIAS };
  nsSVGNumber2 mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];

  enum { KERNEL_UNIT_LENGTH };
  nsSVGNumberPair mNumberPairAttributes[1];
  static NumberPairInfo sNumberPairInfo[1];

  enum { TARGET_X, TARGET_Y };
  nsSVGInteger mIntegerAttributes[2];
  static IntegerInfo sIntegerInfo[2];

  enum { ORDER };
  nsSVGIntegerPair mIntegerPairAttributes[1];
  static IntegerPairInfo sIntegerPairInfo[1];

  enum { PRESERVEALPHA };
  nsSVGBoolean mBooleanAttributes[1];
  static BooleanInfo sBooleanInfo[1];

  enum { EDGEMODE };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sEdgeModeMap[];
  static EnumInfo sEnumInfo[1];

  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];

  enum { KERNELMATRIX };
  SVGAnimatedNumberList mNumberListAttributes[1];
  static NumberListInfo sNumberListInfo[1];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEConvolveMatrixElement_h
