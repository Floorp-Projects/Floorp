/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define FILTER_PROCESSING_SCALAR

#include "FilterProcessingSIMD-inl.h"
#include "Logging.h"

namespace mozilla {
namespace gfx {

void
FilterProcessing::ExtractAlpha_Scalar(const IntSize& size, uint8_t* sourceData, int32_t sourceStride, uint8_t* alphaData, int32_t alphaStride)
{
  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      int32_t sourceIndex = y * sourceStride + 4 * x;
      int32_t targetIndex = y * alphaStride + x;
      alphaData[targetIndex] = sourceData[sourceIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_A];
    }
  }
}

TemporaryRef<DataSourceSurface>
FilterProcessing::ConvertToB8G8R8A8_Scalar(SourceSurface* aSurface)
{
  return ConvertToB8G8R8A8_SIMD<simd::Scalaru8x16_t>(aSurface);
}

template<BlendMode aBlendMode>
static TemporaryRef<DataSourceSurface>
ApplyBlending_Scalar(DataSourceSurface* aInput1, DataSourceSurface* aInput2)
{
  IntSize size = aInput1->GetSize();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, SurfaceFormat::B8G8R8A8);
  if (MOZ2D_WARN_IF(!target)) {
    return nullptr;
  }

  uint8_t* source1Data = aInput1->GetData();
  uint8_t* source2Data = aInput2->GetData();
  uint8_t* targetData = target->GetData();
  uint32_t targetStride = target->Stride();
  uint32_t source1Stride = aInput1->Stride();
  uint32_t source2Stride = aInput2->Stride();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      uint32_t targetIndex = y * targetStride + 4 * x;
      uint32_t source1Index = y * source1Stride + 4 * x;
      uint32_t source2Index = y * source2Stride + 4 * x;
      uint32_t qa = source1Data[source1Index + B8G8R8A8_COMPONENT_BYTEOFFSET_A];
      uint32_t qb = source2Data[source2Index + B8G8R8A8_COMPONENT_BYTEOFFSET_A];
      for (int32_t i = std::min(B8G8R8A8_COMPONENT_BYTEOFFSET_B, B8G8R8A8_COMPONENT_BYTEOFFSET_R);
           i <= std::max(B8G8R8A8_COMPONENT_BYTEOFFSET_B, B8G8R8A8_COMPONENT_BYTEOFFSET_R); i++) {
        uint32_t ca = source1Data[source1Index + i];
        uint32_t cb = source2Data[source2Index + i];
        uint32_t val;
        switch (aBlendMode) {
          case BLEND_MODE_MULTIPLY:
            val = ((255 - qa) * cb + (255 - qb + cb) * ca);
            break;
          case BLEND_MODE_SCREEN:
            val = 255 * (cb + ca) - ca * cb;
            break;
          case BLEND_MODE_DARKEN:
            val = umin((255 - qa) * cb + 255 * ca,
                       (255 - qb) * ca + 255 * cb);
            break;
          case BLEND_MODE_LIGHTEN:
            val = umax((255 - qa) * cb + 255 * ca,
                       (255 - qb) * ca + 255 * cb);
            break;
          default:
            MOZ_CRASH();
        }
        val = umin(FilterProcessing::FastDivideBy255<unsigned>(val), 255U);
        targetData[targetIndex + i] = static_cast<uint8_t>(val);
      }
      uint32_t alpha = 255 * 255 - (255 - qa) * (255 - qb);
      targetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_A] =
        FilterProcessing::FastDivideBy255<uint8_t>(alpha);
    }
  }

  return target.forget();
}

TemporaryRef<DataSourceSurface>
FilterProcessing::ApplyBlending_Scalar(DataSourceSurface* aInput1, DataSourceSurface* aInput2,
                                       BlendMode aBlendMode)
{
  switch (aBlendMode) {
    case BLEND_MODE_MULTIPLY:
      return gfx::ApplyBlending_Scalar<BLEND_MODE_MULTIPLY>(aInput1, aInput2);
    case BLEND_MODE_SCREEN:
      return gfx::ApplyBlending_Scalar<BLEND_MODE_SCREEN>(aInput1, aInput2);
    case BLEND_MODE_DARKEN:
      return gfx::ApplyBlending_Scalar<BLEND_MODE_DARKEN>(aInput1, aInput2);
    case BLEND_MODE_LIGHTEN:
      return gfx::ApplyBlending_Scalar<BLEND_MODE_LIGHTEN>(aInput1, aInput2);
    default:
      return nullptr;
  }
}

template<MorphologyOperator Operator>
static void
ApplyMorphologyHorizontal_Scalar(uint8_t* aSourceData, int32_t aSourceStride,
                                 uint8_t* aDestData, int32_t aDestStride,
                                 const IntRect& aDestRect, int32_t aRadius)
{
  static_assert(Operator == MORPHOLOGY_OPERATOR_ERODE ||
                Operator == MORPHOLOGY_OPERATOR_DILATE,
                "unexpected morphology operator");

  for (int32_t y = aDestRect.y; y < aDestRect.YMost(); y++) {
    int32_t startX = aDestRect.x - aRadius;
    int32_t endX = aDestRect.x + aRadius;
    for (int32_t x = aDestRect.x; x < aDestRect.XMost(); x++, startX++, endX++) {
      int32_t sourceIndex = y * aSourceStride + 4 * startX;
      uint8_t u[4];
      for (size_t i = 0; i < 4; i++) {
        u[i] = aSourceData[sourceIndex + i];
      }
      sourceIndex += 4;
      for (int32_t ix = startX + 1; ix <= endX; ix++, sourceIndex += 4) {
        for (size_t i = 0; i < 4; i++) {
          if (Operator == MORPHOLOGY_OPERATOR_ERODE) {
            u[i] = umin(u[i], aSourceData[sourceIndex + i]);
          } else {
            u[i] = umax(u[i], aSourceData[sourceIndex + i]);
          }
        }
      }

      int32_t destIndex = y * aDestStride + 4 * x;
      for (size_t i = 0; i < 4; i++) {
        aDestData[destIndex+i] = u[i];
      }
    }
  }
}

void
FilterProcessing::ApplyMorphologyHorizontal_Scalar(uint8_t* aSourceData, int32_t aSourceStride,
                                                   uint8_t* aDestData, int32_t aDestStride,
                                                   const IntRect& aDestRect, int32_t aRadius,
                                                   MorphologyOperator aOp)
{
  if (aOp == MORPHOLOGY_OPERATOR_ERODE) {
    gfx::ApplyMorphologyHorizontal_Scalar<MORPHOLOGY_OPERATOR_ERODE>(
      aSourceData, aSourceStride, aDestData, aDestStride, aDestRect, aRadius);
  } else {
    gfx::ApplyMorphologyHorizontal_Scalar<MORPHOLOGY_OPERATOR_DILATE>(
      aSourceData, aSourceStride, aDestData, aDestStride, aDestRect, aRadius);
  }
}

template<MorphologyOperator Operator>
static void ApplyMorphologyVertical_Scalar(uint8_t* aSourceData, int32_t aSourceStride,
                                           uint8_t* aDestData, int32_t aDestStride,
                                           const IntRect& aDestRect, int32_t aRadius)
{
  static_assert(Operator == MORPHOLOGY_OPERATOR_ERODE ||
                Operator == MORPHOLOGY_OPERATOR_DILATE,
                "unexpected morphology operator");

  int32_t startY = aDestRect.y - aRadius;
  int32_t endY = aDestRect.y + aRadius;
  for (int32_t y = aDestRect.y; y < aDestRect.YMost(); y++, startY++, endY++) {
    for (int32_t x = aDestRect.x; x < aDestRect.XMost(); x++) {
      int32_t sourceIndex = startY * aSourceStride + 4 * x;
      uint8_t u[4];
      for (size_t i = 0; i < 4; i++) {
        u[i] = aSourceData[sourceIndex + i];
      }
      sourceIndex += aSourceStride;
      for (int32_t iy = startY + 1; iy <= endY; iy++, sourceIndex += aSourceStride) {
        for (size_t i = 0; i < 4; i++) {
          if (Operator == MORPHOLOGY_OPERATOR_ERODE) {
            u[i] = umin(u[i], aSourceData[sourceIndex + i]);
          } else {
            u[i] = umax(u[i], aSourceData[sourceIndex + i]);
          }
        }
      }

      int32_t destIndex = y * aDestStride + 4 * x;
      for (size_t i = 0; i < 4; i++) {
        aDestData[destIndex+i] = u[i];
      }
    }
  }
}

void
FilterProcessing::ApplyMorphologyVertical_Scalar(uint8_t* aSourceData, int32_t aSourceStride,
                                                   uint8_t* aDestData, int32_t aDestStride,
                                                   const IntRect& aDestRect, int32_t aRadius,
                                                   MorphologyOperator aOp)
{
  if (aOp == MORPHOLOGY_OPERATOR_ERODE) {
    gfx::ApplyMorphologyVertical_Scalar<MORPHOLOGY_OPERATOR_ERODE>(
      aSourceData, aSourceStride, aDestData, aDestStride, aDestRect, aRadius);
  } else {
    gfx::ApplyMorphologyVertical_Scalar<MORPHOLOGY_OPERATOR_DILATE>(
      aSourceData, aSourceStride, aDestData, aDestStride, aDestRect, aRadius);
  }
}

TemporaryRef<DataSourceSurface>
FilterProcessing::ApplyColorMatrix_Scalar(DataSourceSurface* aInput, const Matrix5x4 &aMatrix)
{
  return ApplyColorMatrix_SIMD<simd::Scalari32x4_t,simd::Scalari16x8_t,simd::Scalaru8x16_t>(aInput, aMatrix);
}

void
FilterProcessing::ApplyComposition_Scalar(DataSourceSurface* aSource, DataSourceSurface* aDest,
                                          CompositeOperator aOperator)
{
  return ApplyComposition_SIMD<simd::Scalari32x4_t,simd::Scalaru16x8_t,simd::Scalaru8x16_t>(aSource, aDest, aOperator);
}

void
FilterProcessing::SeparateColorChannels_Scalar(const IntSize &size, uint8_t* sourceData, int32_t sourceStride, uint8_t* channel0Data, uint8_t* channel1Data, uint8_t* channel2Data, uint8_t* channel3Data, int32_t channelStride)
{
  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      int32_t sourceIndex = y * sourceStride + 4 * x;
      int32_t targetIndex = y * channelStride + x;
      channel0Data[targetIndex] = sourceData[sourceIndex];
      channel1Data[targetIndex] = sourceData[sourceIndex+1];
      channel2Data[targetIndex] = sourceData[sourceIndex+2];
      channel3Data[targetIndex] = sourceData[sourceIndex+3];
    }
  }
}

void
FilterProcessing::CombineColorChannels_Scalar(const IntSize &size, int32_t resultStride, uint8_t* resultData, int32_t channelStride, uint8_t* channel0Data, uint8_t* channel1Data, uint8_t* channel2Data, uint8_t* channel3Data)
{
  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      int32_t resultIndex = y * resultStride + 4 * x;
      int32_t channelIndex = y * channelStride + x;
      resultData[resultIndex] = channel0Data[channelIndex];
      resultData[resultIndex+1] = channel1Data[channelIndex];
      resultData[resultIndex+2] = channel2Data[channelIndex];
      resultData[resultIndex+3] = channel3Data[channelIndex];
    }
  }
}

void
FilterProcessing::DoPremultiplicationCalculation_Scalar(const IntSize& aSize,
                                     uint8_t* aTargetData, int32_t aTargetStride,
                                     uint8_t* aSourceData, int32_t aSourceStride)
{
  for (int32_t y = 0; y < aSize.height; y++) {
    for (int32_t x = 0; x < aSize.width; x++) {
      int32_t inputIndex = y * aSourceStride + 4 * x;
      int32_t targetIndex = y * aTargetStride + 4 * x;
      uint8_t alpha = aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_A];
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_R] =
        FastDivideBy255<uint8_t>(aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_R] * alpha);
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_G] =
        FastDivideBy255<uint8_t>(aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_G] * alpha);
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_B] =
        FastDivideBy255<uint8_t>(aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_B] * alpha);
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_A] = alpha;
    }
  }
}

void
FilterProcessing::DoUnpremultiplicationCalculation_Scalar(
                                 const IntSize& aSize,
                                 uint8_t* aTargetData, int32_t aTargetStride,
                                 uint8_t* aSourceData, int32_t aSourceStride)
{
  for (int32_t y = 0; y < aSize.height; y++) {
    for (int32_t x = 0; x < aSize.width; x++) {
      int32_t inputIndex = y * aSourceStride + 4 * x;
      int32_t targetIndex = y * aTargetStride + 4 * x;
      uint8_t alpha = aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_A];
      uint16_t alphaFactor = sAlphaFactors[alpha];
      // inputColor * alphaFactor + 128 is guaranteed to fit into uint16_t
      // because the input is premultiplied and thus inputColor <= inputAlpha.
      // The maximum value this can attain is 65520 (which is less than 65535)
      // for color == alpha == 244:
      // 244 * sAlphaFactors[244] + 128 == 244 * 268 + 128 == 65520
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_R] =
        (aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_R] * alphaFactor + 128) >> 8;
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_G] =
        (aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_G] * alphaFactor + 128) >> 8;
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_B] =
        (aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_B] * alphaFactor + 128) >> 8;
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_A] = alpha;
    }
  }
}

TemporaryRef<DataSourceSurface>
FilterProcessing::RenderTurbulence_Scalar(const IntSize &aSize, const Point &aOffset, const Size &aBaseFrequency,
                                          int32_t aSeed, int aNumOctaves, TurbulenceType aType, bool aStitch, const Rect &aTileRect)
{
   return RenderTurbulence_SIMD<simd::Scalarf32x4_t,simd::Scalari32x4_t,simd::Scalaru8x16_t>(
     aSize, aOffset, aBaseFrequency, aSeed, aNumOctaves, aType, aStitch, aTileRect);
}

TemporaryRef<DataSourceSurface>
FilterProcessing::ApplyArithmeticCombine_Scalar(DataSourceSurface* aInput1, DataSourceSurface* aInput2, Float aK1, Float aK2, Float aK3, Float aK4)
{
  return ApplyArithmeticCombine_SIMD<simd::Scalari32x4_t,simd::Scalari16x8_t,simd::Scalaru8x16_t>(aInput1, aInput2, aK1, aK2, aK3, aK4);
}

} // namespace mozilla
} // namespace gfx
