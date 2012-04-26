/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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


