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



#include "xp.h"
#include "pa_tags.h"
#include "layout.h"
#include "laylayer.h"
#include "layers.h"
#include "libmocha.h"
#include "libevent.h"
#include "prefapi.h"

#define IL_CLIENT               /* XXXM12N Defined by Image Library clients */
#include "libimg.h"             /* Image Library public API. */
#include "il_icons.h"           /* Image icon enumeration. */

/* Z-order for internal layers within a group layer */
#define Z_CONTENT_LAYERS          -1  /* HTML blocks, images, blinking text, windowless plugins */
#define Z_CELL_BACKGROUND_LAYER   -1000  /* Table cell backgrounds */
#define Z_BACKGROUND_LAYER        -1001  /* Layer/document backgrounds */


#ifdef PROFILE
#pragma profile on
#endif

extern Bool UserOverride;

static MWContext*
lo_get_layer_context(CL_Layer* layer)
{
    lo_AnyLayerClosure *closure = (lo_AnyLayerClosure *)CL_GetLayerClientData(layer);
    return closure->context;
}

static lo_TopState*
lo_get_layer_top_state(CL_Layer * layer)
{
    MWContext *context = lo_get_layer_context(layer);
    if (! context)              /* Paranoia */
        return NULL;

    return lo_FetchTopState(XP_DOCID(context));
}

/**********************
 *
 * BLINK tag-related layering code
 *
 **********************/

#define LO_BLINK_RATE 750


static void
lo_blink_destroy_func(CL_Layer *layer)
{
    CL_RemoveLayerFromGroup(CL_GetLayerCompositor(layer), 
                            layer, LO_BLINK_GROUP_NAME);
    XP_DELETE(CL_GetLayerClientData(layer));
}

static void
lo_blink_painter_func(CL_Drawable *drawable, 
                      CL_Layer *layer, 
                      FE_Region update_region)
{
    lo_BlinkLayerClosure *closure;
    int32 layer_x_offset, layer_y_offset;
    LO_TextStruct *text;
    closure = (lo_BlinkLayerClosure *)CL_GetLayerClientData(layer);
    text = closure->text;

    /* Temporarily change the text element's position to be layer relative */
    layer_x_offset = CL_GetLayerXOffset(layer);
    layer_y_offset = CL_GetLayerYOffset(layer);

    text->x -= layer_x_offset;
    text->y -= layer_y_offset;
    
    FE_SetDrawable(closure->context, drawable);

	/* Temporarily turn off the blink attribute so that this text
	   will be drawn. */
	text->text_attr->attrmask &= ~LO_ATTR_BLINK;

    /* Draw the text element associated with this layer */
    lo_DisplayText(closure->context, text, FALSE);

    /* Restore blink attribute */
	text->text_attr->attrmask |= LO_ATTR_BLINK;

    text->x += layer_x_offset;
    text->y += layer_y_offset;

    FE_SetDrawable(closure->context, NULL);
}

static void
lo_blink_callback(void *closure)
{
    MWContext *context = (MWContext *)closure;

    if (context->blink_hidden == FALSE) {
        CL_HideLayerGroup(context->compositor, LO_BLINK_GROUP_NAME);
        context->blink_hidden = TRUE;
    }
    else {
        CL_UnhideLayerGroup(context->compositor, LO_BLINK_GROUP_NAME);
        context->blink_hidden = FALSE;
    }

    /* Set the next timeout */
    context->blink_timeout = FE_SetTimeout(lo_blink_callback,
                                           (void *)context,
                                           LO_BLINK_RATE);
}

void
lo_CreateBlinkLayer (MWContext *context, LO_TextStruct *text,
                     CL_Layer *parent)
{
	CL_Layer *blink_layer;
    lo_BlinkLayerClosure *closure;
    XP_Rect bbox;
    CL_LayerVTable vtable;
    int32 layer_x_offset, layer_y_offset;
    
    /* The layer is the size of the text element */
    bbox.left   = 0;
    bbox.top    = 0;
    bbox.right  = text->width;
    bbox.bottom = text->height;

    layer_x_offset = text->x + text->x_offset;
    layer_y_offset = text->y + text->y_offset;

    /* Create the client_data */
    closure = XP_NEW_ZAP(lo_BlinkLayerClosure);
    closure->type = LO_BLINK_LAYER;
    closure->context = context;
    closure->text = text;
    
    XP_BZERO(&vtable, sizeof(CL_LayerVTable));    
    vtable.painter_func = (CL_PainterFunc)lo_blink_painter_func;
    vtable.destroy_func = (CL_DestroyFunc)lo_blink_destroy_func;
    
    blink_layer = CL_NewLayer(NULL, layer_x_offset, layer_y_offset,
                              &bbox, &vtable,
                              CL_NO_FLAGS | CL_DONT_ENUMERATE, (void *)closure);

    CL_InsertChildByZ(parent, blink_layer, Z_CONTENT_LAYERS);
    CL_AddLayerToGroup(context->compositor, blink_layer, LO_BLINK_GROUP_NAME);

    /* If noone has setup the callback yet, start it up */
    if (context->blink_timeout == NULL) {
        context->blink_hidden = FALSE;
        context->blink_timeout = FE_SetTimeout(lo_blink_callback,
                                               (void *)context,
                                               LO_BLINK_RATE);
    }
}

/* Update the bounding box of a blink layer to its current text element position. */
static PRBool
lo_blink_update_func(CL_Layer *blink_layer, void *dummy)
{
    lo_BlinkLayerClosure *closure;
    LO_TextStruct *text;

    closure = ((lo_BlinkLayerClosure *)CL_GetLayerClientData(blink_layer));
    text = closure->text;
    
    CL_MoveLayer(blink_layer, text->x + text->x_offset, text->y + text->y_offset);
    return PR_TRUE;
}

void
lo_UpdateBlinkLayers(MWContext *context)
{
    CL_ForEachLayerInGroup(context->compositor, LO_BLINK_GROUP_NAME,
                           lo_blink_update_func, NULL);
}

void
lo_DestroyBlinkers(MWContext *context)
{
    if (context->blink_timeout) {
        FE_ClearTimeout(context->blink_timeout);
        context->blink_timeout = NULL;
    }
}

/**********************
 *
 * HTML and background layer code
 *
 **********************/

static void
lo_html_destroy_func(CL_Layer *layer)
{
    lo_AnyLayerClosure *closure;
    lo_TopState *top_state = NULL;

    closure = (lo_AnyLayerClosure *)CL_GetLayerClientData(layer);
    top_state = lo_FetchTopState(XP_DOCID(closure->context));
    
    XP_DELETE(closure);

    if (top_state && top_state->doc_state && 
		    top_state->doc_state->selection_layer == layer)
	top_state->doc_state->selection_layer = NULL;
}

static void
lo_html_block_destroy_func(CL_Layer *layer)
{
    lo_GroupLayerClosure *closure;
	lo_LayerDocState *layer_state;
    JSObject *obj;
    lo_TopState *top_state = NULL;
    int32 layer_id;
    
    closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(layer);

    top_state = lo_get_layer_top_state(layer);

    layer_state = closure->layer_state;
	if (!layer_state)			/* Paranoia */
	    goto done;
    obj = layer_state->mocha_object;
    if (obj) {
	    layer_state->mocha_object = NULL;
        ET_DestroyLayer(closure->context, obj);
    }
    layer_id = layer_state->id;

    if (top_state->layers[layer_id] == layer_state)
        top_state->layers[layer_id] = NULL;

    /* 
     * Don't get rid of the layer associated with this layer_state,
     * since the entire layer tree will be dealt with separately.
     */
    layer_state->layer = NULL;

    /* 
     * If this layer corresponds to an inflow layer, its cell
     * will be recycled with the rest of the document.
     */
    if (layer_state->cell && layer_state->cell->cell_inflow_layer)
        layer_state->cell = NULL;
    lo_DeleteLayerState(closure->context, top_state->doc_state, layer_state);

  done:
    XP_DELETE(closure);

    if (top_state && top_state->doc_state && 
		    top_state->doc_state->selection_layer == layer)
	top_state->doc_state->selection_layer = NULL;

}

typedef struct html_event_closure {
    CL_Layer	*layer;
    CL_Event	*event;
} html_event_closure;

static void
lo_html_event_callback(MWContext * pContext, LO_Element * pEle, int32 event,
                     void * pObj, ETEventStatus status)
{
    html_event_closure *event_closure = (html_event_closure *)pObj;

    if (status == EVENT_PANIC){
	if (event_closure->event->fe_event)
	    XP_FREE(event_closure->event->fe_event);
	XP_FREE(event_closure->event);
	XP_FREE(event_closure);
    return;
    }

    if (status == EVENT_OK){
        FE_HandleLayerEvent(pContext, event_closure->layer, 
                            event_closure->event);
    }

    if (event_closure->event->fe_event)
	XP_FREE(event_closure->event->fe_event);
    XP_FREE(event_closure->event);
    XP_FREE(event_closure);
    return;
}

#define LEFT_BUTTON_DRAG 1
#define RIGHT_BUTTON_DRAG 2

static PRBool
lo_html_event_handler_func(CL_Layer *layer,
                           CL_Event *event)
{
    html_event_closure *event_closure;
    JSEvent *jsevent, *jskd_event;
    LO_Element *pElement;
    uint32 event_capture_bit = 0;
    MWContext *context;
    int32 client_x_pos, client_y_pos, layer_id;
    LO_AnchorData *anchor = NULL;

    lo_AnyLayerClosure *closure = (lo_AnyLayerClosure *)CL_GetLayerClientData(layer);

    if (EDT_IS_EDITOR(closure->context))
	return FE_HandleLayerEvent(closure->context, layer, event);

    event_capture_bit |= closure->context->event_bit;

    if (closure->context->is_grid_cell) {
        context = closure->context;
	while (context->grid_parent) {
	    context = context->grid_parent;
	    event_capture_bit |= context->event_bit;
	}
    }

    layer_id = (LO_GetLayerType(layer) == LO_GROUP_LAYER) ?
        LO_GetIdFromLayer(closure->context, layer) : LO_DOCUMENT_LAYER_ID;

    if ((event->type != CL_EVENT_MOUSE_ENTER) &&
        (event->type != CL_EVENT_MOUSE_LEAVE) &&
        (event->type != CL_EVENT_KEY_FOCUS_GAINED) &&
        (event->type != CL_EVENT_KEY_FOCUS_LOST)) 
	    pElement = LO_XYToElement(closure->context, event->x, event->y, 
				      layer);
    else {
        pElement = NULL;

	/* These events shouldn't be going to the document. */
	if (layer_id == LO_DOCUMENT_LAYER_ID)	{
	    return FE_HandleLayerEvent(closure->context, layer, event);
	}
    }

    if (pElement && pElement->type == LO_FORM_ELE) {
	if ((event->x - pElement->lo_form.x - pElement->lo_form.x_offset > pElement->lo_form.width) ||
	    (event->x - pElement->lo_form.x - pElement->lo_form.x_offset < 0) ||
	    (event->y - pElement->lo_form.y - pElement->lo_form.y_offset > pElement->lo_form.height) ||
	    (event->y - pElement->lo_form.y - pElement->lo_form.y_offset < 0)) {
	    pElement = NULL;
	}
    }
    
    FE_GetWindowOffset(closure->context, &client_x_pos, &client_y_pos);

    jsevent = XP_NEW_ZAP(JSEvent);
    switch (event->type) 
    {
    case CL_EVENT_MOUSE_BUTTON_DOWN:
	if (pElement) {
	    if (pElement->type == LO_TEXT)
		anchor = pElement->lo_text.anchor_href;
	    if (pElement->type == LO_IMAGE)	{
			anchor = pElement->lo_image.anchor_href;
		}
	    
	    if (anchor && !(event_capture_bit & EVENT_MOUSEDOWN) 
			    && anchor->event_handler_present != TRUE) {
		XP_FREE(jsevent);
		return FE_HandleLayerEvent(closure->context, layer, event);
	    }
	}
#ifdef XP_MAC
	/* the mac sets the data to the number of clicks instead of
	   sending a multiclick event */
	if (event->data > 1)
		jsevent->type = EVENT_DBLCLICK;
	else	
#endif /* XP_MAC */
        jsevent->type = EVENT_MOUSEDOWN;
	if (closure->context->js_drag_enabled) {
	    CL_GrabMouseEvents(closure->context->compositor, layer);
	    if (event->which == 1)
		closure->context->js_dragging |= LEFT_BUTTON_DRAG;
	    if (event->which == 3)
		closure->context->js_dragging |= RIGHT_BUTTON_DRAG;
	}
	break;
    case CL_EVENT_MOUSE_BUTTON_UP:
	if (pElement) {
	    if (pElement->type == LO_TEXT)
		anchor = pElement->lo_text.anchor_href;
	    if (pElement->type == LO_IMAGE)
		anchor = pElement->lo_image.anchor_href;
	    
	    if (anchor && !(event_capture_bit & EVENT_MOUSEUP) 
			    && anchor->event_handler_present != TRUE) {
		XP_FREE(jsevent);
		return FE_HandleLayerEvent(closure->context, layer, event);
	    }
	}
        jsevent->type = EVENT_MOUSEUP;
	if (closure->context->js_dragging) {
	    CL_GrabMouseEvents(closure->context->compositor, NULL);
	    if (event->which == 1)
		closure->context->js_dragging &= ~LEFT_BUTTON_DRAG;
	    if (event->which == 3)
		closure->context->js_dragging &= ~RIGHT_BUTTON_DRAG;
	}
	break;
    case CL_EVENT_MOUSE_BUTTON_MULTI_CLICK:
        jsevent->type = EVENT_DBLCLICK;
	break;
    case CL_EVENT_KEY_DOWN:
	if (!event->data) {

	    /* Key events are handled a little strangely.  The initial hitting of the
	     * key triggers two events, a KEYDOWN event and a KEYPRESS event.  Any
	     * key repetition is accomplished through additional KEYPRESS events.  So
	     * if this is our first time through, we need to create and send an 
	     * additional event.  However, since the main program triggers off of the
	     * KEYPRESS only we don't need a callback and closure.
	     */
	    jskd_event = XP_NEW_ZAP(JSEvent);
	    jskd_event->type = EVENT_KEYDOWN;
	    jskd_event->x = event->x;
	    jskd_event->y = event->y;
	    jskd_event->docx = event->x + CL_GetLayerXOrigin(layer);
	    jskd_event->docy = event->y + CL_GetLayerYOrigin(layer);
	    jskd_event->screenx = jskd_event->docx + client_x_pos 
				- CL_GetCompositorXOffset(closure->context->compositor);
	    jskd_event->screeny = jskd_event->docy + client_y_pos 
				- CL_GetCompositorXOffset(closure->context->compositor);
	    jskd_event->which = event->which;
	    jskd_event->modifiers = event->modifiers;
	    jskd_event->layer_id = layer_id;
        
	    ET_SendEvent(closure->context, pElement, jskd_event, 0, 0);
	}   
        jsevent->type = EVENT_KEYPRESS;
	break;
    case CL_EVENT_KEY_UP:
        jsevent->type = EVENT_KEYUP;
	break;
    case CL_EVENT_MOUSE_ENTER:
        jsevent->type = EVENT_MOUSEOVER;
	break;
    case CL_EVENT_MOUSE_LEAVE:
        jsevent->type = EVENT_MOUSEOUT;
	break;
    case CL_EVENT_MOUSE_MOVE:
	if (event_capture_bit & EVENT_MOUSEMOVE || 
		    closure->context->js_dragging==TRUE)
    	    jsevent->type = EVENT_MOUSEMOVE;
	break;
    case CL_EVENT_KEY_FOCUS_GAINED:
        jsevent->type = EVENT_FOCUS;
        break;
    case CL_EVENT_KEY_FOCUS_LOST:
        jsevent->type = EVENT_BLUR;
        break;
    default:
	XP_FREE(jsevent);
	return FE_HandleLayerEvent(closure->context, layer, event);
    }
    
    if (!jsevent->type) {
	XP_FREE(jsevent);
	return FE_HandleLayerEvent(closure->context, layer, event);
    }

    jsevent->x = event->x;
    jsevent->y = event->y;
    jsevent->docx = event->x + CL_GetLayerXOrigin(layer);
    jsevent->docy = event->y + CL_GetLayerYOrigin(layer);
    jsevent->screenx = jsevent->docx + client_x_pos 
		- CL_GetCompositorXOffset(closure->context->compositor);
    jsevent->screeny = jsevent->docy + client_y_pos 
		- CL_GetCompositorYOffset(closure->context->compositor);
    jsevent->which = event->which;
    jsevent->modifiers = event->modifiers;
    jsevent->layer_id = layer_id;
    
    /* This probably isn't the best place for these but I don't want to 
     * put the setting of all of these at the top of the function because
     * of possible early returns and since this is part of the event loop 
     * I'm trying to be stingy with cycles.
     */
    if (jsevent->type == EVENT_FOCUS || jsevent->type == EVENT_BLUR) {
	jsevent->docx = 0;
	jsevent->docy = 0;
	jsevent->screenx = 0;
	jsevent->screeny = 0;
	jsevent->which = 0;
    }
    if (jsevent->type == EVENT_MOUSEOVER || jsevent->type == EVENT_MOUSEOUT) {
	jsevent->which = 0;
    }
    
    event_closure = XP_NEW_ZAP(html_event_closure);
    event_closure->layer = layer;
    event_closure->event = XP_NEW_ZAP(CL_Event);
    XP_MEMCPY(event_closure->event, event, sizeof(CL_Event));
    if (event->fe_event) {
	event_closure->event->fe_event = XP_ALLOC(event->fe_event_size);
	XP_MEMCPY(event_closure->event->fe_event, event->fe_event, event->fe_event_size);
    }

    ET_SendEvent(closure->context, pElement, jsevent, lo_html_event_callback, event_closure);

    return PR_TRUE;
}

static PRBool
lo_html_inflow_layer_event_handler_func(CL_Layer *layer,
                                        CL_Event *event)
{
/* XXX - Doesn't work yet */
#if 0
    lo_LayerClosure *closure = (lo_LayerClosure *)CL_GetLayerClientData(layer);
    CL_Layer *parent = CL_GetLayerParent(layer);
    int32 x_offset = closure->u.block_closure.x_offset;
    int32 y_offset = closure->u.block_closure.y_offset;
    event->x += x_offset;
    event->y += y_offset;
    if (lo_html_event_handler_func(parent, event))
        return PR_TRUE;
    event->x += x_offset;
    event->y += y_offset;
#endif
    return PR_FALSE;
}

static PRBool
lo_is_document_layer(CL_Layer * layer)
{
    lo_TopState *top_state = lo_get_layer_top_state(layer);
    return (PRBool)(top_state->doc_layer == layer);
}

/* Callback that tells the front-end to expand the scrollable extent
   of the document to encompass the _DOCUMENT layer. */
static void
doc_bbox_changed_func(CL_Layer *layer, XP_Rect *new_bbox)
{
    lo_GroupLayerClosure *closure;
    lo_TopState *top_state = lo_get_layer_top_state(layer);
    lo_LayerDocState *layer_state = top_state->layers[LO_DOCUMENT_LAYER_ID];

    int32 width  = new_bbox->right - new_bbox->left;
    int32 height = new_bbox->bottom - new_bbox->top;
    closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(layer);

    XP_ASSERT(layer_state->id == LO_DOCUMENT_LAYER_ID);

    if (layer_state->overrideScrollWidth)
        width = layer_state->contentWidth;
    else
        layer_state->contentWidth = width;

    if (layer_state->overrideScrollHeight)
        height = layer_state->contentHeight;
    else
        layer_state->contentHeight = height;
    
    FE_SetDocDimension(closure->context, FE_VIEW, width, height);
}

/* Expand a bbox to contain a given layer.
   (Callback for CL_ForEachChildOfLayer(), below.) */
static PRBool
expand_bbox(CL_Layer *layer, void *closure)
{
    XP_Rect layer_bbox;
    XP_Rect *enclosing_bbox = closure;

    CL_GetLayerBboxAbsolute(layer, &layer_bbox);

    /* Expand the enclosing bbox to encompass the layer. */
    XP_RectsBbox(&layer_bbox, enclosing_bbox, enclosing_bbox);
    return PR_TRUE;
}

/* The _BODY layer is treated specially in that if it shrinks, it may
   cause the document to shrink as well.  At the moment, the only case
   in which that happens is when editing a document. */
static void
body_bbox_changed_func(CL_Layer *layer, XP_Rect *new_bbox)
{
    XP_Rect bbox = {0, 0, 0, 0};
    CL_Layer *doc_layer = CL_GetLayerParent(layer);

    /* Set the bbox of the document to be the rectangle that encloses
       the BODY layer and all its siblings. */
    CL_ForEachChildOfLayer(doc_layer, expand_bbox, &bbox);
    CL_SetLayerBbox(doc_layer, &bbox);
}

lo_LayerDocState *
lo_GetLayerState(CL_Layer *layer)
{
    lo_GroupLayerClosure *closure;
    lo_TopState *top_state;

    XP_ASSERT(layer);
    if (! layer)
        return NULL;
        
    top_state = lo_get_layer_top_state(layer);

    closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(layer);
/*    XP_ASSERT(closure->type == LO_GROUP_LAYER); */
    /* Hack for document layer and body layers, necessary for event handling */
    if ((top_state->body_layer == layer) || 
        (top_state->doc_layer == layer) || 
        (closure->type != LO_GROUP_LAYER))
        return top_state->layers[LO_DOCUMENT_LAYER_ID];

    return closure->layer_state;

}

PRBool
lo_InsideLayer(lo_DocState *state)
{
    int32 layer_id = lo_CurrentLayerId(state);
    if (layer_id != LO_DOCUMENT_LAYER_ID)
        return PR_TRUE;
    return PR_FALSE;
}

PRBool
lo_InsideInflowLayer(lo_DocState *state)
{
	lo_LayerDocState *layer_state;
	lo_GroupLayerClosure *closure;
    
    layer_state = lo_CurrentLayerState(state);
    
    if (layer_state->id == LO_DOCUMENT_LAYER_ID)
        return PR_FALSE;
    
    closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(layer_state->layer);
    XP_ASSERT(closure->type == LO_GROUP_LAYER);
    if (closure->type != LO_GROUP_LAYER)
    	return PR_FALSE;
    	
    return closure->is_inflow;
}

/* Get XY needed to convert from layer coordinates to layout coordinates */
void
lo_GetLayerXYShift(CL_Layer *layer, int32 *xp, int32 *yp)
{
    lo_GroupLayerClosure *closure;

    *xp = 0;
    *yp = 0;
    XP_ASSERT(layer);
    if (! layer)
        return;
        
    closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(layer);
    if (closure->type != LO_GROUP_LAYER)
        return;
    
    *xp = closure->x_offset;
    *yp = closure->y_offset;
}

void
LO_MoveLayer(CL_Layer *layer, int32 x, int32 y)
{
    CL_Layer *parent_layer;
    lo_GroupLayerClosure *closure, *parent_closure;
    int32 x_offset, y_offset;

    XP_ASSERT(layer);
    if (! layer)
        return;
        
    closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(layer);
    XP_ASSERT(closure->type == LO_GROUP_LAYER);
    parent_layer = CL_GetLayerParent(layer);
    x_offset = closure->x_offset;
    y_offset = closure->y_offset;

    /* Handle special case of inflow layer nested in another inflow layer. */
    if (closure->is_inflow && parent_layer) {
        parent_closure = CL_GetLayerClientData(parent_layer);
        x_offset -= parent_closure->x_offset;
        y_offset -= parent_closure->y_offset;
    }

    CL_MoveLayer(layer, x + x_offset, y + y_offset);
}

void
LO_SetLayerBbox(CL_Layer *layer, XP_Rect *bbox)
{
    lo_LayerDocState *layer_state = lo_GetLayerState(layer);
    XP_Rect resultBbox;
    XP_IntersectRect(&layer_state->viewRect, bbox, &resultBbox);
    CL_SetLayerBbox(layer, &resultBbox);
}

int32
LO_GetLayerXOffset(CL_Layer *layer)
{
    CL_Layer *parent_layer;
    lo_GroupLayerClosure *closure, *parent_closure;
    int32 x_offset;

    XP_ASSERT(layer);
    if (! layer)
        return 0;
        
    closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(layer);
    XP_ASSERT(closure->type == LO_GROUP_LAYER);
    parent_layer = CL_GetLayerParent(layer);
    x_offset = closure->x_offset;

    /* Handle special case of inflow layer nested in another inflow layer. */
    if (closure->is_inflow && parent_layer) {
        parent_closure = CL_GetLayerClientData(parent_layer);
        x_offset -= parent_closure->x_offset;
    }

    return CL_GetLayerXOffset(layer) - x_offset;
}
    
int32
LO_GetLayerYOffset(CL_Layer *layer)
{
    CL_Layer *parent_layer;
    lo_GroupLayerClosure *closure, *parent_closure;
    int32 y_offset = 0;

    XP_ASSERT(layer);
    if (! layer)
        return 0;
        
    closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(layer);
    XP_ASSERT(closure->type == LO_GROUP_LAYER);
    parent_layer = CL_GetLayerParent(layer);
    y_offset = closure->y_offset;

    /* Handle special case of inflow layer nested in another inflow layer. */
    if (closure->is_inflow && parent_layer) {
        parent_closure = CL_GetLayerClientData(parent_layer);
        y_offset -= parent_closure->y_offset;
    }

    return CL_GetLayerYOffset(layer) - y_offset;
}

int32
LO_GetLayerScrollWidth(CL_Layer *layer)
{
    lo_LayerDocState *layer_state;
    XP_ASSERT(layer);
    if (!layer)
        return -1;
    
    layer_state = lo_GetLayerState(layer);
    XP_ASSERT(layer_state);
    if (!layer_state)
        return -1;
    return layer_state->contentWidth;
}

int32
LO_GetLayerScrollHeight(CL_Layer *layer)
{
    lo_LayerDocState *layer_state;
    XP_ASSERT(layer);
    if (!layer)
        return -1;

    layer_state = lo_GetLayerState(layer);
    XP_ASSERT(layer_state);
    if (!layer_state)
        return -1;
    return layer_state->contentHeight;
}

void
LO_SetLayerScrollWidth(CL_Layer *layer, uint32 width)
{
    uint32 height;
    lo_LayerDocState *layer_state;
    XP_ASSERT(layer);
    if (!layer)
        return;

    /* The document layer is special.  It's the only one with a
       mutable scroll width and height.*/
    if (!lo_is_document_layer(layer))
        return;
    
    layer_state = lo_GetLayerState(layer);
    layer_state->contentWidth = width;
    layer_state->overrideScrollWidth = PR_TRUE;
    height = layer_state->contentHeight;
    LO_SetDocumentDimensions(lo_get_layer_context(layer), width, height);
}

void
LO_SetLayerScrollHeight(CL_Layer *layer, uint32 height)
{
    uint32 width;
    lo_LayerDocState *layer_state;
    XP_ASSERT(layer);
    if (!layer)
        return;

    /* The document layer is special.  It's the only one with a
       mutable scroll width and height.*/
    if (!lo_is_document_layer(layer))
        return;
    
    layer_state = lo_GetLayerState(layer);
    layer_state->contentHeight = height;
    layer_state->overrideScrollHeight = PR_TRUE;
    width = layer_state->contentWidth;
    LO_SetDocumentDimensions(lo_get_layer_context(layer), width, height);
}

int32
LO_GetLayerWrapWidth(CL_Layer *layer)
{
    lo_GroupLayerClosure *closure;

    XP_ASSERT(layer);
    if (! layer)
        return 0;
        
    closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(layer);
    XP_ASSERT(closure->type == LO_GROUP_LAYER);

    return closure->wrap_width;
}

extern void
lo_SetLayerClipExpansionPolicy(CL_Layer *layer, int policy)
{
    lo_GroupLayerClosure *closure;

    XP_ASSERT(layer);
    if (! layer)
        return;
        
    closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(layer);
    XP_ASSERT(closure->type == LO_GROUP_LAYER);

    closure->clip_expansion_policy = policy;
}

extern int
lo_GetLayerClipExpansionPolicy(CL_Layer *layer)
{
    lo_GroupLayerClosure *closure;

    XP_ASSERT(layer);
    if (! layer)
        return 0;
        
    closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(layer);
    XP_ASSERT(closure->type == LO_GROUP_LAYER);

    return closure->clip_expansion_policy;
}

typedef struct lo_BlockRectStruct
{
    lo_HTMLBlockClosure *closure;
    CL_Layer *layer;
} lo_BlockRectStruct;

/* Called for each rectangle in the update region */
static void
lo_block_rect_func(void *closure,
                   XP_Rect *rect)
{
    lo_BlockRectStruct *block_struct = (lo_BlockRectStruct *)closure;
    lo_HTMLBlockClosure *myclosure = block_struct->closure;
    
    /* Intersect the layer to be drawn with the layer's bbox */
    CL_WindowToLayerRect(CL_GetLayerCompositor(block_struct->layer),
                         block_struct->layer, rect);
                      
    lo_DisplayCellContents(myclosure->context,
			   myclosure->layer_state->cell, 
               -myclosure->x_offset, 
               -myclosure->y_offset, 
			   rect->left, rect->top,
			   (rect->right - rect->left),
			   (rect->bottom - rect->top));
}

/* painter_func for block layers */
static void
lo_block_painter_func(CL_Drawable *drawable, 
                      CL_Layer *layer, 
                      FE_Region update_region)
{
    lo_BlockRectStruct block_struct;
    
    block_struct.closure = (lo_HTMLBlockClosure *)CL_GetLayerClientData(layer);
    block_struct.layer = layer;
    
    FE_SetDrawable(block_struct.closure->context, drawable);

    CL_ForEachRectCoveringRegion(update_region, 
				 (FE_RectInRegionFunc)lo_block_rect_func,
				 (void *)&block_struct);

    FE_SetDrawable(block_struct.closure->context, NULL);
}


/* Called for each rectangle in the update region */
static void
lo_body_rect_func(void *closure,
				  XP_Rect *rect)
{
    lo_HTMLBodyClosure *myclosure = (lo_HTMLBodyClosure *)closure;
    
    CL_WindowToLayerRect(myclosure->context->compositor, 
                         myclosure->layer,
                         rect);
    LO_RefreshArea(myclosure->context,
                   rect->left, rect->top,
                   (rect->right - rect->left),
                   (rect->bottom - rect->top));
}

/* painter_func for HTML document layers */
static void
lo_body_painter_func(CL_Drawable *drawable, 
					 CL_Layer *layer, 
					 FE_Region update_region)
{
    lo_HTMLBodyClosure *closure;
    
    closure = (lo_HTMLBodyClosure *)CL_GetLayerClientData(layer);
    FE_SetDrawable(closure->context, drawable);

    CL_ForEachRectCoveringRegion(update_region, 
                                 (FE_RectInRegionFunc)lo_body_rect_func,
                                 (void *)closure);

    FE_SetDrawable(closure->context, NULL);
}

/* 
 * Create an grouping layer and make it the parent of the content
 * and background layers specified.
 */
static CL_Layer *
lo_CreateGroupLayer(MWContext *context, char *name,
                    int32 x_offset, int32 y_offset,
                    Bool enumerate_for_javascript,
                    Bool clip_children, CL_Layer *content,
                    lo_LayerDocState *layer_state,

                    CL_BboxChangedFunc bbox_changed_func,
                    CL_EventHandlerFunc event_handler_func,

                    CL_DestroyFunc destroy_func)

{
    CL_Layer *layer;
    CL_LayerVTable vtable;
    lo_GroupLayerClosure *closure;

    uint32 flags = 
        CL_HIDDEN |
        (CL_DONT_ENUMERATE * (enumerate_for_javascript == FALSE)) |
        (CL_CLIP_CHILD_LAYERS * clip_children);

    /* Bbox values don't matter since they will be changed afterwards. */
    XP_Rect bbox = {0,0,0,0};

    XP_BZERO(&vtable, sizeof(CL_LayerVTable));
    vtable.destroy_func = destroy_func;

    vtable.bbox_changed_func = bbox_changed_func;
    vtable.event_handler_func = event_handler_func;

    closure = XP_NEW_ZAP(lo_GroupLayerClosure);
    closure->type = LO_GROUP_LAYER;
    closure->context = context;    
    closure->content_layer = content;
    closure->x_offset = x_offset;
    closure->y_offset = y_offset;
    closure->layer_state = layer_state;


    /* Initially, layer created at [0,0] and moved to final position later. */
    layer = CL_NewLayer(name, 0, 0, &bbox, &vtable,
                        flags, (void *)closure);

    if (content)
        CL_InsertChildByZ(layer, content, Z_CONTENT_LAYERS);
    
    return layer;
}

/* Create the _CONTENT child of a block layer */
PRIVATE
CL_Layer *
lo_CreateBlockContentLayer(MWContext *context,
                           PRBool is_inflow,
                           int32 x_offset,
                           int32 y_offset,
                           lo_LayerDocState *layer_state,
                           lo_DocState *state)
{
    CL_Layer *layer;
    lo_HTMLBlockClosure *closure;
    CL_LayerVTable vtable;

    /* Bbox values don't matter since CL_DONT_CLIP_SELF flag is set */
    XP_Rect bbox = {0,0,0,0};
    
    uint32 flags = CL_DONT_ENUMERATE | CL_DONT_CLIP_SELF;

    closure = XP_NEW_ZAP(lo_HTMLBlockClosure);
    closure->type = LO_HTML_BLOCK_LAYER;
    closure->context = context;
    closure->layer_state = layer_state;
    closure->state = state;
    closure->is_inflow = is_inflow;
    closure->x_offset = x_offset;
    closure->y_offset = y_offset;
    
    XP_BZERO(&vtable, sizeof(CL_LayerVTable));    
    vtable.painter_func = (CL_PainterFunc)lo_block_painter_func;
    vtable.destroy_func = (CL_DestroyFunc)lo_html_destroy_func;
    
    layer = CL_NewLayer(LO_CONTENT_LAYER_NAME, 0, 0, &bbox, &vtable,
                        flags, (void *)closure);
    
    return layer;
}

typedef struct lo_BackgroundRectStruct
{
    lo_BackgroundLayerClosure *closure;
    CL_Layer *layer;
} lo_BackgroundRectStruct;

/* Called for each rectangle in the update region */
static void
lo_background_rect_func(void *inclosure,
                        XP_Rect *rect)
{
    LO_ImageStruct *backdrop;

    LO_Color * bg;
    PRBool is_transparent_backdrop, is_complete;
    lo_TileMode tile_mode;
    PRBool fully_tiled;         /* PR_TRUE, if tiled in both X and Y */

    XP_Rect doc_rect;
    lo_BackgroundLayerClosure *closure = ((lo_BackgroundRectStruct *)inclosure)->closure;
    CL_Layer *layer = ((lo_BackgroundRectStruct *)inclosure)->layer;
    CL_Compositor *compositor = CL_GetLayerCompositor(layer);
    MWContext * context = closure->context;
    static LO_Color white_color = {0xFF, 0xFF, 0xFF};

    XP_CopyRect(rect, &doc_rect);
    CL_WindowToDocumentRect(compositor, &doc_rect);

    backdrop = closure->backdrop_image;
	bg = closure->bg_color;
    tile_mode = closure->tile_mode;

    /* Erase with solid color if required. */
    fully_tiled = (PRBool)(tile_mode == LO_TILE_BOTH);
    is_complete = (PRBool)(backdrop && 
        ((backdrop->image_status == IL_IMAGE_COMPLETE) || 
         (backdrop->image_status == IL_FRAME_COMPLETE)));
    is_transparent_backdrop = (PRBool)(backdrop && (!is_complete ||
                                           backdrop->is_transparent));

    /*
     * For printing never draw backdrop images. Also, only draw the
     * backgrounds of table cells in their actual color (everything
     * else will be drawn with white.
     */

    if ((context->type == MWContextPrint) || 
        (context->type == MWContextPostScript)) {
#ifdef XP_MAC
		/* the Mac does print background images, if requested */
		XP_Bool backgroundPref;
		if (PREF_GetBoolPref("browser.mac.print_background", &backgroundPref) != PREF_OK || !backgroundPref)
			backdrop = NULL;
#elif defined(XP_WIN32)
		XP_Bool backgroundPref = FALSE;
		PREF_GetBoolPref("browser.print_background",&backgroundPref);
		if (!backgroundPref)
			backdrop = NULL;

#else
		backdrop = NULL;
#endif
#ifndef XP_WIN32
        if (closure->bg_type != BG_CELL) 
            bg = &white_color;
#else
		if (!backgroundPref) {
	        if (closure->bg_type != BG_CELL) 
				bg = &white_color;
		}
#endif
    }

    if (bg && (!backdrop || is_transparent_backdrop || !fully_tiled))
        FE_EraseBackground(closure->context,
                           FE_VIEW,
                           doc_rect.left,
                           doc_rect.top,
                           doc_rect.right - doc_rect.left,
                           doc_rect.bottom - doc_rect.top,
                           bg);

    /* Paint the backdrop image if there is one. */
    if (backdrop && !backdrop->is_icon && is_complete) {
        XP_Rect tile_area = CL_MAX_RECT;

        /* Convert to layer coordinates */
        CL_WindowToLayerRect(compositor, layer, rect);

        /* Constrain tiling area based on tiling mode */
        if ((int)tile_mode & LO_TILE_HORIZ)
            tile_area.bottom = backdrop->height;
        if ((int)tile_mode & LO_TILE_VERT)
            tile_area.right = backdrop->width;

        XP_IntersectRect(rect, &tile_area, rect);

        /* Note: if the compositor is ever used with contexts which have a
           scale factor other than unity, then all length arguments should
           be divided by the appropriate context scaling factor i.e.
           convertPixX, convertPixY.  The FE callback is responsible for
           context specific scaling. */
        IL_DisplaySubImage(backdrop->image_req, 0, 0,
                           rect->left / context->convertPixX,
                           rect->top / context->convertPixY,
                           (rect->right - rect->left) / context->convertPixX,
                           (rect->bottom - rect->top) / context->convertPixY);
    }
}

/* painter_func for a background layer */
static void
lo_background_painter_func(CL_Drawable *drawable, 
                           CL_Layer *layer, 
                           FE_Region update_region)
{
    lo_BackgroundRectStruct bgrect;
    lo_BackgroundLayerClosure *closure;
    closure = (lo_BackgroundLayerClosure *)CL_GetLayerClientData(layer);
    
    bgrect.closure = closure;
    bgrect.layer = layer;

#ifdef XP_MAC
    /* If we're a grid parent, then we don't want to draw our background */
    if (closure->context->grid_children && !XP_ListIsEmpty(closure->context->grid_children))
        return;
#endif /* XP_MAC */
 
    /* 
     * For Windows and X, we draw the backgrounds of grid parents
     * since clipping of child windows is done by the windowing system.
     */
    
    FE_SetDrawable(closure->context, drawable);

    /* XXX - temporary, because Exceed X server has bugs with its
       offscreen tiling of clipped areas. */
    /* NOTE -- this causes a significant performance hit on other
     * Unix platforms.  Is there no other way around this Exceed bug?
     */
#ifdef XP_UNIX
    FE_ForEachRectInRegion(update_region, 
			   (FE_RectInRegionFunc)lo_background_rect_func,
			   (void *)&bgrect);
#else
    CL_ForEachRectCoveringRegion(update_region, 
				 (FE_RectInRegionFunc)lo_background_rect_func,
				 (void *)&bgrect);
#endif

    FE_SetDrawable(closure->context, NULL);
}

static void
lo_background_destroy_func(CL_Layer *layer)
{
    LO_Color *bg_color;
    LO_ImageStruct *backdrop;
    lo_BackgroundLayerClosure *closure;
    closure = (lo_BackgroundLayerClosure *)CL_GetLayerClientData(layer);
    
    XP_ASSERT(closure->type == LO_HTML_BACKGROUND_LAYER);

    bg_color = closure->bg_color;
    if (bg_color)
        XP_FREE(bg_color);

    backdrop = closure->backdrop_image;
    if (backdrop) {
        if (backdrop->image_req)
            IL_DestroyImage(backdrop->image_req);
        if (backdrop->image_url)
            XP_FREE(backdrop->image_url);
        XP_FREE(backdrop->image_attr);
        XP_FREE(backdrop);
    }

    XP_DELETE(closure);
}

/* Creates a HTML background layer to be a child of a layer group */
PRIVATE
CL_Layer *
lo_CreateBackgroundLayer(MWContext *context, PRBool is_document_backdrop)
{
    CL_Layer *layer;
    XP_Rect bbox = {0, 0, 0, 0};

    CL_LayerVTable vtable;
    lo_BackgroundLayerClosure *closure;
    
    uint32 flags = CL_DONT_ENUMERATE | CL_DONT_CLIP_SELF;

    XP_BZERO(&vtable, sizeof(CL_LayerVTable));
    vtable.painter_func = lo_background_painter_func;
    vtable.destroy_func = lo_background_destroy_func;
    if (is_document_backdrop)
        vtable.event_handler_func = lo_html_event_handler_func;

    closure = XP_NEW_ZAP(lo_BackgroundLayerClosure);
    closure->type = LO_HTML_BACKGROUND_LAYER;
    closure->context = context;
    closure->bg_type = is_document_backdrop ? BG_DOCUMENT : BG_LAYER;
    closure->tile_mode = LO_TILE_BOTH; /* Default tiling mode */
    
    layer = CL_NewLayer(LO_BACKGROUND_LAYER_NAME, 0, 0, &bbox, &vtable,
                        flags, (void *)closure);

    return layer;
}

/* Retrieve the background sub-layer for a layer group.
   If the <create> argument is TRUE, build the background layer
   if it doesn't exist. */
PRIVATE
CL_Layer *
lo_get_group_background(CL_Layer *layer, PRBool create)
{
    lo_GroupLayerClosure *closure;
    MWContext *context;
    CL_Layer *background_layer;

    if (! layer)
        return NULL;
        
    closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(layer);
    context = closure->context;

    XP_ASSERT(closure->type == LO_GROUP_LAYER);
    if (closure->type != LO_GROUP_LAYER)
        return NULL;

    background_layer = closure->background_layer;
    if (!background_layer && create) {
        PRBool is_document_backdrop = lo_is_document_layer(layer);
        background_layer = lo_CreateBackgroundLayer(context, 
                                                    is_document_backdrop);

        if (! background_layer) /* OOM */
            return NULL;
        CL_InsertChildByZ(layer, background_layer, Z_BACKGROUND_LAYER);
        closure->background_layer = background_layer;
    }
    return background_layer;
}

static void
lo_set_background_layer_bgcolor(CL_Layer *background_layer, LO_Color *new_bg_color)
{
    lo_BackgroundLayerClosure *closure;
    LO_Color *bg_color, *old_bg_color;
    LO_ImageStruct *backdrop;
    
    closure = (lo_BackgroundLayerClosure *)CL_GetLayerClientData(background_layer);

    bg_color = NULL;
    if (new_bg_color) {
        bg_color = XP_NEW_ZAP(LO_Color);
        if (!bg_color)
            return;
        *bg_color = *new_bg_color;
    }
    
    old_bg_color = closure->bg_color;
    if (old_bg_color)
        XP_FREE(old_bg_color);
    closure->bg_color = bg_color;
    backdrop = closure->backdrop_image;

    /* Make layer opaque if solid color or backdrop has no transparent areas */
    CL_ChangeLayerFlag(background_layer, CL_OPAQUE, 
                       (PRBool)(new_bg_color || (backdrop && !backdrop->is_transparent)));

    /* We can optimize drawing on top of this layer if it's a uniform color */
    if (!backdrop)
        CL_SetLayerUniformColor(background_layer, (CL_Color*)bg_color);

    /* Redraw the entire layer to reflect its new color. */
    {
        MWContext *context = closure->context;
        PRBool is_document_backdrop = (PRBool)(closure->bg_type == BG_DOCUMENT);
        XP_Rect rect = CL_MAX_RECT;

        if (is_document_backdrop) {
            XP_ASSERT(bg_color);
            if (bg_color) {
                /* Prevent default doc color from overriding JS-supplied color */
                lo_TopState *top_state = lo_get_layer_top_state(background_layer);
                if (top_state)  /* Paranoia */
                    top_state->doc_specified_bg = TRUE;
                FE_SetBackgroundColor(context,
                                      bg_color->red, bg_color->green, bg_color->blue);
            }
        }
        
        CL_UpdateLayerRect(context->compositor, background_layer, &rect, PR_FALSE);
    }
}

/* Set the background color for a layer group to the given RGB color.
   If <new_bg_color> is NULL, make the layer transparent. */
void
LO_SetLayerBgColor(CL_Layer *layer, LO_Color *new_bg_color)
{
    CL_Layer *background_layer;

    background_layer = lo_get_group_background(layer, PR_TRUE);
    if (!background_layer)
        return;
    
    lo_set_background_layer_bgcolor(background_layer, new_bg_color);
}

/* Retrieve a layer group's solid background color.
   Returned LO_Color structure must not be free'd or modified. */
LO_Color *
LO_GetLayerBgColor(CL_Layer *layer)
{
    lo_BackgroundLayerClosure *closure;
    CL_Layer *background_layer;

    background_layer = lo_get_group_background(layer, PR_FALSE);
    if (!background_layer)
        return NULL;
    
    closure = (lo_BackgroundLayerClosure *)CL_GetLayerClientData(background_layer);
    return closure->bg_color;
}

static LO_ImageStruct *
lo_get_background_layer_image(CL_Layer *background_layer, PRBool create)
{
    lo_BackgroundLayerClosure *closure;
    LO_ImageStruct *backdrop;
    
    closure = (lo_BackgroundLayerClosure *)CL_GetLayerClientData(background_layer);
    backdrop = closure->backdrop_image;

    /* Create the layout image structure if it doesn't exist. */
    if (! backdrop && create) {
        LO_ImageAttr *image_attr;
        PRBool is_cell_backdrop = (PRBool)(closure->bg_type == BG_CELL);
		PRBool is_layer_backdrop = (PRBool)(closure->bg_type == BG_LAYER);

        backdrop = XP_NEW_ZAP(LO_ImageStruct);
        if (!backdrop)
            return NULL;

        image_attr = XP_NEW(LO_ImageAttr);
        if (!image_attr) {
            XP_FREE(backdrop);
            return NULL;
        }

        /* Fake layout ID, guaranteed not to match any real layout element */
        backdrop->ele_id = -1;
		backdrop->type = LO_IMAGE;

        backdrop->image_attr = image_attr;
		backdrop->image_attr->attrmask = LO_ATTR_BACKDROP;
        if (is_layer_backdrop)
            backdrop->image_attr->attrmask |= LO_ATTR_LAYER_BACKDROP;
        else if (is_cell_backdrop)
            backdrop->image_attr->attrmask |= LO_ATTR_CELL_BACKDROP;
        
		backdrop->layer = background_layer;
        closure->backdrop_image = backdrop;
    }

    return backdrop;
}

/* Retrieve the LO_ImageStruct corresponding to a layer's backdrop,
   creating it if necessary. */
LO_ImageStruct *
LO_GetLayerBackdropImage(CL_Layer *layer)
{
    CL_Layer *background_layer;
    
    background_layer = lo_get_group_background(layer, PR_TRUE);
    if (!background_layer)
        return NULL;
    return lo_get_background_layer_image(background_layer, PR_TRUE);
}

void
LO_SetImageURL(MWContext *context,
               IL_GroupContext *img_cx,
               LO_ImageStruct *image,
               const char *url,
               NET_ReloadMethod reload_policy)
{
	PRBool replace = PR_FALSE; /* True if we are replacing an
								   existing image. */
    XP_ObserverList image_obs_list;
    CL_Layer *image_layer = image->layer;

    /* Clear out the resources of any previous backdrop image. */
    if (image->image_url)
        XP_FREE(image->image_url);
    image->image_url = NULL;
    if (image->image_req) {
		replace = PR_TRUE;
        IL_DestroyImage(image->image_req);
        image->image_req = NULL;
    }
    image->is_icon = FALSE;
    image->image_status = IL_START_URL;

    /* If there is a backdrop url to load, start loading the image. */
    if (url) {
        image->image_url = (PA_Block)XP_STRDUP(url);
        if (!image->image_url)
            return;
        
        image_obs_list = lo_NewImageObserverList(context, image);
        if (!image_obs_list)
            return;

        
        if (image_layer) {
			/* Is this a backdrop image ? */
			if (LO_GetLayerType(image_layer) == LO_HTML_BACKGROUND_LAYER) {
            
            /* Backdrop images are never scaled. */
            image->width = image->height = 0;

            /* We can't optimize drawing on top of a solid-colored
               background now that we have an image backdrop. */
            CL_SetLayerUniformColor(image_layer, NULL);
			}
			
			/* If we are replacing an existing image, then make the layer
				transparent to minimize flicker as the new image comes in. */
			if (replace) {
				CL_ChangeLayerFlag(image_layer, CL_OPAQUE, PR_FALSE);
				CL_ChangeLayerFlag(image_layer, CL_PREFER_DRAW_OFFSCREEN,
					               PR_TRUE);
			}
		}

        /* Initiate the loading of this image. */
        lo_GetImage(context, img_cx, image, image_obs_list, reload_policy);
    } else if (image_layer) {
        XP_Rect rect = CL_MAX_RECT;
        LO_LayerType layer_type = LO_GetLayerType(image_layer);

        /* No backdrop image.  Make the background layer transparent,
           unless the layer consists of a solid color. */
        if (layer_type == LO_HTML_BACKGROUND_LAYER) {
            lo_BackgroundLayerClosure *closure =
                (lo_BackgroundLayerClosure *)CL_GetLayerClientData(image_layer);

            CL_ChangeLayerFlag(image_layer, CL_OPAQUE, 
                               (PRBool)(closure->bg_color != NULL));

            /* Now, either the backdrop is a solid color or it's
               completely transparent. */
            CL_SetLayerUniformColor(image_layer, (CL_Color*)closure->bg_color);
        }

        /* If there is no URL, refresh the layer because it is
           now transparent. */
        CL_UpdateLayerRect(context->compositor, image_layer, &rect, PR_FALSE);
    }
}
    
/* set the tile mode of a backdrop image.  SetLayerBackdropURL must
 * be called before this function.
 */
void
lo_SetLayerBackdropTileMode(CL_Layer *layer, lo_TileMode tile_mode)
{
    lo_BackgroundLayerClosure *closure;
    CL_Layer *background_layer = lo_get_group_background(layer, PR_FALSE);
    if (!background_layer)
        return;
    closure = (lo_BackgroundLayerClosure *)CL_GetLayerClientData(background_layer);
    closure->tile_mode = tile_mode;
}

/* Set the backdrop URL for a layer group.
   If <url> is NULL or empty string, then clear the backdrop. */
void
LO_SetLayerBackdropURL(CL_Layer *layer, const char *url)
{
    LO_ImageStruct *image = LO_GetLayerBackdropImage(layer);
    MWContext *context = lo_get_layer_context(layer);
    lo_TopState *top_state = lo_get_layer_top_state(layer);
    IL_GroupContext *img_cx = context->img_cx;

    if (url && *url) {
        url = NET_MakeAbsoluteURL(top_state->base_url, (char*)url);
        LO_SetImageURL(context, img_cx, image, url, top_state->force_reload);
    } else
        LO_SetImageURL(context, img_cx, image, NULL, top_state->force_reload);
}

/* Get the backdrop URL for a layer group.  Return NULL if their is no
   backdrop URL. */
const char *
LO_GetLayerBackdropURL(CL_Layer *layer)
{
    CL_Layer *background_layer;
    LO_ImageStruct *backdrop;
    
    background_layer = lo_get_group_background(layer, PR_FALSE);
    if (!background_layer)
        return NULL;
    backdrop = lo_get_background_layer_image(background_layer, PR_FALSE);
    if (!backdrop)
        return NULL;
    
    return (const char *)backdrop->image_url;
}

void
lo_OffsetInflowLayer(CL_Layer *layer, int32 dx, int32 dy)
{
    lo_GroupLayerClosure *closure;
    lo_HTMLBlockClosure *content_closure;
    CL_Layer *content_layer;

    CL_OffsetLayer(layer, dx, dy);

    closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(layer);
    content_layer = closure->content_layer;
    if (content_layer) {
        content_closure = 
            (lo_HTMLBlockClosure *)CL_GetLayerClientData(content_layer);
        content_closure->x_offset += dx;
        content_closure->y_offset += dy;
    }
    
    closure->x_offset += dx;
    closure->y_offset += dy;
}

LO_CellStruct *
lo_GetCellFromLayer(MWContext *context, CL_Layer *layer)
{
    lo_GroupLayerClosure *closure;

    if (!layer)                 /* Paranoia */
        return NULL;
    
    closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(layer);
    if (closure->type != LO_GROUP_LAYER)
        return NULL;
    if (!closure->layer_state)  /* This must be the BODY layer */
        return NULL;
    return closure->layer_state->cell;
}


/**********************
 *
 * Cell background layering code
 *
 **********************/

static void
lo_cellbg_destroy_func(CL_Layer *layer)
{
    lo_BackgroundLayerClosure *closure;
    closure = (lo_BackgroundLayerClosure *)CL_GetLayerClientData(layer);
    /* 
     * NULL out the corresponding cell's pointer to this layer so that
     * there won't be double destruction. We assume that if the cell
     * is destroyed first, it will also destroy this layer.
     */
    if (closure->cell)
        closure->cell->cell_bg_layer = NULL;
    lo_background_destroy_func(layer);
}

/* Cell backgrounds are almost identical to regular backgrounds,
 * except that they are of finite size and they don't do any event
 * handling.
 */
CL_Layer *
lo_CreateCellBackgroundLayer(MWContext *context, LO_CellStruct *cell,
                             CL_Layer *parent_layer, int16 table_nesting_level)
{
    CL_Layer *cellbg_layer;
    XP_Rect bbox;
    lo_BackgroundLayerClosure *closure;    
    CL_LayerVTable vtable;
    int32 layer_x_offset, layer_y_offset;
    int32 parent_x_shift, parent_y_shift;
	int32 z_order;

        

    /* The layer is the size of the cell */
    bbox.left   = 0;
    bbox.top    = 0;
    bbox.right  = cell->width;
    bbox.bottom = cell->height;

    lo_GetLayerXYShift(parent_layer, &parent_x_shift, &parent_y_shift);

    layer_x_offset = cell->x + cell->x_offset - parent_x_shift;
    layer_y_offset = cell->y + cell->y_offset - parent_y_shift;
    
    /* Create the client_data */
    closure = XP_NEW_ZAP(lo_BackgroundLayerClosure);
    closure->type = LO_HTML_BACKGROUND_LAYER;
    closure->context = context; 
    closure->bg_type = BG_CELL;
    closure->tile_mode = cell->backdrop.tile_mode;
    closure->cell = cell;
   
    XP_BZERO(&vtable, sizeof(CL_LayerVTable));    
    vtable.painter_func = lo_background_painter_func;
    vtable.destroy_func = lo_cellbg_destroy_func;

    cellbg_layer = CL_NewLayer(NULL, layer_x_offset, layer_y_offset,
                               &bbox, &vtable,
                               CL_HIDDEN | CL_DONT_ENUMERATE, 
                               (void *)closure);

    if (!cellbg_layer)
        return NULL;

	/* The z-order of the background layer of cells should increase
	   as the cell's nest within tables.  This ensures that the innermost
	   cell's background ends up with the highest z-order and gets
	   displayed 
	*/	   
	z_order = Z_CELL_BACKGROUND_LAYER + table_nesting_level;
    CL_InsertChildByZ(parent_layer, cellbg_layer, z_order);

    /* Start loading tiled cell backdrop image, if present */
    if (cell->backdrop.url && *cell->backdrop.url) {
        LO_ImageStruct *image;

        image = lo_get_background_layer_image(cellbg_layer, PR_TRUE);
        if (image) {
            lo_TopState *top_state = lo_get_layer_top_state(parent_layer);
            char *url = NET_MakeAbsoluteURL(top_state->base_url, cell->backdrop.url);
            if (url) {
                XP_FREE(cell->backdrop.url);
                cell->backdrop.url = url;
				/* Note: In the case of nested tables with background
				images, the image request is destroyed before the table
				relayout and will not exist in the cache. Changing reload
				policy if it is NET_CACHE_ONLY_RELOAD to NET_DONT_RELOAD
				allows loading the image again from a file if it has 
				been removed from the cache.

				Trying to use the trash list would cause memory leaks
				as the trash list removes items from the mem arena. Allocating
				the image_request by taking it from the arena (instead of
				explicitly allocating it) could cause problems in
				the editor code.
				*/	
                LO_SetImageURL(context, context->img_cx, image, url,
					   ((top_state->force_reload == NET_CACHE_ONLY_RELOAD)?
									NET_DONT_RELOAD : top_state->force_reload));
				XP_FREE(url);
            }
        }
    }

    /* Set cell background color, if the cell isn't transparent. */
    if (cell->backdrop.bg_color) {
	if (UserOverride)
	    lo_set_background_layer_bgcolor(cellbg_layer, &lo_master_colors[LO_COLOR_BG]);
	else
            lo_set_background_layer_bgcolor(cellbg_layer, cell->backdrop.bg_color);
    }

    return cellbg_layer;
}

/* Create a block layer - a layer created by a HTML <LAYER> tag */
CL_Layer *
lo_CreateBlockLayer(MWContext *context, 
                    char *name,
                    PRBool is_inflow,
                    int32 x_offset, int32 y_offset,
                    int32 wrap_width,
                    lo_LayerDocState *layer_state,
                    lo_DocState *state)
{
    CL_Layer *content_layer, *block_layer;
    lo_GroupLayerClosure *closure;
    
    content_layer = lo_CreateBlockContentLayer(context, is_inflow,
                                               x_offset, y_offset, 
                                               layer_state, state);

    if (! content_layer)
        return NULL;

    block_layer = lo_CreateGroupLayer(context, name,
                                      x_offset, y_offset,
                                      TRUE, TRUE, content_layer,
                                      layer_state,

                                      NULL, 
                                      is_inflow ? 
                                      lo_html_inflow_layer_event_handler_func :
                                      lo_html_event_handler_func,

                                      lo_html_block_destroy_func);


    if (! block_layer) {
        CL_DestroyLayer(content_layer);
        return NULL;
    }

    closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(block_layer);
    closure->wrap_width = wrap_width;
    closure->is_inflow = is_inflow;
    
    return block_layer;
}


/* Create the _CONTENT child of the _BODY layer */
static CL_Layer *
lo_CreateBodyContentLayer(MWContext *context)
{
    CL_Layer *layer;
    XP_Rect bbox;
    CL_LayerVTable vtable;
    lo_HTMLBodyClosure *closure;

    uint32 flags = CL_DONT_ENUMERATE | CL_DONT_CLIP_SELF;

    bbox.left = 0;
    bbox.top = 0;
    bbox.right = 0;
    bbox.bottom = 0;
    
    XP_BZERO(&vtable, sizeof(CL_LayerVTable));
    vtable.painter_func = lo_body_painter_func;
    vtable.destroy_func = lo_html_destroy_func;

    closure = XP_NEW_ZAP(lo_HTMLBodyClosure);
    closure->type = LO_HTML_BODY_LAYER;
    closure->context = context;

    layer = CL_NewLayer(LO_CONTENT_LAYER_NAME, 0, 0, &bbox, &vtable,
                        flags, (void *)closure);
    closure->layer = layer;

    return layer;
}


/* 
 * Creates the HTML body layer
 */
static CL_Layer *
lo_CreateHTMLBodyLayer(MWContext *context)
{
    CL_Layer *content_layer, *body_layer;

    /* First create the _CONTENT part of the _BODY layer */
    content_layer = lo_CreateBodyContentLayer(context);
    
    /* 
     * Now create the abstract _BODY layer. The _CONTENT layer
     * is a child of this layer, but there's no background.
     */
    body_layer = lo_CreateGroupLayer(context, LO_BODY_LAYER_NAME, 
                                     0, 0,
                                     FALSE, TRUE, content_layer,
                                     NULL,

                                     body_bbox_changed_func,
                                     lo_html_event_handler_func,

                                     lo_html_destroy_func);

    CL_ChangeLayerFlag(body_layer, CL_HIDDEN, PR_FALSE);
    return body_layer;
}

void
LO_SetDocumentDimensions(MWContext *context, int32 width, int32 height)
{
    XP_Rect bbox;
    int32 current_width, current_height;
    
    lo_TopState *top_state = lo_FetchTopState(XP_DOCID(context));
    if (!top_state)
        return;

    /* If layering is enabled, set the size of the _BODY layer.  That
       will enlarge/shrink the _DOCUMENT layer as necessary, and call
       FE_SetDocDimension if the document layer changes size. */
    if (context->compositor) {
        bbox.left = bbox.top = 0;
        bbox.right = width;
        bbox.bottom = height;
        CL_SetLayerBbox(top_state->body_layer, &bbox);

        /* The document layer is always at least as large as the body
           layer, so if the body expands beyond the document's size,
           tell the FE about it immediately, rather than waiting for
           it to occur as a callback from the timing-driven composite
           cycle.  That avoids race conditions in which
           FE_SetDocPosition() is called before FE_SetDocDimension()
           has specified a sufficiently large document to encompass
           the new document position. */
        current_width = LO_GetLayerScrollWidth(top_state->doc_layer);
        current_height = LO_GetLayerScrollHeight(top_state->doc_layer);

        if ((width > current_width) || (height > current_height) || (width == 0) || (height == 0))
            doc_bbox_changed_func(top_state->doc_layer, &bbox);
    } else {
        FE_SetDocDimension(context, FE_VIEW, width, height);
    }
}

/* Creates the _DOCUMENT, _BODY and _BACKGROUND layers and 
   adds them to the layer tree */
void
lo_CreateDefaultLayers(MWContext *context,
                       CL_Layer **doc_layer,
                       CL_Layer **body_layer)
{
    *doc_layer = NULL;
    
    *body_layer = lo_CreateHTMLBodyLayer(context);
    if (!*body_layer)
        return;
    
    *doc_layer = lo_CreateGroupLayer(context, LO_DOCUMENT_LAYER_NAME,
                                     0, 0, FALSE, FALSE,
                                     *body_layer,
                                     NULL,

                                     doc_bbox_changed_func, NULL,

                                     lo_html_destroy_func);


    if (!*doc_layer)
        return;

    CL_ChangeLayerFlag(*doc_layer, CL_HIDDEN, PR_FALSE);
    if (context->compositor)
        CL_SetCompositorRoot(context->compositor, *doc_layer);        
}

/* Attaches the new layer into the layer structure */
void
lo_AttachHTMLLayer(MWContext *context, CL_Layer *layer, CL_Layer *parent,
                   char *above, char *below, int32 z_order)
{
    CL_Layer *sibling;
    CL_LayerPosition pos;

    if (parent != NULL)
    {
	if (z_order >= 0)
	{
	    CL_InsertChildByZ(parent, layer, z_order);
	}
	else
	{
	    sibling = NULL;
	    if (above != NULL)
	    {
		pos = CL_BELOW;
		sibling = CL_GetLayerChildByName(parent, above);
	    }
	    if ((sibling == NULL)&&(below != NULL))
	    {
		pos = CL_ABOVE;
		sibling = CL_GetLayerChildByName(parent, below);
	    }
	    if (sibling == NULL)
	    {
		pos = CL_ABOVE;
	    }
	    CL_InsertChild(parent, layer, sibling, pos);
	}
    }
}

/* Destroy all HTML-created layers */
void
lo_DestroyLayers(MWContext *context)
{
    CL_DestroyLayerTree(CL_GetCompositorRoot(context->compositor));
    CL_SetCompositorRoot(context->compositor, NULL);
}

LO_LayerType
LO_GetLayerType(CL_Layer *layer)
{
    lo_AnyLayerClosure *closure = (lo_AnyLayerClosure *)CL_GetLayerClientData(layer);
    
    return closure->type;
}

/* Push a layer onto the document's layer stack */
void
lo_PushLayerState(lo_TopState *top_state, lo_LayerDocState *layer_state)
{
    lo_LayerStack *lptr;

    lptr = XP_NEW(lo_LayerStack);
    if (lptr == NULL)
    {
	return;
    }
    lptr->layer_state = layer_state;
    lptr->next = top_state->layer_stack;
    top_state->layer_stack = lptr;
}

/* Pop a layer off the document's layer stack */
lo_LayerDocState *
lo_PopLayerState(lo_DocState *state)
{
    lo_LayerStack *lptr;
    lo_LayerDocState *layer_state;
    lo_TopState *top_state = state->top_state;
    
    if (top_state->layer_stack == NULL)
        return NULL;
    
    lptr = top_state->layer_stack;
    layer_state = lptr->layer_state;
    top_state->layer_stack = lptr->next;
    
    XP_DELETE(lptr);
    
    return layer_state;
}

/* Look at the top layer on the document's layer stack, without popping it */
lo_LayerDocState *
lo_CurrentLayerState(lo_DocState *state)
{
    lo_LayerDocState *layer_state;
    lo_TopState *top_state = state->top_state;
    if (top_state->layer_stack == NULL)
        return NULL;
    layer_state = top_state->layer_stack->layer_state;
    return layer_state;
}

int32
lo_CurrentLayerId(lo_DocState *state)
{
    lo_LayerDocState *layer_state = lo_CurrentLayerState(state);
    return layer_state->id;
}

CL_Layer *
lo_CurrentLayer(lo_DocState *state)
{
    lo_LayerDocState *layer_state = lo_CurrentLayerState(state);
    return layer_state->layer;
}

void
lo_DeleteLayerStack(lo_DocState *state)
{
    lo_LayerStack *lptr;
    lo_LayerStack *prev;
    lo_TopState *top_state = state->top_state;

    lptr = top_state->layer_stack;
    while (lptr != NULL) {
        prev = lptr;
        lptr = lptr->next;
        XP_DELETE(prev);
    }
    top_state->layer_stack = NULL;
}

/**********************
 *
 * Embedded object layering code
 *
 **********************/


/* Redraw an embedded object window because its position or visibility may
   have been altered. */
static void
lo_update_embedded_object_window(CL_Layer *layer)
{ 
    int32 x_save, y_save;
    LO_Any *any;
    MWContext *context;
    lo_EmbeddedObjectClosure *closure;
    LO_Element *tptr;
    XP_Rect rect;
    CL_Compositor *compositor = CL_GetLayerCompositor(layer);

    closure = (lo_EmbeddedObjectClosure *)CL_GetLayerClientData(layer);
    XP_ASSERT(closure);
    if (! closure)
        return;

    /* If embedded object is not (yet) a window.  There's
       nothing to do. */
    if (!closure->is_windowed)
        return;

    context = closure->context;

    tptr = closure->element;
    any = &tptr->lo_any;
    XP_ASSERT(any);

    /* Convert layer-relative coordinates for element to document
       coordinates, since that's what the FE uses. */
    rect.top = rect.left = rect.right = rect.bottom = 0;
    CL_LayerToWindowRect(compositor, layer, &rect);
    CL_WindowToDocumentRect(compositor, &rect);
	
    /* Save old, layer-relative coordinates */
    x_save = any->x;
    y_save = any->y;

    /* Temporarily shift element to document coordinates */
    any->x = rect.left - any->x_offset;
    any->y = rect.top - any->y_offset;

    /* Call the platform-specific display code. */
    switch (tptr->type) {
    case LO_FORM_ELE:
        any->x -= tptr->lo_form.border_horiz_space;
        any->y -= tptr->lo_form.border_vert_space;
	FE_DisplayFormElement(context, FE_VIEW, &tptr->lo_form);
	break;
    case LO_EMBED:
	FE_DisplayEmbed(context, FE_VIEW, &tptr->lo_embed);
	break;
#ifdef JAVA
    case LO_JAVA:
	FE_DisplayJavaApp(context, FE_VIEW, &tptr->lo_java);
	break;
#endif
    default:
	XP_ASSERT(0);
    }

    /* Restore layer-relative coordinates */
    any->x = x_save;
    any->y = y_save;
}

#ifdef XP_UNIX

/* Same as FE_DisplayFormElement, except coordinates converted
   from layer-relative to document-relative to satisfy FE
   expectations. */
void
LO_DisplayFormElement(LO_FormElementStruct *form)
{
   XP_ASSERT(form->layer);
   if (!form->layer)
        return;
   lo_update_embedded_object_window(form->layer);
}

#endif /* XP_UNIX */

static void
lo_window_layer_visibility_changed(CL_Layer *layer,
				   PRBool visible)
{
    lo_EmbeddedObjectClosure *closure;
    LO_Element *tptr;

    closure = (lo_EmbeddedObjectClosure *)CL_GetLayerClientData(layer);
    XP_ASSERT(closure);

    tptr = closure->element;
    XP_ASSERT(tptr);

    switch (tptr->type) {
    case LO_FORM_ELE:
	tptr->lo_form.ele_attrmask &= ~LO_ELE_INVISIBLE;
	tptr->lo_form.ele_attrmask |= LO_ELE_INVISIBLE * !visible;
	break;
    case LO_EMBED:
	tptr->lo_embed.ele_attrmask &= ~LO_ELE_INVISIBLE;
	tptr->lo_embed.ele_attrmask |= LO_ELE_INVISIBLE * !visible;
	break;
#ifdef JAVA
    case LO_JAVA:
	tptr->lo_java.ele_attrmask &= ~LO_ELE_INVISIBLE;
	tptr->lo_java.ele_attrmask |= LO_ELE_INVISIBLE * !visible;
	break;
#endif
    default:
	XP_ASSERT(0);
	return;
    }

    lo_update_embedded_object_window(layer);
}

static void
lo_window_layer_position_changed(CL_Layer *layer,
				 int32 x_offset, int32 y_offset)
{ 
    lo_update_embedded_object_window(layer);
}

/* Only set if we're a print context */
static void
lo_embedded_object_painter_func(CL_Drawable *drawable, 
                                CL_Layer *layer, 
                                FE_Region update_region)
{
    lo_EmbeddedObjectClosure *closure = 
        (lo_EmbeddedObjectClosure *)CL_GetLayerClientData(layer);
    
    FE_SetDrawable(closure->context, drawable);

    lo_update_embedded_object_window(layer);

    FE_SetDrawable(closure->context, NULL);
}

/* Create a child layer to contain an embedded object, e.g. a plugin,
   applet or form widget. The child layer may be created as a "cutout"
   layer if the embedded object is a window, to prevent the compositor
   from overdrawing the window's contents. */
CL_Layer *
lo_CreateEmbeddedObjectLayer(MWContext *context,
                             lo_DocState *state, LO_Element *tptr)
{
    PRBool is_window;
    LO_Any *any = &tptr->lo_any;
    CL_Layer *layer, *parent_layer, *doc_layer;
    char *name;
    XP_Rect bbox;
    lo_EmbeddedObjectClosure *closure;
    CL_LayerVTable vtable;
    int vspace, hspace;
    int32 layer_x_offset, layer_y_offset;

    /* Layer is initially hidden, until lo_DisplayWhatever is called. */
    uint32 flags = CL_DONT_ENUMERATE | CL_HIDDEN;

    if (! context->compositor)
        return NULL;

    /* These types of elements aren't truly layers (they're embedded
       windows), so we can't hide them directly through the layer API.
       We need to set a flag so that the FE knows not to draw the
       window initially. */
    switch (tptr->type) {
    case LO_FORM_ELE:
	name = "_FORM_ELEMENT";
	tptr->lo_form.ele_attrmask  |= LO_ELE_INVISIBLE;
	vspace = tptr->lo_form.border_vert_space;
	hspace = tptr->lo_form.border_horiz_space;
	is_window = PR_TRUE;
	break;
    case LO_EMBED:
	name = "_PLUGIN";
	tptr->lo_embed.ele_attrmask |= LO_ELE_INVISIBLE;
	/* We don't commit to the type of a plugin layer until
	   it's known whether or not the plugin is windowless. */
	vspace = tptr->lo_embed.border_vert_space;
	hspace = tptr->lo_embed.border_horiz_space;
	is_window = PR_FALSE;
#ifdef SHACK
	if (XP_STRNCASECMP (tptr->lo_embed.value_list[0], "builtin/", 8) == 0) {
		is_window = PR_TRUE;
	}
#endif /* SHACK */
	break;
#ifdef JAVA
    case LO_JAVA:
	name = "_JAVA_APPLET";
	tptr->lo_java.ele_attrmask  |= LO_ELE_INVISIBLE;
	vspace = tptr->lo_java.border_vert_space;
	hspace = tptr->lo_java.border_horiz_space;
	is_window = PR_TRUE;
	break;
#endif
    default:
	XP_ASSERT(0);
	return NULL;
    }

    /* The layer is the size of the layout element.
     (For a plugin, this may not *really* reflect the size of the element,
     but it will get resized later.) */
    layer_x_offset = any->x + any->x_offset + hspace;
    layer_y_offset = any->y + any->y_offset + vspace;

    bbox.left   = 0;
    bbox.top    = 0;
    bbox.right  = any->width  - 2 * hspace;
    bbox.bottom = any->height - 2 * vspace;
    
    closure = XP_NEW_ZAP(lo_EmbeddedObjectClosure);
    closure->type = LO_EMBEDDED_OBJECT_LAYER;

    closure->context = context;
    closure->element = tptr;

    XP_BZERO(&vtable, sizeof(CL_LayerVTable));    

    if (is_window) {
        if (context->type != MWContextPrint)
            flags |= CL_CUTOUT;
        closure->is_windowed = PR_TRUE;
    }

    vtable.visibility_changed_func = lo_window_layer_visibility_changed;
    vtable.position_changed_func = lo_window_layer_position_changed;
    vtable.destroy_func = (CL_DestroyFunc)lo_html_destroy_func;
    if (context->type == MWContextPrint)
        vtable.painter_func = (CL_PainterFunc)lo_embedded_object_painter_func;

    layer = CL_NewLayer(name, layer_x_offset, layer_y_offset,
						&bbox, &vtable, flags, (void *)closure);

    /*
     * XXX I don't really like this. For embeds that
     * are part of the _BASE layer, we need to special case
     * the parent layer of the embed layer, since
     * the _DOCUMENT layer (and not the _BASE layer) is on
     * the layer stack.
     * This code exists for cell bg creation, too.
     */
    doc_layer = CL_GetCompositorRoot(context->compositor);
    parent_layer = lo_CurrentLayer(state);
    if (parent_layer == doc_layer) {
        lo_TopState *top_state = lo_get_layer_top_state(layer);
        parent_layer = top_state->body_layer;
    }

    XP_ASSERT(parent_layer);

	if (parent_layer)
  	    CL_InsertChildByZ(parent_layer, layer, Z_CONTENT_LAYERS);

    return layer;
}

/**********************
 *
 * Java Applet layer code
 *
 **********************/
#ifdef JAVA

#include "java.h"

static void
lo_java_painter_func(CL_Drawable *drawable, CL_Layer *layer, FE_Region update_region)
{
    lo_EmbeddedObjectClosure *closure;
    LO_JavaAppStruct *javaData;
    
    closure = (lo_EmbeddedObjectClosure *)CL_GetLayerClientData(layer);
    javaData = (LO_JavaAppStruct*)closure->element;
    
    FE_SetDrawable(closure->context, drawable);
    FE_DrawJavaApp(closure->context, FE_VIEW, javaData);
    FE_SetDrawable(closure->context, NULL);
}

static PRBool
lo_java_event_handler_func(CL_Layer *layer, CL_Event *event)
{
    NAppletEvent evt;
    lo_EmbeddedObjectClosure *closure =
                                (lo_EmbeddedObjectClosure *)CL_GetLayerClientData(layer);

    evt.id = 0;
    evt.data = (void*)event;
    return LJ_HandleEvent(closure->context, 
                            (LO_JavaAppStruct*)closure->element, 
                            (void*)&evt);
}


/* Modify the way in which a plugin's layer draws depending on whether
   or not it is a windowless plugin. */
void
LO_SetJavaAppTransparent(LO_JavaAppStruct *javaData)
{
    XP_Rect bbox;
    CL_LayerVTable vtable;
    lo_EmbeddedObjectClosure *closure;
    CL_Layer *layer = javaData->layer;
    XP_ASSERT(layer);
    if (! layer)
	return;
    
    closure = (lo_EmbeddedObjectClosure *)CL_GetLayerClientData(layer);
    XP_ASSERT(closure->type == LO_EMBEDDED_OBJECT_LAYER);
    closure->is_windowed = PR_FALSE;

    XP_BZERO(&vtable, sizeof(CL_LayerVTable));    
    vtable.destroy_func = (CL_DestroyFunc)lo_html_destroy_func;

	/* Windowless plugin */
	vtable.painter_func = lo_java_painter_func;
	vtable.event_handler_func = lo_java_event_handler_func;

    CL_SetLayerVTable(layer, &vtable);
    CL_ChangeLayerFlag(layer, CL_CUTOUT, PR_FALSE);

    /* At this point, the size of the plugin is known. */
    bbox.left = 0;
    bbox.top = 0;
    bbox.right = javaData->width;
    bbox.bottom = javaData->height;
    CL_SetLayerBbox(layer, &bbox);
}

#endif

/**********************
 *
 * Plugin layer code
 *
 **********************/

static void
lo_embed_painter_func(CL_Drawable *drawable, CL_Layer *layer, 
                      FE_Region update_region)
{
    lo_EmbeddedObjectClosure *closure;
    LO_EmbedStruct *embed;
    
    closure = (lo_EmbeddedObjectClosure *)CL_GetLayerClientData(layer);
    embed = (LO_EmbedStruct*)closure->element;
    
    FE_SetDrawable(closure->context, drawable);
    FE_DisplayEmbed(closure->context, FE_VIEW, embed);
    FE_SetDrawable(closure->context, NULL);
}

static PRBool
lo_windowless_embed_event_handler_func(CL_Layer *layer, CL_Event *event)
{
    lo_EmbeddedObjectClosure *closure =
        (lo_EmbeddedObjectClosure *)CL_GetLayerClientData(layer);

    return FE_HandleEmbedEvent(closure->context, 
                               (LO_EmbedStruct*)closure->element, 
                               event);
}


/* Modify the way in which a plugin's layer draws depending on whether
   or not it is a windowless plugin. */
void
LO_SetEmbedType(LO_EmbedStruct *embed, PRBool is_windowed)
{
    XP_Rect bbox;
    CL_LayerVTable vtable;
    lo_EmbeddedObjectClosure *closure;
    CL_Layer *layer = embed->layer;
    XP_ASSERT(layer);
    if (! layer)
	return;
    
    closure = (lo_EmbeddedObjectClosure *)CL_GetLayerClientData(layer);
    XP_ASSERT(closure->type == LO_EMBEDDED_OBJECT_LAYER);
    closure->is_windowed = is_windowed;

    XP_BZERO(&vtable, sizeof(CL_LayerVTable));    
    vtable.destroy_func = (CL_DestroyFunc)lo_html_destroy_func;
    if (is_windowed) {
	/* Windowed plugin */
	vtable.visibility_changed_func = lo_window_layer_visibility_changed;
	vtable.position_changed_func = lo_window_layer_position_changed;
    if (closure->context->type == MWContextPrint)
        vtable.painter_func = (CL_PainterFunc)lo_embedded_object_painter_func;
    } else {
	/* Windowless plugin */
	vtable.painter_func = lo_embed_painter_func;
	vtable.event_handler_func = lo_windowless_embed_event_handler_func;
    CL_ChangeLayerFlag(layer, CL_PREFER_DRAW_OFFSCREEN,
                       (PRBool)((CL_GetLayerFlags(layer) & CL_OPAQUE) == 0));
   }
    CL_SetLayerVTable(layer, &vtable);
    if (closure->context->type != MWContextPrint)
        CL_ChangeLayerFlag(layer, CL_CUTOUT, is_windowed);

    /* At this point, the size of the plugin is known. */
    bbox.left = 0;
    bbox.top = 0;
    bbox.right = embed->width;
    bbox.bottom = embed->height;
    CL_SetLayerBbox(layer, &bbox);

    if (is_windowed && !CL_GetLayerHidden(layer))
        lo_update_embedded_object_window(layer);
}


/**********************
 *
 * Image layering code
 *
 **********************/

#define ICON_X_OFFSET 4
#define ICON_Y_OFFSET 4

typedef struct lo_ImageRectStruct
{
    lo_ImageLayerClosure *closure;
    CL_Layer *layer;
} lo_ImageRectStruct;


/* Create a text element to be used to display ALT text. */
static LO_TextStruct *
lo_AltTextElement(MWContext *context)
{
	lo_TopState *top_state;
	lo_DocState *state;
    LO_TextStruct *text;

	/*
	 * Get the unique document ID, and retrieve this
	 * documents layout state.
	 */
	top_state = lo_FetchTopState(XP_DOCID(context));
	if ((top_state == NULL) || (top_state->doc_state == NULL)) {
		return NULL;
	}
	state = lo_CurrentSubState(top_state->doc_state);

    text = XP_NEW_ZAP(LO_TextStruct);
    if (!text)
        return NULL;
    text->text_attr = XP_NEW_ZAP(LO_TextAttr);
    if (!text->text_attr)
        return NULL;

    lo_SetDefaultFontAttr(state, text->text_attr, context);

    return text;
}

/* Draw an icon, ALT text and a temporary border. */
static void
lo_ImageDelayed(MWContext *context, LO_ImageStruct *image, int icon_number, int x, int y)
{
    int icon_width, icon_height;
    int x_offset, y_offset;
    PRBool drew_icon = PR_FALSE;
    LO_Color color;
    LO_TextStruct *text;
    IL_GroupContext *img_cx;
    
    /* Offsets for drawing the icon and ALT text. */
    x_offset = ICON_X_OFFSET;
    y_offset = ICON_Y_OFFSET;
    
    /* Draw the delayed icon. */
    img_cx = context->img_cx;
    IL_GetIconDimensions(img_cx, icon_number, &icon_width, &icon_height);
    if (x_offset + icon_width <= image->width &&
        y_offset + icon_height <= image->height) {
        IL_DisplayIcon(img_cx, icon_number, x + x_offset, y + y_offset);
        drew_icon = PR_TRUE;
    }
    
    /* Draw the ALT text. */
    if (image->alt) {
        text = lo_AltTextElement(context);
        if (text) {
            XP_Rect frame;
            int total_width, total_height, text_width, text_height;
			LO_TextInfo text_info;

            text->text = image->alt;
            text->text_len = image->alt_len;
 			FE_GetTextInfo(context, text, &text_info);

			text->width = text_info.max_width;
			text->height = text_info.ascent + text_info.descent;
 			if (drew_icon) {
				int h_space = 3;

				x_offset += icon_width + h_space;
				y_offset += (icon_height - text->height) / 2;
            }
			text->x = x + x_offset;
            text->y = y + y_offset;

			FE_GetTextFrame(context, text, 0, text->text_len - 1, &frame);

            text_width = frame.right - frame.left;
            text_height = frame.bottom - frame.top;

            if (text_width && text_height) {
                total_width = text_width + x_offset;
                total_height = text_height + y_offset;

                if (total_width <= image->width &&
                    total_height <= image->height)
                    FE_DisplayText(context, FE_VIEW, text, FALSE);
            }

            /* Free the ALT text element. */
            XP_FREE(text->text_attr);
            XP_FREE(text);
        }
    }

    /* Draw a temporary border within the bounds of the image.  We only do this
       if a border has not already been specified for the image. */
    if (!image->border_width) {
        color.red = color.green = color.blue = 0;
        FE_DisplayBorder(context, FE_VIEW, x, y, image->width, image->height,
                         1, &color, LO_BEVEL);
    }
}


/* Called for each rectangle in the update region */
static void
lo_image_rect_func(void *inclosure,
                   XP_Rect *rect)
{
    lo_ImageLayerClosure *closure = ((lo_ImageRectStruct *)inclosure)->closure;
    MWContext *context = closure->context;
    CL_Layer *layer = ((lo_ImageRectStruct *)inclosure)->layer;
    LO_ImageStruct *image = closure->image;
    int32 layer_x_origin, layer_y_origin;
	XP_Rect bbox = {0, 0, 0, 0};
	CL_Compositor *compositor = CL_GetLayerCompositor(layer);

    CL_WindowToDocumentRect(CL_GetLayerCompositor(layer), rect);

    /* Convert layer origin to document coordinates */
    CL_LayerToWindowRect(compositor, layer, &bbox);
    CL_WindowToDocumentRect(compositor, &bbox);
    layer_x_origin = bbox.left;
    layer_y_origin = bbox.top;

    /* Draw the image or icon. */
    if (image->is_icon) {
		char *image_url;

		PA_LOCK(image_url, char *, image->image_url);
		/* Determine whether the icon was explicitly requested by URL
		   (in which case only the icon is drawn) or whether it needs
		   to be drawn as a placeholder (along with the ALT text and
		   placeholder border) as a result of a failed image request. */
		if (image_url &&
            (*image_url == 'i'              ||
             !XP_STRNCMP(image_url, "/mc-", 4)  ||
             !XP_STRNCMP(image_url, "/ns-", 4))) {
			IL_DisplayIcon(closure->context->img_cx, image->icon_number, 0, 0);
		}
		else {
			lo_ImageDelayed(closure->context, image, image->icon_number, 0, 0);
		}
		PA_UNLOCK(image->image_url);
    }
    else {
        /* Note: if the compositor is ever used with contexts which have a
           scale factor other than unity, then all length arguments should
           be divided by the appropriate context scaling factor i.e.
           convertPixX, convertPixY.  The FE callback is responsible for
           context specific scaling. */
       IL_DisplaySubImage(image->image_req, 0, 0,
                          (rect->left - layer_x_origin) / context->convertPixX,
                          (rect->top - layer_y_origin) / context->convertPixY,
                          (rect->right - rect->left) / context->convertPixX,
                          (rect->bottom - rect->top) / context->convertPixY);
    }
}

/* painter_func for an image layer */
static void
lo_image_painter_func(CL_Drawable *drawable, 
                      CL_Layer *layer, 
                      FE_Region update_region)
{
    lo_ImageLayerClosure *closure = (lo_ImageLayerClosure *)CL_GetLayerClientData(layer);
    lo_ImageRectStruct image_rect;
    LO_ImageStruct *image = closure->image;
    int32 save_x, save_y, save_y_offset, save_bw;
    int16 save_x_offset;
    
    image_rect.closure = closure;
    image_rect.layer = layer;

    FE_SetDrawable(closure->context, drawable);

    /* XXX - temporary, because Exceed X server has bugs with its
       offscreen tiling of clipped areas. */
    /* NOTE -- this may cause a significant performance hit on other
     * Unix platforms.  Is there no other way around this Exceed bug?
     */
#ifdef XP_UNIX
    FE_ForEachRectInRegion(update_region, 
			   (FE_RectInRegionFunc)lo_image_rect_func,
			   (void *)&image_rect);
#else
    CL_ForEachRectCoveringRegion(update_region, 
				 (FE_RectInRegionFunc)lo_image_rect_func,
				 (void *)&image_rect);
#endif

    /* Draw selection feedback. */
    save_x = image->x;
    save_y = image->y;
    save_x_offset = image->x_offset;
    save_y_offset = image->y_offset;
    save_bw = image->border_width;
    image->x = image->y = image->y_offset = image->border_width = 0;
    image->x_offset = 0;
    
    FE_DisplayFeedback(closure->context, FE_VIEW, (LO_Element*)image);

    image->x = save_x;
    image->y = save_y;
    image->x_offset = save_x_offset;
    image->y_offset = save_y_offset;
    image->border_width = save_bw;

    FE_SetDrawable(closure->context, NULL);
}

static void
lo_image_destroy_func(CL_Layer *layer)
{
    lo_ImageLayerClosure *closure = (lo_ImageLayerClosure *)CL_GetLayerClientData(layer);

    closure->image->layer = NULL;
    
    XP_DELETE(closure);
}

/* Create the image layer */
CL_Layer *
lo_CreateImageLayer(MWContext *context, LO_ImageStruct *image,
                    CL_Layer *parent)
{
    CL_Layer *layer;
    XP_Rect bbox = {0, 0, 0, 0};
    CL_LayerVTable vtable;
    lo_ImageLayerClosure *closure;
    int32 layer_x_offset, layer_y_offset;
    int bw = image->border_width;
    char *name, *str;

    uint32 flags = CL_OPAQUE | CL_DONT_ENUMERATE | CL_HIDDEN;

    layer_x_offset = image->x + image->x_offset + bw;
    layer_y_offset = image->y + image->y_offset + bw;
    
    XP_BZERO(&vtable, sizeof(CL_LayerVTable));
    vtable.painter_func = lo_image_painter_func;
    vtable.destroy_func = lo_image_destroy_func;

    closure = XP_NEW_ZAP(lo_ImageLayerClosure);
    closure->type = LO_IMAGE_LAYER;
    closure->context = context;

    PA_LOCK(str, char *, image->image_url);
    name = str;

    PA_UNLOCK(image->image_url);

    layer = CL_NewLayer(name, layer_x_offset, layer_y_offset, &bbox,
                        &vtable, flags, (void *)closure);

    /* Insert the image layer the _CONTENT layer. */
    if (parent)
        CL_InsertChildByZ(parent, layer, Z_CONTENT_LAYERS);

    closure->image = image;

    return layer;
}

/* Activate an image layer, allowing the compositor to start making calls to
   display the image.  This routine should be called only after the layout
   position of an image has been finalized, and it should only be called if
   the compositor exists. */
void
lo_ActivateImageLayer(MWContext *context, LO_ImageStruct *image)
{
    PRBool suppress_mode, is_mocha_image, draw_delayed_icon;
 	int x, y;
	CL_Layer *layer;
    char *image_url;
    
    XP_ASSERT(context->compositor);
    layer = image->layer;
    XP_ASSERT(layer);
    if (! layer)				/* Paranoia */
        return;

    /* Determine the feedback suppression mode for non-icon, non-backdrop
       images. */
    switch (image->suppress_mode) {
    case LO_SUPPRESS_UNDEFINED:
        suppress_mode = (PRBool)image->is_transparent;
        break;
        
    case LO_SUPPRESS:
        suppress_mode = PR_TRUE;
        break;
        
    case LO_DONT_SUPPRESS:
        suppress_mode = PR_FALSE;
        break;
        
    default:
        break;
    }

    /* Don't draw delayed icon for JavaScript-generated images. */
    PA_LOCK(image_url, char *, image->image_url);
    if (NET_URL_Type(image_url) == MOCHA_TYPE_URL)
        is_mocha_image = PR_TRUE;
    else
        is_mocha_image = PR_FALSE;
    PA_UNLOCK(image->image_url);

    /* Determine whether to draw the delayed icon, ALT text and a 
       temporary border as the image is loading. */
    draw_delayed_icon = (PRBool)(!suppress_mode && !image->is_icon &&
        !is_mocha_image && image->image_attr &&
        !(image->image_attr->attrmask & LO_ATTR_BACKDROP) &&
        (image->image_status != IL_FRAME_COMPLETE) &&
        (image->image_status != IL_IMAGE_COMPLETE));

    if (!(image->ele_attrmask & LO_ELE_DRAWN)) {
		/* Determine the new position of the layer. */
		x = image->x + image->x_offset + image->border_width;
		y = image->y + image->y_offset + image->border_width;

        /* Move layer to new position and unhide it. */
        CL_MoveLayer(layer,	x, y);
        CL_SetLayerHidden(layer, PR_FALSE);
        image->ele_attrmask |= LO_ELE_DRAWN;

        /* In order to increase the perceived speed of image loading,
           non-backdrop, non-icon images have the delayed icon, ALT text
           and a temporary border drawn in their place before they come in. */
        if (draw_delayed_icon)
            lo_ImageDelayed(context, image, IL_IMAGE_DELAYED, x, y);
    }
    else {
        /* In order to increase the perceived speed of image loading,
           non-backdrop, non-icon images have the delayed icon, ALT text
           and a temporary border drawn in their place before they come in. */
        XP_Rect *valid_rect = &image->valid_rect;
        int valid_width, valid_height;

        valid_width = valid_rect->right - valid_rect->left;
        valid_height = valid_rect->bottom - valid_rect->top;

	    /* Get the offset of the image layer relative to its parent layer. */
		x = CL_GetLayerXOffset(image->layer);
		y = CL_GetLayerYOffset(image->layer);

        if (draw_delayed_icon && !valid_width && !valid_height)
            lo_ImageDelayed(context, image, IL_IMAGE_DELAYED, x, y);
    }
}

#ifdef DEBUG
/* Utility function for debugging */
MWContext *
LO_CompositorToContext(CL_Compositor *compositor)
{
    CL_Layer *doc_layer = CL_GetCompositorRoot(compositor);
    lo_GroupLayerClosure *closure = (lo_GroupLayerClosure *)CL_GetLayerClientData(doc_layer);
    return closure->context;
}
#endif /* DEBUG */

#ifdef PROFILE
#pragma profile off
#endif
