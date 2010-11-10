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
#include "nsIDOMSVGPathSeg.h"
#include "DOMSVGPathSeg.h"
#include "DOMSVGPathSegList.h"
#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsSVGPathDataParser.h"
#include "nsSVGPathElement.h"
#include "nsISVGValueUtils.h"
#include "nsSVGUtils.h"
#include "nsSVGPoint.h"
#include "gfxContext.h"
#include "gfxPlatform.h"

using namespace mozilla;

nsSVGElement::NumberInfo nsSVGPathElement::sNumberInfo = 
                                                  { &nsGkAtoms::pathLength, 0 };

NS_IMPL_NS_NEW_SVG_ELEMENT(Path)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGPathElement,nsSVGPathElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGPathElement,nsSVGPathElementBase)

DOMCI_NODE_DATA(SVGPathElement, nsSVGPathElement)

NS_INTERFACE_TABLE_HEAD(nsSVGPathElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGPathElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGPathElement,
                           nsIDOMSVGAnimatedPathData)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGPathElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPathElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGPathElement::nsSVGPathElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGPathElementBase(aNodeInfo)
{
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

  nsRefPtr<gfxFlattenedPath> flat = GetFlattenedPath(gfxMatrix());

  if (!flat)
    return NS_ERROR_FAILURE;

  *_retval = flat->GetLength();

  return NS_OK;
}

/* nsIDOMSVGPoint getPointAtLength (in float distance); */
NS_IMETHODIMP
nsSVGPathElement::GetPointAtLength(float distance, nsIDOMSVGPoint **_retval)
{
  NS_ENSURE_FINITE(distance, NS_ERROR_ILLEGAL_VALUE);

  nsRefPtr<gfxFlattenedPath> flat = GetFlattenedPath(gfxMatrix());
  if (!flat)
    return NS_ERROR_FAILURE;

  float totalLength = flat->GetLength();
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::pathLength)) {
    float pathLength = mPathLength.GetAnimValue();
    distance *= totalLength / pathLength;
  }
  distance = NS_MAX(0.f,         distance);
  distance = NS_MIN(totalLength, distance);

  return NS_NewSVGPoint(_retval, flat->FindPoint(gfxPoint(distance, 0)));
}

/* unsigned long getPathSegAtLength (in float distance); */
NS_IMETHODIMP
nsSVGPathElement::GetPathSegAtLength(float distance, PRUint32 *_retval)
{
  NS_ENSURE_FINITE(distance, NS_ERROR_ILLEGAL_VALUE);
  *_retval = mD.GetAnimValue().GetPathSegAtLength(distance);
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
  NS_ENSURE_FINITE2(x, y, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegMovetoAbs(x, y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegMovetoRel createSVGPathSegMovetoRel (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegMovetoRel(float x, float y, nsIDOMSVGPathSegMovetoRel **_retval)
{
  NS_ENSURE_FINITE2(x, y, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegMovetoRel(x, y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegLinetoAbs createSVGPathSegLinetoAbs (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoAbs(float x, float y, nsIDOMSVGPathSegLinetoAbs **_retval)
{
  NS_ENSURE_FINITE2(x, y, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegLinetoAbs(x, y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegLinetoRel createSVGPathSegLinetoRel (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoRel(float x, float y, nsIDOMSVGPathSegLinetoRel **_retval)
{
  NS_ENSURE_FINITE2(x, y, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegLinetoRel(x, y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoCubicAbs createSVGPathSegCurvetoCubicAbs (in float x, in float y, in float x1, in float y1, in float x2, in float y2); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2, nsIDOMSVGPathSegCurvetoCubicAbs **_retval)
{
  NS_ENSURE_FINITE6(x, y, x1, y1, x2, y2, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoCubicAbs(x, y, x1, y1, x2, y2);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoCubicRel createSVGPathSegCurvetoCubicRel (in float x, in float y, in float x1, in float y1, in float x2, in float y2); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2, nsIDOMSVGPathSegCurvetoCubicRel **_retval)
{
  NS_ENSURE_FINITE6(x, y, x1, y1, x2, y2, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoCubicRel(x, y, x1, y1, x2, y2);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoQuadraticAbs createSVGPathSegCurvetoQuadraticAbs (in float x, in float y, in float x1, in float y1); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1, nsIDOMSVGPathSegCurvetoQuadraticAbs **_retval)
{
  NS_ENSURE_FINITE4(x, y, x1, y1, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoQuadraticAbs(x, y, x1, y1);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoQuadraticRel createSVGPathSegCurvetoQuadraticRel (in float x, in float y, in float x1, in float y1); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1, nsIDOMSVGPathSegCurvetoQuadraticRel **_retval)
{
  NS_ENSURE_FINITE4(x, y, x1, y1, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoQuadraticRel(x, y, x1, y1);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegArcAbs createSVGPathSegArcAbs (in float x, in float y, in float r1, in float r2, in float angle, in boolean largeArcFlag, in boolean sweepFlag); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, PRBool largeArcFlag, PRBool sweepFlag, nsIDOMSVGPathSegArcAbs **_retval)
{
  NS_ENSURE_FINITE5(x, y, r1, r2, angle, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegArcAbs(x, y, r1, r2, angle,
                                                 largeArcFlag, sweepFlag);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegArcRel createSVGPathSegArcRel (in float x, in float y, in float r1, in float r2, in float angle, in boolean largeArcFlag, in boolean sweepFlag); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, PRBool largeArcFlag, PRBool sweepFlag, nsIDOMSVGPathSegArcRel **_retval)
{
  NS_ENSURE_FINITE5(x, y, r1, r2, angle, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegArcRel(x, y, r1, r2, angle,
                                                 largeArcFlag, sweepFlag);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegLinetoHorizontalAbs createSVGPathSegLinetoHorizontalAbs (in float x); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoHorizontalAbs(float x, nsIDOMSVGPathSegLinetoHorizontalAbs **_retval)
{
  NS_ENSURE_FINITE(x, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegLinetoHorizontalAbs(x);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegLinetoHorizontalRel createSVGPathSegLinetoHorizontalRel (in float x); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoHorizontalRel(float x, nsIDOMSVGPathSegLinetoHorizontalRel **_retval)
{
  NS_ENSURE_FINITE(x, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegLinetoHorizontalRel(x);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegLinetoVerticalAbs createSVGPathSegLinetoVerticalAbs (in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoVerticalAbs(float y, nsIDOMSVGPathSegLinetoVerticalAbs **_retval)
{
  NS_ENSURE_FINITE(y, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegLinetoVerticalAbs(y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegLinetoVerticalRel createSVGPathSegLinetoVerticalRel (in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegLinetoVerticalRel(float y, nsIDOMSVGPathSegLinetoVerticalRel **_retval)
{
  NS_ENSURE_FINITE(y, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegLinetoVerticalRel(y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoCubicSmoothAbs createSVGPathSegCurvetoCubicSmoothAbs (in float x, in float y, in float x2, in float y2); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2, nsIDOMSVGPathSegCurvetoCubicSmoothAbs **_retval)
{
  NS_ENSURE_FINITE4(x, y, x2, y2, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoCubicSmoothAbs(x, y, x2, y2);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoCubicSmoothRel createSVGPathSegCurvetoCubicSmoothRel (in float x, in float y, in float x2, in float y2); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2, nsIDOMSVGPathSegCurvetoCubicSmoothRel **_retval)
{
  NS_ENSURE_FINITE4(x, y, x2, y2, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoCubicSmoothRel(x, y, x2, y2);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs createSVGPathSegCurvetoQuadraticSmoothAbs (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y, nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs **_retval)
{
  NS_ENSURE_FINITE2(x, y, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoQuadraticSmoothAbs(x, y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
}

/* nsIDOMSVGPathSegCurvetoQuadraticSmoothRel createSVGPathSegCurvetoQuadraticSmoothRel (in float x, in float y); */
NS_IMETHODIMP
nsSVGPathElement::CreateSVGPathSegCurvetoQuadraticSmoothRel(float x, float y, nsIDOMSVGPathSegCurvetoQuadraticSmoothRel **_retval)
{
  NS_ENSURE_FINITE2(x, y, NS_ERROR_ILLEGAL_VALUE);
  nsIDOMSVGPathSeg* seg = NS_NewSVGPathSegCurvetoQuadraticSmoothRel(x, y);
  NS_ENSURE_TRUE(seg, NS_ERROR_OUT_OF_MEMORY);
  return CallQueryInterface(seg, _retval);
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
  void *key = mD.GetBaseValKey();
  *aPathSegList = DOMSVGPathSegList::GetDOMWrapper(key, this, PR_FALSE).get();
  return *aPathSegList ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute nsIDOMSVGPathSegList normalizedPathSegList; */
NS_IMETHODIMP nsSVGPathElement::GetNormalizedPathSegList(nsIDOMSVGPathSegList * *aNormalizedPathSegList)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMSVGPathSegList animatedPathSegList; */
NS_IMETHODIMP nsSVGPathElement::GetAnimatedPathSegList(nsIDOMSVGPathSegList * *aAnimatedPathSegList)
{
  void *key = mD.GetAnimValKey();
  *aAnimatedPathSegList =
    DOMSVGPathSegList::GetDOMWrapper(key, this, PR_TRUE).get();
  return *aAnimatedPathSegList ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
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

already_AddRefed<gfxFlattenedPath>
nsSVGPathElement::GetFlattenedPath(const gfxMatrix &aMatrix)
{
  return mD.GetAnimValue().ToFlattenedPath(aMatrix);
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

PRBool
nsSVGPathElement::AttributeDefinesGeometry(const nsIAtom *aName)
{
  if (aName == nsGkAtoms::d ||
      aName == nsGkAtoms::pathLength)
    return PR_TRUE;

  return PR_FALSE;
}

PRBool
nsSVGPathElement::IsMarkable()
{
  return PR_TRUE;
}

void
nsSVGPathElement::GetMarkPoints(nsTArray<nsSVGMark> *aMarks)
{
  mD.GetAnimValue().GetMarkerPositioningData(aMarks);
}

void
nsSVGPathElement::ConstructPath(gfxContext *aCtx)
{
  mD.GetAnimValue().ConstructPath(aCtx);
}

