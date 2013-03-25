/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEConvolveMatrixElement.h"

namespace mozilla {
namespace dom {

nsSVGElement::NumberInfo nsSVGFEConvolveMatrixElement::sNumberInfo[2] =
{
  { &nsGkAtoms::divisor, 1, false },
  { &nsGkAtoms::bias, 0, false }
};

nsSVGElement::NumberPairInfo nsSVGFEConvolveMatrixElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::kernelUnitLength, 0, 0 }
};

nsSVGElement::IntegerInfo nsSVGFEConvolveMatrixElement::sIntegerInfo[2] =
{
  { &nsGkAtoms::targetX, 0 },
  { &nsGkAtoms::targetY, 0 }
};

nsSVGElement::IntegerPairInfo nsSVGFEConvolveMatrixElement::sIntegerPairInfo[1] =
{
  { &nsGkAtoms::order, 3, 3 }
};

nsSVGElement::BooleanInfo nsSVGFEConvolveMatrixElement::sBooleanInfo[1] =
{
  { &nsGkAtoms::preserveAlpha, false }
};

nsSVGEnumMapping nsSVGFEConvolveMatrixElement::sEdgeModeMap[] = {
  {&nsGkAtoms::duplicate, nsSVGFEConvolveMatrixElement::SVG_EDGEMODE_DUPLICATE},
  {&nsGkAtoms::wrap, nsSVGFEConvolveMatrixElement::SVG_EDGEMODE_WRAP},
  {&nsGkAtoms::none, nsSVGFEConvolveMatrixElement::SVG_EDGEMODE_NONE},
  {nullptr, 0}
};

nsSVGElement::EnumInfo nsSVGFEConvolveMatrixElement::sEnumInfo[1] =
{
  { &nsGkAtoms::edgeMode,
    sEdgeModeMap,
    nsSVGFEConvolveMatrixElement::SVG_EDGEMODE_DUPLICATE
  }
};

nsSVGElement::StringInfo nsSVGFEConvolveMatrixElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

nsSVGElement::NumberListInfo nsSVGFEConvolveMatrixElement::sNumberListInfo[1] =
{
  { &nsGkAtoms::kernelMatrix }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEConvolveMatrix)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEConvolveMatrixElement,nsSVGFEConvolveMatrixElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEConvolveMatrixElement,nsSVGFEConvolveMatrixElementBase)

DOMCI_NODE_DATA(SVGFEConvolveMatrixElement, nsSVGFEConvolveMatrixElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEConvolveMatrixElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEConvolveMatrixElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEConvolveMatrixElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEConvolveMatrixElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEConvolveMatrixElementBase)


//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEConvolveMatrixElement)

//----------------------------------------------------------------------
// nsSVGFEConvolveMatrixElement methods

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetOrderX(nsIDOMSVGAnimatedInteger * *aOrderX)
{
  return mIntegerPairAttributes[ORDER].ToDOMAnimatedInteger(aOrderX, nsSVGIntegerPair::eFirst, this);
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetOrderY(nsIDOMSVGAnimatedInteger * *aOrderY)
{
  return mIntegerPairAttributes[ORDER].ToDOMAnimatedInteger(aOrderY, nsSVGIntegerPair::eSecond, this);
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetKernelMatrix(nsISupports * *aKernelMatrix)
{
  *aKernelMatrix = DOMSVGAnimatedNumberList::GetDOMWrapper(&mNumberListAttributes[KERNELMATRIX],
                                                           this, KERNELMATRIX).get();
  return NS_OK;
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetTargetX(nsIDOMSVGAnimatedInteger * *aTargetX)
{
  return mIntegerAttributes[TARGET_X].ToDOMAnimatedInteger(aTargetX, this);
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetTargetY(nsIDOMSVGAnimatedInteger * *aTargetY)
{
  return mIntegerAttributes[TARGET_Y].ToDOMAnimatedInteger(aTargetY, this);
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetEdgeMode(nsIDOMSVGAnimatedEnumeration * *aEdgeMode)
{
  return mEnumAttributes[EDGEMODE].ToDOMAnimatedEnum(aEdgeMode, this);
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetPreserveAlpha(nsISupports * *aPreserveAlpha)
{
  return mBooleanAttributes[PRESERVEALPHA].ToDOMAnimatedBoolean(aPreserveAlpha, this);
}

NS_IMETHODIMP
nsSVGFEConvolveMatrixElement::GetDivisor(nsIDOMSVGAnimatedNumber **aDivisor)
{
  return mNumberAttributes[DIVISOR].ToDOMAnimatedNumber(aDivisor, this);
}

NS_IMETHODIMP
nsSVGFEConvolveMatrixElement::GetBias(nsIDOMSVGAnimatedNumber **aBias)
{
  return mNumberAttributes[BIAS].ToDOMAnimatedNumber(aBias, this);
}

NS_IMETHODIMP
nsSVGFEConvolveMatrixElement::GetKernelUnitLengthX(nsIDOMSVGAnimatedNumber **aKernelX)
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(aKernelX,
                                                                       nsSVGNumberPair::eFirst,
                                                                       this);
}

NS_IMETHODIMP
nsSVGFEConvolveMatrixElement::GetKernelUnitLengthY(nsIDOMSVGAnimatedNumber **aKernelY)
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(aKernelY,
                                                                       nsSVGNumberPair::eSecond,
                                                                       this);
}

void
nsSVGFEConvolveMatrixElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

nsIntRect
nsSVGFEConvolveMatrixElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  // XXX A more precise box is possible when 'bias' is zero and 'edgeMode' is
  // 'none', but it requires analysis of 'kernelUnitLength', 'order' and
  // 'targetX/Y', so it's quite a lot of work. Don't do it for now.
  return GetMaxRect();
}

void
nsSVGFEConvolveMatrixElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  // XXX Precise results are possible but we're going to skip that work
  // for now. Do nothing, which means the needed-box remains the
  // source's output bounding box.
}

nsIntRect
nsSVGFEConvolveMatrixElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
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
            case nsSVGFEConvolveMatrixElement::SVG_EDGEMODE_DUPLICATE:
              sum[i] +=
                aSourceData[BoundInterval(sampleY, aHeight) * aStride +
                            BoundInterval(sampleX, aWidth) * 4 + offsets[i]] *
                aKernel[aOrderX * y + x];
              break;
            case nsSVGFEConvolveMatrixElement::SVG_EDGEMODE_WRAP:
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
nsSVGFEConvolveMatrixElement::Filter(nsSVGFilterInstance *instance,
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
nsSVGFEConvolveMatrixElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                        nsIAtom* aAttribute) const
{
  return nsSVGFEConvolveMatrixElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
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
nsSVGFEConvolveMatrixElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

nsSVGElement::NumberPairAttributesInfo
nsSVGFEConvolveMatrixElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  ArrayLength(sNumberPairInfo));
}

nsSVGElement::IntegerAttributesInfo
nsSVGFEConvolveMatrixElement::GetIntegerInfo()
{
  return IntegerAttributesInfo(mIntegerAttributes, sIntegerInfo,
                               ArrayLength(sIntegerInfo));
}

nsSVGElement::IntegerPairAttributesInfo
nsSVGFEConvolveMatrixElement::GetIntegerPairInfo()
{
  return IntegerPairAttributesInfo(mIntegerPairAttributes, sIntegerPairInfo,
                                   ArrayLength(sIntegerPairInfo));
}

nsSVGElement::BooleanAttributesInfo
nsSVGFEConvolveMatrixElement::GetBooleanInfo()
{
  return BooleanAttributesInfo(mBooleanAttributes, sBooleanInfo,
                               ArrayLength(sBooleanInfo));
}

nsSVGElement::EnumAttributesInfo
nsSVGFEConvolveMatrixElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGFEConvolveMatrixElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

nsSVGElement::NumberListAttributesInfo
nsSVGFEConvolveMatrixElement::GetNumberListInfo()
{
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  ArrayLength(sNumberListInfo));
}

} // namespace dom
} // namespace mozilla
