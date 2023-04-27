/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MediaDataDecoderProxy_h_)
#  define MediaDataDecoderProxy_h_

#  include "PlatformDecoderModule.h"
#  include "mozilla/Atomics.h"
#  include "mozilla/RefPtr.h"
#  include "nsThreadUtils.h"
#  include "nscore.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(MediaDataDecoderProxy, MediaDataDecoder);

class MediaDataDecoderProxy
    : public MediaDataDecoder,
      public DecoderDoctorLifeLogger<MediaDataDecoderProxy> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDataDecoderProxy, final);

  explicit MediaDataDecoderProxy(
      already_AddRefed<MediaDataDecoder> aProxyDecoder,
      already_AddRefed<nsISerialEventTarget> aProxyThread = nullptr)
      : mProxyDecoder(aProxyDecoder), mProxyThread(aProxyThread) {
    DDLINKCHILD("proxy decoder", mProxyDecoder.get());
  }

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  bool CanDecodeBatch() const override;
  RefPtr<DecodePromise> DecodeBatch(
      nsTArray<RefPtr<MediaRawData>>&& aSamples) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  nsCString GetDescriptionName() const override;
  nsCString GetProcessName() const override;
  nsCString GetCodecName() const override;
  void SetSeekThreshold(const media::TimeUnit& aTime) override;
  bool SupportDecoderRecycling() const override;
  ConversionRequired NeedsConversion() const override;

 protected:
  ~MediaDataDecoderProxy() = default;

 private:
  // Set on construction and clear on the proxy thread if set.
  RefPtr<MediaDataDecoder> mProxyDecoder;
  const nsCOMPtr<nsISerialEventTarget> mProxyThread;

#  if defined(DEBUG)
  Atomic<bool> mIsShutdown = Atomic<bool>(false);
#  endif
};

}  // namespace mozilla

#endif  // MediaDataDecoderProxy_h_
