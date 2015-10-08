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
#include "mozilla/Preferences.h"
#include "gmp-audio-decode.h"
#include "gmp-video-decode.h"

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
GMPDecoderModule::CreateVideoDecoder(const VideoInfo& aConfig,
                                     layers::LayersBackend aLayersBackend,
                                     layers::ImageContainer* aImageContainer,
                                     FlushableTaskQueue* aVideoTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  if (!aConfig.mMimeType.EqualsLiteral("video/avc")) {
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
GMPDecoderModule::CreateAudioDecoder(const AudioInfo& aConfig,
                                     FlushableTaskQueue* aAudioTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  if (!aConfig.mMimeType.EqualsLiteral("audio/mp4a-latm")) {
    return nullptr;
  }

  nsRefPtr<MediaDataDecoderProxy> wrapper = CreateDecoderWrapper(aCallback);
  wrapper->SetProxyTarget(new GMPAudioDecoder(aConfig,
                                              aAudioTaskQueue,
                                              wrapper->Callback()));
  return wrapper.forget();
}

PlatformDecoderModule::ConversionRequired
GMPDecoderModule::DecoderNeedsConversion(const TrackInfo& aConfig) const
{
  // GMPVideoCodecType::kGMPVideoCodecH264 specifies that encoded frames must be in AVCC format.
  if (aConfig.IsVideo()) {
    return kNeedAVCC;
  } else {
    return kNeedNone;
  }
}

static uint32_t sPreferredAacGmp = 0;
static uint32_t sPreferredH264Gmp = 0;

/* static */
void
GMPDecoderModule::Init()
{
  Preferences::AddUintVarCache(&sPreferredAacGmp,
                               "media.fragmented-mp4.gmp.aac", 0);
  Preferences::AddUintVarCache(&sPreferredH264Gmp,
                               "media.fragmented-mp4.gmp.h264", 0);
}

/* static */
const Maybe<nsCString>
GMPDecoderModule::PreferredGMP(const nsACString& aMimeType)
{
  Maybe<nsCString> rv;
  if (aMimeType.EqualsLiteral("audio/mp4a-latm")) {
    switch (sPreferredAacGmp) {
      case 1: rv.emplace(NS_LITERAL_CSTRING("org.w3.clearkey")); break;
      case 2: rv.emplace(NS_LITERAL_CSTRING("com.adobe.primetime")); break;
      default: break;
    }
  }

  if (aMimeType.EqualsLiteral("video/avc")) {
    switch (sPreferredH264Gmp) {
      case 1: rv.emplace(NS_LITERAL_CSTRING("org.w3.clearkey")); break;
      case 2: rv.emplace(NS_LITERAL_CSTRING("com.adobe.primetime")); break;
      default: break;
    }
  }

  return rv;
}

bool
GMPDecoderModule::SupportsMimeType(const nsACString& aMimeType,
                                   const Maybe<nsCString>& aGMP)
{
  nsTArray<nsCString> tags;
  nsCString api;
  if (aMimeType.EqualsLiteral("audio/mp4a-latm")) {
    tags.AppendElement(NS_LITERAL_CSTRING("aac"));
    api = NS_LITERAL_CSTRING(GMP_API_AUDIO_DECODER);
  } else if (aMimeType.EqualsLiteral("video/avc") ||
             aMimeType.EqualsLiteral("video/mp4")) {
    tags.AppendElement(NS_LITERAL_CSTRING("h264"));
    api = NS_LITERAL_CSTRING(GMP_API_VIDEO_DECODER);
  } else {
    return false;
  }
  if (aGMP.isSome()) {
    tags.AppendElement(aGMP.value());
  }
  nsCOMPtr<mozIGeckoMediaPluginService> mps =
    do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  if (NS_WARN_IF(!mps)) {
    return false;
  }
  bool hasPlugin = false;
  if (NS_FAILED(mps->HasPluginForAPI(api, &tags, &hasPlugin))) {
    return false;
  }
  return hasPlugin;
}

bool
GMPDecoderModule::SupportsMimeType(const nsACString& aMimeType)
{
  return SupportsMimeType(aMimeType, PreferredGMP(aMimeType));
}

} // namespace mozilla
