/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDMFactory.h"

#ifdef MOZ_AV1
#  include "AOMDecoder.h"
#endif
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
#include "OpusDecoder.h"
#include "TheoraDecoder.h"
#include "VPXDecoder.h"
#include "VideoUtils.h"
#include "VorbisDecoder.h"
#include "WAVDecoder.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/RemoteDecoderManagerChild.h"
#include "mozilla/RemoteDecoderModule.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/gfx/gfxVars.h"
#include "nsIXULRuntime.h"  // for BrowserTabsRemoteAutostart
#include "nsPrintfCString.h"

#ifdef XP_WIN
#  include "WMFDecoderModule.h"
#  include "mozilla/WindowsVersion.h"
#  ifdef MOZ_WMF_MEDIA_ENGINE
#    include "MFMediaEngineDecoderModule.h"
#  endif
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

#define PDM_INIT_LOG(msg, ...) \
  MOZ_LOG(sPDMLog, LogLevel::Debug, ("PDMInitializer, " msg, ##__VA_ARGS__))

extern already_AddRefed<PlatformDecoderModule> CreateNullDecoderModule();

class PDMInitializer final {
 public:
  // This function should only be executed ONCE per process.
  static void InitPDMs();

  // Return true if we've finished PDMs initialization.
  static bool HasInitializedPDMs();

 private:
  static void InitGpuPDMs() {
#ifdef XP_WIN
    if (!IsWin7AndPre2000Compatible()) {
      WMFDecoderModule::Init();
    }
#endif
  }

  static void InitRddPDMs() {
#ifdef XP_WIN
    if (!IsWin7AndPre2000Compatible()) {
      WMFDecoderModule::Init();
    }
#  ifdef MOZ_WMF_MEDIA_ENGINE
    if (IsWin8OrLater() && StaticPrefs::media_wmf_media_engine_enabled()) {
      MFMediaEngineDecoderModule::Init();
    }
#  endif
#endif
#ifdef MOZ_APPLEMEDIA
    AppleDecoderModule::Init();
#endif
#ifdef MOZ_FFVPX
    FFVPXRuntimeLinker::Init();
#endif
#ifdef MOZ_FFMPEG
    if (StaticPrefs::media_rdd_ffmpeg_enabled()) {
      FFmpegRuntimeLinker::Init();
    }
#endif
  }

  static void InitUtilityPDMs() {
#ifdef XP_WIN
    if (!IsWin7AndPre2000Compatible()) {
      WMFDecoderModule::Init();
    }
#endif
#ifdef MOZ_APPLEMEDIA
    AppleDecoderModule::Init();
#endif
#ifdef MOZ_FFVPX
    FFVPXRuntimeLinker::Init();
#endif
#ifdef MOZ_FFMPEG
    if (StaticPrefs::media_utility_ffmpeg_enabled()) {
      FFmpegRuntimeLinker::Init();
    }
#endif
  }

  static void InitContentPDMs() {
#ifdef XP_WIN
    if (!IsWin7AndPre2000Compatible()) {
#  ifdef MOZ_WMF
      if (!StaticPrefs::media_rdd_process_enabled() ||
          !StaticPrefs::media_rdd_wmf_enabled() ||
          !StaticPrefs::media_utility_process_enabled() ||
          !StaticPrefs::media_utility_wmf_enabled()) {
        WMFDecoderModule::Init();
      }
#  endif
    }
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
    RemoteDecoderManagerChild::Init();
  }

  static void InitDefaultPDMs() {
#ifdef XP_WIN
    if (!IsWin7AndPre2000Compatible()) {
      WMFDecoderModule::Init();
    }
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

  static bool sHasInitializedPDMs;
  static StaticMutex sMonitor MOZ_UNANNOTATED;
};

bool PDMInitializer::sHasInitializedPDMs = false;
StaticMutex PDMInitializer::sMonitor;

/* static */
void PDMInitializer::InitPDMs() {
  StaticMutexAutoLock mon(sMonitor);
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sHasInitializedPDMs);
  if (XRE_IsGPUProcess()) {
    PDM_INIT_LOG("Init PDMs in GPU process");
    InitGpuPDMs();
  } else if (XRE_IsRDDProcess()) {
    PDM_INIT_LOG("Init PDMs in RDD process");
    InitRddPDMs();
  } else if (XRE_IsUtilityProcess()) {
    PDM_INIT_LOG("Init PDMs in Utility process");
    InitUtilityPDMs();
  } else if (XRE_IsContentProcess()) {
    PDM_INIT_LOG("Init PDMs in Content process");
    InitContentPDMs();
  } else {
    MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(),
                          "PDMFactory is only usable in the "
                          "Parent/GPU/RDD/Utility/Content process");
    PDM_INIT_LOG("Init PDMs in Chrome process");
    InitDefaultPDMs();
  }
  sHasInitializedPDMs = true;
}

/* static */
bool PDMInitializer::HasInitializedPDMs() {
  StaticMutexAutoLock mon(sMonitor);
  return sHasInitializedPDMs;
}

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

PDMFactory::~PDMFactory() = default;

/* static */
void PDMFactory::EnsureInit() {
  if (PDMInitializer::HasInitializedPDMs()) {
    return;
  }
  auto initalization = []() {
    MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
    if (!PDMInitializer::HasInitializedPDMs()) {
      // Ensure that all system variables are initialized.
      gfx::gfxVars::Initialize();
      // Prime the preferences system from the main thread.
      Unused << BrowserTabsRemoteAutostart();
      PDMInitializer::InitPDMs();
    }
  };
  // If on the main thread, then initialize PDMs. Otherwise, do a sync-dispatch
  // to main thread.
  if (NS_IsMainThread()) {
    initalization();
    return;
  }
  nsCOMPtr<nsIEventTarget> mainTarget = GetMainThreadEventTarget();
  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
      "PDMFactory::EnsureInit", std::move(initalization));
  SyncRunnable::DispatchToThread(mainTarget, runnable);
}

RefPtr<PlatformDecoderModule::CreateDecoderPromise> PDMFactory::CreateDecoder(
    const CreateDecoderParams& aParams) {
  if (aParams.mUseNullDecoder.mUse) {
    MOZ_ASSERT(mNullPDM);
    return CreateDecoderWithPDM(mNullPDM, aParams);
  }
  bool isEncrypted = mEMEPDM && aParams.mConfig.mCrypto.IsEncrypted();

  if (isEncrypted) {
    return CreateDecoderWithPDM(mEMEPDM, aParams);
  }

  return CheckAndMaybeCreateDecoder(CreateDecoderParamsForAsync(aParams), 0);
}

RefPtr<PlatformDecoderModule::CreateDecoderPromise>
PDMFactory::CheckAndMaybeCreateDecoder(CreateDecoderParamsForAsync&& aParams,
                                       uint32_t aIndex,
                                       Maybe<MediaResult> aEarlierError) {
  uint32_t i = aIndex;
  auto params = SupportDecoderParams(aParams);
  for (; i < mCurrentPDMs.Length(); i++) {
    if (mCurrentPDMs[i]->Supports(params, nullptr /* diagnostic */) ==
        media::DecodeSupport::Unsupported) {
      continue;
    }
    RefPtr<PlatformDecoderModule::CreateDecoderPromise> p =
        CreateDecoderWithPDM(mCurrentPDMs[i], aParams)
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [](RefPtr<MediaDataDecoder>&& aDecoder) {
                  return PlatformDecoderModule::CreateDecoderPromise::
                      CreateAndResolve(std::move(aDecoder), __func__);
                },
                [self = RefPtr{this}, i, params = std::move(aParams)](
                    const MediaResult& aError) mutable {
                  // Try the next PDM.
                  return self->CheckAndMaybeCreateDecoder(std::move(params),
                                                          i + 1, Some(aError));
                });
    return p;
  }
  if (aEarlierError) {
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        std::move(*aEarlierError), __func__);
  }
  return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  nsPrintfCString("Error no decoder found for %s",
                                  aParams.mConfig->mMimeType.get())
                      .get()),
      __func__);
}

RefPtr<PlatformDecoderModule::CreateDecoderPromise>
PDMFactory::CreateDecoderWithPDM(PlatformDecoderModule* aPDM,
                                 const CreateDecoderParams& aParams) {
  MOZ_ASSERT(aPDM);
  MediaResult result = NS_OK;

  SupportChecker supportChecker;
  const TrackInfo& config = aParams.mConfig;
  supportChecker.AddMediaFormatChecker(config);

  auto checkResult = supportChecker.Check();
  if (checkResult.mReason != SupportChecker::Reason::kSupported) {
    if (checkResult.mReason ==
        SupportChecker::Reason::kVideoFormatNotSupported) {
      result = checkResult.mMediaResult;
    } else if (checkResult.mReason ==
               SupportChecker::Reason::kAudioFormatNotSupported) {
      result = checkResult.mMediaResult;
    }
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        result, __func__);
  }

  if (config.IsAudio()) {
    RefPtr<PlatformDecoderModule::CreateDecoderPromise> p;
    p = aPDM->AsyncCreateDecoder(aParams)->Then(
        GetCurrentSerialEventTarget(), __func__,
        [params = CreateDecoderParamsForAsync(aParams)](
            RefPtr<MediaDataDecoder>&& aDecoder) {
          RefPtr<MediaDataDecoder> decoder = std::move(aDecoder);
          if (!params.mNoWrapper.mDontUseWrapper) {
            decoder =
                new AudioTrimmer(decoder.forget(), CreateDecoderParams(params));
          }
          return PlatformDecoderModule::CreateDecoderPromise::CreateAndResolve(
              decoder, __func__);
        },
        [](const MediaResult& aError) {
          return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
              aError, __func__);
        });
    return p;
  }

  if (!config.IsVideo()) {
    return PlatformDecoderModule::CreateDecoderPromise::CreateAndReject(
        MediaResult(
            NS_ERROR_DOM_MEDIA_FATAL_ERR,
            RESULT_DETAIL(
                "Decoder configuration error, expected audio or video.")),
        __func__);
  }

  if ((MP4Decoder::IsH264(config.mMimeType) ||
#ifdef MOZ_AV1
       AOMDecoder::IsAV1(config.mMimeType) ||
#endif
       VPXDecoder::IsVPX(config.mMimeType)) &&
      !aParams.mUseNullDecoder.mUse && !aParams.mNoWrapper.mDontUseWrapper) {
    return MediaChangeMonitor::Create(this, aParams);
  }
  return aPDM->AsyncCreateDecoder(aParams);
}

bool PDMFactory::SupportsMimeType(const nsACString& aMimeType) const {
  UniquePtr<TrackInfo> trackInfo = CreateTrackInfoWithMIMEType(aMimeType);
  if (!trackInfo) {
    return false;
  }
  return Supports(SupportDecoderParams(*trackInfo), nullptr);
}

bool PDMFactory::Supports(const SupportDecoderParams& aParams,
                          DecoderDoctorDiagnostics* aDiagnostics) const {
  if (mEMEPDM) {
    return mEMEPDM->Supports(aParams, aDiagnostics) !=
           media::DecodeSupport::Unsupported;
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
  } else if (XRE_IsUtilityProcess()) {
    CreateUtilityPDMs();
  } else if (XRE_IsContentProcess()) {
    CreateContentPDMs();
  } else {
    MOZ_DIAGNOSTIC_ASSERT(
        XRE_IsParentProcess(),
        "PDMFactory is only usable in the Parent/GPU/RDD/Content process");
    CreateDefaultPDMs();
  }
}

void PDMFactory::CreateGpuPDMs() {
#ifdef XP_WIN
  if (StaticPrefs::media_wmf_enabled() && !IsWin7AndPre2000Compatible()) {
    CreateAndStartupPDM<WMFDecoderModule>();
  }
#endif
}

#if defined(MOZ_FFMPEG)
static DecoderDoctorDiagnostics::Flags GetFailureFlagBasedOnFFmpegStatus(
    const FFmpegRuntimeLinker::LinkStatus& aStatus) {
  switch (aStatus) {
    case FFmpegRuntimeLinker::LinkStatus_INVALID_FFMPEG_CANDIDATE:
    case FFmpegRuntimeLinker::LinkStatus_UNUSABLE_LIBAV57:
    case FFmpegRuntimeLinker::LinkStatus_INVALID_LIBAV_CANDIDATE:
    case FFmpegRuntimeLinker::LinkStatus_OBSOLETE_FFMPEG:
    case FFmpegRuntimeLinker::LinkStatus_OBSOLETE_LIBAV:
    case FFmpegRuntimeLinker::LinkStatus_INVALID_CANDIDATE:
      return DecoderDoctorDiagnostics::Flags::LibAVCodecUnsupported;
    default:
      MOZ_DIAGNOSTIC_ASSERT(
          aStatus == FFmpegRuntimeLinker::LinkStatus_NOT_FOUND,
          "Only call this method when linker fails.");
      return DecoderDoctorDiagnostics::Flags::FFmpegNotFound;
  }
}
#endif

void PDMFactory::CreateRddPDMs() {
#ifdef XP_WIN
#  ifdef MOZ_WMF_MEDIA_ENGINE
  if (IsWin8OrLater() && StaticPrefs::media_wmf_media_engine_enabled()) {
    CreateAndStartupPDM<MFMediaEngineDecoderModule>();
  }
#  endif
  if (StaticPrefs::media_wmf_enabled() &&
      StaticPrefs::media_rdd_wmf_enabled()) {
    CreateAndStartupPDM<WMFDecoderModule>();
  }
#endif
#ifdef MOZ_APPLEMEDIA
  if (StaticPrefs::media_rdd_applemedia_enabled()) {
    CreateAndStartupPDM<AppleDecoderModule>();
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
    mFailureFlags += GetFailureFlagBasedOnFFmpegStatus(
        FFmpegRuntimeLinker::LinkStatusCode());
  }
#endif
  CreateAndStartupPDM<AgnosticDecoderModule>();
}

void PDMFactory::CreateUtilityPDMs() {
#ifdef XP_WIN
  if (StaticPrefs::media_wmf_enabled() &&
      StaticPrefs::media_utility_wmf_enabled()) {
    CreateAndStartupPDM<WMFDecoderModule>();
  }
#endif
#ifdef MOZ_APPLEMEDIA
  if (StaticPrefs::media_utility_applemedia_enabled()) {
    CreateAndStartupPDM<AppleDecoderModule>();
  }
#endif
#ifdef MOZ_FFVPX
  if (StaticPrefs::media_ffvpx_enabled() &&
      StaticPrefs::media_utility_ffvpx_enabled()) {
    CreateAndStartupPDM<FFVPXRuntimeLinker>();
  }
#endif
#ifdef MOZ_FFMPEG
  if (StaticPrefs::media_ffmpeg_enabled() &&
      StaticPrefs::media_utility_ffmpeg_enabled() &&
      !CreateAndStartupPDM<FFmpegRuntimeLinker>()) {
    mFailureFlags += GetFailureFlagBasedOnFFmpegStatus(
        FFmpegRuntimeLinker::LinkStatusCode());
  }
#endif
#ifdef MOZ_WIDGET_ANDROID
  if (StaticPrefs::media_utility_android_media_codec_enabled()) {
    StartupPDM(AndroidDecoderModule::Create(),
               StaticPrefs::media_android_media_codec_preferred());
  }
#endif
  CreateAndStartupPDM<AgnosticDecoderModule>();
}

void PDMFactory::CreateContentPDMs() {
  if (StaticPrefs::media_gpu_process_decoder()) {
    CreateAndStartupPDM<RemoteDecoderModule>(RemoteDecodeIn::GpuProcess);
  }

  if (StaticPrefs::media_rdd_process_enabled()) {
    CreateAndStartupPDM<RemoteDecoderModule>(RemoteDecodeIn::RddProcess);
  }

  if (StaticPrefs::media_utility_process_enabled()) {
    CreateAndStartupPDM<RemoteDecoderModule>(RemoteDecodeIn::UtilityProcess);
  }

#ifdef XP_WIN
  if (StaticPrefs::media_wmf_enabled() && !IsWin7AndPre2000Compatible()) {
#  ifdef MOZ_WMF
    if (!StaticPrefs::media_rdd_process_enabled() ||
        !StaticPrefs::media_rdd_wmf_enabled()) {
      if (!CreateAndStartupPDM<WMFDecoderModule>()) {
        mFailureFlags += DecoderDoctorDiagnostics::Flags::WMFFailedToLoad;
      }
    }
#  endif
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
    mFailureFlags += GetFailureFlagBasedOnFFmpegStatus(
        FFmpegRuntimeLinker::LinkStatusCode());
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

void PDMFactory::CreateDefaultPDMs() {
#ifdef XP_WIN
  if (StaticPrefs::media_wmf_enabled() && !IsWin7AndPre2000Compatible()) {
    if (!CreateAndStartupPDM<WMFDecoderModule>()) {
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
    mFailureFlags += GetFailureFlagBasedOnFFmpegStatus(
        FFmpegRuntimeLinker::LinkStatusCode());
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
  for (const auto& current : mCurrentPDMs) {
    if (current->Supports(aParams, aDiagnostics) !=
        media::DecodeSupport::Unsupported) {
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

/* static */
PDMFactory::MediaCodecsSupported PDMFactory::Supported(bool aForceRefresh) {
  MOZ_ASSERT(NS_IsMainThread());

  static auto calculate = []() {
    auto pdm = MakeRefPtr<PDMFactory>();
    MediaCodecsSupported supported;
    // H264 and AAC depends on external framework that must be dynamically
    // loaded.
    // We currently only ship a single PDM per platform able to decode AAC or
    // H264. As such we can assert that being able to create a H264 or AAC
    // decoder indicates that with WMF on Windows or FFmpeg on Unixes is
    // available.
    // This logic will have to be revisited if a PDM supporting either codec
    // will be added in addition to the WMF and FFmpeg PDM (such as OpenH264)
    if (pdm->SupportsMimeType("video/avc"_ns)) {
      supported += MediaCodecs::H264;
    }
    if (pdm->SupportsMimeType("video/vp9"_ns)) {
      supported += MediaCodecs::VP9;
    }
    if (pdm->SupportsMimeType("video/vp8"_ns)) {
      supported += MediaCodecs::VP8;
    }
    if (pdm->SupportsMimeType("video/av1"_ns)) {
      supported += MediaCodecs::AV1;
    }
    if (pdm->SupportsMimeType("video/theora"_ns)) {
      supported += MediaCodecs::Theora;
    }
    if (pdm->SupportsMimeType("audio/mp4a-latm"_ns)) {
      supported += MediaCodecs::AAC;
    }
    // MP3 can be either decoded by ffvpx or WMF/FFmpeg
    if (pdm->SupportsMimeType("audio/mpeg"_ns)) {
      supported += MediaCodecs::MP3;
    }
    if (pdm->SupportsMimeType("audio/opus"_ns)) {
      supported += MediaCodecs::Opus;
    }
    if (pdm->SupportsMimeType("audio/vorbis"_ns)) {
      supported += MediaCodecs::Vorbis;
    }
    if (pdm->SupportsMimeType("audio/flac"_ns)) {
      supported += MediaCodecs::Flac;
    }
    if (pdm->SupportsMimeType("audio/x-wav"_ns)) {
      supported += MediaCodecs::Wave;
    }
    return supported;
  };
  static MediaCodecsSupported supported = calculate();
  if (aForceRefresh) {
    supported = calculate();
  }
  return supported;
}

/* static */
bool PDMFactory::SupportsMimeType(const nsACString& aMimeType,
                                  const MediaCodecsSupported& aSupported,
                                  RemoteDecodeIn aLocation) {
  const bool videoSupport = aLocation != RemoteDecodeIn::UtilityProcess;
  const bool audioSupport = (aLocation == RemoteDecodeIn::UtilityProcess &&
                             StaticPrefs::media_utility_process_enabled()) ||
                            (aLocation == RemoteDecodeIn::RddProcess &&
                             !StaticPrefs::media_utility_process_enabled());

  if (videoSupport) {
    if (MP4Decoder::IsH264(aMimeType)) {
      return aSupported.contains(MediaCodecs::H264);
    }
    if (VPXDecoder::IsVP9(aMimeType)) {
      return aSupported.contains(MediaCodecs::VP9);
    }
    if (VPXDecoder::IsVP8(aMimeType)) {
      return aSupported.contains(MediaCodecs::VP8);
    }
#ifdef MOZ_AV1
    if (AOMDecoder::IsAV1(aMimeType)) {
      return aSupported.contains(MediaCodecs::AV1);
    }
#endif
    if (TheoraDecoder::IsTheora(aMimeType)) {
      return aSupported.contains(MediaCodecs::Theora);
    }
  }

  if (audioSupport) {
    if (MP4Decoder::IsAAC(aMimeType)) {
      return aSupported.contains(MediaCodecs::AAC);
    }
    if (aMimeType.EqualsLiteral("audio/mpeg")) {
      return aSupported.contains(MediaCodecs::MP3);
    }
    if (OpusDataDecoder::IsOpus(aMimeType)) {
      return aSupported.contains(MediaCodecs::Opus);
    }
    if (VorbisDataDecoder::IsVorbis(aMimeType)) {
      return aSupported.contains(MediaCodecs::Vorbis);
    }
    if (aMimeType.EqualsLiteral("audio/flac")) {
      return aSupported.contains(MediaCodecs::Flac);
    }
    if (WaveDataDecoder::IsWave(aMimeType)) {
      return aSupported.contains(MediaCodecs::Wave);
    }
  }
  return false;
}

/* static */
bool PDMFactory::AllDecodersAreRemote() {
  return StaticPrefs::media_rdd_process_enabled() &&
#if defined(MOZ_FFVPX)
         StaticPrefs::media_rdd_ffvpx_enabled() &&
#endif
         StaticPrefs::media_rdd_opus_enabled() &&
         StaticPrefs::media_rdd_theora_enabled() &&
         StaticPrefs::media_rdd_vorbis_enabled() &&
         StaticPrefs::media_rdd_vpx_enabled() &&
#if defined(MOZ_WMF)
         StaticPrefs::media_rdd_wmf_enabled() &&
#endif
         StaticPrefs::media_rdd_wav_enabled();
}

#undef PDM_INIT_LOG
}  // namespace mozilla
