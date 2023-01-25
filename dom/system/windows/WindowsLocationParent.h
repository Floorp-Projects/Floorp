/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WindowsLocationParent_h__
#define mozilla_dom_WindowsLocationParent_h__

#include "nsCOMPtr.h"
#include "mozilla/dom/PWindowsLocationParent.h"

class nsGeoPosition;
class nsIGeolocationUpdate;

namespace mozilla::dom {

class WindowsLocationProvider;

// Geolocation actor in main process.
// This may receive messages asynchronously, even after it sends Unregister
// to the child.
class WindowsLocationParent final : public PWindowsLocationParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WindowsLocationParent, override);

  using IPCResult = ::mozilla::ipc::IPCResult;

  explicit WindowsLocationParent(WindowsLocationProvider* aProvider)
      : mProvider(aProvider) {}

  // Update geolocation with new position information.
  IPCResult RecvUpdate(RefPtr<nsIDOMGeoPosition> aGeoPosition);

  // A failure occurred.  This may be translated into a
  // nsIGeolocationUpdate::NotifyError or may be ignored if the MLS fallback
  // is available.
  IPCResult RecvFailed(uint16_t err);

  void ActorDestroy(ActorDestroyReason aReason) override;

  // After this, the actor will simply ignore any incoming messages.
  void DetachFromLocationProvider() { mProvider = nullptr; }

 private:
  ~WindowsLocationParent() override = default;

  WindowsLocationProvider* mProvider;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_WindowsLocationParent_h__
