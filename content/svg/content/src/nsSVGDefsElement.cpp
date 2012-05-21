/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsSVGGraphicElement.h"
#include "nsIDOMSVGDefsElement.h"
#include "DOMSVGTests.h"

using namespace mozilla;

typedef nsSVGGraphicElement nsSVGDefsElementBase;

class nsSVGDefsElement : public nsSVGDefsElementBase,
                         public nsIDOMSVGDefsElement,
                         public DOMSVGTests
{
protected:
  friend nsresult NS_NewSVGDefsElement(nsIContent **aResult,
                                       already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGDefsElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGDEFSELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGDefsElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGDefsElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGDefsElementBase::)

  // nsIContent
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

////////////////////////////////////////////////////////////////////////
// implementation


NS_IMPL_NS_NEW_SVG_ELEMENT(Defs)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGDefsElement,nsSVGDefsElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGDefsElement,nsSVGDefsElementBase)

DOMCI_NODE_DATA(SVGDefsElement, nsSVGDefsElement)

NS_INTERFACE_TABLE_HEAD(nsSVGDefsElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGDefsElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTests,
                           nsIDOMSVGDefsElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGDefsElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGDefsElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGDefsElement::nsSVGDefsElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGDefsElementBase(aNodeInfo)
{

}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGDefsElement)


//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGDefsElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFEFloodMap,
    sFiltersMap,
    sFontSpecificationMap,
    sGradientStopMap,
    sLightingEffectsMap,
    sMarkersMap,
    sTextContentElementsMap,
    sViewportsMap
  };
  
  return FindAttributeDependence(name, map) ||
    nsSVGDefsElementBase::IsAttributeMapped(name);
}
