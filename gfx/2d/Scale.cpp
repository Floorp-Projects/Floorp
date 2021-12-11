/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Scale.h"

#include "HelpersSkia.h"
#include "skia/src/core/SkBitmapScaler.h"

namespace mozilla {
namespace gfx {

bool Scale(uint8_t* srcData, int32_t srcWidth, int32_t srcHeight,
           int32_t srcStride, uint8_t* dstData, int32_t dstWidth,
           int32_t dstHeight, int32_t dstStride, SurfaceFormat format) {
  SkPixmap srcPixmap(MakeSkiaImageInfo(IntSize(srcWidth, srcHeight), format),
                     srcData, srcStride);

  // Rescaler is compatible with N32 only. Convert to N32 if needed.
  SkBitmap tmpBitmap;
  if (srcPixmap.colorType() != kN32_SkColorType) {
    if (!tmpBitmap.tryAllocPixels(
            SkImageInfo::MakeN32Premul(srcWidth, srcHeight)) ||
        !tmpBitmap.writePixels(srcPixmap) ||
        !tmpBitmap.peekPixels(&srcPixmap)) {
      return false;
    }
  }

  SkPixmap dstPixmap(SkImageInfo::MakeN32Premul(dstWidth, dstHeight), dstData,
                     dstStride);
  return SkBitmapScaler::Resize(dstPixmap, srcPixmap,
                                SkBitmapScaler::RESIZE_LANCZOS3);
}

}  // namespace gfx
}  // namespace mozilla
