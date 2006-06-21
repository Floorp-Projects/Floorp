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

#include "nsSVGPathGeometryElement.h"
#include "nsSVGAtoms.h"
#include "nsSVGPointList.h"
#include "nsIDOMSVGPolygonElement.h"
#include "nsIDOMSVGAnimatedPoints.h"
#include "nsCOMPtr.h"
#include "nsIDOMSVGPoint.h"
#include "nsSVGUtils.h"

typedef nsSVGPathGeometryElement nsSVGPolygonElementBase;

class nsSVGPolygonElement : public nsSVGPolygonElementBase,
                            public nsIDOMSVGPolygonElement,
                            public nsIDOMSVGAnimatedPoints
{
protected:
  friend nsresult NS_NewSVGPolygonElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
  nsSVGPolygonElement(nsINodeInfo* aNodeInfo);
  nsresult Init();
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGPOLYGONELEMENT
  NS_DECL_NSIDOMSVGANIMATEDPOINTS

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGPolygonElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGPolygonElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGPolygonElementBase::)

  // nsIContent interface
  NS_IMETHODIMP_(PRBool) IsAttributeMapped(const nsIAtom* name) const;

  // nsSVGPathGeometryElement methods:
  virtual PRBool IsDependentAttribute(nsIAtom *aName);
  virtual PRBool IsMarkable() { return PR_TRUE; }
  virtual void GetMarkPoints(nsTArray<nsSVGMark> *aMarks);
  virtual void ConstructPath(cairo_t *aCtx);

protected:
  nsCOMPtr<nsIDOMSVGPointList> mPoints;
};


NS_IMPL_NS_NEW_SVG_ELEMENT(Polygon)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGPolygonElement,nsSVGPolygonElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGPolygonElement,nsSVGPolygonElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGPolygonElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPolygonElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedPoints)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGPolygonElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPolygonElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGPolygonElement::nsSVGPolygonElement(nsINodeInfo* aNodeInfo)
  : nsSVGPolygonElementBase(aNodeInfo)
{

}


nsresult
nsSVGPolygonElement::Init()
{
  nsresult rv = nsSVGPolygonElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:
  
  // points #IMPLIED
  rv = nsSVGPointList::Create(getter_AddRefs(mPoints));
  NS_ENSURE_SUCCESS(rv,rv);
  rv = AddMappedSVGValue(nsSVGAtoms::points, mPoints);
  NS_ENSURE_SUCCESS(rv,rv);

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGPolygonElement)


//----------------------------------------------------------------------
// nsIDOMSGAnimatedPoints methods:

/* readonly attribute nsIDOMSVGPointList points; */
NS_IMETHODIMP nsSVGPolygonElement::GetPoints(nsIDOMSVGPointList * *aPoints)
{
  *aPoints = mPoints;
  NS_ADDREF(*aPoints);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGPointList animatedPoints; */
NS_IMETHODIMP nsSVGPolygonElement::GetAnimatedPoints(nsIDOMSVGPointList * *aAnimatedPoints)
{
  *aAnimatedPoints = mPoints;
  NS_ADDREF(*aAnimatedPoints);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(PRBool)
nsSVGPolygonElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sMarkersMap,
  };
  
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGPolygonElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

PRBool
nsSVGPolygonElement::IsDependentAttribute(nsIAtom *aName)
{
  if (aName == nsGkAtoms::points)
    return PR_TRUE;

  return PR_FALSE;
}

void
nsSVGPolygonElement::GetMarkPoints(nsTArray<nsSVGMark> *aMarks)
{
  if (!mPoints)
    return;

  PRUint32 count;
  mPoints->GetNumberOfItems(&count);
  if (count == 0)
    return;

  float px = 0.0, py = 0.0, prevAngle, startAngle;

  nsCOMPtr<nsIDOMSVGPoint> point;
  for (PRUint32 i = 0; i < count; ++i) {
    mPoints->GetItem(i, getter_AddRefs(point));

    float x, y;
    point->GetX(&x);
    point->GetY(&y);

    float angle = atan2(y-py, x-px);
    if (i == 1)
      startAngle = angle;
    else if (i > 1)
      aMarks->ElementAt(aMarks->Length() - 1).angle =
        nsSVGUtils::AngleBisect(prevAngle, angle);

    aMarks->AppendElement(nsSVGMark(x, y, 0));

    prevAngle = angle;
    px = x;
    py = y;
  }

  float nx, ny, angle;
  mPoints->GetItem(0, getter_AddRefs(point));
  point->GetX(&nx);
  point->GetY(&ny);
  angle = atan2(ny - py, nx - px);

  aMarks->ElementAt(aMarks->Length() - 1).angle =
    nsSVGUtils::AngleBisect(prevAngle, angle);
  aMarks->ElementAt(0).angle =
    nsSVGUtils::AngleBisect(angle, startAngle);
}

void
nsSVGPolygonElement::ConstructPath(cairo_t *aCtx)
{
  if (!mPoints)
    return;

  PRUint32 count;
  mPoints->GetNumberOfItems(&count);
  if (count == 0)
    return;

  PRUint32 i;
  for (i = 0; i < count; ++i) {
    nsCOMPtr<nsIDOMSVGPoint> point;
    mPoints->GetItem(i, getter_AddRefs(point));

    float x, y;
    point->GetX(&x);
    point->GetY(&y);
    if (i == 0)
      cairo_move_to(aCtx, x, y);
    else
      cairo_line_to(aCtx, x, y);
  }
  // the difference between a polyline and a polygon is that the
  // polygon is closed:
  cairo_close_path(aCtx);
}
