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
#include "WMF.h"
#include "WMFAudioMFTManager.h"
#include "WMFMediaDataDecoder.h"
#include "WMFVideoMFTManager.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_media.h"
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

#define LOG(...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

extern const GUID CLSID_WebmMfVpxDec;

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

static Atomic<bool> sDXVAEnabled(false);
static Atomic<bool> sUsableVPXMFT(false);

/* static */
already_AddRefed<PlatformDecoderModule> WMFDecoderModule::Create() {
  RefPtr<WMFDecoderModule> wmf = new WMFDecoderModule();
  return wmf.forget();
}

WMFDecoderModule::~WMFDecoderModule() {
  if (mWMFInitialized) {
    DebugOnly<HRESULT> hr = wmf::MFShutdown();
    NS_ASSERTION(SUCCEEDED(hr), "MFShutdown failed");
  }
}

static bool IsRemoteAcceleratedCompositor(
    layers::KnowsCompositor* aKnowsCompositor) {
  if (!aKnowsCompositor) {
    return false;
  }

  if (aKnowsCompositor->UsingSoftwareWebRenderD3D11()) {
    return true;
  }

  TextureFactoryIdentifier ident =
      aKnowsCompositor->GetTextureFactoryIdentifier();
  return ident.mParentBackend != LayersBackend::LAYERS_BASIC &&
         !aKnowsCompositor->UsingSoftwareWebRender() &&
         ident.mParentProcessType == GeckoProcessType_GPU;
}

static bool CanCreateMFTDecoder(const GUID& aGuid) {
  // The IMFTransform interface used by MFTDecoder is documented to require to
  // run on an MTA thread.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ee892371(v=vs.85).aspx#components
  // Note: our normal SharedThreadPool task queues are initialized to MTA, but
  // the main thread (which calls in here from our CanPlayType implementation)
  // is not.
  bool canCreateDecoder = false;
  mozilla::mscom::EnsureMTA([&]() -> void {
    if (FAILED(wmf::MFStartup())) {
      return;
    }
    RefPtr<MFTDecoder> decoder(new MFTDecoder());
    canCreateDecoder = SUCCEEDED(decoder->Create(aGuid));
    wmf::MFShutdown();
  });
  return canCreateDecoder;
}

/* static */
void WMFDecoderModule::Init() {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  if (XRE_IsContentProcess()) {
    // If we're in the content process and the UseGPUDecoder pref is set, it
    // means that we've given up on the GPU process (it's been crashing) so we
    // should disable DXVA
    sDXVAEnabled = !StaticPrefs::media_gpu_process_decoder();
  } else if (XRE_IsGPUProcess()) {
    // Always allow DXVA in the GPU process.
    sDXVAEnabled = true;
  } else if (XRE_IsRDDProcess()) {
    // Only allows DXVA if we have an image device. We may have explicitly
    // disabled its creation following an earlier RDD process crash.
    sDXVAEnabled = !!DeviceManagerDx::Get()->GetImageDevice();
  } else {
    // Only allow DXVA in the UI process if we aren't in e10s Firefox
    sDXVAEnabled = !mozilla::BrowserTabsRemoteAutostart();
  }

  // We have heavy logging below to help diagnose issue around hardware decoding
  // failures. Due to these failures often relating to driver level problems
  // they're hard to nail down, so we want lots of info. We may be able to relax
  // this in future if we're not seeing such problems (see bug 1673007 for
  // references to the bugs motivating this).
  sDXVAEnabled = sDXVAEnabled && gfx::gfxVars::CanUseHardwareVideoDecoding();
  bool testForVPx = gfx::gfxVars::CanUseHardwareVideoDecoding();
  if (testForVPx && StaticPrefs::media_wmf_vp9_enabled_AtStartup()) {
    gfx::WMFVPXVideoCrashGuard guard;
    if (!guard.Crashed()) {
      WmfDecoderModuleMarkerAndLog("WMFInit VPx Pending",
                                   "Attempting to create MFT decoder for VPx");

      sUsableVPXMFT = CanCreateMFTDecoder(CLSID_WebmMfVpxDec);

      WmfDecoderModuleMarkerAndLog("WMFInit VPx Initialized",
                                   "CanCreateMFTDecoder returned %s for VPx",
                                   sUsableVPXMFT ? "true" : "false");
    } else {
      WmfDecoderModuleMarkerAndLog(
          "WMFInit VPx Failure",
          "Will not use MFT VPx due to crash guard reporting a crash");
    }
  }

  WmfDecoderModuleMarkerAndLog(
      "WMFInit Result",
      "WMFDecoderModule::Init finishing with sDXVAEnabled=%s testForVPx=%s "
      "sUsableVPXMFT=%s",
      sDXVAEnabled ? "true" : "false", testForVPx ? "true" : "false",
      sUsableVPXMFT ? "true" : "false");
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

nsresult WMFDecoderModule::Startup() {
  mWMFInitialized = SUCCEEDED(wmf::MFStartup());
  return mWMFInitialized ? NS_OK : NS_ERROR_FAILURE;
}

already_AddRefed<MediaDataDecoder> WMFDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  ReportUsageForTelemetry();

  // In GPU process, only support decoding if an accelerated compositor is
  // known.
  if (XRE_IsGPUProcess() &&
      !IsRemoteAcceleratedCompositor(aParams.mKnowsCompositor)) {
    return nullptr;
  }

  UniquePtr<WMFVideoMFTManager> manager(new WMFVideoMFTManager(
      aParams.VideoConfig(), aParams.mKnowsCompositor, aParams.mImageContainer,
      aParams.mRate.mValue, aParams.mOptions, sDXVAEnabled));

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

  WmfDecoderModuleMarkerAndLog(
      "WMFVDecoderCreation Success",
      "WMFDecoderModule::CreateVideoDecoder success for manager with "
      "description %s",
      manager->GetDescriptionName().get());

  RefPtr<MediaDataDecoder> decoder = new WMFMediaDataDecoder(manager.release());
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder> WMFDecoderModule::CreateAudioDecoder(
    const CreateDecoderParams& aParams) {
  ReportUsageForTelemetry();

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

template <const GUID& aGuid>
static bool CanCreateWMFDecoder() {
  static StaticMutex sMutex;
  StaticMutexAutoLock lock(sMutex);
  static Maybe<bool> result;
  if (result.isNothing()) {
    result.emplace(CanCreateMFTDecoder(aGuid));
  }
  return result.value();
}

/* static */
bool WMFDecoderModule::HasH264() {
  return CanCreateWMFDecoder<CLSID_CMSH264DecoderMFT>();
}

/* static */
bool WMFDecoderModule::HasVP8() {
  return sUsableVPXMFT && CanCreateWMFDecoder<CLSID_WebmMfVpxDec>();
}

/* static */
bool WMFDecoderModule::HasVP9() {
  return sUsableVPXMFT && CanCreateWMFDecoder<CLSID_WebmMfVpxDec>();
}

/* static */
bool WMFDecoderModule::HasAAC() {
  return CanCreateWMFDecoder<CLSID_CMSAACDecMFT>();
}

/* static */
bool WMFDecoderModule::HasMP3() {
  return CanCreateWMFDecoder<CLSID_CMP3DecMediaObject>();
}

bool WMFDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  UniquePtr<TrackInfo> trackInfo = CreateTrackInfoWithMIMEType(aMimeType);
  if (!trackInfo) {
    return false;
  }
  return Supports(SupportDecoderParams(*trackInfo), aDiagnostics);
}

bool WMFDecoderModule::Supports(const SupportDecoderParams& aParams,
                                DecoderDoctorDiagnostics* aDiagnostics) const {
  ReportUsageForTelemetry();

  // In GPU process, only support decoding if video. This only gives a hint of
  // what the GPU decoder *may* support. The actual check will occur in
  // CreateVideoDecoder.
  const auto& trackInfo = aParams.mConfig;
  if (XRE_IsGPUProcess() && !trackInfo.GetAsVideoInfo()) {
    return false;
  }

  const auto* videoInfo = trackInfo.GetAsVideoInfo();
  // Temporary - forces use of VPXDecoder when alpha is present.
  // Bug 1263836 will handle alpha scenario once implemented. It will shift
  // the check for alpha to PDMFactory but not itself remove the need for a
  // check.
  if (videoInfo && (!SupportsColorDepth(videoInfo->mColorDepth, aDiagnostics) ||
                    videoInfo->HasAlpha())) {
    return false;
  }

  if ((trackInfo.mMimeType.EqualsLiteral("audio/mp4a-latm") ||
       trackInfo.mMimeType.EqualsLiteral("audio/mp4")) &&
      WMFDecoderModule::HasAAC()) {
    return true;
  }
  if (MP4Decoder::IsH264(trackInfo.mMimeType) && WMFDecoderModule::HasH264()) {
    return true;
  }
  if (trackInfo.mMimeType.EqualsLiteral("audio/mpeg") &&
      !StaticPrefs::media_ffvpx_mp3_enabled() && WMFDecoderModule::HasMP3()) {
    return true;
  }
  static const uint32_t VP8_USABLE_BUILD = 16287;
  if (VPXDecoder::IsVP8(trackInfo.mMimeType) &&
      IsWindowsBuildOrLater(VP8_USABLE_BUILD) && WMFDecoderModule::HasVP8()) {
    return true;
  }
  if (VPXDecoder::IsVP9(trackInfo.mMimeType) && WMFDecoderModule::HasVP9()) {
    return true;
  }

  // Some unsupported codec.
  return false;
}

void WMFDecoderModule::ReportUsageForTelemetry() const {
  if (XRE_IsParentProcess() || XRE_IsContentProcess()) {
    Telemetry::ScalarSet(Telemetry::ScalarID::MEDIA_WMF_PROCESS_USAGE, true);
  }
}

}  // namespace mozilla

#undef WFM_DECODER_MODULE_STATUS_MARKER
#undef LOG
