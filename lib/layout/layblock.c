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
#include "pa_parse.h"
#include "pa_tags.h"
#include "layout.h"
#include "laylayer.h"
#include "laystyle.h"
#include "libmocha.h"
#include "libevent.h"
#include "layers.h"
#include "intl_csi.h"

extern Bool lo_IsEmptyTag(TagType type);

/* This struct is used during the processing of a <LAYER> or <ILAYER>
 * tag, but discarded after the tag is closed. It is used to store the
 * current document state while process the tag and channel everything
 * into the layer.
 */
struct lo_Block {
    MWContext *context;

    /* Saved document state, restored after block is layed out */
    lo_DocState *saved_state;

	int32 start_x, start_y;     /* Initial upper-left corner of block,
                                   in parent layer coord system */
	char *old_base_url;
    uint8 old_body_attr;

	char * name;        /* Identifier for this layer */
	char * above;		/* Name of layer above this layer or NULL */
	char * below;		/* Name of layer below this layer or NULL */
    int32 z_order;		/* Z-order if above/below unspecified */

    int32 x_offset, y_offset;   /* Layer coordinate translation
                                   (for in-flow layers) */
    PRPackedBool is_inflow; /* Is this an out-of-flow or in-flow layer ? */
    PRPackedBool is_inline; /* Is this inline or SRCed */
    PRPackedBool is_dynamic; /* Has the SRC been dynamically changed?  */
    PRPackedBool uses_ss_positioning; /* Created using style-sheet 'position'
                                         property instead of tag ? */
    int clip_expansion_policy;  /* Expand clip to contain layer content ? */

    CL_Layer *layer;
    LO_CellStruct *cell;

    lo_LayerDocState *old_layer_state;  /* Used during table relayout */
    char *match_code;           /* Value of layer tag's MATCH parameter */
	char *source_url;
};

static int lo_AppendLayerElement(MWContext *context, lo_DocState *state, int32 is_end);

#ifdef XP_MAC
PRIVATE
#endif
lo_LayerDocState *
lo_GetLayerStateFromId(MWContext *context, int32 layer_id)
{
	int32 doc_id;
	lo_TopState *top_state;
    lo_LayerDocState *layer_state;

	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	XP_ASSERT(top_state);
	if (!top_state)
		return NULL;

    XP_ASSERT(layer_id <= top_state->max_layer_num);
    if (layer_id > top_state->max_layer_num)
        return NULL;
    layer_state = top_state->layers[layer_id];
/*    XP_ASSERT(layer_state); */
    return layer_state;
}

void *
LO_GetLayerMochaObjectFromId(MWContext *context, int32 layer_id)
{
    lo_LayerDocState *layer_state = lo_GetLayerStateFromId(context, layer_id);
    if (!layer_state)
        return NULL;
    return layer_state->mocha_object;
}

void *
LO_GetLayerMochaObjectFromLayer(MWContext *context, CL_Layer *layer)
{
	lo_LayerDocState *layer_state = lo_GetLayerState(layer);
	if (!layer_state)
        return NULL;
    return layer_state->mocha_object;
}

void
LO_SetLayerMochaObject(MWContext *context, int32 layer_id, void *mocha_object)
{
	lo_LayerDocState *layer_state = lo_GetLayerStateFromId(context, layer_id);
	if (!layer_state)
        return;
    layer_state->mocha_object = mocha_object;
}

CL_Layer *
LO_GetLayerFromId(MWContext *context, int32 layer_id)
{
	lo_LayerDocState *layer_state = lo_GetLayerStateFromId(context, layer_id);
	if (!layer_state)
        return NULL;
    return layer_state->layer;
}

int32
LO_GetIdFromLayer(MWContext *context, CL_Layer *layer)
{
	lo_LayerDocState *layer_state = lo_GetLayerState(layer);
    XP_ASSERT(layer_state);
    if (!layer_state)
        return 0;
    return layer_state->id;
}

int32
LO_GetNumberOfLayers(MWContext *context)
{
    int32 doc_id;
    lo_TopState *top_state;

    doc_id = XP_DOCID(context);
    top_state = lo_FetchTopState(doc_id);
    if (!top_state)
	    return 0;

    return (top_state->max_layer_num);
}

PRBool
lo_IsCurrentLayerDynamic(lo_DocState *state)
{
	lo_LayerDocState *layer_state = lo_CurrentLayerState(state);
    lo_Block *block = layer_state->temp_block;

    if (layer_state->id == LO_DOCUMENT_LAYER_ID)
        return PR_FALSE;

    XP_ASSERT(block);
    if (!block)
        return PR_FALSE;

    return (PRBool)block->is_dynamic;
}

PRBool
lo_IsAnyCurrentAncestorSourced(lo_DocState *state)
{
    lo_LayerStack *lptr;
    lo_TopState *top_state = state->top_state;

    if (top_state->layer_stack == NULL)
        return PR_FALSE;

    lptr = top_state->layer_stack;
    while (lptr && lptr->layer_state &&
           (lptr->layer_state->id != LO_DOCUMENT_LAYER_ID)) {
        lo_Block *block = lptr->layer_state->temp_block;
        if (block && !block->is_inline)
            return PR_TRUE;

        lptr = lptr->next;
    }

    return PR_FALSE;
}

PRBool
lo_IsTagInSourcedLayer(lo_DocState *state, PA_Tag *tag)
{
    PRBool ancestor_sourced = PR_FALSE;
    PRBool current_layer_sourced = PR_FALSE;
    lo_LayerStack *lptr;
    lo_TopState *top_state = state->top_state;

    if (top_state->layer_stack == NULL)
        return PR_FALSE;

    lptr = top_state->layer_stack;
    while (lptr && lptr->layer_state &&
           (lptr->layer_state->id != LO_DOCUMENT_LAYER_ID)) {
        lo_Block *block = lptr->layer_state->temp_block;
        if (block && !block->is_inline) {
            if (top_state->layer_stack == lptr)
                current_layer_sourced = PR_TRUE;
            else {
                ancestor_sourced = PR_TRUE;
                break;
            }
        }
        lptr = lptr->next;
    }

    /*
     * If we're not dealing with a </LAYER> or </ILAYER> tag,
     * then we just return whether any current layer ancestor
     * has a SRC attribute.
     */
    if (((tag->type != P_LAYER) && (tag->type != P_ILAYER)) ||
        (tag->is_end == FALSE) ||
        (top_state->ignore_layer_nest_level != 0))
        return (PRBool)(ancestor_sourced || current_layer_sourced);
    /*
     * Otherwise, it may be possible that the current close tag
     * actually closes out the sourced layer and isn't actually
     * part of the content of the sourced layer.
     */
    else
        return ancestor_sourced;
}


/*
 * Are there any dynamic layers on the current layer_stack?
 */
PRBool
lo_IsAnyCurrentAncestorDynamic(lo_DocState *state)
{
    lo_LayerStack *lptr;
    lo_TopState *top_state = state->top_state;

    if (top_state->layer_stack == NULL)
        return PR_FALSE;

    lptr = top_state->layer_stack;
    while (lptr && lptr->layer_state &&
           (lptr->layer_state->id != LO_DOCUMENT_LAYER_ID)) {
        lo_Block *block = lptr->layer_state->temp_block;
        if (block && block->is_dynamic)
            return PR_TRUE;

        lptr = lptr->next;
    }

    return PR_FALSE;
}

static void
lo_init_block_cell(MWContext *context, lo_DocState *state, lo_Block *block)
{
	LO_CellStruct *cell;

	if ((block == NULL)||(block->cell == NULL))
	{
		return;
	}

	cell = block->cell;

	cell->type = LO_CELL;
	cell->ele_id = NEXT_ELEMENT;
	cell->x = state->x;
	cell->x_offset = 0;
	cell->y = state->y;
	cell->y_offset = 0;
	cell->width = 0;
	cell->height = 0;
    cell->line_height = state->line_height;
	cell->next = NULL;
	cell->prev = NULL;
	cell->FE_Data = NULL;
	cell->cell_float_list = NULL;
	cell->backdrop.bg_color = NULL;
    cell->backdrop.url = NULL;
	cell->border_width = 0;
	cell->border_vert_space = 0;
	cell->border_horiz_space = 0;
	cell->ele_attrmask = 0;
	cell->sel_start = -1;
	cell->sel_end = -1;
	cell->cell_list = NULL;
	cell->cell_list_end = NULL;
	cell->cell_float_list = NULL;
    cell->cell_inflow_layer = NULL;
    cell->cell_bg_layer = NULL;
	cell->table_cell = NULL;
	cell->table_row = NULL;
	cell->table = NULL;
}

lo_LayerDocState *
lo_NewLayerState(MWContext *context)
{
	lo_LayerDocState *layer_state;
    XP_Rect everything = CL_MAX_RECT;

	layer_state = XP_NEW_ZAP(lo_LayerDocState);
	if (layer_state == NULL)
		return NULL;

	layer_state->id = 0;  /* This will be filled in later */
	layer_state->cell = NULL;
    layer_state->viewRect = everything;
    layer_state->doc_lists = XP_NEW_ZAP(lo_DocLists);
    if (!layer_state->doc_lists) {
		XP_FREE(layer_state);
        return NULL;
    }
	if (!lo_InitDocLists(context, layer_state->doc_lists)) {
        XP_FREE(layer_state->doc_lists);
		XP_FREE(layer_state);
		return NULL;
	}

	return layer_state;
}

#ifdef XP_MAC
PRIVATE
#endif
void
lo_DeleteBlock(lo_Block* block)
{
    XP_FREEIF(block->name);
    XP_FREEIF(block->above);
    XP_FREEIF(block->below);
    XP_FREEIF(block->saved_state);
    XP_FREEIF(block->match_code);
    XP_FREEIF(block->source_url);
    XP_FREE(block);
}

void
lo_DeleteLayerState(MWContext *context, lo_DocState *state,
                    lo_LayerDocState *layer_state)
{
	/* Get rid of the layer_state's contents */
	if (layer_state->cell)
		lo_RecycleElements(context, state, (LO_Element *)layer_state->cell);

	/* Get rid of the layer_state's layer */
	if (layer_state->layer) {
		CL_Layer *parent_layer = CL_GetLayerParent(layer_state->layer);
		if (parent_layer)
			CL_RemoveChild(parent_layer, layer_state->layer);
        LO_LockLayout();
		CL_DestroyLayerTree(layer_state->layer);
        LO_UnlockLayout();
	}

	lo_DeleteDocLists(context, layer_state->doc_lists);
    if (layer_state->id != LO_DOCUMENT_LAYER_ID)
        XP_FREE(layer_state->doc_lists);
    if (layer_state->temp_block)
        lo_DeleteBlock(layer_state->temp_block);
	XP_FREE(layer_state);
}

/* Number of entries to grow layers array when it is too small */
#define LAYER_ARRAY_GROW_SIZE  50

/*
 * Adds the layer_state to the layer list. If one with the same
 * id already exists (because of table relayout), we replace
 * it with the new one. The old layer_state is returned, so that
 * it may be discarded when the new layer is done loading.
 */
lo_LayerDocState *
lo_append_to_layer_array(MWContext *context, lo_TopState *top_state,
                         lo_DocState *state,
                         lo_LayerDocState *layer_state)
{
    int32 id;

    XP_ASSERT(top_state);
    if (!top_state)
        return NULL;

	id = ++top_state->current_layer_num;
    layer_state->id = id;

    if (state)
        state->current_layer_num_max = id;

	/*
	 * If we're doing table relayout, we find the corresponding layer in
	 * the layer list from a previous pass and replace it with the current
	 * layer.
	 */
	if (state && state->in_relayout) {
        lo_LayerDocState **old_layer_statep = &top_state->layers[id];
        lo_LayerDocState *old_layer_state = *old_layer_statep;

        XP_ASSERT(old_layer_state);
        XP_ASSERT(old_layer_state->id == id);

        /*
         * Copy over the mocha object, since the reflection might
         * already have happened.
         */
        layer_state->mocha_object = old_layer_state->mocha_object;

        /* Out with the old and in with the new */
        *old_layer_statep = layer_state;
        return old_layer_state;
    }

    /* Extend the layers array if it's full */
    if (id >= top_state->num_layers_allocated) {
        int32 new_count = top_state->num_layers_allocated + LAYER_ARRAY_GROW_SIZE;
        int32 nsize = new_count * sizeof(lo_LayerDocState*);
        lo_LayerDocState **new_layers;

        if (top_state->num_layers_allocated == 0)
            new_layers = (lo_LayerDocState**)XP_ALLOC(nsize);
        else
            new_layers = (lo_LayerDocState**)XP_REALLOC(top_state->layers, nsize);

        XP_ASSERT(new_layers);
        if (!new_layers)
            return NULL;
        top_state->layers = new_layers;
        top_state->num_layers_allocated = new_count;
    }

    top_state->layers[id] = layer_state;

    if (id > top_state->max_layer_num)
        top_state->max_layer_num = id;
    return NULL;
}

static void
lo_block_src_exit_fn(URL_Struct *url_struct, int status, MWContext *context)
{
    int32 doc_id;
    lo_TopState *top_state;
    lo_DocState *state;

    /* XXX need state to be subdoc state? */
    doc_id = XP_DOCID(context);
    top_state = lo_FetchTopState(doc_id);
    if ((top_state == NULL) ||
        ((state = top_state->doc_state) == NULL)) {
        return;
    }

    /* Flush tags blocked by this <LAYER SRC="URL"> tag. We treat
       the special case of an interrupted stream as normal completion,
       as this is the resulting error code when we do
       <LAYER SRC=some.gif> and some.gif is in the image cache. */
	if (status >= 0 || status == MK_INTERRUPTED) {
		top_state->layout_blocking_element = NULL;
		lo_FlushBlockage(context, state, state);
	}
	/* XXX What's the right thing to do when we fail???
	 * Presumably we don't want to flush the blockage and
	 * continue as if nothing happened. But in some cases,
	 * we do want to recover in some way.
	 */
    NET_FreeURLStruct(url_struct);
}

static void
lo_free_stream(MWContext *context, NET_StreamClass *stream)
{
    XP_DELETE(stream);
}

int32
lo_GetEnclosingLayerWidth(lo_DocState *state)
{
    lo_LayerDocState *layer_state = lo_CurrentLayerState(state);

    /* Special case 100% value for top-level document so that
       it doesn't include window margins. */
    if (layer_state->id == LO_DOCUMENT_LAYER_ID)
        return state->win_left + state->win_width + state->win_right;

    return state->right_margin - state->left_margin;
}

/* Convert a string containing a horizontal dimension into layout coordinates,
   handling percentages if necessary. */
static int32
lo_parse_horizontal_param(char *value, lo_DocState *state, MWContext *context)
{
    int32 parent_width = lo_GetEnclosingLayerWidth(state);
    XP_Bool is_percent;
    int32 ival;

    ival = lo_ValueOrPercent(value, &is_percent);
    if (is_percent)
    {
        if (state->allow_percent_width == FALSE)
        {
            ival = 0;
        }
        else
        {
            ival = (parent_width * ival) / 100;
        }
    }
    else
    {
        ival = FEUNITS_X(ival, context);
    }
    return ival;
}

int32
lo_GetEnclosingLayerHeight(lo_DocState *state)
{
    lo_LayerDocState *layer_state = lo_CurrentLayerState(state);
    return layer_state->height;
}

/* Convert a string containing a vertical dimension into layout coordinates,
   handling percentages if necessary. */
static int32
lo_parse_vertical_param(char *value, lo_DocState *state, MWContext *context)
{
    XP_Bool is_percent;
    int32 ival;
    int32 parent_height = lo_GetEnclosingLayerHeight(state);

    ival = lo_ValueOrPercent(value, &is_percent);
    if (is_percent)
        ival = (parent_height * ival) / 100;
    else
        ival = FEUNITS_Y(ival, context);
    return ival;
}

#ifdef XP_MAC
PRIVATE
#endif
int
lo_parse_clip(char *str, XP_Rect *clip,
              lo_DocState *state, MWContext *context)
{
    int32 coord, coords[4];
    int coord_count;
    int clip_expansion_policy = LO_AUTO_EXPAND_NONE;
    for (coord_count = 0; coord_count <= 3; coord_count++) {

        /* Skip leading whitespace and commas */
        while (XP_IS_SPACE(*str) || (*str == ','))
            str++;

        if (!*str)
            break;

        /* Parse distinguished "auto" value */
        if (!XP_STRNCASECMP(str, "auto", 4)) {
            str += 4;
            clip_expansion_policy |= 1;
            coords[coord_count] = 0;
        } else {
            if (coord_count & 1)
                coord = lo_parse_vertical_param(str, state, context);
            else
                coord = lo_parse_horizontal_param(str, state, context);
            coords[coord_count] = coord;

            /* Skip over the number and percentage sign */
            while (*str && !XP_IS_SPACE(*str) && !(*str == ','))
                str++;
        }
        clip_expansion_policy <<= 1;
    }
    clip_expansion_policy >>= 1;

    if (coord_count == 2) {
        clip->right  = coords[0];
        clip->bottom = coords[1];
        return clip_expansion_policy;
    } else if (coord_count >= 4) {
        clip->left   = coords[0];
        clip->top    = coords[1];
        clip->right  = coords[2];
        clip->bottom = coords[3];
        return clip_expansion_policy;
    }

    /* Error - don't clip at all */
    return LO_AUTO_EXPAND_CLIP;
}

extern XP_Bool
lo_BeginLayer(MWContext *context,
			  lo_DocState *state,
			  LO_BlockInitializeStruct *param,
			  XP_Bool is_inflow);

#ifdef XP_MAC
PRIVATE
#endif
void
lo_FreeBlockInitializeStruct(LO_BlockInitializeStruct *param)
{
	if(param)
	{
		XP_FREEIF(param->clip);
		XP_FREEIF(param->overflow);
		XP_FREEIF(param->name);
		XP_FREEIF(param->id);
		XP_FREEIF(param->above);
		XP_FREEIF(param->below);
		XP_FREEIF(param->visibility);
		XP_FREEIF(param->bgcolor);
		XP_FREEIF(param->bgimage);
		XP_FREEIF(param->src);

		XP_FREE(param);
	}
}

/* Start a <LAYER> or an <ILAYER>. */
void
lo_BeginLayerTag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
    int16 win_csid = INTL_GetCSIWinCSID(c);
	LO_BlockInitializeStruct *param;
	char *val;
    lo_LayerDocState *layer_state;
    CL_Layer *parent_layer;


    if (!context->compositor)
        return;

    layer_state = lo_CurrentLayerState(state);
    parent_layer = layer_state->layer;

	param = XP_NEW_ZAP(LO_BlockInitializeStruct);

	if(!param)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}

	/*
	 * Get the horizontal position of the block
	 */
    val = (char*) lo_FetchParamValue(context, tag, PARAM_LEFT);
    if(val)
	{
		param->has_left = TRUE;
		param->left = lo_parse_horizontal_param(val, state, context);
		XP_FREE(val);
	} else {
        /* No LEFT attribute.  Is there an X attribute ? */
        val = (char*) lo_FetchParamValue(context, tag, PARAM_PAGEX);
        if(val)
        {
            param->has_left = TRUE;
            param->left = (int32)XP_ATOI(val);

            /* Convert from absolute coordinates to layer-relative coordinates */
            param->left -= CL_GetLayerXOrigin(parent_layer);
            XP_FREE(val);
        }
    }

	/*
	 * Get the vertical position of the block
	 */
    val = (char*)lo_FetchParamValue(context, tag, PARAM_TOP);
	if (val)
	{
		param->has_top = TRUE;
		param->top = lo_parse_vertical_param(val, state, context);
		XP_FREE(val);
	} else {
        /* No TOP attribute.  Is there a Y attribute ? */
        val = (char*) lo_FetchParamValue(context, tag, PARAM_PAGEY);
        if(val)
        {
            param->has_top = TRUE;
            param->top = (int32)XP_ATOI(val);

            /* Convert from absolute coordinates to layer-relative coordinates */
            param->top -= CL_GetLayerYOrigin(parent_layer);
            XP_FREE(val);
        }
    }

	/*
	 * Parse the comma separated coordinate list into an
	 * array of integers.
	 */
	val = (char*)lo_FetchParamValue(context, tag, PARAM_CLIP);
	if(val)
	{
		param->clip = XP_NEW_ZAP(XP_Rect);

		if(param->clip)
		{
			XP_BZERO(param->clip, sizeof(XP_Rect));
			param->clip_expansion_policy =
                lo_parse_clip(val, param->clip, state, context);
		}
	}
	else
		param->clip_expansion_policy = LO_AUTO_EXPAND_CLIP;

	/*
	 * Get the width parameter, in absolute or percentage.
	 * If percentage, make it absolute.
	 */
	val = (char*)lo_FetchParamValue(context, tag, PARAM_WIDTH);
	if(val)
	{
        param->has_width = TRUE;
		param->width = lo_parse_horizontal_param(val, state, context);
		XP_FREE(val);
	}

	/*
	 * Get the height parameter, in absolute or percentage.
	 * If percentage, make it absolute.
	 */
	val = (char*)lo_FetchParamValue(context, tag, PARAM_HEIGHT);
	if(val)
	{
        param->has_height = TRUE;
		param->height = lo_parse_vertical_param(val, state, context);
		XP_FREE(val);
	}

    /* Get the OVERFLOW attribute. */
	param->overflow = (char*)lo_FetchParamValue(context, tag, PARAM_OVERFLOW);

	/*
	 * Get the optional name for this layer.
	 */
	param->name = (char*)PA_FetchParamValue(tag, PARAM_NAME, win_csid);

	/* Use either NAME or ID, since we will probably be switching to
       the latter */
	param->id = (char*)PA_FetchParamValue(tag, PARAM_ID, win_csid);

	/*
	 * Get the optional "above" name for the layer we are above.
	 */
	param->above = (char*)PA_FetchParamValue(tag, PARAM_ABOVE, win_csid);

	/*
	 * Get the optional "below" name for the layer we are below.
	 */
    param->below = (char*)PA_FetchParamValue(tag, PARAM_BELOW, win_csid);

    /*
     * Get the Z-order of the block
     */
	val  = (char*)lo_FetchParamValue(context, tag, PARAM_ZINDEX);
	if (val) {
		param->zindex = XP_ATOI(val);
		param->has_zindex = TRUE;
		XP_FREE(val);
	}
	else
		param->has_zindex = FALSE;

	/* Get the VISIBILITY parameter to know if this layer starts hidden. */
	param->visibility = (char*)PA_FetchParamValue(tag, PARAM_VISIBILITY, win_csid);

    /* Process background color (BGCOLOR) attribute, if present. */
    param->bgcolor = (char*)PA_FetchParamValue(tag, PARAM_BGCOLOR, win_csid);

    /* Process backdrop (BACKGROUND) image attribute, if present. */
    param->bgimage = lo_ParseBackgroundAttribute(context,
                                                     state,
                                                     tag, FALSE);

	param->src = (char*)PA_FetchParamValue(tag, PARAM_SRC, win_csid);

	param->tag = tag;
    param->ss_tag = NULL;

	if (lo_BeginLayer(context, state, param, tag->type == P_ILAYER))
		lo_FreeBlockInitializeStruct(param);
}

static int lo_AppendLayerElement(MWContext *context, lo_DocState *state, int32 is_end)
{
	LO_LayerStruct *layer;
	layer = (LO_LayerStruct*)lo_NewElement(context, state, LO_LAYER, NULL, 0);

	XP_ASSERT(layer);
	if (!layer) 
		return FALSE;

	layer->lo_any.type = LO_LAYER;
	layer->lo_any.x = state->x;
	layer->lo_any.y = state->y;
	layer->lo_any.x_offset = 0;
	layer->lo_any.y_offset = 0;
	layer->lo_any.width = 0;
	layer->lo_any.height = 0;
	layer->lo_any.line_height = 0;
	layer->lo_any.ele_id = NEXT_ELEMENT;

	layer->is_end = is_end;
	layer->initParams = NULL;	/* Not keeping params for now.  Just trying to fix ILAYER crash. */
	
	lo_AppendToLineList(context, state, (LO_Element*)layer, 0);

	return TRUE;
}

/* parse a comma separated list of 2 or 4 values
 *
 * Format is "rect(top, right, bottom, left)"
 * or "rect(right, bottom)"
 * or degenerate case of "top,right,bottom,left" with no rect()
 */
#ifdef XP_MAC
PRIVATE
#endif
XP_Rect *
lo_ParseStyleCoords(MWContext *context,
					lo_DocState *state,
					StyleStruct *style_struct,
					char *coord_string)
{

	XP_Rect *coords = XP_NEW_ZAP(XP_Rect);
	int32 val[4];
	int index=0;
	char *value;
	SS_Number *ss_num;

	if(!coords)
		return(NULL);

	coord_string = XP_StripLine(coord_string);

    /* go past "rect(" and kill ")" character */
#define _RECT "rect"
    if(!strncasecomp(coord_string, _RECT, sizeof(_RECT)-1)) {
        char *t;

        /* go past "rect" */
        coord_string += sizeof(_RECT)-1;

        /* go past spaces */
        while(XP_IS_SPACE(*coord_string)) coord_string++;

        /* go past '(' */
        if(*coord_string == '(')
            coord_string++;

        /* kill any more spaces */
        while(XP_IS_SPACE(*coord_string)) coord_string++;

        /* Kill the ending ')' */
        t = XP_STRCHR(coord_string, ')');
        if (t)
            *t = '\0';
    }

	value = XP_STRTOK(coord_string, ", ");

	while(value && index < 4)
	{
		ss_num = STYLESTRUCT_StringToSSNumber(style_struct, value);

		/* First and third args are heights.  Note that we use
           LAYER_WIDTH_STYLE instead of WIDTH_STYLE, since the two are
           subtly different. (For the top-level DOCUMENT layer, 100%
           WIDTH is the distance between the margins and 100%
           LAYER_WIDTH is the width of the window.) */
		if(index & 1)
			LO_AdjustSSUnits(ss_num, LAYER_WIDTH_STYLE, context, state);
		else
			LO_AdjustSSUnits(ss_num, HEIGHT_STYLE, context, state);

		val[index++] = (int32)ss_num->value;

		value = XP_STRTOK(NULL, ",) ");
	}

	if (index == 2)
	{
		coords->right  = FEUNITS_X(val[0], context);
		coords->bottom = FEUNITS_Y(val[1], context);
	}
	else if (index == 4)
	{
		coords->top    = FEUNITS_Y(val[0], context);
		coords->right  = FEUNITS_X(val[1], context);
		coords->bottom = FEUNITS_Y(val[2], context);
		coords->left   = FEUNITS_X(val[3], context);
	}
	else
	{
		XP_FREE(coords);
		return NULL;
	}

	return coords;
}

void
lo_SetStyleSheetLayerProperties(MWContext *context,
                                lo_DocState *state,
                                StyleStruct *style_struct,
                                PA_Tag *tag)
{
	LO_BlockInitializeStruct *param;
	char *prop;
	char *src_prop;
	XP_Bool is_inflow;
	SS_Number *ss_num;

    if(!style_struct)
        return;

	prop = STYLESTRUCT_GetString(style_struct, POSITION_STYLE);
	src_prop = STYLESTRUCT_GetString(style_struct, LAYER_SRC_STYLE);

	if(!prop && !src_prop)
		return;

	if(lo_IsEmptyTag(tag->type))
	{
		/* setting positioning on an empty tag will
		 * cause things to not work because of blocked image
		 * problems, so don't allow it.
		 */
		return;
	}

	if(prop && !strcasecomp(prop, ABSOLUTE_STYLE))
	{
		is_inflow = FALSE;
	}
	else if(prop && !strcasecomp(prop, RELATIVE_STYLE))
	{
		is_inflow = TRUE;
	}
	else if(src_prop)
	{
        /* not positioned, but an include src attribute */
		is_inflow = TRUE;
	}
	else
	{
		XP_FREEIF(prop);
		return;  /* don't do layers */
	}

	XP_FREEIF(prop);

	param = XP_NEW_ZAP(LO_BlockInitializeStruct);

	if(!param)
		return;

	param->name = (char*)lo_FetchParamValue(context, tag, PARAM_NAME);
	param->id = (char*)lo_FetchParamValue(context, tag, PARAM_ID);

	ss_num = STYLESTRUCT_GetNumber(style_struct, LEFT_STYLE);
	if(ss_num)
	{
		LO_AdjustSSUnits(ss_num, LEFT_STYLE, context, state);
		param->left = FEUNITS_X((int32)ss_num->value, context);
		param->has_left = TRUE;
		STYLESTRUCT_FreeSSNumber(style_struct, ss_num);
	}

	ss_num = STYLESTRUCT_GetNumber(style_struct, TOP_STYLE);
	if(ss_num)
	{
		LO_AdjustSSUnits(ss_num, TOP_STYLE, context, state);
		param->top = FEUNITS_Y((int32)ss_num->value, context);
		param->has_top = TRUE;
		STYLESTRUCT_FreeSSNumber(style_struct, ss_num);
	}

	ss_num = STYLESTRUCT_GetNumber(style_struct, HEIGHT_STYLE);
	if(ss_num)
	{
		LO_AdjustSSUnits(ss_num, HEIGHT_STYLE, context, state);
		param->height = FEUNITS_Y((int32)ss_num->value, context);
		param->has_height = TRUE;
		STYLESTRUCT_FreeSSNumber(style_struct, ss_num);
	}

	/* don't set width. normal style sheets will handle it */

	prop = STYLESTRUCT_GetString(style_struct, CLIP_STYLE);

	if(prop) {
		param->clip = lo_ParseStyleCoords(context, state, style_struct, prop);
        param->clip_expansion_policy = LO_AUTO_EXPAND_NONE;
    }

	param->above = NULL;
	param->below = NULL;
	ss_num = STYLESTRUCT_GetNumber(style_struct, ZINDEX_STYLE);
	if (ss_num) {
		param->zindex = (int32)ss_num->value;
		param->has_zindex = TRUE;
		STYLESTRUCT_FreeSSNumber(style_struct, ss_num);
	}
	else
		param->has_zindex = FALSE;

	param->visibility = STYLESTRUCT_GetString(style_struct, VISIBILITY_STYLE);

	param->src = src_prop;
	if(param->src)
	{
		char *tmp = param->src;

		param->src = lo_ParseStyleSheetURL(param->src);
		param->src = NET_MakeAbsoluteURL(state->top_state->base_url, param->src);
		XP_FREE(tmp);
	}

	param->overflow = STYLESTRUCT_GetString(style_struct, OVERFLOW_STYLE);

	param->bgcolor = STYLESTRUCT_GetString(style_struct, LAYER_BG_COLOR_STYLE);
	param->is_style_bgcolor = TRUE;
	param->bgimage = STYLESTRUCT_GetString(style_struct, LAYER_BG_IMAGE_STYLE);
	if(param->bgimage)
	{
		char *tmp = param->bgimage;
		if(!strcasecomp(param->bgimage, "none"))
		{
			param->bgimage = NULL;
		}
		else
		{
			param->bgimage = lo_ParseStyleSheetURL(param->bgimage);
			param->bgimage = NET_MakeAbsoluteURL(state->top_state->base_url, param->bgimage);
		}
		XP_FREE(tmp);
	}

    STYLESTRUCT_SetString(style_struct, STYLE_NEED_TO_POP_LAYER, "1", 0);

	param->tag = NULL;
    param->ss_tag = tag;

	if (lo_BeginLayer(context, state, param, is_inflow))
		lo_FreeBlockInitializeStruct(param);
}

static void
lo_expand_parent_bbox(CL_Layer *layer); /* Forward declaration */


/* Save a copy of the document state, so that we can restore it later. */
static PRBool
lo_SaveDocState(lo_Block *block, lo_DocState *state)
{
    lo_DocState *saved_state;

    block->start_x = state->x;
    block->start_y = state->y;

    saved_state = XP_NEW(lo_DocState);
    if (!saved_state)
        return PR_FALSE;

    block->saved_state = saved_state;
    XP_BCOPY(state, saved_state, sizeof *saved_state);
	if (!block->is_inflow) {
		state->top_state->in_head = TRUE;
		state->top_state->in_body = FALSE;
	}
    block->old_body_attr = state->top_state->body_attr;
    state->top_state->body_attr = 0;
    return PR_TRUE;
}

static void
lo_RestoreDocState(lo_Block *block, lo_DocState *state)
{
    lo_DocState *saved = block->saved_state;
    lo_FontStack *font_stack;
    PA_Tag *subdoc_tags_end, *subdoc_tags;
    LO_TextInfo text_info;
    intn layer_nest_level;
    int32 layer_num_max;

    /*
     * If there was a BODY tag with a TEXT attribute in this layer,
     * then we pop the font that was pushed to enable layer-specific
     * text coloring.
     */
    if (state->top_state->body_attr & BODY_ATTR_TEXT) {
        lo_PopFont(state, P_BODY);
    }
    state->top_state->body_attr = block->old_body_attr;

    lo_SetBaseUrl(state->top_state, block->old_base_url, FALSE);

	if (block->is_inflow) {
        state->line_num = saved->line_num;
        state->float_list = saved->float_list;
        state->line_list = saved->line_list;
        state->end_last_line = saved->end_last_line;

        /* Push the cell representing the inflow layer onto the line
           list, so that it appears like any other (non-layer)
           element. */
        if (state->line_list == NULL) {
            state->line_list = (LO_Element*)block->cell;
        } else {
            LO_Element *eptr = state->line_list;
            while (eptr->lo_any.next != NULL)
                eptr = eptr->lo_any.next;
            eptr->lo_any.next = (LO_Element*)block->cell;
            block->cell->prev = eptr;
        }

        if (state->end_last_line != NULL)
           state->end_last_line->lo_any.next = NULL;
        state->linefeed_state = 0;
        state->text_fg = saved->text_fg;
        state->text_bg = saved->text_bg;
        state->anchor_color = saved->anchor_color;
        state->visited_anchor_color = saved->visited_anchor_color;
        state->active_anchor_color = saved->active_anchor_color;
        return;
    }

	state->top_state->in_head = FALSE;
	state->top_state->in_body = TRUE;

    /* Save a few special state variables that we *don't* restore.
       See lo_InitDocState(). */

    /* XXX - Right now, font info is not reset when entering and
       exiting layers.  Is that right ? */
    font_stack       = state->font_stack;
    text_info        = state->text_info;
    layer_nest_level = state->layer_nest_level;
    layer_num_max    = state->current_layer_num_max;
    subdoc_tags_end  = state->subdoc_tags_end;
    subdoc_tags      = state->subdoc_tags;

    /* First, free up the pieces of the document state that we're
       about to overwrite with their restored values. This gets rid of
       default values or values left on the stack because the document
       author didn't properly close tags inside the layer. */
    state->font_stack = NULL;   /* But, don't free the font stack */
    lo_free_layout_state_data(block->context, state);

    /* Restore the entire document state ... */
    XP_BCOPY(saved, state, sizeof *state);

    /* ...except for these variables which are retained across LAYERs */
    state->font_stack            = font_stack;
    state->text_info             = text_info;
    state->layer_nest_level      = layer_nest_level;
    state->current_layer_num_max = layer_num_max;
    state->subdoc_tags_end       = subdoc_tags_end;
    state->subdoc_tags           = subdoc_tags;

    if (state->end_last_line != NULL)
        state->end_last_line->lo_any.next = NULL;

    XP_FREE(block->saved_state);
    block->saved_state = NULL;
}

static PRBool
lo_SetupDocStateForLayer(lo_DocState *state,
                         lo_LayerDocState *layer_state,
                         int32 width,
                         int32 height,
                         PRBool is_inflow)
{
    lo_Block *block = layer_state->temp_block;
    MWContext *context = block->context;
    lo_DocState *saved_state;

    block->is_inflow = is_inflow;

    /* Save old document state so that we can selectively restore
       parts of it after the layer is closed. */
    if (!lo_SaveDocState(block, state))
        return PR_FALSE;

	state->float_list = NULL;
	state->line_list = NULL;

	if (state->top_state->base_url)
		block->old_base_url = XP_STRDUP(state->top_state->base_url);

    if (is_inflow)
        return PR_TRUE;

	/*
	 * XXX This doesn't seem right. We're saving some of the doc's
	 * layout state and resetting all of it while the block
	 * is laid out. This layout state will be restored once the block
	 * is completed. However, things like the float_list are part of the
	 * incremental layout state as well as a representation of the final
	 * document. So, if we're asked to redraw while the block is being laid
	 * out, we will probably do the wrong thing for floating elements.
	 */

    /* Unspecified height won't change state's height property. */
    if (height == 0)
        height = state->win_height;

    state = lo_InitDocState(state, context,
                            width, height,
                            0, 0, /* margin_width, margin_height */
                            NULL, /* clone_state */
                            layer_state->doc_lists, PR_TRUE);

    if (!state)
        return PR_FALSE;

    /* Though most state variables are reset to their virgin state (in
       lo_InitDocState, above), there are a few state variables
       related to table layout that are preserved when a layer tag is
       opened. */
    saved_state = block->saved_state;
    state->is_a_subdoc = saved_state->is_a_subdoc;
    state->in_relayout = saved_state->in_relayout;
    state->subdoc_tags = saved_state->subdoc_tags;
    state->subdoc_tags_end = saved_state->subdoc_tags_end;

    state->text_fg = saved_state->text_fg;
    state->text_bg = saved_state->text_bg;
    state->anchor_color = saved_state->anchor_color;
    state->visited_anchor_color = saved_state->visited_anchor_color;
    state->active_anchor_color = saved_state->active_anchor_color;

    return PR_TRUE;
}

static char layer_end_tag[] = "</" PT_LAYER ">";
static char layer_suppress_tag[] = "<" PT_LAYER " suppress>";
static char display_none_style[] = PARAM_STYLE "='display:none'>";

#ifdef XP_MAC
PRIVATE
#endif
void
lo_insert_suppress_tags(MWContext *context, lo_TopState *top_state,
                        LO_BlockInitializeStruct *param)
{
    PA_Tag *end_tag;
	uint32 i;

    /*
     * Since we're adding tags to the blocked tags list without going through
     * lo_BlockTag, we need to see if this is a flushed to block transition
     * and, if so, add to the doc_data's ref count. We don't want to do this
     * if we're in the process of flushing blockage - in that case, the count
     *  has already been incremented.
     */
    if (top_state->tags == NULL && top_state->flushing_blockage == FALSE)
	PA_HoldDocData(top_state->doc_data);

    /* If we can here through a LAYER or ILAYER tag */
    if (param->tag) {
        /*
         * Shove in a </LAYER><LAYER suppress> sequence into the
         * tag stream so that all inline content of this layer is
         * suppressed. The sourced content comes in before the
         * first end tag, the inline conent comes in after the
         * <LAYER suppress> tag.
         */
        end_tag = pa_CreateMDLTag(top_state->doc_data,
                                  layer_end_tag,
                                  sizeof layer_end_tag - 1);

        if (end_tag) {
            end_tag->newline_count = LO_IGNORE_TAG_MARKER;
            end_tag->next = pa_CreateMDLTag(top_state->doc_data,
                                            layer_suppress_tag,
                                            sizeof layer_suppress_tag - 1);
            if (end_tag->next) {
                end_tag->next->newline_count = LO_IGNORE_TAG_MARKER;
                if (top_state->tags == NULL)
                    top_state->tags_end = &end_tag->next->next;
                else
                    end_tag->next->next = top_state->tags;
                top_state->tags = end_tag;

                /*
                 * If there are any other input_write_levels on the stack
                 * that correspond to the front of the tag list, move them
                 * to account for the newly inserted tags.
                 */
                for (i = 0; i < top_state->input_write_level; i++)
                    if (top_state->input_write_point[i] == &top_state->tags)
                        top_state->input_write_point[i] = &end_tag->next->next;
            }
            else {
                top_state->out_of_memory = TRUE;
                PA_FreeTag(end_tag);
            }
        }
        else
            top_state->out_of_memory = TRUE;
    }
    /*
     * If we came here through the include-source style and
     * the tag is a container tag
     */
    else if (param->ss_tag && !lo_IsEmptyTag(param->ss_tag->type)) {
        /*
         * Shove in a </Foo><Foo STYLE="display:none"> sequence into
         * the tag stream. The sourced content comes in before the
         * first end tag, the inline conent comes in after the
         * display:none tag and is ignored.
         */
        end_tag = (PA_Tag *)XP_NEW_ZAP(PA_Tag);

        if (end_tag) {
            end_tag->type = param->ss_tag->type;
            end_tag->is_end = TRUE;
            end_tag->newline_count = LO_IGNORE_TAG_MARKER;

            end_tag->next = PA_CloneMDLTag(param->ss_tag);
            if (end_tag->next) {
                PA_Tag *begin_tag = end_tag->next;

                begin_tag->newline_count = LO_IGNORE_TAG_MARKER;
                if (begin_tag->data)
                    PA_FREE(begin_tag->data);
                /* Can't do a strdup because it's parser memory */
                begin_tag->data_len = XP_STRLEN(display_none_style);
                begin_tag->true_len = begin_tag->data_len;
                begin_tag->data = PA_ALLOC(begin_tag->data_len + 1);
                if (begin_tag->data) {
                    char *buff;

                    /* Copy over the new tag data */
                    PA_LOCK(buff, char *, begin_tag->data);
                    XP_BCOPY(display_none_style, buff, begin_tag->data_len);
                    buff[begin_tag->data_len] = '\0';
                    PA_UNLOCK(buff);

                    if (top_state->tags == NULL)
                        top_state->tags_end = &begin_tag->next;
                    else
                        begin_tag->next = top_state->tags;
                    top_state->tags = end_tag;
                }
                else {
                    top_state->out_of_memory = TRUE;
                    PA_FreeTag(begin_tag);
                    PA_FreeTag(end_tag);
                }
            }
            else {
                top_state->out_of_memory = TRUE;
                PA_FreeTag(end_tag);
            }
        }
        else
            top_state->out_of_memory = TRUE;
    }
}


/* Start a <LAYER> or an <ILAYER>. */
#ifdef XP_MAC
PRIVATE
#endif
void
lo_begin_layer_internal(MWContext *context,
						lo_DocState *state,
						LO_BlockInitializeStruct *param,
						XP_Bool is_inflow)
{
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
    int16 win_csid = INTL_GetCSIWinCSID(c);
	lo_Block *block;
    lo_LayerDocState *layer_state, *parent_layer_state;
	char *str, *layer_name;
	int32 block_wrap_width, x_parent_offset, y_parent_offset;
	int32 block_x, block_y;
    lo_TopState *top_state;
	XP_Rect bbox;
    PRBool hidden, inherit_visibility;
	CL_Layer *layer, *parent_layer;
    char *backdrop_image_url = NULL;
    LO_Color rgb, *bg_color = NULL;
    char *url = NULL;

    if (!context->compositor || !param)
        return;

    /*
     * If this is a nested inflow layer, then flush the line list into our parent
     * (without introducing a line break). This ensures that whatever line changes
     * (specifically shifting of cell origin) that occur while laying out this ilayer
     * are reflected in our parent.
     */
    if (lo_InsideInflowLayer(state) && is_inflow)
    {
        lo_FlushLineList(context, state, LO_LINEFEED_BREAK_SOFT, LO_CLEAR_NONE, FALSE);
    }

    top_state = state->top_state;
	block = XP_NEW_ZAP(lo_Block);
	if (block == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}
    block->context = context;
	layer_state = lo_NewLayerState(context);
	if (layer_state == NULL) {
		state->top_state->out_of_memory = TRUE;
		lo_DeleteBlock(block);
		return;
	}
    layer_state->temp_block = block;

    /* Retain MATCH attribute for use in comparison of attribute of
       same name in matching closing layer tag. */
    block->match_code = NULL;
    if (param->tag) {
        char *match =
            (char*)PA_FetchParamValue(param->tag, PARAM_MATCH, win_csid);
        if (match) {
            block->match_code = XP_STRDUP(match);
            PA_FREE(match);
        }
    }

    block->name = NULL;
	block->above = NULL;
	block->below = NULL;
	hidden = PR_FALSE;
    inherit_visibility = PR_TRUE;
	block->z_order = -1;
	block->old_layer_state = NULL;

    /* If there is no tag associated with this layer, then it was
       created as a result of a style sheet. */
    block->uses_ss_positioning = (param->tag == NULL);

	block->is_inflow = is_inflow;

    /* In-flow layers use coordinates that are relative to their
       "natural", in-flow position. */
    if (block->is_inflow) {
        block->x_offset = state->x;
        block->y_offset = state->y;
    } else {
        block->x_offset = 0;
        block->y_offset = 0;
    }


    x_parent_offset = y_parent_offset = 0;
    parent_layer_state = lo_CurrentLayerState(state);
    parent_layer = parent_layer_state->layer;
    if (parent_layer)           /* Paranoia */
        lo_GetLayerXYShift(parent_layer, &x_parent_offset, &y_parent_offset);

	/*
	 * Get the X position of the block
	 */
    if (param->has_left)
    {
        block_x = param->left;
    } else {
        if (block->is_inflow) {
            block_x = 0;
        } else {
            block_x = state->x - x_parent_offset;
        }
	}

	/*
	 * Get the Y position of the block
	 */
    if (param->has_top)
    {
		block_y = param->top;
	} else {
        if (block->is_inflow) {
            block_y = 0;
        } else {
            block_y = state->y - y_parent_offset;
        }
	}

	XP_BZERO(&bbox, sizeof(bbox));

	/*
	 * Get the width parameter, in absolute or percentage.
	 * If percentage, make it absolute.
	 */
	if (param->has_width)
		block_wrap_width = param->width;
    else if (param->has_left)
        block_wrap_width = state->right_margin - param->left;
    else
        block_wrap_width = state->right_margin - state->x;

	/*
	 * Parse the comma separated coordinate list into an
	 * array of integers.
	 */
	if (param->clip)
	{
        bbox = *param->clip;

        /* Don't allow the layer's clip to expand */
        block->clip_expansion_policy = param->clip_expansion_policy;
	} else {
        /* Allow the clip to expand to include the layer's contents. */
        block->clip_expansion_policy = LO_AUTO_EXPAND_CLIP;
        if (param->has_width)
            bbox.right = block_wrap_width;
    }

    if (param->has_height) {
        layer_state->height = param->height;
        /* If no CLIP set, set initial bottom edge of layer clip to be
           the same as HEIGHT. */
        if (block->clip_expansion_policy & LO_AUTO_EXPAND_CLIP_BOTTOM)
            bbox.bottom = param->height;
    }

    /* Treat any value of OVERFLOW, except "hidden" to be the same as
       "visible".  For compatibility with the older CSS positioning
       spec, we also treat "none" specially.  Can't set OVERFLOW
       unless HEIGHT is provided. */
    if (param->has_height &&
        param->overflow &&
        XP_STRCASECMP(param->overflow, "none") &&
        XP_STRCASECMP(param->overflow, "visible")) {

        /* Constrain all drawing to {0, 0, width, height} box */
        XP_Rect *viewRect = &layer_state->viewRect;
        viewRect->left = 0;
        viewRect->top = 0;
        viewRect->right = block_wrap_width;
        viewRect->bottom = param->height;
    }

	/*
	 * Get the optional name for this layer.
	 */
    layer_name = NULL;
	if(param->name)
        layer_name = param->name;
	else if(param->id)
		layer_name = param->id;

    /* Don't allow layer names that start with a number or underscore */
    if (layer_name && ((layer_name[0] < '0') || (layer_name[0] > '9')) &&
        (layer_name[0] != '_'))
        block->name = XP_STRDUP(layer_name);

	/*
	 * Get the optional "above" name for the layer we are above.
	 */
	if(param->above)
		block->above = XP_STRDUP(param->above);

	/*
	 * Get the optional "below" name for the layer we are below.
	 */
	if (!block->above)
	{
		if(param->below)
	    	block->below = XP_STRDUP(param->below);

	    if (!block->below)
	    {
            /*
             * Get the Z-order of the block
             */
            if (param->has_zindex)
            {
                block->z_order = param->zindex;
            }
	    }
	}

	/* Get the VISIBILITY parameter to know if this layer starts hidden. */
	if (param->visibility)
	{
        /* Handle "HIDE", "HIDDEN", etc. */
		hidden = (PRBool)!XP_STRNCASECMP(param->visibility, "hid", 3);
        inherit_visibility = (PRBool)!XP_STRCASECMP(param->visibility, "inherit");
	}

	if (is_inflow)
	{
		/* Add a new LO_LAYER dummy layout element to the line list.  Relayout will step through
		   the line list and reflow the layer when the LO_LAYER element is encountered. */
		if (!lo_AppendLayerElement(context, state, FALSE)) 
			return;
	}
    
	/* Reset document layout state, saving old state to be restored
       after we lay out this layer. */
    if (!lo_SetupDocStateForLayer(state, layer_state,
                                  block_wrap_width, layer_state->height,
                                  (PRBool)is_inflow)) {
		state->top_state->out_of_memory = TRUE;
		lo_DeleteLayerState(context, state, layer_state);
		return;
	}

	block->cell = layer_state->cell =
		(LO_CellStruct *)lo_NewElement(context, state,
									   LO_CELL, NULL, 0);
	if (block->cell == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		lo_DeleteLayerState(context, state, layer_state);
		return;
	}
	lo_init_block_cell(context, state, block);

    /* Create the layer that corresponds to this block, initially invisible
       and with empty clip and [0,0] origin. */
    PA_LOCK(str, char *, block->name);
    layer = lo_CreateBlockLayer(context,
                                str,
                                (PRBool)block->is_inflow,
                                block->x_offset,
                                block->y_offset,
                                block_wrap_width,
                                layer_state, state);
    PA_UNLOCK(block->name);
    if (!layer) {
		state->top_state->out_of_memory = TRUE;
		lo_DeleteLayerState(context, state, layer_state);
        return;
    }
    block->layer = layer_state->layer = layer;
    if (block->is_inflow)
        block->cell->cell_inflow_layer = layer;

    /* Indicate that this layer's contents were not obtained as a
       result of setting the JavaScript 'src' property of the layer. */
    block->is_dynamic = FALSE;
    block->is_inline = TRUE;

	/*
	 * Attach the layer doc_state to the layer list. The return
	 * value is the old layer_state if we're in table relayout.
	 * We need to wait till we're done processing this layer tag
	 * before we actually get rid of the layer_state and its
	 * contents.
	 */
	block->old_layer_state = lo_append_to_layer_array(context, state->top_state,
                                                      state,
                                                      layer_state);

    /* Attach the layer into the layer tree */
    lo_AttachHTMLLayer(context, layer, parent_layer,
                       block->above, block->below, block->z_order);

    /* Reflect the layer into JavaScript */
    ET_ReflectObject(context, NULL, param->tag,
                     parent_layer_state->id,
                     layer_state->id, LM_LAYERS);

    /* Process background color (BGCOLOR) attribute, if present. */
    if (param->bgcolor) {

		XP_Bool rv;
		if(param->is_style_bgcolor)
        	rv = LO_ParseStyleSheetRGB(param->bgcolor, &rgb.red, &rgb.green, &rgb.blue);
		else
        	rv = LO_ParseRGB(param->bgcolor, &rgb.red, &rgb.green, &rgb.blue);
		if(rv)
        	bg_color = &rgb;
    }
    if (bg_color)
        LO_SetLayerBgColor(layer, bg_color);

    /* Process backdrop (BACKGROUND) image attribute, if present. */
	if(param->bgimage)
    	backdrop_image_url = XP_STRDUP(param->bgimage);

    if (!bg_color && !backdrop_image_url) {
        /* This is a transparent block, so let's go offscreen */
        CL_ChangeLayerFlag(layer, CL_PREFER_DRAW_OFFSCREEN, PR_TRUE);
    }
    if (backdrop_image_url) {
        LO_SetLayerBackdropURL(layer, backdrop_image_url);
        XP_FREE(backdrop_image_url);
    }

    lo_SetLayerClipExpansionPolicy(layer, block->clip_expansion_policy);

    /* Move layer into position.  Set clip dimensions and initial visibility. */
    LO_MoveLayer(layer, block_x, block_y);
    LO_SetLayerBbox(layer, &bbox);
    lo_expand_parent_bbox(layer);
    CL_ChangeLayerFlag(layer, CL_HIDDEN, hidden);
#ifdef XP_MAC
    CL_ChangeLayerFlag(layer, CL_OVERRIDE_INHERIT_VISIBILITY,
                       (PRBool)!inherit_visibility);
#else
    CL_ChangeLayerFlag(layer, CL_OVERRIDE_INHERIT_VISIBILITY,
                       (int32)!inherit_visibility);
#endif

    /* Push this layer on the layer stack */
    lo_PushLayerState(state->top_state, layer_state);
#ifdef MOCHA
	ET_SetActiveLayer(context, layer_state->id);
#endif /* MOCHA */
	state->layer_nest_level++;

	if (param->src) {
        url = NET_MakeAbsoluteURL(top_state->base_url, param->src);
        if (url == NULL) {
            top_state->out_of_memory = TRUE;
            return;
        }
    }

    if ((url != NULL) && (top_state->doc_data != NULL)) {
        URL_Struct *url_struct;
        int status;

		block->source_url = XP_STRDUP(url);
        block->is_inline = FALSE;
        url_struct = NET_CreateURLStruct(url, top_state->force_reload);
		if (block->source_url == NULL || url_struct == NULL) {
            top_state->out_of_memory = TRUE;
			if (url_struct != NULL)
				NET_FreeURLStruct(url_struct);
		}
        else {
			char *referer;
			lo_LayerStack *lptr;

			/*
			 * The referer for this SRC="url" fetch will be the base document
			 * url if there are no enclosing out-of-line or dynamic layer tags,
			 * otherwise it will be the nearest enclosing layer block's source
			 * url (set via a previous SRC= or by LO_PrepareLayerForWriting).
			 */
			referer = top_state->base_url;
			lptr = top_state->layer_stack->next;
			while (lptr && lptr->layer_state &&
				   (lptr->layer_state->id != LO_DOCUMENT_LAYER_ID)) {
				lo_Block *temp_block;

				temp_block = lptr->layer_state->temp_block;
				if (temp_block &&
					(!temp_block->is_inline || temp_block->is_dynamic)) {
					referer = temp_block->source_url;
					break;
				}
				lptr = lptr->next;
			}

			url_struct->referer = XP_STRDUP(referer);
			if (url_struct->referer == NULL) {
				top_state->out_of_memory = TRUE;
				NET_FreeURLStruct(url_struct);
				return;
			}

            /*
             * We're blocking on the current element till we
             * read in the new stream.
             */
            top_state->layout_blocking_element = (LO_Element *)block->cell;

            lo_insert_suppress_tags(context, top_state, param);

			/*
			 * New tags will be inserted at the start of the
			 * blocked tags list.
			 */
             top_state->input_write_point[top_state->input_write_level] =
                 &top_state->tags;

            lo_SetBaseUrl(state->top_state, url, FALSE);
            XP_FREEIF(top_state->inline_stream_blocked_base_url);

			status = NET_GetURL(url_struct, FO_CACHE_AND_PRESENT_INLINE, context,
                                lo_block_src_exit_fn);

			if (status < 0)
				top_state->layout_blocking_element = NULL;

        }
        XP_FREE(url);
    }
}

static Bool
lo_create_layer_blockage(MWContext *context, lo_DocState *state)
{
    lo_TopState *top_state;
    top_state = state->top_state;

    top_state->layout_blocking_element
        = lo_NewElement(context, state, LO_CELL, NULL, 0);
    if (top_state->layout_blocking_element == NULL) {
        top_state->out_of_memory = TRUE;
    } else {
        top_state->layout_blocking_element->type = LO_CELL;
        top_state->layout_blocking_element->lo_any.ele_id = NEXT_ELEMENT;
		top_state->layout_blocking_element->lo_cell.cell_list = NULL;
		top_state->layout_blocking_element->lo_cell.cell_list_end = NULL;
		top_state->layout_blocking_element->lo_cell.cell_float_list = NULL;
		top_state->layout_blocking_element->lo_cell.cell_bg_layer = NULL;
		top_state->layout_blocking_element->lo_cell.backdrop.bg_color = NULL;
	}

    return(top_state->layout_blocking_element != NULL);
}

typedef struct
{
	MWContext *context;
	lo_DocState *state;
	XP_Bool is_inflow;
	int32 doc_id;
} lo_RestoreLayerClosure;

static void
lo_RestoreLayerExitFn(void *myclosure,
					  LO_BlockInitializeStruct *param)
{
	lo_RestoreLayerClosure *closure = (lo_RestoreLayerClosure *)myclosure;
	MWContext *context = closure->context;
	lo_DocState *state = closure->state;
	lo_TopState *top_state;
	int32 doc_id = closure->doc_id;
	PRBool flushp = (PRBool)(param->src == NULL);
	LO_Element *blocking_element;

	if (doc_id == XP_DOCID(context)) {
		top_state = state->top_state;
		/*
		 * We check that we're still blocking, since it's possible that
		 * something's come along and changed things under us e.g. if
		 * the context is interrupted while we were out to mocha.
		 */
		blocking_element = top_state->layout_blocking_element;
		if (blocking_element)
			lo_begin_layer_internal(context, state, param, closure->is_inflow);
	}

	XP_FREE(closure);

	/* Get rid of the cloned tag */
	if(param->tag) {
        PA_FreeTag(param->tag);
	}
    else if (param->ss_tag) {
        PA_FreeTag(param->ss_tag);
    }

	lo_FreeBlockInitializeStruct(param);

	/* Get rid of the fake layout blocking element */
	if ((doc_id == XP_DOCID(context)) &&
		(blocking_element)) {
		lo_FreeElement(context, blocking_element, FALSE);

		/*
		 * If there was a SRC attribute in the param list, then
		 * the block layout will have created a new blocking element.
		 * Otherwise, flush the blocked list and carry on as if
		 * nothing happened.
		 */
		if (flushp) {
			top_state->layout_blocking_element = NULL;
			lo_FlushBlockage(context, state, state->top_state->doc_state);
		}
	}
}

XP_Bool
lo_BeginLayer(MWContext *context,
			  lo_DocState *state,
			  LO_BlockInitializeStruct *param,
			  XP_Bool is_inflow)
{
    if (!context->compositor || !param)
        return TRUE;

	if (state->top_state->resize_reload && !state->in_relayout &&
        !lo_IsAnyCurrentAncestorDynamic(state) && LM_CanDoJS(context)) {
		lo_RestoreLayerClosure *closure;
		PA_Tag *new_tag;

		/* I guess this whole section can go away, now that we have resize
		   without reload a la Mariner??? */

		closure = XP_NEW_ZAP(lo_RestoreLayerClosure);
		if (!closure)
			return TRUE;

		closure->context = context;
		closure->state = state;
		closure->is_inflow = is_inflow;
		closure->doc_id = XP_DOCID(context);

		/*
		 * Block layout and send an event mocha to restore the layer
		 * state from the preserved mocha object.
		 */
		lo_create_layer_blockage(context, state);
		lo_BlockLayerTag(context, state, NULL);

		if (param->tag) {
			new_tag = PA_CloneMDLTag(param->tag);
			if (!new_tag) {
				state->top_state->out_of_memory = TRUE;
				return TRUE;
			}
			param->tag = new_tag;
		}
        else if (param->ss_tag) {
			new_tag = PA_CloneMDLTag(param->ss_tag);
			if (!new_tag) {
				state->top_state->out_of_memory = TRUE;
				return TRUE;
			}
			param->ss_tag = new_tag;
        }

		ET_RestoreLayerState(context, state->top_state->current_layer_num + 1,
							 param, lo_RestoreLayerExitFn, closure);
		return FALSE;
	}
	else {
		lo_begin_layer_internal(context, state, param, is_inflow);
		return TRUE;
	}
}

void
lo_BlockLayerTag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
    lo_TopState *top_state;
    pa_DocData *doc_data;

    top_state = state->top_state;
    doc_data = top_state->doc_data;
    XP_ASSERT(doc_data != NULL && doc_data->url_struct != NULL);
    if (doc_data == NULL || doc_data->url_struct == NULL)   /* Paranoia */
        return;
}

void
lo_UnblockLayerTag(lo_DocState *state)
{
	/*
	 * I guess we don't really need this anymore, since we're not
	 * maintaining a blocked_tag count.
	 */
}

static void
lo_set_layer_bbox(CL_Layer *layer, XP_Rect *bbox, PRBool grow_parent);

/* A layer has expanded in order to contain its content.  Expand it's
   parent also.*/
static void
lo_expand_parent_bbox(CL_Layer *layer)
{
    XP_Rect parent_bbox, child_bbox;
    CL_Compositor *compositor = CL_GetLayerCompositor(layer);

    /* Get the parent's and child's bbox */
    CL_Layer *parent_layer = CL_GetLayerParent(layer);

    if (! parent_layer)
      return;

    CL_GetLayerBbox(parent_layer, &parent_bbox);
    CL_GetLayerBbox(layer, &child_bbox);

    /* Convert the child and parent to the same coordinate system. */
    CL_LayerToWindowRect(compositor, parent_layer, &parent_bbox);
    CL_LayerToWindowRect(compositor, layer, &child_bbox);

    /* Expand the parent's bbox to encompass the child layer. */
    XP_RectsBbox(&child_bbox, &parent_bbox, &parent_bbox);
    CL_WindowToLayerRect(compositor, parent_layer, &parent_bbox);
    lo_set_layer_bbox(parent_layer, &parent_bbox, PR_TRUE);

    lo_expand_parent_bbox(parent_layer);
}

/* Set the clipping bounds of a layer, but don't override any
   explicit clip set by the user with the CLIP attribute. */
static void
lo_set_layer_bbox(CL_Layer *layer, XP_Rect *bbox, PRBool grow_parent)
{
    XP_Rect new_bbox, old_bbox;

    int clip_expansion_policy = lo_GetLayerClipExpansionPolicy(layer);

    CL_GetLayerBbox(layer, &old_bbox);
    XP_CopyRect(&old_bbox, &new_bbox);

    if (clip_expansion_policy & LO_AUTO_EXPAND_CLIP_LEFT)
        new_bbox.left = bbox->left;

    if (clip_expansion_policy & LO_AUTO_EXPAND_CLIP_RIGHT)
        new_bbox.right = bbox->right;

    if (clip_expansion_policy & LO_AUTO_EXPAND_CLIP_TOP)
        new_bbox.top = bbox->top;

    if (clip_expansion_policy & LO_AUTO_EXPAND_CLIP_BOTTOM)
        new_bbox.bottom = bbox->bottom;

    /* If the layer's clip didn't change, there's nothing to do */
    if (XP_EqualRect(&old_bbox, &new_bbox))
        return;

    LO_SetLayerBbox(layer, &new_bbox);

    /* Expand the parent layer's bbox to contain this child's bbox */
    if (grow_parent)
        lo_expand_parent_bbox(layer);
}

/* Expand the layer associated with an HTML block in case the block
   has grown.  Don't expand the layer's width if the width is fixed
   and don't expand the layer's height if the height is fixed.  */
static void
lo_block_grown(lo_Block *block)
{
    CL_Layer *layer = block->layer;
    lo_LayerDocState *layer_state;
    LO_CellStruct *cell = block->cell;

    XP_Rect clip, existing_clip;

    clip.left = cell->x;
    clip.top  = cell->y;
    clip.right = cell->x + cell->width;
    clip.bottom = cell->y + cell->height;

    layer_state = lo_GetLayerState(layer);
    layer_state->contentWidth = cell->width;
    layer_state->contentHeight = cell->height;

    /* The coordinates of elements inside in-flow layers is with respect
       to the containing layer. */
    if (block->is_inflow)
        XP_OffsetRect(&clip, -block->start_x, -block->start_y);

    /* Don't allow a layer's clip to shrink. */
    CL_GetLayerBbox(layer, &existing_clip);
    XP_RectsBbox(&existing_clip, &clip, &clip);

    lo_set_layer_bbox(layer, &clip, (PRBool)!block->is_dynamic);
}

static LO_CellStruct *
lo_compose_block(MWContext *context, lo_DocState *state, lo_Block *block)
{
	int32 shift;
    XP_Rect update_rect, ele_bbox;
	LO_Element *tptr;
	LO_CellStruct *cell;

	cell = block->cell;

    XP_BZERO(&update_rect, sizeof(XP_Rect));

    /* Expands the dimensions of the cell to encompass any floating
     * elements that are outside its bounds.
     */
	tptr = state->float_list;
    while (tptr != NULL) {
        lo_GetElementBbox(tptr, &ele_bbox);

        shift = cell->x - ele_bbox.left;
        if (shift > 0) {
            cell->x -= shift;
            cell->width += shift;
        }
        shift = ele_bbox.right - (cell->x + cell->width);
        if (shift > 0)
            cell->width += shift;
        shift = cell->y - ele_bbox.top;
        if (shift > 0) {
            cell->y -= shift;
            cell->height += shift;
        }
        shift = ele_bbox.bottom - (cell->y + cell->height);
        if (shift > 0)
            cell->height += shift;

        XP_RectsBbox(&ele_bbox, &update_rect, &update_rect);

        tptr = tptr->lo_any.next;
    }
	tptr = state->float_list;
	cell->cell_float_list = tptr;

    if (context->compositor) {
        /* Expand the block layer. */
        lo_block_grown(block);

        if (block->is_inflow)
            XP_OffsetRect(&update_rect, -block->start_x, -block->start_y);

        /* Update the area occupied by the floating elements. */
        CL_UpdateLayerRect(context->compositor, block->layer, &update_rect,
                           PR_FALSE);
    }

	return(cell);
}

/* Process a </LAYER> or </ILAYER> tag. */
void
lo_EndLayerTag(MWContext *context,
               lo_DocState *state,
               PA_Tag *tag)
{
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
    int16 win_csid = INTL_GetCSIWinCSID(c);
    lo_LayerDocState *layer_state = lo_CurrentLayerState(state);
    lo_Block *block = layer_state->temp_block;

    if (! block)
        return;

    /* If opening layer tag had a MATCH attribute.  The value of the
       MATCH attribute in the closing tag must be identical or we
       refuse to really close the layer.  This capability is used by
       libmime to prevent renegade mail messages from escaping the
       bounds of their layer-enforced encapsulation using extra layer
       end tags in the message. */
    if (block->match_code) {
        char *attempted_match =
            (char*)PA_FetchParamValue(tag, PARAM_MATCH, win_csid);
        if (!attempted_match)
            return;
        if (XP_STRCMP(attempted_match, block->match_code)) {
            XP_TRACE(("Failed attempt to circumvent mail message security"));
            XP_ASSERT(0);
            PA_FREE(attempted_match);
            return;
        }
        PA_FREE(attempted_match);
    }

    lo_EndLayer(context, state, PR_TRUE);
}

static PRBool
lo_destroy_ancillary_child_layers(CL_Layer *layer, void *unused)
{
	LO_LayerType type;
	type = LO_GetLayerType(layer);
    if (type == LO_GROUP_LAYER)
        return PR_TRUE;
	CL_DestroyLayer(layer);
    return PR_TRUE;
}

void
lo_EndLayer(MWContext *context,
            lo_DocState *state,
            PRBool send_load_event)
{
    int32 right_edge;
	LO_CellStruct *cell;
    lo_LayerDocState *enclosing_layer_state;
    lo_LayerDocState *layer_state = lo_CurrentLayerState(state);
    lo_Block *block = layer_state->temp_block;

    /* During destruction, the compositor can disappear out from underneath us. */
    if (!context->compositor)
        return;

    /*
     * Protect from bad params.
     */
    if (! block) {
        return;
    }

    if (! block->is_inflow) {
        lo_CloseOutLayout(context, state);
    }
    else {
        /*
         * Flush out the last line of the block, without
         * introducing a line break.
         * BUGBUG In reality, calling lo_FlushLineList
         * also introduces a linefeed. We've set it up
         * so that linefeeds within blocks are of 0
         * size, so the linefeed is effectively ignored.
         * The 0 linefeed stuff is a hack!
         */
        lo_FlushLineList(context, state, LO_LINEFEED_BREAK_SOFT, LO_CLEAR_NONE, FALSE);
    }

    cell = lo_compose_block(context, state, block);

    /* For an in-flow layer, expand the enclosing cell's size on the
       right and bottom edges to contain any descendant
       layers. However, we only do this for ILAYER tags, not for
       layers created as a result of 'position:relative' applied to
       style-sheet elements. */
    if (block->is_inflow && !block->uses_ss_positioning) {
        int32 grow_width, grow_height;
        XP_Rect bbox;

        CL_GetLayerBbox(block->layer, &bbox);
        grow_width = (bbox.right - cell->width);
        grow_height = (bbox.bottom - cell->height);

        /* Position of right-edge of cell containing layer */
        right_edge = state->x;

        /* If the inflow layer is caused to grow vertically, force a
           line break. */
        if (grow_height > 0) {
            lo_SetSoftLineBreakState(context, state, FALSE, 1);
            cell->height = bbox.bottom;
            state->y += grow_height;
        }

        /* Compute new position of cell's right edge. */
        if (grow_width > 0) {
            right_edge += grow_width;
            cell->width = bbox.right;
        }

        /* Set minimum and maximum possible layout widths so that any
           enclosing table cells will be dimensioned properly. */
        if (right_edge + state->win_right > state->max_width)
            state->max_width = right_edge + state->win_right;
        if (right_edge + state->win_right > state->min_width)
            state->min_width = right_edge + state->win_right;
    }

    /* Restore document layout state, if necessary */
    lo_RestoreDocState(block, state);

	if (block->is_inflow)
	{
		/* Mark the </LAYER> tag by appending a LO_LAYER element to the line list. */
		if (!lo_AppendLayerElement(context, state, TRUE)) 
			return;
	}

	if (cell != NULL  && !block->is_inflow)
	{
		int32 max_y;

		cell->next = NULL;
		cell->ele_id = NEXT_ELEMENT;
		lo_RenumberCell(state, cell);

        /* XXX - Do we really want to expand the document size to
           include the layer ? */
		max_y = cell->y + cell->y_offset + cell->height;
		if (max_y > state->max_height)
		{
			state->max_height = max_y;
		}
    }

	/*
	 * If we're in table relayout, we can now safely delete the
	 * layer from a previous layout pass.
	 */
	if (state->in_relayout && block->old_layer_state) {
		/* Get rid of child image layers, cell background layers, embedded window layers and blink layers */
		CL_ForEachChildOfLayer(block->old_layer_state->layer,
						   lo_destroy_ancillary_child_layers,
						   (void *)block->old_layer_state->layer);
		CL_DestroyLayer(block->old_layer_state->layer);
	}

	state->layer_nest_level--;
    lo_PopLayerState(state);
#ifdef MOCHA
    enclosing_layer_state = lo_CurrentLayerState(state);
    ET_SetActiveLayer(context, enclosing_layer_state->id);
#endif /* MOCHA */

	if (block->is_dynamic) {
	    lo_TopState * top_state;

	    top_state = state->top_state;
	    top_state->nurl = NULL;
	    top_state->layout_status = PA_COMPLETE;

            lo_CloseMochaWriteStream(top_state, EVENT_LOAD);

	    lo_FreeLayoutData(context, state);

		/*
		 * We force a composite so that the layer is drawn, even
		 * if it is immediately changed again (as is sometimes the
		 * case when we're dynamically resizing layers.
		 */
		if (context->compositor) {
			XP_Rect bbox;

			CL_GetLayerBbox(block->layer, &bbox);
			CL_UpdateLayerRect(context->compositor, block->layer,
							   &bbox, PR_TRUE);
		}
	}

#ifdef MOCHA
    if (send_load_event)
        ET_SendLoadEvent(context, EVENT_LOAD, NULL, NULL, layer_state->id,
                         state->top_state->resize_reload);
#endif

	lo_DeleteBlock(block);
    layer_state->temp_block = NULL;
}

void
lo_AddLineListToLayer(MWContext *context,
                      lo_DocState *state,
                      LO_Element *line_list_end)
{
	int32 max_y, max_x, min_x, height, width;
	LO_Element *line_list;
	LO_CellStruct *cell;
    lo_LayerDocState *layer_state = lo_CurrentLayerState(state);
    lo_Block *block = layer_state->temp_block;

	line_list = state->line_list;
	if (line_list == NULL)
		return;

	cell = block->cell;

	/*
	 * For dynamic src changing, we just refresh the area occupied by
	 * the old layer. For the autoexpanding case, we reset the size
	 * of the layer to zero.
	 */
	if ((cell->cell_list == NULL) && (cell->cell_float_list == NULL)) {

            /* If no page background color is specified, we must
               commit to one as soon as any layout element is
               displayed in order to avoid the excessive flashing that
               would occur had we created the background later. */
            if (state->top_state->nothing_displayed != FALSE) {
                lo_use_default_doc_background(context, state);
                state->top_state->nothing_displayed = FALSE;
            }

            if (block->is_dynamic) {
		XP_Rect bbox;

		CL_GetLayerBbox(block->layer, &bbox);
		CL_UpdateLayerRect(context->compositor, block->layer, &bbox, PR_FALSE);

		if (block->clip_expansion_policy == LO_AUTO_EXPAND_CLIP)
		{
			XP_Rect empty_bbox = {0, 0, 0, 0};

			CL_SetLayerBbox(block->layer, &empty_bbox);
		}
            }
        }

	if (cell->cell_list_end == NULL)
	{
		line_list->lo_any.prev = NULL;
		cell->cell_list = line_list;
		line_list_end->lo_any.next = NULL;
		cell->cell_list_end = line_list_end;
	}
	else
	{
		cell->cell_list_end->lo_any.next = line_list;
		line_list->lo_any.prev = cell->cell_list_end;
		line_list_end->lo_any.next = NULL;
		cell->cell_list_end = line_list_end;
	}

	max_y = line_list_end->lo_any.y + line_list_end->lo_any.y_offset +
			line_list_end->lo_any.height;

	if (block->is_inflow)
		height = max_y - cell->y;
	else
		height = max_y;
    if (height > cell->height)
        cell->height = height;

    /*
     * If the new line extends past the previous bounds of the
     * cell, extend the bounds of the cell.
     */
    max_x = line_list_end->lo_any.x + line_list_end->lo_any.x_offset +
        line_list_end->lo_any.width;

	if (block->is_inflow)
		width = max_x - cell->x;
	else
		width = max_x;
    if (width > cell->width)
        cell->width = width;

    /*
     * This deals with the case where the new line laid out
     * has a start position to the left of the start of the
     * block. This can happen if the layer (with no absolute
     * positioning starts to the right of an element that
     * been aligned left and then its contents flow to the
     * left of the start position below the aligned element i.e.
     *
     *     *******
     *     ******* -----
     *     ******* -----
     *     ******* -----
     *     -------------
     *     -------------
     *
     * where * represents the element aligned left (an image
     * for instance) and - represents the contents of the layer.
     * The layer is shifted left (to the start position of
     * the new line and expanded) i.e.
     *
     *     *******
     *     ####### -----
     *     ####### -----
     *     ####### -----
     *     -------------
     *     -------------
     *
     * where # represents the (transparent) part of the layer
     * overlapping the aligned element.
     */
    min_x = line_list->lo_any.x + line_list->lo_any.x_offset;
    if (min_x < cell->x) {
        cell->width += cell->x - min_x;
        cell->x = min_x;
    }

	if (context->compositor && (block->layer != NULL)) {
        int32 min_y;
        XP_Rect update_rect;

        /* Expand the block layer. */
	    lo_block_grown(block);

        /* Update the area of the line we just added. */
        min_y = max_y - state->line_height;
        update_rect.left = min_x;
        update_rect.top = min_y;
        update_rect.right = max_x;
        update_rect.bottom = max_y;

        if (block->is_inflow)
            XP_OffsetRect(&update_rect, -block->start_x, -block->start_y);

        CL_UpdateLayerRect(context->compositor, block->layer, &update_rect,
                           PR_FALSE);
    }
}


void
lo_FinishLayerLayout(MWContext *context, lo_DocState *state, int mocha_event)
{
    lo_TopState *top_state = state->top_state;
	PA_Tag *tag;

	/* End the current block if we're done processing all tags */
	if ((top_state->layout_blocking_element == NULL) &&
		(top_state->tags == NULL))
		lo_EndLayer(context, state, PR_TRUE);
	/* We're still blocked on something, so
	 * create a fake </LAYER> tag and add it to the end of the
	 * blocked list. Processing this tag will eventually close
	 * out layout.
	 */
	else
	{
		tag = pa_CreateMDLTag(top_state->doc_data,
							  layer_end_tag,
							  sizeof layer_end_tag - 1);

		if (tag != NULL) {
			/* Put it at the end of the tag list */
			if (top_state->tags == NULL)
				top_state->tags = tag;
			else
				*top_state->tags_end = tag;
			top_state->tags_end = &tag->next;
		}
		else
			top_state->out_of_memory = TRUE;
	}
}

static void
lo_block_src_setter_exit_fn(URL_Struct *url_struct,
							int status,
							MWContext *context)
{
    NET_FreeURLStruct(url_struct);
}

static lo_Block *
lo_init_block(MWContext *context, lo_DocState *state,
			  lo_LayerDocState *layer_state, int32 block_x, int32 block_y,
			  int32 width)
{
	lo_Block *block;

	/* Create a new block and initialize it */
	block = XP_NEW_ZAP(lo_Block);
	if (block == NULL)
		return NULL;
    block->context = context;

	layer_state->temp_block = block;

    if (!lo_SetupDocStateForLayer(state, layer_state,
                                  width, layer_state->height, PR_FALSE)) {
        lo_DeleteBlock(block);
        layer_state->temp_block = NULL;
        return NULL;
    }

	return block;
}

static PRBool
lo_remove_extra_layers(CL_Layer *child, void *closure)
{
	LO_LayerType type;
	CL_Layer *parent = (CL_Layer *)closure;

	type = LO_GetLayerType(child);
    if ((type == LO_HTML_BLOCK_LAYER) || (type == LO_HTML_BACKGROUND_LAYER))
        return PR_TRUE;
    CL_RemoveChild(parent, child);
    LO_LockLayout();
    CL_DestroyLayerTree(child);
    LO_UnlockLayout();
	return PR_TRUE;
}

Bool
LO_PrepareLayerForWriting(MWContext *context, int32 layer_id,
						  const char *referer, int32 width)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	CL_Layer *layer;
	lo_LayerDocState *layer_state;
	lo_Block *block;
	int32 block_x, block_y;

	/* Get the top state of the document */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
	    return FALSE;
	}
	state = top_state->doc_state;

	/*
	 * XXX For now, if we're still loading, we can't
	 * change the src of a layer. Is this the right
	 * thing to check that we're done with the original
	 * HTML stream?
	 */
	if ((top_state->doc_data != NULL) || lo_InsideLayer(state))
	    return FALSE;

	layer_state = lo_GetLayerStateFromId(context, layer_id);
	if (!layer_state)
		return FALSE;
	layer = layer_state->layer;

	/* Delete the contents of the old layer */
	lo_SaveFormElementStateInFormList(context,
									  layer_state->doc_lists->form_list,
									  TRUE);
	if (layer_state->cell)
		lo_RecycleElements(context, state, (LO_Element *)layer_state->cell);
	lo_DeleteDocLists(context, layer_state->doc_lists);

	/*
	 * Get rid of all "extra" layers associated (blink, cell_bg, images, etc.)
	 * associated with this layer and its content.  Get rid of all group
     * layer descendants, too.
	 */
	CL_ForEachChildOfLayer(layer,
						   lo_remove_extra_layers,
						   (void *)layer);
	/*
	 * XXX We're assuming that FinishLayout has already been called and
	 * some of the doc_state has been trashed. Here we reset some of the
	 * state so that new tags can be parsed.
	 */
	state->base_font_size = DEFAULT_BASE_FONT_SIZE;
	state->font_stack = lo_DefaultFont(state, context);
	state->line_buf = PA_ALLOC(LINE_BUF_INC * sizeof(char));
	if (state->line_buf == NULL) {
		state->top_state->out_of_memory = TRUE;
		return FALSE;
	}
	state->line_buf_size = LINE_BUF_INC;
	state->line_buf_len = 0;

	/*
	 * We reset the script_tag_count so that we'll reset the
	 * mocha decoder stream.
	 */
	top_state->script_tag_count = 0;

	block_x = CL_GetLayerXOffset(layer);
	block_y = CL_GetLayerYOffset(layer);

	if (!lo_InitDocLists(context, layer_state->doc_lists)) {
		state->top_state->out_of_memory = FALSE;
		return FALSE;
	}

	block = lo_init_block(context, state, layer_state, block_x, block_y,
						  width);
	if (block)
		block->cell = (LO_CellStruct *)lo_NewElement(context, state,
												  LO_CELL, NULL, 0);

	if ((block == NULL) || (block->cell == NULL))
	{
		state->top_state->out_of_memory = TRUE;
		if (block) {
			lo_DeleteBlock(block);
            layer_state->temp_block = NULL;
        }

		return FALSE;
	}

	layer_state->cell = block->cell;

	lo_init_block_cell(context, state, block);

	block->layer = layer;
	block->is_dynamic = TRUE;
    block->is_inline = TRUE;
	block->source_url = referer ? XP_STRDUP(referer) : NULL;

	state->layer_nest_level++;
    /* Push this layer on the layer stack */
    lo_PushLayerState(state->top_state, layer_state);
#ifdef MOCHA
	ET_SetActiveLayer(context, layer_state->id);
#endif /* MOCHA */

    block->clip_expansion_policy = lo_GetLayerClipExpansionPolicy(layer);

	return TRUE;
}


Bool
LO_SetLayerSrc(MWContext *context, int32 layer_id, char *str,
			   const char *referer, int32 width)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	char *url;
	URL_Struct *url_struct;
	int status;
	lo_LayerDocState *layer_state;

	/* Get the top state of the document */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
	    return FALSE;
	}
	state = top_state->doc_state;

	if (!LO_PrepareLayerForWriting(context, layer_id, referer, width))
		return FALSE;

    layer_state = lo_CurrentLayerState(state);
    if (layer_state && layer_state->temp_block)
        layer_state->temp_block->is_inline = FALSE;

	/* Ask netlib to start loading the new URL */
	url = NET_MakeAbsoluteURL(top_state->base_url, str);
	if (url == NULL) {
	  /*lo_Block *block;*/

		top_state->out_of_memory = TRUE;
/* XXX		block = state->current_block;
		if (block) {
			lo_RecycleElements(context, state, (LO_Element *)block->cell);
			XP_DELETE(block);
		}
		state->current_block = NULL; */
		return FALSE;
	}

	url_struct = NET_CreateURLStruct(url, top_state->force_reload);
	if (url_struct == NULL)
		top_state->out_of_memory = TRUE;
	else {
		url_struct->referer = XP_STRDUP(referer);
		if (!url_struct->referer) {
			top_state->out_of_memory = TRUE;
			NET_FreeURLStruct(url_struct);
			XP_FREE(url);
			return FALSE;
		}

        lo_SetBaseUrl(state->top_state, url, FALSE);
        XP_FREEIF(top_state->inline_stream_blocked_base_url);

		status = NET_GetURL(url_struct, FO_CACHE_AND_PRESENT_INLINE, context,
							lo_block_src_setter_exit_fn);
    }

	XP_FREE(url);
	return TRUE;
}

/* Create an anonymous layer, not tied to the HTML source.
   Returns -1 on error, 0 if unable to create a new layer because the parser
   stream is still active. */
int32
LO_CreateNewLayer(MWContext *context, int32 wrap_width, int32 parent_layer_id)
{
	int32 doc_id, layer_id;
	LO_BlockInitializeStruct *param;
    CL_Layer *parent_layer;
    lo_TopState *top_state;
    lo_DocState *state;
    lo_LayerDocState *layer_state;
    CL_Layer *layer;

	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if (!top_state)
		return 0;

	/*
	 * XXX For now, if we're still loading, we can't
	 * change the src of a layer. Is this the right
	 * thing to check that we're done with the original
	 * HTML stream?
	 */
    state = top_state->doc_state;
	if ((top_state->doc_data != NULL) || lo_InsideLayer(state))
	    return 0;

	param = XP_NEW_ZAP(LO_BlockInitializeStruct);
	if(!param)
        return -1;

    param->has_width = PR_TRUE;
    param->width = wrap_width;
    param->has_left = PR_TRUE;
    param->left = 0;
    param->has_top = PR_TRUE;
    param->top = 0;
    param->visibility = "hide";
    param->clip_expansion_policy = LO_AUTO_EXPAND_CLIP;

    layer_id = top_state->max_layer_num;

    /* Create a fake layer and append it to the document */
    lo_begin_layer_internal(context, state, param, FALSE);
    lo_EndLayer(context, state, PR_FALSE);
    XP_FREE(param);

    /* The only way we know if we succeeded is to see if the
       top_state->layers array has grown. */
    if (layer_id == top_state->max_layer_num)
        return -1;

    /* Reparent the layer, in case the parent wasn't the _DOCUMENT */
    parent_layer = LO_GetLayerFromId(context, parent_layer_id);
    layer_id = top_state->max_layer_num;
    layer_state = lo_GetLayerStateFromId(context, layer_id);
    layer_state->is_constructed_layer = PR_TRUE;
    layer = layer_state->layer;
    CL_RemoveChild(CL_GetLayerParent(layer), layer);
    CL_InsertChild(parent_layer, layer, NULL, CL_ABOVE);

    return layer_id;
}
