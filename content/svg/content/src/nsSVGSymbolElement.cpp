/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsIDOMSVGSymbolElement.h"
#include "DOMSVGTests.h"
#include "nsSVGElement.h"
#include "nsSVGViewBox.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "nsIDOMSVGFitToViewBox.h"
#include "nsGkAtoms.h"

using namespace mozilla;
typedef nsSVGElement nsSVGSymbolElementBase;

class nsSVGSymbolElement : public nsSVGSymbolElementBase,
                           public nsIDOMSVGSymbolElement,
                           public DOMSVGTests,
                           public nsIDOMSVGFitToViewBox
{
protected:
  friend nsresult NS_NewSVGSymbolElement(nsIContent **aResult,
                                         already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGSymbolElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGSYMBOLELEMENT
  NS_DECL_NSIDOMSVGFITTOVIEWBOX

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGElement::)

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* name) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:
  virtual nsSVGViewBox *GetViewBox();
  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio();

  nsSVGViewBox mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Symbol)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGSymbolElement,nsSVGSymbolElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGSymbolElement,nsSVGSymbolElementBase)

DOMCI_NODE_DATA(SVGSymbolElement, nsSVGSymbolElement)

NS_INTERFACE_TABLE_HEAD(nsSVGSymbolElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGSymbolElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGFitToViewBox,
                           nsIDOMSVGSymbolElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGSymbolElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGSymbolElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGSymbolElement::nsSVGSymbolElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGSymbolElementBase(aNodeInfo)
{
}


//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGSymbolElement)

//----------------------------------------------------------------------
// nsIDOMSVGFitToViewBox methods

/* readonly attribute nsIDOMSVGAnimatedRect viewBox; */
NS_IMETHODIMP nsSVGSymbolElement::GetViewBox(nsIDOMSVGAnimatedRect * *aViewBox)
{
  return mViewBox.ToDOMAnimatedRect(aViewBox, this);
}

/* readonly attribute SVGPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
nsSVGSymbolElement::GetPreserveAspectRatio(nsISupports
                                           **aPreserveAspectRatio)
{
  return mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(aPreserveAspectRatio, this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGSymbolElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFEFloodMap,
    sFillStrokeMap,
    sFiltersMap,
    sFontSpecificationMap,
    sGradientStopMap,
    sGraphicsMap,
    sLightingEffectsMap,
    sMarkersMap,
    sTextContentElementsMap,
    sViewportsMap
   };

  return FindAttributeDependence(name, map) ||
    nsSVGSymbolElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGViewBox *
nsSVGSymbolElement::GetViewBox()
{
  return &mViewBox;
}

SVGAnimatedPreserveAspectRatio *
nsSVGSymbolElement::GetPreserveAspectRatio()
{
  return &mPreserveAspectRatio;
}
