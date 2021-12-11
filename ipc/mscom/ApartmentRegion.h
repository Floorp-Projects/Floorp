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

namespace mozilla {
namespace mscom {

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

  HRESULT
  GetHResult() const { return mInitResult; }

 private:
  ApartmentRegion(const ApartmentRegion&) = delete;
  ApartmentRegion& operator=(const ApartmentRegion&) = delete;
  ApartmentRegion(ApartmentRegion&&) = delete;
  ApartmentRegion& operator=(ApartmentRegion&&) = delete;

  HRESULT mInitResult;
};

template <COINIT T>
class MOZ_NON_TEMPORARY_CLASS ApartmentRegionT final {
 public:
  ApartmentRegionT() : mAptRgn(T) {}

  ~ApartmentRegionT() = default;

  explicit operator bool() const { return mAptRgn.IsValid(); }

  bool IsValidOutermost() const { return mAptRgn.IsValidOutermost(); }

  bool IsValid() const { return mAptRgn.IsValid(); }

  HRESULT GetHResult() const { return mAptRgn.GetHResult(); }

 private:
  ApartmentRegionT(const ApartmentRegionT&) = delete;
  ApartmentRegionT& operator=(const ApartmentRegionT&) = delete;
  ApartmentRegionT(ApartmentRegionT&&) = delete;
  ApartmentRegionT& operator=(ApartmentRegionT&&) = delete;

  ApartmentRegion mAptRgn;
};

typedef ApartmentRegionT<COINIT_APARTMENTTHREADED> STARegion;
typedef ApartmentRegionT<COINIT_MULTITHREADED> MTARegion;

}  // namespace mscom
}  // namespace mozilla

#endif  // mozilla_mscom_ApartmentRegion_h
