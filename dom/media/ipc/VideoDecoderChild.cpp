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
{
}

VideoDecoderChild::~VideoDecoderChild()
{
  AssertOnManagerThread();
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
}

bool
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
  mCallback->Output(video);
  return true;
}

bool
VideoDecoderChild::RecvInputExhausted()
{
  AssertOnManagerThread();
  mCallback->InputExhausted();
  return true;
}

bool
VideoDecoderChild::RecvDrainComplete()
{
  AssertOnManagerThread();
  mCallback->DrainComplete();
  return true;
}

bool
VideoDecoderChild::RecvError(const nsresult& aError)
{
  AssertOnManagerThread();
  mCallback->Error(aError);
  return true;
}

bool
VideoDecoderChild::RecvInitComplete(const bool& aHardware, const nsCString& aHardwareReason)
{
  AssertOnManagerThread();
  mInitPromise.Resolve(TrackInfo::kVideoTrack, __func__);
  mInitialized = true;
  mIsHardwareAccelerated = aHardware;
  mHardwareAcceleratedReason = aHardwareReason;
  return true;
}

bool
VideoDecoderChild::RecvInitFailed(const nsresult& aReason)
{
  AssertOnManagerThread();
  mInitPromise.Reject(aReason, __func__);
  return true;
}

void
VideoDecoderChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (aWhy == AbnormalShutdown) {
    if (mInitialized) {
      mCallback->Error(NS_ERROR_DOM_MEDIA_FATAL_ERR);
    } else {
      mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    }
  }
  mCanSend = false;
}

void
VideoDecoderChild::InitIPDL(MediaDataDecoderCallback* aCallback,
                            const VideoInfo& aVideoInfo,
                            layers::KnowsCompositor* aKnowsCompositor)
{
  RefPtr<VideoDecoderManagerChild> manager = VideoDecoderManagerChild::GetSingleton();
  if (!manager) {
    return;
  }
  mIPDLSelfRef = this;
  mCallback = aCallback;
  mVideoInfo = aVideoInfo;
  mKnowsCompositor = aKnowsCompositor;
  if (manager->SendPVideoDecoderConstructor(this)) {
    mCanSend = true;
  }
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
  if (!mCanSend || !SendInit(mVideoInfo, mKnowsCompositor->GetTextureFactoryIdentifier())) {
    return MediaDataDecoder::InitPromise::CreateAndReject(
      NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
  }
  return mInitPromise.Ensure(__func__);
}

void
VideoDecoderChild::Input(MediaRawData* aSample)
{
  AssertOnManagerThread();
  if (!mCanSend) {
    mCallback->Error(NS_ERROR_DOM_MEDIA_FATAL_ERR);
    return;
  }

  // TODO: It would be nice to add an allocator method to
  // MediaDataDecoder so that the demuxer could write directly
  // into shmem rather than requiring a copy here.
  Shmem buffer;
  if (!AllocShmem(aSample->Size(), Shmem::SharedMemory::TYPE_BASIC, &buffer)) {
    mCallback->Error(NS_ERROR_DOM_MEDIA_FATAL_ERR);
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
  if (!SendInput(sample)) {
    mCallback->Error(NS_ERROR_DOM_MEDIA_FATAL_ERR);
  }
}

void
VideoDecoderChild::Flush()
{
  AssertOnManagerThread();
  if (!mCanSend || !SendFlush()) {
    mCallback->Error(NS_ERROR_DOM_MEDIA_FATAL_ERR);
  }
}

void
VideoDecoderChild::Drain()
{
  AssertOnManagerThread();
  if (!mCanSend || !SendDrain()) {
    mCallback->Error(NS_ERROR_DOM_MEDIA_FATAL_ERR);
  }
}

void
VideoDecoderChild::Shutdown()
{
  AssertOnManagerThread();
  if (!mCanSend || !SendShutdown()) {
    mCallback->Error(NS_ERROR_DOM_MEDIA_FATAL_ERR);
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
  if (!mCanSend || !SendSetSeekThreshold(aTime.ToMicroseconds())) {
    mCallback->Error(NS_ERROR_DOM_MEDIA_FATAL_ERR);
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
