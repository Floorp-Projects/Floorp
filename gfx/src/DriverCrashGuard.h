/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef gfx_src_DriverCrashGuard_h__
#define gfx_src_DriverCrashGuard_h__

#include "nsCOMPtr.h"
#include "nsIGfxInfo.h"
#include "nsIFile.h"
#include "nsString.h"
#include <string>

namespace mozilla {

namespace dom {
class ContentParent;
} // namespace dom

namespace gfx {

enum class DriverInitStatus
{
  // Drivers have not been initialized yet.
  Unknown,

  // We're attempting to initialize drivers.
  Attempting,

  // Drivers were successfully initialized last run.
  Okay,

  // We crashed during driver initialization, and have restarted.
  Crashed
};

enum class CrashGuardType : uint32_t
{
  D3D11Layers,
  D3D9Video,
  GLContext,
  NUM_TYPES
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
  DriverCrashGuard(CrashGuardType aType, dom::ContentParent* aContentParent);
  virtual ~DriverCrashGuard();

  bool Crashed();
  void NotifyCrashed();

  // These are the values reported to Telemetry (GRAPHICS_DRIVER_STARTUP_TEST).
  // Values should not change; add new values to the end.
  enum class TelemetryState {
    Okay = 0,
    EnvironmentChanged = 1,
    RecoveredFromCrash = 2,
    FeatureDisabled = 3
  };

  enum class Mode {
    // Normal operation.
    Normal,

    // Acting as a proxy between the parent and child process.
    Proxy
  };

protected:
  virtual void Initialize();
  virtual bool UpdateEnvironment() = 0;
  virtual void LogCrashRecovery() = 0;
  virtual void LogFeatureDisabled() = 0;

  // Helper functions.
  bool FeatureEnabled(int aFeature, bool aDefault=true);
  bool CheckAndUpdatePref(const char* aPrefName, const nsAString& aCurrentValue);
  bool CheckAndUpdateBoolPref(const char* aPrefName, bool aCurrentValue);
  std::string GetFullPrefName(const char* aPref);

private:
  // Either process.
  void InitializeIfNeeded();
  bool CheckOrRefreshEnvironment();
  bool UpdateBaseEnvironment();
  DriverInitStatus GetStatus() const;

  // Parent process only.
  nsCOMPtr<nsIFile> GetGuardFile();
  bool RecoverFromCrash();
  void ActivateGuard();
  void FlushPreferences();
  void SetStatus(DriverInitStatus aStatus);

private:
  CrashGuardType mType;
  Mode mMode;
  bool mInitialized;
  bool mGuardActivated;
  bool mCrashDetected;
  nsCOMPtr<nsIFile> mGuardFile;

protected:
  nsCString mStatusPref;
  nsCOMPtr<nsIGfxInfo> mGfxInfo;
};

class D3D11LayersCrashGuard final : public DriverCrashGuard
{
 public:
  explicit D3D11LayersCrashGuard(dom::ContentParent* aContentParent = nullptr);

 protected:
  void Initialize() override;
  bool UpdateEnvironment() override;
  void LogCrashRecovery() override;
  void LogFeatureDisabled() override;

 private:
  void RecordTelemetry(TelemetryState aState);
};

class D3D9VideoCrashGuard final : public DriverCrashGuard
{
 public:
  explicit D3D9VideoCrashGuard(dom::ContentParent* aContentParent = nullptr);

 protected:
  bool UpdateEnvironment() override;
  void LogCrashRecovery() override;
  void LogFeatureDisabled() override;
};

class GLContextCrashGuard final : public DriverCrashGuard
{
 public:
  explicit GLContextCrashGuard(dom::ContentParent* aContentParent = nullptr);
  void Initialize() override;

 protected:
  bool UpdateEnvironment() override;
  void LogCrashRecovery() override;
  void LogFeatureDisabled() override;
};

} // namespace gfx
} // namespace mozilla

#endif // gfx_src_DriverCrashGuard_h__

