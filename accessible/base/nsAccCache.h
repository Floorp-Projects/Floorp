/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsAccCache_H_
#define _nsAccCache_H_

#include "xpcAccessibleDocument.h"

////////////////////////////////////////////////////////////////////////////////
// Accessible cache utils
////////////////////////////////////////////////////////////////////////////////

/**
 * Shutdown and removes the accessible from cache.
 */
template <class T>
static PLDHashOperator
ClearCacheEntry(const void* aKey, nsRefPtr<T>& aAccessible, void* aUserArg)
{
  NS_ASSERTION(aAccessible, "Calling ClearCacheEntry with a nullptr pointer!");
  if (aAccessible && !aAccessible->IsDefunct())
    aAccessible->Shutdown();

  return PL_DHASH_REMOVE;
}

template <class T>
static PLDHashOperator
UnbindCacheEntryFromDocument(const void* aKey, nsRefPtr<T>& aAccessible,
                             void* aUserArg)
{
  MOZ_ASSERT(aAccessible && !aAccessible->IsDefunct());
  aAccessible->Document()->UnbindFromDocument(aAccessible);

  return PL_DHASH_REMOVE;
}

/**
 * Clear the cache and shutdown the accessibles.
 */

template <class T>
static void
ClearCache(nsRefPtrHashtable<nsPtrHashKey<const void>, T>& aCache)
{
  aCache.Enumerate(ClearCacheEntry<T>, nullptr);
}

#endif
