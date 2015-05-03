/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEDiffuseLightingElement_h
#define mozilla_dom_SVGFEDiffuseLightingElement_h

#include "nsSVGFilters.h"

nsresult NS_NewSVGFEDiffuseLightingElement(nsIContent **aResult,
                                           already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGFELightingElement SVGFEDiffuseLightingElementBase;

class SVGFEDiffuseLightingElement : public SVGFEDiffuseLightingElementBase
{
  friend nsresult (::NS_NewSVGFEDiffuseLightingElement(nsIContent **aResult,
                                                       already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
protected:
  explicit SVGFEDiffuseLightingElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : SVGFEDiffuseLightingElementBase(aNodeInfo)
  {
  }
  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

public:
  virtual FilterPrimitiveDescription
    GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                            const IntRect& aFilterSubregion,
                            const nsTArray<bool>& aInputsAreTainted,
                            nsTArray<mozilla::RefPtr<SourceSurface>>& aInputImages) override;
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const override;

  // WebIDL
  already_AddRefed<SVGAnimatedString> In1();
  already_AddRefed<SVGAnimatedNumber> SurfaceScale();
  already_AddRefed<SVGAnimatedNumber> DiffuseConstant();
  already_AddRefed<SVGAnimatedNumber> KernelUnitLengthX();
  already_AddRefed<SVGAnimatedNumber> KernelUnitLengthY();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEDiffuseLightingElement_h
