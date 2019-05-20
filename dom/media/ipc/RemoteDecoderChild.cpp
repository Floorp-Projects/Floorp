/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderChild.h"

#include "RemoteDecoderManagerChild.h"

namespace mozilla {

RemoteDecoderChild::RemoteDecoderChild()
    : mThread(RemoteDecoderManagerChild::GetManagerThread()) {}

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
    const TrackInfo::TrackType& trackType, const nsCString& aDecoderDescription,
    const ConversionRequired& aConversion) {
  AssertOnManagerThread();
  mInitPromise.ResolveIfExists(trackType, __func__);
  mInitialized = true;
  mDescription = aDecoderDescription;
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

void RemoteDecoderChild::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(mCanSend);
  // If the IPC channel is gone pending promises need to be resolved/rejected.
  if (aWhy == AbnormalShutdown) {
    MediaResult error(NS_ERROR_DOM_MEDIA_DECODE_ERR);
    mDecodePromise.RejectIfExists(error, __func__);
    mDrainPromise.RejectIfExists(error, __func__);
    mFlushPromise.RejectIfExists(error, __func__);
    mShutdownPromise.ResolveIfExists(true, __func__);
    RefPtr<RemoteDecoderChild> kungFuDeathGrip = mShutdownSelfRef.forget();
    Unused << kungFuDeathGrip;
  }
  mCanSend = false;
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

  if (!mIPDLSelfRef || !mCanSend) {
    return MediaDataDecoder::InitPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }

  SendInit();

  return mInitPromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::DecodePromise> RemoteDecoderChild::Decode(
    MediaRawData* aSample) {
  AssertOnManagerThread();

  if (!mCanSend) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }

  // TODO: It would be nice to add an allocator method to
  // MediaDataDecoder so that the demuxer could write directly
  // into shmem rather than requiring a copy here.
  Shmem buffer;
  if (!AllocShmem(aSample->Size(), Shmem::SharedMemory::TYPE_BASIC, &buffer)) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }

  memcpy(buffer.get<uint8_t>(), aSample->Data(), aSample->Size());

  MediaRawDataIPDL sample(
      MediaDataIPDL(aSample->mOffset, aSample->mTime, aSample->mTimecode,
                    aSample->mDuration, aSample->mKeyframe),
      aSample->mEOS, std::move(buffer));
  SendInput(sample);

  return mDecodePromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::FlushPromise> RemoteDecoderChild::Flush() {
  AssertOnManagerThread();
  mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  if (!mCanSend) {
    return MediaDataDecoder::FlushPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }
  SendFlush();
  return mFlushPromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::DecodePromise> RemoteDecoderChild::Drain() {
  AssertOnManagerThread();
  if (!mCanSend) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }
  SendDrain();
  return mDrainPromise.Ensure(__func__);
}

RefPtr<ShutdownPromise> RemoteDecoderChild::Shutdown() {
  AssertOnManagerThread();
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mInitialized = false;
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
