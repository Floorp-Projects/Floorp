/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteVideoDecoderChild.h"

#include "base/thread.h"
#include "mozilla/layers/ImageDataSerializer.h"

#include "ImageContainer.h"  // for PlanarYCbCrData and BufferRecycleBin
#include "RemoteDecoderManagerChild.h"

namespace mozilla {

using base::Thread;
using namespace layers;  // for PlanarYCbCrData and BufferRecycleBin

RemoteVideoDecoderChild::RemoteVideoDecoderChild()
    : mThread(RemoteDecoderManagerChild::GetManagerThread()),
      mCanSend(false),
      mInitialized(false),
      mIsHardwareAccelerated(false),
      mConversion(MediaDataDecoder::ConversionRequired::kNeedNone),
      mBufferRecycleBin(new BufferRecycleBin) {}

RemoteVideoDecoderChild::~RemoteVideoDecoderChild() {
  AssertOnManagerThread();
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
}

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

mozilla::ipc::IPCResult RemoteVideoDecoderChild::RecvVideoOutput(
    const RemoteVideoDataIPDL& aData) {
  AssertOnManagerThread();

  RefPtr<Image> image = DeserializeImage(aData.sdBuffer(), aData.frameSize());

  RefPtr<VideoData> video = VideoData::CreateFromImage(
      aData.display(), aData.base().offset(),
      media::TimeUnit::FromMicroseconds(aData.base().time()),
      media::TimeUnit::FromMicroseconds(aData.base().duration()), image,
      aData.base().keyframe(),
      media::TimeUnit::FromMicroseconds(aData.base().timecode()));

  mDecodedData.AppendElement(std::move(video));
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteVideoDecoderChild::RecvInputExhausted() {
  AssertOnManagerThread();
  mDecodePromise.ResolveIfExists(std::move(mDecodedData), __func__);
  mDecodedData = MediaDataDecoder::DecodedData();
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteVideoDecoderChild::RecvDrainComplete() {
  AssertOnManagerThread();
  mDrainPromise.ResolveIfExists(std::move(mDecodedData), __func__);
  mDecodedData = MediaDataDecoder::DecodedData();
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteVideoDecoderChild::RecvError(
    const nsresult& aError) {
  AssertOnManagerThread();
  mDecodedData = MediaDataDecoder::DecodedData();
  mDecodePromise.RejectIfExists(aError, __func__);
  mDrainPromise.RejectIfExists(aError, __func__);
  mFlushPromise.RejectIfExists(aError, __func__);
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteVideoDecoderChild::RecvInitComplete(
    const nsCString& aDecoderDescription,
    const ConversionRequired& aConversion) {
  AssertOnManagerThread();
  mInitPromise.ResolveIfExists(TrackInfo::kVideoTrack, __func__);
  mInitialized = true;
  mDescription = aDecoderDescription;
  mConversion = aConversion;
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteVideoDecoderChild::RecvInitFailed(
    const nsresult& aReason) {
  AssertOnManagerThread();
  mInitPromise.RejectIfExists(aReason, __func__);
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteVideoDecoderChild::RecvFlushComplete() {
  AssertOnManagerThread();
  mFlushPromise.ResolveIfExists(true, __func__);
  return IPC_OK();
}

void RemoteVideoDecoderChild::ActorDestroy(ActorDestroyReason aWhy) {
  mCanSend = false;
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
  if (manager->SendPRemoteVideoDecoderConstructor(this, aVideoInfo, aFramerate,
                                                  aOptions, &success,
                                                  &errorDescription)) {
    mCanSend = true;
  }

  return success ? MediaResult(NS_OK)
                 : MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, errorDescription);
}

void RemoteVideoDecoderChild::DestroyIPDL() {
  if (mCanSend) {
    PRemoteVideoDecoderChild::Send__delete__(this);
  }
}

void RemoteVideoDecoderChild::IPDLActorDestroyed() { mIPDLSelfRef = nullptr; }

// MediaDataDecoder methods

RefPtr<MediaDataDecoder::InitPromise> RemoteVideoDecoderChild::Init() {
  AssertOnManagerThread();

  if (!mIPDLSelfRef || !mCanSend) {
    return MediaDataDecoder::InitPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }

  SendInit();

  return mInitPromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::DecodePromise> RemoteVideoDecoderChild::Decode(
    MediaRawData* aSample) {
  AssertOnManagerThread();

  if (!mCanSend) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
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

  MediaRawDataIPDL sample(
      MediaDataIPDL(aSample->mOffset, aSample->mTime.ToMicroseconds(),
                    aSample->mTimecode.ToMicroseconds(),
                    aSample->mDuration.ToMicroseconds(), aSample->mFrames,
                    aSample->mKeyframe),
      buffer);
  SendInput(sample);
  return mDecodePromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::FlushPromise> RemoteVideoDecoderChild::Flush() {
  AssertOnManagerThread();
  mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  if (!mCanSend) {
    return MediaDataDecoder::FlushPromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }
  SendFlush();
  return mFlushPromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::DecodePromise> RemoteVideoDecoderChild::Drain() {
  AssertOnManagerThread();
  if (!mCanSend) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }
  SendDrain();
  return mDrainPromise.Ensure(__func__);
}

void RemoteVideoDecoderChild::Shutdown() {
  AssertOnManagerThread();
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  if (mCanSend) {
    SendShutdown();
  }
  mInitialized = false;
}

bool RemoteVideoDecoderChild::IsHardwareAccelerated(
    nsACString& aFailureReason) const {
  AssertOnManagerThread();
  aFailureReason = mHardwareAcceleratedReason;
  return mIsHardwareAccelerated;
}

nsCString RemoteVideoDecoderChild::GetDescriptionName() const {
  AssertOnManagerThread();
  return mDescription;
}

void RemoteVideoDecoderChild::SetSeekThreshold(const media::TimeUnit& aTime) {
  AssertOnManagerThread();
  if (mCanSend) {
    SendSetSeekThreshold(aTime.IsValid() ? aTime.ToMicroseconds() : INT64_MIN);
  }
}

MediaDataDecoder::ConversionRequired RemoteVideoDecoderChild::NeedsConversion()
    const {
  AssertOnManagerThread();
  return mConversion;
}

void RemoteVideoDecoderChild::AssertOnManagerThread() const {
  MOZ_ASSERT(NS_GetCurrentThread() == mThread);
}

RemoteDecoderManagerChild* RemoteVideoDecoderChild::GetManager() {
  if (!mCanSend) {
    return nullptr;
  }
  return static_cast<RemoteDecoderManagerChild*>(Manager());
}

}  // namespace mozilla
