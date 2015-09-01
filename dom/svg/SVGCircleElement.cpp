/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGCircleElement.h"
#include "mozilla/gfx/2D.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/SVGCircleElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Circle)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGCircleElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGCircleElementBinding::Wrap(aCx, this, aGivenProto);
}

nsSVGElement::LengthInfo SVGCircleElement::sLengthInfo[3] =
{
  { &nsGkAtoms::cx, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::cy, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::r, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::XY }
};

//----------------------------------------------------------------------
// Implementation

SVGCircleElement::SVGCircleElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGCircleElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGCircleElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedLength>
SVGCircleElement::Cx()
{
  return mLengthAttributes[ATTR_CX].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGCircleElement::Cy()
{
  return mLengthAttributes[ATTR_CY].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGCircleElement::R()
{
  return mLengthAttributes[ATTR_R].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ bool
SVGCircleElement::HasValidDimensions() const
{
  return mLengthAttributes[ATTR_R].IsExplicitlySet() &&
         mLengthAttributes[ATTR_R].GetAnimValInSpecifiedUnits() > 0;
}

nsSVGElement::LengthAttributesInfo
SVGCircleElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// nsSVGPathGeometryElement methods

bool
SVGCircleElement::GetGeometryBounds(Rect* aBounds,
                                    const StrokeOptions& aStrokeOptions,
                                    const Matrix& aToBoundsSpace,
                                    const Matrix* aToNonScalingStrokeSpace)
{
  float x, y, r;
  GetAnimatedLengthValues(&x, &y, &r, nullptr);

  if (r <= 0.f) {
    // Rendering of the element is disabled
    *aBounds = Rect(aToBoundsSpace * Point(x, y), Size());
    return true;
  }

  if (aToBoundsSpace.IsRectilinear()) {
    // Optimize the case where we can treat the circle as a rectangle and
    // still get tight bounds.
    if (aStrokeOptions.mLineWidth > 0.f) {
      if (aToNonScalingStrokeSpace) {
        if (aToNonScalingStrokeSpace->IsRectilinear()) {
          Rect userBounds(x - r, y - r, 2 * r, 2 * r);
          SVGContentUtils::RectilinearGetStrokeBounds(
            userBounds, aToBoundsSpace, *aToNonScalingStrokeSpace,
            aStrokeOptions.mLineWidth, aBounds);
          return true;
        }
        return false;
      }
      r += aStrokeOptions.mLineWidth / 2.f;
    }
    Rect rect(x - r, y - r, 2 * r, 2 * r);
    *aBounds = aToBoundsSpace.TransformBounds(rect);
    return true;
  }

  return false;
}

already_AddRefed<Path>
SVGCircleElement::BuildPath(PathBuilder* aBuilder)
{
  float x, y, r;
  GetAnimatedLengthValues(&x, &y, &r, nullptr);

  if (r <= 0.0f) {
    return nullptr;
  }

  aBuilder->Arc(Point(x, y), r, 0, Float(2*M_PI));

  return aBuilder->Finish();
}

} // namespace dom
} // namespace mozilla
