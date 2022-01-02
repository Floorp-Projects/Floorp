/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimelineConsumers.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ObservedDocShell.h"
#include "mozilla/Services.h"
#include "mozilla/TimelineMarker.h"
#include "jsapi.h"
#include "nsAppRunner.h"  // for XRE_IsContentProcess, XRE_IsParentProcess
#include "nsCRT.h"
#include "nsDocShell.h"
#include "nsIObserverService.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(TimelineConsumers, nsIObserver);

StaticMutex TimelineConsumers::sMutex;

// Manually manage this singleton's lifetime and destroy it before shutdown.
// This avoids the leakchecker detecting false-positive memory leaks when
// using automatic memory management (i.e. statically instantiating this
// singleton inside the `Get` method), which would automatically destroy it on
// application shutdown, but too late for the leakchecker. Sigh...
StaticRefPtr<TimelineConsumers> TimelineConsumers::sInstance;

// This flag makes sure the singleton never gets instantiated while a shutdown
// is in progress. This can actually happen, and `ClearOnShutdown` doesn't work
// in these cases.
Atomic<bool> TimelineConsumers::sInShutdown{false};

already_AddRefed<TimelineConsumers> TimelineConsumers::Get() {
  // Using this class is not supported yet for other processes other than
  // parent or content. To avoid accidental checks to methods like `IsEmpty`,
  // which would probably always be true in those cases, assert here.
  // Remember, there will be different singletons available to each process.

  // TODO: we have to avoid calling this function in socket process.
  MOZ_ASSERT(XRE_IsContentProcess() || XRE_IsParentProcess());

  // If we are shutting down, don't bother doing anything. Note: we can only
  // know whether or not we're in shutdown if we're instantiated.
  if (sInShutdown) {
    return nullptr;
  }

  RefPtr<TimelineConsumers> copy = sInstance.get();
  return copy.forget();
}

/* static */
void TimelineConsumers::Init() {
  MOZ_ASSERT(!sInstance);
  RefPtr<TimelineConsumers> instance = new TimelineConsumers();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    return;
  }
  if (NS_WARN_IF(NS_FAILED(
          obs->AddObserver(instance, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false)))) {
    return;
  }

  sInstance = instance;
  ClearOnShutdown(&sInstance);
}

bool TimelineConsumers::RemoveObservers() {
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    return false;
  }
  if (NS_WARN_IF(NS_FAILED(
          obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID)))) {
    return false;
  }
  return true;
}

nsresult TimelineConsumers::Observe(nsISupports* aSubject, const char* aTopic,
                                    const char16_t* aData) {
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    sInShutdown = true;
    RemoveObservers();
    return NS_OK;
  }

  MOZ_ASSERT(false, "TimelineConsumers got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

TimelineConsumers::TimelineConsumers() : mActiveConsumers(0) {}

void TimelineConsumers::AddConsumer(nsDocShell* aDocShell) {
  MOZ_ASSERT(NS_IsMainThread());
  StaticMutexAutoLock lock(
      sMutex);  // for `mActiveConsumers` and `mMarkersStores`.

  UniquePtr<ObservedDocShell>& observed = aDocShell->mObserved;
  MOZ_ASSERT(!observed);

  if (mActiveConsumers == 0) {
    JS::SetProfileTimelineRecordingEnabled(true);
  }
  mActiveConsumers++;

  ObservedDocShell* obsDocShell = new ObservedDocShell(aDocShell);
  MarkersStorage* storage = static_cast<MarkersStorage*>(obsDocShell);

  observed.reset(obsDocShell);
  mMarkersStores.insertFront(storage);
}

void TimelineConsumers::RemoveConsumer(nsDocShell* aDocShell) {
  MOZ_ASSERT(NS_IsMainThread());
  StaticMutexAutoLock lock(
      sMutex);  // for `mActiveConsumers` and `mMarkersStores`.

  UniquePtr<ObservedDocShell>& observed = aDocShell->mObserved;
  MOZ_ASSERT(observed);

  mActiveConsumers--;
  if (mActiveConsumers == 0) {
    JS::SetProfileTimelineRecordingEnabled(false);
  }

  // Clear all markers from the `mTimelineMarkers` store.
  observed.get()->ClearMarkers();
  // Remove self from the `mMarkersStores` store.
  observed.get()->remove();
  // Prepare for becoming a consumer later.
  observed.reset(nullptr);
}

bool TimelineConsumers::HasConsumer(nsIDocShell* aDocShell) {
  MOZ_ASSERT(NS_IsMainThread());
  return aDocShell ? aDocShell->GetRecordProfileTimelineMarkers() : false;
}

bool TimelineConsumers::IsEmpty() {
  StaticMutexAutoLock lock(sMutex);  // for `mActiveConsumers`.
  return mActiveConsumers == 0;
}

void TimelineConsumers::AddMarkerForDocShell(nsDocShell* aDocShell,
                                             const char* aName,
                                             MarkerTracingType aTracingType,
                                             MarkerStackRequest aStackRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  if (HasConsumer(aDocShell)) {
    aDocShell->mObserved->AddMarker(
        MakeUnique<TimelineMarker>(aName, aTracingType, aStackRequest));
  }
}

void TimelineConsumers::AddMarkerForDocShell(nsDocShell* aDocShell,
                                             const char* aName,
                                             const TimeStamp& aTime,
                                             MarkerTracingType aTracingType,
                                             MarkerStackRequest aStackRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  if (HasConsumer(aDocShell)) {
    aDocShell->mObserved->AddMarker(
        MakeUnique<TimelineMarker>(aName, aTime, aTracingType, aStackRequest));
  }
}

void TimelineConsumers::AddMarkerForDocShell(
    nsDocShell* aDocShell, UniquePtr<AbstractTimelineMarker>&& aMarker) {
  MOZ_ASSERT(NS_IsMainThread());
  if (HasConsumer(aDocShell)) {
    aDocShell->mObserved->AddMarker(std::move(aMarker));
  }
}

void TimelineConsumers::AddMarkerForDocShell(nsIDocShell* aDocShell,
                                             const char* aName,
                                             MarkerTracingType aTracingType,
                                             MarkerStackRequest aStackRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  AddMarkerForDocShell(static_cast<nsDocShell*>(aDocShell), aName, aTracingType,
                       aStackRequest);
}

void TimelineConsumers::AddMarkerForDocShell(nsIDocShell* aDocShell,
                                             const char* aName,
                                             const TimeStamp& aTime,
                                             MarkerTracingType aTracingType,
                                             MarkerStackRequest aStackRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  AddMarkerForDocShell(static_cast<nsDocShell*>(aDocShell), aName, aTime,
                       aTracingType, aStackRequest);
}

void TimelineConsumers::AddMarkerForDocShell(
    nsIDocShell* aDocShell, UniquePtr<AbstractTimelineMarker>&& aMarker) {
  MOZ_ASSERT(NS_IsMainThread());
  AddMarkerForDocShell(static_cast<nsDocShell*>(aDocShell), std::move(aMarker));
}

void TimelineConsumers::AddMarkerForAllObservedDocShells(
    const char* aName, MarkerTracingType aTracingType,
    MarkerStackRequest aStackRequest /* = STACK */) {
  bool isMainThread = NS_IsMainThread();
  StaticMutexAutoLock lock(sMutex);  // for `mMarkersStores`.

  for (MarkersStorage* storage = mMarkersStores.getFirst(); storage != nullptr;
       storage = storage->getNext()) {
    UniquePtr<AbstractTimelineMarker> marker =
        MakeUnique<TimelineMarker>(aName, aTracingType, aStackRequest);
    if (isMainThread) {
      storage->AddMarker(std::move(marker));
    } else {
      storage->AddOTMTMarker(std::move(marker));
    }
  }
}

void TimelineConsumers::AddMarkerForAllObservedDocShells(
    const char* aName, const TimeStamp& aTime, MarkerTracingType aTracingType,
    MarkerStackRequest aStackRequest /* = STACK */) {
  bool isMainThread = NS_IsMainThread();
  StaticMutexAutoLock lock(sMutex);  // for `mMarkersStores`.

  for (MarkersStorage* storage = mMarkersStores.getFirst(); storage != nullptr;
       storage = storage->getNext()) {
    UniquePtr<AbstractTimelineMarker> marker =
        MakeUnique<TimelineMarker>(aName, aTime, aTracingType, aStackRequest);
    if (isMainThread) {
      storage->AddMarker(std::move(marker));
    } else {
      storage->AddOTMTMarker(std::move(marker));
    }
  }
}

void TimelineConsumers::AddMarkerForAllObservedDocShells(
    UniquePtr<AbstractTimelineMarker>& aMarker) {
  bool isMainThread = NS_IsMainThread();
  StaticMutexAutoLock lock(sMutex);  // for `mMarkersStores`.

  for (MarkersStorage* storage = mMarkersStores.getFirst(); storage != nullptr;
       storage = storage->getNext()) {
    UniquePtr<AbstractTimelineMarker> clone = aMarker->Clone();
    if (isMainThread) {
      storage->AddMarker(std::move(clone));
    } else {
      storage->AddOTMTMarker(std::move(clone));
    }
  }
}

void TimelineConsumers::PopMarkers(
    nsDocShell* aDocShell, JSContext* aCx,
    nsTArray<dom::ProfileTimelineMarker>& aStore) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aDocShell || !aDocShell->mObserved) {
    return;
  }

  aDocShell->mObserved->PopMarkers(aCx, aStore);
}

}  // namespace mozilla
