/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPAudioDecoderParent.h"
#include "GMPContentParent.h"
#include <stdio.h>
#include "mozilla/unused.h"
#include "GMPMessageUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/Logging.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

extern PRLogModuleInfo* GetGMPLog();

#define LOGD(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Debug, msg)
#define LOG(level, msg) MOZ_LOG(GetGMPLog(), (level), msg)

namespace gmp {

GMPAudioDecoderParent::GMPAudioDecoderParent(GMPContentParent* aPlugin)
  : mIsOpen(false)
  , mShuttingDown(false)
  , mActorDestroyed(false)
  , mPlugin(aPlugin)
  , mCallback(nullptr)
{
  MOZ_ASSERT(mPlugin);
}

GMPAudioDecoderParent::~GMPAudioDecoderParent()
{
}

nsresult
GMPAudioDecoderParent::InitDecode(GMPAudioCodecType aCodecType,
                                  uint32_t aChannelCount,
                                  uint32_t aBitsPerChannel,
                                  uint32_t aSamplesPerSecond,
                                  nsTArray<uint8_t>& aExtraData,
                                  GMPAudioDecoderCallbackProxy* aCallback)
{
  if (mIsOpen) {
    NS_WARNING("Trying to re-init an in-use GMP audio decoder!");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!aCallback) {
    return NS_ERROR_FAILURE;
  }
  mCallback = aCallback;

  GMPAudioCodecData data;
  data.mCodecType() = aCodecType;
  data.mChannelCount() = aChannelCount;
  data.mBitsPerChannel() = aBitsPerChannel;
  data.mSamplesPerSecond() = aSamplesPerSecond;
  data.mExtraData() = aExtraData;
  if (!SendInitDecode(data)) {
    return NS_ERROR_FAILURE;
  }
  mIsOpen = true;

  // Async IPC, we don't have access to a return value.
  return NS_OK;
}

nsresult
GMPAudioDecoderParent::Decode(GMPAudioSamplesImpl& aEncodedSamples)
{

  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP Audio decoder!");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  GMPAudioEncodedSampleData samples;
  aEncodedSamples.RelinquishData(samples);

  if (!SendDecode(samples)) {
    return NS_ERROR_FAILURE;
  }

  // Async IPC, we don't have access to a return value.
  return NS_OK;
}

nsresult
GMPAudioDecoderParent::Reset()
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP Audio decoder!");
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
GMPAudioDecoderParent::Drain()
{
  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP Audio decoder!");
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
GMPAudioDecoderParent::Close()
{
  LOGD(("%s: %p", __FUNCTION__, this));
  MOZ_ASSERT(!mPlugin || mPlugin->GMPThread() == NS_GetCurrentThread());

  // Consumer is done with us; we can shut down.  No more callbacks should
  // be made to mCallback.  Note: do this before Shutdown()!
  mCallback = nullptr;
  // Let Shutdown mark us as dead so it knows if we had been alive

  // In case this is the last reference
  nsRefPtr<GMPAudioDecoderParent> kungfudeathgrip(this);
  Release();
  Shutdown();

  return NS_OK;
}

// Note: Consider keeping ActorDestroy sync'd up when making changes here.
nsresult
GMPAudioDecoderParent::Shutdown()
{
  LOGD(("%s: %p", __FUNCTION__, this));
  MOZ_ASSERT(!mPlugin || mPlugin->GMPThread() == NS_GetCurrentThread());

  if (mShuttingDown) {
    return NS_OK;
  }
  mShuttingDown = true;

  // Notify client we're gone!  Won't occur after Close()
  if (mCallback) {
    mCallback->Terminated();
    mCallback = nullptr;
  }

  mIsOpen = false;
  if (!mActorDestroyed) {
    unused << SendDecodingComplete();
  }

  return NS_OK;
}

// Note: Keep this sync'd up with DecodingComplete
void
GMPAudioDecoderParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mIsOpen = false;
  mActorDestroyed = true;
  if (mCallback) {
    // May call Close() (and Shutdown()) immediately or with a delay
    mCallback->Terminated();
    mCallback = nullptr;
  }
  if (mPlugin) {
    // Ignore any return code. It is OK for this to fail without killing the process.
    mPlugin->AudioDecoderDestroyed(this);
    mPlugin = nullptr;
  }
}

bool
GMPAudioDecoderParent::RecvDecoded(const GMPAudioDecodedSampleData& aDecoded)
{
  if (!mCallback) {
    return false;
  }

  mCallback->Decoded(aDecoded.mData(),
                     aDecoded.mTimeStamp(),
                     aDecoded.mChannelCount(),
                     aDecoded.mSamplesPerSecond());

  return true;
}

bool
GMPAudioDecoderParent::RecvInputDataExhausted()
{
  if (!mCallback) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->InputDataExhausted();

  return true;
}

bool
GMPAudioDecoderParent::RecvDrainComplete()
{
  if (!mCallback) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->DrainComplete();

  return true;
}

bool
GMPAudioDecoderParent::RecvResetComplete()
{
  if (!mCallback) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->ResetComplete();

  return true;
}

bool
GMPAudioDecoderParent::RecvError(const GMPErr& aError)
{
  if (!mCallback) {
    return false;
  }

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->Error(aError);

  return true;
}

bool
GMPAudioDecoderParent::RecvShutdown()
{
  Shutdown();
  return true;
}

bool
GMPAudioDecoderParent::Recv__delete__()
{
  if (mPlugin) {
    // Ignore any return code. It is OK for this to fail without killing the process.
    mPlugin->AudioDecoderDestroyed(this);
    mPlugin = nullptr;
  }

  return true;
}

} // namespace gmp
} // namespace mozilla
