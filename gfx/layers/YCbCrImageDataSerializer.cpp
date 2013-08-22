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
  uint32_t mYWidth;
  uint32_t mYHeight;
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
  return info->mYWidth;
}

uint32_t YCbCrImageDataDeserializerBase::GetCbCrStride()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mData);
  return info->mCbCrWidth;
}

gfxIntSize YCbCrImageDataDeserializerBase::GetYSize()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mData);
  return gfxIntSize(info->mYWidth, info->mYHeight);
}

gfxIntSize YCbCrImageDataDeserializerBase::GetCbCrSize()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mData);
  return gfxIntSize(info->mCbCrWidth, info->mCbCrHeight);
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

size_t
YCbCrImageDataSerializer::ComputeMinBufferSize(const gfxIntSize& aYSize,
                                               const gfxIntSize& aCbCrSize)
{
  return ComputeMinBufferSize(gfx::IntSize(aYSize.width, aYSize.height),
                              gfx::IntSize(aCbCrSize.width, aCbCrSize.height));
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
YCbCrImageDataSerializer::InitializeBufferInfo(const gfx::IntSize& aYSize,
                                               const gfx::IntSize& aCbCrSize,
                                               StereoMode aStereoMode)
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mData);
  info->mYOffset = MOZ_ALIGN_WORD(sizeof(YCbCrBufferInfo));
  info->mCbOffset = info->mYOffset
                  + MOZ_ALIGN_WORD(aYSize.width * aYSize.height);
  info->mCrOffset = info->mCbOffset
                  + MOZ_ALIGN_WORD(aCbCrSize.width * aCbCrSize.height);

  info->mYWidth = aYSize.width;
  info->mYHeight = aYSize.height;
  info->mCbCrWidth = aCbCrSize.width;
  info->mCbCrHeight = aCbCrSize.height;
  info->mStereoMode = aStereoMode;
}

void
YCbCrImageDataSerializer::InitializeBufferInfo(const gfxIntSize& aYSize,
                                               const gfxIntSize& aCbCrSize,
                                               StereoMode aStereoMode)
{
  InitializeBufferInfo(gfx::IntSize(aYSize.width, aYSize.height),
                       gfx::IntSize(aCbCrSize.width, aCbCrSize.height),
                       aStereoMode);
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
                                   gfxIntSize aYSize, uint32_t aYStride,
                                   gfxIntSize aCbCrSize, uint32_t aCbCrStride,
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
    gfx::Factory::CreateDataSourceSurface(ToIntSize(GetYSize()), gfx::FORMAT_R8G8B8X8);

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
