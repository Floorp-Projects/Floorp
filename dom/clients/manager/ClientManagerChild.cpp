/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientManagerChild.h"

#include "ClientHandleChild.h"
#include "ClientManagerOpChild.h"
#include "ClientNavigateOpChild.h"
#include "ClientSourceChild.h"

namespace mozilla {
namespace dom {

using mozilla::dom::workers::WorkerHolderToken;
using mozilla::dom::workers::WorkerPrivate;

void
ClientManagerChild::ActorDestroy(ActorDestroyReason aReason)
{
  if (mWorkerHolderToken) {
    mWorkerHolderToken->RemoveListener(this);
    mWorkerHolderToken = nullptr;

  }

  if (mManager) {
    mManager->RevokeActor(this);

    // Revoking the actor link should automatically cause the owner
    // to call RevokeOwner() as well.
    MOZ_DIAGNOSTIC_ASSERT(!mManager);
  }
}

PClientHandleChild*
ClientManagerChild::AllocPClientHandleChild(const IPCClientInfo& aClientInfo)
{
  return new ClientHandleChild();
}

bool
ClientManagerChild::DeallocPClientHandleChild(PClientHandleChild* aActor)
{
  delete aActor;
  return true;
}

PClientManagerOpChild*
ClientManagerChild::AllocPClientManagerOpChild(const ClientOpConstructorArgs& aArgs)
{
  MOZ_ASSERT_UNREACHABLE("ClientManagerOpChild must be explicitly constructed.");
  return nullptr;
}

bool
ClientManagerChild::DeallocPClientManagerOpChild(PClientManagerOpChild* aActor)
{
  delete aActor;
  return true;
}

PClientNavigateOpChild*
ClientManagerChild::AllocPClientNavigateOpChild(const ClientNavigateOpConstructorArgs& aArgs)
{
  return new ClientNavigateOpChild();
}

bool
ClientManagerChild::DeallocPClientNavigateOpChild(PClientNavigateOpChild* aActor)
{
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult
ClientManagerChild::RecvPClientNavigateOpConstructor(PClientNavigateOpChild* aActor,
                                                     const ClientNavigateOpConstructorArgs& aArgs)
{
  auto actor = static_cast<ClientNavigateOpChild*>(aActor);
  actor->Init(aArgs);
  return IPC_OK();
}

PClientSourceChild*
ClientManagerChild::AllocPClientSourceChild(const ClientSourceConstructorArgs& aArgs)
{
  return new ClientSourceChild(aArgs);
}

bool
ClientManagerChild::DeallocPClientSourceChild(PClientSourceChild* aActor)
{
  delete aActor;
  return true;
}

void
ClientManagerChild::WorkerShuttingDown()
{
  MaybeStartTeardown();
}

ClientManagerChild::ClientManagerChild(WorkerHolderToken* aWorkerHolderToken)
  : mManager(nullptr)
  , mWorkerHolderToken(aWorkerHolderToken)
  , mTeardownStarted(false)
{
  MOZ_ASSERT_IF(!NS_IsMainThread(), mWorkerHolderToken);

  if (mWorkerHolderToken) {
    mWorkerHolderToken->AddListener(this);
  }
}

void
ClientManagerChild::SetOwner(ClientThing<ClientManagerChild>* aThing)
{
  MOZ_DIAGNOSTIC_ASSERT(aThing);
  MOZ_DIAGNOSTIC_ASSERT(!mManager);
  mManager = aThing;
}

void
ClientManagerChild::RevokeOwner(ClientThing<ClientManagerChild>* aThing)
{
  MOZ_DIAGNOSTIC_ASSERT(mManager);
  MOZ_DIAGNOSTIC_ASSERT(mManager == aThing);
  mManager = nullptr;
}

void
ClientManagerChild::MaybeStartTeardown()
{
  if (mTeardownStarted) {
    return;
  }
  mTeardownStarted = true;
  SendTeardown();
}

WorkerPrivate*
ClientManagerChild::GetWorkerPrivate() const
{
  if (!mWorkerHolderToken) {
    return nullptr;
  }
  return mWorkerHolderToken->GetWorkerPrivate();
}

} // namespace dom
} // namespace mozilla
