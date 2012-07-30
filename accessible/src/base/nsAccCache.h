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
  NS_ASSERTION(aAccessible, "Calling ClearCacheEntry with a NULL pointer!");
  if (aAccessible)
    aAccessible->Shutdown();

  return PL_DHASH_REMOVE;
}

/**
 * Clear the cache and shutdown the accessibles.
 */

static void
ClearCache(AccessibleHashtable& aCache)
{
  aCache.Enumerate(ClearCacheEntry<Accessible>, nullptr);
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
 * Traverse the accessible cache for cycle collector.
 */

static void
CycleCollectorTraverseCache(AccessibleHashtable& aCache,
                            nsCycleCollectionTraversalCallback *aCallback)
{
  aCache.EnumerateRead(CycleCollectorTraverseCacheEntry<Accessible>, aCallback);
}

#endif
