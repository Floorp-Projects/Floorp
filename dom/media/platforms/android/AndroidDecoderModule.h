/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidDecoderModule_h_
#define AndroidDecoderModule_h_

#include "PlatformDecoderModule.h"
#include "mozilla/MediaDrmCDMProxy.h"

namespace mozilla {

class AndroidDecoderModule : public PlatformDecoderModule {
public:
  already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const CreateDecoderParams& aParams) override;

  already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const CreateDecoderParams& aParams) override;

  AndroidDecoderModule(CDMProxy* aProxy = nullptr);
  virtual ~AndroidDecoderModule() {}

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const override;

  ConversionRequired
  DecoderNeedsConversion(const TrackInfo& aConfig) const override;

private:
  RefPtr<MediaDrmCDMProxy> mProxy;
};

extern LazyLogModule sAndroidDecoderModuleLog;

} // namespace mozilla

#endif
