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
 * Communications Portions created by Netscape are
 * Copyright (C) 1999mozilla.org. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Mike Shaver <shaver@zeroknowledge.com>
 *   Tomi Leppikangas <Tomi.Leppikangas@oulu.fi>
 */

#include <stdio.h>
#include "nsGCCache.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* The GC cache is shared among all windows, since it doesn't hog
   any scarce resources (like colormap entries.) */

Region nsGCCacheXlib::copyRegion = 0;

nsGCCacheXlib::nsGCCacheXlib()
{
  PR_INIT_CLIST(&GCCache);
  PR_INIT_CLIST(&GCFreeList);
  for (int i = 0; i < GC_CACHE_SIZE; i++) {
    GCCacheEntryXlib *entry = new GCCacheEntryXlib();
    entry->gc=NULL;
    PR_INSERT_LINK(&entry->clist, &GCFreeList);
  }
  DEBUG_METER(memset(&GCCacheStats, 0, sizeof(GCCacheStats));)
}

void
nsGCCacheXlib::move_cache_entry(PRCList *clist)
{
  /* thread on the freelist, at the front */
  PR_REMOVE_LINK(clist);
  PR_INSERT_LINK(clist, &GCFreeList);
}

void
nsGCCacheXlib::free_cache_entry(PRCList *clist)
{
  GCCacheEntryXlib *entry = (GCCacheEntryXlib *)clist;
  entry->gc->Release();
  if (entry->clipRegion)
    ::XDestroyRegion(entry->clipRegion);
  
  /* thread on the freelist, at the front */
  PR_REMOVE_LINK(clist);
  memset(entry, 0, sizeof(*entry));
  PR_INSERT_LINK(clist, &GCFreeList);
}

nsGCCacheXlib::~nsGCCacheXlib()
{
  PRCList *head;

  ReportStats();

  while (!PR_CLIST_IS_EMPTY(&GCCache)) {
    head = PR_LIST_HEAD(&GCCache);
    if (head == &GCCache)
      break;
    free_cache_entry(head);
  }

  while (!PR_CLIST_IS_EMPTY(&GCFreeList)) {
    head = PR_LIST_HEAD(&GCFreeList);
    if (head == &GCFreeList)
      break;
    PR_REMOVE_LINK(head);
    delete (GCCacheEntryXlib *)head;
  }
}

void
nsGCCacheXlib::ReportStats() { 
  DEBUG_METER(
              fprintf(stderr, "GC Cache:\n\thits:");
              int hits = 0;
              for (int i = 0; i < GC_CACHE_SIZE; i++) {
                fprintf(stderr, " %4d", GCCacheStats.hits[i]);
                hits+=GCCacheStats.hits[i];
              }
              int total = hits + GCCacheStats.misses;
              float percent = float(float(hits) / float(total));
              percent *= 100;
              fprintf(stderr, "\n\thits: %d, misses: %d, hit percent: %f%%\n", 
                      hits, GCCacheStats.misses, percent);
              );
}


void
nsGCCacheXlib::XCopyRegion(Region srca, Region dr_return)
{
  if (!copyRegion) copyRegion = ::XCreateRegion();
  ::XUnionRegion(srca, copyRegion, dr_return);
}

/* Dispose of entries matching the given flags, compressing the GC cache */
void nsGCCacheXlib::Flush(unsigned long flags)
{
  while (!PR_CLIST_IS_EMPTY(&GCCache)) {
    PRCList *head = PR_LIST_HEAD(&GCCache);
    if (head == &GCCache)
      break;
    GCCacheEntryXlib *entry = (GCCacheEntryXlib *)head;
    if (entry->flags & flags)
      free_cache_entry(head);
  }
}

xGC *nsGCCacheXlib::GetGC(Display *display, Drawable drawable, unsigned long flags, XGCValues *gcv, Region clipRegion)
{
  PRCList *iter;
  GCCacheEntryXlib *entry;
  DEBUG_METER(int i = 0;)
  
  for (iter = PR_LIST_HEAD(&GCCache); iter != &GCCache;
       iter = PR_NEXT_LINK(iter)) {

    entry = (GCCacheEntryXlib *)iter;
    if (flags == entry->flags && 
        !memcmp (gcv, &entry->gcv, sizeof (*gcv))) {
      /* if there's a clipRegion, we have to match */

      if ((clipRegion && entry->clipRegion &&
           ::XEqualRegion(clipRegion, entry->clipRegion)) ||
          /* and if there isn't, we can't have one */
          (!clipRegion && !entry->clipRegion)) {

        /* move to the front of the list, if needed */
        if (iter != PR_LIST_HEAD(&GCCache)) {
          PR_REMOVE_LINK(iter);
          PR_INSERT_LINK(iter, &GCCache);
        }
        DEBUG_METER(GCCacheStats.hits[i]++;)

        entry->gc->AddRef();
        return entry->gc;
      }
    }
    DEBUG_METER(++i;)
  }
    
  /* might need to forcibly free the LRU cache entry */
  if (PR_CLIST_IS_EMPTY(&GCFreeList)) {
    DEBUG_METER(GCCacheStats.reclaim++);
    move_cache_entry(PR_LIST_TAIL(&GCCache));
  }

  DEBUG_METER(GCCacheStats.misses++;)
  
  iter = PR_LIST_HEAD(&GCFreeList);
  PR_REMOVE_LINK(iter);
  PR_INSERT_LINK(iter, &GCCache);
  entry = (GCCacheEntryXlib *)iter;

  if (!entry->gc) {
    // No old GC, greate new
    entry->gc = new xGC(display, drawable, flags, gcv);
    entry->gc->AddRef(); // addref the newly created xGC
    entry->flags = flags;
    entry->gcv = *gcv;
    entry->clipRegion = NULL;
    //printf("creating new gc=%X\n",entry->gc); 
  }
  else if (entry->gc->mRefCnt > 0) {
    // Old GC still in use, create new
    entry->gc->Release();
    entry->gc = new xGC(display, drawable, flags, gcv);
    entry->gc->AddRef(); // addref the newly created xGC
    entry->flags = flags;
    entry->gcv = *gcv;
    if (entry->clipRegion)
	XDestroyRegion(entry->clipRegion);
    entry->clipRegion = NULL;
    //printf("creating new (use)gc=%X\n",entry->gc); 
  }
  else {
    ReuseGC(entry, flags, gcv);
  }

  if (clipRegion) {
    entry->clipRegion = ::XCreateRegion();
    XCopyRegion(clipRegion, entry->clipRegion);
    if (entry->clipRegion)
      ::XSetRegion(display, entry->gc->mGC, entry->clipRegion);
    /* XXX what if it fails? */
  }
  
  entry->gc->AddRef();
  return entry->gc;
}

void nsGCCacheXlib::ReuseGC(GCCacheEntryXlib *entry, unsigned long flags, XGCValues *gcv)
{
  // We have old GC, reuse it and check what
  // we have to change

  if (entry->clipRegion) {
    // set it to none here and then set the clip region with
    // gdk_gc_set_clip_region in GetGC()
    gcv->clip_mask = None;
    flags |= GCClipMask;
    ::XDestroyRegion(entry->clipRegion);
    entry->clipRegion = NULL;
  }

  if (flags != 0) {
    ::XChangeGC(entry->gc->mDisplay, entry->gc->mGC,
                flags, gcv);
  }
  entry->flags = flags;  entry->gcv = *gcv;
}
