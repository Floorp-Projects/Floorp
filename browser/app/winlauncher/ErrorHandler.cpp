/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ErrorHandler.h"

#if defined(MOZ_LAUNCHER_PROCESS)
#  include "mozilla/LauncherRegistryInfo.h"
#  include "mozilla/Unused.h"
#endif  // defined(MOZ_LAUNCHER_PROCESS)

namespace mozilla {

void HandleLauncherError(const LauncherError& aError) {
  // This is a placeholder error handler. We'll add telemetry and a fallback
  // error log in future revisions.

#if defined(MOZ_LAUNCHER_PROCESS)
  LauncherRegistryInfo regInfo;
  Unused << regInfo.DisableDueToFailure();
#endif  // defined(MOZ_LAUNCHER_PROCESS)

  WindowsError::UniqueString msg = aError.mError.AsString();
  if (!msg) {
    return;
  }

  ::MessageBoxW(nullptr, msg.get(), L"Firefox", MB_OK | MB_ICONERROR);
}

}  // namespace mozilla
