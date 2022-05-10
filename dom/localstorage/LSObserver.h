/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_LSObserver_h
#define mozilla_dom_localstorage_LSObserver_h

#include "mozilla/Assertions.h"
#include "nsISupports.h"
#include "nsString.h"

namespace mozilla::dom {

class LSObserverChild;

/**
 * Effectively just a refcounted life-cycle management wrapper around
 * LSObserverChild which exists to receive "storage" event information from
 * other processes.  (Same-process events are handled within the process, see
 * `LSObject::OnChange`.)
 *
 * ## Lifecycle ##
 * - Created by LSObject::EnsureObserver via synchronous LSRequest idiom
 *   whenever the first window's origin adds a "storage" event.  Placed in the
 *   gLSObservers LSObserverHashtable for subsequent LSObject's via
 *   LSObserver::Get lookup.
 * - The LSObserverChild directly handles "Observe" messages, shunting them
 *   directly to Storage::NotifyChange which does all the legwork of notifying
 *   windows about "storage" events.
 * - Destroyed when refcount goes to zero due to all owning LSObjects being
 *   destroyed or having their `LSObject::DropObserver` methods invoked due to
 *   the last "storage" event listener being removed from the owning window.
 */
class LSObserver final {
  friend class LSObject;

  LSObserverChild* mActor;

  const nsCString mOrigin;

 public:
  static LSObserver* Get(const nsACString& aOrigin);

  NS_INLINE_DECL_REFCOUNTING(LSObserver)

  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(LSObserver); }

  void SetActor(LSObserverChild* aActor);

  void ClearActor() {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mActor);

    mActor = nullptr;
  }

 private:
  // Only created by LSObject.
  explicit LSObserver(const nsACString& aOrigin);

  ~LSObserver();
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_localstorage_LSObserver_h
