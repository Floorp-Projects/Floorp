/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDMFactory.h"
#include "AgnosticDecoderModule.h"
#include "AudioTrimmer.h"
#include "DecoderDoctorDiagnostics.h"
#include "EMEDecoderModule.h"
#include "GMPDecoderModule.h"
#include "H264.h"
#include "MP4Decoder.h"
#include "MediaChangeMonitor.h"
#include "MediaInfo.h"
#include "VPXDecoder.h"
#include "gfxPrefs.h"
#include "nsIXULRuntime.h" // for BrowserTabsRemoteAutostart
#include "mozilla/CDMProxy.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/GpuDecoderModule.h"
#include "mozilla/RemoteDecoderModule.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/gfx/gfxVars.h"

#ifdef XP_WIN
#  include "WMFDecoderModule.h"
#  include "mozilla/WindowsVersion.h"
#endif
#ifdef MOZ_FFVPX
#  include "FFVPXRuntimeLinker.h"
#endif
#ifdef MOZ_FFMPEG
#  include "FFmpegRuntimeLinker.h"
#endif
#ifdef MOZ_APPLEMEDIA
#  include "AppleDecoderModule.h"
#endif
#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidDecoderModule.h"
#endif
#ifdef MOZ_OMX
#  include "OmxDecoderModule.h"
#endif

#include <functional>

namespace mozilla {

extern already_AddRefed<PlatformDecoderModule> CreateBlankDecoderModule();
extern already_AddRefed<PlatformDecoderModule> CreateNullDecoderModule();

class PDMFactoryImpl final {
 public:
  PDMFactoryImpl() {
#ifdef XP_WIN
    WMFDecoderModule::Init();
#endif
#ifdef MOZ_APPLEMEDIA
    AppleDecoderModule::Init();
#endif
#ifdef MOZ_OMX
    OmxDecoderModule::Init();
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

class SupportChecker {
 public:
  enum class Reason : uint8_t {
    kSupported,
    kVideoFormatNotSupported,
    kAudioFormatNotSupported,
    kUnknown,
  };

  struct CheckResult {
    explicit CheckResult(Reason aReason,
                         MediaResult aResult = MediaResult(NS_OK))
        : mReason(aReason), mMediaResult(std::move(aResult)) {}
    CheckResult(const CheckResult& aOther) = default;
    CheckResult(CheckResult&& aOther) = default;
    CheckResult& operator=(const CheckResult& aOther) = default;
    CheckResult& operator=(CheckResult&& aOther) = default;

    Reason mReason;
    MediaResult mMediaResult;
  };

  template <class Func>
  void AddToCheckList(Func&& aChecker) {
    mCheckerList.AppendElement(std::forward<Func>(aChecker));
  }

  void AddMediaFormatChecker(const TrackInfo& aTrackConfig) {
    if (aTrackConfig.IsVideo()) {
      auto mimeType = aTrackConfig.GetAsVideoInfo()->mMimeType;
      RefPtr<MediaByteBuffer> extraData =
          aTrackConfig.GetAsVideoInfo()->mExtraData;
      AddToCheckList([mimeType, extraData]() {
        if (MP4Decoder::IsH264(mimeType)) {
          SPSData spsdata;
          // WMF H.264 Video Decoder and Apple ATDecoder
          // do not support YUV444 format.
          // For consistency, all decoders should be checked.
          if (H264::DecodeSPSFromExtraData(extraData, spsdata) &&
              (spsdata.profile_idc == 244 /* Hi444PP */ ||
               spsdata.chroma_format_idc == PDMFactory::kYUV444)) {
            return CheckResult(
                SupportChecker::Reason::kVideoFormatNotSupported,
                MediaResult(
                    NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Decoder may not have the capability "
                                  "to handle the requested video format "
                                  "with YUV444 chroma subsampling.")));
          }
        }
        return CheckResult(SupportChecker::Reason::kSupported);
      });
    }
  }

  SupportChecker::CheckResult Check() {
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
};  // SupportChecker

PDMFactory::PDMFactory() {
  EnsureInit();
  CreatePDMs();
  CreateNullPDM();
}

PDMFactory::~PDMFactory() {}

/* static */
void PDMFactory::EnsureInit() {
  {
    StaticMutexAutoLock mon(sMonitor);
    if (sInstance) {
      // Quick exit if we already have an instance.
      return;
    }
  }

  auto initalization = []() {
    MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
    StaticMutexAutoLock mon(sMonitor);
    if (!sInstance) {
      // Ensure that all system variables are initialized.
      gfx::gfxVars::Initialize();
      gfxPrefs::GetSingleton();
      // On the main thread and holding the lock -> Create instance.
      sInstance = new PDMFactoryImpl();
      ClearOnShutdown(&sInstance);
    }
  };
  if (NS_IsMainThread()) {
    initalization();
    return;
  }

  // Not on the main thread -> Sync-dispatch creation to main thread.
  nsCOMPtr<nsIEventTarget> mainTarget = GetMainThreadEventTarget();
  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
      "PDMFactory::EnsureInit", std::move(initalization));
  SyncRunnable::DispatchToThread(mainTarget, runnable);
}

already_AddRefed<MediaDataDecoder> PDMFactory::CreateDecoder(
    const CreateDecoderParams& aParams) {
  RefPtr<MediaDataDecoder> decoder;
  const TrackInfo& config = aParams.mConfig;
  if (aParams.mUseNullDecoder.mUse) {
    MOZ_ASSERT(mNullPDM);
    decoder = CreateDecoderWithPDM(mNullPDM, aParams);
  } else {
    bool isEncrypted = mEMEPDM && config.mCrypto.IsEncrypted();

    if (isEncrypted) {
      decoder = CreateDecoderWithPDM(mEMEPDM, aParams);
    } else {
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
        if (!current->Supports(config, diagnostics)) {
          continue;
        }
        decoder = CreateDecoderWithPDM(current, aParams);
        if (decoder) {
          break;
        }
      }
    }
  }
  if (!decoder) {
    NS_WARNING("Unable to create a decoder, no platform found.");
    return nullptr;
  }
  if (config.IsAudio()) {
    decoder = new AudioTrimmer(decoder.forget(), aParams);
  }
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder> PDMFactory::CreateDecoderWithPDM(
    PlatformDecoderModule* aPDM, const CreateDecoderParams& aParams) {
  MOZ_ASSERT(aPDM);
  RefPtr<MediaDataDecoder> m;
  MediaResult* result = aParams.mError;

  SupportChecker supportChecker;
  const TrackInfo& config = aParams.mConfig;
  supportChecker.AddMediaFormatChecker(config);

  auto checkResult = supportChecker.Check();
  if (checkResult.mReason != SupportChecker::Reason::kSupported) {
    DecoderDoctorDiagnostics* diagnostics = aParams.mDiagnostics;
    if (checkResult.mReason ==
        SupportChecker::Reason::kVideoFormatNotSupported) {
      if (diagnostics) {
        diagnostics->SetVideoNotSupported();
      }
      if (result) {
        *result = checkResult.mMediaResult;
      }
    } else if (checkResult.mReason ==
               SupportChecker::Reason::kAudioFormatNotSupported) {
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

  if ((MP4Decoder::IsH264(config.mMimeType) ||
       VPXDecoder::IsVPX(config.mMimeType)) &&
      !aParams.mUseNullDecoder.mUse && !aParams.mNoWrapper.mDontUseWrapper) {
    RefPtr<MediaChangeMonitor> h = new MediaChangeMonitor(aPDM, aParams);
    const MediaResult result = h->GetLastError();
    if (NS_SUCCEEDED(result) || result == NS_ERROR_NOT_INITIALIZED) {
      // The MediaChangeMonitor either successfully created the wrapped decoder,
      // or there wasn't enough initialization data to do so (such as what can
      // happen with AVC3). Otherwise, there was some problem, for example WMF
      // DLLs were missing.
      m = h.forget();
    } else if (aParams.mError) {
      *aParams.mError = result;
    }
  } else {
    m = aPDM->CreateVideoDecoder(aParams);
  }

  return m.forget();
}

bool PDMFactory::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  UniquePtr<TrackInfo> trackInfo = CreateTrackInfoWithMIMEType(aMimeType);
  if (!trackInfo) {
    return false;
  }
  return Supports(*trackInfo, aDiagnostics);
}

bool PDMFactory::Supports(const TrackInfo& aTrackInfo,
                          DecoderDoctorDiagnostics* aDiagnostics) const {
  if (mEMEPDM) {
    return mEMEPDM->Supports(aTrackInfo, aDiagnostics);
  }
  if (VPXDecoder::IsVPX(aTrackInfo.mMimeType,
                        VPXDecoder::VP8 | VPXDecoder::VP9)) {
    // Work around bug 1521370, where trying to instantiate an external decoder
    // could cause a crash.
    // We always ship a VP8/VP9 decoder (libvpx) and optionally we have ffvpx.
    // So we can speed up the test by assuming that this codec is supported.
    return true;
  }
  RefPtr<PlatformDecoderModule> current = GetDecoder(aTrackInfo, aDiagnostics);
  return !!current;
}

void PDMFactory::CreatePDMs() {
  RefPtr<PlatformDecoderModule> m;

  if (StaticPrefs::MediaUseBlankDecoder()) {
    m = CreateBlankDecoderModule();
    StartupPDM(m);
    // The Blank PDM SupportsMimeType reports true for all codecs; the creation
    // of its decoder is infallible. As such it will be used for all media, we
    // can stop creating more PDM from this point.
    return;
  }

  if (StaticPrefs::MediaRddProcessEnabled()
      && BrowserTabsRemoteAutostart()) {
    m = new RemoteDecoderModule;
    StartupPDM(m);
  }

#ifdef XP_WIN
  if (StaticPrefs::MediaWmfEnabled() && !IsWin7AndPre2000Compatible()) {
    m = new WMFDecoderModule();
    RefPtr<PlatformDecoderModule> remote = new GpuDecoderModule(m);
    StartupPDM(remote);
    mWMFFailedToLoad = !StartupPDM(m);
  } else {
    mWMFFailedToLoad = StaticPrefs::MediaDecoderDoctorWmfDisabledIsFailure();
  }
#endif
#ifdef MOZ_OMX
  if (StaticPrefs::MediaOmxEnabled()) {
    m = OmxDecoderModule::Create();
    StartupPDM(m);
  }
#endif
#ifdef MOZ_FFVPX
  if (StaticPrefs::MediaFfvpxEnabled()) {
    m = FFVPXRuntimeLinker::CreateDecoderModule();
    StartupPDM(m);
  }
#endif
#ifdef MOZ_FFMPEG
  if (StaticPrefs::MediaFfmpegEnabled()) {
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
#ifdef MOZ_WIDGET_ANDROID
  if (StaticPrefs::MediaAndroidMediaCodecEnabled()) {
    m = new AndroidDecoderModule();
    StartupPDM(m, StaticPrefs::MediaAndroidMediaCodecPreferred());
  }
#endif

  m = new AgnosticDecoderModule();
  StartupPDM(m);

  if (StaticPrefs::MediaGmpDecoderEnabled()) {
    m = new GMPDecoderModule();
    mGMPPDMFailedToStartup = !StartupPDM(m);
  } else {
    mGMPPDMFailedToStartup = false;
  }
}

void PDMFactory::CreateNullPDM() {
  mNullPDM = CreateNullDecoderModule();
  MOZ_ASSERT(mNullPDM && NS_SUCCEEDED(mNullPDM->Startup()));
}

bool PDMFactory::StartupPDM(PlatformDecoderModule* aPDM,
                            bool aInsertAtBeginning) {
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

already_AddRefed<PlatformDecoderModule> PDMFactory::GetDecoder(
    const TrackInfo& aTrackInfo, DecoderDoctorDiagnostics* aDiagnostics) const {
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

void PDMFactory::SetCDMProxy(CDMProxy* aProxy) {
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
