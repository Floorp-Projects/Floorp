/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGMaskElement.h"

#include "nsGkAtoms.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/dom/SVGMaskElementBinding.h"
#include "mozilla/dom/SVGUnitTypesBinding.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(Mask)

namespace mozilla {
namespace dom {

using namespace SVGUnitTypes_Binding;

JSObject* SVGMaskElement::WrapNode(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return SVGMaskElement_Binding::Wrap(aCx, this, aGivenProto);
}

//--------------------- Masks ------------------------

SVGElement::LengthInfo SVGMaskElement::sLengthInfo[4] = {
    {nsGkAtoms::x, -10, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::X},
    {nsGkAtoms::y, -10, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::Y},
    {nsGkAtoms::width, 120, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::X},
    {nsGkAtoms::height, 120, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::Y},
};

SVGElement::EnumInfo SVGMaskElement::sEnumInfo[2] = {
    {nsGkAtoms::maskUnits, sSVGUnitTypesMap, SVG_UNIT_TYPE_OBJECTBOUNDINGBOX},
    {nsGkAtoms::maskContentUnits, sSVGUnitTypesMap,
     SVG_UNIT_TYPE_USERSPACEONUSE}};

//----------------------------------------------------------------------
// Implementation

SVGMaskElement::SVGMaskElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGMaskElementBase(std::move(aNodeInfo)) {}

//----------------------------------------------------------------------
// nsINode method

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGMaskElement)

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedEnumeration> SVGMaskElement::MaskUnits() {
  return mEnumAttributes[MASKUNITS].ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedEnumeration> SVGMaskElement::MaskContentUnits() {
  return mEnumAttributes[MASKCONTENTUNITS].ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGMaskElement::X() {
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGMaskElement::Y() {
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGMaskElement::Width() {
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGMaskElement::Height() {
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// SVGElement methods

/* virtual */
bool SVGMaskElement::HasValidDimensions() const {
  return (!mLengthAttributes[ATTR_WIDTH].IsExplicitlySet() ||
          mLengthAttributes[ATTR_WIDTH].GetAnimValInSpecifiedUnits() > 0) &&
         (!mLengthAttributes[ATTR_HEIGHT].IsExplicitlySet() ||
          mLengthAttributes[ATTR_HEIGHT].GetAnimValInSpecifiedUnits() > 0);
}

SVGElement::LengthAttributesInfo SVGMaskElement::GetLengthInfo() {
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

SVGElement::EnumAttributesInfo SVGMaskElement::GetEnumInfo() {
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo, ArrayLength(sEnumInfo));
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGMaskElement::IsAttributeMapped(const nsAtom* name) const {
  static const MappedAttributeEntry* const map[] = {sColorMap,
                                                    sFEFloodMap,
                                                    sFillStrokeMap,
                                                    sFiltersMap,
                                                    sFontSpecificationMap,
                                                    sGradientStopMap,
                                                    sGraphicsMap,
                                                    sMarkersMap,
                                                    sMaskMap,
                                                    sTextContentElementsMap,
                                                    sViewportsMap};

  return FindAttributeDependence(name, map) ||
         SVGMaskElementBase::IsAttributeMapped(name);
}

}  // namespace dom
}  // namespace mozilla
