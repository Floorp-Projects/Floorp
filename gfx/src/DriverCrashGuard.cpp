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
#include "mozilla/dom/ContentChild.h"

namespace mozilla {
namespace gfx {

static const size_t NUM_CRASH_GUARD_TYPES = size_t(CrashGuardType::NUM_TYPES);
static const char* sCrashGuardNames[NUM_CRASH_GUARD_TYPES] = {
  "d3d11layers",
  "d3d9video",
  "glcontext",
};

DriverCrashGuard::DriverCrashGuard(CrashGuardType aType, dom::ContentParent* aContentParent)
 : mType(aType)
 , mMode(aContentParent ? Mode::Proxy : Mode::Normal)
 , mInitialized(false)
 , mGuardActivated(false)
 , mCrashDetected(false)
{
  MOZ_ASSERT(mType < CrashGuardType::NUM_TYPES);

  mStatusPref.Assign("gfx.crash-guard.status.");
  mStatusPref.Append(sCrashGuardNames[size_t(mType)]);
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
#ifdef NIGHTLY_BUILD
  // We only use the crash guard on non-nightly channels, since the nightly
  // channel is for development and having graphics features perma-disabled
  // is rather annoying.
  return;
#endif

  // Using DriverCrashGuard off the main thread currently does not work. Under
  // e10s it could conceivably work by dispatching the IPC calls via the main
  // thread. In the parent process this would be harder. For now, we simply
  // exit early instead.
  if (!NS_IsMainThread()) {
    return;
  }

  // Check to see if all guards have been disabled through the environment.
  static bool sAllGuardsDisabled = !!PR_GetEnv("MOZ_DISABLE_CRASH_GUARD");
  if (sAllGuardsDisabled) {
    return;
  }

  mGfxInfo = services::GetGfxInfo();

  if (XRE_IsContentProcess()) {
    // Ask the parent whether or not activating the guard is okay. The parent
    // won't bother if it detected a crash.
    dom::ContentChild* cc = dom::ContentChild::GetSingleton();
    cc->SendBeginDriverCrashGuard(uint32_t(mType), &mCrashDetected);
    if (mCrashDetected) {
      LogFeatureDisabled();
      return;
    }

    ActivateGuard();
    return;
  }

  // Always check whether or not the lock file exists. For example, we could
  // have crashed creating a D3D9 device in the parent process, and on restart
  // are now requesting one in the child process. We catch everything here.
  if (RecoverFromCrash()) {
    mCrashDetected = true;
    return;
  }

  // If the environment has changed, we always activate the guard. In the
  // parent process this performs main-thread disk I/O. Child process guards
  // only incur an IPC cost, so if we're proxying for a child process, we
  // play it safe and activate the guard as long as we don't expect it to
  // crash.
  if (CheckOrRefreshEnvironment() ||
      (mMode == Mode::Proxy && GetStatus() != DriverInitStatus::Crashed))
  {
    ActivateGuard();
    return;
  }

  // If we got here and our status is "crashed", then the environment has not
  // updated and we do not want to attempt to use the driver again.
  if (GetStatus() == DriverInitStatus::Crashed) {
    mCrashDetected = true;
    LogFeatureDisabled();
  }
}

DriverCrashGuard::~DriverCrashGuard()
{
  if (!mGuardActivated) {
    return;
  }

  if (XRE_IsParentProcess()) {
    if (mGuardFile) {
      mGuardFile->Remove(false);
    }

    // If during our initialization, no other process encountered a crash, we
    // proceed to mark the status as okay.
    if (GetStatus() != DriverInitStatus::Crashed) {
      SetStatus(DriverInitStatus::Okay);
    }
  } else {
    dom::ContentChild::GetSingleton()->SendEndDriverCrashGuard(uint32_t(mType));
  }

#ifdef MOZ_CRASHREPORTER
  // Remove the crash report annotation.
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("GraphicsStartupTest"),
                                     NS_LITERAL_CSTRING(""));
#endif
}

bool
DriverCrashGuard::Crashed()
{
  InitializeIfNeeded();

  // Note, we read mCrashDetected instead of GetStatus(), since in child
  // processes we're not guaranteed that the prefs have been synced in
  // time.
  return mCrashDetected;
}

nsCOMPtr<nsIFile>
DriverCrashGuard::GetGuardFile()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  nsCString filename;
  filename.Assign(sCrashGuardNames[size_t(mType)]);
  filename.Append(".guard");

  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR, getter_AddRefs(file));
  if (!file) {
    return nullptr;
  }
  if (!NS_SUCCEEDED(file->AppendNative(filename))) {
    return nullptr;
  }
  return file;
}

void
DriverCrashGuard::ActivateGuard()
{
  mGuardActivated = true;

#ifdef MOZ_CRASHREPORTER
  // Anotate crash reports only if we're a real guard. Otherwise, we could
  // attribute a random parent process crash to a graphics problem in a child
  // process.
  if (mMode != Mode::Proxy) {
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("GraphicsStartupTest"),
                                       NS_LITERAL_CSTRING("1"));
  }
#endif

  // If we're in the content process, the rest of the guarding is handled
  // in the parent.
  if (XRE_IsContentProcess()) {
    return;
  }

  SetStatus(DriverInitStatus::Attempting);

  if (mMode != Mode::Proxy) {
    // In parent process guards, we use two tombstones to detect crashes: a
    // preferences and a zero-byte file on the filesystem.
    FlushPreferences();

    // Create a temporary tombstone/lockfile.
    FILE* fp = nullptr;
    mGuardFile = GetGuardFile();
    if (!mGuardFile || !NS_SUCCEEDED(mGuardFile->OpenANSIFileDesc("w", &fp))) {
      return;
    }
    fclose(fp);
  }
}

void
DriverCrashGuard::NotifyCrashed()
{
  CheckOrRefreshEnvironment();
  SetStatus(DriverInitStatus::Crashed);
  FlushPreferences();
  LogCrashRecovery();
}

bool
DriverCrashGuard::RecoverFromCrash()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  nsCOMPtr<nsIFile> file = GetGuardFile();
  bool exists;
  if ((file &&
       NS_SUCCEEDED(file->Exists(&exists)) &&
       exists) ||
      (GetStatus() == DriverInitStatus::Attempting))
  {
    // If we get here, we've just recovered from a crash. Disable acceleration
    // until the environment changes.
    if (file) {
      file->Remove(false);
    }
    NotifyCrashed();
    return true;
  }
  return false;
}

// Return true if the caller should proceed to guard for crashes. False if
// the environment has not changed. We persist the "changed" status across
// calls, so that after an environment changes, all guards for the new
// session are activated rather than just the first.
bool
DriverCrashGuard::CheckOrRefreshEnvironment()
{
  // Our result can be cached statically since we don't check live prefs.
  static bool sBaseInfoChanged = false;
  static bool sBaseInfoChecked = false;

  if (!sBaseInfoChecked) {
    // None of the prefs we care about, so we cache the result statically.
    sBaseInfoChecked = true;
    sBaseInfoChanged = UpdateBaseEnvironment();
  }

  // Always update the full environment, even if the base info didn't change.
  return UpdateEnvironment() ||
         sBaseInfoChanged ||
         GetStatus() == DriverInitStatus::Unknown;
}

bool
DriverCrashGuard::UpdateBaseEnvironment()
{
  bool changed = false;
  if (mGfxInfo) {
    nsString value;

    // Driver properties.
    mGfxInfo->GetAdapterDriverVersion(value);
    changed |= CheckAndUpdatePref("driverVersion", value);
    mGfxInfo->GetAdapterDeviceID(value);
    changed |= CheckAndUpdatePref("deviceID", value);
  }

  // Firefox properties.
  changed |= CheckAndUpdatePref("appVersion", NS_LITERAL_STRING(MOZ_APP_VERSION));

  return changed;
}

bool
DriverCrashGuard::FeatureEnabled(int aFeature, bool aDefault)
{
  if (!mGfxInfo) {
    return aDefault;
  }
  int32_t status;
  if (!NS_SUCCEEDED(mGfxInfo->GetFeatureStatus(aFeature, &status))) {
    return false;
  }
  return status == nsIGfxInfo::FEATURE_STATUS_OK;
}

bool
DriverCrashGuard::CheckAndUpdateBoolPref(const char* aPrefName, bool aCurrentValue)
{
  std::string pref = GetFullPrefName(aPrefName);

  bool oldValue;
  if (NS_SUCCEEDED(Preferences::GetBool(pref.c_str(), &oldValue)) &&
      oldValue == aCurrentValue)
  {
    return false;
  }
  Preferences::SetBool(pref.c_str(), aCurrentValue);
  return true;
}

bool
DriverCrashGuard::CheckAndUpdatePref(const char* aPrefName, const nsAString& aCurrentValue)
{
  std::string pref = GetFullPrefName(aPrefName);

  nsAdoptingString oldValue = Preferences::GetString(pref.c_str());
  if (oldValue == aCurrentValue) {
    return false;
  }
  Preferences::SetString(pref.c_str(), aCurrentValue);
  return true;
}

std::string
DriverCrashGuard::GetFullPrefName(const char* aPref)
{
  return std::string("gfx.crash-guard.") +
         std::string(sCrashGuardNames[uint32_t(mType)]) +
         std::string(".") +
         std::string(aPref);
}

DriverInitStatus
DriverCrashGuard::GetStatus() const
{
  return (DriverInitStatus)Preferences::GetInt(mStatusPref.get(), 0);
}

void
DriverCrashGuard::SetStatus(DriverInitStatus aStatus)
{
  MOZ_ASSERT(XRE_IsParentProcess());

  Preferences::SetInt(mStatusPref.get(), int32_t(aStatus));
}

void
DriverCrashGuard::FlushPreferences()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  if (nsIPrefService* prefService = Preferences::GetService()) {
    prefService->SavePrefFile(nullptr);
  }
}

D3D11LayersCrashGuard::D3D11LayersCrashGuard(dom::ContentParent* aContentParent)
 : DriverCrashGuard(CrashGuardType::D3D11Layers, aContentParent)
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
  // Our result can be cached statically since we don't check live prefs.
  static bool checked = false;
  static bool changed = false;

  if (checked) {
    return changed;
  }

  checked = true;

  // Feature status.
#if defined(XP_WIN)
  bool d2dEnabled = gfxPrefs::Direct2DForceEnabled() ||
                    (!gfxPrefs::Direct2DDisabled() && FeatureEnabled(nsIGfxInfo::FEATURE_DIRECT2D));
  changed |= CheckAndUpdateBoolPref("feature-d2d", d2dEnabled);

  bool d3d11Enabled = !gfxPrefs::LayersPreferD3D9();
  if (!FeatureEnabled(nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS)) {
    d3d11Enabled = false;
  }
  changed |= CheckAndUpdateBoolPref("feature-d3d11", d3d11Enabled);
#endif

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

D3D9VideoCrashGuard::D3D9VideoCrashGuard(dom::ContentParent* aContentParent)
 : DriverCrashGuard(CrashGuardType::D3D9Video, aContentParent)
{
}

bool
D3D9VideoCrashGuard::UpdateEnvironment()
{
  // We don't care about any extra preferences here.
  return false;
}

void
D3D9VideoCrashGuard::LogCrashRecovery()
{
  gfxCriticalError(CriticalLog::DefaultOptions(false)) << "DXVA2D3D9 just crashed; hardware video will be disabled.";
}

void
D3D9VideoCrashGuard::LogFeatureDisabled()
{
  gfxCriticalError(CriticalLog::DefaultOptions(false)) << "DXVA2D3D9 video decoding is disabled due to a previous crash.";
}

GLContextCrashGuard::GLContextCrashGuard(dom::ContentParent* aContentParent)
 : DriverCrashGuard(CrashGuardType::GLContext, aContentParent)
{
}

void
GLContextCrashGuard::Initialize()
{
  if (XRE_IsContentProcess()) {
    // Disable the GL crash guard in content processes, since we're not going
    // to lose the entire browser and we don't want to hinder WebGL availability.
    return;
  }

  DriverCrashGuard::Initialize();
}

bool
GLContextCrashGuard::UpdateEnvironment()
{
  static bool checked = false;
  static bool changed = false;

  if (checked) {
    return changed;
  }

  checked = true;

#if defined(XP_WIN)
  changed |= CheckAndUpdateBoolPref("gfx.driver-init.webgl-angle-force-d3d11",
                                    gfxPrefs::WebGLANGLEForceD3D11());
  changed |= CheckAndUpdateBoolPref("gfx.driver-init.webgl-angle-try-d3d11",
                                    gfxPrefs::WebGLANGLETryD3D11());
  changed |= CheckAndUpdateBoolPref("gfx.driver-init.webgl-angle-force-warp",
                                    gfxPrefs::WebGLANGLEForceWARP());
  changed |= CheckAndUpdateBoolPref("gfx.driver-init.webgl-angle",
                                    FeatureEnabled(nsIGfxInfo::FEATURE_WEBGL_ANGLE, false));
  changed |= CheckAndUpdateBoolPref("gfx.driver-init.direct3d11-angle",
                                    FeatureEnabled(nsIGfxInfo::FEATURE_DIRECT3D_11_ANGLE, false));
#endif

  return changed;
}

void
GLContextCrashGuard::LogCrashRecovery()
{
  gfxCriticalError(CriticalLog::DefaultOptions(false)) << "GLContext just crashed and is now disabled.";
}

void
GLContextCrashGuard::LogFeatureDisabled()
{
  gfxCriticalError(CriticalLog::DefaultOptions(false)) << "GLContext is disabled due to a previous crash.";
}

} // namespace gfx
} // namespace mozilla
