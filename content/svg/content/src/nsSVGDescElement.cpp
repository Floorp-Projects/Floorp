/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGStylableElement.h"
#include "nsIDOMSVGDescElement.h"

typedef nsSVGStylableElement nsSVGDescElementBase;

class nsSVGDescElement : public nsSVGDescElementBase,
                         public nsIDOMSVGDescElement
{
protected:
  friend nsresult NS_NewSVGDescElement(nsIContent **aResult,
                                       already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGDescElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  nsresult Init();

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGDESCELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGDescElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGDescElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGDescElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Desc)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGDescElement, nsSVGDescElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGDescElement, nsSVGDescElementBase)

DOMCI_NODE_DATA(SVGDescElement, nsSVGDescElement)

NS_INTERFACE_TABLE_HEAD(nsSVGDescElement)
  NS_NODE_INTERFACE_TABLE4(nsSVGDescElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGDescElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGDescElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGDescElementBase)


//----------------------------------------------------------------------
// Implementation

nsSVGDescElement::nsSVGDescElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGDescElementBase(aNodeInfo)
{
}


nsresult
nsSVGDescElement::Init()
{
  return nsSVGDescElementBase::Init();
}


//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGDescElement)

