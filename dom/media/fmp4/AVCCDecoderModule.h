/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AVCCDecoderModule_h
#define mozilla_AVCCDecoderModule_h

#include "PlatformDecoderModule.h"

namespace mozilla {

class AVCCMediaDataDecoder;

// AVCCDecoderModule is a PlatformDecoderModule wrapper used to ensure that
// only AVCC format is fed to the underlying PlatformDecoderModule.
// The AVCCDecoderModule allows playback of content where the SPS NAL may not be
// provided in the init segment (e.g. AVC3 or Annex B)
// AVCCDecoderModule will monitor the input data, and will delay creation of the
// MediaDataDecoder until a SPS and PPS NALs have been extracted.
//
// AVCC-only decoder modules are AppleVideoDecoder and EMEH264Decoder.

class AVCCDecoderModule : public PlatformDecoderModule {
public:
  explicit AVCCDecoderModule(PlatformDecoderModule* aPDM);
  virtual ~AVCCDecoderModule();

  virtual nsresult Startup() MOZ_OVERRIDE;
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

  virtual bool SupportsAudioMimeType(const char* aMimeType) MOZ_OVERRIDE;
  virtual bool SupportsVideoMimeType(const char* aMimeType) MOZ_OVERRIDE;

private:
  nsRefPtr<PlatformDecoderModule> mPDM;
};

} // namespace mozilla

#endif // mozilla_AVCCDecoderModule_h
