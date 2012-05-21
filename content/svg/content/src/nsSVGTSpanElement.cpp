/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsGkAtoms.h"
#include "nsIDOMSVGTSpanElement.h"
#include "nsSVGSVGElement.h"
#include "nsSVGTextPositioningElement.h"
#include "nsContentUtils.h"

using namespace mozilla;

typedef nsSVGTextPositioningElement nsSVGTSpanElementBase;

class nsSVGTSpanElement : public nsSVGTSpanElementBase, // = nsIDOMSVGTextPositioningElement
                          public nsIDOMSVGTSpanElement
{
protected:
  friend nsresult NS_NewSVGTSpanElement(nsIContent **aResult,
                                        already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGTSpanElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGTSPANELEMENT

  // xxx If xpcom allowed virtual inheritance we wouldn't need to
  // forward here :-(
  NS_FORWARD_NSIDOMNODE(nsSVGTSpanElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGTSpanElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGTSpanElementBase::)
  NS_FORWARD_NSIDOMSVGTEXTCONTENTELEMENT(nsSVGTSpanElementBase::)
  NS_FORWARD_NSIDOMSVGTEXTPOSITIONINGELEMENT(nsSVGTSpanElementBase::)

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:

  // nsSVGElement overrides
  virtual bool IsEventName(nsIAtom* aName);
};


NS_IMPL_NS_NEW_SVG_ELEMENT(TSpan)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGTSpanElement,nsSVGTSpanElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGTSpanElement,nsSVGTSpanElementBase)

DOMCI_NODE_DATA(SVGTSpanElement, nsSVGTSpanElement)

NS_INTERFACE_TABLE_HEAD(nsSVGTSpanElement)
  NS_NODE_INTERFACE_TABLE7(nsSVGTSpanElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTSpanElement,
                           nsIDOMSVGTextPositioningElement,
                           nsIDOMSVGTextContentElement,
                           nsIDOMSVGTests)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGTSpanElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGTSpanElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGTSpanElement::nsSVGTSpanElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGTSpanElementBase(aNodeInfo)
{

}

  
//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGTSpanElement)


//----------------------------------------------------------------------
// nsIDOMSVGTSpanElement methods

// - no methods -

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGTSpanElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFillStrokeMap,
    sFontSpecificationMap,
    sGraphicsMap,
    sTextContentElementsMap
  };
  
  return FindAttributeDependence(name, map) ||
    nsSVGTSpanElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement overrides

bool
nsSVGTSpanElement::IsEventName(nsIAtom* aName)
{
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_SVGGraphic);
}
