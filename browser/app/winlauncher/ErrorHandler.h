/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ErrorHandler_h
#define mozilla_ErrorHandler_h

#include "mozilla/Assertions.h"
#include "mozilla/LauncherResult.h"

namespace mozilla {

/**
 * All launcher process error handling should live in the implementation of
 * this function.
 */
void HandleLauncherError(const LauncherError& aError);

// This function is simply a convenience overload that automatically unwraps
// the LauncherError from the provided LauncherResult and then forwards it to
// the main implementation.
template <typename T>
inline void HandleLauncherError(const LauncherResult<T>& aResult) {
  MOZ_ASSERT(aResult.isErr());
  if (aResult.isOk()) {
    return;
  }

  HandleLauncherError(aResult.inspectErr());
}

// This function is simply a convenience overload that unwraps the provided
// GenericErrorResult<LauncherError> and forwards it to the main implementation.
inline void HandleLauncherError(
    const GenericErrorResult<LauncherError>& aResult) {
  LauncherVoidResult r(aResult);
  HandleLauncherError(r);
}

// Forward declaration
struct StaticXREAppData;

void SetLauncherErrorAppData(const StaticXREAppData& aAppData);

void SetLauncherErrorForceEventLog();

}  // namespace mozilla

#endif  //  mozilla_ErrorHandler_h
