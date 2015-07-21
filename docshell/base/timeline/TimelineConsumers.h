/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TimelineConsumers_h_
#define mozilla_TimelineConsumers_h_

#include "mozilla/LinkedList.h"
#include "mozilla/UniquePtr.h"
#include "timeline/ObservedDocShell.h"

class nsDocShell;

namespace mozilla {

class TimelineConsumers
{
private:
  // Counter for how many timelines are currently interested in markers.
  static unsigned long sActiveConsumers;
  static LinkedList<ObservedDocShell>* sObservedDocShells;
  static LinkedList<ObservedDocShell>& GetOrCreateObservedDocShellsList();

public:
  static void AddConsumer(nsDocShell* aDocShell,
                          UniquePtr<ObservedDocShell>& aObservedPtr);
  static void RemoveConsumer(nsDocShell* aDocShell,
                             UniquePtr<ObservedDocShell>& aObservedPtr);
  static bool IsEmpty();
  static bool GetKnownDocShells(Vector<nsRefPtr<nsDocShell>>& aStore);
};

} // namespace mozilla

#endif /* mozilla_TimelineConsumers_h_ */
