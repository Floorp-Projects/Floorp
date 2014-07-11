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
#include "mozilla/gmp/GMPTypes.h"

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

nsresult
GMPVideoDecoderParent::InitDecode(const GMPVideoCodec& aCodecSettings,
                                  const nsTArray<uint8_t>& aCodecSpecific,
                                  GMPVideoDecoderCallback* aCallback,
                                  int32_t aCoreCount)
{
  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
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

  // Async IPC, we don't have access to a return value.
  return NS_OK;
}

nsresult
GMPVideoDecoderParent::Decode(GMPVideoEncodedFrame* aInputFrame,
                              bool aMissingFrames,
                              const nsTArray<uint8_t>& aCodecSpecificInfo,
                              int64_t aRenderTimeMs)
{
  nsAutoRef<GMPVideoEncodedFrame> autoDestroy(aInputFrame);

  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  auto inputFrameImpl = static_cast<GMPVideoEncodedFrameImpl*>(aInputFrame);

  GMPVideoEncodedFrameData frameData;
  inputFrameImpl->RelinquishFrameData(frameData);

  // Very rough kill-switch if the plugin stops processing.  If it's merely
  // hung and continues, we'll come back to life eventually.
  // 3* is because we're using 3 buffers per frame for i420 data for now.
  if (NumInUse(kGMPFrameData) > 3*GMPSharedMemManager::kGMPBufLimit ||
      NumInUse(kGMPEncodedData) > GMPSharedMemManager::kGMPBufLimit) {
    return NS_ERROR_FAILURE;
  }

  if (!SendDecode(frameData,
                  aMissingFrames,
                  aCodecSpecificInfo,
                  aRenderTimeMs)) {
    return NS_ERROR_FAILURE;
  }

  // Async IPC, we don't have access to a return value.
  return NS_OK;
}

nsresult
GMPVideoDecoderParent::Reset()
{
  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!SendReset()) {
    return NS_ERROR_FAILURE;
  }

  // Async IPC, we don't have access to a return value.
  return NS_OK;
}

nsresult
GMPVideoDecoderParent::Drain()
{
  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!SendDrain()) {
    return NS_ERROR_FAILURE;
  }

  // Async IPC, we don't have access to a return value.
  return NS_OK;
}

// Note: Consider keeping ActorDestroy sync'd up when making changes here.
nsresult
GMPVideoDecoderParent::DecodingComplete()
{
  if (!mCanSendMessages) {
    NS_WARNING("Trying to use an invalid GMP video decoder!");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  mCanSendMessages = false;

  mCallback = nullptr;

  mVideoHost.DoneWithAPI();

  unused << SendDecodingComplete();

  return NS_OK;
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

void
GMPVideoDecoderParent::CheckThread()
{
  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());
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
GMPVideoDecoderParent::RecvDrainComplete()
{
  if (!mCallback) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->DrainComplete();

  return true;
}

bool
GMPVideoDecoderParent::RecvResetComplete()
{
  if (!mCallback) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->ResetComplete();

  return true;
}

bool
GMPVideoDecoderParent::RecvParentShmemForPool(Shmem& aEncodedBuffer)
{
  if (aEncodedBuffer.IsWritable()) {
    mVideoHost.SharedMemMgr()->MgrDeallocShmem(GMPSharedMemManager::kGMPEncodedData,
                                               aEncodedBuffer);
  }
  return true;
}

bool
GMPVideoDecoderParent::AnswerNeedShmem(const uint32_t& aFrameBufferSize,
                                       Shmem* aMem)
{
  ipc::Shmem mem;

  if (!mVideoHost.SharedMemMgr()->MgrAllocShmem(GMPSharedMemManager::kGMPFrameData,
                                                aFrameBufferSize,
                                                ipc::SharedMemory::TYPE_BASIC, &mem))
  {
    return false;
  }
  *aMem = mem;
  mem = ipc::Shmem();
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
