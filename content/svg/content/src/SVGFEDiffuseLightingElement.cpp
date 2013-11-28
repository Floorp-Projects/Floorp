/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEDiffuseLightingElement.h"
#include "mozilla/dom/SVGFEDiffuseLightingElementBinding.h"
#include "nsSVGUtils.h"
#include "nsSVGFilterInstance.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEDiffuseLighting)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGFEDiffuseLightingElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEDiffuseLightingElementBinding::Wrap(aCx, aScope, this);
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEDiffuseLightingElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedString>
SVGFEDiffuseLightingElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEDiffuseLightingElement::SurfaceScale()
{
  return mNumberAttributes[SURFACE_SCALE].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEDiffuseLightingElement::DiffuseConstant()
{
  return mNumberAttributes[DIFFUSE_CONSTANT].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEDiffuseLightingElement::KernelUnitLengthX()
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(
    nsSVGNumberPair::eFirst, this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEDiffuseLightingElement::KernelUnitLengthY()
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(
    nsSVGNumberPair::eSecond, this);
}

FilterPrimitiveDescription
SVGFEDiffuseLightingElement::GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                                                     const IntRect& aFilterSubregion,
                                                     nsTArray<nsRefPtr<gfxASurface> >& aInputImages)
{
  float diffuseConstant = mNumberAttributes[DIFFUSE_CONSTANT].GetAnimValue();

  FilterPrimitiveDescription descr(FilterPrimitiveDescription::eDiffuseLighting);
  descr.Attributes().Set(eDiffuseLightingDiffuseConstant, diffuseConstant);
  return AddLightingAttributes(descr, aInstance);
}

bool
SVGFEDiffuseLightingElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                       nsIAtom* aAttribute) const
{
  return SVGFEDiffuseLightingElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          aAttribute == nsGkAtoms::diffuseConstant);
}

} // namespace dom
} // namespace mozilla
