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
#include "RemoteDecoderManagerChild.h"
#include "RemoteDecoderManagerParent.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/Telemetry.h"
#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/VideoBridgeChild.h"

namespace mozilla {

using namespace layers;  // for PlanarYCbCrData and BufferRecycleBin
using namespace ipc;
using namespace gfx;

class KnowsCompositorVideo : public layers::KnowsCompositor {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(KnowsCompositorVideo, override)

  layers::TextureForwarder* GetTextureForwarder() override {
    auto* vbc = VideoBridgeChild::GetSingleton();
    return (vbc && vbc->CanSend()) ? vbc : nullptr;
  }
  layers::LayersIPCActor* GetLayersIPCActor() override {
    return GetTextureForwarder();
  }

  static already_AddRefed<KnowsCompositorVideo> TryCreateForIdentifier(
      const layers::TextureFactoryIdentifier& aIdentifier) {
    VideoBridgeChild* child = VideoBridgeChild::GetSingleton();
    if (!child) {
      return nullptr;
    }

    // The RDD process will never use hardware decoding since it's
    // sandboxed, so don't bother trying to create a sync object.
    TextureFactoryIdentifier ident = aIdentifier;
    if (XRE_IsRDDProcess()) {
      ident.mSyncHandle = 0;
    }

    RefPtr<KnowsCompositorVideo> knowsCompositor = new KnowsCompositorVideo();
    knowsCompositor->IdentifyTextureHost(ident);
    return knowsCompositor.forget();
  }

 private:
  KnowsCompositorVideo() = default;
  virtual ~KnowsCompositorVideo() = default;
};

RemoteVideoDecoderChild::RemoteVideoDecoderChild(bool aRecreatedOnCrash)
    : RemoteDecoderChild(aRecreatedOnCrash),
      mBufferRecycleBin(new BufferRecycleBin) {}

RefPtr<mozilla::layers::Image> RemoteVideoDecoderChild::DeserializeImage(
    const SurfaceDescriptorBuffer& aSdBuffer, const IntSize& aPicSize) {
  MOZ_ASSERT(aSdBuffer.desc().type() == BufferDescriptor::TYCbCrDescriptor);
  if (aSdBuffer.desc().type() != BufferDescriptor::TYCbCrDescriptor) {
    return nullptr;
  }
  const YCbCrDescriptor& descriptor = aSdBuffer.desc().get_YCbCrDescriptor();

  uint8_t* buffer = nullptr;
  const MemoryOrShmem& memOrShmem = aSdBuffer.data();
  switch (memOrShmem.type()) {
    case MemoryOrShmem::Tuintptr_t:
      buffer = reinterpret_cast<uint8_t*>(memOrShmem.get_uintptr_t());
      break;
    case MemoryOrShmem::TShmem:
      buffer = memOrShmem.get_Shmem().get<uint8_t>();
      break;
    default:
      MOZ_ASSERT(false, "Unknown MemoryOrShmem type");
  }
  if (!buffer) {
    return nullptr;
  }

  PlanarYCbCrData pData;
  pData.mYSize = descriptor.ySize();
  pData.mYStride = descriptor.yStride();
  pData.mCbCrSize = descriptor.cbCrSize();
  pData.mCbCrStride = descriptor.cbCrStride();
  // default mYSkip, mCbSkip, mCrSkip because not held in YCbCrDescriptor
  pData.mYSkip = pData.mCbSkip = pData.mCrSkip = 0;
  // default mPicX, mPicY because not held in YCbCrDescriptor
  pData.mPicX = pData.mPicY = 0;
  pData.mPicSize = aPicSize;
  pData.mStereoMode = descriptor.stereoMode();
  pData.mColorDepth = descriptor.colorDepth();
  pData.mYUVColorSpace = descriptor.yUVColorSpace();
  pData.mYChannel = ImageDataSerializer::GetYChannel(buffer, descriptor);
  pData.mCbChannel = ImageDataSerializer::GetCbChannel(buffer, descriptor);
  pData.mCrChannel = ImageDataSerializer::GetCrChannel(buffer, descriptor);

  // images coming from AOMDecoder are RecyclingPlanarYCbCrImages.
  RefPtr<RecyclingPlanarYCbCrImage> image =
      new RecyclingPlanarYCbCrImage(mBufferRecycleBin);
  bool setData = image->CopyData(pData);
  MOZ_ASSERT(setData);

  switch (memOrShmem.type()) {
    case MemoryOrShmem::Tuintptr_t:
      delete[] reinterpret_cast<uint8_t*>(memOrShmem.get_uintptr_t());
      break;
    case MemoryOrShmem::TShmem:
      // Memory buffer will be recycled by the parent automatically.
      break;
    default:
      MOZ_ASSERT(false, "Unknown MemoryOrShmem type");
  }

  if (!setData) {
    return nullptr;
  }

  return image;
}

MediaResult RemoteVideoDecoderChild::ProcessOutput(
    const DecodedOutputIPDL& aDecodedData) {
  AssertOnManagerThread();
  MOZ_ASSERT(aDecodedData.type() ==
             DecodedOutputIPDL::TArrayOfRemoteVideoDataIPDL);

  const nsTArray<RemoteVideoDataIPDL>& arrayData =
      aDecodedData.get_ArrayOfRemoteVideoDataIPDL();

  for (auto&& data : arrayData) {
    RefPtr<Image> image;
    if (data.sd().type() == SurfaceDescriptor::TSurfaceDescriptorBuffer) {
      image = DeserializeImage(data.sd().get_SurfaceDescriptorBuffer(),
                               data.frameSize());
    } else {
      // The Image here creates a TextureData object that takes ownership
      // of the SurfaceDescriptor, and is responsible for making sure that
      // it gets deallocated.
      SurfaceDescriptorRemoteDecoder remoteSD =
          static_cast<const SurfaceDescriptorGPUVideo&>(data.sd());
      remoteSD.source() = Some(GetManager()->GetSource());
      image = new GPUVideoImage(GetManager(), remoteSD, data.frameSize());
    }

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
    const layers::TextureFactoryIdentifier* aIdentifier) {
  RefPtr<RemoteDecoderManagerChild> manager =
      RemoteDecoderManagerChild::GetRDDProcessSingleton();

  // The manager isn't available because RemoteDecoderManagerChild has been
  // initialized with null end points and we don't want to decode video on RDD
  // process anymore. Return false here so that we can fallback to other PDMs.
  if (!manager) {
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("RemoteDecoderManager is not available."));
  }

  if (!manager->CanSend()) {
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("RemoteDecoderManager unable to send."));
  }

  mIPDLSelfRef = this;
  bool success = false;
  nsCString errorDescription;
  VideoDecoderInfoIPDL decoderInfo(aVideoInfo, aFramerate);
  Unused << manager->SendPRemoteDecoderConstructor(this, decoderInfo, aOptions,
                                                   ToMaybe(aIdentifier),
                                                   &success, &errorDescription);

  return success ? MediaResult(NS_OK)
                 : MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, errorDescription);
}

GpuRemoteVideoDecoderChild::GpuRemoteVideoDecoderChild()
    : RemoteVideoDecoderChild(true) {}

MediaResult GpuRemoteVideoDecoderChild::InitIPDL(
    const VideoInfo& aVideoInfo, float aFramerate,
    const CreateDecoderParams::OptionSet& aOptions,
    const layers::TextureFactoryIdentifier& aIdentifier) {
  RefPtr<RemoteDecoderManagerChild> manager =
      RemoteDecoderManagerChild::GetGPUProcessSingleton();

  // The manager isn't available because RemoteDecoderManagerChild has been
  // initialized with null end points and we don't want to decode video on GPU
  // process anymore. Return false here so that we can fallback to other PDMs.
  if (!manager) {
    return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                       RESULT_DETAIL("RemoteDecoderManager is not available."));
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
  VideoDecoderInfoIPDL decoderInfo(aVideoInfo, aFramerate);
  Unused << manager->SendPRemoteDecoderConstructor(this, decoderInfo, aOptions,
                                                   Some(aIdentifier), &success,
                                                   &errorDescription);

  return success ? MediaResult(NS_OK)
                 : MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, errorDescription);
}

RemoteVideoDecoderParent::RemoteVideoDecoderParent(
    RemoteDecoderManagerParent* aParent, const VideoInfo& aVideoInfo,
    float aFramerate, const CreateDecoderParams::OptionSet& aOptions,
    const Maybe<layers::TextureFactoryIdentifier>& aIdentifier,
    nsISerialEventTarget* aManagerThread, TaskQueue* aDecodeTaskQueue,
    bool* aSuccess, nsCString* aErrorDescription)
    : RemoteDecoderParent(aParent, aManagerThread, aDecodeTaskQueue),
      mVideoInfo(aVideoInfo) {
  if (aIdentifier) {
    // Check to see if we have a direct PVideoBridge connection to the
    // destination process specified in aIdentifier, and create a
    // KnowsCompositor representing that connection if so. If this fails, then
    // we fall back to returning the decoded frames directly via Output().
    mKnowsCompositor =
        KnowsCompositorVideo::TryCreateForIdentifier(*aIdentifier);
  }

  RefPtr<layers::ImageContainer> container = new layers::ImageContainer();
  if (mKnowsCompositor && XRE_IsRDDProcess()) {
    // Ensure to allocate recycle allocator
    container->EnsureRecycleAllocatorForRDD(mKnowsCompositor);
  }

  CreateDecoderParams params(mVideoInfo);
  params.mKnowsCompositor = mKnowsCompositor;
  params.mImageContainer = container;
  params.mRate = CreateDecoderParams::VideoFrameRate(aFramerate);
  params.mOptions = aOptions;
  MediaResult error(NS_OK);
  params.mError = &error;

  RefPtr<MediaDataDecoder> decoder;
  if (XRE_IsGPUProcess()) {
#ifdef XP_WIN
    // Ensure everything is properly initialized on the right thread.
    PDMFactory::EnsureInit();

    // TODO: Ideally we wouldn't hardcode the WMF PDM, and we'd use the normal
    // PDM factory logic for picking a decoder.
    RefPtr<WMFDecoderModule> pdm(new WMFDecoderModule());
    pdm->Startup();
    decoder = pdm->CreateVideoDecoder(params);
#else
    MOZ_ASSERT(false,
               "Can't use RemoteVideoDecoder in the GPU process on non-Windows "
               "platforms yet");
#endif
  }

#ifdef MOZ_AV1
  if (AOMDecoder::IsAV1(params.mConfig.mMimeType)) {
    if (StaticPrefs::media_av1_use_dav1d()) {
      decoder = new DAV1DDecoder(params);
    } else {
      decoder = new AOMDecoder(params);
    }
  }
#endif

  if (NS_FAILED(error)) {
    MOZ_ASSERT(aErrorDescription);
    *aErrorDescription = error.Description();
  }

  if (decoder) {
    mDecoder = new MediaDataDecoderProxy(decoder.forget(),
                                         do_AddRef(mDecodeTaskQueue.get()));
  }
  *aSuccess = !!mDecoder;
}

MediaResult RemoteVideoDecoderParent::ProcessDecodedData(
    const MediaDataDecoder::DecodedData& aData,
    DecodedOutputIPDL& aDecodedData) {
  MOZ_ASSERT(OnManagerThread());

  nsTArray<RemoteVideoDataIPDL> array;

  // If the video decoder bridge has shut down, stop.
  if (mKnowsCompositor && !mKnowsCompositor->GetTextureForwarder()) {
    aDecodedData = std::move(array);
    return NS_OK;
  }

  for (const auto& data : aData) {
    MOZ_ASSERT(data->mType == MediaData::Type::VIDEO_DATA,
               "Can only decode videos using RemoteDecoderParent!");
    VideoData* video = static_cast<VideoData*>(data.get());

    MOZ_ASSERT(video->mImage,
               "Decoded video must output a layer::Image to "
               "be used with RemoteDecoderParent");

    SurfaceDescriptor sd;
    IntSize size;

    if (mKnowsCompositor) {
      RefPtr<TextureClient> texture =
          video->mImage->GetTextureClient(mKnowsCompositor);

      if (!texture) {
        texture = ImageClient::CreateTextureClientForImage(video->mImage,
                                                           mKnowsCompositor);
      }

      if (texture) {
        if (!texture->IsAddedToCompositableClient()) {
          texture->InitIPDLActor(mKnowsCompositor);
          texture->SetAddedToCompositableClient();
        }
        sd = mParent->StoreImage(video->mImage, texture);
        size = texture->GetSize();
      }
    }

    // If failed to create a GPU accelerated surface descriptor, fall back to
    // copying frames via shmem.
    if (!IsSurfaceDescriptorValid(sd)) {
      PlanarYCbCrImage* image = video->mImage->AsPlanarYCbCrImage();
      if (!image) {
        return MediaResult(NS_ERROR_UNEXPECTED,
                           "Expected Planar YCbCr image in "
                           "RemoteVideoDecoderParent::ProcessDecodedData");
      }

      SurfaceDescriptorBuffer sdBuffer;
      ShmemBuffer buffer = AllocateBuffer(image->GetDataSize());
      if (!buffer.Valid()) {
        return MediaResult(NS_ERROR_OUT_OF_MEMORY,
                           "AllocShmem failed in "
                           "RemoteVideoDecoderParent::ProcessDecodedData");
      }

      sdBuffer.data() = std::move(buffer.Get());
      image->BuildSurfaceDescriptorBuffer(sdBuffer);

      sd = sdBuffer;
      size = image->GetSize();
    }

    RemoteVideoDataIPDL output(
        MediaDataIPDL(data->mOffset, data->mTime, data->mTimecode,
                      data->mDuration, data->mKeyframe),
        video->mDisplay, size, sd, video->mFrameID);

    array.AppendElement(output);
  }

  aDecodedData = std::move(array);

  return NS_OK;
}

}  // namespace mozilla
