/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_LOCKS_LOCKMANAGERPARENT_H_
#define DOM_LOCKS_LOCKMANAGERPARENT_H_

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/locks/PLockManagerParent.h"
#include "mozilla/dom/locks/LockRequestParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/WeakPtr.h"

namespace mozilla::dom::locks {

class ManagedLocks : public SupportsWeakPtr {
 public:
  NS_INLINE_DECL_REFCOUNTING(ManagedLocks)

  nsTArray<RefPtr<LockRequestParent>> mHeldLocks;
  nsTHashMap<nsStringHashKey, nsTArray<RefPtr<LockRequestParent>>> mQueueMap;

 private:
  ~ManagedLocks() = default;
};

class LockManagerParent final : public PLockManagerParent {
  using IPCResult = mozilla::ipc::IPCResult;

 public:
  NS_INLINE_DECL_REFCOUNTING(LockManagerParent)

  LockManagerParent(NotNull<nsIPrincipal*> aPrincipal, const nsID& aClientId);

  void ProcessRequestQueue(nsTArray<RefPtr<LockRequestParent>>& aQueue);
  bool IsGrantableRequest(const IPCLockRequest& aRequest);

  IPCResult RecvQuery(QueryResolver&& aResolver);

  already_AddRefed<PLockRequestParent> AllocPLockRequestParent(
      const IPCLockRequest& aRequest);
  IPCResult RecvPLockRequestConstructor(PLockRequestParent* aActor,
                                        const IPCLockRequest& aRequest) final;

  ManagedLocks& Locks() { return *mManagedLocks; }

 private:
  ~LockManagerParent() = default;

  void ActorDestroy(ActorDestroyReason aWhy) final;

  RefPtr<ManagedLocks> mManagedLocks;
  nsString mClientId;
  NotNull<nsCOMPtr<nsIPrincipal>> mPrincipal;
};

}  // namespace mozilla::dom::locks

#endif  // DOM_LOCKS_LOCKMANAGERPARENT_H_
