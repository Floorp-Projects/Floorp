/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteServiceWorkerContainerImpl.h"

#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "ServiceWorkerContainerChild.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::BackgroundChild;
using mozilla::ipc::PBackgroundChild;
using mozilla::ipc::ResponseRejectReason;

RemoteServiceWorkerContainerImpl::~RemoteServiceWorkerContainerImpl()
{
  Shutdown();
  MOZ_DIAGNOSTIC_ASSERT(!mOuter);
}

void
RemoteServiceWorkerContainerImpl::Shutdown()
{
  if (mShutdown) {
    return;
  }
  mShutdown = true;

  if (mActor) {
    mActor->RevokeOwner(this);
    mActor->MaybeStartTeardown();
    mActor = nullptr;
  }
}

void
RemoteServiceWorkerContainerImpl::AddContainer(ServiceWorkerContainer* aOuter)
{
  MOZ_DIAGNOSTIC_ASSERT(aOuter);
  MOZ_DIAGNOSTIC_ASSERT(!mOuter);
  mOuter = aOuter;
}

void
RemoteServiceWorkerContainerImpl::RemoveContainer(ServiceWorkerContainer* aOuter)
{
  MOZ_DIAGNOSTIC_ASSERT(aOuter);
  MOZ_DIAGNOSTIC_ASSERT(mOuter == aOuter);
  mOuter = nullptr;
}

void
RemoteServiceWorkerContainerImpl::Register(const ClientInfo& aClientInfo,
                                           const nsACString& aScopeURL,
                                           const nsACString& aScriptURL,
                                           ServiceWorkerUpdateViaCache aUpdateViaCache,
                                           ServiceWorkerRegistrationCallback&& aSuccessCB,
                                           ServiceWorkerFailureCallback&& aFailureCB) const
{
  if (!mActor) {
    aFailureCB(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return;
  }

  mActor->SendRegister(
    aClientInfo.ToIPC(), nsCString(aScopeURL), nsCString(aScriptURL),
    aUpdateViaCache,
    [successCB = std::move(aSuccessCB), aFailureCB]
    (const IPCServiceWorkerRegistrationDescriptorOrCopyableErrorResult& aResult) {
      if (aResult.type() == IPCServiceWorkerRegistrationDescriptorOrCopyableErrorResult::TCopyableErrorResult) {
        // application layer error
        auto& rv = aResult.get_CopyableErrorResult();
        MOZ_DIAGNOSTIC_ASSERT(rv.Failed());
        aFailureCB(CopyableErrorResult(rv));
        return;
      }
      // success
      auto& ipcDesc = aResult.get_IPCServiceWorkerRegistrationDescriptor();
      successCB(ServiceWorkerRegistrationDescriptor(ipcDesc));
    }, [aFailureCB] (ResponseRejectReason aReason) {
      // IPC layer error
      aFailureCB(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    });
}

void
RemoteServiceWorkerContainerImpl::GetRegistration(const ClientInfo& aClientInfo,
                                                  const nsACString& aURL,
                                                  ServiceWorkerRegistrationCallback&& aSuccessCB,
                                                  ServiceWorkerFailureCallback&& aFailureCB) const
{
  if (!mActor) {
    aFailureCB(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return;
  }

  mActor->SendGetRegistration(aClientInfo.ToIPC(), nsCString(aURL),
    [successCB = std::move(aSuccessCB), aFailureCB]
    (const IPCServiceWorkerRegistrationDescriptorOrCopyableErrorResult& aResult) {
      if (aResult.type() == IPCServiceWorkerRegistrationDescriptorOrCopyableErrorResult::TCopyableErrorResult) {
        auto& rv = aResult.get_CopyableErrorResult();
        // If rv is a failure then this is an application layer error.  Note,
        // though, we also reject with NS_OK to indicate that we just didn't
        // find a registration.
        aFailureCB(CopyableErrorResult(rv));
        return;
      }
      // success
      auto& ipcDesc = aResult.get_IPCServiceWorkerRegistrationDescriptor();
      successCB(ServiceWorkerRegistrationDescriptor(ipcDesc));
    }, [aFailureCB] (ResponseRejectReason aReason) {
      // IPC layer error
      aFailureCB(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    });
}

void
RemoteServiceWorkerContainerImpl::GetRegistrations(const ClientInfo& aClientInfo,
                                                   ServiceWorkerRegistrationListCallback&& aSuccessCB,
                                                   ServiceWorkerFailureCallback&& aFailureCB) const
{
  if (!mActor) {
    aFailureCB(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    return;
  }

  mActor->SendGetRegistrations(aClientInfo.ToIPC(),
   [successCB = std::move(aSuccessCB), aFailureCB]
   (const IPCServiceWorkerRegistrationDescriptorListOrCopyableErrorResult& aResult) {
      if (aResult.type() == IPCServiceWorkerRegistrationDescriptorListOrCopyableErrorResult::TCopyableErrorResult) {
        // application layer error
        auto& rv = aResult.get_CopyableErrorResult();
        MOZ_DIAGNOSTIC_ASSERT(rv.Failed());
        aFailureCB(CopyableErrorResult(rv));
        return;
      }
      // success
      auto& ipcList = aResult.get_IPCServiceWorkerRegistrationDescriptorList();
      nsTArray<ServiceWorkerRegistrationDescriptor> list(ipcList.values().Length());
      for (auto& ipcDesc : ipcList.values()) {
        list.AppendElement(ServiceWorkerRegistrationDescriptor(ipcDesc));
      }
      successCB(std::move(list));
    }, [aFailureCB] (ResponseRejectReason aReason) {
      // IPC layer error
      aFailureCB(CopyableErrorResult(NS_ERROR_DOM_INVALID_STATE_ERR));
    });
}

void
RemoteServiceWorkerContainerImpl::GetReady(const ClientInfo& aClientInfo,
                                           ServiceWorkerRegistrationCallback&& aSuccessCB,
                                           ServiceWorkerFailureCallback&& aFailureCB) const
{
  // TODO
}

RemoteServiceWorkerContainerImpl::RemoteServiceWorkerContainerImpl()
  : mActor(nullptr)
  , mOuter(nullptr)
  , mShutdown(false)
{
  PBackgroundChild* parentActor = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!parentActor)) {
    Shutdown();
    return;
  }

  RefPtr<WorkerHolderToken> workerHolderToken;
  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_DIAGNOSTIC_ASSERT(workerPrivate);

    workerHolderToken =
      WorkerHolderToken::Create(workerPrivate, Terminating,
                                WorkerHolderToken::AllowIdleShutdownStart);

    if (NS_WARN_IF(!workerHolderToken)) {
      Shutdown();
      return;
    }
  }

  ServiceWorkerContainerChild* actor =
    new ServiceWorkerContainerChild(workerHolderToken);
  PServiceWorkerContainerChild* sentActor =
    parentActor->SendPServiceWorkerContainerConstructor(actor);
  if (NS_WARN_IF(!sentActor)) {
    Shutdown();
    return;
  }
  MOZ_DIAGNOSTIC_ASSERT(sentActor == actor);

  mActor = actor;
  mActor->SetOwner(this);
}

void
RemoteServiceWorkerContainerImpl::RevokeActor(ServiceWorkerContainerChild* aActor)
{
  MOZ_DIAGNOSTIC_ASSERT(mActor);
  MOZ_DIAGNOSTIC_ASSERT(mActor == aActor);
  mActor->RevokeOwner(this);
  mActor = nullptr;

  mShutdown = true;
}

} // namespace dom
} // namespace mozilla
