/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderParent.h"

#include "RemoteDecoderManagerParent.h"
#include "mozilla/Unused.h"

namespace mozilla {

RemoteDecoderParent::RemoteDecoderParent(
    RemoteDecoderManagerParent* aParent,
    const CreateDecoderParams::OptionSet& aOptions,
    nsISerialEventTarget* aManagerThread, TaskQueue* aDecodeTaskQueue,
    const Maybe<uint64_t>& aMediaEngineId, Maybe<TrackingId> aTrackingId)
    : ShmemRecycleAllocator(this),
      mParent(aParent),
      mOptions(aOptions),
      mDecodeTaskQueue(aDecodeTaskQueue),
      mTrackingId(aTrackingId),
      mMediaEngineId(aMediaEngineId),
      mManagerThread(aManagerThread) {
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
      mManagerThread, __func__,
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
              track, self->mDecoder->GetDescriptionName(),
              self->mDecoder->GetProcessName(), self->mDecoder->GetCodecName(),
              hardwareAccelerated, hardwareReason,
              self->mDecoder->NeedsConversion()});
        }
      });
  return IPC_OK();
}

void RemoteDecoderParent::DecodeNextSample(
    const RefPtr<ArrayOfRemoteMediaRawData>& aData, size_t aIndex,
    MediaDataDecoder::DecodedData&& aOutput, DecodeResolver&& aResolver) {
  MOZ_ASSERT(OnManagerThread());

  if (!CanRecv()) {
    // Avoid unnecessarily creating shmem objects later.
    return;
  }

  if (!mDecoder) {
    // We got shutdown or the child got destroyed.
    aResolver(MediaResult(NS_ERROR_ABORT, __func__));
    return;
  }

  if (aData->Count() == aIndex) {
    DecodedOutputIPDL result;
    MediaResult rv = ProcessDecodedData(std::move(aOutput), result);
    if (NS_FAILED(rv)) {
      aResolver(std::move(rv));  // Out of Memory.
    } else {
      aResolver(std::move(result));
    }
    return;
  }

  RefPtr<MediaRawData> rawData = aData->ElementAt(aIndex);
  if (!rawData) {
    // OOM
    aResolver(MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__));
    return;
  }

  mDecoder->Decode(rawData)->Then(
      mManagerThread, __func__,
      [self = RefPtr{this}, this, aData, aIndex, output = std::move(aOutput),
       resolver = std::move(aResolver)](
          MediaDataDecoder::DecodePromise::ResolveOrRejectValue&&
              aValue) mutable {
        if (aValue.IsReject()) {
          resolver(aValue.RejectValue());
          return;
        }

        output.AppendElements(std::move(aValue.ResolveValue()));

        // Call again in case we have more data to decode.
        DecodeNextSample(aData, aIndex + 1, std::move(output),
                         std::move(resolver));
      });
}

mozilla::ipc::IPCResult RemoteDecoderParent::RecvDecode(
    ArrayOfRemoteMediaRawData* aData, DecodeResolver&& aResolver) {
  MOZ_ASSERT(OnManagerThread());
  // If we are here, we know all previously returned DecodedOutputIPDL got
  // used by the child. We can mark all previously sent ShmemBuffer as
  // available again.
  ReleaseAllBuffers();
  MediaDataDecoder::DecodedData output;
  DecodeNextSample(aData, 0, std::move(output), std::move(aResolver));

  return IPC_OK();
}

mozilla::ipc::IPCResult RemoteDecoderParent::RecvFlush(
    FlushResolver&& aResolver) {
  MOZ_ASSERT(OnManagerThread());
  RefPtr<RemoteDecoderParent> self = this;
  mDecoder->Flush()->Then(
      mManagerThread, __func__,
      [self, resolver = std::move(aResolver)](
          MediaDataDecoder::FlushPromise::ResolveOrRejectValue&& aValue) {
        self->ReleaseAllBuffers();
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
      mManagerThread, __func__,
      [self, this, resolver = std::move(aResolver)](
          MediaDataDecoder::DecodePromise::ResolveOrRejectValue&& aValue) {
        ReleaseAllBuffers();
        if (!self->CanRecv()) {
          // Avoid unnecessarily creating shmem objects later.
          return;
        }
        if (aValue.IsReject()) {
          resolver(aValue.RejectValue());
          return;
        }
        DecodedOutputIPDL output;
        MediaResult rv =
            ProcessDecodedData(std::move(aValue.ResolveValue()), output);
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
        mManagerThread, __func__,
        [self, resolver = std::move(aResolver)](
            const ShutdownPromise::ResolveOrRejectValue& aValue) {
          MOZ_ASSERT(aValue.IsResolve());
          self->ReleaseAllBuffers();
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
  CleanupShmemRecycleAllocator();
}

bool RemoteDecoderParent::OnManagerThread() {
  return mParent->OnManagerThread();
}

}  // namespace mozilla
