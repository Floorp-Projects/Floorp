/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PortalLocationProvider_h
#define PortalLocationProvider_h

#include "nsCOMPtr.h"
#include "mozilla/GRefPtr.h"
#include "mozilla/GUniquePtr.h"
#include "Geolocation.h"
#include "nsIGeolocationProvider.h"
#include <gio/gio.h>

class MLSFallback;

namespace mozilla::dom {

class PortalLocationProvider final : public nsIGeolocationProvider,
                                     public nsITimerCallback,
                                     public nsINamed {
  class MLSGeolocationUpdate;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGEOLOCATIONPROVIDER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  PortalLocationProvider();

  void Update(nsIDOMGeoPosition* aPosition);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void NotifyError(int aError);

 private:
  ~PortalLocationProvider();
  void SetRefreshTimer(int aDelay);

  RefPtr<GDBusProxy> mDBUSLocationProxy;
  gulong mDBUSSignalHandler = 0;

  GUniquePtr<gchar> mPortalSession;
  nsCOMPtr<nsIGeolocationUpdate> mCallback;
  RefPtr<MLSFallback> mMLSProvider;
  nsCOMPtr<nsIDOMGeoPositionCoords> mLastGeoPositionCoords;
  nsCOMPtr<nsITimer> mRefreshTimer;
};

}  // namespace mozilla::dom

#endif /* GpsLocationProvider_h */
