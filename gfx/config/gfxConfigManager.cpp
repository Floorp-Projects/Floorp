/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/gfx/gfxConfigManager.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layers.h"
#include "gfxConfig.h"
#include "gfxPlatform.h"
#include "nsIGfxInfo.h"
#include "nsXULAppAPI.h"
#include "WebRenderRollout.h"

#ifdef XP_WIN
#  include "mozilla/WindowsVersion.h"
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "mozilla/gfx/DisplayConfigWindows.h"
#endif

namespace mozilla {
namespace gfx {

void gfxConfigManager::Init() {
  MOZ_ASSERT(XRE_IsParentProcess());

  WebRenderRollout::Init();
  mWrQualifiedOverride = WebRenderRollout::CalculateQualifiedOverride();
  mWrQualified = WebRenderRollout::CalculateQualified();

  EmplaceUserPref("gfx.webrender.compositor", mWrCompositorEnabled);
  mWrForceEnabled = gfxPlatform::WebRenderPrefEnabled();
  mWrForceDisabled = StaticPrefs::gfx_webrender_force_disabled_AtStartup();
  mWrCompositorForceEnabled =
      StaticPrefs::gfx_webrender_compositor_force_enabled_AtStartup();
  mGPUProcessAllowSoftware =
      StaticPrefs::layers_gpu_process_allow_software_AtStartup();
  mWrPictureCaching = StaticPrefs::gfx_webrender_picture_caching();
  mWrPartialPresent =
      StaticPrefs::gfx_webrender_max_partial_present_rects_AtStartup() > 0;
#ifdef XP_WIN
  mWrForceAngle = StaticPrefs::gfx_webrender_force_angle_AtStartup();
#  ifdef NIGHTLY_BUILD
  mWrForceAngleNoGPUProcess = StaticPrefs::
      gfx_webrender_enabled_no_gpu_process_with_angle_win_AtStartup();
#  endif
  mWrDCompWinEnabled =
      Preferences::GetBool("gfx.webrender.dcomp-win.enabled", false);
#endif

  mWrEnvForceEnabled = gfxPlatform::WebRenderEnvvarEnabled();
  mWrEnvForceDisabled = gfxPlatform::WebRenderEnvvarDisabled();

#ifdef XP_WIN
  mHwStretchingSupport =
      DeviceManagerDx::Get()->CheckHardwareStretchingSupport();
  mScaledResolution = HasScaledResolution();
  mIsWin10OrLater = IsWin10OrLater();
#else
  mHwStretchingSupport = true;
#endif

#ifdef MOZ_WIDGET_GTK
  mDisableHwCompositingNoWr = true;
#endif

#ifdef NIGHTLY_BUILD
  mIsNightly = true;
#endif
  mSafeMode = gfxPlatform::InSafeMode();

  mGfxInfo = services::GetGfxInfo();

  mFeatureWr = &gfxConfig::GetFeature(Feature::WEBRENDER);
  mFeatureWrQualified = &gfxConfig::GetFeature(Feature::WEBRENDER_QUALIFIED);
  mFeatureWrCompositor = &gfxConfig::GetFeature(Feature::WEBRENDER_COMPOSITOR);
  mFeatureWrAngle = &gfxConfig::GetFeature(Feature::WEBRENDER_ANGLE);
  mFeatureWrDComp = &gfxConfig::GetFeature(Feature::WEBRENDER_DCOMP_PRESENT);
  mFeatureWrPartial = &gfxConfig::GetFeature(Feature::WEBRENDER_PARTIAL);

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
                           NS_LITERAL_CSTRING("FEATURE_FAILURE_NO_GFX_INFO"));

  } else {
    if (status != nsIGfxInfo::FEATURE_STATUS_OK) {
      aFeatureState->Disable(FeatureStatus::Blacklisted,
                             "Blacklisted by gfxInfo", blockId);
    }
  }
}

bool gfxConfigManager::ConfigureWebRenderQualified() {
  MOZ_ASSERT(mFeatureWrQualified);
  MOZ_ASSERT(mFeatureWrCompositor);

  bool guarded = true;
  mFeatureWrQualified->EnableByDefault();

  if (mWrQualifiedOverride) {
    if (!*mWrQualifiedOverride) {
      mFeatureWrQualified->Disable(
          FeatureStatus::BlockedOverride, "HW qualification pref override",
          NS_LITERAL_CSTRING("FEATURE_FAILURE_WR_QUALIFICATION_OVERRIDE"));
    }
    return guarded;
  }

  nsCString failureId;
  int32_t status;
  if (NS_FAILED(mGfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_WEBRENDER,
                                           failureId, &status))) {
    mFeatureWrQualified->Disable(
        FeatureStatus::BlockedNoGfxInfo, "gfxInfo is broken",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_WR_NO_GFX_INFO"));
    return guarded;
  }

  switch (status) {
    case nsIGfxInfo::FEATURE_ALLOW_ALWAYS:
      // We want to honour ALLOW_ALWAYS on beta and release, but on nightly,
      // we still want to perform experiments. A larger population is the most
      // useful, demote nightly to merely qualified.
      guarded = mIsNightly;
      break;
    case nsIGfxInfo::FEATURE_ALLOW_QUALIFIED:
      break;
    case nsIGfxInfo::FEATURE_DENIED:
      mFeatureWrQualified->Disable(FeatureStatus::Denied, "Not on allowlist",
                                   failureId);
      break;
    default:
      mFeatureWrQualified->Disable(FeatureStatus::Blacklisted,
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
    if (adapterVendorID == u"0x8086") {
      bool hasBattery = false;
      mGfxInfo->GetHasBattery(&hasBattery);
      if (hasBattery && !mFeatureWrCompositor->IsEnabled()) {
        mFeatureWrQualified->Disable(
            FeatureStatus::Blocked, "Battery Intel requires os compositor",
            NS_LITERAL_CSTRING("INTEL_BATTERY_REQUIRES_DCOMP"));
      }
    }
  }

  return guarded;
}

void gfxConfigManager::ConfigureWebRender() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(mFeatureWr);
  MOZ_ASSERT(mFeatureWrQualified);
  MOZ_ASSERT(mFeatureWrCompositor);
  MOZ_ASSERT(mFeatureWrAngle);
  MOZ_ASSERT(mFeatureWrDComp);
  MOZ_ASSERT(mFeatureWrPartial);
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
  if (!mHwStretchingSupport && mScaledResolution) {
    mFeatureWrCompositor->Disable(
        FeatureStatus::Unavailable, "No hardware stretching support",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_NO_HARDWARE_STRETCHING"));
  }

  bool guardedByQualifiedPref = ConfigureWebRenderQualified();

  mFeatureWr->DisableByDefault(
      FeatureStatus::OptIn, "WebRender is an opt-in feature",
      NS_LITERAL_CSTRING("FEATURE_FAILURE_DEFAULT_OFF"));

  // envvar works everywhere; note that we need this for testing in CI.
  // Prior to bug 1523788, the `prefEnabled` check was only done on Nightly,
  // so as to prevent random users from easily enabling WebRender on
  // unqualified hardware in beta/release.
  if (mWrEnvForceEnabled) {
    mFeatureWr->UserEnable("Force enabled by envvar");
  } else if (mWrForceEnabled) {
    mFeatureWr->UserEnable("Force enabled by pref");
  } else if (mFeatureWrQualified->IsEnabled()) {
    // If the HW is qualified, we enable if either the HW has been qualified
    // on the release channel (i.e. it's no longer guarded by the qualified
    // pref), or if the qualified pref is enabled.
    if (!guardedByQualifiedPref) {
      mFeatureWr->UserEnable("Qualified in release");
    } else if (mWrQualified) {
      mFeatureWr->UserEnable("Qualified enabled by pref");
    }
  }

  // If the user set the pref to force-disable, let's do that. This will
  // override all the other enabling prefs (gfx.webrender.enabled,
  // gfx.webrender.all, and gfx.webrender.all.qualified).
  if (mWrForceDisabled ||
      (mWrEnvForceDisabled && mWrQualifiedOverride.isNothing())) {
    mFeatureWr->UserDisable(
        "User force-disabled WR",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_USER_FORCE_DISABLED"));
  }

  // HW_COMPOSITING being disabled implies interfacing with the GPU might break
  if (!mFeatureHwCompositing->IsEnabled()) {
    mFeatureWr->ForceDisable(
        FeatureStatus::UnavailableNoHwCompositing,
        "Hardware compositing is disabled",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBRENDER_NEED_HWCOMP"));
  }

  if (mSafeMode) {
    mFeatureWr->ForceDisable(FeatureStatus::UnavailableInSafeMode,
                             "Safe-mode is enabled",
                             NS_LITERAL_CSTRING("FEATURE_FAILURE_SAFE_MODE"));
  }

  mFeatureWrAngle->DisableByDefault(
      FeatureStatus::OptIn, "WebRender ANGLE is an opt-in feature",
      NS_LITERAL_CSTRING("FEATURE_FAILURE_DEFAULT_OFF"));

  if (mFeatureD3D11HwAngle && mWrForceAngle) {
    if (!mFeatureD3D11HwAngle->IsEnabled()) {
      mFeatureWr->ForceDisable(
          FeatureStatus::UnavailableNoAngle, "ANGLE is disabled",
          NS_LITERAL_CSTRING("FEATURE_FAILURE_ANGLE_DISABLED"));
    } else if (!mFeatureGPUProcess->IsEnabled() &&
               (!mIsNightly || !mWrForceAngleNoGPUProcess)) {
      // WebRender with ANGLE relies on the GPU process when on Windows
      mFeatureWr->ForceDisable(
          FeatureStatus::UnavailableNoGpuProcess, "GPU Process is disabled",
          NS_LITERAL_CSTRING("FEATURE_FAILURE_GPU_PROCESS_DISABLED"));
    } else if (mFeatureWr->IsEnabled()) {
      mFeatureWrAngle->UserEnable("Enabled");
    }
  }

  if (!mFeatureWr->IsEnabled() && mDisableHwCompositingNoWr) {
    if (mFeatureHwCompositing->IsEnabled()) {
      // Hardware compositing should be disabled by default if we aren't using
      // WebRender. We had to check if it is enabled at all, because it may
      // already have been forced disabled (e.g. safe mode, headless). It may
      // still be forced on by the user, and if so, this should have no effect.
      mFeatureHwCompositing->Disable(FeatureStatus::Blocked,
                                     "Acceleration blocked by platform",
                                     EmptyCString());
    }

    if (!mFeatureHwCompositing->IsEnabled() &&
        mFeatureGPUProcess->IsEnabled() && !mGPUProcessAllowSoftware) {
      // We have neither WebRender nor OpenGL, we don't allow the GPU process
      // for basic compositor, and it wasn't disabled already.
      mFeatureGPUProcess->Disable(FeatureStatus::Unavailable,
                                  "Hardware compositing is unavailable.",
                                  EmptyCString());
    }
  }

  mFeatureWrDComp->DisableByDefault(
      FeatureStatus::OptIn, "WebRender DirectComposition is an opt-in feature",
      NS_LITERAL_CSTRING("FEATURE_FAILURE_DEFAULT_OFF"));

  if (mWrDCompWinEnabled) {
    // XXX relax win version to windows 8.
    if (mIsWin10OrLater && mFeatureWr->IsEnabled() &&
        mFeatureWrAngle->IsEnabled()) {
      mFeatureWrDComp->UserEnable("Enabled");
    }
  }

  if (!mWrPictureCaching) {
    mFeatureWrCompositor->ForceDisable(
        FeatureStatus::Unavailable, "Picture caching is disabled",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_PICTURE_CACHING_DISABLED"));
  }

  if (!mFeatureWrDComp->IsEnabled()) {
    mFeatureWrCompositor->ForceDisable(
        FeatureStatus::Unavailable, "No DirectComposition usage",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_NO_DIRECTCOMPOSITION"));
  }

  // Initialize WebRender partial present config.
  // Partial present is used only when WebRender compositor is not used.
  if (mWrPartialPresent) {
    if (mFeatureWr->IsEnabled()) {
      mFeatureWrPartial->EnableByDefault();
      if (mWrPictureCaching) {
        // Call UserEnable() only for reporting to Decision Log.
        // If feature is enabled by default. It is not reported to Decision Log.
        mFeatureWrPartial->UserEnable("Enabled");
      } else {
        mFeatureWrPartial->ForceDisable(
            FeatureStatus::Unavailable, "Picture caching is disabled",
            NS_LITERAL_CSTRING("FEATURE_FAILURE_PICTURE_CACHING_DISABLED"));
      }
    }
  }
}

}  // namespace gfx
}  // namespace mozilla
