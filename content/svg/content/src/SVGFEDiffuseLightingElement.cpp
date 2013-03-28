/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEDiffuseLightingElement.h"
#include "mozilla/dom/SVGFEDiffuseLightingElementBinding.h"
#include "nsSVGUtils.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEDiffuseLighting)

namespace mozilla {
namespace dom {

JSObject*
SVGFEDiffuseLightingElement::WrapNode(JSContext* aCx, JSObject* aScope)
{
  return SVGFEDiffuseLightingElementBinding::Wrap(aCx, aScope, this);
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEDiffuseLightingElement)

//----------------------------------------------------------------------

already_AddRefed<nsIDOMSVGAnimatedString>
SVGFEDiffuseLightingElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFEDiffuseLightingElement::SurfaceScale()
{
  return mNumberAttributes[SURFACE_SCALE].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFEDiffuseLightingElement::DiffuseConstant()
{
  return mNumberAttributes[DIFFUSE_CONSTANT].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFEDiffuseLightingElement::KernelUnitLengthX()
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(
    nsSVGNumberPair::eFirst, this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFEDiffuseLightingElement::KernelUnitLengthY()
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(
    nsSVGNumberPair::eSecond, this);
}

bool
SVGFEDiffuseLightingElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                       nsIAtom* aAttribute) const
{
  return SVGFEDiffuseLightingElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          aAttribute == nsGkAtoms::diffuseConstant);
}

//----------------------------------------------------------------------
// nsSVGElement methods

void
SVGFEDiffuseLightingElement::LightPixel(const float *N, const float *L,
                                        nscolor color, uint8_t *targetData)
{
  float diffuseNL =
    mNumberAttributes[DIFFUSE_CONSTANT].GetAnimValue() * DOT(N, L);

  if (diffuseNL < 0) diffuseNL = 0;

  targetData[GFX_ARGB32_OFFSET_B] =
    std::min(uint32_t(diffuseNL * NS_GET_B(color)), 255U);
  targetData[GFX_ARGB32_OFFSET_G] =
    std::min(uint32_t(diffuseNL * NS_GET_G(color)), 255U);
  targetData[GFX_ARGB32_OFFSET_R] =
    std::min(uint32_t(diffuseNL * NS_GET_R(color)), 255U);
  targetData[GFX_ARGB32_OFFSET_A] = 255;
}

} // namespace dom
} // namespace mozilla
