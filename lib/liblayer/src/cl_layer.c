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
 *  cl_layer.c - API for the CL_Layer type
 */


#include "prtypes.h"
#include "xp_mem.h"             /* For XP_NEW_ZAP */
#include "xpassert.h"           /* For XP_ASSERT, et al */
#include "xp_list.h"
#include "layers.h"
#include "cl_priv.h"

static void
cl_hidden_changed(CL_Layer *layer);

/* Allocate and initialize a new layer */
CL_Layer *
CL_NewLayer(char *name, int32 x_offset, int32 y_offset,
            XP_Rect *bbox, 
            CL_LayerVTable *vtable,
            uint32 flags, void *client_data)
{
    CL_Layer *new_layer;

    new_layer = XP_NEW_ZAP(CL_Layer);

    if (new_layer == NULL)
        return NULL;

    new_layer->name = name ? strdup(name) : NULL;
    if (bbox)
        new_layer->bbox = *bbox;

    /* XXX - consider sharing the vtable, instead of copying it */
    if (vtable)
        new_layer->vtable = *vtable;

    new_layer->x_offset = x_offset;
    new_layer->y_offset = y_offset;

    new_layer->hidden               = !!(flags & CL_HIDDEN);
    new_layer->scrolling            = !!(flags & CL_DONT_SCROLL_WITH_DOCUMENT);
    new_layer->clip_children        = !!(flags & CL_CLIP_CHILD_LAYERS);
    new_layer->clip_self            =  !(flags & CL_DONT_CLIP_SELF);
    new_layer->opaque               = !!(flags & CL_OPAQUE);
    new_layer->prefer_draw_onscreen = !!(flags & CL_PREFER_DRAW_ONSCREEN);
    new_layer->prefer_draw_offscreen  = !!(flags & CL_PREFER_DRAW_OFFSCREEN);
    new_layer->enumerated           =  !(flags & CL_DONT_ENUMERATE);
    new_layer->cutout               = !!(flags & CL_CUTOUT);
    new_layer->override_inherit_visibility =
                                      !!(flags & CL_OVERRIDE_INHERIT_VISIBILITY);
    new_layer->client_data = client_data;
    new_layer->draw_region = FE_NULL_REGION;
    
    return new_layer;
}

/* Destroy an existing layer */
void 
CL_DestroyLayer(CL_Layer *layer)
{
    CL_Layer *parent_layer;
    XP_ASSERT(layer);

    if (!layer)
        return;

    if (layer->compositor)
        cl_LayerDestroyed(layer->compositor, layer);
    
    parent_layer = CL_GetLayerParent(layer);
    if (parent_layer)
        CL_RemoveChild(parent_layer, layer);

    if (layer->vtable.destroy_func)
        (*layer->vtable.destroy_func)(layer);
  
    XP_FREEIF(layer->uniform_color);
    XP_FREEIF(layer->name);
    XP_FREE(layer);
}

/* Frees an entire sub-tree, rooted at the specified node */
void 
CL_DestroyLayerTree(CL_Layer *root)
{
    CL_Layer *child, *next;

    XP_ASSERT(root);

    if (!root)
        return;

    /* Iterate through the child list. For each child, recurse and free */
    for (child = root->top_child; child; child = next) {
        /* Need to grab the next child here, because child will be freed */
        next = child->sib_below;
        CL_DestroyLayerTree(child);
    }

    CL_DestroyLayer(root);
}

uint32
CL_GetLayerFlags(CL_Layer *layer)
{
    uint32 flags = 0;
    flags |= CL_HIDDEN * (layer->hidden == PR_TRUE);
    flags |= CL_DONT_SCROLL_WITH_DOCUMENT * (layer->scrolling == PR_FALSE);
    flags |= CL_CLIP_CHILD_LAYERS * (layer->clip_children == PR_TRUE);
    flags |= CL_DONT_CLIP_SELF * (layer->clip_self == PR_FALSE);
    flags |= CL_OPAQUE * (layer->opaque == PR_TRUE);
    flags |= CL_PREFER_DRAW_ONSCREEN * (layer->prefer_draw_onscreen == PR_TRUE);
    flags |= CL_PREFER_DRAW_OFFSCREEN * (layer->prefer_draw_offscreen == PR_TRUE);
    flags |= CL_DONT_ENUMERATE * (layer->enumerated == PR_FALSE);
    flags |= CL_CUTOUT * (layer->cutout == PR_TRUE);
    flags |= CL_OVERRIDE_INHERIT_VISIBILITY * (layer->override_inherit_visibility == PR_TRUE);

    return flags;
}

static PRBool
cl_change_layer_flags(CL_Layer *layer, uint32 new_flags)
{
    PRBool success;
    uint32 old_flags = CL_GetLayerFlags(layer);
    uint32 changed_flags = old_flags ^ new_flags;

    /* Assume success */
    success = PR_TRUE;

    if (changed_flags == 0)
	return success;

    if (changed_flags & CL_HIDDEN)
	CL_SetLayerHidden(layer, (PRBool)((new_flags & CL_HIDDEN) != 0));

    if (changed_flags & CL_OVERRIDE_INHERIT_VISIBILITY) {
        layer->override_inherit_visibility =
            !!(new_flags & CL_OVERRIDE_INHERIT_VISIBILITY);
        cl_hidden_changed(layer);
    }
    
    if (changed_flags &
	(CL_DONT_SCROLL_WITH_DOCUMENT | CL_CLIP_CHILD_LAYERS | CL_DONT_CLIP_SELF)) {
	XP_ASSERT(0);		/* We don't support changing this on the fly yet */
	success = PR_FALSE;
    }

    if (changed_flags & CL_OPAQUE) {
	layer->opaque = ((new_flags & CL_OPAQUE) != 0);
        if (!layer->opaque)
            CL_SetLayerUniformColor(layer, NULL);
    }

    if (changed_flags & CL_PREFER_DRAW_ONSCREEN) {
	layer->prefer_draw_onscreen = (new_flags & CL_PREFER_DRAW_ONSCREEN) != 0;
        layer->prefer_draw_offscreen = PR_FALSE;
    }

    if (changed_flags & CL_PREFER_DRAW_OFFSCREEN) {
	layer->prefer_draw_offscreen = (new_flags & CL_PREFER_DRAW_OFFSCREEN) != 0;
        layer->prefer_draw_onscreen = PR_FALSE;
    }

    if (changed_flags & CL_DONT_ENUMERATE)
	layer->enumerated = (new_flags & CL_DONT_ENUMERATE) == 0;

    if (changed_flags & CL_CUTOUT)
	layer->cutout = (new_flags & CL_CUTOUT) != 0;

    return success;
}

extern PRBool
CL_ChangeLayerFlag(CL_Layer *layer, uint32 flag, PRBool value)
{
    uint32 old_flags, new_flags;

    LOCK_LAYERS(layer);

    old_flags = CL_GetLayerFlags(layer);
    if (value)
        new_flags = old_flags | flag;
    else
        new_flags = old_flags & ~flag;

    cl_change_layer_flags(layer, new_flags);

    UNLOCK_LAYERS(layer);

    return value;
}

PRBool
CL_ForEachChildOfLayer(CL_Layer *parent, 
                       CL_ChildOfLayerFunc func,
                       void *closure)
{
    CL_Layer *child, *next;
    PRBool done;
    CL_Compositor *compositor;

    XP_ASSERT(parent);
    XP_ASSERT(func);

    if (!parent)
        return PR_FALSE;

    compositor = parent->compositor;
    if (compositor)
        LOCK_COMPOSITOR(compositor);

    /* Iterate through the child list. For each child, recurse and free */
    for (child = parent->top_child; child; child = next) {
        /* 
         * Need to grab the next child here, in case child 
         * is no longer valid 
         */
        next = child->sib_below;
        if (func) {
            done = (PRBool) ! (*func)(child, closure);
	    if (done) {
                if (compositor)
                    UNLOCK_COMPOSITOR(compositor);
                return PR_FALSE;
            }
	}
    }
    
    if (compositor)
        UNLOCK_COMPOSITOR(compositor);
    return PR_TRUE;
}

void
cl_SetCompositorRecursive(CL_Layer *layer, CL_Compositor *compositor)
{
    CL_Layer *child;

    for(child = layer->top_child; child; child = child->sib_below) 
        cl_SetCompositorRecursive(child, compositor);        

    layer->compositor = compositor;
}

/* Insert a layer into the layer tree as a child of the parent layer.
 * If sibling is NULL, the layer is added as the topmost (in z-order) child 
 * if where==CL_ABOVE or the bottommost child if where==CL_BELOW.
 * If sibling is non-NULL, the layer is added above or below (in z-order)
 * sibling based on the value of where.
 */ 
void 
CL_InsertChild(CL_Layer *parent, CL_Layer *child,
	       CL_Layer *sibling, CL_LayerPosition where)
{
    XP_ASSERT(parent);

    if (!parent)
        return;
    
    LOCK_LAYERS(parent);

    /* If no sibling is specified... */
    if (sibling == NULL) {
        /* If there are no children */
        if (parent->top_child == NULL) 
            parent->top_child = parent->bottom_child = child;

        else if (where == CL_ABOVE) {
            /* Add it as the first child */
            child->sib_below = parent->top_child;
            child->sib_above = NULL;
            parent->top_child = child;
            if (child->sib_below)
                child->sib_below->sib_above = child;
	    child->z_index = child->sib_below->z_index;
        }

        else {
            /* Add it as the last child */
            child->sib_above = parent->bottom_child;
            child->sib_below = NULL;
            parent->bottom_child = child;
            if (child->sib_above)
                child->sib_above->sib_below = child;
	    child->z_index = child->sib_above->z_index;
        }
    }
    else {
        if (where == CL_ABOVE) {
            child->sib_above = sibling->sib_above;
            child->sib_below = sibling;
            /* If there is a sibling above this, point it at child */
            if (sibling->sib_above)
                sibling->sib_above->sib_below = child;
            /* Otherwise, sibling was the first child */
            else
                parent->top_child = child;
            sibling->sib_above = child;
        }
        else {
            child->sib_below = sibling->sib_below;
            child->sib_above = sibling;
            /* If there is a sibling below this, point it at child */
            if (sibling->sib_below)
                sibling->sib_below->sib_above = child;
            /* Otherwise, sibling was the last child */
            else
                parent->bottom_child = child;
            sibling->sib_below = child;
        }
	child->z_index = sibling->z_index;
    }

    cl_SetCompositorRecursive(child, parent->compositor);    
    child->parent = parent;

    if (child->compositor)
        cl_LayerAdded(child->compositor, child);

    UNLOCK_LAYERS(parent);
}

void
CL_InsertChildByZ(CL_Layer *parent, CL_Layer *child, int32 z_index)
{
    CL_Layer *sibling;
    XP_ASSERT(parent);

    if (!parent)
	return;

    LOCK_LAYERS(parent);

    for (sibling = parent->top_child; sibling; sibling = sibling->sib_below) {
        if (sibling->z_index <= z_index)
	        break;
    }

	if (sibling)
        CL_InsertChild(parent, child, sibling, CL_ABOVE);
	else
        CL_InsertChild(parent, child, NULL, CL_BELOW);

    child->z_index = z_index;

    UNLOCK_LAYERS(parent);
}

/* Removes a layer from a parent's sub-tree */
void 
CL_RemoveChild(CL_Layer *parent, CL_Layer *child)
{
    XP_ASSERT(parent);
    XP_ASSERT(child);

    if (!parent || !child)
        return;

    LOCK_LAYERS(parent);

    if (child == parent->top_child) 
        parent->top_child = child->sib_below;
    if (child == parent->bottom_child)
        parent->bottom_child = child->sib_above;

    if (child->sib_above)
        child->sib_above->sib_below = child->sib_below;
    if (child->sib_below)
        child->sib_below->sib_above = child->sib_above;
  
    if (CL_IsMouseEventGrabber(child->compositor, child))
        CL_GrabMouseEvents(child->compositor, NULL);
    if (CL_IsKeyEventGrabber(child->compositor, child))
        CL_GrabKeyEvents(child->compositor, NULL);

    child->parent = NULL;
    
    cl_LayerRemoved(parent->compositor, child);

    UNLOCK_LAYERS(parent);
}

/* A rectangle that covers the entire world.
   Note: We don't use the extrema values for 32-bit integers.  Instead,
   we deliberately leave headroom to avoid overflow of the coordinates
   as a result of translating the rect.  */
static XP_Rect max_rect = CL_MAX_RECT;

/* Return the clipping box, in document coordinates, that this layer
   applies to child layers */
static XP_Rect *
cl_clipping_bbox(CL_Layer *layer)
{
    if (! layer)
        return &max_rect;

    if (layer->clip_children)
        return &layer->clipped_bbox;

    return cl_clipping_bbox(layer->parent);
}

/* Propagate changes in the bboxes of layers to their children.  This
   may cause the origin of child layers to change and may also change
   their widths and heights due to clipping. */
static PRBool
cl_bbox_changed_recurse(CL_Layer *layer, XP_Rect *parent_clip_bbox, 
                        PRBool ancestor_not_hidden,
                        int32 x_parent_origin, int32 y_parent_origin) 
{
    CL_Layer *child;
    XP_Rect *child_clip_bbox;
    int32 x_origin, y_origin;
    PRBool visible, not_hidden;
    int descendant_visible;
    XP_Rect *clipped_bbox = &layer->clipped_bbox;
    XP_Rect *layer_bbox = &layer->bbox;
    PRBool clip_children = (PRBool)(layer->clip_children && layer->clip_self);

    /* Compute new layer origin, in document coordinates */
    x_origin = layer->x_origin = x_parent_origin + layer->x_offset;
    y_origin = layer->y_origin = y_parent_origin + layer->y_offset;

    /* A layer is considered visible for purposes of compositing if ...
          = the layer is not hidden, and
          = none of the layer's ancestors is hidden, and
          = the layer is not entirely clipped by ancestor layers */
    not_hidden = visible = (PRBool)(!layer->hidden &&
        (ancestor_not_hidden || layer->override_inherit_visibility));

    if (layer->clip_self) {
        
        /* Get layer bounds in coordinates relative to parent layer */
        XP_CopyRect(layer_bbox, clipped_bbox);

        /* Convert to document coordinates */
        XP_OffsetRect(clipped_bbox, x_origin, y_origin);

        /* Clip by parents bbox */
        if (parent_clip_bbox) {

	    if (layer->cutout) {
		/* Special case: a cutout layer is rendered invisibly if it is 
		   clipped, even partially, by an ancestor. */
		visible &= XP_RectContainsRect(parent_clip_bbox, clipped_bbox);
		XP_IntersectRect(clipped_bbox, parent_clip_bbox, clipped_bbox);
	    } else {
		XP_IntersectRect(clipped_bbox, parent_clip_bbox, clipped_bbox);
		/* "Normal" layers are only invisible if they're wholly clipped
		   by ancestors. */
		visible &= !XP_IsEmptyRect(clipped_bbox);
	    }
	}
    } else {
        XP_CopyRect(parent_clip_bbox, clipped_bbox);
    }

    layer->visible = visible;
    child_clip_bbox = clip_children ? clipped_bbox : parent_clip_bbox;

    descendant_visible = 0;
    for (child = layer->top_child; child; child = child->sib_below) {
        descendant_visible |=
            (int)cl_bbox_changed_recurse(child, child_clip_bbox, not_hidden,
                                         layer->x_origin, layer->y_origin);
    }

    layer->visible = visible;

    layer->descendant_visible = (PRBool)descendant_visible;
    
    return visible;
}

static void
cl_property_changed(CL_Layer *layer)
{
    int32 x_offset, y_offset;
    XP_Rect *child_clip_bbox;
    PRBool ancestor_not_hidden;
    CL_Layer *parent, *layer2;

    if (layer->compositor) {
        layer->compositor->recompute_update_region = PR_TRUE;
        cl_start_compositor_timeouts(layer->compositor);
    }

    parent = layer->parent;
    if (parent) {
        layer2 = parent;
        ancestor_not_hidden = PR_TRUE;
        while (layer2) {
            if (layer2->hidden) {
                ancestor_not_hidden = PR_FALSE;
                break;
            }
            layer2 = layer2->parent;
        }
        child_clip_bbox = cl_clipping_bbox(parent);
        x_offset = parent->x_origin;
        y_offset = parent->y_origin;
    } else {
        ancestor_not_hidden = PR_TRUE;
        child_clip_bbox = &max_rect;
        x_offset = 0;
        y_offset = 0;
    }
    
    /* If a layer is visible for compositing, then so are all its ancestors */
    if (cl_bbox_changed_recurse(layer, child_clip_bbox, ancestor_not_hidden,
                                x_offset, y_offset))
        while (parent) {
            parent->descendant_visible = PR_TRUE;
            parent = parent->parent;
        }
}

static void
cl_bbox_changed(CL_Layer *layer)
{
    cl_property_changed(layer);
}

void
cl_ParentChanged(CL_Layer *layer)
{
    cl_property_changed(layer);
}

static void
cl_hidden_changed(CL_Layer *layer)
{
    cl_property_changed(layer);
}

static void
cl_position_changed(CL_Layer *layer)
{
    cl_property_changed(layer);
}

static void
cl_set_hidden (CL_Layer *layer, PRBool hidden)
{
    if (hidden == layer->hidden)
        return;

    layer->hidden = hidden;

    /* If the layer is currently visible and it's to be hidden, or the
       layer isn't visible and it's being un-hidden, update the area
       occupied by the layer. */
    if (layer->visible == hidden)
        cl_hidden_changed(layer);
}

/* Change the physical position of a layer with relative values */
/* May trigger asynchronous drawing to update screen            */
void 
CL_OffsetLayer(CL_Layer *layer, int32 x_offset, int32 y_offset)
{
    XP_ASSERT(layer);
    
    if (!layer)
        return;

    CL_MoveLayer(layer, layer->x_offset + x_offset, layer->y_offset + y_offset);
}

/* Change the position of a layer.  XY coordinates are relative to the
   origin of the parent layer.  May trigger asynchronous drawing to
   update screen */
void 
CL_MoveLayer(CL_Layer *layer, int32 x, int32 y)
{
    XP_ASSERT(layer);
    
    if (!layer)
        return;

    LOCK_LAYERS(layer);

    /* Layer position changed ? */
    if ((x == layer->x_offset) && (y == layer->y_offset)) {
        UNLOCK_LAYERS(layer);
        return;
    }

    layer->x_offset = x;
    layer->y_offset = y;
    cl_position_changed(layer);
    cl_LayerMoved(layer->compositor, layer);
    
    UNLOCK_LAYERS(layer);
}

/* 
 * Change the dimensions of a layer
 * May trigger asynchronous drawing to update screen
 * FIXME - There is a performance bug here that shows up when a
 *         layer's width grows, but its height shrinks or vice-versa.
 *         The update rectangles are overdrawn for these cases.
 */
void 
CL_ResizeLayer(CL_Layer *layer, int32 width, int32 height)
{
    XP_Rect *layer_bbox;
    int32 old_width, old_height;

    XP_ASSERT(layer);

    if (!layer)                 /* Paranoia */
        return;

    LOCK_LAYERS(layer);
    
    /* Get the existing layer width and height */
    layer_bbox = &layer->bbox;
    old_width  = layer_bbox->right  - layer_bbox->left;
    old_height = layer_bbox->bottom - layer_bbox->top;
    if ((width == old_width) && (height == old_height)) {
        UNLOCK_LAYERS(layer);
        return;
    }

    /* Reset layer bbox to new size */
    layer_bbox->right = layer_bbox->left + width;
    layer_bbox->bottom = layer_bbox->top + height;
    cl_bbox_changed(layer);

    UNLOCK_LAYERS(layer);
}

int32
CL_GetLayerXOffset(CL_Layer *layer)
{
    return layer->x_offset;
}

int32
CL_GetLayerYOffset(CL_Layer *layer)
{
    return layer->y_offset;
}

int32
CL_GetLayerXOrigin(CL_Layer *layer)
{
    return layer->x_origin;
}

int32
CL_GetLayerYOrigin(CL_Layer *layer)
{
    return layer->y_origin;
}

void
CL_SetLayerUniformColor(CL_Layer *layer, CL_Color *color)
{
    if (color) {
        CL_Color *copy_color = XP_NEW(CL_Color);
        XP_ASSERT(copy_color);
        if (!copy_color)
            return;
        *copy_color = *color;
        layer->uniform_color = copy_color;
    } else {
        XP_FREEIF(layer->uniform_color);
    }
}

void
CL_SetLayerOpaque(CL_Layer *layer, PRBool opaque)
{
    layer->opaque = opaque;
}

PRBool
CL_GetLayerOpaque(CL_Layer *layer)
{
    return layer->opaque;
}

int32
CL_GetLayerZIndex(CL_Layer *layer)
{
    return layer->z_index;
}

/* Change the bounding box of a layer */
void
CL_SetLayerBbox(CL_Layer *layer, XP_Rect *new_bbox)
{
    XP_ASSERT(layer);
    if (!layer)
        return;

    LOCK_LAYERS(layer);

    /* Don't do anything if the bbox didn't change. */
    if (XP_EqualRect(new_bbox, &layer->bbox)) {
        UNLOCK_LAYERS(layer);
        return;
    }

    layer->bbox = *new_bbox;
    cl_sanitize_bbox(&layer->bbox);

    cl_bbox_changed(layer);

    UNLOCK_LAYERS(layer);
}

void 
CL_GetLayerBbox(CL_Layer *layer, XP_Rect *bbox)
{
    XP_ASSERT(layer);
    XP_ASSERT(bbox);
    
    if (!layer || !bbox)
        return;

    LOCK_LAYERS(layer);
    bbox->left = layer->bbox.left;
    bbox->top = layer->bbox.top;
    bbox->right = layer->bbox.right;
    bbox->bottom = layer->bbox.bottom;
    UNLOCK_LAYERS(layer);
}

void 
CL_GetLayerBboxAbsolute(CL_Layer *layer, XP_Rect *bbox)
{
    XP_ASSERT(layer);
    XP_ASSERT(bbox);
    
    if (!layer || !bbox)
        return;

    LOCK_LAYERS(layer);
    bbox->left = layer->x_origin;
    bbox->top = layer->y_origin;
    bbox->right = layer->x_origin + (layer->bbox.right - layer->bbox.left);
    bbox->bottom = layer->y_origin + (layer->bbox.bottom - layer->bbox.top);
    UNLOCK_LAYERS(layer);
}

char *
CL_GetLayerName(CL_Layer *layer)
{
    XP_ASSERT(layer);
    
    if (!layer)
        return NULL;
    else
        return layer->name;
}

void 
CL_SetLayerName(CL_Layer *layer, char *name)
{
    XP_ASSERT(layer);
    
    if (!layer)
        return;
    
    layer->name = name;
}

PRBool 
CL_GetLayerHidden(CL_Layer *layer)
{
    XP_ASSERT(layer);
    
    if (!layer)
        return PR_FALSE;
    else
        return layer->hidden;
}

void 
CL_SetLayerHidden(CL_Layer *layer, PRBool hidden)
{
    XP_ASSERT(layer);
    
    if (!layer)
        return;
    
    cl_set_hidden(layer, hidden);
}

void *
CL_GetLayerClientData(CL_Layer *layer)
{
    XP_ASSERT(layer);
    
    if (!layer)
        return NULL;
    else
        return layer->client_data;
}

void 
CL_SetLayerClientData(CL_Layer *layer, void *client_data)
{
    XP_ASSERT(layer);
    
    if (!layer)
        return;
    
    layer->client_data = client_data;
}

CL_Compositor *
CL_GetLayerCompositor(CL_Layer *layer)
{
    XP_ASSERT(layer);
    
    if (!layer)
        return NULL;
    else
        return layer->compositor;
}

CL_Layer *
CL_GetLayerSiblingAbove(CL_Layer *layer)
{
    XP_ASSERT(layer);
    
    if (!layer)
        return NULL;
    else
        return layer->sib_above;
}

CL_Layer *
CL_GetLayerSiblingBelow(CL_Layer *layer)
{
    XP_ASSERT(layer);
    
    if (!layer)
        return NULL;
    else
        return layer->sib_below;
}

static CL_Layer *
cl_bottommost(CL_Layer *layer)
{
    while (layer->bottom_child && layer->bottom_child->z_index < 0)
        layer = layer->bottom_child;

    return layer;
}

static CL_Layer *
cl_GetLayerAbove2(CL_Layer *layer, PRBool consider_children)
{
    CL_Layer *layer_above;
    
    if (! layer)
        return NULL;

    if (consider_children &&
        layer->top_child && (layer->top_child->z_index >= 0)) {
        CL_Layer *child;

        child = layer->bottom_child;
        while (child->z_index < 0)
            child = child->sib_above;

        layer_above = child;
    } else if (layer->z_index < 0) {
        if (layer->sib_above && (layer->sib_above->z_index < 0))
            layer_above = cl_bottommost(layer->sib_above);
        else
            layer_above = layer->parent;
    } else {
        if (layer->sib_above)
            layer_above =  cl_bottommost(layer->sib_above);
        else
            layer_above =  cl_GetLayerAbove2(layer->parent, PR_FALSE);
    }

    if (!layer_above)
        return NULL;

    if (!layer_above->enumerated)
        return cl_GetLayerAbove2(layer_above, PR_TRUE);
    
    return layer_above;
}

CL_Layer *
CL_GetLayerAbove(CL_Layer *layer)
{
    CL_Layer *layer_above;
    XP_ASSERT(layer);
    LOCK_LAYERS(layer);
    layer_above = cl_GetLayerAbove2(layer, PR_TRUE);
    UNLOCK_LAYERS(layer);
    return layer_above;
}

static CL_Layer *
cl_topmost(CL_Layer *layer)
{
    while (layer->top_child && layer->top_child->z_index >= 0)
        layer = layer->top_child;

    return layer;
}

static CL_Layer *
cl_GetLayerBelow2(CL_Layer *layer, PRBool consider_children)
{
    CL_Layer *layer_below;

    if (! layer)
        return NULL;

    if (consider_children &&
        layer->bottom_child && (layer->bottom_child->z_index < 0)) {
        CL_Layer *child;

        child = layer->top_child;
        while (child->z_index >= 0)
            child = child->sib_below;

        layer_below = child;
    } else if (layer->z_index >= 0) {
        if (layer->sib_below && (layer->sib_below->z_index >= 0))
            layer_below = cl_topmost(layer->sib_below);
        else
            layer_below = layer->parent;
    } else {
        if (layer->sib_below)
            layer_below = cl_topmost(layer->sib_below);
        else
            layer_below = cl_GetLayerBelow2(layer->parent, PR_FALSE);
    }

    if (!layer_below)
        return NULL;

    if (!layer_below->enumerated)
        return cl_GetLayerBelow2(layer_below, PR_TRUE);

    return layer_below;
}

CL_Layer *
CL_GetLayerBelow(CL_Layer *layer)
{
    CL_Layer *layer_below;
    XP_ASSERT(layer);
    LOCK_LAYERS(layer);
    layer_below = cl_GetLayerBelow2(layer, PR_TRUE);
    UNLOCK_LAYERS(layer);
    return layer_below;
}

CL_Layer *
CL_GetLayerChildByName(CL_Layer *layer, char *name)
{
    CL_Layer *child;

    XP_ASSERT(layer);

    if (!layer)
        return NULL;
    for (child = layer->top_child; child; child = child->sib_below)
        if (child->name && strcmp(child->name, name) == 0) 
            return child;

    return NULL;
}

CL_Layer *
CL_GetLayerChildByIndex(CL_Layer *layer, uint index)
{
    CL_Layer *child;

    XP_ASSERT(layer);

    if (!layer)
        return NULL;

    LOCK_LAYERS(layer);
    for (child = layer->bottom_child; child && index; child = child->sib_above) {
	if (child->enumerated)
	    index--;
    }

    while (child && !child->enumerated)
      child = child->sib_above;
    
    UNLOCK_LAYERS(layer);
    return child;
}

int
CL_GetLayerChildCount(CL_Layer *layer)
{
    CL_Layer *child;
    int child_count = 0;

    if (!layer)
        return 0;

    XP_ASSERT(layer);

    LOCK_LAYERS(layer);
    for (child = layer->top_child; child; child = child->sib_below) {
      if (child->enumerated)
	  child_count++;
    }
    UNLOCK_LAYERS(layer);
    
    return child_count;
}

void 
CL_GetLayerVTable(CL_Layer *layer, CL_LayerVTable *vtable)
{
    XP_ASSERT(layer);
    XP_ASSERT(vtable);

    if (!layer || !vtable)
        return;

    *vtable = layer->vtable;
}


void 
CL_SetLayerVTable(CL_Layer *layer, CL_LayerVTable *vtable)
{
    XP_ASSERT(layer);

    if (!layer)
        return;

    if (vtable)
        layer->vtable = *vtable;
    else {
        layer->vtable.painter_func = NULL;
        layer->vtable.region_cleanup_func = NULL;
        layer->vtable.event_handler_func = NULL;
        layer->vtable.destroy_func = NULL;
    }
}

CL_Layer *CL_GetLayerParent(CL_Layer *layer)
{
    XP_ASSERT(layer);
    
    if (!layer)
        return NULL;
    else
        return layer->parent;
}


