/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGFEMergeNodeElement_h
#define mozilla_dom_SVGFEMergeNodeElement_h

#include "nsSVGFilters.h"

nsresult NS_NewSVGFEMergeNodeElement(nsIContent** aResult,
                                     already_AddRefed<nsINodeInfo> aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGFEUnstyledElement SVGFEMergeNodeElementBase;

class SVGFEMergeNodeElement : public SVGFEMergeNodeElementBase,
                              public nsIDOMSVGElement
{
  friend nsresult (::NS_NewSVGFEMergeNodeElement(nsIContent **aResult,
                                                 already_AddRefed<nsINodeInfo> aNodeInfo));
protected:
  SVGFEMergeNodeElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : SVGFEMergeNodeElementBase(aNodeInfo)
  {
    SetIsDOMBinding();
  }
  virtual JSObject* WrapNode(JSContext* aCx, JSObject* aScope) MOZ_OVERRIDE;

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIDOMSVGELEMENT(SVGFEMergeNodeElementBase::)

  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual bool AttributeAffectsRendering(
          int32_t aNameSpaceID, nsIAtom* aAttribute) const;

  const nsSVGString* GetIn1() { return &mStringAttributes[IN1]; }

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // WebIDL
  already_AddRefed<nsIDOMSVGAnimatedString> In1();

protected:
  virtual StringAttributesInfo GetStringInfo();

  enum { IN1 };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_SVGFEMergeNodeElement_h
