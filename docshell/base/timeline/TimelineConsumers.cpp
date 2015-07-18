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
TimelineConsumers::AddConsumer(nsDocShell* aDocShell,
                               UniquePtr<ObservedDocShell>& aObservedPtr)
{
  MOZ_ASSERT(!aObservedPtr);
  sActiveConsumers++;
  aObservedPtr.reset(new ObservedDocShell(aDocShell));
  GetOrCreateObservedDocShellsList().insertFront(aObservedPtr.get());
}

void
TimelineConsumers::RemoveConsumer(nsDocShell* aDocShell,
                                  UniquePtr<ObservedDocShell>& aObservedPtr)
{
  MOZ_ASSERT(aObservedPtr);
  sActiveConsumers--;
  aObservedPtr.get()->remove();
  aObservedPtr.reset(nullptr);
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

} // namespace mozilla
