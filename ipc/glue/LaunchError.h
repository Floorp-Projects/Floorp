/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_LaunchError_h
#define mozilla_ipc_LaunchError_h

#include "prerror.h"

#if defined(XP_WIN)
#  include <windows.h>
#  include <winerror.h>
using OsError = HRESULT;
#else
using OsError = int;
#endif

namespace mozilla::ipc {

class LaunchError {
 public:
  explicit LaunchError(const char* aFunction, OsError aError = 0);
#if defined(XP_WIN)
  explicit LaunchError(const char* aFunction, PRErrorCode aError);
  explicit LaunchError(const char* aFunction, DWORD aError);
#endif  // defined(XP_WIN)

  const char* FunctionName() const;
  OsError ErrorCode() const;

 private:
  const char* mFunction;
  OsError mError;
};

}  // namespace mozilla::ipc

#endif  // ifndef mozilla_ipc_LaunchError_h
