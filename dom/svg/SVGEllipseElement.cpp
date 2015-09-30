/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGEllipseElement.h"
#include "mozilla/dom/SVGEllipseElementBinding.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/RefPtr.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Ellipse)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGEllipseElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGEllipseElementBinding::Wrap(aCx, this, aGivenProto);
}

nsSVGElement::LengthInfo SVGEllipseElement::sLengthInfo[4] =
{
  { &nsGkAtoms::cx, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::cy, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::rx, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::ry, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
};

//----------------------------------------------------------------------
// Implementation

SVGEllipseElement::SVGEllipseElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGEllipseElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGEllipseElement)

//----------------------------------------------------------------------
// nsIDOMSVGEllipseElement methods

already_AddRefed<SVGAnimatedLength>
SVGEllipseElement::Cx()
{
  return mLengthAttributes[CX].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGEllipseElement::Cy()
{
  return mLengthAttributes[CY].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGEllipseElement::Rx()
{
  return mLengthAttributes[RX].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGEllipseElement::Ry()
{
  return mLengthAttributes[RY].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ bool
SVGEllipseElement::HasValidDimensions() const
{
  return mLengthAttributes[RX].IsExplicitlySet() &&
         mLengthAttributes[RX].GetAnimValInSpecifiedUnits() > 0 &&
         mLengthAttributes[RY].IsExplicitlySet() &&
         mLengthAttributes[RY].GetAnimValInSpecifiedUnits() > 0;
}

nsSVGElement::LengthAttributesInfo
SVGEllipseElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

bool
SVGEllipseElement::GetGeometryBounds(Rect* aBounds,
                                     const StrokeOptions& aStrokeOptions,
                                     const Matrix& aToBoundsSpace,
                                     const Matrix* aToNonScalingStrokeSpace)
{
  float x, y, rx, ry;
  GetAnimatedLengthValues(&x, &y, &rx, &ry, nullptr);

  if (rx <= 0.f || ry <= 0.f) {
    // Rendering of the element is disabled
    *aBounds = Rect(aToBoundsSpace * Point(x, y), Size());
    return true;
  }

  if (aToBoundsSpace.IsRectilinear()) {
    // Optimize the case where we can treat the ellipse as a rectangle and
    // still get tight bounds.
    if (aStrokeOptions.mLineWidth > 0.f) {
      if (aToNonScalingStrokeSpace) {
        if (aToNonScalingStrokeSpace->IsRectilinear()) {
          MOZ_ASSERT(!aToNonScalingStrokeSpace->IsSingular());
          Rect userBounds(x - rx, y - ry, 2 * rx, 2 * ry);
          SVGContentUtils::RectilinearGetStrokeBounds(
            userBounds, aToBoundsSpace, *aToNonScalingStrokeSpace,
            aStrokeOptions.mLineWidth, aBounds);
          return true;
        }
        return false;
      }
      rx += aStrokeOptions.mLineWidth / 2.f;
      ry += aStrokeOptions.mLineWidth / 2.f;
    }
    Rect rect(x - rx, y - ry, 2 * rx, 2 * ry);
    *aBounds = aToBoundsSpace.TransformBounds(rect);
    return true;
  }

  return false;
}

already_AddRefed<Path>
SVGEllipseElement::BuildPath(PathBuilder* aBuilder)
{
  float x, y, rx, ry;
  GetAnimatedLengthValues(&x, &y, &rx, &ry, nullptr);

  if (rx <= 0.0f || ry <= 0.0f) {
    return nullptr;
  }

  EllipseToBezier(aBuilder, Point(x, y), Size(rx, ry));

  return aBuilder->Finish();
}

} // namespace dom
} // namespace mozilla
