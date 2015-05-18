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

class EMEDecoderModule : public PlatformDecoderModule {
private:

public:
  EMEDecoderModule(CDMProxy* aProxy,
                   PlatformDecoderModule* aPDM,
                   bool aCDMDecodesAudio,
                   bool aCDMDecodesVideo);

  virtual ~EMEDecoderModule();

  // Decode thread.
  virtual already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const VideoInfo& aConfig,
                    layers::LayersBackend aLayersBackend,
                    layers::ImageContainer* aImageContainer,
                    FlushableMediaTaskQueue* aVideoTaskQueue,
                    MediaDataDecoderCallback* aCallback) override;

  // Decode thread.
  virtual already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const AudioInfo& aConfig,
                     FlushableMediaTaskQueue* aAudioTaskQueue,
                     MediaDataDecoderCallback* aCallback) override;

  virtual ConversionRequired
  DecoderNeedsConversion(const TrackInfo& aConfig) const override;

private:
  nsRefPtr<CDMProxy> mProxy;
  // Will be null if CDM has decoding capability.
  nsRefPtr<PlatformDecoderModule> mPDM;
  // We run the PDM on its own task queue.
  nsRefPtr<MediaTaskQueue> mTaskQueue;
  bool mCDMDecodesAudio;
  bool mCDMDecodesVideo;

};

} // namespace mozilla

#endif // EMEDecoderModule_h_
