/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEComponentTransferElement_h
#define mozilla_dom_SVGFEComponentTransferElement_h

#include "nsSVGFilters.h"

typedef nsSVGFE SVGFEComponentTransferElementBase;

nsresult NS_NewSVGFEComponentTransferElement(nsIContent **aResult,
                                             already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

class SVGFEComponentTransferElement : public SVGFEComponentTransferElementBase,
                                      public nsIDOMSVGElement
{
  friend nsresult (::NS_NewSVGFEComponentTransferElement(nsIContent **aResult,
                                                         already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEComponentTransferElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGFEComponentTransferElementBase(aNodeInfo)
  {
    SetIsDOMBinding();
  }
  virtual JSObject* WrapNode(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources);

  NS_FORWARD_NSIDOMSVGELEMENT(SVGFEComponentTransferElementBase::)

  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIContent
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedString> In1();

protected:
  virtual bool OperatesOnPremultipledAlpha(int32_t) { return false; }

  virtual StringAttributesInfo GetStringInfo();

  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

} // namespace mozilla
} // namespace dom

#endif // mozilla_dom_SVGFEComponentTransferElement_h
