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

#include "OpusDecoder.h"
#include "VorbisDecoder.h"
#include "VPXDecoder.h"

namespace mozilla {

extern already_AddRefed<PlatformDecoderModule> CreateAgnosticDecoderModule();
extern already_AddRefed<PlatformDecoderModule> CreateBlankDecoderModule();

bool PDMFactory::sUseBlankDecoder = false;
bool PDMFactory::sGonkDecoderEnabled = false;
bool PDMFactory::sAndroidMCDecoderEnabled = false;
bool PDMFactory::sAndroidMCDecoderPreferred = false;
bool PDMFactory::sGMPDecoderEnabled = false;
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
                               "media.fragmented-mp4.use-blank-decoder");
#ifdef MOZ_GONK_MEDIACODEC
  Preferences::AddBoolVarCache(&sGonkDecoderEnabled,
                               "media.fragmented-mp4.gonk.enabled", false);
#endif
#ifdef MOZ_WIDGET_ANDROID
  Preferences::AddBoolVarCache(&sAndroidMCDecoderEnabled,
                               "media.fragmented-mp4.android-media-codec.enabled", false);
  Preferences::AddBoolVarCache(&sAndroidMCDecoderPreferred,
                               "media.fragmented-mp4.android-media-codec.preferred", false);
#endif

  Preferences::AddBoolVarCache(&sGMPDecoderEnabled,
                               "media.fragmented-mp4.gmp.enabled", false);

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
}

PDMFactory::PDMFactory()
  : mCurrentPDM(CreatePDM())
{
  if (!mCurrentPDM || NS_FAILED(mCurrentPDM->Startup())) {
    mCurrentPDM = CreateAgnosticDecoderModule();
  }
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
  MOZ_ASSERT(mCurrentPDM);

  nsRefPtr<MediaDataDecoder> m;

  bool hasPlatformDecoder = mCurrentPDM->SupportsMimeType(aConfig.mMimeType);

  if (aConfig.GetAsAudioInfo()) {
    if (!hasPlatformDecoder && VorbisDataDecoder::IsVorbis(aConfig.mMimeType)) {
      m = new VorbisDataDecoder(*aConfig.GetAsAudioInfo(),
                                aTaskQueue,
                                aCallback);
    } else if (!hasPlatformDecoder && OpusDataDecoder::IsOpus(aConfig.mMimeType)) {
      m = new OpusDataDecoder(*aConfig.GetAsAudioInfo(),
                              aTaskQueue,
                              aCallback);
    } else {
      m = mCurrentPDM->CreateAudioDecoder(*aConfig.GetAsAudioInfo(),
                                          aTaskQueue,
                                          aCallback);
    }
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
      = new H264Converter(mCurrentPDM,
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
  } else if (!hasPlatformDecoder && VPXDecoder::IsVPX(aConfig.mMimeType)) {
    m = new VPXDecoder(*aConfig.GetAsVideoInfo(),
                       aImageContainer,
                       aTaskQueue,
                       callback);
  } else {
    m = mCurrentPDM->CreateVideoDecoder(*aConfig.GetAsVideoInfo(),
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
  MOZ_ASSERT(mCurrentPDM);
  return mCurrentPDM->SupportsMimeType(aMimeType) ||
    VPXDecoder::IsVPX(aMimeType) ||
    OpusDataDecoder::IsOpus(aMimeType) ||
    VorbisDataDecoder::IsVorbis(aMimeType);
}

already_AddRefed<PlatformDecoderModule>
PDMFactory::CreatePDM()
{
  if (sGMPDecoderEnabled) {
    nsRefPtr<PlatformDecoderModule> m(new GMPDecoderModule());
    return m.forget();
  }
#ifdef MOZ_WIDGET_ANDROID
  if(sAndroidMCDecoderPreferred && sAndroidMCDecoderEnabled){
    nsRefPtr<PlatformDecoderModule> m(new AndroidDecoderModule());
    return m.forget();
  }
#endif
  if (sUseBlankDecoder) {
    return CreateBlankDecoderModule();
  }
#ifdef XP_WIN
  nsRefPtr<PlatformDecoderModule> m(new WMFDecoderModule());
  return m.forget();
#endif
#ifdef MOZ_FFMPEG
  nsRefPtr<PlatformDecoderModule> mffmpeg = FFmpegRuntimeLinker::CreateDecoderModule();
  if (mffmpeg) {
    return mffmpeg.forget();
  }
#endif
#ifdef MOZ_APPLEMEDIA
  nsRefPtr<PlatformDecoderModule> m(new AppleDecoderModule());
  return m.forget();
#endif
#ifdef MOZ_GONK_MEDIACODEC
  if (sGonkDecoderEnabled) {
    nsRefPtr<PlatformDecoderModule> m(new GonkDecoderModule());
    return m.forget();
  }
#endif
#ifdef MOZ_WIDGET_ANDROID
  if(sAndroidMCDecoderEnabled){
    nsRefPtr<PlatformDecoderModule> m(new AndroidDecoderModule());
    return m.forget();
  }
#endif
  return nullptr;
}

}  // namespace mozilla
