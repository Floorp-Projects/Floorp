/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilterProcessing.h"

#include "SIMD.h"
#include "SVGTurbulenceRenderer-inl.h"

namespace mozilla {
namespace gfx {

template<typename u8x16_t>
inline already_AddRefed<DataSourceSurface>
ConvertToB8G8R8A8_SIMD(SourceSurface* aSurface)
{
  IntSize size = aSurface->GetSize();
  RefPtr<DataSourceSurface> input = aSurface->GetDataSurface();
  RefPtr<DataSourceSurface> output =
    Factory::CreateDataSourceSurface(size, SurfaceFormat::B8G8R8A8);
  uint8_t *inputData = input->GetData();
  uint8_t *outputData = output->GetData();
  int32_t inputStride = input->Stride();
  int32_t outputStride = output->Stride();
  switch (input->GetFormat()) {
    case SurfaceFormat::B8G8R8A8:
      output = input;
      break;
    case SurfaceFormat::B8G8R8X8:
      for (int32_t y = 0; y < size.height; y++) {
        for (int32_t x = 0; x < size.width; x++) {
          int32_t inputIndex = y * inputStride + 4 * x;
          int32_t outputIndex = y * outputStride + 4 * x;
          outputData[outputIndex + 0] = inputData[inputIndex + 0];
          outputData[outputIndex + 1] = inputData[inputIndex + 1];
          outputData[outputIndex + 2] = inputData[inputIndex + 2];
          outputData[outputIndex + 3] = 255;
        }
      }
      break;
    case SurfaceFormat::R8G8B8A8:
      for (int32_t y = 0; y < size.height; y++) {
        for (int32_t x = 0; x < size.width; x++) {
          int32_t inputIndex = y * inputStride + 4 * x;
          int32_t outputIndex = y * outputStride + 4 * x;
          outputData[outputIndex + 2] = inputData[inputIndex + 0];
          outputData[outputIndex + 1] = inputData[inputIndex + 1];
          outputData[outputIndex + 0] = inputData[inputIndex + 2];
          outputData[outputIndex + 3] = inputData[inputIndex + 3];
        }
      }
      break;
    case SurfaceFormat::R8G8B8X8:
      for (int32_t y = 0; y < size.height; y++) {
        for (int32_t x = 0; x < size.width; x++) {
          int32_t inputIndex = y * inputStride + 4 * x;
          int32_t outputIndex = y * outputStride + 4 * x;
          outputData[outputIndex + 2] = inputData[inputIndex + 0];
          outputData[outputIndex + 1] = inputData[inputIndex + 1];
          outputData[outputIndex + 0] = inputData[inputIndex + 2];
          outputData[outputIndex + 3] = 255;
        }
      }
      break;
    case SurfaceFormat::A8:
      for (int32_t y = 0; y < size.height; y++) {
        for (int32_t x = 0; x < size.width; x += 16) {
          int32_t inputIndex = y * inputStride + x;
          int32_t outputIndex = y * outputStride + 4 * x;
          u8x16_t p1To16 = simd::Load8<u8x16_t>(&inputData[inputIndex]);
          // Turn AAAAAAAAAAAAAAAA into four chunks of 000A000A000A000A by
          // interleaving with 0000000000000000 twice.
          u8x16_t zero = simd::FromZero8<u8x16_t>();
          u8x16_t p1To8 = simd::InterleaveLo8(zero, p1To16);
          u8x16_t p9To16 = simd::InterleaveHi8(zero, p1To16);
          u8x16_t p1To4 = simd::InterleaveLo8(zero, p1To8);
          u8x16_t p5To8 = simd::InterleaveHi8(zero, p1To8);
          u8x16_t p9To12 = simd::InterleaveLo8(zero, p9To16);
          u8x16_t p13To16 = simd::InterleaveHi8(zero, p9To16);
          simd::Store8(&outputData[outputIndex], p1To4);
          if ((x + 4) * 4 < outputStride) {
            simd::Store8(&outputData[outputIndex + 4 * 4], p5To8);
          }
          if ((x + 8) * 4 < outputStride) {
            simd::Store8(&outputData[outputIndex + 4 * 8], p9To12);
          }
          if ((x + 12) * 4 < outputStride) {
            simd::Store8(&outputData[outputIndex + 4 * 12], p13To16);
          }
        }
      }
      break;
    default:
      output = nullptr;
      break;
  }
  return output.forget();
}

template<typename u8x16_t>
inline void
ExtractAlpha_SIMD(const IntSize& size, uint8_t* sourceData, int32_t sourceStride, uint8_t* alphaData, int32_t alphaStride)
{
  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x += 16) {
      // Process 16 pixels at a time.
      // Turn up to four chunks of BGRABGRABGRABGRA into one chunk of AAAAAAAAAAAAAAAA.
      int32_t sourceIndex = y * sourceStride + 4 * x;
      int32_t targetIndex = y * alphaStride + x;

      u8x16_t bgrabgrabgrabgra1 = simd::FromZero8<u8x16_t>();
      u8x16_t bgrabgrabgrabgra2 = simd::FromZero8<u8x16_t>();
      u8x16_t bgrabgrabgrabgra3 = simd::FromZero8<u8x16_t>();
      u8x16_t bgrabgrabgrabgra4 = simd::FromZero8<u8x16_t>();

      bgrabgrabgrabgra1 = simd::Load8<u8x16_t>(&sourceData[sourceIndex]);
      if (4 * (x + 4) < sourceStride) {
        bgrabgrabgrabgra2 = simd::Load8<u8x16_t>(&sourceData[sourceIndex + 4 * 4]);
      }
      if (4 * (x + 8) < sourceStride) {
        bgrabgrabgrabgra3 = simd::Load8<u8x16_t>(&sourceData[sourceIndex + 4 * 8]);
      }
      if (4 * (x + 12) < sourceStride) {
        bgrabgrabgrabgra4 = simd::Load8<u8x16_t>(&sourceData[sourceIndex + 4 * 12]);
      }

      u8x16_t bbggrraabbggrraa1 = simd::InterleaveLo8(bgrabgrabgrabgra1, bgrabgrabgrabgra3);
      u8x16_t bbggrraabbggrraa2 = simd::InterleaveHi8(bgrabgrabgrabgra1, bgrabgrabgrabgra3);
      u8x16_t bbggrraabbggrraa3 = simd::InterleaveLo8(bgrabgrabgrabgra2, bgrabgrabgrabgra4);
      u8x16_t bbggrraabbggrraa4 = simd::InterleaveHi8(bgrabgrabgrabgra2, bgrabgrabgrabgra4);
      u8x16_t bbbbggggrrrraaaa1 = simd::InterleaveLo8(bbggrraabbggrraa1, bbggrraabbggrraa3);
      u8x16_t bbbbggggrrrraaaa2 = simd::InterleaveHi8(bbggrraabbggrraa1, bbggrraabbggrraa3);
      u8x16_t bbbbggggrrrraaaa3 = simd::InterleaveLo8(bbggrraabbggrraa2, bbggrraabbggrraa4);
      u8x16_t bbbbggggrrrraaaa4 = simd::InterleaveHi8(bbggrraabbggrraa2, bbggrraabbggrraa4);
      u8x16_t rrrrrrrraaaaaaaa1 = simd::InterleaveHi8(bbbbggggrrrraaaa1, bbbbggggrrrraaaa3);
      u8x16_t rrrrrrrraaaaaaaa2 = simd::InterleaveHi8(bbbbggggrrrraaaa2, bbbbggggrrrraaaa4);
      u8x16_t aaaaaaaaaaaaaaaa = simd::InterleaveHi8(rrrrrrrraaaaaaaa1, rrrrrrrraaaaaaaa2);

      simd::Store8(&alphaData[targetIndex], aaaaaaaaaaaaaaaa);
    }
  }
}

// This function calculates the result color values for four pixels, but for
// only two color channels - either b & r or g & a. However, the a result will
// not be used.
// source and dest each contain 8 values, either bbbb gggg or rrrr aaaa.
// sourceAlpha and destAlpha are of the form aaaa aaaa, where each aaaa is the
// alpha of all four pixels (and both aaaa's are the same).
// blendendComponent1 and blendedComponent2 are the out parameters.
template<typename i16x8_t, typename i32x4_t, uint32_t aBlendMode>
inline void
BlendTwoComponentsOfFourPixels(i16x8_t source, i16x8_t sourceAlpha,
                               i16x8_t dest, const i16x8_t& destAlpha,
                               i32x4_t& blendedComponent1, i32x4_t& blendedComponent2)
{
  i16x8_t x255 = simd::FromI16<i16x8_t>(255);

  switch (aBlendMode) {

    case BLEND_MODE_MULTIPLY:
    {
      // val = ((255 - destAlpha) * source + (255 - sourceAlpha + source) * dest);
      i16x8_t twoFiftyFiveMinusDestAlpha = simd::Sub16(x255, destAlpha);
      i16x8_t twoFiftyFiveMinusSourceAlpha = simd::Sub16(x255, sourceAlpha);
      i16x8_t twoFiftyFiveMinusSourceAlphaPlusSource = simd::Add16(twoFiftyFiveMinusSourceAlpha, source);

      i16x8_t sourceInterleavedWithDest1 = simd::InterleaveLo16(source, dest);
      i16x8_t leftFactor1 = simd::InterleaveLo16(twoFiftyFiveMinusDestAlpha, twoFiftyFiveMinusSourceAlphaPlusSource);
      blendedComponent1 = simd::MulAdd16x8x2To32x4(sourceInterleavedWithDest1, leftFactor1);
      blendedComponent1 = simd::FastDivideBy255(blendedComponent1);

      i16x8_t sourceInterleavedWithDest2 = simd::InterleaveHi16(source, dest);
      i16x8_t leftFactor2 = simd::InterleaveHi16(twoFiftyFiveMinusDestAlpha, twoFiftyFiveMinusSourceAlphaPlusSource);
      blendedComponent2 = simd::MulAdd16x8x2To32x4(sourceInterleavedWithDest2, leftFactor2);
      blendedComponent2 = simd::FastDivideBy255(blendedComponent2);

      break;
    }

    case BLEND_MODE_SCREEN:
    {
      // val = 255 * (source + dest) + (0 - dest) * source;
      i16x8_t sourcePlusDest = simd::Add16(source, dest);
      i16x8_t zeroMinusDest = simd::Sub16(simd::FromI16<i16x8_t>(0), dest);

      i16x8_t twoFiftyFiveInterleavedWithZeroMinusDest1 = simd::InterleaveLo16(x255, zeroMinusDest);
      i16x8_t sourcePlusDestInterleavedWithSource1 = simd::InterleaveLo16(sourcePlusDest, source);
      blendedComponent1 = simd::MulAdd16x8x2To32x4(twoFiftyFiveInterleavedWithZeroMinusDest1, sourcePlusDestInterleavedWithSource1);
      blendedComponent1 = simd::FastDivideBy255(blendedComponent1);

      i16x8_t twoFiftyFiveInterleavedWithZeroMinusDest2 = simd::InterleaveHi16(x255, zeroMinusDest);
      i16x8_t sourcePlusDestInterleavedWithSource2 = simd::InterleaveHi16(sourcePlusDest, source);
      blendedComponent2 = simd::MulAdd16x8x2To32x4(twoFiftyFiveInterleavedWithZeroMinusDest2, sourcePlusDestInterleavedWithSource2);
      blendedComponent2 = simd::FastDivideBy255(blendedComponent2);

      break;
    }

    case BLEND_MODE_DARKEN:
    case BLEND_MODE_LIGHTEN:
    {
      // Darken:
      // val = min((255 - destAlpha) * source + 255                 * dest,
      //           255               * source + (255 - sourceAlpha) * dest);
      //
      // Lighten:
      // val = max((255 - destAlpha) * source + 255                 * dest,
      //           255               * source + (255 - sourceAlpha) * dest);

      i16x8_t twoFiftyFiveMinusDestAlpha = simd::Sub16(x255, destAlpha);
      i16x8_t twoFiftyFiveMinusSourceAlpha = simd::Sub16(x255, sourceAlpha);

      i16x8_t twoFiftyFiveMinusDestAlphaInterleavedWithTwoFiftyFive1 = simd::InterleaveLo16(twoFiftyFiveMinusDestAlpha, x255);
      i16x8_t twoFiftyFiveInterleavedWithTwoFiftyFiveMinusSourceAlpha1 = simd::InterleaveLo16(x255, twoFiftyFiveMinusSourceAlpha);
      i16x8_t sourceInterleavedWithDest1 = simd::InterleaveLo16(source, dest);
      i32x4_t product1_1 = simd::MulAdd16x8x2To32x4(twoFiftyFiveMinusDestAlphaInterleavedWithTwoFiftyFive1, sourceInterleavedWithDest1);
      i32x4_t product1_2 = simd::MulAdd16x8x2To32x4(twoFiftyFiveInterleavedWithTwoFiftyFiveMinusSourceAlpha1, sourceInterleavedWithDest1);
      blendedComponent1 = aBlendMode == BLEND_MODE_DARKEN ? simd::Min32(product1_1, product1_2) : simd::Max32(product1_1, product1_2);
      blendedComponent1 = simd::FastDivideBy255(blendedComponent1);

      i16x8_t twoFiftyFiveMinusDestAlphaInterleavedWithTwoFiftyFive2 = simd::InterleaveHi16(twoFiftyFiveMinusDestAlpha, x255);
      i16x8_t twoFiftyFiveInterleavedWithTwoFiftyFiveMinusSourceAlpha2 = simd::InterleaveHi16(x255, twoFiftyFiveMinusSourceAlpha);
      i16x8_t sourceInterleavedWithDest2 = simd::InterleaveHi16(source, dest);
      i32x4_t product2_1 = simd::MulAdd16x8x2To32x4(twoFiftyFiveMinusDestAlphaInterleavedWithTwoFiftyFive2, sourceInterleavedWithDest2);
      i32x4_t product2_2 = simd::MulAdd16x8x2To32x4(twoFiftyFiveInterleavedWithTwoFiftyFiveMinusSourceAlpha2, sourceInterleavedWithDest2);
      blendedComponent2 = aBlendMode == BLEND_MODE_DARKEN ? simd::Min32(product2_1, product2_2) : simd::Max32(product2_1, product2_2);
      blendedComponent2 = simd::FastDivideBy255(blendedComponent2);

      break;
    }

  }
}

// The alpha channel is subject to a different calculation than the RGB
// channels, and this calculation is the same for all blend modes:
// resultAlpha * 255 = 255 * 255 - (255 - sourceAlpha) * (255 - destAlpha)
template<typename i16x8_t, typename i32x4_t>
inline i32x4_t
BlendAlphaOfFourPixels(i16x8_t s_rrrraaaa1234, i16x8_t d_rrrraaaa1234)
{
  // We're using MulAdd16x8x2To32x4, so we need to interleave our factors
  // appropriately. The calculation is rewritten as follows:
  // resultAlpha[0] * 255 = 255 * 255 - (255 - sourceAlpha[0]) * (255 - destAlpha[0])
  //                      = 255 * 255 + (255 - sourceAlpha[0]) * (destAlpha[0] - 255)
  //                      = (255 - 0) * (510 - 255) + (255 - sourceAlpha[0]) * (destAlpha[0] - 255)
  //                      = MulAdd(255 - IntLv(0, sourceAlpha), IntLv(510, destAlpha) - 255)[0]
  i16x8_t zeroInterleavedWithSourceAlpha = simd::InterleaveHi16(simd::FromI16<i16x8_t>(0), s_rrrraaaa1234);
  i16x8_t fiveTenInterleavedWithDestAlpha = simd::InterleaveHi16(simd::FromI16<i16x8_t>(510), d_rrrraaaa1234);
  i16x8_t f1 = simd::Sub16(simd::FromI16<i16x8_t>(255), zeroInterleavedWithSourceAlpha);
  i16x8_t f2 = simd::Sub16(fiveTenInterleavedWithDestAlpha, simd::FromI16<i16x8_t>(255));
  return simd::FastDivideBy255(simd::MulAdd16x8x2To32x4(f1, f2));
}

template<typename u8x16_t, typename i16x8_t>
inline void
UnpackAndShuffleComponents(u8x16_t bgrabgrabgrabgra1234,
                           i16x8_t& bbbbgggg1234, i16x8_t& rrrraaaa1234)
{
  // bgrabgrabgrabgra1234 -> bbbbgggg1234, rrrraaaa1234
  i16x8_t bgrabgra12 = simd::UnpackLo8x8ToI16x8(bgrabgrabgrabgra1234);
  i16x8_t bgrabgra34 = simd::UnpackHi8x8ToI16x8(bgrabgrabgrabgra1234);
  i16x8_t bbggrraa13 = simd::InterleaveLo16(bgrabgra12, bgrabgra34);
  i16x8_t bbggrraa24 = simd::InterleaveHi16(bgrabgra12, bgrabgra34);
  bbbbgggg1234 = simd::InterleaveLo16(bbggrraa13, bbggrraa24);
  rrrraaaa1234 = simd::InterleaveHi16(bbggrraa13, bbggrraa24);
}

template<typename i32x4_t, typename i16x8_t, typename u8x16_t>
inline u8x16_t
ShuffleAndPackComponents(i32x4_t bbbb1234, i32x4_t gggg1234,
                         i32x4_t rrrr1234, const i32x4_t& aaaa1234)
{
  // bbbb1234, gggg1234, rrrr1234, aaaa1234 -> bgrabgrabgrabgra1234
  i16x8_t bbbbgggg1234 = simd::PackAndSaturate32To16(bbbb1234, gggg1234);
  i16x8_t rrrraaaa1234 = simd::PackAndSaturate32To16(rrrr1234, aaaa1234);
  i16x8_t brbrbrbr1234 = simd::InterleaveLo16(bbbbgggg1234, rrrraaaa1234);
  i16x8_t gagagaga1234 = simd::InterleaveHi16(bbbbgggg1234, rrrraaaa1234);
  i16x8_t bgrabgra12 = simd::InterleaveLo16(brbrbrbr1234, gagagaga1234);
  i16x8_t bgrabgra34 = simd::InterleaveHi16(brbrbrbr1234, gagagaga1234);
  return simd::PackAndSaturate16To8(bgrabgra12, bgrabgra34);
}

template<typename i32x4_t, typename i16x8_t, typename u8x16_t, BlendMode mode>
inline already_AddRefed<DataSourceSurface>
ApplyBlending_SIMD(DataSourceSurface* aInput1, DataSourceSurface* aInput2)
{
  IntSize size = aInput1->GetSize();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, SurfaceFormat::B8G8R8A8);
  if (!target) {
    return nullptr;
  }

  uint8_t* source1Data = aInput1->GetData();
  uint8_t* source2Data = aInput2->GetData();
  uint8_t* targetData = target->GetData();
  int32_t targetStride = target->Stride();
  int32_t source1Stride = aInput1->Stride();
  int32_t source2Stride = aInput2->Stride();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x += 4) {
      int32_t targetIndex = y * targetStride + 4 * x;
      int32_t source1Index = y * source1Stride + 4 * x;
      int32_t source2Index = y * source2Stride + 4 * x;

      u8x16_t s1234 = simd::Load8<u8x16_t>(&source2Data[source2Index]);
      u8x16_t d1234 = simd::Load8<u8x16_t>(&source1Data[source1Index]);

      // The blending calculation for the RGB channels all need access to the
      // alpha channel of their pixel, and the alpha calculation is different,
      // so it makes sense to separate by channel.

      i16x8_t s_bbbbgggg1234, s_rrrraaaa1234;
      i16x8_t d_bbbbgggg1234, d_rrrraaaa1234;
      UnpackAndShuffleComponents(s1234, s_bbbbgggg1234, s_rrrraaaa1234);
      UnpackAndShuffleComponents(d1234, d_bbbbgggg1234, d_rrrraaaa1234);
      i16x8_t s_aaaaaaaa1234 = simd::Shuffle32<3,2,3,2>(s_rrrraaaa1234);
      i16x8_t d_aaaaaaaa1234 = simd::Shuffle32<3,2,3,2>(d_rrrraaaa1234);

      // We only use blendedB, blendedG and blendedR.
      i32x4_t blendedB, blendedG, blendedR, blendedA;
      BlendTwoComponentsOfFourPixels<i16x8_t,i32x4_t,mode>(s_bbbbgggg1234, s_aaaaaaaa1234, d_bbbbgggg1234, d_aaaaaaaa1234, blendedB, blendedG);
      BlendTwoComponentsOfFourPixels<i16x8_t,i32x4_t,mode>(s_rrrraaaa1234, s_aaaaaaaa1234, d_rrrraaaa1234, d_aaaaaaaa1234, blendedR, blendedA);

      // Throw away blendedA and overwrite it with the correct blended alpha.
      blendedA = BlendAlphaOfFourPixels<i16x8_t,i32x4_t>(s_rrrraaaa1234, d_rrrraaaa1234);

      u8x16_t result1234 = ShuffleAndPackComponents<i32x4_t,i16x8_t,u8x16_t>(blendedB, blendedG, blendedR, blendedA);
      simd::Store8(&targetData[targetIndex], result1234);
    }
  }

  return target.forget();
}

template<typename i32x4_t, typename i16x8_t, typename u8x16_t>
static already_AddRefed<DataSourceSurface>
ApplyBlending_SIMD(DataSourceSurface* aInput1, DataSourceSurface* aInput2,
                      BlendMode aBlendMode)
{
  switch (aBlendMode) {
    case BLEND_MODE_MULTIPLY:
      return ApplyBlending_SIMD<i32x4_t,i16x8_t,u8x16_t, BLEND_MODE_MULTIPLY>(aInput1, aInput2);
    case BLEND_MODE_SCREEN:
      return ApplyBlending_SIMD<i32x4_t,i16x8_t,u8x16_t, BLEND_MODE_SCREEN>(aInput1, aInput2);
    case BLEND_MODE_DARKEN:
      return ApplyBlending_SIMD<i32x4_t,i16x8_t,u8x16_t, BLEND_MODE_DARKEN>(aInput1, aInput2);
    case BLEND_MODE_LIGHTEN:
      return ApplyBlending_SIMD<i32x4_t,i16x8_t,u8x16_t, BLEND_MODE_LIGHTEN>(aInput1, aInput2);
    default:
      return nullptr;
  }
}

template<MorphologyOperator Operator, typename u8x16_t>
static u8x16_t
Morph8(u8x16_t a, u8x16_t b)
{
  return Operator == MORPHOLOGY_OPERATOR_ERODE ?
    simd::Min8(a, b) : simd::Max8(a, b);
}

// Set every pixel to the per-component minimum or maximum of the pixels around
// it that are up to aRadius pixels away from it (horizontally).
template<MorphologyOperator op, typename i16x8_t, typename u8x16_t>
inline void ApplyMorphologyHorizontal_SIMD(uint8_t* aSourceData, int32_t aSourceStride,
                                           uint8_t* aDestData, int32_t aDestStride,
                                           const IntRect& aDestRect, int32_t aRadius)
{
  static_assert(op == MORPHOLOGY_OPERATOR_ERODE ||
                op == MORPHOLOGY_OPERATOR_DILATE,
                "unexpected morphology operator");

  int32_t kernelSize = aRadius + 1 + aRadius;
  MOZ_ASSERT(kernelSize >= 3, "don't call this with aRadius <= 0");
  MOZ_ASSERT(kernelSize % 4 == 1 || kernelSize % 4 == 3);
  int32_t completeKernelSizeForFourPixels = kernelSize + 3;
  MOZ_ASSERT(completeKernelSizeForFourPixels % 4 == 0 ||
             completeKernelSizeForFourPixels % 4 == 2);

  // aSourceData[-aRadius] and aDestData[0] are both aligned to 16 bytes, just
  // the way we need them to be.

  IntRect sourceRect = aDestRect;
  sourceRect.Inflate(aRadius, 0);

  for (int32_t y = aDestRect.y; y < aDestRect.YMost(); y++) {
    int32_t kernelStartX = aDestRect.x - aRadius;
    for (int32_t x = aDestRect.x; x < aDestRect.XMost(); x += 4, kernelStartX += 4) {
      // We process four pixels (16 color values) at a time.
      // aSourceData[0] points to the pixel located at aDestRect.TopLeft();
      // source values can be read beyond that because the source is extended
      // by aRadius pixels.

      int32_t sourceIndex = y * aSourceStride + 4 * kernelStartX;
      u8x16_t p1234 = simd::Load8<u8x16_t>(&aSourceData[sourceIndex]);
      u8x16_t m1234 = p1234;

      for (int32_t i = 4; i < completeKernelSizeForFourPixels; i += 4) {
        u8x16_t p5678 = (kernelStartX + i < sourceRect.XMost()) ?
          simd::Load8<u8x16_t>(&aSourceData[sourceIndex + 4 * i]) :
          simd::FromZero8<u8x16_t>();
        u8x16_t p2345 = simd::Rotate8<4>(p1234, p5678);
        u8x16_t p3456 = simd::Rotate8<8>(p1234, p5678);
        m1234 = Morph8<op,u8x16_t>(m1234, p2345);
        m1234 = Morph8<op,u8x16_t>(m1234, p3456);
        if (i + 2 < completeKernelSizeForFourPixels) {
          u8x16_t p4567 = simd::Rotate8<12>(p1234, p5678);
          m1234 = Morph8<op,u8x16_t>(m1234, p4567);
          m1234 = Morph8<op,u8x16_t>(m1234, p5678);
        }
        p1234 = p5678;
      }

      int32_t destIndex = y * aDestStride + 4 * x;
      simd::Store8(&aDestData[destIndex], m1234);
    }
  }
}

template<typename i16x8_t, typename u8x16_t>
inline void ApplyMorphologyHorizontal_SIMD(uint8_t* aSourceData, int32_t aSourceStride,
                                           uint8_t* aDestData, int32_t aDestStride,
                                           const IntRect& aDestRect, int32_t aRadius,
                                           MorphologyOperator aOp)
{
  if (aOp == MORPHOLOGY_OPERATOR_ERODE) {
    ApplyMorphologyHorizontal_SIMD<MORPHOLOGY_OPERATOR_ERODE,i16x8_t,u8x16_t>(
      aSourceData, aSourceStride, aDestData, aDestStride, aDestRect, aRadius);
  } else {
    ApplyMorphologyHorizontal_SIMD<MORPHOLOGY_OPERATOR_DILATE,i16x8_t,u8x16_t>(
      aSourceData, aSourceStride, aDestData, aDestStride, aDestRect, aRadius);
  }
}

// Set every pixel to the per-component minimum or maximum of the pixels around
// it that are up to aRadius pixels away from it (vertically).
template<MorphologyOperator op, typename i16x8_t, typename u8x16_t>
static void ApplyMorphologyVertical_SIMD(uint8_t* aSourceData, int32_t aSourceStride,
                                         uint8_t* aDestData, int32_t aDestStride,
                                         const IntRect& aDestRect, int32_t aRadius)
{
  static_assert(op == MORPHOLOGY_OPERATOR_ERODE ||
                op == MORPHOLOGY_OPERATOR_DILATE,
                "unexpected morphology operator");

  int32_t startY = aDestRect.y - aRadius;
  int32_t endY = aDestRect.y + aRadius;
  for (int32_t y = aDestRect.y; y < aDestRect.YMost(); y++, startY++, endY++) {
    for (int32_t x = aDestRect.x; x < aDestRect.XMost(); x += 4) {
      int32_t sourceIndex = startY * aSourceStride + 4 * x;
      u8x16_t u = simd::Load8<u8x16_t>(&aSourceData[sourceIndex]);
      sourceIndex += aSourceStride;
      for (int32_t iy = startY + 1; iy <= endY; iy++, sourceIndex += aSourceStride) {
        u8x16_t u2 = simd::Load8<u8x16_t>(&aSourceData[sourceIndex]);
        u = Morph8<op,u8x16_t>(u, u2);
      }

      int32_t destIndex = y * aDestStride + 4 * x;
      simd::Store8(&aDestData[destIndex], u);
    }
  }
}

template<typename i16x8_t, typename u8x16_t>
inline void ApplyMorphologyVertical_SIMD(uint8_t* aSourceData, int32_t aSourceStride,
                                           uint8_t* aDestData, int32_t aDestStride,
                                           const IntRect& aDestRect, int32_t aRadius,
                                           MorphologyOperator aOp)
{
  if (aOp == MORPHOLOGY_OPERATOR_ERODE) {
    ApplyMorphologyVertical_SIMD<MORPHOLOGY_OPERATOR_ERODE,i16x8_t,u8x16_t>(
      aSourceData, aSourceStride, aDestData, aDestStride, aDestRect, aRadius);
  } else {
    ApplyMorphologyVertical_SIMD<MORPHOLOGY_OPERATOR_DILATE,i16x8_t,u8x16_t>(
      aSourceData, aSourceStride, aDestData, aDestStride, aDestRect, aRadius);
  }
}

template<typename i32x4_t, typename i16x8_t>
static i32x4_t
ColorMatrixMultiply(i16x8_t p, i16x8_t rows_bg, i16x8_t rows_ra, const i32x4_t& bias)
{
  // int16_t p[8] == { b, g, r, a, b, g, r, a }.
  // int16_t rows_bg[8] == { bB, bG, bR, bA, gB, gG, gR, gA }.
  // int16_t rows_ra[8] == { rB, rG, rR, rA, aB, aG, aR, aA }.
  // int32_t bias[4] == { _B, _G, _R, _A }.

  i32x4_t sum = bias;

  // int16_t bg[8] = { b, g, b, g, b, g, b, g };
  i16x8_t bg = simd::ShuffleHi16<1,0,1,0>(simd::ShuffleLo16<1,0,1,0>(p));
  // int32_t prodsum_bg[4] = { b * bB + g * gB, b * bG + g * gG, b * bR + g * gR, b * bA + g * gA }
  i32x4_t prodsum_bg = simd::MulAdd16x8x2To32x4(bg, rows_bg);
  sum = simd::Add32(sum, prodsum_bg);

  // uint16_t ra[8] = { r, a, r, a, r, a, r, a };
  i16x8_t ra = simd::ShuffleHi16<3,2,3,2>(simd::ShuffleLo16<3,2,3,2>(p));
  // int32_t prodsum_ra[4] = { r * rB + a * aB, r * rG + a * aG, r * rR + a * aR, r * rA + a * aA }
  i32x4_t prodsum_ra = simd::MulAdd16x8x2To32x4(ra, rows_ra);
  sum = simd::Add32(sum, prodsum_ra);

  // int32_t sum[4] == { b * bB + g * gB + r * rB + a * aB + _B, ... }.
  return sum;
}

template<typename i32x4_t, typename i16x8_t, typename u8x16_t>
static already_AddRefed<DataSourceSurface>
ApplyColorMatrix_SIMD(DataSourceSurface* aInput, const Matrix5x4 &aMatrix)
{
  IntSize size = aInput->GetSize();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, SurfaceFormat::B8G8R8A8);
  if (!target) {
    return nullptr;
  }

  uint8_t* sourceData = aInput->GetData();
  uint8_t* targetData = target->GetData();
  int32_t sourceStride = aInput->Stride();
  int32_t targetStride = target->Stride();

  const int16_t factor = 128;
  const Float floatElementMax = INT16_MAX / factor; // 255
  MOZ_ASSERT((floatElementMax * factor) <= INT16_MAX, "badly chosen float-to-int scale");

  const Float *floats = &aMatrix._11;

  ptrdiff_t componentOffsets[4] = {
    B8G8R8A8_COMPONENT_BYTEOFFSET_R,
    B8G8R8A8_COMPONENT_BYTEOFFSET_G,
    B8G8R8A8_COMPONENT_BYTEOFFSET_B,
    B8G8R8A8_COMPONENT_BYTEOFFSET_A
  };

  // We store the color matrix in rows_bgra in the following format:
  // { bB, bG, bR, bA, gB, gG, gR, gA }.
  // { bB, gB, bG, gG, bR, gR, bA, gA }
  // The way this is interleaved allows us to use the intrinsic _mm_madd_epi16
  // which works especially well for our use case.
  int16_t rows_bgra[2][8];
  for (size_t rowIndex = 0; rowIndex < 4; rowIndex++) {
    for (size_t colIndex = 0; colIndex < 4; colIndex++) {
      const Float& floatMatrixElement = floats[rowIndex * 4 + colIndex];
      Float clampedFloatMatrixElement = std::min(std::max(floatMatrixElement, -floatElementMax), floatElementMax);
      int16_t scaledIntMatrixElement = int16_t(clampedFloatMatrixElement * factor + 0.5);
      int8_t bg_or_ra = componentOffsets[rowIndex] / 2;
      int8_t g_or_a = componentOffsets[rowIndex] % 2;
      int8_t B_or_G_or_R_or_A = componentOffsets[colIndex];
      rows_bgra[bg_or_ra][B_or_G_or_R_or_A * 2 + g_or_a] = scaledIntMatrixElement;
    }
  }

  int32_t rowBias[4];
  Float biasMax = (INT32_MAX - 4 * 255 * INT16_MAX) / (factor * 255);
  for (size_t colIndex = 0; colIndex < 4; colIndex++) {
    size_t rowIndex = 4;
    const Float& floatMatrixElement = floats[rowIndex * 4 + colIndex];
    Float clampedFloatMatrixElement = std::min(std::max(floatMatrixElement, -biasMax), biasMax);
    int32_t scaledIntMatrixElement = int32_t(clampedFloatMatrixElement * factor * 255 + 0.5);
    rowBias[componentOffsets[colIndex]] = scaledIntMatrixElement;
  }

  i16x8_t row_bg_v = simd::FromI16<i16x8_t>(
    rows_bgra[0][0], rows_bgra[0][1], rows_bgra[0][2], rows_bgra[0][3],
    rows_bgra[0][4], rows_bgra[0][5], rows_bgra[0][6], rows_bgra[0][7]);

  i16x8_t row_ra_v = simd::FromI16<i16x8_t>(
    rows_bgra[1][0], rows_bgra[1][1], rows_bgra[1][2], rows_bgra[1][3],
    rows_bgra[1][4], rows_bgra[1][5], rows_bgra[1][6], rows_bgra[1][7]);

  i32x4_t rowsBias_v =
    simd::From32<i32x4_t>(rowBias[0], rowBias[1], rowBias[2], rowBias[3]);

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x += 4) {
      MOZ_ASSERT(sourceStride >= 4 * (x + 4), "need to be able to read 4 pixels at this position");
      MOZ_ASSERT(targetStride >= 4 * (x + 4), "need to be able to write 4 pixels at this position");
      int32_t sourceIndex = y * sourceStride + 4 * x;
      int32_t targetIndex = y * targetStride + 4 * x;

      // We load 4 pixels, unpack them, process them 1 pixel at a time, and
      // finally pack and store the 4 result pixels.

      u8x16_t p1234 = simd::Load8<u8x16_t>(&sourceData[sourceIndex]);

      // Splat needed to get each pixel twice into i16x8
      i16x8_t p11 = simd::UnpackLo8x8ToI16x8(simd::Splat32On8<0>(p1234));
      i16x8_t p22 = simd::UnpackLo8x8ToI16x8(simd::Splat32On8<1>(p1234));
      i16x8_t p33 = simd::UnpackLo8x8ToI16x8(simd::Splat32On8<2>(p1234));
      i16x8_t p44 = simd::UnpackLo8x8ToI16x8(simd::Splat32On8<3>(p1234));

      i32x4_t result_p1 = ColorMatrixMultiply(p11, row_bg_v, row_ra_v, rowsBias_v);
      i32x4_t result_p2 = ColorMatrixMultiply(p22, row_bg_v, row_ra_v, rowsBias_v);
      i32x4_t result_p3 = ColorMatrixMultiply(p33, row_bg_v, row_ra_v, rowsBias_v);
      i32x4_t result_p4 = ColorMatrixMultiply(p44, row_bg_v, row_ra_v, rowsBias_v);

      static_assert(factor == 1 << 7, "Please adapt the calculation in the lines below for a different factor.");
      u8x16_t result_p1234 = simd::PackAndSaturate32To8(simd::ShiftRight32<7>(result_p1),
                                                        simd::ShiftRight32<7>(result_p2),
                                                        simd::ShiftRight32<7>(result_p3),
                                                        simd::ShiftRight32<7>(result_p4));
      simd::Store8(&targetData[targetIndex], result_p1234);
    }
  }

  return target.forget();
}

// source / dest: bgra bgra
// sourceAlpha / destAlpha: aaaa aaaa
// result: bgra bgra
template<typename i32x4_t, typename u16x8_t, uint32_t aCompositeOperator>
static inline u16x8_t
CompositeTwoPixels(u16x8_t source, u16x8_t sourceAlpha, u16x8_t dest, const u16x8_t& destAlpha)
{
  u16x8_t x255 = simd::FromU16<u16x8_t>(255);

  switch (aCompositeOperator) {

    case COMPOSITE_OPERATOR_OVER:
    {
      // val = dest * (255 - sourceAlpha) + source * 255;
      u16x8_t twoFiftyFiveMinusSourceAlpha = simd::Sub16(x255, sourceAlpha);

      u16x8_t destSourceInterleaved1 = simd::InterleaveLo16(dest, source);
      u16x8_t rightFactor1 = simd::InterleaveLo16(twoFiftyFiveMinusSourceAlpha, x255);
      i32x4_t result1 = simd::MulAdd16x8x2To32x4(destSourceInterleaved1, rightFactor1);

      u16x8_t destSourceInterleaved2 = simd::InterleaveHi16(dest, source);
      u16x8_t rightFactor2 = simd::InterleaveHi16(twoFiftyFiveMinusSourceAlpha, x255);
      i32x4_t result2 = simd::MulAdd16x8x2To32x4(destSourceInterleaved2, rightFactor2);

      return simd::PackAndSaturate32ToU16(simd::FastDivideBy255(result1),
                                          simd::FastDivideBy255(result2));
    }

    case COMPOSITE_OPERATOR_IN:
    {
      // val = source * destAlpha;
      return simd::FastDivideBy255_16(simd::Mul16(source, destAlpha));
    }

    case COMPOSITE_OPERATOR_OUT:
    {
      // val = source * (255 - destAlpha);
      u16x8_t prod = simd::Mul16(source, simd::Sub16(x255, destAlpha));
      return simd::FastDivideBy255_16(prod);
    }

    case COMPOSITE_OPERATOR_ATOP:
    {
      // val = dest * (255 - sourceAlpha) + source * destAlpha;
      u16x8_t twoFiftyFiveMinusSourceAlpha = simd::Sub16(x255, sourceAlpha);

      u16x8_t destSourceInterleaved1 = simd::InterleaveLo16(dest, source);
      u16x8_t rightFactor1 = simd::InterleaveLo16(twoFiftyFiveMinusSourceAlpha, destAlpha);
      i32x4_t result1 = simd::MulAdd16x8x2To32x4(destSourceInterleaved1, rightFactor1);

      u16x8_t destSourceInterleaved2 = simd::InterleaveHi16(dest, source);
      u16x8_t rightFactor2 = simd::InterleaveHi16(twoFiftyFiveMinusSourceAlpha, destAlpha);
      i32x4_t result2 = simd::MulAdd16x8x2To32x4(destSourceInterleaved2, rightFactor2);

      return simd::PackAndSaturate32ToU16(simd::FastDivideBy255(result1),
                                          simd::FastDivideBy255(result2));
    }

    case COMPOSITE_OPERATOR_XOR:
    {
      // val = dest * (255 - sourceAlpha) + source * (255 - destAlpha);
      u16x8_t twoFiftyFiveMinusSourceAlpha = simd::Sub16(x255, sourceAlpha);
      u16x8_t twoFiftyFiveMinusDestAlpha = simd::Sub16(x255, destAlpha);

      u16x8_t destSourceInterleaved1 = simd::InterleaveLo16(dest, source);
      u16x8_t rightFactor1 = simd::InterleaveLo16(twoFiftyFiveMinusSourceAlpha,
                                                     twoFiftyFiveMinusDestAlpha);
      i32x4_t result1 = simd::MulAdd16x8x2To32x4(destSourceInterleaved1, rightFactor1);

      u16x8_t destSourceInterleaved2 = simd::InterleaveHi16(dest, source);
      u16x8_t rightFactor2 = simd::InterleaveHi16(twoFiftyFiveMinusSourceAlpha,
                                                     twoFiftyFiveMinusDestAlpha);
      i32x4_t result2 = simd::MulAdd16x8x2To32x4(destSourceInterleaved2, rightFactor2);

      return simd::PackAndSaturate32ToU16(simd::FastDivideBy255(result1),
                                          simd::FastDivideBy255(result2));
    }

    default:
      return simd::FromU16<u16x8_t>(0);

  }
}

template<typename i32x4_t, typename u16x8_t, typename u8x16_t, uint32_t op>
static void
ApplyComposition(DataSourceSurface* aSource, DataSourceSurface* aDest)
{
  IntSize size = aDest->GetSize();

  uint8_t* sourceData = aSource->GetData();
  uint8_t* destData = aDest->GetData();
  uint32_t sourceStride = aSource->Stride();
  uint32_t destStride = aDest->Stride();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x += 4) {
      uint32_t sourceIndex = y * sourceStride + 4 * x;
      uint32_t destIndex = y * destStride + 4 * x;

      u8x16_t s1234 = simd::Load8<u8x16_t>(&sourceData[sourceIndex]);
      u8x16_t d1234 = simd::Load8<u8x16_t>(&destData[destIndex]);

      u16x8_t s12 = simd::UnpackLo8x8ToU16x8(s1234);
      u16x8_t d12 = simd::UnpackLo8x8ToU16x8(d1234);
      u16x8_t sa12 = simd::Splat16<3,3>(s12);
      u16x8_t da12 = simd::Splat16<3,3>(d12);
      u16x8_t result12 = CompositeTwoPixels<i32x4_t,u16x8_t,op>(s12, sa12, d12, da12);

      u16x8_t s34 = simd::UnpackHi8x8ToU16x8(s1234);
      u16x8_t d34 = simd::UnpackHi8x8ToU16x8(d1234);
      u16x8_t sa34 = simd::Splat16<3,3>(s34);
      u16x8_t da34 = simd::Splat16<3,3>(d34);
      u16x8_t result34 = CompositeTwoPixels<i32x4_t,u16x8_t,op>(s34, sa34, d34, da34);

      u8x16_t result1234 = simd::PackAndSaturate16To8(result12, result34);
      simd::Store8(&destData[destIndex], result1234);
    }
  }
}

template<typename i32x4_t, typename i16x8_t, typename u8x16_t>
static void
ApplyComposition_SIMD(DataSourceSurface* aSource, DataSourceSurface* aDest,
                      CompositeOperator aOperator)
{
  switch (aOperator) {
    case COMPOSITE_OPERATOR_OVER:
      ApplyComposition<i32x4_t,i16x8_t,u8x16_t, COMPOSITE_OPERATOR_OVER>(aSource, aDest);
      break;
    case COMPOSITE_OPERATOR_IN:
      ApplyComposition<i32x4_t,i16x8_t,u8x16_t, COMPOSITE_OPERATOR_IN>(aSource, aDest);
      break;
    case COMPOSITE_OPERATOR_OUT:
      ApplyComposition<i32x4_t,i16x8_t,u8x16_t, COMPOSITE_OPERATOR_OUT>(aSource, aDest);
      break;
    case COMPOSITE_OPERATOR_ATOP:
      ApplyComposition<i32x4_t,i16x8_t,u8x16_t, COMPOSITE_OPERATOR_ATOP>(aSource, aDest);
      break;
    case COMPOSITE_OPERATOR_XOR:
      ApplyComposition<i32x4_t,i16x8_t,u8x16_t, COMPOSITE_OPERATOR_XOR>(aSource, aDest);
      break;
    default:
      MOZ_CRASH("GFX: Incomplete switch");
  }
}

template<typename u8x16_t>
static void
SeparateColorChannels_SIMD(const IntSize &size, uint8_t* sourceData, int32_t sourceStride,
                           uint8_t* channel0Data, uint8_t* channel1Data,
                           uint8_t* channel2Data, uint8_t* channel3Data,
                           int32_t channelStride)
{
  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x += 16) {
      // Process 16 pixels at a time.
      int32_t sourceIndex = y * sourceStride + 4 * x;
      int32_t targetIndex = y * channelStride + x;

      u8x16_t bgrabgrabgrabgra1 = simd::FromZero8<u8x16_t>();
      u8x16_t bgrabgrabgrabgra2 = simd::FromZero8<u8x16_t>();
      u8x16_t bgrabgrabgrabgra3 = simd::FromZero8<u8x16_t>();
      u8x16_t bgrabgrabgrabgra4 = simd::FromZero8<u8x16_t>();

      bgrabgrabgrabgra1 = simd::Load8<u8x16_t>(&sourceData[sourceIndex]);
      if (4 * (x + 4) < sourceStride) {
        bgrabgrabgrabgra2 = simd::Load8<u8x16_t>(&sourceData[sourceIndex + 4 * 4]);
      }
      if (4 * (x + 8) < sourceStride) {
        bgrabgrabgrabgra3 = simd::Load8<u8x16_t>(&sourceData[sourceIndex + 4 * 8]);
      }
      if (4 * (x + 12) < sourceStride) {
        bgrabgrabgrabgra4 = simd::Load8<u8x16_t>(&sourceData[sourceIndex + 4 * 12]);
      }

      u8x16_t bbggrraabbggrraa1 = simd::InterleaveLo8(bgrabgrabgrabgra1, bgrabgrabgrabgra3);
      u8x16_t bbggrraabbggrraa2 = simd::InterleaveHi8(bgrabgrabgrabgra1, bgrabgrabgrabgra3);
      u8x16_t bbggrraabbggrraa3 = simd::InterleaveLo8(bgrabgrabgrabgra2, bgrabgrabgrabgra4);
      u8x16_t bbggrraabbggrraa4 = simd::InterleaveHi8(bgrabgrabgrabgra2, bgrabgrabgrabgra4);
      u8x16_t bbbbggggrrrraaaa1 = simd::InterleaveLo8(bbggrraabbggrraa1, bbggrraabbggrraa3);
      u8x16_t bbbbggggrrrraaaa2 = simd::InterleaveHi8(bbggrraabbggrraa1, bbggrraabbggrraa3);
      u8x16_t bbbbggggrrrraaaa3 = simd::InterleaveLo8(bbggrraabbggrraa2, bbggrraabbggrraa4);
      u8x16_t bbbbggggrrrraaaa4 = simd::InterleaveHi8(bbggrraabbggrraa2, bbggrraabbggrraa4);
      u8x16_t bbbbbbbbgggggggg1 = simd::InterleaveLo8(bbbbggggrrrraaaa1, bbbbggggrrrraaaa3);
      u8x16_t rrrrrrrraaaaaaaa1 = simd::InterleaveHi8(bbbbggggrrrraaaa1, bbbbggggrrrraaaa3);
      u8x16_t bbbbbbbbgggggggg2 = simd::InterleaveLo8(bbbbggggrrrraaaa2, bbbbggggrrrraaaa4);
      u8x16_t rrrrrrrraaaaaaaa2 = simd::InterleaveHi8(bbbbggggrrrraaaa2, bbbbggggrrrraaaa4);
      u8x16_t bbbbbbbbbbbbbbbb = simd::InterleaveLo8(bbbbbbbbgggggggg1, bbbbbbbbgggggggg2);
      u8x16_t gggggggggggggggg = simd::InterleaveHi8(bbbbbbbbgggggggg1, bbbbbbbbgggggggg2);
      u8x16_t rrrrrrrrrrrrrrrr = simd::InterleaveLo8(rrrrrrrraaaaaaaa1, rrrrrrrraaaaaaaa2);
      u8x16_t aaaaaaaaaaaaaaaa = simd::InterleaveHi8(rrrrrrrraaaaaaaa1, rrrrrrrraaaaaaaa2);

      simd::Store8(&channel0Data[targetIndex], bbbbbbbbbbbbbbbb);
      simd::Store8(&channel1Data[targetIndex], gggggggggggggggg);
      simd::Store8(&channel2Data[targetIndex], rrrrrrrrrrrrrrrr);
      simd::Store8(&channel3Data[targetIndex], aaaaaaaaaaaaaaaa);
    }
  }
}

template<typename u8x16_t>
static void
CombineColorChannels_SIMD(const IntSize &size, int32_t resultStride, uint8_t* resultData, int32_t channelStride, uint8_t* channel0Data, uint8_t* channel1Data, uint8_t* channel2Data, uint8_t* channel3Data)
{
  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x += 16) {
      // Process 16 pixels at a time.
      int32_t resultIndex = y * resultStride + 4 * x;
      int32_t channelIndex = y * channelStride + x;

      u8x16_t bbbbbbbbbbbbbbbb = simd::Load8<u8x16_t>(&channel0Data[channelIndex]);
      u8x16_t gggggggggggggggg = simd::Load8<u8x16_t>(&channel1Data[channelIndex]);
      u8x16_t rrrrrrrrrrrrrrrr = simd::Load8<u8x16_t>(&channel2Data[channelIndex]);
      u8x16_t aaaaaaaaaaaaaaaa = simd::Load8<u8x16_t>(&channel3Data[channelIndex]);

      u8x16_t brbrbrbrbrbrbrbr1 = simd::InterleaveLo8(bbbbbbbbbbbbbbbb, rrrrrrrrrrrrrrrr);
      u8x16_t brbrbrbrbrbrbrbr2 = simd::InterleaveHi8(bbbbbbbbbbbbbbbb, rrrrrrrrrrrrrrrr);
      u8x16_t gagagagagagagaga1 = simd::InterleaveLo8(gggggggggggggggg, aaaaaaaaaaaaaaaa);
      u8x16_t gagagagagagagaga2 = simd::InterleaveHi8(gggggggggggggggg, aaaaaaaaaaaaaaaa);

      u8x16_t bgrabgrabgrabgra1 = simd::InterleaveLo8(brbrbrbrbrbrbrbr1, gagagagagagagaga1);
      u8x16_t bgrabgrabgrabgra2 = simd::InterleaveHi8(brbrbrbrbrbrbrbr1, gagagagagagagaga1);
      u8x16_t bgrabgrabgrabgra3 = simd::InterleaveLo8(brbrbrbrbrbrbrbr2, gagagagagagagaga2);
      u8x16_t bgrabgrabgrabgra4 = simd::InterleaveHi8(brbrbrbrbrbrbrbr2, gagagagagagagaga2);

      simd::Store8(&resultData[resultIndex], bgrabgrabgrabgra1);
      if (4 * (x + 4) < resultStride) {
        simd::Store8(&resultData[resultIndex + 4 * 4], bgrabgrabgrabgra2);
      }
      if (4 * (x + 8) < resultStride) {
        simd::Store8(&resultData[resultIndex + 8 * 4], bgrabgrabgrabgra3);
      }
      if (4 * (x + 12) < resultStride) {
        simd::Store8(&resultData[resultIndex + 12 * 4], bgrabgrabgrabgra4);
      }
    }
  }
}


template<typename i32x4_t, typename u16x8_t, typename u8x16_t>
static void
DoPremultiplicationCalculation_SIMD(const IntSize& aSize,
                                    uint8_t* aTargetData, int32_t aTargetStride,
                                    uint8_t* aSourceData, int32_t aSourceStride)
{
  const u8x16_t alphaMask = simd::From8<u8x16_t>(0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff);
  for (int32_t y = 0; y < aSize.height; y++) {
    for (int32_t x = 0; x < aSize.width; x += 4) {
      int32_t inputIndex = y * aSourceStride + 4 * x;
      int32_t targetIndex = y * aTargetStride + 4 * x;

      u8x16_t p1234 = simd::Load8<u8x16_t>(&aSourceData[inputIndex]);
      u16x8_t p12 = simd::UnpackLo8x8ToU16x8(p1234);
      u16x8_t p34 = simd::UnpackHi8x8ToU16x8(p1234);

      // Multiply all components with alpha.
      p12 = simd::Mul16(p12, simd::Splat16<3,3>(p12));
      p34 = simd::Mul16(p34, simd::Splat16<3,3>(p34));

      // Divide by 255 and pack.
      u8x16_t result = simd::PackAndSaturate16To8(simd::FastDivideBy255_16(p12),
                                                  simd::FastDivideBy255_16(p34));

      // Get the original alpha channel value back from p1234.
      result = simd::Pick(alphaMask, result, p1234);

      simd::Store8(&aTargetData[targetIndex], result);
    }
  }
}

// We use a table of precomputed factors for unpremultiplying.
// We want to compute round(r / (alpha / 255.0f)) for arbitrary values of
// r and alpha in constant time. This table of factors has the property that
// (r * sAlphaFactors[alpha] + 128) >> 8 roughly gives the result we want (with
// a maximum deviation of 1).
//
// sAlphaFactors[alpha] == round(255.0 * (1 << 8) / alpha)
//
// This table has been created using the python code
// ", ".join("%d" % (round(255.0 * 256 / alpha) if alpha > 0 else 0) for alpha in range(256))
static const uint16_t sAlphaFactors[256] = {
  0, 65280, 32640, 21760, 16320, 13056, 10880, 9326, 8160, 7253, 6528, 5935,
  5440, 5022, 4663, 4352, 4080, 3840, 3627, 3436, 3264, 3109, 2967, 2838, 2720,
  2611, 2511, 2418, 2331, 2251, 2176, 2106, 2040, 1978, 1920, 1865, 1813, 1764,
  1718, 1674, 1632, 1592, 1554, 1518, 1484, 1451, 1419, 1389, 1360, 1332, 1306,
  1280, 1255, 1232, 1209, 1187, 1166, 1145, 1126, 1106, 1088, 1070, 1053, 1036,
  1020, 1004, 989, 974, 960, 946, 933, 919, 907, 894, 882, 870, 859, 848, 837,
  826, 816, 806, 796, 787, 777, 768, 759, 750, 742, 733, 725, 717, 710, 702,
  694, 687, 680, 673, 666, 659, 653, 646, 640, 634, 628, 622, 616, 610, 604,
  599, 593, 588, 583, 578, 573, 568, 563, 558, 553, 549, 544, 540, 535, 531,
  526, 522, 518, 514, 510, 506, 502, 498, 495, 491, 487, 484, 480, 476, 473,
  470, 466, 463, 460, 457, 453, 450, 447, 444, 441, 438, 435, 432, 429, 427,
  424, 421, 418, 416, 413, 411, 408, 405, 403, 400, 398, 396, 393, 391, 389,
  386, 384, 382, 380, 377, 375, 373, 371, 369, 367, 365, 363, 361, 359, 357,
  355, 353, 351, 349, 347, 345, 344, 342, 340, 338, 336, 335, 333, 331, 330,
  328, 326, 325, 323, 322, 320, 318, 317, 315, 314, 312, 311, 309, 308, 306,
  305, 304, 302, 301, 299, 298, 297, 295, 294, 293, 291, 290, 289, 288, 286,
  285, 284, 283, 281, 280, 279, 278, 277, 275, 274, 273, 272, 271, 270, 269,
  268, 266, 265, 264, 263, 262, 261, 260, 259, 258, 257, 256
};

template<typename u16x8_t, typename u8x16_t>
static void
DoUnpremultiplicationCalculation_SIMD(const IntSize& aSize,
                                 uint8_t* aTargetData, int32_t aTargetStride,
                                 uint8_t* aSourceData, int32_t aSourceStride)
{
  for (int32_t y = 0; y < aSize.height; y++) {
    for (int32_t x = 0; x < aSize.width; x += 4) {
      int32_t inputIndex = y * aSourceStride + 4 * x;
      int32_t targetIndex = y * aTargetStride + 4 * x;
      union {
        u8x16_t p1234;
        uint8_t u8[4][4];
      };
      p1234 = simd::Load8<u8x16_t>(&aSourceData[inputIndex]);

      // Prepare the alpha factors.
      uint16_t aF1 = sAlphaFactors[u8[0][B8G8R8A8_COMPONENT_BYTEOFFSET_A]];
      uint16_t aF2 = sAlphaFactors[u8[1][B8G8R8A8_COMPONENT_BYTEOFFSET_A]];
      uint16_t aF3 = sAlphaFactors[u8[2][B8G8R8A8_COMPONENT_BYTEOFFSET_A]];
      uint16_t aF4 = sAlphaFactors[u8[3][B8G8R8A8_COMPONENT_BYTEOFFSET_A]];
      u16x8_t aF12 = simd::FromU16<u16x8_t>(aF1, aF1, aF1, 1 << 8, aF2, aF2, aF2, 1 << 8);
      u16x8_t aF34 = simd::FromU16<u16x8_t>(aF3, aF3, aF3, 1 << 8, aF4, aF4, aF4, 1 << 8);

      u16x8_t p12 = simd::UnpackLo8x8ToU16x8(p1234);
      u16x8_t p34 = simd::UnpackHi8x8ToU16x8(p1234);

      // Multiply with the alpha factors, add 128 for rounding, and shift right by 8 bits.
      p12 = simd::ShiftRight16<8>(simd::Add16(simd::Mul16(p12, aF12), simd::FromU16<u16x8_t>(128)));
      p34 = simd::ShiftRight16<8>(simd::Add16(simd::Mul16(p34, aF34), simd::FromU16<u16x8_t>(128)));

      u8x16_t result = simd::PackAndSaturate16To8(p12, p34);
      simd::Store8(&aTargetData[targetIndex], result);
    }
  }
}

template<typename f32x4_t, typename i32x4_t, typename u8x16_t>
static already_AddRefed<DataSourceSurface>
RenderTurbulence_SIMD(const IntSize &aSize, const Point &aOffset, const Size &aBaseFrequency,
                      int32_t aSeed, int aNumOctaves, TurbulenceType aType, bool aStitch, const Rect &aTileRect)
{
#define RETURN_TURBULENCE(Type, Stitch) \
  SVGTurbulenceRenderer<Type,Stitch,f32x4_t,i32x4_t,u8x16_t> \
    renderer(aBaseFrequency, aSeed, aNumOctaves, aTileRect); \
  return renderer.Render(aSize, aOffset);

  switch (aType) {
    case TURBULENCE_TYPE_TURBULENCE:
    {
      if (aStitch) {
        RETURN_TURBULENCE(TURBULENCE_TYPE_TURBULENCE, true);
      }
      RETURN_TURBULENCE(TURBULENCE_TYPE_TURBULENCE, false);
    }
    case TURBULENCE_TYPE_FRACTAL_NOISE:
    {
      if (aStitch) {
        RETURN_TURBULENCE(TURBULENCE_TYPE_FRACTAL_NOISE, true);
      }
      RETURN_TURBULENCE(TURBULENCE_TYPE_FRACTAL_NOISE, false);
    }
  }
  return nullptr;
#undef RETURN_TURBULENCE
}

// k1 * in1 * in2 + k2 * in1 + k3 * in2 + k4
template<typename i32x4_t, typename i16x8_t>
static MOZ_ALWAYS_INLINE i16x8_t
ArithmeticCombineTwoPixels(i16x8_t in1, i16x8_t in2,
                           const i16x8_t &k1And4, const i16x8_t &k2And3)
{
  // Calculate input product: inProd = (in1 * in2) / 255.
  i32x4_t inProd_1, inProd_2;
  simd::Mul16x4x2x2To32x4x2(in1, in2, inProd_1, inProd_2);
  i16x8_t inProd = simd::PackAndSaturate32To16(simd::FastDivideBy255(inProd_1), simd::FastDivideBy255(inProd_2));

  // Calculate k1 * ((in1 * in2) / 255) + (k4/128) * 128
  i16x8_t oneTwentyEight = simd::FromI16<i16x8_t>(128);
  i16x8_t inProd1AndOneTwentyEight = simd::InterleaveLo16(inProd, oneTwentyEight);
  i16x8_t inProd2AndOneTwentyEight = simd::InterleaveHi16(inProd, oneTwentyEight);
  i32x4_t inProdTimesK1PlusK4_1 = simd::MulAdd16x8x2To32x4(k1And4, inProd1AndOneTwentyEight);
  i32x4_t inProdTimesK1PlusK4_2 = simd::MulAdd16x8x2To32x4(k1And4, inProd2AndOneTwentyEight);

  // Calculate k2 * in1 + k3 * in2
  i16x8_t in12_1 = simd::InterleaveLo16(in1, in2);
  i16x8_t in12_2 = simd::InterleaveHi16(in1, in2);
  i32x4_t inTimesK2K3_1 = simd::MulAdd16x8x2To32x4(k2And3, in12_1);
  i32x4_t inTimesK2K3_2 = simd::MulAdd16x8x2To32x4(k2And3, in12_2);

  // Sum everything up and truncate the fractional part.
  i32x4_t result_1 = simd::ShiftRight32<7>(simd::Add32(inProdTimesK1PlusK4_1, inTimesK2K3_1));
  i32x4_t result_2 = simd::ShiftRight32<7>(simd::Add32(inProdTimesK1PlusK4_2, inTimesK2K3_2));
  return simd::PackAndSaturate32To16(result_1, result_2);
}

template<typename i32x4_t, typename i16x8_t, typename u8x16_t>
static already_AddRefed<DataSourceSurface>
ApplyArithmeticCombine_SIMD(DataSourceSurface* aInput1, DataSourceSurface* aInput2,
                            Float aK1, Float aK2, Float aK3, Float aK4)
{
  IntSize size = aInput1->GetSize();
  RefPtr<DataSourceSurface> target =
  Factory::CreateDataSourceSurface(size, SurfaceFormat::B8G8R8A8);
  if (!target) {
    return nullptr;
  }

  uint8_t* source1Data = aInput1->GetData();
  uint8_t* source2Data = aInput2->GetData();
  uint8_t* targetData = target->GetData();
  uint32_t source1Stride = aInput1->Stride();
  uint32_t source2Stride = aInput2->Stride();
  uint32_t targetStride = target->Stride();

  // The arithmetic combine filter does the following calculation:
  // result = k1 * in1 * in2 + k2 * in1 + k3 * in2 + k4
  //
  // Or, with in1/2 integers between 0 and 255:
  // result = (k1 * in1 * in2) / 255 + k2 * in1 + k3 * in2 + k4 * 255
  //
  // We want the whole calculation to happen in integer, with 16-bit factors.
  // So we convert our factors to fixed-point with precision 1.8.7.
  // K4 is premultiplied with 255, and it will be multiplied with 128 later
  // during the actual calculation, because premultiplying it with 255 * 128
  // would overflow int16.

  i16x8_t k1 = simd::FromI16<i16x8_t>(int16_t(floorf(std::min(std::max(aK1, -255.0f), 255.0f) * 128 + 0.5f)));
  i16x8_t k2 = simd::FromI16<i16x8_t>(int16_t(floorf(std::min(std::max(aK2, -255.0f), 255.0f) * 128 + 0.5f)));
  i16x8_t k3 = simd::FromI16<i16x8_t>(int16_t(floorf(std::min(std::max(aK3, -255.0f), 255.0f) * 128 + 0.5f)));
  i16x8_t k4 = simd::FromI16<i16x8_t>(int16_t(floorf(std::min(std::max(aK4, -128.0f), 128.0f) * 255 + 0.5f)));

  i16x8_t k1And4 = simd::InterleaveLo16(k1, k4);
  i16x8_t k2And3 = simd::InterleaveLo16(k2, k3);

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x += 4) {
      uint32_t source1Index = y * source1Stride + 4 * x;
      uint32_t source2Index = y * source2Stride + 4 * x;
      uint32_t targetIndex = y * targetStride + 4 * x;

      // Load and unpack.
      u8x16_t in1 = simd::Load8<u8x16_t>(&source1Data[source1Index]);
      u8x16_t in2 = simd::Load8<u8x16_t>(&source2Data[source2Index]);
      i16x8_t in1_12 = simd::UnpackLo8x8ToI16x8(in1);
      i16x8_t in1_34 = simd::UnpackHi8x8ToI16x8(in1);
      i16x8_t in2_12 = simd::UnpackLo8x8ToI16x8(in2);
      i16x8_t in2_34 = simd::UnpackHi8x8ToI16x8(in2);

      // Multiply and add.
      i16x8_t result_12 = ArithmeticCombineTwoPixels<i32x4_t,i16x8_t>(in1_12, in2_12, k1And4, k2And3);
      i16x8_t result_34 = ArithmeticCombineTwoPixels<i32x4_t,i16x8_t>(in1_34, in2_34, k1And4, k2And3);

      // Pack and store.
      simd::Store8(&targetData[targetIndex], simd::PackAndSaturate16To8(result_12, result_34));
    }
  }

  return target.forget();
}

} // namespace gfx
} // namespace mozilla
