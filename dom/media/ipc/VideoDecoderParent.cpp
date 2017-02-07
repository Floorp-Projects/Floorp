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
  virtual ~KnowsCompositorVideo() = default;
};

VideoDecoderParent::VideoDecoderParent(VideoDecoderManagerParent* aParent,
                                       const VideoInfo& aVideoInfo,
                                       const layers::TextureFactoryIdentifier& aIdentifier,
                                       TaskQueue* aManagerTaskQueue,
                                       TaskQueue* aDecodeTaskQueue,
                                       bool* aSuccess)
  : mParent(aParent)
  , mManagerTaskQueue(aManagerTaskQueue)
  , mDecodeTaskQueue(aDecodeTaskQueue)
  , mKnowsCompositor(new KnowsCompositorVideo)
  , mDestroyed(false)
{
  MOZ_COUNT_CTOR(VideoDecoderParent);
  MOZ_ASSERT(OnManagerThread());
  // We hold a reference to ourselves to keep us alive until IPDL
  // explictly destroys us. There may still be refs held by
  // tasks, but no new ones should be added after we're
  // destroyed.
  mIPDLSelfRef = this;

  mKnowsCompositor->IdentifyTextureHost(aIdentifier);

#ifdef XP_WIN
  // TODO: Ideally we wouldn't hardcode the WMF PDM, and we'd use the normal PDM
  // factory logic for picking a decoder.
  WMFDecoderModule::Init();
  RefPtr<WMFDecoderModule> pdm(new WMFDecoderModule());
  pdm->Startup();

  CreateDecoderParams params(aVideoInfo);
  params.mTaskQueue = mDecodeTaskQueue;
  params.mKnowsCompositor = mKnowsCompositor;
  params.mImageContainer = new layers::ImageContainer();

  mDecoder = pdm->CreateVideoDecoder(params);
#else
  MOZ_ASSERT(false, "Can't use RemoteVideoDecoder on non-Windows platforms yet");
#endif

  *aSuccess = !!mDecoder;
}

VideoDecoderParent::~VideoDecoderParent()
{
  MOZ_COUNT_DTOR(VideoDecoderParent);
}

void
VideoDecoderParent::Destroy()
{
  MOZ_ASSERT(OnManagerThread());
  mDecodeTaskQueue->AwaitShutdownAndIdle();
  mDestroyed = true;
  mIPDLSelfRef = nullptr;
}

mozilla::ipc::IPCResult
VideoDecoderParent::RecvInit()
{
  MOZ_ASSERT(OnManagerThread());
  RefPtr<VideoDecoderParent> self = this;
  mDecoder->Init()->Then(mManagerTaskQueue, __func__,
    [self] (TrackInfo::TrackType aTrack) {
      if (self->mDecoder) {
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
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderParent::RecvInput(const MediaRawDataIPDL& aData)
{
  MOZ_ASSERT(OnManagerThread());
  // XXX: This copies the data into a buffer owned by the MediaRawData. Ideally we'd just take ownership
  // of the shmem.
  RefPtr<MediaRawData> data = new MediaRawData(aData.buffer().get<uint8_t>(),
                                               aData.buffer().Size<uint8_t>());
  if (!data->Data()) {
    // OOM
    Error(NS_ERROR_OUT_OF_MEMORY);
    return IPC_OK();
  }
  data->mOffset = aData.base().offset();
  data->mTime = aData.base().time();
  data->mTimecode = aData.base().timecode();
  data->mDuration = aData.base().duration();
  data->mKeyframe = aData.base().keyframe();

  DeallocShmem(aData.buffer());

  RefPtr<VideoDecoderParent> self = this;
  mDecoder->Decode(data)->Then(
    mManagerTaskQueue, __func__,
    [self, this](const MediaDataDecoder::DecodedData& aResults) {
      if (mDestroyed) {
        return;
      }
      ProcessDecodedData(aResults);
      Unused << SendInputExhausted();
    },
    [self, this](const MediaResult& aError) { Error(aError); });
  return IPC_OK();
}

void
VideoDecoderParent::ProcessDecodedData(
  const MediaDataDecoder::DecodedData& aData)
{
  MOZ_ASSERT(OnManagerThread());

  for (const auto& data : aData) {
    MOZ_ASSERT(data->mType == MediaData::VIDEO_DATA,
                "Can only decode videos using VideoDecoderParent!");
    VideoData* video = static_cast<VideoData*>(data.get());

    MOZ_ASSERT(video->mImage, "Decoded video must output a layer::Image to "
                              "be used with VideoDecoderParent");

    RefPtr<TextureClient> texture =
      video->mImage->GetTextureClient(mKnowsCompositor);

    if (!texture) {
      texture = ImageClient::CreateTextureClientForImage(video->mImage,
                                                          mKnowsCompositor);
    }

    if (texture && !texture->IsAddedToCompositableClient()) {
      texture->InitIPDLActor(mKnowsCompositor);
      texture->SetAddedToCompositableClient();
    }

    VideoDataIPDL output(
      MediaDataIPDL(data->mOffset, data->mTime, data->mTimecode,
                    data->mDuration, data->mFrames, data->mKeyframe),
      video->mDisplay,
      texture ? mParent->StoreImage(video->mImage, texture)
              : SurfaceDescriptorGPUVideo(0),
      video->mFrameID);
    Unused << SendOutput(output);
  }
}

mozilla::ipc::IPCResult
VideoDecoderParent::RecvFlush()
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(OnManagerThread());
  RefPtr<VideoDecoderParent> self = this;
  mDecoder->Flush()->Then(
    mManagerTaskQueue, __func__,
    [self, this]() {
      if (!mDestroyed) {
        Unused << SendFlushComplete();
      }
    },
    [self, this](const MediaResult& aError) { Error(aError); });

  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderParent::RecvDrain()
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(OnManagerThread());
  RefPtr<VideoDecoderParent> self = this;
  mDecoder->Drain()->Then(
    mManagerTaskQueue, __func__,
    [self, this](const MediaDataDecoder::DecodedData& aResults) {
      if (!mDestroyed) {
        ProcessDecodedData(aResults);
        Unused << SendDrainComplete();
      }
    },
    [self, this](const MediaResult& aError) { Error(aError); });
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderParent::RecvShutdown()
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(OnManagerThread());
  if (mDecoder) {
    mDecoder->Shutdown();
  }
  mDecoder = nullptr;
  return IPC_OK();
}

mozilla::ipc::IPCResult
VideoDecoderParent::RecvSetSeekThreshold(const int64_t& aTime)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(OnManagerThread());
  mDecoder->SetSeekThreshold(media::TimeUnit::FromMicroseconds(aTime));
  return IPC_OK();
}

void
VideoDecoderParent::ActorDestroy(ActorDestroyReason aWhy)
{
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(OnManagerThread());
  if (mDecoder) {
    mDecoder->Shutdown();
    mDecoder = nullptr;
  }
  if (mDecodeTaskQueue) {
    mDecodeTaskQueue->BeginShutdown();
  }
}

void
VideoDecoderParent::Error(const MediaResult& aError)
{
  MOZ_ASSERT(OnManagerThread());
  if (!mDestroyed) {
    Unused << SendError(aError);
  }
}

bool
VideoDecoderParent::OnManagerThread()
{
  return mParent->OnManagerThread();
}

} // namespace dom
} // namespace mozilla
