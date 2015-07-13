/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstring>

#include "2D.h"
#include "DataSurfaceHelpers.h"
#include "Logging.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/PodOperations.h"
#include "Tools.h"

namespace mozilla {
namespace gfx {

uint8_t*
DataAtOffset(DataSourceSurface* aSurface,
             DataSourceSurface::MappedSurface* aMap,
             IntPoint aPoint)
{
  if (!SurfaceContainsPoint(aSurface, aPoint)) {
    MOZ_CRASH("sample position needs to be inside surface!");
  }

  MOZ_ASSERT(Factory::CheckSurfaceSize(aSurface->GetSize()),
             "surface size overflows - this should have been prevented when the surface was created");

  uint8_t* data = aMap->mData + aPoint.y * aMap->mStride +
    aPoint.x * BytesPerPixel(aSurface->GetFormat());

  if (data < aMap->mData) {
    MOZ_CRASH("out-of-range data access");
  }

  return data;
}

// This check is safe against integer overflow.
bool
SurfaceContainsPoint(SourceSurface* aSurface, const IntPoint& aPoint)
{
  IntSize size = aSurface->GetSize();
  return aPoint.x >= 0 && aPoint.x < size.width &&
         aPoint.y >= 0 && aPoint.y < size.height;
}

void
ConvertBGRXToBGRA(uint8_t* aData, const IntSize &aSize, const int32_t aStride)
{
  int height = aSize.height, width = aSize.width * 4;

  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; column += 4) {
#ifdef IS_BIG_ENDIAN
      aData[column] = 0xFF;
#else
      aData[column + 3] = 0xFF;
#endif
    }
    aData += aStride;
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
    for (int col = 0; col < aSrcSize.width; ++col) {
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

/**
 * aSrcRect: Rect relative to the aSrc surface
 * aDestPoint: Point inside aDest surface
 */
void
CopyRect(DataSourceSurface* aSrc, DataSourceSurface* aDest,
         IntRect aSrcRect, IntPoint aDestPoint)
{
  if (aSrcRect.Overflows() ||
      IntRect(aDestPoint, aSrcRect.Size()).Overflows()) {
    MOZ_CRASH("we should never be getting invalid rects at this point");
  }

  MOZ_RELEASE_ASSERT(aSrc->GetFormat() == aDest->GetFormat(),
                     "different surface formats");
  MOZ_RELEASE_ASSERT(IntRect(IntPoint(), aSrc->GetSize()).Contains(aSrcRect),
                     "source rect too big for source surface");
  MOZ_RELEASE_ASSERT(IntRect(IntPoint(), aDest->GetSize()).Contains(IntRect(aDestPoint, aSrcRect.Size())),
                     "dest surface too small");

  if (aSrcRect.IsEmpty()) {
    return;
  }

  DataSourceSurface::ScopedMap srcMap(aSrc, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap destMap(aDest, DataSourceSurface::WRITE);
  if (MOZ2D_WARN_IF(!srcMap.IsMapped() || !destMap.IsMapped())) {
    return;
  }

  uint8_t* sourceData = DataAtOffset(aSrc, srcMap.GetMappedSurface(), aSrcRect.TopLeft());
  uint32_t sourceStride = srcMap.GetStride();
  uint8_t* destData = DataAtOffset(aDest, destMap.GetMappedSurface(), aDestPoint);
  uint32_t destStride = destMap.GetStride();

  if (BytesPerPixel(aSrc->GetFormat()) == 4) {
    for (int32_t y = 0; y < aSrcRect.height; y++) {
      PodCopy((int32_t*)destData, (int32_t*)sourceData, aSrcRect.width);
      sourceData += sourceStride;
      destData += destStride;
    }
  } else if (BytesPerPixel(aSrc->GetFormat()) == 1) {
    for (int32_t y = 0; y < aSrcRect.height; y++) {
      PodCopy(destData, sourceData, aSrcRect.width);
      sourceData += sourceStride;
      destData += destStride;
    }
  }
}

already_AddRefed<DataSourceSurface>
CreateDataSourceSurfaceByCloning(DataSourceSurface* aSource)
{
  RefPtr<DataSourceSurface> copy =
    Factory::CreateDataSourceSurface(aSource->GetSize(), aSource->GetFormat(), true);
  if (copy) {
    CopyRect(aSource, copy, IntRect(IntPoint(), aSource->GetSize()), IntPoint());
  }
  return copy.forget();
}

} // namespace gfx
} // namespace mozilla
