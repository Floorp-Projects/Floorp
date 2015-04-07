/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPDecoderModule.h"
#include "GMPAudioDecoder.h"
#include "GMPVideoDecoder.h"
#include "MediaDataDecoderProxy.h"
#include "mozIGeckoMediaPluginService.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {

GMPDecoderModule::GMPDecoderModule()
{
}

GMPDecoderModule::~GMPDecoderModule()
{
}

static already_AddRefed<MediaDataDecoderProxy>
CreateDecoderWrapper(MediaDataDecoderCallback* aCallback)
{
  nsCOMPtr<mozIGeckoMediaPluginService> gmpService = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  if (!gmpService) {
    return nullptr;
  }

  nsCOMPtr<nsIThread> thread;
  nsresult rv = gmpService->GetThread(getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  nsRefPtr<MediaDataDecoderProxy> decoder(new MediaDataDecoderProxy(thread, aCallback));
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
GMPDecoderModule::CreateVideoDecoder(const mp4_demuxer::VideoDecoderConfig& aConfig,
                                     layers::LayersBackend aLayersBackend,
                                     layers::ImageContainer* aImageContainer,
                                     FlushableMediaTaskQueue* aVideoTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  if (!aConfig.mime_type.EqualsLiteral("video/avc")) {
    return nullptr;
  }

  nsRefPtr<MediaDataDecoderProxy> wrapper = CreateDecoderWrapper(aCallback);
  wrapper->SetProxyTarget(new GMPVideoDecoder(aConfig,
                                              aLayersBackend,
                                              aImageContainer,
                                              aVideoTaskQueue,
                                              wrapper->Callback()));
  return wrapper.forget();
}

already_AddRefed<MediaDataDecoder>
GMPDecoderModule::CreateAudioDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                                     FlushableMediaTaskQueue* aAudioTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  if (!aConfig.mime_type.EqualsLiteral("audio/mp4a-latm")) {
    return nullptr;
  }

  nsRefPtr<MediaDataDecoderProxy> wrapper = CreateDecoderWrapper(aCallback);
  wrapper->SetProxyTarget(new GMPAudioDecoder(aConfig,
                                              aAudioTaskQueue,
                                              wrapper->Callback()));
  return wrapper.forget();
}

PlatformDecoderModule::ConversionRequired
GMPDecoderModule::DecoderNeedsConversion(const mp4_demuxer::TrackConfig& aConfig) const
{
  // GMPVideoCodecType::kGMPVideoCodecH264 specifies that encoded frames must be in AVCC format.
  if (aConfig.IsVideoConfig()) {
    return kNeedAVCC;
  } else {
    return kNeedNone;
  }
}

} // namespace mozilla
