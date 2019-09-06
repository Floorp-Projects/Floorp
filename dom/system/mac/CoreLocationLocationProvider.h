/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIGeolocationProvider.h"
#include "mozilla/Attributes.h"

/*
 * The CoreLocationObjects class contains the CoreLocation objects
 * we'll need.
 *
 * Declaring them directly in CoreLocationLocationProvider
 * would require Objective-C++ syntax, which would contaminate all
 * files that include this header and require them to be Objective-C++
 * as well.
 *
 * The solution then is to forward-declare CoreLocationObjects here and
 * hold a pointer to it in CoreLocationLocationProvider, and only actually
 * define it in CoreLocationLocationProvider.mm, thus making it safe
 * for nsGeolocation.cpp, which is C++-only, to include this header.
 */
class CoreLocationObjects;
class MLSFallback;

bool isMacGeoSystemPermissionEnabled();

class CoreLocationLocationProvider : public nsIGeolocationProvider {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGEOLOCATIONPROVIDER

  CoreLocationLocationProvider();
  // MOZ_CAN_RUN_SCRIPT_BOUNDARY because we can't mark Objective-C methods as
  // MOZ_CAN_RUN_SCRIPT as far as I can tell, and this method is called from
  // Objective-C.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void NotifyError(uint16_t aErrorCode);
  void Update(nsIDOMGeoPosition* aSomewhere);
  void CreateMLSFallbackProvider();
  void CancelMLSFallbackProvider();

 private:
  virtual ~CoreLocationLocationProvider() = default;

  CoreLocationObjects* mCLObjects;
  nsCOMPtr<nsIGeolocationUpdate> mCallback;
  RefPtr<MLSFallback> mMLSFallbackProvider;

  class MLSUpdate : public nsIGeolocationUpdate {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIGEOLOCATIONUPDATE

    explicit MLSUpdate(CoreLocationLocationProvider& parentProvider);

   private:
    CoreLocationLocationProvider& mParentLocationProvider;
    virtual ~MLSUpdate() = default;
  };
};
