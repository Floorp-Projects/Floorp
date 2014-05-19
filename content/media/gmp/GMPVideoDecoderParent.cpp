/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPVideoDecoderParent.h"
#include "GMPVideoEncodedFrameImpl.h"
#include "GMPVideoi420FrameImpl.h"
#include "GMPParent.h"
#include <stdio.h>
#include "mozilla/unused.h"
#include "GMPMessageUtils.h"
#include "nsAutoRef.h"
#include "nsThreadUtils.h"

template <>
class nsAutoRefTraits<GMPVideoEncodedFrame> : public nsPointerRefTraits<GMPVideoEncodedFrame>
{
public:
  static void Release(GMPVideoEncodedFrame* aFrame) { aFrame->Destroy(); }
};

namespace mozilla {
namespace gmp {

GMPVideoDecoderParent::GMPVideoDecoderParent(GMPParent* aPlugin)
  : mCanSendMessages(true)
  , mPlugin(aPlugin)
  , mCallback(nullptr)
  , mVideoHost(MOZ_THIS_IN_INITIALIZER_LIST())
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

bool
GMPVideoDecoderParent::MgrAllocShmem(size_t aSize,
                                     ipc::Shmem::SharedMemory::SharedMemoryType aType,
                                     ipc::Shmem* aMem)
{
  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  return AllocShmem(aSize, aType, aMem);
}

bool
GMPVideoDecoderParent::MgrDeallocShmem(Shmem& aMem)
{
  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  return DeallocShmem(aMem);
}

GMPVideoErr
GMPVideoDecoderParent::InitDecode(const GMPVideoCodec& aCodecSettings,
                                  GMPDecoderCallback* aCallback,
                                  int32_t aCoreCount)
{
  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return GMPVideoGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!aCallback) {
    return GMPVideoGenericErr;
  }
  mCallback = aCallback;

  if (!SendInitDecode(aCodecSettings, aCoreCount)) {
    return GMPVideoGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoDecoderParent::Decode(GMPVideoEncodedFrame* aInputFrame,
                              bool aMissingFrames,
                              const GMPCodecSpecificInfo& aCodecSpecificInfo,
                              int64_t aRenderTimeMs)
{
  nsAutoRef<GMPVideoEncodedFrame> autoDestroy(aInputFrame);

  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return GMPVideoGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  auto inputFrameImpl = static_cast<GMPVideoEncodedFrameImpl*>(aInputFrame);

  GMPVideoEncodedFrameData frameData;
  inputFrameImpl->RelinquishFrameData(frameData);

  if (!SendDecode(frameData,
                  aMissingFrames,
                  aCodecSpecificInfo,
                  aRenderTimeMs)) {
    return GMPVideoGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoDecoderParent::Reset()
{
  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return GMPVideoGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!SendReset()) {
    return GMPVideoGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPVideoNoErr;
}

GMPVideoErr
GMPVideoDecoderParent::Drain()
{
  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return GMPVideoGenericErr;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!SendDrain()) {
    return GMPVideoGenericErr;
  }

  // Async IPC, we don't have access to a return value.
  return GMPVideoNoErr;
}

// Note: Consider keeping ActorDestroy sync'd up when making changes here.
void
GMPVideoDecoderParent::DecodingComplete()
{
  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  mCanSendMessages = false;

  mCallback = nullptr;

  mVideoHost.DoneWithAPI();

  unused << SendDecodingComplete();
}

// Note: Keep this sync'd up with DecodingComplete
void
GMPVideoDecoderParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mPlugin) {
    // Ignore any return code. It is OK for this to fail without killing the process.
    mPlugin->VideoDecoderDestroyed(this);
    mPlugin = nullptr;
  }
  mCanSendMessages = false;
  mCallback = nullptr;
  mVideoHost.ActorDestroyed();
}

bool
GMPVideoDecoderParent::RecvDecoded(const GMPVideoi420FrameData& aDecodedFrame)
{
  if (!mCallback) {
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
  if (!mCallback) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->InputDataExhausted();

  return true;
}

bool
GMPVideoDecoderParent::Recv__delete__()
{
  if (mPlugin) {
    // Ignore any return code. It is OK for this to fail without killing the process.
    mPlugin->VideoDecoderDestroyed(this);
    mPlugin = nullptr;
  }

  return true;
}

} // namespace gmp
} // namespace mozilla
