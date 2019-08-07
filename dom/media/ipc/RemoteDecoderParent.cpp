/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderParent.h"

#include "mozilla/Unused.h"

#include "RemoteDecoderManagerParent.h"

namespace mozilla {

using media::TimeUnit;

RemoteDecoderParent::RemoteDecoderParent(RemoteDecoderManagerParent* aParent,
                                         TaskQueue* aManagerTaskQueue,
                                         TaskQueue* aDecodeTaskQueue)
    : mParent(aParent),
      mManagerTaskQueue(aManagerTaskQueue),
      mDecodeTaskQueue(aDecodeTaskQueue),
      mDecodedFramePool(4) {
  MOZ_COUNT_CTOR(RemoteDecoderParent);
  MOZ_ASSERT(OnManagerThread());
  // We hold a reference to ourselves to keep us alive until IPDL
  // explictly destroys us. There may still be refs held by
  // tasks, but no new ones should be added after we're
  // destroyed.
  mIPDLSelfRef = this;
}

RemoteDecoderParent::~RemoteDecoderParent() {
  MOZ_COUNT_DTOR(RemoteDecoderParent);
}

void RemoteDecoderParent::Destroy() {
  MOZ_ASSERT(OnManagerThread());
  mDestroyed = true;
  mIPDLSelfRef = nullptr;
}

mozilla::ipc::IPCResult RemoteDecoderParent::RecvInit() {
  MOZ_ASSERT(OnManagerThread());
  RefPtr<RemoteDecoderParent> self = this;
  mDecoder->Init()->Then(
      mManagerTaskQueue, __func__,
      [self](TrackInfo::TrackType aTrack) {
        MOZ_ASSERT(aTrack == TrackInfo::kAudioTrack ||
                   aTrack == TrackInfo::kVideoTrack);
        if (self->mDecoder) {
          nsCString hardwareReason;
          bool hardwareAccelerated =
              self->mDecoder->IsHardwareAccelerated(hardwareReason);
          Unused << self->SendInitComplete(
              aTrack, self->mDecoder->GetDescriptionName(), hardwareAccelerated,
              hardwareReason, self->mDecoder->NeedsConversion());
        }
      },
      [self](MediaResult aReason) {
        if (!self->mDestroyed) {
          Unused << self->SendInitFailed(aReason);
        }
      });
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderParent::RecvInput(
    const MediaRawDataIPDL& aData) {
  MOZ_ASSERT(OnManagerThread());
  // XXX: This copies the data into a buffer owned by the MediaRawData. Ideally
  // we'd just take ownership of the shmem.
  // Use the passed bufferSize in MediaRawDataIPDL since we can get a Shmem
  // buffer from ShmemPool larger than the requested size.
  RefPtr<MediaRawData> data =
      new MediaRawData(aData.buffer().get<uint8_t>(),
                       std::min((unsigned long)aData.bufferSize(),
                                (unsigned long)aData.buffer().Size<uint8_t>()));
  if (aData.buffer().Size<uint8_t>() && !data->Data()) {
    // OOM
    Error(NS_ERROR_OUT_OF_MEMORY);
    return IPC_OK();
  }
  data->mOffset = aData.base().offset();
  data->mTime = aData.base().time();
  data->mTimecode = aData.base().timecode();
  data->mDuration = aData.base().duration();
  data->mKeyframe = aData.base().keyframe();
  data->mEOS = aData.eos();
  data->mDiscardPadding = aData.discardPadding();

  // Send back to the child to reuse in ShmemPool
  Unused << SendDoneWithInput(std::move(aData.buffer()));

  RefPtr<RemoteDecoderParent> self = this;
  mDecoder->Decode(data)->Then(
      mManagerTaskQueue, __func__,
      [self, this](const MediaDataDecoder::DecodedData& aResults) {
        if (mDestroyed) {
          return;
        }
        MediaResult res = ProcessDecodedData(aResults);
        if (res != NS_OK) {
          self->Error(res);
          return;
        }
        Unused << SendInputExhausted();
      },
      [self](const MediaResult& aError) { self->Error(aError); });
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderParent::RecvFlush() {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(OnManagerThread());
  RefPtr<RemoteDecoderParent> self = this;
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

mozilla::ipc::IPCResult RemoteDecoderParent::RecvDrain() {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(OnManagerThread());
  RefPtr<RemoteDecoderParent> self = this;
  mDecoder->Drain()->Then(
      mManagerTaskQueue, __func__,
      [self, this](const MediaDataDecoder::DecodedData& aResults) {
        if (!mDestroyed) {
          MediaResult res = ProcessDecodedData(aResults);
          if (res != NS_OK) {
            self->Error(res);
            return;
          }
          Unused << SendDrainComplete();
        }
      },
      [self](const MediaResult& aError) { self->Error(aError); });
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderParent::RecvShutdown() {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(OnManagerThread());
  if (mDecoder) {
    RefPtr<RemoteDecoderParent> self = this;
    mDecoder->Shutdown()->Then(
        mManagerTaskQueue, __func__,
        [self](const ShutdownPromise::ResolveOrRejectValue& aValue) {
          MOZ_ASSERT(aValue.IsResolve());
          if (!self->mDestroyed) {
            Unused << self->SendShutdownComplete();
          }
        });
  }
  mDecoder = nullptr;
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderParent::RecvSetSeekThreshold(
    const TimeUnit& aTime) {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(OnManagerThread());
  mDecoder->SetSeekThreshold(aTime);
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderParent::RecvDoneWithOutput(
    Shmem&& aOutputShmem) {
  mDecodedFramePool.Put(ShmemBuffer(aOutputShmem));
  return IPC_OK();
}

void RemoteDecoderParent::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(OnManagerThread());
  if (mDecoder) {
    mDecoder->Shutdown();
    mDecoder = nullptr;
  }
  mDecodedFramePool.Cleanup(this);
}

void RemoteDecoderParent::Error(const MediaResult& aError) {
  MOZ_ASSERT(OnManagerThread());
  if (!mDestroyed) {
    Unused << SendError(aError);
  }
}

bool RemoteDecoderParent::OnManagerThread() {
  return mParent->OnManagerThread();
}

}  // namespace mozilla
