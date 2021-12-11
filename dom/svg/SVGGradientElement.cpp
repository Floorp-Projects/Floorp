/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGGradientElement.h"

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/dom/SVGGradientElementBinding.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/dom/SVGLinearGradientElementBinding.h"
#include "mozilla/dom/SVGRadialGradientElementBinding.h"
#include "mozilla/dom/SVGUnitTypesBinding.h"
#include "DOMSVGAnimatedTransformList.h"
#include "nsGkAtoms.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(LinearGradient)
NS_IMPL_NS_NEW_SVG_ELEMENT(RadialGradient)

namespace mozilla {
namespace dom {

using namespace SVGGradientElement_Binding;
using namespace SVGUnitTypes_Binding;

//--------------------- Gradients------------------------

SVGEnumMapping SVGGradientElement::sSpreadMethodMap[] = {
    {nsGkAtoms::pad, SVG_SPREADMETHOD_PAD},
    {nsGkAtoms::reflect, SVG_SPREADMETHOD_REFLECT},
    {nsGkAtoms::repeat, SVG_SPREADMETHOD_REPEAT},
    {nullptr, 0}};

SVGElement::EnumInfo SVGGradientElement::sEnumInfo[2] = {
    {nsGkAtoms::gradientUnits, sSVGUnitTypesMap,
     SVG_UNIT_TYPE_OBJECTBOUNDINGBOX},
    {nsGkAtoms::spreadMethod, sSpreadMethodMap, SVG_SPREADMETHOD_PAD}};

SVGElement::StringInfo SVGGradientElement::sStringInfo[2] = {
    {nsGkAtoms::href, kNameSpaceID_None, true},
    {nsGkAtoms::href, kNameSpaceID_XLink, true}};

//----------------------------------------------------------------------
// Implementation

SVGGradientElement::SVGGradientElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGGradientElementBase(std::move(aNodeInfo)) {}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::EnumAttributesInfo SVGGradientElement::GetEnumInfo() {
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo, ArrayLength(sEnumInfo));
}

SVGElement::StringAttributesInfo SVGGradientElement::GetStringInfo() {
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

already_AddRefed<DOMSVGAnimatedEnumeration>
SVGGradientElement::GradientUnits() {
  return mEnumAttributes[GRADIENTUNITS].ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedTransformList>
SVGGradientElement::GradientTransform() {
  // We're creating a DOM wrapper, so we must tell GetAnimatedTransformList
  // to allocate the DOMSVGAnimatedTransformList if it hasn't already done so:
  return DOMSVGAnimatedTransformList::GetDOMWrapper(
      GetAnimatedTransformList(DO_ALLOCATE), this);
}

already_AddRefed<DOMSVGAnimatedEnumeration> SVGGradientElement::SpreadMethod() {
  return mEnumAttributes[SPREADMETHOD].ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedString> SVGGradientElement::Href() {
  return mStringAttributes[HREF].IsExplicitlySet()
             ? mStringAttributes[HREF].ToDOMAnimatedString(this)
             : mStringAttributes[XLINK_HREF].ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGGradientElement::IsAttributeMapped(const nsAtom* name) const {
  static const MappedAttributeEntry* const map[] = {sColorMap,
                                                    sGradientStopMap};

  return FindAttributeDependence(name, map) ||
         SVGGradientElementBase::IsAttributeMapped(name);
}

//---------------------Linear Gradients------------------------

JSObject* SVGLinearGradientElement::WrapNode(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return SVGLinearGradientElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::LengthInfo SVGLinearGradientElement::sLengthInfo[4] = {
    {nsGkAtoms::x1, 0, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::X},
    {nsGkAtoms::y1, 0, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::Y},
    {nsGkAtoms::x2, 100, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::X},
    {nsGkAtoms::y2, 0, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::Y},
};

//----------------------------------------------------------------------
// Implementation

SVGLinearGradientElement::SVGLinearGradientElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGLinearGradientElementBase(std::move(aNodeInfo)) {}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGLinearGradientElement)

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedLength> SVGLinearGradientElement::X1() {
  return mLengthAttributes[ATTR_X1].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGLinearGradientElement::Y1() {
  return mLengthAttributes[ATTR_Y1].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGLinearGradientElement::X2() {
  return mLengthAttributes[ATTR_X2].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGLinearGradientElement::Y2() {
  return mLengthAttributes[ATTR_Y2].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGAnimatedTransformList* SVGGradientElement::GetAnimatedTransformList(
    uint32_t aFlags) {
  if (!mGradientTransform && (aFlags & DO_ALLOCATE)) {
    mGradientTransform = MakeUnique<SVGAnimatedTransformList>();
  }
  return mGradientTransform.get();
}

SVGElement::LengthAttributesInfo SVGLinearGradientElement::GetLengthInfo() {
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//-------------------------- Radial Gradients ----------------------------

JSObject* SVGRadialGradientElement::WrapNode(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return SVGRadialGradientElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::LengthInfo SVGRadialGradientElement::sLengthInfo[6] = {
    {nsGkAtoms::cx, 50, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::X},
    {nsGkAtoms::cy, 50, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::Y},
    {nsGkAtoms::r, 50, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::XY},
    {nsGkAtoms::fx, 50, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::X},
    {nsGkAtoms::fy, 50, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::Y},
    {nsGkAtoms::fr, 0, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::XY},
};

//----------------------------------------------------------------------
// Implementation

SVGRadialGradientElement::SVGRadialGradientElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGRadialGradientElementBase(std::move(aNodeInfo)) {}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGRadialGradientElement)

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedLength> SVGRadialGradientElement::Cx() {
  return mLengthAttributes[ATTR_CX].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGRadialGradientElement::Cy() {
  return mLengthAttributes[ATTR_CY].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGRadialGradientElement::R() {
  return mLengthAttributes[ATTR_R].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGRadialGradientElement::Fx() {
  return mLengthAttributes[ATTR_FX].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGRadialGradientElement::Fy() {
  return mLengthAttributes[ATTR_FY].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGRadialGradientElement::Fr() {
  return mLengthAttributes[ATTR_FR].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::LengthAttributesInfo SVGRadialGradientElement::GetLengthInfo() {
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

}  // namespace dom
}  // namespace mozilla
