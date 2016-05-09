/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(EMEDecoderModule_h_)
#define EMEDecoderModule_h_

#include "PlatformDecoderModule.h"
#include "PDMFactory.h"
#include "gmp-decryption.h"

namespace mozilla {

class CDMProxy;

class EMEDecoderModule : public PlatformDecoderModule {
private:

public:
  EMEDecoderModule(CDMProxy* aProxy, PDMFactory* aPDM);

  virtual ~EMEDecoderModule();

protected:
  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const VideoInfo& aConfig,
                    layers::LayersBackend aLayersBackend,
                    layers::ImageContainer* aImageContainer,
                    FlushableTaskQueue* aVideoTaskQueue,
                    MediaDataDecoderCallback* aCallback,
                    DecoderDoctorDiagnostics* aDiagnostics) override;

  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const AudioInfo& aConfig,
                     FlushableTaskQueue* aAudioTaskQueue,
                     MediaDataDecoderCallback* aCallback,
                     DecoderDoctorDiagnostics* aDiagnostics) override;

  ConversionRequired
  DecoderNeedsConversion(const TrackInfo& aConfig) const override;

  bool
  SupportsMimeType(const nsACString& aMimeType,
                   DecoderDoctorDiagnostics* aDiagnostics) const override;

private:
  RefPtr<CDMProxy> mProxy;
  // Will be null if CDM has decoding capability.
  RefPtr<PDMFactory> mPDM;
  // We run the PDM on its own task queue.
  RefPtr<TaskQueue> mTaskQueue;
};

} // namespace mozilla

#endif // EMEDecoderModule_h_
