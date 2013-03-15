/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEDistantLightElement_h
#define mozilla_dom_SVGFEDistantLightElement_h

#include "nsSVGFilters.h"

typedef SVGFEUnstyledElement nsSVGFEDistantLightElementBase;

class nsSVGFEDistantLightElement : public nsSVGFEDistantLightElementBase,
                                   public nsIDOMSVGFEDistantLightElement
{
  friend nsresult NS_NewSVGFEDistantLightElement(nsIContent **aResult,
                                                 already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEDistantLightElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEDistantLightElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGFEDISTANTLIGHTELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEDistantLightElementBase::)
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:
  virtual NumberAttributesInfo GetNumberInfo();

  enum { AZIMUTH, ELEVATION };
  nsSVGNumber2 mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];
};

#endif // mozilla_dom_SVGFEDistantLightElement_h
