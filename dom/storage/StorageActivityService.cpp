/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageActivityService.h"

#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIMutableArray.h"
#include "nsIObserverService.h"
#include "nsIPrincipal.h"
#include "nsSupportsPrimitives.h"
#include "nsXPCOM.h"

// This const is used to know when origin activities should be purged because
// too old. This value should be in sync with what the UI needs.
#define TIME_MAX_SECS 86400 /* 24 hours */

namespace mozilla {
namespace dom {

static StaticRefPtr<StorageActivityService> gStorageActivityService;
static bool gStorageActivityShutdown = false;

/* static */
void StorageActivityService::SendActivity(nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aPrincipal || BasePrincipal::Cast(aPrincipal)->Kind() !=
                         BasePrincipal::eContentPrincipal) {
    // Only content principals.
    return;
  }

  RefPtr<StorageActivityService> service = GetOrCreate();
  if (NS_WARN_IF(!service)) {
    return;
  }

  service->SendActivityInternal(aPrincipal);
}

/* static */
void StorageActivityService::SendActivity(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  if (aPrincipalInfo.type() !=
      mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) {
    // only content principal.
    return;
  }

  RefPtr<Runnable> r = NS_NewRunnableFunction(
      "StorageActivityService::SendActivity", [aPrincipalInfo]() {
        MOZ_ASSERT(NS_IsMainThread());

        auto principalOrErr =
            mozilla::ipc::PrincipalInfoToPrincipal(aPrincipalInfo);

        if (principalOrErr.isOk()) {
          nsCOMPtr<nsIPrincipal> principal = principalOrErr.unwrap();
          StorageActivityService::SendActivity(principal);
        } else {
          NS_WARNING(
              "Could not obtain principal from "
              "mozilla::ipc::PrincipalInfoToPrincipal");
        }
      });

  SchedulerGroup::Dispatch(TaskCategory::Other, r.forget());
}

/* static */
void StorageActivityService::SendActivity(const nsACString& aOrigin) {
  MOZ_ASSERT(XRE_IsParentProcess());

  nsCString origin;
  origin.Assign(aOrigin);

  RefPtr<Runnable> r = NS_NewRunnableFunction(
      "StorageActivityService::SendActivity", [origin]() {
        MOZ_ASSERT(NS_IsMainThread());

        RefPtr<StorageActivityService> service = GetOrCreate();
        if (NS_WARN_IF(!service)) {
          return;
        }

        service->SendActivityInternal(origin);
      });

  if (NS_IsMainThread()) {
    Unused << r->Run();
  } else {
    SchedulerGroup::Dispatch(TaskCategory::Other, r.forget());
  }
}

/* static */
already_AddRefed<StorageActivityService> StorageActivityService::GetOrCreate() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!gStorageActivityService && !gStorageActivityShutdown) {
    RefPtr<StorageActivityService> service = new StorageActivityService();

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (NS_WARN_IF(!obs)) {
      return nullptr;
    }

    nsresult rv =
        obs->AddObserver(service, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    gStorageActivityService = service;
  }

  RefPtr<StorageActivityService> service = gStorageActivityService;
  return service.forget();
}

StorageActivityService::StorageActivityService() {
  MOZ_ASSERT(NS_IsMainThread());
}

StorageActivityService::~StorageActivityService() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mTimer);
}

void StorageActivityService::SendActivityInternal(nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(BasePrincipal::Cast(aPrincipal)->Kind() ==
             BasePrincipal::eContentPrincipal);

  if (!XRE_IsParentProcess()) {
    SendActivityToParent(aPrincipal);
    return;
  }

  nsAutoCString origin;
  nsresult rv = aPrincipal->GetOrigin(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  SendActivityInternal(origin);
}

void StorageActivityService::SendActivityInternal(const nsACString& aOrigin) {
  MOZ_ASSERT(XRE_IsParentProcess());

  mActivities.InsertOrUpdate(aOrigin, PR_Now());
  MaybeStartTimer();
}

void StorageActivityService::SendActivityToParent(nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!XRE_IsParentProcess());

  ::mozilla::ipc::PBackgroundChild* actor =
      ::mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actor)) {
    return;
  }

  mozilla::ipc::PrincipalInfo principalInfo;
  nsresult rv =
      mozilla::ipc::PrincipalToPrincipalInfo(aPrincipal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  actor->SendStorageActivity(principalInfo);
}

NS_IMETHODIMP
StorageActivityService::Observe(nsISupports* aSubject, const char* aTopic,
                                const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID));

  MaybeStopTimer();

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }

  gStorageActivityShutdown = true;
  gStorageActivityService = nullptr;
  return NS_OK;
}

void StorageActivityService::MaybeStartTimer() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mTimer) {
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    mTimer->InitWithCallback(this, 1000 * 5 * 60 /* any 5 minutes */,
                             nsITimer::TYPE_REPEATING_SLACK);
  }
}

void StorageActivityService::MaybeStopTimer() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

NS_IMETHODIMP
StorageActivityService::Notify(nsITimer* aTimer) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mTimer == aTimer);

  uint64_t now = PR_Now();

  for (auto iter = mActivities.Iter(); !iter.Done(); iter.Next()) {
    if ((now - iter.UserData()) / PR_USEC_PER_SEC > TIME_MAX_SECS) {
      iter.Remove();
    }
  }

  // If no activities, let's stop the timer.
  if (mActivities.Count() == 0) {
    MaybeStopTimer();
  }

  return NS_OK;
}

NS_IMETHODIMP
StorageActivityService::GetName(nsACString& aName) {
  aName.AssignLiteral("StorageActivityService");
  return NS_OK;
}

NS_IMETHODIMP
StorageActivityService::GetActiveOrigins(PRTime aFrom, PRTime aTo,
                                         nsIArray** aRetval) {
  uint64_t now = PR_Now();
  if (((now - aFrom) / PR_USEC_PER_SEC) > TIME_MAX_SECS || aFrom >= aTo) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> devices =
      do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (const auto& activityEntry : mActivities) {
    if (activityEntry.GetData() >= aFrom && activityEntry.GetData() <= aTo) {
      RefPtr<BasePrincipal> principal =
          BasePrincipal::CreateContentPrincipal(activityEntry.GetKey());
      MOZ_ASSERT(principal);

      rv = devices->AppendElement(principal);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  devices.forget(aRetval);
  return NS_OK;
}

NS_IMETHODIMP
StorageActivityService::MoveOriginInTime(nsIPrincipal* aPrincipal,
                                         PRTime aWhen) {
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString origin;
  nsresult rv = aPrincipal->GetOrigin(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mActivities.InsertOrUpdate(origin, aWhen / PR_USEC_PER_SEC);
  return NS_OK;
}

NS_IMETHODIMP
StorageActivityService::TestOnlyReset() {
  mActivities.Clear();
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(StorageActivityService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStorageActivityService)
  NS_INTERFACE_MAP_ENTRY(nsIStorageActivityService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(StorageActivityService)
NS_IMPL_RELEASE(StorageActivityService)

}  // namespace dom
}  // namespace mozilla
