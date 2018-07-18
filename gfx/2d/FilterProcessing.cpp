/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilterProcessing.h"
#include "Logging.h"

namespace mozilla {
namespace gfx {

already_AddRefed<DataSourceSurface>
FilterProcessing::ExtractAlpha(DataSourceSurface* aSource)
{
  IntSize size = aSource->GetSize();
  RefPtr<DataSourceSurface> alpha = Factory::CreateDataSourceSurface(size, SurfaceFormat::A8);
  if (MOZ2D_WARN_IF(!alpha)) {
    return nullptr;
  }

  DataSourceSurface::ScopedMap sourceMap(aSource, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap alphaMap(alpha, DataSourceSurface::WRITE);
  if (MOZ2D_WARN_IF(!sourceMap.IsMapped() || !alphaMap.IsMapped())) {
    return nullptr;
  }

  uint8_t* sourceData = sourceMap.GetData();
  int32_t sourceStride = sourceMap.GetStride();
  uint8_t* alphaData = alphaMap.GetData();
  int32_t alphaStride = alphaMap.GetStride();

  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    ExtractAlpha_SSE2(size, sourceData, sourceStride, alphaData, alphaStride);
#endif
  } else {
    ExtractAlpha_Scalar(size, sourceData, sourceStride, alphaData, alphaStride);
  }

  return alpha.forget();
}

already_AddRefed<DataSourceSurface>
FilterProcessing::ConvertToB8G8R8A8(SourceSurface* aSurface)
{
  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    return ConvertToB8G8R8A8_SSE2(aSurface);
#endif
  }
  return ConvertToB8G8R8A8_Scalar(aSurface);
}

already_AddRefed<DataSourceSurface>
FilterProcessing::ApplyBlending(DataSourceSurface* aInput1, DataSourceSurface* aInput2,
                                BlendMode aBlendMode)
{
  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    return ApplyBlending_SSE2(aInput1, aInput2, aBlendMode);
#endif
  }
  return nullptr;
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

already_AddRefed<DataSourceSurface>
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
  if (MOZ2D_WARN_IF(!(aChannel0 && aChannel1 && aChannel2 && aChannel3))) {
    return;
  }

  DataSourceSurface::ScopedMap sourceMap(aSource, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap channel0Map(aChannel0, DataSourceSurface::WRITE);
  DataSourceSurface::ScopedMap channel1Map(aChannel1, DataSourceSurface::WRITE);
  DataSourceSurface::ScopedMap channel2Map(aChannel2, DataSourceSurface::WRITE);
  DataSourceSurface::ScopedMap channel3Map(aChannel3, DataSourceSurface::WRITE);
  if (MOZ2D_WARN_IF(!(sourceMap.IsMapped() &&
                      channel0Map.IsMapped() && channel1Map.IsMapped() &&
                      channel2Map.IsMapped() && channel3Map.IsMapped()))) {
    return;
  }
  uint8_t* sourceData = sourceMap.GetData();
  int32_t sourceStride = sourceMap.GetStride();
  uint8_t* channel0Data = channel0Map.GetData();
  uint8_t* channel1Data = channel1Map.GetData();
  uint8_t* channel2Data = channel2Map.GetData();
  uint8_t* channel3Data = channel3Map.GetData();
  int32_t channelStride = channel0Map.GetStride();

  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    SeparateColorChannels_SSE2(size, sourceData, sourceStride, channel0Data, channel1Data, channel2Data, channel3Data, channelStride);
#endif
  } else {
    SeparateColorChannels_Scalar(size, sourceData, sourceStride, channel0Data, channel1Data, channel2Data, channel3Data, channelStride);
  }
}

already_AddRefed<DataSourceSurface>
FilterProcessing::CombineColorChannels(DataSourceSurface* aChannel0, DataSourceSurface* aChannel1,
                                       DataSourceSurface* aChannel2, DataSourceSurface* aChannel3)
{
  IntSize size = aChannel0->GetSize();
  RefPtr<DataSourceSurface> result =
    Factory::CreateDataSourceSurface(size, SurfaceFormat::B8G8R8A8);
  if (MOZ2D_WARN_IF(!result)) {
    return nullptr;
  }
  DataSourceSurface::ScopedMap resultMap(result, DataSourceSurface::WRITE);
  DataSourceSurface::ScopedMap channel0Map(aChannel0, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap channel1Map(aChannel1, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap channel2Map(aChannel2, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap channel3Map(aChannel3, DataSourceSurface::READ);
  if (MOZ2D_WARN_IF(!(resultMap.IsMapped() &&
                      channel0Map.IsMapped() && channel1Map.IsMapped() &&
                      channel2Map.IsMapped() && channel3Map.IsMapped()))) {
    return nullptr;
  }
  int32_t resultStride = resultMap.GetStride();
  uint8_t* resultData = resultMap.GetData();
  int32_t channelStride = channel0Map.GetStride();
  uint8_t* channel0Data = channel0Map.GetData();
  uint8_t* channel1Data = channel1Map.GetData();
  uint8_t* channel2Data = channel2Map.GetData();
  uint8_t* channel3Data = channel3Map.GetData();

  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    CombineColorChannels_SSE2(size, resultStride, resultData, channelStride, channel0Data, channel1Data, channel2Data, channel3Data);
#endif
  } else {
    CombineColorChannels_Scalar(size, resultStride, resultData, channelStride, channel0Data, channel1Data, channel2Data, channel3Data);
  }

  return result.forget();
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

void
FilterProcessing::DoOpacityCalculation(const IntSize& aSize,
  uint8_t* aTargetData, int32_t aTargetStride,
  uint8_t* aSourceData, int32_t aSourceStride,
  Float aValue)
{
  if (Factory::HasSSE2()) {
#ifdef USE_SSE2
    DoOpacityCalculation_SSE2(
      aSize, aTargetData, aTargetStride, aSourceData, aSourceStride, aValue);
#endif
  }
  else {
    DoOpacityCalculation_Scalar(
      aSize, aTargetData, aTargetStride, aSourceData, aSourceStride, aValue);
  }
}

void
FilterProcessing::DoOpacityCalculationA8(const IntSize& aSize,
  uint8_t* aTargetData, int32_t aTargetStride,
  uint8_t* aSourceData, int32_t aSourceStride,
  Float aValue)
{
  DoOpacityCalculationA8_Scalar(
    aSize, aTargetData, aTargetStride, aSourceData, aSourceStride, aValue);
}

already_AddRefed<DataSourceSurface>
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

already_AddRefed<DataSourceSurface>
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
