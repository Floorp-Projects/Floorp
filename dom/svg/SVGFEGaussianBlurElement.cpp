/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEGaussianBlurElement.h"
#include "mozilla/dom/SVGFEGaussianBlurElementBinding.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGUtils.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FEGaussianBlur)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject* SVGFEGaussianBlurElement::WrapNode(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return SVGFEGaussianBlurElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::NumberPairInfo SVGFEGaussianBlurElement::sNumberPairInfo[1] = {
    {nsGkAtoms::stdDeviation, 0, 0}};

SVGElement::StringInfo SVGFEGaussianBlurElement::sStringInfo[2] = {
    {nsGkAtoms::result, kNameSpaceID_None, true},
    {nsGkAtoms::in, kNameSpaceID_None, true}};

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEGaussianBlurElement)

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedString> SVGFEGaussianBlurElement::In1() {
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGFEGaussianBlurElement::StdDeviationX() {
  return mNumberPairAttributes[STD_DEV].ToDOMAnimatedNumber(
      SVGAnimatedNumberPair::eFirst, this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGFEGaussianBlurElement::StdDeviationY() {
  return mNumberPairAttributes[STD_DEV].ToDOMAnimatedNumber(
      SVGAnimatedNumberPair::eSecond, this);
}

void SVGFEGaussianBlurElement::SetStdDeviation(float stdDeviationX,
                                               float stdDeviationY) {
  mNumberPairAttributes[STD_DEV].SetBaseValues(stdDeviationX, stdDeviationY,
                                               this);
}

FilterPrimitiveDescription SVGFEGaussianBlurElement::GetPrimitiveDescription(
    nsSVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
    const nsTArray<bool>& aInputsAreTainted,
    nsTArray<RefPtr<SourceSurface>>& aInputImages) {
  float stdX = aInstance->GetPrimitiveNumber(SVGContentUtils::X,
                                             &mNumberPairAttributes[STD_DEV],
                                             SVGAnimatedNumberPair::eFirst);
  float stdY = aInstance->GetPrimitiveNumber(SVGContentUtils::Y,
                                             &mNumberPairAttributes[STD_DEV],
                                             SVGAnimatedNumberPair::eSecond);
  if (stdX < 0 || stdY < 0) {
    return FilterPrimitiveDescription();
  }

  GaussianBlurAttributes atts;
  atts.mStdDeviation = Size(stdX, stdY);
  return FilterPrimitiveDescription(AsVariant(std::move(atts)));
}

bool SVGFEGaussianBlurElement::AttributeAffectsRendering(
    int32_t aNameSpaceID, nsAtom* aAttribute) const {
  return SVGFEGaussianBlurElementBase::AttributeAffectsRendering(aNameSpaceID,
                                                                 aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::stdDeviation));
}

void SVGFEGaussianBlurElement::GetSourceImageNames(
    nsTArray<SVGStringInfo>& aSources) {
  aSources.AppendElement(SVGStringInfo(&mStringAttributes[IN1], this));
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::NumberPairAttributesInfo
SVGFEGaussianBlurElement::GetNumberPairInfo() {
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  ArrayLength(sNumberPairInfo));
}

SVGElement::StringAttributesInfo SVGFEGaussianBlurElement::GetStringInfo() {
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

}  // namespace dom
}  // namespace mozilla
