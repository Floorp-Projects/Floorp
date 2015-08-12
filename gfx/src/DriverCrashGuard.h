/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef gfx_src_DriverCrashGuard_h__
#define gfx_src_DriverCrashGuard_h__

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

// DriverCrashGuard is used to detect crashes at graphics driver callsites.
// 
// If the graphics environment is unrecognized or has changed since the last
// session, the crash guard will activate and will detect any crashes within
// the scope of the guard object.
//
// If a callsite has a previously encountered crash, and the environment has
// not changed since the last session, then the guard will set a status flag
// indicating that the driver should not be used.
class DriverCrashGuard
{
public:
  DriverCrashGuard();
  ~DriverCrashGuard();

  bool Crashed();

  // These are the values reported to Telemetry (GRAPHICS_DRIVER_STARTUP_TEST).
  // Values should not change; add new values to the end.
  enum class TelemetryState {
    Okay = 0,
    EnvironmentChanged = 1,
    RecoveredFromCrash = 2,
    AccelerationDisabled = 3
  };

protected:
  virtual void RecordTelemetry(TelemetryState aState);
  virtual bool UpdateEnvironment() = 0;
  virtual void Initialize() = 0;

  // Helper functions.
  bool FeatureEnabled(int aFeature);
  bool CheckAndUpdatePref(const char* aPrefName, const nsAString& aCurrentValue);
  bool CheckAndUpdateBoolPref(const char* aPrefName, bool aCurrentValue);

private:
  void InitializeIfNeeded();
  bool InitLockFilePath();
  void AllowDriverInitAttempt();
  bool RecoverFromDriverInitCrash();
  void FlushPreferences();
  bool PrepareToGuard();
  bool UpdateBaseEnvironment();

private:
  bool mInitialized;
  nsCOMPtr<nsIFile> mLockFile;

protected:
  bool mIsChromeProcess;
  nsCOMPtr<nsIGfxInfo> mGfxInfo;
};

class D3D11LayersCrashGuard final : public DriverCrashGuard
{
 public:
  D3D11LayersCrashGuard();

 protected:
  void Initialize() override;
  bool UpdateEnvironment() override;
  void RecordTelemetry(TelemetryState aState) override;
};

} // namespace gfx
} // namespace mozilla

#endif // gfx_src_DriverCrashGuard_h__

