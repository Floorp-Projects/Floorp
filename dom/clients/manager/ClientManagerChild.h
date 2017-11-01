/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientManagerChild_h
#define _mozilla_dom_ClientManagerChild_h

#include "ClientThing.h"
#include "mozilla/dom/PClientManagerChild.h"
#include "mozilla/dom/workers/bindings/WorkerHolderToken.h"

namespace mozilla {
namespace dom {

namespace workers {
class WorkerPrivate;
} // workers namespace

class ClientManagerChild final : public PClientManagerChild
                               , public mozilla::dom::workers::WorkerHolderToken::Listener
{
  ClientThing<ClientManagerChild>* mManager;

  RefPtr<mozilla::dom::workers::WorkerHolderToken> mWorkerHolderToken;
  bool mTeardownStarted;

  // PClientManagerChild interface
  void
  ActorDestroy(ActorDestroyReason aReason) override;

  PClientHandleChild*
  AllocPClientHandleChild(const IPCClientInfo& aClientInfo) override;

  bool
  DeallocPClientHandleChild(PClientHandleChild* aActor) override;

  PClientManagerOpChild*
  AllocPClientManagerOpChild(const ClientOpConstructorArgs& aArgs) override;

  bool
  DeallocPClientManagerOpChild(PClientManagerOpChild* aActor) override;

  PClientNavigateOpChild*
  AllocPClientNavigateOpChild(const ClientNavigateOpConstructorArgs& aArgs) override;

  bool
  DeallocPClientNavigateOpChild(PClientNavigateOpChild* aActor) override;

  mozilla::ipc::IPCResult
  RecvPClientNavigateOpConstructor(PClientNavigateOpChild* aActor,
                                   const ClientNavigateOpConstructorArgs& aArgs) override;

  PClientSourceChild*
  AllocPClientSourceChild(const ClientSourceConstructorArgs& aArgs) override;

  bool
  DeallocPClientSourceChild(PClientSourceChild* aActor) override;

  // WorkerHolderToken::Listener interface
  void
  WorkerShuttingDown() override;

public:
  explicit ClientManagerChild(workers::WorkerHolderToken* aWorkerHolderToken);

  void
  SetOwner(ClientThing<ClientManagerChild>* aThing);

  void
  RevokeOwner(ClientThing<ClientManagerChild>* aThing);

  void
  MaybeStartTeardown();

  mozilla::dom::workers::WorkerPrivate*
  GetWorkerPrivate() const;
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientManagerChild_h
