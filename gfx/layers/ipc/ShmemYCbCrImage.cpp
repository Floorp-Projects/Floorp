/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShmemYCbCrImage.h"

#define MOZ_ALIGN_WORD(x) (((x) + 3) & ~3)

using namespace mozilla::ipc;

namespace mozilla {
namespace layers {

// The Data is layed out as follows:
//
//  +-----------------+   -+   <-- Beginning of the Shmem
//  |                 |    |
//  |      ...        |    | offset
//  |                 |    |
//  +-----------------+   -++ --+ --+
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
};

static YCbCrBufferInfo* GetYCbCrBufferInfo(Shmem& aShmem, size_t aOffset)
{
  return reinterpret_cast<YCbCrBufferInfo*>(aShmem.get<uint8_t>() + aOffset);
}


uint8_t* ShmemYCbCrImage::GetYData()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mShmem, mOffset);
  return reinterpret_cast<uint8_t*>(info) + info->mYOffset;
}

uint8_t* ShmemYCbCrImage::GetCbData()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mShmem, mOffset);
  return reinterpret_cast<uint8_t*>(info) + info->mCbOffset;
}

uint8_t* ShmemYCbCrImage::GetCrData()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mShmem, mOffset);
  return reinterpret_cast<uint8_t*>(info) + info->mCrOffset;
}

uint32_t ShmemYCbCrImage::GetYStride()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mShmem, mOffset);
  return info->mYWidth;
}

uint32_t ShmemYCbCrImage::GetCbCrStride()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mShmem, mOffset);
  return info->mCbCrWidth;
}

gfxIntSize ShmemYCbCrImage::GetYSize()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mShmem, mOffset);
  return gfxIntSize(info->mYWidth, info->mYHeight);
}

gfxIntSize ShmemYCbCrImage::GetCbCrSize()
{
  YCbCrBufferInfo* info = GetYCbCrBufferInfo(mShmem, mOffset);
  return gfxIntSize(info->mCbCrWidth, info->mCbCrHeight);
}


bool ShmemYCbCrImage::Open(Shmem& aShmem, size_t aOffset)
{
    mShmem = aShmem;
    mOffset = aOffset;

    return IsValid();
}

// Offset in bytes
static size_t ComputeOffset(uint32_t aHeight, uint32_t aStride)
{
  return MOZ_ALIGN_WORD(aHeight * aStride);
}

// Minimum required shmem size in bytes
size_t ShmemYCbCrImage::ComputeMinBufferSize(const gfxIntSize& aYSize,
                                              const gfxIntSize& aCbCrSize)
{
  uint32_t yStride = aYSize.width;
  uint32_t CbCrStride = aCbCrSize.width;

  return ComputeOffset(aYSize.height, yStride)
         + 2 * ComputeOffset(aCbCrSize.height, CbCrStride)
         + MOZ_ALIGN_WORD(sizeof(YCbCrBufferInfo));
}

void ShmemYCbCrImage::InitializeBufferInfo(uint8_t* aBuffer,
                                           const gfxIntSize& aYSize,
                                           const gfxIntSize& aCbCrSize)
{
  YCbCrBufferInfo* info = reinterpret_cast<YCbCrBufferInfo*>(aBuffer);
  info->mYOffset = MOZ_ALIGN_WORD(sizeof(YCbCrBufferInfo));
  info->mCbOffset = info->mYOffset
                  + MOZ_ALIGN_WORD(aYSize.width * aYSize.height);
  info->mCrOffset = info->mCbOffset
                  + MOZ_ALIGN_WORD(aCbCrSize.width * aCbCrSize.height);

  info->mYWidth = aYSize.width;
  info->mYHeight = aYSize.height;
  info->mCbCrWidth = aCbCrSize.width;
  info->mCbCrHeight = aCbCrSize.height;
}

bool ShmemYCbCrImage::IsValid()
{
  if (mShmem == Shmem()) {
    return false;
  }
  size_t bufferInfoSize = MOZ_ALIGN_WORD(sizeof(YCbCrBufferInfo));
  if (mShmem.Size<uint8_t>() < bufferInfoSize ||
      GetYCbCrBufferInfo(mShmem, mOffset)->mYOffset != bufferInfoSize ||
      mShmem.Size<uint8_t>() < mOffset + ComputeMinBufferSize(GetYSize(),GetCbCrSize())) {
    return false;
  }
  return true;
}

static void CopyLineWithSkip(const uint8_t* src, uint8_t* dst, uint32_t len, uint32_t skip) {
  for (uint32_t i = 0; i < len; ++i) {
    *dst = *src;
    src += 1 + skip;
    ++dst;
  }
}

bool ShmemYCbCrImage::CopyData(const uint8_t* aYData,
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


} // namespace
} // namespace
