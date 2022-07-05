/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoEncoderParent.h"

#include "GMPContentParent.h"
#include "GMPCrashHelper.h"
#include "GMPLog.h"
#include "GMPMessageUtils.h"
#include "GMPVideoEncodedFrameImpl.h"
#include "GMPVideoi420FrameImpl.h"
#include "mozilla/gmp/GMPTypes.h"
#include "mozilla/Unused.h"
#include "nsAutoRef.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "runnable_utils.h"

namespace mozilla::gmp {

#ifdef __CLASS__
#  undef __CLASS__
#endif
#define __CLASS__ "GMPVideoEncoderParent"

// States:
// Initial: mIsOpen == false
//    on InitDecode success -> Open
//    on Shutdown -> Dead
// Open: mIsOpen == true
//    on Close -> Dead
//    on ActorDestroy -> Dead
//    on Shutdown -> Dead
// Dead: mIsOpen == false

GMPVideoEncoderParent::GMPVideoEncoderParent(GMPContentParent* aPlugin)
    : GMPSharedMemManager(aPlugin),
      mIsOpen(false),
      mShuttingDown(false),
      mActorDestroyed(false),
      mPlugin(aPlugin),
      mCallback(nullptr),
      mVideoHost(this),
      mPluginId(aPlugin->GetPluginId()) {
  MOZ_ASSERT(mPlugin);
}

GMPVideoHostImpl& GMPVideoEncoderParent::Host() { return mVideoHost; }

// Note: may be called via Terminated()
void GMPVideoEncoderParent::Close() {
  GMP_LOG_DEBUG("%s::%s: %p", __CLASS__, __FUNCTION__, this);
  MOZ_ASSERT(mPlugin->GMPEventTarget()->IsOnCurrentThread());
  // Consumer is done with us; we can shut down.  No more callbacks should
  // be made to mCallback.  Note: do this before Shutdown()!
  mCallback = nullptr;

  // Let Shutdown mark us as dead so it knows if we had been alive

  // In case this is the last reference
  RefPtr<GMPVideoEncoderParent> kungfudeathgrip(this);
  Release();
  Shutdown();
}

GMPErr GMPVideoEncoderParent::InitEncode(
    const GMPVideoCodec& aCodecSettings,
    const nsTArray<uint8_t>& aCodecSpecific,
    GMPVideoEncoderCallbackProxy* aCallback, int32_t aNumberOfCores,
    uint32_t aMaxPayloadSize) {
  GMP_LOG_DEBUG("%s::%s: %p", __CLASS__, __FUNCTION__, this);
  if (mIsOpen) {
    NS_WARNING("Trying to re-init an in-use GMP video encoder!");
    return GMPGenericErr;
    ;
  }

  MOZ_ASSERT(mPlugin->GMPEventTarget()->IsOnCurrentThread());
  MOZ_ASSERT(!mCallback);

  if (!aCallback) {
    return GMPGenericErr;
  }
  mCallback = aCallback;

  if (!SendInitEncode(aCodecSettings, aCodecSpecific, aNumberOfCores,
                      aMaxPayloadSize)) {
    return GMPGenericErr;
  }
  mIsOpen = true;

  // Async IPC, we don't have access to a return value.
  return GMPNoErr;
}

GMPErr GMPVideoEncoderParent::Encode(
    GMPUniquePtr<GMPVideoi420Frame> aInputFrame,
    const nsTArray<uint8_t>& aCodecSpecificInfo,
    const nsTArray<GMPVideoFrameType>& aFrameTypes) {
  if (!mIsOpen) {
    NS_WARNING("Trying to use an dead GMP video encoder");
    return GMPGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPEventTarget()->IsOnCurrentThread());

  GMPUniquePtr<GMPVideoi420FrameImpl> inputFrameImpl(
      static_cast<GMPVideoi420FrameImpl*>(aInputFrame.release()));

  // Very rough kill-switch if the plugin stops processing.  If it's merely
  // hung and continues, we'll come back to life eventually.
  // 3* is because we're using 3 buffers per frame for i420 data for now.
  if ((NumInUse(GMPSharedMem::kGMPFrameData) >
       3 * GMPSharedMem::kGMPBufLimit) ||
      (NumInUse(GMPSharedMem::kGMPEncodedData) > GMPSharedMem::kGMPBufLimit)) {
    GMP_LOG_ERROR(
        "%s::%s: Out of mem buffers. Frame Buffers:%lu Max:%lu, Encoded "
        "Buffers: %lu Max: %lu",
        __CLASS__, __FUNCTION__,
        static_cast<unsigned long>(NumInUse(GMPSharedMem::kGMPFrameData)),
        static_cast<unsigned long>(3 * GMPSharedMem::kGMPBufLimit),
        static_cast<unsigned long>(NumInUse(GMPSharedMem::kGMPEncodedData)),
        static_cast<unsigned long>(GMPSharedMem::kGMPBufLimit));
    return GMPGenericErr;
  }

  GMPVideoi420FrameData frameData;
  inputFrameImpl->InitFrameData(frameData);

  if (!SendEncode(frameData, aCodecSpecificInfo, aFrameTypes)) {
    GMP_LOG_ERROR("%s::%s: failed to send encode", __CLASS__, __FUNCTION__);
    return GMPGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPNoErr;
}

GMPErr GMPVideoEncoderParent::SetChannelParameters(uint32_t aPacketLoss,
                                                   uint32_t aRTT) {
  if (!mIsOpen) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPEventTarget()->IsOnCurrentThread());

  if (!SendSetChannelParameters(aPacketLoss, aRTT)) {
    return GMPGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPNoErr;
}

GMPErr GMPVideoEncoderParent::SetRates(uint32_t aNewBitRate,
                                       uint32_t aFrameRate) {
  if (!mIsOpen) {
    NS_WARNING("Trying to use an dead GMP video decoder");
    return GMPGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPEventTarget()->IsOnCurrentThread());

  if (!SendSetRates(aNewBitRate, aFrameRate)) {
    return GMPGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPNoErr;
}

GMPErr GMPVideoEncoderParent::SetPeriodicKeyFrames(bool aEnable) {
  if (!mIsOpen) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPEventTarget()->IsOnCurrentThread());

  if (!SendSetPeriodicKeyFrames(aEnable)) {
    return GMPGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPNoErr;
}

// Note: Consider keeping ActorDestroy sync'd up when making changes here.
void GMPVideoEncoderParent::Shutdown() {
  GMP_LOG_DEBUG("%s::%s: %p", __CLASS__, __FUNCTION__, this);
  MOZ_ASSERT(mPlugin->GMPEventTarget()->IsOnCurrentThread());

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

// Note: Keep this sync'd up with Shutdown
void GMPVideoEncoderParent::ActorDestroy(ActorDestroyReason aWhy) {
  GMP_LOG_DEBUG("%s::%s: %p (%d)", __CLASS__, __FUNCTION__, this, (int)aWhy);
  mIsOpen = false;
  mActorDestroyed = true;
  if (mCallback) {
    // May call Close() (and Shutdown()) immediately or with a delay
    mCallback->Terminated();
    mCallback = nullptr;
  }
  if (mPlugin) {
    // Ignore any return code. It is OK for this to fail without killing the
    // process.
    mPlugin->VideoEncoderDestroyed(this);
    mPlugin = nullptr;
  }
  mVideoHost.ActorDestroyed();  // same as DoneWithAPI
  MaybeDisconnect(aWhy == AbnormalShutdown);
}

mozilla::ipc::IPCResult GMPVideoEncoderParent::RecvEncoded(
    const GMPVideoEncodedFrameData& aEncodedFrame,
    nsTArray<uint8_t>&& aCodecSpecificInfo) {
  if (mCallback) {
    auto f = new GMPVideoEncodedFrameImpl(aEncodedFrame, &mVideoHost);
    mCallback->Encoded(f, aCodecSpecificInfo);
    f->Destroy();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoEncoderParent::RecvError(const GMPErr& aError) {
  if (mCallback) {
    mCallback->Error(aError);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoEncoderParent::RecvShutdown() {
  Shutdown();
  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoEncoderParent::RecvParentShmemForPool(
    Shmem&& aFrameBuffer) {
  if (aFrameBuffer.IsWritable()) {
    // This test may be paranoia now that we don't shut down the VideoHost
    // in ::Shutdown, but doesn't hurt
    if (mVideoHost.SharedMemMgr()) {
      mVideoHost.SharedMemMgr()->MgrDeallocShmem(GMPSharedMem::kGMPFrameData,
                                                 aFrameBuffer);
    } else {
      GMP_LOG_DEBUG(
          "%s::%s: %p Called in shutdown, ignoring and freeing directly",
          __CLASS__, __FUNCTION__, this);
      DeallocShmem(aFrameBuffer);
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoEncoderParent::RecvNeedShmem(
    const uint32_t& aEncodedBufferSize, Shmem* aMem) {
  ipc::Shmem mem;

  // This test may be paranoia now that we don't shut down the VideoHost
  // in ::Shutdown, but doesn't hurt
  if (!mVideoHost.SharedMemMgr() ||
      !mVideoHost.SharedMemMgr()->MgrAllocShmem(GMPSharedMem::kGMPEncodedData,
                                                aEncodedBufferSize, &mem)) {
    GMP_LOG_ERROR(
        "%s::%s: Failed to get a shared mem buffer for Child! size %u",
        __CLASS__, __FUNCTION__, aEncodedBufferSize);
    return IPC_FAIL(this, "Failed to get a shared mem buffer for Child!");
  }
  *aMem = mem;
  mem = ipc::Shmem();
  return IPC_OK();
}

mozilla::ipc::IPCResult GMPVideoEncoderParent::Recv__delete__() {
  if (mPlugin) {
    // Ignore any return code. It is OK for this to fail without killing the
    // process.
    mPlugin->VideoEncoderDestroyed(this);
    mPlugin = nullptr;
  }

  return IPC_OK();
}

}  // namespace mozilla::gmp

#undef __CLASS__
