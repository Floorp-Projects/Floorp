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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "mozilla/Util.h"

#include "nsSVGPolyElement.h"
#include "DOMSVGPointList.h"
#include "gfxContext.h"
#include "nsSVGUtils.h"

using namespace mozilla;

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGPolyElement,nsSVGPolyElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGPolyElement,nsSVGPolyElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGPolyElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedPoints)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPolyElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGPolyElement::nsSVGPolyElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGPolyElementBase(aNodeInfo)
{

}

//----------------------------------------------------------------------
// nsIDOMSGAnimatedPoints methods:

/* readonly attribute nsIDOMSVGPointList points; */
NS_IMETHODIMP 
nsSVGPolyElement::GetPoints(nsIDOMSVGPointList * *aPoints)
{
  void *key = mPoints.GetBaseValKey();
  *aPoints = DOMSVGPointList::GetDOMWrapper(key, this, PR_FALSE).get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGPointList animatedPoints; */
NS_IMETHODIMP 
nsSVGPolyElement::GetAnimatedPoints(nsIDOMSVGPointList * *aAnimatedPoints)
{
  void *key = mPoints.GetAnimValKey();
  *aAnimatedPoints = DOMSVGPointList::GetDOMWrapper(key, this, PR_TRUE).get();
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGPolyElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sMarkersMap
  };
  
  return FindAttributeDependence(name, map, ArrayLength(map)) ||
    nsSVGPolyElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

bool
nsSVGPolyElement::AttributeDefinesGeometry(const nsIAtom *aName)
{
  if (aName == nsGkAtoms::points)
    return PR_TRUE;

  return PR_FALSE;
}

void
nsSVGPolyElement::GetMarkPoints(nsTArray<nsSVGMark> *aMarks)
{
  const SVGPointList &points = mPoints.GetAnimValue();

  if (!points.Length())
    return;

  float px = 0.0, py = 0.0, prevAngle = 0.0;

  for (PRUint32 i = 0; i < points.Length(); ++i) {
    float x = points[i].mX;
    float y = points[i].mY;
    float angle = atan2(y-py, x-px);
    if (i == 1)
      aMarks->ElementAt(aMarks->Length() - 1).angle = angle;
    else if (i > 1)
      aMarks->ElementAt(aMarks->Length() - 1).angle =
        nsSVGUtils::AngleBisect(prevAngle, angle);

    aMarks->AppendElement(nsSVGMark(x, y, 0));

    prevAngle = angle;
    px = x;
    py = y;
  }

  aMarks->ElementAt(aMarks->Length() - 1).angle = prevAngle;
}

void
nsSVGPolyElement::ConstructPath(gfxContext *aCtx)
{
  const SVGPointList &points = mPoints.GetAnimValue();

  if (!points.Length())
    return;

  aCtx->MoveTo(points[0]);
  for (PRUint32 i = 1; i < points.Length(); ++i) {
    aCtx->LineTo(points[i]);
  }
}

