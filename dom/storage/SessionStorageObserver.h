/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStorageObserver_h
#define mozilla_dom_SessionStorageObserver_h

#include "nsISupportsImpl.h"

namespace mozilla::dom {

class SessionStorageObserverChild;

/**
 * Effectively just a refcounted life-cycle management wrapper around
 * SessionStorageObserverChild which exists to receive chrome observer
 * notifications from the main process.
 *
 * ## Lifecycle ##
 * - Created by SessionStorageManager::SessionStorageManager.  Placed in the
 *   gSessionStorageObserver variable for subsequent SessionStorageManager's via
 *   SessionStorageObserver::Get lookup.
 * - The SessionStorageObserverChild directly handles "Observe" messages,
 *   shunting them directly to StorageObserver::Notify which distributes them to
 *   individual observer sinks.
 * - Destroyed when refcount goes to zero due to all owning
 *   SessionStorageManager being destroyed.
 */
class SessionStorageObserver final {
  friend class SessionStorageManager;

  SessionStorageObserverChild* mActor;

 public:
  static SessionStorageObserver* Get();

  NS_INLINE_DECL_REFCOUNTING(SessionStorageObserver)

  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(SessionStorageObserver);
  }

  void SetActor(SessionStorageObserverChild* aActor);

  void ClearActor() {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mActor);

    mActor = nullptr;
  }

 private:
  // Only created by SessionStorageManager.
  SessionStorageObserver();

  ~SessionStorageObserver();
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_SessionStorageObserver_h
