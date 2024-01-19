/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientManagerChild_h
#define _mozilla_dom_ClientManagerChild_h

#include "ClientThing.h"
#include "mozilla/dom/PClientManagerChild.h"

namespace mozilla::dom {

class IPCWorkerRef;
class WorkerPrivate;

class ClientManagerChild final : public PClientManagerChild {
  ClientThing<ClientManagerChild>* mManager;

  RefPtr<IPCWorkerRef> mIPCWorkerRef;
  bool mTeardownStarted;

  ClientManagerChild();
  ~ClientManagerChild();

  // PClientManagerChild interface
  void ActorDestroy(ActorDestroyReason aReason) override;

  PClientManagerOpChild* AllocPClientManagerOpChild(
      const ClientOpConstructorArgs& aArgs) override;

  bool DeallocPClientManagerOpChild(PClientManagerOpChild* aActor) override;

  PClientNavigateOpChild* AllocPClientNavigateOpChild(
      const ClientNavigateOpConstructorArgs& aArgs) override;

  bool DeallocPClientNavigateOpChild(PClientNavigateOpChild* aActor) override;

  mozilla::ipc::IPCResult RecvPClientNavigateOpConstructor(
      PClientNavigateOpChild* aActor,
      const ClientNavigateOpConstructorArgs& aArgs) override;

 public:
  NS_INLINE_DECL_REFCOUNTING(ClientManagerChild, override)

  static already_AddRefed<ClientManagerChild> Create();

  void SetOwner(ClientThing<ClientManagerChild>* aThing);

  void RevokeOwner(ClientThing<ClientManagerChild>* aThing);

  void MaybeStartTeardown();

  mozilla::dom::WorkerPrivate* GetWorkerPrivate() const;
};

}  // namespace mozilla::dom

#endif  // _mozilla_dom_ClientManagerChild_h
