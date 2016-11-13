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

RemoteVideoDecoder::RemoteVideoDecoder(MediaDataDecoderCallback* aCallback)
  : mActor(new VideoDecoderChild())
{
#ifdef DEBUG
  mCallback = aCallback;
#endif
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
}

RemoteVideoDecoder::~RemoteVideoDecoder()
{
  // We're about to be destroyed and drop our ref to
  // VideoDecoderChild. Make sure we put a ref into the
  // task queue for the VideoDecoderChild thread to keep
  // it alive until we send the delete message.
  RefPtr<VideoDecoderChild> actor = mActor;
  VideoDecoderManagerChild::GetManagerThread()->Dispatch(NS_NewRunnableFunction([actor]() {
    MOZ_ASSERT(actor);
    actor->DestroyIPDL();
  }), NS_DISPATCH_NORMAL);
}

RefPtr<MediaDataDecoder::InitPromise>
RemoteVideoDecoder::Init()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  return InvokeAsync(VideoDecoderManagerChild::GetManagerAbstractThread(),
                     this, __func__, &RemoteVideoDecoder::InitInternal);
}

RefPtr<MediaDataDecoder::InitPromise>
RemoteVideoDecoder::InitInternal()
{
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(NS_GetCurrentThread() == VideoDecoderManagerChild::GetManagerThread());
  return mActor->Init();
}

void
RemoteVideoDecoder::Input(MediaRawData* aSample)
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  RefPtr<RemoteVideoDecoder> self = this;
  RefPtr<MediaRawData> sample = aSample;
  VideoDecoderManagerChild::GetManagerThread()->Dispatch(NS_NewRunnableFunction([self, sample]() {
    MOZ_ASSERT(self->mActor);
    self->mActor->Input(sample);
  }), NS_DISPATCH_NORMAL);
}

void
RemoteVideoDecoder::Flush()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  RefPtr<RemoteVideoDecoder> self = this;
  VideoDecoderManagerChild::GetManagerThread()->Dispatch(NS_NewRunnableFunction([self]() {
    MOZ_ASSERT(self->mActor);
    self->mActor->Flush();
  }), NS_DISPATCH_NORMAL);
}

void
RemoteVideoDecoder::Drain()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  RefPtr<RemoteVideoDecoder> self = this;
  VideoDecoderManagerChild::GetManagerThread()->Dispatch(NS_NewRunnableFunction([self]() {
    MOZ_ASSERT(self->mActor);
    self->mActor->Drain();
  }), NS_DISPATCH_NORMAL);
}

void
RemoteVideoDecoder::Shutdown()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  SynchronousTask task("Shutdown");
  RefPtr<RemoteVideoDecoder> self = this;
  VideoDecoderManagerChild::GetManagerThread()->Dispatch(NS_NewRunnableFunction([&]() {
    AutoCompleteTask complete(&task);
    MOZ_ASSERT(self->mActor);
    self->mActor->Shutdown();
  }), NS_DISPATCH_NORMAL);
  task.Wait();
}

bool
RemoteVideoDecoder::IsHardwareAccelerated(nsACString& aFailureReason) const
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  return mActor->IsHardwareAccelerated(aFailureReason);
}

void
RemoteVideoDecoder::SetSeekThreshold(const media::TimeUnit& aTime)
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  RefPtr<RemoteVideoDecoder> self = this;
  media::TimeUnit time = aTime;
  VideoDecoderManagerChild::GetManagerThread()->Dispatch(NS_NewRunnableFunction([=]() {
    MOZ_ASSERT(self->mActor);
    self->mActor->SetSeekThreshold(time);
  }), NS_DISPATCH_NORMAL);

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

PlatformDecoderModule::ConversionRequired
RemoteDecoderModule::DecoderNeedsConversion(const TrackInfo& aConfig) const
{
  return mWrapped->DecoderNeedsConversion(aConfig);
}

already_AddRefed<MediaDataDecoder>
RemoteDecoderModule::CreateVideoDecoder(const CreateDecoderParams& aParams)
{
  if (!MediaPrefs::PDMUseGPUDecoder() ||
      !aParams.mKnowsCompositor ||
      aParams.mKnowsCompositor->GetTextureFactoryIdentifier().mParentProcessType != GeckoProcessType_GPU) {
    return nullptr;
  }

  MediaDataDecoderCallback* callback = aParams.mCallback;
  MOZ_ASSERT(callback->OnReaderTaskQueue());
  RefPtr<RemoteVideoDecoder> object = new RemoteVideoDecoder(callback);

  VideoInfo info = aParams.VideoConfig();

  TextureFactoryIdentifier ident = aParams.mKnowsCompositor->GetTextureFactoryIdentifier();
  VideoDecoderManagerChild::GetManagerThread()->Dispatch(NS_NewRunnableFunction([=]() {
    object->mActor->InitIPDL(callback, info, ident);
  }), NS_DISPATCH_NORMAL);

  return object.forget();
}

} // namespace dom
} // namespace mozilla
