/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGFilters.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FESpecularLighting)

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFESpecularLightingElement,nsSVGFESpecularLightingElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFESpecularLightingElement,nsSVGFESpecularLightingElementBase)

DOMCI_NODE_DATA(SVGFESpecularLightingElement, nsSVGFESpecularLightingElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFESpecularLightingElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFESpecularLightingElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFESpecularLightingElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFESpecularLightingElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFESpecularLightingElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFESpecularLightingElement)

//----------------------------------------------------------------------
// nsSVGFESpecularLightingElement methods

NS_IMETHODIMP
nsSVGFESpecularLightingElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

NS_IMETHODIMP
nsSVGFESpecularLightingElement::GetSurfaceScale(nsIDOMSVGAnimatedNumber **aScale)
{
  return mNumberAttributes[SURFACE_SCALE].ToDOMAnimatedNumber(aScale,
                                                              this);
}

NS_IMETHODIMP
nsSVGFESpecularLightingElement::GetSpecularConstant(nsIDOMSVGAnimatedNumber **aConstant)
{
  return mNumberAttributes[SPECULAR_CONSTANT].ToDOMAnimatedNumber(aConstant,
                                                                  this);
}

NS_IMETHODIMP
nsSVGFESpecularLightingElement::GetSpecularExponent(nsIDOMSVGAnimatedNumber **aExponent)
{
  return mNumberAttributes[SPECULAR_EXPONENT].ToDOMAnimatedNumber(aExponent,
                                                                  this);
}

NS_IMETHODIMP
nsSVGFESpecularLightingElement::GetKernelUnitLengthX(nsIDOMSVGAnimatedNumber **aKernelX)
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(aKernelX,
                                                                       nsSVGNumberPair::eFirst,
                                                                       this);
}

NS_IMETHODIMP
nsSVGFESpecularLightingElement::GetKernelUnitLengthY(nsIDOMSVGAnimatedNumber **aKernelY)
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(aKernelY,
                                                                       nsSVGNumberPair::eSecond,
                                                                       this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsresult
nsSVGFESpecularLightingElement::Filter(nsSVGFilterInstance *instance,
                                       const nsTArray<const Image*>& aSources,
                                       const Image* aTarget,
                                       const nsIntRect& rect)
{
  float specularExponent = mNumberAttributes[SPECULAR_EXPONENT].GetAnimValue();

  // specification defined range (15.22)
  if (specularExponent < 1 || specularExponent > 128)
    return NS_ERROR_FAILURE;

  return nsSVGFESpecularLightingElementBase::Filter(instance, aSources, aTarget, rect);
}

bool
nsSVGFESpecularLightingElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                          nsIAtom* aAttribute) const
{
  return nsSVGFESpecularLightingElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::specularConstant ||
           aAttribute == nsGkAtoms::specularExponent));
}

void
nsSVGFESpecularLightingElement::LightPixel(const float *N, const float *L,
                                           nscolor color, uint8_t *targetData)
{
  float H[3];
  H[0] = L[0];
  H[1] = L[1];
  H[2] = L[2] + 1;
  NORMALIZE(H);

  float kS = mNumberAttributes[SPECULAR_CONSTANT].GetAnimValue();
  float dotNH = DOT(N, H);

  bool invalid = dotNH <= 0 || kS <= 0;
  kS *= invalid ? 0 : 1;
  uint8_t minAlpha = invalid ? 255 : 0;

  float specularNH =
    kS * pow(dotNH, mNumberAttributes[SPECULAR_EXPONENT].GetAnimValue());

  targetData[GFX_ARGB32_OFFSET_B] =
    std::min(uint32_t(specularNH * NS_GET_B(color)), 255U);
  targetData[GFX_ARGB32_OFFSET_G] =
    std::min(uint32_t(specularNH * NS_GET_G(color)), 255U);
  targetData[GFX_ARGB32_OFFSET_R] =
    std::min(uint32_t(specularNH * NS_GET_R(color)), 255U);

  targetData[GFX_ARGB32_OFFSET_A] =
    std::max(minAlpha, std::max(targetData[GFX_ARGB32_OFFSET_B],
                            std::max(targetData[GFX_ARGB32_OFFSET_G],
                                   targetData[GFX_ARGB32_OFFSET_R])));
}

} // namespace dom
} // namespace mozilla
