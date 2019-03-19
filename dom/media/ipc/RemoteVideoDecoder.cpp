/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteVideoDecoder.h"

#include "mozilla/layers/ImageDataSerializer.h"

#ifdef MOZ_AV1
#  include "AOMDecoder.h"
#endif
#include "ImageContainer.h"  // for PlanarYCbCrData and BufferRecycleBin
#include "RemoteDecoderManagerChild.h"

namespace mozilla {

using namespace layers;  // for PlanarYCbCrData and BufferRecycleBin

RemoteVideoDecoderChild::RemoteVideoDecoderChild()
    : RemoteDecoderChild(), mBufferRecycleBin(new BufferRecycleBin) {}

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
      DeallocShmem(memOrShmem.get_Shmem());
      break;
    default:
      MOZ_ASSERT(false, "Unknown MemoryOrShmem type");
  }

  if (!setData) {
    return nullptr;
  }

  return image;
}

mozilla::ipc::IPCResult RemoteVideoDecoderChild::RecvOutput(
    const DecodedOutputIPDL& aDecodedData) {
  AssertOnManagerThread();
  MOZ_ASSERT(aDecodedData.type() == DecodedOutputIPDL::TRemoteVideoDataIPDL);
  const RemoteVideoDataIPDL& aData = aDecodedData.get_RemoteVideoDataIPDL();

  RefPtr<Image> image = DeserializeImage(aData.sdBuffer(), aData.frameSize());

  RefPtr<VideoData> video = VideoData::CreateFromImage(
      aData.display(), aData.base().offset(), aData.base().time(),
      aData.base().duration(), image, aData.base().keyframe(),
      aData.base().timecode());

  mDecodedData.AppendElement(std::move(video));
  return IPC_OK();
}

MediaResult RemoteVideoDecoderChild::InitIPDL(
    const VideoInfo& aVideoInfo, float aFramerate,
    const CreateDecoderParams::OptionSet& aOptions) {
  RefPtr<RemoteDecoderManagerChild> manager =
      RemoteDecoderManagerChild::GetSingleton();

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
  if (manager->SendPRemoteDecoderConstructor(this, decoderInfo, aOptions,
                                             &success, &errorDescription)) {
    mCanSend = true;
  }

  return success ? MediaResult(NS_OK)
                 : MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, errorDescription);
}

RemoteVideoDecoderParent::RemoteVideoDecoderParent(
    RemoteDecoderManagerParent* aParent, const VideoInfo& aVideoInfo,
    float aFramerate, const CreateDecoderParams::OptionSet& aOptions,
    TaskQueue* aManagerTaskQueue, TaskQueue* aDecodeTaskQueue, bool* aSuccess,
    nsCString* aErrorDescription)
    : RemoteDecoderParent(aParent, aManagerTaskQueue, aDecodeTaskQueue),
      mVideoInfo(aVideoInfo) {
  CreateDecoderParams params(mVideoInfo);
  params.mTaskQueue = mDecodeTaskQueue;
  params.mImageContainer = new layers::ImageContainer();
  params.mRate = CreateDecoderParams::VideoFrameRate(aFramerate);
  params.mOptions = aOptions;
  MediaResult error(NS_OK);
  params.mError = &error;

#ifdef MOZ_AV1
  if (AOMDecoder::IsAV1(params.mConfig.mMimeType)) {
    mDecoder = new AOMDecoder(params);
  }
#endif

  if (NS_FAILED(error)) {
    MOZ_ASSERT(aErrorDescription);
    *aErrorDescription = error.Description();
  }

  *aSuccess = !!mDecoder;
}

MediaResult RemoteVideoDecoderParent::ProcessDecodedData(
    const MediaDataDecoder::DecodedData& aData) {
  MOZ_ASSERT(OnManagerThread());

  for (const auto& data : aData) {
    MOZ_ASSERT(data->mType == MediaData::Type::VIDEO_DATA,
               "Can only decode videos using RemoteDecoderParent!");
    VideoData* video = static_cast<VideoData*>(data.get());

    MOZ_ASSERT(video->mImage,
               "Decoded video must output a layer::Image to "
               "be used with RemoteDecoderParent");

    PlanarYCbCrImage* image =
        static_cast<PlanarYCbCrImage*>(video->mImage.get());

    SurfaceDescriptorBuffer sdBuffer;
    Shmem buffer;
    if (!AllocShmem(image->GetDataSize(), Shmem::SharedMemory::TYPE_BASIC,
                    &buffer)) {
      return MediaResult(NS_ERROR_OUT_OF_MEMORY,
                         "AllocShmem failed in "
                         "RemoteVideoDecoderParent::ProcessDecodedData");
    }
    if (image->GetDataSize() > buffer.Size<uint8_t>()) {
      return MediaResult(NS_ERROR_OUT_OF_MEMORY,
                         "AllocShmem returned less than requested in "
                         "RemoteVideoDecoderParent::ProcessDecodedData");
    }

    sdBuffer.data() = std::move(buffer);
    image->BuildSurfaceDescriptorBuffer(sdBuffer);

    RemoteVideoDataIPDL output(
        MediaDataIPDL(data->mOffset, data->mTime, data->mTimecode,
                      data->mDuration, data->mKeyframe),
        video->mDisplay, image->GetSize(), sdBuffer, video->mFrameID);
    Unused << SendOutput(output);
  }

  return NS_OK;
}

}  // namespace mozilla
