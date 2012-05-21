/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGPolyElement.h"
#include "nsIDOMSVGPolygonElement.h"
#include "gfxContext.h"
#include "nsSVGUtils.h"

typedef nsSVGPolyElement nsSVGPolygonElementBase;

class nsSVGPolygonElement : public nsSVGPolygonElementBase,
                            public nsIDOMSVGPolygonElement
{
protected:
  friend nsresult NS_NewSVGPolygonElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGPolygonElement(already_AddRefed<nsINodeInfo> aNodeInfo);

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGPOLYGONELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGPolygonElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGPolygonElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGPolygonElementBase::)

  // nsSVGPathGeometryElement methods:
  virtual void GetMarkPoints(nsTArray<nsSVGMark> *aMarks);
  virtual void ConstructPath(gfxContext *aCtx);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Polygon)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGPolygonElement,nsSVGPolygonElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGPolygonElement,nsSVGPolygonElementBase)

DOMCI_NODE_DATA(SVGPolygonElement, nsSVGPolygonElement)

NS_INTERFACE_TABLE_HEAD(nsSVGPolygonElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGPolygonElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTests,
                           nsIDOMSVGPolygonElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGPolygonElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPolygonElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGPolygonElement::nsSVGPolygonElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGPolygonElementBase(aNodeInfo)
{

}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGPolygonElement)

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

void
nsSVGPolygonElement::GetMarkPoints(nsTArray<nsSVGMark> *aMarks)
{
  nsSVGPolyElement::GetMarkPoints(aMarks);
  if (aMarks->Length() > 0) {
    nsSVGMark *endMark = &aMarks->ElementAt(aMarks->Length()-1);
    nsSVGMark *startMark = &aMarks->ElementAt(0);
    float angle = atan2(startMark->y - endMark->y, startMark->x - endMark->x);

    endMark->angle = nsSVGUtils::AngleBisect(angle, endMark->angle);
    startMark->angle = nsSVGUtils::AngleBisect(angle, startMark->angle);
    // for a polygon (as opposed to a polyline) there's an implicit extra point
    // co-located with the start point that nsSVGPolyElement::GetMarkPoints
    // doesn't return
    aMarks->AppendElement(nsSVGMark(startMark->x, startMark->y, startMark->angle));
  }
}

void
nsSVGPolygonElement::ConstructPath(gfxContext *aCtx)
{
  nsSVGPolygonElementBase::ConstructPath(aCtx);
  // the difference between a polyline and a polygon is that the
  // polygon is closed:
  aCtx->ClosePath();
}


