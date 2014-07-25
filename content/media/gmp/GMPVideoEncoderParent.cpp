/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoEncoderParent.h"
#include "GMPVideoi420FrameImpl.h"
#include "GMPVideoEncodedFrameImpl.h"
#include <stdio.h>
#include "mozilla/unused.h"
#include "GMPMessageUtils.h"
#include "nsAutoRef.h"
#include "GMPParent.h"
#include "mozilla/gmp/GMPTypes.h"
#include "nsThreadUtils.h"

template <>
class nsAutoRefTraits<GMPVideoi420Frame> : public nsPointerRefTraits<GMPVideoi420Frame>
{
public:
  static void Release(GMPVideoi420Frame* aFrame) { aFrame->Destroy(); }
};

namespace mozilla {
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

GMPVideoEncoderParent::GMPVideoEncoderParent(GMPParent *aPlugin)
: mIsOpen(false),
  mPlugin(aPlugin),
  mCallback(nullptr),
  mVideoHost(MOZ_THIS_IN_INITIALIZER_LIST())
{
  MOZ_ASSERT(mPlugin);
}

GMPVideoEncoderParent::~GMPVideoEncoderParent()
{
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
  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());
  // Consumer is done with us; we can shut down.  No more callbacks should
  // be made to mCallback.  Note: do this before Shutdown()!
  mCallback = nullptr;
  // Let Shutdown mark us as dead so it knows if we had been alive

  // In case this is the last reference
  nsRefPtr<GMPVideoEncoderParent> kungfudeathgrip(this);
  NS_RELEASE(kungfudeathgrip);
  Shutdown();
}

GMPErr
GMPVideoEncoderParent::InitEncode(const GMPVideoCodec& aCodecSettings,
                                  const nsTArray<uint8_t>& aCodecSpecific,
                                  GMPVideoEncoderCallbackProxy* aCallback,
                                  int32_t aNumberOfCores,
                                  uint32_t aMaxPayloadSize)
{
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
GMPVideoEncoderParent::Encode(GMPVideoi420Frame* aInputFrame,
                              const nsTArray<uint8_t>& aCodecSpecificInfo,
                              const nsTArray<GMPVideoFrameType>& aFrameTypes)
{
  nsAutoRef<GMPVideoi420Frame> frameRef(aInputFrame);

  if (!mIsOpen) {
    NS_WARNING("Trying to use an dead GMP video encoder");
    return GMPGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  auto inputFrameImpl = static_cast<GMPVideoi420FrameImpl*>(aInputFrame);

  // Very rough kill-switch if the plugin stops processing.  If it's merely
  // hung and continues, we'll come back to life eventually.
  // 3* is because we're using 3 buffers per frame for i420 data for now.
  if (NumInUse(kGMPFrameData) > 3*GMPSharedMemManager::kGMPBufLimit ||
      NumInUse(kGMPEncodedData) > GMPSharedMemManager::kGMPBufLimit) {
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
  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  // Notify client we're gone!  Won't occur after Close()
  if (mCallback) {
    mCallback->Terminated();
    mCallback = nullptr;
  }
  mVideoHost.DoneWithAPI();
  if (mIsOpen) {
    // Don't send EncodingComplete if we died
    mIsOpen = false;
    unused << SendEncodingComplete();
  }
}

// Note: Keep this sync'd up with Shutdown
void
GMPVideoEncoderParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mIsOpen = false;
  if (mCallback) {
    // May call Close() (and Shutdown()) immediately or with a delay
    mCallback->Terminated();
    mCallback = nullptr;
  }
  if (mPlugin) {
    // Ignore any return code. It is OK for this to fail without killing the process.
    mPlugin->VideoEncoderDestroyed(this);
    mPlugin = nullptr;
  }
  mVideoHost.ActorDestroyed();
}

void
GMPVideoEncoderParent::CheckThread()
{
  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());
}

bool
GMPVideoEncoderParent::RecvEncoded(const GMPVideoEncodedFrameData& aEncodedFrame,
                                   const nsTArray<uint8_t>& aCodecSpecificInfo)
{
  if (!mCallback) {
    return false;
  }

  auto f = new GMPVideoEncodedFrameImpl(aEncodedFrame, &mVideoHost);

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->Encoded(f, aCodecSpecificInfo);

  // Return SHM to sender to recycle
  //SendEncodedReturn(aEncodedFrame, aCodecSpecificInfo);
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
GMPVideoEncoderParent::RecvParentShmemForPool(Shmem& aFrameBuffer)
{
  if (aFrameBuffer.IsWritable()) {
    mVideoHost.SharedMemMgr()->MgrDeallocShmem(GMPSharedMemManager::kGMPFrameData,
                                               aFrameBuffer);
  }
  return true;
}

bool
GMPVideoEncoderParent::AnswerNeedShmem(const uint32_t& aEncodedBufferSize,
                                       Shmem* aMem)
{
  ipc::Shmem mem;

  if (!mVideoHost.SharedMemMgr()->MgrAllocShmem(GMPSharedMemManager::kGMPEncodedData,
                                                aEncodedBufferSize,
                                                ipc::SharedMemory::TYPE_BASIC, &mem))
  {
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
