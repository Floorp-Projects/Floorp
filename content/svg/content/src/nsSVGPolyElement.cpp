/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGPolyElement.h"
#include "DOMSVGPointList.h"
#include "gfxContext.h"
#include "mozilla/gfx/2D.h"
#include "SVGContentUtils.h"

using namespace mozilla;
using namespace mozilla::gfx;

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGPolyElement,nsSVGPolyElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGPolyElement,nsSVGPolyElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGPolyElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPolyElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGPolyElement::nsSVGPolyElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsSVGPolyElementBase(aNodeInfo)
{
}

already_AddRefed<DOMSVGPointList>
nsSVGPolyElement::Points()
{
  void *key = mPoints.GetBaseValKey();
  nsRefPtr<DOMSVGPointList> points = DOMSVGPointList::GetDOMWrapper(key, this, false);
  return points.forget();
}

already_AddRefed<DOMSVGPointList>
nsSVGPolyElement::AnimatedPoints()
{
  void *key = mPoints.GetAnimValKey();
  nsRefPtr<DOMSVGPointList> points = DOMSVGPointList::GetDOMWrapper(key, this, true);
  return points.forget();
}


//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGPolyElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sMarkersMap
  };
  
  return FindAttributeDependence(name, map) ||
    nsSVGPolyElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ bool
nsSVGPolyElement::HasValidDimensions() const
{
  return !mPoints.GetAnimValue().IsEmpty();
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

bool
nsSVGPolyElement::AttributeDefinesGeometry(const nsIAtom *aName)
{
  if (aName == nsGkAtoms::points)
    return true;

  return false;
}

void
nsSVGPolyElement::GetMarkPoints(nsTArray<nsSVGMark> *aMarks)
{
  const SVGPointList &points = mPoints.GetAnimValue();

  if (!points.Length())
    return;

  float px = points[0].mX, py = points[0].mY, prevAngle = 0.0;

  aMarks->AppendElement(nsSVGMark(px, py, 0, nsSVGMark::eStart));

  for (uint32_t i = 1; i < points.Length(); ++i) {
    float x = points[i].mX;
    float y = points[i].mY;
    float angle = atan2(y-py, x-px);

    // Vertex marker.
    if (i == 1) {
      aMarks->ElementAt(0).angle = angle;
    } else {
      aMarks->ElementAt(aMarks->Length() - 2).angle =
        SVGContentUtils::AngleBisect(prevAngle, angle);
    }

    aMarks->AppendElement(nsSVGMark(x, y, 0, nsSVGMark::eMid));

    prevAngle = angle;
    px = x;
    py = y;
  }

  aMarks->LastElement().angle = prevAngle;
  aMarks->LastElement().type = nsSVGMark::eEnd;
}

void
nsSVGPolyElement::ConstructPath(gfxContext *aCtx)
{
  const SVGPointList &points = mPoints.GetAnimValue();

  if (!points.Length())
    return;

  aCtx->MoveTo(points[0]);
  for (uint32_t i = 1; i < points.Length(); ++i) {
    aCtx->LineTo(points[i]);
  }
}

TemporaryRef<Path>
nsSVGPolyElement::BuildPath()
{
  const SVGPointList &points = mPoints.GetAnimValue();

  if (points.IsEmpty()) {
    return nullptr;
  }

  RefPtr<PathBuilder> pathBuilder = CreatePathBuilder();

  pathBuilder->MoveTo(points[0]);
  for (uint32_t i = 1; i < points.Length(); ++i) {
    pathBuilder->LineTo(points[i]);
  }

  return pathBuilder->Finish();
}
