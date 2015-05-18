/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GonkDecoderModule.h"
#include "GonkVideoDecoderManager.h"
#include "GonkAudioDecoderManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/DebugOnly.h"
#include "GonkMediaDataDecoder.h"

namespace mozilla {
GonkDecoderModule::GonkDecoderModule()
{
}

GonkDecoderModule::~GonkDecoderModule()
{
}

/* static */
void
GonkDecoderModule::Init()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
}

already_AddRefed<MediaDataDecoder>
GonkDecoderModule::CreateVideoDecoder(const VideoInfo& aConfig,
                                     mozilla::layers::LayersBackend aLayersBackend,
                                     mozilla::layers::ImageContainer* aImageContainer,
                                     FlushableMediaTaskQueue* aVideoTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  nsRefPtr<MediaDataDecoder> decoder =
  new GonkMediaDataDecoder(new GonkVideoDecoderManager(aImageContainer, aConfig),
                           aVideoTaskQueue, aCallback);
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
GonkDecoderModule::CreateAudioDecoder(const AudioInfo& aConfig,
                                      FlushableMediaTaskQueue* aAudioTaskQueue,
                                      MediaDataDecoderCallback* aCallback)
{
  nsRefPtr<MediaDataDecoder> decoder =
  new GonkMediaDataDecoder(new GonkAudioDecoderManager(aConfig),
                           aAudioTaskQueue, aCallback);
  return decoder.forget();
}

PlatformDecoderModule::ConversionRequired
GonkDecoderModule::DecoderNeedsConversion(const TrackInfo& aConfig) const
{
  if (aConfig.IsVideo()) {
    return kNeedAnnexB;
  } else {
    return kNeedNone;
  }
}

bool
GonkDecoderModule::SupportsMimeType(const nsACString& aMimeType)
{
  return aMimeType.EqualsLiteral("audio/mp4a-latm") ||
    aMimeType.EqualsLiteral("audio/3gpp") ||
    aMimeType.EqualsLiteral("audio/amr-wb") ||
    aMimeType.EqualsLiteral("video/mp4") ||
    aMimeType.EqualsLiteral("video/mp4v-es") ||
    aMimeType.EqualsLiteral("video/avc");
}
} // namespace mozilla
