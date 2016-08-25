/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPAudioDecoderParent.h"
#include "GMPContentParent.h"
#include <stdio.h>
#include "mozilla/Unused.h"
#include "GMPMessageUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/Logging.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

extern LogModule* GetGMPLog();

#define LOGV(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Verbose, msg)
#define LOGD(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Debug, msg)
#define LOG(level, msg) MOZ_LOG(GetGMPLog(), (level), msg)

namespace gmp {

GMPAudioDecoderParent::GMPAudioDecoderParent(GMPContentParent* aPlugin)
  : mIsOpen(false)
  , mShuttingDown(false)
  , mActorDestroyed(false)
  , mIsAwaitingResetComplete(false)
  , mIsAwaitingDrainComplete(false)
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
  LOGD(("GMPAudioDecoderParent[%p]::InitDecode()", this));

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
  LOGV(("GMPAudioDecoderParent[%p]::Decode() timestamp=%lld",
        this, aEncodedSamples.TimeStamp()));

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
  LOGD(("GMPAudioDecoderParent[%p]::Reset()", this));

  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP Audio decoder!");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mPlugin->GMPThread() == NS_GetCurrentThread());

  if (!SendReset()) {
    return NS_ERROR_FAILURE;
  }

  mIsAwaitingResetComplete = true;

  // Async IPC, we don't have access to a return value.
  return NS_OK;
}

nsresult
GMPAudioDecoderParent::Drain()
{
  LOGD(("GMPAudioDecoderParent[%p]::Drain()", this));

  if (!mIsOpen) {
    NS_WARNING("Trying to use a dead GMP Audio decoder!");
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

// Note: Consider keeping ActorDestroy sync'd up when making changes here.
nsresult
GMPAudioDecoderParent::Close()
{
  LOGD(("GMPAudioDecoderParent[%p]::Close()", this));
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
  RefPtr<GMPAudioDecoderParent> kungfudeathgrip(this);
  Release();
  Shutdown();

  return NS_OK;
}

// Note: Consider keeping ActorDestroy sync'd up when making changes here.
nsresult
GMPAudioDecoderParent::Shutdown()
{
  LOGD(("GMPAudioDecoderParent[%p]::Shutdown()", this));
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

// Note: Keep this sync'd up with DecodingComplete
void
GMPAudioDecoderParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOGD(("GMPAudioDecoderParent[%p]::ActorDestroy(reason=%d)", this, aWhy));

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
    mPlugin->AudioDecoderDestroyed(this);
    mPlugin = nullptr;
  }
  MaybeDisconnect(aWhy == AbnormalShutdown);
}

bool
GMPAudioDecoderParent::RecvDecoded(const GMPAudioDecodedSampleData& aDecoded)
{
  LOGV(("GMPAudioDecoderParent[%p]::RecvDecoded() timestamp=%lld",
        this, aDecoded.mTimeStamp()));

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
  LOGV(("GMPAudioDecoderParent[%p]::RecvInputDataExhausted()", this));

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
  LOGD(("GMPAudioDecoderParent[%p]::RecvDrainComplete()", this));

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
GMPAudioDecoderParent::RecvResetComplete()
{
  LOGD(("GMPAudioDecoderParent[%p]::RecvResetComplete()", this));

  if (!mCallback) {
    return false;
  }

  if (!mIsAwaitingResetComplete) {
    return true;
  }
  mIsAwaitingResetComplete = false;

  // Ignore any return code. It is OK for this to fail without killing the process.
  mCallback->ResetComplete();

  return true;
}

bool
GMPAudioDecoderParent::RecvError(const GMPErr& aError)
{
  LOGD(("GMPAudioDecoderParent[%p]::RecvError(error=%d)", this, aError));

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
GMPAudioDecoderParent::RecvShutdown()
{
  LOGD(("GMPAudioDecoderParent[%p]::RecvShutdown()", this));

  Shutdown();
  return true;
}

bool
GMPAudioDecoderParent::Recv__delete__()
{
  LOGD(("GMPAudioDecoderParent[%p]::Recv__delete__()", this));

  if (mPlugin) {
    // Ignore any return code. It is OK for this to fail without killing the process.
    mPlugin->AudioDecoderDestroyed(this);
    mPlugin = nullptr;
  }

  return true;
}

void
GMPAudioDecoderParent::UnblockResetAndDrain()
{
  LOGD(("GMPAudioDecoderParent[%p]::UnblockResetAndDrain()", this));

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
}

} // namespace gmp
} // namespace mozilla
