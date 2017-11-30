/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientManagerParent.h"

#include "ClientHandleParent.h"
#include "ClientManagerOpParent.h"
#include "ClientManagerService.h"
#include "ClientSourceParent.h"
#include "mozilla/dom/PClientNavigateOpParent.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::IPCResult;

IPCResult
ClientManagerParent::RecvTeardown()
{
  Unused << Send__delete__(this);
  return IPC_OK();
}

void
ClientManagerParent::ActorDestroy(ActorDestroyReason aReason)
{
}

PClientHandleParent*
ClientManagerParent::AllocPClientHandleParent(const IPCClientInfo& aClientInfo)
{
  return new ClientHandleParent();
}

bool
ClientManagerParent::DeallocPClientHandleParent(PClientHandleParent* aActor)
{
  delete aActor;
  return true;
}

IPCResult
ClientManagerParent::RecvPClientHandleConstructor(PClientHandleParent* aActor,
                                                  const IPCClientInfo& aClientInfo)
{
  ClientHandleParent* actor = static_cast<ClientHandleParent*>(aActor);
  actor->Init(aClientInfo);
  return IPC_OK();
}

PClientManagerOpParent*
ClientManagerParent::AllocPClientManagerOpParent(const ClientOpConstructorArgs& aArgs)
{
  return new ClientManagerOpParent(mService);
}

bool
ClientManagerParent::DeallocPClientManagerOpParent(PClientManagerOpParent* aActor)
{
  delete aActor;
  return true;
}

IPCResult
ClientManagerParent::RecvPClientManagerOpConstructor(PClientManagerOpParent* aActor,
                                                     const ClientOpConstructorArgs& aArgs)
{
  ClientManagerOpParent* actor = static_cast<ClientManagerOpParent*>(aActor);
  actor->Init(aArgs);
  return IPC_OK();
}

PClientNavigateOpParent*
ClientManagerParent::AllocPClientNavigateOpParent(const ClientNavigateOpConstructorArgs& aArgs)
{
  MOZ_ASSERT_UNREACHABLE("ClientNavigateOpParent should be explicitly constructed.");
  return nullptr;
}

bool
ClientManagerParent::DeallocPClientNavigateOpParent(PClientNavigateOpParent* aActor)
{
  delete aActor;
  return true;
}

PClientSourceParent*
ClientManagerParent::AllocPClientSourceParent(const ClientSourceConstructorArgs& aArgs)
{
  return new ClientSourceParent(aArgs);
}

bool
ClientManagerParent::DeallocPClientSourceParent(PClientSourceParent* aActor)
{
  delete aActor;
  return true;
}

IPCResult
ClientManagerParent::RecvPClientSourceConstructor(PClientSourceParent* aActor,
                                                  const ClientSourceConstructorArgs& aArgs)
{
  ClientSourceParent* actor = static_cast<ClientSourceParent*>(aActor);
  actor->Init();
  return IPC_OK();
}

ClientManagerParent::ClientManagerParent()
  : mService(ClientManagerService::GetOrCreateInstance())
{
}

ClientManagerParent::~ClientManagerParent()
{
  mService->RemoveManager(this);
}

void
ClientManagerParent::Init()
{
  mService->AddManager(this);
}

} // namespace dom
} // namespace mozilla
