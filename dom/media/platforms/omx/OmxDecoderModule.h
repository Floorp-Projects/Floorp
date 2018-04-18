/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(OmxDecoderModule_h_)
#define OmxDecoderModule_h_

#include "PlatformDecoderModule.h"

namespace mozilla {

class OmxDecoderModule : public PlatformDecoderModule
{
public:
  // Called on main thread.
  static bool Init();
  static OmxDecoderModule* Create();

  already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const CreateDecoderParams& aParams) override;

  already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const CreateDecoderParams& aParams) override;

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const override;
};

} // namespace mozilla

#endif // OmxDecoderModule_h_
