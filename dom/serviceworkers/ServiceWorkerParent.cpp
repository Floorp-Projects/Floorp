/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerParent.h"

#include "ServiceWorkerProxy.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::IPCResult;

void
ServiceWorkerParent::ActorDestroy(ActorDestroyReason aReason)
{
  if (mProxy) {
    mProxy->RevokeActor(this);
    mProxy = nullptr;
  }
}

IPCResult
ServiceWorkerParent::RecvTeardown()
{
  MaybeSendDelete();
  return IPC_OK();
}

ServiceWorkerParent::ServiceWorkerParent()
  : mDeleteSent(false)
{
}

ServiceWorkerParent::~ServiceWorkerParent()
{
  MOZ_DIAGNOSTIC_ASSERT(!mProxy);
}

void
ServiceWorkerParent::Init(const IPCServiceWorkerDescriptor& aDescriptor)
{
  MOZ_DIAGNOSTIC_ASSERT(!mProxy);
  mProxy = new ServiceWorkerProxy(ServiceWorkerDescriptor(aDescriptor));
  mProxy->Init(this);
}

void
ServiceWorkerParent::MaybeSendDelete()
{
  if (mDeleteSent) {
    return;
  }
  mDeleteSent = true;
  Unused << Send__delete__(this);
}

} // namespace dom
} // namespace mozilla
