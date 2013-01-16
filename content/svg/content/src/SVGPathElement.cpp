/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsGkAtoms.h"
#include "DOMSVGPathSeg.h"
#include "DOMSVGPathSegList.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "mozilla/dom/SVGPathElement.h"
#include "DOMSVGPoint.h"
#include "gfxContext.h"
#include <algorithm>
#include "mozilla/dom/SVGPathElementBinding.h"

DOMCI_NODE_DATA(SVGPathElement, mozilla::dom::SVGPathElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Path)

namespace mozilla {
namespace dom {

JSObject*
SVGPathElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGPathElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

nsSVGElement::NumberInfo SVGPathElement::sNumberInfo = 
{ &nsGkAtoms::pathLength, 0, false };

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGPathElement,SVGPathElementBase)
NS_IMPL_RELEASE_INHERITED(SVGPathElement,SVGPathElementBase)

NS_INTERFACE_TABLE_HEAD(SVGPathElement)
  NS_NODE_INTERFACE_TABLE4(SVGPathElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGPathElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGPathElement)
NS_INTERFACE_MAP_END_INHERITING(SVGPathElementBase)

//----------------------------------------------------------------------
// Implementation

SVGPathElement::SVGPathElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGPathElementBase(aNodeInfo)
{
  SetIsDOMBinding();
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGPathElement)

//----------------------------------------------------------------------
// nsIDOMSVGPathElement methods:

/* readonly attribute nsIDOMSVGAnimatedNumber pathLength; */
NS_IMETHODIMP
SVGPathElement::GetPathLength(nsIDOMSVGAnimatedNumber * *aPathLength)
{
  *aPathLength = PathLength().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGPathElement::PathLength()
{
  nsCOMPtr<nsIDOMSVGAnimatedNumber> number;
  mPathLength.ToDOMAnimatedNumber(getter_AddRefs(number), this);
  return number.forget();
}

/* float getTotalLength (); */
NS_IMETHODIMP
SVGPathElement::GetTotalLength(float *_retval)
{
  ErrorResult rv;
  *_retval = GetTotalLength(rv);
  return rv.ErrorCode();
}

float
SVGPathElement::GetTotalLength(ErrorResult& rv)
{
  nsRefPtr<gfxFlattenedPath> flat = GetFlattenedPath(gfxMatrix());

  if (!flat) {
    rv.Throw(NS_ERROR_FAILURE);
    return 0.f;
  }

  return flat->GetLength();
}

/* DOMSVGPoint getPointAtLength (in float distance); */
NS_IMETHODIMP
SVGPathElement::GetPointAtLength(float distance, nsISupports **_retval)
{
  NS_ENSURE_FINITE(distance, NS_ERROR_ILLEGAL_VALUE);

  ErrorResult rv;
  *_retval = GetPointAtLength(distance, rv).get();
  return rv.ErrorCode();
}

already_AddRefed<nsISVGPoint>
SVGPathElement::GetPointAtLength(float distance, ErrorResult& rv)
{
  nsRefPtr<gfxFlattenedPath> flat = GetFlattenedPath(gfxMatrix());
  if (!flat) {
    rv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  float totalLength = flat->GetLength();
  if (mPathLength.IsExplicitlySet()) {
    float pathLength = mPathLength.GetAnimValue();
    if (pathLength <= 0) {
      rv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
    distance *= totalLength / pathLength;
  }
  distance = std::max(0.f,         distance);
  distance = std::min(totalLength, distance);

  nsCOMPtr<nsISVGPoint> point = new DOMSVGPoint(flat->FindPoint(gfxPoint(distance, 0)));
  return point.forget();
}

/* unsigned long getPathSegAtLength (in float distance); */
NS_IMETHODIMP
SVGPathElement::GetPathSegAtLength(float distance, uint32_t *_retval)
{
  NS_ENSURE_FINITE(distance, NS_ERROR_ILLEGAL_VALUE);
  *_retval = GetPathSegAtLength(distance);
  return NS_OK;
}

uint32_t
SVGPathElement::GetPathSegAtLength(float distance)
{
  return mD.GetAnimValue().GetPathSegAtLength(distance);
}

/* nsISupports createSVGPathSegClosePath (); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegClosePath(nsISupports **_retval)
{
  *_retval = CreateSVGPathSegClosePath().get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegClosePath>
SVGPathElement::CreateSVGPathSegClosePath()
{
  nsRefPtr<DOMSVGPathSegClosePath> pathSeg = new DOMSVGPathSegClosePath();
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegMovetoAbs (in float x, in float y); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegMovetoAbs(float x, float y, nsISupports **_retval)
{
  NS_ENSURE_FINITE2(x, y, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegMovetoAbs(x, y).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegMovetoAbs>
SVGPathElement::CreateSVGPathSegMovetoAbs(float x, float y)
{
  nsRefPtr<DOMSVGPathSegMovetoAbs> pathSeg = new DOMSVGPathSegMovetoAbs(x, y);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegMovetoRel (in float x, in float y); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegMovetoRel(float x, float y, nsISupports **_retval)
{
  NS_ENSURE_FINITE2(x, y, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegMovetoRel(x, y).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegMovetoRel>
SVGPathElement::CreateSVGPathSegMovetoRel(float x, float y)
{
  nsRefPtr<DOMSVGPathSegMovetoRel> pathSeg = new DOMSVGPathSegMovetoRel(x, y);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegLinetoAbs (in float x, in float y); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegLinetoAbs(float x, float y, nsISupports **_retval)
{
  NS_ENSURE_FINITE2(x, y, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegLinetoAbs(x, y).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegLinetoAbs>
SVGPathElement::CreateSVGPathSegLinetoAbs(float x, float y)
{
  nsRefPtr<DOMSVGPathSegLinetoAbs> pathSeg = new DOMSVGPathSegLinetoAbs(x, y);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegLinetoRel (in float x, in float y); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegLinetoRel(float x, float y, nsISupports **_retval)
{
  NS_ENSURE_FINITE2(x, y, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegLinetoRel(x, y).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegLinetoRel>
SVGPathElement::CreateSVGPathSegLinetoRel(float x, float y)
{
  nsRefPtr<DOMSVGPathSegLinetoRel> pathSeg = new DOMSVGPathSegLinetoRel(x, y);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegCurvetoCubicAbs (in float x, in float y, in float x1, in float y1, in float x2, in float y2); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2, nsISupports **_retval)
{
  NS_ENSURE_FINITE6(x, y, x1, y1, x2, y2, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegCurvetoCubicAbs(x, y, x1, y1, x2, y2).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegCurvetoCubicAbs>
SVGPathElement::CreateSVGPathSegCurvetoCubicAbs(float x, float y, float x1, float y1, float x2, float y2)
{
  // Note that we swap from DOM API argument order to the argument order used
  // in the <path> element's 'd' attribute (i.e. we put the arguments for the
  // end point of the segment last instead of first).
  nsRefPtr<DOMSVGPathSegCurvetoCubicAbs> pathSeg =
    new DOMSVGPathSegCurvetoCubicAbs(x1, y1, x2, y2, x, y);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegCurvetoCubicRel (in float x, in float y, in float x1, in float y1, in float x2, in float y2); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2, nsISupports **_retval)
{
  NS_ENSURE_FINITE6(x, y, x1, y1, x2, y2, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegCurvetoCubicRel(x, y, x1, y1, x2, y2).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegCurvetoCubicRel>
SVGPathElement::CreateSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2)
{
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  nsRefPtr<DOMSVGPathSegCurvetoCubicRel> pathSeg =
    new DOMSVGPathSegCurvetoCubicRel(x1, y1, x2, y2, x, y);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegCurvetoQuadraticAbs (in float x, in float y, in float x1, in float y1); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1, nsISupports **_retval)
{
  NS_ENSURE_FINITE4(x, y, x1, y1, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegCurvetoQuadraticAbs(x, y, x1, y1).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegCurvetoQuadraticAbs>
SVGPathElement::CreateSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1)
{
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  nsRefPtr<DOMSVGPathSegCurvetoQuadraticAbs> pathSeg =
    new DOMSVGPathSegCurvetoQuadraticAbs(x1, y1, x, y);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegCurvetoQuadraticRel (in float x, in float y, in float x1, in float y1); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1, nsISupports **_retval)
{
  NS_ENSURE_FINITE4(x, y, x1, y1, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegCurvetoQuadraticRel(x, y, x1, y1).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegCurvetoQuadraticRel>
SVGPathElement::CreateSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1)
{
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  nsRefPtr<DOMSVGPathSegCurvetoQuadraticRel> pathSeg =
    new DOMSVGPathSegCurvetoQuadraticRel(x1, y1, x, y);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegArcAbs (in float x, in float y, in float r1, in float r2, in float angle, in boolean largeArcFlag, in boolean sweepFlag); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, nsISupports **_retval)
{
  NS_ENSURE_FINITE5(x, y, r1, r2, angle, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegArcAbs(x, y, r1, r2, angle, largeArcFlag, sweepFlag).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegArcAbs>
SVGPathElement::CreateSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag)
{
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  nsRefPtr<DOMSVGPathSegArcAbs> pathSeg =
    new DOMSVGPathSegArcAbs(r1, r2, angle, largeArcFlag, sweepFlag, x, y);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegArcRel (in float x, in float y, in float r1, in float r2, in float angle, in boolean largeArcFlag, in boolean sweepFlag); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, nsISupports **_retval)
{
  NS_ENSURE_FINITE5(x, y, r1, r2, angle, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegArcRel(x, y, r1, r2, angle, largeArcFlag, sweepFlag).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegArcRel>
SVGPathElement::CreateSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag)
{
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  nsRefPtr<DOMSVGPathSegArcRel> pathSeg =
    new DOMSVGPathSegArcRel(r1, r2, angle, largeArcFlag, sweepFlag, x, y);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegLinetoHorizontalAbs (in float x); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegLinetoHorizontalAbs(float x, nsISupports **_retval)
{
  NS_ENSURE_FINITE(x, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegLinetoHorizontalAbs(x).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegLinetoHorizontalAbs>
SVGPathElement::CreateSVGPathSegLinetoHorizontalAbs(float x)
{
  nsRefPtr<DOMSVGPathSegLinetoHorizontalAbs> pathSeg =
    new DOMSVGPathSegLinetoHorizontalAbs(x);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegLinetoHorizontalRel (in float x); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegLinetoHorizontalRel(float x, nsISupports **_retval)
{
  NS_ENSURE_FINITE(x, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegLinetoHorizontalRel(x).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegLinetoHorizontalRel>
SVGPathElement::CreateSVGPathSegLinetoHorizontalRel(float x)
{
  nsRefPtr<DOMSVGPathSegLinetoHorizontalRel> pathSeg =
    new DOMSVGPathSegLinetoHorizontalRel(x);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegLinetoVerticalAbs (in float y); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegLinetoVerticalAbs(float y, nsISupports **_retval)
{
  NS_ENSURE_FINITE(y, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegLinetoVerticalAbs(y).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegLinetoVerticalAbs>
SVGPathElement::CreateSVGPathSegLinetoVerticalAbs(float y)
{
  nsRefPtr<DOMSVGPathSegLinetoVerticalAbs> pathSeg =
    new DOMSVGPathSegLinetoVerticalAbs(y);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegLinetoVerticalRel (in float y); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegLinetoVerticalRel(float y, nsISupports **_retval)
{
  NS_ENSURE_FINITE(y, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegLinetoVerticalRel(y).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegLinetoVerticalRel>
SVGPathElement::CreateSVGPathSegLinetoVerticalRel(float y)
{
  nsRefPtr<DOMSVGPathSegLinetoVerticalRel> pathSeg =
    new DOMSVGPathSegLinetoVerticalRel(y);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegCurvetoCubicSmoothAbs (in float x, in float y, in float x2, in float y2); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2, nsISupports **_retval)
{
  NS_ENSURE_FINITE4(x, y, x2, y2, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegCurvetoCubicSmoothAbs(x, y, x2, y2).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegCurvetoCubicSmoothAbs>
SVGPathElement::CreateSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2)
{
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  nsRefPtr<DOMSVGPathSegCurvetoCubicSmoothAbs> pathSeg =
    new DOMSVGPathSegCurvetoCubicSmoothAbs(x2, y2, x, y);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegCurvetoCubicSmoothRel (in float x, in float y, in float x2, in float y2); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2, nsISupports **_retval)
{
  NS_ENSURE_FINITE4(x, y, x2, y2, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegCurvetoCubicSmoothRel(x, y, x2, y2).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegCurvetoCubicSmoothRel>
SVGPathElement::CreateSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2)
{
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  nsRefPtr<DOMSVGPathSegCurvetoCubicSmoothRel> pathSeg =
    new DOMSVGPathSegCurvetoCubicSmoothRel(x2, y2, x, y);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegCurvetoQuadraticSmoothAbs (in float x, in float y); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y, nsISupports **_retval)
{
  NS_ENSURE_FINITE2(x, y, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegCurvetoQuadraticSmoothAbs(x, y).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegCurvetoQuadraticSmoothAbs>
SVGPathElement::CreateSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y)
{
  nsRefPtr<DOMSVGPathSegCurvetoQuadraticSmoothAbs> pathSeg =
    new DOMSVGPathSegCurvetoQuadraticSmoothAbs(x, y);
  return pathSeg.forget();
}

/* nsISupports createSVGPathSegCurvetoQuadraticSmoothRel (in float x, in float y); */
NS_IMETHODIMP
SVGPathElement::CreateSVGPathSegCurvetoQuadraticSmoothRel(float x, float y, nsISupports **_retval)
{
  NS_ENSURE_FINITE2(x, y, NS_ERROR_ILLEGAL_VALUE);
  *_retval = CreateSVGPathSegCurvetoQuadraticSmoothRel(x, y).get();
  return NS_OK;
}

already_AddRefed<DOMSVGPathSegCurvetoQuadraticSmoothRel>
SVGPathElement::CreateSVGPathSegCurvetoQuadraticSmoothRel(float x, float y)
{
  nsRefPtr<DOMSVGPathSegCurvetoQuadraticSmoothRel> pathSeg =
    new DOMSVGPathSegCurvetoQuadraticSmoothRel(x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegList>
SVGPathElement::PathSegList()
{
  return DOMSVGPathSegList::GetDOMWrapper(mD.GetBaseValKey(), this, false);
}

already_AddRefed<DOMSVGPathSegList>
SVGPathElement::AnimatedPathSegList()
{
  return DOMSVGPathSegList::GetDOMWrapper(mD.GetAnimValKey(), this, true);
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ bool
SVGPathElement::HasValidDimensions() const
{
  return !mD.GetAnimValue().IsEmpty();
}

nsSVGElement::NumberAttributesInfo
SVGPathElement::GetNumberInfo()
{
  return NumberAttributesInfo(&mPathLength, &sNumberInfo, 1);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGPathElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sMarkersMap
  };

  return FindAttributeDependence(name, map) ||
    SVGPathElementBase::IsAttributeMapped(name);
}

already_AddRefed<gfxFlattenedPath>
SVGPathElement::GetFlattenedPath(const gfxMatrix &aMatrix)
{
  return mD.GetAnimValue().ToFlattenedPath(aMatrix);
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

bool
SVGPathElement::AttributeDefinesGeometry(const nsIAtom *aName)
{
  return aName == nsGkAtoms::d ||
         aName == nsGkAtoms::pathLength;
}

bool
SVGPathElement::IsMarkable()
{
  return true;
}

void
SVGPathElement::GetMarkPoints(nsTArray<nsSVGMark> *aMarks)
{
  mD.GetAnimValue().GetMarkerPositioningData(aMarks);
}

void
SVGPathElement::ConstructPath(gfxContext *aCtx)
{
  mD.GetAnimValue().ConstructPath(aCtx);
}

gfxFloat
SVGPathElement::GetPathLengthScale(PathLengthScaleForType aFor)
{
  NS_ABORT_IF_FALSE(aFor == eForTextPath || aFor == eForStroking,
                    "Unknown enum");
  if (mPathLength.IsExplicitlySet()) {
    float authorsPathLengthEstimate = mPathLength.GetAnimValue();
    if (authorsPathLengthEstimate > 0) {
      gfxMatrix matrix;
      if (aFor == eForTextPath) {
        // For textPath, a transform on the referenced path affects the
        // textPath layout, so when calculating the actual path length
        // we need to take that into account.
        matrix = PrependLocalTransformsTo(matrix);
      }
      nsRefPtr<gfxFlattenedPath> path = GetFlattenedPath(matrix);
      if (path) {
        return path->GetLength() / authorsPathLengthEstimate;
      }
    }
  }
  return 1.0;
}

} // namespace dom
} // namespace mozilla
