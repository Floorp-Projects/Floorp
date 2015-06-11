/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef gfx_src_DriverInitCrashDetection_h__
#define gfx_src_DriverInitCrashDetection_h__

#include "gfxCore.h"
#include "nsCOMPtr.h"
#include "nsIGfxInfo.h"
#include "nsIFile.h"

namespace mozilla {
namespace gfx {

enum class DriverInitStatus
{
  // Drivers have not been initialized yet.
  None,

  // We're attempting to initialize drivers.
  Attempting,

  // Drivers were successfully initialized last run.
  Okay,

  // We crashed during driver initialization, and have restarted.
  Recovered
};

class DriverInitCrashDetection
{
public:
  DriverInitCrashDetection();
  ~DriverInitCrashDetection();

  bool DisableAcceleration() const {
    return sDisableAcceleration;
  }

  // NOTE: These are the values reported to Telemetry (GRAPHICS_DRIVER_STARTUP_TEST).
  // Values should not change; add new values to the end.
  //
  // We report exactly one of these values on every startup. The only exception
  // is when we crash during driver initialization; the relevant value will be
  // reported on the next startup. The value can change from startup to startup.
  //
  // The initial value for any profile is "EnvironmentChanged"; this means we
  // have not seen the environment before. The environment includes graphics
  // driver versions, adpater IDs, the MOZ_APP version, and blacklisting
  // information.
  //
  // If the user crashes, the next startup will have the "RecoveredFromCrash"
  // state. Graphics acceleration will be disabled. Subsequent startups will
  // have the "DriverUseDisabled" state.
  //
  // If the user does not crash, subsequent startups will have the "Okay"
  // state.
  //
  // If the environment changes, the state (no matter what it is) will
  // transition back to "EnvironmentChanged".
  enum class TelemetryState {
    // Environment is the same as before, and we detected no crash at that time.
    //
    // Valid state transitions: Okay -> EnvironmentChanged.
    Okay = 0,

    // Environment has changed since the last time we checked drivers. If we
    // detected a crash before, we re-enable acceleration to see if it will
    // work in the new environment.
    //
    // Valid state transitions: EnvironmentChanged -> RecoveredFromCrash | Okay
    EnvironmentChanged = 1,

    // The last startup crashed trying to enable graphics acceleration, and
    // acceleration has just been disabled.
    //
    // Valid state transitions: RecoveredFromCrash -> DriverUseDisabled | EnvironmentChanged
    RecoveredFromCrash = 2,

    // A previous session was in the RecoveredFromCrash state, and now graphics
    // acceleration is disabled until the environment changes.
    //
    // Valid state transitions: DriverUseDisabled -> EnvironmentChanged
    DriverUseDisabled = 3
  };

private:
  bool InitLockFilePath();
  bool UpdateEnvironment();
  bool CheckAndUpdatePref(const char* aPrefName, const nsAString& aCurrentValue);
  bool CheckAndUpdateBoolPref(const char* aPrefName, bool aCurrentValue);
  bool FeatureEnabled(int aFeature);
  void AllowDriverInitAttempt();
  bool RecoverFromDriverInitCrash();
  void FlushPreferences();

  void RecordTelemetry(TelemetryState aState);

private:
  static bool sDisableAcceleration;
  static bool sEnvironmentHasBeenUpdated;

private:
  bool mIsChromeProcess;
  nsCOMPtr<nsIGfxInfo> mGfxInfo;
  nsCOMPtr<nsIFile> mLockFile;
};

} // namespace gfx
} // namespace mozilla

#endif // gfx_src_DriverInitCrashDetection_h__

