/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Mike Shaver <shaver@zeroknowledge.com>
 *   Tomi Leppikangas <Tomi.Leppikangas@oulu.fi>
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

#include <stdio.h>
#include "nsGCCache.h"
#include "nsISupportsUtils.h"
#include <gdk/gdkx.h>
#include <gdk/gdkprivate.h>
#include <X11/Xlib.h>
/* The GC cache is shared among all windows, since it doesn't hog
   any scarce resources (like colormap entries.) */

GdkRegion *nsGCCache::copyRegion = NULL;

MOZ_DECL_CTOR_COUNTER(nsGCCache)

nsGCCache::nsGCCache()
{
  MOZ_COUNT_CTOR(nsGCCache);
  PR_INIT_CLIST(&GCCache);
  PR_INIT_CLIST(&GCFreeList);
  for (int i = 0; i < GC_CACHE_SIZE; i++) {
    GCCacheEntry *entry = new GCCacheEntry();
    entry->gc=NULL;
    PR_INSERT_LINK(&entry->clist, &GCFreeList);
  }
  DEBUG_METER(memset(&GCCacheStats, 0, sizeof(GCCacheStats));)
}

/* static */ void
nsGCCache::Shutdown()
{
    if (copyRegion) {
        gdk_region_destroy(copyRegion);
        copyRegion = nsnull;
    }
}

void
nsGCCache::move_cache_entry(PRCList *clist)
{
  /* thread on the freelist, at the front */
  PR_REMOVE_LINK(clist);
  PR_INSERT_LINK(clist, &GCFreeList);
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

  MOZ_COUNT_DTOR(nsGCCache);

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
              int total = hits + GCCacheStats.misses;
              float percent = float(float(hits) / float(total));
              percent *= 100;
              fprintf(stderr, "\n\thits: %d, misses: %d, hit percent: %f%%\n", 
                      hits, GCCacheStats.misses, percent);
              );
}

#ifdef MOZ_WIDGET_GTK

GdkRegion *
nsGCCache::gdk_region_copy(GdkRegion *region)
{
  if (!copyRegion) copyRegion = gdk_region_new();

  return gdk_regions_union(region, copyRegion);
}

#endif

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
    move_cache_entry(PR_LIST_TAIL(&GCCache));
  }

  DEBUG_METER(GCCacheStats.misses++;)
  
  iter = PR_LIST_HEAD(&GCFreeList);
  PR_REMOVE_LINK(iter);
  PR_INSERT_LINK(iter, &GCCache);
  entry = (GCCacheEntry *)iter;

  if (!entry->gc) {
    // No old GC, greate new
    entry->gc = gdk_gc_new_with_values(window, gcv, flags);
    entry->flags = flags;
    entry->gcv = *gcv;
    entry->clipRegion = NULL;
    //printf("creating new gc=%X\n",entry->gc); 
  }
#ifdef MOZ_WIDGET_GTK
  else if ( ((GdkGCPrivate*)entry->gc)->ref_count > 1 ) {
#endif /* MOZ_WIDGET_GTK */
#ifdef MOZ_WIDGET_GTK2
  else if ( G_OBJECT(entry->gc)->ref_count > 1 ) {
#endif /* MOZ_WIDGET_GTK2 */
    // Old GC still in use, create new
    gdk_gc_unref(entry->gc);
    entry->gc=gdk_gc_new_with_values(window, gcv, flags);
    entry->flags = flags;
    entry->gcv = *gcv;
    entry->clipRegion = NULL;
    //printf("creating new (use)gc=%X\n",entry->gc); 
  }
  else {
    ReuseGC(entry, gcv, flags);
  }

  if (clipRegion) {
    entry->clipRegion = gdk_region_copy(clipRegion);
    if (entry->clipRegion)
      gdk_gc_set_clip_region(entry->gc, entry->clipRegion);
    /* XXX what if it fails? */
  }
  
  return gdk_gc_ref(entry->gc);
}

void nsGCCache::ReuseGC(GCCacheEntry *entry, GdkGCValues *gcv, GdkGCValuesMask flags)
{
  // We have old GC, reuse it and check what
  // we have to change

  GdkGCValues xvalues;
  int xvalues_mask = 0;

  if (entry->clipRegion) {
    // set it to none here and then set the clip region with
    // gdk_gc_set_clip_region in GetGC()
    xvalues.clip_mask = None;
    xvalues_mask |= GDK_GC_CLIP_MASK;
    gdk_region_destroy(entry->clipRegion);
    entry->clipRegion = NULL;
  }

  if (entry->gcv.foreground.pixel != gcv->foreground.pixel) {
    xvalues.foreground.pixel = gcv->foreground.pixel;
    xvalues_mask |= GDK_GC_FOREGROUND;
  }

  if (entry->gcv.function != gcv->function) {
    xvalues.function = gcv->function;
    xvalues_mask |= GDK_GC_FUNCTION;
  }

  if(entry->gcv.font != gcv->font && flags & GDK_GC_FONT) {
    xvalues.font = gcv->font;
    xvalues_mask |= GDK_GC_FONT;
  }

  if (entry->gcv.line_style != gcv->line_style) {
    xvalues.line_style = gcv->line_style;
    xvalues_mask |= GDK_GC_LINE_STYLE;
  }

  if (xvalues_mask != 0) {
    gdk_gc_set_values(entry->gc, &xvalues, (GdkGCValuesMask)xvalues_mask);
  }

  entry->flags = flags;
  entry->gcv = *gcv;
}
