/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceEntry.h"
#include "MainThreadUtils.h"
#include "mozilla/dom/PerformanceEntryBinding.h"
#include "nsIURI.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PerformanceEntry, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(PerformanceEntry)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PerformanceEntry)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PerformanceEntry)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

PerformanceEntry::PerformanceEntry(nsISupports* aParent,
                                   const nsAString& aName,
                                   const nsAString& aEntryType)
: mParent(aParent),
  mName(aName),
  mEntryType(aEntryType)
{
  // mParent is null in workers.
  MOZ_ASSERT(mParent || !NS_IsMainThread());
}

PerformanceEntry::~PerformanceEntry()
{
}

JSObject*
PerformanceEntry::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::PerformanceEntryBinding::Wrap(aCx, this, aGivenProto);
}

size_t
PerformanceEntry::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return mName.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
         mEntryType.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

size_t
PerformanceEntry::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}
