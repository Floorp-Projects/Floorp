/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GlobalTeardownObserver.h"
#include "mozilla/Sprintf.h"
#include "mozilla/dom/Document.h"

namespace mozilla {

GlobalTeardownObserver::GlobalTeardownObserver() = default;
GlobalTeardownObserver::GlobalTeardownObserver(nsIGlobalObject* aGlobalObject,
                                               bool aHasOrHasHadOwnerWindow)
    : mHasOrHasHadOwnerWindow(aHasOrHasHadOwnerWindow) {
  BindToOwner(aGlobalObject);
}

GlobalTeardownObserver::~GlobalTeardownObserver() {
  if (mParentObject) {
    mParentObject->RemoveGlobalTeardownObserver(this);
  }
}

void GlobalTeardownObserver::BindToOwner(nsIGlobalObject* aOwner) {
  MOZ_ASSERT(!mParentObject);

  if (aOwner) {
    mParentObject = aOwner;
    aOwner->AddGlobalTeardownObserver(this);
    // Let's cache the result of this QI for fast access and off main thread
    // usage
    mOwnerWindow =
        nsCOMPtr<nsPIDOMWindowInner>(do_QueryInterface(aOwner)).get();
    if (mOwnerWindow) {
      mHasOrHasHadOwnerWindow = true;
    }
  }
}

void GlobalTeardownObserver::DisconnectFromOwner() {
  if (mParentObject) {
    mParentObject->RemoveGlobalTeardownObserver(this);
  }
  mOwnerWindow = nullptr;
  mParentObject = nullptr;
}

nsresult GlobalTeardownObserver::CheckCurrentGlobalCorrectness() const {
  NS_ENSURE_STATE(!mHasOrHasHadOwnerWindow || mOwnerWindow);

  // Main-thread.
  if (mOwnerWindow && !mOwnerWindow->IsCurrentInnerWindow()) {
    return NS_ERROR_FAILURE;
  }

  if (NS_IsMainThread()) {
    return NS_OK;
  }

  if (!mParentObject) {
    return NS_ERROR_FAILURE;
  }

  if (mParentObject->IsDying()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

};  // namespace mozilla
