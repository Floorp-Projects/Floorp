/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_MainThreadRuntime_h
#define mozilla_mscom_MainThreadRuntime_h

#include "mozilla/Attributes.h"
#include "mozilla/mscom/COMApartmentRegion.h"

namespace mozilla {
namespace mscom {

class MOZ_NON_TEMPORARY_CLASS MainThreadRuntime
{
public:
  MainThreadRuntime();

  explicit operator bool() const
  {
    return mStaRegion.IsValidOutermost() && SUCCEEDED(mInitResult);
  }

  MainThreadRuntime(MainThreadRuntime&) = delete;
  MainThreadRuntime(MainThreadRuntime&&) = delete;
  MainThreadRuntime& operator=(MainThreadRuntime&) = delete;
  MainThreadRuntime& operator=(MainThreadRuntime&&) = delete;

private:
  HRESULT InitializeSecurity();

  STARegion mStaRegion;
  HRESULT mInitResult;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_MainThreadRuntime_h

