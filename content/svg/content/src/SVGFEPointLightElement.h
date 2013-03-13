/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEPointLightElement_h
#define mozilla_dom_SVGFEPointLightElement_h

#include "nsSVGFilters.h"
#include "nsSVGNumber2.h"

typedef SVGFEUnstyledElement SVGFEPointLightElementBase;

nsresult NS_NewSVGFEPointLightElement(nsIContent **aResult,
                                      already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

class SVGFEPointLightElement : public SVGFEPointLightElementBase,
                               public nsIDOMSVGElement
{
  friend nsresult (::NS_NewSVGFEPointLightElement(nsIContent **aResult,
                                                  already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEPointLightElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGFEPointLightElementBase(aNodeInfo)
  {
    SetIsDOMBinding();
  }
  virtual JSObject* WrapNode(JSContext *cx, JSObject *scope) MOZ_OVERRIDE;

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMSVGELEMENT(SVGFEPointLightElementBase::)
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedNumber> X();
  already_AddRefed<nsIDOMSVGAnimatedNumber> Y();
  already_AddRefed<nsIDOMSVGAnimatedNumber> Z();

protected:
  virtual NumberAttributesInfo GetNumberInfo();

  enum { ATTR_X, ATTR_Y, ATTR_Z };
  nsSVGNumber2 mNumberAttributes[3];
  static NumberInfo sNumberInfo[3];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEPointLightElement_h
