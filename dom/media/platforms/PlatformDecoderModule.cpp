/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlatformDecoderModule.h"

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

#ifdef MOZ_EME
#include "EMEDecoderModule.h"
#include "mozilla/CDMProxy.h"
#endif
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

bool PlatformDecoderModule::sUseBlankDecoder = false;
bool PlatformDecoderModule::sFFmpegDecoderEnabled = false;
bool PlatformDecoderModule::sGonkDecoderEnabled = false;
bool PlatformDecoderModule::sAndroidMCDecoderEnabled = false;
bool PlatformDecoderModule::sAndroidMCDecoderPreferred = false;
bool PlatformDecoderModule::sGMPDecoderEnabled = false;
bool PlatformDecoderModule::sEnableFuzzingWrapper = false;
uint32_t PlatformDecoderModule::sVideoOutputMinimumInterval_ms = 0;
bool PlatformDecoderModule::sDontDelayInputExhausted = false;

/* static */
void
PlatformDecoderModule::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  static bool alreadyInitialized = false;
  if (alreadyInitialized) {
    return;
  }
  alreadyInitialized = true;

  Preferences::AddBoolVarCache(&sUseBlankDecoder,
                               "media.fragmented-mp4.use-blank-decoder");
  Preferences::AddBoolVarCache(&sFFmpegDecoderEnabled,
                               "media.fragmented-mp4.ffmpeg.enabled", false);

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

#ifdef MOZ_EME
/* static */
already_AddRefed<PlatformDecoderModule>
PlatformDecoderModule::CreateCDMWrapper(CDMProxy* aProxy)
{
  bool cdmDecodesAudio;
  bool cdmDecodesVideo;
  {
    CDMCaps::AutoLock caps(aProxy->Capabilites());
    cdmDecodesAudio = caps.CanDecryptAndDecodeAudio();
    cdmDecodesVideo = caps.CanDecryptAndDecodeVideo();
  }

  // We always create a default PDM in order to decode
  // non-encrypted tracks.
  nsRefPtr<PlatformDecoderModule> pdm = Create();
  if (!pdm) {
    return nullptr;
  }

  nsRefPtr<PlatformDecoderModule> emepdm(
    new EMEDecoderModule(aProxy, pdm, cdmDecodesAudio, cdmDecodesVideo));
  return emepdm.forget();
}
#endif

/* static */
already_AddRefed<PlatformDecoderModule>
PlatformDecoderModule::Create()
{
  // Note: This (usually) runs on the decode thread.

  nsRefPtr<PlatformDecoderModule> m(CreatePDM());

  if (m && NS_SUCCEEDED(m->Startup())) {
    return m.forget();
  }
  return CreateAgnosticDecoderModule();
}

/* static */
already_AddRefed<PlatformDecoderModule>
PlatformDecoderModule::CreatePDM()
{
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
  if (sFFmpegDecoderEnabled) {
    nsRefPtr<PlatformDecoderModule> m = FFmpegRuntimeLinker::CreateDecoderModule();
    if (m) {
      return m.forget();
    }
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
  if (sGMPDecoderEnabled) {
    nsRefPtr<PlatformDecoderModule> m(new GMPDecoderModule());
    return m.forget();
  }
  return nullptr;
}

already_AddRefed<MediaDataDecoder>
PlatformDecoderModule::CreateDecoder(const TrackInfo& aConfig,
                                     FlushableTaskQueue* aTaskQueue,
                                     MediaDataDecoderCallback* aCallback,
                                     layers::LayersBackend aLayersBackend,
                                     layers::ImageContainer* aImageContainer)
{
  nsRefPtr<MediaDataDecoder> m;

  bool hasPlatformDecoder = SupportsMimeType(aConfig.mMimeType);

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
      m = CreateAudioDecoder(*aConfig.GetAsAudioInfo(),
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
    m = new H264Converter(this,
                          *aConfig.GetAsVideoInfo(),
                          aLayersBackend,
                          aImageContainer,
                          aTaskQueue,
                          callback);
  } else if (!hasPlatformDecoder && VPXDecoder::IsVPX(aConfig.mMimeType)) {
    m = new VPXDecoder(*aConfig.GetAsVideoInfo(),
                       aImageContainer,
                       aTaskQueue,
                       callback);
  } else {
    m = CreateVideoDecoder(*aConfig.GetAsVideoInfo(),
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
PlatformDecoderModule::SupportsMimeType(const nsACString& aMimeType)
{
  return aMimeType.EqualsLiteral("audio/mp4a-latm") ||
    aMimeType.EqualsLiteral("video/mp4") ||
    aMimeType.EqualsLiteral("video/avc");
}

/* static */
bool
PlatformDecoderModule::AgnosticMimeType(const nsACString& aMimeType)
{
  return VPXDecoder::IsVPX(aMimeType) ||
    OpusDataDecoder::IsOpus(aMimeType) ||
    VorbisDataDecoder::IsVorbis(aMimeType);
}


} // namespace mozilla
