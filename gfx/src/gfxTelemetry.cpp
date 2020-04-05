/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gfxTelemetry.h"

#include "mozilla/Assertions.h"

namespace mozilla {
namespace gfx {

const char* FeatureStatusToString(FeatureStatus aStatus) {
  switch (aStatus) {
    case FeatureStatus::Unused:
      return "unused";
    case FeatureStatus::Unavailable:
      return "unavailable";
    case FeatureStatus::UnavailableInSafeMode:
      return "unavailable-in-safe-mode";
    case FeatureStatus::UnavailableNoGpuProcess:
      return "unavailable-no-gpu-process";
    case FeatureStatus::UnavailableNoHwCompositing:
      return "unavailable-no-hw-compositing";
    case FeatureStatus::UnavailableNotBuilt:
      return "unavailable-not-built";
    case FeatureStatus::UnavailableNoAngle:
      return "unavailable-no-angle";
    case FeatureStatus::CrashedInHandler:
      return "crashed";
    case FeatureStatus::Blocked:
      return "blocked";
    case FeatureStatus::BlockedDeviceUnknown:
      return "blocked-device-unknown";
    case FeatureStatus::BlockedDeviceTooOld:
      return "blocked-device-too-old";
    case FeatureStatus::BlockedVendorUnsupported:
      return "blocked-vendor-unsupported";
    case FeatureStatus::BlockedHasBattery:
      return "blocked-has-battery";
    case FeatureStatus::BlockedScreenTooLarge:
      return "blocked-screen-too-large";
    case FeatureStatus::BlockedScreenUnknown:
      return "blocked-screen-unknown";
    case FeatureStatus::BlockedNoGfxInfo:
      return "blocked-no-gfx-info";
    case FeatureStatus::BlockedOverride:
      return "blocked-override";
    case FeatureStatus::BlockedReleaseChannelIntel:
      return "blocked-release-channel-intel";
    case FeatureStatus::BlockedReleaseChannelAMD:
      return "blocked-release-channel-amd";
    case FeatureStatus::BlockedReleaseChannelNvidia:
      return "blocked-release-channel-nvidia";
    case FeatureStatus::BlockedReleaseChannelBattery:
      return "blocked-release-channel-battery";
    case FeatureStatus::BlockedReleaseChannelAndroid:
      return "blocked-release-channel-android";
    case FeatureStatus::Denied:
      return "denied";
    case FeatureStatus::Blacklisted:
      return "blacklisted";
    case FeatureStatus::OptIn:
      return "opt-in";
    case FeatureStatus::Failed:
      return "failed";
    case FeatureStatus::Disabled:
      return "disabled";
    case FeatureStatus::Available:
      return "available";
    case FeatureStatus::ForceEnabled:
      return "force_enabled";
    case FeatureStatus::CrashedOnStartup:
      return "crashed_on_startup";
    case FeatureStatus::Broken:
      return "broken";
    default:
      MOZ_ASSERT_UNREACHABLE("missing status case");
      return "unknown";
  }
}

bool IsFeatureStatusFailure(FeatureStatus aStatus) {
  return !(aStatus == FeatureStatus::Unused ||
           aStatus == FeatureStatus::Available ||
           aStatus == FeatureStatus::ForceEnabled);
}

bool IsFeatureStatusSuccess(FeatureStatus aStatus) {
  return aStatus == FeatureStatus::Available ||
         aStatus == FeatureStatus::ForceEnabled;
}

}  // namespace gfx
}  // namespace mozilla
