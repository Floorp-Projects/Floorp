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
#include "Swizzle.h"
#include "Tools.h"

namespace mozilla {
namespace gfx {

int32_t
StrideForFormatAndWidth(SurfaceFormat aFormat, int32_t aWidth)
{
  MOZ_ASSERT(aFormat <= SurfaceFormat::UNKNOWN);
  MOZ_ASSERT(aWidth > 0);

  // There's nothing special about this alignment, other than that it's what
  // cairo_format_stride_for_width uses.
  static const int32_t alignment = sizeof(int32_t);

  const int32_t bpp = BytesPerPixel(aFormat);

  if (aWidth >= (INT32_MAX - alignment) / bpp) {
    return -1; // too big
  }

  return (bpp * aWidth + alignment-1) & ~(alignment-1);
}

already_AddRefed<DataSourceSurface>
CreateDataSourceSurfaceFromData(const IntSize& aSize,
                                SurfaceFormat aFormat,
                                const uint8_t* aData,
                                int32_t aDataStride)
{
  RefPtr<DataSourceSurface> srcSurface =
      Factory::CreateWrappingDataSourceSurface(const_cast<uint8_t*>(aData),
                                               aDataStride,
                                               aSize,
                                               aFormat);
  RefPtr<DataSourceSurface> destSurface =
      Factory::CreateDataSourceSurface(aSize, aFormat, false);

  if (!srcSurface || !destSurface) {
    return nullptr;
  }

  if (CopyRect(srcSurface,
               destSurface,
               IntRect(IntPoint(), srcSurface->GetSize()),
               IntPoint())) {
    return destSurface.forget();
  }

  return nullptr;
}

already_AddRefed<DataSourceSurface>
CreateDataSourceSurfaceWithStrideFromData(const IntSize &aSize,
                                          SurfaceFormat aFormat,
                                          int32_t aStride,
                                          const uint8_t* aData,
                                          int32_t aDataStride)
{
  RefPtr<DataSourceSurface> srcSurface =
      Factory::CreateWrappingDataSourceSurface(const_cast<uint8_t*>(aData),
                                               aDataStride,
                                               aSize,
                                               aFormat);
  RefPtr<DataSourceSurface> destSurface =
      Factory::CreateDataSourceSurfaceWithStride(aSize, aFormat, aStride, false);

  if (!srcSurface || !destSurface) {
    return nullptr;
  }

  if (CopyRect(srcSurface,
               destSurface,
               IntRect(IntPoint(), srcSurface->GetSize()),
               IntPoint())) {
    return destSurface.forget();
  }

  return nullptr;
}

uint8_t*
DataAtOffset(DataSourceSurface* aSurface,
             const DataSourceSurface::MappedSurface* aMap,
             IntPoint aPoint)
{
  if (!SurfaceContainsPoint(aSurface, aPoint)) {
    MOZ_CRASH("GFX: sample position needs to be inside surface!");
  }

  MOZ_ASSERT(Factory::CheckSurfaceSize(aSurface->GetSize()),
             "surface size overflows - this should have been prevented when the surface was created");

  uint8_t* data = aMap->mData + aPoint.y * aMap->mStride +
    aPoint.x * BytesPerPixel(aSurface->GetFormat());

  if (data < aMap->mData) {
    MOZ_CRASH("GFX: out-of-range data access");
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

UniquePtr<uint8_t[]>
SurfaceToPackedBGRA(DataSourceSurface *aSurface)
{
  SurfaceFormat format = aSurface->GetFormat();
  if (format != SurfaceFormat::B8G8R8A8 && format != SurfaceFormat::B8G8R8X8) {
    return nullptr;
  }

  IntSize size = aSurface->GetSize();

  UniquePtr<uint8_t[]> imageBuffer(
    new (std::nothrow) uint8_t[size.width * size.height * sizeof(uint32_t)]);
  if (!imageBuffer) {
    return nullptr;
  }

  DataSourceSurface::MappedSurface map;
  if (!aSurface->Map(DataSourceSurface::MapType::READ, &map)) {
    return nullptr;
  }

  CopySurfaceDataToPackedArray(map.mData, imageBuffer.get(), size,
                               map.mStride, 4 * sizeof(uint8_t));

  aSurface->Unmap();

  if (format == SurfaceFormat::B8G8R8X8) {
    // Convert BGRX to BGRA by setting a to 255.
    SwizzleData(imageBuffer.get(), size.width * sizeof(uint32_t), SurfaceFormat::X8R8G8B8_UINT32,
                imageBuffer.get(), size.width * sizeof(uint32_t), SurfaceFormat::A8R8G8B8_UINT32,
                size);
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

  SwizzleData(map.mData, map.mStride, SurfaceFormat::B8G8R8X8,
              imageBuffer, size.width * 3, SurfaceFormat::B8G8R8,
              size);

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
  if (MOZ_UNLIKELY(aHeight <= 0) || MOZ_UNLIKELY(aStride <= 0)) {
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
    gfxWarning() << "Buffer size too big; returning zero " << aStride << ", " << aHeight << ", " << aExtraBytes;
    return 0;
  }
  return requiredBytes.value();
}

size_t
BufferSizeFromDimensions(int32_t aWidth,
                         int32_t aHeight,
                         int32_t aDepth,
                         int32_t aExtraBytes)
{
  if (MOZ_UNLIKELY(aHeight <= 0) ||
      MOZ_UNLIKELY(aWidth <= 0) ||
      MOZ_UNLIKELY(aDepth <= 0)) {
    return 0;
  }

  // Similar to BufferSizeFromStrideAndHeight, but with an extra parameter.

  CheckedInt32 requiredBytes = CheckedInt32(aWidth) * CheckedInt32(aHeight) * CheckedInt32(aDepth) + CheckedInt32(aExtraBytes);
  if (MOZ_UNLIKELY(!requiredBytes.isValid())) {
    gfxWarning() << "Buffer size too big; returning zero " << aWidth << ", " << aHeight << ", " << aDepth << ", " << aExtraBytes;
    return 0;
  }
  return requiredBytes.value();
}

/**
 * aSrcRect: Rect relative to the aSrc surface
 * aDestPoint: Point inside aDest surface
 */
bool
CopyRect(DataSourceSurface* aSrc, DataSourceSurface* aDest,
         IntRect aSrcRect, IntPoint aDestPoint)
{
  if (aSrcRect.Overflows() ||
      IntRect(aDestPoint, aSrcRect.Size()).Overflows()) {
    MOZ_CRASH("GFX: we should never be getting invalid rects at this point");
  }

  MOZ_RELEASE_ASSERT(aSrc->GetFormat() == aDest->GetFormat(),
                     "GFX: different surface formats");
  MOZ_RELEASE_ASSERT(IntRect(IntPoint(), aSrc->GetSize()).Contains(aSrcRect),
                     "GFX: source rect too big for source surface");
  MOZ_RELEASE_ASSERT(IntRect(IntPoint(), aDest->GetSize()).Contains(IntRect(aDestPoint, aSrcRect.Size())),
                     "GFX: dest surface too small");

  if (aSrcRect.IsEmpty()) {
    return false;
  }

  DataSourceSurface::ScopedMap srcMap(aSrc, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap destMap(aDest, DataSourceSurface::WRITE);
  if (MOZ2D_WARN_IF(!srcMap.IsMapped() || !destMap.IsMapped())) {
    return false;
  }

  uint8_t* sourceData = DataAtOffset(aSrc, srcMap.GetMappedSurface(), aSrcRect.TopLeft());
  uint8_t* destData = DataAtOffset(aDest, destMap.GetMappedSurface(), aDestPoint);

  SwizzleData(sourceData, srcMap.GetStride(), aSrc->GetFormat(),
              destData, destMap.GetStride(), aDest->GetFormat(),
              aSrcRect.Size());

  return true;
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
