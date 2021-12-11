/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LockManagerParent.h"
#include "LockRequestParent.h"

#include "mozilla/dom/Promise.h"

namespace mozilla::dom::locks {

mozilla::ipc::IPCResult LockRequestParent::Recv__delete__(bool aAborted) {
  RefPtr<LockManagerParent> manager =
      static_cast<LockManagerParent*>(Manager());
  ManagedLocks& managed = manager->Locks();

  DebugOnly<bool> unheld = managed.mHeldLocks.RemoveElement(this);
  MOZ_ASSERT_IF(!aAborted, unheld);

  if (auto queue = managed.mQueueMap.Lookup(mRequest.name())) {
    if (aAborted) {
      DebugOnly<bool> dequeued = queue.Data().RemoveElement(this);
      MOZ_ASSERT_IF(!unheld, dequeued);
    }
    manager->ProcessRequestQueue(queue.Data());
    if (queue.Data().IsEmpty()) {
      // Remove if empty, to prevent the queue map from growing forever
      queue.Remove();
    }
  }
  // or else, the queue is removed during the previous lock release (since
  // multiple held locks are possible with `shared: true`)
  return IPC_OK();
}

}  // namespace mozilla::dom::locks
