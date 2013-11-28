/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEPointLightElement.h"
#include "mozilla/dom/SVGFEPointLightElementBinding.h"
#include "nsSVGFilterInstance.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEPointLight)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGFEPointLightElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEPointLightElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::NumberInfo SVGFEPointLightElement::sNumberInfo[3] =
{
  { &nsGkAtoms::x, 0, false },
  { &nsGkAtoms::y, 0, false },
  { &nsGkAtoms::z, 0, false }
};

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEPointLightElement)

//----------------------------------------------------------------------
// nsFEUnstyledElement methods

bool
SVGFEPointLightElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                  nsIAtom* aAttribute) const
{
  return aNameSpaceID == kNameSpaceID_None &&
         (aAttribute == nsGkAtoms::x ||
          aAttribute == nsGkAtoms::y ||
          aAttribute == nsGkAtoms::z);
}

//----------------------------------------------------------------------

AttributeMap
SVGFEPointLightElement::ComputeLightAttributes(nsSVGFilterInstance* aInstance)
{
  Point3D lightPos;
  GetAnimatedNumberValues(&lightPos.x, &lightPos.y, &lightPos.z, nullptr);
  
  AttributeMap map;
  map.Set(eLightType, (uint32_t)eLightTypePoint);
  map.Set(ePointLightPosition, aInstance->ConvertLocation(lightPos));
  return map;
}

already_AddRefed<SVGAnimatedNumber>
SVGFEPointLightElement::X()
{
  return mNumberAttributes[ATTR_X].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEPointLightElement::Y()
{
  return mNumberAttributes[ATTR_Y].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEPointLightElement::Z()
{
  return mNumberAttributes[ATTR_Z].ToDOMAnimatedNumber(this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
SVGFEPointLightElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

} // namespace dom
} // namespace mozilla
