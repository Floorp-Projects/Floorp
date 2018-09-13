/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidevineVideoFrame.h"
#include "GMPLog.h"
#include "WidevineUtils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/IntegerPrintfMacros.h"

using namespace cdm;

namespace mozilla {

WidevineVideoFrame::WidevineVideoFrame()
  : mFormat(kUnknownVideoFormat)
  , mSize{ 0, 0 }
  , mBuffer(nullptr)
  , mTimestamp(0)
{
  MOZ_ASSERT(mSize.height == 0 && mSize.width == 0, "Size should be zeroed");
  GMP_LOG("WidevineVideoFrame::WidevineVideoFrame() this=%p", this);
  memset(mPlaneOffsets, 0, sizeof(mPlaneOffsets));
  memset(mPlaneStrides, 0, sizeof(mPlaneStrides));
}

WidevineVideoFrame::WidevineVideoFrame(WidevineVideoFrame&& aOther)
  : mFormat(aOther.mFormat)
  , mSize(aOther.mSize)
  , mBuffer(aOther.mBuffer)
  , mTimestamp(aOther.mTimestamp)
{
  GMP_LOG("WidevineVideoFrame::WidevineVideoFrame(WidevineVideoFrame&&) "
          "this=%p, other=%p",
          this,
          &aOther);
  memcpy(mPlaneOffsets, aOther.mPlaneOffsets, sizeof(mPlaneOffsets));
  memcpy(mPlaneStrides, aOther.mPlaneStrides, sizeof(mPlaneStrides));
  aOther.mBuffer = nullptr;
}

WidevineVideoFrame::~WidevineVideoFrame()
{
  if (mBuffer) {
    mBuffer->Destroy();
    mBuffer = nullptr;
  }
}

void
WidevineVideoFrame::SetFormat(cdm::VideoFormat aFormat)
{
  GMP_LOG("WidevineVideoFrame::SetFormat(%d) this=%p", aFormat, this);
  mFormat = aFormat;
}

cdm::VideoFormat
WidevineVideoFrame::Format() const
{
  return mFormat;
}

void
WidevineVideoFrame::SetSize(cdm::Size aSize)
{
  GMP_LOG("WidevineVideoFrame::SetSize(%d,%d) this=%p",
          aSize.width,
          aSize.height,
          this);
  mSize.width = aSize.width;
  mSize.height = aSize.height;
}

cdm::Size
WidevineVideoFrame::Size() const
{
  return mSize;
}

void
WidevineVideoFrame::SetFrameBuffer(cdm::Buffer* aFrameBuffer)
{
  GMP_LOG("WidevineVideoFrame::SetFrameBuffer(%p) this=%p", aFrameBuffer, this);
  MOZ_ASSERT(!mBuffer);
  mBuffer = aFrameBuffer;
}

cdm::Buffer*
WidevineVideoFrame::FrameBuffer()
{
  return mBuffer;
}

void
WidevineVideoFrame::SetPlaneOffset(cdm::VideoFrame::VideoPlane aPlane, uint32_t aOffset)
{
  GMP_LOG("WidevineVideoFrame::SetPlaneOffset(%d, %" PRIu32 ") this=%p",
          aPlane,
          aOffset,
          this);
  mPlaneOffsets[aPlane] = aOffset;
}

uint32_t
WidevineVideoFrame::PlaneOffset(cdm::VideoFrame::VideoPlane aPlane)
{
  return mPlaneOffsets[aPlane];
}

void
WidevineVideoFrame::SetStride(cdm::VideoFrame::VideoPlane aPlane, uint32_t aStride)
{
  GMP_LOG(
    "WidevineVideoFrame::SetStride(%d, %" PRIu32 ") this=%p", aPlane, aStride, this);
  mPlaneStrides[aPlane] = aStride;
}

uint32_t
WidevineVideoFrame::Stride(cdm::VideoFrame::VideoPlane aPlane)
{
  return mPlaneStrides[aPlane];
}

void
WidevineVideoFrame::SetTimestamp(int64_t timestamp)
{
  GMP_LOG(
    "WidevineVideoFrame::SetTimestamp(%" PRId64 ") this=%p", timestamp, this);
  mTimestamp = timestamp;
}

int64_t
WidevineVideoFrame::Timestamp() const
{
  return mTimestamp;
}

bool
WidevineVideoFrame::InitToBlack(int32_t aWidth,
                                int32_t aHeight,
                                int64_t aTimeStamp)
{
  MOZ_ASSERT(aWidth >= 0 && aHeight >= 0,
             "Frame dimensions should be positive");
  CheckedInt<size_t> ySizeChk = aWidth;
  ySizeChk *= aHeight;
  // If w*h didn't overflow, half of them won't.
  const size_t uSize = ((aWidth + 1) / 2) * ((aHeight + 1) / 2);
  CheckedInt<size_t> yuSizeChk = ySizeChk + uSize;
  if (!yuSizeChk.isValid()) {
    return false;
  }
  WidevineBuffer* buffer = new WidevineBuffer(yuSizeChk.value());
  const size_t& ySize = ySizeChk.value();
  // Black in YCbCr is (0,128,128).
  memset(buffer->Data(), 0, ySize);
  memset(buffer->Data() + ySize, 128, uSize);
  if (mBuffer) {
    mBuffer->Destroy();
    mBuffer = nullptr;
  }
  SetFormat(VideoFormat::kI420);
  SetSize(cdm::Size{ aWidth, aHeight });
  SetFrameBuffer(buffer);
  SetPlaneOffset(VideoFrame::kYPlane, 0);
  SetStride(VideoFrame::kYPlane, aWidth);
  // Note: U and V planes are stored at the same place in order to
  // save memory since their contents are the same.
  SetPlaneOffset(VideoFrame::kUPlane, ySize);
  SetStride(VideoFrame::kUPlane, (aWidth + 1) / 2);
  SetPlaneOffset(VideoFrame::kVPlane, ySize);
  SetStride(VideoFrame::kVPlane, (aWidth + 1) / 2);
  SetTimestamp(aTimeStamp);
  return true;
}

} // namespace mozilla
