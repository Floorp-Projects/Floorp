/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGElement.h"
#include "nsIDOMSVGMetadataElement.h"

typedef nsSVGElement nsSVGMetadataElementBase;

class nsSVGMetadataElement : public nsSVGMetadataElementBase,
                             public nsIDOMSVGMetadataElement
{
protected:
  friend nsresult NS_NewSVGMetadataElement(nsIContent **aResult,
                                           already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGMetadataElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  nsresult Init();

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGMETADATAELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGElement::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGElement::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Metadata)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGMetadataElement, nsSVGMetadataElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGMetadataElement, nsSVGMetadataElementBase)

DOMCI_NODE_DATA(SVGMetadataElement, nsSVGMetadataElement)

NS_INTERFACE_TABLE_HEAD(nsSVGMetadataElement)
  NS_NODE_INTERFACE_TABLE4(nsSVGMetadataElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGMetadataElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGMetadataElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGMetadataElementBase)


//----------------------------------------------------------------------
// Implementation

nsSVGMetadataElement::nsSVGMetadataElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGMetadataElementBase(aNodeInfo)
{
}


nsresult
nsSVGMetadataElement::Init()
{
  return NS_OK;
}


//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGMetadataElement)
