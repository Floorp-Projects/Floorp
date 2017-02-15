/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDataDecoderProxy.h"

namespace mozilla {

RefPtr<MediaDataDecoder::InitPromise>
MediaDataDecoderProxy::Init()
{
  MOZ_ASSERT(!mIsShutdown);

  if (!mProxyThread) {
    return mProxyDecoder->Init();
  }
  RefPtr<MediaDataDecoderProxy> self = this;
  return InvokeAsync(mProxyThread, __func__,
                     [self, this]() { return mProxyDecoder->Init(); });
}

RefPtr<MediaDataDecoder::DecodePromise>
MediaDataDecoderProxy::Decode(MediaRawData* aSample)
{
  MOZ_ASSERT(!mIsShutdown);

  if (!mProxyThread) {
    return mProxyDecoder->Decode(aSample);
  }
  RefPtr<MediaDataDecoderProxy> self = this;
  RefPtr<MediaRawData> sample = aSample;
  return InvokeAsync(mProxyThread, __func__, [self, this, sample]() {
    return mProxyDecoder->Decode(sample);
  });
}

RefPtr<MediaDataDecoder::FlushPromise>
MediaDataDecoderProxy::Flush()
{
  MOZ_ASSERT(!mIsShutdown);

  if (!mProxyThread) {
    return mProxyDecoder->Flush();
  }
  RefPtr<MediaDataDecoderProxy> self = this;
  return InvokeAsync(mProxyThread, __func__,
                     [self, this]() { return mProxyDecoder->Flush(); });
}

RefPtr<MediaDataDecoder::DecodePromise>
MediaDataDecoderProxy::Drain()
{
  MOZ_ASSERT(!mIsShutdown);

  if (!mProxyThread) {
    return mProxyDecoder->Drain();
  }
  RefPtr<MediaDataDecoderProxy> self = this;
  return InvokeAsync(mProxyThread, __func__,
                     [self, this]() { return mProxyDecoder->Drain(); });
}

RefPtr<ShutdownPromise>
MediaDataDecoderProxy::Shutdown()
{
  MOZ_ASSERT(!mIsShutdown);
#if defined(DEBUG)
  mIsShutdown = true;
#endif

  if (!mProxyThread) {
    return mProxyDecoder->Shutdown();
  }
  RefPtr<MediaDataDecoderProxy> self = this;
  return InvokeAsync(mProxyThread, __func__,
                     [self, this]() { return mProxyDecoder->Shutdown(); });
}

} // namespace mozilla
