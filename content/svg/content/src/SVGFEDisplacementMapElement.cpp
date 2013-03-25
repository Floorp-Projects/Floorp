/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEDisplacementMapElement.h"

namespace mozilla {
namespace dom {

nsSVGElement::NumberInfo nsSVGFEDisplacementMapElement::sNumberInfo[1] =
{
  { &nsGkAtoms::scale, 0, false },
};

nsSVGEnumMapping nsSVGFEDisplacementMapElement::sChannelMap[] = {
  {&nsGkAtoms::R, nsSVGFEDisplacementMapElement::SVG_CHANNEL_R},
  {&nsGkAtoms::G, nsSVGFEDisplacementMapElement::SVG_CHANNEL_G},
  {&nsGkAtoms::B, nsSVGFEDisplacementMapElement::SVG_CHANNEL_B},
  {&nsGkAtoms::A, nsSVGFEDisplacementMapElement::SVG_CHANNEL_A},
  {nullptr, 0}
};

nsSVGElement::EnumInfo nsSVGFEDisplacementMapElement::sEnumInfo[2] =
{
  { &nsGkAtoms::xChannelSelector,
    sChannelMap,
    nsSVGFEDisplacementMapElement::SVG_CHANNEL_A
  },
  { &nsGkAtoms::yChannelSelector,
    sChannelMap,
    nsSVGFEDisplacementMapElement::SVG_CHANNEL_A
  }
};

nsSVGElement::StringInfo nsSVGFEDisplacementMapElement::sStringInfo[3] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true },
  { &nsGkAtoms::in2, kNameSpaceID_None, true }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEDisplacementMap)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEDisplacementMapElement,nsSVGFEDisplacementMapElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEDisplacementMapElement,nsSVGFEDisplacementMapElementBase)

DOMCI_NODE_DATA(SVGFEDisplacementMapElement, nsSVGFEDisplacementMapElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEDisplacementMapElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEDisplacementMapElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEDisplacementMapElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEDisplacementMapElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEDisplacementMapElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEDisplacementMapElement)

//----------------------------------------------------------------------
// nsIDOMSVGFEDisplacementMapElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEDisplacementMapElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

/* readonly attribute nsIDOMSVGAnimatedString in2; */
NS_IMETHODIMP nsSVGFEDisplacementMapElement::GetIn2(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN2].ToDOMAnimatedString(aIn, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber scale; */
NS_IMETHODIMP nsSVGFEDisplacementMapElement::GetScale(nsIDOMSVGAnimatedNumber * *aScale)
{
  return mNumberAttributes[SCALE].ToDOMAnimatedNumber(aScale, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration xChannelSelector; */
NS_IMETHODIMP nsSVGFEDisplacementMapElement::GetXChannelSelector(nsIDOMSVGAnimatedEnumeration * *aChannel)
{
  return mEnumAttributes[CHANNEL_X].ToDOMAnimatedEnum(aChannel, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration yChannelSelector; */
NS_IMETHODIMP nsSVGFEDisplacementMapElement::GetYChannelSelector(nsIDOMSVGAnimatedEnumeration * *aChannel)
{
  return mEnumAttributes[CHANNEL_Y].ToDOMAnimatedEnum(aChannel, this);
}

nsresult
nsSVGFEDisplacementMapElement::Filter(nsSVGFilterInstance *instance,
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
nsSVGFEDisplacementMapElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                         nsIAtom* aAttribute) const
{
  return nsSVGFEDisplacementMapElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::in2 ||
           aAttribute == nsGkAtoms::scale ||
           aAttribute == nsGkAtoms::xChannelSelector ||
           aAttribute == nsGkAtoms::yChannelSelector));
}

void
nsSVGFEDisplacementMapElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN2], this));
}

nsIntRect
nsSVGFEDisplacementMapElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance)
{
  // XXX we could do something clever here involving analysis of 'scale'
  // to figure out the maximum displacement, and then return mIn1's bounds
  // adjusted for the maximum displacement
  return GetMaxRect();
}

void
nsSVGFEDisplacementMapElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
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
nsSVGFEDisplacementMapElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                                 const nsSVGFilterInstance& aInstance)
{
  // XXX we could do something clever here involving analysis of 'scale'
  // to figure out the maximum displacement
  return GetMaxRect();
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFEDisplacementMapElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

nsSVGElement::EnumAttributesInfo
nsSVGFEDisplacementMapElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGFEDisplacementMapElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}
