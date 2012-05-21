/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGElement.h"
#include "nsIDOMSVGElement.h"

using namespace mozilla;

typedef nsSVGElement nsSVGUnknownElementBase;

class nsSVGUnknownElement : public nsSVGUnknownElementBase,
                            public nsIDOMSVGElement
{
protected:
  friend nsresult NS_NewSVGUnknownElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGUnknownElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  
  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGUnknownElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGUnknownElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGUnknownElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Unknown)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGUnknownElement, nsSVGUnknownElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGUnknownElement, nsSVGUnknownElementBase)

DOMCI_NODE_DATA(SVGUnknownElement, nsSVGUnknownElement)

NS_INTERFACE_TABLE_HEAD(nsSVGUnknownElement)
  NS_NODE_INTERFACE_TABLE3(nsSVGUnknownElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGUnknownElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGUnknownElementBase)
//----------------------------------------------------------------------
// Implementation

nsSVGUnknownElement::nsSVGUnknownElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGUnknownElementBase(aNodeInfo)
{
}

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGUnknownElement)
