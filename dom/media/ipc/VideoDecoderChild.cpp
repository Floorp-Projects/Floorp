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
#include "mozilla/layers/SynchronousTask.h"

namespace mozilla {
namespace dom {

using base::Thread;
using namespace ipc;
using namespace layers;
using namespace gfx;

VideoDecoderChild::VideoDecoderChild()
  : mThread(VideoDecoderManagerChild::GetManagerThread())
  , mFlushTask(nullptr)
  , mCanSend(false)
  , mInitialized(false)
  , mIsHardwareAccelerated(false)
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
  VideoInfo info(aData.display().width, aData.display().height);

  // The Image here creates a TextureData object that takes ownership
  // of the SurfaceDescriptor, and is responsible for making sure that
  // it gets deallocated.
  RefPtr<Image> image = new GPUVideoImage(GetManager(), aData.sd(), aData.display());

  RefPtr<VideoData> video = VideoData::CreateFromImage(info,
                                                       aData.base().offset(),
                                                       aData.base().time(),
                                                       aData.base().duration(),
                                                       image,
                                                       aData.base().keyframe(),
                                                       aData.base().timecode(),
                                                       IntRect());
  if (mCallback) {
    mCallback->Output(video);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderChild::RecvInputExhausted()
{
  AssertOnManagerThread();
  if (mCallback) {
    mCallback->InputExhausted();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderChild::RecvDrainComplete()
{
  AssertOnManagerThread();
  if (mCallback) {
    mCallback->DrainComplete();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderChild::RecvError(const nsresult& aError)
{
  AssertOnManagerThread();
  if (mCallback) {
    mCallback->Error(aError);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderChild::RecvInitComplete(const bool& aHardware, const nsCString& aHardwareReason)
{
  AssertOnManagerThread();
  mInitPromise.Resolve(TrackInfo::kVideoTrack, __func__);
  mInitialized = true;
  mIsHardwareAccelerated = aHardware;
  mHardwareAcceleratedReason = aHardwareReason;
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderChild::RecvInitFailed(const nsresult& aReason)
{
  AssertOnManagerThread();
  mInitPromise.Reject(aReason, __func__);
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderChild::RecvFlushComplete()
{
  MOZ_ASSERT(mFlushTask);
  AutoCompleteTask complete(mFlushTask);
  mFlushTask = nullptr;
  return IPC_OK();
}

void
VideoDecoderChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (aWhy == AbnormalShutdown) {
    // Defer reporting an error until we've recreated the manager so that
    // it'll be safe for MediaFormatReader to recreate decoders
    RefPtr<VideoDecoderChild> ref = this;
    GetManager()->RunWhenRecreated(NS_NewRunnableFunction([=]() {
      if (ref->mInitialized && ref->mCallback) {
        ref->mCallback->Error(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER);
      } else {
        ref->mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER, __func__);
      }
    }));
  }
  if (mFlushTask) {
    AutoCompleteTask complete(mFlushTask);
    mFlushTask = nullptr;
  }
  mCanSend = false;
}

bool
VideoDecoderChild::InitIPDL(MediaDataDecoderCallback* aCallback,
                            const VideoInfo& aVideoInfo,
                            const layers::TextureFactoryIdentifier& aIdentifier)
{
  RefPtr<VideoDecoderManagerChild> manager = VideoDecoderManagerChild::GetSingleton();
  // If the manager isn't available, then don't initialize mIPDLSelfRef and leave
  // us in an error state. We'll then immediately reject the promise when Init()
  // is called and the caller can try again. Hopefully by then the new manager is
  // ready, or we've notified the caller of it being no longer available.
  // If not, then the cycle repeats until we're ready.
  if (!manager || !manager->CanSend()) {
    return true;
  }

  mIPDLSelfRef = this;
  mCallback = aCallback;
  bool success = false;
  if (manager->SendPVideoDecoderConstructor(this, aVideoInfo, aIdentifier, &success)) {
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

void
VideoDecoderChild::Input(MediaRawData* aSample)
{
  AssertOnManagerThread();
  if (!mCanSend) {
    return;
  }

  // TODO: It would be nice to add an allocator method to
  // MediaDataDecoder so that the demuxer could write directly
  // into shmem rather than requiring a copy here.
  Shmem buffer;
  if (!AllocShmem(aSample->Size(), Shmem::SharedMemory::TYPE_BASIC, &buffer)) {
    mCallback->Error(NS_ERROR_DOM_MEDIA_DECODE_ERR);
    return;
  }

  memcpy(buffer.get<uint8_t>(), aSample->Data(), aSample->Size());

  MediaRawDataIPDL sample(MediaDataIPDL(aSample->mOffset,
                                        aSample->mTime,
                                        aSample->mTimecode,
                                        aSample->mDuration,
                                        aSample->mFrames,
                                        aSample->mKeyframe),
                          buffer);
  SendInput(sample);
}

void
VideoDecoderChild::Flush(SynchronousTask* aTask)
{
  AssertOnManagerThread();
  if (mCanSend) {
    SendFlush();
    mFlushTask = aTask;
  } else {
    AutoCompleteTask complete(aTask);
  }
}

void
VideoDecoderChild::Drain()
{
  AssertOnManagerThread();
  if (mCanSend) {
    SendDrain();
  }
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
  mCallback = nullptr;
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

void
VideoDecoderChild::AssertOnManagerThread()
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
