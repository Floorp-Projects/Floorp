/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageNotifierService.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace dom {

namespace {

// This boolean is used to avoid the creation of the service after been
// distroyed on shutdown.
bool gStorageShuttingDown = false;

StaticRefPtr<StorageNotifierService> gStorageNotifierService;

}  // namespace

/* static */
StorageNotifierService* StorageNotifierService::GetOrCreate() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!gStorageNotifierService && !gStorageShuttingDown) {
    gStorageNotifierService = new StorageNotifierService();
    ClearOnShutdown(&gStorageNotifierService);
  }

  return gStorageNotifierService;
}

StorageNotifierService::StorageNotifierService() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gStorageNotifierService);
}

StorageNotifierService::~StorageNotifierService() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gStorageNotifierService);
  gStorageShuttingDown = true;
}

/* static */
void StorageNotifierService::Broadcast(StorageEvent* aEvent,
                                       const char16_t* aStorageType,
                                       bool aPrivateBrowsing,
                                       bool aImmediateDispatch) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<StorageNotifierService> service = gStorageNotifierService;
  if (!service) {
    return;
  }

  RefPtr<StorageEvent> event = aEvent;

  nsTObserverArray<RefPtr<StorageNotificationObserver>>::ForwardIterator iter(
      service->mObservers);

  while (iter.HasMore()) {
    RefPtr<StorageNotificationObserver> observer = iter.GetNext();

    // Enforce that the source storage area's private browsing state matches
    // this window's state.  These flag checks and their maintenance independent
    // from the principal's OriginAttributes matter because chrome docshells
    // that are part of private browsing windows can be private browsing without
    // having their OriginAttributes set (because they have the system
    // principal).
    if (aPrivateBrowsing != observer->IsPrivateBrowsing()) {
      continue;
    }

    // No reasons to continue if the principal of the event doesn't match with
    // the window's one.
    if (!StorageUtils::PrincipalsEqual(aEvent->GetPrincipal(),
                                       observer->GetEffectiveStoragePrincipal())) {
      continue;
    }

    RefPtr<Runnable> r = NS_NewRunnableFunction(
        "StorageNotifierService::Broadcast",
        [observer, event, aStorageType, aPrivateBrowsing]() {
          observer->ObserveStorageNotification(event, aStorageType,
                                               aPrivateBrowsing);
        });

    if (aImmediateDispatch) {
      r->Run();
    } else {
      nsCOMPtr<nsIEventTarget> et = observer->GetEventTarget();
      if (et) {
        et->Dispatch(r.forget());
      }
    }
  }
}

void StorageNotifierService::Register(StorageNotificationObserver* aObserver) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aObserver);
  MOZ_ASSERT(!mObservers.Contains(aObserver));

  mObservers.AppendElement(aObserver);
}

void StorageNotifierService::Unregister(
    StorageNotificationObserver* aObserver) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aObserver);

  // No assertion about mObservers containing aObserver because window calls
  // this method multiple times.

  mObservers.RemoveElement(aObserver);
}

}  // namespace dom
}  // namespace mozilla
