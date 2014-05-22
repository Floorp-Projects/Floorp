/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMF.h"
#include "WMFDecoderModule.h"
#include "WMFDecoder.h"
#include "WMFVideoOutputSource.h"
#include "WMFAudioOutputSource.h"
#include "mozilla/Preferences.h"
#include "mozilla/DebugOnly.h"
#include "WMFMediaDataDecoder.h"

namespace mozilla {

bool WMFDecoderModule::sIsWMFEnabled = false;
bool WMFDecoderModule::sDXVAEnabled = false;

WMFDecoderModule::WMFDecoderModule()
{
}

WMFDecoderModule::~WMFDecoderModule()
{
}

/* static */
void
WMFDecoderModule::Init()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  sIsWMFEnabled = Preferences::GetBool("media.windows-media-foundation.enabled", false);
  if (!sIsWMFEnabled) {
    return;
  }
  if (NS_FAILED(WMFDecoder::LoadDLLs())) {
    sIsWMFEnabled = false;
  }
  sDXVAEnabled = Preferences::GetBool("media.windows-media-foundation.use-dxva", false);
}

nsresult
WMFDecoderModule::Startup()
{
  if (!sIsWMFEnabled) {
    return NS_ERROR_FAILURE;
  }
  if (FAILED(wmf::MFStartup())) {
    NS_WARNING("Failed to initialize Windows Media Foundation");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
WMFDecoderModule::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");

  DebugOnly<HRESULT> hr = wmf::MFShutdown();
  NS_ASSERTION(SUCCEEDED(hr), "MFShutdown failed");

  return NS_OK;
}

MediaDataDecoder*
WMFDecoderModule::CreateH264Decoder(const mp4_demuxer::VideoDecoderConfig& aConfig,
                                    mozilla::layers::LayersBackend aLayersBackend,
                                    mozilla::layers::ImageContainer* aImageContainer,
                                    MediaTaskQueue* aVideoTaskQueue,
                                    MediaDataDecoderCallback* aCallback)
{
  return new WMFMediaDataDecoder(new WMFVideoOutputSource(aLayersBackend,
                                                          aImageContainer,
                                                          sDXVAEnabled),
                                 aVideoTaskQueue,
                                 aCallback);
}

MediaDataDecoder*
WMFDecoderModule::CreateAACDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                                   MediaTaskQueue* aAudioTaskQueue,
                                   MediaDataDecoderCallback* aCallback)
{
  return new WMFMediaDataDecoder(new WMFAudioOutputSource(aConfig),
                                 aAudioTaskQueue,
                                 aCallback);
}

} // namespace mozilla
