/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteVideoDecoder.h"
#include "VideoDecoderChild.h"
#include "VideoDecoderManagerChild.h"
#include "mozilla/layers/TextureClient.h"
#include "base/thread.h"
#include "MediaInfo.h"
#include "MediaPrefs.h"
#include "ImageContainer.h"
#include "mozilla/layers/SynchronousTask.h"

namespace mozilla {
namespace dom {

using base::Thread;
using namespace ipc;
using namespace layers;
using namespace gfx;

RemoteVideoDecoder::RemoteVideoDecoder()
  : mActor(new VideoDecoderChild())
{
}

RemoteVideoDecoder::~RemoteVideoDecoder()
{
  // We're about to be destroyed and drop our ref to
  // VideoDecoderChild. Make sure we put a ref into the
  // task queue for the VideoDecoderChild thread to keep
  // it alive until we send the delete message.
  RefPtr<VideoDecoderChild> actor = mActor;

  RefPtr<Runnable> task = NS_NewRunnableFunction([actor]() {
    MOZ_ASSERT(actor);
    actor->DestroyIPDL();
  });

  // Drop out references to the actor so that the last ref
  // always gets released on the manager thread.
  actor = nullptr;
  mActor = nullptr;

  VideoDecoderManagerChild::GetManagerThread()->Dispatch(task.forget(), NS_DISPATCH_NORMAL);
}

RefPtr<MediaDataDecoder::InitPromise>
RemoteVideoDecoder::Init()
{
  RefPtr<RemoteVideoDecoder> self = this;
  return InvokeAsync(VideoDecoderManagerChild::GetManagerAbstractThread(),
                     __func__, [self, this]() { return mActor->Init(); });
}

RefPtr<MediaDataDecoder::DecodePromise>
RemoteVideoDecoder::Decode(MediaRawData* aSample)
{
  RefPtr<RemoteVideoDecoder> self = this;
  RefPtr<MediaRawData> sample = aSample;
  return InvokeAsync(VideoDecoderManagerChild::GetManagerAbstractThread(),
                     __func__,
                     [self, this, sample]() { return mActor->Decode(sample); });
}

RefPtr<MediaDataDecoder::FlushPromise>
RemoteVideoDecoder::Flush()
{
  RefPtr<RemoteVideoDecoder> self = this;
  return InvokeAsync(VideoDecoderManagerChild::GetManagerAbstractThread(),
                     __func__, [self, this]() { return mActor->Flush(); });
}

RefPtr<MediaDataDecoder::DecodePromise>
RemoteVideoDecoder::Drain()
{
  RefPtr<RemoteVideoDecoder> self = this;
  return InvokeAsync(VideoDecoderManagerChild::GetManagerAbstractThread(),
                     __func__, [self, this]() { return mActor->Drain(); });
}

RefPtr<ShutdownPromise>
RemoteVideoDecoder::Shutdown()
{
  RefPtr<RemoteVideoDecoder> self = this;
  return InvokeAsync(VideoDecoderManagerChild::GetManagerAbstractThread(),
                     __func__, [self, this]() {
                       mActor->Shutdown();
                       return ShutdownPromise::CreateAndResolve(true, __func__);
                     });
}

bool
RemoteVideoDecoder::IsHardwareAccelerated(nsACString& aFailureReason) const
{
  return mActor->IsHardwareAccelerated(aFailureReason);
}

void
RemoteVideoDecoder::SetSeekThreshold(const media::TimeUnit& aTime)
{
  RefPtr<RemoteVideoDecoder> self = this;
  media::TimeUnit time = aTime;
  VideoDecoderManagerChild::GetManagerThread()->Dispatch(NS_NewRunnableFunction([=]() {
    MOZ_ASSERT(self->mActor);
    self->mActor->SetSeekThreshold(time);
  }), NS_DISPATCH_NORMAL);
}

MediaDataDecoder::ConversionRequired
RemoteVideoDecoder::NeedsConversion() const
{
  return mActor->NeedsConversion();
}

nsresult
RemoteDecoderModule::Startup()
{
  if (!VideoDecoderManagerChild::GetManagerThread()) {
    return NS_ERROR_FAILURE;
  }
  return mWrapped->Startup();
}

bool
RemoteDecoderModule::SupportsMimeType(const nsACString& aMimeType,
                                      DecoderDoctorDiagnostics* aDiagnostics) const
{
  return mWrapped->SupportsMimeType(aMimeType, aDiagnostics);
}

bool
RemoteDecoderModule::Supports(const TrackInfo& aTrackInfo,
                              DecoderDoctorDiagnostics* aDiagnostics) const
{
  return mWrapped->Supports(aTrackInfo, aDiagnostics);
}

static inline bool
IsRemoteAcceleratedCompositor(KnowsCompositor* aKnows)
{
  TextureFactoryIdentifier ident = aKnows->GetTextureFactoryIdentifier();
  return ident.mParentBackend != LayersBackend::LAYERS_BASIC &&
         ident.mParentProcessType == GeckoProcessType_GPU;
}

already_AddRefed<MediaDataDecoder>
RemoteDecoderModule::CreateVideoDecoder(const CreateDecoderParams& aParams)
{
  if (!MediaPrefs::PDMUseGPUDecoder() ||
      !aParams.mKnowsCompositor ||
      !IsRemoteAcceleratedCompositor(aParams.mKnowsCompositor))
  {
    return mWrapped->CreateVideoDecoder(aParams);
  }

  RefPtr<RemoteVideoDecoder> object = new RemoteVideoDecoder();

  SynchronousTask task("InitIPDL");
  bool success;
  VideoDecoderManagerChild::GetManagerThread()->Dispatch(NS_NewRunnableFunction([&]() {
    AutoCompleteTask complete(&task);
    success = object->mActor->InitIPDL(aParams.VideoConfig(),
                                       aParams.mKnowsCompositor->GetTextureFactoryIdentifier());
  }), NS_DISPATCH_NORMAL);
  task.Wait();

  if (!success) {
    return nullptr;
  }

  return object.forget();
}

} // namespace dom
} // namespace mozilla
