/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientHandleParent.h"

#include "ClientHandleOpParent.h"
#include "ClientSourceParent.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::IPCResult;

IPCResult
ClientHandleParent::RecvTeardown()
{
  Unused << Send__delete__(this);
  return IPC_OK();
}

void
ClientHandleParent::ActorDestroy(ActorDestroyReason aReason)
{
}

PClientHandleOpParent*
ClientHandleParent::AllocPClientHandleOpParent(const ClientOpConstructorArgs& aArgs)
{
  return new ClientHandleOpParent();
}

bool
ClientHandleParent::DeallocPClientHandleOpParent(PClientHandleOpParent* aActor)
{
  delete aActor;
  return true;
}

IPCResult
ClientHandleParent::RecvPClientHandleOpConstructor(PClientHandleOpParent* aActor,
                                                   const ClientOpConstructorArgs& aArgs)
{
  auto actor = static_cast<ClientHandleOpParent*>(aActor);
  actor->Init(aArgs);
  return IPC_OK();
}

ClientHandleParent::ClientHandleParent()
{
}

ClientHandleParent::~ClientHandleParent()
{
}

void
ClientHandleParent::Init(const IPCClientInfo& aClientInfo)
{
}

} // namespace dom
} // namespace mozilla
