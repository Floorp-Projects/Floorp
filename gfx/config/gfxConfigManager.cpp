/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/gfx/gfxConfigManager.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/Preferences.h"
#include "mozilla/Components.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layers.h"
#include "gfxConfig.h"
#include "gfxPlatform.h"
#include "nsIGfxInfo.h"
#include "nsIXULRuntime.h"  // for FissionAutostart
#include "nsPrintfCString.h"
#include "nsXULAppAPI.h"

#ifdef XP_WIN
#  include "mozilla/WindowsVersion.h"
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "mozilla/gfx/DisplayConfigWindows.h"
#endif

namespace mozilla {
namespace gfx {

void gfxConfigManager::Init() {
  MOZ_ASSERT(XRE_IsParentProcess());

  EmplaceUserPref("gfx.webrender.compositor", mWrCompositorEnabled);
  mWrForceEnabled = gfxPlatform::WebRenderPrefEnabled();
  mWrForceDisabled = StaticPrefs::gfx_webrender_force_disabled_AtStartup();
  mWrSoftwareForceEnabled = StaticPrefs::gfx_webrender_software_AtStartup();
  mWrCompositorForceEnabled =
      StaticPrefs::gfx_webrender_compositor_force_enabled_AtStartup();
  mGPUProcessAllowSoftware =
      StaticPrefs::layers_gpu_process_allow_software_AtStartup();
  mWrPartialPresent =
      StaticPrefs::gfx_webrender_max_partial_present_rects_AtStartup() > 0;
  mWrOptimizedShaders =
      StaticPrefs::gfx_webrender_use_optimized_shaders_AtStartup();
#ifdef XP_WIN
  mWrForceAngle = StaticPrefs::gfx_webrender_force_angle_AtStartup();
  mWrForceAngleNoGPUProcess = StaticPrefs::
      gfx_webrender_enabled_no_gpu_process_with_angle_win_AtStartup();
  mWrDCompWinEnabled =
      Preferences::GetBool("gfx.webrender.dcomp-win.enabled", false);
#endif

  mWrEnvForceEnabled = gfxPlatform::WebRenderEnvvarEnabled();
  mWrEnvForceDisabled = gfxPlatform::WebRenderEnvvarDisabled();

#ifdef XP_WIN
  DeviceManagerDx::Get()->CheckHardwareStretchingSupport(mHwStretchingSupport);
  mScaledResolution = HasScaledResolution();
  mIsWin10OrLater = IsWin10OrLater();
  mWrCompositorDCompRequired = true;
#else
  ++mHwStretchingSupport.mBoth;
#endif

#ifdef MOZ_WIDGET_GTK
  mDisableHwCompositingNoWr = true;
  mXRenderEnabled = mozilla::Preferences::GetBool("gfx.xrender.enabled");
#endif

#ifdef NIGHTLY_BUILD
  mIsNightly = true;
#endif
  mSafeMode = gfxPlatform::InSafeMode();

  mGfxInfo = components::GfxInfo::Service();

  mFeatureWr = &gfxConfig::GetFeature(Feature::WEBRENDER);
  mFeatureWrQualified = &gfxConfig::GetFeature(Feature::WEBRENDER_QUALIFIED);
  mFeatureWrCompositor = &gfxConfig::GetFeature(Feature::WEBRENDER_COMPOSITOR);
  mFeatureWrAngle = &gfxConfig::GetFeature(Feature::WEBRENDER_ANGLE);
  mFeatureWrDComp = &gfxConfig::GetFeature(Feature::WEBRENDER_DCOMP_PRESENT);
  mFeatureWrPartial = &gfxConfig::GetFeature(Feature::WEBRENDER_PARTIAL);
  mFeatureWrOptimizedShaders =
      &gfxConfig::GetFeature(Feature::WEBRENDER_OPTIMIZED_SHADERS);
  mFeatureWrSoftware = &gfxConfig::GetFeature(Feature::WEBRENDER_SOFTWARE);

  mFeatureHwCompositing = &gfxConfig::GetFeature(Feature::HW_COMPOSITING);
#ifdef XP_WIN
  mFeatureD3D11HwAngle = &gfxConfig::GetFeature(Feature::D3D11_HW_ANGLE);
#endif
  mFeatureGPUProcess = &gfxConfig::GetFeature(Feature::GPU_PROCESS);
}

void gfxConfigManager::EmplaceUserPref(const char* aPrefName,
                                       Maybe<bool>& aValue) {
  if (Preferences::HasUserValue(aPrefName)) {
    aValue.emplace(Preferences::GetBool(aPrefName, false));
  }
}

void gfxConfigManager::ConfigureFromBlocklist(long aFeature,
                                              FeatureState* aFeatureState) {
  MOZ_ASSERT(aFeatureState);

  nsCString blockId;
  int32_t status;
  if (!NS_SUCCEEDED(mGfxInfo->GetFeatureStatus(aFeature, blockId, &status))) {
    aFeatureState->Disable(FeatureStatus::BlockedNoGfxInfo, "gfxInfo is broken",
                           "FEATURE_FAILURE_NO_GFX_INFO"_ns);

  } else {
    if (status != nsIGfxInfo::FEATURE_STATUS_OK) {
      aFeatureState->Disable(FeatureStatus::Blocklisted,
                             "Blocklisted by gfxInfo", blockId);
    }
  }
}

void gfxConfigManager::ConfigureWebRenderSoftware() {
  MOZ_ASSERT(mFeatureWrSoftware);

  mFeatureWrSoftware->EnableByDefault();

  // Note that for testing in CI, software WebRender uses gfx.webrender.software
  // to force enable WebRender Software. As a result, we need to prefer that
  // over the MOZ_WEBRENDER envvar which is used to otherwise force on WebRender
  // (hardware). See bug 1656811.
  if (mWrSoftwareForceEnabled) {
    mFeatureWrSoftware->UserForceEnable("Force enabled by pref");
  } else if (FissionAutostart()) {
    mFeatureWrSoftware->UserForceEnable("Force enabled by fission");
  } else if (mWrForceDisabled || mWrEnvForceDisabled) {
    // If the user set the pref to force-disable, let's do that. This
    // will override all the other enabling prefs
    mFeatureWrSoftware->UserDisable("User force-disabled WR",
                                    "FEATURE_FAILURE_USER_FORCE_DISABLED"_ns);
  }

  nsCString failureId;
  int32_t status;
  if (NS_FAILED(mGfxInfo->GetFeatureStatus(
          nsIGfxInfo::FEATURE_WEBRENDER_SOFTWARE, failureId, &status))) {
    mFeatureWrSoftware->Disable(FeatureStatus::BlockedNoGfxInfo,
                                "gfxInfo is broken",
                                "FEATURE_FAILURE_WR_NO_GFX_INFO"_ns);
    return;
  }

  switch (status) {
    case nsIGfxInfo::FEATURE_ALLOW_ALWAYS:
    case nsIGfxInfo::FEATURE_ALLOW_QUALIFIED:
      break;
    case nsIGfxInfo::FEATURE_DENIED:
      mFeatureWrSoftware->Disable(FeatureStatus::Denied, "Not on allowlist",
                                  failureId);
      break;
    default:
      mFeatureWrSoftware->Disable(FeatureStatus::Blocklisted,
                                  "No qualified hardware", failureId);
      break;
    case nsIGfxInfo::FEATURE_STATUS_OK:
      MOZ_ASSERT_UNREACHABLE("We should still be rolling out WebRender!");
      mFeatureWrSoftware->Disable(FeatureStatus::Blocked,
                                  "Not controlled by rollout", failureId);
      break;
  }
}

void gfxConfigManager::ConfigureWebRenderQualified() {
  MOZ_ASSERT(mFeatureWrQualified);
  MOZ_ASSERT(mFeatureWrCompositor);

  mFeatureWrQualified->EnableByDefault();

  nsCString failureId;
  int32_t status;
  if (NS_FAILED(mGfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_WEBRENDER,
                                           failureId, &status))) {
    mFeatureWrQualified->Disable(FeatureStatus::BlockedNoGfxInfo,
                                 "gfxInfo is broken",
                                 "FEATURE_FAILURE_WR_NO_GFX_INFO"_ns);
    return;
  }

  switch (status) {
    case nsIGfxInfo::FEATURE_ALLOW_ALWAYS:
    case nsIGfxInfo::FEATURE_ALLOW_QUALIFIED:
      break;
    case nsIGfxInfo::FEATURE_DENIED:
      mFeatureWrQualified->Disable(FeatureStatus::Denied, "Not on allowlist",
                                   failureId);
      break;
    default:
      mFeatureWrQualified->Disable(FeatureStatus::Blocklisted,
                                   "No qualified hardware", failureId);
      break;
    case nsIGfxInfo::FEATURE_STATUS_OK:
      MOZ_ASSERT_UNREACHABLE("We should still be rolling out WebRender!");
      mFeatureWrQualified->Disable(FeatureStatus::Blocked,
                                   "Not controlled by rollout", failureId);
      break;
  }

  if (!mIsNightly) {
    // Disable WebRender if we don't have DirectComposition
    nsAutoString adapterVendorID;
    mGfxInfo->GetAdapterVendorID(adapterVendorID);
    if (adapterVendorID == u"0x10de") {
      bool mixed = false;
      int32_t maxRefreshRate = mGfxInfo->GetMaxRefreshRate(&mixed);
      if (maxRefreshRate > 60 && mixed) {
        mFeatureWrQualified->Disable(FeatureStatus::Blocked,
                                     "Monitor refresh rate too high/mixed",
                                     "NVIDIA_REFRESH_RATE_MIXED"_ns);
      }
    }
  }
}

void gfxConfigManager::ConfigureWebRender() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(mFeatureWr);
  MOZ_ASSERT(mFeatureWrQualified);
  MOZ_ASSERT(mFeatureWrCompositor);
  MOZ_ASSERT(mFeatureWrAngle);
  MOZ_ASSERT(mFeatureWrDComp);
  MOZ_ASSERT(mFeatureWrPartial);
  MOZ_ASSERT(mFeatureWrSoftware);
  MOZ_ASSERT(mFeatureHwCompositing);
  MOZ_ASSERT(mFeatureGPUProcess);

  // Initialize WebRender native compositor usage
  mFeatureWrCompositor->SetDefaultFromPref("gfx.webrender.compositor", true,
                                           false, mWrCompositorEnabled);

  if (mWrCompositorForceEnabled) {
    mFeatureWrCompositor->UserForceEnable("Force enabled by pref");
  }

  ConfigureFromBlocklist(nsIGfxInfo::FEATURE_WEBRENDER_COMPOSITOR,
                         mFeatureWrCompositor);

  // Disable native compositor when hardware stretching is not supported. It is
  // for avoiding a problem like Bug 1618370.
  // XXX Is there a better check for Bug 1618370?
  if (!mHwStretchingSupport.IsFullySupported() && mScaledResolution) {
    nsPrintfCString failureId(
        "FEATURE_FAILURE_NO_HARDWARE_STRETCHING_B%uW%uF%uN%uE%u",
        mHwStretchingSupport.mBoth, mHwStretchingSupport.mWindowOnly,
        mHwStretchingSupport.mFullScreenOnly, mHwStretchingSupport.mNone,
        mHwStretchingSupport.mError);
    mFeatureWrCompositor->Disable(FeatureStatus::Unavailable,
                                  "No hardware stretching support", failureId);
  }

  ConfigureWebRenderSoftware();
  ConfigureWebRenderQualified();

  mFeatureWr->EnableByDefault();

  // envvar works everywhere; note that we need this for testing in CI.
  // Prior to bug 1523788, the `prefEnabled` check was only done on Nightly,
  // so as to prevent random users from easily enabling WebRender on
  // unqualified hardware in beta/release.
  if (mWrSoftwareForceEnabled) {
    MOZ_ASSERT(mFeatureWrSoftware->IsEnabled());
    mFeatureWr->UserDisable("User force-enabled software WR",
                            "FEATURE_FAILURE_USER_FORCE_ENABLED_SW_WR"_ns);
  } else if (mWrEnvForceEnabled) {
    mFeatureWr->UserForceEnable("Force enabled by envvar");
  } else if (mWrForceEnabled) {
    mFeatureWr->UserForceEnable("Force enabled by pref");
  } else if (mWrForceDisabled || mWrEnvForceDisabled) {
    // If the user set the pref to force-disable, let's do that. This
    // will override all the other enabling prefs
    // (gfx.webrender.enabled, gfx.webrender.all, and
    // gfx.webrender.all.qualified).
    mFeatureWr->UserDisable("User force-disabled WR",
                            "FEATURE_FAILURE_USER_FORCE_DISABLED"_ns);
  }

  if (!mFeatureWrQualified->IsEnabled()) {
    // No qualified hardware. If we haven't allowed software fallback,
    // then we need to disable WR.
    mFeatureWr->Disable(FeatureStatus::Disabled, "Not qualified",
                        "FEATURE_FAILURE_NOT_QUALIFIED"_ns);
  }

  // HW_COMPOSITING being disabled implies interfacing with the GPU might break
  if (!mFeatureHwCompositing->IsEnabled()) {
    mFeatureWr->ForceDisable(FeatureStatus::UnavailableNoHwCompositing,
                             "Hardware compositing is disabled",
                             "FEATURE_FAILURE_WEBRENDER_NEED_HWCOMP"_ns);
  }

  if (mSafeMode) {
    mFeatureWr->ForceDisable(FeatureStatus::UnavailableInSafeMode,
                             "Safe-mode is enabled",
                             "FEATURE_FAILURE_SAFE_MODE"_ns);
    mFeatureWrSoftware->ForceDisable(FeatureStatus::UnavailableInSafeMode,
                                     "Safe-mode is enabled",
                                     "FEATURE_FAILURE_SAFE_MODE"_ns);
  }

  if (mXRenderEnabled) {
    // XRender and WebRender don't play well together. XRender is disabled by
    // default. If the user opts into it don't enable webrender.
    mFeatureWr->ForceDisable(FeatureStatus::Blocked, "XRender is enabled",
                             "FEATURE_FAILURE_XRENDER"_ns);
    mFeatureWrSoftware->ForceDisable(FeatureStatus::Blocked,
                                     "XRender is enabled",
                                     "FEATURE_FAILURE_XRENDER"_ns);
  }

  mFeatureWrAngle->EnableByDefault();
  if (mFeatureD3D11HwAngle) {
    if (mWrForceAngle) {
      if (!mFeatureD3D11HwAngle->IsEnabled()) {
        mFeatureWrAngle->ForceDisable(FeatureStatus::UnavailableNoAngle,
                                      "ANGLE is disabled",
                                      mFeatureD3D11HwAngle->GetFailureId());
      } else if (!mFeatureGPUProcess->IsEnabled() &&
                 !mWrForceAngleNoGPUProcess) {
        // WebRender with ANGLE relies on the GPU process when on Windows
        mFeatureWrAngle->ForceDisable(
            FeatureStatus::UnavailableNoGpuProcess, "GPU Process is disabled",
            "FEATURE_FAILURE_GPU_PROCESS_DISABLED"_ns);
      } else if (!mFeatureWr->IsEnabled() && !mFeatureWrSoftware->IsEnabled()) {
        mFeatureWrAngle->ForceDisable(FeatureStatus::Unavailable,
                                      "WebRender disabled",
                                      "FEATURE_FAILURE_WR_DISABLED"_ns);
      }
    } else {
      mFeatureWrAngle->Disable(FeatureStatus::Disabled, "ANGLE is not forced",
                               "FEATURE_FAILURE_ANGLE_NOT_FORCED"_ns);
    }
  } else {
    mFeatureWrAngle->Disable(FeatureStatus::Unavailable, "OS not supported",
                             "FEATURE_FAILURE_OS_NOT_SUPPORTED"_ns);
  }

  if (mWrForceAngle && mFeatureWr->IsEnabled() &&
      !mFeatureWrAngle->IsEnabled()) {
    // Ensure we disable WebRender if ANGLE is unavailable and it is required.
    mFeatureWr->ForceDisable(FeatureStatus::UnavailableNoAngle,
                             "ANGLE is disabled",
                             mFeatureWrAngle->GetFailureId());
  }

  if (!mFeatureWr->IsEnabled() && mDisableHwCompositingNoWr) {
    if (mFeatureHwCompositing->IsEnabled()) {
      // Hardware compositing should be disabled by default if we aren't using
      // WebRender. We had to check if it is enabled at all, because it may
      // already have been forced disabled (e.g. safe mode, headless). It may
      // still be forced on by the user, and if so, this should have no effect.
      mFeatureHwCompositing->Disable(FeatureStatus::Blocked,
                                     "Acceleration blocked by platform", ""_ns);
    }

    if (!mFeatureHwCompositing->IsEnabled() &&
        mFeatureGPUProcess->IsEnabled() && !mGPUProcessAllowSoftware) {
      // We have neither WebRender nor OpenGL, we don't allow the GPU process
      // for basic compositor, and it wasn't disabled already.
      mFeatureGPUProcess->Disable(FeatureStatus::Unavailable,
                                  "Hardware compositing is unavailable.",
                                  ""_ns);
    }
  }

  mFeatureWrDComp->EnableByDefault();
  if (!mWrDCompWinEnabled) {
    mFeatureWrDComp->UserDisable("User disabled via pref",
                                 "FEATURE_FAILURE_DCOMP_PREF_DISABLED"_ns);
  }

  if (!mIsWin10OrLater) {
    // XXX relax win version to windows 8.
    mFeatureWrDComp->Disable(FeatureStatus::Unavailable,
                             "Requires Windows 10 or later",
                             "FEATURE_FAILURE_DCOMP_NOT_WIN10"_ns);
  }

  mFeatureWrDComp->MaybeSetFailed(
      mFeatureWr->IsEnabled(), FeatureStatus::Unavailable, "Requires WebRender",
      "FEATURE_FAILURE_DCOMP_NOT_WR"_ns);
  mFeatureWrDComp->MaybeSetFailed(mFeatureWrAngle->IsEnabled(),
                                  FeatureStatus::Unavailable, "Requires ANGLE",
                                  "FEATURE_FAILURE_DCOMP_NOT_ANGLE"_ns);

  if (!mFeatureWrDComp->IsEnabled() && mWrCompositorDCompRequired) {
    mFeatureWrCompositor->ForceDisable(FeatureStatus::Unavailable,
                                       "No DirectComposition usage",
                                       mFeatureWrDComp->GetFailureId());
  }

  // Initialize WebRender partial present config.
  // Partial present is used only when WebRender compositor is not used.
  if (mWrPartialPresent) {
    if (mFeatureWr->IsEnabled() || mFeatureWrSoftware->IsEnabled()) {
      mFeatureWrPartial->EnableByDefault();

      nsString adapter;
      mGfxInfo->GetAdapterDeviceID(adapter);
      // Block partial present on some devices due to rendering issues.
      // On Mali-T6xx and T7xx GPUs due to bug 1680087.
      // On Adreno 3xx GPUs due to bug 1695771.
      if (adapter.Find("Mali-T6", /*ignoreCase*/ true) >= 0 ||
          adapter.Find("Mali-T7", /*ignoreCase*/ true) >= 0 ||
          adapter.Find("Adreno (TM) 3", /*ignoreCase*/ true) >= 0) {
        mFeatureWrPartial->Disable(
            FeatureStatus::Blocked, "Partial present blocked",
            "FEATURE_FAILURE_PARTIAL_PRESENT_BLOCKED"_ns);
      }
    }
  }

  mFeatureWrOptimizedShaders->EnableByDefault();
  if (!mWrOptimizedShaders) {
    mFeatureWrOptimizedShaders->UserDisable("User disabled via pref",
                                            "FEATURE_FAILURE_PREF_DISABLED"_ns);
  }
  ConfigureFromBlocklist(nsIGfxInfo::FEATURE_WEBRENDER_OPTIMIZED_SHADERS,
                         mFeatureWrOptimizedShaders);
  if (!mFeatureWr->IsEnabled()) {
    mFeatureWrOptimizedShaders->ForceDisable(FeatureStatus::Unavailable,
                                             "WebRender disabled",
                                             "FEATURE_FAILURE_WR_DISABLED"_ns);
  }
}

}  // namespace gfx
}  // namespace mozilla
