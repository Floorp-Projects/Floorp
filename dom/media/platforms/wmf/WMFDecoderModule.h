/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WMFPlatformDecoderModule_h_)
#  define WMFPlatformDecoderModule_h_

#  include "PlatformDecoderModule.h"
#  include "WMF.h"

namespace mozilla {

class MFTDecoder;

enum class WMFStreamType { Unknown, H264, VP8, VP9, AV1, MP3, AAC, SENTINEL };

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

  // Called on main thread.
  static void Init();

  // Called from any thread, must call init first
  static int GetNumDecoderThreads();

  static HRESULT CreateMFTDecoder(const WMFStreamType& aType,
                                  RefPtr<MFTDecoder>& aDecoder);
  static bool CanCreateMFTDecoder(const WMFStreamType& aType);
  static WMFStreamType GetStreamTypeFromMimeType(const nsCString& aMimeType);

  static inline bool StreamTypeIsVideo(const WMFStreamType& aType) {
    switch (aType) {
      case WMFStreamType::H264:
      case WMFStreamType::VP8:
      case WMFStreamType::VP9:
      case WMFStreamType::AV1:
        return true;
      default:
        return false;
    }
  }

  static inline bool StreamTypeIsAudio(const WMFStreamType& aType) {
    switch (aType) {
      case WMFStreamType::MP3:
      case WMFStreamType::AAC:
        return true;
      default:
        return false;
    }
  }

  // Get a string representation of the stream type. Useful for logging.
  static inline const char* StreamTypeToString(WMFStreamType aStreamType) {
    switch (aStreamType) {
      case WMFStreamType::H264:
        return "H264";
      case WMFStreamType::VP8:
        return "VP8";
      case WMFStreamType::VP9:
        return "VP9";
      case WMFStreamType::AV1:
        return "AV1";
      case WMFStreamType::MP3:
        return "MP3";
      case WMFStreamType::AAC:
        return "AAC";
      default:
        MOZ_ASSERT(aStreamType == WMFStreamType::Unknown);
        return "Unknown";
    }
  }

 private:
  WMFDecoderModule() = default;
  virtual ~WMFDecoderModule() = default;
};

}  // namespace mozilla

#endif
