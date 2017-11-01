/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientHandleChild_h
#define _mozilla_dom_ClientHandleChild_h

#include "ClientThing.h"
#include "mozilla/dom/PClientHandleChild.h"

namespace mozilla {
namespace dom {

class ClientHandle;
class ClientInfo;

template <typename ActorType> class ClientThing;

class ClientHandleChild final : public PClientHandleChild
{
  ClientThing<ClientHandleChild>* mHandle;
  bool mTeardownStarted;

  // PClientHandleChild interface
  void
  ActorDestroy(ActorDestroyReason aReason) override;

  PClientHandleOpChild*
  AllocPClientHandleOpChild(const ClientOpConstructorArgs& aArgs) override;

  bool
  DeallocPClientHandleOpChild(PClientHandleOpChild* aActor) override;

public:
  ClientHandleChild();

  void
  SetOwner(ClientThing<ClientHandleChild>* aThing);

  void
  RevokeOwner(ClientThing<ClientHandleChild>* aThing);

  void
  MaybeStartTeardown();
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientHandleChild_h
