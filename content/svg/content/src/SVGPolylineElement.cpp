/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGPolylineElement.h"
#include "mozilla/dom/SVGPolylineElementBinding.h"
#include "mozilla/gfx/2D.h"

using namespace mozilla;
using namespace mozilla::gfx;

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Polyline)

namespace mozilla {
namespace dom {

JSObject*
SVGPolylineElement::WrapNode(JSContext *aCx)
{
  return SVGPolylineElementBinding::Wrap(aCx, this);
}

//----------------------------------------------------------------------
// Implementation

SVGPolylineElement::SVGPolylineElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGPolylineElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGPolylineElement)

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

TemporaryRef<Path>
SVGPolylineElement::BuildPath(PathBuilder* aBuilder)
{
  const SVGPointList &points = mPoints.GetAnimValue();

  if (points.IsEmpty()) {
    return nullptr;
  }

  aBuilder->MoveTo(points[0]);
  for (uint32_t i = 1; i < points.Length(); ++i) {
    aBuilder->LineTo(points[i]);
  }

  return aBuilder->Finish();
}

} // namespace dom
} // namespace mozilla
