/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderParent.h"

#include "mozilla/Unused.h"

#include "RemoteDecoderManagerParent.h"

namespace mozilla {

RemoteDecoderParent::RemoteDecoderParent(RemoteDecoderManagerParent* aParent,
                                         TaskQueue* aManagerTaskQueue,
                                         TaskQueue* aDecodeTaskQueue)
    : mParent(aParent),
      mDecodeTaskQueue(aDecodeTaskQueue),
      mManagerTaskQueue(aManagerTaskQueue),
      mDecodedFramePool(1, ShmemPool::PoolType::DynamicPool) {
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
  mIPDLSelfRef = nullptr;
}

mozilla::ipc::IPCResult RemoteDecoderParent::RecvInit(
    InitResolver&& aResolver) {
  MOZ_ASSERT(OnManagerThread());
  RefPtr<RemoteDecoderParent> self = this;
  mDecoder->Init()->Then(
      mManagerTaskQueue, __func__,
      [self, resolver = std::move(aResolver)](
          MediaDataDecoder::InitPromise::ResolveOrRejectValue&& aValue) {
        if (!self->CanRecv()) {
          // The promise to the child would have already been rejected.
          return;
        }
        if (aValue.IsReject()) {
          resolver(aValue.RejectValue());
          return;
        }
        auto track = aValue.ResolveValue();
        MOZ_ASSERT(track == TrackInfo::kAudioTrack ||
                   track == TrackInfo::kVideoTrack);
        if (self->mDecoder) {
          nsCString hardwareReason;
          bool hardwareAccelerated =
              self->mDecoder->IsHardwareAccelerated(hardwareReason);
          resolver(InitCompletionIPDL{
              track, self->mDecoder->GetDescriptionName(), hardwareAccelerated,
              hardwareReason, self->mDecoder->NeedsConversion()});
        }
      });
  return IPC_OK();
}

void RemoteDecoderParent::DecodeNextSample(nsTArray<MediaRawDataIPDL>&& aData,
                                           DecodedOutputIPDL&& aOutput,
                                           DecodeResolver&& aResolver) {
  if (aData.IsEmpty()) {
    aResolver(std::move(aOutput));
    return;
  }

  const MediaRawDataIPDL& rawData = aData[0];
  RefPtr<MediaRawData> data = new MediaRawData(
      rawData.buffer().get<uint8_t>(),
      std::min((unsigned long)rawData.bufferSize(),
               (unsigned long)rawData.buffer().Size<uint8_t>()));
  // if MediaRawData's size is less than the actual buffer size recorded
  // in MediaRawDataIPDL, consider it an OOM situation.
  if ((int64_t)data->Size() < rawData.bufferSize()) {
    // OOM
    ReleaseUsedShmems();
    aResolver(MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__));
    return;
  }
  data->mOffset = rawData.base().offset();
  data->mTime = rawData.base().time();
  data->mTimecode = rawData.base().timecode();
  data->mDuration = rawData.base().duration();
  data->mKeyframe = rawData.base().keyframe();
  data->mEOS = rawData.eos();
  data->mDiscardPadding = rawData.discardPadding();

  aData.RemoveElementAt(0);

  RefPtr<RemoteDecoderParent> self = this;
  mDecoder->Decode(data)->Then(
      mManagerTaskQueue, __func__,
      [self, this, aData = std::move(aData), output = std::move(aOutput),
       resolver = std::move(aResolver)](
          MediaDataDecoder::DecodePromise::ResolveOrRejectValue&&
              aValue) mutable {
        if (!CanRecv()) {
          ReleaseUsedShmems();
          return;
        }
        if (aValue.IsReject()) {
          ReleaseUsedShmems();
          resolver(aValue.RejectValue());
          return;
        }

        MediaResult rv = ProcessDecodedData(aValue.ResolveValue(), output);
        if (NS_FAILED(rv)) {
          ReleaseUsedShmems();
          resolver(rv);
        } else {
          // Call again in case we have more data to decode.
          DecodeNextSample(std::move(aData), std::move(output),
                           std::move(resolver));
        }
      });
}

mozilla::ipc::IPCResult RemoteDecoderParent::RecvDecode(
    nsTArray<MediaRawDataIPDL>&& aData, DecodeResolver&& aResolver) {
  MOZ_ASSERT(OnManagerThread());
  // XXX: This copies the data into a buffer owned by the MediaRawData. Ideally
  // we'd just take ownership of the shmem.
  // Use the passed bufferSize in MediaRawDataIPDL since we can get a Shmem
  // buffer from ShmemPool larger than the requested size.

  // If we are here, we know all previously returned DecodedOutputIPDL got
  // used by the child. We can mark all previously sent ShmemBuffer as
  // available again.
  ReleaseUsedShmems();
  DecodedOutputIPDL output;
  DecodeNextSample(std::move(aData), std::move(output), std::move(aResolver));

  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderParent::RecvFlush(
    FlushResolver&& aResolver) {
  MOZ_ASSERT(OnManagerThread());
  RefPtr<RemoteDecoderParent> self = this;
  mDecoder->Flush()->Then(
      mManagerTaskQueue, __func__,
      [self, resolver = std::move(aResolver)](
          MediaDataDecoder::FlushPromise::ResolveOrRejectValue&& aValue) {
        if (aValue.IsReject()) {
          resolver(aValue.RejectValue());
        } else {
          resolver(MediaResult(NS_OK));
        }
      });

  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderParent::RecvDrain(
    DrainResolver&& aResolver) {
  MOZ_ASSERT(OnManagerThread());
  RefPtr<RemoteDecoderParent> self = this;
  mDecoder->Drain()->Then(
      mManagerTaskQueue, __func__,
      [self, this, resolver = std::move(aResolver)](
          MediaDataDecoder::DecodePromise::ResolveOrRejectValue&& aValue) {
        if (!self->CanRecv()) {
          // Avoid unnecessarily creating shmem objects later.
          return;
        }
        if (aValue.IsReject()) {
          resolver(aValue.RejectValue());
          return;
        }
        DecodedOutputIPDL output;
        MediaResult rv = ProcessDecodedData(aValue.ResolveValue(), output);
        if (NS_FAILED(rv)) {
          resolver(rv);
        } else {
          resolver(std::move(output));
        }
      });
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderParent::RecvShutdown(
    ShutdownResolver&& aResolver) {
  MOZ_ASSERT(OnManagerThread());
  if (mDecoder) {
    RefPtr<RemoteDecoderParent> self = this;
    mDecoder->Shutdown()->Then(
        mManagerTaskQueue, __func__,
        [self, resolver = std::move(aResolver)](
            const ShutdownPromise::ResolveOrRejectValue& aValue) {
          MOZ_ASSERT(aValue.IsResolve());
          resolver(true);
        });
  }
  mDecoder = nullptr;
  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderParent::RecvSetSeekThreshold(
    const TimeUnit& aTime) {
  MOZ_ASSERT(OnManagerThread());
  mDecoder->SetSeekThreshold(aTime);
  return IPC_OK();
}

void RemoteDecoderParent::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(OnManagerThread());
  if (mDecoder) {
    mDecoder->Shutdown();
    mDecoder = nullptr;
  }
  ReleaseUsedShmems();
  mDecodedFramePool.Cleanup(this);
}

ShmemBuffer RemoteDecoderParent::AllocateBuffer(size_t aSize) {
  ShmemBuffer buffer =
      mDecodedFramePool.Get(this, aSize, ShmemPool::AllocationPolicy::Unsafe);
  if (!buffer.Valid()) {
    return buffer;
  }
  if (aSize > buffer.Get().Size<uint8_t>()) {
    ReleaseBuffer(std::move(buffer));
    return ShmemBuffer();
  }
  mUsedShmems.AppendElement(buffer.Get());
  return buffer;
}

void RemoteDecoderParent::ReleaseBuffer(ShmemBuffer&& aBuffer) {
  mDecodedFramePool.Put(std::move(aBuffer));
}

void RemoteDecoderParent::ReleaseUsedShmems() {
  for (ShmemBuffer& mem : mUsedShmems) {
    ReleaseBuffer(ShmemBuffer(mem.Get()));
  }
  mUsedShmems.Clear();
}

bool RemoteDecoderParent::OnManagerThread() {
  return mParent->OnManagerThread();
}

}  // namespace mozilla
