/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "2D.h"

namespace mozilla {
namespace gfx {

static inline void
ConvertBGRXToBGRA(uint8_t* aData, const IntSize &aSize, int32_t aStride)
{
  uint32_t* pixel = reinterpret_cast<uint32_t*>(aData);

  for (int row = 0; row < aSize.height; ++row) {
    for (int column = 0; column < aSize.width; ++column) {
#ifdef IS_BIG_ENDIAN
      pixel[column] |= 0x000000FF;
#else
      pixel[column] |= 0xFF000000;
#endif
    }
    pixel += (aStride/4);
  }
}

/*
 * Convert aSurface to a packed buffer in BGRA format. The pixel data is
 * returned in a buffer allocated with new uint8_t[].
 */
inline uint8_t *
SurfaceToPackedBGRA(SourceSurface *aSurface)
{
  RefPtr<DataSourceSurface> data = aSurface->GetDataSurface();
  if (!data) {
    return nullptr;
  }

  SurfaceFormat format = data->GetFormat();
  if (format != SurfaceFormat::B8G8R8A8 && format != SurfaceFormat::B8G8R8X8) {
    return nullptr;
  }

  IntSize size = data->GetSize();

  uint8_t* imageBuffer = new (std::nothrow) uint8_t[size.width * size.height * sizeof(uint32_t)];
  if (!imageBuffer) {
    return nullptr;
  }

  size_t stride = data->Stride();

  uint32_t* src = reinterpret_cast<uint32_t*>(data->GetData());
  uint32_t* dst = reinterpret_cast<uint32_t*>(imageBuffer);

  if (stride == size.width * sizeof(uint32_t)) {
    // DataSourceSurface is already packed. We can use memcpy.
    memcpy(dst, src, size.width * size.height * sizeof(uint32_t));
  } else {
    for (int row = 0; row < size.height; ++row) {
      for (int column = 0; column < size.width; ++column) {
        *dst++ = src[column];
      }
      src += (stride/4);
    }
  }

  if (format == SurfaceFormat::B8G8R8X8) {
    // Convert BGRX to BGRA by setting a to 255.
    ConvertBGRXToBGRA(reinterpret_cast<uint8_t *>(imageBuffer), size, size.width * sizeof(uint32_t));
  }

  return imageBuffer;
}

}
}
