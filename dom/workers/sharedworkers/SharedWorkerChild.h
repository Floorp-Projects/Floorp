/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_dom_SharedWorkerChild_h
#define mozilla_dom_dom_SharedWorkerChild_h

#include "mozilla/dom/PSharedWorkerChild.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

class SharedWorker;

/**
 * Held by SharedWorker bindings to remotely control sharedworker lifecycle and
 * receive error and termination reports.
 */
class SharedWorkerChild final : public mozilla::dom::PSharedWorkerChild {
  friend class PSharedWorkerChild;

 public:
  NS_INLINE_DECL_REFCOUNTING(SharedWorkerChild)

  SharedWorkerChild();

  void SetParent(SharedWorker* aSharedWorker) { mParent = aSharedWorker; }

  void SendClose();

  void SendSuspend();

  void SendResume();

  void SendFreeze();

  void SendThaw();

 private:
  ~SharedWorkerChild();

  mozilla::ipc::IPCResult RecvError(const ErrorValue& aValue);

  mozilla::ipc::IPCResult RecvNotifyLock(bool aCreated);

  mozilla::ipc::IPCResult RecvNotifyWebTransport(bool aCreated);

  mozilla::ipc::IPCResult RecvTerminate();

  void ActorDestroy(ActorDestroyReason aWhy) override;

  // Raw pointer because mParent is set to null when released.
  SharedWorker* MOZ_NON_OWNING_REF mParent;
  bool mActive;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_dom_SharedWorkerChild_h
