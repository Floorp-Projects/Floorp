/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Mike Shaver <shaver@zeroknowledge.com>
 */

#include <stdio.h>
#include "nsGCCache.h"
/* The GC cache is shared among all windows, since it doesn't hog
   any scarce resources (like colormap entries.) */

GdkRegion *nsGCCache::copyRegion = NULL;

  DEBUG_METER(
              static struct {
                int hits[GC_CACHE_SIZE];
                int misses;
                int reclaim;
              } GCCacheStats;
              )

nsGCCache::nsGCCache()
{
  PR_INIT_CLIST(&GCCache);
  PR_INIT_CLIST(&GCFreeList);
  for (int i = 0; i < GC_CACHE_SIZE; i++) {
    GCCacheEntry *entry = new GCCacheEntry();
    PR_INSERT_LINK(&entry->clist, &GCFreeList);
  }
  DEBUG_METER(memset(&GCCacheStats, 0, sizeof(GCCacheStats));)
}

void
nsGCCache::free_cache_entry(PRCList *clist)
{
  GCCacheEntry *entry = (GCCacheEntry *)clist;
  gdk_gc_unref(entry->gc);
  if (entry->clipRegion)
    gdk_region_destroy(entry->clipRegion);
  
  /* thread on the freelist, at the front */
  PR_REMOVE_LINK(clist);
  memset(entry, 0, sizeof(*entry));
  PR_INSERT_LINK(clist, &GCFreeList);
}

nsGCCache::~nsGCCache()
{
  PRCList *head;

  ReportStats();

  while (!PR_CLIST_IS_EMPTY(&GCCache)) {
    head = PR_LIST_HEAD(&GCCache);
    if (head == &GCCache)
      break;
    free_cache_entry(head);
  }

  while(!PR_CLIST_IS_EMPTY(&GCFreeList)) {
    head = PR_LIST_HEAD(&GCFreeList);
    if (head == &GCFreeList)
      break;
    PR_REMOVE_LINK(head);
    delete (GCCacheEntry *)head;
  }
}

void
nsGCCache::ReportStats() { 
  DEBUG_METER(
              fprintf(stderr, "GC Cache:\n\thits:");
              int hits = 0;
              for (int i = 0; i < GC_CACHE_SIZE; i++) {
                fprintf(stderr, " %4d", GCCacheStats.hits[i]);
                hits+=GCCacheStats.hits[i];
              }
              fprintf(stderr, "\n\thits: %d, misses: %d, reclaimed %d times\n", 
                      hits, GCCacheStats.misses, GCCacheStats.reclaim);
              );
}

GdkRegion *
nsGCCache::gdk_region_copy(GdkRegion *region)
{
  if (!copyRegion) copyRegion = gdk_region_new();

  return gdk_regions_union(region, copyRegion);
}

/* Dispose of entries matching the given flags, compressing the GC cache */
void nsGCCache::Flush(unsigned long flags)
{
  while (!PR_CLIST_IS_EMPTY(&GCCache)) {
    PRCList *head = PR_LIST_HEAD(&GCCache);
    if (head == &GCCache)
      break;
    GCCacheEntry *entry = (GCCacheEntry *)head;
    if (entry->flags & flags)
      free_cache_entry(head);
  }
}

GdkGC *nsGCCache::GetGC(GdkWindow *window, GdkGCValues *gcv, GdkGCValuesMask flags, GdkRegion *clipRegion)
{

  PRCList *iter;
  GCCacheEntry *entry;
  DEBUG_METER(int i = 0;)
  
  for (iter = PR_LIST_HEAD(&GCCache); iter != &GCCache;
       iter = PR_NEXT_LINK(iter)) {

    entry = (GCCacheEntry *)iter;
    if (flags == entry->flags && 
        !memcmp (gcv, &entry->gcv, sizeof (*gcv))) {
      /* if there's a clipRegion, we have to match */

      if ((clipRegion && entry->clipRegion &&
           gdk_region_equal(clipRegion, entry->clipRegion)) ||
          /* and if there isn't, we can't have one */
          (!clipRegion && !entry->clipRegion)) {

        /* move to the front of the list, if needed */
        if (iter != PR_LIST_HEAD(&GCCache)) {
          PR_REMOVE_LINK(iter);
          PR_INSERT_LINK(iter, &GCCache);
        }
        DEBUG_METER(GCCacheStats.hits[i]++;)
        return gdk_gc_ref(entry->gc);
      }
    }
    DEBUG_METER(++i;)
  }
    
  /* might need to forcibly free the LRU cache entry */
  if (PR_CLIST_IS_EMPTY(&GCFreeList)) {
    DEBUG_METER(GCCacheStats.reclaim++);
    free_cache_entry(PR_LIST_TAIL(&GCCache));
  }

  DEBUG_METER(GCCacheStats.misses++;)
  
  iter = PR_LIST_HEAD(&GCFreeList);
  PR_REMOVE_LINK(iter);
  PR_INSERT_LINK(iter, &GCCache);
  entry = (GCCacheEntry *)iter;
  
  entry->gc = gdk_gc_new_with_values(window, gcv, flags);
  entry->flags = flags;
  entry->gcv = *gcv;
  entry->clipRegion = NULL;
  if (clipRegion) {
    entry->clipRegion = gdk_region_copy(clipRegion);
    if (entry->clipRegion)
      gdk_gc_set_clip_region(entry->gc, entry->clipRegion);
    /* XXX what if it fails? */
  }
  
  return gdk_gc_ref(entry->gc);
}


