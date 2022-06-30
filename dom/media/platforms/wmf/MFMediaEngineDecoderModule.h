/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEDECODERMODULE_H
#define DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEDECODERMODULE_H

#include "PlatformDecoderModule.h"
#include "WMFUtils.h"

namespace mozilla {

class MFMediaEngineDecoderModule final : public PlatformDecoderModule {
 public:
  static void Init();

  static already_AddRefed<PlatformDecoderModule> Create();

  static bool SupportsConfig(const TrackInfo& aConfig);

  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
      const CreateDecoderParams& aParams) override;

  already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
      const CreateDecoderParams& aParams) override;

  media::DecodeSupportSet SupportsMimeType(
      const nsACString& aMimeType,
      DecoderDoctorDiagnostics* aDiagnostics) const override;
  media::DecodeSupportSet Supports(
      const SupportDecoderParams& aParams,
      DecoderDoctorDiagnostics* aDiagnostics) const override;

 private:
  media::DecodeSupportSet SupportInternal(
      const SupportDecoderParams& aParams,
      DecoderDoctorDiagnostics* aDiagnostics) const;
  bool CanCreateMFTDecoder(const WMFStreamType& aType) const;
  MFMediaEngineDecoderModule() = default;
  ~MFMediaEngineDecoderModule() = default;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINEDECODERMODULE_H
