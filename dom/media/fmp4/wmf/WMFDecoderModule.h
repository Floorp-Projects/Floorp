/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WMFPlatformDecoderModule_h_)
#define WMFPlatformDecoderModule_h_

#include "PlatformDecoderModule.h"

namespace mozilla {

class WMFDecoderModule : public PlatformDecoderModule {
public:
  WMFDecoderModule();
  virtual ~WMFDecoderModule();

  // Initializes the module, loads required dynamic libraries, etc.
  virtual nsresult Startup() MOZ_OVERRIDE;

  // Called when the decoders have shutdown.
  virtual nsresult Shutdown() MOZ_OVERRIDE;

  virtual already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const mp4_demuxer::VideoDecoderConfig& aConfig,
                     layers::LayersBackend aLayersBackend,
                     layers::ImageContainer* aImageContainer,
                     FlushableMediaTaskQueue* aVideoTaskQueue,
                     MediaDataDecoderCallback* aCallback) MOZ_OVERRIDE;

  virtual already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                     FlushableMediaTaskQueue* aAudioTaskQueue,
                     MediaDataDecoderCallback* aCallback) MOZ_OVERRIDE;

  bool SupportsVideoMimeType(const char* aMimeType) MOZ_OVERRIDE;
  bool SupportsAudioMimeType(const char* aMimeType) MOZ_OVERRIDE;

  // Accessors that report whether we have the required MFTs available
  // on the system to play various codecs. Windows Vista doesn't have the
  // H.264/AAC decoders if the "Platform Update Supplement for Windows Vista"
  // is not installed.
  static bool HasAAC();
  static bool HasH264();

  // Called on main thread.
  static void Init();
private:
  static bool sIsWMFEnabled;
  static bool sDXVAEnabled;
};

} // namespace mozilla

#endif
