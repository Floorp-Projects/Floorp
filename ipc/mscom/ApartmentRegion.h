/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_ApartmentRegion_h
#define mozilla_mscom_ApartmentRegion_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/mscom/COMWrappers.h"

namespace mozilla::mscom {

// This runtime-dynamic apartment class is used in ProcessRuntime.cpp, to
// initialize the process's main thread. Do not use it in new contexts if at all
// possible; instead, prefer ApartmentRegionT, below.
//
// For backwards compatibility, this class does not yet automatically disable
// OLE1/DDE, although there is believed to be no code relying on it.
//
// (TODO: phase out all uses of CoInitialize without `COINIT_DISABLE_OLE1DDE`?)
class MOZ_NON_TEMPORARY_CLASS ApartmentRegion final {
 public:
  /**
   * This constructor is to be used when we want to instantiate the object but
   * we do not yet know which type of apartment we want. Call Init() to
   * complete initialization.
   */
  constexpr ApartmentRegion() : mInitResult(CO_E_NOTINITIALIZED) {}

  explicit ApartmentRegion(COINIT aAptType)
      : mInitResult(wrapped::CoInitializeEx(nullptr, aAptType)) {
    // If this fires then we're probably mixing apartments on the same thread
    MOZ_ASSERT(IsValid());
  }

  ~ApartmentRegion() {
    if (IsValid()) {
      wrapped::CoUninitialize();
    }
  }

  explicit operator bool() const { return IsValid(); }

  bool IsValidOutermost() const { return mInitResult == S_OK; }

  bool IsValid() const { return SUCCEEDED(mInitResult); }

  bool Init(COINIT aAptType) {
    MOZ_ASSERT(mInitResult == CO_E_NOTINITIALIZED);
    mInitResult = wrapped::CoInitializeEx(nullptr, aAptType);
    MOZ_ASSERT(IsValid());
    return IsValid();
  }

  HRESULT GetHResult() const { return mInitResult; }

  ApartmentRegion(const ApartmentRegion&) = delete;
  ApartmentRegion& operator=(const ApartmentRegion&) = delete;
  ApartmentRegion(ApartmentRegion&&) = delete;
  ApartmentRegion& operator=(ApartmentRegion&&) = delete;

 private:
  HRESULT mInitResult;
};

template <COINIT AptType, bool UseOLE1 = false>
class MOZ_NON_TEMPORARY_CLASS ApartmentRegionT final {
  static COINIT ActualType() {
    static_assert(
        !((AptType & COINIT_DISABLE_OLE1DDE) == 0 && UseOLE1),
        "only one of `UseOLE1` and `COINIT_DISABLE_OLE1DDE` permitted");
    if (UseOLE1) return AptType;
    return static_cast<COINIT>(AptType | COINIT_DISABLE_OLE1DDE);
  }

 public:
  ApartmentRegionT() : mAptRgn(ActualType()) {}

  ~ApartmentRegionT() = default;

  explicit operator bool() const { return mAptRgn.IsValid(); }

  bool IsValidOutermost() const { return mAptRgn.IsValidOutermost(); }

  bool IsValid() const { return mAptRgn.IsValid(); }

  HRESULT GetHResult() const { return mAptRgn.GetHResult(); }

  ApartmentRegionT(const ApartmentRegionT&) = delete;
  ApartmentRegionT& operator=(const ApartmentRegionT&) = delete;
  ApartmentRegionT(ApartmentRegionT&&) = delete;
  ApartmentRegionT& operator=(ApartmentRegionT&&) = delete;

 private:
  ApartmentRegion mAptRgn;
};

using STARegion = ApartmentRegionT<COINIT_APARTMENTTHREADED>;
using MTARegion = ApartmentRegionT<COINIT_MULTITHREADED>;

}  // namespace mozilla::mscom

#endif  // mozilla_mscom_ApartmentRegion_h
