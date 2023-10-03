/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFESpecularLightingElement.h"
#include "mozilla/dom/SVGFESpecularLightingElementBinding.h"
#include "mozilla/SVGFilterInstance.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/BindContext.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FESpecularLighting)

using namespace mozilla::gfx;

namespace mozilla::dom {

JSObject* SVGFESpecularLightingElement::WrapNode(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return SVGFESpecularLightingElement_Binding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFESpecularLightingElement)

already_AddRefed<DOMSVGAnimatedString> SVGFESpecularLightingElement::In1() {
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGFESpecularLightingElement::SurfaceScale() {
  return mNumberAttributes[SURFACE_SCALE].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGFESpecularLightingElement::SpecularConstant() {
  return mNumberAttributes[SPECULAR_CONSTANT].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGFESpecularLightingElement::SpecularExponent() {
  return mNumberAttributes[SPECULAR_EXPONENT].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGFESpecularLightingElement::KernelUnitLengthX() {
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(
      SVGAnimatedNumberPair::eFirst, this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGFESpecularLightingElement::KernelUnitLengthY() {
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(
      SVGAnimatedNumberPair::eSecond, this);
}

//----------------------------------------------------------------------
// SVGElement methods

FilterPrimitiveDescription
SVGFESpecularLightingElement::GetPrimitiveDescription(
    SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
    const nsTArray<bool>& aInputsAreTainted,
    nsTArray<RefPtr<SourceSurface>>& aInputImages) {
  float specularExponent = mNumberAttributes[SPECULAR_EXPONENT].GetAnimValue();
  float specularConstant = mNumberAttributes[SPECULAR_CONSTANT].GetAnimValue();

  // specification defined range (15.22)
  if (specularExponent < 1 || specularExponent > 128) {
    return FilterPrimitiveDescription();
  }

  SpecularLightingAttributes atts;
  atts.mLightingConstant = specularConstant;
  atts.mSpecularExponent = specularExponent;
  if (!AddLightingAttributes(static_cast<DiffuseLightingAttributes*>(&atts),
                             aInstance)) {
    return FilterPrimitiveDescription();
  }

  return FilterPrimitiveDescription(AsVariant(std::move(atts)));
}

bool SVGFESpecularLightingElement::AttributeAffectsRendering(
    int32_t aNameSpaceID, nsAtom* aAttribute) const {
  return SVGFESpecularLightingElementBase::AttributeAffectsRendering(
             aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::specularConstant ||
           aAttribute == nsGkAtoms::specularExponent));
}

nsresult SVGFESpecularLightingElement::BindToTree(BindContext& aCtx,
                                                  nsINode& aParent) {
  if (aCtx.InComposedDoc()) {
    aCtx.OwnerDoc().SetUseCounter(eUseCounter_custom_feSpecularLighting);
  }

  return SVGFESpecularLightingElementBase::BindToTree(aCtx, aParent);
}

}  // namespace mozilla::dom
