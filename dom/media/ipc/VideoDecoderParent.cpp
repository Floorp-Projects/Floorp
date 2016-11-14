/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VideoDecoderParent.h"
#include "mozilla/Unused.h"
#include "mozilla/layers/CompositorThread.h"
#include "base/thread.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/VideoBridgeChild.h"
#include "mozilla/layers/ImageClient.h"
#include "MediaInfo.h"
#include "VideoDecoderManagerParent.h"
#ifdef XP_WIN
#include "WMFDecoderModule.h"
#endif

namespace mozilla {
namespace dom {

using base::Thread;
using namespace ipc;
using namespace layers;
using namespace gfx;

class KnowsCompositorVideo : public layers::KnowsCompositor
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(KnowsCompositorVideo, override)

  layers::TextureForwarder* GetTextureForwarder() override
  {
    return VideoBridgeChild::GetSingleton();
  }
  layers::LayersIPCActor* GetLayersIPCActor() override
  {
    return VideoBridgeChild::GetSingleton();
  }
private:
  virtual ~KnowsCompositorVideo() {}
};

VideoDecoderParent::VideoDecoderParent(VideoDecoderManagerParent* aParent,
                                       TaskQueue* aManagerTaskQueue,
                                       TaskQueue* aDecodeTaskQueue)
  : mParent(aParent)
  , mManagerTaskQueue(aManagerTaskQueue)
  , mDecodeTaskQueue(aDecodeTaskQueue)
  , mKnowsCompositor(new KnowsCompositorVideo)
  , mDestroyed(false)
{
  MOZ_COUNT_CTOR(VideoDecoderParent);
  // We hold a reference to ourselves to keep us alive until IPDL
  // explictly destroys us. There may still be refs held by
  // tasks, but no new ones should be added after we're
  // destroyed.
  mIPDLSelfRef = this;
}

VideoDecoderParent::~VideoDecoderParent()
{
  MOZ_COUNT_DTOR(VideoDecoderParent);
}

void
VideoDecoderParent::Destroy()
{
  mDecodeTaskQueue->AwaitShutdownAndIdle();
  mDestroyed = true;
  mIPDLSelfRef = nullptr;
}

bool
VideoDecoderParent::RecvInit(const VideoInfo& aInfo, const layers::TextureFactoryIdentifier& aIdentifier)
{
  mKnowsCompositor->IdentifyTextureHost(aIdentifier);

#ifdef XP_WIN
  // TODO: Ideally we wouldn't hardcode the WMF PDM, and we'd use the normal PDM
  // factory logic for picking a decoder.
  WMFDecoderModule::Init();
  RefPtr<WMFDecoderModule> pdm(new WMFDecoderModule());
  pdm->Startup();

  CreateDecoderParams params(aInfo);
  params.mTaskQueue = mDecodeTaskQueue;
  params.mCallback = this;
  params.mKnowsCompositor = mKnowsCompositor;
  params.mImageContainer = new layers::ImageContainer();

  mDecoder = pdm->CreateVideoDecoder(params);
  if (!mDecoder) {
    Unused << SendInitFailed(NS_ERROR_DOM_MEDIA_FATAL_ERR);
    return true;
  }
#else
  MOZ_ASSERT(false, "Can't use RemoteVideoDecoder on non-Windows platforms yet");
#endif

  RefPtr<VideoDecoderParent> self = this;
  mDecoder->Init()->Then(mManagerTaskQueue, __func__,
    [self] (TrackInfo::TrackType aTrack) {
      if (!self->mDestroyed) {
        nsCString hardwareReason;
        bool hardwareAccelerated = self->mDecoder->IsHardwareAccelerated(hardwareReason);
        Unused << self->SendInitComplete(hardwareAccelerated, hardwareReason);
      }
    },
    [self] (MediaResult aReason) {
      if (!self->mDestroyed) {
        Unused << self->SendInitFailed(aReason);
      }
    });
  return true;
}

bool
VideoDecoderParent::RecvInput(const MediaRawDataIPDL& aData)
{
  // XXX: This copies the data into a buffer owned by the MediaRawData. Ideally we'd just take ownership
  // of the shmem.
  RefPtr<MediaRawData> data = new MediaRawData(aData.buffer().get<uint8_t>(), aData.buffer().Size<uint8_t>());
  data->mOffset = aData.base().offset();
  data->mTime = aData.base().time();
  data->mTimecode = aData.base().timecode();
  data->mDuration = aData.base().duration();
  data->mKeyframe = aData.base().keyframe();

  DeallocShmem(aData.buffer());

  mDecoder->Input(data);
  return true;
}

bool
VideoDecoderParent::RecvFlush()
{
  MOZ_ASSERT(!mDestroyed);
  if (mDecoder) {
    mDecoder->Flush();
  }
  return true;
}

bool
VideoDecoderParent::RecvDrain()
{
  MOZ_ASSERT(!mDestroyed);
  mDecoder->Drain();
  return true;
}

bool
VideoDecoderParent::RecvShutdown()
{
  MOZ_ASSERT(!mDestroyed);
  if (mDecoder) {
    mDecoder->Shutdown();
  }
  mDecoder = nullptr;
  return true;
}

bool
VideoDecoderParent::RecvSetSeekThreshold(const int64_t& aTime)
{
  MOZ_ASSERT(!mDestroyed);
  mDecoder->SetSeekThreshold(media::TimeUnit::FromMicroseconds(aTime));
  return true;
}

void
VideoDecoderParent::ActorDestroy(ActorDestroyReason aWhy)
{
  MOZ_ASSERT(!mDestroyed);
  if (mDecoder) {
    mDecoder->Shutdown();
    mDecoder = nullptr;
  }
  if (mDecodeTaskQueue) {
    mDecodeTaskQueue->BeginShutdown();
  }
}

void
VideoDecoderParent::Output(MediaData* aData)
{
  MOZ_ASSERT(mDecodeTaskQueue->IsCurrentThreadIn());
  RefPtr<VideoDecoderParent> self = this;
  RefPtr<KnowsCompositor> knowsCompositor = mKnowsCompositor;
  RefPtr<MediaData> data = aData;
  mManagerTaskQueue->Dispatch(NS_NewRunnableFunction([self, knowsCompositor, data]() {
    if (self->mDestroyed) {
      return;
    }

    MOZ_ASSERT(data->mType == MediaData::VIDEO_DATA, "Can only decode videos using VideoDecoderParent!");
    VideoData* video = static_cast<VideoData*>(data.get());

    MOZ_ASSERT(video->mImage, "Decoded video must output a layer::Image to be used with VideoDecoderParent");

    RefPtr<TextureClient> texture = video->mImage->GetTextureClient(knowsCompositor);

    if (!texture) {
      texture = ImageClient::CreateTextureClientForImage(video->mImage, knowsCompositor);
    }

    if (texture && !texture->IsAddedToCompositableClient()) {
      texture->InitIPDLActor(knowsCompositor);
      texture->SetAddedToCompositableClient();
    }

    VideoDataIPDL output(MediaDataIPDL(data->mOffset,
                                       data->mTime,
                                       data->mTimecode,
                                       data->mDuration,
                                       data->mFrames,
                                       data->mKeyframe),
                         video->mDisplay,
                         texture ? self->mParent->StoreImage(video->mImage, texture) : SurfaceDescriptorGPUVideo(0),
                         video->mFrameID);
    Unused << self->SendOutput(output);
  }));
}

void
VideoDecoderParent::Error(const MediaResult& aError)
{
  MOZ_ASSERT(mDecodeTaskQueue->IsCurrentThreadIn());
  RefPtr<VideoDecoderParent> self = this;
  MediaResult error = aError;
  mManagerTaskQueue->Dispatch(NS_NewRunnableFunction([self, error]() {
    if (!self->mDestroyed) {
      Unused << self->SendError(error);
    }
  }));
}

void
VideoDecoderParent::InputExhausted()
{
  MOZ_ASSERT(mDecodeTaskQueue->IsCurrentThreadIn());
  RefPtr<VideoDecoderParent> self = this;
  mManagerTaskQueue->Dispatch(NS_NewRunnableFunction([self]() {
    if (!self->mDestroyed) {
      Unused << self->SendInputExhausted();
    }
  }));
}

void
VideoDecoderParent::DrainComplete()
{
  MOZ_ASSERT(mDecodeTaskQueue->IsCurrentThreadIn());
  RefPtr<VideoDecoderParent> self = this;
  mManagerTaskQueue->Dispatch(NS_NewRunnableFunction([self]() {
    if (!self->mDestroyed) {
      Unused << self->SendDrainComplete();
    }
  }));
}

bool
VideoDecoderParent::OnReaderTaskQueue()
{
  // Most of our calls into mDecoder come directly from IPDL so are on
  // the right thread, but not actually on the task queue. We only ever
  // run a single thread, not a pool, so this should work fine.
  return mParent->OnManagerThread();
}

} // namespace dom
} // namespace mozilla
