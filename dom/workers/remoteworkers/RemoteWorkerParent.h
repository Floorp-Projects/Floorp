/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWorkerParent_h
#define mozilla_dom_RemoteWorkerParent_h

#include "mozilla/dom/PRemoteWorkerParent.h"

namespace mozilla::dom {

class RemoteWorkerController;

/**
 * PBackground-managed parent actor that is mutually associated with a single
 * RemoteWorkerController.  Relays error/close events to the controller and in
 * turns is told life-cycle events.
 */
class RemoteWorkerParent final : public PRemoteWorkerParent {
  friend class PRemoteWorkerParent;

 public:
  NS_INLINE_DECL_REFCOUNTING(RemoteWorkerParent, override);

  RemoteWorkerParent();

  void Initialize(bool aAlreadyRegistered = false);

  void SetController(RemoteWorkerController* aController);

  void MaybeSendDelete();

 private:
  ~RemoteWorkerParent();

  already_AddRefed<PFetchEventOpProxyParent> AllocPFetchEventOpProxyParent(
      const ParentToChildServiceWorkerFetchEventOpArgs& aArgs);

  void ActorDestroy(mozilla::ipc::IProtocol::ActorDestroyReason) override;

  mozilla::ipc::IPCResult RecvError(const ErrorValue& aValue);

  mozilla::ipc::IPCResult RecvNotifyLock(const bool& aCreated);

  mozilla::ipc::IPCResult RecvNotifyWebTransport(const bool& aCreated);

  mozilla::ipc::IPCResult RecvClose();

  mozilla::ipc::IPCResult RecvCreated(const bool& aStatus);

  mozilla::ipc::IPCResult RecvSetServiceWorkerSkipWaitingFlag(
      SetServiceWorkerSkipWaitingFlagResolver&& aResolve);

  bool mDeleteSent = false;
  RefPtr<RemoteWorkerController> mController;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_RemoteWorkerParent_h
