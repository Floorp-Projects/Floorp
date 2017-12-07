/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaDataDecoderProxy_h_)
#define MediaDataDecoderProxy_h_

#include "PlatformDecoderModule.h"
#include "mozilla/Atomics.h"
#include "mozilla/RefPtr.h"
#include "nsThreadUtils.h"
#include "nscore.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(MediaDataDecoderProxy, MediaDataDecoder);

class MediaDataDecoderProxy
  : public MediaDataDecoder
  , public DecoderDoctorLifeLogger<MediaDataDecoderProxy>
{
public:
  explicit MediaDataDecoderProxy(already_AddRefed<AbstractThread> aProxyThread)
    : mProxyThread(aProxyThread)
#if defined(DEBUG)
    , mIsShutdown(false)
#endif
  {
  }

  explicit MediaDataDecoderProxy(
    already_AddRefed<MediaDataDecoder> aProxyDecoder)
    : mProxyDecoder(aProxyDecoder)
#if defined(DEBUG)
    , mIsShutdown(false)
#endif
  {
    DDLINKCHILD("proxy decoder", mProxyDecoder.get());
  }

  void SetProxyTarget(MediaDataDecoder* aProxyDecoder)
  {
    MOZ_ASSERT(aProxyDecoder);
    mProxyDecoder = aProxyDecoder;
    DDLINKCHILD("proxy decoder", aProxyDecoder);
  }

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override;
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  void SetSeekThreshold(const media::TimeUnit& aTime) override;
  bool SupportDecoderRecycling() const override;
  ConversionRequired NeedsConversion() const override;

private:
  RefPtr<MediaDataDecoder> mProxyDecoder;
  RefPtr<AbstractThread> mProxyThread;

#if defined(DEBUG)
  Atomic<bool> mIsShutdown;
#endif
};

} // namespace mozilla

#endif // MediaDataDecoderProxy_h_
