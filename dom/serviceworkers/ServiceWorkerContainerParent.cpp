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

IPCResult
ServiceWorkerContainerParent::RecvRegister(const IPCClientInfo& aClientInfo,
                                           const nsCString& aScopeURL,
                                           const nsCString& aScriptURL,
                                           const ServiceWorkerUpdateViaCache& aUpdateViaCache,
                                           RegisterResolver&& aResolver)
{
  if (!mProxy) {
    aResolver(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return IPC_OK();
  }

  mProxy->Register(ClientInfo(aClientInfo), aScopeURL, aScriptURL, aUpdateViaCache)->Then(
    GetCurrentThreadSerialEventTarget(), __func__,
    [aResolver] (const ServiceWorkerRegistrationDescriptor& aDescriptor) {
      aResolver(aDescriptor.ToIPC());
    }, [aResolver] (const CopyableErrorResult& aResult) {
      aResolver(aResult);
    });

  return IPC_OK();
}

IPCResult
ServiceWorkerContainerParent::RecvGetRegistration(const IPCClientInfo& aClientInfo,
                                                  const nsCString& aURL,
                                                  GetRegistrationResolver&& aResolver)
{
  if (!mProxy) {
    aResolver(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return IPC_OK();
  }

  mProxy->GetRegistration(ClientInfo(aClientInfo), aURL)->Then(
    GetCurrentThreadSerialEventTarget(), __func__,
    [aResolver] (const ServiceWorkerRegistrationDescriptor& aDescriptor) {
      aResolver(aDescriptor.ToIPC());
    }, [aResolver] (const CopyableErrorResult& aResult) {
      aResolver(aResult);
    });

  return IPC_OK();
}

IPCResult
ServiceWorkerContainerParent::RecvGetRegistrations(const IPCClientInfo& aClientInfo,
                                                   GetRegistrationsResolver&& aResolver)
{
  if (!mProxy) {
    aResolver(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return IPC_OK();
  }

  mProxy->GetRegistrations(ClientInfo(aClientInfo))->Then(
    GetCurrentThreadSerialEventTarget(), __func__,
    [aResolver] (const nsTArray<ServiceWorkerRegistrationDescriptor>& aList) {
      IPCServiceWorkerRegistrationDescriptorList ipcList;
      for (auto& desc : aList) {
        ipcList.values().AppendElement(desc.ToIPC());
      }
      aResolver(std::move(ipcList));
    }, [aResolver] (const CopyableErrorResult& aResult) {
      aResolver(aResult);
    });

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
