/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/YCbCrImageDataSerializer.h"
#include <string.h>                     // for memcpy
#include "gfx2DGlue.h"                  // for ToIntSize
#include "mozilla/gfx/2D.h"             // for DataSourceSurface, Factory
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Types.h"
#include "mozilla/mozalloc.h"           // for operator delete
#include "yuv_convert.h"                // for ConvertYCbCrToRGB32, etc

#define MOZ_ALIGN_WORD(x) (((x) + 3) & ~3)

namespace mozilla {
namespace layers {

// The Data is layed out as follows:
//
//  +-----------------+   -++ --+ --+ <-- Beginning of the buffer
//  | YCbCrBufferInfo |     |   |   |
//  +-----------------+   --+   |   |
//  |      data       |         |   | YCbCrBufferInfo->[mY/mCb/mCr]Offset
//  +-----------------+   ------+   |
//  |      data       |             |
//  +-----------------+   ----------+
//  |      data       |
//  +-----------------+
//
// There can be padding between the blocks above to keep word alignment.

// Structure written at the beginning og the data blob containing the image
// (as shown in the figure above). It contains the necessary informations to
// read the image in the blob.
struct YCbCrBufferInfo
{
  uint32_t mYOffset;
  uint32_t mCbOffset;
  uint32_t mCrOffset;
  uint32_t mYStride;
  uint32_t mYWidth;
  uint32_t mYHeight;
  uint32_t mCbCrStride;
  uint32_t mCbCrWidth;
  uint32_t mCbCrHeight;
  StereoMode mStereoMode;
};

static YCbCrBufferInfo* GetYCbCrBufferInfo(uint8_t* aData)
{
  return reinterpret_cast<YCbCrBufferInfo*>(aData);
}

bool YCbCrImageDataDeserializerBase::IsValid()
{
  if (mData == nullptr) {
    return false;
  }
  return true;
}

uint8_t* YCbCrImageDataDeserializerBase::GetYData()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mData);
  return reinterpret_cast<uint8_t*>(info) + info->mYOffset;
}

uint8_t* YCbCrImageDataDeserializerBase::GetCbData()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mData);
  return reinterpret_cast<uint8_t*>(info) + info->mCbOffset;
}

uint8_t* YCbCrImageDataDeserializerBase::GetCrData()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mData);
  return reinterpret_cast<uint8_t*>(info) + info->mCrOffset;
}

uint8_t* YCbCrImageDataDeserializerBase::GetData()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mData);
  return (reinterpret_cast<uint8_t*>(info)) + MOZ_ALIGN_WORD(sizeof(YCbCrBufferInfo));
}

uint32_t YCbCrImageDataDeserializerBase::GetYStride()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mData);
  return info->mYStride;
}

uint32_t YCbCrImageDataDeserializerBase::GetCbCrStride()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mData);
  return info->mCbCrStride;
}

gfx::IntSize YCbCrImageDataDeserializerBase::GetYSize()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mData);
  return gfx::IntSize(info->mYWidth, info->mYHeight);
}

gfx::IntSize YCbCrImageDataDeserializerBase::GetCbCrSize()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mData);
  return gfx::IntSize(info->mCbCrWidth, info->mCbCrHeight);
}

StereoMode YCbCrImageDataDeserializerBase::GetStereoMode()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mData);
  return info->mStereoMode;
}

// Offset in bytes
static size_t ComputeOffset(uint32_t aHeight, uint32_t aStride)
{
  return MOZ_ALIGN_WORD(aHeight * aStride);
}

// Minimum required shmem size in bytes
size_t
YCbCrImageDataSerializer::ComputeMinBufferSize(const gfx::IntSize& aYSize,
                                               const gfx::IntSize& aCbCrSize)
{
  uint32_t yStride = aYSize.width;
  uint32_t CbCrStride = aCbCrSize.width;

  return ComputeOffset(aYSize.height, yStride)
         + 2 * ComputeOffset(aCbCrSize.height, CbCrStride)
         + MOZ_ALIGN_WORD(sizeof(YCbCrBufferInfo));
}

// Offset in bytes
static size_t ComputeOffset(uint32_t aSize)
{
  return MOZ_ALIGN_WORD(aSize);
}

// Minimum required shmem size in bytes
size_t
YCbCrImageDataSerializer::ComputeMinBufferSize(uint32_t aSize)
{
  return ComputeOffset(aSize) + MOZ_ALIGN_WORD(sizeof(YCbCrBufferInfo));
}

void
YCbCrImageDataSerializer::InitializeBufferInfo(uint32_t aYOffset,
                                               uint32_t aCbOffset,
                                               uint32_t aCrOffset,
                                               uint32_t aYStride,
                                               uint32_t aCbCrStride,
                                               const gfx::IntSize& aYSize,
                                               const gfx::IntSize& aCbCrSize,
                                               StereoMode aStereoMode)
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mData);
  uint32_t info_size = MOZ_ALIGN_WORD(sizeof(YCbCrBufferInfo));
  info->mYOffset = info_size + aYOffset;
  info->mCbOffset = info_size + aCbOffset;
  info->mCrOffset = info_size + aCrOffset;
  info->mYStride = aYStride;
  info->mYWidth = aYSize.width;
  info->mYHeight = aYSize.height;
  info->mCbCrStride = aCbCrStride;
  info->mCbCrWidth = aCbCrSize.width;
  info->mCbCrHeight = aCbCrSize.height;
  info->mStereoMode = aStereoMode;
}

void
YCbCrImageDataSerializer::InitializeBufferInfo(uint32_t aYStride,
                                               uint32_t aCbCrStride,
                                               const gfx::IntSize& aYSize,
                                               const gfx::IntSize& aCbCrSize,
                                               StereoMode aStereoMode)
{
  uint32_t yOffset = 0;
  uint32_t cbOffset = yOffset + MOZ_ALIGN_WORD(aYStride * aYSize.height);
  uint32_t crOffset = cbOffset + MOZ_ALIGN_WORD(aCbCrStride * aCbCrSize.height);
  return InitializeBufferInfo(yOffset, cbOffset, crOffset,
      aYStride, aCbCrStride, aYSize, aCbCrSize, aStereoMode);
}

void
YCbCrImageDataSerializer::InitializeBufferInfo(const gfx::IntSize& aYSize,
                                               const gfx::IntSize& aCbCrSize,
                                               StereoMode aStereoMode)
{
  return InitializeBufferInfo(aYSize.width, aCbCrSize.width, aYSize, aCbCrSize, aStereoMode);
}

static void CopyLineWithSkip(const uint8_t* src, uint8_t* dst, uint32_t len, uint32_t skip) {
  for (uint32_t i = 0; i < len; ++i) {
    *dst = *src;
    src += 1 + skip;
    ++dst;
  }
}

bool
YCbCrImageDataSerializer::CopyData(const uint8_t* aYData,
                                   const uint8_t* aCbData, const uint8_t* aCrData,
                                   gfx::IntSize aYSize, uint32_t aYStride,
                                   gfx::IntSize aCbCrSize, uint32_t aCbCrStride,
                                   uint32_t aYSkip, uint32_t aCbCrSkip)
{
  if (!IsValid() || GetYSize() != aYSize || GetCbCrSize() != aCbCrSize) {
    return false;
  }
  for (int i = 0; i < aYSize.height; ++i) {
    if (aYSkip == 0) {
      // fast path
      memcpy(GetYData() + i * GetYStride(),
             aYData + i * aYStride,
             aYSize.width);
    } else {
      // slower path
      CopyLineWithSkip(aYData + i * aYStride,
                       GetYData() + i * GetYStride(),
                       aYSize.width, aYSkip);
    }
  }
  for (int i = 0; i < aCbCrSize.height; ++i) {
    if (aCbCrSkip == 0) {
      // fast path
      memcpy(GetCbData() + i * GetCbCrStride(),
             aCbData + i * aCbCrStride,
             aCbCrSize.width);
      memcpy(GetCrData() + i * GetCbCrStride(),
             aCrData + i * aCbCrStride,
             aCbCrSize.width);
    } else {
      // slower path
      CopyLineWithSkip(aCbData + i * aCbCrStride,
                       GetCbData() + i * GetCbCrStride(),
                       aCbCrSize.width, aCbCrSkip);
      CopyLineWithSkip(aCrData + i * aCbCrStride,
                       GetCrData() + i * GetCbCrStride(),
                       aCbCrSize.width, aCbCrSkip);
    }
  }
  return true;
}

TemporaryRef<gfx::DataSourceSurface>
YCbCrImageDataDeserializer::ToDataSourceSurface()
{
  RefPtr<gfx::DataSourceSurface> result =
    gfx::Factory::CreateDataSourceSurface(GetYSize(), gfx::SurfaceFormat::R8G8B8X8);

  gfx::ConvertYCbCrToRGB32(GetYData(), GetCbData(), GetCrData(),
                           result->GetData(),
                           0, 0, //pic x and y
                           GetYSize().width, GetYSize().height,
                           GetYStride(), GetCbCrStride(),
                           result->Stride(),
                           gfx::YV12);
  result->MarkDirty();
  return result.forget();
}


} // namespace
} // namespace
