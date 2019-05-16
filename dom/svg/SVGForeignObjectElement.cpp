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

NS_IMPL_NS_NEW_SVG_ELEMENT(ForeignObject)

namespace mozilla {
namespace dom {

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
  const_cast<SVGForeignObjectElement*>(this)->GetAnimatedLengthValues(&x, &y,
                                                                      nullptr);
  gfxMatrix toUserSpace = gfxMatrix::Translation(x, y);
  if (aWhich == eChildToUserSpace) {
    return toUserSpace * aMatrix;
  }
  MOZ_ASSERT(aWhich == eAllTransforms, "Unknown TransformTypes");
  return toUserSpace * fromUserSpace;
}

/* virtual */
bool SVGForeignObjectElement::HasValidDimensions() const {
  return mLengthAttributes[ATTR_WIDTH].IsExplicitlySet() &&
         mLengthAttributes[ATTR_WIDTH].GetAnimValInSpecifiedUnits() > 0 &&
         mLengthAttributes[ATTR_HEIGHT].IsExplicitlySet() &&
         mLengthAttributes[ATTR_HEIGHT].GetAnimValInSpecifiedUnits() > 0;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGForeignObjectElement::IsAttributeMapped(const nsAtom* name) const {
  static const MappedAttributeEntry* const map[] = {sFEFloodMap,
                                                    sFiltersMap,
                                                    sFontSpecificationMap,
                                                    sGradientStopMap,
                                                    sLightingEffectsMap,
                                                    sMarkersMap,
                                                    sTextContentElementsMap,
                                                    sViewportsMap};

  return FindAttributeDependence(name, map) ||
         SVGGraphicsElement::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::LengthAttributesInfo SVGForeignObjectElement::GetLengthInfo() {
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

}  // namespace dom
}  // namespace mozilla
