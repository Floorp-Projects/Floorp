/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageDataSerializer.h"
#include "gfx2DGlue.h"                  // for SurfaceFormatToImageFormat
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/2D.h"             // for DataSourceSurface, Factory
#include "mozilla/gfx/Logging.h"        // for gfxDebug
#include "mozilla/gfx/Tools.h"          // for GetAlignedStride, etc
#include "mozilla/mozalloc.h"           // for operator delete, etc

namespace mozilla {
namespace layers {

using namespace gfx;

// The Data is layed out as follows:
//
//  +-------------------+   -++ --+   <-- ImageDataSerializerBase::mData pointer
//  | SurfaceBufferInfo |     |   |
//  +-------------------+   --+   | offset
//  |        ...        |         |
//  +-------------------+   ------+
//  |                   |
//  |       data        |
//  |                   |
//  +-------------------+

// Structure written at the beginning of the data blob containing the image
// (as shown in the figure above). It contains the necessary informations to
// read the image in the blob.
namespace {
struct SurfaceBufferInfo
{
  int32_t width;
  int32_t height;
  SurfaceFormat format;

  static int32_t GetOffset()
  {
    return GetAlignedStride<16>(sizeof(SurfaceBufferInfo));
  }
};
} // namespace

static SurfaceBufferInfo*
GetBufferInfo(uint8_t* aData, size_t aDataSize)
{
  return aDataSize >= sizeof(SurfaceBufferInfo)
         ? reinterpret_cast<SurfaceBufferInfo*>(aData)
         : nullptr;
}

void
ImageDataSerializer::InitializeBufferInfo(IntSize aSize,
                                          SurfaceFormat aFormat)
{
  SurfaceBufferInfo* info = GetBufferInfo(mData, mDataSize);
  MOZ_ASSERT(info); // OK to assert here, this method is client-side-only
  info->width = aSize.width;
  info->height = aSize.height;
  info->format = aFormat;
  Validate();
}

static inline int32_t
ComputeStride(SurfaceFormat aFormat, int32_t aWidth)
{
  CheckedInt<int32_t> size = BytesPerPixel(aFormat);
  size *= aWidth;
  if (!size.isValid() || size.value() <= 0) {
    gfxDebug() << "ComputeStride overflow " << aWidth;
    return 0;
  }

  return GetAlignedStride<4>(size.value());
}

uint32_t
ImageDataSerializerBase::ComputeMinBufferSize(IntSize aSize,
                                              SurfaceFormat aFormat)
{
  MOZ_ASSERT(aSize.height >= 0 && aSize.width >= 0);
  if (aSize.height <= 0 || aSize.width <= 0) {
    gfxDebug() << "Non-positive image buffer size request " << aSize.width << "x" << aSize.height;
    return 0;
  }

  CheckedInt<int32_t> bufsize = ComputeStride(aFormat, aSize.width);
  bufsize *= aSize.height;

  if (!bufsize.isValid() || bufsize.value() <= 0) {
    gfxDebug() << "Buffer size overflow " << aSize.width << "x" << aSize.height;
    return 0;
  }

  return SurfaceBufferInfo::GetOffset()
       + GetAlignedStride<16>(bufsize.value());
}

void
ImageDataSerializerBase::Validate()
{
  mIsValid = false;
  if (!mData) {
    return;
  }
  SurfaceBufferInfo* info = GetBufferInfo(mData, mDataSize);
  if (!info) {
    return;
  }
  size_t requiredSize =
           ComputeMinBufferSize(IntSize(info->width, info->height), info->format);
  mIsValid = requiredSize <= mDataSize;
}

uint8_t*
ImageDataSerializerBase::GetData()
{
  MOZ_ASSERT(IsValid());
  return mData + SurfaceBufferInfo::GetOffset();
}

uint32_t
ImageDataSerializerBase::GetStride() const
{
  MOZ_ASSERT(IsValid());
  SurfaceBufferInfo* info = GetBufferInfo(mData, mDataSize);
  return ComputeStride(GetFormat(), info->width);
}

IntSize
ImageDataSerializerBase::GetSize() const
{
  MOZ_ASSERT(IsValid());
  SurfaceBufferInfo* info = GetBufferInfo(mData, mDataSize);
  return IntSize(info->width, info->height);
}

SurfaceFormat
ImageDataSerializerBase::GetFormat() const
{
  MOZ_ASSERT(IsValid());
  return GetBufferInfo(mData, mDataSize)->format;
}

already_AddRefed<DrawTarget>
ImageDataSerializerBase::GetAsDrawTarget(gfx::BackendType aBackend)
{
  MOZ_ASSERT(IsValid());
  RefPtr<DrawTarget> dt = gfx::Factory::CreateDrawTargetForData(aBackend,
                                               GetData(), GetSize(),
                                               GetStride(), GetFormat());
  if (!dt) {
    gfxCriticalNote << "Failed GetAsDrawTarget " << IsValid() << ", " << hexa(size_t(mData)) << " + " << SurfaceBufferInfo::GetOffset() << ", " << GetSize() << ", " << GetStride() << ", " << (int)GetFormat();
  }
  return dt.forget();
}

already_AddRefed<gfx::DataSourceSurface>
ImageDataSerializerBase::GetAsSurface()
{
  MOZ_ASSERT(IsValid());
  return Factory::CreateWrappingDataSourceSurface(GetData(),
                                                  GetStride(),
                                                  GetSize(),
                                                  GetFormat());
}

} // namespace layers
} // namespace mozilla
