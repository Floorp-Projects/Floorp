/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageDataSerializer.h"
#include "gfx2DGlue.h"                  // for SurfaceFormatToImageFormat
#include "gfxASurface.h"                // for gfxASurface
#include "gfxImageSurface.h"            // for gfxImageSurface
#include "gfxPoint.h"                   // for gfxIntSize
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/2D.h"             // for DataSourceSurface, Factory
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
  uint32_t width;
  uint32_t height;
  SurfaceFormat format;

  static uint32_t GetOffset()
  {
    return GetAlignedStride<16>(sizeof(SurfaceBufferInfo));
  }
};
} // anonymous namespace

static SurfaceBufferInfo*
GetBufferInfo(uint8_t* aBuffer)
{
  return reinterpret_cast<SurfaceBufferInfo*>(aBuffer);
}


void
ImageDataSerializer::InitializeBufferInfo(IntSize aSize,
                                          SurfaceFormat aFormat)
{
  SurfaceBufferInfo* info = GetBufferInfo(mData);
  info->width = aSize.width;
  info->height = aSize.height;
  info->format = aFormat;
}

static inline uint32_t
ComputeStride(SurfaceFormat aFormat, uint32_t aWidth)
{
  return GetAlignedStride<4>(BytesPerPixel(aFormat) * aWidth);
}

uint32_t
ImageDataSerializer::ComputeMinBufferSize(IntSize aSize,
                                          SurfaceFormat aFormat)
{
  uint32_t bufsize = aSize.height * ComputeStride(aFormat, aSize.width);
  return SurfaceBufferInfo::GetOffset()
       + GetAlignedStride<16>(bufsize);
}

bool
ImageDataSerializerBase::IsValid() const
{
  // XXX - We could use some sanity checks here.
  return !!mData;
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
  SurfaceBufferInfo* info = GetBufferInfo(mData);
  return ComputeStride(GetFormat(), info->width);
}

IntSize
ImageDataSerializerBase::GetSize() const
{
  MOZ_ASSERT(IsValid());
  SurfaceBufferInfo* info = GetBufferInfo(mData);
  return IntSize(info->width, info->height);
}

SurfaceFormat
ImageDataSerializerBase::GetFormat() const
{
  MOZ_ASSERT(IsValid());
  return GetBufferInfo(mData)->format;
}

TemporaryRef<gfxImageSurface>
ImageDataSerializerBase::GetAsThebesSurface()
{
  MOZ_ASSERT(IsValid());
  IntSize size = GetSize();
  return new gfxImageSurface(GetData(),
                             gfxIntSize(size.width, size.height),
                             GetStride(),
                             SurfaceFormatToImageFormat(GetFormat()));
}

TemporaryRef<DrawTarget>
ImageDataSerializerBase::GetAsDrawTarget()
{
  MOZ_ASSERT(IsValid());
  return gfxPlatform::GetPlatform()->CreateDrawTargetForData(GetData(),
                                                             GetSize(),
                                                             GetStride(),
                                                             GetFormat());
}

TemporaryRef<gfx::DataSourceSurface>
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
