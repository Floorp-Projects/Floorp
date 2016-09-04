/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AvailabilityCollection.h"

#include "mozilla/ClearOnShutdown.h"
#include "PresentationAvailability.h"

namespace mozilla {
namespace dom {

/* static */
StaticAutoPtr<AvailabilityCollection>
AvailabilityCollection::sSingleton;
static bool gOnceAliveNowDead = false;

/* static */ AvailabilityCollection*
AvailabilityCollection::GetSingleton()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sSingleton && !gOnceAliveNowDead) {
    sSingleton = new AvailabilityCollection();
    ClearOnShutdown(&sSingleton);
  }

  return sSingleton;
}

AvailabilityCollection::AvailabilityCollection()
{
  MOZ_COUNT_CTOR(AvailabilityCollection);
}

AvailabilityCollection::~AvailabilityCollection()
{
  MOZ_COUNT_DTOR(AvailabilityCollection);
  gOnceAliveNowDead = true;
}

void
AvailabilityCollection::Add(PresentationAvailability* aAvailability)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aAvailability) {
    return;
  }

  WeakPtr<PresentationAvailability> availability = aAvailability;
  if (mAvailabilities.Contains(aAvailability)) {
    return;
  }

  mAvailabilities.AppendElement(aAvailability);
}

void
AvailabilityCollection::Remove(PresentationAvailability* aAvailability)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aAvailability) {
    return;
  }

  WeakPtr<PresentationAvailability> availability = aAvailability;
  mAvailabilities.RemoveElement(availability);
}

already_AddRefed<PresentationAvailability>
AvailabilityCollection::Find(const uint64_t aWindowId, const nsTArray<nsString>& aUrls)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Loop backwards to allow removing elements in the loop.
  for (int i = mAvailabilities.Length() - 1; i >= 0; --i) {
    WeakPtr<PresentationAvailability> availability = mAvailabilities[i];
    if (!availability) {
      // The availability object was destroyed. Remove it from the list.
      mAvailabilities.RemoveElementAt(i);
      continue;
    }

    if (availability->Equals(aWindowId, aUrls)) {
      RefPtr<PresentationAvailability> matchedAvailability = availability.get();
      return matchedAvailability.forget();
    }
  }


  return nullptr;
}

} // namespace dom
} // namespace mozilla
