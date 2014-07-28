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
  mTimeStamp(0ll),
  mDuration(0ll),
  mFrameType(kGMPDeltaFrame),
  mSize(0),
  mCompleteFrame(false),
  mHost(aHost),
  mBufferType(GMP_BufferSingle)
{
  MOZ_ASSERT(aHost);
  aHost->EncodedFrameCreated(this);
}

GMPVideoEncodedFrameImpl::GMPVideoEncodedFrameImpl(const GMPVideoEncodedFrameData& aFrameData,
                                                   GMPVideoHostImpl* aHost)
: mEncodedWidth(aFrameData.mEncodedWidth()),
  mEncodedHeight(aFrameData.mEncodedHeight()),
  mTimeStamp(aFrameData.mTimestamp()),
  mDuration(aFrameData.mDuration()),
  mFrameType(static_cast<GMPVideoFrameType>(aFrameData.mFrameType())),
  mSize(aFrameData.mSize()),
  mCompleteFrame(aFrameData.mCompleteFrame()),
  mHost(aHost),
  mBuffer(aFrameData.mBuffer()),
  mBufferType(aFrameData.mBufferType())
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

const GMPEncryptedBufferMetadata*
GMPVideoEncodedFrameImpl::GetDecryptionData() const
{
  return nullptr;
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
  aFrameData.mTimestamp() = mTimeStamp;
  aFrameData.mDuration() = mDuration;
  aFrameData.mFrameType() = mFrameType;
  aFrameData.mSize() = mSize;
  aFrameData.mCompleteFrame() = mCompleteFrame;
  aFrameData.mBuffer() = mBuffer;
  aFrameData.mBufferType() = mBufferType;

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
    mHost->SharedMemMgr()->MgrDeallocShmem(GMPSharedMem::kGMPEncodedData, mBuffer);
  }
  mBuffer = ipc::Shmem();
}

GMPErr
GMPVideoEncodedFrameImpl::CreateEmptyFrame(uint32_t aSize)
{
  if (aSize == 0) {
    DestroyBuffer();
  } else if (aSize > AllocatedSize()) {
    DestroyBuffer();
    if (!mHost->SharedMemMgr()->MgrAllocShmem(GMPSharedMem::kGMPEncodedData, aSize,
                                              ipc::SharedMemory::TYPE_BASIC, &mBuffer) ||
        !Buffer()) {
      return GMPAllocErr;
    }
  }
  mSize = aSize;

  return GMPNoErr;
}

GMPErr
GMPVideoEncodedFrameImpl::CopyFrame(const GMPVideoEncodedFrame& aFrame)
{
  auto& f = static_cast<const GMPVideoEncodedFrameImpl&>(aFrame);

  if (f.mSize != 0) {
    GMPErr err = CreateEmptyFrame(f.mSize);
    if (err != GMPNoErr) {
      return err;
    }
    memcpy(Buffer(), f.Buffer(), f.mSize);
  }
  mEncodedWidth = f.mEncodedWidth;
  mEncodedHeight = f.mEncodedHeight;
  mTimeStamp = f.mTimeStamp;
  mDuration = f.mDuration;
  mFrameType = f.mFrameType;
  mSize = f.mSize; // already set...
  mCompleteFrame = f.mCompleteFrame;
  mBufferType = f.mBufferType;
  // Don't copy host, that should have been set properly on object creation via host.

  return GMPNoErr;
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
GMPVideoEncodedFrameImpl::SetTimeStamp(uint64_t aTimeStamp)
{
  mTimeStamp = aTimeStamp;
}

uint64_t
GMPVideoEncodedFrameImpl::TimeStamp()
{
  return mTimeStamp;
}

void
GMPVideoEncodedFrameImpl::SetDuration(uint64_t aDuration)
{
  mDuration = aDuration;
}

uint64_t
GMPVideoEncodedFrameImpl::Duration() const
{
  return mDuration;
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
  if (!mHost->SharedMemMgr()->MgrAllocShmem(GMPSharedMem::kGMPEncodedData, aNewSize,
                                            ipc::SharedMemory::TYPE_BASIC, &new_mem) ||
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

GMPBufferType
GMPVideoEncodedFrameImpl::BufferType() const
{
  return mBufferType;
}

void
GMPVideoEncodedFrameImpl::SetBufferType(GMPBufferType aBufferType)
{
  mBufferType = aBufferType;
}

} // namespace gmp
} // namespace mozilla
