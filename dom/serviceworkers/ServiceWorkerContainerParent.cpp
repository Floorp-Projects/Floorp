/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerContainerParent.h"

#include "ServiceWorkerContainerProxy.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::IPCResult;

void
ServiceWorkerContainerParent::ActorDestroy(ActorDestroyReason aReason)
{
  if (mProxy) {
    mProxy->RevokeActor(this);
    mProxy = nullptr;
  }
}

IPCResult
ServiceWorkerContainerParent::RecvTeardown()
{
  Unused << Send__delete__(this);
  return IPC_OK();
}

ServiceWorkerContainerParent::ServiceWorkerContainerParent()
{
}

ServiceWorkerContainerParent::~ServiceWorkerContainerParent()
{
  MOZ_DIAGNOSTIC_ASSERT(!mProxy);
}

void
ServiceWorkerContainerParent::Init()
{
  mProxy = new ServiceWorkerContainerProxy(this);
}

} // namespace dom
} // namespace mozilla
