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

#include "xp_mem.h"             /* For XP_NEW_ZAP */
#include "xpassert.h"           /* For XP_ASSERT, et al */
#include "cl_priv.h"

CL_Drawable *
CL_NewDrawable(uint width, uint height, uint32 flags,
               CL_DrawableVTable *vtable,
               void *client_data)
{
    CL_Drawable *cl_drawable = XP_NEW_ZAP(CL_Drawable);
    if (! client_data)
        return NULL;
    
    cl_drawable->width   = width;
    cl_drawable->height  = height;
    cl_drawable->flags   = flags;
    
    if (vtable)
        cl_drawable->vtable = *vtable;

    cl_drawable->client_data = client_data;

    return cl_drawable;
}

/* Most of the following functions could be successfully turned into
   macros, so perhaps this level of abstraction is excessive */

void
CL_DestroyDrawable(CL_Drawable *drawable)
{
    if (drawable->vtable.destroy_func)
        (*drawable->vtable.destroy_func)(drawable);
    XP_FREE(drawable);
}

void *CL_GetDrawableClientData(CL_Drawable *drawable)
{
    return drawable->client_data;
}

void
cl_SetDrawableOrigin(CL_Drawable *drawable, int32 x_offset, int32 y_offset)
{
    drawable->x_offset = x_offset;
    drawable->y_offset = y_offset;
    if (drawable->vtable.set_origin_func)
        (*drawable->vtable.set_origin_func)(drawable,
                                            x_offset, y_offset);
}

void
cl_GetDrawableOrigin(CL_Drawable *drawable, int32 *x_offset, int32 *y_offset)
{
    if (drawable->vtable.get_origin_func)
        (*drawable->vtable.get_origin_func)(drawable,
                                            x_offset, y_offset);
    else {
        *x_offset = drawable->x_offset;
        *y_offset = drawable->y_offset;
    }
}

void
cl_InitDrawable(CL_Drawable *drawable)
{
    if (drawable->vtable.init_func)
        (*drawable->vtable.init_func)(drawable);
}

void 
cl_RelinquishDrawable(CL_Drawable *drawable)
{
    if (drawable->vtable.relinquish_func)
        (*drawable->vtable.relinquish_func)(drawable);
}

void
cl_SetDrawableClip(CL_Drawable *drawable, FE_Region clip_region)
{
    drawable->clip_region = clip_region;
    if (drawable->vtable.set_clip_func)
        (*drawable->vtable.set_clip_func)(drawable, clip_region);
}

void    
cl_RestoreDrawableClip(CL_Drawable *drawable)
{
    drawable->clip_region = NULL;
    if (drawable->vtable.restore_clip_func)
        (*drawable->vtable.restore_clip_func)(drawable);
}

CL_Drawable *
cl_LockDrawableForRead(CL_Drawable *drawable)
{
    if (drawable->vtable.lock_func) {
        if (! (*drawable->vtable.lock_func)(drawable,
                                            CL_LOCK_DRAWABLE_FOR_READ))
            return NULL;
    }
    return drawable;
}

CL_Drawable *
cl_LockDrawableForReadWrite(CL_Drawable *drawable)
{
    if (drawable->vtable.lock_func) {
        if (! (*drawable->vtable.lock_func)(drawable,
                                            CL_LOCK_DRAWABLE_FOR_READ_WRITE))
            return NULL;
    }
    return drawable;
}

CL_Drawable *
cl_LockDrawableForWrite(CL_Drawable *drawable)
{
    if (drawable->vtable.lock_func) {
        if (! (*drawable->vtable.lock_func)(drawable,
                                          CL_LOCK_DRAWABLE_FOR_WRITE))
            return NULL;
    }
    return drawable;
}

void    
cl_UnlockDrawable(CL_Drawable *drawable)
{
    if (drawable->vtable.lock_func) {
        (*drawable->vtable.lock_func)(drawable,
                                      CL_UNLOCK_DRAWABLE);
    }
}

void
cl_CopyPixels(CL_Drawable *src, CL_Drawable *dest, FE_Region region)
{
    XP_ASSERT(src);
    XP_ASSERT(dest);
    
    if (!src || !dest)
        return;
    
    XP_ASSERT(dest->vtable.copy_pixels_func);
    if (src->vtable.copy_pixels_func) 
        (*src->vtable.copy_pixels_func)(src, dest,
                                        region);
}

void
cl_SetDrawableDimensions(CL_Drawable *drawable, uint32 width, uint32 height)
{
    if (!drawable)
        return;
    if (drawable->vtable.set_dimensions_func) 
        (*drawable->vtable.set_dimensions_func)(drawable,
                                                width, height);
}

/* This function indicates whether or not a given rectangle is painted
   with a single, uniform color and, if so, returns the color.  NULL
   is returned if the area contains more than one color.  This
   function must only be called during drawing. */
CL_Color *
CL_GetDrawableBgColor(CL_Drawable *drawable, XP_Rect *win_rect)
{
    CL_Layer *layer;
    CL_Compositor *compositor = drawable->compositor;

    /* A background color is only valid during the draw of a given layer */
    XP_ASSERT(compositor->composite_in_progress);
    if (!compositor->composite_in_progress)
        return NULL;

    /* Loop in front-to-back order over uniformly colored layers */
    layer = compositor->uniformly_colored_layer_stack;
    while (layer) {
        XP_Rect *layer_win_rect = &layer->win_clipped_bbox;
        XP_Rect overlap;

        XP_IntersectRect(layer_win_rect, win_rect, &overlap);
        if (!XP_IsEmptyRect(&overlap)) {
            if (XP_RectContainsRect(layer_win_rect, win_rect)) {
                return layer->uniform_color;
            } else {
                return NULL;
            }
        }
        layer = layer->uniformly_colored_layer_below;
    }
    return NULL;
}
