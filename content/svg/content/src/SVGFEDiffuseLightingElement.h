/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEDiffuseLightingElement_h
#define mozilla_dom_SVGFEDiffuseLightingElement_h

#include "nsSVGFilters.h"

namespace mozilla {
namespace dom {

typedef nsSVGFELightingElement nsSVGFEDiffuseLightingElementBase;

class nsSVGFEDiffuseLightingElement : public nsSVGFEDiffuseLightingElementBase,
                                      public nsIDOMSVGFEDiffuseLightingElement
{
  friend nsresult NS_NewSVGFEDiffuseLightingElement(nsIContent **aResult,
                                                    already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEDiffuseLightingElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEDiffuseLightingElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // DiffuseLighting
  NS_DECL_NSIDOMSVGFEDIFFUSELIGHTINGELEMENT

  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEDiffuseLightingElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEDiffuseLightingElementBase::)
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:
  virtual void LightPixel(const float *N, const float *L,
                          nscolor color, uint8_t *targetData);

};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEDiffuseLightingElement_h
