/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEColorMatrixElement_h
#define mozilla_dom_SVGFEColorMatrixElement_h

#include "nsSVGEnum.h"
#include "nsSVGFilters.h"
#include "SVGAnimatedNumberList.h"

nsresult NS_NewSVGFEColorMatrixElement(nsIContent **aResult,
                                       already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGFE SVGFEColorMatrixElementBase;

static const unsigned short SVG_FECOLORMATRIX_TYPE_UNKNOWN = 0;
static const unsigned short SVG_FECOLORMATRIX_TYPE_MATRIX = 1;
static const unsigned short SVG_FECOLORMATRIX_TYPE_SATURATE = 2;
static const unsigned short SVG_FECOLORMATRIX_TYPE_HUE_ROTATE = 3;
static const unsigned short SVG_FECOLORMATRIX_TYPE_LUMINANCE_TO_ALPHA = 4;

class SVGFEColorMatrixElement : public SVGFEColorMatrixElementBase
{
  friend nsresult (::NS_NewSVGFEColorMatrixElement(nsIContent **aResult,
                                                   already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEColorMatrixElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGFEColorMatrixElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

public:
  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect) MOZ_OVERRIDE;
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const MOZ_OVERRIDE;
  virtual nsSVGString& GetResultImageName() MOZ_OVERRIDE { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources) MOZ_OVERRIDE;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<SVGAnimatedString> In1();
  already_AddRefed<SVGAnimatedEnumeration> Type();
  already_AddRefed<DOMSVGAnimatedNumberList> Values();

 protected:
  virtual bool OperatesOnPremultipledAlpha(int32_t) MOZ_OVERRIDE { return false; }

  virtual EnumAttributesInfo GetEnumInfo() MOZ_OVERRIDE;
  virtual StringAttributesInfo GetStringInfo() MOZ_OVERRIDE;
  virtual NumberListAttributesInfo GetNumberListInfo() MOZ_OVERRIDE;

  enum { TYPE };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sTypeMap[];
  static EnumInfo sEnumInfo[1];

  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];

  enum { VALUES };
  SVGAnimatedNumberList mNumberListAttributes[1];
  static NumberListInfo sNumberListInfo[1];
};

} // namespace mozilla
} // namespace dom

#endif // mozilla_dom_SVGFEColorMatrixElement_h
