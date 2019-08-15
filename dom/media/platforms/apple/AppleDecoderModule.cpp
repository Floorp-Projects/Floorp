/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppleATDecoder.h"
#include "AppleDecoderModule.h"
#include "AppleVTDecoder.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "mozilla/gfx/gfxVars.h"

namespace mozilla {

bool AppleDecoderModule::sInitialized = false;
bool AppleDecoderModule::sCanUseHardwareVideoDecoder = true;

AppleDecoderModule::AppleDecoderModule() {}

AppleDecoderModule::~AppleDecoderModule() {}

/* static */
void AppleDecoderModule::Init() {
  if (sInitialized) {
    return;
  }

  sCanUseHardwareVideoDecoder = gfx::gfxVars::CanUseHardwareVideoDecoding();

  sInitialized = true;
}

nsresult AppleDecoderModule::Startup() {
  if (!sInitialized) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

already_AddRefed<MediaDataDecoder> AppleDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  RefPtr<MediaDataDecoder> decoder =
      new AppleVTDecoder(aParams.VideoConfig(), aParams.mTaskQueue,
                         aParams.mImageContainer, aParams.mOptions);
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder> AppleDecoderModule::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  RefPtr<MediaDataDecoder> decoder =
      new AppleATDecoder(aParams.AudioConfig(), aParams.mTaskQueue);
  return decoder.forget();
}

bool AppleDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  return aMimeType.EqualsLiteral("audio/mpeg") ||
         aMimeType.EqualsLiteral("audio/mp4a-latm") ||
         aMimeType.EqualsLiteral("video/mp4") ||
         aMimeType.EqualsLiteral("video/avc");
}

}  // namespace mozilla
