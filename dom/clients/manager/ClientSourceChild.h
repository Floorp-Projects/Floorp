/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientSourceChild_h
#define _mozilla_dom_ClientSourceChild_h

#include "mozilla/dom/PClientSourceChild.h"

namespace mozilla::dom {

class ClientSource;
class ClientSourceConstructorArgs;
template <typename ActorType>
class ClientThing;

class ClientSourceChild final : public PClientSourceChild {
  ClientSource* mSource;
  bool mTeardownStarted;

  ~ClientSourceChild() = default;

  // PClientSourceChild interface
  void ActorDestroy(ActorDestroyReason aReason) override;

  PClientSourceOpChild* AllocPClientSourceOpChild(
      const ClientOpConstructorArgs& aArgs) override;

  bool DeallocPClientSourceOpChild(PClientSourceOpChild* aActor) override;

  mozilla::ipc::IPCResult RecvPClientSourceOpConstructor(
      PClientSourceOpChild* aActor,
      const ClientOpConstructorArgs& aArgs) override;

  mozilla::ipc::IPCResult RecvEvictFromBFCache() override;

 public:
  NS_INLINE_DECL_REFCOUNTING(ClientSourceChild, override)

  explicit ClientSourceChild(const ClientSourceConstructorArgs& aArgs);

  void SetOwner(ClientThing<ClientSourceChild>* aThing);

  void RevokeOwner(ClientThing<ClientSourceChild>* aThing);

  ClientSource* GetSource() const;

  void MaybeStartTeardown();
};

}  // namespace mozilla::dom

#endif  // _mozilla_dom_ClientSourceChild_h
