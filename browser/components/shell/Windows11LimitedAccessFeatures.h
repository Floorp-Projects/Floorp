/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHELL_WINDOWS11LIMITEDACCESSFEATURES_H__
#define SHELL_WINDOWS11LIMITEDACCESSFEATURES_H__

#include "nsISupportsImpl.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include <winerror.h>
#include <windows.h>  // for HRESULT
#include "mozilla/DefineEnum.h"
#include <winerror.h>

MOZ_DEFINE_ENUM_CLASS(Win11LimitedAccessFeatureType, (Taskbar));

/**
 * Class to manage unlocking limited access features on Windows 11.
 * Unless stubbing for testing purposes, create objects of this
 * class with CreateWin11LimitedAccessFeaturesInterface.
 *
 * Windows 11 requires certain features to be unlocked in order to work
 * (for instance, the Win11 Taskbar pinning APIs). Call Unlock()
 * to unlock them. Generally, results will be cached in atomic variables
 * and future calls to Unlock will be as long as it takes
 * to fetch an atomic variable.
 */
class Win11LimitedAccessFeaturesInterface {
 public:
  /**
   * Unlocks the limited access features, if possible.
   *
   * Returns an error code on error, true on successful unlock,
   * false on unlock failed (but with no error).
   */
  virtual mozilla::Result<bool, HRESULT> Unlock(
      Win11LimitedAccessFeatureType feature) = 0;

  /**
   * Reference counting and cycle collection.
   */
  NS_INLINE_DECL_REFCOUNTING(Win11LimitedAccessFeaturesInterface)

 protected:
  virtual ~Win11LimitedAccessFeaturesInterface() {}
};

RefPtr<Win11LimitedAccessFeaturesInterface>
CreateWin11LimitedAccessFeaturesInterface();

#endif  // SHELL_WINDOWS11LIMITEDACCESSFEATURES_H__
