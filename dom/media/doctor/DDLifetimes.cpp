/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DDLifetimes.h"

#include "DDLogUtils.h"

namespace mozilla {

DDLifetime*
DDLifetimes::FindLifetime(const DDLogObject& aObject,
                          const DDMessageIndex& aIndex)
{
  LifetimesForObject* lifetimes = mLifetimes.Get(aObject);
  if (lifetimes) {
    for (DDLifetime& lifetime : *lifetimes) {
      if (lifetime.mObject == aObject && lifetime.IsAliveAt(aIndex)) {
        return &lifetime;
      }
    }
  }
  return nullptr;
}

const DDLifetime*
DDLifetimes::FindLifetime(const DDLogObject& aObject,
                          const DDMessageIndex& aIndex) const
{
  const LifetimesForObject* lifetimes = mLifetimes.Get(aObject);
  if (lifetimes) {
    for (const DDLifetime& lifetime : *lifetimes) {
      if (lifetime.mObject == aObject && lifetime.IsAliveAt(aIndex)) {
        return &lifetime;
      }
    }
  }
  return nullptr;
}

DDLifetime&
DDLifetimes::CreateLifetime(const DDLogObject& aObject,
                            DDMessageIndex aIndex,
                            const DDTimeStamp& aConstructionTimeStamp)
{
  // Use negative tags for yet-unclassified messages.
  static int32_t sTag = 0;
  if (--sTag > 0) {
    sTag = -1;
  }
  LifetimesForObject* lifetimes = mLifetimes.LookupOrAdd(aObject, 1);
  DDLifetime& lifetime = *lifetimes->AppendElement(
    DDLifetime(aObject, aIndex, aConstructionTimeStamp, sTag));
  return lifetime;
}

void
DDLifetimes::RemoveLifetime(const DDLifetime* aLifetime)
{
  LifetimesForObject* lifetimes = mLifetimes.Get(aLifetime->mObject);
  MOZ_ASSERT(lifetimes);
  DDL_LOG(aLifetime->mMediaElement ? mozilla::LogLevel::Debug
                                   : mozilla::LogLevel::Warning,
          "Remove lifetime %s",
          aLifetime->Printf().get());
  // We should have been given a pointer inside this lifetimes array.
  auto arrayIndex = aLifetime - lifetimes->Elements();
  MOZ_ASSERT(static_cast<size_t>(arrayIndex) < lifetimes->Length());
  lifetimes->RemoveElementAt(arrayIndex);
}

void
DDLifetimes::RemoveLifetimesFor(const dom::HTMLMediaElement* aMediaElement)
{
  for (auto iter = mLifetimes.Iter(); !iter.Done(); iter.Next()) {
    iter.UserData()->RemoveElementsBy(
      [aMediaElement](const DDLifetime& lifetime) {
        return lifetime.mMediaElement == aMediaElement;
      });
  }
}

void
DDLifetimes::Clear()
{
  mLifetimes.Clear();
}

size_t
DDLifetimes::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t size = mLifetimes.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mLifetimes.ConstIter(); !iter.Done(); iter.Next()) {
    size += iter.UserData()->ShallowSizeOfExcludingThis(aMallocSizeOf);
  }
  return size;
}

} // namespace mozilla
