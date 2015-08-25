/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TimelineConsumers.h"

namespace mozilla {

unsigned long TimelineConsumers::sActiveConsumers = 0;
LinkedList<ObservedDocShell>* TimelineConsumers::sObservedDocShells = nullptr;

LinkedList<ObservedDocShell>&
TimelineConsumers::GetOrCreateObservedDocShellsList()
{
  if (!sObservedDocShells) {
    sObservedDocShells = new LinkedList<ObservedDocShell>();
  }
  return *sObservedDocShells;
}

void
TimelineConsumers::AddConsumer(nsDocShell* aDocShell)
{
  UniquePtr<ObservedDocShell>& observed = aDocShell->mObserved;

  MOZ_ASSERT(!observed);
  sActiveConsumers++;
  observed.reset(new ObservedDocShell(aDocShell));
  GetOrCreateObservedDocShellsList().insertFront(observed.get());
}

void
TimelineConsumers::RemoveConsumer(nsDocShell* aDocShell)
{
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
  return sActiveConsumers == 0;
}

bool
TimelineConsumers::GetKnownDocShells(Vector<nsRefPtr<nsDocShell>>& aStore)
{
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
                                        TracingMetadata aMetaData)
{
  if (aDocShell->IsObserved()) {
    aDocShell->mObserved->AddMarker(Move(MakeUnique<TimelineMarker>(aDocShell, aName, aMetaData)));
  }
}

void
TimelineConsumers::AddMarkerForDocShell(nsDocShell* aDocShell,
                                        const char* aName,
                                        const TimeStamp& aTime,
                                        TracingMetadata aMetaData)
{
  if (aDocShell->IsObserved()) {
    aDocShell->mObserved->AddMarker(Move(MakeUnique<TimelineMarker>(aDocShell, aName, aTime, aMetaData)));
  }
}

void
TimelineConsumers::AddMarkerForDocShell(nsDocShell* aDocShell,
                                        UniquePtr<TimelineMarker>&& aMarker)
{
  if (aDocShell->IsObserved()) {
    aDocShell->mObserved->AddMarker(Move(aMarker));
  }
}

void
TimelineConsumers::AddMarkerForDocShell(nsIDocShell* aDocShell,
                                        const char* aName,
                                        TracingMetadata aMetaData)
{
  AddMarkerForDocShell(static_cast<nsDocShell*>(aDocShell), aName, aMetaData);
}

void
TimelineConsumers::AddMarkerForDocShell(nsIDocShell* aDocShell,
                                        const char* aName,
                                        const TimeStamp& aTime,
                                        TracingMetadata aMetaData)
{
  AddMarkerForDocShell(static_cast<nsDocShell*>(aDocShell), aName, aTime, aMetaData);
}

void
TimelineConsumers::AddMarkerForDocShell(nsIDocShell* aDocShell,
                                        UniquePtr<TimelineMarker>&& aMarker)
{
  AddMarkerForDocShell(static_cast<nsDocShell*>(aDocShell), Move(aMarker));
}

void
TimelineConsumers::AddMarkerForDocShellsList(Vector<nsRefPtr<nsDocShell>>& aDocShells,
                                             const char* aName,
                                             TracingMetadata aMetaData)
{
  for (Vector<nsRefPtr<nsDocShell>>::Range range = aDocShells.all();
       !range.empty();
       range.popFront()) {
    AddMarkerForDocShell(range.front(), aName, aMetaData);
  }
}

void
TimelineConsumers::AddMarkerForAllObservedDocShells(const char* aName,
                                                    TracingMetadata aMetaData)
{
  Vector<nsRefPtr<nsDocShell>> docShells;
  if (!GetKnownDocShells(docShells)) {
    // If we don't successfully populate our vector with *all* docshells being
    // observed, don't add the marker to *any* of them.
    return;
  }

  AddMarkerForDocShellsList(docShells, aName, aMetaData);
}

} // namespace mozilla
