/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* 
 *  cl_comp.c - The compositor API
 */


/* On UNIX, child and sibling windows are always clipped, so
   cutout layers are unnecessary. */
#ifdef XP_UNIX
#    define TRIM_UPDATE_REGION_CUTOUTS PR_FALSE
#else
#    define TRIM_UPDATE_REGION_CUTOUTS PR_TRUE
#endif

#include "prtypes.h"
#ifdef XP_MAC
#include "prpriv.h"
#else
#include "private/prpriv.h" /* For PR_NewNamedMonitor */
#endif

#include "xp_core.h"
#include "xpassert.h"
#include "fe_proto.h"		/* For FE_SetTimeout(), et al */
#include "layers.h"
#include "cl_priv.h"
#ifdef CL_ADAPT_FRAME_RATE
#    include "prtime.h"
#endif

#ifndef ABS
#    define ABS(x)  (((x) < 0) ? -(x) : (x))
#endif

static void
cl_compute_update_region(CL_Compositor *compositor);

static PRBool
cl_composite(CL_Compositor *compositor, PRBool cutoutp);

int cl_trace_level = 0;

#if !defined(XP_MAC) && defined(DEBUG) /* FE_HighlightRegion is not currently in the mac build */
void
CL_HighlightRegion(CL_Compositor *compositor, FE_Region region)
{
    extern MWContext *LO_CompositorToContext(CL_Compositor *compositor);

    int time;
    MWContext *context = LO_CompositorToContext(compositor);

    time = 200;
#if defined(XP_UNIX)
    if (getenv("SHOWREGIONS"))
        time = atoi(getenv("SHOWREGIONS"));
#endif

    FE_HighlightRegion(context, region, time);
}
#endif

/* Allocate and initialize a new compositor */
CL_Compositor *
CL_NewCompositor(CL_Drawable *primary_drawable,
                 CL_Drawable *backing_store,
                 int32 x_offset, int32 y_offset,
                 int32 width, int32 height,
                 uint32 frame_rate)
{
    CL_Compositor *compositor;

    compositor = XP_NEW_ZAP(CL_Compositor);

    if (compositor == NULL)
        return NULL;

    CL_TRACE(0, ("Creating new compositor: %x", compositor));

#ifdef CL_THREAD_SAFE
    compositor->monitor = PR_NewNamedMonitor("compositor-monitor");
    if (!compositor->monitor)
        return NULL;
#endif

    compositor->gen_id = 0;
    compositor->primary_drawable = primary_drawable;
    compositor->backing_store = backing_store;
    compositor->x_offset = x_offset;
    compositor->y_offset = y_offset;
    compositor->window_size.left = compositor->window_size.top = 0;
    compositor->window_size.right = width;
    compositor->window_size.bottom = height;
    if (frame_rate)
        compositor->frame_period = 1000000.0F / frame_rate;
    else
        compositor->frame_period = 0.0F;

    compositor->window_region = FE_CreateRectRegion(&compositor->window_size);
    compositor->update_region = FE_CreateRegion();

    compositor->event_containment_lists[0].layer_list = 
        XP_ALLOC(CL_CONTAINMENT_LIST_SIZE * sizeof(CL_Layer *));
    compositor->event_containment_lists[0].list_size = 
        CL_CONTAINMENT_LIST_SIZE;
    compositor->event_containment_lists[0].list_head = -1;
    compositor->event_containment_lists[1].layer_list = 
        XP_ALLOC(CL_CONTAINMENT_LIST_SIZE * sizeof(CL_Layer *));
    compositor->event_containment_lists[1].list_size = 
        CL_CONTAINMENT_LIST_SIZE;
    compositor->event_containment_lists[1].list_head = -1;

    compositor->backing_store_region = FE_CreateRegion();
    if ((compositor->window_region == NULL) ||
        (compositor->update_region == NULL) ||
        (compositor->backing_store_region == NULL) ||
        (compositor->event_containment_lists[0].layer_list == NULL) ||
        (compositor->event_containment_lists[1].layer_list == NULL)) {
        CL_DestroyCompositor(compositor);
        return NULL;
    }
    compositor->offscreen_enabled = PR_FALSE;
    compositor->back_to_front_only = PR_FALSE;

    compositor->mouse_event_grabber = NULL;
    compositor->key_event_grabber = NULL;
    compositor->last_mouse_event_grabber = NULL;
    compositor->focus_policy = CL_FOCUS_POLICY_CLICK;

    primary_drawable->compositor = compositor;
    if (backing_store)
        backing_store->compositor = compositor;

    cl_InitDrawable(compositor->primary_drawable);

    return compositor;
}

static int
cl_group_list_destroy(PRHashEntry *he,
                      int i,
                      void *arg)
{
    XP_List *list = (XP_List *)he->value;
    
    XP_ListDestroy(list);
    
    return HT_ENUMERATE_REMOVE;
}


/* Free an existing compositor */
void 
CL_DestroyCompositor(CL_Compositor *compositor)
{
    XP_ASSERT(compositor);

    if (!compositor)
        return;

    CL_TRACE(0, ("Destroying compositor: %x", compositor));

    if (compositor->composite_timeout)
        FE_ClearTimeout(compositor->composite_timeout);

    if (compositor->root)
        CL_DestroyLayerTree(compositor->root);

    if (compositor->window_region)
        FE_DestroyRegion(compositor->window_region);
  
    if (compositor->update_region)
        FE_DestroyRegion(compositor->update_region);

    if (compositor->backing_store_region)
        FE_DestroyRegion(compositor->backing_store_region);

    if (compositor->group_table) {
        /* Delete all the group lists in the table */
        PR_HashTableEnumerateEntries(compositor->group_table,
                                     cl_group_list_destroy,
                                     NULL);
        PR_HashTableDestroy(compositor->group_table);
    }

    if (compositor->primary_drawable) {
        cl_RelinquishDrawable(compositor->primary_drawable);
        CL_DestroyDrawable(compositor->primary_drawable);
    }

    if (compositor->backing_store) {
        if (compositor->offscreen_initialized)
            cl_RelinquishDrawable(compositor->backing_store);
        CL_DestroyDrawable(compositor->backing_store);
    }

    if (compositor->event_containment_lists[0].layer_list)
        XP_FREE(compositor->event_containment_lists[0].layer_list);

    if (compositor->event_containment_lists[1].layer_list)
        XP_FREE(compositor->event_containment_lists[1].layer_list);
    
#ifdef CL_THREAD_SAFE
    if (compositor->monitor)
        PR_DestroyMonitor(compositor->monitor);
#endif
  
    XP_FREE(compositor);
}

void
CL_IncrementCompositorGeneration(CL_Compositor *compositor)
{
    XP_ASSERT(compositor);
    
    if (!compositor)
        return;
    
    compositor->gen_id++;
}

/* Find layer by name. This carries out a breadth-first search */
/* of the layer tree. Returns NULL if no such layer exists.    */
CL_Layer *
CL_FindLayer(CL_Compositor *compositor, char *name)
{
    XP_List *list;
    CL_Layer *layer, *child;

    XP_ASSERT(compositor);
    XP_ASSERT(name);

    if (!compositor || !name || !compositor->root)
        return NULL;

    /* BUGBUG This is rather an expensive way to maintain a list */
    /* since a little struct is allocated for each element added */
    /* to the list.                                              */
    list = XP_ListNew();

    if (list == NULL)
        return NULL;

    LOCK_COMPOSITOR(compositor);
  
    XP_ListAddObject(list, compositor->root);

    /* While the list is not empty */
    while ((layer = XP_ListRemoveTopObject(list))) {
	
        /* Check if the name of this layer matches */
        if (layer->name && strcmp(layer->name, name) == 0) {
            XP_ListDestroy(list);
            UNLOCK_COMPOSITOR(compositor);
            return layer;
        }
	
        /* Add each of the children to the end of the list */
        for (child = layer->top_child; child; child = child->sib_below)
            XP_ListAddObjectToEnd(list, child);
    }

    XP_ListDestroy(list);
    UNLOCK_COMPOSITOR(compositor);
    return NULL;
}

static void
cl_refresh_window_region_common(CL_Compositor *compositor, 
                                FE_Region refresh_region,
                                PRBool copy_region)
{
    FE_Region draw_region, save_update_region, copy_refresh_region;
    CL_Drawable *backing_store;
    PRBool save_offscreen_inhibited;
    
    XP_ASSERT(compositor);
    XP_ASSERT(refresh_region);

    if (!compositor || !refresh_region)
        return;
    
    CL_TRACE(0, ("Refreshing window region"));

    LOCK_COMPOSITOR(compositor);

#if 0
    /* Subtract out the cutout layers, since they are out-of-bounds
       for compositor drawing. */
    if (compositor->enabled) {
	cl_prepare_to_draw(compositor, refresh_region, PR_FALSE);
    }
#endif

/*    if (compositor->root)
        CL_HighlightRegion(compositor, refresh_region);*/

    if (compositor->enabled)
        cl_compute_update_region(compositor);

   /* Invalidate any parts of the backing store for which there are
       update requests. */
    FE_SubtractRegion(compositor->backing_store_region,
                      compositor->update_region,
                      compositor->backing_store_region);

    /* Figure out what part of the refresh region has already been
       composited into the backing store and blit that onto the
       primary drawable. */
    if (compositor->backing_store) {
        backing_store = cl_LockDrawableForRead(compositor->backing_store);
        if (backing_store) {
            draw_region = FE_CreateRegion();
            if (! draw_region) {
                UNLOCK_COMPOSITOR(compositor);
                return;
            }
            
            FE_IntersectRegion(compositor->backing_store_region, refresh_region, draw_region);
            cl_CopyPixels(backing_store, compositor->primary_drawable,
                          draw_region);
            FE_SubtractRegion(refresh_region, draw_region, refresh_region);
            FE_DestroyRegion(draw_region);
            cl_UnlockDrawable(backing_store);
        }
    }

    if (compositor->enabled) {
        /* Save out the current update region, since we only want to draw
           the area that we got a refresh request for.  We can't simply
           subtract out the refresh area from the update area because
           of Windows' behavior: In between the BeginPaint() and EndPaint()
           calls, the clip is constrained to be that of the OS's update
           region intersected with the clip that we set.  But, we don't know
           what the resulting cascaded clip. */
        save_update_region = compositor->update_region;

        /* We might have to copy the refresh region, since compositing will
           modify it. */
        if (copy_region) {
            copy_refresh_region = FE_CreateRegion();
            FE_CopyRegion(refresh_region, copy_refresh_region);
            compositor->update_region = copy_refresh_region;
        }
        else
            compositor->update_region = refresh_region;

        /* Subtract out the area that isn't in the composited region */
        FE_IntersectRegion(compositor->window_region, 
                           compositor->update_region,
                           compositor->update_region);

        /* For speedier scrolling and exposures, we always composite
           on-screen for refresh. */
        save_offscreen_inhibited = compositor->offscreen_inhibited;
        compositor->offscreen_inhibited = PR_TRUE;

        /* 
         * Note that we don't subtract out the cutout region when we're
         * going through this entry point, since it can be a performance
         * hog, especially if we're scrolling. This works for Windows and
         * X, but I'm not sure it will work across platforms.
         */
        cl_composite(compositor, PR_FALSE);
        compositor->offscreen_inhibited = save_offscreen_inhibited;

        /* 
         * If a layer has changed during the compositing, the update
         * region will reflect the changes and will need to be dealt
         * with in the next compositing pass. Copy it back into the
         * saved update region.
         */
        FE_UnionRegion(save_update_region, compositor->update_region,
                       save_update_region);

        if (copy_region)
            FE_DestroyRegion(copy_refresh_region);

        compositor->update_region = save_update_region;
    }
    else
        FE_UnionRegion(compositor->update_region, refresh_region,
                       compositor->update_region);

    UNLOCK_COMPOSITOR(compositor);
}


/*
 * Inform the compositor that some part of the window needs to be
 * refreshed, e.g. in response to a window expose event.  The region
 * to be refreshed is expressed in window coordinates.
 */
void
CL_RefreshWindowRegion(CL_Compositor *compositor, FE_Region refresh_region)
{
    cl_refresh_window_region_common(compositor, refresh_region, PR_TRUE);
}

/*
 * Inform the compositor that some part of the window needs to be
 * refreshed, e.g. in response to a window expose event.  The region
 * to be refreshed is expressed in window coordinates.
 */
void
CL_RefreshWindowRect(CL_Compositor *compositor, XP_Rect *refresh_rect)
{
    FE_Region refresh_region = FE_CreateRectRegion(refresh_rect);

    if (! refresh_region)       /* OOM */
        return;
    
    cl_refresh_window_region_common(compositor, refresh_region, PR_FALSE);
    FE_DestroyRegion(refresh_region);
}

/*
 * Inform the compositor that some part of a layer has changed and
 * needs to be redrawn. The region to be updated is expressed in the
 * layer's coordinate system. For efficiency, only the part of the
 * passed in region that is not clipped by ancestor layers is actually
 * updated.  If update_now is PR_TRUE, the composite is done
 * synchronously. Otherwise, it is done at the next timer callback.
 */
void 
CL_UpdateLayerRegion(CL_Compositor *compositor, CL_Layer *layer,
                     FE_Region layer_region, PRBool update_now)
{
    int32 x_offset, y_offset;
    XP_Rect win_clipped_bbox;
    FE_Region update_region, win_clipped_bbox_region;
    
    XP_ASSERT(compositor);
    XP_ASSERT(layer);
    XP_ASSERT(layer_region);

    if (!compositor || !layer || !layer_region)
        return;

    /* Oddball predicate below is used to combat the case where the
       layer was made invisible, updated and made visible again, all
       between two composite cycles.  In that case, we still want
       to repaint the composited area. */
    if (!layer->prev_visible && !layer->visible)
        return;

    CL_TRACE(0, ("Updating layer region"));

    LOCK_COMPOSITOR(compositor);

    /* Compute offsets to convert layer coordinates to window coordinates */
    x_offset = layer->x_origin - compositor->x_offset;
    y_offset = layer->y_origin - compositor->y_offset;
    
    /* 
     * If the offsets are outside the coordinate range we can express
     * for regions, we can assume that the region to be updated is
     * outside the window rectangle.
     */
    if ((ABS(x_offset) > FE_MAX_REGION_COORDINATE) ||
        (ABS(y_offset) > FE_MAX_REGION_COORDINATE)) {
        UNLOCK_COMPOSITOR(compositor);
        return;
    }
    
    /* Convert input region from layer coordinates into window coordinates */
    update_region = FE_CreateRegion();
    
    XP_ASSERT(update_region);
    if (! update_region) {
        UNLOCK_COMPOSITOR(compositor);
        return;
    }
    
    FE_CopyRegion(layer_region, update_region);
    FE_OffsetRegion(update_region, x_offset, y_offset);

    /* Convert clipping bbox of layer from document coordinates to
       window coordinates and clip to window bounds. */
    XP_CopyRect(&layer->clipped_bbox, &win_clipped_bbox);
    XP_OffsetRect(&win_clipped_bbox,
                  -compositor->x_offset, -compositor->y_offset);
    XP_IntersectRect(&compositor->window_size,
                     &win_clipped_bbox, 
                     &win_clipped_bbox);

    /* Make the clipping bbox into a region, so we can do intersection */
    win_clipped_bbox_region = FE_CreateRectRegion(&win_clipped_bbox);
    XP_ASSERT(win_clipped_bbox_region);
    if (! win_clipped_bbox_region) {
        FE_DestroyRegion(update_region);
        UNLOCK_COMPOSITOR(compositor);
        return;
    }

    /* Subtract out the area that isn't visible in the layer */
    FE_IntersectRegion(win_clipped_bbox_region, update_region, update_region);
    FE_DestroyRegion(win_clipped_bbox_region);

    if (FE_IsEmptyRegion(update_region)) {
        FE_DestroyRegion(update_region);
        UNLOCK_COMPOSITOR(compositor);
        return;
    }
    
    /* Add the altered region to the region to update at the next composite */
    FE_UnionRegion(compositor->update_region, update_region, 
                   compositor->update_region);

    FE_DestroyRegion(update_region);

    if (compositor->enabled) {
        if (update_now)
            cl_composite(compositor, TRIM_UPDATE_REGION_CUTOUTS);
        else
            cl_start_compositor_timeouts(compositor);
    }
    UNLOCK_COMPOSITOR(compositor);
}

/*
 * Inform the compositor that some rectangular part of a layer has
 * changed and needs to be redrawn. The region to be updated is
 * expressed in the layer's coordinate system. For efficiency, only
 * the part of the passed in region that is not clipped by ancestor
 * layers is actually updated.  If update_now is PR_TRUE, the
 * composite is done synchronously. Otherwise, it is done at the next
 * timer callback.
 */
void
CL_UpdateLayerRect(CL_Compositor *compositor, CL_Layer *layer,
                   XP_Rect *layer_rect, PRBool composite_now)
{
    int32 x_offset, y_offset;
    XP_Rect update_rect;

    XP_ASSERT(compositor);
    XP_ASSERT(layer);
    XP_ASSERT(layer_rect);

    if (!compositor || !layer || !layer_rect)
        return;

    /* Oddball predicate below is used to combat the case where the
       layer was made invisible, updated and made visible again, all
       between two composite cycles.  In that case, we still want
       to repaint the composited area. */
    if (!layer->prev_visible && !layer->visible)
        return;

    CL_TRACE(0, ("Updating layer rect"));

    XP_CopyRect(layer_rect, &update_rect);
    
    LOCK_COMPOSITOR(compositor);
    
    /* Compute offsets to convert layer coordinates to document coordinates */
    x_offset = layer->x_origin;
    y_offset = layer->y_origin;

    /* Convert the update_rect into document coordinates */
    XP_OffsetRect(&update_rect, x_offset, y_offset);
    
    /* Don't update parts of the layer that are clipped by ancestor layers. */
    XP_IntersectRect(&update_rect, &layer->clipped_bbox, &update_rect);

    CL_UpdateDocumentRect(compositor, &update_rect, composite_now);

    UNLOCK_COMPOSITOR(compositor);
}

/*
 * Inform the compositor that some rectangular part of a document has
 * changed and needs to be redrawn. The region to be updated is
 * expressed in the document's coordinate system.  If update_now is
 * PR_TRUE, the composite is done synchronously. Otherwise, it is done
 * at the next timer callback.
 */
void
CL_UpdateDocumentRect(CL_Compositor *compositor,
                      XP_Rect *doc_rect, PRBool update_now)
{
    int32 x_offset, y_offset;
    XP_Rect update_rect;
    FE_Region update_region;
    
    XP_ASSERT(compositor);
    XP_ASSERT(doc_rect);

    if (!compositor || !doc_rect)
        return;
    
    CL_TRACE(0, ("Updating document rect"));

    XP_CopyRect(doc_rect, &update_rect);

    LOCK_COMPOSITOR(compositor);

    /* Compute offsets to convert document coordinates to window coordinates */
    x_offset = - compositor->x_offset;
    y_offset = - compositor->y_offset;
    
    /* Convert the update_rect into window coordinates */
    XP_OffsetRect(&update_rect, x_offset, y_offset);

    /* 
     * If there's no overlap between the update_rect and the
     * composited area, there's nothing to draw.
     */
    if (!XP_RectsOverlap(&compositor->window_size, &update_rect)) {
        UNLOCK_COMPOSITOR(compositor);
        return;
    }
    
    /* Subtract out the area that isn't in the composited rect */
    XP_IntersectRect(&compositor->window_size, &update_rect,
                     &update_rect);

    update_region = FE_CreateRectRegion(&update_rect);

    XP_ASSERT(update_region);
    if (!update_region) {
        UNLOCK_COMPOSITOR(compositor);
        return;
    }
    
    /* 
     * Add the altered region to the region to update at 
     * the next composite 
     */
    FE_UnionRegion(compositor->update_region, update_region, 
                   compositor->update_region);

    FE_DestroyRegion(update_region);

    if (compositor->enabled) {
        if (update_now)
            cl_composite(compositor, TRIM_UPDATE_REGION_CUTOUTS);
        else
            cl_start_compositor_timeouts(compositor);
    }
    UNLOCK_COMPOSITOR(compositor);
}

/* Update the entire contents of a layer and its children. */
void
cl_UpdateLayer(CL_Layer *layer, PRBool update_now)
{
    if (! layer->compositor)
        return;

    /* Update the layer */
    CL_UpdateDocumentRect(layer->compositor, &layer->clipped_bbox, PR_FALSE);

    /* Update the layer's children, but only if there's a possibility
       that some children are not clipped by this layer, otherwise
       we've already updated with the statement above. */
    if (!layer->clip_children) {
        CL_Layer *child;
        for (child = layer->top_child; child; child = child->sib_below) {
            cl_UpdateLayer(child, PR_FALSE);
        }
    }

    if (layer->compositor->enabled) {
        if (update_now)
            cl_composite(layer->compositor, TRIM_UPDATE_REGION_CUTOUTS);
        else
            cl_start_compositor_timeouts(layer->compositor);
    }
}

/*
The decision whether to draw onscreen or offscreen is made once for
every composite cycle using these rules:

 1) If compositor->offscreen_inhibited is set, drawing is always
    done onscreen.
 2) If compositor->offscreen_enabled is set, drawing is always done
    offscreen.
 3) Otherwise, the offscreen mode depends on the drawing-mode flags
    set for all the layers that fall within the update-region.

    A layer is either neutral or biased towards drawing onscreen or
    drawing offscreen, as indicated by the CL_PREFER_DRAW_ONSCREEN and
    CL_PREFER_DRAW_OFFSCREEN flags.

    For rule 3, the set of all layers that falls within the bounding box
    of the update-region is examined to determine the drawing mode:

    3a) If no layer has the CL_PREFER_DRAW_OFFSCREEN flag set, all drawing
	is performed onscreen.
    3b) Otherwise, bottom-up drawing is performed offscreen until reaching
	a layer that has CL_PREFER_DRAW_ONSCREEN set.  Then the offscreen
	damaged area is BLTed onscreen and remaining layers are drawn onscreen.
    
Top-down drawing is handled differently: It always occurs onscreen unless
CL_PREFER_DRAW_OFFSCREEN is set for an individual layer.
*/

static void
cl_setup_offscreen(CL_Compositor *compositor)
{
    if (!compositor->backing_store)
        return;
    
    if (compositor->offscreen_enabled && !compositor->offscreen_inhibited) {
        if (!compositor->offscreen_initialized) {
            int width = compositor->window_size.right-
                compositor->window_size.left;
            int height = compositor->window_size.bottom-
                compositor->window_size.top;
            cl_InitDrawable(compositor->backing_store);
            cl_SetDrawableDimensions(compositor->backing_store, width, height);
            compositor->offscreen_initialized = PR_TRUE;
        }
    } else if (compositor->offscreen_initialized) {
#ifdef THRASH_OFFSCREEN
        cl_RelinquishDrawable(compositor->backing_store);
        compositor->offscreen_initialized = PR_FALSE;
#endif
    }
}

static void
cl_compute_update_region_recurse(CL_Compositor *compositor,
                                 CL_Layer *layer,
                                 FE_Region update_region)
{
    CL_Layer *child;
    int32 x_offset, y_offset;
    FE_Region layer_update_region;
    XP_Rect *win_clipped_bbox;
    XP_Rect prev_win_clipped_bbox;
    
    PRBool visibility_changed, bbox_changed, position_changed;

    /* Find out what properties of the layer have changed since the
       last time compositing occurred. */
    visibility_changed = layer->visible != layer->prev_visible;
    bbox_changed = !XP_EqualRect(&layer->clipped_bbox,
                                 &layer->prev_clipped_bbox);
    x_offset = layer->x_origin - layer->prev_x_origin;
    y_offset = layer->y_origin - layer->prev_y_origin;
    position_changed = (x_offset || y_offset);

    /* If layer bbox changed, call client-defined callback. */
    if (bbox_changed && layer->vtable.bbox_changed_func) {
        XP_Rect bbox_copy;
        XP_CopyRect(&layer->bbox, &bbox_copy);
        (*layer->vtable.bbox_changed_func)(layer, &bbox_copy);
    }

    /* If visibility changed, call client-defined callback. */
    if (visibility_changed && layer->vtable.visibility_changed_func)
        (*layer->vtable.visibility_changed_func)(layer, layer->visible);

    /* If layer origin changed, call client-defined callback. */
    if (position_changed && layer->vtable.position_changed_func)
        (*layer->vtable.position_changed_func)(layer, x_offset, y_offset);

    /* Compute the bounding box, in window coordinates, of the part of
       the layer that isn't clipped by ancestors and which lies within
       the bounding box of the window. */
    win_clipped_bbox = &layer->win_clipped_bbox;
    XP_CopyRect(&layer->clipped_bbox, win_clipped_bbox);
    XP_OffsetRect(win_clipped_bbox,
                  -compositor->x_offset, -compositor->y_offset);
    XP_IntersectRect(win_clipped_bbox, &compositor->window_size,
                     win_clipped_bbox);

    XP_CopyRect(&layer->prev_clipped_bbox, &prev_win_clipped_bbox);
    XP_OffsetRect(&prev_win_clipped_bbox,
                  -compositor->x_offset, -compositor->y_offset);
    XP_IntersectRect(&prev_win_clipped_bbox, &compositor->window_size,
                     &prev_win_clipped_bbox);

    layer_update_region = FE_NULL_REGION;
    if (visibility_changed) {
        if (layer->visible) {
            if (!layer->cutout) {
                layer_update_region = FE_CreateRectRegion(win_clipped_bbox);
                XP_ASSERT(layer_update_region);
            }
        } else {
            layer_update_region = FE_CreateRectRegion(&prev_win_clipped_bbox);
            XP_ASSERT(layer_update_region);
        }

    } else if (layer->visible && (bbox_changed || position_changed)) {
        FE_Region win_clipped_region, prev_win_clipped_region;

        win_clipped_region = FE_CreateRectRegion(win_clipped_bbox);
        prev_win_clipped_region = FE_CreateRectRegion(&prev_win_clipped_bbox);

        if (win_clipped_region && prev_win_clipped_region) {
            layer_update_region = FE_CreateRegion();

            if (layer->cutout)
                FE_SubtractRegion(prev_win_clipped_region,
                                  win_clipped_region,
                                  layer_update_region);
            else if (bbox_changed && !position_changed)
                cl_XorRegion(win_clipped_region,
                             prev_win_clipped_region,
                             layer_update_region);
            else
                FE_UnionRegion(win_clipped_region,
                               prev_win_clipped_region,
                               layer_update_region);
        }
                               
        if (win_clipped_region)
            FE_DestroyRegion(win_clipped_region);
        if (prev_win_clipped_region)
            FE_DestroyRegion(prev_win_clipped_region);
        XP_ASSERT(layer_update_region);
    }
        
    if (layer_update_region != FE_NULL_REGION) {
        FE_UnionRegion(update_region, layer_update_region, update_region);
        FE_DestroyRegion(layer_update_region);
    }
    
    /* Set layer properties for subsequent composite cycle */
    layer->prev_visible = layer->visible;
    XP_CopyRect(&layer->clipped_bbox, &layer->prev_clipped_bbox);
    layer->prev_x_origin = layer->x_origin;
    layer->prev_y_origin = layer->y_origin;

    for (child = layer->top_child; child; child = child->sib_below)
        cl_compute_update_region_recurse(compositor, child, update_region);
}

static void
cl_compute_update_region(CL_Compositor *compositor)
{
    /* We may need to recompute the update region iteratively since
       layer callbacks may result in changes to layer properties, such
       as bbox, visibility and position. */
    while (compositor->recompute_update_region) {
        compositor->recompute_update_region = PR_FALSE;
        cl_compute_update_region_recurse(compositor, compositor->root,
                                         compositor->update_region);
    }
}

/* Pre-drawing layer initialization in front-to-back order.  (This
   could be folded into cl_draw_front_to_back, but then we would need
   to duplicate this functionality when in back_to_front_only mode. */
static int
cl_prepare_to_draw_recurse(CL_Compositor *compositor,
                           CL_Layer *layer, PRBool cutoutp,
                           XP_Rect *update_bbox,
                           PRBool *prefer_draw_offscreen_p)
{
    CL_Layer *child;
    XP_Rect *win_clipped_bbox;
    int descendant_draw_needed = 0;

    /* Debugging assert to make sure that these regions are
       destroyed at the end of the composite cycle */
    XP_ASSERT(layer->draw_region == FE_NULL_REGION);
    
    /* Check if the layer is eligible for compositing.  A layer won't
       be composited if it is hidden, or it is entirely clipped by
       ancestors. */
    if (! layer->visible) {
        layer->draw_needed = FALSE;
        if (!layer->descendant_visible || layer->cutout) {
            layer->descendant_draw_needed = FALSE;
            return FALSE;
        }
    } else {
        layer->draw_needed = (layer->vtable.painter_func != NULL);
    }

    /* Compute the bounding box, in window coordinates, of the part of
       the layer that isn't clipped by ancestors and which lies within
       the bounding box of the update region. */
    win_clipped_bbox = &layer->win_clipped_bbox;
    XP_CopyRect(&layer->clipped_bbox, win_clipped_bbox);
    XP_OffsetRect(win_clipped_bbox,
                  -compositor->x_offset, -compositor->y_offset);
    XP_IntersectRect(win_clipped_bbox, update_bbox, win_clipped_bbox);

    if (XP_IsEmptyRect(win_clipped_bbox)) {
        layer->draw_needed = PR_FALSE;
        if (!layer->descendant_visible || layer->cutout) {
            layer->descendant_draw_needed = PR_FALSE;
            return FALSE;
        }
    }

    /* If this is a cutout layer, add the layer's area to the
       off-limits drawing region for the compositor */
    if (layer->cutout) {
        if (cutoutp) {
            FE_Region win_clipped_region = FE_CreateRectRegion(win_clipped_bbox);
            
            if (! win_clipped_region)	/* OOM check */
                return 0;
            
            FE_UnionRegion(compositor->cutout_region,
                           win_clipped_region,
                           compositor->cutout_region);
            FE_DestroyRegion(win_clipped_region);
        }

        /* Cutout-layers can't have children and don't draw themselves. */
        layer->draw_needed = PR_FALSE;
        layer->descendant_draw_needed = PR_FALSE;
        return 0;
    }

    /* Recurse for children.
       First get the children to draw in front to back order. */
    for (child = layer->top_child; child; child = child->sib_below) {
        descendant_draw_needed |=
            cl_prepare_to_draw_recurse(compositor, child, cutoutp, update_bbox,
                                       prefer_draw_offscreen_p);
    }
    layer->descendant_draw_needed = (PRBool)descendant_draw_needed;

    /* If the layer must draw offscreen then do the entire composite
       offscreen. */
    if ((layer->draw_needed || descendant_draw_needed) && layer->prefer_draw_offscreen)
        *prefer_draw_offscreen_p = PR_TRUE;


    return (int)layer->draw_needed | descendant_draw_needed;
}

static void
cl_prepare_to_draw(CL_Compositor *compositor, FE_Region update_region, 
                   PRBool cutoutp, PRBool *prefer_draw_offscreen_p)
{
    XP_Rect update_bbox;

    compositor->cutout_region = FE_CreateRegion();
    if (!compositor->cutout_region) /* OOM */
	return;

    FE_GetRegionBoundingBox(update_region, &update_bbox);

    cl_prepare_to_draw_recurse(compositor, compositor->root, cutoutp,
                               &update_bbox, prefer_draw_offscreen_p);
    
    if (cutoutp)
        FE_SubtractRegion(update_region, compositor->cutout_region,
                          update_region);
    FE_DestroyRegion(compositor->cutout_region);
}

/* The front-to-back drawing phase
   Yech!  Let's get rid of all these arguments. */
static PRBool
cl_draw_front_to_back_recurse(CL_Compositor *compositor,
                              CL_Layer *layer,
                              CL_Layer **top_undrawn_layerp,
                              CL_Layer **bottom_undrawn_layerp,
                              CL_Drawable *backing_store,
                              PRBool *offscreen_layer_drawn_above,
                              FE_Region update_region,
                              FE_Region transparent_above,
                              FE_Region unobscured_by_opaque,
                              FE_Region offscreen_region)
{
    FE_Region overlap;
    CL_Layer *child;
    PRBool layer_is_unobscured_by_transparent_layers, done;
    CL_Drawable *drawable;
    int32 old_x_origin, old_y_origin;

    if (layer->descendant_draw_needed) {

        /* First get the children to draw in front to back order. */
        done = PR_FALSE;
        for (child = layer->top_child; child && !done; child = child->sib_below) {
            done = cl_draw_front_to_back_recurse(compositor, child,
                                                 top_undrawn_layerp,
                                                 bottom_undrawn_layerp,
                                                 backing_store,
                                                 offscreen_layer_drawn_above,
                                                 update_region,
                                                 transparent_above,
                                                 unobscured_by_opaque,
                                                 offscreen_region);
        }

        /* Have we reached the last layer in the front-to-back pass ? */
        if (done)
            return PR_TRUE;
    }

    if (!layer->draw_needed)
        return PR_FALSE;

    /* Compute the layer's clipping region for drawing. */
    layer->draw_region = FE_CreateRectRegion(&layer->win_clipped_bbox);
    FE_IntersectRegion(unobscured_by_opaque, 
                       layer->draw_region, 
                       layer->draw_region);

    /* If the layer is wholly obscured by opaque layers in front of it,
       there's nothing to draw. */
    if (FE_IsEmptyRegion(layer->draw_region)) {
        layer->draw_needed = PR_FALSE;
        if (layer->clip_children) {
            layer->descendant_draw_needed = PR_FALSE;
        }
        FE_DestroyRegion(layer->draw_region);
        layer->draw_region = FE_NULL_REGION;
        return PR_FALSE;
    }

    if (layer->opaque) {

        overlap = FE_CreateRegion();
        FE_IntersectRegion(transparent_above, layer->draw_region, overlap);
        layer_is_unobscured_by_transparent_layers = FE_IsEmptyRegion(overlap);
        FE_DestroyRegion(overlap);

        /* Are there any non-opaque layers which must draw on top of this
           layer ? */
        if (layer_is_unobscured_by_transparent_layers) {
            
            /* Figure out if we're drawing to the backing store or not */
            if (layer->prefer_draw_onscreen || !backing_store) {
                drawable = compositor->primary_drawable;
            } else {
                drawable = backing_store;
                FE_UnionRegion(layer->draw_region, 
                               offscreen_region, 
                               offscreen_region);
            }

            /* Set the drawing origin and clip for this layer. */
            cl_GetDrawableOrigin(drawable, &old_x_origin, &old_y_origin);
            cl_SetDrawableOrigin(drawable, layer->x_origin, layer->y_origin);
            cl_SetDrawableClip(drawable, layer->draw_region);
 
            /* Draw the layer's contents */
            (*layer->vtable.painter_func)(drawable,
                                          layer, layer->draw_region);

            cl_SetDrawableOrigin(drawable, old_x_origin, old_y_origin);
            
            /* No other layers either above or below this one need to
               update the screen area occupied by this layer.  Any
               layers are either opaque overlapping layers that have
               already been drawn in the front-to-back phase, or they
               are below this layer and will therefore be obscured by it. */
            FE_SubtractRegion(update_region,
                              layer->draw_region, update_region);

            /* We don't need to draw this layer in the back-to-front phase,
               since we've already drawn it. */
            layer->draw_needed = PR_FALSE;
        } else {

            /* This is the bottommost layer that needs drawing, and which
               has not yet been drawn */
            *bottom_undrawn_layerp = layer;

            if (! *top_undrawn_layerp)
                *top_undrawn_layerp = layer;

            layer->offscreen_layer_drawn_above = *offscreen_layer_drawn_above;
            *offscreen_layer_drawn_above |= (int)layer->prefer_draw_offscreen;
        }

        FE_SubtractRegion(unobscured_by_opaque, layer->draw_region,
                          unobscured_by_opaque);

        /* If we've already drawn the layer, get rid of the draw region */
        if (layer_is_unobscured_by_transparent_layers) {
            FE_DestroyRegion(layer->draw_region);
            layer->draw_region = FE_NULL_REGION;
        }
        
        /* When the entire remaining update region is obscured by
           opaque layers above it, there's nothing left to do. */
        if (FE_IsEmptyRegion(unobscured_by_opaque)) {
            return PR_TRUE;
        }
    } else {
        FE_UnionRegion(transparent_above, layer->draw_region, 
                       transparent_above);
        *bottom_undrawn_layerp = layer;
        if (! *top_undrawn_layerp)
            *top_undrawn_layerp = layer;
        layer->offscreen_layer_drawn_above = *offscreen_layer_drawn_above;
        *offscreen_layer_drawn_above |= (int)layer->prefer_draw_offscreen;
    }

    return PR_FALSE;
}

/* The front-to-back drawing phase */
static void
cl_draw_front_to_back(CL_Compositor *compositor,
                      CL_Layer **top_undrawn_layerp,
                      CL_Layer **bottom_undrawn_layerp,
                      CL_Drawable *backing_store,
                      FE_Region update_region,
                      FE_Region offscreen_region)
{ 
    FE_Region transparent_above, unobscured_by_opaque;   
    PRBool offscreen_layer_drawn_above = PR_FALSE;

    transparent_above = FE_CreateRegion();
    if (! transparent_above) {
        XP_ASSERT(0);      /* OOM */
        return;
    }
         
    unobscured_by_opaque = FE_CopyRegion(update_region, NULL);
    if (! unobscured_by_opaque) {
        XP_ASSERT(0);      /* OOM */
        FE_DestroyRegion(transparent_above);
        return;
    }
    cl_draw_front_to_back_recurse(compositor, compositor->root,
                                  top_undrawn_layerp, bottom_undrawn_layerp,
                                  backing_store,
                                  &offscreen_layer_drawn_above,
                                  update_region,
                                  transparent_above,
                                  unobscured_by_opaque,
                                  offscreen_region);
    FE_DestroyRegion(transparent_above);
    FE_DestroyRegion(unobscured_by_opaque);
}

typedef struct cl_back_to_front_state 
{
    CL_Compositor *compositor;
    CL_Layer *top_undrawn_layer;
    CL_Layer *bottom_undrawn_layer;
    PRBool *onscreen_layer_drawn_below;
    FE_Region update_region;
    CL_Drawable *backing_store;
    FE_Region offscreen_region;
} cl_back_to_front_state;

/* The back-to-front drawing phase */
static CL_BackToFrontStatus
cl_draw_back_to_front_recurse(CL_Layer *layer,
                              cl_back_to_front_state *s,
                              CL_BackToFrontStatus status)
{
    CL_Layer *child;
    CL_Drawable *drawable;
    int32 old_x_origin, old_y_origin;
    PRBool *onscreen_layer_drawn_below = s->onscreen_layer_drawn_below;
    CL_Drawable *backing_store = s->backing_store;
    CL_Compositor *compositor = s->compositor;

    /* Only draw if a draw is necessary and we've already reached
     * the bottommost layer that needs to be drawn.
     */
    if (layer->draw_needed && ((status == CL_REACHED_BOTTOM_UNDRAWN) || 
                               (layer == s->bottom_undrawn_layer))) {

        /* Clip drawing to the bounds of itself and any ancestors */
        if (!layer->draw_region) {
            layer->draw_region = FE_CreateRectRegion(&layer->win_clipped_bbox);
            FE_IntersectRegion(s->update_region, layer->draw_region, 
                               layer->draw_region);
        }

        /* If any layers beneath this one were drawn onscreen, then this
           layer needs to be drawn there also. */
        if (!backing_store || *onscreen_layer_drawn_below) {
            drawable = compositor->primary_drawable;
            *onscreen_layer_drawn_below = PR_TRUE;
        } else {
            FE_Region offscreen_region = s->offscreen_region;
            
            /* If this layer and layers above this one are drawn
               onscreen, but all the layers beneath were drawn
               offscreen, we need to copy what we've painted so far
               from the backing store to the primary drawable. */
            if (layer->prefer_draw_onscreen &&
                !layer->offscreen_layer_drawn_above &&
                !*onscreen_layer_drawn_below) {
                
                /* The clip could have been modified during the
                   front-to-back phase, so clear it. */
                cl_SetDrawableClip(compositor->primary_drawable, NULL);
                cl_CopyPixels(backing_store, compositor->primary_drawable,
                              offscreen_region);
                FE_CLEAR_REGION(offscreen_region);
                drawable = compositor->primary_drawable;
                *onscreen_layer_drawn_below = PR_TRUE;
            } else {
                drawable = backing_store;
                FE_UnionRegion(offscreen_region, layer->draw_region, 
                               offscreen_region);
            }
        }

        /* Set the drawing origin and clip for this layer. */
        cl_GetDrawableOrigin(drawable, &old_x_origin, &old_y_origin);
        cl_SetDrawableOrigin(drawable, layer->x_origin, layer->y_origin);
        cl_SetDrawableClip(drawable, layer->draw_region);

        /* Draw the layer */
        (*layer->vtable.painter_func)(drawable, layer, layer->draw_region);

        cl_SetDrawableOrigin(drawable, old_x_origin, old_y_origin);

        FE_DestroyRegion(layer->draw_region);
        layer->draw_region = FE_NULL_REGION;

        /* If this layer is uniformly-colored, push it on the
           front-to-back stack of uniformly-colored layers. */
        if (layer->uniform_color) {
            XP_ASSERT(layer->opaque);
            layer->uniformly_colored_layer_below = 
                compositor->uniformly_colored_layer_stack;
            compositor->uniformly_colored_layer_stack = layer;
        }
        
        /* If we've reached the last layer that couldn't draw in the
           front to back phase, it's time to stop.              */
        if (layer == s->top_undrawn_layer)
            return CL_REACHED_TOP_UNDRAWN;

        /* If we've reached the bottommost (first) layer that needs to be
         * drawn, the rest of the layers can start drawing. */
        if (layer == s->bottom_undrawn_layer)
            status = CL_REACHED_BOTTOM_UNDRAWN;
    }

    if (layer->descendant_draw_needed) {
        
        /* Get the children to draw in back to front order. 
           Stop if we've reached the last layer to draw.      */
        for (child = layer->bottom_child; 
             child && (status != CL_REACHED_TOP_UNDRAWN) ; 
             child = child->sib_above) {
            status = cl_draw_back_to_front_recurse(child, s, status);
        }
        return status;
    }
    
    return status;
}

/* The back-to-front drawing phase */
static void
cl_draw_back_to_front(CL_Compositor *compositor, 
                      CL_Layer *top_undrawn_layer,
                      CL_Layer *bottom_undrawn_layer,
					  FE_Region update_region,
                      CL_Drawable *backing_store,
                      FE_Region offscreen_region)
{
    PRBool onscreen_layer_drawn_below = PR_FALSE;
    
    cl_back_to_front_state state;
    
    state.compositor = compositor;
    state.top_undrawn_layer = top_undrawn_layer;
    state.bottom_undrawn_layer = bottom_undrawn_layer;
    state.onscreen_layer_drawn_below = &onscreen_layer_drawn_below;
    state.update_region = update_region;
    state.backing_store = backing_store;
    state.offscreen_region = offscreen_region;

    if (compositor->back_to_front_only)
    cl_draw_back_to_front_recurse(compositor->root, &state, 
                                  CL_REACHED_BOTTOM_UNDRAWN);
    else
        cl_draw_back_to_front_recurse(compositor->root, &state, CL_REACHED_NOTHING);
}
    

/* Redraws all regions changed since last call to cl_composite().
 * This is called by the timer callback. It can be also be directly
 * called to force a synchronous composite.
 */
static PRBool
cl_composite(CL_Compositor *compositor, PRBool cutoutp)
{
    CL_Layer *top_undrawn_layer, *bottom_undrawn_layer;
    CL_Drawable *backing_store;
    FE_Region offscreen_region, update_region;
    PRBool save_offscreen_enabled, prefer_draw_offscreen = PR_FALSE;
    
    XP_ASSERT(compositor);

    if (!compositor || !compositor->root)
        return PR_FALSE;

    LOCK_COMPOSITOR(compositor);

    /* Check if there's a region to be drawn... */
    cl_compute_update_region(compositor);
    if (FE_IsEmptyRegion(compositor->update_region)) {
        UNLOCK_COMPOSITOR(compositor);
        return PR_FALSE;
    }

    {
        XP_Rect bbox;
        FE_GetRegionBoundingBox(compositor->update_region, &bbox);
        CL_TRACE(0, ("Compositing rectangle [%d, %d, %d, %d]",
                     bbox.left, bbox.top, bbox.right, bbox.bottom));
    }

    XP_ASSERT(!compositor->composite_in_progress);
    compositor->composite_in_progress = PR_TRUE;

     /* 
      * Copy the current update region and then clear it out. The layer
      * painter funcs might actually perform operations that add to the
      * update regions and we don't want to lose those updates.
      */
     update_region = FE_CopyRegion(compositor->update_region, NULL);
     FE_CLEAR_REGION(compositor->update_region);

     /* Initialize temporary per-layer drawing variables.  Also determine
        whether this composite must be done offscreen. */
     cl_prepare_to_draw(compositor, update_region, cutoutp,
                        &prefer_draw_offscreen);

     if (prefer_draw_offscreen) {
         save_offscreen_enabled = compositor->offscreen_enabled;
         compositor->offscreen_enabled = PR_TRUE;
     }
     cl_setup_offscreen(compositor);

     /* Determine whether or not we'll use a backing store */
     if (compositor->offscreen_inhibited || !compositor->offscreen_enabled ||
         !compositor->backing_store) {
         /* FIXME - what happens if we've disabled the backing store,
            but some layers have prefer_draw_offscreen set ? */
         backing_store = NULL;
         offscreen_region = FE_NULL_REGION;
         
         /* Invalidate the parts of the backing store for which there are
            update requests. */
         FE_SubtractRegion(compositor->backing_store_region,
                           compositor->update_region,
                           compositor->backing_store_region);
     } else {
         backing_store = cl_LockDrawableForReadWrite(compositor->backing_store);
         if (!backing_store) {
             /* Couldn't read the backing store.  Invalidate its contents. */
             FE_CLEAR_REGION(compositor->backing_store_region);
             backing_store = cl_LockDrawableForWrite(compositor->backing_store);
         }

         /* Assume that there is a painter for every area of the drawable to 
            be updated. */
         if (backing_store)
             offscreen_region = FE_CreateRegion();
         
     }

     compositor->uniformly_colored_layer_stack = NULL;
     top_undrawn_layer = bottom_undrawn_layer = NULL;

     /* Front-to-back phase */
     if (!compositor->back_to_front_only) {
         cl_draw_front_to_back(compositor,
                               &top_undrawn_layer, &bottom_undrawn_layer,
                               backing_store,
                               update_region,
                               offscreen_region);
     }

     /* Back-to-front phase */
     if (top_undrawn_layer || compositor->back_to_front_only) {
         cl_draw_back_to_front(compositor,
                               top_undrawn_layer,
                               bottom_undrawn_layer,
                               update_region,
                               backing_store,
                               offscreen_region);
     }
    
     /* Transfer the composited pixels from the backing store to
        the onscreen drawable. */
	 cl_SetDrawableClip(compositor->primary_drawable, NULL);
     if (backing_store) {
         if (!FE_IsEmptyRegion(offscreen_region)) {
             /* The clip could have been modified during the
                front-to-back phase, so clear it. */
             cl_CopyPixels(backing_store, compositor->primary_drawable,
                           offscreen_region);
         }

         cl_UnlockDrawable(backing_store);

         FE_DestroyRegion(offscreen_region);
     }

     /* If this composite was forced offscreen, restore the previous state */
     if (prefer_draw_offscreen)
         compositor->offscreen_enabled = save_offscreen_enabled;

     /* Get rid of the temporary copy */
     FE_DestroyRegion(update_region);

     /* Set the clip region back to its old value */
     cl_RestoreDrawableClip(compositor->primary_drawable);
     
     compositor->composite_in_progress = PR_FALSE;
     
     UNLOCK_COMPOSITOR(compositor);

	 return PR_TRUE;
}

/* External API */
void
CL_CompositeNow(CL_Compositor *compositor)
{
    cl_composite(compositor, TRIM_UPDATE_REGION_CUTOUTS);
}

/* 
 * Indicates that the window the compositor is drawing to has been resized.
 * We make the assumption that any refreshing will be carried out by 
 * calling CL_RefreshWindowRect.
 */
void 
CL_ResizeCompositorWindow(CL_Compositor *compositor, int32 width, int32 height)
{
    XP_ASSERT(compositor);
  
    if (!compositor)
        return;
    
    LOCK_COMPOSITOR(compositor);

    compositor->window_size.right = compositor->window_size.left + width;
    compositor->window_size.bottom = compositor->window_size.top + height;

    if (compositor->window_region) 
        FE_DestroyRegion(compositor->window_region);

    compositor->window_region = FE_CreateRectRegion(&compositor->window_size);

    if (compositor->offscreen_initialized && compositor->backing_store)
        cl_SetDrawableDimensions(compositor->backing_store, width, height);

    UNLOCK_COMPOSITOR(compositor);
}

/* 
 * Called when the compositor's window scrolls. We make the assumption 
 * that any refreshing will be carried out by calling CL_RefreshWindowRect
 */
void 
CL_ScrollCompositorWindow(CL_Compositor *compositor, 
                          int32 x_origin, int32 y_origin)
{
    int32 delta_x, delta_y;
    
    XP_ASSERT(compositor);
    
    if (!compositor)
        return;
    
    LOCK_COMPOSITOR(compositor);

#if defined(XP_WIN) && _MSC_VER >= 1100
    /* Hack to avoid optimizer bug in VC++ v5 */
    delta_x = (compositor->x_offset != x_origin ) ||
              (compositor->y_offset != y_origin );
    if ( delta_x ) {
#else
    if ((compositor->x_offset != x_origin) ||
        (compositor->y_offset != y_origin)) {
#endif
        
        /* Invalidate backing store.
           For better performance, perhaps we should be scrolling it, instead */
        FE_CLEAR_REGION(compositor->backing_store_region);

        delta_x = compositor->x_offset - x_origin;
        delta_y = compositor->y_offset - y_origin;

        /* Are we scrolling a small enough amount that some part of
           the current screen contents may remain onscreen ? */
        if ((ABS(delta_x) < FE_MAX_REGION_COORDINATE) &&
            (ABS(delta_y) < FE_MAX_REGION_COORDINATE)) {
            FE_Region update_region = compositor->update_region;
            FE_Region window_region = compositor->window_region;
            
            /* Scroll compositor update region */
            FE_OffsetRegion(update_region, delta_x, delta_y);
            
            FE_IntersectRegion(window_region, update_region, update_region);
        }

        compositor->x_offset = x_origin;
        compositor->y_offset = y_origin;
        
    }
    UNLOCK_COMPOSITOR(compositor);
}

/* Sets whether a compositor offscreen-drawing should occur or not */
/* If enabled is PR_TRUE, the compositor will use offscreen        */
/* drawing if applicable.                                          */
void
CL_SetCompositorOffscreenDrawing(CL_Compositor *compositor, 
                                 CL_OffscreenMode mode)
{
    XP_ASSERT(compositor);
    if (!compositor)
        return;

    switch (mode) {
    case CL_OFFSCREEN_ENABLED:
        compositor->offscreen_enabled = PR_TRUE;
        compositor->offscreen_inhibited = PR_FALSE;
        break;
    case CL_OFFSCREEN_DISABLED:
        compositor->offscreen_inhibited = PR_TRUE;
        break;
    case CL_OFFSCREEN_AUTO:
        compositor->offscreen_enabled = PR_FALSE;
        compositor->offscreen_inhibited = PR_FALSE;
        break;
    }
}

CL_OffscreenMode
CL_GetCompositorOffscreenDrawing(CL_Compositor *compositor)
{
    PRBool enabled;
    XP_ASSERT(compositor);
    if (! compositor)
        return PR_FALSE;

    if (compositor->offscreen_inhibited)
        return CL_OFFSCREEN_DISABLED;

    enabled = compositor->offscreen_enabled;
    return enabled ? CL_OFFSCREEN_ENABLED : CL_OFFSCREEN_AUTO;
}

/* Frame-rate excursion limits */
#define MAX_FRAME_PERIOD      (1000000.0F / CL_FRAME_RATE_MIN)
#define MIN_FRAME_PERIOD      (1000000.0F / CL_FRAME_RATE_MAX)
#define INITIAL_FRAME_PERIOD  (1000000.0F / CL_FRAME_RATE_INITIAL)

/* Timeout callback for time-based compositing.

   Adaptively adjust the frame-rate at which compositing is done based
   on the CPU load.  (CPU load is estimated by measuring the ability
   of recent composite timeouts to meet their deadlines.)
 */
static void
cl_compositor_callback(void *closure)
{
#ifdef CL_ADAPT_FRAME_RATE
    unsigned int i, index, filter_length, delay_line_index;
    int64 slack64, frame_period64, delay64, nominal_deadline64, now64;
    int32 slack, delay, slip, clamped_slack;
    double earliness, smoothed_slack, frame_period, adjust;
    
    CL_Compositor *compositor = (CL_Compositor *)closure;
    frame_period = compositor->frame_period;

	XP_ASSERT(compositor->frame_period != 0); /* should never be here in static case */
	if (compositor->frame_period == 0)
		return; /* to avoid divide by zero, since fundamentally should never be here */

    /* First, do the actual drawing work */
    if (cl_composite(compositor, TRIM_UPDATE_REGION_CUTOUTS) == PR_FALSE) {

		compositor->nothing_to_do_count++;

		if (compositor->nothing_to_do_count > 5) {

			/*
			 *    Clear the timeout, because we are in the closure
			 *    function, and we don't want another dangling timer.
			 */
			compositor->composite_timeout = NULL; /* VERY IMPORTANT */
			compositor->nothing_to_do_count = 0;
			return;
		}
	} else {
		compositor->nothing_to_do_count = 0;
	}

    /* Compute the "slack", which indicates how late (or early) the
       compositor was meeting its deadline.  Positive slack numbers
       indicate earliness.  Negative numbers lateness. */
    now64 = PR_Now();
    nominal_deadline64 = compositor->nominal_deadline64;
    LL_SUB(slack64, nominal_deadline64, now64);
    LL_L2I(slack, slack64);

    slack -= (int32)((1.0 - CL_CPU_LOAD) * frame_period);

    /* Clamp slack to maximum permitted amount of frame-period adjustment. */
    slip = 0;
    if (frame_period - slack > MAX_FRAME_PERIOD) {
        clamped_slack = (int32)(frame_period - MAX_FRAME_PERIOD);
        slip = clamped_slack - slack;
    }

    /* Store latest slack value in the delay line, a circular array
       that keeps track of the most recent slack values. */
    delay_line_index = compositor->delay_line_index;
    if (delay_line_index == DELAY_LINE_LENGTH) {
        /* First time we're running the timeout.  Initialize the delay line */
        for (i = 0; i < DELAY_LINE_LENGTH; i++)
            compositor->delay_line[i] = slack;
    } else {
        /* Push most recent slack value into circular delay line */
        compositor->delay_line[delay_line_index] = slack;
    }

    /* Compute the number of filter taps corresponding to the
       requested duration of the filter, assuming that the
       compositor's frame-rate changes slowly. */

	/* This should never happen, since we check for this up top. But just in case */	
	/* something changes in the future in the above code, this is here */	
	/* just to make sure we find a potential divide by zero quickly, and don't do it. 	
	XP_ASSERT(frame_period != 0); 

	if(frame_period == 0)
        filter_length = DELAY_LINE_LENGTH;
	else
	*/
	
	filter_length = (unsigned int) ((SLOW_FILTER_DURATION * 1000) / frame_period);

    if (filter_length > DELAY_LINE_LENGTH)
        filter_length = DELAY_LINE_LENGTH;
    if (filter_length < 4)
        filter_length = 4;

    /* Average the last <filter_length> values in the circular delay
       line to create a smoothed slow-response filtering of the slack
       values.  Create a "fast-response" filter that uses half as many
       elements as the "slow" one. */
    smoothed_slack = 0;
    earliness = 0;
    for (i = 0; i < filter_length; i++) {
        int32 slack_val;

        index = delay_line_index - i;
        index &= (DELAY_LINE_LENGTH - 1);
        slack_val = compositor->delay_line[index];
        smoothed_slack += slack_val;
        if (slack_val > 0)
            earliness += slack_val;
    }
    smoothed_slack /= filter_length;
    earliness /= filter_length;

    /* Update next index in circular delay-line */
    delay_line_index++;
    delay_line_index &= (DELAY_LINE_LENGTH - 1);
    compositor->delay_line_index = delay_line_index;

    adjust = earliness + ((smoothed_slack > 0) ? 0 : smoothed_slack * 0.35);

    /* Adjust the frame rate, but don't allow the frame rate to excur
       outside its preset limits. */
    if (adjust < 0) {
        /* Slack is decreasing, Let's decrease the CPU load by
           decreasing the frame rate. */
        frame_period -= 0.10 * adjust;
        if (frame_period > MAX_FRAME_PERIOD)
            frame_period = MAX_FRAME_PERIOD;
    } else {
        /* Slack is negative and decreasing.  (Really, we're
           finishing earlier and getting more so) ; Increase
           frame-rate to make use of available CPU. */
        frame_period -= 0.20 * adjust;
        if (frame_period < MIN_FRAME_PERIOD)
            frame_period = MIN_FRAME_PERIOD;
    }
    compositor->frame_period = (float)frame_period;

    /* Compute delay from now until start of next composite timeout */
    LL_SUB(delay64, nominal_deadline64, now64);
    LL_L2I(delay, delay64);
    delay = (delay + 500) / 1000; /* Convert from microseconds to ms */

    /* Compute nominal completion time for next composite timeout */
    LL_I2L(frame_period64, (int32)frame_period + slip);
    LL_ADD(nominal_deadline64, nominal_deadline64, frame_period64);
    compositor->nominal_deadline64 = nominal_deadline64;

    /* Impose minimum delay to prevent timeouts from being scheduled
       back-to-back */
    if (delay < CL_MIN_TIMEOUT)
        delay = CL_MIN_TIMEOUT;

    /* Schedule the next compositor timeout. */
    compositor->composite_timeout = 
        FE_SetTimeout((TimeoutCallbackFunction)cl_compositor_callback,
                      (void *)compositor,
                      (uint32)delay);

#else  /* !CL_ADAPT_FRAME_RATE */
    CL_Compositor *compositor = (CL_Compositor *)closure;
    cl_composite(compositor, TRIM_UPDATE_REGION_CUTOUTS);
    compositor->composite_timeout = 
        FE_SetTimeout((TimeoutCallbackFunction)cl_compositor_callback,
                      (void *)compositor, 0);
#endif /* CL_ADAPT_FRAME_RATE */
}

PRBool 
CL_GetCompositorEnabled(CL_Compositor *compositor)
{
    XP_ASSERT(compositor);
    
    if (!compositor)
        return PR_FALSE;
    
    return compositor->enabled;
}


void
cl_start_compositor_timeouts(CL_Compositor *compositor)
{
    int64 frame_period64, now64;

	XP_ASSERT(compositor);
	if (compositor == NULL)
		return;

	if (compositor->frame_period == 0) /* indicates static case, no timeouts... */
		return;

    /* No need to start timeouts if already started */
    if (compositor->composite_timeout)
        return;

    if (!compositor->enabled)
        return;

    /* Compute nominal time for next composite timeout */
    now64 = PR_Now();
    LL_I2L(frame_period64, (int32)compositor->frame_period);
    LL_ADD(compositor->nominal_deadline64, now64, frame_period64);

    /* Special value indicates empty delay line */
    compositor->delay_line_index = DELAY_LINE_LENGTH;
    compositor->composite_timeout = 
        FE_SetTimeout((TimeoutCallbackFunction)cl_compositor_callback,
                      (void *)compositor, 0);
}


/* Sets a compositor's enabled state. Disabling a compositor will  */
/* stop timed composites (but will not prevent explicit composites */
/* from happening). Enabling a compositor restarts timed draws.    */
void 
CL_SetCompositorEnabled(CL_Compositor *compositor, PRBool enabled)
{
    XP_ASSERT(compositor);

    if (!compositor)
        return;
    
    CL_TRACE(0, ("Setting compositor enabled state to %d ", enabled));

    if (enabled && (compositor->enabled == PR_FALSE) && compositor->frame_period) {
        cl_composite(compositor, TRIM_UPDATE_REGION_CUTOUTS);
        
        CL_TRACE(0, ("Starting compositor timeout"));
        cl_start_compositor_timeouts(compositor);
    }
    else if (!enabled && (compositor->enabled == PR_TRUE)) {
        if (compositor->composite_timeout)
            FE_ClearTimeout(compositor->composite_timeout);
        compositor->composite_timeout = NULL;
        if (compositor->offscreen_initialized) {
            cl_RelinquishDrawable(compositor->backing_store);
            compositor->offscreen_initialized = PR_FALSE;
        }
    }
	
    compositor->enabled = enabled;
}
    
CL_Drawable *
CL_GetCompositorDrawable(CL_Compositor *compositor)
{
    XP_ASSERT(compositor);
    
    if (!compositor)
        return NULL;
    
    return compositor->primary_drawable;
}

void 
CL_SetCompositorDrawable(CL_Compositor *compositor, 
                         CL_Drawable *drawable)
{
    XP_ASSERT(compositor);
    
    if (!compositor)
        return;
    
    compositor->primary_drawable = drawable;
    drawable->compositor = compositor;
}

CL_Layer *
CL_GetCompositorRoot(CL_Compositor *compositor)
{
    XP_ASSERT(compositor);
    
    if (!compositor)
        return NULL;

    return compositor->root;
}

/* Set a layer as the root of the layer tree */
void 
CL_SetCompositorRoot(CL_Compositor *compositor, CL_Layer *root)
{
    XP_ASSERT(compositor);

    if (!compositor)
        return;

    CL_TRACE(0,("Setting root of compositor %x to layer %x", compositor, root));

    LOCK_COMPOSITOR(compositor);
    
    compositor->root = root;

    if (root) {
        cl_SetCompositorRecursive(root, compositor);
        
        cl_LayerAdded(compositor, root);
    }
    
    UNLOCK_COMPOSITOR(compositor);
}

uint32 
CL_GetCompositorFrameRate(CL_Compositor *compositor)
{
    XP_ASSERT(compositor);
    
    if (!compositor)
        return 0;

    if (compositor->frame_period == 0)
        return 0;
    else
        return (uint32)((1000000.0 / compositor->frame_period) + 0.5);
}

void 
CL_SetCompositorFrameRate(CL_Compositor *compositor, uint32 frame_rate)
{
    XP_ASSERT(compositor);
  
    if (!compositor)
        return;

    LOCK_COMPOSITOR(compositor);

    if (frame_rate)
        compositor->frame_period = 1000000.0F / frame_rate;
    else
        compositor->frame_period = 0.0F;

    /* BUGBUG This is probably the wrong behavior. If we had better */
    /* timing services, we could do better.                         */
    if (compositor->enabled && compositor->frame_period) {
        if (compositor->composite_timeout) {
            FE_ClearTimeout(compositor->composite_timeout);
            compositor->composite_timeout = NULL;
        }
	
        cl_start_compositor_timeouts(compositor);
    }

    UNLOCK_COMPOSITOR(compositor);
}
    
int32 
CL_GetCompositorXOffset(CL_Compositor *compositor)
{
    XP_ASSERT(compositor);
    
    if (!compositor)
        return 0;

    return compositor->x_offset;
}

int32 
CL_GetCompositorYOffset(CL_Compositor *compositor)
{
    XP_ASSERT(compositor);
    
    if (!compositor)
        return 0;

    return compositor->y_offset;
}

void CL_GetCompositorWindowSize(CL_Compositor *compositor,
                                XP_Rect *window_size)
{
    XP_ASSERT(compositor);
    XP_ASSERT(window_size);
    
    if (!compositor || !window_size)
        return;

    LOCK_COMPOSITOR(compositor);
    *window_size = compositor->window_size;
    UNLOCK_COMPOSITOR(compositor);
}

static PRBool
cl_build_containment_list(CL_Compositor *compositor, CL_Layer *layer,
                          int32 which_list)
{
    int32 index = 0;
    CL_EventContainmentList *list = 
        &compositor->event_containment_lists[which_list];

    while (layer) {
        /* 
         * XXX We're assuming here that we don't have to do anything
         * special for Win16, since the depth of the layer tree will
         * hopefully not be > 8k (i.e. 32K segment boundary/4 bytes 
         * per pointer). Watch me eat my words one day.
         */
        if (index >= list->list_size) {
            list->list_size += CL_CONTAINMENT_LIST_INCREMENT;
            list->layer_list = XP_REALLOC(list->layer_list, list->list_size);
            if (list->layer_list == NULL)
                return PR_FALSE;
        }

        list->layer_list[index++] = layer;
        layer = CL_GetLayerParent(layer);
    } 

    list->list_head = index-1;

    return PR_TRUE;
}

/*
 * This function checks for a change in either mouse or keyboard focus.
 * For mouse events, the focus has changed if the last layer to get
 * mouse events is different from the current one. In that case, we
 * build a containment list for the two layers. A containment list is
 * just a glorified name for a list consisting of the path from the
 * root of the layer tree down to the layer. Consider the following
 * example where layer C is the old mouse grabber and layer E the new
 * one.
 *                           root
 *                            /\
 *                           /  ...
 *                          A
 *                          |
 *                          B 
 *                         / \
 *                        C   D
 *                            |
 *                            E
 *            
 * In this case the containment list for layer C is root-A-B-C and the
 * containment list for layer E is root-A-B-D-E. We trace down both 
 * lists till we reach a point where there is a divergence. In this 
 * example, this point is below layer B. For the sub-tree rooted at B
 * that holds the old mouse grabber (layer C) we send MOUSE_LEAVE events
 * and for the sub-tree that holds the new mouse grabber (layer E) we
 * send MOUSE_ENTER events. This ensures that MOUSE_LEAVE and MOUSE_ENTER
 * events are sent for to all layers for which leaving and entering has
 * happened.
 */
static void
cl_check_focus_change(CL_Compositor *compositor, CL_Layer *new_grabber, 
                      CL_Event *mouse_event)
{    
    PRBool mouse_enter_occured = PR_FALSE;

    if (new_grabber != compositor->last_mouse_event_grabber) {
        CL_Event crossing_event;
        CL_Layer *last_grabber = compositor->last_mouse_event_grabber;
        int32 old_list_num = compositor->last_containment_list;
        int32 new_list_num = CL_NEXT_CONTAINMENT_LIST(old_list_num);
        int32 old_list_index, new_list_index;
        CL_EventContainmentList *old_list, *new_list;

        old_list = &compositor->event_containment_lists[old_list_num];
        new_list = &compositor->event_containment_lists[new_list_num];
        
        /* Build the containment list for the new mouse grabber */
        if (!cl_build_containment_list(compositor, 
                                       new_grabber, 
                                       new_list_num))
            return;
        
        old_list_index = old_list->list_head;
        new_list_index = new_list->list_head;

        /* 
         * Starting from the root, we trace downward in the tree, until
         * we find a divergence in the path.
         */
        while ((old_list_index >= 0) && (new_list_index >= 0) &&
               (old_list->layer_list[old_list_index] == 
                new_list->layer_list[new_list_index])) {
            old_list_index--;
            new_list_index--;
        }
               
        crossing_event.x = crossing_event.y = 0;
        
        /* 
         * Send a mouse leave event to all layers that the mouse
         * has moved out of. 
         */
        while (old_list_index >= 0) {
            CL_Layer *layer = old_list->layer_list[old_list_index];
            
            if (layer && (layer->vtable.event_handler_func != NULL)) {
                crossing_event.type = CL_EVENT_MOUSE_LEAVE;
                crossing_event.which = (uint32)new_grabber;
                crossing_event.fe_event = NULL;
                crossing_event.x = mouse_event->x;
                crossing_event.y = mouse_event->y;
                crossing_event.modifiers = 0;
                (*layer->vtable.event_handler_func)(layer, &crossing_event);
            }
            old_list_index--;
        }
        
        
        /* 
         * Send a mouse enter to all the new layers into which the mouse
         * has moved.
         */
        while (new_list_index >= 0) 
        {
            CL_Layer *layer = new_list->layer_list[new_list_index];
            
            if (layer && (layer->vtable.event_handler_func != NULL)) {
                crossing_event.type = CL_EVENT_MOUSE_ENTER;
                crossing_event.which = (uint32)last_grabber;
                crossing_event.fe_event = NULL;
                crossing_event.x = mouse_event->x;
                crossing_event.y = mouse_event->y;
                crossing_event.modifiers = 0;
                (*layer->vtable.event_handler_func)(layer, &crossing_event);
                mouse_enter_occured = PR_TRUE;
            }
            new_list_index--;
        }

        compositor->last_containment_list = new_list_num;
        compositor->last_mouse_event_grabber = new_grabber;
    }
  
    /* 
     * Check for a keyboard focus change, depending on the policy
     */
    if ((((compositor->focus_policy == CL_FOCUS_POLICY_CLICK) &&
          (mouse_event->type == CL_EVENT_MOUSE_BUTTON_DOWN) &&
          (mouse_event->which == 1)) ||
         ((compositor->focus_policy == CL_FOCUS_POLICY_MOUSE_ENTER) &&
          mouse_enter_occured))  &&
        (new_grabber != compositor->key_event_grabber)) {
        CL_Event keyboard_focus_event;
        CL_Layer *last_grabber = compositor->key_event_grabber;
        PRBool accepted = PR_TRUE;
       
        keyboard_focus_event.x = keyboard_focus_event.y
				= keyboard_focus_event.modifiers = 0;
        
        /* Tell the new guy that he's gained focus */
        if (new_grabber &&
            (new_grabber->vtable.event_handler_func != NULL)){
            keyboard_focus_event.type = CL_EVENT_KEY_FOCUS_GAINED;
            keyboard_focus_event.which = (uint32)last_grabber;
            keyboard_focus_event.fe_event = NULL;
            accepted = (*new_grabber->vtable.event_handler_func)(new_grabber,
                                                      &keyboard_focus_event);
        }

        if (accepted) {
            /* Tell the old event focus holder that it's lost focus */
            if (last_grabber &&
                (last_grabber->vtable.event_handler_func != NULL)){
                keyboard_focus_event.type = CL_EVENT_KEY_FOCUS_LOST;
                keyboard_focus_event.which = (uint32)new_grabber;
                keyboard_focus_event.fe_event = NULL;
                (*last_grabber->vtable.event_handler_func)(last_grabber, 
                                                        &keyboard_focus_event);
            }

            compositor->key_event_grabber = new_grabber;
        }
    }
}


static PRBool 
cl_front_to_back_event_dispatch(CL_Compositor *compositor, CL_Layer *layer,
                                CL_Event *event)
{
    CL_Layer *child;
    PRBool event_grabbed = PR_FALSE;
    uint32 old_gen_id;

    old_gen_id = compositor->gen_id;
    
    if (!layer->hidden) {
        /* 
         * First we ask the children if they're interested.
         * Stop if one of them grabs the event.
         */
        for (child = layer->top_child; 
             child && !event_grabbed; 
             child = child->sib_below) {
            event_grabbed = cl_front_to_back_event_dispatch(compositor, 
                                                            child, event);
        }
        
        /* 
         * Now check if the event occurred within this layer.
         * If so, call the event handler if one exists 
         */
        if (!event_grabbed &&
            XP_PointInRect(&layer->clipped_bbox, event->x, event->y) &&
            (layer->vtable.event_handler_func != NULL)) {

            event->x -= layer->x_origin;
            event->y -= layer->y_origin;

            event_grabbed = (*layer->vtable.event_handler_func)(layer, event);

            /*
             * If something in the event handler caused the gen_id to
             * fail, then we just bail out of here and hope that we
             * do nothing else that will access the old state of the
             * compositor.
             */
            if (compositor->gen_id != old_gen_id)
                return PR_TRUE;
            
            /* 
             * Check for mouse entering/leaving for layers. If
             * the current event grabber (for mouse events) is not the 
             * same as the last one, we synthesize the appropriate event.
             */
            if (event_grabbed && CL_IS_MOUSE_EVENT(event))
                cl_check_focus_change(compositor, layer, event);

            event->x += layer->x_origin;
            event->y += layer->y_origin;
        }

        return event_grabbed;
    }
    
    return PR_FALSE;
}

/* Dispatch an event to the correct layer */
PRBool
CL_DispatchEvent(CL_Compositor *compositor, CL_Event *event)
{
    CL_Layer *layer;
    uint32 old_gen_id;

    XP_ASSERT(compositor);
    XP_ASSERT(event);

    if (!compositor || !event || !compositor->root)
        return PR_FALSE;

    if (event->type == CL_EVENT_MOUSE_MOVE) {
        compositor->last_mouse_x = event->x;
        compositor->last_mouse_y = event->y;
        compositor->last_mouse_button = event->which;
    }
    else if (event->type == CL_EVENT_MOUSE_BUTTON_DOWN) 
        compositor->last_mouse_button = event->which;

    old_gen_id = compositor->gen_id;
           
    if (compositor->mouse_event_grabber && CL_IS_MOUSE_EVENT(event)) {
        layer = compositor->mouse_event_grabber;
        if (layer->vtable.event_handler_func) {
            PRBool ret;
            
            event->x -= layer->x_origin;
            event->y -= layer->y_origin;
            ret = ((*layer->vtable.event_handler_func)(layer, event));

            if (compositor->gen_id != old_gen_id)
                return ret;

            /* Check for a focus change */
            if (ret)
                cl_check_focus_change(compositor, layer, event);

            return ret;
        }
    }
    else if (compositor->key_event_grabber && CL_IS_KEY_EVENT(event)) {
        layer = compositor->key_event_grabber;
        if (layer->vtable.event_handler_func) {
            event->x -= layer->x_origin;
            event->y -= layer->y_origin;
            return ((*layer->vtable.event_handler_func)(layer, event));
        }
    }
    else
        return cl_front_to_back_event_dispatch(compositor,
                                               compositor->root,
                                               event);

    return PR_FALSE;
}

/* All mouse events go to this layer. If layer is NULL, stop grabbing */
PRBool
CL_GrabMouseEvents(CL_Compositor *compositor, CL_Layer *layer)
{
    XP_ASSERT(compositor);
    
    if (!compositor)
        return PR_FALSE;

    compositor->mouse_event_grabber = layer;
    return PR_TRUE;
}

/* All key events go to this layer. If layer is NULL, stop grabbing */
PRBool
CL_GrabKeyEvents(CL_Compositor *compositor, CL_Layer *layer)
{
    CL_Event keyboard_focus_event;
    CL_Layer *last_grabber, *new_grabber;
    PRBool accepted = PR_TRUE;

    XP_ASSERT(compositor);
    
    if (!compositor)
        return PR_FALSE;

    keyboard_focus_event.x = keyboard_focus_event.y = 0;
    
    last_grabber = compositor->key_event_grabber;
    new_grabber = layer;

    if (new_grabber &&
        (new_grabber->vtable.event_handler_func != NULL)){
        keyboard_focus_event.type = CL_EVENT_KEY_FOCUS_GAINED;
        keyboard_focus_event.which = (uint32)last_grabber;
        keyboard_focus_event.fe_event = NULL;
        accepted = (*new_grabber->vtable.event_handler_func)(new_grabber,
                                                       &keyboard_focus_event);
    }

    if (accepted) {
        if (last_grabber &&
            (last_grabber->vtable.event_handler_func != NULL)){
            keyboard_focus_event.type = CL_EVENT_KEY_FOCUS_LOST;
            keyboard_focus_event.which = (uint32)new_grabber;
            keyboard_focus_event.fe_event = NULL;
            (*last_grabber->vtable.event_handler_func)(last_grabber, 
                                                       &keyboard_focus_event);
        }

        compositor->key_event_grabber = layer;
        return PR_TRUE;
    }
    else
        return PR_FALSE;
}

/* Returns PR_TRUE if layer is the mouse event grabber. */
PRBool
CL_IsMouseEventGrabber(CL_Compositor *compositor, CL_Layer *layer)
{
    XP_ASSERT(compositor);
    
    if (!compositor)
        return PR_FALSE;
    
    return (PRBool)(compositor->mouse_event_grabber == layer);
}

/* Returns PR_TRUE if layer is the key event grabber. */
PRBool
CL_IsKeyEventGrabber(CL_Compositor *compositor, CL_Layer *layer)
{
    XP_ASSERT(compositor);
    
    if (!compositor)
        return PR_FALSE;
    
    return (PRBool)(compositor->key_event_grabber == layer);
}

/* Returns PR_TRUE if layer is the key event grabber. */
CL_Layer *
CL_GetKeyEventGrabber(CL_Compositor *compositor)
{
    XP_ASSERT(compositor);
    
    if (!compositor)
        return NULL;
    
    return (compositor->key_event_grabber);
}

void
CL_SetKeyboardFocusPolicy(CL_Compositor *compositor, 
                          CL_KeyboardFocusPolicy focus_policy)
{
    XP_ASSERT(compositor);
    
    if (!compositor)
        return;
    
    compositor->focus_policy = focus_policy;
}

void 
CL_SetCompositorDrawingMethod(CL_Compositor *compositor,
                              CL_DrawingMethod method)
{
    XP_ASSERT(compositor);
    
    if (!compositor)
        return;
    
    compositor->back_to_front_only = 
        (method == CL_DRAWING_METHOD_BACK_TO_FRONT_ONLY);
}


/* Called when a layer is added to the layer tree */
void
cl_LayerAdded(CL_Compositor *compositor, CL_Layer *layer)
{
    XP_ASSERT(compositor);
    XP_ASSERT(layer);

    cl_ParentChanged(layer);

    /* If this child is visible, ask the compositor to update its area */
    if (layer->visible)
        cl_UpdateLayer(layer, PR_FALSE);
}

void
cl_LayerDestroyed(CL_Compositor *compositor, CL_Layer *layer)
{
    XP_ASSERT(compositor);
    XP_ASSERT(layer);

    if (!compositor || !layer)
        return;
    
    if (layer == compositor->mouse_event_grabber)
        compositor->mouse_event_grabber = NULL;
    if (layer == compositor->key_event_grabber)
        compositor->key_event_grabber = NULL;
    if (layer == compositor->last_mouse_event_grabber) {
		compositor->event_containment_lists[compositor->last_containment_list].list_head = -1;
        compositor->last_mouse_event_grabber = NULL;
    }

    compositor->gen_id++;
}

/* Called when a layer is removed from the layer tree */
void
cl_LayerRemoved(CL_Compositor *compositor, CL_Layer *layer)
{
    XP_ASSERT(compositor);
    XP_ASSERT(layer);

    if (!compositor || !layer)
        return;

    /* If this child was visible, ask the compositor
       to update the area it formerly occupied. */
    if (layer->visible)
        cl_UpdateLayer(layer, PR_FALSE);

    if (layer == compositor->last_mouse_event_grabber) {
        compositor->event_containment_lists[compositor->last_containment_list].list_head = -1;
        compositor->last_mouse_event_grabber = NULL;
    }
}

void
cl_LayerMoved(CL_Compositor *compositor, CL_Layer *layer)
{

    /* The auto-generation of mousemoves is playing havoc with the
     * REAL mouse moves.  Commented out until further notice. */
    if (0) {
	/*
	 * If the layer that grabbed mouse events has moved, the
	 * movement is equivalent to a mouse move.
	 */
	if (compositor && compositor->mouse_event_grabber == layer) {
	    CL_Event event;
        
	    event.type = CL_EVENT_MOUSE_MOVE;
	    event.fe_event = NULL;
	    event.x = compositor->last_mouse_x - layer->x_offset;
	    event.y = compositor->last_mouse_y - layer->y_offset;
	    event.which = compositor->last_mouse_button;

	    if (layer->vtable.event_handler_func)
		(*layer->vtable.event_handler_func)(layer, &event);
	}
    }
}
