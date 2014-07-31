/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AppleDecoderModule_h
#define mozilla_AppleDecoderModule_h

#include "PlatformDecoderModule.h"

namespace mozilla {

class AppleDecoderModule : public PlatformDecoderModule {
public:
  AppleDecoderModule();
  virtual ~AppleDecoderModule();

  // Perform any per-instance initialization.
  // Main thread only.
  nsresult Startup();

  // Called when the decoders have shutdown. Main thread only.
  // Does this really need to be main thread only????
  virtual nsresult Shutdown() MOZ_OVERRIDE;

  // Decode thread.
  virtual MediaDataDecoder*
  CreateH264Decoder(const mp4_demuxer::VideoDecoderConfig& aConfig,
                    layers::LayersBackend aLayersBackend,
                    layers::ImageContainer* aImageContainer,
                    MediaTaskQueue* aVideoTaskQueue,
                    MediaDataDecoderCallback* aCallback) MOZ_OVERRIDE;

  // Decode thread.
  virtual MediaDataDecoder*
  CreateAACDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                   MediaTaskQueue* aAudioTaskQueue,
                   MediaDataDecoderCallback* aCallback) MOZ_OVERRIDE;

  static void Init();
private:
  static bool sIsEnabled;
};

} // namespace mozilla

#endif // mozilla_AppleDecoderModule_h
