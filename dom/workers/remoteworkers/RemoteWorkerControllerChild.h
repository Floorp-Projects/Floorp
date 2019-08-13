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

namespace mozilla {
namespace dom {

class RemoteWorkerControllerChild final : public PRemoteWorkerControllerChild {
  friend class PRemoteWorkerControllerChild;

 public:
  NS_INLINE_DECL_REFCOUNTING(RemoteWorkerControllerChild)

  explicit RemoteWorkerControllerChild(RefPtr<RemoteWorkerObserver> aObserver);

  void Initialize();

  void RevokeObserver(RemoteWorkerObserver* aObserver);

  void MaybeSendDelete();

 private:
  ~RemoteWorkerControllerChild() = default;

  void ActorDestroy(ActorDestroyReason aReason) override;

  mozilla::ipc::IPCResult RecvCreationFailed();

  mozilla::ipc::IPCResult RecvCreationSucceeded();

  mozilla::ipc::IPCResult RecvErrorReceived(const ErrorValue& aError);

  mozilla::ipc::IPCResult RecvTerminated();

  RefPtr<RemoteWorkerObserver> mObserver;

  bool mIPCActive = true;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_remoteworkercontrollerchild_h__
