/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AppleATDecoder.h"
#include "AppleCMLinker.h"
#include "AppleDecoderModule.h"
#include "AppleVTDecoder.h"
#include "AppleVTLinker.h"
#include "MacIOSurfaceImage.h"
#include "MediaPrefs.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"

namespace mozilla {

bool AppleDecoderModule::sInitialized = false;
bool AppleDecoderModule::sIsCoreMediaAvailable = false;
bool AppleDecoderModule::sIsVTAvailable = false;
bool AppleDecoderModule::sIsVTHWAvailable = false;
bool AppleDecoderModule::sCanUseHardwareVideoDecoder = true;

AppleDecoderModule::AppleDecoderModule()
{
}

AppleDecoderModule::~AppleDecoderModule()
{
}

/* static */
void
AppleDecoderModule::Init()
{
  if (sInitialized) {
    return;
  }

  // Ensure IOSurface framework is loaded.
  MacIOSurfaceLib::LoadLibrary();
  const bool loaded = MacIOSurfaceLib::isInit();

  // dlopen CoreMedia.framework if it's available.
  sIsCoreMediaAvailable = AppleCMLinker::Link();
  // dlopen VideoToolbox.framework if it's available.
  // We must link both CM and VideoToolbox framework to allow for proper
  // paired Link/Unlink calls
  bool haveVideoToolbox = loaded && AppleVTLinker::Link();
  sIsVTAvailable = sIsCoreMediaAvailable && haveVideoToolbox;

  sIsVTHWAvailable = AppleVTLinker::skPropEnableHWAccel != nullptr;

  sCanUseHardwareVideoDecoder = loaded &&
    gfxPlatform::GetPlatform()->CanUseHardwareVideoDecoding();

  sInitialized = true;
}

nsresult
AppleDecoderModule::Startup()
{
  if (!sInitialized || !sIsVTAvailable) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

already_AddRefed<MediaDataDecoder>
AppleDecoderModule::CreateVideoDecoder(const CreateDecoderParams& aParams)
{
  RefPtr<MediaDataDecoder> decoder =
    new AppleVTDecoder(aParams.VideoConfig(),
                       aParams.mTaskQueue,
                       aParams.mCallback,
                       aParams.mImageContainer);
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
AppleDecoderModule::CreateAudioDecoder(const CreateDecoderParams& aParams)
{
  RefPtr<MediaDataDecoder> decoder =
    new AppleATDecoder(aParams.AudioConfig(),
                       aParams.mTaskQueue,
                       aParams.mCallback);
  return decoder.forget();
}

bool
AppleDecoderModule::SupportsMimeType(const nsACString& aMimeType,
                                     DecoderDoctorDiagnostics* aDiagnostics) const
{
  return (sIsCoreMediaAvailable &&
          (aMimeType.EqualsLiteral("audio/mpeg") ||
           aMimeType.EqualsLiteral("audio/mp4a-latm"))) ||
    (sIsVTAvailable && (aMimeType.EqualsLiteral("video/mp4") ||
                        aMimeType.EqualsLiteral("video/avc")));
}

PlatformDecoderModule::ConversionRequired
AppleDecoderModule::DecoderNeedsConversion(const TrackInfo& aConfig) const
{
  if (aConfig.IsVideo()) {
    return ConversionRequired::kNeedAVCC;
  } else {
    return ConversionRequired::kNeedNone;
  }
}

} // namespace mozilla
