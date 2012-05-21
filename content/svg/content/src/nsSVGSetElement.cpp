/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGAnimationElement.h"
#include "nsIDOMSVGSetElement.h"
#include "nsSMILSetAnimationFunction.h"

typedef nsSVGAnimationElement nsSVGSetElementBase;

class nsSVGSetElement : public nsSVGSetElementBase,
                        public nsIDOMSVGSetElement
{
protected:
  friend nsresult NS_NewSVGSetElement(nsIContent **aResult,
                                      already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGSetElement(already_AddRefed<nsINodeInfo> aNodeInfo);

  nsSMILSetAnimationFunction mAnimationFunction;

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGSETELEMENT

  NS_FORWARD_NSIDOMNODE(nsSVGSetElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGSetElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGSetElementBase::)
  NS_FORWARD_NSIDOMSVGANIMATIONELEMENT(nsSVGSetElementBase::)
  
  // nsIDOMNode
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // nsISMILAnimationElement
  virtual nsSMILAnimationFunction& AnimationFunction();

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Set)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGSetElement,nsSVGSetElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGSetElement,nsSVGSetElementBase)

DOMCI_NODE_DATA(SVGSetElement, nsSVGSetElement)

NS_INTERFACE_TABLE_HEAD(nsSVGSetElement)
  NS_NODE_INTERFACE_TABLE6(nsSVGSetElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGAnimationElement,
                           nsIDOMSVGTests, nsIDOMSVGSetElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGSetElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGSetElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGSetElement::nsSVGSetElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGSetElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGSetElement)

//----------------------------------------------------------------------
// nsISMILAnimationElement methods

nsSMILAnimationFunction&
nsSVGSetElement::AnimationFunction()
{
  return mAnimationFunction;
}
