/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define SIMD_COMPILE_SSE2

#include "FilterProcessingSIMD-inl.h"

#ifndef USE_SSE2
static_assert(false, "If this file is built, FilterProcessing.h should know about it!");
#endif

namespace mozilla {
namespace gfx {

void
FilterProcessing::ExtractAlpha_SSE2(const IntSize& size, uint8_t* sourceData, int32_t sourceStride, uint8_t* alphaData, int32_t alphaStride)
{
  ExtractAlpha_SIMD<__m128i>(size, sourceData, sourceStride, alphaData, alphaStride);
}

already_AddRefed<DataSourceSurface>
FilterProcessing::ConvertToB8G8R8A8_SSE2(SourceSurface* aSurface)
{
  return ConvertToB8G8R8A8_SIMD<__m128i>(aSurface);
}

already_AddRefed<DataSourceSurface>
FilterProcessing::ApplyBlending_SSE2(DataSourceSurface* aInput1, DataSourceSurface* aInput2,
                                     BlendMode aBlendMode)
{
  return ApplyBlending_SIMD<__m128i,__m128i,__m128i>(aInput1, aInput2, aBlendMode);
}

void
FilterProcessing::ApplyMorphologyHorizontal_SSE2(uint8_t* aSourceData, int32_t aSourceStride,
                                                 uint8_t* aDestData, int32_t aDestStride,
                                                 const IntRect& aDestRect, int32_t aRadius,
                                                 MorphologyOperator aOp)
{
  ApplyMorphologyHorizontal_SIMD<__m128i,__m128i>(
    aSourceData, aSourceStride, aDestData, aDestStride, aDestRect, aRadius, aOp);
}

void
FilterProcessing::ApplyMorphologyVertical_SSE2(uint8_t* aSourceData, int32_t aSourceStride,
                                                 uint8_t* aDestData, int32_t aDestStride,
                                                 const IntRect& aDestRect, int32_t aRadius,
                                                 MorphologyOperator aOp)
{
  ApplyMorphologyVertical_SIMD<__m128i,__m128i>(
    aSourceData, aSourceStride, aDestData, aDestStride, aDestRect, aRadius, aOp);
}

already_AddRefed<DataSourceSurface>
FilterProcessing::ApplyColorMatrix_SSE2(DataSourceSurface* aInput, const Matrix5x4 &aMatrix)
{
  return ApplyColorMatrix_SIMD<__m128i,__m128i,__m128i>(aInput, aMatrix);
}

void
FilterProcessing::ApplyComposition_SSE2(DataSourceSurface* aSource, DataSourceSurface* aDest,
                                        CompositeOperator aOperator)
{
  return ApplyComposition_SIMD<__m128i,__m128i,__m128i>(aSource, aDest, aOperator);
}

void
FilterProcessing::SeparateColorChannels_SSE2(const IntSize &size, uint8_t* sourceData, int32_t sourceStride, uint8_t* channel0Data, uint8_t* channel1Data, uint8_t* channel2Data, uint8_t* channel3Data, int32_t channelStride)
{
  SeparateColorChannels_SIMD<__m128i>(size, sourceData, sourceStride, channel0Data, channel1Data, channel2Data, channel3Data, channelStride);
}

void
FilterProcessing::CombineColorChannels_SSE2(const IntSize &size, int32_t resultStride, uint8_t* resultData, int32_t channelStride, uint8_t* channel0Data, uint8_t* channel1Data, uint8_t* channel2Data, uint8_t* channel3Data)
{
  CombineColorChannels_SIMD<__m128i>(size, resultStride, resultData, channelStride, channel0Data, channel1Data, channel2Data, channel3Data);
}

void
FilterProcessing::DoPremultiplicationCalculation_SSE2(const IntSize& aSize,
                                     uint8_t* aTargetData, int32_t aTargetStride,
                                     uint8_t* aSourceData, int32_t aSourceStride)
{
  DoPremultiplicationCalculation_SIMD<__m128i,__m128i,__m128i>(aSize, aTargetData, aTargetStride, aSourceData, aSourceStride);
}

void
FilterProcessing::DoUnpremultiplicationCalculation_SSE2(
                                 const IntSize& aSize,
                                 uint8_t* aTargetData, int32_t aTargetStride,
                                 uint8_t* aSourceData, int32_t aSourceStride)
{
  DoUnpremultiplicationCalculation_SIMD<__m128i,__m128i>(aSize, aTargetData, aTargetStride, aSourceData, aSourceStride);
}

already_AddRefed<DataSourceSurface>
FilterProcessing::RenderTurbulence_SSE2(const IntSize &aSize, const Point &aOffset, const Size &aBaseFrequency,
                                        int32_t aSeed, int aNumOctaves, TurbulenceType aType, bool aStitch, const Rect &aTileRect)
{
  return RenderTurbulence_SIMD<__m128,__m128i,__m128i>(aSize, aOffset, aBaseFrequency, aSeed, aNumOctaves, aType, aStitch, aTileRect);
}

already_AddRefed<DataSourceSurface>
FilterProcessing::ApplyArithmeticCombine_SSE2(DataSourceSurface* aInput1, DataSourceSurface* aInput2, Float aK1, Float aK2, Float aK3, Float aK4)
{
  return ApplyArithmeticCombine_SIMD<__m128i,__m128i,__m128i>(aInput1, aInput2, aK1, aK2, aK3, aK4);
}

} // namespace gfx
} // namespace mozilla
