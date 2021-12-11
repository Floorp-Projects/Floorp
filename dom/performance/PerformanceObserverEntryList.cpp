/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceObserverEntryList.h"

#include "mozilla/dom/Performance.h"
#include "mozilla/dom/PerformanceObserverEntryListBinding.h"
#include "nsString.h"
#include "PerformanceResourceTiming.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PerformanceObserverEntryList, mOwner,
                                      mEntries)

NS_IMPL_CYCLE_COLLECTING_ADDREF(PerformanceObserverEntryList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PerformanceObserverEntryList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PerformanceObserverEntryList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

PerformanceObserverEntryList::~PerformanceObserverEntryList() = default;

JSObject* PerformanceObserverEntryList::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return PerformanceObserverEntryList_Binding::Wrap(aCx, this, aGivenProto);
}

void PerformanceObserverEntryList::GetEntries(
    const PerformanceEntryFilterOptions& aFilter,
    nsTArray<RefPtr<PerformanceEntry>>& aRetval) {
  aRetval.Clear();
  RefPtr<nsAtom> name =
      aFilter.mName.WasPassed() ? NS_Atomize(aFilter.mName.Value()) : nullptr;
  RefPtr<nsAtom> entryType = aFilter.mEntryType.WasPassed()
                                 ? NS_Atomize(aFilter.mEntryType.Value())
                                 : nullptr;
  for (const RefPtr<PerformanceEntry>& entry : mEntries) {
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
    if (name && entry->GetName() != name) {
      continue;
    }
    if (entryType && entry->GetEntryType() != entryType) {
      continue;
    }

    aRetval.AppendElement(entry);
  }
  aRetval.Sort(PerformanceEntryComparator());
}

void PerformanceObserverEntryList::GetEntriesByType(
    const nsAString& aEntryType, nsTArray<RefPtr<PerformanceEntry>>& aRetval) {
  aRetval.Clear();
  RefPtr<nsAtom> entryType = NS_Atomize(aEntryType);
  for (const RefPtr<PerformanceEntry>& entry : mEntries) {
    if (entry->GetEntryType() == entryType) {
      aRetval.AppendElement(entry);
    }
  }
  aRetval.Sort(PerformanceEntryComparator());
}

void PerformanceObserverEntryList::GetEntriesByName(
    const nsAString& aName, const Optional<nsAString>& aEntryType,
    nsTArray<RefPtr<PerformanceEntry>>& aRetval) {
  aRetval.Clear();
  RefPtr<nsAtom> name = NS_Atomize(aName);
  RefPtr<nsAtom> entryType =
      aEntryType.WasPassed() ? NS_Atomize(aEntryType.Value()) : nullptr;
  for (const RefPtr<PerformanceEntry>& entry : mEntries) {
    if (entry->GetName() != name) {
      continue;
    }

    if (entryType && entry->GetEntryType() != entryType) {
      continue;
    }

    aRetval.AppendElement(entry);
  }
  aRetval.Sort(PerformanceEntryComparator());
}
