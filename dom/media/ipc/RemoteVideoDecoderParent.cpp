/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteVideoDecoderParent.h"

#include "mozilla/Unused.h"

#ifdef MOZ_AV1
#  include "AOMDecoder.h"
#endif
#include "ImageContainer.h"
#include "RemoteDecoderManagerParent.h"
#include "RemoteDecoderModule.h"

namespace mozilla {

using media::TimeUnit;
using namespace layers;  // for PlanarYCbCrImage and BufferRecycleBin

RemoteVideoDecoderParent::RemoteVideoDecoderParent(
    RemoteDecoderManagerParent* aParent, const VideoInfo& aVideoInfo,
    float aFramerate, const CreateDecoderParams::OptionSet& aOptions,
    TaskQueue* aManagerTaskQueue, TaskQueue* aDecodeTaskQueue, bool* aSuccess,
    nsCString* aErrorDescription)
    : mParent(aParent),
      mManagerTaskQueue(aManagerTaskQueue),
      mDecodeTaskQueue(aDecodeTaskQueue),
      mDestroyed(false),
      mVideoInfo(aVideoInfo) {
  MOZ_COUNT_CTOR(RemoteVideoDecoderParent);
  MOZ_ASSERT(OnManagerThread());
  // We hold a reference to ourselves to keep us alive until IPDL
  // explictly destroys us. There may still be refs held by
  // tasks, but no new ones should be added after we're
  // destroyed.
  mIPDLSelfRef = this;

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

RemoteVideoDecoderParent::~RemoteVideoDecoderParent() {
  MOZ_COUNT_DTOR(RemoteVideoDecoderParent);
}

void RemoteVideoDecoderParent::Destroy() {
  MOZ_ASSERT(OnManagerThread());
  mDestroyed = true;
  mIPDLSelfRef = nullptr;
}

mozilla::ipc::IPCResult RemoteVideoDecoderParent::RecvInit() {
  MOZ_ASSERT(OnManagerThread());
  RefPtr<RemoteVideoDecoderParent> self = this;
  mDecoder->Init()->Then(mManagerTaskQueue, __func__,
                         [self](TrackInfo::TrackType aTrack) {
                           MOZ_ASSERT(aTrack == TrackInfo::kVideoTrack);
                           if (self->mDecoder) {
                             Unused << self->SendInitComplete(
                                 self->mDecoder->GetDescriptionName(),
                                 self->mDecoder->NeedsConversion());
                           }
                         },
                         [self](MediaResult aReason) {
                           if (!self->mDestroyed) {
                             Unused << self->SendInitFailed(aReason);
                           }
                         });
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteVideoDecoderParent::RecvInput(
    const MediaRawDataIPDL& aData) {
  MOZ_ASSERT(OnManagerThread());
  // XXX: This copies the data into a buffer owned by the MediaRawData. Ideally
  // we'd just take ownership of the shmem.
  RefPtr<MediaRawData> data = new MediaRawData(aData.buffer().get<uint8_t>(),
                                               aData.buffer().Size<uint8_t>());
  if (aData.buffer().Size<uint8_t>() && !data->Data()) {
    // OOM
    Error(NS_ERROR_OUT_OF_MEMORY);
    return IPC_OK();
  }
  data->mOffset = aData.base().offset();
  data->mTime = TimeUnit::FromMicroseconds(aData.base().time());
  data->mTimecode = TimeUnit::FromMicroseconds(aData.base().timecode());
  data->mDuration = TimeUnit::FromMicroseconds(aData.base().duration());
  data->mKeyframe = aData.base().keyframe();

  DeallocShmem(aData.buffer());

  RefPtr<RemoteVideoDecoderParent> self = this;
  mDecoder->Decode(data)->Then(
      mManagerTaskQueue, __func__,
      [self, this](const MediaDataDecoder::DecodedData& aResults) {
        if (mDestroyed) {
          return;
        }
        ProcessDecodedData(aResults);
        Unused << SendInputExhausted();
      },
      [self](const MediaResult& aError) { self->Error(aError); });
  return IPC_OK();
}

void RemoteVideoDecoderParent::ProcessDecodedData(
    const MediaDataDecoder::DecodedData& aData) {
  MOZ_ASSERT(OnManagerThread());

  for (const auto& data : aData) {
    MOZ_ASSERT(data->mType == MediaData::VIDEO_DATA,
               "Can only decode videos using RemoteVideoDecoderParent!");
    VideoData* video = static_cast<VideoData*>(data.get());

    MOZ_ASSERT(video->mImage,
               "Decoded video must output a layer::Image to "
               "be used with RemoteVideoDecoderParent");

    PlanarYCbCrImage* image =
        static_cast<PlanarYCbCrImage*>(video->mImage.get());

    SurfaceDescriptorBuffer sdBuffer;
    Shmem buffer;
    if (AllocShmem(image->GetDataSize(), Shmem::SharedMemory::TYPE_BASIC,
                   &buffer) &&
        image->GetDataSize() == buffer.Size<uint8_t>()) {
      sdBuffer.data() = buffer;
      image->BuildSurfaceDescriptorBuffer(sdBuffer);
    }

    RemoteVideoDataIPDL output(
        MediaDataIPDL(data->mOffset, data->mTime.ToMicroseconds(),
                      data->mTimecode.ToMicroseconds(),
                      data->mDuration.ToMicroseconds(), data->mFrames,
                      data->mKeyframe),
        video->mDisplay, image->GetSize(), sdBuffer, video->mFrameID);
    Unused << SendVideoOutput(output);
  }
}

mozilla::ipc::IPCResult RemoteVideoDecoderParent::RecvFlush() {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(OnManagerThread());
  RefPtr<RemoteVideoDecoderParent> self = this;
  mDecoder->Flush()->Then(
      mManagerTaskQueue, __func__,
      [self]() {
        if (!self->mDestroyed) {
          Unused << self->SendFlushComplete();
        }
      },
      [self](const MediaResult& aError) { self->Error(aError); });

  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteVideoDecoderParent::RecvDrain() {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(OnManagerThread());
  RefPtr<RemoteVideoDecoderParent> self = this;
  mDecoder->Drain()->Then(
      mManagerTaskQueue, __func__,
      [self, this](const MediaDataDecoder::DecodedData& aResults) {
        if (!mDestroyed) {
          ProcessDecodedData(aResults);
          Unused << SendDrainComplete();
        }
      },
      [self](const MediaResult& aError) { self->Error(aError); });
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteVideoDecoderParent::RecvShutdown() {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(OnManagerThread());
  if (mDecoder) {
    mDecoder->Shutdown();
  }
  mDecoder = nullptr;
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteVideoDecoderParent::RecvSetSeekThreshold(
    const int64_t& aTime) {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(OnManagerThread());
  mDecoder->SetSeekThreshold(aTime == INT64_MIN
                                 ? TimeUnit::Invalid()
                                 : TimeUnit::FromMicroseconds(aTime));
  return IPC_OK();
}

void RemoteVideoDecoderParent::ActorDestroy(ActorDestroyReason aWhy) {
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

void RemoteVideoDecoderParent::Error(const MediaResult& aError) {
  MOZ_ASSERT(OnManagerThread());
  if (!mDestroyed) {
    Unused << SendError(aError);
  }
}

bool RemoteVideoDecoderParent::OnManagerThread() {
  return mParent->OnManagerThread();
}

}  // namespace mozilla
