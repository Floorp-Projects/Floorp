/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilterProcessing.h"

namespace mozilla {
namespace gfx {

TemporaryRef<DataSourceSurface>
FilterProcessing::ExtractAlpha(DataSourceSurface* aSource)
{
  IntSize size = aSource->GetSize();
  RefPtr<DataSourceSurface> alpha = Factory::CreateDataSourceSurface(size, SurfaceFormat::A8);
  uint8_t* sourceData = aSource->GetData();
  int32_t sourceStride = aSource->Stride();
  uint8_t* alphaData = alpha->GetData();
  int32_t alphaStride = alpha->Stride();

  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    ExtractAlpha_SSE2(size, sourceData, sourceStride, alphaData, alphaStride);
#endif
  } else {
    ExtractAlpha_Scalar(size, sourceData, sourceStride, alphaData, alphaStride);
  }

  return alpha;
}

TemporaryRef<DataSourceSurface>
FilterProcessing::ConvertToB8G8R8A8(SourceSurface* aSurface)
{
  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    return ConvertToB8G8R8A8_SSE2(aSurface);
#endif
  }
  return ConvertToB8G8R8A8_Scalar(aSurface);
}

TemporaryRef<DataSourceSurface>
FilterProcessing::ApplyBlending(DataSourceSurface* aInput1, DataSourceSurface* aInput2,
                                BlendMode aBlendMode)
{
  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    return ApplyBlending_SSE2(aInput1, aInput2, aBlendMode);
#endif
  }
  return ApplyBlending_Scalar(aInput1, aInput2, aBlendMode);
}

void
FilterProcessing::ApplyMorphologyHorizontal(uint8_t* aSourceData, int32_t aSourceStride,
                                            uint8_t* aDestData, int32_t aDestStride,
                                            const IntRect& aDestRect, int32_t aRadius,
                                            MorphologyOperator aOp)
{
  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    ApplyMorphologyHorizontal_SSE2(
      aSourceData, aSourceStride, aDestData, aDestStride, aDestRect, aRadius, aOp);
#endif
  } else {
    ApplyMorphologyHorizontal_Scalar(
      aSourceData, aSourceStride, aDestData, aDestStride, aDestRect, aRadius, aOp);
  }
}

void
FilterProcessing::ApplyMorphologyVertical(uint8_t* aSourceData, int32_t aSourceStride,
                                            uint8_t* aDestData, int32_t aDestStride,
                                            const IntRect& aDestRect, int32_t aRadius,
                                            MorphologyOperator aOp)
{
  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    ApplyMorphologyVertical_SSE2(
      aSourceData, aSourceStride, aDestData, aDestStride, aDestRect, aRadius, aOp);
#endif
  } else {
    ApplyMorphologyVertical_Scalar(
      aSourceData, aSourceStride, aDestData, aDestStride, aDestRect, aRadius, aOp);
  }
}

TemporaryRef<DataSourceSurface>
FilterProcessing::ApplyColorMatrix(DataSourceSurface* aInput, const Matrix5x4 &aMatrix)
{
  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    return ApplyColorMatrix_SSE2(aInput, aMatrix);
#endif
  }
  return ApplyColorMatrix_Scalar(aInput, aMatrix);
}

void
FilterProcessing::ApplyComposition(DataSourceSurface* aSource, DataSourceSurface* aDest,
                                   CompositeOperator aOperator)
{
  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    ApplyComposition_SSE2(aSource, aDest, aOperator);
#endif
  } else {
    ApplyComposition_Scalar(aSource, aDest, aOperator);
  }
}

void
FilterProcessing::SeparateColorChannels(DataSourceSurface* aSource,
                                        RefPtr<DataSourceSurface>& aChannel0,
                                        RefPtr<DataSourceSurface>& aChannel1,
                                        RefPtr<DataSourceSurface>& aChannel2,
                                        RefPtr<DataSourceSurface>& aChannel3)
{
  IntSize size = aSource->GetSize();
  aChannel0 = Factory::CreateDataSourceSurface(size, SurfaceFormat::A8);
  aChannel1 = Factory::CreateDataSourceSurface(size, SurfaceFormat::A8);
  aChannel2 = Factory::CreateDataSourceSurface(size, SurfaceFormat::A8);
  aChannel3 = Factory::CreateDataSourceSurface(size, SurfaceFormat::A8);
  uint8_t* sourceData = aSource->GetData();
  int32_t sourceStride = aSource->Stride();
  uint8_t* channel0Data = aChannel0->GetData();
  uint8_t* channel1Data = aChannel1->GetData();
  uint8_t* channel2Data = aChannel2->GetData();
  uint8_t* channel3Data = aChannel3->GetData();
  int32_t channelStride = aChannel0->Stride();

  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    SeparateColorChannels_SSE2(size, sourceData, sourceStride, channel0Data, channel1Data, channel2Data, channel3Data, channelStride);
#endif
  } else {
    SeparateColorChannels_Scalar(size, sourceData, sourceStride, channel0Data, channel1Data, channel2Data, channel3Data, channelStride);
  }
}

TemporaryRef<DataSourceSurface>
FilterProcessing::CombineColorChannels(DataSourceSurface* aChannel0, DataSourceSurface* aChannel1,
                                       DataSourceSurface* aChannel2, DataSourceSurface* aChannel3)
{
  IntSize size = aChannel0->GetSize();
  RefPtr<DataSourceSurface> result =
    Factory::CreateDataSourceSurface(size, SurfaceFormat::B8G8R8A8);
  int32_t resultStride = result->Stride();
  uint8_t* resultData = result->GetData();
  int32_t channelStride = aChannel0->Stride();
  uint8_t* channel0Data = aChannel0->GetData();
  uint8_t* channel1Data = aChannel1->GetData();
  uint8_t* channel2Data = aChannel2->GetData();
  uint8_t* channel3Data = aChannel3->GetData();

  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    CombineColorChannels_SSE2(size, resultStride, resultData, channelStride, channel0Data, channel1Data, channel2Data, channel3Data);
#endif
  } else {
    CombineColorChannels_Scalar(size, resultStride, resultData, channelStride, channel0Data, channel1Data, channel2Data, channel3Data);
  }

  return result;
}

void
FilterProcessing::DoPremultiplicationCalculation(const IntSize& aSize,
                                                 uint8_t* aTargetData, int32_t aTargetStride,
                                                 uint8_t* aSourceData, int32_t aSourceStride)
{
  if (Factory::HasSSE2()) {
#ifdef USE_SSE2 
    DoPremultiplicationCalculation_SSE2(
      aSize, aTargetData, aTargetStride, aSourceData, aSourceStride);
#endif
  } else {
    DoPremultiplicationCalculation_Scalar(
      aSize, aTargetData, aTargetStride, aSourceData, aSourceStride);
  }
}

void
FilterProcessing::DoUnpremultiplicationCalculation(const IntSize& aSize,
                                                   uint8_t* aTargetData, int32_t aTargetStride,
                                                   uint8_t* aSourceData, int32_t aSourceStride)
{
  if (Factory::HasSSE2()) {
#ifdef USE_SSE2 
    DoUnpremultiplicationCalculation_SSE2(
      aSize, aTargetData, aTargetStride, aSourceData, aSourceStride);
#endif
  } else {
    DoUnpremultiplicationCalculation_Scalar(
      aSize, aTargetData, aTargetStride, aSourceData, aSourceStride);
  }
}

TemporaryRef<DataSourceSurface>
FilterProcessing::RenderTurbulence(const IntSize &aSize, const Point &aOffset, const Size &aBaseFrequency,
                                   int32_t aSeed, int aNumOctaves, TurbulenceType aType, bool aStitch, const Rect &aTileRect)
{
  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    return RenderTurbulence_SSE2(aSize, aOffset, aBaseFrequency, aSeed, aNumOctaves, aType, aStitch, aTileRect);
#endif
  }
  return RenderTurbulence_Scalar(aSize, aOffset, aBaseFrequency, aSeed, aNumOctaves, aType, aStitch, aTileRect);
}

TemporaryRef<DataSourceSurface>
FilterProcessing::ApplyArithmeticCombine(DataSourceSurface* aInput1, DataSourceSurface* aInput2, Float aK1, Float aK2, Float aK3, Float aK4)
{
  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    return ApplyArithmeticCombine_SSE2(aInput1, aInput2, aK1, aK2, aK3, aK4);
#endif
  }
  return ApplyArithmeticCombine_Scalar(aInput1, aInput2, aK1, aK2, aK3, aK4);
}

} // namespace gfx
} // namespace mozilla
