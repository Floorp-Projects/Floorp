/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_remoteworkercontrollerparent_h__
#define mozilla_dom_remoteworkercontrollerparent_h__

#include <functional>

#include "nsISupportsImpl.h"

#include "RemoteWorkerController.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/PRemoteWorkerControllerParent.h"

namespace mozilla {
namespace dom {

class RemoteWorkerControllerParent final : public PRemoteWorkerControllerParent,
                                           public RemoteWorkerObserver {
  friend class PRemoteWorkerControllerParent;

 public:
  NS_INLINE_DECL_REFCOUNTING(RemoteWorkerControllerParent, override)

  explicit RemoteWorkerControllerParent(
      const RemoteWorkerData& aRemoteWorkerData);

  // Returns the corresponding RemoteWorkerParent (if any).
  RefPtr<RemoteWorkerParent> GetRemoteWorkerParent() const;

  void MaybeSendSetServiceWorkerSkipWaitingFlag(
      std::function<void(bool)>&& aCallback);

 private:
  ~RemoteWorkerControllerParent();

  PFetchEventOpParent* AllocPFetchEventOpParent(
      const ServiceWorkerFetchEventOpArgs& aArgs);

  mozilla::ipc::IPCResult RecvPFetchEventOpConstructor(
      PFetchEventOpParent* aActor,
      const ServiceWorkerFetchEventOpArgs& aArgs) override;

  bool DeallocPFetchEventOpParent(PFetchEventOpParent* aActor);

  mozilla::ipc::IPCResult RecvExecServiceWorkerOp(
      ServiceWorkerOpArgs&& aArgs, ExecServiceWorkerOpResolver&& aResolve);

  mozilla::ipc::IPCResult RecvShutdown(ShutdownResolver&& aResolve);

  mozilla::ipc::IPCResult Recv__delete__() override;

  void ActorDestroy(ActorDestroyReason aReason) override;

  void CreationFailed() override;

  void CreationSucceeded() override;

  void ErrorReceived(const ErrorValue& aValue) override;

  void Terminated() override;

  RefPtr<RemoteWorkerController> mRemoteWorkerController;

  bool mIPCActive = true;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_remoteworkercontrollerparent_h__
