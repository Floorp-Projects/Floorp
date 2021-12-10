/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_VsyncWorkerChild_h
#define mozilla_dom_ipc_VsyncWorkerChild_h

#include "mozilla/dom/VsyncChild.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {
class DedicatedWorkerGlobalScope;
class IPCWorkerRef;
class WorkerPrivate;

// The PVsyncWorkerChild actor receives a vsync event from the main process and
// delivers it to the child process. This is specific for workers which wish to
// subscribe to vsync events to drive requestAnimationFrame callbacks.
class VsyncWorkerChild final : public VsyncChild {
  NS_INLINE_DECL_REFCOUNTING(VsyncWorkerChild, override)

  friend class PVsyncChild;

 public:
  VsyncWorkerChild();

  bool Initialize(WorkerPrivate* aWorkerPrivate);

  void Destroy();

  void TryObserve();

  void TryUnobserve();

 private:
  ~VsyncWorkerChild() override;

  mozilla::ipc::IPCResult RecvNotify(const VsyncEvent& aVsync,
                                     const float& aVsyncRate) override;

  void ActorDestroy(ActorDestroyReason aActorDestroyReason) override;

  RefPtr<IPCWorkerRef> mWorkerRef;
  bool mObserving = false;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_ipc_VsyncWorkerChild_h
