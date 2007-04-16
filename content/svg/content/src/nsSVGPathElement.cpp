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

#include "nsGkAtoms.h"
#include "nsSVGPathSegList.h"
#include "nsIDOMSVGPathSeg.h"
#include "nsSVGPathSeg.h"
#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsSVGPathDataParser.h"
#include "nsSVGPathElement.h"
#include "nsISVGValueUtils.h"
#include "nsSVGUtils.h"
#include "nsSVGPoint.h"
#include "gfxContext.h"

nsSVGElement::NumberInfo nsSVGPathElement::sNumberInfo = 
                                                  { &nsGkAtoms::pathLength, 0 };

NS_IMPL_NS_NEW_SVG_ELEMENT(Path)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGPathElement,nsSVGPathElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGPathElement,nsSVGPathElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGPathElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPathElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedPathData)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGPathElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPathElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGPathElement::nsSVGPathElement(nsINodeInfo* aNodeInfo)
  : nsSVGPathElementBase(aNodeInfo)
{
}

nsSVGPathElement::~nsSVGPathElement()
{
  if (mSegments)
    NS_REMOVE_SVGVALUE_OBSERVER(mSegments);
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGPathElement)

//----------------------------------------------------------------------
// nsIDOMSVGPathElement methods:

/* readonly attribute nsIDOMSVGAnimatedNumber pathLength; */
NS_IMETHODIMP
nsSVGPathElement::GetPathLength(nsIDOMSVGAnimatedNumber * *aPathLength)
{
  return mPathLength.ToDOMAnimatedNumber(aPathLength, this);
}

/* float getTotalLength (); */
NS_IMETHODIMP
nsSVGPathElement::GetTotalLength(float *_retval)
{
  *_retval = 0;

  nsRefPtr<gfxFlattenedPath> flat = GetFlattenedPath(nsnull);

  if (!flat)
    return NS_ERROR_FAILURE;

  *_retval = flat->GetLength();

  return NS_OK;
}

/* nsIDOMSVGPoint getPointAtLength (in float distance); */
NS_IMETHODIMP
nsSVGPathElement::GetPointAtLength(float distance, nsIDOMSVGPoint **_retval)
{
  nsRefPtr<gfxFlattenedPath> flat = GetFlattenedPath(nsnull);
  if (!flat)
    return NS_ERROR_FAILURE;

  float totalLength = flat->GetLength();
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::pathLength)) {
    float pathLength = mPathLength.GetAnimValue();
    distance *= totalLength / pathLength;
  }
  distance = PR_MAX(0,           distance);
  distance = PR_MIN(totalLength, distance);

  gfxPoint pt = flat->FindPoint(gfxPoint(distance, 0));

  return NS_NewSVGPoint(_retval, pt.x, pt.y);
}

/* unsigned long getPathSegAtLength (in float distance); */
NS_IMETHODIMP
nsSVGPathElement::GetPathSegAtLength(float distance, PRUint32 *_retval)
{
  //Check if mSegments is null
  nsresult rv = CreatePathSegList();
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 i = 0, numSegments;
  float distCovered = 0;
  nsSVGPathSegTraversalState ts;

  mSegments->GetNumberOfItems(&numSegments);

  //  There is no need to check to see if distance falls within the last segment
  //  because if distance is longer than the total length of the path we return 
  //  the index of the final segment anyway.
  while (distCovered < distance && i < numSegments - 1) {
    nsIDOMSVGPathSeg *iSeg;
    mSegments->GetItem(i, &iSeg);
    nsSVGPathSeg* curSeg = NS_STATIC_CAST(nsSVGPathSeg*, iSeg);
    if (i == 0) {
      curSeg->GetLength(&ts);
    } else {
      distCovered += curSeg->GetLength(&ts);
    }

    if (distCovered >= distance) {
      break;
    }
    ++i;
  }

  *_retval = i;

  return NS_OK;
}

/* nsIDOMSVGPathSegClosePath createSVGPathSegClosePath (); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegClosePath(nsIDOMSVGPathSegClosePath **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegClosePath();
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegMovetoAbs createSVGPathSegMovetoAbs (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegMovetoAbs(float x, float y, nsIDOMSVGPathSegMovetoAbs **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegMovetoAbs(x, y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegMovetoRel createSVGPathSegMovetoRel (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegMovetoRel(float x, float y, nsIDOMSVGPathSegMovetoRel **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegMovetoRel(x, y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegLinetoAbs createSVGPathSegLinetoAbs (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoAbs(float x, float y, nsIDOMSVGPathSegLinetoAbs **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegLinetoAbs(x, y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegLinetoRel createSVGPathSegLinetoRel (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoRel(float x, float y, nsIDOMSVGPathSegLinetoRel **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegLinetoRel(x, y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoCubicAbs createSVGPathSegCurvetoCubicAbs (in float x, in float y, in float x1, in float y1, in float x2, in float y2); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2, nsIDOMSVGPathSegCurvetoCubicAbs **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoCubicAbs(x, y, x1, y1, x2, y2);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoCubicRel createSVGPathSegCurvetoCubicRel (in float x, in float y, in float x1, in float y1, in float x2, in float y2); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2, nsIDOMSVGPathSegCurvetoCubicRel **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoCubicRel(x, y, x1, y1, x2, y2);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoQuadraticAbs createSVGPathSegCurvetoQuadraticAbs (in float x, in float y, in float x1, in float y1); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1, nsIDOMSVGPathSegCurvetoQuadraticAbs **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoQuadraticAbs(x, y, x1, y1);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoQuadraticRel createSVGPathSegCurvetoQuadraticRel (in float x, in float y, in float x1, in float y1); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1, nsIDOMSVGPathSegCurvetoQuadraticRel **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoQuadraticRel(x, y, x1, y1);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegArcAbs createSVGPathSegArcAbs (in float x, in float y, in float r1, in float r2, in float angle, in boolean largeArcFlag, in boolean sweepFlag); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, PRBool largeArcFlag, PRBool sweepFlag, nsIDOMSVGPathSegArcAbs **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegArcAbs(x, y, r1, r2, angle,
                                                 largeArcFlag, sweepFlag);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegArcRel createSVGPathSegArcRel (in float x, in float y, in float r1, in float r2, in float angle, in boolean largeArcFlag, in boolean sweepFlag); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, PRBool largeArcFlag, PRBool sweepFlag, nsIDOMSVGPathSegArcRel **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegArcRel(x, y, r1, r2, angle,
                                                 largeArcFlag, sweepFlag);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegLinetoHorizontalAbs createSVGPathSegLinetoHorizontalAbs (in float x); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoHorizontalAbs(float x, nsIDOMSVGPathSegLinetoHorizontalAbs **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegLinetoHorizontalAbs(x);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegLinetoHorizontalRel createSVGPathSegLinetoHorizontalRel (in float x); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoHorizontalRel(float x, nsIDOMSVGPathSegLinetoHorizontalRel **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegLinetoHorizontalRel(x);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegLinetoVerticalAbs createSVGPathSegLinetoVerticalAbs (in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoVerticalAbs(float y, nsIDOMSVGPathSegLinetoVerticalAbs **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegLinetoVerticalAbs(y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegLinetoVerticalRel createSVGPathSegLinetoVerticalRel (in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoVerticalRel(float y, nsIDOMSVGPathSegLinetoVerticalRel **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegLinetoVerticalRel(y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoCubicSmoothAbs createSVGPathSegCurvetoCubicSmoothAbs (in float x, in float y, in float x2, in float y2); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2, nsIDOMSVGPathSegCurvetoCubicSmoothAbs **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoCubicSmoothAbs(x, y, x2, y2);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoCubicSmoothRel createSVGPathSegCurvetoCubicSmoothRel (in float x, in float y, in float x2, in float y2); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2, nsIDOMSVGPathSegCurvetoCubicSmoothRel **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoCubicSmoothRel(x, y, x2, y2);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs createSVGPathSegCurvetoQuadraticSmoothAbs (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y, nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoQuadraticSmoothAbs(x, y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoQuadraticSmoothRel createSVGPathSegCurvetoQuadraticSmoothRel (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoQuadraticSmoothRel(float x, float y, nsIDOMSVGPathSegCurvetoQuadraticSmoothRel **_retval)
{
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoQuadraticSmoothRel(x, y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

nsresult
nsSVGPathElement::CreatePathSegList()
{
  if (mSegments)
    return NS_OK;

  nsresult rv = NS_NewSVGPathSegList(getter_AddRefs(mSegments));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISVGValue> value = do_QueryInterface(mSegments);

  nsAutoString d;
  if (NS_SUCCEEDED(GetAttr(kNameSpaceID_None, nsGkAtoms::d, d)))
    value->SetValueString(d);

  NS_ADD_SVGVALUE_OBSERVER(mSegments);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGPathElement::GetNumberInfo()
{
  return NumberAttributesInfo(&mPathLength, &sNumberInfo, 1);
}

//----------------------------------------------------------------------
// nsIDOMSVGAnimatedPathData methods:

/* readonly attribute nsIDOMSVGPathSegList pathSegList; */
NS_IMETHODIMP nsSVGPathElement::GetPathSegList(nsIDOMSVGPathSegList * *aPathSegList)
{
  nsresult rv = CreatePathSegList();
  NS_ENSURE_SUCCESS(rv, rv);

  *aPathSegList = mSegments;
  NS_ADDREF(*aPathSegList);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGPathSegList normalizedPathSegList; */
NS_IMETHODIMP nsSVGPathElement::GetNormalizedPathSegList(nsIDOMSVGPathSegList * *aNormalizedPathSegList)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMSVGPathSegList animatedPathSegList; */
NS_IMETHODIMP nsSVGPathElement::GetAnimatedPathSegList(nsIDOMSVGPathSegList * *aAnimatedPathSegList)
{
  nsresult rv = CreatePathSegList();
  NS_ENSURE_SUCCESS(rv, rv);

  *aAnimatedPathSegList = mSegments;
  NS_ADDREF(*aAnimatedPathSegList);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGPathSegList animatedNormalizedPathSegList; */
NS_IMETHODIMP nsSVGPathElement::GetAnimatedNormalizedPathSegList(nsIDOMSVGPathSegList * *aAnimatedNormalizedPathSegList)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(PRBool)
nsSVGPathElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sMarkersMap
  };

  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGPathElementBase::IsAttributeMapped(name);
}

nsresult
nsSVGPathElement::BeforeSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                               const nsAString* aValue, PRBool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None && aName == nsGkAtoms::d) {
    if (mSegments) {
      NS_REMOVE_SVGVALUE_OBSERVER(mSegments);
      mSegments = nsnull;
    }

    nsSVGPathDataParserToInternal parser(&mPathData);
    parser.Parse(*aValue);
  }

  return nsSVGPathElementBase::BeforeSetAttr(aNamespaceID, aName,
                                             aValue, aNotify);
}

NS_IMETHODIMP
nsSVGPathElement::DidModifySVGObservable(nsISVGValue* observable,
                                         nsISVGValue::modificationType aModType)
{
  nsCOMPtr<nsIDOMSVGPathSegList> list = do_QueryInterface(observable);

  if (list && mSegments == list) {
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mSegments);
    nsAutoString d;
    nsresult rv = value->GetValueString(d);
    NS_ENSURE_SUCCESS(rv, rv);

    // Want to keep the seglist alive - SetAttr normally invalidates it
    nsCOMPtr<nsIDOMSVGPathSegList> deathGrip = mSegments;
    mSegments = nsnull;

    rv = SetAttr(kNameSpaceID_None, nsGkAtoms::d, d, PR_TRUE);

    // Restore seglist
    mSegments = deathGrip;

    return rv;
  }

  return nsSVGPathElementBase::DidModifySVGObservable(observable, aModType);
}

already_AddRefed<gfxFlattenedPath>
nsSVGPathElement::GetFlattenedPath(nsIDOMSVGMatrix *aMatrix)
{
  gfxContext ctx(nsSVGUtils::GetThebesComputationalSurface());

  if (aMatrix) {
    ctx.SetMatrix(nsSVGUtils::ConvertSVGMatrixToThebes(aMatrix));
  }

  mPathData.Playback(&ctx);

  ctx.IdentityMatrix();

  return ctx.GetFlattenedPath();
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

PRBool
nsSVGPathElement::IsDependentAttribute(nsIAtom *aName)
{
  if (aName == nsGkAtoms::d)
    return PR_TRUE;

  return PR_FALSE;
}

PRBool
nsSVGPathElement::IsMarkable()
{
  return PR_TRUE;
}

static double
CalcVectorAngle(double ux, double uy, double vx, double vy)
{
  double ta = atan2(uy, ux);
  double tb = atan2(vy, vx);
  if (tb >= ta)
    return tb-ta;
  return 2 * M_PI - (ta-tb);
}

void
nsSVGPathElement::GetMarkPoints(nsTArray<nsSVGMark> *aMarks) {
  if (NS_FAILED(CreatePathSegList()))
    return;

  PRUint32 count;
  mSegments->GetNumberOfItems(&count);
  nsCOMPtr<nsIDOMSVGPathSeg> segment;

  float cx = 0.0f; // current point
  float cy = 0.0f;

  float cx1 = 0.0f; // last controlpoint (for s,S,t,T)
  float cy1 = 0.0f;

  PRUint16 lastSegmentType = nsIDOMSVGPathSeg::PATHSEG_UNKNOWN;

  float px, py;    // subpath initial point
  float pathAngle;
  PRUint32 pathIndex;

  float prevAngle = 0, startAngle, endAngle;

  PRBool newSegment = PR_FALSE;

  PRUint32 i;
  for (i = 0; i < count; ++i) {
    nsCOMPtr<nsIDOMSVGPathSeg> segment;
    mSegments->GetItem(i, getter_AddRefs(segment));

    PRUint16 type = nsIDOMSVGPathSeg::PATHSEG_UNKNOWN;
    segment->GetPathSegType(&type);

    float x, y;
    PRBool absCoords = PR_FALSE;

    switch (type) {
    case nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH:
    {
      x = px;
      y = py;
      startAngle = endAngle = atan2(y - cy, x - cx);
    }
    break;

    case nsIDOMSVGPathSeg::PATHSEG_MOVETO_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_MOVETO_REL:
    {
      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegMovetoRel> moveseg = do_QueryInterface(segment);
        NS_ASSERTION(moveseg, "interface not implemented");
        moveseg->GetX(&x);
        moveseg->GetY(&y);
        x += cx;
        y += cy;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegMovetoAbs> moveseg = do_QueryInterface(segment);
        NS_ASSERTION(moveseg, "interface not implemented");
        moveseg->GetX(&x);
        moveseg->GetY(&y);
      }
      px = x;
      py = y;
      startAngle = endAngle = prevAngle;
      newSegment = PR_TRUE;
    }
    break;

    case nsIDOMSVGPathSeg::PATHSEG_LINETO_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_REL:
    {
      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegLinetoRel> lineseg = do_QueryInterface(segment);
        NS_ASSERTION(lineseg, "interface not implemented");
        lineseg->GetX(&x);
        lineseg->GetY(&y);
        x += cx;
        y += cy;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegLinetoAbs> lineseg = do_QueryInterface(segment);
        NS_ASSERTION(lineseg, "interface not implemented");
        lineseg->GetX(&x);
        lineseg->GetY(&y);
      }
      startAngle = endAngle = atan2(y - cy, x - cx);
    }
    break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_REL:
    {
      float x1, y1, x2, y2;
      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegCurvetoCubicRel> curveseg = do_QueryInterface(segment);
        NS_ASSERTION(curveseg, "interface not implemented");
        curveseg->GetX(&x);
        curveseg->GetY(&y);
        curveseg->GetX1(&x1);
        curveseg->GetY1(&y1);
        curveseg->GetX2(&x2);
        curveseg->GetY2(&y2);
        x  += cx;
        y  += cy;
        x1 += cx;
        y1 += cy;
        x2 += cx;
        y2 += cy;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegCurvetoCubicAbs> curveseg = do_QueryInterface(segment);
        NS_ASSERTION(curveseg, "interface not implemented");
        curveseg->GetX(&x);
        curveseg->GetY(&y);
        curveseg->GetX1(&x1);
        curveseg->GetY1(&y1);
        curveseg->GetX2(&x2);
        curveseg->GetY2(&y2);
      }

      cx1 = x2;
      cy1 = y2;

      if (x1 == cx && y1 == cy) {
        x1 = x2;
        y1 = y2;
      }

      if (x2 == x && y2 == y) {
        x2 = x1;
        y2 = y1;
      }

      startAngle = atan2(y1 - cy, x1 - cx);
      endAngle = atan2(y - y2, x - x2);
    }
    break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL:
    {
      float x1, y1;

      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegCurvetoQuadraticRel> curveseg = do_QueryInterface(segment);
        NS_ASSERTION(curveseg, "interface not implemented");
        curveseg->GetX(&x);
        curveseg->GetY(&y);
        curveseg->GetX1(&x1);
        curveseg->GetY1(&y1);
        x  += cx;
        y  += cy;
        x1 += cx;
        y1 += cy;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegCurvetoQuadraticAbs> curveseg = do_QueryInterface(segment);
        NS_ASSERTION(curveseg, "interface not implemented");
        curveseg->GetX(&x);
        curveseg->GetY(&y);
        curveseg->GetX1(&x1);
        curveseg->GetY1(&y1);
      }

      cx1 = x1;
      cy1 = y1;

      startAngle = atan2(y1 - cy, x1 - cx);
      endAngle = atan2(y - y1, x - x1);
    }
    break;

    case nsIDOMSVGPathSeg::PATHSEG_ARC_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_ARC_REL:
    {
      float r1, r2, angle;
      PRBool largeArcFlag, sweepFlag;

      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegArcRel> arcseg = do_QueryInterface(segment);
        NS_ASSERTION(arcseg, "interface not implemented");
        arcseg->GetX(&x);
        arcseg->GetY(&y);
        arcseg->GetR1(&r1);
        arcseg->GetR2(&r2);
        arcseg->GetAngle(&angle);
        arcseg->GetLargeArcFlag(&largeArcFlag);
        arcseg->GetSweepFlag(&sweepFlag);

        x  += cx;
        y  += cy;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegArcAbs> arcseg = do_QueryInterface(segment);
        NS_ASSERTION(arcseg, "interface not implemented");
        arcseg->GetX(&x);
        arcseg->GetY(&y);
        arcseg->GetR1(&r1);
        arcseg->GetR2(&r2);
        arcseg->GetAngle(&angle);
        arcseg->GetLargeArcFlag(&largeArcFlag);
        arcseg->GetSweepFlag(&sweepFlag);
      }

      /* check for degenerate ellipse */
      if (r1 == 0.0 || r2 == 0.0) {
        startAngle = endAngle = atan2(y - cy, x - cx);
        break;
      }

      r1 = fabs(r1);  r2 = fabs(r2);

      float xp, yp, cxp, cyp;

      /* slope fun&games ... see SVG spec, section F.6 */
      angle = angle*M_PI/180.0;
      xp = cos(angle)*(cx-x)/2.0 + sin(angle)*(cy-y)/2.0;
      yp = -sin(angle)*(cx-x)/2.0 + cos(angle)*(cy-y)/2.0;

      /* make sure radii are large enough */
      float root, numerator = r1*r1*r2*r2 - r1*r1*yp*yp - r2*r2*xp*xp;
      if (numerator < 0.0) {
        float s = sqrt(1.0 - numerator/(r1*r1*r2*r2));
        r1 *= s;
        r2 *= s;
        root = 0.0;
      } else {
        root = sqrt(numerator/(r1*r1*yp*yp + r2*r2*xp*xp));
        if (largeArcFlag == sweepFlag)
          root = -root;
      }
      cxp = root*r1*yp/r2;
      cyp = -root*r2*xp/r1;

      float theta, delta;
      theta = CalcVectorAngle(1.0, 0.0,  (xp-cxp)/r1, (yp-cyp)/r2);
      delta  = CalcVectorAngle((xp-cxp)/r1, (yp-cyp)/r2,
                               (-xp-cxp)/r1, (-yp-cyp)/r2);
      if (!sweepFlag && delta > 0)
        delta -= 2.0*M_PI;
      else if (sweepFlag && delta < 0)
        delta += 2.0*M_PI;

      float tx1, ty1, tx2, ty2;
      tx1 = -cos(angle)*r1*sin(theta) - sin(angle)*r2*cos(theta);
      ty1 = -sin(angle)*r1*sin(theta) + cos(angle)*r2*cos(theta);
      tx2 = -cos(angle)*r1*sin(theta+delta) - sin(angle)*r2*cos(theta+delta);
      ty2 = -sin(angle)*r1*sin(theta+delta) + cos(angle)*r2*cos(theta+delta);

      if (delta < 0.0f) {
        tx1 = -tx1;
        ty1 = -ty1;
        tx2 = -tx2;
        ty2 = -ty2;
      }

      startAngle = atan2(ty1, tx1);
      endAngle = atan2(ty2, tx2);
    }
    break;

    case nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_REL:
    {
      y = cy;
      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegLinetoHorizontalRel> lineseg = do_QueryInterface(segment);
        NS_ASSERTION(lineseg, "interface not implemented");
        lineseg->GetX(&x);
        x += cx;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegLinetoHorizontalAbs> lineseg = do_QueryInterface(segment);
        NS_ASSERTION(lineseg, "interface not implemented");
        lineseg->GetX(&x);
      }
      startAngle = endAngle = atan2(0, x - cx);
    }
    break;

    case nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_REL:
    {
      x = cx;
      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegLinetoVerticalRel> lineseg = do_QueryInterface(segment);
        NS_ASSERTION(lineseg, "interface not implemented");
        lineseg->GetY(&y);
        y += cy;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegLinetoVerticalAbs> lineseg = do_QueryInterface(segment);
        NS_ASSERTION(lineseg, "interface not implemented");
        lineseg->GetY(&y);
      }
      startAngle = endAngle = atan2(y - cy, 0);
    }
    break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
    {
      float x1, y1, x2, y2;

      if (lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_REL        ||
          lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS        ||
          lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL ||
          lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS ) {
        // the first controlpoint is the reflection of the last one about the current point:
        x1 = 2*cx - cx1;
        y1 = 2*cy - cy1;
      }
      else {
        // the first controlpoint is equal to the current point:
        x1 = cx;
        y1 = cy;
      }

      if (!absCoords) {
        nsCOMPtr<nsIDOMSVGPathSegCurvetoCubicSmoothRel> curveseg = do_QueryInterface(segment);
        NS_ASSERTION(curveseg, "interface not implemented");
        curveseg->GetX(&x);
        curveseg->GetY(&y);
        curveseg->GetX2(&x2);
        curveseg->GetY2(&y2);
        x  += cx;
        y  += cy;
        x2 += cx;
        y2 += cy;
      } else {
        nsCOMPtr<nsIDOMSVGPathSegCurvetoCubicSmoothAbs> curveseg = do_QueryInterface(segment);
        NS_ASSERTION(curveseg, "interface not implemented");
        curveseg->GetX(&x);
        curveseg->GetY(&y);
        curveseg->GetX2(&x2);
        curveseg->GetY2(&y2);
      }

      cx1 = x2;
      cy1 = y2;

      if (x1 == cx && y1 == cy) {
        x1 = x2;
        y1 = y2;
      }

      if (x2 == x && y2 == y) {
        x2 = x1;
        y2 = y1;
      }

      startAngle = atan2(y1 - cy, x1 - cx);
      endAngle = atan2(y - y2, x - x2);
    }
    break;

    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
      absCoords = PR_TRUE;
    case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
      {
        float x1, y1;

        if (lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL        ||
            lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS        ||
            lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL ||
            lastSegmentType == nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS ) {
          // the first controlpoint is the reflection of the last one about the current point:
          x1 = 2*cx - cx1;
          y1 = 2*cy - cy1;
        }
        else {
          // the first controlpoint is equal to the current point:
          x1 = cx;
          y1 = cy;
        }

        if (!absCoords) {
          nsCOMPtr<nsIDOMSVGPathSegCurvetoQuadraticSmoothRel> curveseg = do_QueryInterface(segment);
          NS_ASSERTION(curveseg, "interface not implemented");
          curveseg->GetX(&x);
          curveseg->GetY(&y);
          x  += cx;
          y  += cy;
        } else {
          nsCOMPtr<nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs> curveseg = do_QueryInterface(segment);
          NS_ASSERTION(curveseg, "interface not implemented");
          curveseg->GetX(&x);
          curveseg->GetY(&y);
        }

        cx1 = x1;
        cy1 = y1;

        startAngle = atan2(y1 - cy, x1 - cx);
        endAngle = atan2(y - y1, x - x1);
      }
      break;

    default:
      NS_ASSERTION(1==0, "unknown path segment");
      break;
    }
    lastSegmentType = type;

    if (newSegment &&
        type != nsIDOMSVGPathSeg::PATHSEG_MOVETO_ABS &&
        type != nsIDOMSVGPathSeg::PATHSEG_MOVETO_REL) {
      pathIndex = aMarks->Length() - 1;
      pathAngle = startAngle;
      aMarks->ElementAt(pathIndex).angle = pathAngle;
      newSegment = PR_FALSE;
      prevAngle = endAngle;
    } else if (type == nsIDOMSVGPathSeg::PATHSEG_MOVETO_ABS ||
               type == nsIDOMSVGPathSeg::PATHSEG_MOVETO_REL) {
      if (aMarks->Length())
        aMarks->ElementAt(aMarks->Length() - 1).angle = prevAngle;
    } else {
      aMarks->ElementAt(aMarks->Length() - 1).angle =
        nsSVGUtils::AngleBisect(prevAngle, startAngle);
      prevAngle = endAngle;
    }

    aMarks->AppendElement(nsSVGMark(x, y, 0));

    if (type == nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH) {
      prevAngle = nsSVGUtils::AngleBisect(endAngle, pathAngle);
      aMarks->ElementAt(pathIndex).angle = prevAngle;
    }

    cx = x;
    cy = y;
  }

  if (aMarks->Length())
    aMarks->ElementAt(aMarks->Length() - 1).angle = prevAngle;
}



//==================================================================
// nsSVGPathList

void
nsSVGPathList::Clear()
{
  if (mArguments) {
    free(mArguments);
    mArguments = nsnull;
  }
  mNumCommands = 0;
  mNumArguments = 0;
}

void
nsSVGPathList::Playback(gfxContext *aCtx)
{
  float *args = mArguments;
  for (PRUint32 i = 0; i < mNumCommands; i++) {
    PRUint8 command =
      NS_REINTERPRET_CAST(PRUint8*, mArguments + mNumArguments)[i / 4];
    command = (command >> (2 * (i % 4))) & 0x3;
    switch (command) {
    case MOVETO:
      aCtx->MoveTo(gfxPoint(args[0], args[1]));
      args += 2;
      break;
    case LINETO:
      aCtx->LineTo(gfxPoint(args[0], args[1]));
      args += 2;
      break;
    case CURVETO:
      aCtx->CurveTo(gfxPoint(args[0], args[1]),
                    gfxPoint(args[2], args[3]),
                    gfxPoint(args[4], args[5]));
      args += 6;
      break;
    case CLOSEPATH:
      aCtx->ClosePath();
      break;
    default:
      break;
    }
  }
}

void
nsSVGPathElement::ConstructPath(gfxContext *aCtx)
{
  mPathData.Playback(aCtx);
}
