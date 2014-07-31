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
#include "mozilla/Preferences.h"
#include "mozilla/DebugOnly.h"

namespace mozilla {

bool AppleDecoderModule::sIsEnabled = false;

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
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  sIsEnabled = Preferences::GetBool("media.apple.mp4.enabled", false);
  if (!sIsEnabled) {
    return;
  }

  // dlopen CoreMedia.framework if it's available.
  sIsEnabled = AppleCMLinker::Link();
  if (!sIsEnabled) {
    return;
  }

  // dlopen VideoToolbox.framework if it's available.
  sIsEnabled = AppleVTLinker::Link();
}

nsresult
AppleDecoderModule::Startup()
{
  // We don't have any per-instance initialization to do.
  // Check whether ::Init() above succeeded to know if
  // we're functional.
  if (!sIsEnabled) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
AppleDecoderModule::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  AppleVTLinker::Unlink();
  AppleCMLinker::Unlink();

  return NS_OK;
}

MediaDataDecoder*
AppleDecoderModule::CreateH264Decoder(const mp4_demuxer::VideoDecoderConfig& aConfig,
                                      layers::LayersBackend aLayersBackend,
                                      layers::ImageContainer* aImageContainer,
                                      MediaTaskQueue* aVideoTaskQueue,
                                      MediaDataDecoderCallback* aCallback)
{
  return new AppleVTDecoder(aConfig, aVideoTaskQueue, aCallback, aImageContainer);
}

MediaDataDecoder*
AppleDecoderModule::CreateAACDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                                     MediaTaskQueue* aAudioTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  return new AppleATDecoder(aConfig, aAudioTaskQueue, aCallback);
}

} // namespace mozilla
