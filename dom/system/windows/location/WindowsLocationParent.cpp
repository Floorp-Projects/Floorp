/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "WindowsLocationParent.h"
#include "nsIDOMGeoPosition.h"
#include "WindowsLocationProvider.h"

namespace mozilla::dom {

::mozilla::ipc::IPCResult WindowsLocationParent::RecvUpdate(
    RefPtr<nsIDOMGeoPosition> aGeoPosition) {
  if (mProvider) {
    mProvider->RecvUpdate(aGeoPosition);
  }
  return IPC_OK();
}

// A failure occurred.  This may be translated into a
// nsIGeolocationUpdate::NotifyError or may be ignored if the MLS fallback
// is available.
::mozilla::ipc::IPCResult WindowsLocationParent::RecvFailed(uint16_t err) {
  if (mProvider) {
    mProvider->RecvFailed(err);
  }
  return IPC_OK();
}

void WindowsLocationParent::ActorDestroy(ActorDestroyReason aReason) {
  if (mProvider) {
    mProvider->ActorStopped();
  }
}

}  // namespace mozilla::dom
