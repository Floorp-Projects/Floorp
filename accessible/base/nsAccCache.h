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

template <class T>
void
UnbindCacheEntriesFromDocument(
  nsRefPtrHashtable<nsPtrHashKey<const void>, T>& aCache)
{
  for (auto iter = aCache.Iter(); !iter.Done(); iter.Next()) {
    T* accessible = iter.Data();
    MOZ_ASSERT(accessible && !accessible->IsDefunct());
    accessible->Document()->UnbindFromDocument(accessible);
    iter.Remove();
  }
}

/**
 * Clear the cache and shutdown the accessibles.
 */
template <class T>
static void
ClearCache(nsRefPtrHashtable<nsPtrHashKey<const void>, T>& aCache)
{
  for (auto iter = aCache.Iter(); !iter.Done(); iter.Next()) {
    T* accessible = iter.Data();
    MOZ_ASSERT(accessible);
    if (accessible && !accessible->IsDefunct()) {
      accessible->Shutdown();
    }
    iter.Remove();
  }
}

#endif
