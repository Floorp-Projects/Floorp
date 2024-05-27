/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDataEncoderProxy.h"

namespace mozilla {

MediaDataEncoderProxy::MediaDataEncoderProxy(
    already_AddRefed<MediaDataEncoder> aProxyEncoder,
    already_AddRefed<nsISerialEventTarget> aProxyThread)
    : mProxyEncoder(aProxyEncoder), mProxyThread(aProxyThread) {}

MediaDataEncoderProxy::~MediaDataEncoderProxy() = default;

RefPtr<MediaDataEncoder::InitPromise> MediaDataEncoderProxy::Init() {
  MOZ_ASSERT(!mIsShutdown);

  if (!mProxyThread || mProxyThread->IsOnCurrentThread()) {
    return mProxyEncoder->Init();
  }
  return InvokeAsync(mProxyThread, __func__, [self = RefPtr{this}] {
    return self->mProxyEncoder->Init();
  });
}

RefPtr<MediaDataEncoder::EncodePromise> MediaDataEncoderProxy::Encode(
    const MediaData* aSample) {
  MOZ_ASSERT(!mIsShutdown);

  if (!mProxyThread || mProxyThread->IsOnCurrentThread()) {
    return mProxyEncoder->Encode(aSample);
  }
  return InvokeAsync(mProxyThread, __func__,
                     [self = RefPtr{this}, sample = RefPtr{aSample}] {
                       return self->mProxyEncoder->Encode(sample);
                     });
}

RefPtr<MediaDataEncoder::ReconfigurationPromise>
MediaDataEncoderProxy::Reconfigure(
    const RefPtr<const EncoderConfigurationChangeList>& aConfigurationChanges) {
  MOZ_ASSERT(!mIsShutdown);

  if (!mProxyThread || mProxyThread->IsOnCurrentThread()) {
    return mProxyEncoder->Reconfigure(aConfigurationChanges);
  }
  return InvokeAsync(
      mProxyThread, __func__,
      [self = RefPtr{this}, changes = RefPtr{aConfigurationChanges}] {
        return self->mProxyEncoder->Reconfigure(changes);
      });
}

RefPtr<MediaDataEncoder::EncodePromise> MediaDataEncoderProxy::Drain() {
  MOZ_ASSERT(!mIsShutdown);

  if (!mProxyThread || mProxyThread->IsOnCurrentThread()) {
    return mProxyEncoder->Drain();
  }
  return InvokeAsync(mProxyThread, __func__, [self = RefPtr{this}] {
    return self->mProxyEncoder->Drain();
  });
}

RefPtr<ShutdownPromise> MediaDataEncoderProxy::Shutdown() {
  MOZ_ASSERT(!mIsShutdown);

#if defined(DEBUG)
  mIsShutdown = true;
#endif

  RefPtr<MediaDataEncoder> proxyEncoder = std::move(mProxyEncoder);
  if (!mProxyThread || mProxyThread->IsOnCurrentThread()) {
    return proxyEncoder->Shutdown();
  }
  // We chain another promise to ensure that the proxied encoder gets destructed
  // on the proxy thread.
  return InvokeAsync(
      mProxyThread, __func__,
      [self = RefPtr{this}, proxyEncoder = std::move(proxyEncoder)] {
        return proxyEncoder->Shutdown();
      });
}

RefPtr<GenericPromise> MediaDataEncoderProxy::SetBitrate(uint32_t aBitsPerSec) {
  MOZ_ASSERT(!mIsShutdown);

  if (!mProxyThread || mProxyThread->IsOnCurrentThread()) {
    return mProxyEncoder->SetBitrate(aBitsPerSec);
  }
  return InvokeAsync(mProxyThread, __func__,
                     [self = RefPtr{this}, bitsPerSec = aBitsPerSec] {
                       return self->mProxyEncoder->SetBitrate(bitsPerSec);
                     });
}

bool MediaDataEncoderProxy::IsHardwareAccelerated(
    nsACString& aFailureReason) const {
  MOZ_ASSERT(!mIsShutdown);

  return mProxyEncoder->IsHardwareAccelerated(aFailureReason);
}

nsCString MediaDataEncoderProxy::GetDescriptionName() const {
  MOZ_ASSERT(!mIsShutdown);

  return mProxyEncoder->GetDescriptionName();
}

}  // namespace mozilla
