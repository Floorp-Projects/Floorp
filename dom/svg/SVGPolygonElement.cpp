/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGPolygonElement.h"
#include "mozilla/dom/SVGPolygonElementBinding.h"
#include "mozilla/gfx/2D.h"
#include "SVGContentUtils.h"

using namespace mozilla;
using namespace mozilla::gfx;

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Polygon)

namespace mozilla {
namespace dom {

JSObject*
SVGPolygonElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGPolygonElement_Binding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
// Implementation

SVGPolygonElement::SVGPolygonElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGPolygonElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGPolygonElement)

//----------------------------------------------------------------------
// SVGGeometryElement methods

void
SVGPolygonElement::GetMarkPoints(nsTArray<nsSVGMark> *aMarks)
{
  SVGPolyElement::GetMarkPoints(aMarks);

  if (aMarks->IsEmpty() || aMarks->LastElement().type != nsSVGMark::eEnd) {
    return;
  }

  nsSVGMark *endMark = &aMarks->LastElement();
  nsSVGMark *startMark = &aMarks->ElementAt(0);
  float angle = atan2(startMark->y - endMark->y, startMark->x - endMark->x);

  endMark->type = nsSVGMark::eMid;
  endMark->angle = SVGContentUtils::AngleBisect(angle, endMark->angle);
  startMark->angle = SVGContentUtils::AngleBisect(angle, startMark->angle);
  // for a polygon (as opposed to a polyline) there's an implicit extra point
  // co-located with the start point that SVGPolyElement::GetMarkPoints
  // doesn't return
  aMarks->AppendElement(nsSVGMark(startMark->x, startMark->y, startMark->angle,
                                  nsSVGMark::eEnd));
}

already_AddRefed<Path>
SVGPolygonElement::BuildPath(PathBuilder* aBuilder)
{
  const SVGPointList &points = mPoints.GetAnimValue();

  if (points.IsEmpty()) {
    return nullptr;
  }

  aBuilder->MoveTo(points[0]);
  for (uint32_t i = 1; i < points.Length(); ++i) {
    aBuilder->LineTo(points[i]);
  }

  aBuilder->Close();

  return aBuilder->Finish();
}

} // namespace dom
} // namespace mozilla
