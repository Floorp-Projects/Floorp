/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WindowsLocationProvider_h__
#define mozilla_dom_WindowsLocationProvider_h__

#include "nsCOMPtr.h"
#include "nsIGeolocationProvider.h"
#include "mozilla/MozPromise.h"

class MLSFallback;

namespace mozilla::dom {

class WindowsLocationParent;

// Uses a PWindowsLocation actor to subscribe to geolocation updates from the
// Windows utility process and falls back to MLS when it is not available or
// fails.
class WindowsLocationProvider final : public nsIGeolocationProvider {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGEOLOCATIONPROVIDER

  WindowsLocationProvider();

 private:
  friend WindowsLocationParent;

  ~WindowsLocationProvider();

  nsresult CreateAndWatchMLSProvider(nsIGeolocationUpdate* aCallback);
  void CancelMLSProvider();

  void MaybeCreateLocationActor();
  void ReleaseUtilityProcess();

  // These methods either send the message on the existing actor or queue
  // the messages to be sent (in order) once the actor exists.
  bool SendStartup();
  bool SendRegisterForReport(nsIGeolocationUpdate* aCallback);
  bool SendUnregisterForReport();
  bool SendSetHighAccuracy(bool aEnable);
  bool Send__delete__();

  void RecvUpdate(RefPtr<nsIDOMGeoPosition> aGeoPosition);
  // See bug 1539864 for MOZ_CAN_RUN_SCRIPT_BOUNDARY justification.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void RecvFailed(uint16_t err);

  // The utility process actor has ended its connection, either successfully
  // or with an error.
  void ActorStopped();

  // Run fn once actor is ready to send messages, which may be immediately.
  template <typename Fn>
  bool WhenActorIsReady(Fn&& fn);

  RefPtr<MLSFallback> mMLSProvider;

  nsCOMPtr<nsIGeolocationUpdate> mCallback;

  using WindowsLocationPromise =
      MozPromise<RefPtr<WindowsLocationParent>, bool, false>;

  // Before the utility process exists, we have a promise that we will get our
  // location actor.  mActor and mActorPromise are never both set.
  RefPtr<WindowsLocationPromise> mActorPromise;
  RefPtr<WindowsLocationParent> mActor;

  bool mWatching = false;
  bool mEverUpdated = false;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_WindowsLocationProvider_h__
