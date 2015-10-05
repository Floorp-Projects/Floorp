/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(PDMFactory_h_)
#define PDMFactory_h_

#include "PlatformDecoderModule.h"

namespace mozilla {

class PDMFactory : public PlatformDecoderModule {
public:
  PDMFactory();
  virtual ~PDMFactory();

  // Call on the main thread to initialize the static state
  // needed by Create().
  static void Init();

  already_AddRefed<MediaDataDecoder>
  CreateDecoder(const TrackInfo& aConfig,
                FlushableTaskQueue* aTaskQueue,
                MediaDataDecoderCallback* aCallback,
                layers::LayersBackend aLayersBackend = layers::LayersBackend::LAYERS_NONE,
                layers::ImageContainer* aImageContainer = nullptr) override;

  bool SupportsMimeType(const nsACString& aMimeType) override;

  ConversionRequired
  DecoderNeedsConversion(const TrackInfo& aConfig) const override
  {
    MOZ_CRASH("Should never reach this function");
    return ConversionRequired::kNeedNone;
  }

protected:
  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const VideoInfo& aConfig,
                     layers::LayersBackend aLayersBackend,
                     layers::ImageContainer* aImageContainer,
                     FlushableTaskQueue* aVideoTaskQueue,
                     MediaDataDecoderCallback* aCallback) override
  {
    MOZ_CRASH("Should never reach this function");
    return nullptr;
  }

  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const AudioInfo& aConfig,
                     FlushableTaskQueue* aAudioTaskQueue,
                     MediaDataDecoderCallback* aCallback) override
  {
    MOZ_CRASH("Should never reach this function");
    return nullptr;
  }

private:
  already_AddRefed<PlatformDecoderModule> CreatePDM();

  // Caches pref media.fragmented-mp4.use-blank-decoder
  static bool sUseBlankDecoder;
  static bool sGonkDecoderEnabled;
  static bool sAndroidMCDecoderPreferred;
  static bool sAndroidMCDecoderEnabled;
  static bool sGMPDecoderEnabled;
  static bool sEnableFuzzingWrapper;
  static uint32_t sVideoOutputMinimumInterval_ms;
  static bool sDontDelayInputExhausted;

  nsRefPtr<PlatformDecoderModule> mCurrentPDM;
};

} // namespace mozilla

#endif /* PDMFactory_h_ */
