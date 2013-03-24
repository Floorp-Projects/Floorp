/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEDiffuseLightingElement_h
#define mozilla_dom_SVGFEDiffuseLightingElement_h

#include "nsSVGFilters.h"

nsresult NS_NewSVGFEDiffuseLightingElement(nsIContent **aResult,
                                           already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef nsSVGFELightingElement SVGFEDiffuseLightingElementBase;

class SVGFEDiffuseLightingElement : public SVGFEDiffuseLightingElementBase,
                                    public nsIDOMSVGElement
{
  friend nsresult (::NS_NewSVGFEDiffuseLightingElement(nsIContent **aResult,
                                                       already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEDiffuseLightingElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGFEDiffuseLightingElementBase(aNodeInfo)
  {
    SetIsDOMBinding();
  }
  virtual JSObject* WrapNode(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMSVGELEMENT(SVGFEDiffuseLightingElementBase::)
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedString> In1();
  already_AddRefed<nsIDOMSVGAnimatedNumber> SurfaceScale();
  already_AddRefed<nsIDOMSVGAnimatedNumber> DiffuseConstant();
  already_AddRefed<nsIDOMSVGAnimatedNumber> KernelUnitLengthX();
  already_AddRefed<nsIDOMSVGAnimatedNumber> KernelUnitLengthY();

protected:
  virtual void LightPixel(const float *N, const float *L,
                          nscolor color, uint8_t *targetData);

};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEDiffuseLightingElement_h
