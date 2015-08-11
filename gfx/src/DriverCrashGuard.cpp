/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DriverCrashGuard.h"
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
#include "mozilla/Services.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla {
namespace gfx {

static const size_t NUM_CRASH_GUARD_TYPES = size_t(CrashGuardType::NUM_TYPES);
static const char* sCrashGuardNames[NUM_CRASH_GUARD_TYPES] = {
  "d3d11layers",
};

DriverCrashGuard::DriverCrashGuard(CrashGuardType aType)
 : mType(aType)
 , mInitialized(false)
{
  MOZ_ASSERT(mType < CrashGuardType::NUM_TYPES);

  mStatusPref.Assign("gfx.driver-init.status.");
  mStatusPref.Append(sCrashGuardNames[size_t(mType)]);

  mGuardFilename.Assign(sCrashGuardNames[size_t(mType)]);
  mGuardFilename.Append(".guard");
}

void
DriverCrashGuard::InitializeIfNeeded()
{
  if (mInitialized) {
    return;
  }

  mInitialized = true;
  Initialize();
}

void
DriverCrashGuard::Initialize()
{
  if (!InitLockFilePath()) {
    gfxCriticalError(CriticalLog::DefaultOptions(false)) << "Failed to create the graphics startup lockfile.";
    return;
  }

  if (RecoverFromDriverInitCrash()) {
    return;
  }

  if (PrepareToGuard()) {
    // Something in the environment changed, *or* a previous instance of this
    // class already updated the environment in this session. Enable the
    // guard.
    AllowDriverInitAttempt();
  }
}

DriverCrashGuard::~DriverCrashGuard()
{
  if (mGuardFile) {
    mGuardFile->Remove(false);
  }

  if (GetStatus() == DriverInitStatus::Attempting) {
    // If we attempted to initialize the driver, and got this far without
    // crashing, assume everything is okay.
    SetStatus(DriverInitStatus::Okay);

#ifdef MOZ_CRASHREPORTER
    // Remove the crash report annotation.
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("GraphicsStartupTest"),
                                       NS_LITERAL_CSTRING(""));
#endif
  }
}

bool
DriverCrashGuard::Crashed()
{
  InitializeIfNeeded();
  return gfxPrefs::DriverInitStatus() == int32_t(DriverInitStatus::Recovered);
}

bool
DriverCrashGuard::InitLockFilePath()
{
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR, getter_AddRefs(mGuardFile));
  if (!mGuardFile) {
    return false;
  }

  if (!NS_SUCCEEDED(mGuardFile->AppendNative(mGuardFilename))) {
    return false;
  }
  return true;
}

void
DriverCrashGuard::AllowDriverInitAttempt()
{
  // Create a temporary tombstone/lockfile.
  FILE* fp;
  if (!NS_SUCCEEDED(mGuardFile->OpenANSIFileDesc("w", &fp))) {
    return;
  }
  fclose(fp);

  SetStatus(DriverInitStatus::Attempting);

  // Flush preferences, so if we crash, we don't think the environment has changed again.
  FlushPreferences();

#ifdef MOZ_CRASHREPORTER
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("GraphicsStartupTest"),
                                     NS_LITERAL_CSTRING("1"));
#endif
}

bool
DriverCrashGuard::RecoverFromDriverInitCrash()
{
  bool exists;
  if (mGuardFile &&
      NS_SUCCEEDED(mGuardFile->Exists(&exists)) &&
      exists)
  {
    // If we get here, we've just recovered from a crash. Disable acceleration
    // until the environment changes. Since we may have crashed before
    // preferences we're flushed, we cache the environment again, then flush
    // preferences so child processes can start right away.
    SetStatus(DriverInitStatus::Recovered);
    UpdateBaseEnvironment();
    UpdateEnvironment();
    FlushPreferences();
    LogCrashRecovery();
    return true;
  }
  if (GetStatus() == DriverInitStatus::Recovered) {
    // If we get here, we crashed in a previous session.
    LogFeatureDisabled();
    return true;
  }
  return false;
}

// Return true if the caller should proceed to guard for crashes. False if
// the environment has not changed.
bool
DriverCrashGuard::PrepareToGuard()
{
  static bool sBaseInfoChanged = false;
  static bool sBaseInfoChecked = false;

  if (!sBaseInfoChecked) {
    sBaseInfoChecked = true;
    sBaseInfoChanged = UpdateBaseEnvironment();
  }

  // Always update the full environment, even if the base info didn't change.
  return UpdateEnvironment() ||
         sBaseInfoChanged ||
         gfxPrefs::DriverInitStatus() == int32_t(DriverInitStatus::None);
}

bool
DriverCrashGuard::UpdateBaseEnvironment()
{
  bool changed = false;
  if (mGfxInfo = services::GetGfxInfo()) {
    nsString value;

    // Driver properties.
    mGfxInfo->GetAdapterDriverVersion(value);
    changed |= CheckAndUpdatePref("gfx.driver-init.driverVersion", value);
    mGfxInfo->GetAdapterDeviceID(value);
    changed |= CheckAndUpdatePref("gfx.driver-init.deviceID", value);
  }

  // Firefox properties.
  changed |= CheckAndUpdatePref("gfx.driver-init.appVersion", NS_LITERAL_STRING(MOZ_APP_VERSION));

  // Finally, mark as changed if the status has been reset by the user.
  changed |= (GetStatus() == DriverInitStatus::None);

  return changed;
}

bool
DriverCrashGuard::FeatureEnabled(int aFeature)
{
  int32_t status;
  if (!NS_SUCCEEDED(mGfxInfo->GetFeatureStatus(aFeature, &status))) {
    return false;
  }
  return status == nsIGfxInfo::FEATURE_STATUS_OK;
}

bool
DriverCrashGuard::CheckAndUpdateBoolPref(const char* aPrefName, bool aCurrentValue)
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
DriverCrashGuard::CheckAndUpdatePref(const char* aPrefName, const nsAString& aCurrentValue)
{
  nsAdoptingString oldValue = Preferences::GetString(aPrefName);
  if (oldValue == aCurrentValue) {
    return false;
  }
  Preferences::SetString(aPrefName, aCurrentValue);
  return true;
}

DriverInitStatus
DriverCrashGuard::GetStatus() const
{
  return (DriverInitStatus)Preferences::GetInt(mStatusPref.get(), 0);
}

void
DriverCrashGuard::SetStatus(DriverInitStatus aStatus)
{
  Preferences::SetInt(mStatusPref.get(), int32_t(aStatus));
}

void
DriverCrashGuard::FlushPreferences()
{
  if (nsIPrefService* prefService = Preferences::GetService()) {
    prefService->SavePrefFile(nullptr);
  }
}

D3D11LayersCrashGuard::D3D11LayersCrashGuard()
 : DriverCrashGuard(CrashGuardType::D3D11Layers)
{
}

void
D3D11LayersCrashGuard::Initialize()
{
  if (!XRE_IsParentProcess()) {
    // We assume the parent process already performed crash detection for
    // graphics devices.
    return;
  }

  DriverCrashGuard::Initialize();

  // If no telemetry states have been recorded, this will set the state to okay.
  // Otherwise, it will have no effect.
  RecordTelemetry(TelemetryState::Okay);
}

bool
D3D11LayersCrashGuard::UpdateEnvironment()
{
  static bool checked = false;
  static bool changed = false;

  if (checked) {
    return changed;
  }

  checked = true;

  if (mGfxInfo) {
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

  if (!changed) {
    return false;
  }

  RecordTelemetry(TelemetryState::EnvironmentChanged);
  return true;
}

void
D3D11LayersCrashGuard::LogCrashRecovery()
{
  RecordTelemetry(TelemetryState::RecoveredFromCrash);
  gfxCriticalError(CriticalLog::DefaultOptions(false)) << "D3D11 layers just crashed; D3D11 will be disabled.";
}

void
D3D11LayersCrashGuard::LogFeatureDisabled()
{
  RecordTelemetry(TelemetryState::FeatureDisabled);
  gfxCriticalError(CriticalLog::DefaultOptions(false)) << "D3D11 layers disabled due to a prior crash.";
}

void
D3D11LayersCrashGuard::RecordTelemetry(TelemetryState aState)
{
  // D3D11LayersCrashGuard is a no-op in the child process.
  if (!XRE_IsParentProcess()) {
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
