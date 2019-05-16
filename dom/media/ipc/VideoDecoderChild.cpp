/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VideoDecoderChild.h"
#include "GPUVideoImage.h"
#include "ImageContainer.h"
#include "MediaInfo.h"
#include "VideoDecoderManagerChild.h"
#include "base/thread.h"
#include "mozilla/Telemetry.h"
#include "mozilla/layers/TextureClient.h"

namespace mozilla {

using base::Thread;
using namespace ipc;
using namespace layers;
using namespace gfx;

#ifdef XP_WIN
static void ReportUnblacklistingTelemetry(
    bool isGPUProcessCrashed, const nsCString& aD3D11BlacklistedDriver,
    const nsCString& aD3D9BlacklistedDriver) {
  const nsCString& blacklistedDLL = !aD3D11BlacklistedDriver.IsEmpty()
                                        ? aD3D11BlacklistedDriver
                                        : aD3D9BlacklistedDriver;

  if (!blacklistedDLL.IsEmpty()) {
    Telemetry::Accumulate(
        Telemetry::VIDEO_UNBLACKINGLISTING_DXVA_DRIVER_RUNTIME_STATUS,
        blacklistedDLL, isGPUProcessCrashed ? 1 : 0);
  }
}
#endif  // XP_WIN

VideoDecoderChild::VideoDecoderChild()
    : mThread(VideoDecoderManagerChild::GetManagerThread()),
      mCanSend(false),
      mInitialized(false),
      mIsHardwareAccelerated(false),
      mConversion(MediaDataDecoder::ConversionRequired::kNeedNone),
      mNeedNewDecoder(false) {}

VideoDecoderChild::~VideoDecoderChild() {
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
}

mozilla::ipc::IPCResult VideoDecoderChild::RecvOutput(
    const VideoDataIPDL& aData) {
  AssertOnManagerThread();

  // The Image here creates a TextureData object that takes ownership
  // of the SurfaceDescriptor, and is responsible for making sure that
  // it gets deallocated.
  RefPtr<Image> image =
      new GPUVideoImage(GetManager(), aData.sd(), aData.frameSize());

  RefPtr<VideoData> video = VideoData::CreateFromImage(
      aData.display(), aData.base().offset(), aData.base().time(),
      aData.base().duration(), image, aData.base().keyframe(),
      aData.base().timecode());

  mDecodedData.AppendElement(std::move(video));
  return IPC_OK();
}

mozilla::ipc::IPCResult VideoDecoderChild::RecvInputExhausted() {
  AssertOnManagerThread();
  mDecodePromise.ResolveIfExists(std::move(mDecodedData), __func__);
  mDecodedData = MediaDataDecoder::DecodedData();
  return IPC_OK();
}

mozilla::ipc::IPCResult VideoDecoderChild::RecvDrainComplete() {
  AssertOnManagerThread();
  mDrainPromise.ResolveIfExists(std::move(mDecodedData), __func__);
  mDecodedData = MediaDataDecoder::DecodedData();
  return IPC_OK();
}

mozilla::ipc::IPCResult VideoDecoderChild::RecvError(const nsresult& aError) {
  AssertOnManagerThread();
  mDecodedData = MediaDataDecoder::DecodedData();
  mDecodePromise.RejectIfExists(aError, __func__);
  mDrainPromise.RejectIfExists(aError, __func__);
  mFlushPromise.RejectIfExists(aError, __func__);
  mShutdownPromise.ResolveIfExists(true, __func__);
  RefPtr<VideoDecoderChild> kungFuDeathGrip = mShutdownSelfRef.forget();
  Unused << kungFuDeathGrip;
  return IPC_OK();
}

mozilla::ipc::IPCResult VideoDecoderChild::RecvInitComplete(
    const nsCString& aDecoderDescription, const bool& aHardware,
    const nsCString& aHardwareReason, const uint32_t& aConversion) {
  AssertOnManagerThread();
  mInitPromise.ResolveIfExists(TrackInfo::kVideoTrack, __func__);
  mInitialized = true;
  mDescription = aDecoderDescription;
  mIsHardwareAccelerated = aHardware;
  mHardwareAcceleratedReason = aHardwareReason;
  mConversion = static_cast<MediaDataDecoder::ConversionRequired>(aConversion);
  return IPC_OK();
}

mozilla::ipc::IPCResult VideoDecoderChild::RecvInitFailed(
    const nsresult& aReason) {
  AssertOnManagerThread();
  mInitPromise.RejectIfExists(aReason, __func__);
  return IPC_OK();
}

mozilla::ipc::IPCResult VideoDecoderChild::RecvFlushComplete() {
  AssertOnManagerThread();
  mFlushPromise.ResolveIfExists(true, __func__);
  return IPC_OK();
}

mozilla::ipc::IPCResult VideoDecoderChild::RecvShutdownComplete() {
  AssertOnManagerThread();
  MOZ_ASSERT(mShutdownSelfRef);
  mShutdownPromise.ResolveIfExists(true, __func__);
  RefPtr<VideoDecoderChild> kungFuDeathGrip = mShutdownSelfRef.forget();
  Unused << kungFuDeathGrip;
  return IPC_OK();
}

void VideoDecoderChild::ActorDestroy(ActorDestroyReason aWhy) {
  if (aWhy == AbnormalShutdown) {
    // GPU process crashed, record the time and send back to MFR for telemetry.
    mGPUCrashTime = TimeStamp::Now();

    // Defer reporting an error until we've recreated the manager so that
    // it'll be safe for MediaFormatReader to recreate decoders
    RefPtr<VideoDecoderChild> ref = this;
    // Make sure shutdown self reference is null. Since ref is captured by the
    // lambda it is not necessary to keep it any longer.
    mShutdownSelfRef = nullptr;
    GetManager()->RunWhenRecreated(
        NS_NewRunnableFunction("VideoDecoderChild::ActorDestroy", [=]() {
          MediaResult error(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER);
          error.SetGPUCrashTimeStamp(ref->mGPUCrashTime);
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
  }
  mCanSend = false;

#ifdef XP_WIN
  ReportUnblacklistingTelemetry(aWhy == AbnormalShutdown,
                                mBlacklistedD3D11Driver,
                                mBlacklistedD3D9Driver);
#endif  // XP_WIN
}

MediaResult VideoDecoderChild::InitIPDL(
    const VideoInfo& aVideoInfo, float aFramerate,
    const CreateDecoderParams::OptionSet& aOptions,
    const layers::TextureFactoryIdentifier& aIdentifier) {
  RefPtr<VideoDecoderManagerChild> manager =
      VideoDecoderManagerChild::GetSingleton();

  // The manager isn't available because VideoDecoderManagerChild has been
  // initialized with null end points and we don't want to decode video on GPU
  // process anymore. Return false here so that we can fallback to other PDMs.
  if (!manager) {
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("VideoDecoderManager is not available."));
  }

  // The manager doesn't support sending messages because we've just crashed
  // and are working on reinitialization. Don't initialize mIPDLSelfRef and
  // leave us in an error state. We'll then immediately reject the promise when
  // Init() is called and the caller can try again. Hopefully by then the new
  // manager is ready, or we've notified the caller of it being no longer
  // available. If not, then the cycle repeats until we're ready.
  if (!manager->CanSend()) {
    return NS_OK;
  }

  mIPDLSelfRef = this;
  bool success = false;
  nsCString errorDescription;
  if (manager->SendPVideoDecoderConstructor(
          this, aVideoInfo, aFramerate, aOptions, aIdentifier, &success,
          &mBlacklistedD3D11Driver, &mBlacklistedD3D9Driver,
          &errorDescription)) {
    mCanSend = true;
  }

  return success ? MediaResult(NS_OK)
                 : MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, errorDescription);
}

void VideoDecoderChild::DestroyIPDL() {
  AssertOnManagerThread();
  if (mCanSend) {
    PVideoDecoderChild::Send__delete__(this);
  }
}

void VideoDecoderChild::IPDLActorDestroyed() { mIPDLSelfRef = nullptr; }

// MediaDataDecoder methods

RefPtr<MediaDataDecoder::InitPromise> VideoDecoderChild::Init() {
  AssertOnManagerThread();

  if (!mIPDLSelfRef) {
    return MediaDataDecoder::InitPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }
  // If we failed to send this, then we'll still resolve the Init promise
  // as ActorDestroy handles it.
  if (mCanSend) {
    SendInit();
  }
  return mInitPromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::DecodePromise> VideoDecoderChild::Decode(
    MediaRawData* aSample) {
  AssertOnManagerThread();

  if (mNeedNewDecoder) {
    MediaResult error(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER);
    error.SetGPUCrashTimeStamp(mGPUCrashTime);
    return MediaDataDecoder::DecodePromise::CreateAndReject(error, __func__);
  }
  if (!mCanSend) {
    // We're here if the IPC channel has died but we're still waiting for the
    // RunWhenRecreated task to complete. The decode promise will be rejected
    // when that task is run.
    return mDecodePromise.Ensure(__func__);
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

RefPtr<MediaDataDecoder::FlushPromise> VideoDecoderChild::Flush() {
  AssertOnManagerThread();
  mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  if (mNeedNewDecoder) {
    MediaResult error(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER);
    error.SetGPUCrashTimeStamp(mGPUCrashTime);
    return MediaDataDecoder::FlushPromise::CreateAndReject(error, __func__);
  }
  if (mCanSend) {
    SendFlush();
  }
  return mFlushPromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::DecodePromise> VideoDecoderChild::Drain() {
  AssertOnManagerThread();
  if (mNeedNewDecoder) {
    MediaResult error(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER);
    error.SetGPUCrashTimeStamp(mGPUCrashTime);
    return MediaDataDecoder::DecodePromise::CreateAndReject(error, __func__);
  }
  if (mCanSend) {
    SendDrain();
  }
  return mDrainPromise.Ensure(__func__);
}

RefPtr<ShutdownPromise> VideoDecoderChild::Shutdown() {
  AssertOnManagerThread();
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mInitialized = false;
  if (mNeedNewDecoder) {
    MediaResult error(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER);
    error.SetGPUCrashTimeStamp(mGPUCrashTime);
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

bool VideoDecoderChild::IsHardwareAccelerated(
    nsACString& aFailureReason) const {
  AssertOnManagerThread();
  aFailureReason = mHardwareAcceleratedReason;
  return mIsHardwareAccelerated;
}

nsCString VideoDecoderChild::GetDescriptionName() const {
  AssertOnManagerThread();
  return mDescription;
}

void VideoDecoderChild::SetSeekThreshold(const media::TimeUnit& aTime) {
  AssertOnManagerThread();
  if (mCanSend) {
    SendSetSeekThreshold(aTime);
  }
}

MediaDataDecoder::ConversionRequired VideoDecoderChild::NeedsConversion()
    const {
  AssertOnManagerThread();
  return mConversion;
}

void VideoDecoderChild::AssertOnManagerThread() const {
  MOZ_ASSERT(NS_GetCurrentThread() == mThread);
}

VideoDecoderManagerChild* VideoDecoderChild::GetManager() {
  if (!mCanSend) {
    return nullptr;
  }
  return static_cast<VideoDecoderManagerChild*>(Manager());
}

}  // namespace mozilla
