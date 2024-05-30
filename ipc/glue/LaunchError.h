/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_LaunchError_h
#define mozilla_ipc_LaunchError_h

#include "mozilla/StaticString.h"
#include "nsError.h"  // for nsresult

#if defined(XP_WIN)
#  include <windows.h>
#  include <winerror.h>
#endif

namespace mozilla::ipc {

class LaunchError {
 public:
  // The context of an error code (at the very least, the function that emitted
  // it and where it was called) is fundamentally necessary to interpret its
  // value. Since we should have that, we're not too fussed about integral
  // coercions, so long as we don't lose any bits.
  //
  // Under POSIX, the usual OS-error type is `int`. Under Windows, the usual
  // error type is `HRESULT`, which (despite having bitfield semantics) is
  // ultimately a typedef for a flavor of (signed) `long`. For simplicity, and
  // in the absence of other constraints, our "error type" container is simply
  // the join of these two.
  using ErrorType = long;

  // Constructor.
  //
  // The default of `0` is intended for failure cases where we don't have an
  // error code -- usually because the function we've called to determine
  // failure has only a boolean return, but also for when we've detected some
  // internal failure and there's no appropriate `nsresult` for it.
  explicit LaunchError(StaticString aFunction, ErrorType aError = 0)
      : mFunction(aFunction), mError(aError) {}

#ifdef WIN32
  // The error code returned by ::GetLastError() is (usually) not an HRESULT.
  // This convenience-function maps its return values to the segment of
  // HRESULT's namespace reserved for that.
  //
  // This is not formally necessary -- the error location should suffice to
  // disambiguate -- but it's nice to have it converted when reading through
  // error-values: `0x80070005` is marginally more human-meaningful than `5`.
  static LaunchError FromWin32Error(StaticString aFunction, DWORD aError) {
    return LaunchError(aFunction, HRESULT_FROM_WIN32(aError));
  }
#endif

  // By design, `nsresult` does not implicitly coerce to an integer.
  LaunchError(StaticString aFunction, nsresult aError)
      : mFunction(aFunction), mError((ErrorType)aError) {}

  StaticString FunctionName() const { return mFunction; }
  ErrorType ErrorCode() const { return mError; }

 private:
  StaticString mFunction;
  ErrorType mError;
};

}  // namespace mozilla::ipc

#endif  // ifndef mozilla_ipc_LaunchError_h
