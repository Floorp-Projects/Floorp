/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGForeignObjectElement.h"

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/SVGDocument.h"
#include "mozilla/dom/SVGForeignObjectElementBinding.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "SVGGeometryProperty.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(ForeignObject)

namespace mozilla::dom {

JSObject* SVGForeignObjectElement::WrapNode(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return SVGForeignObjectElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::LengthInfo SVGForeignObjectElement::sLengthInfo[4] = {
    {nsGkAtoms::x, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::X},
    {nsGkAtoms::y, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::Y},
    {nsGkAtoms::width, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::X},
    {nsGkAtoms::height, 0, SVGLength_Binding::SVG_LENGTHTYPE_NUMBER,
     SVGContentUtils::Y},
};

//----------------------------------------------------------------------
// Implementation

SVGForeignObjectElement::SVGForeignObjectElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGGraphicsElement(std::move(aNodeInfo)) {}

namespace SVGT = SVGGeometryProperty::Tags;

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGForeignObjectElement)

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedLength> SVGForeignObjectElement::X() {
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGForeignObjectElement::Y() {
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGForeignObjectElement::Width() {
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGForeignObjectElement::Height() {
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// SVGElement methods

/* virtual */
gfxMatrix SVGForeignObjectElement::PrependLocalTransformsTo(
    const gfxMatrix& aMatrix, SVGTransformTypes aWhich) const {
  // 'transform' attribute:
  gfxMatrix fromUserSpace =
      SVGGraphicsElement::PrependLocalTransformsTo(aMatrix, aWhich);
  if (aWhich == eUserSpaceToParent) {
    return fromUserSpace;
  }
  // our 'x' and 'y' attributes:
  float x, y;

  if (!SVGGeometryProperty::ResolveAll<SVGT::X, SVGT::Y>(this, &x, &y)) {
    // This function might be called for element in display:none subtree
    // (e.g. getScreenCTM), we fall back to use SVG attributes.
    const_cast<SVGForeignObjectElement*>(this)->GetAnimatedLengthValues(
        &x, &y, nullptr);
  }

  gfxMatrix toUserSpace = gfxMatrix::Translation(x, y);
  if (aWhich == eChildToUserSpace) {
    return toUserSpace * aMatrix;
  }
  MOZ_ASSERT(aWhich == eAllTransforms, "Unknown TransformTypes");
  return toUserSpace * fromUserSpace;
}

/* virtual */
bool SVGForeignObjectElement::HasValidDimensions() const {
  float width, height;

  DebugOnly<bool> ok =
      SVGGeometryProperty::ResolveAll<SVGT::Width, SVGT::Height>(this, &width,
                                                                 &height);
  MOZ_ASSERT(ok, "SVGGeometryProperty::ResolveAll failed");
  return width > 0 && height > 0;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGForeignObjectElement::IsAttributeMapped(const nsAtom* name) const {
  return IsInLengthInfo(name, sLengthInfo) ||
         SVGGraphicsElement::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::LengthAttributesInfo SVGForeignObjectElement::GetLengthInfo() {
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

nsCSSPropertyID SVGForeignObjectElement::GetCSSPropertyIdForAttrEnum(
    uint8_t aAttrEnum) {
  switch (aAttrEnum) {
    case ATTR_X:
      return eCSSProperty_x;
    case ATTR_Y:
      return eCSSProperty_y;
    case ATTR_WIDTH:
      return eCSSProperty_width;
    case ATTR_HEIGHT:
      return eCSSProperty_height;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown attr enum");
      return eCSSProperty_UNKNOWN;
  }
}

}  // namespace mozilla::dom
