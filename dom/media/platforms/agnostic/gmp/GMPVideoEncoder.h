/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GMPVideoEncoder_h_
#define mozilla_GMPVideoEncoder_h_

#include "GMPVideoEncoderProxy.h"
#include "mozilla/StaticString.h"
#include "mozIGeckoMediaPluginService.h"
#include "nsRefPtrHashtable.h"
#include "PlatformEncoderModule.h"
#include "TimeUnits.h"

class GMPVideoHost;

namespace mozilla {

class GMPVideoEncoder final : public MediaDataEncoder,
                              public GMPVideoEncoderCallbackProxy {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPVideoEncoder, final);

  explicit GMPVideoEncoder(const EncoderConfig& aConfig) : mConfig(aConfig) {
    MOZ_ASSERT(mConfig.mSize.width > 0 && mConfig.mSize.height > 0);
  }

  RefPtr<InitPromise> Init() override;
  RefPtr<EncodePromise> Encode(const MediaData* aSample) override;
  RefPtr<ReconfigurationPromise> Reconfigure(
      const RefPtr<const EncoderConfigurationChangeList>& aConfigurationChanges)
      override;
  RefPtr<EncodePromise> Drain() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  RefPtr<GenericPromise> SetBitrate(uint32_t aBitsPerSec) override;

  void Encoded(GMPVideoEncodedFrame* aEncodedFrame,
               const nsTArray<uint8_t>& aCodecSpecificInfo) override;
  void Error(GMPErr aError) override;
  void Terminated() override;

  nsCString GetDescriptionName() const override {
    return "GMP Video Encoder"_ns;
  }

 private:
  class InitDoneCallback final : public GetGMPVideoEncoderCallback {
   public:
    explicit InitDoneCallback(GMPVideoEncoder* aEncoder) : mEncoder(aEncoder) {}

    void Done(GMPVideoEncoderProxy* aGMP, GMPVideoHost* aHost) override {
      mEncoder->InitComplete(aGMP, aHost);
    }

   private:
    RefPtr<GMPVideoEncoder> mEncoder;
  };

  virtual ~GMPVideoEncoder() = default;
  void InitComplete(GMPVideoEncoderProxy* aGMP, GMPVideoHost* aHost);

  bool IsInitialized() const { return mGMP && mHost; }

  void Teardown(const MediaResult& aResult, StaticString aCallSite);

  const EncoderConfig mConfig;
  nsCOMPtr<mozIGeckoMediaPluginService> mMPS;
  GMPVideoEncoderProxy* mGMP = nullptr;
  GMPVideoHost* mHost = nullptr;
  MozPromiseHolder<InitPromise> mInitPromise;
  MozPromiseHolder<EncodePromise> mDrainPromise;

  using PendingEncodePromises =
      nsRefPtrHashtable<nsUint64HashKey, EncodePromise::Private>;
  PendingEncodePromises mPendingEncodes;
};

}  // namespace mozilla

#endif  // mozilla_GMPVideoEncoder_h_
