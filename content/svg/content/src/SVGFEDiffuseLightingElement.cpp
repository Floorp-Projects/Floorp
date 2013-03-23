/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEDiffuseLightingElement.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FEDiffuseLighting)

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEDiffuseLightingElement,nsSVGFEDiffuseLightingElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEDiffuseLightingElement,nsSVGFEDiffuseLightingElementBase)

DOMCI_NODE_DATA(SVGFEDiffuseLightingElement, nsSVGFEDiffuseLightingElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEDiffuseLightingElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEDiffuseLightingElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEDiffuseLightingElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEDiffuseLightingElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEDiffuseLightingElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEDiffuseLightingElement)

//----------------------------------------------------------------------
// nsSVGFEDiffuseLightingElement methods

NS_IMETHODIMP
nsSVGFEDiffuseLightingElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

NS_IMETHODIMP
nsSVGFEDiffuseLightingElement::GetSurfaceScale(nsIDOMSVGAnimatedNumber **aScale)
{
  return mNumberAttributes[SURFACE_SCALE].ToDOMAnimatedNumber(aScale,
                                                              this);
}

NS_IMETHODIMP
nsSVGFEDiffuseLightingElement::GetDiffuseConstant(nsIDOMSVGAnimatedNumber **aConstant)
{
  return mNumberAttributes[DIFFUSE_CONSTANT].ToDOMAnimatedNumber(aConstant,
                                                              this);
}

NS_IMETHODIMP
nsSVGFEDiffuseLightingElement::GetKernelUnitLengthX(nsIDOMSVGAnimatedNumber **aKernelX)
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(aKernelX,
                                                                       nsSVGNumberPair::eFirst,
                                                                       this);
}

NS_IMETHODIMP
nsSVGFEDiffuseLightingElement::GetKernelUnitLengthY(nsIDOMSVGAnimatedNumber **aKernelY)
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(aKernelY,
                                                                       nsSVGNumberPair::eSecond,
                                                                       this);
}

bool
nsSVGFEDiffuseLightingElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                         nsIAtom* aAttribute) const
{
  return nsSVGFEDiffuseLightingElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          aAttribute == nsGkAtoms::diffuseConstant);
}

//----------------------------------------------------------------------
// nsSVGElement methods

void
nsSVGFEDiffuseLightingElement::LightPixel(const float *N, const float *L,
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
