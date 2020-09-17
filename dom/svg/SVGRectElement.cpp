/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGRectElement.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/dom/SVGRectElementBinding.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/PathHelpers.h"
#include "nsGkAtoms.h"
#include "SVGGeometryProperty.h"
#include <algorithm>

NS_IMPL_NS_NEW_SVG_ELEMENT(Rect)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

class DOMSVGAnimatedLength;

JSObject* SVGRectElement::WrapNode(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return SVGRectElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::LengthInfo SVGRectElement::sLengthInfo[6] = {
    {nsGkAtoms::x, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::X},
    {nsGkAtoms::y, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::Y},
    {nsGkAtoms::width, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::X},
    {nsGkAtoms::height, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::Y},
    {nsGkAtoms::rx, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::X},
    {nsGkAtoms::ry, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::Y}};

//----------------------------------------------------------------------
// Implementation

SVGRectElement::SVGRectElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGRectElementBase(std::move(aNodeInfo)) {}

bool SVGRectElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  return IsInLengthInfo(aAttribute, sLengthInfo) ||
         SVGRectElementBase::IsAttributeMapped(aAttribute);
}

namespace SVGT = SVGGeometryProperty::Tags;

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGRectElement)

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedLength> SVGRectElement::X() {
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGRectElement::Y() {
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGRectElement::Width() {
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGRectElement::Height() {
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGRectElement::Rx() {
  return mLengthAttributes[ATTR_RX].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGRectElement::Ry() {
  return mLengthAttributes[ATTR_RY].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// SVGElement methods

/* virtual */
bool SVGRectElement::HasValidDimensions() const {
  float width, height;

  MOZ_ASSERT(GetPrimaryFrame());
  SVGGeometryProperty::ResolveAll<SVGT::Width, SVGT::Height>(this, &width,
                                                             &height);

  return width > 0 && height > 0;
}

SVGElement::LengthAttributesInfo SVGRectElement::GetLengthInfo() {
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//----------------------------------------------------------------------
// SVGGeometryElement methods

bool SVGRectElement::GetGeometryBounds(Rect* aBounds,
                                       const StrokeOptions& aStrokeOptions,
                                       const Matrix& aToBoundsSpace,
                                       const Matrix* aToNonScalingStrokeSpace) {
  Rect rect;
  Float rx, ry;

  MOZ_ASSERT(GetPrimaryFrame());
  SVGGeometryProperty::ResolveAll<SVGT::X, SVGT::Y, SVGT::Width, SVGT::Height,
                                  SVGT::Rx, SVGT::Ry>(
      this, &rect.x, &rect.y, &rect.width, &rect.height, &rx, &ry);

  if (rect.IsEmpty()) {
    // Rendering of the element disabled
    rect.SetEmpty();  // Make sure width/height are zero and not negative
    // We still want the x/y position from 'rect'
    *aBounds = aToBoundsSpace.TransformBounds(rect);
    return true;
  }

  if (!aToBoundsSpace.IsRectilinear()) {
    // We can't ignore the radii in this case if we want tight bounds
    rx = std::max(rx, 0.0f);
    ry = std::max(ry, 0.0f);

    if (rx != 0 || ry != 0) {
      return false;
    }
  }

  if (aStrokeOptions.mLineWidth > 0.f) {
    if (aToNonScalingStrokeSpace) {
      if (aToNonScalingStrokeSpace->IsRectilinear()) {
        MOZ_ASSERT(!aToNonScalingStrokeSpace->IsSingular());
        rect = aToNonScalingStrokeSpace->TransformBounds(rect);
        // Note that, in principle, an author could cause the corners of the
        // rect to be beveled by specifying stroke-linejoin or setting
        // stroke-miterlimit to be less than sqrt(2). In that very unlikely
        // event the bounds that we calculate here may be too big if
        // aToBoundsSpace is non-rectilinear. This is likely to be so rare it's
        // not worth handling though.
        rect.Inflate(aStrokeOptions.mLineWidth / 2.f);
        Matrix nonScalingToBounds =
            aToNonScalingStrokeSpace->Inverse() * aToBoundsSpace;
        *aBounds = nonScalingToBounds.TransformBounds(rect);
        return true;
      }
      return false;
    }
    // The "beveled" comment above applies here too
    rect.Inflate(aStrokeOptions.mLineWidth / 2.f);
  }

  *aBounds = aToBoundsSpace.TransformBounds(rect);
  return true;
}

void SVGRectElement::GetAsSimplePath(SimplePath* aSimplePath) {
  float x, y, width, height, rx, ry;

  SVGGeometryProperty::ResolveAllAllowFallback<
      SVGT::X, SVGT::Y, SVGT::Width, SVGT::Height, SVGT::Rx, SVGT::Ry>(
      this, &x, &y, &width, &height, &rx, &ry);

  if (width <= 0 || height <= 0) {
    aSimplePath->Reset();
    return;
  }

  rx = std::max(rx, 0.0f);
  ry = std::max(ry, 0.0f);

  if (rx != 0 || ry != 0) {
    aSimplePath->Reset();
    return;
  }

  aSimplePath->SetRect(x, y, width, height);
}

already_AddRefed<Path> SVGRectElement::BuildPath(PathBuilder* aBuilder) {
  float x, y, width, height, rx, ry;

  if (!SVGGeometryProperty::ResolveAllAllowFallback<
          SVGT::X, SVGT::Y, SVGT::Width, SVGT::Height, SVGT::Rx, SVGT::Ry>(
          this, &x, &y, &width, &height, &rx, &ry)) {
    // This function might be called for element in display:none subtree
    // (e.g. getTotalLength), we fall back to use SVG attributes.
    GetAnimatedLengthValues(&x, &y, &width, &height, &rx, &ry, nullptr);
    // If either the 'rx' or the 'ry' attribute isn't set, then we have to
    // set it to the value of the other:
    bool hasRx = mLengthAttributes[ATTR_RX].IsExplicitlySet();
    bool hasRy = mLengthAttributes[ATTR_RY].IsExplicitlySet();
    if (hasRx && !hasRy) {
      ry = rx;
    } else if (hasRy && !hasRx) {
      rx = ry;
    }
  }

  if (width <= 0 || height <= 0) {
    return nullptr;
  }

  rx = std::max(rx, 0.0f);
  ry = std::max(ry, 0.0f);

  if (rx == 0 && ry == 0) {
    // Optimization for the no rounded corners case.
    Rect r(x, y, width, height);
    aBuilder->MoveTo(r.TopLeft());
    aBuilder->LineTo(r.TopRight());
    aBuilder->LineTo(r.BottomRight());
    aBuilder->LineTo(r.BottomLeft());
    aBuilder->Close();
  } else {
    // Clamp rx and ry to half the rect's width and height respectively:
    rx = std::min(rx, width / 2);
    ry = std::min(ry, height / 2);

    RectCornerRadii radii(rx, ry);
    AppendRoundedRectToPath(aBuilder, Rect(x, y, width, height), radii);
  }

  return aBuilder->Finish();
}

bool SVGRectElement::IsLengthChangedViaCSS(const ComputedStyle& aNewStyle,
                                           const ComputedStyle& aOldStyle) {
  auto *newSVGReset = aNewStyle.StyleSVGReset(),
       *oldSVGReset = aOldStyle.StyleSVGReset();
  auto *newPosition = aNewStyle.StylePosition(),
       *oldPosition = aOldStyle.StylePosition();

  return newSVGReset->mX != oldSVGReset->mX ||
         newSVGReset->mY != oldSVGReset->mY ||
         newPosition->mWidth != oldPosition->mWidth ||
         newPosition->mHeight != oldPosition->mHeight ||
         newSVGReset->mRx != oldSVGReset->mRx ||
         newSVGReset->mRy != oldSVGReset->mRy;
}

nsCSSPropertyID SVGRectElement::GetCSSPropertyIdForAttrEnum(uint8_t aAttrEnum) {
  switch (aAttrEnum) {
    case ATTR_X:
      return eCSSProperty_x;
    case ATTR_Y:
      return eCSSProperty_y;
    case ATTR_WIDTH:
      return eCSSProperty_width;
    case ATTR_HEIGHT:
      return eCSSProperty_height;
    case ATTR_RX:
      return eCSSProperty_rx;
    case ATTR_RY:
      return eCSSProperty_ry;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown attr enum");
      return eCSSProperty_UNKNOWN;
  }
}

}  // namespace dom
}  // namespace mozilla
