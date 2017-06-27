/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VideoDecoderChild.h"
#include "VideoDecoderManagerChild.h"
#include "mozilla/layers/TextureClient.h"
#include "base/thread.h"
#include "MediaInfo.h"
#include "ImageContainer.h"
#include "GPUVideoImage.h"

namespace mozilla {
namespace dom {

using base::Thread;
using namespace ipc;
using namespace layers;
using namespace gfx;

VideoDecoderChild::VideoDecoderChild()
  : mThread(VideoDecoderManagerChild::GetManagerThread())
  , mCanSend(false)
  , mInitialized(false)
  , mIsHardwareAccelerated(false)
  , mConversion(MediaDataDecoder::ConversionRequired::kNeedNone)
  , mNeedNewDecoder(false)
{
}

VideoDecoderChild::~VideoDecoderChild()
{
  AssertOnManagerThread();
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
}

mozilla::ipc::IPCResult
VideoDecoderChild::RecvOutput(const VideoDataIPDL& aData)
{
  AssertOnManagerThread();

  // The Image here creates a TextureData object that takes ownership
  // of the SurfaceDescriptor, and is responsible for making sure that
  // it gets deallocated.
  RefPtr<Image> image = new GPUVideoImage(GetManager(), aData.sd(), aData.frameSize());

  RefPtr<VideoData> video = VideoData::CreateFromImage(
    aData.display(),
    aData.base().offset(),
    media::TimeUnit::FromMicroseconds(aData.base().time()),
    media::TimeUnit::FromMicroseconds(aData.base().duration()),
    image,
    aData.base().keyframe(),
    media::TimeUnit::FromMicroseconds(aData.base().timecode()));

  mDecodedData.AppendElement(Move(video));
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderChild::RecvInputExhausted()
{
  AssertOnManagerThread();
  mDecodePromise.ResolveIfExists(mDecodedData, __func__);
  mDecodedData.Clear();
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderChild::RecvDrainComplete()
{
  AssertOnManagerThread();
  mDrainPromise.ResolveIfExists(mDecodedData, __func__);
  mDecodedData.Clear();
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderChild::RecvError(const nsresult& aError)
{
  AssertOnManagerThread();
  mDecodedData.Clear();
  mDecodePromise.RejectIfExists(aError, __func__);
  mDrainPromise.RejectIfExists(aError, __func__);
  mFlushPromise.RejectIfExists(aError, __func__);
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderChild::RecvInitComplete(const bool& aHardware,
                                    const nsCString& aHardwareReason,
                                    const uint32_t& aConversion)
{
  AssertOnManagerThread();
  mInitPromise.ResolveIfExists(TrackInfo::kVideoTrack, __func__);
  mInitialized = true;
  mIsHardwareAccelerated = aHardware;
  mHardwareAcceleratedReason = aHardwareReason;
  mConversion = static_cast<MediaDataDecoder::ConversionRequired>(aConversion);
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderChild::RecvInitFailed(const nsresult& aReason)
{
  AssertOnManagerThread();
  mInitPromise.RejectIfExists(aReason, __func__);
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderChild::RecvFlushComplete()
{
  AssertOnManagerThread();
  mFlushPromise.ResolveIfExists(true, __func__);
  return IPC_OK();
}

void
VideoDecoderChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (aWhy == AbnormalShutdown) {
    // Defer reporting an error until we've recreated the manager so that
    // it'll be safe for MediaFormatReader to recreate decoders
    RefPtr<VideoDecoderChild> ref = this;
    GetManager()->RunWhenRecreated(
      NS_NewRunnableFunction("dom::VideoDecoderChild::ActorDestroy", [=]() {
        if (ref->mInitialized) {
          mDecodedData.Clear();
          mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER,
                                        __func__);
          mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER,
                                       __func__);
          mFlushPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER,
                                       __func__);
          // Make sure the next request will be rejected accordingly if ever
          // called.
          mNeedNewDecoder = true;
        } else {
          ref->mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER,
                                           __func__);
        }
      }));
  }
  mCanSend = false;
}

bool
VideoDecoderChild::InitIPDL(const VideoInfo& aVideoInfo,
                            const layers::TextureFactoryIdentifier& aIdentifier)
{
  RefPtr<VideoDecoderManagerChild> manager =
    VideoDecoderManagerChild::GetSingleton();
  // If the manager isn't available, then don't initialize mIPDLSelfRef and
  // leave us in an error state. We'll then immediately reject the promise when
  // Init() is called and the caller can try again. Hopefully by then the new
  // manager is ready, or we've notified the caller of it being no longer
  // available. If not, then the cycle repeats until we're ready.
  if (!manager || !manager->CanSend()) {
    return true;
  }

  mIPDLSelfRef = this;
  bool success = false;
  if (manager->SendPVideoDecoderConstructor(this, aVideoInfo, aIdentifier,
                                            &success)) {
    mCanSend = true;
  }
  return success;
}

void
VideoDecoderChild::DestroyIPDL()
{
  if (mCanSend) {
    PVideoDecoderChild::Send__delete__(this);
  }
}

void
VideoDecoderChild::IPDLActorDestroyed()
{
  mIPDLSelfRef = nullptr;
}

// MediaDataDecoder methods

RefPtr<MediaDataDecoder::InitPromise>
VideoDecoderChild::Init()
{
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

RefPtr<MediaDataDecoder::DecodePromise>
VideoDecoderChild::Decode(MediaRawData* aSample)
{
  AssertOnManagerThread();

  if (mNeedNewDecoder) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
      NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER, __func__);
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

  MediaRawDataIPDL sample(MediaDataIPDL(aSample->mOffset,
                                        aSample->mTime.ToMicroseconds(),
                                        aSample->mTimecode.ToMicroseconds(),
                                        aSample->mDuration.ToMicroseconds(),
                                        aSample->mFrames,
                                        aSample->mKeyframe),
                          buffer);
  SendInput(sample);
  return mDecodePromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::FlushPromise>
VideoDecoderChild::Flush()
{
  AssertOnManagerThread();
  mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  if (mNeedNewDecoder) {
    return MediaDataDecoder::FlushPromise::CreateAndReject(
      NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER, __func__);
  }
  if (mCanSend) {
    SendFlush();
  }
  return mFlushPromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::DecodePromise>
VideoDecoderChild::Drain()
{
  AssertOnManagerThread();
  if (mNeedNewDecoder) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
      NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER, __func__);
  }
  if (mCanSend) {
    SendDrain();
  }
  return mDrainPromise.Ensure(__func__);
}

void
VideoDecoderChild::Shutdown()
{
  AssertOnManagerThread();
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  if (mCanSend) {
    SendShutdown();
  }
  mInitialized = false;
}

bool
VideoDecoderChild::IsHardwareAccelerated(nsACString& aFailureReason) const
{
  aFailureReason = mHardwareAcceleratedReason;
  return mIsHardwareAccelerated;
}

void
VideoDecoderChild::SetSeekThreshold(const media::TimeUnit& aTime)
{
  AssertOnManagerThread();
  if (mCanSend) {
    SendSetSeekThreshold(aTime.ToMicroseconds());
  }
}

MediaDataDecoder::ConversionRequired
VideoDecoderChild::NeedsConversion() const
{
  return mConversion;
}

void
VideoDecoderChild::AssertOnManagerThread() const
{
  MOZ_ASSERT(NS_GetCurrentThread() == mThread);
}

VideoDecoderManagerChild*
VideoDecoderChild::GetManager()
{
  if (!mCanSend) {
    return nullptr;
  }
  return static_cast<VideoDecoderManagerChild*>(Manager());
}

} // namespace dom
} // namespace mozilla
