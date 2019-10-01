/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderChild.h"

#include "RemoteDecoderManagerChild.h"

namespace mozilla {

RemoteDecoderChild::RemoteDecoderChild(bool aRecreatedOnCrash)
    : mThread(RemoteDecoderManagerChild::GetManagerThread()),
      mRecreatedOnCrash(aRecreatedOnCrash),
      mRawFramePool(4) {}

void RemoteDecoderChild::HandleRejectionError(
    const ipc::ResponseRejectReason& aReason,
    std::function<void(const MediaResult&)>&& aCallback) {
  // If the channel goes down and CanSend() returns false, the IPDL promise will
  // be rejected with SendError rather than ActorDestroyed. Both means the same
  // thing and we can consider that the parent has crashed. The child can no
  // longer be used.

  // The GPU/RDD process crashed, record the time and send back to MFR for
  // telemetry.
  mRemoteProcessCrashTime = TimeStamp::Now();
  if (mRecreatedOnCrash) {
    // Defer reporting an error until we've recreated the manager so that
    // it'll be safe for MediaFormatReader to recreate decoders
    RefPtr<RemoteDecoderChild> self = this;
    GetManager()->RunWhenGPUProcessRecreated(NS_NewRunnableFunction(
        "RemoteDecoderChild::HandleRejectionError",
        [self, callback = std::move(aCallback)]() {
          MediaResult error(NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER, __func__);
          error.SetGPUCrashTimeStamp(self->mRemoteProcessCrashTime);
          callback(error);
        }));
    return;
  }
  aCallback(MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__));
}

// ActorDestroy is called if the channel goes down while waiting for a response.
void RemoteDecoderChild::ActorDestroy(ActorDestroyReason aWhy) {
  mDecodedData.Clear();
  mRawFramePool.Cleanup(this);
  RecordShutdownTelemetry(aWhy == AbnormalShutdown);
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
            mIsHardwareAccelerated = initResponse.hardware();
            mHardwareAcceleratedReason = initResponse.hardwareReason();
            mConversion = initResponse.conversion();
            mInitPromise.Resolve(initResponse.type(), __func__);
          },
          [self](const mozilla::ipc::ResponseRejectReason& aReason) {
            self->mInitPromiseRequest.Complete();
            self->HandleRejectionError(
                aReason, [self](const MediaResult& aError) {
                  self->mInitPromise.Reject(aError, __func__);
                });
          })
      ->Track(mInitPromiseRequest);

  return mInitPromise.Ensure(__func__);
}

RefPtr<MediaDataDecoder::DecodePromise> RemoteDecoderChild::Decode(
    MediaRawData* aSample) {
  AssertOnManagerThread();

  // TODO: It would be nice to add an allocator method to
  // MediaDataDecoder so that the demuxer could write directly
  // into shmem rather than requiring a copy here.
  ShmemBuffer buffer = mRawFramePool.Get(this, aSample->Size(),
                                         ShmemPool::AllocationPolicy::Unsafe);
  if (!buffer.Valid()) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }

  memcpy(buffer.Get().get<uint8_t>(), aSample->Data(), aSample->Size());

  // Make a copy of the Shmem to re-use it later as IDPL will move the one used
  // in the MediaRawDataIPDL.
  Shmem mem(buffer.Get());
  MediaRawDataIPDL sample(
      MediaDataIPDL(aSample->mOffset, aSample->mTime, aSample->mTimecode,
                    aSample->mDuration, aSample->mKeyframe),
      aSample->mEOS, aSample->mDiscardPadding, aSample->Size(),
      std::move(buffer.Get()));

  RefPtr<RemoteDecoderChild> self = this;
  SendDecode(sample)->Then(
      mThread, __func__,
      [self, this, mem = std::move(mem)](
          PRemoteDecoderChild::DecodePromise::ResolveOrRejectValue&& aValue) {
        // We no longer need the ShmemBuffer as the data has been processed by
        // the parent.
        mRawFramePool.Put(ShmemBuffer(mem));

        if (aValue.IsReject()) {
          self->HandleRejectionError(
              aValue.RejectValue(), [self](const MediaResult& aError) {
                self->mDecodePromise.RejectIfExists(aError, __func__);
              });
          return;
        }
        if (mDecodePromise.IsEmpty()) {
          // We got flushed.
          return;
        }
        const auto& response = aValue.ResolveValue();
        if (response.type() == DecodeResultIPDL::TMediaResult) {
          mDecodePromise.Reject(response.get_MediaResult(), __func__);
          return;
        }
        ProcessOutput(response.get_DecodedOutputIPDL());
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
          self->mFlushPromise.Resolve(true, __func__);
        } else {
          self->mFlushPromise.Reject(aResult, __func__);
        }
      },
      [self](const mozilla::ipc::ResponseRejectReason& aReason) {
        self->HandleRejectionError(aReason, [self](const MediaResult& aError) {
          self->mFlushPromise.Reject(aError, __func__);
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
        if (aResponse.type() == DecodeResultIPDL::TMediaResult) {
          mDrainPromise.Reject(aResponse.get_MediaResult(), __func__);
          return;
        }
        ProcessOutput(aResponse.get_DecodedOutputIPDL());
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
  MOZ_DIAGNOSTIC_ASSERT(mDecodePromise.IsEmpty() && mDrainPromise.IsEmpty() &&
                            mFlushPromise.IsEmpty(),
                        "Promises must have been resolved prior to shutdown");

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
  MOZ_ASSERT(NS_GetCurrentThread() == mThread);
}

RemoteDecoderManagerChild* RemoteDecoderChild::GetManager() {
  if (!CanSend()) {
    return nullptr;
  }
  return static_cast<RemoteDecoderManagerChild*>(Manager());
}

}  // namespace mozilla
