/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstring>

#include "2D.h"
#include "DataSurfaceHelpers.h"
#include "Logging.h"
#include "mozilla/MathAlgorithms.h"
#include "Tools.h"

namespace mozilla {
namespace gfx {

void
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

void
CopySurfaceDataToPackedArray(uint8_t* aSrc, uint8_t* aDst, IntSize aSrcSize,
                             int32_t aSrcStride, int32_t aBytesPerPixel)
{
  MOZ_ASSERT(aBytesPerPixel > 0,
             "Negative stride for aDst not currently supported");
  MOZ_ASSERT(BufferSizeFromStrideAndHeight(aSrcStride, aSrcSize.height) > 0,
             "How did we end up with a surface with such a big buffer?");

  int packedStride = aSrcSize.width * aBytesPerPixel;

  if (aSrcStride == packedStride) {
    // aSrc is already packed, so we can copy with a single memcpy.
    memcpy(aDst, aSrc, packedStride * aSrcSize.height);
  } else {
    // memcpy one row at a time.
    for (int row = 0; row < aSrcSize.height; ++row) {
      memcpy(aDst, aSrc, packedStride);
      aSrc += aSrcStride;
      aDst += packedStride;
    }
  }
}

void
CopyBGRXSurfaceDataToPackedBGRArray(uint8_t* aSrc, uint8_t* aDst,
                                    IntSize aSrcSize, int32_t aSrcStride)
{
  int packedStride = aSrcSize.width * 3;

  uint8_t* srcPx = aSrc;
  uint8_t* dstPx = aDst;

  for (int row = 0; row < aSrcSize.height; ++row) {
    for (int col = 0; col < aSrcSize.height; ++col) {
      dstPx[0] = srcPx[0];
      dstPx[1] = srcPx[1];
      dstPx[2] = srcPx[2];
      // srcPx[3] (unused or alpha component) dropped on floor
      srcPx += 4;
      dstPx += 3;
    }
    srcPx = aSrc += aSrcStride;
    dstPx = aDst += packedStride;
  }
}

uint8_t*
SurfaceToPackedBGRA(DataSourceSurface *aSurface)
{
  SurfaceFormat format = aSurface->GetFormat();
  if (format != SurfaceFormat::B8G8R8A8 && format != SurfaceFormat::B8G8R8X8) {
    return nullptr;
  }

  IntSize size = aSurface->GetSize();

  uint8_t* imageBuffer = new (std::nothrow) uint8_t[size.width * size.height * sizeof(uint32_t)];
  if (!imageBuffer) {
    return nullptr;
  }

  DataSourceSurface::MappedSurface map;
  if (!aSurface->Map(DataSourceSurface::MapType::READ, &map)) {
    delete [] imageBuffer;
    return nullptr;
  }

  CopySurfaceDataToPackedArray(map.mData, imageBuffer, size,
                               map.mStride, 4 * sizeof(uint8_t));

  aSurface->Unmap();

  if (format == SurfaceFormat::B8G8R8X8) {
    // Convert BGRX to BGRA by setting a to 255.
    ConvertBGRXToBGRA(reinterpret_cast<uint8_t *>(imageBuffer), size, size.width * sizeof(uint32_t));
  }

  return imageBuffer;
}

uint8_t*
SurfaceToPackedBGR(DataSourceSurface *aSurface)
{
  SurfaceFormat format = aSurface->GetFormat();
  MOZ_ASSERT(format == SurfaceFormat::B8G8R8X8, "Format not supported");

  if (format != SurfaceFormat::B8G8R8X8) {
    // To support B8G8R8A8 we'd need to un-pre-multiply alpha
    return nullptr;
  }

  IntSize size = aSurface->GetSize();

  uint8_t* imageBuffer = new (std::nothrow) uint8_t[size.width * size.height * 3 * sizeof(uint8_t)];
  if (!imageBuffer) {
    return nullptr;
  }

  DataSourceSurface::MappedSurface map;
  if (!aSurface->Map(DataSourceSurface::MapType::READ, &map)) {
    delete [] imageBuffer;
    return nullptr;
  }

  CopyBGRXSurfaceDataToPackedBGRArray(map.mData, imageBuffer, size,
                                      map.mStride);

  aSurface->Unmap();

  return imageBuffer;
}

void
ClearDataSourceSurface(DataSourceSurface *aSurface)
{
  DataSourceSurface::MappedSurface map;
  if (!aSurface->Map(DataSourceSurface::MapType::WRITE, &map)) {
    MOZ_ASSERT(false, "Failed to map DataSourceSurface");
    return;
  }

  // We avoid writing into the gaps between the rows here since we can't be
  // sure that some drivers don't use those bytes.

  uint32_t width = aSurface->GetSize().width;
  uint32_t bytesPerRow = width * BytesPerPixel(aSurface->GetFormat());
  uint8_t* row = map.mData;
  // converting to size_t here because otherwise the temporaries can overflow
  // and we can end up with |end| being a bad address!
  uint8_t* end = row + size_t(map.mStride) * size_t(aSurface->GetSize().height);

  while (row != end) {
    memset(row, 0, bytesPerRow);
    row += map.mStride;
  }

  aSurface->Unmap();
}

size_t
BufferSizeFromStrideAndHeight(int32_t aStride,
                              int32_t aHeight,
                              int32_t aExtraBytes)
{
  if (MOZ_UNLIKELY(aHeight <= 0)) {
    return 0;
  }

  // We limit the length returned to values that can be represented by int32_t
  // because we don't want to allocate buffers any bigger than that. This
  // allows for a buffer size of over 2 GiB which is already rediculously
  // large and will make the process janky. (Note the choice of the signed type
  // is deliberate because we specifically don't want the returned value to
  // overflow if someone stores the buffer length in an int32_t variable.)

  CheckedInt32 requiredBytes =
    CheckedInt32(aStride) * CheckedInt32(aHeight) + CheckedInt32(aExtraBytes);
  if (MOZ_UNLIKELY(!requiredBytes.isValid())) {
    gfxWarning() << "Buffer size too big; returning zero";
    return 0;
  }
  return requiredBytes.value();
}

}
}
