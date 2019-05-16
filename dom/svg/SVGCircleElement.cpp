/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ComputedStyle.h"
#include "mozilla/dom/SVGCircleElement.h"
#include "mozilla/gfx/2D.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/SVGCircleElementBinding.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "SVGGeometryProperty.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(Circle)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject* SVGCircleElement::WrapNode(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return SVGCircleElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::LengthInfo SVGCircleElement::sLengthInfo[3] = {
    {nsGkAtoms::cx, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::X},
    {nsGkAtoms::cy, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::Y},
    {nsGkAtoms::r, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::XY}};

//----------------------------------------------------------------------
// Implementation

SVGCircleElement::SVGCircleElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGCircleElementBase(std::move(aNodeInfo)) {}

bool SVGCircleElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  return IsInLengthInfo(aAttribute, sLengthInfo) ||
         SVGCircleElementBase::IsAttributeMapped(aAttribute);
}

namespace SVGT = SVGGeometryProperty::Tags;

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGCircleElement)

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedLength> SVGCircleElement::Cx() {
  return mLengthAttributes[ATTR_CX].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGCircleElement::Cy() {
  return mLengthAttributes[ATTR_CY].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGCircleElement::R() {
  return mLengthAttributes[ATTR_R].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// SVGElement methods

/* virtual */
bool SVGCircleElement::HasValidDimensions() const {
  float r;

  MOZ_ASSERT(GetPrimaryFrame());
  SVGGeometryProperty::ResolveAll<SVGT::R>(this, &r);
  return r > 0;
}

SVGElement::LengthAttributesInfo SVGCircleElement::GetLengthInfo() {
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// SVGGeometryElement methods

bool SVGCircleElement::GetGeometryBounds(
    Rect* aBounds, const StrokeOptions& aStrokeOptions,
    const Matrix& aToBoundsSpace, const Matrix* aToNonScalingStrokeSpace) {
  float x, y, r;

  MOZ_ASSERT(GetPrimaryFrame());
  SVGGeometryProperty::ResolveAll<SVGT::Cx, SVGT::Cy, SVGT::R>(this, &x, &y,
                                                               &r);

  if (r <= 0.f) {
    // Rendering of the element is disabled
    *aBounds = Rect(aToBoundsSpace.TransformPoint(Point(x, y)), Size());
    return true;
  }

  if (aToBoundsSpace.IsRectilinear()) {
    // Optimize the case where we can treat the circle as a rectangle and
    // still get tight bounds.
    if (aStrokeOptions.mLineWidth > 0.f) {
      if (aToNonScalingStrokeSpace) {
        if (aToNonScalingStrokeSpace->IsRectilinear()) {
          MOZ_ASSERT(!aToNonScalingStrokeSpace->IsSingular());
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

already_AddRefed<Path> SVGCircleElement::BuildPath(PathBuilder* aBuilder) {
  float x, y, r;
  MOZ_ASSERT(GetPrimaryFrame());
  SVGGeometryProperty::ResolveAll<SVGT::Cx, SVGT::Cy, SVGT::R>(this, &x, &y,
                                                               &r);

  if (r <= 0.0f) {
    return nullptr;
  }

  aBuilder->Arc(Point(x, y), r, 0, Float(2 * M_PI));

  return aBuilder->Finish();
}

bool SVGCircleElement::IsLengthChangedViaCSS(const ComputedStyle& aNewStyle,
                                             const ComputedStyle& aOldStyle) {
  auto *newSVGReset = aNewStyle.StyleSVGReset(),
       *oldSVGReset = aOldStyle.StyleSVGReset();

  return newSVGReset->mCx != oldSVGReset->mCx ||
         newSVGReset->mCy != oldSVGReset->mCy ||
         newSVGReset->mR != oldSVGReset->mR;
}

nsCSSPropertyID SVGCircleElement::GetCSSPropertyIdForAttrEnum(
    uint8_t aAttrEnum) {
  switch (aAttrEnum) {
    case ATTR_CX:
      return eCSSProperty_cx;
    case ATTR_CY:
      return eCSSProperty_cy;
    case ATTR_R:
      return eCSSProperty_r;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown attr enum");
      return eCSSProperty_UNKNOWN;
  }
}

}  // namespace dom
}  // namespace mozilla
