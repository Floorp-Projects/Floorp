/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(AgnosticDecoderModule_h_)
#  define AgnosticDecoderModule_h_

#  include "PlatformDecoderModule.h"

namespace mozilla {

class AgnosticDecoderModule : public PlatformDecoderModule {
 public:
  AgnosticDecoderModule() = default;

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const override;

 protected:
  virtual ~AgnosticDecoderModule() = default;
  // Decode thread.
  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
      const CreateDecoderParams& aParams) override;

  // Decode thread.
  already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
      const CreateDecoderParams& aParams) override;
};

}  // namespace mozilla

#endif /* AgnosticDecoderModule_h_ */
