/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsAccCache_H_
#define _nsAccCache_H_

#include "nsIAccessible.h"
#include "nsRefPtrHashtable.h"
#include "nsCycleCollectionParticipant.h"

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
  if (aAccessible)
    aAccessible->Shutdown();

  return PL_DHASH_REMOVE;
}

/**
 * Clear the cache and shutdown the accessibles.
 */

static void
ClearCache(mozilla::a11y::AccessibleHashtable& aCache)
{
  aCache.Enumerate(ClearCacheEntry<mozilla::a11y::Accessible>, nullptr);
}

/**
 * Traverse the accessible cache entry for cycle collector.
 */
template <class T>
static PLDHashOperator
CycleCollectorTraverseCacheEntry(const void *aKey, T *aAccessible,
                                 void *aUserArg)
{
  nsCycleCollectionTraversalCallback *cb =
    static_cast<nsCycleCollectionTraversalCallback*>(aUserArg);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "accessible cache entry");

  nsISupports *supports = static_cast<nsIAccessible*>(aAccessible);
  cb->NoteXPCOMChild(supports);
  return PL_DHASH_NEXT;
}

/**
 * Unlink the accessible cache for the cycle collector.
 */
inline void
ImplCycleCollectionUnlink(mozilla::a11y::AccessibleHashtable& aCache)
{
  ClearCache(aCache);
}

/**
 * Traverse the accessible cache for cycle collector.
 */
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            mozilla::a11y::AccessibleHashtable& aCache,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  aCache.EnumerateRead(CycleCollectorTraverseCacheEntry<mozilla::a11y::Accessible>,
                       &aCallback);
}

#endif
