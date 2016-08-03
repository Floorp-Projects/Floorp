/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WidevineVideoFrame.h"

#include "WidevineUtils.h"

using namespace cdm;

namespace mozilla {

WidevineVideoFrame::WidevineVideoFrame()
  : mFormat(kUnknownVideoFormat)
  , mSize(0,0)
  , mBuffer(nullptr)
  , mTimestamp(0)
{
  Log("WidevineVideoFrame::WidevineVideoFrame() this=%p", this);
  memset(mPlaneOffsets, 0, sizeof(mPlaneOffsets));
  memset(mPlaneStrides, 0, sizeof(mPlaneStrides));
}

WidevineVideoFrame::WidevineVideoFrame(WidevineVideoFrame&& aOther)
  : mFormat(aOther.mFormat)
  , mSize(aOther.mSize)
  , mBuffer(aOther.mBuffer)
  , mTimestamp(aOther.mTimestamp)
{
  Log("WidevineVideoFrame::WidevineVideoFrame(WidevineVideoFrame&&) this=%p, other=%p",
      this, &aOther);
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
  Log("WidevineVideoFrame::SetFormat(%d) this=%p", aFormat, this);
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
  Log("WidevineVideoFrame::SetSize(%d,%d) this=%p", aSize.width, aSize.height, this);
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
  Log("WidevineVideoFrame::SetFrameBuffer(%p) this=%p", aFrameBuffer, this);
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
  Log("WidevineVideoFrame::SetPlaneOffset(%d, %d) this=%p", aPlane, aOffset, this);
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
  Log("WidevineVideoFrame::SetStride(%d, %d) this=%p", aPlane, aStride, this);
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
  Log("WidevineVideoFrame::SetTimestamp(%lld) this=%p", timestamp, this);
  mTimestamp = timestamp;
}

int64_t
WidevineVideoFrame::Timestamp() const
{
  return mTimestamp;
}

} // namespace mozilla
