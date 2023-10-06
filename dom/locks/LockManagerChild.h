/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_LOCKS_LOCKMANAGERCHILD_H_
#define DOM_LOCKS_LOCKMANAGERCHILD_H_

#include "mozilla/dom/locks/PLockManagerChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerRef.h"
#include "nsIUUIDGenerator.h"

namespace mozilla::dom::locks {

struct LockRequest;

class LockManagerChild final : public PLockManagerChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(LockManagerChild)

  static void NotifyBFCacheOnMainThread(nsPIDOMWindowInner* aInner,
                                        bool aCreated);

  explicit LockManagerChild(nsIGlobalObject* aOwner);

  nsIGlobalObject* GetParentObject() const { return mOwner; };

  void RequestLock(const LockRequest& aRequest, const LockOptions& aOptions);

  void NotifyRequestDestroy() const;

  void NotifyToWindow(bool aCreated) const;

 private:
  ~LockManagerChild() = default;

  nsCOMPtr<nsIGlobalObject> mOwner;

  // This WorkerRef is deleted by destructor.
  //
  // Here we want to make sure the IPC layer deletes the actor before allowing
  // WorkerPrivate deletion, since managed LockRequestChild holds a reference to
  // global object (currently via mRequest member variable) and thus deleting
  // the worker first causes an assertion failure. Having this ensures that all
  // LockRequestChild instances must first have been deleted.
  //
  // (Because each LockRequestChild's ActorLifecycleProxy will hold
  // a strong reference to its manager LockManagerChild's ActorLifecycleProxy,
  // which means that the LockManagerChild will only be destroyed once all
  // LockRequestChild instances have been destroyed. At that point, all
  // references to the global should have been dropped, and then the
  // LockManagerChild IPCWorkerRef will be dropped and the worker will be able
  // to transition to the Killing state.)
  //
  // ActorDestroy can't release this since it does not guarantee to destruct and
  // GC the managees (LockRequestChild) immediately.
  RefPtr<IPCWorkerRef> mWorkerRef;
};

}  // namespace mozilla::dom::locks

#endif  // DOM_LOCKS_LOCKMANAGERCHILD_H_
