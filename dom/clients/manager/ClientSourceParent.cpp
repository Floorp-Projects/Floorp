/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientSourceParent.h"

#include "ClientHandleParent.h"
#include "ClientManagerService.h"
#include "ClientSourceOpParent.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::IPCResult;
using mozilla::ipc::PrincipalInfo;

IPCResult
ClientSourceParent::RecvTeardown()
{
  Unused << Send__delete__(this);
  return IPC_OK();
}

void
ClientSourceParent::ActorDestroy(ActorDestroyReason aReason)
{
  mService->RemoveSource(this);

  nsTArray<ClientHandleParent*> handleList(mHandleList);
  for (ClientHandleParent* handle : handleList) {
    // This should trigger DetachHandle() to be called removing
    // the entry from the mHandleList.
    Unused << ClientHandleParent::Send__delete__(handle);
  }
  MOZ_DIAGNOSTIC_ASSERT(mHandleList.IsEmpty());
}

PClientSourceOpParent*
ClientSourceParent::AllocPClientSourceOpParent(const ClientOpConstructorArgs& aArgs)
{
  MOZ_ASSERT_UNREACHABLE("ClientSourceOpParent should be explicitly constructed.");
  return nullptr;
}

bool
ClientSourceParent::DeallocPClientSourceOpParent(PClientSourceOpParent* aActor)
{
  delete aActor;
  return true;
}

ClientSourceParent::ClientSourceParent(const ClientSourceConstructorArgs& aArgs)
  : mClientInfo(aArgs.id(), aArgs.type(), aArgs.principalInfo(), aArgs.creationTime())
  , mService(ClientManagerService::GetOrCreateInstance())
{
  mService->AddSource(this);
}

ClientSourceParent::~ClientSourceParent()
{
  MOZ_DIAGNOSTIC_ASSERT(mHandleList.IsEmpty());
}

const ClientInfo&
ClientSourceParent::Info() const
{
  return mClientInfo;
}

void
ClientSourceParent::AttachHandle(ClientHandleParent* aClientHandle)
{
  MOZ_DIAGNOSTIC_ASSERT(aClientHandle);
  MOZ_ASSERT(!mHandleList.Contains(aClientHandle));
  mHandleList.AppendElement(aClientHandle);
}

void
ClientSourceParent::DetachHandle(ClientHandleParent* aClientHandle)
{
  MOZ_DIAGNOSTIC_ASSERT(aClientHandle);
  MOZ_ASSERT(mHandleList.Contains(aClientHandle));
  mHandleList.RemoveElement(aClientHandle);
}

} // namespace dom
} // namespace mozilla
