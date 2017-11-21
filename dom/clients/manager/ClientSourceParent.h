/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientSourceParent_h
#define _mozilla_dom_ClientSourceParent_h

#include "ClientInfo.h"
#include "mozilla/dom/PClientSourceParent.h"

namespace mozilla {
namespace dom {

class ClientHandleParent;
class ClientManagerService;

class ClientSourceParent final : public PClientSourceParent
{
  ClientInfo mClientInfo;
  RefPtr<ClientManagerService> mService;
  nsTArray<ClientHandleParent*> mHandleList;
  bool mExecutionReady;
  bool mFrozen;

  void
  KillInvalidChild();

  // PClientSourceParent
  mozilla::ipc::IPCResult
  RecvTeardown() override;

  mozilla::ipc::IPCResult
  RecvExecutionReady(const ClientSourceExecutionReadyArgs& aArgs) override;

  mozilla::ipc::IPCResult
  RecvFreeze() override;

  mozilla::ipc::IPCResult
  RecvThaw() override;

  void
  ActorDestroy(ActorDestroyReason aReason) override;

  PClientSourceOpParent*
  AllocPClientSourceOpParent(const ClientOpConstructorArgs& aArgs) override;

  bool
  DeallocPClientSourceOpParent(PClientSourceOpParent* aActor) override;

public:
  explicit ClientSourceParent(const ClientSourceConstructorArgs& aArgs);
  ~ClientSourceParent();

  void
  Init();

  const ClientInfo&
  Info() const;

  bool
  IsFrozen() const;

  void
  AttachHandle(ClientHandleParent* aClientSource);

  void
  DetachHandle(ClientHandleParent* aClientSource);
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientSourceParent_h
