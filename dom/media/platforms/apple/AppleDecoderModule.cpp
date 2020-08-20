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
#include "VPXDecoder.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
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
bool AppleDecoderModule::sCanUseHardwareVideoDecoder = true;
bool AppleDecoderModule::sCanUseVP9Decoder = false;

AppleDecoderModule::AppleDecoderModule() {}

AppleDecoderModule::~AppleDecoderModule() {}

/* static */
void AppleDecoderModule::Init() {
  if (sInitialized) {
    return;
  }

  sCanUseHardwareVideoDecoder = gfx::gfxVars::CanUseHardwareVideoDecoding();

  sInitialized = true;
  if (RegisterSupplementalVP9Decoder()) {
    sCanUseVP9Decoder = CanCreateVP9Decoder();
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
  RefPtr<MediaDataDecoder> decoder;
  if (IsVideoSupported(aParams.VideoConfig(), aParams.mOptions)) {
    decoder = new AppleVTDecoder(aParams.VideoConfig(), aParams.mImageContainer,
                                 aParams.mOptions, aParams.mKnowsCompositor);
  }
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder> AppleDecoderModule::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  RefPtr<MediaDataDecoder> decoder = new AppleATDecoder(aParams.AudioConfig());
  return decoder.forget();
}

bool AppleDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  return aMimeType.EqualsLiteral("audio/mpeg") ||
         aMimeType.EqualsLiteral("audio/mp4a-latm") ||
         MP4Decoder::IsH264(aMimeType) || VPXDecoder::IsVP9(aMimeType);
}

bool AppleDecoderModule::Supports(
    const TrackInfo& aTrackInfo, DecoderDoctorDiagnostics* aDiagnostics) const {
  if (aTrackInfo.IsAudio()) {
    return SupportsMimeType(aTrackInfo.mMimeType, aDiagnostics);
  }
  return aTrackInfo.GetAsVideoInfo() &&
         IsVideoSupported(*aTrackInfo.GetAsVideoInfo());
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
bool AppleDecoderModule::CanCreateVP9Decoder() {
  // We must wrap the code within __builtin_available to avoid compilation
  // warning as VTIsHardwareDecodeSupported is only available from macOS 10.13.
  if (__builtin_available(macOS 10.13, *)) {
    // Only use VP9 decoder if it's hardware accelerated.
    if (!sCanUseHardwareVideoDecoder || !VTIsHardwareDecodeSupported ||
        !VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9)) {
      return false;
    }

    VideoInfo info(1920, 1080);
    info.mMimeType = "video/vp9";
    VPXDecoder::GetVPCCBox(info.mExtraData, VPXDecoder::VPXStreamInfo());

    RefPtr<AppleVTDecoder> decoder =
        new AppleVTDecoder(info, nullptr, {}, nullptr);
    nsAutoCString reason;
    MediaResult rv = decoder->InitializeSession();
    bool isHardwareAccelerated = decoder->IsHardwareAccelerated(reason);
    decoder->Shutdown();

    return NS_SUCCEEDED(rv) && isHardwareAccelerated;
  }

  return false;
}

/* static */
bool AppleDecoderModule::RegisterSupplementalVP9Decoder() {
  static bool sRegisterIfAvailable = []() {
    if (VTRegisterSupplementalVideoDecoderIfAvailable) {
      VTRegisterSupplementalVideoDecoderIfAvailable(kCMVideoCodecType_VP9);
      return true;
    }
    return false;
  }();
  return sRegisterIfAvailable;
}

}  // namespace mozilla
