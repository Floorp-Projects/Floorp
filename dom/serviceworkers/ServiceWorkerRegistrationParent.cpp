/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistrationParent.h"

#include "ServiceWorkerRegistrationProxy.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::IPCResult;

void
ServiceWorkerRegistrationParent::ActorDestroy(ActorDestroyReason aReason)
{
  if (mProxy) {
    mProxy->RevokeActor(this);
    mProxy = nullptr;
  }
}

IPCResult
ServiceWorkerRegistrationParent::RecvTeardown()
{
  MaybeSendDelete();
  return IPC_OK();
}

IPCResult
ServiceWorkerRegistrationParent::RecvUpdate(UpdateResolver&& aResolver)
{
  if (!mProxy) {
    aResolver(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return IPC_OK();
  }

  mProxy->Update()->Then(GetCurrentThreadSerialEventTarget(), __func__,
    [aResolver] (const ServiceWorkerRegistrationDescriptor& aDescriptor) {
      aResolver(aDescriptor.ToIPC());
    }, [aResolver] (const CopyableErrorResult& aResult) {
      aResolver(aResult);
    });

  return IPC_OK();
}

ServiceWorkerRegistrationParent::ServiceWorkerRegistrationParent()
  : mDeleteSent(false)
{
}

ServiceWorkerRegistrationParent::~ServiceWorkerRegistrationParent()
{
  MOZ_DIAGNOSTIC_ASSERT(!mProxy);
}

void
ServiceWorkerRegistrationParent::Init(const IPCServiceWorkerRegistrationDescriptor& aDescriptor)
{
  MOZ_DIAGNOSTIC_ASSERT(!mProxy);
  mProxy = new ServiceWorkerRegistrationProxy(ServiceWorkerRegistrationDescriptor(aDescriptor));
  mProxy->Init(this);
}

void
ServiceWorkerRegistrationParent::MaybeSendDelete()
{
  if (mDeleteSent) {
    return;
  }
  mDeleteSent = true;
  Unused << Send__delete__(this);
}

} // namespace dom
} // namespace mozilla
