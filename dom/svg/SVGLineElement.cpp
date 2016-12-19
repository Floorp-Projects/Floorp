/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGLineElement.h"
#include "mozilla/dom/SVGLineElementBinding.h"
#include "mozilla/gfx/2D.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Line)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGLineElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGLineElementBinding::Wrap(aCx, this, aGivenProto);
}

nsSVGElement::LengthInfo SVGLineElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x1, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::y1, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::x2, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::y2, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
};

//----------------------------------------------------------------------
// Implementation

SVGLineElement::SVGLineElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGLineElementBase(aNodeInfo)
{
}

void
SVGLineElement::MaybeAdjustForZeroLength(float aX1, float aY1,
                                         float& aX2, float aY2)
{
  if (aX1 == aX2 && aY1 == aY2) {
    SVGContentUtils::AutoStrokeOptions strokeOptions;
    SVGContentUtils::GetStrokeOptions(&strokeOptions, this, nullptr, nullptr,
                                      SVGContentUtils::eIgnoreStrokeDashing);

    if (strokeOptions.mLineCap != CapStyle::BUTT) {
      float tinyLength =
        strokeOptions.mLineWidth / SVG_ZERO_LENGTH_PATH_FIX_FACTOR;
      aX2 += tinyLength;
    }
  }
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGLineElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedLength>
SVGLineElement::X1()
{
  return mLengthAttributes[ATTR_X1].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGLineElement::Y1()
{
  return mLengthAttributes[ATTR_Y1].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGLineElement::X2()
{
  return mLengthAttributes[ATTR_X2].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGLineElement::Y2()
{
  return mLengthAttributes[ATTR_Y2].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGLineElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sMarkersMap
  };

  return FindAttributeDependence(name, map) ||
    SVGLineElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthAttributesInfo
SVGLineElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// SVGGeometryElement methods

void
SVGLineElement::GetMarkPoints(nsTArray<nsSVGMark> *aMarks) {
  float x1, y1, x2, y2;

  GetAnimatedLengthValues(&x1, &y1, &x2, &y2, nullptr);

  float angle = atan2(y2 - y1, x2 - x1);

  aMarks->AppendElement(nsSVGMark(x1, y1, angle, nsSVGMark::eStart));
  aMarks->AppendElement(nsSVGMark(x2, y2, angle, nsSVGMark::eEnd));
}

void
SVGLineElement::GetAsSimplePath(SimplePath* aSimplePath)
{
  float x1, y1, x2, y2;
  GetAnimatedLengthValues(&x1, &y1, &x2, &y2, nullptr);

  MaybeAdjustForZeroLength(x1, y1, x2, y2);
  aSimplePath->SetLine(x1, y1, x2, y2);
}

already_AddRefed<Path>
SVGLineElement::BuildPath(PathBuilder* aBuilder)
{
  float x1, y1, x2, y2;
  GetAnimatedLengthValues(&x1, &y1, &x2, &y2, nullptr);

  MaybeAdjustForZeroLength(x1, y1, x2, y2);
  aBuilder->MoveTo(Point(x1, y1));
  aBuilder->LineTo(Point(x2, y2));

  return aBuilder->Finish();
}

bool
SVGLineElement::GetGeometryBounds(Rect* aBounds,
                                  const StrokeOptions& aStrokeOptions,
                                  const Matrix& aToBoundsSpace,
                                  const Matrix* aToNonScalingStrokeSpace)
{
  float x1, y1, x2, y2;
  GetAnimatedLengthValues(&x1, &y1, &x2, &y2, nullptr);

  if (aStrokeOptions.mLineWidth <= 0) {
    *aBounds = Rect(aToBoundsSpace.TransformPoint(Point(x1, y1)), Size());
    aBounds->ExpandToEnclose(aToBoundsSpace.TransformPoint(Point(x2, y2)));
    return true;
  }

  // transform from non-scaling-stroke space to the space in which we compute
  // bounds
  Matrix nonScalingToBounds;
  if (aToNonScalingStrokeSpace) {
    MOZ_ASSERT(!aToNonScalingStrokeSpace->IsSingular());
    Matrix nonScalingToUser = aToNonScalingStrokeSpace->Inverse();
    nonScalingToBounds = nonScalingToUser * aToBoundsSpace;
  }

  if (aStrokeOptions.mLineCap == CapStyle::ROUND) {
    if (!aToBoundsSpace.IsRectilinear() ||
        (aToNonScalingStrokeSpace &&
         !aToNonScalingStrokeSpace->IsRectilinear())) {
      // TODO: handle this case.
      return false;
    }
    Rect bounds(Point(x1, y1), Size());
    bounds.ExpandToEnclose(Point(x2, y2));
    if (aToNonScalingStrokeSpace) {
      bounds = aToNonScalingStrokeSpace->TransformBounds(bounds);
      bounds.Inflate(aStrokeOptions.mLineWidth / 2.f);
      *aBounds = nonScalingToBounds.TransformBounds(bounds);
    } else {
      bounds.Inflate(aStrokeOptions.mLineWidth / 2.f);
      *aBounds = aToBoundsSpace.TransformBounds(bounds);
    }
    return true;
  }

  // Handle butt and square linecap, normal and non-scaling stroke cases
  // together: start with endpoints (x1, y1), (x2, y2) in the stroke space,
  // compute the four corners of the stroked line, transform the corners to
  // bounds space, and compute bounds there.

  if (aToNonScalingStrokeSpace) {
    Point nonScalingSpaceP1, nonScalingSpaceP2;
    nonScalingSpaceP1 = aToNonScalingStrokeSpace->TransformPoint(Point(x1, y1));
    nonScalingSpaceP2 = aToNonScalingStrokeSpace->TransformPoint(Point(x2, y2));
    x1 = nonScalingSpaceP1.x;
    y1 = nonScalingSpaceP1.y;
    x2 = nonScalingSpaceP2.x;
    y2 = nonScalingSpaceP2.y;
  }

  Float length = Float(NS_hypot(x2 - x1, y2 - y1));
  Float xDelta;
  Float yDelta;
  Point points[4];

  if (aStrokeOptions.mLineCap == CapStyle::BUTT) {
    if (length == 0.f) {
      xDelta = yDelta = 0.f;
    } else {
      Float ratio = aStrokeOptions.mLineWidth / 2.f / length;
      xDelta = ratio * (y2 - y1);
      yDelta = ratio * (x2 - x1);
    }
    points[0] = Point(x1 - xDelta, y1 + yDelta);
    points[1] = Point(x1 + xDelta, y1 - yDelta);
    points[2] = Point(x2 + xDelta, y2 - yDelta);
    points[3] = Point(x2 - xDelta, y2 + yDelta);
  } else {
    MOZ_ASSERT(aStrokeOptions.mLineCap == CapStyle::SQUARE);
    if (length == 0.f) {
      xDelta = yDelta = aStrokeOptions.mLineWidth / 2.f;
      points[0] = Point(x1 - xDelta, y1 + yDelta);
      points[1] = Point(x1 - xDelta, y1 - yDelta);
      points[2] = Point(x1 + xDelta, y1 - yDelta);
      points[3] = Point(x1 + xDelta, y1 + yDelta);
    } else {
      Float ratio = aStrokeOptions.mLineWidth / 2.f / length;
      yDelta = ratio * (x2 - x1);
      xDelta = ratio * (y2 - y1);
      points[0] = Point(x1 - yDelta - xDelta, y1 - xDelta + yDelta);
      points[1] = Point(x1 - yDelta + xDelta, y1 - xDelta - yDelta);
      points[2] = Point(x2 + yDelta + xDelta, y2 + xDelta - yDelta);
      points[3] = Point(x2 + yDelta - xDelta, y2 + xDelta + yDelta);
    }
  }

  const Matrix& toBoundsSpace = aToNonScalingStrokeSpace ?
    nonScalingToBounds : aToBoundsSpace;

  *aBounds = Rect(toBoundsSpace.TransformPoint(points[0]), Size());
  for (uint32_t i = 1; i < 4; ++i) {
    aBounds->ExpandToEnclose(toBoundsSpace.TransformPoint(points[i]));
  }

  return true;
}

} // namespace dom
} // namespace mozilla
