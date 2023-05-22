/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_ProcessRuntime_h
#define mozilla_mscom_ProcessRuntime_h

#include "mozilla/Attributes.h"
#include "mozilla/mscom/ApartmentRegion.h"
#include "nsWindowsHelpers.h"
#if defined(MOZILLA_INTERNAL_API)
#  include "nsXULAppAPI.h"
#endif  // defined(MOZILLA_INTERNAL_API)

namespace mozilla {
namespace mscom {

class MOZ_NON_TEMPORARY_CLASS ProcessRuntime final {
#if !defined(MOZILLA_INTERNAL_API)
 public:
#endif  // defined(MOZILLA_INTERNAL_API)
  enum class ProcessCategory {
    GeckoBrowserParent,
    // We give Launcher its own process category, but internally to this class
    // it should be treated identically to GeckoBrowserParent.
    Launcher = GeckoBrowserParent,
    GeckoChild,
    Service,
  };

  // This constructor is only public when compiled outside of XUL
  explicit ProcessRuntime(const ProcessCategory aProcessCategory);

 public:
#if defined(MOZILLA_INTERNAL_API)
  ProcessRuntime();
  ~ProcessRuntime();
#else
  ~ProcessRuntime() = default;
#endif  // defined(MOZILLA_INTERNAL_API)

  explicit operator bool() const { return SUCCEEDED(mInitResult); }
  HRESULT GetHResult() const { return mInitResult; }

  ProcessRuntime(const ProcessRuntime&) = delete;
  ProcessRuntime(ProcessRuntime&&) = delete;
  ProcessRuntime& operator=(const ProcessRuntime&) = delete;
  ProcessRuntime& operator=(ProcessRuntime&&) = delete;

  /**
   * @return 0 if call is in-process or resolving the calling thread failed,
   *         otherwise contains the thread id of the calling thread.
   */
  static DWORD GetClientThreadId();

 private:
#if defined(MOZILLA_INTERNAL_API)
  explicit ProcessRuntime(const GeckoProcessType aProcessType);
#  if defined(MOZ_SANDBOX)
  void InitUsingPersistentMTAThread(const nsAutoHandle& aCurThreadToken);
#  endif  // defined(MOZ_SANDBOX)
#endif    // defined(MOZILLA_INTERNAL_API)
  void InitInsideApartment();

#if defined(MOZILLA_INTERNAL_API)
  static void PostInit();
#endif  // defined(MOZILLA_INTERNAL_API)
  static HRESULT InitializeSecurity(const ProcessCategory aProcessCategory);
  static COINIT GetDesiredApartmentType(const ProcessCategory aProcessCategory);

 private:
  HRESULT mInitResult;
  const ProcessCategory mProcessCategory;
  ApartmentRegion mAptRegion;

 private:
#if defined(MOZILLA_INTERNAL_API)
  static ProcessRuntime* sInstance;
#endif  // defined(MOZILLA_INTERNAL_API)
};

}  // namespace mscom
}  // namespace mozilla

#endif  // mozilla_mscom_ProcessRuntime_h
