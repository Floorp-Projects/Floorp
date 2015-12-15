/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerManagerService.h"
#include "ServiceWorkerManagerParent.h"
#include "ServiceWorkerRegistrar.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/unused.h"

namespace mozilla {

using namespace ipc;

namespace dom {
namespace workers {

namespace {

ServiceWorkerManagerService* sInstance = nullptr;

struct NotifySoftUpdateData
{
  RefPtr<ServiceWorkerManagerParent> mParent;
  RefPtr<ContentParent> mContentParent;

  ~NotifySoftUpdateData()
  {
    MOZ_ASSERT(!mContentParent);
  }
};

class NotifySoftUpdateIfPrincipalOkRunnable final : public nsRunnable
{
public:
  NotifySoftUpdateIfPrincipalOkRunnable(
      nsAutoPtr<nsTArray<NotifySoftUpdateData>>& aData,
      const PrincipalOriginAttributes& aOriginAttributes,
      const nsAString& aScope)
    : mData(aData)
    , mOriginAttributes(aOriginAttributes)
    , mScope(aScope)
    , mBackgroundThread(NS_GetCurrentThread())
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();

    MOZ_ASSERT(mData && !aData);
    MOZ_ASSERT(mBackgroundThread);
  }

  NS_IMETHODIMP
  Run() override
  {
    if (NS_IsMainThread()) {
      for (uint32_t i = 0; i < mData->Length(); ++i) {
        NotifySoftUpdateData& data = mData->ElementAt(i);
        nsTArray<TabContext> contextArray =
          data.mContentParent->GetManagedTabContext();
        // mContentParent needs to be released in the main thread.
        data.mContentParent = nullptr;
        // We only send the notification about the soft update to the
        // tabs/apps with the same appId and inBrowser values.
        // Sending a notification to the wrong process will make the process
        // to be killed.
        for (uint32_t j = 0; j < contextArray.Length(); ++j) {
          if ((contextArray[j].OwnOrContainingAppId() == mOriginAttributes.mAppId) &&
              (contextArray[j].IsBrowserElement() == mOriginAttributes.mInBrowser)) {
            continue;
          }
          // Array entries with no mParent won't receive any notification.
          data.mParent = nullptr;
        }
      }
      nsresult rv = mBackgroundThread->Dispatch(this, NS_DISPATCH_NORMAL);
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(rv));
      return NS_OK;
    }

    AssertIsOnBackgroundThread();

    for (uint32_t i = 0; i < mData->Length(); ++i) {
      NotifySoftUpdateData& data = mData->ElementAt(i);
      MOZ_ASSERT(!(data.mContentParent));
      ServiceWorkerManagerParent* parent = data.mParent;
      if (parent && !parent->ActorDestroyed()) {
        Unused << parent->SendNotifySoftUpdate(mOriginAttributes, mScope);
      }
    }
    return NS_OK;
  }

private:
  nsAutoPtr<nsTArray<NotifySoftUpdateData>> mData;
  PrincipalOriginAttributes mOriginAttributes;
  nsString mScope;
  nsCOMPtr<nsIThread> mBackgroundThread;
};

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
                                      const PrincipalOriginAttributes& aOriginAttributes,
                                      const nsAString& aScope)
{
  AssertIsOnBackgroundThread();

  nsAutoPtr<nsTArray<NotifySoftUpdateData>> notifySoftUpdateDataArray(
      new nsTArray<NotifySoftUpdateData>());
  DebugOnly<bool> parentFound = false;
  for (auto iter = mAgents.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<ServiceWorkerManagerParent> parent = iter.Get()->GetKey();
    MOZ_ASSERT(parent);

#ifdef DEBUG
    if (parent->ID() == aParentID) {
      parentFound = true;
    }
#endif

    RefPtr<ContentParent> contentParent = parent->GetContentParent();

    // If the ContentParent is null we are dealing with a same-process actor.
    if (!contentParent) {
      Unused << parent->SendNotifySoftUpdate(aOriginAttributes,
                                             nsString(aScope));
      continue;
    }

    NotifySoftUpdateData* data = notifySoftUpdateDataArray->AppendElement();
    data->mContentParent.swap(contentParent);
    data->mParent.swap(parent);
  }

  if (notifySoftUpdateDataArray->IsEmpty()) {
    return;
  }

  RefPtr<NotifySoftUpdateIfPrincipalOkRunnable> runnable =
    new NotifySoftUpdateIfPrincipalOkRunnable(notifySoftUpdateDataArray,
                                              aOriginAttributes, aScope);
  MOZ_ASSERT(!notifySoftUpdateDataArray);
  nsresult rv = NS_DispatchToMainThread(runnable);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(rv));

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

} // namespace workers
} // namespace dom
} // namespace mozilla
