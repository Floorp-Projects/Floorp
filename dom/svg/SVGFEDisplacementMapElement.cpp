/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEDisplacementMapElement.h"
#include "mozilla/dom/SVGFEDisplacementMapElementBinding.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGUtils.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEDisplacementMap)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGFEDisplacementMapElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGFEDisplacementMapElementBinding::Wrap(aCx, this, aGivenProto);
}

nsSVGElement::NumberInfo SVGFEDisplacementMapElement::sNumberInfo[1] =
{
  { &nsGkAtoms::scale, 0, false },
};

nsSVGEnumMapping SVGFEDisplacementMapElement::sChannelMap[] = {
  {&nsGkAtoms::R, SVG_CHANNEL_R},
  {&nsGkAtoms::G, SVG_CHANNEL_G},
  {&nsGkAtoms::B, SVG_CHANNEL_B},
  {&nsGkAtoms::A, SVG_CHANNEL_A},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGFEDisplacementMapElement::sEnumInfo[2] =
{
  { &nsGkAtoms::xChannelSelector,
    sChannelMap,
    SVG_CHANNEL_A
  },
  { &nsGkAtoms::yChannelSelector,
    sChannelMap,
    SVG_CHANNEL_A
  }
};

nsSVGElement::StringInfo SVGFEDisplacementMapElement::sStringInfo[3] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true },
  { &nsGkAtoms::in2, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEDisplacementMapElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedString>
SVGFEDisplacementMapElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<SVGAnimatedString>
SVGFEDisplacementMapElement::In2()
{
  return mStringAttributes[IN2].ToDOMAnimatedString(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEDisplacementMapElement::Scale()
{
  return mNumberAttributes[SCALE].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedEnumeration>
SVGFEDisplacementMapElement::XChannelSelector()
{
  return mEnumAttributes[CHANNEL_X].ToDOMAnimatedEnum(this);
}

already_AddRefed<SVGAnimatedEnumeration>
SVGFEDisplacementMapElement::YChannelSelector()
{
  return mEnumAttributes[CHANNEL_Y].ToDOMAnimatedEnum(this);
}

FilterPrimitiveDescription
SVGFEDisplacementMapElement::GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                                                     const IntRect& aFilterSubregion,
                                                     const nsTArray<bool>& aInputsAreTainted,
                                                     nsTArray<RefPtr<SourceSurface>>& aInputImages)
{
  if (aInputsAreTainted[1]) {
    // If the map is tainted, refuse to apply the effect and act as a
    // pass-through filter instead, as required by the spec.
    FilterPrimitiveDescription descr(PrimitiveType::Offset);
    descr.Attributes().Set(eOffsetOffset, IntPoint(0, 0));
    return descr;
  }

  float scale = aInstance->GetPrimitiveNumber(SVGContentUtils::XY,
                                              &mNumberAttributes[SCALE]);
  uint32_t xChannel = mEnumAttributes[CHANNEL_X].GetAnimValue();
  uint32_t yChannel = mEnumAttributes[CHANNEL_Y].GetAnimValue();
  FilterPrimitiveDescription descr(PrimitiveType::DisplacementMap);
  descr.Attributes().Set(eDisplacementMapScale, scale);
  descr.Attributes().Set(eDisplacementMapXChannel, xChannel);
  descr.Attributes().Set(eDisplacementMapYChannel, yChannel);
  return descr;
}

bool
SVGFEDisplacementMapElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                       nsAtom* aAttribute) const
{
  return SVGFEDisplacementMapElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::in2 ||
           aAttribute == nsGkAtoms::scale ||
           aAttribute == nsGkAtoms::xChannelSelector ||
           aAttribute == nsGkAtoms::yChannelSelector));
}

void
SVGFEDisplacementMapElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN2], this));
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
SVGFEDisplacementMapElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

nsSVGElement::EnumAttributesInfo
SVGFEDisplacementMapElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
SVGFEDisplacementMapElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
