/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimelineConsumers.h"

#include "mozilla/ObservedDocShell.h"
#include "mozilla/TimelineMarker.h"
#include "jsapi.h"
#include "nsAppRunner.h"  // for XRE_IsContentProcess, XRE_IsParentProcess
#include "nsCRT.h"
#include "nsDocShell.h"

namespace mozilla {

StaticMutex TimelineConsumers::sMutex;

uint32_t TimelineConsumers::sActiveConsumers = 0;

StaticAutoPtr<LinkedList<MarkersStorage>> TimelineConsumers::sMarkersStores;

LinkedList<MarkersStorage>& TimelineConsumers::MarkersStores() {
  if (!sMarkersStores) {
    sMarkersStores = new LinkedList<MarkersStorage>;
  }
  return *sMarkersStores;
}

void TimelineConsumers::AddConsumer(nsDocShell* aDocShell) {
  MOZ_ASSERT(NS_IsMainThread());
  StaticMutexAutoLock lock(
      sMutex);  // for `sActiveConsumers` and `sMarkersStores`.

  UniquePtr<ObservedDocShell>& observed = aDocShell->mObserved;
  MOZ_ASSERT(!observed);

  if (sActiveConsumers == 0) {
    JS::SetProfileTimelineRecordingEnabled(true);
  }
  sActiveConsumers++;

  ObservedDocShell* obsDocShell = new ObservedDocShell(aDocShell);
  MarkersStorage* storage = static_cast<MarkersStorage*>(obsDocShell);

  observed.reset(obsDocShell);
  MarkersStores().insertFront(storage);
}

void TimelineConsumers::RemoveConsumer(nsDocShell* aDocShell) {
  MOZ_ASSERT(NS_IsMainThread());
  StaticMutexAutoLock lock(
      sMutex);  // for `sActiveConsumers` and `sMarkersStores`.

  UniquePtr<ObservedDocShell>& observed = aDocShell->mObserved;
  MOZ_ASSERT(observed);

  sActiveConsumers--;
  if (sActiveConsumers == 0) {
    JS::SetProfileTimelineRecordingEnabled(false);
  }

  // Clear all markers from the `mTimelineMarkers` store.
  observed->ClearMarkers();
  // Remove self from the `sMarkersStores` store.
  observed->remove();
  // Prepare for becoming a consumer later.
  observed.reset(nullptr);
}

bool TimelineConsumers::HasConsumer(nsIDocShell* aDocShell) {
  MOZ_ASSERT(NS_IsMainThread());
  return aDocShell ? aDocShell->GetRecordProfileTimelineMarkers() : false;
}

bool TimelineConsumers::IsEmpty() {
  StaticMutexAutoLock lock(sMutex);  // for `sActiveConsumers`.
  return sActiveConsumers == 0;
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
  StaticMutexAutoLock lock(sMutex);  // for `sMarkersStores`.

  for (MarkersStorage* storage = MarkersStores().getFirst(); storage != nullptr;
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
  StaticMutexAutoLock lock(sMutex);  // for `sMarkersStores`.

  for (MarkersStorage* storage = MarkersStores().getFirst(); storage != nullptr;
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
  StaticMutexAutoLock lock(sMutex);  // for `sMarkersStores`.

  for (MarkersStorage* storage = MarkersStores().getFirst(); storage != nullptr;
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
