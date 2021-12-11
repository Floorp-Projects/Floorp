/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEColorMatrixElement.h"

#include "DOMSVGAnimatedNumberList.h"
#include "mozilla/dom/SVGFEColorMatrixElementBinding.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/BindContext.h"

#define NUM_ENTRIES_IN_4x5_MATRIX 20

NS_IMPL_NS_NEW_SVG_ELEMENT(FEColorMatrix)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject* SVGFEColorMatrixElement::WrapNode(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return SVGFEColorMatrixElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGEnumMapping SVGFEColorMatrixElement::sTypeMap[] = {
    {nsGkAtoms::matrix, SVG_FECOLORMATRIX_TYPE_MATRIX},
    {nsGkAtoms::saturate, SVG_FECOLORMATRIX_TYPE_SATURATE},
    {nsGkAtoms::hueRotate, SVG_FECOLORMATRIX_TYPE_HUE_ROTATE},
    {nsGkAtoms::luminanceToAlpha, SVG_FECOLORMATRIX_TYPE_LUMINANCE_TO_ALPHA},
    {nullptr, 0}};

SVGElement::EnumInfo SVGFEColorMatrixElement::sEnumInfo[1] = {
    {nsGkAtoms::type, sTypeMap, SVG_FECOLORMATRIX_TYPE_MATRIX}};

SVGElement::StringInfo SVGFEColorMatrixElement::sStringInfo[2] = {
    {nsGkAtoms::result, kNameSpaceID_None, true},
    {nsGkAtoms::in, kNameSpaceID_None, true}};

SVGElement::NumberListInfo SVGFEColorMatrixElement::sNumberListInfo[1] = {
    {nsGkAtoms::values}};

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEColorMatrixElement)

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedString> SVGFEColorMatrixElement::In1() {
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<DOMSVGAnimatedEnumeration> SVGFEColorMatrixElement::Type() {
  return mEnumAttributes[TYPE].ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedNumberList> SVGFEColorMatrixElement::Values() {
  return DOMSVGAnimatedNumberList::GetDOMWrapper(&mNumberListAttributes[VALUES],
                                                 this, VALUES);
}

void SVGFEColorMatrixElement::GetSourceImageNames(
    nsTArray<SVGStringInfo>& aSources) {
  aSources.AppendElement(SVGStringInfo(&mStringAttributes[IN1], this));
}

FilterPrimitiveDescription SVGFEColorMatrixElement::GetPrimitiveDescription(
    SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
    const nsTArray<bool>& aInputsAreTainted,
    nsTArray<RefPtr<SourceSurface>>& aInputImages) {
  uint32_t type = mEnumAttributes[TYPE].GetAnimValue();
  const SVGNumberList& values = mNumberListAttributes[VALUES].GetAnimValue();

  ColorMatrixAttributes atts;
  if (!mNumberListAttributes[VALUES].IsExplicitlySet() &&
      (type == SVG_FECOLORMATRIX_TYPE_MATRIX ||
       type == SVG_FECOLORMATRIX_TYPE_SATURATE ||
       type == SVG_FECOLORMATRIX_TYPE_HUE_ROTATE)) {
    atts.mType = (uint32_t)SVG_FECOLORMATRIX_TYPE_MATRIX;
    static const float identityMatrix[] = {
        // clang-format off
        1, 0, 0, 0, 0,
        0, 1, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 1, 0
        // clang-format on
    };
    atts.mValues.AppendElements(identityMatrix, 20);
  } else {
    atts.mType = type;
    if (values.Length()) {
      atts.mValues.AppendElements(&values[0], values.Length());
    }
  }

  return FilterPrimitiveDescription(AsVariant(std::move(atts)));
}

bool SVGFEColorMatrixElement::AttributeAffectsRendering(
    int32_t aNameSpaceID, nsAtom* aAttribute) const {
  return SVGFEColorMatrixElementBase::AttributeAffectsRendering(aNameSpaceID,
                                                                aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in || aAttribute == nsGkAtoms::type ||
           aAttribute == nsGkAtoms::values));
}

nsresult SVGFEColorMatrixElement::BindToTree(BindContext& aCtx,
                                             nsINode& aParent) {
  if (aCtx.InComposedDoc()) {
    aCtx.OwnerDoc().SetUseCounter(eUseCounter_custom_feColorMatrix);
  }

  return SVGFE::BindToTree(aCtx, aParent);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::EnumAttributesInfo SVGFEColorMatrixElement::GetEnumInfo() {
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo, ArrayLength(sEnumInfo));
}

SVGElement::StringAttributesInfo SVGFEColorMatrixElement::GetStringInfo() {
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

SVGElement::NumberListAttributesInfo
SVGFEColorMatrixElement::GetNumberListInfo() {
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  ArrayLength(sNumberListInfo));
}

}  // namespace dom
}  // namespace mozilla
