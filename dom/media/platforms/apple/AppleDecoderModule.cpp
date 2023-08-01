/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppleDecoderModule.h"

#include <dlfcn.h>

#include "AppleATDecoder.h"
#include "AppleVTDecoder.h"
#include "MP4Decoder.h"
#include "VideoUtils.h"
#include "VPXDecoder.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/gfx/gfxVars.h"

extern "C" {
// Only exists from MacOS 11
extern void VTRegisterSupplementalVideoDecoderIfAvailable(
    CMVideoCodecType codecType) __attribute__((weak_import));
extern Boolean VTIsHardwareDecodeSupported(CMVideoCodecType codecType)
    __attribute__((weak_import));
}

namespace mozilla {

bool AppleDecoderModule::sInitialized = false;
bool AppleDecoderModule::sCanUseVP9Decoder = false;

/* static */
void AppleDecoderModule::Init() {
  if (sInitialized) {
    return;
  }

  sInitialized = true;
  if (RegisterSupplementalVP9Decoder()) {
    sCanUseVP9Decoder = CanCreateHWDecoder(media::MediaCodec::VP9);
  }
}

nsresult AppleDecoderModule::Startup() {
  if (!sInitialized) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

already_AddRefed<MediaDataDecoder> AppleDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  if (Supports(SupportDecoderParams(aParams), nullptr /* diagnostics */) ==
      media::DecodeSupport::Unsupported) {
    return nullptr;
  }
  RefPtr<MediaDataDecoder> decoder;
  if (IsVideoSupported(aParams.VideoConfig(), aParams.mOptions)) {
    decoder = new AppleVTDecoder(aParams.VideoConfig(), aParams.mImageContainer,
                                 aParams.mOptions, aParams.mKnowsCompositor,
                                 aParams.mTrackingId);
  }
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder> AppleDecoderModule::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  if (Supports(SupportDecoderParams(aParams), nullptr /* diagnostics */) ==
      media::DecodeSupport::Unsupported) {
    return nullptr;
  }
  RefPtr<MediaDataDecoder> decoder = new AppleATDecoder(aParams.AudioConfig());
  return decoder.forget();
}

media::DecodeSupportSet AppleDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  bool checkSupport = (aMimeType.EqualsLiteral("audio/mpeg") &&
                       !StaticPrefs::media_ffvpx_mp3_enabled()) ||
                      aMimeType.EqualsLiteral("audio/mp4a-latm") ||
                      MP4Decoder::IsH264(aMimeType) ||
                      VPXDecoder::IsVP9(aMimeType);
  media::DecodeSupportSet supportType{media::DecodeSupport::Unsupported};

  if (checkSupport) {
    UniquePtr<TrackInfo> trackInfo = CreateTrackInfoWithMIMEType(aMimeType);
    if (!trackInfo) {
      supportType = media::DecodeSupport::Unsupported;
    } else if (trackInfo->IsAudio()) {
      supportType = media::DecodeSupport::SoftwareDecode;
    } else {
      supportType = Supports(SupportDecoderParams(*trackInfo), aDiagnostics);
    }
  }

  MOZ_LOG(sPDMLog, LogLevel::Debug,
          ("Apple decoder %s requested type '%s'",
           supportType == media::DecodeSupport::Unsupported ? "rejects"
                                                            : "supports",
           aMimeType.BeginReading()));
  return supportType;
}

media::DecodeSupportSet AppleDecoderModule::Supports(
    const SupportDecoderParams& aParams,
    DecoderDoctorDiagnostics* aDiagnostics) const {
  const auto& trackInfo = aParams.mConfig;
  if (trackInfo.IsAudio()) {
    return SupportsMimeType(trackInfo.mMimeType, aDiagnostics);
  }
  bool checkSupport = trackInfo.GetAsVideoInfo() &&
                      IsVideoSupported(*trackInfo.GetAsVideoInfo());
  if (checkSupport) {
    if (trackInfo.mMimeType == "video/vp9" &&
        CanCreateHWDecoder(media::MediaCodec::VP9)) {
      return media::DecodeSupport::HardwareDecode;
    }
    return media::DecodeSupport::SoftwareDecode;
  }
  return media::DecodeSupport::Unsupported;
}

bool AppleDecoderModule::IsVideoSupported(
    const VideoInfo& aConfig,
    const CreateDecoderParams::OptionSet& aOptions) const {
  if (MP4Decoder::IsH264(aConfig.mMimeType)) {
    return true;
  }
  if (!VPXDecoder::IsVP9(aConfig.mMimeType) || !sCanUseVP9Decoder ||
      aOptions.contains(
          CreateDecoderParams::Option::HardwareDecoderNotAllowed)) {
    return false;
  }
  if (aConfig.HasAlpha()) {
    return false;
  }

  // HW VP9 decoder only supports 8 or 10 bit color.
  if (aConfig.mColorDepth != gfx::ColorDepth::COLOR_8 &&
      aConfig.mColorDepth != gfx::ColorDepth::COLOR_10) {
    return false;
  }

  // See if we have a vpcC box, and check further constraints.
  // HW VP9 Decoder supports Profile 0 & 2 (YUV420)
  if (aConfig.mExtraData && aConfig.mExtraData->Length() < 5) {
    return true;  // Assume it's okay.
  }
  int profile = aConfig.mExtraData->ElementAt(4);

  if (profile != 0 && profile != 2) {
    return false;
  }

  return true;
}

/* static */
bool AppleDecoderModule::CanCreateHWDecoder(media::MediaCodec aCodec) {
  // Check whether HW decode should even be enabled
  if (!gfx::gfxVars::CanUseHardwareVideoDecoding()) {
    return false;
  }

  VideoInfo info(1920, 1080);
  bool checkSupport = false;

  // We must wrap the code within __builtin_available to avoid compilation
  // warning as VTIsHardwareDecodeSupported is only available from macOS 10.13.
  if (__builtin_available(macOS 10.13, *)) {
    if (!VTIsHardwareDecodeSupported) {
      return false;
    }
    switch (aCodec) {
      case media::MediaCodec::VP9:
        info.mMimeType = "video/vp9";
        VPXDecoder::GetVPCCBox(info.mExtraData, VPXDecoder::VPXStreamInfo());
        checkSupport = VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9);
        break;
      default:
        // Only support VP9 HW decode for time being
        checkSupport = false;
        break;
    }
  }
  // Attempt to create decoder
  if (checkSupport) {
    RefPtr<AppleVTDecoder> decoder =
        new AppleVTDecoder(info, nullptr, {}, nullptr, Nothing());
    MediaResult rv = decoder->InitializeSession();
    if (!NS_SUCCEEDED(rv)) {
      return false;
    }
    nsAutoCString failureReason;
    bool hwSupport = decoder->IsHardwareAccelerated(failureReason);
    decoder->Shutdown();
    if (!hwSupport) {
      MOZ_LOG(sPDMLog, LogLevel::Debug,
              ("Apple HW decode failure: '%s'", failureReason.BeginReading()));
    }
    return hwSupport;
  }
  return false;
}

/* static */
bool AppleDecoderModule::RegisterSupplementalVP9Decoder() {
  static bool sRegisterIfAvailable = []() {
#if !defined(MAC_OS_VERSION_11_0) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_VERSION_11_0
    if (nsCocoaFeatures::OnBigSurOrLater()) {
#else
    if (__builtin_available(macos 11.0, *)) {
#endif
      VTRegisterSupplementalVideoDecoderIfAvailable(kCMVideoCodecType_VP9);
      return true;
    }
    return false;
  }();
  return sRegisterIfAvailable;
}

/* static */
already_AddRefed<PlatformDecoderModule> AppleDecoderModule::Create() {
  return MakeAndAddRef<AppleDecoderModule>();
}

}  // namespace mozilla
