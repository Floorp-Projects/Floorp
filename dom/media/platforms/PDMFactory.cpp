/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDMFactory.h"

#ifdef XP_WIN
#include "WMFDecoderModule.h"
#endif
#ifdef MOZ_FFVPX
#include "FFVPXRuntimeLinker.h"
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

#include "mozilla/CDMProxy.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TaskQueue.h"

#include "MediaInfo.h"
#include "MediaPrefs.h"
#include "H264Converter.h"

#include "AgnosticDecoderModule.h"
#include "EMEDecoderModule.h"

#include "DecoderDoctorDiagnostics.h"

#include "MP4Decoder.h"
#include "mozilla/dom/RemoteVideoDecoder.h"

#include "mp4_demuxer/H264.h"

#include <functional>

namespace mozilla {

extern already_AddRefed<PlatformDecoderModule> CreateAgnosticDecoderModule();
extern already_AddRefed<PlatformDecoderModule> CreateBlankDecoderModule();
extern already_AddRefed<PlatformDecoderModule> CreateNullDecoderModule();

class PDMFactoryImpl final
{
public:
  PDMFactoryImpl()
  {
#ifdef XP_WIN
    WMFDecoderModule::Init();
#endif
#ifdef MOZ_APPLEMEDIA
    AppleDecoderModule::Init();
#endif
#ifdef MOZ_FFVPX
    FFVPXRuntimeLinker::Init();
#endif
#ifdef MOZ_FFMPEG
    FFmpegRuntimeLinker::Init();
#endif
  }
};

StaticAutoPtr<PDMFactoryImpl> PDMFactory::sInstance;
StaticMutex PDMFactory::sMonitor;

class SupportChecker
{
public:
  enum class Reason : uint8_t
  {
    kSupported,
    kVideoFormatNotSupported,
    kAudioFormatNotSupported,
    kUnknown,
  };

  struct CheckResult
  {
    explicit CheckResult(Reason aReason,
                         MediaResult aResult = MediaResult(NS_OK))
      : mReason(aReason),
        mMediaResult(mozilla::Move(aResult))
    {
    }
    CheckResult(const CheckResult& aOther) = default;
    CheckResult(CheckResult&& aOther) = default;
    CheckResult& operator=(const CheckResult& aOther) = default;
    CheckResult& operator=(CheckResult&& aOther) = default;

    Reason mReason;
    MediaResult mMediaResult;
  };

  template<class Func>
  void
  AddToCheckList(Func&& aChecker)
  {
    mCheckerList.AppendElement(mozilla::Forward<Func>(aChecker));
  }

  void
  AddMediaFormatChecker(const TrackInfo& aTrackConfig)
  {
    if (aTrackConfig.IsVideo()) {
      auto mimeType = aTrackConfig.GetAsVideoInfo()->mMimeType;
      RefPtr<MediaByteBuffer> extraData =
        aTrackConfig.GetAsVideoInfo()->mExtraData;
      AddToCheckList([mimeType, extraData]() {
        if (MP4Decoder::IsH264(mimeType)) {
          mp4_demuxer::SPSData spsdata;
          // WMF H.264 Video Decoder and Apple ATDecoder
          // do not support YUV444 format.
          // For consistency, all decoders should be checked.
          if (mp4_demuxer::H264::DecodeSPSFromExtraData(extraData, spsdata)
              && (spsdata.profile_idc == 244 /* Hi444PP */
                  || spsdata.chroma_format_idc == PDMFactory::kYUV444)) {
            return CheckResult(
              SupportChecker::Reason::kVideoFormatNotSupported,
              MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                          RESULT_DETAIL("Decoder may not have the capability "
                                        "to handle the requested video format "
                                        "with YUV444 chroma subsampling.")));
          }
        }
        return CheckResult(SupportChecker::Reason::kSupported);
      });
    }
  }

  SupportChecker::CheckResult
  Check()
  {
    for (auto& checker : mCheckerList) {
      auto result = checker();
        if (result.mReason != SupportChecker::Reason::kSupported) {
          return result;
      }
    }
    return CheckResult(SupportChecker::Reason::kSupported);
  }

  void Clear() { mCheckerList.Clear(); }

private:
  nsTArray<std::function<CheckResult()>> mCheckerList;
}; // SupportChecker

PDMFactory::PDMFactory()
{
  EnsureInit();
  CreatePDMs();
  CreateNullPDM();
}

PDMFactory::~PDMFactory()
{
}

void
PDMFactory::EnsureInit() const
{
  {
    StaticMutexAutoLock mon(sMonitor);
    if (sInstance) {
      // Quick exit if we already have an instance.
      return;
    }
    if (NS_IsMainThread()) {
      // On the main thread and holding the lock -> Create instance.
      sInstance = new PDMFactoryImpl();
      ClearOnShutdown(&sInstance);
      return;
    }
  }

  // Not on the main thread -> Sync-dispatch creation to main thread.
  nsCOMPtr<nsIEventTarget> mainTarget = GetMainThreadEventTarget();
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableFunction([]() {
      StaticMutexAutoLock mon(sMonitor);
      if (!sInstance) {
        sInstance = new PDMFactoryImpl();
        ClearOnShutdown(&sInstance);
      }
    });
  SyncRunnable::DispatchToThread(mainTarget, runnable);
}

already_AddRefed<MediaDataDecoder>
PDMFactory::CreateDecoder(const CreateDecoderParams& aParams)
{
  if (aParams.mUseNullDecoder) {
    MOZ_ASSERT(mNullPDM);
    return CreateDecoderWithPDM(mNullPDM, aParams);
  }

  const TrackInfo& config = aParams.mConfig;
  bool isEncrypted = mEMEPDM && config.mCrypto.mValid;

  if (isEncrypted) {
    return CreateDecoderWithPDM(mEMEPDM, aParams);
  }

  DecoderDoctorDiagnostics* diagnostics = aParams.mDiagnostics;
  if (diagnostics) {
    // If libraries failed to load, the following loop over mCurrentPDMs
    // will not even try to use them. So we record failures now.
    if (mWMFFailedToLoad) {
      diagnostics->SetWMFFailedToLoad();
    }
    if (mFFmpegFailedToLoad) {
      diagnostics->SetFFmpegFailedToLoad();
    }
    if (mGMPPDMFailedToStartup) {
      diagnostics->SetGMPPDMFailedToStartup();
    }
  }

  for (auto& current : mCurrentPDMs) {
    if (!current->SupportsMimeType(config.mMimeType, diagnostics)) {
      continue;
    }
    RefPtr<MediaDataDecoder> m = CreateDecoderWithPDM(current, aParams);
    if (m) {
      return m.forget();
    }
  }
  NS_WARNING("Unable to create a decoder, no platform found.");
  return nullptr;
}

already_AddRefed<MediaDataDecoder>
PDMFactory::CreateDecoderWithPDM(PlatformDecoderModule* aPDM,
                                 const CreateDecoderParams& aParams)
{
  MOZ_ASSERT(aPDM);
  RefPtr<MediaDataDecoder> m;
  MediaResult* result = aParams.mError;

  SupportChecker supportChecker;
  const TrackInfo& config = aParams.mConfig;
  supportChecker.AddMediaFormatChecker(config);

  auto checkResult = supportChecker.Check();
  if (checkResult.mReason != SupportChecker::Reason::kSupported) {
    DecoderDoctorDiagnostics* diagnostics = aParams.mDiagnostics;
    if (checkResult.mReason
        == SupportChecker::Reason::kVideoFormatNotSupported) {
      if (diagnostics) {
        diagnostics->SetVideoNotSupported();
      }
      if (result) {
        *result = checkResult.mMediaResult;
      }
    } else if (checkResult.mReason
               == SupportChecker::Reason::kAudioFormatNotSupported) {
      if (diagnostics) {
        diagnostics->SetAudioNotSupported();
      }
      if (result) {
        *result = checkResult.mMediaResult;
      }
    }
    return nullptr;
  }

  if (config.IsAudio()) {
    m = aPDM->CreateAudioDecoder(aParams);
    return m.forget();
  }

  if (!config.IsVideo()) {
    *result = MediaResult(
      NS_ERROR_DOM_MEDIA_FATAL_ERR,
      RESULT_DETAIL("Decoder configuration error, expected audio or video."));
    return nullptr;
  }

  if (MP4Decoder::IsH264(config.mMimeType) && !aParams.mUseNullDecoder) {
    RefPtr<H264Converter> h = new H264Converter(aPDM, aParams);
    const nsresult rv = h->GetLastError();
    if (NS_SUCCEEDED(rv) || rv == NS_ERROR_NOT_INITIALIZED) {
      // The H264Converter either successfully created the wrapped decoder,
      // or there wasn't enough AVCC data to do so. Otherwise, there was some
      // problem, for example WMF DLLs were missing.
      m = h.forget();
    }
  } else {
    m = aPDM->CreateVideoDecoder(aParams);
  }

  return m.forget();
}

bool
PDMFactory::SupportsMimeType(const nsACString& aMimeType,
                             DecoderDoctorDiagnostics* aDiagnostics) const
{
  UniquePtr<TrackInfo> trackInfo = CreateTrackInfoWithMIMEType(aMimeType);
  if (!trackInfo) {
    return false;
  }
  return Supports(*trackInfo, aDiagnostics);
}

bool
PDMFactory::Supports(const TrackInfo& aTrackInfo,
                     DecoderDoctorDiagnostics* aDiagnostics) const
{
  if (mEMEPDM) {
    return mEMEPDM->Supports(aTrackInfo, aDiagnostics);
  }
  RefPtr<PlatformDecoderModule> current = GetDecoder(aTrackInfo, aDiagnostics);
  return !!current;
}

void
PDMFactory::CreatePDMs()
{
  RefPtr<PlatformDecoderModule> m;

  if (MediaPrefs::PDMUseBlankDecoder()) {
    m = CreateBlankDecoderModule();
    StartupPDM(m);
    // The Blank PDM SupportsMimeType reports true for all codecs; the creation
    // of its decoder is infallible. As such it will be used for all media, we
    // can stop creating more PDM from this point.
    return;
  }

#ifdef XP_WIN
  if (MediaPrefs::PDMWMFEnabled()) {
    m = new WMFDecoderModule();
    RefPtr<PlatformDecoderModule> remote = new dom::RemoteDecoderModule(m);
    StartupPDM(remote);
    mWMFFailedToLoad = !StartupPDM(m);
  } else {
    mWMFFailedToLoad = MediaPrefs::DecoderDoctorWMFDisabledIsFailure();
  }
#endif
#ifdef MOZ_FFVPX
  if (MediaPrefs::PDMFFVPXEnabled()) {
    m = FFVPXRuntimeLinker::CreateDecoderModule();
    StartupPDM(m);
  }
#endif
#ifdef MOZ_FFMPEG
  if (MediaPrefs::PDMFFmpegEnabled()) {
    m = FFmpegRuntimeLinker::CreateDecoderModule();
    mFFmpegFailedToLoad = !StartupPDM(m);
  } else {
    mFFmpegFailedToLoad = false;
  }
#endif
#ifdef MOZ_APPLEMEDIA
  m = new AppleDecoderModule();
  StartupPDM(m);
#endif
#ifdef MOZ_GONK_MEDIACODEC
  if (MediaPrefs::PDMGonkDecoderEnabled()) {
    m = new GonkDecoderModule();
    StartupPDM(m);
  }
#endif
#ifdef MOZ_WIDGET_ANDROID
  if(MediaPrefs::PDMAndroidMediaCodecEnabled()){
    m = new AndroidDecoderModule();
    StartupPDM(m, MediaPrefs::PDMAndroidMediaCodecPreferred());
  }
#endif

  m = new AgnosticDecoderModule();
  StartupPDM(m);

  if (MediaPrefs::PDMGMPEnabled()) {
    m = new GMPDecoderModule();
    mGMPPDMFailedToStartup = !StartupPDM(m);
  } else {
    mGMPPDMFailedToStartup = false;
  }
}

void
PDMFactory::CreateNullPDM()
{
  mNullPDM = CreateNullDecoderModule();
  MOZ_ASSERT(mNullPDM && NS_SUCCEEDED(mNullPDM->Startup()));
}

bool
PDMFactory::StartupPDM(PlatformDecoderModule* aPDM, bool aInsertAtBeginning)
{
  if (aPDM && NS_SUCCEEDED(aPDM->Startup())) {
    if (aInsertAtBeginning) {
      mCurrentPDMs.InsertElementAt(0, aPDM);
    } else {
      mCurrentPDMs.AppendElement(aPDM);
    }
    return true;
  }
  return false;
}

already_AddRefed<PlatformDecoderModule>
PDMFactory::GetDecoder(const TrackInfo& aTrackInfo,
                       DecoderDoctorDiagnostics* aDiagnostics) const
{
  if (aDiagnostics) {
    // If libraries failed to load, the following loop over mCurrentPDMs
    // will not even try to use them. So we record failures now.
    if (mWMFFailedToLoad) {
      aDiagnostics->SetWMFFailedToLoad();
    }
    if (mFFmpegFailedToLoad) {
      aDiagnostics->SetFFmpegFailedToLoad();
    }
    if (mGMPPDMFailedToStartup) {
      aDiagnostics->SetGMPPDMFailedToStartup();
    }
  }

  RefPtr<PlatformDecoderModule> pdm;
  for (auto& current : mCurrentPDMs) {
    if (current->Supports(aTrackInfo, aDiagnostics)) {
      pdm = current;
      break;
    }
  }
  return pdm.forget();
}

void
PDMFactory::SetCDMProxy(CDMProxy* aProxy)
{
  MOZ_ASSERT(aProxy);

#ifdef MOZ_WIDGET_ANDROID
  if (IsWidevineKeySystem(aProxy->KeySystem())) {
    mEMEPDM = new AndroidDecoderModule(aProxy);
    return;
  }
#endif
  RefPtr<PDMFactory> m = new PDMFactory();
  mEMEPDM = new EMEDecoderModule(aProxy, m);
}

}  // namespace mozilla
