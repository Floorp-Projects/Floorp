/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderChild.h"

#include "RemoteDecoderManagerChild.h"

namespace mozilla {

RemoteDecoderChild::RemoteDecoderChild(bool aRecreatedOnCrash)
    : mThread(RemoteDecoderManagerChild::GetManagerThread()),
      mRecreatedOnCrash(aRecreatedOnCrash),
      mRawFramePool(4) {}

RemoteDecoderChild::~RemoteDecoderChild() {
  AssertOnManagerThread();
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
}

mozilla::ipc::IPCResult RemoteDecoderChild::RecvInputExhausted() {
  AssertOnManagerThread();
  mDecodePromise.ResolveIfExists(std::move(mDecodedData), __func__);
  mDecodedData = MediaDataDecoder::DecodedData();
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderChild::RecvDrainComplete() {
  AssertOnManagerThread();
  mDrainPromise.ResolveIfExists(std::move(mDecodedData), __func__);
  mDecodedData = MediaDataDecoder::DecodedData();
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderChild::RecvError(const nsresult& aError) {
  AssertOnManagerThread();
  mDecodedData = MediaDataDecoder::DecodedData();
  mDecodePromise.RejectIfExists(aError, __func__);
  mDrainPromise.RejectIfExists(aError, __func__);
  mFlushPromise.RejectIfExists(aError, __func__);
  mShutdownPromise.ResolveIfExists(true, __func__);
  RefPtr<RemoteDecoderChild> kungFuDeathGrip = mShutdownSelfRef.forget();
  Unused << kungFuDeathGrip;
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderChild::RecvInitComplete(
    const TrackInfo::TrackType& aTrackType,
    const nsCString& aDecoderDescription, const bool& aHardware,
    const nsCString& aHardwareReason, const ConversionRequired& aConversion) {
  AssertOnManagerThread();
  mInitPromise.ResolveIfExists(aTrackType, __func__);
  mInitialized = true;
  mDescription = aDecoderDescription;
  mIsHardwareAccelerated = aHardware;
  mHardwareAcceleratedReason = aHardwareReason;
  mConversion = aConversion;
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderChild::RecvInitFailed(
    const nsresult& aReason) {
  AssertOnManagerThread();
  mInitPromise.RejectIfExists(aReason, __func__);
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderChild::RecvFlushComplete() {
  AssertOnManagerThread();
  mFlushPromise.ResolveIfExists(true, __func__);
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderChild::RecvShutdownComplete() {
  AssertOnManagerThread();
  MOZ_ASSERT(mShutdownSelfRef);
  mShutdownPromise.ResolveIfExists(true, __func__);
  RefPtr<RemoteDecoderChild> kungFuDeathGrip = mShutdownSelfRef.forget();
  Unused << kungFuDeathGrip;
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderChild::RecvDoneWithInput(
    Shmem&& aInputShmem) {
  mRawFramePool.Put(ShmemBuffer(aInputShmem));
  return IPC_OK();
}

void RemoteDecoderChild::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(mCanSend);
  // If the IPC channel is gone pending promises need to be resolved/rejected.
  if (aWhy == AbnormalShutdown) {
    // GPU process crashed, record the time and send back to MFR for telemetry.
    mRemoteProcessCrashTime = TimeStamp::Now();

    if (mRecreatedOnCrash) {
      // Defer reporting an error until we've recreated the manager so that
      // it'll be safe for MediaFormatReader to recreate decoders
      RefPtr<RemoteDecoderChild> ref = this;
      // Make sure shutdown self reference is null. Since ref is captured by the
      // lambda it is not necessary to keep it any longer.
      mShutdownSelfRef = nullptr;
      GetManager()->RunWhenGPUProcessRecreated(
          NS_NewRunnableFunction("VideoDecoderChild::ActorDestroy", [=]() {
            MediaResult error(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER);
            error.SetGPUCrashTimeStamp(ref->mRemoteProcessCrashTime);
            if (ref->mInitialized) {
              mDecodedData = MediaDataDecoder::DecodedData();
              mDecodePromise.RejectIfExists(error, __func__);
              mDrainPromise.RejectIfExists(error, __func__);
              mFlushPromise.RejectIfExists(error, __func__);
              mShutdownPromise.ResolveIfExists(true, __func__);
              // Make sure the next request will be rejected accordingly if ever
              // called.
              mNeedNewDecoder = true;
            } else {
              ref->mInitPromise.RejectIfExists(error, __func__);
            }
          }));
    } else {
      MediaResult error(NS_ERROR_DOM_MEDIA_DECODE_ERR);
      mDecodePromise.RejectIfExists(error, __func__);
      mDrainPromise.RejectIfExists(error, __func__);
      mFlushPromise.RejectIfExists(error, __func__);
      mShutdownPromise.ResolveIfExists(true, __func__);
      RefPtr<RemoteDecoderChild> kungFuDeathGrip = mShutdownSelfRef.forget();
      Unused << kungFuDeathGrip;
    }
  }
  mCanSend = false;
  mRawFramePool.Cleanup(this);
  RecordShutdownTelemetry(aWhy == AbnormalShutdown);
}

void RemoteDecoderChild::DestroyIPDL() {
  AssertOnManagerThread();
  if (mCanSend) {
    PRemoteDecoderChild::Send__delete__(this);
  }
}

void RemoteDecoderChild::IPDLActorDestroyed() { mIPDLSelfRef = nullptr; }

// MediaDataDecoder methods

RefPtr<MediaDataDecoder::InitPromise> RemoteDecoderChild::Init() {
  AssertOnManagerThread();

  if (!mIPDLSelfRef) {
    return MediaDataDecoder::InitPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }

  if (mCanSend) {
    SendInit();
  } else if (!mRecreatedOnCrash) {
    return MediaDataDecoder::InitPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }

  // If we failed to send this, then we'll still resolve the Init promise
  // as ActorDestroy handles it.

  return mInitPromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::DecodePromise> RemoteDecoderChild::Decode(
    MediaRawData* aSample) {
  AssertOnManagerThread();

  if (mNeedNewDecoder) {
    MediaResult error(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER);
    error.SetGPUCrashTimeStamp(mRemoteProcessCrashTime);
    return MediaDataDecoder::DecodePromise::CreateAndReject(error, __func__);
  }

  if (!mCanSend) {
    if (mRecreatedOnCrash) {
      // We're here if the IPC channel has died but we're still waiting for the
      // RunWhenRecreated task to complete. The decode promise will be rejected
      // when that task is run.
      return mDecodePromise.Ensure(__func__);
    } else {
      return MediaDataDecoder::DecodePromise::CreateAndReject(
          NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
    }
  }

  // TODO: It would be nice to add an allocator method to
  // MediaDataDecoder so that the demuxer could write directly
  // into shmem rather than requiring a copy here.
  ShmemBuffer buffer = mRawFramePool.Get(this, aSample->Size());
  if (!buffer.Valid()) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }

  memcpy(buffer.Get().get<uint8_t>(), aSample->Data(), aSample->Size());

  MediaRawDataIPDL sample(
      MediaDataIPDL(aSample->mOffset, aSample->mTime, aSample->mTimecode,
                    aSample->mDuration, aSample->mKeyframe),
      aSample->mEOS, aSample->mDiscardPadding, aSample->Size(),
      std::move(buffer.Get()));

  SendInput(sample);

  return mDecodePromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::FlushPromise> RemoteDecoderChild::Flush() {
  AssertOnManagerThread();
  mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  if (mNeedNewDecoder) {
    MediaResult error(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER);
    error.SetGPUCrashTimeStamp(mRemoteProcessCrashTime);
    return MediaDataDecoder::FlushPromise::CreateAndReject(error, __func__);
  }

  if (mCanSend) {
    SendFlush();
  } else if (!mRecreatedOnCrash) {
    return MediaDataDecoder::FlushPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }
  return mFlushPromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::DecodePromise> RemoteDecoderChild::Drain() {
  AssertOnManagerThread();
  if (mNeedNewDecoder) {
    MediaResult error(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER);
    error.SetGPUCrashTimeStamp(mRemoteProcessCrashTime);
    return MediaDataDecoder::DecodePromise::CreateAndReject(error, __func__);
  }

  if (mCanSend) {
    SendDrain();
  } else if (!mRecreatedOnCrash) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }
  return mDrainPromise.Ensure(__func__);
}

RefPtr<ShutdownPromise> RemoteDecoderChild::Shutdown() {
  AssertOnManagerThread();
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mInitialized = false;
  if (mNeedNewDecoder) {
    MediaResult error(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER);
    error.SetGPUCrashTimeStamp(mRemoteProcessCrashTime);
    return ShutdownPromise::CreateAndResolve(true, __func__);
  }
  if (!mCanSend) {
    return ShutdownPromise::CreateAndResolve(true, __func__);
  }
  SendShutdown();
  MOZ_ASSERT(!mShutdownSelfRef);
  mShutdownSelfRef = this;
  return mShutdownPromise.Ensure(__func__);
}

bool RemoteDecoderChild::IsHardwareAccelerated(
    nsACString& aFailureReason) const {
  AssertOnManagerThread();
  aFailureReason = mHardwareAcceleratedReason;
  return mIsHardwareAccelerated;
}

nsCString RemoteDecoderChild::GetDescriptionName() const {
  AssertOnManagerThread();
  return mDescription;
}

void RemoteDecoderChild::SetSeekThreshold(const media::TimeUnit& aTime) {
  AssertOnManagerThread();
  if (mCanSend) {
    SendSetSeekThreshold(aTime);
  }
}

MediaDataDecoder::ConversionRequired RemoteDecoderChild::NeedsConversion()
    const {
  AssertOnManagerThread();
  return mConversion;
}

void RemoteDecoderChild::AssertOnManagerThread() const {
  MOZ_ASSERT(NS_GetCurrentThread() == mThread);
}

RemoteDecoderManagerChild* RemoteDecoderChild::GetManager() {
  if (!mCanSend) {
    return nullptr;
  }
  return static_cast<RemoteDecoderManagerChild*>(Manager());
}

}  // namespace mozilla
