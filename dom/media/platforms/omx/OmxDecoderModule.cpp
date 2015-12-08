/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OmxDecoderModule.h"
#include "OmxDataDecoder.h"

namespace mozilla {

already_AddRefed<MediaDataDecoder>
OmxDecoderModule::CreateVideoDecoder(const VideoInfo& aConfig,
                                     mozilla::layers::LayersBackend aLayersBackend,
                                     mozilla::layers::ImageContainer* aImageContainer,
                                     FlushableTaskQueue* aVideoTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  return nullptr;
}

already_AddRefed<MediaDataDecoder>
OmxDecoderModule::CreateAudioDecoder(const AudioInfo& aConfig,
                                     FlushableTaskQueue* aAudioTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  RefPtr<OmxDataDecoder> decoder = new OmxDataDecoder(aConfig, aCallback);
  return decoder.forget();
}

void
OmxDecoderModule::Init()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
}

PlatformDecoderModule::ConversionRequired
OmxDecoderModule::DecoderNeedsConversion(const TrackInfo& aConfig) const
{
  return kNeedNone;
}

bool
OmxDecoderModule::SupportsMimeType(const nsACString& aMimeType) const
{
  return aMimeType.EqualsLiteral("audio/mp4a-latm");
}

}
