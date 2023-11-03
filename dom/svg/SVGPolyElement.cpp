/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGPolyElement.h"
#include "DOMSVGPointList.h"
#include "mozilla/gfx/2D.h"
#include "SVGContentUtils.h"

using namespace mozilla::gfx;

namespace mozilla::dom {

//----------------------------------------------------------------------
// Implementation

SVGPolyElement::SVGPolyElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGPolyElementBase(std::move(aNodeInfo)) {}

already_AddRefed<DOMSVGPointList> SVGPolyElement::Points() {
  void* key = mPoints.GetBaseValKey();
  RefPtr<DOMSVGPointList> points =
      DOMSVGPointList::GetDOMWrapper(key, this, false);
  return points.forget();
}

already_AddRefed<DOMSVGPointList> SVGPolyElement::AnimatedPoints() {
  void* key = mPoints.GetAnimValKey();
  RefPtr<DOMSVGPointList> points =
      DOMSVGPointList::GetDOMWrapper(key, this, true);
  return points.forget();
}

//----------------------------------------------------------------------
// SVGElement methods

/* virtual */
bool SVGPolyElement::HasValidDimensions() const {
  return !mPoints.GetAnimValue().IsEmpty();
}

//----------------------------------------------------------------------
// SVGGeometryElement methods

bool SVGPolyElement::AttributeDefinesGeometry(const nsAtom* aName) {
  return aName == nsGkAtoms::points;
}

void SVGPolyElement::GetMarkPoints(nsTArray<SVGMark>* aMarks) {
  const SVGPointList& points = mPoints.GetAnimValue();

  if (!points.Length()) return;

  float px = points[0].mX, py = points[0].mY, prevAngle = 0.0;

  aMarks->AppendElement(SVGMark(px, py, 0, SVGMark::eStart));

  for (uint32_t i = 1; i < points.Length(); ++i) {
    float x = points[i].mX;
    float y = points[i].mY;
    float angle = std::atan2(y - py, x - px);

    // Vertex marker.
    if (i == 1) {
      aMarks->ElementAt(0).angle = angle;
    } else {
      aMarks->LastElement().angle =
          SVGContentUtils::AngleBisect(prevAngle, angle);
    }

    aMarks->AppendElement(SVGMark(x, y, 0, SVGMark::eMid));

    prevAngle = angle;
    px = x;
    py = y;
  }

  aMarks->LastElement().angle = prevAngle;
  aMarks->LastElement().type = SVGMark::eEnd;
}

bool SVGPolyElement::GetGeometryBounds(Rect* aBounds,
                                       const StrokeOptions& aStrokeOptions,
                                       const Matrix& aToBoundsSpace,
                                       const Matrix* aToNonScalingStrokeSpace) {
  const SVGPointList& points = mPoints.GetAnimValue();

  if (!points.Length()) {
    // Rendering of the element is disabled
    aBounds->SetEmpty();
    return true;
  }

  if (aStrokeOptions.mLineWidth > 0 || aToNonScalingStrokeSpace) {
    // We don't handle non-scaling-stroke or stroke-miterlimit etc. yet
    return false;
  }

  if (aToBoundsSpace.IsRectilinear()) {
    // We can avoid transforming each point and just transform the result.
    // Important for large point lists.
    Rect bounds(points[0], Size());
    for (uint32_t i = 1; i < points.Length(); ++i) {
      bounds.ExpandToEnclose(points[i]);
    }
    *aBounds = aToBoundsSpace.TransformBounds(bounds);
  } else {
    *aBounds = Rect(aToBoundsSpace.TransformPoint(points[0]), Size());
    for (uint32_t i = 1; i < points.Length(); ++i) {
      aBounds->ExpandToEnclose(aToBoundsSpace.TransformPoint(points[i]));
    }
  }
  return true;
}
}  // namespace mozilla::dom
