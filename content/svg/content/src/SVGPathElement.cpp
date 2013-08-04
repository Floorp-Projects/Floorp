/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGPathElement.h"

#include <algorithm>

#include "DOMSVGPathSeg.h"
#include "DOMSVGPathSegList.h"
#include "DOMSVGPoint.h"
#include "gfxPath.h"
#include "mozilla/dom/SVGPathElementBinding.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"

class gfxContext;

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Path)

namespace mozilla {
namespace dom {

JSObject*
SVGPathElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return SVGPathElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::NumberInfo SVGPathElement::sNumberInfo = 
{ &nsGkAtoms::pathLength, 0, false };

//----------------------------------------------------------------------
// Implementation

SVGPathElement::SVGPathElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGPathElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGPathElement)

already_AddRefed<SVGAnimatedNumber>
SVGPathElement::PathLength()
{
  return mPathLength.ToDOMAnimatedNumber(this);
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

uint32_t
SVGPathElement::GetPathSegAtLength(float distance)
{
  return mD.GetAnimValue().GetPathSegAtLength(distance);
}

already_AddRefed<DOMSVGPathSegClosePath>
SVGPathElement::CreateSVGPathSegClosePath()
{
  nsRefPtr<DOMSVGPathSegClosePath> pathSeg = new DOMSVGPathSegClosePath();
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegMovetoAbs>
SVGPathElement::CreateSVGPathSegMovetoAbs(float x, float y)
{
  nsRefPtr<DOMSVGPathSegMovetoAbs> pathSeg = new DOMSVGPathSegMovetoAbs(x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegMovetoRel>
SVGPathElement::CreateSVGPathSegMovetoRel(float x, float y)
{
  nsRefPtr<DOMSVGPathSegMovetoRel> pathSeg = new DOMSVGPathSegMovetoRel(x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegLinetoAbs>
SVGPathElement::CreateSVGPathSegLinetoAbs(float x, float y)
{
  nsRefPtr<DOMSVGPathSegLinetoAbs> pathSeg = new DOMSVGPathSegLinetoAbs(x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegLinetoRel>
SVGPathElement::CreateSVGPathSegLinetoRel(float x, float y)
{
  nsRefPtr<DOMSVGPathSegLinetoRel> pathSeg = new DOMSVGPathSegLinetoRel(x, y);
  return pathSeg.forget();
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

already_AddRefed<DOMSVGPathSegCurvetoCubicRel>
SVGPathElement::CreateSVGPathSegCurvetoCubicRel(float x, float y, float x1, float y1, float x2, float y2)
{
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  nsRefPtr<DOMSVGPathSegCurvetoCubicRel> pathSeg =
    new DOMSVGPathSegCurvetoCubicRel(x1, y1, x2, y2, x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegCurvetoQuadraticAbs>
SVGPathElement::CreateSVGPathSegCurvetoQuadraticAbs(float x, float y, float x1, float y1)
{
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  nsRefPtr<DOMSVGPathSegCurvetoQuadraticAbs> pathSeg =
    new DOMSVGPathSegCurvetoQuadraticAbs(x1, y1, x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegCurvetoQuadraticRel>
SVGPathElement::CreateSVGPathSegCurvetoQuadraticRel(float x, float y, float x1, float y1)
{
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  nsRefPtr<DOMSVGPathSegCurvetoQuadraticRel> pathSeg =
    new DOMSVGPathSegCurvetoQuadraticRel(x1, y1, x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegArcAbs>
SVGPathElement::CreateSVGPathSegArcAbs(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag)
{
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  nsRefPtr<DOMSVGPathSegArcAbs> pathSeg =
    new DOMSVGPathSegArcAbs(r1, r2, angle, largeArcFlag, sweepFlag, x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegArcRel>
SVGPathElement::CreateSVGPathSegArcRel(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag)
{
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  nsRefPtr<DOMSVGPathSegArcRel> pathSeg =
    new DOMSVGPathSegArcRel(r1, r2, angle, largeArcFlag, sweepFlag, x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegLinetoHorizontalAbs>
SVGPathElement::CreateSVGPathSegLinetoHorizontalAbs(float x)
{
  nsRefPtr<DOMSVGPathSegLinetoHorizontalAbs> pathSeg =
    new DOMSVGPathSegLinetoHorizontalAbs(x);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegLinetoHorizontalRel>
SVGPathElement::CreateSVGPathSegLinetoHorizontalRel(float x)
{
  nsRefPtr<DOMSVGPathSegLinetoHorizontalRel> pathSeg =
    new DOMSVGPathSegLinetoHorizontalRel(x);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegLinetoVerticalAbs>
SVGPathElement::CreateSVGPathSegLinetoVerticalAbs(float y)
{
  nsRefPtr<DOMSVGPathSegLinetoVerticalAbs> pathSeg =
    new DOMSVGPathSegLinetoVerticalAbs(y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegLinetoVerticalRel>
SVGPathElement::CreateSVGPathSegLinetoVerticalRel(float y)
{
  nsRefPtr<DOMSVGPathSegLinetoVerticalRel> pathSeg =
    new DOMSVGPathSegLinetoVerticalRel(y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegCurvetoCubicSmoothAbs>
SVGPathElement::CreateSVGPathSegCurvetoCubicSmoothAbs(float x, float y, float x2, float y2)
{
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  nsRefPtr<DOMSVGPathSegCurvetoCubicSmoothAbs> pathSeg =
    new DOMSVGPathSegCurvetoCubicSmoothAbs(x2, y2, x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegCurvetoCubicSmoothRel>
SVGPathElement::CreateSVGPathSegCurvetoCubicSmoothRel(float x, float y, float x2, float y2)
{
  // See comment in CreateSVGPathSegCurvetoCubicAbs
  nsRefPtr<DOMSVGPathSegCurvetoCubicSmoothRel> pathSeg =
    new DOMSVGPathSegCurvetoCubicSmoothRel(x2, y2, x, y);
  return pathSeg.forget();
}

already_AddRefed<DOMSVGPathSegCurvetoQuadraticSmoothAbs>
SVGPathElement::CreateSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y)
{
  nsRefPtr<DOMSVGPathSegCurvetoQuadraticSmoothAbs> pathSeg =
    new DOMSVGPathSegCurvetoQuadraticSmoothAbs(x, y);
  return pathSeg.forget();
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
