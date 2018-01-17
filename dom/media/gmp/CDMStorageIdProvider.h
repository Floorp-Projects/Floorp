/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CDMStorageIdProvider_h_
#define CDMStorageIdProvider_h_

/**
 * CDM will try to request a latest version(0) of storage id.
 * If the storage id computation algorithm changed, we should increase the kCurrentVersion.
*/

#include <string>

#include "nsString.h"

namespace mozilla {

class CDMStorageIdProvider
{
  static constexpr const char* kBrowserIdentifier =
    "mozilla_firefox_gecko";

public:
  // Should increase the value when the storage id algorithm changed.
  static constexpr int kCurrentVersion = 1;
  static constexpr int kCDMRequestLatestVersion = 0;

  // Return empty string when
  // 1. Call on unsupported storageid platform.
  // 2. Failed to compute the storage id.
  // This function only provide the storage id for kCurrentVersion=1.
  // If you want to change the algorithm or output of storageid,
  // you should keep the version 1 storage id and consider to provide
  // higher version storage id in another function.
  static nsCString ComputeStorageId(const nsCString& aOriginSalt);

};

} // namespace mozilla

#endif // CDMStorageIdProvider_h_
