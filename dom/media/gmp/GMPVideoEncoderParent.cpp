/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoEncoderParent.h"
#include "mozilla/Logging.h"
#include "GMPVideoi420FrameImpl.h"
#include "GMPVideoEncodedFrameImpl.h"
#include "mozilla/unused.h"
#include "GMPMessageUtils.h"
#include "nsAutoRef.h"
#include "GMPContentParent.h"
#include "mozilla/gmp/GMPTypes.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "runnable_utils.h"
#include "GMPUtils.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

extern LogModule* GetGMPLog();

#define LOGD(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Debug, msg)
#define LOG(level, msg) MOZ_LOG(GetGMPLog(), (level), msg)

#ifdef __CLASS__
#undef __CLASS__
#endif
#define __CLASS__ "GMPVideoEncoderParent"

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

GMPVideoEncoderParent::GMPVideoEncoderParent(GMPContentParent *aPlugin)
: GMPSharedMemManager(aPlugin),
  mIsOpen(false),
  mShuttingDown(false),
  mActorDestroyed(false),
  mPlugin(aPlugin),
  mCallback(nullptr),
  mVideoHost(this),
  mPluginId(aPlugin->GetPluginId())
{
  MOZ_ASSERT(mPlugin);

  nsresult rv = NS_NewNamedThread("GMPEncoded", getter_AddRefs(mEncodedThread));
  if (NS_FAILED(rv)) {
    MOZ_CRASH();
  }
}

GMPVideoEncoderParent::~GMPVideoEncoderParent()
{
  if (mEncodedThread) {
    mEncodedThread->Shutdown();
  }
}

GMPVideoHostImpl&
GMPVideoEncoderParent::Host()
{
  return mVideoHost;
}

// Note: may be called via Terminated()
void
GMPVideoEncoderParent::Close()
{
  LOGD(("%s::%s: %p", __CLASS__, __FUNCTION__, this));
  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());
  // Consumer is done with us; we can shut down.  No more callbacks should
  // be made to mCallback.  Note: do this before Shutdown()!
  mCallback = nullptr;
  // Let Shutdown mark us as dead so it knows if we had been alive

  // In case this is the last reference
  RefPtr<GMPVideoEncoderParent> kungfudeathgrip(this);
  Release();
  Shutdown();
}

GMPErr
GMPVideoEncoderParent::InitEncode(const GMPVideoCodec& aCodecSettings,
                                  const nsTArray<uint8_t>& aCodecSpecific,
                                  GMPVideoEncoderCallbackProxy* aCallback,
                                  int32_t aNumberOfCores,
                                  uint32_t aMaxPayloadSize)
{
  LOGD(("%s::%s: %p", __CLASS__, __FUNCTION__, this));
  if (mIsOpen) {
    NS_WARNING("Trying to re-init an in-use GMP video encoder!");
    return GMPGenericErr;;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!aCallback) {
    return GMPGenericErr;
  }
  mCallback = aCallback;

  if (!SendInitEncode(aCodecSettings, aCodecSpecific, aNumberOfCores, aMaxPayloadSize)) {
    return GMPGenericErr;
  }
  mIsOpen = true;

  // Async IPC, we don't have access to a return value.
  return GMPNoErr;
}

GMPErr
GMPVideoEncoderParent::Encode(GMPUniquePtr<GMPVideoi420Frame> aInputFrame,
                              const nsTArray<uint8_t>& aCodecSpecificInfo,
                              const nsTArray<GMPVideoFrameType>& aFrameTypes)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use an dead GMP video encoder");
    return GMPGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  GMPUniquePtr<GMPVideoi420FrameImpl> inputFrameImpl(
    static_cast<GMPVideoi420FrameImpl*>(aInputFrame.release()));

  // Very rough kill-switch if the plugin stops processing.  If it's merely
  // hung and continues, we'll come back to life eventually.
  // 3* is because we're using 3 buffers per frame for i420 data for now.
  if ((NumInUse(GMPSharedMem::kGMPFrameData) > 3*GMPSharedMem::kGMPBufLimit) ||
      (NumInUse(GMPSharedMem::kGMPEncodedData) > GMPSharedMem::kGMPBufLimit)) {
    return GMPGenericErr;
  }

  GMPVideoi420FrameData frameData;
  inputFrameImpl->InitFrameData(frameData);

  if (!SendEncode(frameData,
                  aCodecSpecificInfo,
                  aFrameTypes)) {
    return GMPGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPNoErr;
}

GMPErr
GMPVideoEncoderParent::SetChannelParameters(uint32_t aPacketLoss, uint32_t aRTT)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!SendSetChannelParameters(aPacketLoss, aRTT)) {
    return GMPGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPNoErr;
}

GMPErr
GMPVideoEncoderParent::SetRates(uint32_t aNewBitRate, uint32_t aFrameRate)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use an dead GMP video decoder");
    return GMPGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!SendSetRates(aNewBitRate, aFrameRate)) {
    return GMPGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPNoErr;
}

GMPErr
GMPVideoEncoderParent::SetPeriodicKeyFrames(bool aEnable)
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!SendSetPeriodicKeyFrames(aEnable)) {
    return GMPGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPNoErr;
}

// Note: Consider keeping ActorDestroy sync'd up when making changes here.
void
GMPVideoEncoderParent::Shutdown()
{
  LOGD(("%s::%s: %p", __CLASS__, __FUNCTION__, this));
  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (mShuttingDown) {
    return;
  }
  mShuttingDown = true;

  // Notify client we're gone!  Won't occur after Close()
  if (mCallback) {
    mCallback->Terminated();
    mCallback = nullptr;
  }

  mIsOpen = false;
  if (!mActorDestroyed) {
    Unused << SendEncodingComplete();
  }
}

static void
ShutdownEncodedThread(nsCOMPtr<nsIThread>& aThread)
{
  aThread->Shutdown();
}

// Note: Keep this sync'd up with Shutdown
void
GMPVideoEncoderParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGD(("%s::%s: %p (%d)", __CLASS__, __FUNCTION__, this, (int) aWhy));
  mIsOpen = false;
  mActorDestroyed = true;
  if (mCallback) {
    // May call Close() (and Shutdown()) immediately or with a delay
    mCallback->Terminated();
    mCallback = nullptr;
  }
  // Must be shut down before VideoEncoderDestroyed(), since this can recurse
  // the GMPThread event loop.  See bug 1049501
  if (mEncodedThread) {
    // Can't get it to allow me to use WrapRunnable with a nsCOMPtr<nsIThread>()
    NS_DispatchToMainThread(
      WrapRunnableNM<decltype(&ShutdownEncodedThread),
                     nsCOMPtr<nsIThread> >(&ShutdownEncodedThread, mEncodedThread));
    mEncodedThread = nullptr;
  }
  if (mPlugin) {
    // Ignore any return code. It is OK for this to fail without killing the process.
    mPlugin->VideoEncoderDestroyed(this);
    mPlugin = nullptr;
  }
  mVideoHost.ActorDestroyed(); // same as DoneWithAPI
}

static void
EncodedCallback(GMPVideoEncoderCallbackProxy* aCallback,
                GMPVideoEncodedFrame* aEncodedFrame,
                nsTArray<uint8_t>* aCodecSpecificInfo,
                nsCOMPtr<nsIThread> aThread)
{
  aCallback->Encoded(aEncodedFrame, *aCodecSpecificInfo);
  delete aCodecSpecificInfo;
  // Ugh.  Must destroy the frame on GMPThread.
  // XXX add locks to the ShmemManager instead?
  aThread->Dispatch(WrapRunnable(aEncodedFrame,
                                &GMPVideoEncodedFrame::Destroy),
                   NS_DISPATCH_NORMAL);
}

bool
GMPVideoEncoderParent::RecvEncoded(const GMPVideoEncodedFrameData& aEncodedFrame,
                                   InfallibleTArray<uint8_t>&& aCodecSpecificInfo)
{
  if (!mCallback) {
    return false;
  }

  auto f = new GMPVideoEncodedFrameImpl(aEncodedFrame, &mVideoHost);
  nsTArray<uint8_t> *codecSpecificInfo = new nsTArray<uint8_t>;
  codecSpecificInfo->AppendElements((uint8_t*)aCodecSpecificInfo.Elements(), aCodecSpecificInfo.Length());
  nsCOMPtr<nsIThread> thread = NS_GetCurrentThread();

  mEncodedThread->Dispatch(WrapRunnableNM(&EncodedCallback,
                                          mCallback, f, codecSpecificInfo, thread),
                           NS_DISPATCH_NORMAL);

  return true;
}

bool
GMPVideoEncoderParent::RecvError(const GMPErr& aError)
{
  if (!mCallback) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->Error(aError);

  return true;
}

bool
GMPVideoEncoderParent::RecvShutdown()
{
  Shutdown();
  return true;
}

bool
GMPVideoEncoderParent::RecvParentShmemForPool(Shmem&& aFrameBuffer)
{
  if (aFrameBuffer.IsWritable()) {
    // This test may be paranoia now that we don't shut down the VideoHost
    // in ::Shutdown, but doesn't hurt
    if (mVideoHost.SharedMemMgr()) {
      mVideoHost.SharedMemMgr()->MgrDeallocShmem(GMPSharedMem::kGMPFrameData,
                                                 aFrameBuffer);
    } else {
      LOGD(("%s::%s: %p Called in shutdown, ignoring and freeing directly", __CLASS__, __FUNCTION__, this));
      DeallocShmem(aFrameBuffer);
    }
  }
  return true;
}

bool
GMPVideoEncoderParent::AnswerNeedShmem(const uint32_t& aEncodedBufferSize,
                                       Shmem* aMem)
{
  ipc::Shmem mem;

  // This test may be paranoia now that we don't shut down the VideoHost
  // in ::Shutdown, but doesn't hurt
  if (!mVideoHost.SharedMemMgr() ||
      !mVideoHost.SharedMemMgr()->MgrAllocShmem(GMPSharedMem::kGMPEncodedData,
                                                aEncodedBufferSize,
                                                ipc::SharedMemory::TYPE_BASIC, &mem))
  {
    LOG(LogLevel::Error, ("%s::%s: Failed to get a shared mem buffer for Child! size %u",
                       __CLASS__, __FUNCTION__, aEncodedBufferSize));
    return false;
  }
  *aMem = mem;
  mem = ipc::Shmem();
  return true;
}

bool
GMPVideoEncoderParent::Recv__delete__()
{
  if (mPlugin) {
    // Ignore any return code. It is OK for this to fail without killing the process.
    mPlugin->VideoEncoderDestroyed(this);
    mPlugin = nullptr;
  }

  return true;
}

} // namespace gmp
} // namespace mozilla
