/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteMediaDataDecoder.h"

#include "base/thread.h"

#include "IRemoteDecoderChild.h"
#include "RemoteDecoderManagerChild.h"

namespace mozilla {

RemoteMediaDataDecoder::RemoteMediaDataDecoder(IRemoteDecoderChild* aChild)
    : mChild(aChild) {}

RemoteMediaDataDecoder::~RemoteMediaDataDecoder() {
  /* Shutdown method should have been called. */
  MOZ_ASSERT(!mChild);
}

RefPtr<MediaDataDecoder::InitPromise> RemoteMediaDataDecoder::Init() {
  RefPtr<RemoteMediaDataDecoder> self = this;
  return InvokeAsync(RemoteDecoderManagerChild::GetManagerThread(), __func__,
                     [self]() { return self->mChild->Init(); })
      ->Then(
          RemoteDecoderManagerChild::GetManagerThread(), __func__,
          [self, this](TrackType aTrack) {
            // If shutdown has started in the meantime shutdown promise may
            // be resloved before this task. In this case mChild will be null
            // and the init promise has to be canceled.
            if (!mChild) {
              return InitPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_CANCELED,
                                                  __func__);
            }
            mDescription =
                mChild->GetDescriptionName() + NS_LITERAL_CSTRING(" (remote)");
            mIsHardwareAccelerated =
                mChild->IsHardwareAccelerated(mHardwareAcceleratedReason);
            mConversion = mChild->NeedsConversion();
            return InitPromise::CreateAndResolve(aTrack, __func__);
          },
          [self](const MediaResult& aError) {
            return InitPromise::CreateAndReject(aError, __func__);
          });
}

RefPtr<MediaDataDecoder::DecodePromise> RemoteMediaDataDecoder::Decode(
    MediaRawData* aSample) {
  RefPtr<RemoteMediaDataDecoder> self = this;
  RefPtr<MediaRawData> sample = aSample;
  return InvokeAsync(
      RemoteDecoderManagerChild::GetManagerThread(), __func__,
      [self, sample]() {
        return self->mChild->Decode(nsTArray<RefPtr<MediaRawData>>{sample});
      });
}

RefPtr<MediaDataDecoder::DecodePromise> RemoteMediaDataDecoder::DecodeBatch(
    nsTArray<RefPtr<MediaRawData>>&& aSamples) {
  RefPtr<RemoteMediaDataDecoder> self = this;
  return InvokeAsync(RemoteDecoderManagerChild::GetManagerThread(), __func__,
                     [self, samples = std::move(aSamples)]() {
                       return self->mChild->Decode(samples);
                     });
}

RefPtr<MediaDataDecoder::FlushPromise> RemoteMediaDataDecoder::Flush() {
  RefPtr<RemoteMediaDataDecoder> self = this;
  return InvokeAsync(RemoteDecoderManagerChild::GetManagerThread(), __func__,
                     [self]() { return self->mChild->Flush(); });
}

RefPtr<MediaDataDecoder::DecodePromise> RemoteMediaDataDecoder::Drain() {
  RefPtr<RemoteMediaDataDecoder> self = this;
  return InvokeAsync(RemoteDecoderManagerChild::GetManagerThread(), __func__,
                     [self]() { return self->mChild->Drain(); });
}

RefPtr<ShutdownPromise> RemoteMediaDataDecoder::Shutdown() {
  RefPtr<RemoteMediaDataDecoder> self = this;
  return InvokeAsync(
      RemoteDecoderManagerChild::GetManagerThread(), __func__, [self]() {
        RefPtr<ShutdownPromise> p = self->mChild->Shutdown();

        // We're about to be destroyed and drop our ref to
        // *DecoderChild. Make sure we put a ref into the
        // task queue for the *DecoderChild thread to keep
        // it alive until we send the delete message.
        p->Then(RemoteDecoderManagerChild::GetManagerThread(), __func__,
                [child = std::move(self->mChild)](
                    const ShutdownPromise::ResolveOrRejectValue& aValue) {
                  MOZ_ASSERT(aValue.IsResolve());
                  child->DestroyIPDL();
                  return ShutdownPromise::CreateAndResolveOrReject(aValue,
                                                                   __func__);
                });
        return p;
      });
}

bool RemoteMediaDataDecoder::IsHardwareAccelerated(
    nsACString& aFailureReason) const {
  aFailureReason = mHardwareAcceleratedReason;
  return mIsHardwareAccelerated;
}

void RemoteMediaDataDecoder::SetSeekThreshold(const media::TimeUnit& aTime) {
  RefPtr<RemoteMediaDataDecoder> self = this;
  media::TimeUnit time = aTime;
  RemoteDecoderManagerChild::GetManagerThread()->Dispatch(
      NS_NewRunnableFunction("dom::RemoteMediaDataDecoder::SetSeekThreshold",
                             [=]() {
                               MOZ_ASSERT(self->mChild);
                               self->mChild->SetSeekThreshold(time);
                             }),
      NS_DISPATCH_NORMAL);
}

MediaDataDecoder::ConversionRequired RemoteMediaDataDecoder::NeedsConversion()
    const {
  return mConversion;
}

nsCString RemoteMediaDataDecoder::GetDescriptionName() const {
  return mDescription;
}

}  // namespace mozilla
