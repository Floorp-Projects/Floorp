/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ComputedStyle.h"
#include "mozilla/dom/SVGEllipseElement.h"
#include "mozilla/dom/SVGEllipseElementBinding.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/RefPtr.h"
#include "SVGGeometryProperty.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(Ellipse)

using namespace mozilla::gfx;

namespace mozilla::dom {

JSObject* SVGEllipseElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return SVGEllipseElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::LengthInfo SVGEllipseElement::sLengthInfo[4] = {
    {nsGkAtoms::cx, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::X},
    {nsGkAtoms::cy, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::Y},
    {nsGkAtoms::rx, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::X},
    {nsGkAtoms::ry, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::Y},
};

//----------------------------------------------------------------------
// Implementation

SVGEllipseElement::SVGEllipseElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGEllipseElementBase(std::move(aNodeInfo)) {}

bool SVGEllipseElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  return IsInLengthInfo(aAttribute, sLengthInfo) ||
         SVGEllipseElementBase::IsAttributeMapped(aAttribute);
}

namespace SVGT = SVGGeometryProperty::Tags;

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGEllipseElement)

//----------------------------------------------------------------------
// nsIDOMSVGEllipseElement methods

already_AddRefed<DOMSVGAnimatedLength> SVGEllipseElement::Cx() {
  return mLengthAttributes[CX].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGEllipseElement::Cy() {
  return mLengthAttributes[CY].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGEllipseElement::Rx() {
  return mLengthAttributes[RX].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGEllipseElement::Ry() {
  return mLengthAttributes[RY].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// SVGElement methods

/* virtual */
bool SVGEllipseElement::HasValidDimensions() const {
  float rx, ry;

  if (SVGGeometryProperty::ResolveAll<SVGT::Rx, SVGT::Ry>(this, &rx, &ry)) {
    return rx > 0 && ry > 0;
  }
  // This function might be called for an element in display:none subtree
  // (e.g. SMIL animateMotion), we fall back to use SVG attributes.
  bool hasRx = mLengthAttributes[RX].IsExplicitlySet();
  bool hasRy = mLengthAttributes[RY].IsExplicitlySet();
  if ((hasRx && mLengthAttributes[RX].GetAnimValInSpecifiedUnits() <= 0) ||
      (hasRy && mLengthAttributes[RY].GetAnimValInSpecifiedUnits() <= 0)) {
    return false;
  }
  return hasRx || hasRy;
}

SVGElement::LengthAttributesInfo SVGEllipseElement::GetLengthInfo() {
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// SVGGeometryElement methods

bool SVGEllipseElement::GetGeometryBounds(
    Rect* aBounds, const StrokeOptions& aStrokeOptions,
    const Matrix& aToBoundsSpace, const Matrix* aToNonScalingStrokeSpace) {
  float x, y, rx, ry;

  DebugOnly<bool> ok =
      SVGGeometryProperty::ResolveAll<SVGT::Cx, SVGT::Cy, SVGT::Rx, SVGT::Ry>(
          this, &x, &y, &rx, &ry);
  MOZ_ASSERT(ok, "SVGGeometryProperty::ResolveAll failed");

  if (rx <= 0.f || ry <= 0.f) {
    // Rendering of the element is disabled
    *aBounds = Rect(aToBoundsSpace.TransformPoint(Point(x, y)), Size());
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

already_AddRefed<Path> SVGEllipseElement::BuildPath(PathBuilder* aBuilder) {
  float x, y, rx, ry;

  if (!SVGGeometryProperty::ResolveAll<SVGT::Cx, SVGT::Cy, SVGT::Rx, SVGT::Ry>(
          this, &x, &y, &rx, &ry)) {
    // This function might be called for element in display:none subtree
    // (e.g. getTotalLength), we fall back to use SVG attributes.
    GetAnimatedLengthValues(&x, &y, &rx, &ry, nullptr);
  }

  if (rx <= 0.0f || ry <= 0.0f) {
    return nullptr;
  }

  EllipseToBezier(aBuilder, Point(x, y), Size(rx, ry));

  return aBuilder->Finish();
}

bool SVGEllipseElement::IsLengthChangedViaCSS(const ComputedStyle& aNewStyle,
                                              const ComputedStyle& aOldStyle) {
  const auto& newSVGReset = *aNewStyle.StyleSVGReset();
  const auto& oldSVGReset = *aOldStyle.StyleSVGReset();
  return newSVGReset.mCx != oldSVGReset.mCx ||
         newSVGReset.mCy != oldSVGReset.mCy ||
         newSVGReset.mRx != oldSVGReset.mRx ||
         newSVGReset.mRy != oldSVGReset.mRy;
}

nsCSSPropertyID SVGEllipseElement::GetCSSPropertyIdForAttrEnum(
    uint8_t aAttrEnum) {
  switch (aAttrEnum) {
    case CX:
      return eCSSProperty_cx;
    case CY:
      return eCSSProperty_cy;
    case RX:
      return eCSSProperty_rx;
    case RY:
      return eCSSProperty_ry;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown attr enum");
      return eCSSProperty_UNKNOWN;
  }
}

}  // namespace mozilla::dom
