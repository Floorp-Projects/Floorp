/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoDecoderParent.h"
#include "mozilla/Logging.h"
#include "mozilla/Unused.h"
#include "nsAutoRef.h"
#include "nsThreadUtils.h"
#include "GMPUtils.h"
#include "GMPVideoEncodedFrameImpl.h"
#include "GMPVideoi420FrameImpl.h"
#include "GMPContentParent.h"
#include "GMPMessageUtils.h"
#include "mozilla/gmp/GMPTypes.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

extern LogModule* GetGMPLog();

#define LOGV(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Verbose, msg)
#define LOGD(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Debug, msg)
#define LOGE(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Error, msg)
#define LOG(level, msg) MOZ_LOG(GetGMPLog(), (level), msg)

namespace gmp {

// States:
// Initial: mIsOpen == false
//    on InitDecode success -> Open
//    on Shutdown -> Dead
// Open: mIsOpen == true
//    on Close -> Dead
//    on ActorDestroy -> Dead
//    on Shutdown -> Dead
// Dead: mIsOpen == false

GMPVideoDecoderParent::GMPVideoDecoderParent(GMPContentParent* aPlugin)
  : GMPSharedMemManager(aPlugin)
  , mIsOpen(false)
  , mShuttingDown(false)
  , mActorDestroyed(false)
  , mIsAwaitingResetComplete(false)
  , mIsAwaitingDrainComplete(false)
  , mPlugin(aPlugin)
  , mCallback(nullptr)
  , mVideoHost(this)
  , mPluginId(aPlugin->GetPluginId())
  , mFrameCount(0)
{
  MOZ_ASSERT(mPlugin);
}

GMPVideoDecoderParent::~GMPVideoDecoderParent()
{
}

GMPVideoHostImpl&
GMPVideoDecoderParent::Host()
{
  return mVideoHost;
}

// Note: may be called via Terminated()
void
GMPVideoDecoderParent::Close()
{
  LOGD(("GMPVideoDecoderParent[%p]::Close()", this));
  MOZ_ASSERT(!mPlugin || mPlugin->GMPThread() == NS_GetCurrentThread());

  // Ensure if we've received a Close while waiting for a ResetComplete
  // or DrainComplete notification, we'll unblock the caller before processing
  // the close. This seems unlikely to happen, but better to be careful.
  UnblockResetAndDrain();

  // Consumer is done with us; we can shut down.  No more callbacks should
  // be made to mCallback.  Note: do this before Shutdown()!
  mCallback = nullptr;
  // Let Shutdown mark us as dead so it knows if we had been alive

  // In case this is the last reference
  RefPtr<GMPVideoDecoderParent> kungfudeathgrip(this);
  Release();
  Shutdown();
}

nsresult
GMPVideoDecoderParent::InitDecode(const GMPVideoCodec& aCodecSettings,
                                  const nsTArray<uint8_t>& aCodecSpecific,
                                  GMPVideoDecoderCallbackProxy* aCallback,
                                  int32_t aCoreCount)
{
  LOGD(("GMPVideoDecoderParent[%p]::InitDecode()", this));

  if (mActorDestroyed) {
    NS_WARNING("Trying to use a destroyed GMP video decoder!");
    return NS_ERROR_FAILURE;
  }
  if (mIsOpen) {
    NS_WARNING("Trying to re-init an in-use GMP video decoder!");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!aCallback) {
    return NS_ERROR_FAILURE;
  }
  mCallback = aCallback;

  if (!SendInitDecode(aCodecSettings, aCodecSpecific, aCoreCount)) {
    return NS_ERROR_FAILURE;
  }
  mIsOpen = true;

  // Async IPC, we don't have access to a return value.
  return NS_OK;
}

nsresult
GMPVideoDecoderParent::Decode(GMPUniquePtr<GMPVideoEncodedFrame> aInputFrame,
                              bool aMissingFrames,
                              const nsTArray<uint8_t>& aCodecSpecificInfo,
                              int64_t aRenderTimeMs)
{
  LOGV(("GMPVideoDecoderParent[%p]::Decode() timestamp=%lld keyframe=%d",
        this, aInputFrame->TimeStamp(),
        aInputFrame->FrameType() == kGMPKeyFrame));

  if (!mIsOpen) {
    LOGE(("GMPVideoDecoderParent[%p]::Decode() ERROR; dead GMPVideoDecoder", this));
    NS_WARNING("Trying to use an dead GMP video decoder");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  GMPUniquePtr<GMPVideoEncodedFrameImpl> inputFrameImpl(
    static_cast<GMPVideoEncodedFrameImpl*>(aInputFrame.release()));

  // Very rough kill-switch if the plugin stops processing.  If it's merely
  // hung and continues, we'll come back to life eventually.
  // 3* is because we're using 3 buffers per frame for i420 data for now.
  if ((NumInUse(GMPSharedMem::kGMPFrameData) > 3*GMPSharedMem::kGMPBufLimit) ||
      (NumInUse(GMPSharedMem::kGMPEncodedData) > GMPSharedMem::kGMPBufLimit)) {
    LOGE(("GMPVideoDecoderParent[%p]::Decode() ERROR; shmem buffer limit hit frame=%d encoded=%d",
          this, NumInUse(GMPSharedMem::kGMPFrameData), NumInUse(GMPSharedMem::kGMPEncodedData)));
    return NS_ERROR_FAILURE;
  }

  GMPVideoEncodedFrameData frameData;
  inputFrameImpl->RelinquishFrameData(frameData);

  if (!SendDecode(frameData,
                  aMissingFrames,
                  aCodecSpecificInfo,
                  aRenderTimeMs)) {
    LOGE(("GMPVideoDecoderParent[%p]::Decode() ERROR; SendDecode() failure.", this));
    return NS_ERROR_FAILURE;
  }
  mFrameCount++;

  // Async IPC, we don't have access to a return value.
  return NS_OK;
}

nsresult
GMPVideoDecoderParent::Reset()
{
  LOGD(("GMPVideoDecoderParent[%p]::Reset()", this));

  if (!mIsOpen) {
    NS_WARNING("Trying to use an dead GMP video decoder");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!SendReset()) {
    return NS_ERROR_FAILURE;
  }

  mIsAwaitingResetComplete = true;

  RefPtr<GMPVideoDecoderParent> self(this);
  nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction([self]() -> void
  {
    LOGD(("GMPVideoDecoderParent[%p]::ResetCompleteTimeout() timed out waiting for ResetComplete", self.get()));
    self->mResetCompleteTimeout = nullptr;
    LogToBrowserConsole(NS_LITERAL_STRING("GMPVideoDecoderParent timed out waiting for ResetComplete()"));
  });
  CancelResetCompleteTimeout();
  mResetCompleteTimeout = SimpleTimer::Create(task, 5000, mPlugin->GMPThread());

  // Async IPC, we don't have access to a return value.
  return NS_OK;
}

void
GMPVideoDecoderParent::CancelResetCompleteTimeout()
{
  if (mResetCompleteTimeout) {
    mResetCompleteTimeout->Cancel();
    mResetCompleteTimeout = nullptr;
  }
}

nsresult
GMPVideoDecoderParent::Drain()
{
  LOGD(("GMPVideoDecoderParent[%p]::Drain() frameCount=%d", this, mFrameCount));

  if (!mIsOpen) {
    NS_WARNING("Trying to use an dead GMP video decoder");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!SendDrain()) {
    return NS_ERROR_FAILURE;
  }

  mIsAwaitingDrainComplete = true;

  // Async IPC, we don't have access to a return value.
  return NS_OK;
}

const nsCString&
GMPVideoDecoderParent::GetDisplayName() const
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use an dead GMP video decoder");
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  return mPlugin->GetDisplayName();
}

// Note: Consider keeping ActorDestroy sync'd up when making changes here.
nsresult
GMPVideoDecoderParent::Shutdown()
{
  LOGD(("GMPVideoDecoderParent[%p]::Shutdown()", this));
  MOZ_ASSERT(!mPlugin || mPlugin->GMPThread() == NS_GetCurrentThread());

  if (mShuttingDown) {
    return NS_OK;
  }
  mShuttingDown = true;

  // Ensure if we've received a shutdown while waiting for a ResetComplete
  // or DrainComplete notification, we'll unblock the caller before processing
  // the shutdown.
  UnblockResetAndDrain();

  // Notify client we're gone!  Won't occur after Close()
  if (mCallback) {
    mCallback->Terminated();
    mCallback = nullptr;
  }

  mIsOpen = false;
  if (!mActorDestroyed) {
    Unused << SendDecodingComplete();
  }

  return NS_OK;
}

// Note: Keep this sync'd up with Shutdown
void
GMPVideoDecoderParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGD(("GMPVideoDecoderParent[%p]::ActorDestroy reason=%d", this, aWhy));

  mIsOpen = false;
  mActorDestroyed = true;

  // Ensure if we've received a destroy while waiting for a ResetComplete
  // or DrainComplete notification, we'll unblock the caller before processing
  // the error.
  UnblockResetAndDrain();

  if (mCallback) {
    // May call Close() (and Shutdown()) immediately or with a delay
    mCallback->Terminated();
    mCallback = nullptr;
  }
  if (mPlugin) {
    // Ignore any return code. It is OK for this to fail without killing the process.
    mPlugin->VideoDecoderDestroyed(this);
    mPlugin = nullptr;
  }
  mVideoHost.ActorDestroyed();
  MaybeDisconnect(aWhy == AbnormalShutdown);
}

bool
GMPVideoDecoderParent::RecvDecoded(const GMPVideoi420FrameData& aDecodedFrame)
{
  --mFrameCount;
  LOGV(("GMPVideoDecoderParent[%p]::RecvDecoded() timestamp=%lld frameCount=%d",
    this, aDecodedFrame.mTimestamp(), mFrameCount));

  if (!mCallback) {
    return false;
  }

  if (!GMPVideoi420FrameImpl::CheckFrameData(aDecodedFrame)) {
    LOGE(("GMPVideoDecoderParent[%p]::RecvDecoded() "
       "timestamp=%lld decoded frame corrupt, ignoring"));
    return false;
  }
  auto f = new GMPVideoi420FrameImpl(aDecodedFrame, &mVideoHost);

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->Decoded(f);

  return true;
}

bool
GMPVideoDecoderParent::RecvReceivedDecodedReferenceFrame(const uint64_t& aPictureId)
{
  if (!mCallback) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->ReceivedDecodedReferenceFrame(aPictureId);

  return true;
}

bool
GMPVideoDecoderParent::RecvReceivedDecodedFrame(const uint64_t& aPictureId)
{
  if (!mCallback) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->ReceivedDecodedFrame(aPictureId);

  return true;
}

bool
GMPVideoDecoderParent::RecvInputDataExhausted()
{
  LOGV(("GMPVideoDecoderParent[%p]::RecvInputDataExhausted()", this));

  if (!mCallback) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->InputDataExhausted();

  return true;
}

bool
GMPVideoDecoderParent::RecvDrainComplete()
{
  LOGD(("GMPVideoDecoderParent[%p]::RecvDrainComplete() frameCount=%d", this, mFrameCount));
  nsAutoString msg;
  msg.AppendLiteral("GMPVideoDecoderParent::RecvDrainComplete() outstanding frames=");
  msg.AppendInt(mFrameCount);
  LogToBrowserConsole(msg);
  if (!mCallback) {
    return false;
  }

  if (!mIsAwaitingDrainComplete) {
    return true;
  }
  mIsAwaitingDrainComplete = false;

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->DrainComplete();

  return true;
}

bool
GMPVideoDecoderParent::RecvResetComplete()
{
  LOGD(("GMPVideoDecoderParent[%p]::RecvResetComplete()", this));

  CancelResetCompleteTimeout();

  if (!mCallback) {
    return false;
  }

  if (!mIsAwaitingResetComplete) {
    return true;
  }
  mIsAwaitingResetComplete = false;
  mFrameCount = 0;

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->ResetComplete();

  return true;
}

bool
GMPVideoDecoderParent::RecvError(const GMPErr& aError)
{
  LOGD(("GMPVideoDecoderParent[%p]::RecvError(error=%d)", this, aError));

  if (!mCallback) {
    return false;
  }

  // Ensure if we've received an error while waiting for a ResetComplete
  // or DrainComplete notification, we'll unblock the caller before processing
  // the error.
  UnblockResetAndDrain();

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->Error(aError);

  return true;
}

bool
GMPVideoDecoderParent::RecvShutdown()
{
  LOGD(("GMPVideoDecoderParent[%p]::RecvShutdown()", this));

  Shutdown();
  return true;
}

bool
GMPVideoDecoderParent::RecvParentShmemForPool(Shmem&& aEncodedBuffer)
{
  if (aEncodedBuffer.IsWritable()) {
    mVideoHost.SharedMemMgr()->MgrDeallocShmem(GMPSharedMem::kGMPEncodedData,
                                               aEncodedBuffer);
  }
  return true;
}

bool
GMPVideoDecoderParent::AnswerNeedShmem(const uint32_t& aFrameBufferSize,
                                       Shmem* aMem)
{
  ipc::Shmem mem;

  if (!mVideoHost.SharedMemMgr()->MgrAllocShmem(GMPSharedMem::kGMPFrameData,
                                                aFrameBufferSize,
                                                ipc::SharedMemory::TYPE_BASIC, &mem))
  {
    LOGE(("%s: Failed to get a shared mem buffer for Child! size %u",
         __FUNCTION__, aFrameBufferSize));
    return false;
  }
  *aMem = mem;
  mem = ipc::Shmem();
  return true;
}

bool
GMPVideoDecoderParent::Recv__delete__()
{
  LOGD(("GMPVideoDecoderParent[%p]::Recv__delete__()", this));

  if (mPlugin) {
    // Ignore any return code. It is OK for this to fail without killing the process.
    mPlugin->VideoDecoderDestroyed(this);
    mPlugin = nullptr;
  }

  return true;
}

void
GMPVideoDecoderParent::UnblockResetAndDrain()
{
  LOGD(("GMPVideoDecoderParent[%p]::UnblockResetAndDrain() "
        "awaitingResetComplete=%d awaitingDrainComplete=%d",
       this, mIsAwaitingResetComplete, mIsAwaitingDrainComplete));

  if (!mCallback) {
    MOZ_ASSERT(!mIsAwaitingResetComplete);
    MOZ_ASSERT(!mIsAwaitingDrainComplete);
    return;
  }
  if (mIsAwaitingResetComplete) {
    mIsAwaitingResetComplete = false;
    mCallback->ResetComplete();
  }
  if (mIsAwaitingDrainComplete) {
    mIsAwaitingDrainComplete = false;
    mCallback->DrainComplete();
  }
  CancelResetCompleteTimeout();
}

} // namespace gmp
} // namespace mozilla
