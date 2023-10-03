/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEMorphologyElement.h"
#include "mozilla/dom/SVGFEMorphologyElementBinding.h"
#include "mozilla/SVGFilterInstance.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/Document.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FEMorphology)

using namespace mozilla::gfx;

namespace mozilla::dom {

JSObject* SVGFEMorphologyElement::WrapNode(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return SVGFEMorphologyElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::NumberPairInfo SVGFEMorphologyElement::sNumberPairInfo[1] = {
    {nsGkAtoms::radius, 0, 0}};

SVGEnumMapping SVGFEMorphologyElement::sOperatorMap[] = {
    {nsGkAtoms::erode, SVG_OPERATOR_ERODE},
    {nsGkAtoms::dilate, SVG_OPERATOR_DILATE},
    {nullptr, 0}};

SVGElement::EnumInfo SVGFEMorphologyElement::sEnumInfo[1] = {
    {nsGkAtoms::_operator, sOperatorMap, SVG_OPERATOR_ERODE}};

SVGElement::StringInfo SVGFEMorphologyElement::sStringInfo[2] = {
    {nsGkAtoms::result, kNameSpaceID_None, true},
    {nsGkAtoms::in, kNameSpaceID_None, true}};

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEMorphologyElement)

//----------------------------------------------------------------------
// SVGFEMorphologyElement methods

already_AddRefed<DOMSVGAnimatedString> SVGFEMorphologyElement::In1() {
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<DOMSVGAnimatedEnumeration> SVGFEMorphologyElement::Operator() {
  return mEnumAttributes[OPERATOR].ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedNumber> SVGFEMorphologyElement::RadiusX() {
  return mNumberPairAttributes[RADIUS].ToDOMAnimatedNumber(
      SVGAnimatedNumberPair::eFirst, this);
}

already_AddRefed<DOMSVGAnimatedNumber> SVGFEMorphologyElement::RadiusY() {
  return mNumberPairAttributes[RADIUS].ToDOMAnimatedNumber(
      SVGAnimatedNumberPair::eSecond, this);
}

void SVGFEMorphologyElement::SetRadius(float rx, float ry) {
  mNumberPairAttributes[RADIUS].SetBaseValues(rx, ry, this);
}

void SVGFEMorphologyElement::GetSourceImageNames(
    nsTArray<SVGStringInfo>& aSources) {
  aSources.AppendElement(SVGStringInfo(&mStringAttributes[IN1], this));
}

#define MORPHOLOGY_EPSILON 0.0001

void SVGFEMorphologyElement::GetRXY(int32_t* aRX, int32_t* aRY,
                                    const SVGFilterInstance& aInstance) {
  // Subtract an epsilon here because we don't want a value that's just
  // slightly larger than an integer to round up to the next integer; it's
  // probably meant to be the integer it's close to, modulo machine precision
  // issues.
  *aRX = NSToIntCeil(aInstance.GetPrimitiveNumber(
                         SVGContentUtils::X, &mNumberPairAttributes[RADIUS],
                         SVGAnimatedNumberPair::eFirst) -
                     MORPHOLOGY_EPSILON);
  *aRY = NSToIntCeil(aInstance.GetPrimitiveNumber(
                         SVGContentUtils::Y, &mNumberPairAttributes[RADIUS],
                         SVGAnimatedNumberPair::eSecond) -
                     MORPHOLOGY_EPSILON);
}

FilterPrimitiveDescription SVGFEMorphologyElement::GetPrimitiveDescription(
    SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
    const nsTArray<bool>& aInputsAreTainted,
    nsTArray<RefPtr<SourceSurface>>& aInputImages) {
  int32_t rx, ry;
  GetRXY(&rx, &ry, *aInstance);
  MorphologyAttributes atts;
  atts.mRadii = Size(rx, ry);
  atts.mOperator = (uint32_t)mEnumAttributes[OPERATOR].GetAnimValue();
  return FilterPrimitiveDescription(AsVariant(std::move(atts)));
}

bool SVGFEMorphologyElement::AttributeAffectsRendering(
    int32_t aNameSpaceID, nsAtom* aAttribute) const {
  return SVGFEMorphologyElementBase::AttributeAffectsRendering(aNameSpaceID,
                                                               aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in || aAttribute == nsGkAtoms::radius ||
           aAttribute == nsGkAtoms::_operator));
}

nsresult SVGFEMorphologyElement::BindToTree(BindContext& aCtx,
                                            nsINode& aParent) {
  if (aCtx.InComposedDoc()) {
    aCtx.OwnerDoc().SetUseCounter(eUseCounter_custom_feMorphology);
  }

  return SVGFEMorphologyElementBase::BindToTree(aCtx, aParent);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::NumberPairAttributesInfo
SVGFEMorphologyElement::GetNumberPairInfo() {
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  ArrayLength(sNumberPairInfo));
}

SVGElement::EnumAttributesInfo SVGFEMorphologyElement::GetEnumInfo() {
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo, ArrayLength(sEnumInfo));
}

SVGElement::StringAttributesInfo SVGFEMorphologyElement::GetStringInfo() {
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

}  // namespace mozilla::dom
