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

GMPVideoEncoderParent::GMPVideoEncoderParent(GMPParent *aPlugin)
: mCanSendMessages(true),
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

bool
GMPVideoEncoderParent::MgrAllocShmem(size_t aSize,
                                     ipc::Shmem::SharedMemory::SharedMemoryType aType,
                                     ipc::Shmem* aMem)
{
  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  return AllocShmem(aSize, aType, aMem);
}

bool
GMPVideoEncoderParent::MgrDeallocShmem(Shmem& aMem)
{
  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  return DeallocShmem(aMem);
}

GMPVideoErr
GMPVideoEncoderParent::InitEncode(const GMPVideoCodec& aCodecSettings,
                                  GMPEncoderCallback* aCallback,
                                  int32_t aNumberOfCores,
                                  uint32_t aMaxPayloadSize)
{
  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPVideoGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!aCallback) {
    return GMPVideoGenericErr;
  }
  mCallback = aCallback;

  if (!SendInitEncode(aCodecSettings, aNumberOfCores, aMaxPayloadSize)) {
    return GMPVideoGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoEncoderParent::Encode(GMPVideoi420Frame* aInputFrame,
                              const GMPCodecSpecificInfo& aCodecSpecificInfo,
                              const std::vector<GMPVideoFrameType>& aFrameTypes)
{
  nsAutoRef<GMPVideoi420Frame> frameRef(aInputFrame);

  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPVideoGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  auto inputFrameImpl = static_cast<GMPVideoi420FrameImpl*>(aInputFrame);

  GMPVideoi420FrameData frameData;
  inputFrameImpl->InitFrameData(frameData);

  InfallibleTArray<int> frameTypes;
  frameTypes.SetCapacity(aFrameTypes.size());
  for (std::vector<int>::size_type i = 0; i != aFrameTypes.size(); i++) {
    frameTypes.AppendElement(static_cast<int>(aFrameTypes[i]));
  }

  if (!SendEncode(frameData,
                  aCodecSpecificInfo,
                  frameTypes)) {
    return GMPVideoGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoEncoderParent::SetChannelParameters(uint32_t aPacketLoss, uint32_t aRTT)
{
  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPVideoGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!SendSetChannelParameters(aPacketLoss, aRTT)) {
    return GMPVideoGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoEncoderParent::SetRates(uint32_t aNewBitRate, uint32_t aFrameRate)
{
  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPVideoGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!SendSetRates(aNewBitRate, aFrameRate)) {
    return GMPVideoGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoEncoderParent::SetPeriodicKeyFrames(bool aEnable)
{
  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return GMPVideoGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!SendSetPeriodicKeyFrames(aEnable)) {
    return GMPVideoGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPVideoNoErr;
}

// Note: Consider keeping ActorDestroy sync'd up when making changes here.
void
GMPVideoEncoderParent::EncodingComplete()
{
  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video encoder!");
    return;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  mCanSendMessages = false;

  mCallback = nullptr;

  mVideoHost.DoneWithAPI();

  unused << SendEncodingComplete();
}

// Note: Keep this sync'd up with DecodingComplete
void
GMPVideoEncoderParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mPlugin) {
    // Ignore any return code. It is OK for this to fail without killing the process.
    mPlugin->VideoEncoderDestroyed(this);
    mPlugin = nullptr;
  }
  mCanSendMessages = false;
  mCallback = nullptr;
  mVideoHost.ActorDestroyed();
}

bool
GMPVideoEncoderParent::RecvEncoded(const GMPVideoEncodedFrameData& aEncodedFrame,
                                   const GMPCodecSpecificInfo& aCodecSpecificInfo)
{
  if (!mCallback) {
    return false;
  }

  auto f = new GMPVideoEncodedFrameImpl(aEncodedFrame, &mVideoHost);

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->Encoded(f, aCodecSpecificInfo);

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
