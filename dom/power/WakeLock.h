/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_power_WakeLock_h
#define mozilla_dom_power_WakeLock_h

#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"
#include "mozilla/ErrorResult.h"

class nsIDOMWindow;

namespace mozilla {
namespace dom {

class ContentParent;

class WakeLock MOZ_FINAL
  : public nsIDOMEventListener
  , public nsWrapperCache
  , public nsIObserver
  , public nsSupportsWeakReference
{
public:
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIOBSERVER

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(WakeLock, nsIDOMEventListener)

  // Note: WakeLock lives for the lifetime of the document in order to avoid
  // exposing GC behavior to pages. This means that
  // |var foo = navigator.requestWakeLock('cpu'); foo = null;|
  // doesn't unlock the 'cpu' resource.

  WakeLock();

  // Initialize this wake lock on behalf of the given window.  Null windows are
  // allowed; a lock without an associated window is always considered
  // invisible.
  nsresult Init(const nsAString &aTopic, nsIDOMWindow* aWindow);

  // Initialize this wake lock on behalf of the given process.  If the process
  // dies, the lock is released.  A wake lock initialized via this method is
  // always considered visible.
  nsresult Init(const nsAString &aTopic, ContentParent* aContentParent);

  // WebIDL methods

  nsISupports* GetParentObject() const;

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  void GetTopic(nsAString& aTopic);

  void Unlock(ErrorResult& aRv);

private:
  virtual ~WakeLock();

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

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_power_WakeLock_h
