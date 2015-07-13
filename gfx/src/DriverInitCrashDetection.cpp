/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DriverInitCrashDetection.h"
#include "gfxPrefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsXULAppAPI.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla {
namespace gfx {

bool DriverInitCrashDetection::sDisableAcceleration = false;
bool DriverInitCrashDetection::sEnvironmentHasBeenUpdated = false;

DriverInitCrashDetection::DriverInitCrashDetection()
 : mIsChromeProcess(XRE_GetProcessType() == GeckoProcessType_Default)
{
  if (sDisableAcceleration) {
    // We already disabled acceleration earlier.
    return;
  }

  if (!mIsChromeProcess) {
    // In child processes we only need to check the pref state set by the
    // parent process.
    sDisableAcceleration = (gfxPrefs::DriverInitStatus() == int32_t(DriverInitStatus::Recovered));
    return;
  }

  if (!InitLockFilePath()) {
    gfxCriticalError(CriticalLog::DefaultOptions(false)) << "Failed to create the graphics startup lockfile.";
    return;
  }

  if (RecoverFromDriverInitCrash()) {
    // This is the first time we're checking for a crash recovery, so print
    // a message and disable acceleration for anyone who asks for it.
    gfxCriticalError(CriticalLog::DefaultOptions(false)) << "Recovered from graphics driver startup crash; acceleration disabled.";
    sDisableAcceleration = true;
    return;
  }

  if (UpdateEnvironment() || sEnvironmentHasBeenUpdated) {
    // Something in the environment changed, *or* a previous instance of this
    // class already updated the environment. Allow a fresh attempt at driver
    // acceleration. This doesn't mean the previous attempt failed, it just
    // means we want to detect whether the new environment crashes.
    AllowDriverInitAttempt();
    sEnvironmentHasBeenUpdated = true;
    return;
  }

  RecordTelemetry(TelemetryState::Okay);
}

DriverInitCrashDetection::~DriverInitCrashDetection()
{
  if (mLockFile) {
    mLockFile->Remove(false);
  }

  if (gfxPrefs::DriverInitStatus() == int32_t(DriverInitStatus::Attempting)) {
    // If we attempted to initialize the driver, and got this far without
    // crashing, assume everything is okay.
    gfxPrefs::SetDriverInitStatus(int32_t(DriverInitStatus::Okay));

#ifdef MOZ_CRASHREPORTER
    // Remove the crash report annotation.
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("GraphicsStartupTest"),
                                       NS_LITERAL_CSTRING(""));
#endif
  }
}

bool
DriverInitCrashDetection::InitLockFilePath()
{
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR, getter_AddRefs(mLockFile));
  if (!mLockFile) {
    return false;
  }
  if (!NS_SUCCEEDED(mLockFile->AppendNative(NS_LITERAL_CSTRING("gfxinit.lock")))) {
    return false;
  }
  return true;
}

void
DriverInitCrashDetection::AllowDriverInitAttempt()
{
  // Create a temporary tombstone/lockfile.
  FILE* fp;
  if (!NS_SUCCEEDED(mLockFile->OpenANSIFileDesc("w", &fp))) {
    return;
  }
  fclose(fp);

  gfxPrefs::SetDriverInitStatus(int32_t(DriverInitStatus::Attempting));

  // Flush preferences, so if we crash, we don't think the environment has changed again.
  FlushPreferences();

  // If we crash, we'll just lose this. Not a big deal, next startup we'll
  // record the failure.
  RecordTelemetry(TelemetryState::EnvironmentChanged);

#ifdef MOZ_CRASHREPORTER
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("GraphicsStartupTest"),
                                     NS_LITERAL_CSTRING("1"));
#endif
}

bool
DriverInitCrashDetection::RecoverFromDriverInitCrash()
{
  bool exists;
  if (mLockFile &&
      NS_SUCCEEDED(mLockFile->Exists(&exists)) &&
      exists)
  {
    // If we get here, we've just recovered from a crash. Disable acceleration
    // until the environment changes. Since we may have crashed before
    // preferences we're flushed, we cache the environment again, then flush
    // preferences so child processes can start right away.
    gfxPrefs::SetDriverInitStatus(int32_t(DriverInitStatus::Recovered));
    UpdateEnvironment();
    FlushPreferences();
    RecordTelemetry(TelemetryState::RecoveredFromCrash);
    return true;
  }
  if (gfxPrefs::DriverInitStatus() == int32_t(DriverInitStatus::Recovered)) {
    // If we get here, we crashed in the current environment and have already
    // disabled acceleration.
    RecordTelemetry(TelemetryState::AccelerationDisabled);
    return true;
  }
  return false;
}

bool
DriverInitCrashDetection::UpdateEnvironment()
{
  mGfxInfo = do_GetService("@mozilla.org/gfx/info;1");

  bool changed = false;
  if (mGfxInfo) {
    nsString value;

    // Driver properties.
    mGfxInfo->GetAdapterDriverVersion(value);
    changed |= CheckAndUpdatePref("gfx.driver-init.driverVersion", value);
    mGfxInfo->GetAdapterDeviceID(value);
    changed |= CheckAndUpdatePref("gfx.driver-init.deviceID", value);

    // Feature status.
#if defined(XP_WIN)
    bool d2dEnabled = gfxPrefs::Direct2DForceEnabled() ||
                      (!gfxPrefs::Direct2DDisabled() && FeatureEnabled(nsIGfxInfo::FEATURE_DIRECT2D));
    changed |= CheckAndUpdateBoolPref("gfx.driver-init.feature-d2d", d2dEnabled);

    bool d3d11Enabled = !gfxPrefs::LayersPreferD3D9();
    if (!FeatureEnabled(nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS)) {
      d3d11Enabled = false;
    }
    changed |= CheckAndUpdateBoolPref("gfx.driver-init.feature-d3d11", d3d11Enabled);
#endif
  }

  // Firefox properties.
  changed |= CheckAndUpdatePref("gfx.driver-init.appVersion", NS_LITERAL_STRING(MOZ_APP_VERSION));

  // Finally, mark as changed if the status has been reset by the user.
  changed |= (gfxPrefs::DriverInitStatus() == int32_t(DriverInitStatus::None));

  mGfxInfo = nullptr;
  return changed;
}

bool
DriverInitCrashDetection::FeatureEnabled(int aFeature)
{
  int32_t status;
  if (!NS_SUCCEEDED(mGfxInfo->GetFeatureStatus(aFeature, &status))) {
    return false;
  }
  return status == nsIGfxInfo::FEATURE_STATUS_OK;
}

bool
DriverInitCrashDetection::CheckAndUpdateBoolPref(const char* aPrefName, bool aCurrentValue)
{
  bool oldValue;
  if (NS_SUCCEEDED(Preferences::GetBool(aPrefName, &oldValue)) &&
      oldValue == aCurrentValue)
  {
    return false;
  }
  Preferences::SetBool(aPrefName, aCurrentValue);
  return true;
}

bool
DriverInitCrashDetection::CheckAndUpdatePref(const char* aPrefName, const nsAString& aCurrentValue)
{
  nsAdoptingString oldValue = Preferences::GetString(aPrefName);
  if (oldValue == aCurrentValue) {
    return false;
  }
  Preferences::SetString(aPrefName, aCurrentValue);
  return true;
}

void
DriverInitCrashDetection::FlushPreferences()
{
  if (nsIPrefService* prefService = Preferences::GetService()) {
    prefService->SavePrefFile(nullptr);
  }
}

void
DriverInitCrashDetection::RecordTelemetry(TelemetryState aState)
{
  // Since we run this in each child process, we only want the initial results
  // from the chrome process.
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return;
  }

  // Since we instantiate this class more than once, make sure we only record
  // the first state (since that is really all we care about).
  static bool sTelemetryStateRecorded = false;
  if (sTelemetryStateRecorded) {
    return;
  }

  Telemetry::Accumulate(Telemetry::GRAPHICS_DRIVER_STARTUP_TEST, int32_t(aState));
  sTelemetryStateRecorded = true;
}

} // namespace gfx
} // namespace mozilla
