/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoEncodedFrameImpl.h"
#include "GMPVideoHost.h"
#include "mozilla/gmp/GMPTypes.h"
#include "GMPSharedMemManager.h"

namespace mozilla {
namespace gmp {

GMPVideoEncodedFrameImpl::GMPVideoEncodedFrameImpl(GMPVideoHostImpl* aHost)
: mEncodedWidth(0),
  mEncodedHeight(0),
  mTimeStamp(0),
  mCaptureTime_ms(0),
  mFrameType(kGMPDeltaFrame),
  mSize(0),
  mCompleteFrame(false),
  mHost(aHost)
{
  MOZ_ASSERT(aHost);
  aHost->EncodedFrameCreated(this);
}

GMPVideoEncodedFrameImpl::GMPVideoEncodedFrameImpl(const GMPVideoEncodedFrameData& aFrameData,
                                                   GMPVideoHostImpl* aHost)
: mEncodedWidth(aFrameData.mEncodedWidth()),
  mEncodedHeight(aFrameData.mEncodedHeight()),
  mTimeStamp(aFrameData.mTimeStamp()),
  mCaptureTime_ms(aFrameData.mCaptureTime_ms()),
  mFrameType(static_cast<GMPVideoFrameType>(aFrameData.mFrameType())),
  mSize(aFrameData.mSize()),
  mCompleteFrame(aFrameData.mCompleteFrame()),
  mHost(aHost),
  mBuffer(aFrameData.mBuffer())
{
  MOZ_ASSERT(aHost);
  aHost->EncodedFrameCreated(this);
}

GMPVideoEncodedFrameImpl::~GMPVideoEncodedFrameImpl()
{
  DestroyBuffer();
  if (mHost) {
    mHost->EncodedFrameDestroyed(this);
  }
}

GMPVideoFrameFormat
GMPVideoEncodedFrameImpl::GetFrameFormat()
{
  return kGMPEncodedVideoFrame;
}

void
GMPVideoEncodedFrameImpl::DoneWithAPI()
{
  DestroyBuffer();

  // Do this after destroying the buffer because destruction
  // involves deallocation, which requires a host.
  mHost = nullptr;
}

void
GMPVideoEncodedFrameImpl::ActorDestroyed()
{
  // Simply clear out Shmem reference, do not attempt to
  // properly free it. It has already been freed.
  mBuffer = ipc::Shmem();
  // No more host.
  mHost = nullptr;
}

bool
GMPVideoEncodedFrameImpl::RelinquishFrameData(GMPVideoEncodedFrameData& aFrameData)
{
  aFrameData.mEncodedWidth() = mEncodedWidth;
  aFrameData.mEncodedHeight() = mEncodedHeight;
  aFrameData.mTimeStamp() = mTimeStamp;
  aFrameData.mCaptureTime_ms() = mCaptureTime_ms;
  aFrameData.mFrameType() = mFrameType;
  aFrameData.mSize() = mSize;
  aFrameData.mCompleteFrame() = mCompleteFrame;
  aFrameData.mBuffer() = mBuffer;

  // This method is called right before Shmem is sent to another process.
  // We need to effectively zero out our member copy so that we don't
  // try to delete Shmem we don't own later.
  mBuffer = ipc::Shmem();

  return true;
}

void
GMPVideoEncodedFrameImpl::DestroyBuffer()
{
  if (mHost && mBuffer.IsWritable()) {
    mHost->SharedMemMgr()->MgrDeallocShmem(mBuffer);
  }
  mBuffer = ipc::Shmem();
}

GMPVideoErr
GMPVideoEncodedFrameImpl::CreateEmptyFrame(uint32_t aSize)
{
  DestroyBuffer();

  if (aSize != 0) {
    if (!mHost->SharedMemMgr()->MgrAllocShmem(aSize, ipc::SharedMemory::TYPE_BASIC, &mBuffer) ||
        !Buffer()) {
      return GMPVideoAllocErr;
    }
  }

  mSize = aSize;

  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoEncodedFrameImpl::CopyFrame(const GMPVideoEncodedFrame& aFrame)
{
  auto& f = static_cast<const GMPVideoEncodedFrameImpl&>(aFrame);

  if (f.mSize != 0) {
    GMPVideoErr err = CreateEmptyFrame(f.mSize);
    if (err != GMPVideoNoErr) {
      return err;
    }
    memcpy(Buffer(), f.Buffer(), f.mSize);
  }
  mEncodedWidth = f.mEncodedWidth;
  mEncodedHeight = f.mEncodedHeight;
  mTimeStamp = f.mTimeStamp;
  mCaptureTime_ms = f.mCaptureTime_ms;
  mFrameType = f.mFrameType;
  mSize = f.mSize;
  mCompleteFrame = f.mCompleteFrame;
  // Don't copy host, that should have been set properly on object creation via host.

  return GMPVideoNoErr;
}

void
GMPVideoEncodedFrameImpl::SetEncodedWidth(uint32_t aEncodedWidth)
{
  mEncodedWidth = aEncodedWidth;
}

uint32_t
GMPVideoEncodedFrameImpl::EncodedWidth()
{
  return mEncodedWidth;
}

void
GMPVideoEncodedFrameImpl::SetEncodedHeight(uint32_t aEncodedHeight)
{
  mEncodedHeight = aEncodedHeight;
}

uint32_t
GMPVideoEncodedFrameImpl::EncodedHeight()
{
  return mEncodedHeight;
}

void
GMPVideoEncodedFrameImpl::SetTimeStamp(uint32_t aTimeStamp)
{
  mTimeStamp = aTimeStamp;
}

uint32_t
GMPVideoEncodedFrameImpl::TimeStamp()
{
  return mTimeStamp;
}

void
GMPVideoEncodedFrameImpl::SetCaptureTime(int64_t aCaptureTime)
{
  mCaptureTime_ms = aCaptureTime;
}

int64_t
GMPVideoEncodedFrameImpl::CaptureTime()
{
  return mCaptureTime_ms;
}

void
GMPVideoEncodedFrameImpl::SetFrameType(GMPVideoFrameType aFrameType)
{
  mFrameType = aFrameType;
}

GMPVideoFrameType
GMPVideoEncodedFrameImpl::FrameType()
{
  return mFrameType;
}

void
GMPVideoEncodedFrameImpl::SetAllocatedSize(uint32_t aNewSize)
{
  if (aNewSize <= AllocatedSize()) {
    return;
  }

  if (!mHost) {
    return;
  }

  ipc::Shmem new_mem;
  if (!mHost->SharedMemMgr()->MgrAllocShmem(aNewSize, ipc::SharedMemory::TYPE_BASIC, &new_mem) ||
      !new_mem.get<uint8_t>()) {
    return;
  }

  if (mBuffer.IsReadable()) {
    memcpy(new_mem.get<uint8_t>(), Buffer(), mSize);
  }

  DestroyBuffer();

  mBuffer = new_mem;
}

uint32_t
GMPVideoEncodedFrameImpl::AllocatedSize()
{
  if (mBuffer.IsWritable()) {
    return mBuffer.Size<uint8_t>();
  }
  return 0;
}

void
GMPVideoEncodedFrameImpl::SetSize(uint32_t aSize)
{
  mSize = aSize;
}

uint32_t
GMPVideoEncodedFrameImpl::Size()
{
  return mSize;
}

void
GMPVideoEncodedFrameImpl::SetCompleteFrame(bool aCompleteFrame)
{
  mCompleteFrame = aCompleteFrame;
}

bool
GMPVideoEncodedFrameImpl::CompleteFrame()
{
  return mCompleteFrame;
}

const uint8_t*
GMPVideoEncodedFrameImpl::Buffer() const
{
  return mBuffer.get<uint8_t>();
}

uint8_t*
GMPVideoEncodedFrameImpl::Buffer()
{
  return mBuffer.get<uint8_t>();
}

void
GMPVideoEncodedFrameImpl::Destroy()
{
  delete this;
}

} // namespace gmp
} // namespace mozilla
