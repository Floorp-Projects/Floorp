/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_MainThreadRuntime_h
#define mozilla_mscom_MainThreadRuntime_h

#include "mozilla/Attributes.h"
#if defined(ACCESSIBILITY)
#include "mozilla/mscom/ActivationContext.h"
#endif // defined(ACCESSIBILITY)
#include "mozilla/mscom/COMApartmentRegion.h"
#include "mozilla/mscom/MainThreadClientInfo.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace mscom {

class MOZ_NON_TEMPORARY_CLASS MainThreadRuntime
{
public:
  MainThreadRuntime();
  ~MainThreadRuntime();

  explicit operator bool() const
  {
    return mStaRegion.IsValidOutermost() && SUCCEEDED(mInitResult);
  }

  MainThreadRuntime(MainThreadRuntime&) = delete;
  MainThreadRuntime(MainThreadRuntime&&) = delete;
  MainThreadRuntime& operator=(MainThreadRuntime&) = delete;
  MainThreadRuntime& operator=(MainThreadRuntime&&) = delete;

  /**
   * @return 0 if call is in-process or resolving the calling thread failed,
   *         otherwise contains the thread id of the calling thread.
   */
  static DWORD GetClientThreadId();

private:
  HRESULT InitializeSecurity();

  HRESULT mInitResult;
#if defined(ACCESSIBILITY)
  ActivationContextRegion mActCtxRgn;
#endif // defined(ACCESSIBILITY)
  STARegion mStaRegion;

  RefPtr<MainThreadClientInfo>  mClientInfo;

  static MainThreadRuntime* sInstance;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_MainThreadRuntime_h

