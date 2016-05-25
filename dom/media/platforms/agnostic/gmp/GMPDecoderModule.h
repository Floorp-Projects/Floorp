/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GMPDecoderModule_h_)
#define GMPDecoderModule_h_

#include "PlatformDecoderModule.h"
#include "mozilla/Maybe.h"

// The special NodeId we use when doing unencrypted decoding using the GMP's
// decoder. This ensures that each GMP MediaDataDecoder we create doesn't
// require spinning up a new process, but instead we run all instances of
// GMP decoders in the one process, to reduce overhead.
//
// Note: GMP storage is isolated by NodeId, and non persistent for this
// special NodeId, and the only way a GMP can communicate with the outside
// world is through the EME GMP APIs, and we never run EME with this NodeID
// (because NodeIds are random strings which can't contain the '-' character),
// so there's no way a malicious GMP can harvest, store, and then report any
// privacy sensitive data about what users are watching.
#define SHARED_GMP_DECODING_NODE_ID NS_LITERAL_CSTRING("gmp-shared-decoding")

namespace mozilla {

class GMPDecoderModule : public PlatformDecoderModule {
public:
  GMPDecoderModule();

  virtual ~GMPDecoderModule();

  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const VideoInfo& aConfig,
                     layers::LayersBackend aLayersBackend,
                     layers::ImageContainer* aImageContainer,
                     TaskQueue* aTaskQueue,
                     MediaDataDecoderCallback* aCallback,
                     DecoderDoctorDiagnostics* aDiagnostics) override;

  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const AudioInfo& aConfig,
                     TaskQueue* aTaskQueue,
                     MediaDataDecoderCallback* aCallback,
                     DecoderDoctorDiagnostics* aDiagnostics) override;

  ConversionRequired
  DecoderNeedsConversion(const TrackInfo& aConfig) const override;

  bool
  SupportsMimeType(const nsACString& aMimeType,
                   DecoderDoctorDiagnostics* aDiagnostics) const override;

  // Main thread only.
  static void Init();

  static const Maybe<nsCString> PreferredGMP(const nsACString& aMimeType);

  static bool SupportsMimeType(const nsACString& aMimeType,
                               const Maybe<nsCString>& aGMP);

  // Main thread only.
  static void UpdateUsableCodecs();
};

} // namespace mozilla

#endif // GMPDecoderModule_h_
