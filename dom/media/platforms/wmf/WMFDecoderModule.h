/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WMFPlatformDecoderModule_h_)
#  define WMFPlatformDecoderModule_h_

#  include "PlatformDecoderModule.h"
#  include "WMF.h"
#  include "WMFUtils.h"

namespace mozilla {

class MFTDecoder;

class WMFDecoderModule : public PlatformDecoderModule {
 public:
  static already_AddRefed<PlatformDecoderModule> Create();

  // Initializes the module, loads required dynamic libraries, etc.
  nsresult Startup() override;

  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
      const CreateDecoderParams& aParams) override;

  already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
      const CreateDecoderParams& aParams) override;

  bool SupportsColorDepth(
      gfx::ColorDepth aColorDepth,
      DecoderDoctorDiagnostics* aDiagnostics) const override;
  media::DecodeSupportSet SupportsMimeType(
      const nsACString& aMimeType,
      DecoderDoctorDiagnostics* aDiagnostics) const override;
  media::DecodeSupportSet Supports(
      const SupportDecoderParams& aParams,
      DecoderDoctorDiagnostics* aDiagnostics) const override;

  enum class Config {
    None,
    ForceEnableHEVC,
  };

  // Called on main thread.
  static void Init(Config aConfig = Config::None);

  // Called from any thread, must call init first
  static int GetNumDecoderThreads();

  static HRESULT CreateMFTDecoder(const WMFStreamType& aType,
                                  RefPtr<MFTDecoder>& aDecoder);
  static bool CanCreateMFTDecoder(const WMFStreamType& aType);

 private:
  // This is used for GPU process only, where we can't set the preference
  // directly (it can only set in the parent process) So we need a way to force
  // enable the HEVC in order to report the support information via telemetry.
  static inline bool sForceEnableHEVC = false;

  static bool IsHEVCSupported();

  WMFDecoderModule() = default;
  virtual ~WMFDecoderModule() = default;
};

}  // namespace mozilla

#endif
