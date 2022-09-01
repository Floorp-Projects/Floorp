/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderChild.h"

#include "RemoteDecoderManagerChild.h"

#include "mozilla/RemoteDecodeUtils.h"

namespace mozilla {

RemoteDecoderChild::RemoteDecoderChild(RemoteDecodeIn aLocation)
    : ShmemRecycleAllocator(this),
      mLocation(aLocation),
      mThread(GetCurrentSerialEventTarget()) {
  MOZ_DIAGNOSTIC_ASSERT(
      RemoteDecoderManagerChild::GetManagerThread() &&
          RemoteDecoderManagerChild::GetManagerThread()->IsOnCurrentThread(),
      "Must be created on the manager thread");
}

RemoteDecoderChild::~RemoteDecoderChild() = default;

void RemoteDecoderChild::HandleRejectionError(
    const ipc::ResponseRejectReason& aReason,
    std::function<void(const MediaResult&)>&& aCallback) {
  // If the channel goes down and CanSend() returns false, the IPDL promise will
  // be rejected with SendError rather than ActorDestroyed. Both means the same
  // thing and we can consider that the parent has crashed. The child can no
  // longer be used.
  //

  // The GPU/RDD process crashed.
  if (mLocation == RemoteDecodeIn::GpuProcess) {
    // The GPU process will get automatically restarted by the parent process.
    // Once it has been restarted the ContentChild will receive the message and
    // will call GetManager()->InitForGPUProcess.
    // We defer reporting an error until we've recreated the RemoteDecoder
    // manager so that it'll be safe for MediaFormatReader to recreate decoders
    RefPtr<RemoteDecoderChild> self = this;
    GetManager()->RunWhenGPUProcessRecreated(NS_NewRunnableFunction(
        "RemoteDecoderChild::HandleRejectionError",
        [self, callback = std::move(aCallback)]() {
          MediaResult error(
              NS_ERROR_DOM_MEDIA_REMOTE_DECODER_CRASHED_RDD_OR_GPU_ERR,
              __func__);
          callback(error);
        }));
    return;
  }

  nsresult err = ((mLocation == RemoteDecodeIn::GpuProcess) ||
                  (mLocation == RemoteDecodeIn::RddProcess))
                     ? NS_ERROR_DOM_MEDIA_REMOTE_DECODER_CRASHED_RDD_OR_GPU_ERR
                     : NS_ERROR_DOM_MEDIA_REMOTE_DECODER_CRASHED_UTILITY_ERR;
  // The RDD process is restarted on demand and asynchronously, we can
  // immediately inform the caller that a new decoder is needed. The RDD will
  // then be restarted during the new decoder creation by
  aCallback(MediaResult(err, __func__));
}

// ActorDestroy is called if the channel goes down while waiting for a response.
void RemoteDecoderChild::ActorDestroy(ActorDestroyReason aWhy) {
  mRemoteDecoderCrashed = (aWhy == AbnormalShutdown);
  mDecodedData.Clear();
  CleanupShmemRecycleAllocator();
  RecordShutdownTelemetry(mRemoteDecoderCrashed);
}

void RemoteDecoderChild::DestroyIPDL() {
  AssertOnManagerThread();
  MOZ_DIAGNOSTIC_ASSERT(mInitPromise.IsEmpty() && mDecodePromise.IsEmpty() &&
                            mDrainPromise.IsEmpty() &&
                            mFlushPromise.IsEmpty() &&
                            mShutdownPromise.IsEmpty(),
                        "All promises should have been rejected");
  if (CanSend()) {
    PRemoteDecoderChild::Send__delete__(this);
  }
}

void RemoteDecoderChild::IPDLActorDestroyed() { mIPDLSelfRef = nullptr; }

// MediaDataDecoder methods

RefPtr<MediaDataDecoder::InitPromise> RemoteDecoderChild::Init() {
  AssertOnManagerThread();

  mRemoteDecoderCrashed = false;

  RefPtr<RemoteDecoderChild> self = this;
  SendInit()
      ->Then(
          mThread, __func__,
          [self, this](InitResultIPDL&& aResponse) {
            mInitPromiseRequest.Complete();
            if (aResponse.type() == InitResultIPDL::TMediaResult) {
              mInitPromise.Reject(aResponse.get_MediaResult(), __func__);
              return;
            }
            const auto& initResponse = aResponse.get_InitCompletionIPDL();
            mDescription = initResponse.decoderDescription();
            mDescription.Append(" (");
            mDescription.Append(RemoteDecodeInToStr(GetManager()->Location()));
            mDescription.Append(" remote)");

            mIsHardwareAccelerated = initResponse.hardware();
            mHardwareAcceleratedReason = initResponse.hardwareReason();
            mConversion = initResponse.conversion();
            // Either the promise has not yet been resolved or the handler has
            // been disconnected and we can't get here.
            mInitPromise.Resolve(initResponse.type(), __func__);
          },
          [self](const mozilla::ipc::ResponseRejectReason& aReason) {
            self->mInitPromiseRequest.Complete();
            self->HandleRejectionError(
                aReason, [self](const MediaResult& aError) {
                  self->mInitPromise.RejectIfExists(aError, __func__);
                });
          })
      ->Track(mInitPromiseRequest);

  return mInitPromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::DecodePromise> RemoteDecoderChild::Decode(
    const nsTArray<RefPtr<MediaRawData>>& aSamples) {
  AssertOnManagerThread();

  if (mRemoteDecoderCrashed) {
    nsresult err =
        ((mLocation == RemoteDecodeIn::GpuProcess) ||
         (mLocation == RemoteDecodeIn::RddProcess))
            ? NS_ERROR_DOM_MEDIA_REMOTE_DECODER_CRASHED_RDD_OR_GPU_ERR
            : NS_ERROR_DOM_MEDIA_REMOTE_DECODER_CRASHED_UTILITY_ERR;
    return MediaDataDecoder::DecodePromise::CreateAndReject(err, __func__);
  }

  auto samples = MakeRefPtr<ArrayOfRemoteMediaRawData>();
  if (!samples->Fill(aSamples,
                     [&](size_t aSize) { return AllocateBuffer(aSize); })) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        NS_ERROR_OUT_OF_MEMORY, __func__);
  }
  SendDecode(samples)->Then(
      mThread, __func__,
      [self = RefPtr{this}, this](
          PRemoteDecoderChild::DecodePromise::ResolveOrRejectValue&& aValue) {
        // We no longer need the samples as the data has been
        // processed by the parent.
        // If the parent died, the error being fatal will cause the
        // decoder to be torn down and all shmem in the pool will be
        // deallocated.
        ReleaseAllBuffers();

        if (aValue.IsReject()) {
          HandleRejectionError(
              aValue.RejectValue(), [self](const MediaResult& aError) {
                self->mDecodePromise.RejectIfExists(aError, __func__);
              });
          return;
        }
        MOZ_DIAGNOSTIC_ASSERT(CanSend(),
                              "The parent unexpectedly died, promise should "
                              "have been rejected first");
        if (mDecodePromise.IsEmpty()) {
          // We got flushed.
          return;
        }
        auto response = std::move(aValue.ResolveValue());
        if (response.type() == DecodeResultIPDL::TMediaResult &&
            NS_FAILED(response.get_MediaResult())) {
          mDecodePromise.Reject(response.get_MediaResult(), __func__);
          return;
        }
        if (response.type() == DecodeResultIPDL::TDecodedOutputIPDL) {
          ProcessOutput(std::move(response.get_DecodedOutputIPDL()));
        }
        mDecodePromise.Resolve(std::move(mDecodedData), __func__);
        mDecodedData = MediaDataDecoder::DecodedData();
      });

  return mDecodePromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::FlushPromise> RemoteDecoderChild::Flush() {
  AssertOnManagerThread();
  mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);

  RefPtr<RemoteDecoderChild> self = this;
  SendFlush()->Then(
      mThread, __func__,
      [self](const MediaResult& aResult) {
        if (NS_SUCCEEDED(aResult)) {
          self->mFlushPromise.ResolveIfExists(true, __func__);
        } else {
          self->mFlushPromise.RejectIfExists(aResult, __func__);
        }
      },
      [self](const mozilla::ipc::ResponseRejectReason& aReason) {
        self->HandleRejectionError(aReason, [self](const MediaResult& aError) {
          self->mFlushPromise.RejectIfExists(aError, __func__);
        });
      });
  return mFlushPromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::DecodePromise> RemoteDecoderChild::Drain() {
  AssertOnManagerThread();

  RefPtr<RemoteDecoderChild> self = this;
  SendDrain()->Then(
      mThread, __func__,
      [self, this](DecodeResultIPDL&& aResponse) {
        if (mDrainPromise.IsEmpty()) {
          // We got flushed.
          return;
        }
        if (aResponse.type() == DecodeResultIPDL::TMediaResult &&
            NS_FAILED(aResponse.get_MediaResult())) {
          mDrainPromise.Reject(aResponse.get_MediaResult(), __func__);
          return;
        }
        MOZ_DIAGNOSTIC_ASSERT(CanSend(),
                              "The parent unexpectedly died, promise should "
                              "have been rejected first");
        if (aResponse.type() == DecodeResultIPDL::TDecodedOutputIPDL) {
          ProcessOutput(std::move(aResponse.get_DecodedOutputIPDL()));
        }
        mDrainPromise.Resolve(std::move(mDecodedData), __func__);
        mDecodedData = MediaDataDecoder::DecodedData();
      },
      [self](const mozilla::ipc::ResponseRejectReason& aReason) {
        self->HandleRejectionError(aReason, [self](const MediaResult& aError) {
          self->mDrainPromise.RejectIfExists(aError, __func__);
        });
      });
  return mDrainPromise.Ensure(__func__);
}

RefPtr<mozilla::ShutdownPromise> RemoteDecoderChild::Shutdown() {
  AssertOnManagerThread();
  // Shutdown() can be called while an InitPromise is pending.
  mInitPromiseRequest.DisconnectIfExists();
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mFlushPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);

  RefPtr<RemoteDecoderChild> self = this;
  SendShutdown()->Then(
      mThread, __func__,
      [self](
          PRemoteDecoderChild::ShutdownPromise::ResolveOrRejectValue&& aValue) {
        self->mShutdownPromise.Resolve(aValue.IsResolve(), __func__);
      });
  return mShutdownPromise.Ensure(__func__);
}

bool RemoteDecoderChild::IsHardwareAccelerated(
    nsACString& aFailureReason) const {
  AssertOnManagerThread();
  aFailureReason = mHardwareAcceleratedReason;
  return mIsHardwareAccelerated;
}

nsCString RemoteDecoderChild::GetDescriptionName() const {
  AssertOnManagerThread();
  return mDescription;
}

void RemoteDecoderChild::SetSeekThreshold(const media::TimeUnit& aTime) {
  AssertOnManagerThread();
  Unused << SendSetSeekThreshold(aTime);
}

MediaDataDecoder::ConversionRequired RemoteDecoderChild::NeedsConversion()
    const {
  AssertOnManagerThread();
  return mConversion;
}

void RemoteDecoderChild::AssertOnManagerThread() const {
  MOZ_ASSERT(mThread->IsOnCurrentThread());
}

RemoteDecoderManagerChild* RemoteDecoderChild::GetManager() {
  if (!CanSend()) {
    return nullptr;
  }
  return static_cast<RemoteDecoderManagerChild*>(Manager());
}

}  // namespace mozilla
