/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEDiffuseLightingElement.h"
#include "mozilla/dom/SVGFEDiffuseLightingElementBinding.h"
#include "mozilla/SVGFilterInstance.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/BindContext.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FEDiffuseLighting)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject* SVGFEDiffuseLightingElement::WrapNode(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return SVGFEDiffuseLightingElement_Binding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEDiffuseLightingElement)

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedString> SVGFEDiffuseLightingElement::In1() {
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGFEDiffuseLightingElement::SurfaceScale() {
  return mNumberAttributes[SURFACE_SCALE].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGFEDiffuseLightingElement::DiffuseConstant() {
  return mNumberAttributes[DIFFUSE_CONSTANT].ToDOMAnimatedNumber(this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGFEDiffuseLightingElement::KernelUnitLengthX() {
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(
      SVGAnimatedNumberPair::eFirst, this);
}

already_AddRefed<DOMSVGAnimatedNumber>
SVGFEDiffuseLightingElement::KernelUnitLengthY() {
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(
      SVGAnimatedNumberPair::eSecond, this);
}

FilterPrimitiveDescription SVGFEDiffuseLightingElement::GetPrimitiveDescription(
    SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
    const nsTArray<bool>& aInputsAreTainted,
    nsTArray<RefPtr<SourceSurface>>& aInputImages) {
  float diffuseConstant = mNumberAttributes[DIFFUSE_CONSTANT].GetAnimValue();

  DiffuseLightingAttributes atts;
  atts.mLightingConstant = diffuseConstant;
  if (!AddLightingAttributes(&atts, aInstance)) {
    return FilterPrimitiveDescription();
  }

  return FilterPrimitiveDescription(AsVariant(std::move(atts)));
}

bool SVGFEDiffuseLightingElement::AttributeAffectsRendering(
    int32_t aNameSpaceID, nsAtom* aAttribute) const {
  return SVGFEDiffuseLightingElementBase::AttributeAffectsRendering(
             aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          aAttribute == nsGkAtoms::diffuseConstant);
}

nsresult SVGFEDiffuseLightingElement::BindToTree(BindContext& aCtx,
                                                 nsINode& aParent) {
  if (aCtx.InComposedDoc()) {
    aCtx.OwnerDoc().SetUseCounter(eUseCounter_custom_feDiffuseLighting);
  }

  return SVGFE::BindToTree(aCtx, aParent);
}

}  // namespace dom
}  // namespace mozilla
