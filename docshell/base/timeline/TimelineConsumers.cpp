/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TimelineConsumers.h"

namespace mozilla {

unsigned long TimelineConsumers::sActiveConsumers = 0;
LinkedList<ObservedDocShell>* TimelineConsumers::sObservedDocShells = nullptr;
Mutex* TimelineConsumers::sLock = nullptr;

LinkedList<ObservedDocShell>&
TimelineConsumers::GetOrCreateObservedDocShellsList()
{
  if (!sObservedDocShells) {
    sObservedDocShells = new LinkedList<ObservedDocShell>();
  }
  return *sObservedDocShells;
}

Mutex&
TimelineConsumers::GetLock()
{
  if (!sLock) {
    sLock = new Mutex("TimelineConsumersMutex");
  }
  return *sLock;
}

void
TimelineConsumers::AddConsumer(nsDocShell* aDocShell)
{
  MOZ_ASSERT(NS_IsMainThread());
  UniquePtr<ObservedDocShell>& observed = aDocShell->mObserved;

  MOZ_ASSERT(!observed);
  sActiveConsumers++;
  observed.reset(new ObservedDocShell(aDocShell));
  GetOrCreateObservedDocShellsList().insertFront(observed.get());
}

void
TimelineConsumers::RemoveConsumer(nsDocShell* aDocShell)
{
  MOZ_ASSERT(NS_IsMainThread());
  UniquePtr<ObservedDocShell>& observed = aDocShell->mObserved;

  MOZ_ASSERT(observed);
  sActiveConsumers--;
  observed.get()->ClearMarkers();
  observed.get()->remove();
  observed.reset(nullptr);
}

bool
TimelineConsumers::IsEmpty()
{
  MOZ_ASSERT(NS_IsMainThread());
  return sActiveConsumers == 0;
}

bool
TimelineConsumers::GetKnownDocShells(Vector<nsRefPtr<nsDocShell>>& aStore)
{
  MOZ_ASSERT(NS_IsMainThread());
  const LinkedList<ObservedDocShell>& docShells = GetOrCreateObservedDocShellsList();

  for (const ObservedDocShell* rds = docShells.getFirst();
       rds != nullptr;
       rds = rds->getNext()) {
    if (!aStore.append(**rds)) {
      return false;
    }
  }

  return true;
}

void
TimelineConsumers::AddMarkerForDocShell(nsDocShell* aDocShell,
                                        const char* aName,
                                        MarkerTracingType aTracingType)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (aDocShell->IsObserved()) {
    aDocShell->mObserved->AddMarker(Move(MakeUnique<TimelineMarker>(aName, aTracingType)));
  }
}

void
TimelineConsumers::AddMarkerForDocShell(nsDocShell* aDocShell,
                                        const char* aName,
                                        const TimeStamp& aTime,
                                        MarkerTracingType aTracingType)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (aDocShell->IsObserved()) {
    aDocShell->mObserved->AddMarker(Move(MakeUnique<TimelineMarker>(aName, aTime, aTracingType)));
  }
}

void
TimelineConsumers::AddMarkerForDocShell(nsDocShell* aDocShell,
                                        UniquePtr<AbstractTimelineMarker>&& aMarker)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (aDocShell->IsObserved()) {
    aDocShell->mObserved->AddMarker(Move(aMarker));
  }
}

void
TimelineConsumers::AddOTMTMarkerForDocShell(nsDocShell* aDocShell,
                                            UniquePtr<AbstractTimelineMarker>& aMarker)
{
  MOZ_ASSERT(!NS_IsMainThread());
  GetLock().AssertCurrentThreadOwns();
  if (aDocShell->IsObserved()) {
    aDocShell->mObserved->AddOTMTMarkerClone(aMarker);
  }
}

void
TimelineConsumers::AddMarkerForDocShell(nsIDocShell* aDocShell,
                                        const char* aName,
                                        MarkerTracingType aTracingType)
{
  MOZ_ASSERT(NS_IsMainThread());
  AddMarkerForDocShell(static_cast<nsDocShell*>(aDocShell), aName, aTracingType);
}

void
TimelineConsumers::AddMarkerForDocShell(nsIDocShell* aDocShell,
                                        const char* aName,
                                        const TimeStamp& aTime,
                                        MarkerTracingType aTracingType)
{
  MOZ_ASSERT(NS_IsMainThread());
  AddMarkerForDocShell(static_cast<nsDocShell*>(aDocShell), aName, aTime, aTracingType);
}

void
TimelineConsumers::AddMarkerForDocShell(nsIDocShell* aDocShell,
                                        UniquePtr<AbstractTimelineMarker>&& aMarker)
{
  MOZ_ASSERT(NS_IsMainThread());
  AddMarkerForDocShell(static_cast<nsDocShell*>(aDocShell), Move(aMarker));
}

void
TimelineConsumers::AddOTMTMarkerForDocShell(nsIDocShell* aDocShell,
                                            UniquePtr<AbstractTimelineMarker>& aMarker)
{
  MOZ_ASSERT(!NS_IsMainThread());
  GetLock().AssertCurrentThreadOwns();
  AddOTMTMarkerForDocShell(static_cast<nsDocShell*>(aDocShell), aMarker);
}

void
TimelineConsumers::AddMarkerForDocShellsList(Vector<nsRefPtr<nsDocShell>>& aDocShells,
                                             const char* aName,
                                             MarkerTracingType aTracingType)
{
  MOZ_ASSERT(NS_IsMainThread());
  for (Vector<nsRefPtr<nsDocShell>>::Range range = aDocShells.all();
       !range.empty();
       range.popFront()) {
    AddMarkerForDocShell(range.front(), aName, aTracingType);
  }
}

void
TimelineConsumers::AddMarkerForDocShellsList(Vector<nsRefPtr<nsDocShell>>& aDocShells,
                                             const char* aName,
                                             const TimeStamp& aTime,
                                             MarkerTracingType aTracingType)
{
  MOZ_ASSERT(NS_IsMainThread());
  for (Vector<nsRefPtr<nsDocShell>>::Range range = aDocShells.all();
       !range.empty();
       range.popFront()) {
    AddMarkerForDocShell(range.front(), aName, aTime, aTracingType);
  }
}

void
TimelineConsumers::AddMarkerForDocShellsList(Vector<nsRefPtr<nsDocShell>>& aDocShells,
                                             UniquePtr<AbstractTimelineMarker>& aMarker)
{
  MOZ_ASSERT(NS_IsMainThread());
  for (Vector<nsRefPtr<nsDocShell>>::Range range = aDocShells.all();
       !range.empty();
       range.popFront()) {
    UniquePtr<AbstractTimelineMarker> cloned = aMarker->Clone();
    AddMarkerForDocShell(range.front(), Move(cloned));
  }
}

void
TimelineConsumers::AddOTMTMarkerForDocShellsList(Vector<nsRefPtr<nsDocShell>>& aDocShells,
                                                 UniquePtr<AbstractTimelineMarker>& aMarker)
{
  MOZ_ASSERT(!NS_IsMainThread());
  GetLock().AssertCurrentThreadOwns();
  for (Vector<nsRefPtr<nsDocShell>>::Range range = aDocShells.all();
       !range.empty();
       range.popFront()) {
    AddOTMTMarkerForDocShell(range.front(), aMarker);
  }
}

void
TimelineConsumers::AddMarkerForAllObservedDocShells(const char* aName,
                                                    MarkerTracingType aTracingType)
{
  MOZ_ASSERT(NS_IsMainThread());
  Vector<nsRefPtr<nsDocShell>> docShells;
  if (GetKnownDocShells(docShells)) {
    AddMarkerForDocShellsList(docShells, aName, aTracingType);
  }
}

void
TimelineConsumers::AddMarkerForAllObservedDocShells(const char* aName,
                                                    const TimeStamp& aTime,
                                                    MarkerTracingType aTracingType)
{
  MOZ_ASSERT(NS_IsMainThread());
  Vector<nsRefPtr<nsDocShell>> docShells;
  if (GetKnownDocShells(docShells)) {
    AddMarkerForDocShellsList(docShells, aName, aTime, aTracingType);
  }
}

void
TimelineConsumers::AddMarkerForAllObservedDocShells(UniquePtr<AbstractTimelineMarker>& aMarker)
{
  MOZ_ASSERT(NS_IsMainThread());
  Vector<nsRefPtr<nsDocShell>> docShells;
  if (GetKnownDocShells(docShells)) {
    AddMarkerForDocShellsList(docShells, aMarker);
  }
}

void
TimelineConsumers::AddOTMTMarkerForAllObservedDocShells(UniquePtr<AbstractTimelineMarker>& aMarker)
{
  MOZ_ASSERT(!NS_IsMainThread());
  GetLock().AssertCurrentThreadOwns();
  Vector<nsRefPtr<nsDocShell>> docShells;
  if (GetKnownDocShells(docShells)) {
    AddOTMTMarkerForDocShellsList(docShells, aMarker);
  }
}

} // namespace mozilla
