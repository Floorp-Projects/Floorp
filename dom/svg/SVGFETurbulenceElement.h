/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGFETURBULENCEELEMENT_H_
#define DOM_SVG_SVGFETURBULENCEELEMENT_H_

#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedInteger.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedString.h"
#include "mozilla/dom/SVGFilters.h"

nsresult NS_NewSVGFETurbulenceElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

using SVGFETurbulenceElementBase = SVGFE;

class SVGFETurbulenceElement : public SVGFETurbulenceElementBase {
  friend nsresult(::NS_NewSVGFETurbulenceElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));

 protected:
  explicit SVGFETurbulenceElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
      : SVGFETurbulenceElementBase(std::move(aNodeInfo)) {}
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  virtual bool SubregionIsUnionOfRegions() override { return false; }

  virtual FilterPrimitiveDescription GetPrimitiveDescription(
      SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
      const nsTArray<bool>& aInputsAreTainted,
      nsTArray<RefPtr<SourceSurface>>& aInputImages) override;
  virtual bool AttributeAffectsRendering(int32_t aNameSpaceID,
                                         nsAtom* aAttribute) const override;
  virtual SVGAnimatedString& GetResultImageName() override {
    return mStringAttributes[RESULT];
  }

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;

  // WebIDL
  already_AddRefed<DOMSVGAnimatedNumber> BaseFrequencyX();
  already_AddRefed<DOMSVGAnimatedNumber> BaseFrequencyY();
  already_AddRefed<DOMSVGAnimatedInteger> NumOctaves();
  already_AddRefed<DOMSVGAnimatedNumber> Seed();
  already_AddRefed<DOMSVGAnimatedEnumeration> StitchTiles();
  already_AddRefed<DOMSVGAnimatedEnumeration> Type();

 protected:
  virtual NumberAttributesInfo GetNumberInfo() override;
  virtual NumberPairAttributesInfo GetNumberPairInfo() override;
  virtual IntegerAttributesInfo GetIntegerInfo() override;
  virtual EnumAttributesInfo GetEnumInfo() override;
  virtual StringAttributesInfo GetStringInfo() override;

  enum { SEED };  // floating point seed?!
  SVGAnimatedNumber mNumberAttributes[1];
  static NumberInfo sNumberInfo[1];

  enum { BASE_FREQ };
  SVGAnimatedNumberPair mNumberPairAttributes[1];
  static NumberPairInfo sNumberPairInfo[1];

  enum { OCTAVES };
  SVGAnimatedInteger mIntegerAttributes[1];
  static IntegerInfo sIntegerInfo[1];

  enum { TYPE, STITCHTILES };
  SVGAnimatedEnumeration mEnumAttributes[2];
  static SVGEnumMapping sTypeMap[];
  static SVGEnumMapping sStitchTilesMap[];
  static EnumInfo sEnumInfo[2];

  enum { RESULT };
  SVGAnimatedString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_SVGFETURBULENCEELEMENT_H_
