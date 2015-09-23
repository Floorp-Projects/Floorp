/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GonkPlatformDecoderModule_h_)
#define GonkPlatformDecoderModule_h_

#include "PlatformDecoderModule.h"

namespace mozilla {

class GonkDecoderModule : public PlatformDecoderModule {
public:
  GonkDecoderModule();
  virtual ~GonkDecoderModule();

  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const VideoInfo& aConfig,
                     mozilla::layers::LayersBackend aLayersBackend,
                     mozilla::layers::ImageContainer* aImageContainer,
                     FlushableTaskQueue* aVideoTaskQueue,
                     MediaDataDecoderCallback* aCallback) override;

  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const AudioInfo& aConfig,
                     FlushableTaskQueue* aAudioTaskQueue,
                     MediaDataDecoderCallback* aCallback) override;

  static void Init();

  ConversionRequired
  DecoderNeedsConversion(const TrackInfo& aConfig) const override;

  bool SupportsMimeType(const nsACString& aMimeType) override;

};

} // namespace mozilla

#endif
