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
 */

#include <stdio.h>
#include "nsGCCache.h"
/* The GC cache is shared among all windows, since it doesn't hog
   any scarce resources (like colormap entries.) */

struct GCData nsGCCache::gc_cache[30];
int nsGCCache::gc_cache_fp = 0;
int nsGCCache::gc_cache_wrapped_p = 0;
GdkRegion *nsGCCache::copyRegion = NULL;

nsGCCache::nsGCCache()
{

}

nsGCCache::~nsGCCache()
{
  int i;
  int maxi = (gc_cache_wrapped_p ? countof(gc_cache) : gc_cache_fp);
  for (i = 0; i < maxi; i++) {
    gdk_gc_unref(gc_cache[i].gc);
    if (gc_cache[i].clipRegion)
      gdk_region_destroy(gc_cache[i].clipRegion);
  }
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

  int i, new_fp;

  int maxi = (gc_cache_wrapped_p ? countof (gc_cache) : gc_cache_fp);
  new_fp = 0;
  for (i = 0; i < maxi; i++) {
    if (gc_cache [i].flags & flags) {
      gdk_gc_unref(gc_cache [i].gc);
      if (gc_cache [i].clipRegion)
        gdk_region_destroy(gc_cache [i].clipRegion);
      memset (&gc_cache [i], 0,  sizeof (gc_cache [i]));
    }
    else
      gc_cache[new_fp++] = gc_cache[i];
  }
  if (new_fp == countof (gc_cache)) {
      gc_cache_wrapped_p = 1;
      gc_cache_fp = 0;
  } else {
    gc_cache_wrapped_p = 0;
    gc_cache_fp = new_fp;
  }
}

GdkGC *nsGCCache::GetGCFromDW(GdkWindow *window, GdkGCValues *gcv, GdkGCValuesMask flags, GdkRegion *clipRegion)
{
  //  static int numCalls = 0;
  int i;
  for (i = 0;
       i < (gc_cache_wrapped_p ? countof (gc_cache) : gc_cache_fp);
       i++)
  {
    if (flags == gc_cache [i].flags && 
        !memcmp (gcv, &gc_cache [i].gcv, sizeof (*gcv)))
      if (clipRegion) {
        if (gc_cache[i].clipRegion &&
            gdk_region_equal(clipRegion, gc_cache[i].clipRegion)) {
          //          printf("found GC in cache %i\n", ++numCalls);
          return gdk_gc_ref(gc_cache[i].gc);
        }
      } else {
        if(!gc_cache[i].clipRegion)
          //          printf("found GC in cache (no region) %i\n", ++numCalls);
          return gdk_gc_ref(gc_cache[i].gc);
      }
  }
  
  {
    GdkGC *gc;
    int this_slot = gc_cache_fp;
    int clear_p = gc_cache_wrapped_p;
    
    gc_cache_fp++;
    if (gc_cache_fp >= countof (gc_cache)) {
      gc_cache_fp = 0;
      gc_cache_wrapped_p = 1;
    }
    
    if (clear_p) {
      gdk_gc_unref(gc_cache [this_slot].gc);
      if (gc_cache [this_slot].clipRegion)
        gdk_region_destroy(gc_cache [this_slot].clipRegion);
      gc_cache [this_slot].gc = NULL;
      gc_cache [this_slot].clipRegion = NULL;
    }

    gc = gdk_gc_new_with_values(window, gcv, flags);

    gc_cache [this_slot].flags = flags;
    gc_cache [this_slot].gcv = *gcv;
    gc_cache [this_slot].clipRegion = NULL;
    if (clipRegion) {
      gc_cache [this_slot].clipRegion = gdk_region_copy(clipRegion);
      
      if (gc_cache [this_slot].clipRegion) {
        gdk_gc_set_clip_region(gc, gc_cache [this_slot].clipRegion);
      }
    }

    gc_cache [this_slot].gc = gc;
    
    return gdk_gc_ref(gc);
  }
}


