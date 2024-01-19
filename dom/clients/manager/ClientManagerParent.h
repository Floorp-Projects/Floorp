/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientManagerParent_h
#define _mozilla_dom_ClientManagerParent_h

#include "mozilla/dom/PClientManagerParent.h"

namespace mozilla::dom {

class ClientManagerService;

class ClientManagerParent final : public PClientManagerParent {
  RefPtr<ClientManagerService> mService;

  ~ClientManagerParent();

  // PClientManagerParent interface
  mozilla::ipc::IPCResult RecvTeardown() override;

  void ActorDestroy(ActorDestroyReason aReason) override;

  already_AddRefed<PClientHandleParent> AllocPClientHandleParent(
      const IPCClientInfo& aClientInfo) override;

  mozilla::ipc::IPCResult RecvPClientHandleConstructor(
      PClientHandleParent* aActor, const IPCClientInfo& aClientInfo) override;

  PClientManagerOpParent* AllocPClientManagerOpParent(
      const ClientOpConstructorArgs& aArgs) override;

  bool DeallocPClientManagerOpParent(PClientManagerOpParent* aActor) override;

  mozilla::ipc::IPCResult RecvPClientManagerOpConstructor(
      PClientManagerOpParent* aActor,
      const ClientOpConstructorArgs& aArgs) override;

  PClientNavigateOpParent* AllocPClientNavigateOpParent(
      const ClientNavigateOpConstructorArgs& aArgs) override;

  bool DeallocPClientNavigateOpParent(PClientNavigateOpParent* aActor) override;

  already_AddRefed<PClientSourceParent> AllocPClientSourceParent(
      const ClientSourceConstructorArgs& aArgs) override;

  mozilla::ipc::IPCResult RecvPClientSourceConstructor(
      PClientSourceParent* aActor,
      const ClientSourceConstructorArgs& aArgs) override;

  mozilla::ipc::IPCResult RecvExpectFutureClientSource(
      const IPCClientInfo& aClientInfo) override;

  mozilla::ipc::IPCResult RecvForgetFutureClientSource(
      const IPCClientInfo& aClientInfo) override;

 public:
  NS_INLINE_DECL_REFCOUNTING(ClientManagerParent, override)

  ClientManagerParent();

  void Init();
};

}  // namespace mozilla::dom

#endif  // _mozilla_dom_ClientManagerParent_h
