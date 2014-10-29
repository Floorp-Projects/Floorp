/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
SVGCircleElement::WrapNode(JSContext *aCx)
{
  return SVGCircleElementBinding::Wrap(aCx, this);
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
SVGCircleElement::GetGeometryBounds(Rect* aBounds, Float aStrokeWidth,
                                    const Matrix& aTransform)
{
  float x, y, r;
  GetAnimatedLengthValues(&x, &y, &r, nullptr);

  if (r <= 0.f) {
    // Rendering of the element is disabled
    aBounds->MoveTo(x, y);
    aBounds->SetEmpty();
    return true;
  }

  if (aTransform.IsRectilinear()) {
    // Optimize the case where we can treat the circle as a rectangle and
    // still get tight bounds.
    if (aStrokeWidth > 0.f) {
      r += aStrokeWidth / 2.f;
    }
    Rect rect(x - r, y - r, 2 * r, 2 * r);
    *aBounds = aTransform.TransformBounds(rect);
    return true;
  }

  return false;
}

TemporaryRef<Path>
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
