/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "2D.h"
#include "Logging.h"
#include "PathHelpers.h"

#include "DrawTargetCapture.h"

#ifdef BUILD_ARM_NEON
#include "mozilla/arm.h"
#include "LuminanceNEON.h"
#endif

namespace mozilla {
namespace gfx {

/**
 * Byte offsets of channels in a native packed gfxColor or cairo image surface.
 */
#ifdef IS_BIG_ENDIAN
#define GFX_ARGB32_OFFSET_A 0
#define GFX_ARGB32_OFFSET_R 1
#define GFX_ARGB32_OFFSET_G 2
#define GFX_ARGB32_OFFSET_B 3
#else
#define GFX_ARGB32_OFFSET_A 3
#define GFX_ARGB32_OFFSET_R 2
#define GFX_ARGB32_OFFSET_G 1
#define GFX_ARGB32_OFFSET_B 0
#endif

// c = n / 255
// c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4)) * 255 + 0.5
static const uint8_t gsRGBToLinearRGBMap[256] = {
  0,   0,   0,   0,   0,   0,   0,   1,
  1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   2,   2,   2,   2,   2,   2,
  2,   2,   3,   3,   3,   3,   3,   3,
  4,   4,   4,   4,   4,   5,   5,   5,
  5,   6,   6,   6,   6,   7,   7,   7,
  8,   8,   8,   8,   9,   9,   9,  10,
 10,  10,  11,  11,  12,  12,  12,  13,
 13,  13,  14,  14,  15,  15,  16,  16,
 17,  17,  17,  18,  18,  19,  19,  20,
 20,  21,  22,  22,  23,  23,  24,  24,
 25,  25,  26,  27,  27,  28,  29,  29,
 30,  30,  31,  32,  32,  33,  34,  35,
 35,  36,  37,  37,  38,  39,  40,  41,
 41,  42,  43,  44,  45,  45,  46,  47,
 48,  49,  50,  51,  51,  52,  53,  54,
 55,  56,  57,  58,  59,  60,  61,  62,
 63,  64,  65,  66,  67,  68,  69,  70,
 71,  72,  73,  74,  76,  77,  78,  79,
 80,  81,  82,  84,  85,  86,  87,  88,
 90,  91,  92,  93,  95,  96,  97,  99,
100, 101, 103, 104, 105, 107, 108, 109,
111, 112, 114, 115, 116, 118, 119, 121,
122, 124, 125, 127, 128, 130, 131, 133,
134, 136, 138, 139, 141, 142, 144, 146,
147, 149, 151, 152, 154, 156, 157, 159,
161, 163, 164, 166, 168, 170, 171, 173,
175, 177, 179, 181, 183, 184, 186, 188,
190, 192, 194, 196, 198, 200, 202, 204,
206, 208, 210, 212, 214, 216, 218, 220,
222, 224, 226, 229, 231, 233, 235, 237,
239, 242, 244, 246, 248, 250, 253, 255
};

static void
ComputesRGBLuminanceMask(const uint8_t *aSourceData,
                         int32_t aSourceStride,
                         uint8_t *aDestData,
                         int32_t aDestStride,
                         const IntSize &aSize,
                         float aOpacity)
{
#ifdef BUILD_ARM_NEON
  if (mozilla::supports_neon()) {
    ComputesRGBLuminanceMask_NEON(aSourceData, aSourceStride,
                                  aDestData, aDestStride,
                                  aSize, aOpacity);
    return;
  }
#endif

  int32_t redFactor = 55 * aOpacity; // 255 * 0.2125 * opacity
  int32_t greenFactor = 183 * aOpacity; // 255 * 0.7154 * opacity
  int32_t blueFactor = 18 * aOpacity; // 255 * 0.0721
  int32_t sourceOffset = aSourceStride - 4 * aSize.width;
  const uint8_t *sourcePixel = aSourceData;
  int32_t destOffset = aDestStride - aSize.width;
  uint8_t *destPixel = aDestData;

  for (int32_t y = 0; y < aSize.height; y++) {
    for (int32_t x = 0; x < aSize.width; x++) {
      uint8_t a = sourcePixel[GFX_ARGB32_OFFSET_A];

      if (a) {
        *destPixel = (redFactor * sourcePixel[GFX_ARGB32_OFFSET_R] +
                      greenFactor * sourcePixel[GFX_ARGB32_OFFSET_G] +
                      blueFactor * sourcePixel[GFX_ARGB32_OFFSET_B]) >> 8;
      } else {
        *destPixel = 0;
      }
      sourcePixel += 4;
      destPixel++;
    }
    sourcePixel += sourceOffset;
    destPixel += destOffset;
  }
}

static void
ComputeLinearRGBLuminanceMask(const uint8_t *aSourceData,
                              int32_t aSourceStride,
                              uint8_t *aDestData,
                              int32_t aDestStride,
                              const IntSize &aSize,
                              float aOpacity)
{
  int32_t redFactor = 55 * aOpacity; // 255 * 0.2125 * opacity
  int32_t greenFactor = 183 * aOpacity; // 255 * 0.7154 * opacity
  int32_t blueFactor = 18 * aOpacity; // 255 * 0.0721
  int32_t sourceOffset = aSourceStride - 4 * aSize.width;
  const uint8_t *sourcePixel = aSourceData;
  int32_t destOffset = aDestStride - aSize.width;
  uint8_t *destPixel = aDestData;

  for (int32_t y = 0; y < aSize.height; y++) {
    for (int32_t x = 0; x < aSize.width; x++) {
      uint8_t a = sourcePixel[GFX_ARGB32_OFFSET_A];

      // unpremultiply
      if (a) {
        if (a == 255) {
          /* sRGB -> linearRGB -> intensity */
          *destPixel =
            static_cast<uint8_t>
                       ((gsRGBToLinearRGBMap[sourcePixel[GFX_ARGB32_OFFSET_R]] *
                         redFactor +
                         gsRGBToLinearRGBMap[sourcePixel[GFX_ARGB32_OFFSET_G]] *
                         greenFactor +
                         gsRGBToLinearRGBMap[sourcePixel[GFX_ARGB32_OFFSET_B]] *
                         blueFactor) >> 8);
        } else {
          uint8_t tempPixel[4];
          tempPixel[GFX_ARGB32_OFFSET_B] =
            (255 * sourcePixel[GFX_ARGB32_OFFSET_B]) / a;
          tempPixel[GFX_ARGB32_OFFSET_G] =
            (255 * sourcePixel[GFX_ARGB32_OFFSET_G]) / a;
          tempPixel[GFX_ARGB32_OFFSET_R] =
            (255 * sourcePixel[GFX_ARGB32_OFFSET_R]) / a;

          /* sRGB -> linearRGB -> intensity */
          *destPixel =
            static_cast<uint8_t>
                       (((gsRGBToLinearRGBMap[tempPixel[GFX_ARGB32_OFFSET_R]] *
                          redFactor +
                          gsRGBToLinearRGBMap[tempPixel[GFX_ARGB32_OFFSET_G]] *
                          greenFactor +
                          gsRGBToLinearRGBMap[tempPixel[GFX_ARGB32_OFFSET_B]] *
                          blueFactor) >> 8) * (a / 255.0f));
        }
      } else {
        *destPixel = 0;
      }
      sourcePixel += 4;
      destPixel++;
    }
    sourcePixel += sourceOffset;
    destPixel += destOffset;
  }
}

already_AddRefed<DrawTargetCapture>
DrawTarget::CreateCaptureDT(const IntSize& aSize)
{
  RefPtr<DrawTargetCaptureImpl> dt = new DrawTargetCaptureImpl();

  if (!dt->Init(aSize, this)) {
    gfxWarning() << "Failed to initialize Capture DrawTarget!";
    return nullptr;
  }

  return dt.forget();
}

void
DrawTarget::DrawCapturedDT(DrawTargetCapture *aCaptureDT,
                           const Matrix& aTransform)
{
  if (aTransform.HasNonIntegerTranslation()) {
    gfxWarning() << "Non integer translations are not supported for DrawCaptureDT at this time!";
    return;
  }
  static_cast<DrawTargetCaptureImpl*>(aCaptureDT)->ReplayToDrawTarget(this, aTransform);
}

void
DrawTarget::PushDeviceSpaceClipRects(const IntRect* aRects, uint32_t aCount)
{
  Matrix oldTransform = GetTransform();
  SetTransform(Matrix());

  RefPtr<PathBuilder> pathBuilder = CreatePathBuilder();
  for (uint32_t i = 0; i < aCount; i++) {
    AppendRectToPath(pathBuilder, Rect(aRects[i]));
  }
  RefPtr<Path> path = pathBuilder->Finish();
  PushClip(path);

  SetTransform(oldTransform);
}

void
DrawTarget::StrokeGlyphs(ScaledFont* aFont,
                         const GlyphBuffer& aBuffer,
                         const Pattern& aPattern,
                         const StrokeOptions& aStrokeOptions,
                         const DrawOptions& aOptions,
                         const GlyphRenderingOptions* aRenderingOptions)
{
  RefPtr<Path> path = aFont->GetPathForGlyphs(aBuffer, this);
  Stroke(path, aPattern, aStrokeOptions, aOptions);
}

already_AddRefed<SourceSurface>
DrawTarget::IntoLuminanceSource(LuminanceType aMaskType, float aOpacity)
{
  RefPtr<SourceSurface> surface = Snapshot();
  IntSize size = surface->GetSize();

  RefPtr<DataSourceSurface> maskSurface = surface->GetDataSurface();
  DataSourceSurface::MappedSurface map;
  if (!maskSurface->Map(DataSourceSurface::MapType::READ, &map)) {
    return nullptr;
  }

  // Create alpha channel mask for output
  RefPtr<DataSourceSurface> destMaskSurface =
    Factory::CreateDataSourceSurface(size, SurfaceFormat::A8);
  if (!destMaskSurface) {
    return nullptr;
  }
  DataSourceSurface::MappedSurface destMap;
  if (!destMaskSurface->Map(DataSourceSurface::MapType::WRITE, &destMap)) {
    return nullptr;
  }

  switch (aMaskType) {
    case LuminanceType::LUMINANCE:
    {
      ComputesRGBLuminanceMask(map.mData, map.mStride,
                               destMap.mData, destMap.mStride,
                               size, aOpacity);
      break;
    }
    case LuminanceType::LINEARRGB:
    {
      ComputeLinearRGBLuminanceMask(map.mData, map.mStride,
                                    destMap.mData, destMap.mStride,
                                    size, aOpacity);
      break;
    }
  }

  maskSurface->Unmap();
  destMaskSurface->Unmap();

  return destMaskSurface.forget();
}

} // namespace gfx
} // namespace mozilla
