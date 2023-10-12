/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEDisplacementMapElement.h"
#include "mozilla/dom/SVGFEDisplacementMapElementBinding.h"
#include "mozilla/SVGFilterInstance.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/BindContext.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FEDisplacementMap)

using namespace mozilla::gfx;

namespace mozilla::dom {

JSObject* SVGFEDisplacementMapElement::WrapNode(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return SVGFEDisplacementMapElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::NumberInfo SVGFEDisplacementMapElement::sNumberInfo[1] = {
    {nsGkAtoms::scale, 0},
};

SVGEnumMapping SVGFEDisplacementMapElement::sChannelMap[] = {
    {nsGkAtoms::R, SVG_CHANNEL_R},
    {nsGkAtoms::G, SVG_CHANNEL_G},
    {nsGkAtoms::B, SVG_CHANNEL_B},
    {nsGkAtoms::A, SVG_CHANNEL_A},
    {nullptr, 0}};

SVGElement::EnumInfo SVGFEDisplacementMapElement::sEnumInfo[2] = {
    {nsGkAtoms::xChannelSelector, sChannelMap, SVG_CHANNEL_A},
    {nsGkAtoms::yChannelSelector, sChannelMap, SVG_CHANNEL_A}};

SVGElement::StringInfo SVGFEDisplacementMapElement::sStringInfo[3] = {
    {nsGkAtoms::result, kNameSpaceID_None, true},
    {nsGkAtoms::in, kNameSpaceID_None, true},
    {nsGkAtoms::in2, kNameSpaceID_None, true}};

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEDisplacementMapElement)

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedString> SVGFEDisplacementMapElement::In1() {
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<DOMSVGAnimatedString> SVGFEDisplacementMapElement::In2() {
  return mStringAttributes[IN2].ToDOMAnimatedString(this);
}

already_AddRefed<DOMSVGAnimatedNumber> SVGFEDisplacementMapElement::Scale() {
  return mNumberAttributes[SCALE].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedEnumeration>
SVGFEDisplacementMapElement::XChannelSelector() {
  return mEnumAttributes[CHANNEL_X].ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedEnumeration>
SVGFEDisplacementMapElement::YChannelSelector() {
  return mEnumAttributes[CHANNEL_Y].ToDOMAnimatedEnum(this);
}

FilterPrimitiveDescription SVGFEDisplacementMapElement::GetPrimitiveDescription(
    SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
    const nsTArray<bool>& aInputsAreTainted,
    nsTArray<RefPtr<SourceSurface>>& aInputImages) {
  if (aInputsAreTainted[1]) {
    // If the map is tainted, refuse to apply the effect and act as a
    // pass-through filter instead, as required by the spec.
    OffsetAttributes atts;
    atts.mValue = IntPoint(0, 0);
    return FilterPrimitiveDescription(AsVariant(std::move(atts)));
  }

  float scale = aInstance->GetPrimitiveNumber(SVGContentUtils::XY,
                                              &mNumberAttributes[SCALE]);
  uint32_t xChannel = mEnumAttributes[CHANNEL_X].GetAnimValue();
  uint32_t yChannel = mEnumAttributes[CHANNEL_Y].GetAnimValue();
  DisplacementMapAttributes atts;
  atts.mScale = scale;
  atts.mXChannel = xChannel;
  atts.mYChannel = yChannel;
  return FilterPrimitiveDescription(AsVariant(std::move(atts)));
}

bool SVGFEDisplacementMapElement::AttributeAffectsRendering(
    int32_t aNameSpaceID, nsAtom* aAttribute) const {
  return SVGFEDisplacementMapElementBase::AttributeAffectsRendering(
             aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in || aAttribute == nsGkAtoms::in2 ||
           aAttribute == nsGkAtoms::scale ||
           aAttribute == nsGkAtoms::xChannelSelector ||
           aAttribute == nsGkAtoms::yChannelSelector));
}

void SVGFEDisplacementMapElement::GetSourceImageNames(
    nsTArray<SVGStringInfo>& aSources) {
  aSources.AppendElement(SVGStringInfo(&mStringAttributes[IN1], this));
  aSources.AppendElement(SVGStringInfo(&mStringAttributes[IN2], this));
}

nsresult SVGFEDisplacementMapElement::BindToTree(BindContext& aCtx,
                                                 nsINode& aParent) {
  if (aCtx.InComposedDoc()) {
    aCtx.OwnerDoc().SetUseCounter(eUseCounter_custom_feDisplacementMap);
  }

  return SVGFEDisplacementMapElementBase::BindToTree(aCtx, aParent);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::NumberAttributesInfo SVGFEDisplacementMapElement::GetNumberInfo() {
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

SVGElement::EnumAttributesInfo SVGFEDisplacementMapElement::GetEnumInfo() {
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo, ArrayLength(sEnumInfo));
}

SVGElement::StringAttributesInfo SVGFEDisplacementMapElement::GetStringInfo() {
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

}  // namespace mozilla::dom
