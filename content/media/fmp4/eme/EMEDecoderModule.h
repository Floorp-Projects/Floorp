/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(EMEDecoderModule_h_)
#define EMEDecoderModule_h_

#include "PlatformDecoderModule.h"
#include "gmp-decryption.h"

namespace mozilla {

class CDMProxy;
class MediaTaskQueue;

class EMEDecoderModule : public PlatformDecoderModule {
private:
  typedef mp4_demuxer::AudioDecoderConfig AudioDecoderConfig;
  typedef mp4_demuxer::VideoDecoderConfig VideoDecoderConfig;

public:
  EMEDecoderModule(CDMProxy* aProxy,
                   PlatformDecoderModule* aPDM,
                   bool aCDMDecodesAudio,
                   bool aCDMDecodesVideo,
                   already_AddRefed<MediaTaskQueue> aDecodeTaskQueue);

  virtual ~EMEDecoderModule();

  // Called when the decoders have shutdown. Main thread only.
  virtual nsresult Shutdown() MOZ_OVERRIDE;

  // Decode thread.
  virtual already_AddRefed<MediaDataDecoder>
  CreateH264Decoder(const mp4_demuxer::VideoDecoderConfig& aConfig,
                    layers::LayersBackend aLayersBackend,
                    layers::ImageContainer* aImageContainer,
                    MediaTaskQueue* aVideoTaskQueue,
                    MediaDataDecoderCallback* aCallback) MOZ_OVERRIDE;

  // Decode thread.
  virtual already_AddRefed<MediaDataDecoder>
  CreateAACDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                   MediaTaskQueue* aAudioTaskQueue,
                   MediaDataDecoderCallback* aCallback) MOZ_OVERRIDE;

private:
  nsRefPtr<CDMProxy> mProxy;
  // Will be null if CDM has decoding capability.
  nsAutoPtr<PlatformDecoderModule> mPDM;
  // We run the PDM on its own task queue.
  nsRefPtr<MediaTaskQueue> mTaskQueue;
  bool mCDMDecodesAudio;
  bool mCDMDecodesVideo;

};

} // namespace mozilla

#endif // EMEDecoderModule_h_
