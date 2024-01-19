/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientSourceParent_h
#define _mozilla_dom_ClientSourceParent_h

#include "ClientInfo.h"
#include "ClientOpPromise.h"
#include "mozilla/dom/PClientSourceParent.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/MozPromise.h"

namespace mozilla::dom {

class ClientHandleParent;
class ClientManagerService;

class ClientSourceParent final : public PClientSourceParent {
  ClientInfo mClientInfo;
  Maybe<ServiceWorkerDescriptor> mController;
  const Maybe<ContentParentId> mContentParentId;
  RefPtr<ClientManagerService> mService;
  nsTArray<ClientHandleParent*> mHandleList;
  MozPromiseHolder<GenericNonExclusivePromise> mExecutionReadyPromise;
  bool mExecutionReady;
  bool mFrozen;

  void KillInvalidChild();

  ~ClientSourceParent();

  // PClientSourceParent
  mozilla::ipc::IPCResult RecvWorkerSyncPing() override;

  mozilla::ipc::IPCResult RecvTeardown() override;

  mozilla::ipc::IPCResult RecvExecutionReady(
      const ClientSourceExecutionReadyArgs& aArgs) override;

  mozilla::ipc::IPCResult RecvFreeze() override;

  mozilla::ipc::IPCResult RecvThaw() override;

  mozilla::ipc::IPCResult RecvInheritController(
      const ClientControlledArgs& aArgs) override;

  mozilla::ipc::IPCResult RecvNoteDOMContentLoaded() override;

  void ActorDestroy(ActorDestroyReason aReason) override;

  PClientSourceOpParent* AllocPClientSourceOpParent(
      const ClientOpConstructorArgs& aArgs) override;

  bool DeallocPClientSourceOpParent(PClientSourceOpParent* aActor) override;

 public:
  NS_INLINE_DECL_REFCOUNTING(ClientSourceParent, override)

  explicit ClientSourceParent(const ClientSourceConstructorArgs& aArgs,
                              const Maybe<ContentParentId>& aContentParentId);

  void Init();

  const ClientInfo& Info() const;

  bool IsFrozen() const;

  bool ExecutionReady() const;

  RefPtr<GenericNonExclusivePromise> ExecutionReadyPromise();

  const Maybe<ServiceWorkerDescriptor>& GetController() const;

  void ClearController();

  bool IsOwnedByProcess(ContentParentId aContentParentId) const {
    return mContentParentId && mContentParentId.value() == aContentParentId;
  }

  void AttachHandle(ClientHandleParent* aClientSource);

  void DetachHandle(ClientHandleParent* aClientSource);

  RefPtr<ClientOpPromise> StartOp(ClientOpConstructorArgs&& aArgs);
};

}  // namespace mozilla::dom

#endif  // _mozilla_dom_ClientSourceParent_h
