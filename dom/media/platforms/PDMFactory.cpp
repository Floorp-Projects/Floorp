/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDMFactory.h"

#include "AgnosticDecoderModule.h"
#include "AudioTrimmer.h"
#include "BlankDecoderModule.h"
#include "DecoderDoctorDiagnostics.h"
#include "EMEDecoderModule.h"
#include "GMPDecoderModule.h"
#include "H264.h"
#include "MP4Decoder.h"
#include "MediaChangeMonitor.h"
#include "MediaInfo.h"
#include "VPXDecoder.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/RemoteDecoderManagerChild.h"
#include "mozilla/RemoteDecoderModule.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/gfx/gfxVars.h"
#include "nsIXULRuntime.h"  // for BrowserTabsRemoteAutostart

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
    RemoteDecoderModule::Init();
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
      // Prime the preferences system from the main thread.
      Unused << BrowserTabsRemoteAutostart();
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
        diagnostics->SetFailureFlags(mFailureFlags);
      }

      for (auto& current : mCurrentPDMs) {
        if (!current->Supports(SupportDecoderParams(aParams), diagnostics)) {
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
    if (!aParams.mNoWrapper.mDontUseWrapper) {
      m = new AudioTrimmer(m.forget(), aParams);
    }
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
      m = std::move(h);
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
  return Supports(SupportDecoderParams(*trackInfo), aDiagnostics);
}

bool PDMFactory::Supports(const SupportDecoderParams& aParams,
                          DecoderDoctorDiagnostics* aDiagnostics) const {
  if (mEMEPDM) {
    return mEMEPDM->Supports(aParams, aDiagnostics);
  }
  if (VPXDecoder::IsVPX(aParams.MimeType(),
                        VPXDecoder::VP8 | VPXDecoder::VP9)) {
    // Work around bug 1521370, where trying to instantiate an external decoder
    // could cause a crash.
    // We always ship a VP8/VP9 decoder (libvpx) and optionally we have ffvpx.
    // So we can speed up the test by assuming that this codec is supported.
    return true;
  }

  RefPtr<PlatformDecoderModule> current =
      GetDecoderModule(aParams, aDiagnostics);
  return !!current;
}

void PDMFactory::CreatePDMs() {
  if (StaticPrefs::media_use_blank_decoder()) {
    CreateAndStartupPDM<BlankDecoderModule>();
    // The Blank PDM SupportsMimeType reports true for all codecs; the creation
    // of its decoder is infallible. As such it will be used for all media, we
    // can stop creating more PDM from this point.
    return;
  }

  if (XRE_IsGPUProcess()) {
    CreateGpuPDMs();
  } else if (XRE_IsRDDProcess()) {
    CreateRddPDMs();
  } else {
    CreateDefaultPDMs();
  }
}

void PDMFactory::CreateGpuPDMs() {
  MOZ_ASSERT(XRE_IsGPUProcess());
#ifdef XP_WIN
  if (StaticPrefs::media_wmf_enabled() && !IsWin7AndPre2000Compatible()) {
    CreateAndStartupPDM<WMFDecoderModule>();
  }
#endif
}

void PDMFactory::CreateRddPDMs() {
  MOZ_ASSERT(XRE_IsRDDProcess());
#ifdef XP_WIN
  if (StaticPrefs::media_wmf_enabled() &&
      StaticPrefs::media_rdd_wmf_enabled()) {
    CreateAndStartupPDM<WMFDecoderModule>();
  }
#endif
#ifdef MOZ_FFVPX
  if (StaticPrefs::media_ffvpx_enabled() &&
      StaticPrefs::media_rdd_ffvpx_enabled()) {
    CreateAndStartupPDM<FFVPXRuntimeLinker>();
  }
#endif
#ifdef MOZ_FFMPEG
  if (StaticPrefs::media_ffmpeg_enabled() &&
      StaticPrefs::media_rdd_ffmpeg_enabled() &&
      !CreateAndStartupPDM<FFmpegRuntimeLinker>()) {
    mFailureFlags += DecoderDoctorDiagnostics::Flags::FFmpegFailedToLoad;
  } else {
    mFailureFlags -= DecoderDoctorDiagnostics::Flags::FFmpegFailedToLoad;
  }
#endif
  CreateAndStartupPDM<AgnosticDecoderModule>();
}

void PDMFactory::CreateDefaultPDMs() {
  MOZ_ASSERT(!XRE_IsGPUProcess() && !XRE_IsRDDProcess());
  if (StaticPrefs::media_gpu_process_decoder()) {
    CreateAndStartupPDM<RemoteDecoderModule>(RemoteDecodeIn::GpuProcess);
  }

  if (StaticPrefs::media_rdd_process_enabled() &&
      BrowserTabsRemoteAutostart()) {
    CreateAndStartupPDM<RemoteDecoderModule>(RemoteDecodeIn::RddProcess);
  }

#ifdef XP_WIN
  if (StaticPrefs::media_wmf_enabled() && !IsWin7AndPre2000Compatible()) {
    RefPtr<WMFDecoderModule> m = MakeAndAddRef<WMFDecoderModule>();
    if (!StartupPDM(m.forget())) {
      mFailureFlags += DecoderDoctorDiagnostics::Flags::WMFFailedToLoad;
    }
  } else if (StaticPrefs::media_decoder_doctor_wmf_disabled_is_failure()) {
    mFailureFlags += DecoderDoctorDiagnostics::Flags::WMFFailedToLoad;
  }
#endif

#ifdef MOZ_APPLEMEDIA
  CreateAndStartupPDM<AppleDecoderModule>();
#endif
#ifdef MOZ_OMX
  if (StaticPrefs::media_omx_enabled()) {
    CreateAndStartupPDM<OmxDecoderModule>();
  }
#endif
#ifdef MOZ_FFVPX
  if (StaticPrefs::media_ffvpx_enabled()) {
    CreateAndStartupPDM<FFVPXRuntimeLinker>();
  }
#endif
#ifdef MOZ_FFMPEG
  if (StaticPrefs::media_ffmpeg_enabled() &&
      !CreateAndStartupPDM<FFmpegRuntimeLinker>()) {
    mFailureFlags += DecoderDoctorDiagnostics::Flags::FFmpegFailedToLoad;
  }
#endif
#ifdef MOZ_WIDGET_ANDROID
  if (StaticPrefs::media_android_media_codec_enabled()) {
    StartupPDM(AndroidDecoderModule::Create(),
               StaticPrefs::media_android_media_codec_preferred());
  }
#endif

  CreateAndStartupPDM<AgnosticDecoderModule>();

  if (StaticPrefs::media_gmp_decoder_enabled() &&
      !CreateAndStartupPDM<GMPDecoderModule>()) {
    mFailureFlags += DecoderDoctorDiagnostics::Flags::GMPPDMFailedToStartup;
  }
}

void PDMFactory::CreateNullPDM() {
  mNullPDM = CreateNullDecoderModule();
  MOZ_ASSERT(mNullPDM && NS_SUCCEEDED(mNullPDM->Startup()));
}

bool PDMFactory::StartupPDM(already_AddRefed<PlatformDecoderModule> aPDM,
                            bool aInsertAtBeginning) {
  RefPtr<PlatformDecoderModule> pdm = aPDM;
  if (pdm && NS_SUCCEEDED(pdm->Startup())) {
    if (aInsertAtBeginning) {
      mCurrentPDMs.InsertElementAt(0, pdm);
    } else {
      mCurrentPDMs.AppendElement(pdm);
    }
    return true;
  }
  return false;
}

already_AddRefed<PlatformDecoderModule> PDMFactory::GetDecoderModule(
    const SupportDecoderParams& aParams,
    DecoderDoctorDiagnostics* aDiagnostics) const {
  if (aDiagnostics) {
    // If libraries failed to load, the following loop over mCurrentPDMs
    // will not even try to use them. So we record failures now.
    aDiagnostics->SetFailureFlags(mFailureFlags);
  }

  RefPtr<PlatformDecoderModule> pdm;
  for (auto& current : mCurrentPDMs) {
    if (current->Supports(aParams, aDiagnostics)) {
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
    mEMEPDM = AndroidDecoderModule::Create(aProxy);
    return;
  }
#endif
  auto m = MakeRefPtr<PDMFactory>();
  mEMEPDM = MakeRefPtr<EMEDecoderModule>(aProxy, m);
}

}  // namespace mozilla
