/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBitmapColorUtils.h"
#include "libyuv.h"

namespace mozilla {
namespace dom {

/*
 * Utility function form libyuv source files.
 */
static __inline int32 clamp0(int32 v) {
  return ((-(v) >> 31) & (v));
}

static __inline int32 clamp255(int32 v) {
  return (((255 - (v)) >> 31) | (v)) & 255;
}

static __inline uint32 Clamp(int32 val) {
  int v = clamp0(val);
  return (uint32)(clamp255(v));
}

#define YG 74 /* (int8)(1.164 * 64 + 0.5) */

#define UB 127 /* min(63,(int8)(2.018 * 64)) */
#define UG -25 /* (int8)(-0.391 * 64 - 0.5) */
#define UR 0

#define VB 0
#define VG -52 /* (int8)(-0.813 * 64 - 0.5) */
#define VR 102 /* (int8)(1.596 * 64 + 0.5) */

// Bias
#define BB UB * 128 + VB * 128
#define BG UG * 128 + VG * 128
#define BR UR * 128 + VR * 128

static __inline void
YuvPixel(uint8 y, uint8 u, uint8 v, uint8* b, uint8* g, uint8* r)
{
  int32 y1 = ((int32)(y) - 16) * YG;
  *b = Clamp((int32)((u * UB + v * VB) - (BB) + y1) >> 6);
  *g = Clamp((int32)((u * UG + v * VG) - (BG) + y1) >> 6);
  *r = Clamp((int32)((u * UR + v * VR) - (BR) + y1) >> 6);
}

static __inline int
RGBToY(uint8 r, uint8 g, uint8 b)
{
  return (66 * r + 129 * g +  25 * b + 0x1080) >> 8;
}

static __inline int
RGBToU(uint8 r, uint8 g, uint8 b)
{
  return (112 * b - 74 * g - 38 * r + 0x8080) >> 8;
}

static __inline int
RGBToV(uint8 r, uint8 g, uint8 b)
{
  return (112 * r - 94 * g - 18 * b + 0x8080) >> 8;
}

/*
 * Generic functions.
 */
template<int aSrcRIndex, int aSrcGIndex, int aSrcBIndex,
         int aDstRIndex, int aDstGIndex, int aDstBIndex, int aDstAIndex>
static int
RGBFamilyToRGBAFamily(const uint8_t* aSrcBuffer, int aSrcStride,
                      uint8_t* aDstBuffer, int aDstStride,
                      int aWidth, int aHeight)
{
  static_assert(aSrcRIndex == 0 || aSrcRIndex == 2, "Wrong SrcR index.");
  static_assert(aSrcGIndex == 1, "Wrong SrcG index.");
  static_assert(aSrcBIndex == 0 || aSrcBIndex == 2, "Wrong SrcB index.");
  static_assert(aDstRIndex == 0 || aDstRIndex == 2, "Wrong DstR index.");
  static_assert(aDstGIndex == 1, "Wrong DstG index.");
  static_assert(aDstBIndex == 0 || aDstBIndex == 2, "Wrong DstB index.");
  static_assert(aDstAIndex == 3, "Wrong DstA index.");

  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    for (int j = 0; j < aWidth; ++j) {
      uint8_t r = *(srcBuffer + aSrcRIndex);
      uint8_t g = *(srcBuffer + aSrcGIndex);
      uint8_t b = *(srcBuffer + aSrcBIndex);
      *(dstBuffer + aDstRIndex) = r;
      *(dstBuffer + aDstGIndex) = g;
      *(dstBuffer + aDstBIndex) = b;
      *(dstBuffer + aDstAIndex) = 255;
      srcBuffer += 3;
      dstBuffer += 4;
    }
  }

  return 0;
}

template<int aSrcRIndex, int aSrcGIndex, int aSrcBIndex,
         int aDstRIndex, int aDstGIndex, int aDstBIndex>
static int
RGBAFamilyToRGBFamily(const uint8_t* aSrcBuffer, int aSrcStride,
                      uint8_t* aDstBuffer, int aDstStride,
                      int aWidth, int aHeight)
{
  static_assert(aSrcRIndex == 0 || aSrcRIndex == 2, "Wrong SrcR index.");
  static_assert(aSrcGIndex == 1, "Wrong SrcG index.");
  static_assert(aSrcBIndex == 0 || aSrcBIndex == 2, "Wrong SrcB index.");
  static_assert(aDstRIndex == 0 || aDstRIndex == 2, "Wrong DstR index.");
  static_assert(aDstGIndex == 1, "Wrong DstG index.");
  static_assert(aDstBIndex == 0 || aDstBIndex == 2, "Wrong DstB index.");

  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    for (int j = 0; j < aWidth; ++j) {
      uint8_t r = *(srcBuffer + aSrcRIndex);
      uint8_t g = *(srcBuffer + aSrcGIndex);
      uint8_t b = *(srcBuffer + aSrcBIndex);
      *(dstBuffer + aDstRIndex) = r;
      *(dstBuffer + aDstGIndex) = g;
      *(dstBuffer + aDstBIndex) = b;
      srcBuffer += 4;
      dstBuffer += 3;
    }
  }

  return 0;
}

template<int aPixel1YOffset, int aPixel1UOffset, int aPixel1VOffset,
         int aPixel2YOffset, int aPixel2UOffset, int aPixel2VOffset,
         int aYStep, int aUStep, int aVStep,
         int aRIndex, int aGIndex, int aBIndex>
void
YUVFamilyToRGBFamily_Row(const uint8_t* aYBuffer,
                         const uint8_t* aUBuffer,
                         const uint8_t* aVBuffer,
                         uint8_t* aDstBuffer,
                         int aWidth)
{
  static_assert(aRIndex == 0 || aRIndex == 2, "Wrong R index.");
  static_assert(aGIndex == 1, "Wrong G index.");
  static_assert(aBIndex == 0 || aBIndex == 2, "Wrong B index.");

  for (int j = 0; j < aWidth - 1; j += 2) {
    YuvPixel(aYBuffer[aPixel1YOffset], aUBuffer[aPixel1UOffset], aVBuffer[aPixel1VOffset],
             aDstBuffer + aBIndex, aDstBuffer + aGIndex, aDstBuffer + aRIndex);
    YuvPixel(aYBuffer[aPixel2YOffset], aUBuffer[aPixel2UOffset], aVBuffer[aPixel2VOffset],
             aDstBuffer + aBIndex + 3, aDstBuffer + aGIndex + 3, aDstBuffer + aRIndex + 3);
    aYBuffer += aYStep;
    aUBuffer += aUStep;
    aVBuffer += aVStep;
    aDstBuffer += 6;
  }

  if (aWidth & 1) {
    YuvPixel(aYBuffer[aPixel1YOffset], aUBuffer[aPixel1UOffset], aVBuffer[aPixel1VOffset],
             aDstBuffer + aBIndex, aDstBuffer + aGIndex, aDstBuffer + aRIndex);
  }
}

template<int aPixel1YOffset, int aPixel1UOffset, int aPixel1VOffset,
         int aPixel2YOffset, int aPixel2UOffset, int aPixel2VOffset,
         int aYStep, int aUStep, int aVStep,
         int aRIndex, int aGIndex, int aBIndex, int aAIndex>
void
YUVFamilyToRGBAFamily_Row(const uint8_t* aYBuffer,
                          const uint8_t* aUBuffer,
                          const uint8_t* aVBuffer,
                          uint8_t* aDstBuffer,
                          int aWidth)
{
  static_assert(aRIndex == 0 || aRIndex == 2, "Wrong R index.");
  static_assert(aGIndex == 1, "Wrong G index.");
  static_assert(aBIndex == 0 || aBIndex == 2, "Wrong B index.");
  static_assert(aAIndex == 3, "Wrong A index.");

  for (int j = 0; j < aWidth - 1; j += 2) {
    YuvPixel(aYBuffer[aPixel1YOffset], aUBuffer[aPixel1UOffset], aVBuffer[aPixel1VOffset],
             aDstBuffer + aBIndex, aDstBuffer + aGIndex, aDstBuffer + aRIndex);
    YuvPixel(aYBuffer[aPixel2YOffset], aUBuffer[aPixel2UOffset], aVBuffer[aPixel2VOffset],
             aDstBuffer + aBIndex + 4, aDstBuffer + aGIndex + 4, aDstBuffer + aRIndex + 4);
    aDstBuffer[aAIndex] = 255;
    aDstBuffer[aAIndex + 4] = 255;

    aYBuffer += aYStep;
    aUBuffer += aUStep;
    aVBuffer += aVStep;
    aDstBuffer += 8;
  }

  if (aWidth & 1) {
    YuvPixel(aYBuffer[aPixel1YOffset], aUBuffer[aPixel1UOffset], aVBuffer[aPixel1VOffset],
             aDstBuffer + aBIndex, aDstBuffer + aGIndex, aDstBuffer + aRIndex);
    aDstBuffer[aAIndex] = 255;
  }
}

template< int aRIndex, int aGIndex, int aBIndex>
static void
RGBFamilyToY_Row(const uint8_t* aSrcBuffer, uint8_t* aYBuffer, int aWidth)
{
  static_assert(aRIndex == 0 || aRIndex == 2, "Wrong R index.");
  static_assert(aGIndex == 1, "Wrong G index.");
  static_assert(aBIndex == 0 || aBIndex == 2, "Wrong B index.");

  for (int j = 0; j < aWidth - 1; j += 2) {
    aYBuffer[0] = RGBToY(aSrcBuffer[aRIndex], aSrcBuffer[aGIndex], aSrcBuffer[aBIndex]);
    aYBuffer[1] = RGBToY(aSrcBuffer[aRIndex + 3], aSrcBuffer[aGIndex + 3], aSrcBuffer[aBIndex + 3]);

    aYBuffer += 2;
    aSrcBuffer += 3 * 2;
  }

  if (aWidth & 1) {
    aYBuffer[0] = RGBToY(aSrcBuffer[aRIndex], aSrcBuffer[aGIndex], aSrcBuffer[aBIndex]);
  }
}

template< int aRIndex, int aGIndex, int aBIndex, int aUStep, int aVStep>
static void
RGBFamilyToUV_Row(const uint8_t* aSrcBuffer, int aSrcStride,
                  uint8_t* aUBuffer, uint8_t* aVBuffer, int aWidth)
{
  static_assert(aRIndex == 0 || aRIndex == 2, "Wrong R index.");
  static_assert(aGIndex == 1, "Wrong G index.");
  static_assert(aBIndex == 0 || aBIndex == 2, "Wrong B index.");

  uint8_t averageR = 0;
  uint8_t averageG = 0;
  uint8_t averageB = 0;

  const uint8_t* aSrcBufferNextRow = aSrcBuffer + aSrcStride;
  for (int j = 0; j < aWidth - 1; j += 2) {
    averageR = (aSrcBuffer[aRIndex] + aSrcBuffer[aRIndex + 3] + aSrcBufferNextRow[aRIndex] + aSrcBufferNextRow[aRIndex + 3]) >> 2;
    averageG = (aSrcBuffer[aGIndex] + aSrcBuffer[aGIndex + 3] + aSrcBufferNextRow[aGIndex] + aSrcBufferNextRow[aGIndex + 3]) >> 2;
    averageB = (aSrcBuffer[aBIndex] + aSrcBuffer[aBIndex + 3] + aSrcBufferNextRow[aBIndex] + aSrcBufferNextRow[aBIndex + 3]) >> 2;

    aUBuffer[0] = RGBToU(averageR, averageG, averageB);
    aVBuffer[0] = RGBToV(averageR, averageG, averageB);

    aUBuffer += aUStep;
    aVBuffer += aVStep;
    aSrcBuffer += 3 * 2;
    aSrcBufferNextRow += 3 * 2;
  }

  if (aWidth & 1) {
    averageR = (aSrcBuffer[aRIndex] + aSrcBufferNextRow[aRIndex]) >> 1;
    averageG = (aSrcBuffer[aGIndex] + aSrcBufferNextRow[aGIndex]) >> 1;
    averageB = (aSrcBuffer[aBIndex] + aSrcBufferNextRow[aBIndex]) >> 1;

    aUBuffer[0] = RGBToU(averageR, averageG, averageB);
    aVBuffer[0] = RGBToV(averageR, averageG, averageB);
  }
}

template< int aRIndex, int aGIndex, int aBIndex>
static void
RGBAFamilyToY_Row(const uint8_t* aSrcBuffer, uint8_t* aYBuffer, int aWidth)
{
  static_assert(aRIndex == 0 || aRIndex == 2, "Wrong R index.");
  static_assert(aGIndex == 1, "Wrong G index.");
  static_assert(aBIndex == 0 || aBIndex == 2, "Wrong B index.");

  for (int j = 0; j < aWidth - 1; j += 2) {
    aYBuffer[0] = RGBToY(aSrcBuffer[aRIndex], aSrcBuffer[aGIndex], aSrcBuffer[aBIndex]);
    aYBuffer[1] = RGBToY(aSrcBuffer[aRIndex + 4], aSrcBuffer[aGIndex + 4], aSrcBuffer[aBIndex + 4]);

    aYBuffer += 2;
    aSrcBuffer += 4 * 2;
  }

  if (aWidth & 1) {
    aYBuffer[0] = RGBToY(aSrcBuffer[aRIndex], aSrcBuffer[aGIndex], aSrcBuffer[aBIndex]);
  }
}

template< int aRIndex, int aGIndex, int aBIndex, int aUStep, int aVStep>
static void
RGBAFamilyToUV_Row(const uint8_t* aSrcBuffer, int aSrcStride,
                   uint8_t* aUBuffer, uint8_t* aVBuffer, int aWidth)
{
  static_assert(aRIndex == 0 || aRIndex == 2, "Wrong R index.");
  static_assert(aGIndex == 1, "Wrong G index.");
  static_assert(aBIndex == 0 || aBIndex == 2, "Wrong B index.");

  uint8_t averageR = 0;
  uint8_t averageG = 0;
  uint8_t averageB = 0;

  const uint8_t* aSrcBufferNextRow = aSrcBuffer + aSrcStride;
  for (int j = 0; j < aWidth - 1; j += 2) {
    averageR = (aSrcBuffer[aRIndex] + aSrcBuffer[aRIndex + 4] + aSrcBufferNextRow[aRIndex] + aSrcBufferNextRow[aRIndex + 4]) >> 2;
    averageG = (aSrcBuffer[aGIndex] + aSrcBuffer[aGIndex + 4] + aSrcBufferNextRow[aGIndex] + aSrcBufferNextRow[aGIndex + 4]) >> 2;
    averageB = (aSrcBuffer[aBIndex] + aSrcBuffer[aBIndex + 4] + aSrcBufferNextRow[aBIndex] + aSrcBufferNextRow[aBIndex + 4]) >> 2;

    aUBuffer[0] = RGBToU(averageR, averageG, averageB);
    aVBuffer[0] = RGBToV(averageR, averageG, averageB);

    aUBuffer += aUStep;
    aVBuffer += aVStep;
    aSrcBuffer += 4 * 2;
    aSrcBufferNextRow += 4 * 2;
  }

  if (aWidth & 1) {
    averageR = (aSrcBuffer[aRIndex] + aSrcBufferNextRow[aRIndex]) >> 1;
    averageG = (aSrcBuffer[aGIndex] + aSrcBufferNextRow[aGIndex]) >> 1;
    averageB = (aSrcBuffer[aBIndex] + aSrcBufferNextRow[aBIndex]) >> 1;

    aUBuffer[0] = RGBToU(averageR, averageG, averageB);
    aVBuffer[0] = RGBToV(averageR, averageG, averageB);
  }
}

/*
 * RGB family -> RGBA family.
 */
int
RGB24ToRGBA32(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight)
{
  return RGBFamilyToRGBAFamily<0, 1, 2, 0, 1, 2, 3>(aSrcBuffer, aSrcStride,
                                                    aDstBuffer, aDstStride,
                                                    aWidth, aHeight);
}

int
BGR24ToRGBA32(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight)
{
  return RGBFamilyToRGBAFamily<2, 1, 0, 0, 1, 2, 3>(aSrcBuffer, aSrcStride,
                                                    aDstBuffer, aDstStride,
                                                    aWidth, aHeight);
}

int
RGB24ToBGRA32(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight)
{
  return RGBFamilyToRGBAFamily<0, 1, 2, 2, 1, 0, 3>(aSrcBuffer, aSrcStride,
                                                    aDstBuffer, aDstStride,
                                                    aWidth, aHeight);
}

int
BGR24ToBGRA32(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight)
{
  return RGBFamilyToRGBAFamily<2, 1, 0, 2, 1, 0, 3>(aSrcBuffer, aSrcStride,
                                                    aDstBuffer, aDstStride,
                                                    aWidth, aHeight);
}

/*
 * RGBA family -> RGB family.
 */
int
RGBA32ToRGB24(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight)
{
  return RGBAFamilyToRGBFamily<0, 1, 2, 0, 1, 2>(aSrcBuffer, aSrcStride,
                                                 aDstBuffer, aDstStride,
                                                 aWidth, aHeight);
}

int
BGRA32ToRGB24(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight)
{
  return RGBAFamilyToRGBFamily<2, 1, 0, 0, 1, 2>(aSrcBuffer, aSrcStride,
                                                 aDstBuffer, aDstStride,
                                                 aWidth, aHeight);
}

int
RGBA32ToBGR24(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight)
{
  return RGBAFamilyToRGBFamily<0, 1, 2, 2, 1, 0>(aSrcBuffer, aSrcStride,
                                                 aDstBuffer, aDstStride,
                                                 aWidth, aHeight);
}

int
BGRA32ToBGR24(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight)
{
  return RGBAFamilyToRGBFamily<2, 1, 0, 2, 1, 0>(aSrcBuffer, aSrcStride,
                                                 aDstBuffer, aDstStride,
                                                 aWidth, aHeight);
}

/*
 * Among RGB family.
 */

int
RGB24ToBGR24(const uint8_t* aSrcBuffer, int aSrcStride,
             uint8_t* aDstBuffer, int aDstStride,
             int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    for (int j = 0; j < aWidth; ++j) {
      *(dstBuffer + 0) = *(srcBuffer + 2);
      *(dstBuffer + 1) = *(srcBuffer + 1);
      *(dstBuffer + 2) = *(srcBuffer + 0);
      srcBuffer += 3;
      dstBuffer += 3;
    }
  }

  return 0;
}

/*
 * YUV family -> RGB family.
 */
int
YUV444PToRGB24(const uint8_t* aYBuffer, int aYStride,
               const uint8_t* aUBuffer, int aUStride,
               const uint8_t* aVBuffer, int aVStride,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUBuffer + aUStride * i;
    const uint8_t* vBuffer = aVBuffer + aVStride * i;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBFamily_Row<0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 1, 2>(yBuffer,
                                                                 uBuffer,
                                                                 vBuffer,
                                                                 dstBuffer,
                                                                 aWidth);
  }

  return 0;
}

int
YUV422PToRGB24(const uint8_t* aYBuffer, int aYStride,
               const uint8_t* aUBuffer, int aUStride,
               const uint8_t* aVBuffer, int aVStride,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUBuffer + aUStride * i;
    const uint8_t* vBuffer = aVBuffer + aVStride * i;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBFamily_Row<0, 0, 0, 1, 0, 0, 2, 1, 1, 0, 1, 2>(yBuffer,
                                                                 uBuffer,
                                                                 vBuffer,
                                                                 dstBuffer,
                                                                 aWidth);
  }

  return 0;
}

int
YUV420PToRGB24(const uint8_t* aYBuffer, int aYStride,
               const uint8_t* aUBuffer, int aUStride,
               const uint8_t* aVBuffer, int aVStride,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUBuffer + aUStride * (i / 2);
    const uint8_t* vBuffer = aVBuffer + aVStride * (i / 2);
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBFamily_Row<0, 0, 0, 1, 0, 0, 2, 1, 1, 0, 1, 2>(yBuffer,
                                                                 uBuffer,
                                                                 vBuffer,
                                                                 dstBuffer,
                                                                 aWidth);
  }

  return 0;
}

int
NV12ToRGB24(const uint8_t* aYBuffer, int aYStride,
            const uint8_t* aUVBuffer, int aUVStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUVBuffer + aUVStride * (i / 2);
    const uint8_t* vBuffer = aUVBuffer + aUVStride * (i / 2) + 1;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBFamily_Row<0, 0, 0, 1, 0, 0, 2, 2, 2, 0, 1, 2>(yBuffer,
                                                                 uBuffer,
                                                                 vBuffer,
                                                                 dstBuffer,
                                                                 aWidth);
  }

  return 0;
}

int
NV21ToRGB24(const uint8_t* aYBuffer, int aYStride,
            const uint8_t* aVUBuffer, int aVUStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aVUBuffer + aVUStride * (i / 2) + 1;
    const uint8_t* vBuffer = aVUBuffer + aVUStride * (i / 2);
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBFamily_Row<0, 0, 0, 1, 0, 0, 2, 2, 2, 0, 1, 2>(yBuffer,
                                                                 uBuffer,
                                                                 vBuffer,
                                                                 dstBuffer,
                                                                 aWidth);
  }

  return 0;
}

int
YUV444PToBGR24(const uint8_t* aYBuffer, int aYStride,
               const uint8_t* aUBuffer, int aUStride,
               const uint8_t* aVBuffer, int aVStride,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUBuffer + aUStride * i;
    const uint8_t* vBuffer = aVBuffer + aVStride * i;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBFamily_Row<0, 0, 0, 1, 1, 1, 2, 2, 2, 2, 1, 0>(yBuffer,
                                                                 uBuffer,
                                                                 vBuffer,
                                                                 dstBuffer,
                                                                 aWidth);
  }

  return 0;
}

int
YUV422PToBGR24(const uint8_t* aYBuffer, int aYStride,
               const uint8_t* aUBuffer, int aUStride,
               const uint8_t* aVBuffer, int aVStride,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUBuffer + aUStride * i;
    const uint8_t* vBuffer = aVBuffer + aVStride * i;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBFamily_Row<0, 0, 0, 1, 0, 0, 2, 1, 1, 2, 1, 0>(yBuffer,
                                                                 uBuffer,
                                                                 vBuffer,
                                                                 dstBuffer,
                                                                 aWidth);
  }

  return 0;
}

int
YUV420PToBGR24(const uint8_t* aYBuffer, int aYStride,
               const uint8_t* aUBuffer, int aUStride,
               const uint8_t* aVBuffer, int aVStride,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUBuffer + aUStride * (i / 2);
    const uint8_t* vBuffer = aVBuffer + aVStride * (i / 2);
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBFamily_Row<0, 0, 0, 1, 0, 0, 2, 1, 1, 2, 1, 0>(yBuffer,
                                                                 uBuffer,
                                                                 vBuffer,
                                                                 dstBuffer,
                                                                 aWidth);
  }

  return 0;
}

int
NV12ToBGR24(const uint8_t* aYBuffer, int aYStride,
            const uint8_t* aUVBuffer, int aUVStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUVBuffer + aUVStride * (i / 2);
    const uint8_t* vBuffer = aUVBuffer + aUVStride * (i / 2) + 1;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBFamily_Row<0, 0, 0, 1, 0, 0, 2, 2, 2, 2, 1, 0>(yBuffer,
                                                                 uBuffer,
                                                                 vBuffer,
                                                                 dstBuffer,
                                                                 aWidth);
  }

  return 0;
}

int
NV21ToBGR24(const uint8_t* aYBuffer, int aYStride,
            const uint8_t* aVUBuffer, int aVUStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aVUBuffer + aVUStride * (i / 2) + 1;
    const uint8_t* vBuffer = aVUBuffer + aVUStride * (i / 2);
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBFamily_Row<0, 0, 0, 1, 0, 0, 2, 2, 2, 2, 1, 0>(yBuffer,
                                                                 uBuffer,
                                                                 vBuffer,
                                                                 dstBuffer,
                                                                 aWidth);
  }

  return 0;
}

/*
 * YUV family -> RGBA family.
 */
int
YUV444PToRGBA32(const uint8_t* aYBuffer, int aYStride,
                const uint8_t* aUBuffer, int aUStride,
                const uint8_t* aVBuffer, int aVStride,
                uint8_t* aDstBuffer, int aDstStride,
                int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUBuffer + aUStride * i;
    const uint8_t* vBuffer = aVBuffer + aVStride * i;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBAFamily_Row<0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 1, 2, 3>(yBuffer,
                                                                     uBuffer,
                                                                     vBuffer,
                                                                     dstBuffer,
                                                                     aWidth);
  }

  return 0;
}

int
YUV422PToRGBA32(const uint8_t* aYBuffer, int aYStride,
                const uint8_t* aUBuffer, int aUStride,
                const uint8_t* aVBuffer, int aVStride,
                uint8_t* aDstBuffer, int aDstStride,
                int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUBuffer + aUStride * i;
    const uint8_t* vBuffer = aVBuffer + aVStride * i;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBAFamily_Row<0, 0, 0, 1, 0, 0, 2, 1, 1, 0, 1, 2, 3>(yBuffer,
                                                                     uBuffer,
                                                                     vBuffer,
                                                                     dstBuffer,
                                                                     aWidth);
  }

  return 0;
}

int
YUV420PToRGBA32(const uint8_t* aYBuffer, int aYStride,
                const uint8_t* aUBuffer, int aUStride,
                const uint8_t* aVBuffer, int aVStride,
                uint8_t* aDstBuffer, int aDstStride,
                int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUBuffer + aUStride * (i / 2);
    const uint8_t* vBuffer = aVBuffer + aVStride * (i / 2);
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBAFamily_Row<0, 0, 0, 1, 0, 0, 2, 1, 1, 0, 1, 2, 3>(yBuffer,
                                                                     uBuffer,
                                                                     vBuffer,
                                                                     dstBuffer,
                                                                     aWidth);
  }

  return 0;
}

int
NV12ToRGBA32(const uint8_t* aYBuffer, int aYStride,
             const uint8_t* aUVBuffer, int aUVStride,
             uint8_t* aDstBuffer, int aDstStride,
             int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUVBuffer + aUVStride * (i / 2);
    const uint8_t* vBuffer = aUVBuffer + aUVStride * (i / 2) + 1;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBAFamily_Row<0, 0, 0, 1, 0, 0, 2, 2, 2, 0, 1, 2, 3>(yBuffer,
                                                                     uBuffer,
                                                                     vBuffer,
                                                                     dstBuffer,
                                                                     aWidth);
  }

  return 0;
}

int
NV21ToRGBA32(const uint8_t* aYBuffer, int aYStride,
             const uint8_t* aVUBuffer, int aVUStride,
             uint8_t* aDstBuffer, int aDstStride,
             int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aVUBuffer + aVUStride * (i / 2) + 1;
    const uint8_t* vBuffer = aVUBuffer + aVUStride * (i / 2);
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBAFamily_Row<0, 0, 0, 1, 0, 0, 2, 2, 2, 0, 1, 2, 3>(yBuffer,
                                                                     uBuffer,
                                                                     vBuffer,
                                                                     dstBuffer,
                                                                     aWidth);
  }

  return 0;
}

int
YUV444PToBGRA32(const uint8_t* aYBuffer, int aYStride,
                const uint8_t* aUBuffer, int aUStride,
                const uint8_t* aVBuffer, int aVStride,
                uint8_t* aDstBuffer, int aDstStride,
                int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUBuffer + aUStride * i;
    const uint8_t* vBuffer = aVBuffer + aVStride * i;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBAFamily_Row<0, 0, 0, 1, 1, 1, 2, 2, 2, 2, 1, 0, 3>(yBuffer,
                                                                     uBuffer,
                                                                     vBuffer,
                                                                     dstBuffer,
                                                                     aWidth);
  }

  return 0;
}

int
YUV422PToBGRA32(const uint8_t* aYBuffer, int aYStride,
                const uint8_t* aUBuffer, int aUStride,
                const uint8_t* aVBuffer, int aVStride,
                uint8_t* aDstBuffer, int aDstStride,
                int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUBuffer + aUStride * i;
    const uint8_t* vBuffer = aVBuffer + aVStride * i;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBAFamily_Row<0, 0, 0, 1, 0, 0, 2, 1, 1, 2, 1, 0, 3>(yBuffer,
                                                                     uBuffer,
                                                                     vBuffer,
                                                                     dstBuffer,
                                                                     aWidth);
  }

  return 0;
}

int
YUV420PToBGRA32(const uint8_t* aYBuffer, int aYStride,
                const uint8_t* aUBuffer, int aUStride,
                const uint8_t* aVBuffer, int aVStride,
                uint8_t* aDstBuffer, int aDstStride,
                int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUBuffer + aUStride * (i / 2);
    const uint8_t* vBuffer = aVBuffer + aVStride * (i / 2);
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBAFamily_Row<0, 0, 0, 1, 0, 0, 2, 1, 1, 2, 1, 0, 3>(yBuffer,
                                                                     uBuffer,
                                                                     vBuffer,
                                                                     dstBuffer,
                                                                     aWidth);
  }

  return 0;
}

int
NV12ToBGRA32(const uint8_t* aYBuffer, int aYStride,
             const uint8_t* aUVBuffer, int aUVStride,
             uint8_t* aDstBuffer, int aDstStride,
             int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aUVBuffer + aUVStride * (i / 2);
    const uint8_t* vBuffer = aUVBuffer + aUVStride * (i / 2) + 1;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBAFamily_Row<0, 0, 0, 1, 0, 0, 2, 2, 2, 2, 1, 0, 3>(yBuffer,
                                                                     uBuffer,
                                                                     vBuffer,
                                                                     dstBuffer,
                                                                     aWidth);
  }

  return 0;
}

int
NV21ToBGRA32(const uint8_t* aYBuffer, int aYStride,
             const uint8_t* aVUBuffer, int aVUStride,
             uint8_t* aDstBuffer, int aDstStride,
             int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* yBuffer = aYBuffer + aYStride * i;
    const uint8_t* uBuffer = aVUBuffer + aVUStride * (i / 2) + 1;
    const uint8_t* vBuffer = aVUBuffer + aVUStride * (i / 2);
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    YUVFamilyToRGBAFamily_Row<0, 0, 0, 1, 0, 0, 2, 2, 2, 2, 1, 0, 3>(yBuffer,
                                                                     uBuffer,
                                                                     vBuffer,
                                                                     dstBuffer,
                                                                     aWidth);
  }

  return 0;
}

/*
 * RGB family -> YUV family.
 */
int
RGB24ToYUV444P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * i;
    uint8_t* vBuffer = aVBuffer + aVStride * i;

    for (int j = 0; j < aWidth; ++j) {
      yBuffer[0] = RGBToY(srcBuffer[0], srcBuffer[1], srcBuffer[2]);
      uBuffer[0] = RGBToU(srcBuffer[0], srcBuffer[1], srcBuffer[2]);
      vBuffer[0] = RGBToV(srcBuffer[0], srcBuffer[1], srcBuffer[2]);

      yBuffer += 1;
      uBuffer += 1;
      vBuffer += 1;
      srcBuffer += 3;
    }
  }

  return 0;
}

int
RGB24ToYUV422P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * i;
    uint8_t* vBuffer = aVBuffer + aVStride * i;

    RGBFamilyToY_Row<0, 1, 2>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBFamilyToUV_Row<0, 1, 2, 1, 1>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

int
RGB24ToYUV420P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight - 1; i += 2) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * (i / 2);
    uint8_t* vBuffer = aVBuffer + aVStride * (i / 2);

    RGBFamilyToY_Row<0, 1, 2>(srcBuffer, yBuffer, aWidth);
    RGBFamilyToY_Row<0, 1, 2>(srcBuffer + aSrcStride, yBuffer + aYStride, aWidth);
    RGBFamilyToUV_Row<0, 1, 2, 1, 1>(srcBuffer, aSrcStride, uBuffer, vBuffer, aWidth);
  }

  if (aHeight & 1) {
    const int i = aHeight - 1;
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * (i / 2);
    uint8_t* vBuffer = aVBuffer + aVStride * (i / 2);

    RGBFamilyToY_Row<0, 1, 2>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBFamilyToUV_Row<0, 1, 2, 1, 1>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

int
RGB24ToNV12(const uint8_t* aSrcBuffer, int aSrcStride,
            uint8_t* aYBuffer, int aYStride,
            uint8_t* aUVBuffer, int aUVStride,
            int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight - 1; i += 2) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUVBuffer + aUVStride * (i / 2);
    uint8_t* vBuffer = aUVBuffer + aUVStride * (i / 2) + 1;

    RGBFamilyToY_Row<0, 1, 2>(srcBuffer, yBuffer, aWidth);
    RGBFamilyToY_Row<0, 1, 2>(srcBuffer + aSrcStride, yBuffer + aYStride, aWidth);
    RGBFamilyToUV_Row<0, 1, 2, 2, 2>(srcBuffer, aSrcStride, uBuffer, vBuffer, aWidth);
  }

  if (aHeight & 1) {
    const int i = aHeight - 1;
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUVBuffer + aUVStride * (i / 2);
    uint8_t* vBuffer = aUVBuffer + aUVStride * (i / 2) + 1;

    RGBFamilyToY_Row<0, 1, 2>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBFamilyToUV_Row<0, 1, 2, 2, 2>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

int
RGB24ToNV21(const uint8_t* aSrcBuffer, int aSrcStride,
            uint8_t* aYBuffer, int aYStride,
            uint8_t* aVUBuffer, int aVUStride,
            int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight - 1; i += 2) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aVUBuffer + aVUStride * (i / 2) + 1;
    uint8_t* vBuffer = aVUBuffer + aVUStride * (i / 2);

    RGBFamilyToY_Row<0, 1, 2>(srcBuffer, yBuffer, aWidth);
    RGBFamilyToY_Row<0, 1, 2>(srcBuffer + aSrcStride, yBuffer + aYStride, aWidth);
    RGBFamilyToUV_Row<0, 1, 2, 2, 2>(srcBuffer, aSrcStride, uBuffer, vBuffer, aWidth);
  }

  if (aHeight & 1) {
    const int i = aHeight - 1;
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aVUBuffer + aVUStride * (i / 2) + 1;
    uint8_t* vBuffer = aVUBuffer + aVUStride * (i / 2);

    RGBFamilyToY_Row<0, 1, 2>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBFamilyToUV_Row<0, 1, 2, 2, 2>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

int
BGR24ToYUV444P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * i;
    uint8_t* vBuffer = aVBuffer + aVStride * i;

    for (int j = 0; j < aWidth; ++j) {
      yBuffer[0] = RGBToY(srcBuffer[2], srcBuffer[1], srcBuffer[0]);
      uBuffer[0] = RGBToU(srcBuffer[2], srcBuffer[1], srcBuffer[0]);
      vBuffer[0] = RGBToV(srcBuffer[2], srcBuffer[1], srcBuffer[0]);

      yBuffer += 1;
      uBuffer += 1;
      vBuffer += 1;
      srcBuffer += 3;
    }
  }

  return 0;
}

int
BGR24ToYUV422P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * i;
    uint8_t* vBuffer = aVBuffer + aVStride * i;

    RGBFamilyToY_Row<2, 1, 0>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBFamilyToUV_Row<2, 1, 0, 1, 1>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

int
BGR24ToYUV420P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight - 1; i += 2) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * (i / 2);
    uint8_t* vBuffer = aVBuffer + aVStride * (i / 2);

    RGBFamilyToY_Row<2, 1, 0>(srcBuffer, yBuffer, aWidth);
    RGBFamilyToY_Row<2, 1, 0>(srcBuffer + aSrcStride, yBuffer + aYStride, aWidth);
    RGBFamilyToUV_Row<2, 1, 0, 1, 1>(srcBuffer, aSrcStride, uBuffer, vBuffer, aWidth);
  }

  if (aHeight & 1) {
    const int i = aHeight - 1;
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * (i / 2);
    uint8_t* vBuffer = aVBuffer + aVStride * (i / 2);

    RGBFamilyToY_Row<2, 1, 0>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBFamilyToUV_Row<2, 1, 0, 1, 1>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

int
BGR24ToNV12(const uint8_t* aSrcBuffer, int aSrcStride,
            uint8_t* aYBuffer, int aYStride,
            uint8_t* aUVBuffer, int aUVStride,
            int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight - 1; i += 2) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUVBuffer + aUVStride * (i / 2);
    uint8_t* vBuffer = aUVBuffer + aUVStride * (i / 2) + 1;

    RGBFamilyToY_Row<2, 1, 0>(srcBuffer, yBuffer, aWidth);
    RGBFamilyToY_Row<2, 1, 0>(srcBuffer + aSrcStride, yBuffer + aYStride, aWidth);
    RGBFamilyToUV_Row<2, 1, 0, 2, 2>(srcBuffer, aSrcStride, uBuffer, vBuffer, aWidth);
  }

  if (aHeight & 1) {
    const int i = aHeight - 1;
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUVBuffer + aUVStride * (i / 2);
    uint8_t* vBuffer = aUVBuffer + aUVStride * (i / 2) + 1;

    RGBFamilyToY_Row<2, 1, 0>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBFamilyToUV_Row<2, 1, 0, 2, 2>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

int
BGR24ToNV21(const uint8_t* aSrcBuffer, int aSrcStride,
            uint8_t* aYBuffer, int aYStride,
            uint8_t* aVUBuffer, int aVUStride,
            int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight - 1; i += 2) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aVUBuffer + aVUStride * (i / 2) + 1;
    uint8_t* vBuffer = aVUBuffer + aVUStride * (i / 2);

    RGBFamilyToY_Row<2, 1, 0>(srcBuffer, yBuffer, aWidth);
    RGBFamilyToY_Row<2, 1, 0>(srcBuffer + aSrcStride, yBuffer + aYStride, aWidth);
    RGBFamilyToUV_Row<2, 1, 0, 2, 2>(srcBuffer, aSrcStride, uBuffer, vBuffer, aWidth);
  }

  if (aHeight & 1) {
    const int i = aHeight - 1;
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aVUBuffer + aVUStride * (i / 2) + 1;
    uint8_t* vBuffer = aVUBuffer + aVUStride * (i / 2);

    RGBFamilyToY_Row<2, 1, 0>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBFamilyToUV_Row<2, 1, 0, 2, 2>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

/*
 * RGBA family -> YUV family.
 */
int
RGBA32ToYUV444P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * i;
    uint8_t* vBuffer = aVBuffer + aVStride * i;

    for (int j = 0; j < aWidth; ++j) {
      yBuffer[0] = RGBToY(srcBuffer[0], srcBuffer[1], srcBuffer[2]);
      uBuffer[0] = RGBToU(srcBuffer[0], srcBuffer[1], srcBuffer[2]);
      vBuffer[0] = RGBToV(srcBuffer[0], srcBuffer[1], srcBuffer[2]);

      yBuffer += 1;
      uBuffer += 1;
      vBuffer += 1;
      srcBuffer += 4;
    }
  }

  return 0;
}

int
RGBA32ToYUV422P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * i;
    uint8_t* vBuffer = aVBuffer + aVStride * i;

    RGBAFamilyToY_Row<0, 1, 2>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBAFamilyToUV_Row<0, 1, 2, 1, 1>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

int
RGBA32ToYUV420P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight - 1; i += 2) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * (i / 2);
    uint8_t* vBuffer = aVBuffer + aVStride * (i / 2);

    RGBAFamilyToY_Row<0, 1, 2>(srcBuffer, yBuffer, aWidth);
    RGBAFamilyToY_Row<0, 1, 2>(srcBuffer + aSrcStride, yBuffer + aYStride, aWidth);
    RGBAFamilyToUV_Row<0, 1, 2, 1, 1>(srcBuffer, aSrcStride, uBuffer, vBuffer, aWidth);
  }

  if (aHeight & 1) {
    const int i = aHeight - 1;
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * (i / 2);
    uint8_t* vBuffer = aVBuffer + aVStride * (i / 2);

    RGBAFamilyToY_Row<0, 1, 2>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBAFamilyToUV_Row<0, 1, 2, 1, 1>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

int
RGBA32ToNV12(const uint8_t* aSrcBuffer, int aSrcStride,
            uint8_t* aYBuffer, int aYStride,
            uint8_t* aUVBuffer, int aUVStride,
            int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight - 1; i += 2) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUVBuffer + aUVStride * (i / 2);
    uint8_t* vBuffer = aUVBuffer + aUVStride * (i / 2) + 1;

    RGBAFamilyToY_Row<0, 1, 2>(srcBuffer, yBuffer, aWidth);
    RGBAFamilyToY_Row<0, 1, 2>(srcBuffer + aSrcStride, yBuffer + aYStride, aWidth);
    RGBAFamilyToUV_Row<0, 1, 2, 2, 2>(srcBuffer, aSrcStride, uBuffer, vBuffer, aWidth);
  }

  if (aHeight & 1) {
    const int i = aHeight - 1;
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUVBuffer + aUVStride * (i / 2);
    uint8_t* vBuffer = aUVBuffer + aUVStride * (i / 2) + 1;

    RGBAFamilyToY_Row<0, 1, 2>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBAFamilyToUV_Row<0, 1, 2, 2, 2>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

int
RGBA32ToNV21(const uint8_t* aSrcBuffer, int aSrcStride,
            uint8_t* aYBuffer, int aYStride,
            uint8_t* aVUBuffer, int aVUStride,
            int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight - 1; i += 2) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aVUBuffer + aVUStride * (i / 2) + 1;
    uint8_t* vBuffer = aVUBuffer + aVUStride * (i / 2);

    RGBAFamilyToY_Row<0, 1, 2>(srcBuffer, yBuffer, aWidth);
    RGBAFamilyToY_Row<0, 1, 2>(srcBuffer + aSrcStride, yBuffer + aYStride, aWidth);
    RGBAFamilyToUV_Row<0, 1, 2, 2, 2>(srcBuffer, aSrcStride, uBuffer, vBuffer, aWidth);
  }

  if (aHeight & 1) {
    const int i = aHeight - 1;
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aVUBuffer + aVUStride * (i / 2) + 1;
    uint8_t* vBuffer = aVUBuffer + aVUStride * (i / 2);

    RGBAFamilyToY_Row<0, 1, 2>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBAFamilyToUV_Row<0, 1, 2, 2, 2>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

int
BGRA32ToYUV444P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * i;
    uint8_t* vBuffer = aVBuffer + aVStride * i;

    for (int j = 0; j < aWidth; ++j) {
      yBuffer[0] = RGBToY(srcBuffer[2], srcBuffer[1], srcBuffer[0]);
      uBuffer[0] = RGBToU(srcBuffer[2], srcBuffer[1], srcBuffer[0]);
      vBuffer[0] = RGBToV(srcBuffer[2], srcBuffer[1], srcBuffer[0]);

      yBuffer += 1;
      uBuffer += 1;
      vBuffer += 1;
      srcBuffer += 4;
    }
  }

  return 0;
}

int
BGRA32ToYUV422P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * i;
    uint8_t* vBuffer = aVBuffer + aVStride * i;

    RGBAFamilyToY_Row<2, 1, 0>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBAFamilyToUV_Row<2, 1, 0, 1, 1>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

int
BGRA32ToYUV420P(const uint8_t* aSrcBuffer, int aSrcStride,
               uint8_t* aYBuffer, int aYStride,
               uint8_t* aUBuffer, int aUStride,
               uint8_t* aVBuffer, int aVStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight - 1; i += 2) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * (i / 2);
    uint8_t* vBuffer = aVBuffer + aVStride * (i / 2);

    RGBAFamilyToY_Row<2, 1, 0>(srcBuffer, yBuffer, aWidth);
    RGBAFamilyToY_Row<2, 1, 0>(srcBuffer + aSrcStride, yBuffer + aYStride, aWidth);
    RGBAFamilyToUV_Row<2, 1, 0, 1, 1>(srcBuffer, aSrcStride, uBuffer, vBuffer, aWidth);
  }

  if (aHeight & 1) {
    const int i = aHeight - 1;
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUBuffer + aUStride * (i / 2);
    uint8_t* vBuffer = aVBuffer + aVStride * (i / 2);

    RGBAFamilyToY_Row<2, 1, 0>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBAFamilyToUV_Row<2, 1, 0, 1, 1>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

int
BGRA32ToNV12(const uint8_t* aSrcBuffer, int aSrcStride,
            uint8_t* aYBuffer, int aYStride,
            uint8_t* aUVBuffer, int aUVStride,
            int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight - 1; i += 2) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUVBuffer + aUVStride * (i / 2);
    uint8_t* vBuffer = aUVBuffer + aUVStride * (i / 2) + 1;

    RGBAFamilyToY_Row<2, 1, 0>(srcBuffer, yBuffer, aWidth);
    RGBAFamilyToY_Row<2, 1, 0>(srcBuffer + aSrcStride, yBuffer + aYStride, aWidth);
    RGBAFamilyToUV_Row<2, 1, 0, 2, 2>(srcBuffer, aSrcStride, uBuffer, vBuffer, aWidth);
  }

  if (aHeight & 1) {
    const int i = aHeight - 1;
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aUVBuffer + aUVStride * (i / 2);
    uint8_t* vBuffer = aUVBuffer + aUVStride * (i / 2) + 1;

    RGBAFamilyToY_Row<2, 1, 0>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBAFamilyToUV_Row<2, 1, 0, 2, 2>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

int
BGRA32ToNV21(const uint8_t* aSrcBuffer, int aSrcStride,
            uint8_t* aYBuffer, int aYStride,
            uint8_t* aVUBuffer, int aVUStride,
            int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight - 1; i += 2) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aVUBuffer + aVUStride * (i / 2) + 1;
    uint8_t* vBuffer = aVUBuffer + aVUStride * (i / 2);

    RGBAFamilyToY_Row<2, 1, 0>(srcBuffer, yBuffer, aWidth);
    RGBAFamilyToY_Row<2, 1, 0>(srcBuffer + aSrcStride, yBuffer + aYStride, aWidth);
    RGBAFamilyToUV_Row<2, 1, 0, 2, 2>(srcBuffer, aSrcStride, uBuffer, vBuffer, aWidth);
  }

  if (aHeight & 1) {
    const int i = aHeight - 1;
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* yBuffer = aYBuffer + aYStride * i;
    uint8_t* uBuffer = aVUBuffer + aVUStride * (i / 2) + 1;
    uint8_t* vBuffer = aVUBuffer + aVUStride * (i / 2);

    RGBAFamilyToY_Row<2, 1, 0>(srcBuffer, yBuffer, aWidth);

    // Pass 0 as the aSrcStride so we don't sample next row's RGB information.
    RGBAFamilyToUV_Row<2, 1, 0, 2, 2>(srcBuffer, 0, uBuffer, vBuffer, aWidth);
  }

  return 0;
}

/*
 * RGBA/RGB family -> HSV.
 * Reference:
 * (1) https://en.wikipedia.org/wiki/HSL_and_HSV
 * (2) OpenCV implementation:
 *     http://docs.opencv.org/3.1.0/de/d25/imgproc_color_conversions.html
 */
const float EPSILON = 1e-10f;

template<int aRIndex, int aGIndex, int aBIndex, int aSrcStep>
int
RGBFamilyToHSV(const uint8_t* aSrcBuffer, int aSrcStride,
               float* aDstBuffer, int aDstStride,
               int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    float* dstBuffer = (float*)((uint8_t*)(aDstBuffer) + aDstStride * i);

    for (int j = 0; j < aWidth; ++j) {
      const float r = (float)(srcBuffer[aRIndex]) / 255.0f;
      const float g = (float)(srcBuffer[aGIndex]) / 255.0f;
      const float b = (float)(srcBuffer[aBIndex]) / 255.0f;
      float& h = dstBuffer[0];
      float& s = dstBuffer[1];
      float& v = dstBuffer[2];

      float min = r;
      if (g < min) min = g;
      if (b < min) min = b;

      float max = r;
      if (g > max) max = g;
      if (b > max) max = b;

      const float diff = max - min + EPSILON; // Prevent dividing by zero.

      // Calculate v.
      v = max;

      // Calculate s.
      if (max == 0.0f) {
        s = 0.0f;
      } else {
        s = diff / v;
      }

      // Calculate h.
      if (max == r) {
        h = 60.0f * (g - b) / diff;
      } else if (max == g) {
        h = 60.0f * (b - r) / diff + 120.0f;
      } else if (max == b) {
        h = 60.0f * (r - g) / diff + 240.0f;
      }

      if (h < 0.0f) {
        h += 360.0f;
      }

      // Step one pixel.
      srcBuffer += aSrcStep;
      dstBuffer += 3;
    }
  }

  return 0;
}

static const int sector_data[][3]= {{0,3,1}, {2,0,1}, {1,0,3}, {1,2,0}, {3,1,0}, {0,1,2}};

// If the destination is a RGB24 or BGR24, set the aAIndex to be 0, 1 or 2,
// so that the r, g or b value will be set to 255 first than to the right value.
template<int aRIndex, int aGIndex, int aBIndex, int aAIndex, int aDstStep>
int
HSVToRGBAFamily(const float* aSrcBuffer, int aSrcStride,
                uint8_t* aDstBuffer, int aDstStride,
                int aWidth, int aHeight)
{
  static_assert(aRIndex == 0 || aRIndex == 2, "Wrong R index.");
  static_assert(aGIndex == 1, "Wrong G index.");
  static_assert(aBIndex == 0 || aBIndex == 2, "Wrong B index.");
  static_assert(aAIndex == 0 || aAIndex == 1 || aAIndex == 2 || aAIndex == 3, "Wrong A index.");

  for (int i = 0; i < aHeight; ++i) {
    const float* srcBuffer = (const float*)((const uint8_t*)(aSrcBuffer) + aSrcStride * i);
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    for (int j = 0; j < aWidth; ++j) {
      const float h = srcBuffer[0];
      const float s = srcBuffer[1];
      const float v = srcBuffer[2];

      // Calculate h-prime which should be in range [0, 6). -> h should be in
      // range [0, 360).
      float hPrime = h / 60.0f;
      if (hPrime < 0.0f)
          do hPrime += 6.0f; while (hPrime < 0.0f);
      else if (hPrime >= 6.0f)
          do hPrime -= 6.0f; while (hPrime >= 6.0f);
      const int sector = floor(hPrime);
      const float hMod1 = hPrime - sector;

      float values[4];
      values[0] = v;
      values[1] = v * (1.0f - s);
      values[2] = v * (1.0f - s * hMod1);
      values[3] = v * (1.0f - s * (1.0f - hMod1));

      dstBuffer[aAIndex] = 255;
      dstBuffer[aRIndex] = Clamp(values[sector_data[sector][0]] * 255.0f);
      dstBuffer[aGIndex] = Clamp(values[sector_data[sector][1]] * 255.0f);
      dstBuffer[aBIndex] = Clamp(values[sector_data[sector][2]] * 255.0f);

      // Step one pixel.
      srcBuffer += 3;
      dstBuffer += aDstStep;
    }
  }

  return 0;
}

int
RGBA32ToHSV(const uint8_t* aSrcBuffer, int aSrcStride,
            float* aDstBuffer, int aDstStride,
            int aWidth, int aHeight)
{
  return RGBFamilyToHSV<0, 1, 2, 4>(aSrcBuffer, aSrcStride,
                                    aDstBuffer, aDstStride,
                                    aWidth, aHeight);
}

int
BGRA32ToHSV(const uint8_t* aSrcBuffer, int aSrcStride,
            float* aDstBuffer, int aDstStride,
            int aWidth, int aHeight)
{
  return RGBFamilyToHSV<2, 1, 0, 4>(aSrcBuffer, aSrcStride,
                                    aDstBuffer, aDstStride,
                                    aWidth, aHeight);
}

int
RGB24ToHSV(const uint8_t* aSrcBuffer, int aSrcStride,
           float* aDstBuffer, int aDstStride,
           int aWidth, int aHeight)
{
  return RGBFamilyToHSV<0, 1, 2, 3>(aSrcBuffer, aSrcStride,
                                    aDstBuffer, aDstStride,
                                    aWidth, aHeight);
}

int
BGR24ToHSV(const uint8_t* aSrcBuffer, int aSrcStride,
           float* aDstBuffer, int aDstStride,
           int aWidth, int aHeight)
{
  return RGBFamilyToHSV<2, 1, 0, 3>(aSrcBuffer, aSrcStride,
                                    aDstBuffer, aDstStride,
                                    aWidth, aHeight);
}

int
HSVToRGBA32(const float* aSrcBuffer, int aSrcStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight)
{
  return HSVToRGBAFamily<0, 1, 2, 3, 4>(aSrcBuffer, aSrcStride,
                                        aDstBuffer, aDstStride,
                                        aWidth, aHeight);
}

int
HSVToBGRA32(const float* aSrcBuffer, int aSrcStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight)
{
  return HSVToRGBAFamily<2, 1, 0, 3, 4>(aSrcBuffer, aSrcStride,
                                        aDstBuffer, aDstStride,
                                        aWidth, aHeight);
}

int
HSVToRGB24(const float* aSrcBuffer, int aSrcStride,
           uint8_t* aDstBuffer, int aDstStride,
           int aWidth, int aHeight)
{
  return HSVToRGBAFamily<0, 1, 2, 0, 3>(aSrcBuffer, aSrcStride,
                                        aDstBuffer, aDstStride,
                                        aWidth, aHeight);
}

int
HSVToBGR24(const float* aSrcBuffer, int aSrcStride,
           uint8_t* aDstBuffer, int aDstStride,
           int aWidth, int aHeight)
{
  return HSVToRGBAFamily<2, 1, 0, 0, 3>(aSrcBuffer, aSrcStride,
                                        aDstBuffer, aDstStride,
                                        aWidth, aHeight);
}

/*
 * RGBA/RGB family -> Lab.
 * Reference:
 * (1) https://en.wikipedia.org/wiki/SRGB
 * (2) https://en.wikipedia.org/wiki/Lab_color_space
 * (3) OpenCV implementation:
 *     http://docs.opencv.org/3.1.0/de/d25/imgproc_color_conversions.html
 */
static const float sRGBToXYZ_D65[] = {0.412453f, 0.357580f, 0.180423f,
                                      0.212671f, 0.715160f, 0.072169f,
                                      0.019334f, 0.119193f, 0.950227f};
static const float XYZTosRGB_D65[] = {3.240479f,  -1.53715f,  -0.498535f,
                                      -0.969256f, 1.875991f,  0.041556f,
                                      0.055648f,  -0.204043f, 1.057311f};
static const float whitept_D65[] = {0.950456f, 1.0f, 1.088754f};
static const float _magic = std::pow((6.0 / 29.0), 3.0); // should be around 0.008856.
static const float _1_3 = 1.0f / 3.0f;
static const float _a = std::pow((29.0 / 6.0), 2.0) / 3.0; // should be around 7.787.
static const float _b = 16.0f / 116.0f; // should be around 0.1379.

template<int aRIndex, int aGIndex, int aBIndex, int aSrcStep>
int
RGBFamilyToLab(const uint8_t* aSrcBuffer, int aSrcStride,
               float* aDstBuffer, int aDstStride,
               int aWidth, int aHeight)
{
  static_assert(aRIndex == 0 || aRIndex == 2, "Wrong R index.");
  static_assert(aGIndex == 1, "Wrong G index.");
  static_assert(aBIndex == 0 || aBIndex == 2, "Wrong B index.");

  const float C0 = sRGBToXYZ_D65[0] / whitept_D65[0],
              C1 = sRGBToXYZ_D65[1] / whitept_D65[0],
              C2 = sRGBToXYZ_D65[2] / whitept_D65[0],
              C3 = sRGBToXYZ_D65[3] / whitept_D65[1],
              C4 = sRGBToXYZ_D65[4] / whitept_D65[1],
              C5 = sRGBToXYZ_D65[5] / whitept_D65[1],
              C6 = sRGBToXYZ_D65[6] / whitept_D65[2],
              C7 = sRGBToXYZ_D65[7] / whitept_D65[2],
              C8 = sRGBToXYZ_D65[8] / whitept_D65[2];

  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    float* dstBuffer = (float*)((uint8_t*)(aDstBuffer) + aDstStride * i);

    for (int j = 0; j < aWidth; ++j) {
      float r = (float)(srcBuffer[aRIndex]) / 255.0f;
      float g = (float)(srcBuffer[aGIndex]) / 255.0f;
      float b = (float)(srcBuffer[aBIndex]) / 255.0f;

      // gamma correction of sRGB
      r = r <= 0.04045f ? r / 12.92f : std::pow((r + 0.055) / 1.055, 2.4);
      g = g <= 0.04045f ? g / 12.92f : std::pow((g + 0.055) / 1.055, 2.4);
      b = b <= 0.04045f ? b / 12.92f : std::pow((b + 0.055) / 1.055, 2.4);

      const float X = C0 * r + C1 * g + C2 * b;
      const float Y = C3 * r + C4 * g + C5 * b;
      const float Z = C6 * r + C7 * g + C8 * b;

      const float FX = X > _magic ? std::pow(X, _1_3) : (_a * X + _b);
      const float FY = Y > _magic ? std::pow(Y, _1_3) : (_a * Y + _b);
      const float FZ = Z > _magic ? std::pow(Z, _1_3) : (_a * Z + _b);

      dstBuffer[0] = 116.0f * FY - 16.0f;
      dstBuffer[1] = 500.0f * (FX - FY);
      dstBuffer[2] = 200.0f * (FY - FZ);

      // Step one pixel.
      srcBuffer += aSrcStep;
      dstBuffer += 3;
    }
  }
  return 0;
}

// If the destination is a RGB24 or BGR24, set the aAIndex to be 0, 1 or 2,
// so that the r, g or b value will be set to 255 first than to the right value.
template<int aRIndex, int aGIndex, int aBIndex, int aAIndex, int aDstStep>
int
LabToRGBAFamily(const float* aSrcBuffer, int aSrcStride,
                uint8_t* aDstBuffer, int aDstStride,
                int aWidth, int aHeight)
{
  static_assert(aRIndex == 0 || aRIndex == 2, "Wrong R index.");
  static_assert(aGIndex == 1, "Wrong G index.");
  static_assert(aBIndex == 0 || aBIndex == 2, "Wrong B index.");
  static_assert(aAIndex == 0 || aAIndex == 1 || aAIndex == 2 || aAIndex == 3, "Wrong A index.");

  const float C0 = XYZTosRGB_D65[0] * whitept_D65[0],
              C1 = XYZTosRGB_D65[1] * whitept_D65[1],
              C2 = XYZTosRGB_D65[2] * whitept_D65[2],
              C3 = XYZTosRGB_D65[3] * whitept_D65[0],
              C4 = XYZTosRGB_D65[4] * whitept_D65[1],
              C5 = XYZTosRGB_D65[5] * whitept_D65[2],
              C6 = XYZTosRGB_D65[6] * whitept_D65[0],
              C7 = XYZTosRGB_D65[7] * whitept_D65[1],
              C8 = XYZTosRGB_D65[8] * whitept_D65[2];

  for (int i = 0; i < aHeight; ++i) {
    const float* srcBuffer = (const float*)((const uint8_t*)(aSrcBuffer) + aSrcStride * i);
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    for (int j = 0; j < aWidth; ++j) {
      const float L = srcBuffer[0];
      const float a = srcBuffer[1];
      const float b = srcBuffer[2];

      const float FY = (L + 16.0f) / 116.0f;
      const float FX = (a / 500.0f) + FY;
      const float FZ = FY - (b / 200.0f);

      const float X = FX > 6.0f / 29.0f ? std::pow((double)FX, 3.0) : 3.0 * std::pow((6.0 / 29.0), 2.0) * (FX - (4.0 / 29.0));
      const float Y = FY > 6.0f / 29.0f ? std::pow((double)FY, 3.0) : 3.0 * std::pow((6.0 / 29.0), 2.0) * (FY - (4.0 / 29.0));
      const float Z = FZ > 6.0f / 29.0f ? std::pow((double)FZ, 3.0) : 3.0 * std::pow((6.0 / 29.0), 2.0) * (FZ - (4.0 / 29.0));

      const float r0 = C0 * X + C1 * Y + C2 * Z;
      const float g0 = C3 * X + C4 * Y + C5 * Z;
      const float b0 = C6 * X + C7 * Y + C8 * Z;

      // Apply gamma curve of sRGB to the linear rgb values.
      dstBuffer[aAIndex] = 255;
      dstBuffer[aRIndex] = Clamp((r0 <= 0.0031308f ? r0 * 12.92f : 1.055 * std::pow((double)r0, 1.0 / 2.4) - 0.055) * 255.0);
      dstBuffer[aGIndex] = Clamp((g0 <= 0.0031308f ? g0 * 12.92f : 1.055 * std::pow((double)g0, 1.0 / 2.4) - 0.055) * 255.0);
      dstBuffer[aBIndex] = Clamp((b0 <= 0.0031308f ? b0 * 12.92f : 1.055 * std::pow((double)b0, 1.0 / 2.4) - 0.055) * 255.0);

      // Step one pixel.
      srcBuffer += 3;
      dstBuffer += aDstStep;
    }
  }
  return 0;
}

int
RGBA32ToLab(const uint8_t* aSrcBuffer, int aSrcStride,
            float* aDstBuffer, int aDstStride,
            int aWidth, int aHeight)
{
  return RGBFamilyToLab<0, 1, 2, 4>(aSrcBuffer, aSrcStride,
                                    aDstBuffer, aDstStride,
                                    aWidth, aHeight);
}

int
BGRA32ToLab(const uint8_t* aSrcBuffer, int aSrcStride,
            float* aDstBuffer, int aDstStride,
            int aWidth, int aHeight)
{
  return RGBFamilyToLab<2, 1, 0, 4>(aSrcBuffer, aSrcStride,
                                    aDstBuffer, aDstStride,
                                    aWidth, aHeight);
}

int
RGB24ToLab(const uint8_t* aSrcBuffer, int aSrcStride,
           float* aDstBuffer, int aDstStride,
           int aWidth, int aHeight)
{
  return RGBFamilyToLab<0, 1, 2, 3>(aSrcBuffer, aSrcStride,
                                    aDstBuffer, aDstStride,
                                    aWidth, aHeight);
}

int
BGR24ToLab(const uint8_t* aSrcBuffer, int aSrcStride,
           float* aDstBuffer, int aDstStride,
           int aWidth, int aHeight)
{
  return RGBFamilyToLab<2, 1, 0, 3>(aSrcBuffer, aSrcStride,
                                    aDstBuffer, aDstStride,
                                    aWidth, aHeight);
}

int
LabToRGBA32(const float* aSrcBuffer, int aSrcStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight)
{
  return LabToRGBAFamily<0, 1, 2, 3, 4>(aSrcBuffer, aSrcStride,
                                        aDstBuffer, aDstStride,
                                        aWidth, aHeight);
}

int
LabToBGRA32(const float* aSrcBuffer, int aSrcStride,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight)
{
  return LabToRGBAFamily<2, 1, 0, 3, 4>(aSrcBuffer, aSrcStride,
                                        aDstBuffer, aDstStride,
                                        aWidth, aHeight);
}

int
LabToRGB24(const float* aSrcBuffer, int aSrcStride,
           uint8_t* aDstBuffer, int aDstStride,
           int aWidth, int aHeight)
{
  return LabToRGBAFamily<0, 1, 2, 0, 3>(aSrcBuffer, aSrcStride,
                                        aDstBuffer, aDstStride,
                                        aWidth, aHeight);
}

int
LabToBGR24(const float* aSrcBuffer, int aSrcStride,
           uint8_t* aDstBuffer, int aDstStride,
           int aWidth, int aHeight)
{
  return LabToRGBAFamily<2, 1, 0, 0, 3>(aSrcBuffer, aSrcStride,
                                        aDstBuffer, aDstStride,
                                        aWidth, aHeight);
}

/*
 * RGBA/RGB family -> Gray8.
 * Reference:
 * (1) OpenCV implementation:
 * http://docs.opencv.org/3.1.0/de/d25/imgproc_color_conversions.html
 */
template<int aRIndex, int aGIndex, int aBIndex, int aSrcStep>
int
RGBFamilyToGray8(const uint8_t* aSrcBuffer, int aSrcStride,
                 uint8_t* aDstBuffer, int aDstStride,
                 int aWidth, int aHeight)
{
  static_assert(aRIndex == 0 || aRIndex == 2, "Wrong R index.");
  static_assert(aGIndex == 1, "Wrong G index.");
  static_assert(aBIndex == 0 || aBIndex == 2, "Wrong B index.");

  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* srcBuffer = aSrcBuffer + aSrcStride * i;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    for (int j = 0; j < aWidth; ++j) {
      dstBuffer[j] = 0.299 * srcBuffer[aRIndex] +
                     0.587 * srcBuffer[aGIndex] +
                     0.114 * srcBuffer[aBIndex];
      srcBuffer += aSrcStep;
    }
  }

  return 0;
}

int
RGB24ToGray8(const uint8_t* aSrcBuffer, int aSrcStride,
             uint8_t* aDstBuffer, int aDstStride,
             int aWidth, int aHeight)
{
  return RGBFamilyToGray8<0, 1, 2, 3>(aSrcBuffer, aSrcStride,
                                      aDstBuffer, aDstStride,
                                      aWidth, aHeight);
}

int
BGR24ToGray8(const uint8_t* aSrcBuffer, int aSrcStride,
             uint8_t* aDstBuffer, int aDstStride,
             int aWidth, int aHeight)
{
  return RGBFamilyToGray8<2, 1, 0, 3>(aSrcBuffer, aSrcStride,
                                      aDstBuffer, aDstStride,
                                      aWidth, aHeight);
}

int
RGBA32ToGray8(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight)
{
  return RGBFamilyToGray8<0, 1, 2, 4>(aSrcBuffer, aSrcStride,
                                      aDstBuffer, aDstStride,
                                      aWidth, aHeight);
}

int
BGRA32ToGray8(const uint8_t* aSrcBuffer, int aSrcStride,
              uint8_t* aDstBuffer, int aDstStride,
              int aWidth, int aHeight)
{
  return RGBFamilyToGray8<2, 1, 0, 4>(aSrcBuffer, aSrcStride,
                                      aDstBuffer, aDstStride,
                                      aWidth, aHeight);
}

/*
 * YUV family -> Gray8.
 * Reference:
 * (1) OpenCV implementation:
 * http://docs.opencv.org/3.1.0/de/d25/imgproc_color_conversions.html
 */
int
YUVFamilyToGray8(const uint8_t* aSrcYBuffer, int aSrcYStride,
                 uint8_t* aDstBuffer, int aDstStride,
                 int aWidth, int aHeight)
{
  for (int i = 0; i < aHeight; ++i) {
    const uint8_t* srcYBuffer = aSrcYBuffer + aSrcYStride * i;
    uint8_t* dstBuffer = aDstBuffer + aDstStride * i;

    memcpy(dstBuffer, srcYBuffer, aDstStride);
  }

  return 0;
}

int
YUV444PToGray8(const uint8_t* aYBuffer, int aYStride,
               const uint8_t*, int,
               const uint8_t*, int,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight)
{
  return YUVFamilyToGray8(aYBuffer, aYStride,
                          aDstBuffer, aDstStride,
                          aWidth, aHeight);
}

int
YUV422PToGray8(const uint8_t* aYBuffer, int aYStride,
               const uint8_t*, int,
               const uint8_t*, int,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight)
{
  return YUVFamilyToGray8(aYBuffer, aYStride,
                          aDstBuffer, aDstStride,
                          aWidth, aHeight);
}

int
YUV420PToGray8(const uint8_t* aYBuffer, int aYStride,
               const uint8_t*, int,
               const uint8_t*, int,
               uint8_t* aDstBuffer, int aDstStride,
               int aWidth, int aHeight)
{
  return YUVFamilyToGray8(aYBuffer, aYStride,
                          aDstBuffer, aDstStride,
                          aWidth, aHeight);
}

int
NV12ToGray8(const uint8_t* aYBuffer, int aYStride,
            const uint8_t*, int,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight)
{
  return YUVFamilyToGray8(aYBuffer, aYStride,
                          aDstBuffer, aDstStride,
                          aWidth, aHeight);
}

int
NV21ToGray8(const uint8_t* aYBuffer, int aYStride,
            const uint8_t*, int,
            uint8_t* aDstBuffer, int aDstStride,
            int aWidth, int aHeight)
{
  return YUVFamilyToGray8(aYBuffer, aYStride,
                          aDstBuffer, aDstStride,
                          aWidth, aHeight);
}

} // namespace dom
} // namespace mozilla
