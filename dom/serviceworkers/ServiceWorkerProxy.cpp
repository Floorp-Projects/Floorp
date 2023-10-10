/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerProxy.h"
#include "ServiceWorkerCloneData.h"
#include "ServiceWorkerManager.h"
#include "ServiceWorkerParent.h"

#include "mozilla/SchedulerGroup.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/ClientState.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "ServiceWorkerInfo.h"

namespace mozilla::dom {

using mozilla::ipc::AssertIsOnBackgroundThread;

ServiceWorkerProxy::~ServiceWorkerProxy() {
  // Any thread
  MOZ_DIAGNOSTIC_ASSERT(!mActor);
  MOZ_DIAGNOSTIC_ASSERT(!mInfo);
}

void ServiceWorkerProxy::MaybeShutdownOnBGThread() {
  AssertIsOnBackgroundThread();
  if (!mActor) {
    return;
  }
  mActor->MaybeSendDelete();
}

void ServiceWorkerProxy::InitOnMainThread() {
  AssertIsOnMainThread();

  auto scopeExit = MakeScopeExit([&] { MaybeShutdownOnMainThread(); });

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  NS_ENSURE_TRUE_VOID(swm);

  RefPtr<ServiceWorkerRegistrationInfo> reg =
      swm->GetRegistration(mDescriptor.PrincipalInfo(), mDescriptor.Scope());
  NS_ENSURE_TRUE_VOID(reg);

  RefPtr<ServiceWorkerInfo> info = reg->GetByDescriptor(mDescriptor);
  NS_ENSURE_TRUE_VOID(info);

  scopeExit.release();

  mInfo = new nsMainThreadPtrHolder<ServiceWorkerInfo>(
      "ServiceWorkerProxy::mInfo", info);
}

void ServiceWorkerProxy::MaybeShutdownOnMainThread() {
  AssertIsOnMainThread();

  nsCOMPtr<nsIRunnable> r = NewRunnableMethod(
      __func__, this, &ServiceWorkerProxy::MaybeShutdownOnBGThread);

  MOZ_ALWAYS_SUCCEEDS(mEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL));
}

void ServiceWorkerProxy::StopListeningOnMainThread() {
  AssertIsOnMainThread();
  mInfo = nullptr;
}

ServiceWorkerProxy::ServiceWorkerProxy(
    const ServiceWorkerDescriptor& aDescriptor)
    : mEventTarget(GetCurrentSerialEventTarget()), mDescriptor(aDescriptor) {}

void ServiceWorkerProxy::Init(ServiceWorkerParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(aActor);
  MOZ_DIAGNOSTIC_ASSERT(!mActor);
  MOZ_DIAGNOSTIC_ASSERT(mEventTarget);

  mActor = aActor;

  // Note, this must be done from a separate Init() method and not in
  // the constructor.  If done from the constructor the runnable can
  // execute, complete, and release its reference before the constructor
  // returns.
  nsCOMPtr<nsIRunnable> r = NewRunnableMethod(
      "ServiceWorkerProxy::Init", this, &ServiceWorkerProxy::InitOnMainThread);
  MOZ_ALWAYS_SUCCEEDS(SchedulerGroup::Dispatch(r.forget()));
}

void ServiceWorkerProxy::RevokeActor(ServiceWorkerParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(mActor);
  MOZ_DIAGNOSTIC_ASSERT(mActor == aActor);
  mActor = nullptr;

  nsCOMPtr<nsIRunnable> r = NewRunnableMethod(
      __func__, this, &ServiceWorkerProxy::StopListeningOnMainThread);
  MOZ_ALWAYS_SUCCEEDS(SchedulerGroup::Dispatch(r.forget()));
}

void ServiceWorkerProxy::PostMessage(RefPtr<ServiceWorkerCloneData>&& aData,
                                     const ClientInfo& aClientInfo,
                                     const ClientState& aClientState) {
  AssertIsOnBackgroundThread();
  RefPtr<ServiceWorkerProxy> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__,
      [self, data = std::move(aData), aClientInfo, aClientState]() mutable {
        if (!self->mInfo) {
          return;
        }
        self->mInfo->PostMessage(std::move(data), aClientInfo, aClientState);
      });
  MOZ_ALWAYS_SUCCEEDS(SchedulerGroup::Dispatch(r.forget()));
}

}  // namespace mozilla::dom
