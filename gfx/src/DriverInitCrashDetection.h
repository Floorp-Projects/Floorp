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

private:
  bool InitLockFilePath();
  bool UpdateEnvironment();
  bool CheckAndUpdatePref(const char* aPrefName, const nsAString& aCurrentValue);
  bool CheckAndUpdateBoolPref(const char* aPrefName, bool aCurrentValue);
  bool FeatureEnabled(int aFeature);
  void AllowDriverInitAttempt();
  bool RecoverFromDriverInitCrash();
  void FlushPreferences();

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

