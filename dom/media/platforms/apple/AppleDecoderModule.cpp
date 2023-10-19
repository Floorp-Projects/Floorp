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

using media::DecodeSupport;
using media::DecodeSupportSet;
using media::MCSInfo;
using media::MediaCodec;

bool AppleDecoderModule::sInitialized = false;
bool AppleDecoderModule::sCanUseVP9Decoder = false;

/* static */
void AppleDecoderModule::Init() {
  if (sInitialized) {
    return;
  }

  sInitialized = true;
  if (RegisterSupplementalVP9Decoder()) {
    sCanUseVP9Decoder = CanCreateHWDecoder(MediaCodec::VP9);
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
  if (Supports(SupportDecoderParams(aParams), nullptr /* diagnostics */)
          .isEmpty()) {
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
  if (Supports(SupportDecoderParams(aParams), nullptr /* diagnostics */)
          .isEmpty()) {
    return nullptr;
  }
  RefPtr<MediaDataDecoder> decoder = new AppleATDecoder(aParams.AudioConfig());
  return decoder.forget();
}

DecodeSupportSet AppleDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  bool checkSupport = (aMimeType.EqualsLiteral("audio/mpeg") &&
                       !StaticPrefs::media_ffvpx_mp3_enabled()) ||
                      aMimeType.EqualsLiteral("audio/mp4a-latm") ||
                      MP4Decoder::IsH264(aMimeType) ||
                      VPXDecoder::IsVP9(aMimeType);
  DecodeSupportSet supportType{};

  if (checkSupport) {
    UniquePtr<TrackInfo> trackInfo = CreateTrackInfoWithMIMEType(aMimeType);
    if (trackInfo && trackInfo->IsAudio()) {
      supportType = DecodeSupport::SoftwareDecode;
    } else if (trackInfo && trackInfo->IsVideo()) {
      supportType = Supports(SupportDecoderParams(*trackInfo), aDiagnostics);
    }
  }

  MOZ_LOG(sPDMLog, LogLevel::Debug,
          ("Apple decoder %s requested type '%s'",
           supportType.isEmpty() ? "rejects" : "supports",
           aMimeType.BeginReading()));
  return supportType;
}

DecodeSupportSet AppleDecoderModule::Supports(
    const SupportDecoderParams& aParams,
    DecoderDoctorDiagnostics* aDiagnostics) const {
  const auto& trackInfo = aParams.mConfig;
  if (trackInfo.IsAudio()) {
    return SupportsMimeType(trackInfo.mMimeType, aDiagnostics);
  }
  const bool checkSupport = trackInfo.GetAsVideoInfo() &&
                            IsVideoSupported(*trackInfo.GetAsVideoInfo());
  DecodeSupportSet dss{};
  if (!checkSupport) {
    return dss;
  }
  const MediaCodec codec =
      MCSInfo::GetMediaCodecFromMimeType(trackInfo.mMimeType);
  if (CanCreateHWDecoder(codec)) {
    dss += DecodeSupport::HardwareDecode;
  }
  switch (codec) {
    case MediaCodec::VP8:
      [[fallthrough]];
    case MediaCodec::VP9:
      if (StaticPrefs::media_ffvpx_enabled() &&
          StaticPrefs::media_rdd_vpx_enabled() &&
          StaticPrefs::media_utility_ffvpx_enabled()) {
        dss += DecodeSupport::SoftwareDecode;
      }
      break;
    default:
      dss += DecodeSupport::SoftwareDecode;
      break;
  }
  return dss;
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
  if (VPXDecoder::IsVP9(aConfig.mMimeType) &&
      aOptions.contains(CreateDecoderParams::Option::LowLatency)) {
    // SVC layers are unsupported, and may be used in low latency use cases
    // (WebRTC).
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
bool AppleDecoderModule::CanCreateHWDecoder(MediaCodec aCodec) {
  // Check whether HW decode should even be enabled
  if (!gfx::gfxVars::CanUseHardwareVideoDecoding()) {
    return false;
  }

  VideoInfo info(1920, 1080);
  bool vtReportsSupport = false;

  if (!VTIsHardwareDecodeSupported) {
    return false;
  }
  switch (aCodec) {
    case MediaCodec::VP9:
      info.mMimeType = "video/vp9";
      VPXDecoder::GetVPCCBox(info.mExtraData, VPXDecoder::VPXStreamInfo());
      vtReportsSupport = VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9);
      break;
    case MediaCodec::H264:
      // 1806391 - H264 hardware decode check crashes on 10.12 - 10.14
      if (__builtin_available(macos 10.15, *)) {
        info.mMimeType = "video/avc";
        vtReportsSupport = VTIsHardwareDecodeSupported(kCMVideoCodecType_H264);
      }
      break;
    default:
      vtReportsSupport = false;
      break;
  }
  // VT reports HW decode is supported -- verify by creating an actual decoder
  if (vtReportsSupport) {
    RefPtr<AppleVTDecoder> decoder =
        new AppleVTDecoder(info, nullptr, {}, nullptr, Nothing());
    MediaResult rv = decoder->InitializeSession();
    if (!NS_SUCCEEDED(rv)) {
      MOZ_LOG(
          sPDMLog, LogLevel::Debug,
          ("Apple HW decode failure while initializing VT decoder session"));
      return false;
    }
    nsAutoCString failureReason;
    // IsHardwareAccelerated appears to return invalid results for H.264 so
    // we assume that the earlier VTIsHardwareDecodeSupported call is correct.
    // See: https://bugzilla.mozilla.org/show_bug.cgi?id=1716196#c7
    bool hwSupport = decoder->IsHardwareAccelerated(failureReason) ||
                     aCodec == MediaCodec::H264;
    if (!hwSupport) {
      MOZ_LOG(sPDMLog, LogLevel::Debug,
              ("Apple HW decode failure: '%s'", failureReason.BeginReading()));
    }
    decoder->Shutdown();
    return hwSupport;
  }
  return false;
}

/* static */
bool AppleDecoderModule::RegisterSupplementalVP9Decoder() {
  static bool sRegisterIfAvailable = []() {
    if (__builtin_available(macos 11.0, *)) {
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
