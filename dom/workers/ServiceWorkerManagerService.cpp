/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerManagerService.h"
#include "ServiceWorkerManagerParent.h"
#include "ServiceWorkerRegistrar.h"
#include "ServiceWorkerUpdaterParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/Unused.h"
#include "nsAutoPtr.h"

namespace mozilla {

using namespace ipc;

namespace dom {
namespace workers {

namespace {

ServiceWorkerManagerService* sInstance = nullptr;

} // namespace

ServiceWorkerManagerService::ServiceWorkerManagerService()
{
  AssertIsOnBackgroundThread();

  // sInstance is a raw ServiceWorkerManagerService*.
  MOZ_ASSERT(!sInstance);
  sInstance = this;
}

ServiceWorkerManagerService::~ServiceWorkerManagerService()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(sInstance == this);
  MOZ_ASSERT(mAgents.Count() == 0);

  sInstance = nullptr;
}

/* static */ already_AddRefed<ServiceWorkerManagerService>
ServiceWorkerManagerService::Get()
{
  AssertIsOnBackgroundThread();

  RefPtr<ServiceWorkerManagerService> instance = sInstance;
  return instance.forget();
}

/* static */ already_AddRefed<ServiceWorkerManagerService>
ServiceWorkerManagerService::GetOrCreate()
{
  AssertIsOnBackgroundThread();

  RefPtr<ServiceWorkerManagerService> instance = sInstance;
  if (!instance) {
    instance = new ServiceWorkerManagerService();
  }
  return instance.forget();
}

void
ServiceWorkerManagerService::RegisterActor(ServiceWorkerManagerParent* aParent)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(!mAgents.Contains(aParent));

  mAgents.PutEntry(aParent);
}

void
ServiceWorkerManagerService::UnregisterActor(ServiceWorkerManagerParent* aParent)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(mAgents.Contains(aParent));

  mAgents.RemoveEntry(aParent);
}

void
ServiceWorkerManagerService::PropagateRegistration(
                                           uint64_t aParentID,
                                           ServiceWorkerRegistrationData& aData)
{
  AssertIsOnBackgroundThread();

  DebugOnly<bool> parentFound = false;
  for (auto iter = mAgents.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<ServiceWorkerManagerParent> parent = iter.Get()->GetKey();
    MOZ_ASSERT(parent);

    if (parent->ID() != aParentID) {
      Unused << parent->SendNotifyRegister(aData);
#ifdef DEBUG
    } else {
      parentFound = true;
#endif
    }
  }

#ifdef DEBUG
  MOZ_ASSERT(parentFound);
#endif
}

void
ServiceWorkerManagerService::PropagateSoftUpdate(
                                      uint64_t aParentID,
                                      const OriginAttributes& aOriginAttributes,
                                      const nsAString& aScope)
{
  AssertIsOnBackgroundThread();

  DebugOnly<bool> parentFound = false;
  for (auto iter = mAgents.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<ServiceWorkerManagerParent> parent = iter.Get()->GetKey();
    MOZ_ASSERT(parent);

    nsString scope(aScope);
    Unused << parent->SendNotifySoftUpdate(aOriginAttributes,
                                           scope);

#ifdef DEBUG
    if (parent->ID() == aParentID) {
      parentFound = true;
    }
#endif
  }

#ifdef DEBUG
  MOZ_ASSERT(parentFound);
#endif
}

void
ServiceWorkerManagerService::PropagateUnregister(
                                            uint64_t aParentID,
                                            const PrincipalInfo& aPrincipalInfo,
                                            const nsAString& aScope)
{
  AssertIsOnBackgroundThread();

  RefPtr<dom::ServiceWorkerRegistrar> service =
    dom::ServiceWorkerRegistrar::Get();
  MOZ_ASSERT(service);

  // It's possible that we don't have any ServiceWorkerManager managing this
  // scope but we still need to unregister it from the ServiceWorkerRegistrar.
  service->UnregisterServiceWorker(aPrincipalInfo,
                                   NS_ConvertUTF16toUTF8(aScope));

  DebugOnly<bool> parentFound = false;
  for (auto iter = mAgents.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<ServiceWorkerManagerParent> parent = iter.Get()->GetKey();
    MOZ_ASSERT(parent);

    if (parent->ID() != aParentID) {
      nsString scope(aScope);
      Unused << parent->SendNotifyUnregister(aPrincipalInfo, scope);
#ifdef DEBUG
    } else {
      parentFound = true;
#endif
    }
  }

#ifdef DEBUG
  MOZ_ASSERT(parentFound);
#endif
}

void
ServiceWorkerManagerService::PropagateRemove(uint64_t aParentID,
                                             const nsACString& aHost)
{
  AssertIsOnBackgroundThread();

  DebugOnly<bool> parentFound = false;
  for (auto iter = mAgents.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<ServiceWorkerManagerParent> parent = iter.Get()->GetKey();
    MOZ_ASSERT(parent);

    if (parent->ID() != aParentID) {
      nsCString host(aHost);
      Unused << parent->SendNotifyRemove(host);
#ifdef DEBUG
    } else {
      parentFound = true;
#endif
    }
  }

#ifdef DEBUG
  MOZ_ASSERT(parentFound);
#endif
}

void
ServiceWorkerManagerService::PropagateRemoveAll(uint64_t aParentID)
{
  AssertIsOnBackgroundThread();

  RefPtr<dom::ServiceWorkerRegistrar> service =
    dom::ServiceWorkerRegistrar::Get();
  MOZ_ASSERT(service);

  service->RemoveAll();

  DebugOnly<bool> parentFound = false;
  for (auto iter = mAgents.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<ServiceWorkerManagerParent> parent = iter.Get()->GetKey();
    MOZ_ASSERT(parent);

    if (parent->ID() != aParentID) {
      Unused << parent->SendNotifyRemoveAll();
#ifdef DEBUG
    } else {
      parentFound = true;
#endif
    }
  }

#ifdef DEBUG
  MOZ_ASSERT(parentFound);
#endif
}

void
ServiceWorkerManagerService::ProcessUpdaterActor(ServiceWorkerUpdaterParent* aActor,
                                                 const OriginAttributes& aOriginAttributes,
                                                 const nsACString& aScope,
                                                 uint64_t aParentId)
{
  AssertIsOnBackgroundThread();

  nsAutoCString suffix;
  aOriginAttributes.CreateSuffix(suffix);

  nsCString scope(aScope);
  scope.Append(suffix);

  for (uint32_t i = 0; i < mPendingUpdaterActors.Length(); ++i) {
    // We already have an actor doing this update on another process.
    if (mPendingUpdaterActors[i].mScope.Equals(scope) &&
        mPendingUpdaterActors[i].mParentId != aParentId) {
      Unused << aActor->SendProceed(false);
      return;
    }
  }

  if (aActor->Proceed(this)) {
    PendingUpdaterActor* pua = mPendingUpdaterActors.AppendElement();
    pua->mActor = aActor;
    pua->mScope = scope;
    pua->mParentId = aParentId;
  }
}

void
ServiceWorkerManagerService::UpdaterActorDestroyed(ServiceWorkerUpdaterParent* aActor)
{
  for (uint32_t i = 0; i < mPendingUpdaterActors.Length(); ++i) {
    // We already have an actor doing the update for this scope.
    if (mPendingUpdaterActors[i].mActor == aActor) {
      mPendingUpdaterActors.RemoveElementAt(i);
      return;
    }
  }

  MOZ_CRASH("The actor should be found");
}

} // namespace workers
} // namespace dom
} // namespace mozilla
