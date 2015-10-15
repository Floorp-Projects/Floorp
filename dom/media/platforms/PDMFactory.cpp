/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDMFactory.h"

#ifdef XP_WIN
#include "WMFDecoderModule.h"
#endif
#ifdef MOZ_FFMPEG
#include "FFmpegRuntimeLinker.h"
#endif
#ifdef MOZ_APPLEMEDIA
#include "AppleDecoderModule.h"
#endif
#ifdef MOZ_GONK_MEDIACODEC
#include "GonkDecoderModule.h"
#endif
#ifdef MOZ_WIDGET_ANDROID
#include "AndroidDecoderModule.h"
#endif
#include "GMPDecoderModule.h"

#include "mozilla/Preferences.h"
#include "mozilla/TaskQueue.h"

#include "mozilla/SharedThreadPool.h"

#include "MediaInfo.h"
#include "FuzzingWrapper.h"
#include "H264Converter.h"

#include "AgnosticDecoderModule.h"

#ifdef MOZ_EME
#include "EMEDecoderModule.h"
#include "mozilla/CDMProxy.h"
#endif

namespace mozilla {

extern already_AddRefed<PlatformDecoderModule> CreateAgnosticDecoderModule();
extern already_AddRefed<PlatformDecoderModule> CreateBlankDecoderModule();

bool PDMFactory::sUseBlankDecoder = false;
#ifdef MOZ_GONK_MEDIACODEC
bool PDMFactory::sGonkDecoderEnabled = false;
#endif
#ifdef MOZ_WIDGET_ANDROID
bool PDMFactory::sAndroidMCDecoderEnabled = false;
bool PDMFactory::sAndroidMCDecoderPreferred = false;
#endif
bool PDMFactory::sGMPDecoderEnabled = false;
#ifdef MOZ_FFMPEG
bool PDMFactory::sFFmpegDecoderEnabled = false;
#endif

bool PDMFactory::sEnableFuzzingWrapper = false;
uint32_t PDMFactory::sVideoOutputMinimumInterval_ms = 0;
bool PDMFactory::sDontDelayInputExhausted = false;

/* static */
void
PDMFactory::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  static bool alreadyInitialized = false;
  if (alreadyInitialized) {
    return;
  }
  alreadyInitialized = true;

  Preferences::AddBoolVarCache(&sUseBlankDecoder,
                               "media.use-blank-decoder");
#ifdef MOZ_GONK_MEDIACODEC
  Preferences::AddBoolVarCache(&sGonkDecoderEnabled,
                               "media.gonk.enabled", false);
#endif
#ifdef MOZ_WIDGET_ANDROID
  Preferences::AddBoolVarCache(&sAndroidMCDecoderEnabled,
                               "media.android-media-codec.enabled", false);
  Preferences::AddBoolVarCache(&sAndroidMCDecoderPreferred,
                               "media.android-media-codec.preferred", false);
#endif

  Preferences::AddBoolVarCache(&sGMPDecoderEnabled,
                               "media.gmp.decoder.enabled", false);
#ifdef MOZ_FFMPEG
  Preferences::AddBoolVarCache(&sFFmpegDecoderEnabled,
                               "media.ffmpeg.enabled", false);
#endif

  Preferences::AddBoolVarCache(&sEnableFuzzingWrapper,
                               "media.decoder.fuzzing.enabled", false);
  Preferences::AddUintVarCache(&sVideoOutputMinimumInterval_ms,
                               "media.decoder.fuzzing.video-output-minimum-interval-ms", 0);
  Preferences::AddBoolVarCache(&sDontDelayInputExhausted,
                               "media.decoder.fuzzing.dont-delay-inputexhausted", false);

#ifdef XP_WIN
  WMFDecoderModule::Init();
#endif
#ifdef MOZ_APPLEMEDIA
  AppleDecoderModule::Init();
#endif
#ifdef MOZ_FFMPEG
  FFmpegRuntimeLinker::Link();
#endif
  GMPDecoderModule::Init();
}

PDMFactory::PDMFactory()
{
  CreatePDMs();
}

PDMFactory::~PDMFactory()
{
}

already_AddRefed<MediaDataDecoder>
PDMFactory::CreateDecoder(const TrackInfo& aConfig,
                          FlushableTaskQueue* aTaskQueue,
                          MediaDataDecoderCallback* aCallback,
                          layers::LayersBackend aLayersBackend,
                          layers::ImageContainer* aImageContainer)
{
  nsRefPtr<PlatformDecoderModule> current = (mEMEPDM && aConfig.mCrypto.mValid)
    ? mEMEPDM : GetDecoder(aConfig.mMimeType);

  if (!current) {
    return nullptr;
  }
  nsRefPtr<MediaDataDecoder> m;

  if (aConfig.GetAsAudioInfo()) {
    m = current->CreateAudioDecoder(*aConfig.GetAsAudioInfo(),
                                    aTaskQueue,
                                    aCallback);
    return m.forget();
  }

  if (!aConfig.GetAsVideoInfo()) {
    return nullptr;
  }

  MediaDataDecoderCallback* callback = aCallback;
  nsRefPtr<DecoderCallbackFuzzingWrapper> callbackWrapper;
  if (sEnableFuzzingWrapper) {
    callbackWrapper = new DecoderCallbackFuzzingWrapper(aCallback);
    callbackWrapper->SetVideoOutputMinimumInterval(
      TimeDuration::FromMilliseconds(sVideoOutputMinimumInterval_ms));
    callbackWrapper->SetDontDelayInputExhausted(sDontDelayInputExhausted);
    callback = callbackWrapper.get();
  }

  if (H264Converter::IsH264(aConfig)) {
    nsRefPtr<H264Converter> h
      = new H264Converter(current,
                          *aConfig.GetAsVideoInfo(),
                          aLayersBackend,
                          aImageContainer,
                          aTaskQueue,
                          callback);
    const nsresult rv = h->GetLastError();
    if (NS_SUCCEEDED(rv) || rv == NS_ERROR_NOT_INITIALIZED) {
      // The H264Converter either successfully created the wrapped decoder,
      // or there wasn't enough AVCC data to do so. Otherwise, there was some
      // problem, for example WMF DLLs were missing.
      m = h.forget();
    }
  } else {
    m = current->CreateVideoDecoder(*aConfig.GetAsVideoInfo(),
                                    aLayersBackend,
                                    aImageContainer,
                                    aTaskQueue,
                                    callback);
  }

  if (callbackWrapper && m) {
    m = new DecoderFuzzingWrapper(m.forget(), callbackWrapper.forget());
  }

  return m.forget();
}

bool
PDMFactory::SupportsMimeType(const nsACString& aMimeType)
{
  if (mEMEPDM) {
    return mEMEPDM->SupportsMimeType(aMimeType);
  }
  nsRefPtr<PlatformDecoderModule> current = GetDecoder(aMimeType);
  return !!current;
}

void
PDMFactory::CreatePDMs()
{
  nsRefPtr<PlatformDecoderModule> m;

  if (sGMPDecoderEnabled) {
    m = new GMPDecoderModule();
    StartupPDM(m);
  }
#ifdef MOZ_WIDGET_ANDROID
  if(sAndroidMCDecoderPreferred && sAndroidMCDecoderEnabled) {
    m = new AndroidDecoderModule();
    StartupPDM(m);
  }
#endif
#ifdef XP_WIN
  m = new WMFDecoderModule();
  StartupPDM(m);
#endif
#ifdef MOZ_FFMPEG
  if (sFFmpegDecoderEnabled) {
    m = FFmpegRuntimeLinker::CreateDecoderModule();
    StartupPDM(m);
  }
#endif
#ifdef MOZ_APPLEMEDIA
  m = new AppleDecoderModule();
  StartupPDM(m);
#endif
#ifdef MOZ_GONK_MEDIACODEC
  if (sGonkDecoderEnabled) {
    m = new GonkDecoderModule();
    StartupPDM(m);
  }
#endif
#ifdef MOZ_WIDGET_ANDROID
  if(sAndroidMCDecoderEnabled){
    m = new AndroidDecoderModule();
    StartupPDM(m);
  }
#endif

  m = new AgnosticDecoderModule();
  StartupPDM(m);

  if (sUseBlankDecoder) {
    m = CreateBlankDecoderModule();
    StartupPDM(m);
  }
}

bool
PDMFactory::StartupPDM(PlatformDecoderModule* aPDM)
{
  if (aPDM && NS_SUCCEEDED(aPDM->Startup())) {
    mCurrentPDMs.AppendElement(aPDM);
    return true;
  }
  return false;
}

already_AddRefed<PlatformDecoderModule>
PDMFactory::GetDecoder(const nsACString& aMimeType)
{
  nsRefPtr<PlatformDecoderModule> pdm;
  for (auto& current : mCurrentPDMs) {
    if (current->SupportsMimeType(aMimeType)) {
      pdm = current;
      break;
    }
  }
  return pdm.forget();
}

#ifdef MOZ_EME
void
PDMFactory::SetCDMProxy(CDMProxy* aProxy)
{
  bool cdmDecodesAudio;
  bool cdmDecodesVideo;
  {
    CDMCaps::AutoLock caps(aProxy->Capabilites());
    cdmDecodesAudio = caps.CanDecryptAndDecodeAudio();
    cdmDecodesVideo = caps.CanDecryptAndDecodeVideo();
  }

  nsRefPtr<PDMFactory> m = new PDMFactory();
  mEMEPDM = new EMEDecoderModule(aProxy, m, cdmDecodesAudio, cdmDecodesVideo);
}
#endif

}  // namespace mozilla
