/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFDecoderModule.h"

#include <algorithm>
#include <vector>

#include "DriverCrashGuard.h"
#include "GfxDriverInfo.h"
#include "MFTDecoder.h"
#include "MP4Decoder.h"
#include "MediaInfo.h"
#include "PDMFactory.h"
#include "VPXDecoder.h"
#include "WMFAudioMFTManager.h"
#include "WMFMediaDataDecoder.h"
#include "WMFVideoMFTManager.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/mscom/EnsureMTA.h"
#include "mozilla/ProfilerMarkers.h"
#include "nsComponentManagerUtils.h"
#include "nsIXULRuntime.h"
#include "nsIXULRuntime.h"  // for BrowserTabsRemoteAutostart
#include "nsServiceManagerUtils.h"
#include "nsWindowsHelpers.h"
#include "prsystem.h"

#ifdef MOZ_AV1
#  include "AOMDecoder.h"
#endif

#define LOG(...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {

// Helper function to add a profile marker and log at the same time.
static void MOZ_FORMAT_PRINTF(2, 3)
    WmfDecoderModuleMarkerAndLog(const ProfilerString8View& aMarkerTag,
                                 const char* aFormat, ...) {
  va_list ap;
  va_start(ap, aFormat);
  const nsVprintfCString markerString(aFormat, ap);
  va_end(ap);
  PROFILER_MARKER_TEXT(aMarkerTag, MEDIA_PLAYBACK, {}, markerString);
  LOG("%s", markerString.get());
}

static const GUID CLSID_CMSAACDecMFT = {
    0x32D186A7,
    0x218F,
    0x4C75,
    {0x88, 0x76, 0xDD, 0x77, 0x27, 0x3A, 0x89, 0x99}};

static Atomic<bool> sDXVAEnabled(false);

/* static */
already_AddRefed<PlatformDecoderModule> WMFDecoderModule::Create() {
  RefPtr<WMFDecoderModule> wmf = new WMFDecoderModule();
  return wmf.forget();
}

static bool IsRemoteAcceleratedCompositor(
    layers::KnowsCompositor* aKnowsCompositor) {
  if (!aKnowsCompositor) {
    return false;
  }

  if (aKnowsCompositor->UsingSoftwareWebRenderD3D11()) {
    return true;
  }

  layers::TextureFactoryIdentifier ident =
      aKnowsCompositor->GetTextureFactoryIdentifier();
  return !aKnowsCompositor->UsingSoftwareWebRender() &&
         ident.mParentProcessType == GeckoProcessType_GPU;
}

static Atomic<bool> sSupportedTypesInitialized(false);
static EnumSet<WMFStreamType> sSupportedTypes;
static EnumSet<WMFStreamType> sLackOfExtensionTypes;

/* static */
void WMFDecoderModule::Init(Config aConfig) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  if (XRE_IsContentProcess()) {
    // If we're in the content process and the UseGPUDecoder pref is set, it
    // means that we've given up on the GPU process (it's been crashing) so we
    // should disable DXVA
    sDXVAEnabled = !StaticPrefs::media_gpu_process_decoder();
  } else if (XRE_IsGPUProcess()) {
    // Always allow DXVA in the GPU process.
    sDXVAEnabled = true;
    if (aConfig == Config::ForceEnableHEVC) {
      WmfDecoderModuleMarkerAndLog(
          "ReportHardwareSupport",
          "Enable HEVC for reporting hardware support telemetry");
      sForceEnableHEVC = true;
    } else {
      sForceEnableHEVC = false;
    }
  } else if (XRE_IsRDDProcess()) {
    // Hardware accelerated decoding is explicitly only done in the GPU process
    // to avoid copying textures whenever possible. Previously, detecting
    // whether the video bridge was set up could be done with the following:
    // sDXVAEnabled = !!DeviceManagerDx::Get()->GetImageDevice();
    // The video bridge was previously broken due to initialization order
    // issues. For more information see Bug 1763880.
    sDXVAEnabled = false;
  } else {
    // Only allow DXVA in the UI process if we aren't in e10s Firefox
    sDXVAEnabled = !mozilla::BrowserTabsRemoteAutostart();
  }

  // We have heavy logging below to help diagnose issue around hardware
  // decoding failures. Due to these failures often relating to driver level
  // problems they're hard to nail down, so we want lots of info. We may be
  // able to relax this in future if we're not seeing such problems (see bug
  // 1673007 for references to the bugs motivating this).
  bool hwVideo = gfx::gfxVars::GetCanUseHardwareVideoDecodingOrDefault();
  WmfDecoderModuleMarkerAndLog(
      "WMFInit DXVA Status",
      "sDXVAEnabled: %s, CanUseHardwareVideoDecoding: %s",
      sDXVAEnabled ? "true" : "false", hwVideo ? "true" : "false");
  sDXVAEnabled = sDXVAEnabled && hwVideo;

  mozilla::mscom::EnsureMTA([&]() {
    // Store the supported MFT decoders.
    sSupportedTypes.clear();
    sLackOfExtensionTypes.clear();
    // i = 1 to skip Unknown.
    for (uint32_t i = 1; i < static_cast<uint32_t>(WMFStreamType::SENTINEL);
         i++) {
      WMFStreamType type = static_cast<WMFStreamType>(i);
      RefPtr<MFTDecoder> decoder = new MFTDecoder();
      HRESULT hr = CreateMFTDecoder(type, decoder);
      if (SUCCEEDED(hr)) {
        sSupportedTypes += type;
        WmfDecoderModuleMarkerAndLog("WMFInit Decoder Supported",
                                     "%s is enabled", StreamTypeToString(type));
      } else if (hr != E_FAIL) {
        // E_FAIL should be logged by CreateMFTDecoder. Skipping those codes
        // will help to keep the logs readable.
        WmfDecoderModuleMarkerAndLog("WMFInit Decoder Failed",
                                     "%s failed with code 0x%lx",
                                     StreamTypeToString(type), hr);
        if (hr == WINCODEC_ERR_COMPONENTNOTFOUND &&
            type == WMFStreamType::AV1) {
          WmfDecoderModuleMarkerAndLog("No AV1 extension",
                                       "Lacking of AV1 extension");
          sLackOfExtensionTypes += type;
        }
      }
    }
  });

  sSupportedTypesInitialized = true;

  WmfDecoderModuleMarkerAndLog("WMFInit Result",
                               "WMFDecoderModule::Init finishing");
}

/* static */
int WMFDecoderModule::GetNumDecoderThreads() {
  int32_t numCores = PR_GetNumberOfProcessors();

  // If we have more than 4 cores, let the decoder decide how many threads.
  // On an 8 core machine, WMF chooses 4 decoder threads.
  static const int WMF_DECODER_DEFAULT = -1;
  if (numCores > 4) {
    return WMF_DECODER_DEFAULT;
  }
  return std::max(numCores - 1, 1);
}

/* static */
HRESULT WMFDecoderModule::CreateMFTDecoder(const WMFStreamType& aType,
                                           RefPtr<MFTDecoder>& aDecoder) {
  // Do not expose any video decoder on utility process which is only for audio
  // decoding.
  if (XRE_IsUtilityProcess()) {
    switch (aType) {
      case WMFStreamType::H264:
      case WMFStreamType::VP8:
      case WMFStreamType::VP9:
      case WMFStreamType::AV1:
      case WMFStreamType::HEVC:
        return E_FAIL;
      default:
        break;
    }
  }

  switch (aType) {
    case WMFStreamType::H264:
      return aDecoder->Create(CLSID_CMSH264DecoderMFT);
    case WMFStreamType::VP8:
      static const uint32_t VP8_USABLE_BUILD = 16287;
      if (!IsWindows10BuildOrLater(VP8_USABLE_BUILD)) {
        WmfDecoderModuleMarkerAndLog("CreateMFTDecoder, VP8 Failure",
                                     "VP8 MFT requires Windows build %" PRId32
                                     " or later",
                                     VP8_USABLE_BUILD);
        return E_FAIL;
      }
      if (!gfx::gfxVars::UseVP8HwDecode()) {
        WmfDecoderModuleMarkerAndLog("CreateMFTDecoder, VP8 Failure",
                                     "Gfx VP8 blocklist");
        return E_FAIL;
      }
      [[fallthrough]];
    case WMFStreamType::VP9:
      if (!sDXVAEnabled) {
        WmfDecoderModuleMarkerAndLog("CreateMFTDecoder, VPx Disabled",
                                     "%s MFT requires DXVA",
                                     StreamTypeToString(aType));
        return E_FAIL;
      }

      {
        gfx::WMFVPXVideoCrashGuard guard;
        if (guard.Crashed()) {
          WmfDecoderModuleMarkerAndLog(
              "CreateMFTDecoder, VPx Failure",
              "Will not use VPx MFT due to crash guard reporting a crash");
          return E_FAIL;
        }
        return aDecoder->Create(CLSID_CMSVPXDecMFT);
      }
#ifdef MOZ_AV1
    case WMFStreamType::AV1:
      // If this process cannot use DXVA, the AV1 decoder will not be used.
      // Also, upon startup, init will be called both before and after
      // layers acceleration is setup. This prevents creating the AV1 decoder
      // twice.
      if (!sDXVAEnabled) {
        WmfDecoderModuleMarkerAndLog("CreateMFTDecoder AV1 Disabled",
                                     "AV1 MFT requires DXVA");
        return E_FAIL;
      }
      // TODO: MFTEnumEx is slower than creating by CLSID, it may be worth
      // investigating other ways to instantiate the AV1 decoder.
      return aDecoder->Create(MFT_CATEGORY_VIDEO_DECODER, MFVideoFormat_AV1,
                              MFVideoFormat_NV12);
#endif
    case WMFStreamType::HEVC:
      if (!WMFDecoderModule::IsHEVCSupported() || !sDXVAEnabled) {
        return E_FAIL;
      }
      return aDecoder->Create(MFT_CATEGORY_VIDEO_DECODER, MFVideoFormat_HEVC,
                              MFVideoFormat_NV12);
    case WMFStreamType::MP3:
      return aDecoder->Create(CLSID_CMP3DecMediaObject);
    case WMFStreamType::AAC:
      return aDecoder->Create(CLSID_CMSAACDecMFT);
    default:
      return E_FAIL;
  }
}

/* static */
bool WMFDecoderModule::CanCreateMFTDecoder(const WMFStreamType& aType) {
  MOZ_ASSERT(WMFStreamType::Unknown < aType && aType < WMFStreamType::SENTINEL);
  if (!sSupportedTypesInitialized) {
    if (NS_IsMainThread()) {
      Init();
    } else {
      nsCOMPtr<nsIRunnable> runnable =
          NS_NewRunnableFunction("WMFDecoderModule::Init", [&]() { Init(); });
      SyncRunnable::DispatchToThread(GetMainThreadSerialEventTarget(),
                                     runnable);
    }
  }

  // Check prefs here rather than CreateMFTDecoder so that prefs aren't baked
  // into sSupportedTypes
  switch (aType) {
    case WMFStreamType::VP8:
    case WMFStreamType::VP9:
      if (!StaticPrefs::media_wmf_vp9_enabled()) {
        return false;
      }
      break;
#ifdef MOZ_AV1
    case WMFStreamType::AV1:
      if (!StaticPrefs::media_av1_enabled() ||
          !StaticPrefs::media_wmf_av1_enabled()) {
        return false;
      }
      break;
#endif
    case WMFStreamType::HEVC:
      if (!WMFDecoderModule::IsHEVCSupported()) {
        return false;
      }
      break;
    // Always use ffvpx for mp3
    case WMFStreamType::MP3:
      return false;
    default:
      break;
  }

  // Do not expose any video decoder on utility process which is only for audio
  // decoding.
  if (XRE_IsUtilityProcess()) {
    switch (aType) {
      case WMFStreamType::H264:
      case WMFStreamType::VP8:
      case WMFStreamType::VP9:
      case WMFStreamType::AV1:
      case WMFStreamType::HEVC:
        return false;
      default:
        break;
    }
  }

  return sSupportedTypes.contains(aType);
}

bool WMFDecoderModule::SupportsColorDepth(
    gfx::ColorDepth aColorDepth, DecoderDoctorDiagnostics* aDiagnostics) const {
  // Color depth support can be determined by creating DX decoders.
  return true;
}

media::DecodeSupportSet WMFDecoderModule::Supports(
    const SupportDecoderParams& aParams,
    DecoderDoctorDiagnostics* aDiagnostics) const {
  // This should only be supported by MFMediaEngineDecoderModule.
  if (aParams.mMediaEngineId) {
    return media::DecodeSupportSet{};
  }
  // In GPU process, only support decoding if video. This only gives a hint of
  // what the GPU decoder *may* support. The actual check will occur in
  // CreateVideoDecoder.
  const auto& trackInfo = aParams.mConfig;
  if (XRE_IsGPUProcess() && !trackInfo.GetAsVideoInfo()) {
    return media::DecodeSupportSet{};
  }

  const auto* videoInfo = trackInfo.GetAsVideoInfo();
  // Temporary - forces use of VPXDecoder when alpha is present.
  // Bug 1263836 will handle alpha scenario once implemented. It will shift
  // the check for alpha to PDMFactory but not itself remove the need for a
  // check.
  if (videoInfo && (!SupportsColorDepth(videoInfo->mColorDepth, aDiagnostics) ||
                    videoInfo->HasAlpha())) {
    return media::DecodeSupportSet{};
  }

  if (videoInfo && VPXDecoder::IsVP9(aParams.MimeType()) &&
      aParams.mOptions.contains(CreateDecoderParams::Option::LowLatency)) {
    // SVC layers are unsupported, and may be used in low latency use cases
    // (WebRTC).
    return media::DecodeSupportSet{};
  }

  WMFStreamType type = GetStreamTypeFromMimeType(aParams.MimeType());
  if (type == WMFStreamType::Unknown) {
    return media::DecodeSupportSet{};
  }

  if (CanCreateMFTDecoder(type)) {
    if (StreamTypeIsVideo(type)) {
      return sDXVAEnabled ? media::DecodeSupport::HardwareDecode
                          : media::DecodeSupport::SoftwareDecode;
    } else {
      // Audio only supports software decode
      return media::DecodeSupport::SoftwareDecode;
    }
  }
  return sLackOfExtensionTypes.contains(type)
             ? media::DecodeSupport::UnsureDueToLackOfExtension
             : media::DecodeSupportSet{};
}

nsresult WMFDecoderModule::Startup() {
  return wmf::MediaFoundationInitializer::HasInitialized() ? NS_OK
                                                           : NS_ERROR_FAILURE;
}

already_AddRefed<MediaDataDecoder> WMFDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  // In GPU process, only support decoding if an accelerated compositor is
  // known.
  if (XRE_IsGPUProcess() &&
      !IsRemoteAcceleratedCompositor(aParams.mKnowsCompositor)) {
    return nullptr;
  }

  UniquePtr<WMFVideoMFTManager> manager(new WMFVideoMFTManager(
      aParams.VideoConfig(), aParams.mKnowsCompositor, aParams.mImageContainer,
      aParams.mRate.mValue, aParams.mOptions, sDXVAEnabled,
      aParams.mTrackingId));

  MediaResult result = manager->Init();
  if (NS_FAILED(result)) {
    if (aParams.mError) {
      *aParams.mError = result;
    }
    WmfDecoderModuleMarkerAndLog(
        "WMFVDecoderCreation Failure",
        "WMFDecoderModule::CreateVideoDecoder failed for manager with "
        "description %s with result: %s",
        manager->GetDescriptionName().get(), result.Description().get());
    return nullptr;
  }

  nsAutoCString hwFailure;
  if (!manager->IsHardwareAccelerated(hwFailure)) {
    // The decoder description includes whether it is using software or
    // hardware, but no information about how the hardware acceleration failed.
    WmfDecoderModuleMarkerAndLog(
        "WMFVDecoderCreation Success",
        "WMFDecoderModule::CreateVideoDecoder success for manager with "
        "description %s - DXVA failure: %s",
        manager->GetDescriptionName().get(), hwFailure.get());
  } else {
    WmfDecoderModuleMarkerAndLog(
        "WMFVDecoderCreation Success",
        "WMFDecoderModule::CreateVideoDecoder success for manager with "
        "description %s",
        manager->GetDescriptionName().get());
  }

  RefPtr<MediaDataDecoder> decoder = new WMFMediaDataDecoder(manager.release());
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder> WMFDecoderModule::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  if (XRE_IsGPUProcess()) {
    // Only allow video in the GPU process.
    return nullptr;
  }

  UniquePtr<WMFAudioMFTManager> manager(
      new WMFAudioMFTManager(aParams.AudioConfig()));

  if (!manager->Init()) {
    WmfDecoderModuleMarkerAndLog(
        "WMFADecoderCreation Failure",
        "WMFDecoderModule::CreateAudioDecoder failed for manager with "
        "description %s",
        manager->GetDescriptionName().get());
    return nullptr;
  }

  WmfDecoderModuleMarkerAndLog(
      "WMFADecoderCreation Success",
      "WMFDecoderModule::CreateAudioDecoder success for manager with "
      "description %s",
      manager->GetDescriptionName().get());

  RefPtr<MediaDataDecoder> decoder = new WMFMediaDataDecoder(manager.release());
  return decoder.forget();
}

media::DecodeSupportSet WMFDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  UniquePtr<TrackInfo> trackInfo = CreateTrackInfoWithMIMEType(aMimeType);
  if (!trackInfo) {
    return media::DecodeSupportSet{};
  }
  auto supports = Supports(SupportDecoderParams(*trackInfo), aDiagnostics);
  MOZ_LOG(
      sPDMLog, LogLevel::Debug,
      ("WMF decoder %s requested type '%s'",
       !supports.isEmpty() ? "supports" : "rejects", aMimeType.BeginReading()));
  return supports;
}

/* static */
bool WMFDecoderModule::IsHEVCSupported() {
  return sForceEnableHEVC || StaticPrefs::media_wmf_hevc_enabled() == 1;
}

}  // namespace mozilla

#undef WFM_DECODER_MODULE_STATUS_MARKER
#undef LOG
