/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_remoteworkercontrollerchild_h__
#define mozilla_dom_remoteworkercontrollerchild_h__

#include "nsISupportsImpl.h"

#include "RemoteWorkerController.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/PRemoteWorkerControllerChild.h"

namespace mozilla::dom {

/**
 * Parent-process main-thread proxy used by ServiceWorkerManager to control
 * RemoteWorkerController instances on the parent-process PBackground thread.
 */
class RemoteWorkerControllerChild final : public PRemoteWorkerControllerChild {
  friend class PRemoteWorkerControllerChild;

 public:
  NS_INLINE_DECL_REFCOUNTING(RemoteWorkerControllerChild, override)

  explicit RemoteWorkerControllerChild(RefPtr<RemoteWorkerObserver> aObserver);

  void Initialize();

  void RevokeObserver(RemoteWorkerObserver* aObserver);

  void MaybeSendDelete();

  TimeStamp GetRemoteWorkerLaunchStart();
  TimeStamp GetRemoteWorkerLaunchEnd();

 private:
  ~RemoteWorkerControllerChild() = default;

  PFetchEventOpChild* AllocPFetchEventOpChild(
      const ParentToParentServiceWorkerFetchEventOpArgs& aArgs);

  bool DeallocPFetchEventOpChild(PFetchEventOpChild* aActor);

  void ActorDestroy(ActorDestroyReason aReason) override;

  mozilla::ipc::IPCResult RecvCreationFailed();

  mozilla::ipc::IPCResult RecvCreationSucceeded();

  mozilla::ipc::IPCResult RecvErrorReceived(const ErrorValue& aError);

  mozilla::ipc::IPCResult RecvTerminated();

  mozilla::ipc::IPCResult RecvSetServiceWorkerSkipWaitingFlag(
      SetServiceWorkerSkipWaitingFlagResolver&& aResolve);

  RefPtr<RemoteWorkerObserver> mObserver;

  bool mIPCActive = true;

  TimeStamp mRemoteWorkerLaunchStart;
  TimeStamp mRemoteWorkerLaunchEnd;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_remoteworkercontrollerchild_h__
