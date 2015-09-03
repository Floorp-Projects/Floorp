/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceObserverEntryList.h"

#include "mozilla/dom/PerformanceObserverEntryListBinding.h"
#include "nsPerformance.h"
#include "nsString.h"
#include "PerformanceResourceTiming.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PerformanceObserverEntryList,
                                      mOwner,
                                      mEntries)

NS_IMPL_CYCLE_COLLECTING_ADDREF(PerformanceObserverEntryList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PerformanceObserverEntryList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PerformanceObserverEntryList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

PerformanceObserverEntryList::~PerformanceObserverEntryList()
{
}

JSObject*
PerformanceObserverEntryList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PerformanceObserverEntryListBinding::Wrap(aCx, this, aGivenProto);
}

void
PerformanceObserverEntryList::GetEntries(
  const PerformanceEntryFilterOptions& aFilter,
  nsTArray<nsRefPtr<PerformanceEntry>>& aRetval)
{
  aRetval.Clear();
  for (const nsRefPtr<PerformanceEntry>& entry : mEntries) {
    if (aFilter.mInitiatorType.WasPassed()) {
      const PerformanceResourceTiming* resourceEntry =
        entry->ToResourceTiming();
      if (!resourceEntry) {
        continue;
      }
      nsAutoString initiatorType;
      resourceEntry->GetInitiatorType(initiatorType);
      if (!initiatorType.Equals(aFilter.mInitiatorType.Value())) {
        continue;
      }
    }
    if (aFilter.mName.WasPassed() &&
        !entry->GetName().Equals(aFilter.mName.Value())) {
      continue;
    }
    if (aFilter.mEntryType.WasPassed() &&
        !entry->GetEntryType().Equals(aFilter.mEntryType.Value())) {
      continue;
    }

    aRetval.AppendElement(entry);
  }
}

void
PerformanceObserverEntryList::GetEntriesByType(
  const nsAString& aEntryType,
  nsTArray<nsRefPtr<PerformanceEntry>>& aRetval)
{
  aRetval.Clear();
  for (const nsRefPtr<PerformanceEntry>& entry : mEntries) {
    if (entry->GetEntryType().Equals(aEntryType)) {
      aRetval.AppendElement(entry);
    }
  }
}

void
PerformanceObserverEntryList::GetEntriesByName(
  const nsAString& aName,
  const Optional<nsAString>& aEntryType,
  nsTArray<nsRefPtr<PerformanceEntry>>& aRetval)
{
  aRetval.Clear();
  for (const nsRefPtr<PerformanceEntry>& entry : mEntries) {
    if (entry->GetName().Equals(aName)) {
      aRetval.AppendElement(entry);
    }
  }
}
