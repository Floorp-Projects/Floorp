/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGAnimationElement.h"
#include "nsIDOMSVGAnimateElement.h"
#include "nsSMILAnimationFunction.h"

typedef nsSVGAnimationElement nsSVGAnimateElementBase;

class nsSVGAnimateElement : public nsSVGAnimateElementBase,
                            public nsIDOMSVGAnimateElement
{
protected:
  friend nsresult NS_NewSVGAnimateElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGAnimateElement(already_AddRefed<nsINodeInfo> aNodeInfo);

  nsSMILAnimationFunction mAnimationFunction;

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGANIMATEELEMENT

  NS_FORWARD_NSIDOMNODE(nsSVGAnimateElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGAnimateElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGAnimateElementBase::)
  NS_FORWARD_NSIDOMSVGANIMATIONELEMENT(nsSVGAnimateElementBase::)
  
  // nsIDOMNode
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // nsISMILAnimationElement
  virtual nsSMILAnimationFunction& AnimationFunction();

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Animate)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGAnimateElement,nsSVGAnimateElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGAnimateElement,nsSVGAnimateElementBase)

DOMCI_NODE_DATA(SVGAnimateElement, nsSVGAnimateElement)

NS_INTERFACE_TABLE_HEAD(nsSVGAnimateElement)
  NS_NODE_INTERFACE_TABLE6(nsSVGAnimateElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGAnimationElement,
                           nsIDOMSVGTests, nsIDOMSVGAnimateElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimateElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGAnimateElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGAnimateElement::nsSVGAnimateElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGAnimateElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGAnimateElement)

//----------------------------------------------------------------------
// nsISMILAnimationElement methods

nsSMILAnimationFunction&
nsSVGAnimateElement::AnimationFunction()
{
  return mAnimationFunction;
}
