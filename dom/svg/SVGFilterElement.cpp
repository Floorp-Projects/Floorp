/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFilterElement.h"

#include "nsGkAtoms.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/SVGFilterElementBinding.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/dom/SVGUnitTypesBinding.h"
#include "nsQueryObject.h"
#include "nsSVGUtils.h"
#include "SVGObserverUtils.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(Filter)

namespace mozilla {
namespace dom {

using namespace SVGUnitTypes_Binding;

JSObject* SVGFilterElement::WrapNode(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return SVGFilterElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::LengthInfo SVGFilterElement::sLengthInfo[4] = {
    {nsGkAtoms::x, -10, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::X},
    {nsGkAtoms::y, -10, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::Y},
    {nsGkAtoms::width, 120, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::X},
    {nsGkAtoms::height, 120, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::Y},
};

SVGElement::EnumInfo SVGFilterElement::sEnumInfo[2] = {
    {nsGkAtoms::filterUnits, sSVGUnitTypesMap, SVG_UNIT_TYPE_OBJECTBOUNDINGBOX},
    {nsGkAtoms::primitiveUnits, sSVGUnitTypesMap,
     SVG_UNIT_TYPE_USERSPACEONUSE}};

SVGElement::StringInfo SVGFilterElement::sStringInfo[2] = {
    {nsGkAtoms::href, kNameSpaceID_None, true},
    {nsGkAtoms::href, kNameSpaceID_XLink, true}};

//----------------------------------------------------------------------
// Implementation

SVGFilterElement::SVGFilterElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGFilterElementBase(std::move(aNodeInfo)) {}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFilterElement)

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedLength> SVGFilterElement::X() {
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGFilterElement::Y() {
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGFilterElement::Width() {
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGFilterElement::Height() {
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedEnumeration> SVGFilterElement::FilterUnits() {
  return mEnumAttributes[FILTERUNITS].ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedEnumeration> SVGFilterElement::PrimitiveUnits() {
  return mEnumAttributes[PRIMITIVEUNITS].ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedString> SVGFilterElement::Href() {
  return mStringAttributes[HREF].IsExplicitlySet()
             ? mStringAttributes[HREF].ToDOMAnimatedString(this)
             : mStringAttributes[XLINK_HREF].ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGFilterElement::IsAttributeMapped(const nsAtom* name) const {
  static const MappedAttributeEntry* const map[] = {
      sColorMap,        sFEFloodMap,
      sFiltersMap,      sFontSpecificationMap,
      sGradientStopMap, sLightingEffectsMap,
      sMarkersMap,      sTextContentElementsMap,
      sViewportsMap};
  return FindAttributeDependence(name, map) ||
         SVGFilterElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// SVGElement methods

/* virtual */
bool SVGFilterElement::HasValidDimensions() const {
  return (!mLengthAttributes[ATTR_WIDTH].IsExplicitlySet() ||
          mLengthAttributes[ATTR_WIDTH].GetAnimValInSpecifiedUnits() > 0) &&
         (!mLengthAttributes[ATTR_HEIGHT].IsExplicitlySet() ||
          mLengthAttributes[ATTR_HEIGHT].GetAnimValInSpecifiedUnits() > 0);
}

SVGElement::LengthAttributesInfo SVGFilterElement::GetLengthInfo() {
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

SVGElement::EnumAttributesInfo SVGFilterElement::GetEnumInfo() {
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo, ArrayLength(sEnumInfo));
}

SVGElement::StringAttributesInfo SVGFilterElement::GetStringInfo() {
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

}  // namespace dom
}  // namespace mozilla
