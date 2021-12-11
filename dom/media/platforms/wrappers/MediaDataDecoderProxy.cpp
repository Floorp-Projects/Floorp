/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDataDecoderProxy.h"

namespace mozilla {

RefPtr<MediaDataDecoder::InitPromise> MediaDataDecoderProxy::Init() {
  MOZ_ASSERT(!mIsShutdown);

  if (!mProxyThread || mProxyThread->IsOnCurrentThread()) {
    return mProxyDecoder->Init();
  }
  return InvokeAsync(mProxyThread, __func__, [self = RefPtr{this}] {
    return self->mProxyDecoder->Init();
  });
}

RefPtr<MediaDataDecoder::DecodePromise> MediaDataDecoderProxy::Decode(
    MediaRawData* aSample) {
  MOZ_ASSERT(!mIsShutdown);

  if (!mProxyThread || mProxyThread->IsOnCurrentThread()) {
    return mProxyDecoder->Decode(aSample);
  }
  RefPtr<MediaRawData> sample = aSample;
  return InvokeAsync(mProxyThread, __func__, [self = RefPtr{this}, sample] {
    return self->mProxyDecoder->Decode(sample);
  });
}

bool MediaDataDecoderProxy::CanDecodeBatch() const {
  return mProxyDecoder->CanDecodeBatch();
}

RefPtr<MediaDataDecoder::DecodePromise> MediaDataDecoderProxy::DecodeBatch(
    nsTArray<RefPtr<MediaRawData>>&& aSamples) {
  MOZ_ASSERT(!mIsShutdown);
  if (!mProxyThread || mProxyThread->IsOnCurrentThread()) {
    return mProxyDecoder->DecodeBatch(std::move(aSamples));
  }
  return InvokeAsync(
      mProxyThread, __func__,
      [self = RefPtr{this}, samples = std::move(aSamples)]() mutable {
        return self->mProxyDecoder->DecodeBatch(std::move(samples));
      });
}

RefPtr<MediaDataDecoder::FlushPromise> MediaDataDecoderProxy::Flush() {
  MOZ_ASSERT(!mIsShutdown);

  if (!mProxyThread || mProxyThread->IsOnCurrentThread()) {
    return mProxyDecoder->Flush();
  }
  return InvokeAsync(mProxyThread, __func__, [self = RefPtr{this}] {
    return self->mProxyDecoder->Flush();
  });
}

RefPtr<MediaDataDecoder::DecodePromise> MediaDataDecoderProxy::Drain() {
  MOZ_ASSERT(!mIsShutdown);

  if (!mProxyThread || mProxyThread->IsOnCurrentThread()) {
    return mProxyDecoder->Drain();
  }
  return InvokeAsync(mProxyThread, __func__, [self = RefPtr{this}] {
    return self->mProxyDecoder->Drain();
  });
}

RefPtr<ShutdownPromise> MediaDataDecoderProxy::Shutdown() {
  MOZ_ASSERT(!mIsShutdown);

#if defined(DEBUG)
  mIsShutdown = true;
#endif

  if (!mProxyThread || mProxyThread->IsOnCurrentThread()) {
    return mProxyDecoder->Shutdown();
  }
  // We chain another promise to ensure that the proxied decoder gets destructed
  // on the proxy thread.
  return InvokeAsync(mProxyThread, __func__, [self = RefPtr{this}] {
    RefPtr<ShutdownPromise> p = self->mProxyDecoder->Shutdown()->Then(
        self->mProxyThread, __func__,
        [self](const ShutdownPromise::ResolveOrRejectValue& aResult) {
          self->mProxyDecoder = nullptr;
          return ShutdownPromise::CreateAndResolveOrReject(aResult, __func__);
        });
    return p;
  });
}

nsCString MediaDataDecoderProxy::GetDescriptionName() const {
  MOZ_ASSERT(!mIsShutdown);

  return mProxyDecoder->GetDescriptionName();
}

bool MediaDataDecoderProxy::IsHardwareAccelerated(
    nsACString& aFailureReason) const {
  MOZ_ASSERT(!mIsShutdown);

  return mProxyDecoder->IsHardwareAccelerated(aFailureReason);
}

void MediaDataDecoderProxy::SetSeekThreshold(const media::TimeUnit& aTime) {
  MOZ_ASSERT(!mIsShutdown);

  if (!mProxyThread || mProxyThread->IsOnCurrentThread()) {
    mProxyDecoder->SetSeekThreshold(aTime);
    return;
  }
  media::TimeUnit time = aTime;
  mProxyThread->Dispatch(NS_NewRunnableFunction(
      "MediaDataDecoderProxy::SetSeekThreshold", [self = RefPtr{this}, time] {
        self->mProxyDecoder->SetSeekThreshold(time);
      }));
}

bool MediaDataDecoderProxy::SupportDecoderRecycling() const {
  MOZ_ASSERT(!mIsShutdown);

  return mProxyDecoder->SupportDecoderRecycling();
}

MediaDataDecoder::ConversionRequired MediaDataDecoderProxy::NeedsConversion()
    const {
  MOZ_ASSERT(!mIsShutdown);

  return mProxyDecoder->NeedsConversion();
}

}  // namespace mozilla
