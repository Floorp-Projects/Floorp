/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientHandleChild_h
#define _mozilla_dom_ClientHandleChild_h

#include "ClientThing.h"
#include "mozilla/dom/PClientHandleChild.h"

namespace mozilla::dom {

class ClientHandle;
class ClientInfo;

template <typename ActorType>
class ClientThing;

class ClientHandleChild final : public PClientHandleChild {
  ClientHandle* mHandle;
  bool mTeardownStarted;

  ~ClientHandleChild() = default;

  // PClientHandleChild interface
  mozilla::ipc::IPCResult RecvExecutionReady(
      const IPCClientInfo& aClientInfo) override;

  void ActorDestroy(ActorDestroyReason aReason) override;

  PClientHandleOpChild* AllocPClientHandleOpChild(
      const ClientOpConstructorArgs& aArgs) override;

  bool DeallocPClientHandleOpChild(PClientHandleOpChild* aActor) override;

 public:
  NS_INLINE_DECL_REFCOUNTING(ClientHandleChild, override)

  ClientHandleChild();

  void SetOwner(ClientThing<ClientHandleChild>* aThing);

  void RevokeOwner(ClientThing<ClientHandleChild>* aThing);

  void MaybeStartTeardown();
};

}  // namespace mozilla::dom

#endif  // _mozilla_dom_ClientHandleChild_h
