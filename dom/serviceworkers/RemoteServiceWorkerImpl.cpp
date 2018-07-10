/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteServiceWorkerImpl.h"

#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ClientState.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "ServiceWorkerChild.h"
#include "ServiceWorkerCloneData.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::BackgroundChild;
using mozilla::ipc::PBackgroundChild;

RemoteServiceWorkerImpl::~RemoteServiceWorkerImpl()
{
  MOZ_DIAGNOSTIC_ASSERT(!mWorker);
  Shutdown();
}

void
RemoteServiceWorkerImpl::Shutdown()
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
RemoteServiceWorkerImpl::AddServiceWorker(ServiceWorker* aWorker)
{
  NS_ASSERT_OWNINGTHREAD(RemoteServiceWorkerImpl);
  MOZ_DIAGNOSTIC_ASSERT(!mWorker);
  MOZ_DIAGNOSTIC_ASSERT(aWorker);
  mWorker = aWorker;
}

void
RemoteServiceWorkerImpl::RemoveServiceWorker(ServiceWorker* aWorker)
{
  NS_ASSERT_OWNINGTHREAD(RemoteServiceWorkerImpl);
  MOZ_DIAGNOSTIC_ASSERT(mWorker);
  MOZ_DIAGNOSTIC_ASSERT(aWorker == mWorker);
  mWorker = nullptr;
}

void
RemoteServiceWorkerImpl::GetRegistration(ServiceWorkerRegistrationCallback&& aSuccessCB,
                                         ServiceWorkerFailureCallback&& aFailureCB)
{
  // TODO
}

void
RemoteServiceWorkerImpl::PostMessage(RefPtr<ServiceWorkerCloneData>&& aData,
                                     const ClientInfo& aClientInfo,
                                     const ClientState& aClientState)
{
  NS_ASSERT_OWNINGTHREAD(RemoteServiceWorkerImpl);
  if (!mActor) {
    return;
  }

  ClonedMessageData data;
  if (!aData->BuildClonedMessageDataForBackgroundChild(mActor->Manager(),
                                                       data)) {
    return;
  }

  mActor->SendPostMessage(data, ClientInfoAndState(aClientInfo.ToIPC(),
                                                   aClientState.ToIPC()));
}

RemoteServiceWorkerImpl::RemoteServiceWorkerImpl(const ServiceWorkerDescriptor& aDescriptor)
  : mActor(nullptr)
  , mWorker(nullptr)
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

  ServiceWorkerChild* actor = new ServiceWorkerChild(workerHolderToken);
  PServiceWorkerChild* sentActor =
    parentActor->SendPServiceWorkerConstructor(actor, aDescriptor.ToIPC());
  if (NS_WARN_IF(!sentActor)) {
    Shutdown();
    return;
  }
  MOZ_DIAGNOSTIC_ASSERT(sentActor == actor);

  mActor = actor;
  mActor->SetOwner(this);
}

void
RemoteServiceWorkerImpl::RevokeActor(ServiceWorkerChild* aActor)
{
  MOZ_DIAGNOSTIC_ASSERT(mActor);
  MOZ_DIAGNOSTIC_ASSERT(mActor == aActor);
  mActor->RevokeOwner(this);
  mActor = nullptr;

  mShutdown = true;
}

} // namespace dom
} // namespace mozilla
