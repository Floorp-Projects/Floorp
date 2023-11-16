/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_power_WakeLock_h
#define mozilla_dom_power_WakeLock_h

#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"
#include "nsIWakeLock.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {
class ErrorResult;

namespace dom {
class Document;

class WakeLock final : public nsIDOMEventListener,
                       public nsSupportsWeakReference,
                       public nsIWakeLock {
 public:
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIWAKELOCK

  NS_DECL_ISUPPORTS

  // Note: WakeLock lives for the lifetime of the document in order to avoid
  // exposing GC behavior to pages. This means that
  // |var foo = navigator.requestWakeLock('cpu'); foo = null;|
  // doesn't unlock the 'cpu' resource.

  WakeLock() = default;

  // Initialize this wake lock on behalf of the given window.  Null windows are
  // allowed; a lock without an associated window is always considered
  // invisible.
  nsresult Init(const nsAString& aTopic, nsPIDOMWindowInner* aWindow);

  // WebIDL methods

  nsPIDOMWindowInner* GetParentObject() const;

  void GetTopic(nsAString& aTopic);

  void Unlock(ErrorResult& aRv);

 private:
  virtual ~WakeLock();

  void DoUnlock();
  void DoLock();
  void AttachEventListener();
  void DetachEventListener();

  bool mLocked = false;
  bool mHidden = true;

  nsString mTopic;

  // window that this was created for.  Weak reference.
  nsWeakPtr mWindow;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_power_WakeLock_h
