/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
ClearCache(nsAccessibleHashtable & aCache)
{
  aCache.Enumerate(ClearCacheEntry<nsAccessible>, nsnull);
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
CycleCollectorTraverseCache(nsAccessibleHashtable & aCache,
                            nsCycleCollectionTraversalCallback *aCallback)
{
  aCache.EnumerateRead(CycleCollectorTraverseCacheEntry<nsAccessible>, aCallback);
}

#endif
