/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEConvolveMatrixElement.h"
#include "mozilla/dom/SVGFEConvolveMatrixElementBinding.h"
#include "mozilla/SVGFilterInstance.h"
#include "mozilla/SVGUtils.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "DOMSVGAnimatedNumberList.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/BindContext.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FEConvolveMatrix)

using namespace mozilla::gfx;

namespace mozilla::dom {

JSObject* SVGFEConvolveMatrixElement::WrapNode(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return SVGFEConvolveMatrixElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::NumberInfo SVGFEConvolveMatrixElement::sNumberInfo[2] = {
    {nsGkAtoms::divisor, 1}, {nsGkAtoms::bias, 0}};

SVGElement::NumberPairInfo SVGFEConvolveMatrixElement::sNumberPairInfo[1] = {
    {nsGkAtoms::kernelUnitLength, 0, 0}};

SVGElement::IntegerInfo SVGFEConvolveMatrixElement::sIntegerInfo[2] = {
    {nsGkAtoms::targetX, 0}, {nsGkAtoms::targetY, 0}};

SVGElement::IntegerPairInfo SVGFEConvolveMatrixElement::sIntegerPairInfo[1] = {
    {nsGkAtoms::order, 3, 3}};

SVGElement::BooleanInfo SVGFEConvolveMatrixElement::sBooleanInfo[1] = {
    {nsGkAtoms::preserveAlpha, false}};

SVGEnumMapping SVGFEConvolveMatrixElement::sEdgeModeMap[] = {
    {nsGkAtoms::duplicate, SVG_EDGEMODE_DUPLICATE},
    {nsGkAtoms::wrap, SVG_EDGEMODE_WRAP},
    {nsGkAtoms::none, SVG_EDGEMODE_NONE},
    {nullptr, 0}};

SVGElement::EnumInfo SVGFEConvolveMatrixElement::sEnumInfo[1] = {
    {nsGkAtoms::edgeMode, sEdgeModeMap, SVG_EDGEMODE_DUPLICATE}};

SVGElement::StringInfo SVGFEConvolveMatrixElement::sStringInfo[2] = {
    {nsGkAtoms::result, kNameSpaceID_None, true},
    {nsGkAtoms::in, kNameSpaceID_None, true}};

SVGElement::NumberListInfo SVGFEConvolveMatrixElement::sNumberListInfo[1] = {
    {nsGkAtoms::kernelMatrix}};

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEConvolveMatrixElement)

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedString> SVGFEConvolveMatrixElement::In1() {
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<DOMSVGAnimatedInteger> SVGFEConvolveMatrixElement::OrderX() {
  return mIntegerPairAttributes[ORDER].ToDOMAnimatedInteger(
      SVGAnimatedIntegerPair::eFirst, this);
}

already_AddRefed<DOMSVGAnimatedInteger> SVGFEConvolveMatrixElement::OrderY() {
  return mIntegerPairAttributes[ORDER].ToDOMAnimatedInteger(
      SVGAnimatedIntegerPair::eSecond, this);
}

already_AddRefed<DOMSVGAnimatedNumberList>
SVGFEConvolveMatrixElement::KernelMatrix() {
  return DOMSVGAnimatedNumberList::GetDOMWrapper(
      &mNumberListAttributes[KERNELMATRIX], this, KERNELMATRIX);
}

already_AddRefed<DOMSVGAnimatedInteger> SVGFEConvolveMatrixElement::TargetX() {
  return mIntegerAttributes[TARGET_X].ToDOMAnimatedInteger(this);
}

already_AddRefed<DOMSVGAnimatedInteger> SVGFEConvolveMatrixElement::TargetY() {
  return mIntegerAttributes[TARGET_Y].ToDOMAnimatedInteger(this);
}

already_AddRefed<DOMSVGAnimatedEnumeration>
SVGFEConvolveMatrixElement::EdgeMode() {
  return mEnumAttributes[EDGEMODE].ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedBoolean>
SVGFEConvolveMatrixElement::PreserveAlpha() {
  return mBooleanAttributes[PRESERVEALPHA].ToDOMAnimatedBoolean(this);
}

already_AddRefed<DOMSVGAnimatedNumber> SVGFEConvolveMatrixElement::Divisor() {
  return mNumberAttributes[DIVISOR].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber> SVGFEConvolveMatrixElement::Bias() {
  return mNumberAttributes[BIAS].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGFEConvolveMatrixElement::KernelUnitLengthX() {
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(
      SVGAnimatedNumberPair::eFirst, this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGFEConvolveMatrixElement::KernelUnitLengthY() {
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(
      SVGAnimatedNumberPair::eSecond, this);
}

void SVGFEConvolveMatrixElement::GetSourceImageNames(
    nsTArray<SVGStringInfo>& aSources) {
  aSources.AppendElement(SVGStringInfo(&mStringAttributes[IN1], this));
}

FilterPrimitiveDescription SVGFEConvolveMatrixElement::GetPrimitiveDescription(
    SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
    const nsTArray<bool>& aInputsAreTainted,
    nsTArray<RefPtr<SourceSurface>>& aInputImages) {
  FilterPrimitiveDescription failureDescription;

  const SVGNumberList& kernelMatrix =
      mNumberListAttributes[KERNELMATRIX].GetAnimValue();
  uint32_t kmLength = kernelMatrix.Length();

  int32_t orderX = mIntegerPairAttributes[ORDER].GetAnimValue(
      SVGAnimatedIntegerPair::eFirst);
  int32_t orderY = mIntegerPairAttributes[ORDER].GetAnimValue(
      SVGAnimatedIntegerPair::eSecond);

  if (orderX <= 0 || orderY <= 0 ||
      static_cast<uint32_t>(orderX * orderY) != kmLength) {
    return failureDescription;
  }

  int32_t targetX, targetY;
  GetAnimatedIntegerValues(&targetX, &targetY, nullptr);

  if (mIntegerAttributes[TARGET_X].IsExplicitlySet()) {
    if (targetX < 0 || targetX >= orderX) return failureDescription;
  } else {
    targetX = orderX / 2;
  }
  if (mIntegerAttributes[TARGET_Y].IsExplicitlySet()) {
    if (targetY < 0 || targetY >= orderY) return failureDescription;
  } else {
    targetY = orderY / 2;
  }

  if (orderX > NS_SVG_OFFSCREEN_MAX_DIMENSION ||
      orderY > NS_SVG_OFFSCREEN_MAX_DIMENSION)
    return failureDescription;
  UniquePtr<float[]> kernel = MakeUniqueFallible<float[]>(orderX * orderY);
  if (!kernel) return failureDescription;
  for (uint32_t i = 0; i < kmLength; i++) {
    kernel[kmLength - 1 - i] = kernelMatrix[i];
  }

  float divisor;
  if (mNumberAttributes[DIVISOR].IsExplicitlySet()) {
    divisor = mNumberAttributes[DIVISOR].GetAnimValue();
    if (divisor == 0) return failureDescription;
  } else {
    divisor = kernel[0];
    for (uint32_t i = 1; i < kmLength; i++) divisor += kernel[i];
    if (divisor == 0) divisor = 1;
  }

  uint32_t edgeMode = mEnumAttributes[EDGEMODE].GetAnimValue();
  bool preserveAlpha = mBooleanAttributes[PRESERVEALPHA].GetAnimValue();
  float bias = mNumberAttributes[BIAS].GetAnimValue();

  Size kernelUnitLength = GetKernelUnitLength(
      aInstance, &mNumberPairAttributes[KERNEL_UNIT_LENGTH]);

  if (kernelUnitLength.width <= 0 || kernelUnitLength.height <= 0) {
    // According to spec, A negative or zero value is an error. See link below
    // for details.
    // https://www.w3.org/TR/SVG/filters.html#feConvolveMatrixElementKernelUnitLengthAttribute
    return failureDescription;
  }

  ConvolveMatrixAttributes atts;
  atts.mKernelSize = IntSize(orderX, orderY);
  atts.mKernelMatrix.AppendElements(&kernelMatrix[0], kmLength);
  atts.mDivisor = divisor;
  atts.mBias = bias;
  atts.mTarget = IntPoint(targetX, targetY);
  atts.mEdgeMode = edgeMode;
  atts.mKernelUnitLength = kernelUnitLength;
  atts.mPreserveAlpha = preserveAlpha;

  return FilterPrimitiveDescription(AsVariant(std::move(atts)));
}

bool SVGFEConvolveMatrixElement::AttributeAffectsRendering(
    int32_t aNameSpaceID, nsAtom* aAttribute) const {
  return SVGFEConvolveMatrixElementBase::AttributeAffectsRendering(
             aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in || aAttribute == nsGkAtoms::divisor ||
           aAttribute == nsGkAtoms::bias ||
           aAttribute == nsGkAtoms::kernelUnitLength ||
           aAttribute == nsGkAtoms::targetX ||
           aAttribute == nsGkAtoms::targetY || aAttribute == nsGkAtoms::order ||
           aAttribute == nsGkAtoms::preserveAlpha ||
           aAttribute == nsGkAtoms::edgeMode ||
           aAttribute == nsGkAtoms::kernelMatrix));
}

nsresult SVGFEConvolveMatrixElement::BindToTree(BindContext& aCtx,
                                                nsINode& aParent) {
  if (aCtx.InComposedDoc()) {
    aCtx.OwnerDoc().SetUseCounter(eUseCounter_custom_feConvolveMatrix);
  }

  return SVGFEConvolveMatrixElementBase::BindToTree(aCtx, aParent);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::NumberAttributesInfo SVGFEConvolveMatrixElement::GetNumberInfo() {
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

SVGElement::NumberPairAttributesInfo
SVGFEConvolveMatrixElement::GetNumberPairInfo() {
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  ArrayLength(sNumberPairInfo));
}

SVGElement::IntegerAttributesInfo SVGFEConvolveMatrixElement::GetIntegerInfo() {
  return IntegerAttributesInfo(mIntegerAttributes, sIntegerInfo,
                               ArrayLength(sIntegerInfo));
}

SVGElement::IntegerPairAttributesInfo
SVGFEConvolveMatrixElement::GetIntegerPairInfo() {
  return IntegerPairAttributesInfo(mIntegerPairAttributes, sIntegerPairInfo,
                                   ArrayLength(sIntegerPairInfo));
}

SVGElement::BooleanAttributesInfo SVGFEConvolveMatrixElement::GetBooleanInfo() {
  return BooleanAttributesInfo(mBooleanAttributes, sBooleanInfo,
                               ArrayLength(sBooleanInfo));
}

SVGElement::EnumAttributesInfo SVGFEConvolveMatrixElement::GetEnumInfo() {
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo, ArrayLength(sEnumInfo));
}

SVGElement::StringAttributesInfo SVGFEConvolveMatrixElement::GetStringInfo() {
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

SVGElement::NumberListAttributesInfo
SVGFEConvolveMatrixElement::GetNumberListInfo() {
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  ArrayLength(sNumberListInfo));
}

}  // namespace mozilla::dom
