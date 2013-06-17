/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEBlendElement.h"
#include "mozilla/dom/SVGFEBlendElementBinding.h"
#include "nsSVGUtils.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEBlend)

namespace mozilla {
namespace dom {

JSObject*
SVGFEBlendElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEBlendElementBinding::Wrap(aCx, aScope, this);
}

nsSVGEnumMapping SVGFEBlendElement::sModeMap[] = {
  {&nsGkAtoms::normal, SVG_FEBLEND_MODE_NORMAL},
  {&nsGkAtoms::multiply, SVG_FEBLEND_MODE_MULTIPLY},
  {&nsGkAtoms::screen, SVG_FEBLEND_MODE_SCREEN},
  {&nsGkAtoms::darken, SVG_FEBLEND_MODE_DARKEN},
  {&nsGkAtoms::lighten, SVG_FEBLEND_MODE_LIGHTEN},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGFEBlendElement::sEnumInfo[1] =
{
  { &nsGkAtoms::mode,
    sModeMap,
    SVG_FEBLEND_MODE_NORMAL
  }
};

nsSVGElement::StringInfo SVGFEBlendElement::sStringInfo[3] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true },
  { &nsGkAtoms::in2, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEBlendElement)

//----------------------------------------------------------------------
// nsIDOMSVGFEBlendElement methods

already_AddRefed<SVGAnimatedString>
SVGFEBlendElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<SVGAnimatedString>
SVGFEBlendElement::In2()
{
  return mStringAttributes[IN2].ToDOMAnimatedString(this);
}

already_AddRefed<nsIDOMSVGAnimatedEnumeration>
SVGFEBlendElement::Mode()
{
  return mEnumAttributes[MODE].ToDOMAnimatedEnum(this);
}

nsresult
SVGFEBlendElement::Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& rect)
{
  CopyRect(aTarget, aSources[0], rect);

  uint8_t* sourceData = aSources[1]->mImage->Data();
  uint8_t* targetData = aTarget->mImage->Data();
  uint32_t stride = aTarget->mImage->Stride();

  uint16_t mode = mEnumAttributes[MODE].GetAnimValue();

  for (int32_t x = rect.x; x < rect.XMost(); x++) {
    for (int32_t y = rect.y; y < rect.YMost(); y++) {
      uint32_t targIndex = y * stride + 4 * x;
      uint32_t qa = targetData[targIndex + GFX_ARGB32_OFFSET_A];
      uint32_t qb = sourceData[targIndex + GFX_ARGB32_OFFSET_A];
      for (int32_t i = std::min(GFX_ARGB32_OFFSET_B, GFX_ARGB32_OFFSET_R);
           i <= std::max(GFX_ARGB32_OFFSET_B, GFX_ARGB32_OFFSET_R); i++) {
        uint32_t ca = targetData[targIndex + i];
        uint32_t cb = sourceData[targIndex + i];
        uint32_t val;
        switch (mode) {
          case SVG_FEBLEND_MODE_NORMAL:
            val = (255 - qa) * cb + 255 * ca;
            break;
          case SVG_FEBLEND_MODE_MULTIPLY:
            val = ((255 - qa) * cb + (255 - qb + cb) * ca);
            break;
          case SVG_FEBLEND_MODE_SCREEN:
            val = 255 * (cb + ca) - ca * cb;
            break;
          case SVG_FEBLEND_MODE_DARKEN:
            val = std::min((255 - qa) * cb + 255 * ca,
                         (255 - qb) * ca + 255 * cb);
            break;
          case SVG_FEBLEND_MODE_LIGHTEN:
            val = std::max((255 - qa) * cb + 255 * ca,
                         (255 - qb) * ca + 255 * cb);
            break;
          default:
            return NS_ERROR_FAILURE;
            break;
        }
        val = std::min(val / 255, 255U);
        targetData[targIndex + i] =  static_cast<uint8_t>(val);
      }
      uint32_t alpha = 255 * 255 - (255 - qa) * (255 - qb);
      FAST_DIVIDE_BY_255(targetData[targIndex + GFX_ARGB32_OFFSET_A], alpha);
    }
  }
  return NS_OK;
}

bool
SVGFEBlendElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                             nsIAtom* aAttribute) const
{
  return SVGFEBlendElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::in2 ||
           aAttribute == nsGkAtoms::mode));
}

void
SVGFEBlendElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN2], this));
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::EnumAttributesInfo
SVGFEBlendElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
SVGFEBlendElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
