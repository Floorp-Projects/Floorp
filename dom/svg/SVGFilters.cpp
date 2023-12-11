/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGFilters.h"

#include <algorithm>
#include "DOMSVGAnimatedNumberList.h"
#include "DOMSVGAnimatedLength.h"
#include "nsGkAtoms.h"
#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedNumberPair.h"
#include "SVGAnimatedString.h"
#include "SVGNumberList.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/SVGContentUtils.h"
#include "mozilla/SVGFilterInstance.h"
#include "mozilla/dom/SVGComponentTransferFunctionElement.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/dom/SVGFEDistantLightElement.h"
#include "mozilla/dom/SVGFEFuncAElementBinding.h"
#include "mozilla/dom/SVGFEFuncBElementBinding.h"
#include "mozilla/dom/SVGFEFuncGElementBinding.h"
#include "mozilla/dom/SVGFEFuncRElementBinding.h"
#include "mozilla/dom/SVGFEPointLightElement.h"
#include "mozilla/dom/SVGFESpotLightElement.h"
#include "mozilla/dom/SVGFilterElement.h"
#include "mozilla/dom/SVGLengthBinding.h"

#if defined(XP_WIN)
// Prevent Windows redefining LoadImage
#  undef LoadImage
#endif

using namespace mozilla::gfx;

namespace mozilla::dom {

//--------------------Filter Element Base Class-----------------------

SVGElement::LengthInfo SVGFilterPrimitiveElement::sLengthInfo[4] = {
    {nsGkAtoms::x, 0, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::X},
    {nsGkAtoms::y, 0, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::Y},
    {nsGkAtoms::width, 100, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::X},
    {nsGkAtoms::height, 100, SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE,
     SVGContentUtils::Y}};

//----------------------------------------------------------------------
// Implementation

void SVGFilterPrimitiveElement::GetSourceImageNames(
    nsTArray<SVGStringInfo>& aSources) {}

bool SVGFilterPrimitiveElement::OutputIsTainted(
    const nsTArray<bool>& aInputsAreTainted,
    nsIPrincipal* aReferencePrincipal) {
  // This is the default implementation for OutputIsTainted.
  // Our output is tainted if we have at least one tainted input.
  return aInputsAreTainted.Contains(true);
}

bool SVGFilterPrimitiveElement::AttributeAffectsRendering(
    int32_t aNameSpaceID, nsAtom* aAttribute) const {
  return aNameSpaceID == kNameSpaceID_None &&
         (aAttribute == nsGkAtoms::x || aAttribute == nsGkAtoms::y ||
          aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height ||
          aAttribute == nsGkAtoms::result);
}

already_AddRefed<DOMSVGAnimatedLength> SVGFilterPrimitiveElement::X() {
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGFilterPrimitiveElement::Y() {
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGFilterPrimitiveElement::Width() {
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGFilterPrimitiveElement::Height() {
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedString> SVGFilterPrimitiveElement::Result() {
  return GetResultImageName().ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------
// SVGElement methods

bool SVGFilterPrimitiveElement::StyleIsSetToSRGB() {
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) return false;

  ComputedStyle* style = frame->Style();
  return style->StyleSVG()->mColorInterpolationFilters ==
         StyleColorInterpolation::Srgb;
}

/* virtual */
bool SVGFilterPrimitiveElement::HasValidDimensions() const {
  return (!mLengthAttributes[ATTR_WIDTH].IsExplicitlySet() ||
          mLengthAttributes[ATTR_WIDTH].GetAnimValInSpecifiedUnits() > 0) &&
         (!mLengthAttributes[ATTR_HEIGHT].IsExplicitlySet() ||
          mLengthAttributes[ATTR_HEIGHT].GetAnimValInSpecifiedUnits() > 0);
}

Size SVGFilterPrimitiveElement::GetKernelUnitLength(
    SVGFilterInstance* aInstance, SVGAnimatedNumberPair* aKernelUnitLength) {
  if (!aKernelUnitLength->IsExplicitlySet()) {
    return Size(1, 1);
  }

  float kernelX = aInstance->GetPrimitiveNumber(
      SVGContentUtils::X, aKernelUnitLength, SVGAnimatedNumberPair::eFirst);
  float kernelY = aInstance->GetPrimitiveNumber(
      SVGContentUtils::Y, aKernelUnitLength, SVGAnimatedNumberPair::eSecond);
  return Size(kernelX, kernelY);
}

SVGElement::LengthAttributesInfo SVGFilterPrimitiveElement::GetLengthInfo() {
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

SVGElement::NumberListInfo
    SVGComponentTransferFunctionElement::sNumberListInfo[1] = {
        {nsGkAtoms::tableValues}};

SVGElement::NumberInfo SVGComponentTransferFunctionElement::sNumberInfo[5] = {
    {nsGkAtoms::slope, 1},
    {nsGkAtoms::intercept, 0},
    {nsGkAtoms::amplitude, 1},
    {nsGkAtoms::exponent, 1},
    {nsGkAtoms::offset, 0}};

SVGEnumMapping SVGComponentTransferFunctionElement::sTypeMap[] = {
    {nsGkAtoms::identity, SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY},
    {nsGkAtoms::table, SVG_FECOMPONENTTRANSFER_TYPE_TABLE},
    {nsGkAtoms::discrete, SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE},
    {nsGkAtoms::linear, SVG_FECOMPONENTTRANSFER_TYPE_LINEAR},
    {nsGkAtoms::gamma, SVG_FECOMPONENTTRANSFER_TYPE_GAMMA},
    {nullptr, 0}};

SVGElement::EnumInfo SVGComponentTransferFunctionElement::sEnumInfo[1] = {
    {nsGkAtoms::type, sTypeMap, SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY}};

//----------------------------------------------------------------------
// nsSVGFilterPrimitiveChildElement methods

bool SVGComponentTransferFunctionElement::AttributeAffectsRendering(
    int32_t aNameSpaceID, nsAtom* aAttribute) const {
  return aNameSpaceID == kNameSpaceID_None &&
         (aAttribute == nsGkAtoms::tableValues ||
          aAttribute == nsGkAtoms::slope ||
          aAttribute == nsGkAtoms::intercept ||
          aAttribute == nsGkAtoms::amplitude ||
          aAttribute == nsGkAtoms::exponent ||
          aAttribute == nsGkAtoms::offset || aAttribute == nsGkAtoms::type);
}

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedEnumeration>
SVGComponentTransferFunctionElement::Type() {
  return mEnumAttributes[TYPE].ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedNumberList>
SVGComponentTransferFunctionElement::TableValues() {
  return DOMSVGAnimatedNumberList::GetDOMWrapper(
      &mNumberListAttributes[TABLEVALUES], this, TABLEVALUES);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGComponentTransferFunctionElement::Slope() {
  return mNumberAttributes[SLOPE].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGComponentTransferFunctionElement::Intercept() {
  return mNumberAttributes[INTERCEPT].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGComponentTransferFunctionElement::Amplitude() {
  return mNumberAttributes[AMPLITUDE].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGComponentTransferFunctionElement::Exponent() {
  return mNumberAttributes[EXPONENT].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGComponentTransferFunctionElement::Offset() {
  return mNumberAttributes[OFFSET].ToDOMAnimatedNumber(this);
}

void SVGComponentTransferFunctionElement::ComputeAttributes(
    int32_t aChannel, ComponentTransferAttributes& aAttributes) {
  uint32_t type = mEnumAttributes[TYPE].GetAnimValue();

  float slope, intercept, amplitude, exponent, offset;
  GetAnimatedNumberValues(&slope, &intercept, &amplitude, &exponent, &offset,
                          nullptr);

  const SVGNumberList& tableValues =
      mNumberListAttributes[TABLEVALUES].GetAnimValue();

  aAttributes.mTypes[aChannel] = (uint8_t)type;
  switch (type) {
    case SVG_FECOMPONENTTRANSFER_TYPE_LINEAR: {
      aAttributes.mValues[aChannel].SetLength(2);
      aAttributes.mValues[aChannel][kComponentTransferSlopeIndex] = slope;
      aAttributes.mValues[aChannel][kComponentTransferInterceptIndex] =
          intercept;
      break;
    }
    case SVG_FECOMPONENTTRANSFER_TYPE_GAMMA: {
      aAttributes.mValues[aChannel].SetLength(3);
      aAttributes.mValues[aChannel][kComponentTransferAmplitudeIndex] =
          amplitude;
      aAttributes.mValues[aChannel][kComponentTransferExponentIndex] = exponent;
      aAttributes.mValues[aChannel][kComponentTransferOffsetIndex] = offset;
      break;
    }
    case SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE:
    case SVG_FECOMPONENTTRANSFER_TYPE_TABLE: {
      if (!tableValues.IsEmpty()) {
        aAttributes.mValues[aChannel].AppendElements(&tableValues[0],
                                                     tableValues.Length());
      }
      break;
    }
  }
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::NumberListAttributesInfo
SVGComponentTransferFunctionElement::GetNumberListInfo() {
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  ArrayLength(sNumberListInfo));
}

SVGElement::EnumAttributesInfo
SVGComponentTransferFunctionElement::GetEnumInfo() {
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo, ArrayLength(sEnumInfo));
}

SVGElement::NumberAttributesInfo
SVGComponentTransferFunctionElement::GetNumberInfo() {
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

/* virtual */
JSObject* SVGFEFuncRElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return SVGFEFuncRElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom

NS_IMPL_NS_NEW_SVG_ELEMENT(FEFuncR)

namespace mozilla::dom {

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEFuncRElement)

/* virtual */
JSObject* SVGFEFuncGElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return SVGFEFuncGElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom

NS_IMPL_NS_NEW_SVG_ELEMENT(FEFuncG)

namespace mozilla::dom {

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEFuncGElement)

/* virtual */
JSObject* SVGFEFuncBElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return SVGFEFuncBElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom

NS_IMPL_NS_NEW_SVG_ELEMENT(FEFuncB)

namespace mozilla::dom {

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEFuncBElement)

/* virtual */
JSObject* SVGFEFuncAElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return SVGFEFuncAElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom

NS_IMPL_NS_NEW_SVG_ELEMENT(FEFuncA)

namespace mozilla::dom {

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEFuncAElement)

//--------------------------------------------------------------------
//
SVGElement::NumberInfo SVGFELightingElement::sNumberInfo[4] = {
    {nsGkAtoms::surfaceScale, 1},
    {nsGkAtoms::diffuseConstant, 1},
    {nsGkAtoms::specularConstant, 1},
    {nsGkAtoms::specularExponent, 1}};

SVGElement::NumberPairInfo SVGFELightingElement::sNumberPairInfo[1] = {
    {nsGkAtoms::kernelUnitLength, 0, 0}};

SVGElement::StringInfo SVGFELightingElement::sStringInfo[2] = {
    {nsGkAtoms::result, kNameSpaceID_None, true},
    {nsGkAtoms::in, kNameSpaceID_None, true}};

//----------------------------------------------------------------------
// Implementation

void SVGFELightingElement::GetSourceImageNames(
    nsTArray<SVGStringInfo>& aSources) {
  aSources.AppendElement(SVGStringInfo(&mStringAttributes[IN1], this));
}

LightType SVGFELightingElement::ComputeLightAttributes(
    SVGFilterInstance* aInstance, nsTArray<float>& aFloatAttributes) {
  // find specified light
  for (nsCOMPtr<nsIContent> child = nsINode::GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsAnyOfSVGElements(nsGkAtoms::feDistantLight,
                                  nsGkAtoms::fePointLight,
                                  nsGkAtoms::feSpotLight)) {
      return static_cast<SVGFELightElement*>(child.get())
          ->ComputeLightAttributes(aInstance, aFloatAttributes);
    }
  }

  return LightType::None;
}

bool SVGFELightingElement::AddLightingAttributes(
    mozilla::gfx::DiffuseLightingAttributes* aAttributes,
    SVGFilterInstance* aInstance) {
  const auto* frame = GetPrimaryFrame();
  if (!frame) {
    return false;
  }

  const nsStyleSVGReset* styleSVGReset = frame->Style()->StyleSVGReset();
  sRGBColor color(
      sRGBColor::FromABGR(styleSVGReset->mLightingColor.CalcColor(frame)));
  color.a = 1.f;
  float surfaceScale = mNumberAttributes[SURFACE_SCALE].GetAnimValue();
  Size kernelUnitLength = GetKernelUnitLength(
      aInstance, &mNumberPairAttributes[KERNEL_UNIT_LENGTH]);

  if (kernelUnitLength.width <= 0 || kernelUnitLength.height <= 0) {
    // According to spec, A negative or zero value is an error. See link below
    // for details.
    // https://www.w3.org/TR/SVG/filters.html#feSpecularLightingKernelUnitLengthAttribute
    return false;
  }

  aAttributes->mLightType =
      ComputeLightAttributes(aInstance, aAttributes->mLightValues);
  aAttributes->mSurfaceScale = surfaceScale;
  aAttributes->mKernelUnitLength = kernelUnitLength;
  aAttributes->mColor = color;

  return true;
}

bool SVGFELightingElement::OutputIsTainted(
    const nsTArray<bool>& aInputsAreTainted,
    nsIPrincipal* aReferencePrincipal) {
  if (const auto* frame = GetPrimaryFrame()) {
    if (frame->Style()->StyleSVGReset()->mLightingColor.IsCurrentColor()) {
      return true;
    }
  }

  return SVGFELightingElementBase::OutputIsTainted(aInputsAreTainted,
                                                   aReferencePrincipal);
}

bool SVGFELightingElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                     nsAtom* aAttribute) const {
  return SVGFELightingElementBase::AttributeAffectsRendering(aNameSpaceID,
                                                             aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::surfaceScale ||
           aAttribute == nsGkAtoms::kernelUnitLength));
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::NumberAttributesInfo SVGFELightingElement::GetNumberInfo() {
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

SVGElement::NumberPairAttributesInfo SVGFELightingElement::GetNumberPairInfo() {
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  ArrayLength(sNumberPairInfo));
}

SVGElement::StringAttributesInfo SVGFELightingElement::GetStringInfo() {
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

}  // namespace mozilla::dom
