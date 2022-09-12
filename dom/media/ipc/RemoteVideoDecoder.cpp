/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteVideoDecoder.h"

#include "mozilla/layers/ImageDataSerializer.h"

#ifdef MOZ_AV1
#  include "AOMDecoder.h"
#  include "DAV1DDecoder.h"
#endif
#ifdef XP_WIN
#  include "WMFDecoderModule.h"
#endif
#include "GPUVideoImage.h"
#include "ImageContainer.h"  // for PlanarYCbCrData and BufferRecycleBin
#include "MediaDataDecoderProxy.h"
#include "MediaInfo.h"
#include "PDMFactory.h"
#include "RemoteDecoderManagerParent.h"
#include "RemoteImageHolder.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/Telemetry.h"
#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/VideoBridgeChild.h"

namespace mozilla {

using namespace layers;  // for PlanarYCbCrData and BufferRecycleBin
using namespace ipc;
using namespace gfx;

layers::TextureForwarder* KnowsCompositorVideo::GetTextureForwarder() {
  auto* vbc = VideoBridgeChild::GetSingleton();
  return (vbc && vbc->CanSend()) ? vbc : nullptr;
}
layers::LayersIPCActor* KnowsCompositorVideo::GetLayersIPCActor() {
  return GetTextureForwarder();
}

/* static */ already_AddRefed<KnowsCompositorVideo>
KnowsCompositorVideo::TryCreateForIdentifier(
    const layers::TextureFactoryIdentifier& aIdentifier) {
  VideoBridgeChild* child = VideoBridgeChild::GetSingleton();
  if (!child) {
    return nullptr;
  }

  RefPtr<KnowsCompositorVideo> knowsCompositor = new KnowsCompositorVideo();
  knowsCompositor->IdentifyTextureHost(aIdentifier);
  return knowsCompositor.forget();
}

RemoteVideoDecoderChild::RemoteVideoDecoderChild(RemoteDecodeIn aLocation)
    : RemoteDecoderChild(aLocation), mBufferRecycleBin(new BufferRecycleBin) {}

MediaResult RemoteVideoDecoderChild::ProcessOutput(
    DecodedOutputIPDL&& aDecodedData) {
  AssertOnManagerThread();
  MOZ_ASSERT(aDecodedData.type() == DecodedOutputIPDL::TArrayOfRemoteVideoData);

  nsTArray<RemoteVideoData>& arrayData =
      aDecodedData.get_ArrayOfRemoteVideoData()->Array();

  for (auto&& data : arrayData) {
    if (data.image().IsEmpty()) {
      // This is a NullData object.
      mDecodedData.AppendElement(MakeRefPtr<NullData>(
          data.base().offset(), data.base().time(), data.base().duration()));
      continue;
    }
    RefPtr<Image> image = data.image().TransferToImage(mBufferRecycleBin);

    RefPtr<VideoData> video = VideoData::CreateFromImage(
        data.display(), data.base().offset(), data.base().time(),
        data.base().duration(), image, data.base().keyframe(),
        data.base().timecode());

    if (!video) {
      // OOM
      return MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__);
    }
    mDecodedData.AppendElement(std::move(video));
  }
  return NS_OK;
}

MediaResult RemoteVideoDecoderChild::InitIPDL(
    const VideoInfo& aVideoInfo, float aFramerate,
    const CreateDecoderParams::OptionSet& aOptions,
    Maybe<layers::TextureFactoryIdentifier> aIdentifier,
    const Maybe<uint64_t>& aMediaEngineId) {
  MOZ_ASSERT_IF(mLocation == RemoteDecodeIn::GpuProcess, aIdentifier);

  RefPtr<RemoteDecoderManagerChild> manager =
      RemoteDecoderManagerChild::GetSingleton(mLocation);

  // The manager isn't available because RemoteDecoderManagerChild has been
  // initialized with null end points and we don't want to decode video on RDD
  // process anymore. Return false here so that we can fallback to other PDMs.
  if (!manager) {
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("RemoteDecoderManager is not available."));
  }

  if (!manager->CanSend()) {
    if (mLocation == RemoteDecodeIn::GpuProcess) {
      // The manager doesn't support sending messages because we've just crashed
      // and are working on reinitialization. Don't initialize mIPDLSelfRef and
      // leave us in an error state. We'll then immediately reject the promise
      // when Init() is called and the caller can try again. Hopefully by then
      // the new manager is ready, or we've notified the caller of it being no
      // longer available. If not, then the cycle repeats until we're ready.
      return NS_OK;
    }

    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("RemoteDecoderManager unable to send."));
  }

  mIPDLSelfRef = this;
  VideoDecoderInfoIPDL decoderInfo(aVideoInfo, aFramerate);
  Unused << manager->SendPRemoteDecoderConstructor(this, decoderInfo, aOptions,
                                                   aIdentifier, aMediaEngineId);

  return NS_OK;
}

RemoteVideoDecoderParent::RemoteVideoDecoderParent(
    RemoteDecoderManagerParent* aParent, const VideoInfo& aVideoInfo,
    float aFramerate, const CreateDecoderParams::OptionSet& aOptions,
    const Maybe<layers::TextureFactoryIdentifier>& aIdentifier,
    nsISerialEventTarget* aManagerThread, TaskQueue* aDecodeTaskQueue,
    Maybe<uint64_t> aMediaEngineId)
    : RemoteDecoderParent(aParent, aOptions, aManagerThread, aDecodeTaskQueue,
                          aMediaEngineId),
      mVideoInfo(aVideoInfo),
      mFramerate(aFramerate) {
  if (aIdentifier) {
    // Check to see if we have a direct PVideoBridge connection to the
    // destination process specified in aIdentifier, and create a
    // KnowsCompositor representing that connection if so. If this fails, then
    // we fall back to returning the decoded frames directly via Output().
    mKnowsCompositor =
        KnowsCompositorVideo::TryCreateForIdentifier(*aIdentifier);
  }
}

IPCResult RemoteVideoDecoderParent::RecvConstruct(
    ConstructResolver&& aResolver) {
  auto imageContainer = MakeRefPtr<layers::ImageContainer>();
  if (mKnowsCompositor && XRE_IsRDDProcess()) {
    // Ensure to allocate recycle allocator
    imageContainer->EnsureRecycleAllocatorForRDD(mKnowsCompositor);
  }
  auto params = CreateDecoderParams{
      mVideoInfo,     mKnowsCompositor,
      imageContainer, CreateDecoderParams::VideoFrameRate(mFramerate),
      mOptions,       CreateDecoderParams::NoWrapper(true),
      mMediaEngineId,
  };

  mParent->EnsurePDMFactory().CreateDecoder(params)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [resolver = std::move(aResolver), self = RefPtr{this}](
          PlatformDecoderModule::CreateDecoderPromise::ResolveOrRejectValue&&
              aValue) {
        if (aValue.IsReject()) {
          resolver(aValue.RejectValue());
          return;
        }
        MOZ_ASSERT(aValue.ResolveValue());
        self->mDecoder =
            new MediaDataDecoderProxy(aValue.ResolveValue().forget(),
                                      do_AddRef(self->mDecodeTaskQueue.get()));
        resolver(NS_OK);
      });
  return IPC_OK();
}

MediaResult RemoteVideoDecoderParent::ProcessDecodedData(
    MediaDataDecoder::DecodedData&& aData, DecodedOutputIPDL& aDecodedData) {
  MOZ_ASSERT(OnManagerThread());

  // If the video decoder bridge has shut down, stop.
  if (mKnowsCompositor && !mKnowsCompositor->GetTextureForwarder()) {
    aDecodedData = MakeRefPtr<ArrayOfRemoteVideoData>();
    return NS_OK;
  }

  nsTArray<RemoteVideoData> array;

  for (const auto& data : aData) {
    MOZ_ASSERT(data->mType == MediaData::Type::VIDEO_DATA ||
                   data->mType == MediaData::Type::NULL_DATA,
               "Can only decode videos using RemoteDecoderParent!");
    if (data->mType == MediaData::Type::NULL_DATA) {
      RemoteVideoData output(
          MediaDataIPDL(data->mOffset, data->mTime, data->mTimecode,
                        data->mDuration, data->mKeyframe),
          IntSize(), RemoteImageHolder(), -1);

      array.AppendElement(std::move(output));
      continue;
    }
    VideoData* video = static_cast<VideoData*>(data.get());

    MOZ_ASSERT(video->mImage,
               "Decoded video must output a layer::Image to "
               "be used with RemoteDecoderParent");

    RefPtr<TextureClient> texture;
    SurfaceDescriptor sd;
    IntSize size;
    bool needStorage = false;

    if (mKnowsCompositor) {
      texture = video->mImage->GetTextureClient(mKnowsCompositor);

      if (!texture) {
        texture = ImageClient::CreateTextureClientForImage(video->mImage,
                                                           mKnowsCompositor);
      }

      if (texture) {
        if (!texture->IsAddedToCompositableClient()) {
          texture->InitIPDLActor(mKnowsCompositor);
          texture->SetAddedToCompositableClient();
        }
        needStorage = true;
        SurfaceDescriptorRemoteDecoder remoteSD;
        texture->GetSurfaceDescriptorRemoteDecoder(&remoteSD);
        sd = remoteSD;
        size = texture->GetSize();
      }
    }

    // If failed to create a GPU accelerated surface descriptor, fall back to
    // copying frames via shmem.
    if (!IsSurfaceDescriptorValid(sd)) {
      needStorage = false;
      PlanarYCbCrImage* image = video->mImage->AsPlanarYCbCrImage();
      if (!image) {
        return MediaResult(NS_ERROR_UNEXPECTED,
                           "Expected Planar YCbCr image in "
                           "RemoteVideoDecoderParent::ProcessDecodedData");
      }

      SurfaceDescriptorBuffer sdBuffer;
      nsresult rv = image->BuildSurfaceDescriptorBuffer(
          sdBuffer, [&](uint32_t aBufferSize) {
            ShmemBuffer buffer = AllocateBuffer(aBufferSize);
            if (buffer.Valid()) {
              return MemoryOrShmem(std::move(buffer.Get()));
            }
            return MemoryOrShmem();
          });
      NS_ENSURE_SUCCESS(rv, rv);

      sd = sdBuffer;
      size = image->GetSize();
    }

    if (needStorage) {
      MOZ_ASSERT(sd.type() != SurfaceDescriptor::TSurfaceDescriptorBuffer);
      mParent->StoreImage(static_cast<const SurfaceDescriptorGPUVideo&>(sd),
                          video->mImage, texture);
    }

    RemoteVideoData output(
        MediaDataIPDL(data->mOffset, data->mTime, data->mTimecode,
                      data->mDuration, data->mKeyframe),
        video->mDisplay,
        RemoteImageHolder(
            mParent,
            XRE_IsGPUProcess()
                ? VideoBridgeSource::GpuProcess
                : (XRE_IsRDDProcess()
                       ? VideoBridgeSource::RddProcess
                       : VideoBridgeSource::MFMediaEngineCDMProcess),
            size, video->mImage->GetColorDepth(), sd),
        video->mFrameID);

    array.AppendElement(std::move(output));
  }

  aDecodedData = MakeRefPtr<ArrayOfRemoteVideoData>(std::move(array));

  return NS_OK;
}

}  // namespace mozilla
