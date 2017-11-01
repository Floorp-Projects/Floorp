/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientSourceParent.h"

#include "ClientHandleParent.h"
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
{
}

ClientSourceParent::~ClientSourceParent()
{
}

} // namespace dom
} // namespace mozilla
