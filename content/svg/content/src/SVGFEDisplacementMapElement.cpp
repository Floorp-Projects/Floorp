/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEDisplacementMapElement.h"
#include "mozilla/dom/SVGFEDisplacementMapElementBinding.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGUtils.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEDisplacementMap)

namespace mozilla {
namespace dom {

// Channel Selectors
static const unsigned short SVG_CHANNEL_UNKNOWN = 0;
static const unsigned short SVG_CHANNEL_R = 1;
static const unsigned short SVG_CHANNEL_G = 2;
static const unsigned short SVG_CHANNEL_B = 3;
static const unsigned short SVG_CHANNEL_A = 4;

JSObject*
SVGFEDisplacementMapElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEDisplacementMapElementBinding::Wrap(aCx, aScope, this);
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
// nsIDOMNode methods

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

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFEDisplacementMapElement::Scale()
{
  return mNumberAttributes[SCALE].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedEnumeration>
SVGFEDisplacementMapElement::XChannelSelector()
{
  return mEnumAttributes[CHANNEL_X].ToDOMAnimatedEnum(this);
}

already_AddRefed<nsIDOMSVGAnimatedEnumeration>
SVGFEDisplacementMapElement::YChannelSelector()
{
  return mEnumAttributes[CHANNEL_Y].ToDOMAnimatedEnum(this);
}

nsresult
SVGFEDisplacementMapElement::Filter(nsSVGFilterInstance* instance,
                                    const nsTArray<const Image*>& aSources,
                                    const Image* aTarget,
                                    const nsIntRect& rect)
{
  float scale = instance->GetPrimitiveNumber(SVGContentUtils::XY,
                                             &mNumberAttributes[SCALE]);
  if (scale == 0.0f) {
    CopyRect(aTarget, aSources[0], rect);
    return NS_OK;
  }

  int32_t width = instance->GetSurfaceWidth();
  int32_t height = instance->GetSurfaceHeight();

  uint8_t* sourceData = aSources[0]->mImage->Data();
  uint8_t* displacementData = aSources[1]->mImage->Data();
  uint8_t* targetData = aTarget->mImage->Data();
  uint32_t stride = aTarget->mImage->Stride();

  static const uint8_t dummyData[4] = { 0, 0, 0, 0 };

  static const uint16_t channelMap[5] = {
                             0,
                             GFX_ARGB32_OFFSET_R,
                             GFX_ARGB32_OFFSET_G,
                             GFX_ARGB32_OFFSET_B,
                             GFX_ARGB32_OFFSET_A };
  uint16_t xChannel = channelMap[mEnumAttributes[CHANNEL_X].GetAnimValue()];
  uint16_t yChannel = channelMap[mEnumAttributes[CHANNEL_Y].GetAnimValue()];

  double scaleOver255 = scale / 255.0;
  double scaleAdjustment = 0.5 - 0.5 * scale;

  for (int32_t y = rect.y; y < rect.YMost(); y++) {
    for (int32_t x = rect.x; x < rect.XMost(); x++) {
      uint32_t targIndex = y * stride + 4 * x;
      // At some point we might want to replace this with a bilinear sample.
      int32_t sourceX = x +
        NSToIntFloor(scaleOver255 * displacementData[targIndex + xChannel] +
                scaleAdjustment);
      int32_t sourceY = y +
        NSToIntFloor(scaleOver255 * displacementData[targIndex + yChannel] +
                scaleAdjustment);

      bool outOfBounds = sourceX < 0 || sourceX >= width ||
                         sourceY < 0 || sourceY >= height;
      const uint8_t* data;
      int32_t multiplier;
      if (outOfBounds) {
        data = dummyData;
        multiplier = 0;
      } else {
        data = sourceData;
        multiplier = 1;
      }
      *(uint32_t*)(targetData + targIndex) =
        *(uint32_t*)(data + multiplier * (sourceY * stride + 4 * sourceX));
    }
  }
  return NS_OK;
}

bool
SVGFEDisplacementMapElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                       nsIAtom* aAttribute) const
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

nsIntRect
SVGFEDisplacementMapElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance)
{
  // XXX we could do something clever here involving analysis of 'scale'
  // to figure out the maximum displacement, and then return mIn1's bounds
  // adjusted for the maximum displacement
  return GetMaxRect();
}

void
SVGFEDisplacementMapElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  // in2 contains the displacements, which we read for each target pixel
  aSourceBBoxes[1] = aTargetBBox;
  // XXX to figure out which parts of 'in' we might read, we could
  // do some analysis of 'scale' to figure out the maximum displacement.
  // For now, just leave aSourceBBoxes[0] alone, i.e. assume we use its
  // entire output bounding box.
  // If we change this, we need to change coordinate assumptions above
}

nsIntRect
SVGFEDisplacementMapElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                               const nsSVGFilterInstance& aInstance)
{
  // XXX we could do something clever here involving analysis of 'scale'
  // to figure out the maximum displacement
  return GetMaxRect();
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
