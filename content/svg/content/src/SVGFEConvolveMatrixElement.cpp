/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEConvolveMatrixElement.h"
#include "mozilla/dom/SVGFEConvolveMatrixElementBinding.h"
#include "DOMSVGAnimatedNumberList.h"
#include "nsSVGUtils.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEConvolveMatrix)

namespace mozilla {
namespace dom {

// Edge Mode Values
static const unsigned short SVG_EDGEMODE_UNKNOWN = 0;
static const unsigned short SVG_EDGEMODE_DUPLICATE = 1;
static const unsigned short SVG_EDGEMODE_WRAP = 2;
static const unsigned short SVG_EDGEMODE_NONE = 3;

JSObject*
SVGFEConvolveMatrixElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEConvolveMatrixElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::NumberInfo SVGFEConvolveMatrixElement::sNumberInfo[2] =
{
  { &nsGkAtoms::divisor, 1, false },
  { &nsGkAtoms::bias, 0, false }
};

nsSVGElement::NumberPairInfo SVGFEConvolveMatrixElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::kernelUnitLength, 0, 0 }
};

nsSVGElement::IntegerInfo SVGFEConvolveMatrixElement::sIntegerInfo[2] =
{
  { &nsGkAtoms::targetX, 0 },
  { &nsGkAtoms::targetY, 0 }
};

nsSVGElement::IntegerPairInfo SVGFEConvolveMatrixElement::sIntegerPairInfo[1] =
{
  { &nsGkAtoms::order, 3, 3 }
};

nsSVGElement::BooleanInfo SVGFEConvolveMatrixElement::sBooleanInfo[1] =
{
  { &nsGkAtoms::preserveAlpha, false }
};

nsSVGEnumMapping SVGFEConvolveMatrixElement::sEdgeModeMap[] = {
  {&nsGkAtoms::duplicate, SVG_EDGEMODE_DUPLICATE},
  {&nsGkAtoms::wrap, SVG_EDGEMODE_WRAP},
  {&nsGkAtoms::none, SVG_EDGEMODE_NONE},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGFEConvolveMatrixElement::sEnumInfo[1] =
{
  { &nsGkAtoms::edgeMode,
    sEdgeModeMap,
    SVG_EDGEMODE_DUPLICATE
  }
};

nsSVGElement::StringInfo SVGFEConvolveMatrixElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

nsSVGElement::NumberListInfo SVGFEConvolveMatrixElement::sNumberListInfo[1] =
{
  { &nsGkAtoms::kernelMatrix }
};

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEConvolveMatrixElement)

//----------------------------------------------------------------------

already_AddRefed<nsIDOMSVGAnimatedString>
SVGFEConvolveMatrixElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<nsIDOMSVGAnimatedInteger>
SVGFEConvolveMatrixElement::OrderX()
{
  return mIntegerPairAttributes[ORDER].ToDOMAnimatedInteger(nsSVGIntegerPair::eFirst, this);
}

already_AddRefed<nsIDOMSVGAnimatedInteger>
SVGFEConvolveMatrixElement::OrderY()
{
  return mIntegerPairAttributes[ORDER].ToDOMAnimatedInteger(nsSVGIntegerPair::eSecond, this);
}

already_AddRefed<DOMSVGAnimatedNumberList>
SVGFEConvolveMatrixElement::KernelMatrix()
{
  return DOMSVGAnimatedNumberList::GetDOMWrapper(&mNumberListAttributes[KERNELMATRIX],
                                                 this, KERNELMATRIX);
}

already_AddRefed<nsIDOMSVGAnimatedInteger>
SVGFEConvolveMatrixElement::TargetX()
{
  return mIntegerAttributes[TARGET_X].ToDOMAnimatedInteger(this);
}

already_AddRefed<nsIDOMSVGAnimatedInteger>
SVGFEConvolveMatrixElement::TargetY()
{
  return mIntegerAttributes[TARGET_Y].ToDOMAnimatedInteger(this);
}

already_AddRefed<nsIDOMSVGAnimatedEnumeration>
SVGFEConvolveMatrixElement::EdgeMode()
{
  return mEnumAttributes[EDGEMODE].ToDOMAnimatedEnum(this);
}

already_AddRefed<SVGAnimatedBoolean>
SVGFEConvolveMatrixElement::PreserveAlpha()
{
  return mBooleanAttributes[PRESERVEALPHA].ToDOMAnimatedBoolean(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFEConvolveMatrixElement::Divisor()
{
  return mNumberAttributes[DIVISOR].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFEConvolveMatrixElement::Bias()
{
  return mNumberAttributes[BIAS].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFEConvolveMatrixElement::KernelUnitLengthX()
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(nsSVGNumberPair::eFirst,
                                                                       this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFEConvolveMatrixElement::KernelUnitLengthY()
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(nsSVGNumberPair::eSecond,
                                                                       this);
}

void
SVGFEConvolveMatrixElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

nsIntRect
SVGFEConvolveMatrixElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  // XXX A more precise box is possible when 'bias' is zero and 'edgeMode' is
  // 'none', but it requires analysis of 'kernelUnitLength', 'order' and
  // 'targetX/Y', so it's quite a lot of work. Don't do it for now.
  return GetMaxRect();
}

void
SVGFEConvolveMatrixElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  // XXX Precise results are possible but we're going to skip that work
  // for now. Do nothing, which means the needed-box remains the
  // source's output bounding box.
}

nsIntRect
SVGFEConvolveMatrixElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                              const nsSVGFilterInstance& aInstance)
{
  // XXX Precise results are possible but we're going to skip that work
  // for now.
  return GetMaxRect();
}

static int32_t BoundInterval(int32_t aVal, int32_t aMax)
{
  aVal = std::max(aVal, 0);
  return std::min(aVal, aMax - 1);
}

static int32_t WrapInterval(int32_t aVal, int32_t aMax)
{
  aVal = aVal % aMax;
  return aVal < 0 ? aMax + aVal : aVal;
}

static void
ConvolvePixel(const uint8_t *aSourceData,
              uint8_t *aTargetData,
              int32_t aWidth, int32_t aHeight,
              int32_t aStride,
              int32_t aX, int32_t aY,
              uint16_t aEdgeMode,
              const float *aKernel,
              float aDivisor, float aBias,
              bool aPreserveAlpha,
              int32_t aOrderX, int32_t aOrderY,
              int32_t aTargetX, int32_t aTargetY)
{
  float sum[4] = {0, 0, 0, 0};
  aBias *= 255;
  int32_t offsets[4] = {GFX_ARGB32_OFFSET_R,
                        GFX_ARGB32_OFFSET_G,
                        GFX_ARGB32_OFFSET_B,
                        GFX_ARGB32_OFFSET_A } ;
  int32_t channels = aPreserveAlpha ? 3 : 4;

  for (int32_t y = 0; y < aOrderY; y++) {
    int32_t sampleY = aY + y - aTargetY;
    bool overscanY = sampleY < 0 || sampleY >= aHeight;
    for (int32_t x = 0; x < aOrderX; x++) {
      int32_t sampleX = aX + x - aTargetX;
      bool overscanX = sampleX < 0 || sampleX >= aWidth;
      for (int32_t i = 0; i < channels; i++) {
        if (overscanY || overscanX) {
          switch (aEdgeMode) {
            case SVG_EDGEMODE_DUPLICATE:
              sum[i] +=
                aSourceData[BoundInterval(sampleY, aHeight) * aStride +
                            BoundInterval(sampleX, aWidth) * 4 + offsets[i]] *
                aKernel[aOrderX * y + x];
              break;
            case SVG_EDGEMODE_WRAP:
              sum[i] +=
                aSourceData[WrapInterval(sampleY, aHeight) * aStride +
                            WrapInterval(sampleX, aWidth) * 4 + offsets[i]] *
                aKernel[aOrderX * y + x];
              break;
            default:
              break;
          }
        } else {
          sum[i] +=
            aSourceData[sampleY * aStride + 4 * sampleX + offsets[i]] *
            aKernel[aOrderX * y + x];
        }
      }
    }
  }
  for (int32_t i = 0; i < channels; i++) {
    aTargetData[aY * aStride + 4 * aX + offsets[i]] =
      BoundInterval(static_cast<int32_t>(sum[i] / aDivisor + aBias), 256);
  }
  if (aPreserveAlpha) {
    aTargetData[aY * aStride + 4 * aX + GFX_ARGB32_OFFSET_A] =
      aSourceData[aY * aStride + 4 * aX + GFX_ARGB32_OFFSET_A];
  }
}

nsresult
SVGFEConvolveMatrixElement::Filter(nsSVGFilterInstance* instance,
                                   const nsTArray<const Image*>& aSources,
                                   const Image* aTarget,
                                   const nsIntRect& rect)
{
  const SVGNumberList &kernelMatrix =
    mNumberListAttributes[KERNELMATRIX].GetAnimValue();
  uint32_t kmLength = kernelMatrix.Length();

  int32_t orderX = mIntegerPairAttributes[ORDER].GetAnimValue(nsSVGIntegerPair::eFirst);
  int32_t orderY = mIntegerPairAttributes[ORDER].GetAnimValue(nsSVGIntegerPair::eSecond);

  if (orderX <= 0 || orderY <= 0 ||
      static_cast<uint32_t>(orderX * orderY) != kmLength) {
    return NS_ERROR_FAILURE;
  }

  int32_t targetX, targetY;
  GetAnimatedIntegerValues(&targetX, &targetY, nullptr);

  if (mIntegerAttributes[TARGET_X].IsExplicitlySet()) {
    if (targetX < 0 || targetX >= orderX)
      return NS_ERROR_FAILURE;
  } else {
    targetX = orderX / 2;
  }
  if (mIntegerAttributes[TARGET_Y].IsExplicitlySet()) {
    if (targetY < 0 || targetY >= orderY)
      return NS_ERROR_FAILURE;
  } else {
    targetY = orderY / 2;
  }

  if (orderX > NS_SVG_OFFSCREEN_MAX_DIMENSION ||
      orderY > NS_SVG_OFFSCREEN_MAX_DIMENSION)
    return NS_ERROR_FAILURE;
  nsAutoArrayPtr<float> kernel(new float[orderX * orderY]);
  if (!kernel)
    return NS_ERROR_FAILURE;
  for (uint32_t i = 0; i < kmLength; i++) {
    kernel[kmLength - 1 - i] = kernelMatrix[i];
  }

  float divisor;
  if (mNumberAttributes[DIVISOR].IsExplicitlySet()) {
    divisor = mNumberAttributes[DIVISOR].GetAnimValue();
    if (divisor == 0)
      return NS_ERROR_FAILURE;
  } else {
    divisor = kernel[0];
    for (uint32_t i = 1; i < kmLength; i++)
      divisor += kernel[i];
    if (divisor == 0)
      divisor = 1;
  }

  ScaleInfo info = SetupScalingFilter(instance, aSources[0], aTarget, rect,
                                      &mNumberPairAttributes[KERNEL_UNIT_LENGTH]);
  if (!info.mTarget)
    return NS_ERROR_FAILURE;

  uint16_t edgeMode = mEnumAttributes[EDGEMODE].GetAnimValue();
  bool preserveAlpha = mBooleanAttributes[PRESERVEALPHA].GetAnimValue();

  float bias = mNumberAttributes[BIAS].GetAnimValue();

  const nsIntRect& dataRect = info.mDataRect;
  int32_t stride = info.mSource->Stride();
  int32_t width = info.mSource->GetSize().width;
  int32_t height = info.mSource->GetSize().height;
  uint8_t *sourceData = info.mSource->Data();
  uint8_t *targetData = info.mTarget->Data();

  for (int32_t y = dataRect.y; y < dataRect.YMost(); y++) {
    for (int32_t x = dataRect.x; x < dataRect.XMost(); x++) {
      ConvolvePixel(sourceData, targetData,
                    width, height, stride,
                    x, y,
                    edgeMode, kernel, divisor, bias, preserveAlpha,
                    orderX, orderY, targetX, targetY);
    }
  }

  FinishScalingFilter(&info);

  return NS_OK;
}

bool
SVGFEConvolveMatrixElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                      nsIAtom* aAttribute) const
{
  return SVGFEConvolveMatrixElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::divisor ||
           aAttribute == nsGkAtoms::bias ||
           aAttribute == nsGkAtoms::kernelUnitLength ||
           aAttribute == nsGkAtoms::targetX ||
           aAttribute == nsGkAtoms::targetY ||
           aAttribute == nsGkAtoms::order ||
           aAttribute == nsGkAtoms::preserveAlpha||
           aAttribute == nsGkAtoms::edgeMode ||
           aAttribute == nsGkAtoms::kernelMatrix));
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
SVGFEConvolveMatrixElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

nsSVGElement::NumberPairAttributesInfo
SVGFEConvolveMatrixElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  ArrayLength(sNumberPairInfo));
}

nsSVGElement::IntegerAttributesInfo
SVGFEConvolveMatrixElement::GetIntegerInfo()
{
  return IntegerAttributesInfo(mIntegerAttributes, sIntegerInfo,
                               ArrayLength(sIntegerInfo));
}

nsSVGElement::IntegerPairAttributesInfo
SVGFEConvolveMatrixElement::GetIntegerPairInfo()
{
  return IntegerPairAttributesInfo(mIntegerPairAttributes, sIntegerPairInfo,
                                   ArrayLength(sIntegerPairInfo));
}

nsSVGElement::BooleanAttributesInfo
SVGFEConvolveMatrixElement::GetBooleanInfo()
{
  return BooleanAttributesInfo(mBooleanAttributes, sBooleanInfo,
                               ArrayLength(sBooleanInfo));
}

nsSVGElement::EnumAttributesInfo
SVGFEConvolveMatrixElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
SVGFEConvolveMatrixElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

nsSVGElement::NumberListAttributesInfo
SVGFEConvolveMatrixElement::GetNumberListInfo()
{
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  ArrayLength(sNumberListInfo));
}

} // namespace dom
} // namespace mozilla
