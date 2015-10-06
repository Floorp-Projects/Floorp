/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMF.h"
#include "WMFDecoderModule.h"
#include "WMFVideoMFTManager.h"
#include "WMFAudioMFTManager.h"
#include "MFTDecoder.h"
#include "mozilla/Preferences.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Services.h"
#include "WMFMediaDataDecoder.h"
#include "nsIWindowsRegKey.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIGfxInfo.h"
#include "GfxDriverInfo.h"
#include "gfxWindowsPlatform.h"
#include "MediaInfo.h"
#include "prsystem.h"
#include "mozilla/Maybe.h"

namespace mozilla {

static bool sDXVAEnabled = false;
static int  sNumDecoderThreads = -1;
static bool sIsIntelDecoderEnabled = false;

WMFDecoderModule::WMFDecoderModule()
  : mWMFInitialized(false)
{
}

WMFDecoderModule::~WMFDecoderModule()
{
  if (mWMFInitialized) {
    DebugOnly<HRESULT> hr = wmf::MFShutdown();
    NS_ASSERTION(SUCCEEDED(hr), "MFShutdown failed");
  }
}

static void
SetNumOfDecoderThreads()
{
  MOZ_ASSERT(NS_IsMainThread(), "Preferences can only be read on main thread");
  int32_t numCores = PR_GetNumberOfProcessors();

  // If we have more than 4 cores, let the decoder decide how many threads.
  // On an 8 core machine, WMF chooses 4 decoder threads
  const int WMF_DECODER_DEFAULT = -1;
  int32_t prefThreadCount = Preferences::GetInt("media.wmf.decoder.thread-count", -1);
  if (prefThreadCount != WMF_DECODER_DEFAULT) {
    sNumDecoderThreads = std::max(prefThreadCount, 1);
  } else if (numCores > 4) {
    sNumDecoderThreads = WMF_DECODER_DEFAULT;
  } else {
    sNumDecoderThreads = std::max(numCores - 1, 1);
  }
}

/* static */
void
WMFDecoderModule::Init()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  sDXVAEnabled = gfxPlatform::GetPlatform()->CanUseHardwareVideoDecoding();
  sIsIntelDecoderEnabled = Preferences::GetBool("media.webm.intel_decoder.enabled", false);
  SetNumOfDecoderThreads();
}

/* static */
int
WMFDecoderModule::GetNumDecoderThreads()
{
  return sNumDecoderThreads;
}

nsresult
WMFDecoderModule::Startup()
{
  mWMFInitialized = SUCCEEDED(wmf::MFStartup());
  return mWMFInitialized ? NS_OK : NS_ERROR_FAILURE;
}

already_AddRefed<MediaDataDecoder>
WMFDecoderModule::CreateVideoDecoder(const VideoInfo& aConfig,
                                     layers::LayersBackend aLayersBackend,
                                     layers::ImageContainer* aImageContainer,
                                     FlushableTaskQueue* aVideoTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  nsAutoPtr<WMFVideoMFTManager> manager(
    new WMFVideoMFTManager(aConfig,
                           aLayersBackend,
                           aImageContainer,
                           sDXVAEnabled));

  if (!manager->Init()) {
    return nullptr;
  }

  nsRefPtr<MediaDataDecoder> decoder =
    new WMFMediaDataDecoder(manager.forget(), aVideoTaskQueue, aCallback);

  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
WMFDecoderModule::CreateAudioDecoder(const AudioInfo& aConfig,
                                     FlushableTaskQueue* aAudioTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  nsAutoPtr<WMFAudioMFTManager> manager(new WMFAudioMFTManager(aConfig));

  if (!manager->Init()) {
    return nullptr;
  }

  nsRefPtr<MediaDataDecoder> decoder =
    new WMFMediaDataDecoder(manager.forget(), aAudioTaskQueue, aCallback);
  return decoder.forget();
}

static bool
CanCreateMFTDecoder(const GUID& aGuid)
{
  if (FAILED(wmf::MFStartup())) {
    return false;
  }
  RefPtr<MFTDecoder> decoder(new MFTDecoder());
  bool hasH264 = SUCCEEDED(decoder->Create(aGuid));
  wmf::MFShutdown();
  return hasH264;
}

template<const GUID& aGuid>
static bool
CanCreateWMFDecoder()
{
  static Maybe<bool> result;
  if (result.isNothing()) {
    result.emplace(CanCreateMFTDecoder(aGuid));
  }
  return result.value();
}

bool
WMFDecoderModule::SupportsMimeType(const nsACString& aMimeType)
{
  if ((aMimeType.EqualsLiteral("audio/mp4a-latm") ||
       aMimeType.EqualsLiteral("audio/mp4")) &&
      CanCreateWMFDecoder<CLSID_CMSAACDecMFT>()) {
    return true;
  }
  if ((aMimeType.EqualsLiteral("video/avc") ||
       aMimeType.EqualsLiteral("video/mp4")) &&
      CanCreateWMFDecoder<CLSID_CMSH264DecoderMFT>()) {
    return true;
  }
  if (aMimeType.EqualsLiteral("audio/mpeg") &&
      CanCreateWMFDecoder<CLSID_CMP3DecMediaObject>()) {
    return true;
  }
  if (sIsIntelDecoderEnabled) {
    if (aMimeType.EqualsLiteral("video/webm; codecs=vp8") &&
        CanCreateWMFDecoder<CLSID_WebmMfVp8Dec>()) {
      return true;
    }
    if (aMimeType.EqualsLiteral("video/webm; codecs=vp9") &&
        CanCreateWMFDecoder<CLSID_WebmMfVp9Dec>()) {
      return true;
    }
  }

  // Some unsupported codec.
  return false;
}

PlatformDecoderModule::ConversionRequired
WMFDecoderModule::DecoderNeedsConversion(const TrackInfo& aConfig) const
{
  if (aConfig.IsVideo() &&
      (aConfig.mMimeType.EqualsLiteral("video/avc") ||
       aConfig.mMimeType.EqualsLiteral("video/mp4"))) {
    return kNeedAnnexB;
  } else {
    return kNeedNone;
  }
}

} // namespace mozilla
