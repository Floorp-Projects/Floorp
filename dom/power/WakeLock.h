/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_power_WakeLock_h
#define mozilla_dom_power_WakeLock_h

#include "nsCOMPtr.h"
#include "nsIDOMWakeLock.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "nsWeakReference.h"

class nsIDOMWindow;

namespace mozilla {
namespace dom {

class ContentParent;

namespace power {

class WakeLock
  : public nsIDOMMozWakeLock
  , public nsIDOMEventListener
  , public nsIObserver
  , public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMOZWAKELOCK
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIOBSERVER

  // Note: WakeLock lives for the lifetime of the document in order to avoid
  // exposing GC behavior to pages. This means that
  // |var foo = navigator.requestWakeLock('cpu'); foo = null;|
  // doesn't unlock the 'cpu' resource.

  WakeLock();
  virtual ~WakeLock();

  // Initialize this wake lock on behalf of the given window.  Null windows are
  // allowed; a lock without an associated window is always considered
  // invisible.
  nsresult Init(const nsAString &aTopic, nsIDOMWindow* aWindow);

  // Initialize this wake lock on behalf of the given process.  If the process
  // dies, the lock is released.  A wake lock initialized via this method is
  // always considered visible.
  nsresult Init(const nsAString &aTopic, ContentParent* aContentParent);

private:
  void     DoUnlock();
  void     DoLock();
  void     AttachEventListener();
  void     DetachEventListener();

  bool      mLocked;
  bool      mHidden;

  // The ID of the ContentParent on behalf of whom we acquired this lock, or
  // CONTENT_PROCESS_UNKNOWN_ID if this lock was acquired on behalf of the
  // current process.
  uint64_t  mContentParentID;
  nsString  mTopic;

  // window that this was created for.  Weak reference.
  nsWeakPtr mWindow;
};

} // namespace power
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_power_WakeLock_h
