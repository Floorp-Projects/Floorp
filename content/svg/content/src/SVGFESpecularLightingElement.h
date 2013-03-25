/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFESpecularLightingElement_h
#define mozilla_dom_SVGFESpecularLightingElement_h

#include "nsSVGFilters.h"

nsresult NS_NewSVGFESpecularLightingElement(nsIContent **aResult,
                                            already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

//---------------------SpecularLighting------------------------

typedef nsSVGFELightingElement SVGFESpecularLightingElementBase;

class SVGFESpecularLightingElement : public SVGFESpecularLightingElementBase,
                                     public nsIDOMSVGElement
{
  friend nsresult (::NS_NewSVGFESpecularLightingElement(nsIContent **aResult,
                                                        already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFESpecularLightingElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGFESpecularLightingElementBase(aNodeInfo)
  {
    SetIsDOMBinding();
  }
  virtual JSObject* WrapNode(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMSVGELEMENT(SVGFESpecularLightingElementBase::)
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedString> In1();
  already_AddRefed<nsIDOMSVGAnimatedNumber> SurfaceScale();
  already_AddRefed<nsIDOMSVGAnimatedNumber> SpecularConstant();
  already_AddRefed<nsIDOMSVGAnimatedNumber> SpecularExponent();
  already_AddRefed<nsIDOMSVGAnimatedNumber> KernelUnitLengthX();
  already_AddRefed<nsIDOMSVGAnimatedNumber> KernelUnitLengthY();

protected:
  virtual void LightPixel(const float *N, const float *L,
                          nscolor color, uint8_t *targetData);

};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFESpecularLightingElement_h
