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

  // Called when the decoders have shutdown.
  virtual nsresult Shutdown() MOZ_OVERRIDE;

  // Decode thread.
  virtual already_AddRefed<MediaDataDecoder>
  CreateH264Decoder(const mp4_demuxer::VideoDecoderConfig& aConfig,
                    mozilla::layers::LayersBackend aLayersBackend,
                    mozilla::layers::ImageContainer* aImageContainer,
                    MediaTaskQueue* aVideoTaskQueue,
                    MediaDataDecoderCallback* aCallback) MOZ_OVERRIDE;

  // Decode thread.
  virtual already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                     MediaTaskQueue* aAudioTaskQueue,
                     MediaDataDecoderCallback* aCallback) MOZ_OVERRIDE;

  static void Init();
};

} // namespace mozilla

#endif
