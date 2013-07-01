/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEOffsetElement_h
#define mozilla_dom_SVGFEOffsetElement_h

#include "nsSVGFilters.h"
#include "nsSVGNumber2.h"
#include "nsSVGString.h"

nsresult NS_NewSVGFEOffsetElement(nsIContent **aResult,
                                  already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGFE SVGFEOffsetElementBase;

class SVGFEOffsetElement : public SVGFEOffsetElementBase
{
  friend nsresult (::NS_NewSVGFEOffsetElement(nsIContent **aResult,
                                              already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEOffsetElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGFEOffsetElementBase(aNodeInfo)
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
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources) MOZ_OVERRIDE;
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance) MOZ_OVERRIDE;
  virtual void ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance) MOZ_OVERRIDE;
  virtual nsIntRect ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
          const nsSVGFilterInstance& aInstance) MOZ_OVERRIDE;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  // WebIDL
  already_AddRefed<SVGAnimatedString> In1();
  already_AddRefed<SVGAnimatedNumber> Dx();
  already_AddRefed<SVGAnimatedNumber> Dy();

protected:
  nsIntPoint GetOffset(const nsSVGFilterInstance& aInstance);

  virtual NumberAttributesInfo GetNumberInfo() MOZ_OVERRIDE;
  virtual StringAttributesInfo GetStringInfo() MOZ_OVERRIDE;

  enum { DX, DY };
  nsSVGNumber2 mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];

  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEOffsetElement_h
