/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef gfx_src_gfxTelemetry_h__
#define gfx_src_gfxTelemetry_h__

namespace mozilla {
namespace gfx {

// Describes the status of a graphics feature, in terms of whether or not we've
// attempted to initialize the feature, and if so, whether or not it succeeded
// (and if not, why).
enum class FeatureStatus
{
  // This feature has not been requested.
  Unused,

  // This feature is unavailable due to Safe Mode, not being included with
  // the operating system, or a dependent feature being disabled.
  Unavailable,

  // This feature crashed immediately when we tried to initialize it, but we
  // were able to recover via SEH (or something similar).
  CrashedInHandler,

  // This feature was blocked for reasons outside the blacklist, such as a
  // runtime test failing.
  Blocked,

  // This feature has been blocked by the graphics blacklist.
  Blacklisted,

  // This feature was attempted but failed to activate.
  Failed,

  // This feature was explicitly disabled by the user.
  Disabled,

  // This feature is available for use.
  Available,

  // This feature was explicitly force-enabled by the user.
  ForceEnabled,

  // This feature was disabled due to the startup crash guard.
  CrashedOnStartup,

  // This feature was attempted but later determined to be broken.
  Broken,

  // Add new entries above here.
  LAST
};

const char* FeatureStatusToString(FeatureStatus aStatus);
bool IsFeatureStatusFailure(FeatureStatus aStatus);
bool IsFeatureStatusSuccess(FeatureStatus aStatus);

enum class TelemetryDeviceCode : uint32_t {
  Content = 0,
  Image = 1,
  D2D1 = 2
};

} // namespace gfx
} // namespace mozilla

#endif // gfx_src_gfxTelemetry_h__
