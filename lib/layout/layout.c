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
#include "lo_ele.h"
#include "net.h"
#include "glhist.h"
#include "shist.h"
#include "merrors.h"
#include "layout.h"
#include "laylayer.h"
#include "layers.h"

#define IL_CLIENT               /* XXXM12N Defined by Image Library clients */
#include "libimg.h"             /* Image Library public API. */

#include "pa_parse.h"
#include "edt.h"
#include "libmocha.h"
#include "libevent.h"
#include "laystyle.h"
#include "hk_funcs.h"
#include "pics.h"
#include "xp_ncent.h"
#include "prefetch.h"

/* WEBFONTS are defined only in laytags.c and layout.c */
#define WEBFONTS

#ifdef WEBFONTS
#include "nf.h"
#include "Mnffbu.h"
#endif /* WEBFONTS */

#ifdef HTML_CERTIFICATE_SUPPORT
#include "cert.h"
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
 
extern int MK_OUT_OF_MEMORY;


#ifdef PROFILE
#pragma profile on
#endif

#ifdef TEST_16BIT
#define XP_WIN16
#endif /* TEST_16BIT */

#ifdef XP_WIN16
#define SIZE_LIMIT		32000
#endif /* XP_WIN16 */


#define DEF_LINE_HEIGHT		20
#define URL_LIST_INC		20
#define LINE_INC		100


typedef struct lo_StateList_struct {
	int32 doc_id;
	lo_TopState *state;
	struct lo_StateList_struct *next;
} lo_StateList;

static lo_StateList *StateList = NULL;

LO_Color lo_master_colors[] = {
	{LO_DEFAULT_BG_RED, LO_DEFAULT_BG_GREEN, LO_DEFAULT_BG_BLUE},
	{LO_DEFAULT_FG_RED, LO_DEFAULT_FG_GREEN, LO_DEFAULT_FG_BLUE},
	{LO_UNVISITED_ANCHOR_RED, LO_UNVISITED_ANCHOR_GREEN,
	 LO_UNVISITED_ANCHOR_BLUE},
	{LO_VISITED_ANCHOR_RED, LO_VISITED_ANCHOR_GREEN,
	 LO_VISITED_ANCHOR_BLUE},
	{LO_SELECTED_ANCHOR_RED, LO_SELECTED_ANCHOR_GREEN,
	 LO_SELECTED_ANCHOR_BLUE}
};
static char *Master_backdrop_url = NULL;
Bool UserOverride = FALSE;

/********** BEGIN PROTOTYPES **************/

static void lo_process_deferred_image_info(void *closure);
static void lo_DisplayLine(MWContext *context, lo_DocState *state, int32 line_num,
						   int32 x, int32 y, uint32 w, uint32 h);
lo_SavedEmbedListData *	lo_NewDocumentEmbedListData(void);
lo_SavedGridData *		lo_NewDocumentGridData(void);
int32					lo_calc_push_right_for_justify(lo_DocState *state, int32 *remainder);
void					lo_BodyMargins(MWContext *context, lo_DocState *state, PA_Tag *tag);
void					lo_FinishLayout_OutOfMemory(MWContext *context, lo_DocState *state);
#ifdef MEMORY_ARENAS 
void					lo_GetRecycleList(
							MWContext* context, int32 doc_id, pa_DocData* doc_data, 
				 			 LO_Element* *recycle_list, lo_arena* *old_arena);
#else
void					lo_GetRecycleList(
							MWContext* context, int32 doc_id, pa_DocData* doc_data, 
				  			LO_Element* *recycle_list);
#endif /* MEMORY_ARENAS */
intn					lo_ProcessTag_OutOfMemory(MWContext* context, LO_Element* recycle_list, lo_TopState* top_state);

/********** END PROTOTYPES **************/

void
LO_SetDefaultBackdrop(char *url)
{
	if ((url != NULL)&&(*url != '\0'))
	{
		Master_backdrop_url = XP_STRDUP(url);
	}
	else
	{
		Master_backdrop_url = NULL;
	}
}


void
LO_SetUserOverride(Bool override)
{
	UserOverride = override;
}


void
LO_SetDefaultColor(intn type, uint8 red, uint8 green, uint8 blue)
{
	LO_Color *color;

	if (type < sizeof lo_master_colors / sizeof lo_master_colors[0])
	{
		color = &lo_master_colors[type];
		color->red = red;
		color->green = green;
		color->blue = blue;
	}
}

Bool
lo_InitDocLists(MWContext *context, lo_DocLists *doc_lists)
{
	LO_AnchorData **anchor_array;
	int32 i;

    doc_lists->embed_list = NULL;
    doc_lists->embed_list_count = 0;
#ifdef notyet /* SHACK */
    doc_lists->builtin_list = NULL;
    doc_lists->builtin_list_count = 0;
#endif /* SHACK */
    doc_lists->applet_list = NULL;
    doc_lists->applet_list_count = 0;

    doc_lists->name_list = NULL;
    doc_lists->image_list = doc_lists->last_image = NULL;
    doc_lists->image_list_count = 0;
    doc_lists->anchor_count = 0;
	doc_lists->form_list = NULL;
	doc_lists->current_form_num = 0;

	doc_lists->url_list = 
        XP_ALLOC_BLOCK(URL_LIST_INC * sizeof(LO_AnchorData *));
	if (doc_lists->url_list == NULL)
	{
		return(FALSE);
	}
	doc_lists->url_list_size = URL_LIST_INC;
	/* url_list is a Handle of (LO_AnchorData*) so the pointer is a (LO_AnchorData**) */
	XP_LOCK_BLOCK(anchor_array, LO_AnchorData **, 
                  doc_lists->url_list);
	for (i=0; i < URL_LIST_INC; i++)
	{
		anchor_array[i] = NULL;
	}
	XP_UNLOCK_BLOCK(doc_lists->url_list);
	doc_lists->url_list_len = 0;

#ifdef XP_WIN16
	{
		XP_Block *ulist_array;

		doc_lists->ulist_array = XP_ALLOC_BLOCK(sizeof(XP_Block));
		if (doc_lists->ulist_array == NULL)
		{
			XP_FREE_BLOCK(doc_lists->url_list);
			return(FALSE);
		}
		XP_LOCK_BLOCK(ulist_array, XP_Block *, 
                      doc_lists->ulist_array);
		ulist_array[0] = doc_lists->url_list;
		XP_UNLOCK_BLOCK(doc_lists->ulist_array);
		doc_lists->ulist_array_size = 1;
	}
#endif /* XP_WIN16 */

     return TRUE;
}


lo_TopState *
lo_NewTopState(MWContext *context, char *url)
{
	lo_TopState *top_state;
	char *name_target;
	LO_TextAttr **text_attr_hash;
	int32 i;
    lo_LayerDocState *layer_state;
    
	top_state = XP_NEW_ZAP(lo_TopState);
	if (top_state == NULL)
	{
		return(NULL);
	}

#ifdef MEMORY_ARENAS
    if ( EDT_IS_EDITOR(context) ) {
        top_state->current_arena = NULL;
    }
    else
    {
	    lo_InitializeMemoryArena(top_state);
	    if (top_state->first_arena == NULL)
	    {
		    XP_DELETE(top_state);
		    return(NULL);
	    }
    }
#endif /* MEMORY_ARENAS */

	top_state->tags = NULL;
	top_state->tags_end = &top_state->tags;
	top_state->insecure_images = FALSE;
	top_state->out_of_memory = FALSE;
	top_state->force_reload = NET_DONT_RELOAD;
    top_state->script_tag_count = 0;
	top_state->script_lineno = 0;
	top_state->in_script = SCRIPT_TYPE_NOT;
	top_state->default_style_script_type = SCRIPT_TYPE_CSS;
	top_state->resize_reload = FALSE;
	top_state->version = JSVERSION_UNKNOWN;
        top_state->scriptData = NULL;
	top_state->doc_state = NULL;

	if (url == NULL)
	{
		top_state->url = NULL;
	}
	else
	{
		top_state->url = XP_STRDUP(url);
	}
    top_state->base_url = NULL;
    top_state->inline_stream_blocked_base_url = NULL;
    top_state->main_stream_blocked_base_url = NULL;
	top_state->base_target = NULL;
    lo_SetBaseUrl(top_state, url, FALSE);

	/*
	 * This is the special named anchor we are jumping to in this
	 * document, it changes the starting display position.
	 */
	name_target = NET_ParseURL(top_state->url, GET_HASH_PART);
	if ((name_target[0] != '#')||(name_target[1] == '\0'))
	{
		XP_FREE(name_target);
		top_state->name_target = NULL;
	}
	else
	{
		top_state->name_target = name_target;
	}

	top_state->element_id = 0;
	top_state->layout_blocking_element = NULL;
	top_state->current_script = NULL;
	top_state->doc_specified_bg = FALSE;
	top_state->nothing_displayed = TRUE;
	top_state->in_head = TRUE;
	top_state->in_body = FALSE;
	top_state->body_attr = 0;
	top_state->have_title = FALSE;
	top_state->scrolling_doc = FALSE;
	top_state->is_grid = FALSE;
    top_state->ignore_tag_nest_level = 0;
    top_state->ignore_layer_nest_level = 0;
	top_state->in_applet = FALSE;
	top_state->unknown_head_tag = NULL;
	top_state->the_grid = NULL;
	top_state->old_grid = NULL;

	top_state->map_list = NULL;
	top_state->current_map = NULL;

    top_state->layers = NULL;
    top_state->current_layer_num = -1; /* incremented on layer allocation */
    top_state->num_layers_allocated = 0;
    top_state->max_layer_num = 0;
    
    layer_state = lo_NewLayerState(context);
    if (!layer_state) {
        XP_FREE_BLOCK(top_state);
        return NULL;
    }
    if (context->compositor)
        layer_state->layer = CL_GetCompositorRoot(context->compositor);
    lo_append_to_layer_array(context, top_state, NULL, layer_state);
    lo_PushLayerState(top_state, layer_state);

	top_state->in_form = FALSE;
	top_state->savedData.FormList = NULL;
	top_state->savedData.EmbedList = NULL;
	top_state->savedData.Grid = NULL;
	top_state->savedData.OnLoad = NULL;
	top_state->savedData.OnUnload = NULL;
	top_state->savedData.OnFocus = NULL;
	top_state->savedData.OnBlur = NULL;
 	top_state->savedData.OnHelp = NULL;
	top_state->savedData.OnMouseOver = NULL;
	top_state->savedData.OnMouseOut = NULL;
	top_state->savedData.OnDragDrop = NULL;
	top_state->savedData.OnMove = NULL;
	top_state->savedData.OnResize = NULL;
	top_state->embed_count = 0;

	top_state->total_bytes = 0;
	top_state->current_bytes = 0;
	top_state->layout_bytes = 0;
	top_state->script_bytes = 0;
	top_state->layout_percent = 0;

	top_state->text_attr_hash = XP_ALLOC_BLOCK(FONT_HASH_SIZE *
		sizeof(LO_TextAttr *));
	if (top_state->text_attr_hash == NULL)
	{
		XP_DELETE(top_state);
		return(NULL);
	}
	XP_LOCK_BLOCK(text_attr_hash, LO_TextAttr **,top_state->text_attr_hash);
	for (i=0; i<FONT_HASH_SIZE; i++)
	{
		text_attr_hash[i] = NULL;
	}
	XP_UNLOCK_BLOCK(top_state->text_attr_hash);

	top_state->font_face_array = NULL;
	top_state->font_face_array_len = 0;
	top_state->font_face_array_size = 0;

    if (!lo_InitDocLists(context, &top_state->doc_lists))
    {
		XP_FREE_BLOCK(top_state->text_attr_hash);
        XP_FREE_BLOCK(top_state);
        return(NULL);
    }
    /* Document layer state keeps its doc_lists in the top_state */
    if ( layer_state->doc_lists )
    {
		lo_DeleteDocLists(context, layer_state->doc_lists);
		XP_FREE( layer_state->doc_lists );
    }
    layer_state->doc_lists = &top_state->doc_lists;
    
	top_state->recycle_list = NULL;
	top_state->trash = NULL;

	top_state->mocha_write_stream = NULL;
	top_state->mocha_loading_applets_count = 0;
	top_state->mocha_loading_embeds_count = 0;
	top_state->mocha_has_onload = FALSE;
	top_state->mocha_has_onunload = FALSE;

	top_state->doc_data = NULL;
	for (i = 0; i < MAX_INPUT_WRITE_LEVEL; i++)
		top_state->input_write_point[i] = NULL;
	top_state->input_write_level = 0;
    top_state->tag_from_inline_stream = FALSE;

#ifdef HTML_CERTIFICATE_SUPPORT
	top_state->cert_list = NULL;
#endif

	top_state->style_stack = SML_StyleStack_Factory_Create();
	if(!top_state->style_stack)
	{
		XP_FREE_BLOCK(top_state->text_attr_hash);
		XP_FREE_BLOCK(top_state->doc_lists.url_list);
		XP_DELETE(top_state);
		return(NULL);
	}

	STYLESTACK_Init(top_state->style_stack, context);

	top_state->diff_state = FALSE;
	top_state->state_pushes = 0;
	top_state->state_pops = 0;

	top_state->object_stack = NULL;
	top_state->object_cache = NULL;

	top_state->tag_count = 0;

    top_state->flushing_blockage = FALSE;
    top_state->wedged_on_mocha = FALSE;
	top_state->in_cell_relayout = FALSE;  /* Used for resize without reload stuff */
	
	return(top_state);
}


/*************************************
 * Function: lo_PushStateLevel
 *
 * Description: Record that we're going one level further
 *  in the state hierarchy.
 *
 * Params: Window context.
 *************************************/
void
lo_PushStateLevel(MWContext *context)
{
	lo_TopState *top_state;

	top_state = lo_FetchTopState( XP_DOCID(context) );
	top_state->state_pushes++;
	
	/*
	 * If we've pushed back to our original state level, then we
	 * need to set a flag saying that we really have a new
	 * state record at the original level. We know this to
	 * be true as we've just added a new state record to the
	 * end of the list.
	 */
	if ( top_state->state_pushes == top_state->state_pops )
	{
		top_state->diff_state = TRUE;
	}
}


/*************************************
 * Function: lo_PopStateLevel
 *
 * Description: Record that we've popped one level up
 *  in the state hierarchy.
 *
 * Params: Window context.
 *************************************/
void
lo_PopStateLevel(MWContext *context)
{
	lo_TopState *top_state;

	top_state = lo_FetchTopState( XP_DOCID(context) );
	top_state->state_pops++;
}

/*************************************
 * Function: lo_GetCurrentDocLists
 *
 * Description: Get the doc_lists for the
 * "current" document i.e. from the top_state
 * in the normal case, from the layer if a 
 * layer is being laid out.
 *
 * Params: DocState
 *************************************/
lo_DocLists *
lo_GetCurrentDocLists(lo_DocState *state)
{
    lo_LayerDocState *layer_state = lo_CurrentLayerState(state);
    return layer_state->doc_lists;
}

lo_DocLists *
lo_GetDocListsById(lo_DocState *state, int32 id)
{
    lo_LayerDocState *layer_state;

    if (id > state->top_state->max_layer_num)
        return NULL;

    layer_state = state->top_state->layers[id];
    if (layer_state) {
        XP_ASSERT(layer_state->doc_lists);
        return layer_state->doc_lists;
    }
    else
        return NULL;
}

/*************************************
 * Function: lo_NewLayout
 *
 * Description: This function creates and initializes a new
 *	layout state structure to be used for all state information
 *	about this document (or sub-document) during its lifetime.
 *
 * Params: Window context,
 *	the width and height of the
 *	window we are formatting to.
 *
 * Returns: A pointer to a lo_DocState structure.
 *	Returns a NULL on error (such as out of memory);
 *************************************/
lo_DocState *
lo_NewLayout(MWContext *context, int32 width, int32 height,
	int32 margin_width, int32 margin_height, lo_DocState* clone_state )
{
    lo_DocLists *doc_lists;
	lo_TopState *top_state;
	lo_DocState *state;
#ifdef XP_WIN16
	int32 i;
	PA_Block *sblock_array;
#endif /* XP_WIN16 */
	int32 doc_id;

	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if (top_state == NULL)
	{
		return(NULL);
	}

	state = XP_NEW_ZAP(lo_DocState);
	if (state == NULL)
	{
		top_state->out_of_memory = TRUE;
		return(NULL);
	}
	state->top_state = top_state;

	state->subdoc_tags = NULL;
	state->subdoc_tags_end = NULL;

    if (top_state->doc_state)
        doc_lists = lo_GetCurrentDocLists(top_state->doc_state);
    else
        doc_lists = lo_GetCurrentDocLists(state);

    if (!lo_InitDocState(state, context,
                         width, height, margin_width, margin_height,
                         clone_state, doc_lists, PR_FALSE))
    {
		top_state->out_of_memory = TRUE;
        XP_FREE(state);
        return NULL;
    }
    return state;
}

        
lo_DocState *
lo_InitDocState(lo_DocState *state, MWContext *context,
                int32 width, int32 height,
                int32 margin_width, int32 margin_height,
                lo_DocState* clone_state,
                lo_DocLists *doc_lists,
                PRBool is_for_new_layer)
{
    lo_TopState *top_state = state->top_state;
    
	/*
	 * Set colors early so default text is correct.
	 */

	if( clone_state == NULL ){
		state->text_fg.red = lo_master_colors[LO_COLOR_FG].red;
		state->text_fg.green = lo_master_colors[LO_COLOR_FG].green;
		state->text_fg.blue = lo_master_colors[LO_COLOR_FG].blue;

		state->text_bg.red = lo_master_colors[LO_COLOR_BG].red;
		state->text_bg.green = lo_master_colors[LO_COLOR_BG].green;
		state->text_bg.blue = lo_master_colors[LO_COLOR_BG].blue;

		state->anchor_color.red = lo_master_colors[LO_COLOR_LINK].red;
		state->anchor_color.green = lo_master_colors[LO_COLOR_LINK].green;
		state->anchor_color.blue = lo_master_colors[LO_COLOR_LINK].blue;

		state->visited_anchor_color.red = lo_master_colors[LO_COLOR_VLINK].red;
		state->visited_anchor_color.green = lo_master_colors[LO_COLOR_VLINK].green;
		state->visited_anchor_color.blue = lo_master_colors[LO_COLOR_VLINK].blue;

		state->active_anchor_color.red = lo_master_colors[LO_COLOR_ALINK].red;
		state->active_anchor_color.green = lo_master_colors[LO_COLOR_ALINK].green;
		state->active_anchor_color.blue = lo_master_colors[LO_COLOR_ALINK].blue;
	}
	else
	{
		state->text_fg.red = clone_state->text_fg.red;
		state->text_fg.green = clone_state->text_fg.green;
		state->text_fg.blue = clone_state->text_fg.blue;

		state->text_bg.red = clone_state->text_bg.red;
		state->text_bg.green = clone_state->text_bg.green;
		state->text_bg.blue = clone_state->text_bg.blue;

		state->anchor_color.red = clone_state->anchor_color.red;
		state->anchor_color.green = clone_state->anchor_color.green;
		state->anchor_color.blue = clone_state->anchor_color.blue;

		state->visited_anchor_color.red = clone_state->visited_anchor_color.red;
		state->visited_anchor_color.green = clone_state->visited_anchor_color.green;
		state->visited_anchor_color.blue = clone_state->visited_anchor_color.blue;

		state->active_anchor_color.red = clone_state->active_anchor_color.red;
		state->active_anchor_color.green = clone_state->active_anchor_color.green;
		state->active_anchor_color.blue = clone_state->active_anchor_color.blue;
	}

	state->win_top = margin_height;
	state->win_bottom = margin_height;
	state->win_width = width;
	state->win_height = height;

	state->base_x = 0;
	state->base_y = 0;
	state->x = 0;
	state->y = 0;
	state->width = 0;

    /* Layers don't use the line array, and resetting the line_num while laying out
       a layer causes display bugs when displaying the _BODY layer. */
    if (!is_for_new_layer)
        state->line_num = 1;

	state->win_left = margin_width;
	state->win_right = margin_width;

	state->max_width = state->win_left + state->win_right;
	state->max_height = state->win_top + state->win_bottom;

	state->x = state->win_left;
	state->y = state->win_top;

	state->left_margin = state->win_left;
	state->right_margin = state->win_width - state->win_right;
	state->left_margin_stack = NULL;
	state->right_margin_stack = NULL;

	state->break_holder = state->x;
	state->min_width = 0;
	state->text_divert = P_UNKNOWN;
	state->linefeed_state = 2;
	state->delay_align = FALSE;
	state->breakable = TRUE;
	state->allow_amp_escapes = TRUE;
	state->preformatted = PRE_TEXT_NO;
	state->preformat_cols = 0;

	state->in_paragraph = FALSE;

	state->display_blocked = FALSE;
	state->display_blocking_element_id = 0;
	state->display_blocking_element_y = 0;

	state->line_list = NULL;
	state->end_last_line = NULL;
	state->float_list = NULL;

    /* Don't bother creating a line array for a layer. Layers don't use them. */
    if (!is_for_new_layer) {
        LO_Element **line_array;

        state->line_array = XP_ALLOC_BLOCK(LINE_INC * sizeof(LO_Element *));
        if (state->line_array == NULL)
        {
            return(NULL);
        }
        XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
        line_array[0] = NULL;
        XP_UNLOCK_BLOCK(state->line_array);
        state->line_array_size = LINE_INC;

#ifdef XP_WIN16
        {
            XP_Block *larray_array;

            state->larray_array = XP_ALLOC_BLOCK(sizeof(XP_Block));
            if (state->larray_array == NULL)
            {
                XP_FREE_BLOCK(state->line_array);
                XP_DELETE(state);
                return(NULL);
            }
            XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
            larray_array[0] = state->line_array;
            XP_UNLOCK_BLOCK(state->larray_array);
            state->larray_array_size = 1;
        }
#endif /* XP_WIN16 */
    }

    /* XXX - No font information is reset upon entering a layer, nor
       is it restored when the layer ends.  Is this the right thing to do ? */
    if (!is_for_new_layer || !state->font_stack) {
        state->base_font_size = DEFAULT_BASE_FONT_SIZE;
        state->font_stack = lo_DefaultFont(state, context);
        if (state->font_stack == NULL)
        {
            XP_FREE_BLOCK(state->line_array);
#ifdef XP_WIN16
            XP_FREE_BLOCK(state->larray_array);
#endif /* XP_WIN16 */
            return(NULL);
        }
        state->font_stack->text_attr->size = state->base_font_size;

        state->text_info.max_width = 0;
        state->text_info.ascent = 0;
        state->text_info.descent = 0;
        state->text_info.lbearing = 0;
        state->text_info.rbearing = 0;
    }

	state->align_stack = NULL;
	state->line_height_stack = NULL;

	state->list_stack = lo_DefaultList(state);
	if (state->list_stack == NULL)
	{
		XP_FREE_BLOCK(state->line_array);
#ifdef XP_WIN16
		XP_FREE_BLOCK(state->larray_array);
#endif /* XP_WIN16 */
		XP_DELETE(state->font_stack);
		return(NULL);
	}

	/*
	 * To initialize the default line height, we need to
	 * jump through hoops here to get the front end
	 * to tell us the default fonts line height.
	 */
	{
		LO_TextStruct tmp_text;
		unsigned char str[1];

		memset (&tmp_text, 0, sizeof (tmp_text));
		str[0] = ' ';
		tmp_text.text = (PA_Block)str;
		tmp_text.text_len = 1;
		tmp_text.text_attr = state->font_stack->text_attr;
		FE_GetTextInfo(context, &tmp_text, &(state->text_info));

		state->default_line_height = state->text_info.ascent +
			state->text_info.descent;
	}
	if (state->default_line_height <= 0)
	{
		state->default_line_height = FEUNITS_Y(DEF_LINE_HEIGHT,context);
	}

	state->line_buf_size = 0;
	state->line_buf = PA_ALLOC(LINE_BUF_INC * sizeof(char));
	if (state->line_buf == NULL)
	{
		XP_FREE_BLOCK(state->line_array);
#ifdef XP_WIN16
		XP_FREE_BLOCK(state->larray_array);
#endif /* XP_WIN16 */
		XP_DELETE(state->font_stack);
		return(NULL);
	}
	state->line_buf_size = LINE_BUF_INC;
	state->line_buf_len = 0;

	state->baseline = 0;
	state->line_height = 0;
	state->break_pos = -1;
	state->break_width = 0;
	state->last_char_CR = FALSE;
	state->trailing_space = FALSE;
	state->at_begin_line = TRUE;
	state->hide_content = FALSE;

	state->old_break = NULL;
	state->old_break_block = NULL;
	state->old_break_pos = -1;
	state->old_break_width = 0;

#ifdef DOM
	state->current_span = NULL;
	state->in_span = FALSE;
#endif

	state->current_named_anchor = NULL;
	state->current_anchor = NULL;

	state->cur_ele_type = LO_NONE;
	state->current_ele = NULL;
	state->current_java = NULL;

	state->current_table = NULL;
	state->current_grid = NULL;
	state->current_multicol = NULL;

    if (!is_for_new_layer)
        state->layer_nest_level = 0;

	/*
	 * These values are saved into the (sub)doc here
	 * so that if it gets Relayout() later, we know where
	 * to reset the from counts to.
	 */
	if (doc_lists->form_list != NULL)
	{
		lo_FormData *form_list;

		form_list = doc_lists->form_list;
		state->form_ele_cnt = form_list->form_ele_cnt;
		state->form_id = form_list->id;
	}
	else
	{
		state->form_ele_cnt = 0;
		state->form_id = 0;
	}
	state->start_in_form = top_state->in_form;
	if (top_state->savedData.FormList != NULL)
	{
		lo_SavedFormListData *all_form_ele;

		all_form_ele = top_state->savedData.FormList;
		state->form_data_index = all_form_ele->data_index;
	}
	else
	{
		state->form_data_index = 0;
	}
	
	/*
	 * Remember number of embeds we had before beginning this
	 * (sub)doc. This information is used in laytable.c so
	 * it can preserve embed indices across a relayout.
	 */
	state->embed_count_base = top_state->embed_count;

	/*
	 * Ditto for anchors, images, applets and embeds
	 */
	state->url_count_base = doc_lists->url_list_len;
    state->image_list_count_base = doc_lists->image_list_count;
    state->applet_list_count_base = doc_lists->applet_list_count;
    state->embed_list_count_base = doc_lists->embed_list_count;
    if (!is_for_new_layer) {
        state->current_layer_num_base = top_state->current_layer_num;
        state->current_layer_num_max = top_state->current_layer_num;
    }

	state->must_relayout_subdoc = FALSE;
	state->allow_percent_width = TRUE;
	state->allow_percent_height = TRUE;
	state->is_a_subdoc = SUBDOC_NOT;
	state->current_subdoc = 0;
	state->sub_documents = NULL;
	state->sub_state = NULL;

	state->current_cell = NULL;

    if (!is_for_new_layer) {
        state->extending_start = FALSE;
        state->selection_start = NULL;
        state->selection_start_pos = 0;
        state->selection_end = NULL;
        state->selection_end_pos = 0;
        state->selection_new = NULL;
        state->selection_new_pos = 0;
        state->selection_layer = NULL;
    }
    
#ifdef EDITOR
	state->edit_force_offset = FALSE;
	state->edit_current_element = 0;
	state->edit_current_offset = 0;
	state->edit_relayout_display_blocked = FALSE;
#endif
	state->in_relayout = FALSE;
	state->tab_stop = DEF_TAB_WIDTH;
	state->beginning_tag_count = top_state->tag_count;

	state->cur_text_block = NULL;
	state->need_min_width = FALSE;
	
	return(state);
}

void LO_SetBackgroundImage( MWContext *context, char *pUrl )
{
	lo_TopState* top_state;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	top_state = lo_FetchTopState(XP_DOCID(context));
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return;
	}
    if (!top_state->doc_layer)
        return;

	top_state->doc_specified_bg = TRUE;

    LO_SetLayerBackdropURL(top_state->doc_layer, pUrl);
}

static void
lo_load_user_backdrop(MWContext *context, lo_DocState *state)
{
	LO_SetBackgroundImage( context, Master_backdrop_url );
}

void
LO_SetDocBgColor(MWContext *context, LO_Color *rgb)
{
	lo_TopState* top_state;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	top_state = lo_FetchTopState(XP_DOCID(context));
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return;
	}
    
    if (!top_state->doc_layer)
        return;
    
#ifdef XP_MAC
    /* This allows the front end to set a background color for
       this context only. Without this call, the color will
       still be the one from the global table */
    FE_GetDefaultBackgroundColor(context, rgb);
#endif
    if (context->compositor)
        LO_SetLayerBgColor(top_state->doc_layer, rgb);
}

int32
lo_calc_push_right_for_justify(lo_DocState *state, int32 *remainder)
{
	int32 count = -1;  /* start at -1 */
	int32 push_right;
	LO_Element *tptr;
	LO_Element *last;

	tptr = state->line_list;

	push_right = state->right_margin - state->x;

	if(push_right > (state->right_margin - state->left_margin)/4)
	{
		return 0; /* don't do justify if it's too large */
	}

	while (tptr != NULL)
	{
		if(tptr->type == LO_TEXT 
		   && tptr->lo_text.text)
			count++;
		last = tptr;
		tptr = tptr->lo_any.next;
	}

	/* if the last element is a space character then
	 * this must be the last line of a paragraph.
	 * don't justify.
	 */
	if(last->type == LO_TEXT
		&& (*last).lo_text.text
		&& !XP_STRCMP((char*)last->lo_text.text, " "))
		return 0;

	/* the push_right is the number of pixels to add to
	 * each space
	 */

	if(count > 0)
	{
		*remainder = push_right % count;
		return push_right/count;
	}
	else
	{
		return 0;
	}
}

PRIVATE void
lo_add_to_y_for_all_elements_in_line(lo_DocState *state, int32 y_add)
{
	LO_Element *tptr;

	if(!state)
		return;

	tptr = state->line_list;

	while (tptr != NULL)
	{
		tptr->lo_any.y_offset += y_add;

		if (tptr->type == LO_CELL)
		{
			lo_ShiftCell((LO_CellStruct *)tptr, 0, y_add);
		}
		tptr = tptr->lo_any.next;
	}
}

void
lo_use_default_doc_background(MWContext *context, lo_DocState *state)
{
    lo_TopState *top_state = state->top_state;
    if (top_state->doc_specified_bg == FALSE) {
        if ((Master_backdrop_url != NULL) && 
			(UserOverride == FALSE || context->type == MWContextDialog)) {
            lo_load_user_backdrop(context, state);
        } else {
            /* Force an opaque solid background. */
            LO_SetDocBgColor(context, &state->text_bg);
        }
    }
}

/*************************************
 * Function: lo_FlushLineList
 *
 * Description: This function adds a linefeed element to the end
 *	of the current line, and now that the line is done, it asks
 *	the front end to display the line.  This function is only
 * 	called by a hard linebreak.
 *
 * Params: Window context, document state, and flag for whether this is
 *	a breaking linefeed of not.  A breaking linefeed is one inserted
 *	just to break a text flow to the current window width.
 *
 * Returns: Nothing
 *************************************/
void
lo_FlushLineList(MWContext *context, lo_DocState *state, uint32 break_type, uint32 clear_type, Bool breaking)
{
	
	LO_LinefeedStruct *linefeed;
	/*
	LO_Element **line_array;
	*/
	/* int32 justification_remainder=0;*/
	/*
#ifdef XP_WIN16
	int32 a_size;
	int32 a_indx;
	int32 a_line;
	XP_Block *larray_array;
#endif 
	*/
	/* XP_WIN16 */

	lo_UpdateStateWhileFlushingLine( context, state );

	/*
	if (state->top_state->nothing_displayed != FALSE) {
	*/
		/*
		 * If we are displaying elements we are
		 * no longer in the HEAD section of the HTML
		 * we are in the BODY section.
		 */
	/*
		state->top_state->in_head = FALSE;
		state->top_state->in_body = TRUE;

        lo_use_default_doc_background(context, state);
        state->top_state->nothing_displayed = FALSE;
    }
	*/

	/*
	 * There is a break at the end of this line.
	 * this may change min_width.
	 */
	/*
	{
		int32 new_break_holder;
		int32 min_width;
		int32 indent;

		new_break_holder = state->x;
		min_width = new_break_holder - state->break_holder;
		indent = state->list_stack->old_left_margin - state->win_left;
		min_width += indent;
		if (min_width > state->min_width)
		{
			state->min_width = min_width;
		}
	*/
		/* If we are not within <NOBR> content, allow break_holder
		 * to be set to the new position where a line break can occur.
		 * This fixes BUG #70782
		 */
	/*
		if (state->breakable != FALSE) {
			state->break_holder = new_break_holder;
		}
	}
	*/

	/*
	 * If in a division centering or right aligning this line
	 */
	/*
	if ((state->align_stack != NULL)&&(state->delay_align == FALSE))
	{
		int32 push_right;
		LO_Element *tptr;

		if (state->align_stack->alignment == LO_ALIGN_CENTER)
		{
			push_right = (state->right_margin - state->x) / 2;
		}
		else if (state->align_stack->alignment == LO_ALIGN_RIGHT)
		{
			push_right = state->right_margin - state->x;
		}
		else if(state->align_stack->alignment == LO_ALIGN_JUSTIFY)
		{
			push_right = lo_calc_push_right_for_justify(state, &justification_remainder);
		}
		else
		{
			push_right = 0;
		}

		if ((push_right > 0 || justification_remainder)
			&&(state->line_list != NULL))
		{
			int32 count = 0;
			int32 move_delta;
			tptr = state->line_list;

			if(state->align_stack->alignment == LO_ALIGN_JUSTIFY)
				move_delta = 0;
			else
				move_delta = push_right;

			while (tptr != NULL)
			{
	*/
                /* 
                 * We don't shift over inflow layers, since their contents
                 * have already been shifted over.
                 */
		/*
		 * We also don't shift bullets of type BULLET_MQUOTE.
		 */
	/*
                if (((tptr->type != LO_CELL)||
			(!tptr->lo_cell.cell_inflow_layer))&&
			((tptr->type != LO_BULLET)||
			((tptr->type == LO_BULLET)&&
			 (tptr->lo_bullet.bullet_type != BULLET_MQUOTE))))
		{
				tptr->lo_any.x += move_delta;
		}
	*/

				/* justification push_rights are additive */
	/*
				if(state->align_stack->alignment == LO_ALIGN_JUSTIFY)
				{
					move_delta += push_right;
	*/
					/* if there is a justification remainder, add a pixel
					 * to the first n word breaks
					 */
	/*
					if(count < justification_remainder)
						move_delta++;
				}
	*/
                /* 
                 * Note that if this is an inflow layer, we don't want
                 * to shift it since the alignment has already propogated
                 * to its contents.
                 */
	/*
				if ((tptr->type == LO_CELL) &&
                    !tptr->lo_cell.cell_inflow_layer)
				{
					lo_ShiftCell((LO_CellStruct *)tptr, move_delta, 0);
				}
				tptr = tptr->lo_any.next;

				count++;
			}

			if(state->align_stack->alignment == LO_ALIGN_JUSTIFY)
				state->x = state->right_margin;
			else
				state->x += push_right;
		}
	}
	*/
#if 0
	/* apply line-height stack mods (if exists) */
	if(state->line_height_stack && state->end_last_line) 
	{
		LO_LinefeedStruct *linefeed = (LO_LinefeedStruct*)state->end_last_line;
		int32 cur_line_height;
		int32 new_line_height;
		int32 line_height_diff;

		if(state->line_height == 0)
		{
        	int32 new_height = state->text_info.ascent +
            						state->text_info.descent;
			int32 new_baseline = state->text_info.ascent;

        	if ((new_height <= 0)&&(state->font_stack != NULL)&&
            	(state->font_stack->text_attr != NULL))
			{
				lo_fillin_text_info(context, state);

            	new_height = state->text_info.ascent + state->text_info.descent;
				new_baseline = state->text_info.ascent;

			}

			if(new_height <= 0)
			{
				new_height = state->default_line_height;
				new_baseline = state->default_line_height;
			}

			state->line_height = new_height;
			state->baseline = new_baseline;
		}

		cur_line_height = (state->y + state->baseline) - 
								(linefeed->y + linefeed->baseline);

		new_line_height = state->line_height_stack->height;
		line_height_diff = new_line_height - cur_line_height;

		/* only allow increasing line heights 
		 * explicitly disallow negative diffs	
		 */
		if(line_height_diff > 0)
		{
			lo_add_to_y_for_all_elements_in_line(state, line_height_diff);

			state->baseline += line_height_diff;
			state->line_height += line_height_diff;
		}
	}
#endif


	/*
	 * Tack a linefeed element on to the end of the line list.
	 * Take advantage of this necessary pass thorugh the line's
	 * elements to set the line_height field in all
	 * elements, and to look and see if we have reached a
	 * possibly display blocking element.
	 */
	linefeed = lo_NewLinefeed(state, context, break_type, clear_type);

	if (linefeed != NULL)
	{
		lo_AppendLineFeed( context, state, linefeed, breaking, TRUE );

		/*
		LO_Element *tptr;

		if (breaking != FALSE)
		{
			linefeed->ele_attrmask |= LO_ELE_BREAKING;
		}
		*/

		/*
		 * Horrible bitflag overuse!!!  For multicolumn text
		 * we need to know if a line of text can be used for
		 * a column break, or if it cannot because it is wrapped
		 * around some object in the margin.  For lines that can be
		 * used for column breaks, we will set the BREAKABLE
		 * flag in their element mask.
		 */
		/*
		if ((state->left_margin_stack == NULL)&&
			(state->right_margin_stack == NULL))
		{
			linefeed->ele_attrmask |= LO_ELE_BREAKABLE;
		}

		if ((state->align_stack != NULL)&&
		    (state->delay_align != FALSE)&&
		    (state->align_stack->alignment != LO_ALIGN_LEFT))
		{
			if (state->align_stack->alignment == LO_ALIGN_CENTER)
			{
				linefeed->ele_attrmask |= LO_ELE_DELAY_CENTER;
			}
			else if (state->align_stack->alignment == LO_ALIGN_RIGHT)
			{
				linefeed->ele_attrmask |= LO_ELE_DELAY_RIGHT;
			}
		}

		tptr = state->line_list;

		if (tptr == NULL)
		{
			state->line_list = (LO_Element *)linefeed;
			linefeed->prev = NULL;
		}
		else
		{
			while (tptr != NULL)
			{
		*/
				/*
				 * If the display is blocked for an element
				 * we havn't reached yet, check to see if
				 * it is in this line, and if so, save its
				 * y position.
				 */
		/*
				if ((state->display_blocked != FALSE)&&
#ifdef EDITOR
				    (!state->edit_relayout_display_blocked)&&
#endif
				    (state->is_a_subdoc == SUBDOC_NOT)&&
				    (state->display_blocking_element_y == 0)&&
				    (state->display_blocking_element_id != -1)&&
				    (tptr->lo_any.ele_id >=
					state->display_blocking_element_id))
				{
					state->display_blocking_element_y =
						state->y;
		*/
					/*
					 * Special case, if the blocking element
					 * is on the first line, no blocking
					 * at all needs to happen.
					 */
		/*
					if (state->y == state->win_top)
					{
						state->display_blocked = FALSE;
						FE_SetDocPosition(context,
						    FE_VIEW, 0, state->base_y);
                        if (context->compositor)
						{
						  XP_Rect rect;
						  
						  rect.left = 0;
						  rect.top = 0;
						  rect.right = state->win_width;
						  rect.bottom = state->win_height;
						  CL_UpdateDocumentRect(context->compositor,
                                                &rect, (PRBool)FALSE);
						}
					}
				}
				tptr->lo_any.line_height = state->line_height;
		*/
				/*
				 * Special for bullets of type BULLET_MQUOTE
				 * They should always be as tall as the line.
				 */
		/*
				if ((tptr->type == LO_BULLET)&&
					(tptr->lo_bullet.bullet_type ==
						BULLET_MQUOTE))
				{
					tptr->lo_any.y_offset = 0;
					tptr->lo_any.height =
						tptr->lo_any.line_height;
				}
				tptr = tptr->lo_any.next;
			}
			linefeed->prev = tptr;
		}
		state->x += linefeed->width;
		*/
	} 
	else
	{
		return;		/* ALEKS OUT OF MEMORY RETURN */
	}

	/*
	 * Nothing to flush
	 */
	if (state->line_list == NULL)
	{
		return;
	}

	/*
	 * We are in a layer within this (sub)doc, stuff the line there instead.
	 */
	if (state->layer_nest_level > 0)
	{
		lo_AddLineListToLayer(context, state, (LO_Element *)linefeed);
		state->line_list = NULL;
		state->old_break = NULL;
		state->old_break_block = NULL;
		state->old_break_pos = -1;
		state->old_break_width = 0;
		state->baseline = 0;
		return;
	}

	/*
	 * If necessary, grow the line array to hold more lines.
	 */
/*
#ifdef XP_WIN16
	a_size = SIZE_LIMIT / sizeof(LO_Element *);
	a_indx = (state->line_num - 1) / a_size;
	a_line = state->line_num - (a_indx * a_size);

	XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
	state->line_array = larray_array[a_indx];

	if (a_line == a_size)
	{
		state->line_array = XP_ALLOC_BLOCK(LINE_INC *
					sizeof(LO_Element *));
		if (state->line_array == NULL)
		{
			XP_UNLOCK_BLOCK(state->larray_array);
			state->top_state->out_of_memory = TRUE;
			return;
		}
		XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
		line_array[0] = NULL;
		XP_UNLOCK_BLOCK(state->line_array);
		state->line_array_size = LINE_INC;

		state->larray_array_size++;
		XP_UNLOCK_BLOCK(state->larray_array);
		state->larray_array = XP_REALLOC_BLOCK(
			state->larray_array, (state->larray_array_size
			* sizeof(XP_Block)));
		if (state->larray_array == NULL)
		{
			state->top_state->out_of_memory = TRUE;
			return;
		}
		XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
		larray_array[state->larray_array_size - 1] = state->line_array;
		state->line_array = larray_array[a_indx];
	}
	else if (a_line >= state->line_array_size)
	{
		state->line_array_size += LINE_INC;
		if (state->line_array_size > a_size)
		{
			state->line_array_size = (intn)a_size;
		}
		state->line_array = XP_REALLOC_BLOCK(state->line_array,
		    (state->line_array_size * sizeof(LO_Element *)));
		if (state->line_array == NULL)
		{
			XP_UNLOCK_BLOCK(state->larray_array);
			state->top_state->out_of_memory = TRUE;
			return;
		}
		larray_array[a_indx] = state->line_array;
	}
*/
	/*
	 * Place this line of elements into the line array.
	 */
/*
	XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
	line_array[a_line - 1] = state->line_list;
	XP_UNLOCK_BLOCK(state->line_array);

	XP_UNLOCK_BLOCK(state->larray_array);
#else
	if (state->line_num > state->line_array_size)
	{
		int32 line_inc;

		if (state->line_array_size > (LINE_INC * 10))
		{
			line_inc = state->line_array_size / 10;
		}
		else
		{
			line_inc = LINE_INC;
		}

		state->line_array = XP_REALLOC_BLOCK(state->line_array,
			((state->line_array_size + line_inc) *
			sizeof(LO_Element *)));
		if (state->line_array == NULL)
		{
			state->top_state->out_of_memory = TRUE;
			return;
		}
		state->line_array_size += line_inc;
	}
*/
	/*
	 * Place this line of elements into the line array.
	 */
/*
	 XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
	line_array[state->line_num - 1] = state->line_list;
	XP_UNLOCK_BLOCK(state->line_array);
#endif */

/* XP_WIN16 */

	/*
	 * connect the complete doubly linked list between this line
	 * and the last one.
	 */
/*
	if (state->end_last_line != NULL)
	{
		state->end_last_line->lo_any.next = state->line_list;
		state->line_list->lo_any.prev = state->end_last_line;
	}
	state->end_last_line = (LO_Element *)linefeed;

	state->line_list = NULL;
	state->line_num++;
*/
	/* 
	 *Don't draw if we're doing layers...we'll just refresh when the 
	 * the layer size increases.
	 */
/*
#ifdef LAYERS
*/
	/*
	 * Have the front end display this line.
	 */
    /* For layers, only draw if a compositor doesn't exist */
/*
    if (!context->compositor)
*/
        /* We need to supply a display rectangle that is guaranteed to
           encompass all elements on the line.  The special 0x3fffffff
           value is approximately half the range of a 32-bit int, so
           it leaves room for overflow if arithmetic is done on these
           values. */
/*
        lo_DisplayLine(context, state, (state->line_num - 2),
                       0, 0, 0x3fffffffL, 0x3fffffffL);
#else
        lo_DisplayLine(context, state, (state->line_num - 2),
                       0, 0, 0x3fffffffL, 0x3fffffffL);
#endif */ /* LAYERS */

	lo_UpdateStateAfterFlushingLine( context, state, linefeed, FALSE );
	/*
	 * We are starting a new line.  Clear any old break
	 * positions left over, clear the line_list, and increase
	 * the line number.
	 */
	/*
	state->old_break = NULL;
	state->old_break_block = NULL;
	state->old_break_pos = -1;
	state->old_break_width = 0;
	state->baseline = 0;
	*/
}


/*************************************
 * Function: lo_CleanTextWhitespace
 *
 * Description: Utility function to pass through a line an clean up
 * 	all its whitespace.   Compress multiple whitespace between
 *	element to a single space, and remove heading and
 *	trailing whitespace.
 *	Text is cleaned in place in the passed buffer.
 *
 * Params: Pointer to text to be cleand, and its length.
 *
 * Returns: Length of cleaned text.
 *************************************/
int32
lo_CleanTextWhitespace(char *text, int32 text_len)
{
	char *from_ptr;
	char *to_ptr;
	int32 len;
	int32 new_len;

	if (text == NULL)
	{
		return(0);
	}

	len = 0;
	new_len = 0;
	from_ptr = text;
	to_ptr = text;

	while (len < text_len)
	{
		/*
		 * Compress chunk of whitespace
		 */
		while ((len < text_len)&&(XP_IS_SPACE(*from_ptr)))
		{
			len++;
			from_ptr++;
		}
		if (len == text_len)
		{
			continue;
		}

		/*
		 * Skip past "good" text
		 */
		while ((len < text_len)&&(!XP_IS_SPACE(*from_ptr)))
		{
			*to_ptr++ = *from_ptr++;
			len++;
			new_len++;
		}

		/*
		 * Put in one space to represent the compressed spaces.
		 */
		if (len != text_len)
		{
			*to_ptr++ = ' ';
			new_len++;
		}
	}

	/*
	 * Remove the trailing space, and terminate string.
	 */
	if ((new_len > 0)&&(*(to_ptr - 1) == ' '))
	{
		to_ptr--;
		new_len--;
	}
	*to_ptr = '\0';

	return(new_len);
}


/*
 * Used to strip white space off of HREF parameter values to make
 * valid URLs out of them.  Remove whitespace from both ends, and
 * remove all non-space whitespace from the middle.
 */
int32
lo_StripTextWhitespace(char *text, int32 text_len)
{
	char *from_ptr;
	char *to_ptr;
	int32 len;
	int32 tail;

	if ((text == NULL)||(text_len < 1))
	{
		return(0);
	}

	len = 0;
	from_ptr = text;
	/*
	 * strip leading whitespace
	 */
	while ((len < text_len)&&(XP_IS_SPACE(*from_ptr)))
	{
		len++;
		from_ptr++;
	}

	if (len == text_len)
	{
		*text = '\0';
		return(0);
	}

	tail = 0;
	from_ptr = (char *)(text + text_len - 1);
	/*
	 * Remove any trailing space
	 */
	while (XP_IS_SPACE(*from_ptr))
	{
		from_ptr--;
		tail++;
	}

	/*
	 * terminate string
	 */
	from_ptr++;
	*from_ptr = '\0';

	/*
	 * remove all non-space whitespace from the middle of the string.
	 */
	from_ptr = (char *)(text + len);
	len = text_len - len - tail;
	to_ptr = text;
	while (*from_ptr != '\0')
	{
		if (XP_IS_SPACE(*from_ptr) && (*from_ptr != ' '))
		{
			from_ptr++;
			len--;
		}
		else
		{
			*to_ptr++ = *from_ptr++;
		}
	}
	*to_ptr = '\0';

	return(len);
}


LO_AnchorData *
lo_NewAnchor(lo_DocState *state, PA_Block href, PA_Block targ)
{
	LO_AnchorData *anchor_data;
	lo_LayerDocState *layer_state;

	anchor_data = XP_NEW_ZAP(LO_AnchorData);
	if (anchor_data)
	{
		anchor_data->anchor = href;
		anchor_data->target = targ;
		layer_state = lo_CurrentLayerState(state);
		if (layer_state )
			anchor_data->layer = layer_state->layer;
	}
	else
	{
		state->top_state->out_of_memory = TRUE;
	}
	return anchor_data;
}


void
lo_DestroyAnchor(LO_AnchorData *anchor_data)
{
	if (anchor_data->anchor != NULL)
	{
		PA_FREE(anchor_data->anchor);
	}
	if (anchor_data->target != NULL)
	{
		PA_FREE(anchor_data->target);
	}
	XP_DELETE(anchor_data);
}


/*
 * Add this url's block to the list
 * of all allocated urls so we can free
 * it later.
 */
void
lo_AddToUrlList(MWContext *context, lo_DocState *state,
	LO_AnchorData *url_buff)
{
	lo_TopState *top_state;
	int32 i;
	LO_AnchorData **anchor_array;
	intn a_url;
#ifdef XP_WIN16
	intn a_size;
	intn a_indx;
	XP_Block *ulist_array;
#endif /* XP_WIN16 */
    lo_DocLists *doc_lists;
    
    doc_lists = lo_GetCurrentDocLists(state);

	/*
	 * Error checking
	 */
	if ((url_buff == NULL)||(state == NULL))
	{
		return;
	}

	top_state = state->top_state;

	/*
	 * We may need to grow the url_list.
	 */
#ifdef XP_WIN16
	a_size = SIZE_LIMIT / sizeof(XP_Block *);
	a_indx = doc_lists->url_list_len / a_size;
	a_url = doc_lists->url_list_len - (a_indx * a_size);

	XP_LOCK_BLOCK(ulist_array, XP_Block *, doc_lists->ulist_array);


	if ((a_url == 0)&&(a_indx > 0))
	{
		doc_lists->url_list = XP_ALLOC_BLOCK(URL_LIST_INC *
					sizeof(LO_AnchorData *));
		if (doc_lists->url_list == NULL)
		{
			XP_UNLOCK_BLOCK(doc_lists->ulist_array);
			top_state->out_of_memory = TRUE;
			return;
		}
		doc_lists->url_list_size = URL_LIST_INC;
		XP_LOCK_BLOCK(anchor_array, LO_AnchorData **,
			doc_lists->url_list);
		for (i=0; i < URL_LIST_INC; i++)
		{
			anchor_array[i] = NULL;
		}
		XP_UNLOCK_BLOCK(doc_lists->url_list);

		doc_lists->ulist_array_size++;
		XP_UNLOCK_BLOCK(doc_lists->ulist_array);
		doc_lists->ulist_array = XP_REALLOC_BLOCK(
			doc_lists->ulist_array, (doc_lists->ulist_array_size
			* sizeof(XP_Block)));
		if (doc_lists->ulist_array == NULL)
		{
			top_state->out_of_memory = TRUE;
			return;
		}
		XP_LOCK_BLOCK(ulist_array, XP_Block *, doc_lists->ulist_array);
		ulist_array[doc_lists->ulist_array_size - 1] = doc_lists->url_list;
		doc_lists->url_list = ulist_array[a_indx];
	}
	else if (a_url >= doc_lists->url_list_size)
	{
		int32 url_list_inc;

		if ((doc_lists->url_list_size / 10) > URL_LIST_INC)
		{
			url_list_inc = doc_lists->url_list_size / 10;
		}
		else
		{
			url_list_inc = URL_LIST_INC;
		}
		doc_lists->url_list_size += (intn)url_list_inc;
		if (doc_lists->url_list_size > a_size)
		{
			doc_lists->url_list_size = a_size;
		}

		doc_lists->url_list = ulist_array[a_indx];
		doc_lists->url_list = XP_REALLOC_BLOCK(doc_lists->url_list,
			(doc_lists->url_list_size * sizeof(LO_AnchorData *)));
		if (doc_lists->url_list == NULL)
		{
			XP_UNLOCK_BLOCK(doc_lists->ulist_array);
			top_state->out_of_memory = TRUE;
			return;
		}
		ulist_array[a_indx] = doc_lists->url_list;
		/*
		 * Clear the new entries
		 */
		XP_LOCK_BLOCK(anchor_array, LO_AnchorData **,
			doc_lists->url_list);
		for (i = doc_lists->url_list_len; i < doc_lists->url_list_size; i++)
		{
			anchor_array[i] = NULL;
		}
		XP_UNLOCK_BLOCK(doc_lists->url_list);
	}
	doc_lists->url_list = ulist_array[a_indx];
#else
	if (doc_lists->url_list_len ==
		doc_lists->url_list_size)
	{
		int32 url_list_inc;

		if ((doc_lists->url_list_size / 10) > URL_LIST_INC)
		{
			url_list_inc = doc_lists->url_list_size / 10;
		}
		else
		{
			url_list_inc = URL_LIST_INC;
		}
		doc_lists->url_list_size += url_list_inc;
		doc_lists->url_list = XP_REALLOC_BLOCK(doc_lists->url_list,
					(doc_lists->url_list_size *
					sizeof(LO_AnchorData *)));
		if (doc_lists->url_list == NULL)
		{
			top_state->out_of_memory = TRUE;
			return;
		}

		/*
		 * Clear the new entries
		 */
		XP_LOCK_BLOCK(anchor_array, LO_AnchorData **, doc_lists->url_list);
		for (i = doc_lists->url_list_len; i < doc_lists->url_list_size; i++)
		{
			anchor_array[i] = NULL;
		}
		XP_UNLOCK_BLOCK(doc_lists->url_list);
	}
	a_url = doc_lists->url_list_len;
#endif /* XP_WIN16 */

	XP_LOCK_BLOCK(anchor_array, LO_AnchorData **, doc_lists->url_list);
	if (anchor_array[a_url] != NULL)
	{
		lo_DestroyAnchor(anchor_array[a_url]);
	}
	anchor_array[a_url] = url_buff;
	doc_lists->url_list_len++;
	XP_UNLOCK_BLOCK(doc_lists->url_list);
}


void
lo_BodyMargins(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	PA_Block buff;
	char *str;
	int32 margin_width;
	int32 margin_height;
	Bool changes;

	margin_width = state->win_left;
	margin_height = state->win_top;
	changes = FALSE;

	/*
	 * Get the margin width.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_MARGINWIDTH);
	if (buff != NULL)
	{
		int32 val;

		PA_LOCK(str, char *, buff);
		val = XP_ATOI(str);
		if (val < 0)
		{
			val = 0;
		}
		margin_width = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
		margin_width = FEUNITS_X(margin_width, context);
		/*
		 * Sanify based on window width.
		 */
		if (margin_width > ((state->win_width  / 2) - 1))
		{
			margin_width = ((state->win_width  / 2) - 1);
		}
		state->top_state->body_attr |= BODY_ATTR_MARGINS;
		changes = TRUE;
	}

	/*
	 * Get the margin height.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_MARGINHEIGHT);
	if (buff != NULL)
	{
		int32 val;

		PA_LOCK(str, char *, buff);
		val = XP_ATOI(str);
		if (val < 0)
		{
			val = 0;
		}
		margin_height = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
		margin_height = FEUNITS_Y(margin_height, context);
		/*
		 * Sanify based on window height.
		 */
		if (margin_height > ((state->win_height  / 2) - 1))
		{
			margin_height = ((state->win_height  / 2) - 1);
		}
		state->top_state->body_attr |= BODY_ATTR_MARGINS;
		changes = TRUE;
	}

	if (changes != FALSE)
	{
		state->win_top = margin_height;
		state->win_bottom = margin_height;

		state->win_left = margin_width;
		state->win_right = margin_width;

		state->max_width = state->win_left + state->win_right;
		state->max_height = state->win_top + state->win_bottom;

		state->x = state->win_left;
		state->y = state->win_top;

		state->left_margin = state->win_left;
		state->right_margin = state->win_width - state->win_right;

		state->break_holder = state->x;

		/*
		 * Need to reset the margin values in the default list stack
		 */
		if ((state->list_stack->type == P_UNKNOWN)&&
			(state->list_stack->next == NULL))
		{
			state->list_stack->old_left_margin = state->win_left;
			state->list_stack->old_right_margin =
					state->win_width - state->win_right;
		}
		else
		{
		/*
		 * This should never happen.  If it doesm it means we somehow
		 * started a list without knowing that we had entered the BODY
		 * of the document.
		 */
		}
	}
}


void
lo_ProcessBodyTag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	if (tag->is_end == FALSE)
	{
		if ((state->end_last_line == NULL)&&
		    ((state->top_state->body_attr & BODY_ATTR_MARGINS) == 0))
		{
			lo_BodyMargins(context, state, tag);
		}
		if (UserOverride == FALSE || context->type == MWContextDialog)
		{
			lo_BodyBackground(context, state, tag, FALSE);
		}
		if (( !EDT_IS_EDITOR( context ) )&&
			((state->top_state->body_attr & BODY_ATTR_JAVA) == 0))
		{
			if (lo_ProcessContextEventHandlers(context, state, tag))
			{
				state->top_state->body_attr |= BODY_ATTR_JAVA;
			}
		}
	}
}

void
lo_SetBaseUrl(lo_TopState *top_state, char *url, Bool is_blocked)
{
    if (url)
        url = XP_STRDUP(url);

    if (is_blocked)
    {
        if (top_state->tag_from_inline_stream)
        {
            XP_FREEIF(top_state->inline_stream_blocked_base_url);
            top_state->inline_stream_blocked_base_url = url;
        }
        else
        {
            XP_FREEIF(top_state->main_stream_blocked_base_url);
            top_state->main_stream_blocked_base_url = url;
        }
    }
    else 
    {
        XP_FREEIF(top_state->base_url);
        top_state->base_url = url;
    }
}

/*************************************
 * Function: lo_BlockTag
 *
 * Description: This function blocks the layout of the tag, and
 *	saves them in the context.
 *
 * Params: Window context and document state, and the tag to be blocked.
 *
 * Returns: Nothing
 *************************************/
void
lo_BlockTag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	PA_Tag ***tags_end_ptr, **tags_end;	/* triple-indirect, wow! see below */
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *new_state;
	char *save_base;

	/*
	 * All blocked tags should be at the top level of state
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return;
	}
	state = top_state->doc_state;

	new_state = lo_CurrentSubState(state);

	if (top_state->tags == NULL)
	    PA_HoldDocData(top_state->doc_data);

	/*
	 * For everything except BASE tags, we need to use
	 * the blocked base here as our real base so that
	 * relative URLs are processed properly.
	 */
	if (tag->type != P_BASE)
    {
        save_base = top_state->base_url;
        if (top_state->tag_from_inline_stream)
        {
            if (top_state->inline_stream_blocked_base_url)
                top_state->base_url = top_state->inline_stream_blocked_base_url;
        }
        else
        {
            if (top_state->main_stream_blocked_base_url)
                top_state->base_url = top_state->main_stream_blocked_base_url;
        }
    }

	switch (tag->type)
	{
		/*
		 * We need to process these when blocked so that
		 * relative URLs of images that are processed when
		 * blocked are correct.
		 */
		case P_BASE:
			{
				PA_Block buff;
				char *url;

				buff = lo_FetchParamValue(context, tag,
						PARAM_HREF);
				if (buff != NULL)
				{
					PA_LOCK(url, char *, buff);
					if (url != NULL)
					{
					    int32 len;

					    len = lo_StripTextWhitespace(
							url, XP_STRLEN(url));
					}
                    lo_SetBaseUrl(top_state, url, TRUE);
					PA_UNLOCK(buff);
					PA_FREE(buff);
				}
			}
			break;
		case P_IMAGE:
		case P_NEW_IMAGE:
			if (tag->is_end == FALSE)
			{
                /*
                 * For images that are part of the main document, we 
                 * can use the doc_data's url as the base URL. For
                 * images that are part of a LAYER that has a SRC
                 * attribute, we use the top_state's version (the base_url
                 * is temporarily changed to that of the LAYER while
                 * we process tags within the layer). The input_write_level
                 * tells us whether the tag is part of the main document
                 * or the layer.
                 */
                lo_BlockedImageLayout(context, new_state, tag,
                                      top_state->base_url);
			}
			break;
		case P_SCRIPT:
		case P_STYLE:
		case P_LINK:
			lo_BlockScriptTag(context, new_state, tag);
			break;
        case P_LAYER:
            lo_BlockLayerTag(context, new_state, tag);
            break;
		case P_PARAM:
			lo_ProcessParamTag(context, new_state, tag, TRUE);
			break;
		case P_OBJECT:          
			lo_ProcessObjectTag(context, new_state, tag, TRUE);
			break;
	}
	/*
	 * Now restore the base that you switched if it
	 * wasn't a BASE tag.
	 */
	if (tag->type != P_BASE)
	{
		top_state->base_url = save_base;
	}

	/*
	 * Unify the basis and induction cases of singly-linked list insert
	 * using a double-indirect pointer.  Instead of
	 *
	 *	if (head == NULL)
	 *		head = tail = elem;
	 *	else
	 *		tail = tail->next = elem;
	 *
	 * where head = tail = NULL is the empty state and elem->next = NULL,
	 * use a pointer to the pointer to the last element:
	 *
	 *	*tail_ptr = elem;
	 *	tail_ptr = &elem->next;
	 *
	 * where head = NULL, tail_ptr = &head is the empty state.
	 *
	 * The triple-indirection below is to avoid further splitting cases
	 * based on whether we're inserting at the input_write_point or 
	 * at the end of the tags list.  The 
	 * 'if (top_state->input_write_level)' test is unavoidable, but 
	 * once the tags_end_ptr variable is set to point at the right 
	 * double-indirect "tail_ptr" (per above sketch), there is shared code.
	 */

	if (top_state->input_write_level)
	{
	    tags_end_ptr = &top_state->input_write_point[top_state->input_write_level-1];
		if (*tags_end_ptr == NULL)
		{
			*tags_end_ptr = top_state->tags_end;
		}
		if (top_state->tags_end == *tags_end_ptr)
		{
			*tags_end_ptr = &tag->next;
			tags_end_ptr = &top_state->tags_end;
		}
	}
	else
	{
		tags_end_ptr = &top_state->tags_end;
	}
	tags_end = *tags_end_ptr;
	tag->next = *tags_end;
	*tags_end = tag;
	*tags_end_ptr = &tag->next;
}


lo_DocState *
lo_TopSubState(lo_TopState *top_state)
{
	lo_DocState *new_state;

	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return(NULL);
	}

	new_state = top_state->doc_state;
	while (new_state->sub_state != NULL)
	{
		new_state = new_state->sub_state;
	}

	return(new_state);
}


lo_DocState *
lo_CurrentSubState(lo_DocState *state)
{
	lo_DocState *new_state;

	if (state == NULL)
	{
		return(NULL);
	}

	new_state = state;
	while (new_state->sub_state != NULL)
	{
		new_state = new_state->sub_state;
	}

	return(new_state);
}


/*************************************
 * Function: lo_FetchTopState
 *
 * Description: This function uses the document ID to locate the
 *	lo_TopState structure for this document.
 *	Eventually this state will be kept somewhere in the
 *	Window context.
 *
 * Params: A unique document ID as derived from the window context.
 *
 * Returns: A pointer to the lo_TopState structure for this document.
 *	    NULL on error.
 *************************************/
lo_TopState *
lo_FetchTopState(int32 doc_id)
{
	lo_StateList *sptr;
	lo_TopState *state;

	sptr = StateList;
	while (sptr != NULL)
	{
		if (sptr->doc_id == doc_id)
		{
			break;
		}
		sptr = sptr->next;
	}
	if (sptr == NULL)
	{
		state = NULL;
	}
	else
	{
		state = sptr->state;
	}

	return(state);
}


static void
lo_RemoveTopState(int32 doc_id)
{
	lo_StateList *state;
	lo_StateList *sptr;

	state = StateList;
	sptr = StateList;
	while (sptr != NULL)
	{
		if (sptr->doc_id == doc_id)
		{
			break;
		}
		state = sptr;
		sptr = sptr->next;
	}
	if (sptr != NULL)
	{
		if (sptr == StateList)
		{
			StateList = StateList->next;
		}
		else
		{
			state->next = sptr->next;
		}
		XP_DELETE(sptr);
	}
}


Bool
lo_StoreTopState(int32 doc_id, lo_TopState *new_state)
{
	lo_StateList *sptr;

	sptr = StateList;
	while (sptr != NULL)
	{
		if (sptr->doc_id == doc_id)
		{
			break;
		}
		sptr = sptr->next;
	}

	if (sptr == NULL)
	{
		sptr = XP_NEW(lo_StateList);
		if (sptr == NULL)
		{
			return FALSE;
		}
		sptr->doc_id = doc_id;
		sptr->next = StateList;
		StateList = sptr;
	}
	sptr->state = new_state;
	return TRUE;
}


void
lo_SaveSubdocTags(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	PA_Tag *tag_ptr;

	tag->next = NULL;

	if (state->subdoc_tags_end == NULL)
	{
		if (state->subdoc_tags == NULL)
		{
			state->subdoc_tags = tag;
			state->subdoc_tags_end = tag;
		}
		else
		{
			state->subdoc_tags->next = tag;
			state->subdoc_tags = tag;
			state->subdoc_tags_end = tag;
		}
	}
	else
	{
		tag_ptr = state->subdoc_tags_end;
		tag_ptr->next = tag;
		state->subdoc_tags_end = tag;
	}
}



void
lo_FlushBlockage(MWContext *context, lo_DocState *state,
				 lo_DocState *main_doc_state)
{
	PA_Tag *tag_ptr;
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *orig_state;
	lo_DocState *up_state;
	Bool was_blockage;

	/*
	 * All blocked tags should be at the top level of state
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return;
	}
    /* Prevent re-entering */
    if (top_state->flushing_blockage)
        return;
    top_state->flushing_blockage = TRUE;

	was_blockage = (top_state->tags != NULL);
 	tag_ptr = top_state->tags;
   
	orig_state = top_state->doc_state;
	
	up_state = NULL;
	state = orig_state;
	while (state->sub_state != NULL)
	{
		up_state = state;
		state = state->sub_state;
	}

	top_state->layout_blocking_element = NULL;

	while ((tag_ptr != NULL)&&(top_state->layout_blocking_element == NULL))
	{
		PA_Tag *tag;
		lo_DocState *new_state;
		lo_DocState *new_up_state;
		lo_DocState *tmp_state;
		Bool may_save;
		int32 state_diff;

		tag = tag_ptr;
		tag_ptr = tag_ptr->next;
		tag->next = NULL;

		/*
		 * Always keep top_state->tags sane
		 */
		top_state->tags = tag_ptr;

		/*
		 * Since script processing is asynchronous we need to do this
		 *   whenever we flush blockage
		 */
		if (lo_FilterTag(context, state, tag) == FALSE)
		{
			PA_FreeTag(tag);
			continue;
		}
				
		if ((state->is_a_subdoc == SUBDOC_CELL)||
			(state->is_a_subdoc == SUBDOC_CAPTION))
		{
			may_save = TRUE;
		}
		else
		{
			may_save = FALSE;
		}

		/*
		 * Reset these so we can tell if anything happened
		 */
		top_state->diff_state = FALSE;
		top_state->state_pushes = 0;
		top_state->state_pops = 0;

		if (state->top_state->out_of_memory == FALSE)
		{
			lo_LayoutTag(context, state, tag);
            if (tag->type == P_LAYER)
            {
                lo_UnblockLayerTag(state);
            }
			if(top_state->wedged_on_mocha) {
			    top_state->wedged_on_mocha = FALSE;
			    goto done;
			}
		}

		/* how has our state level changed? */
		state_diff = top_state->state_pushes - top_state->state_pops;

		new_up_state = NULL;
		new_state = orig_state;
		while (new_state->sub_state != NULL)
		{
			new_up_state = new_state;
			new_state = new_state->sub_state;
		}
		tmp_state = new_state;
		if (may_save != FALSE)
		{
			if (state_diff == -1)
			{
				/*
				 * That tag popped us up one state level.  If this new
				 * state is still a subdoc, save the tag there.
				*/
				if ((tmp_state->is_a_subdoc == SUBDOC_CELL)||
				    (tmp_state->is_a_subdoc == SUBDOC_CAPTION))
				{

                    /* if we just popped a table we need to insert
                     * a dummy end tag to pop the dummy start tag
                     * we shove on the stack after createing a table
                     */
   					PA_Tag *new_tag = LO_CreateStyleSheetDummyTag(tag);
					if(new_tag)
					{
						lo_SaveSubdocTags(context, tmp_state, new_tag);
					}

				    lo_SaveSubdocTags(context, tmp_state, tag);
				}
				else
				{
				    PA_FreeTag(tag);
				}
			}
			else if (( up_state != NULL ) &&
				( top_state->diff_state != FALSE ) &&
				( state_diff == 0 ))
			{
				/*
				 * We have returned to the same state level but
				 * we have a different subdoc.
				 */
				if ((up_state->is_a_subdoc == SUBDOC_CELL)||
				     (up_state->is_a_subdoc == SUBDOC_CAPTION))
				{
				    lo_SaveSubdocTags(context, up_state, tag);
				}
				else
				{
				    PA_FreeTag(tag);
				}
			}
			else if (( top_state->diff_state == FALSE ) &&
				( state_diff == 0 ))
			{
				/*
				 * We are still in the same subdoc
				 */
				lo_SaveSubdocTags(context, state, tag);
			}
			else if (( state_diff == 1 ))
			{
				PA_Tag *new_tag;

				/*
				 * That tag started a new, nested subdoc.
				 * Add the starting tag to the parent.
				 */
				lo_SaveSubdocTags(context, state, tag);
				/*
				 * Since we have extended the parent chain,
				 * we need to reset the child to the new
				 * parent end-chain.
				 */
				if ((tmp_state->is_a_subdoc == SUBDOC_CELL)||
				    (tmp_state->is_a_subdoc == SUBDOC_CAPTION))
				{
					tmp_state->subdoc_tags =
						state->subdoc_tags_end;

					/* add an aditional dummy tag so that style sheets
					 * can use it to query styles from for this entry
					 * that created a table
					 */
					new_tag = LO_CreateStyleSheetDummyTag(tag);
					if(new_tag)
					{
						lo_SaveSubdocTags(context, tmp_state, new_tag);
					}
				}

			}
			/*
			 * This can never happen.
			 */
			else
			{
				PA_FreeTag(tag);
			}
		}
		else
		{
			if(state_diff == 1)
			{
				/* everytime a table is started we need to save the
				 * tag that created it so that style sheets can apply
				 * styles to it.  So we will save even in the "dont save"
				 * case
				 */
				PA_Tag *new_tag = LO_CreateStyleSheetDummyTag(tag);
				if(new_tag)
				{
					/* We just pushed another table into a previous table 
					 * we need to do some table magic to make the subdoc
					 * list work right
					 */
					lo_SaveSubdocTags(context, tmp_state, new_tag);
				}
			}
            else if(state_diff == -1)
            {
                /* if we just popped a table we need to insert
                 * a dummy end tag to pop the dummy start tag
                 * we shove on the stack after createing a table
                 */
   				PA_Tag *new_tag = LO_CreateStyleSheetDummyTag(tag);
				if(new_tag)
				{
					lo_SaveSubdocTags(context, tmp_state, new_tag);
				}

				lo_SaveSubdocTags(context, tmp_state, tag);

            }
			PA_FreeTag(tag);
		}
		tag_ptr = top_state->tags;
		state = new_state;
		up_state = new_up_state;
	}

	if (tag_ptr != NULL)
	{
		top_state->tags = tag_ptr;
	}
	else
	{
	    int i;
		top_state->tags = NULL;
		top_state->tags_end = &top_state->tags;
		for (i = 0; i < MAX_INPUT_WRITE_LEVEL; i++)
			top_state->input_write_point[i] = NULL;
		if (was_blockage) {
			NET_StreamClass s;
			s.data_object=(pa_DocData *)top_state->doc_data;
		    top_state->doc_data = PA_DropDocData(&s); 
		}
	}

	if ((top_state->layout_blocking_element == NULL)&&
		(top_state->tags == NULL)&&
		(top_state->layout_status == PA_COMPLETE))
	{
		/*
		 * makes sure we are at the bottom
		 * of everything in the document.
		 */
		state = lo_CurrentSubState(main_doc_state);
		lo_CloseOutLayout(context, state);
		lo_FinishLayout(context, state, EVENT_LOAD);
	}
done:
	top_state->flushing_blockage = FALSE;
}


/*************************************
 * Function: lo_DisplayLine
 *
 * Description: This function displays a single line
 *	of the document, by having the front end individually
 *	display each element in it.
 *
 * Params: Window context, docuemnt state, and an index into
 *	the line array.
 *
 * Returns: Nothing.
 *************************************/
void
lo_DisplayLine(MWContext *context, lo_DocState *state, int32 line_num,
	int32 x, int32 y, uint32 w, uint32 h)
{
	LO_Element *tptr;
	LO_Element *end_ptr;
	LO_Element **line_array;

#ifdef LOCAL_DEBUG
XP_TRACE(("lo_DisplayLine(%d)\n", line_num));
#endif
	if (state == NULL)
	{
		return;
	}

	if (state->display_blocked != FALSE)
	{
		return;
	}

#ifdef XP_WIN16
	{
		intn a_size;
		intn a_indx;
		intn a_line;
		XP_Block *larray_array;

		a_size = SIZE_LIMIT / sizeof(LO_Element *);
		a_indx = (intn)(line_num / a_size);
		a_line = (intn)(line_num - (a_indx * a_size));

		XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
		state->line_array = larray_array[a_indx];
		XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
		tptr = line_array[a_line];

		if (line_num >= (state->line_num - 2))
		{
			end_ptr = NULL;
		}
		else
		{
			if ((a_line + 1) == a_size)
			{
				XP_UNLOCK_BLOCK(state->line_array);
				state->line_array = larray_array[a_indx + 1];
				XP_LOCK_BLOCK(line_array, LO_Element **,
					state->line_array);
				end_ptr = line_array[0];
			}
			else
			{
				end_ptr = line_array[a_line + 1];
			}
		}
		XP_UNLOCK_BLOCK(state->line_array);
		XP_UNLOCK_BLOCK(state->larray_array);
	}
#else
	XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
	tptr = line_array[line_num];

	if (line_num >= (state->line_num - 2))
	{
		end_ptr = NULL;
	}
	else
	{
		end_ptr = line_array[line_num + 1];
	}
	XP_UNLOCK_BLOCK(state->line_array);
#endif /* XP_WIN16 */

#ifdef LOCAL_DEBUG
	if (tptr == NULL)
	{
		XP_TRACE(("	line %d is NULL!\n", line_num));
	}
#endif

	while (tptr != NULL && tptr != end_ptr)
	{
		lo_DisplayElement(context, tptr,
						  state->base_x, state->base_y,
						  x, y, w, h);
	  
		tptr = tptr->lo_any.next;
	}
}

/*************************************
 * Function: lo_PointToLine
 *
 * Description: This function calculates the line which contains
 *	a point. It's used for tracking mouse motion and button presses.
 *
 * Params: Window context, x, y position of point.
 *
 * Returns: The line that contains the point.
 *	Returns -1 on error.
 *************************************/
int32
lo_PointToLine (MWContext *context, lo_DocState *state, int32 x, int32 y)
{
	int32 line, start, middle, end;
	LO_Element **line_array;
#ifdef XP_WIN16
	int32 line_add;
#endif /* XP_WIN16 */

	line = -1;

	/*
	 * No document state yet.
	 */
	if (state == NULL)
	{
		return(line);
	}

	/*
	 * No laid out lines yet.
	 */
	if (state->line_num == 1 || !state->line_array)
	{
		return(line);
	}

#ifdef XP_WIN16
	{
		intn a_size;
		intn a_indx;
		intn a_line;
		XP_Block *larray_array;

		a_size = SIZE_LIMIT / sizeof(LO_Element *);
		a_indx = (intn)((state->line_num - 2) / a_size);
		a_line = (intn)((state->line_num - 2) - (a_indx * a_size));

		XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
		while (a_indx > 0)
		{
			XP_LOCK_BLOCK(line_array, LO_Element **,
					larray_array[a_indx]);
			if (y < line_array[0]->lo_any.y)
			{
				XP_UNLOCK_BLOCK(larray_array[a_indx]);
				a_indx--;
			}
			else
			{
				XP_UNLOCK_BLOCK(larray_array[a_indx]);
				break;
			}
		}
		state->line_array = larray_array[a_indx];
		XP_UNLOCK_BLOCK(state->larray_array);

		/*
		 * We are about to do a binary search through the line
		 * array, lock it down.
		 */
		XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
		line_add = a_indx * a_size;
		start = 0;
		end = ((a_indx + 1) * a_size) - 1;
		if (end > (state->line_num - 2))
		{
			end = state->line_num - 2;
		}
		end = end - line_add;
	}
#else
	/*
	 * We are about to do a binary search through the line
	 * array, lock it down.
	 */
	XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
	start = 0;
	end = (intn) state->line_num - 2;
#endif /* XP_WIN16 */

	/*
	 * Special case if area is off the top.
	 */
	if (y <= 0)
	{
		line = 0;
	}
	/*
	 * Else another special case if area is off the bottom.
	 */
	else if (y >= line_array[end]->lo_any.y)
	{
		line = end;
	}
	/*
	 * Else a binary search through the document
	 * line list to find the line that contains the point.
	 */
	else
	{
		/*
		 * find the containing line.
		 */
		while ((end - start) > 0)
		{
			middle = (start + end + 1) / 2;
			if (y < line_array[middle]->lo_any.y)
			{
				end = middle - 1;
			}
			else
			{
				start = middle;
			}
		}
		line = start;
		end = (intn) state->line_num - 2; /* reset end */
	}

	/* check for a match higher up 
	 * if it matches find the highest matching line
	 *
	 * matches higher up can be caused by stylesheet 
	 * negative margins
	 */
	while(line > 0 
		  && line_array[line-1]->lo_any.y <= y 
		  && line_array[line-1]->lo_any.y+line_array[line-1]->lo_any.height > y)
	{
		line--;
	}

    /* search through any lines that have the same Y coordinates and
	 * find the closes x directly to the left of the current point
	 */
    if(line != end
	   && line_array[line+1]->lo_any.y <= y 
	   && line_array[line+1]->lo_any.y+line_array[line+1]->lo_any.height > y)
	{
		int32 closest=line;
		int32 cur_line=line+1;

		for(; cur_line <= end
			  && line_array[cur_line]->lo_any.y <= y 
			  && line_array[cur_line]->lo_any.y+line_array[cur_line]->lo_any.height > y
			; cur_line++)
		{
			if(line_array[cur_line]->lo_any.x < x
				&& (line_array[closest]->lo_any.x > x
				    || line_array[closest]->lo_any.x < line_array[cur_line]->lo_any.x))
			{
				closest = cur_line;
			}
		}

		line = closest;
	}

	XP_UNLOCK_BLOCK(state->line_array);
#ifdef XP_WIN16
	line = line + line_add;
#endif /* XP_WIN16 */
	return(line);
}



/*************************************
 * Function: lo_RegionToLines
 *
 * Description: This function calculates the lines which intersect
 * a front end drawable region. It's used for drawing and in
 * calculating page breaks for printing.
 *
 * Params: Window context, x, y position of upper left corner,
 *	and width X height of the rectangle. Also pointers to
 * return values. dontCrop is true if we do not want to draw lines
 * which would be cropped (fall on the top/bottom region boundary).
 * This is for printing.
 *
 * Returns: Sets top and bottom to the lines numbers
 * to display. Sets them to -1 on error.
 *************************************/
void
lo_RegionToLines (MWContext *context, lo_DocState *state, int32 x, int32 y,
	uint32 width, uint32 height, Bool dontCrop, int32 * topLine, int32 * bottomLine)
{
	LO_Element **line_array;
	int32 top, bottom;
	int32 y2;
	int32	topCrop;
	int32	bottomCrop;
	
	/*
		Set drawable lines to -1 (assuming error by default)
	*/
	*topLine = -1;
	*bottomLine = -1;
	
	/*
	 * No document state yet.
	 */
	if (state == NULL)
	{
		return;
	}

	/*
	 * No laid out lines yet.
	 */
	if (state->line_num == 1)
	{
		return;
	}

	y2 = y + height;

	top = lo_PointToLine(context, state, x, y);
	bottom = lo_PointToLine(context, state, x, y2);

	if ( dontCrop )
	{
		XP_LOCK_BLOCK(line_array, LO_Element**, state->line_array );
		
		topCrop = line_array[ top ]->lo_any.y;
		bottomCrop = line_array[ bottom ]->lo_any.y + line_array[ bottom ]->lo_any.line_height;
		XP_UNLOCK_BLOCK( state->line_array );
		
		if ( y > topCrop )
		{
			top++;
			if ( top > ( state->line_num - 2 ) )
				top = -1;
		} 
		if ( y2 < bottomCrop )
			bottom--;
	}
	
	/*
	 * Sanity check
	 */
	if (bottom < top)
	{
		bottom = top;
	}

	*topLine = top;
	*bottomLine = bottom;
}

/*
	LO_CalcPrintArea
    
    Takes a region and adjusts it inward so that the top and bottom lines
    are not cropped. Results can be passed to LO_RefreshArea to print
    one page. To calculate the next page, use y+height as the next y.
*/
intn
LO_CalcPrintArea (MWContext *context, int32 x, int32 y,
				  uint32 width, uint32 height,
				  int32 *top, int32 *bottom)
{
	LO_Element **line_array;
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	int32		lineTop;
	int32		lineBottom;
	
	*top = -1;
	*bottom = -1;
	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return(0);
	}
	state = top_state->doc_state;
	
	/*	
		Find which lines intersect the drawable region.
	*/
	lo_RegionToLines( context, state, x, y, width, height, TRUE, &lineTop, &lineBottom );
	
	if ( lineTop != -1 && lineBottom != -1 )
	{
		XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array );
	
		*top = line_array[ lineTop ]->lo_any.y;
		*bottom = line_array[ lineBottom ]->lo_any.y + line_array[ lineBottom ]->lo_any.line_height - 1;
		
		XP_UNLOCK_BLOCK( state->line_array );
		return (1);
	}
	return (0);
}

void
lo_CloseOutLayout(MWContext *context, lo_DocState *state)
{
	lo_FlushLineBuffer(context, state);

	/*
	 * makes sure we are at the bottom
	 * of everything in the document.
	 *
	 * Only close forms if we aren't in a subdoc because
	 * forms can span subdocs.
	 */
	if ((state->is_a_subdoc == SUBDOC_NOT)&&
		(state->top_state->in_form != FALSE))
	{
		lo_EndForm(context, state);
	}

	lo_SetLineBreakState( context, state, FALSE, LO_LINEFEED_BREAK_SOFT, 1, FALSE);
	lo_ClearToBothMargins(context, state);

	/* Dummy Floating elements on a new line at the end of a document were not getting put into
	   the line_array.  This code should ensure that anything left on the line_list gets flushed
	   to the line_array. */
	if (state->line_list != NULL)
		lo_AppendLineListToLineArray( context, state, NULL);
}

/* Relayout version of lo_CloseOutLayout() */
void lo_EndLayoutDuringReflow( MWContext *context, lo_DocState *state )
{
	/*
	 * makes sure we are at the bottom
	 * of everything in the document.
	 *
	 * Only close forms if we aren't in a subdoc because
	 * forms can span subdocs.
	 */
	if ((state->is_a_subdoc == SUBDOC_NOT)&&
		(state->top_state->in_form != FALSE))
	{
		state->top_state->in_form = FALSE;
	}
	

	lo_SetLineBreakState( context, state, FALSE, LO_LINEFEED_BREAK_SOFT, 1, TRUE);
	lo_ClearToBothMargins(context, state);
}

void
LO_CloseAllTags(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	lo_DocState *sub_state;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return;
	}
	state = top_state->doc_state;

	/*
	 * If we are in a table, get us out.
	 */
	sub_state = lo_CurrentSubState(state);
	while (sub_state != state)
	{
		lo_DocState *old_state;

		lo_CloseOutTable(context, sub_state);
		old_state = sub_state;
		sub_state = lo_CurrentSubState(state);
		/*
		 * If sub_state doesn't change we could be in a loop.
		 * break it here.
		 */
		if (sub_state == old_state)
		{
			break;
		}
	}
	state->sub_state = NULL;

	/*
	 * First do the normal closing.
	 */
	lo_CloseOutLayout(context, state);

	/*
	 * Reset a bunch of state variables.
	 */
	state->text_divert = P_UNKNOWN;
	state->delay_align = FALSE;
	state->breakable = TRUE;
	state->allow_amp_escapes = TRUE;
	state->preformatted = PRE_TEXT_NO;
	state->preformat_cols = 0;
	state->in_paragraph = FALSE;
	/*
	 * clear the alignment stack.
	 */
	while (lo_PopAlignment(state) != NULL) {;}
	/*
	 * clear the list stack.
	 */
	while (lo_PopList(state, NULL) != NULL) {;}
	/*
	 * clear all open anchors.
	 */
	lo_PopAllAnchors(state);
	/*
	 * clear the font stack.
	 */
	while (state->font_stack->next != NULL)
	{
		(void)lo_PopFont(state, P_UNKNOWN);
	}
	state->base_font_size = DEFAULT_BASE_FONT_SIZE;
	state->font_stack->text_attr->size = state->base_font_size;
	state->current_anchor = NULL;
	state->current_ele = NULL;
	state->current_table = NULL;
	state->current_cell = NULL;
	state->current_java = NULL;
	state->current_multicol = NULL;
	state->must_relayout_subdoc = FALSE;
	state->allow_percent_width = TRUE;
	state->allow_percent_height = TRUE;
	state->is_a_subdoc = SUBDOC_NOT;
	state->current_subdoc = 0;

	/*
	 * Escape an open script
	 */
	if (top_state->in_script != SCRIPT_TYPE_NOT)
	{
		top_state->in_script = SCRIPT_TYPE_NOT;
		state->line_buf_len = 0;
		lo_UnblockLayout(context, top_state);
	}

	/*
	 * Reset a few top_state variables.
	 */
	top_state->current_map = NULL;
	top_state->in_nogrids = FALSE;
    top_state->ignore_tag_nest_level = 0;
    top_state->ignore_layer_nest_level = 0;
	top_state->in_applet = FALSE;
}


void
lo_free_layout_state_data(MWContext *context, lo_DocState *state)
{
	/*
	 * Short circuit out if the state was freed out from
	 * under us.
	 */
	if (state == NULL)
	{
		return;
	}

	if (state->current_table != NULL)
	{
		lo_FreePartialTable(context, state, state->current_table);
		state->current_table = NULL;
	}

	if (state->left_margin_stack != NULL)
	{
		lo_MarginStack *mptr;
		lo_MarginStack *margin;

		mptr = state->left_margin_stack;
		while (mptr != NULL)
		{
			margin = mptr;
			mptr = mptr->next;
			XP_DELETE(margin);
		}
		state->left_margin_stack = NULL;
	}
	if (state->right_margin_stack != NULL)
	{
		lo_MarginStack *mptr;
		lo_MarginStack *margin;

		mptr = state->right_margin_stack;
		while (mptr != NULL)
		{
			margin = mptr;
			mptr = mptr->next;
			XP_DELETE(margin);
		}
		state->right_margin_stack = NULL;
	}

	if (state->line_list != NULL)
	{
		LO_Element *eptr;
		LO_Element *element;

		eptr = state->line_list;
		while (eptr != NULL)
		{
			element = eptr;
			eptr = eptr->lo_any.next;
			lo_FreeElement(context, element, TRUE);
		}
		state->line_list = NULL;
	}

	if (state->font_stack != NULL)
	{
		lo_FontStack *fstack;
		lo_FontStack *fptr;

		fptr = state->font_stack;
		while (fptr != NULL)
		{
			fstack = fptr;
			fptr = fptr->next;
			XP_DELETE(fstack);
		}
		state->font_stack = NULL;
	}

	if (state->align_stack != NULL)
	{
		lo_AlignStack *aptr;
		lo_AlignStack *align;

		aptr = state->align_stack;
		while (aptr != NULL)
		{
			align = aptr;
			aptr = aptr->next;
			XP_DELETE(align);
		}
		state->align_stack = NULL;
	}

	if (state->list_stack != NULL)
	{
		lo_ListStack *lptr;
		lo_ListStack *list;

		lptr = state->list_stack;
		while (lptr != NULL)
		{
			list = lptr;
			lptr = lptr->next;
			XP_DELETE(list);
		}
		state->list_stack = NULL;
	}

    if (state->line_height_stack != NULL)
    {
        lo_LineHeightStack *lptr;
        lo_LineHeightStack *height;
        
        lptr = state->line_height_stack;
        while (lptr != NULL) {
            height = lptr;
            lptr = lptr->next;
            XP_DELETE(height);
        }
        state->line_height_stack = NULL;
    }
            
	if (state->line_buf != NULL)
	{
		PA_FREE(state->line_buf);
		state->line_buf = NULL;
	}
}


/* Clean up the layout data structures. This used to be inside lo_FinishLayout,
 * but we needed to call it from the editor, without all of the UI code that
 * lo_FinishLayout also calls.
 */

void
lo_FreeLayoutData(MWContext *context, lo_DocState *state)
{
	int32 i, cnt;

	/*
	 * Clean out the lo_DocState
	 */
	lo_free_layout_state_data(context, state);

	/*
	 * Short circuit out if the state was freed out from
	 * under us.
	 */
	if (state == NULL)
	{
		return;
	}

	LO_LockLayout();

	/*
	 * These are tags that were blocked instead of laid out.
	 * If there was an image in here, we already started the
	 * layout of it, we better free it now.
	 */
	if (state->top_state->tags != NULL)
	{
		PA_Tag *tptr;
		PA_Tag *tag;

		tptr = state->top_state->tags;
		while (tptr != NULL)
		{
			tag = tptr;
			tptr = tptr->next;
			if (((tag->type == P_IMAGE)||
			     (tag->type == P_NEW_IMAGE))&&
			    (tag->lo_data != NULL))
			{
				LO_Element *element;
				element = (LO_Element *)tag->lo_data;
				lo_FreeElement(context, element, TRUE);
				tag->lo_data = NULL;
			}
			PA_FreeTag(tag);
		}
		state->top_state->tags = NULL;
		state->top_state->tags_end = &state->top_state->tags;
		for (i = 0; i < MAX_INPUT_WRITE_LEVEL; i++)
			state->top_state->input_write_point[i] = NULL;
	}

	/*
	 * If we were blocked on and image when we left this page, we need to
	 * free up that partially loaded image structure.
	 */
	if (state->top_state->layout_blocking_element != NULL)
	{
		LO_Element *element;

		element = state->top_state->layout_blocking_element;
		lo_FreeElement(context, element, TRUE);
		state->top_state->layout_blocking_element = NULL;
	}

	/* Delete object stack */ 
    if (state->top_state->object_stack != NULL)
    {
        lo_ObjectStack* top;
        lo_ObjectStack* object;
        
        top = state->top_state->object_stack;
        while (top != NULL)
        {
            object = top;
            top = top->next;
			lo_DeleteObjectStack(object);
        }
        state->top_state->object_stack = NULL;
    }

	/* Delete object cache */
	if (state->top_state->object_cache != NULL)
    {
        lo_ObjectStack* top;
        lo_ObjectStack* object;
        
        top = state->top_state->object_cache;
        while (top != NULL)
        {
            object = top;
            top = top->next;
			lo_DeleteObjectStack(object);
        }
        state->top_state->object_cache = NULL;
    }

	LO_UnlockLayout();

	/*
	 * now we're done laying out the entire document. we had saved all
	 * the layout structures from the previous document so we're throwing
	 * away unused ones.
	 */
	cnt = lo_FreeRecycleList(context, state->top_state->recycle_list);
#ifdef MEMORY_ARENAS
	if (state->top_state->current_arena != NULL)
	{
		cnt = lo_FreeMemoryArena(state->top_state->current_arena->next);
		state->top_state->current_arena->next = NULL;
	}
#endif /* MEMORY_ARENAS */
	state->top_state->recycle_list = NULL;
}


void
lo_CloseMochaWriteStream(lo_TopState *top_state, int mocha_event)
{
	NET_StreamClass *stream = top_state->mocha_write_stream;

	if (stream != NULL)
	{
		top_state->mocha_write_stream = NULL;
		if (mocha_event == EVENT_LOAD)
		{
			stream->complete(stream);
		}
		else
		{
			stream->abort(stream, top_state->layout_status);
		}
		XP_DELETE(stream);
	}
}



void
lo_FinishLayout_OutOfMemory(MWContext *context, lo_DocState *state)
{
	lo_TopState *top_state;
	lo_DocState *main_state;
	lo_DocState *sub_state;

	/* Reset state for force loading images. */
	LO_SetForceLoadImage(NULL, FALSE);

	/*
	 * Short circuit out if the state was freed out from
	 * under us.
	 */
	if (state == NULL)
	{
		return;
	}

	/*
	 * Check for dangling sub-states, and clean them up if they exist.
	 */
	top_state = state->top_state;
	main_state = top_state->doc_state;
	sub_state = lo_TopSubState(top_state);
	while ((sub_state != NULL)&&(sub_state != main_state))
	{
		state = main_state;
		while ((state != NULL)&&(state->sub_state != sub_state))
		{
			state = state->sub_state;
		}
		if (state != NULL)
		{
			lo_free_layout_state_data(context, sub_state);
			XP_DELETE(sub_state);
			state->sub_state = NULL;
		}
		sub_state = lo_TopSubState(top_state);
	}
	state = lo_TopSubState(top_state);

	lo_CloseMochaWriteStream(top_state, EVENT_ABORT);

	/* Could be left over if someone forgets to close a tag */
	if (state->top_state->scriptData)
		lo_DestroyScriptData(state->top_state->scriptData);

	/*
	 * Short circuit out if the state was freed out from
	 * under us.
	 */
	if (state == NULL)
	{
		return;
	}

	lo_free_layout_state_data(context, state);
}


void
lo_FinishLayout(MWContext *context, lo_DocState *state, int32 mocha_event)
{
	lo_TopState *top_state;
	lo_DocState *main_state;
	lo_DocState *sub_state;

    /* Close any layers (blocks) that were opened in this document, in
	   case the document author forgot to. */
    if (state != NULL ) {
        if (state->layer_nest_level) {
            while (state->layer_nest_level > 0)
                lo_EndLayer(context, state, PR_TRUE);
            /* 
             * This call ensures that we flush the line list (which still might
             * have something on it if we had unclosed ILAYERs).
             */
            lo_CloseOutLayout(context, state); 
        }
    }

	if ((state != NULL) &&
		(state->top_state != NULL))
	{
		lo_CloseMochaWriteStream(state->top_state, mocha_event);

		/* Could be left over if someone forgets to close a tag */
		if (state->top_state->scriptData)
			lo_DestroyScriptData(state->top_state->scriptData);

        /* Handle the page background for the case of a document that contains 
           no displayed layout elements. */
        if (state->top_state->nothing_displayed != FALSE)
            lo_use_default_doc_background(context, state);
	}

        if (state && state->top_state)
            ET_SendLoadEvent(context, mocha_event, NULL, NULL, 
                             LO_DOCUMENT_LAYER_ID, 
                             state->top_state->resize_reload);
        else
            ET_SendLoadEvent(context, mocha_event, NULL, NULL, 
                             LO_DOCUMENT_LAYER_ID, FALSE);

	/* Reset state for force loading images. */
	LO_SetForceLoadImage(NULL, FALSE);

	/*
	 * Short circuit out if the state was freed out from
	 * under us.
	 */
	if (state == NULL)
	{
		FE_SetProgressBarPercent(context, 100);
#ifdef OLD_MSGS
		FE_Progress(context, "Layout Phase Complete");
#endif /* OLD_MSGS */

        /* Flush out layer callbacks so that document dimensions are correct. */
        if (context->compositor)
            CL_CompositeNow(context->compositor);

		FE_FinishedLayout(context);
		return;
	}

	/*
	 * Check for dangling sub-states, and clean them up if they exist.
	 */
	top_state = state->top_state;
	main_state = top_state->doc_state;
	sub_state = lo_TopSubState(top_state);
	while ((sub_state != NULL)&&(sub_state != main_state))
	{
		state = main_state;
		while ((state != NULL)&&(state->sub_state != sub_state))
		{
			state = state->sub_state;
		}
		if (state != NULL)
		{
			lo_free_layout_state_data(context, sub_state);
			XP_DELETE(sub_state);
			state->sub_state = NULL;
		}
		sub_state = lo_TopSubState(top_state);
	}
	state = lo_TopSubState(top_state);

	/*
	 * Short circuit out if the state was freed out from
	 * under us.
	 */
	if (state == NULL)
	{
		FE_SetProgressBarPercent(context, 100);
#ifdef OLD_MSGS
		FE_Progress(context, "Layout Phase Complete");
#endif /* OLD_MSGS */

        /* Flush out layer callbacks so that document dimensions are correct. */
        if (context->compositor)
            CL_CompositeNow(context->compositor);

		FE_FinishedLayout(context);
		return;
	}

    lo_FreeLayoutData(context, state);

	if (state->display_blocked != FALSE)
	{
		int32 y;

		if (state->top_state->name_target != NULL)
		{
			XP_FREE(state->top_state->name_target);
			state->top_state->name_target = NULL;
		}
		state->display_blocking_element_id = 0;
		state->display_blocked = FALSE;
		y = state->display_blocking_element_y;
		state->display_blocking_element_y = 0;
		state->y += state->win_bottom;

		LO_SetDocumentDimensions(context, state->max_width, state->y);

		if (y != state->win_top)
		{
			y = state->y - state->win_height;
			if (y < 0)
			{
				y = 0;
			}
		}
		FE_SetDocPosition(context, FE_VIEW, 0, y);

        if (context->compositor)
        {
            XP_Rect rect;
            
            rect.left = 0;
            rect.top = y;
            rect.right = state->win_width;
            rect.bottom = y + state->win_height;
            CL_UpdateDocumentRect(context->compositor,
                                  &rect, (PRBool)FALSE);
        }
	}
	else
	{
		state->y += state->win_bottom;
		LO_SetDocumentDimensions(context, state->max_width, state->y);
	}

	/* get rid of the style stack data since it wont
	 * be needed any more
	 */
	if(top_state->style_stack)
	{
		/* LJM: Need to purge stack to free memory
		 */
		STYLESTACK_Purge(top_state->style_stack);
	}

	if(!state->top_state->is_binary)
		FE_SetProgressBarPercent(context, 100);

#ifdef OLD_MSGS
	FE_Progress(context, "Layout Phase Complete");
#endif /* OLD_MSGS */

    /* Flush out layer callbacks so that document dimensions are correct. */
    if (context->compositor)
        CL_CompositeNow(context->compositor);

	if (state->is_a_subdoc == SUBDOC_NOT && state->top_state && !state->top_state->have_title)
	{
		/* We never got a title. We better notify the FE that the title is blank */
		
		FE_SetDocTitle(context, "");
	}		

	FE_FinishedLayout(context);
#ifdef EDITOR
	if (EDT_IS_EDITOR (context))
		EDT_FinishedLayout(context);
#endif

    /*
     * Update the blink layers to reflect the current positions of their
     * associated text elements.
     */
    if (context->compositor)
        lo_UpdateBlinkLayers(context);
}

/*******************************************************************************
 * LO_ProcessTag helper routines
 ******************************************************************************/

#ifdef MEMORY_ARENAS 
void
lo_GetRecycleList(MWContext* context, int32 doc_id, pa_DocData* doc_data, 
				  LO_Element* *recycle_list, lo_arena* *old_arena)
#else
void
lo_GetRecycleList(MWContext* context, int32 doc_id, pa_DocData* doc_data, 
				  LO_Element* *recycle_list)
#endif /* MEMORY_ARENAS */
{
	lo_TopState* old_top_state;
	lo_DocState* old_state;

	LO_LockLayout();

	/*
	 * Free up any old document displayed in this
	 * window.
	 */
	old_top_state = lo_FetchTopState(XP_DOCID(context));
	if (old_top_state == NULL)
	{
		old_state = NULL;
	}
	else
	{
		old_state = old_top_state->doc_state;
	}
	if (old_state != NULL)
	{
#ifdef MEMORY_ARENAS
		*old_arena = old_top_state->first_arena;
#endif /* MEMORY_ARENAS */
		lo_SaveFormElementState(context, old_state, TRUE);
		/*
		 * If this document has no session history
		 * there is no place to save this data.
		 * It will be freed by history cleanup.
		 */
		if (SHIST_GetCurrent(&context->hist) == NULL)
		{
			lo_TopState *top_state = old_state->top_state;
			top_state->savedData.FormList = NULL;
			top_state->savedData.EmbedList = NULL;
			top_state->savedData.Grid = NULL;
		}
		*recycle_list =
			lo_InternalDiscardDocument(context, old_state, doc_data, TRUE);
	}

	/* Now that we've discarded the old document, set the new doc_id. */
	XP_SET_DOCID(context, doc_id);
	LO_UnlockLayout();

}

intn
lo_ProcessTag_OutOfMemory(MWContext* context, LO_Element* recycle_list, lo_TopState* top_state)
{
	int32 cnt;
	
	if ((top_state != NULL)&&(top_state->doc_state != NULL))
	{
		lo_FinishLayout_OutOfMemory(context, top_state->doc_state);
	}

	cnt = lo_FreeRecycleList(context, recycle_list);

#ifdef MEMORY_ARENAS
	if ((top_state != NULL)&&(top_state->first_arena != NULL))
	{
		cnt = lo_FreeMemoryArena(top_state->first_arena->next);
		top_state->first_arena->next = NULL;
	}
#endif /* MEMORY_ARENAS */

	return MK_OUT_OF_MEMORY;
}

#define RESTORE_SAVED_DATA(Type, var, doc_data, recycle_list, top_state) \
{ \
	var = (lo_Saved##Type##Data *) \
		(doc_data->url_struct->savedData.Type); \
	if (var == NULL) \
	{ \
		/* \
		 * A document we have never visited, insert a pointer \
		 * to a form list into its history, if it has one. \
		 */ \
		var = lo_NewDocument##Type##Data(); \
		if (SHIST_GetCurrent(&context->hist) != NULL) \
		{ \
			SHIST_SetCurrentDoc##Type##Data(context, (void *)var); \
		} \
		/* \
		 * Stuff it in the url_struct as well for \
		 * continuous loading documents. \
		 */ \
		doc_data->url_struct->savedData.Type = (void *)var; \
	} \
	else if (doc_data->from_net != FALSE) \
	{ \
		lo_FreeDocument##Type##Data(context, var); \
	} \
	 \
	/* \
	 * If we don't have a form list, something very bad \
	 * has happened.  Like we are out of memory. \
	 */ \
	if (var == NULL) \
		return lo_ProcessTag_OutOfMemory(context, recycle_list, top_state); \
	 \
	top_state->savedData.Type = var; \
}

/****************************************************************************
 ****************************************************************************
 **
 **  Beginning of Public Utility Functions
 **
 ****************************************************************************
 ****************************************************************************/


static void *hooked_data_object = NULL;
static intn hooked_status = 0;
static PA_Tag *hooked_tag = NULL;

/*************************************
 * Function: LO_ProcessTag
 *
 * Description: This function is the main layout entry point.
 *	New tags or tag lists are sent here along with the
 *	status as to where they came from.
 *
 * Params: A blank data object from the parser, right now it
 *	is parse document data.  The tag or list of tags to process.
 * 	and a status field as to why it came.
 *
 * Returns: 1 - Done ok, continuing.
 *	    0 - Done ok, stopping.
 *	   -1 - not done, error.
 *************************************/
intn
LO_ProcessTag(void *data_object, PA_Tag *tag, intn status)
{
	pa_DocData *doc_data;
	int32 doc_id;
	MWContext *context;
    CL_Layer *doc_layer = NULL, *body_layer = NULL;
	lo_TopState *top_state;
	lo_DocState *state;
	lo_DocState *orig_state;
	lo_DocState *up_state;

	/*
	 * If this call came from a hook feed
	 */
	if (hooked_data_object != NULL)
	{
		data_object = hooked_data_object;
		status = hooked_status;
		/*
		 * Ignore hooked calls with no tags.
		 */
		if (tag == NULL)
		{
			return(1);
		}
		if (hooked_tag != NULL)
		{
			tag->newline_count += hooked_tag->newline_count;
		}
	}

	doc_data = (pa_DocData *)data_object;
	doc_id = doc_data->doc_id;
	top_state = (lo_TopState *)(doc_data->layout_state);

    /* 
     * If this is an "inline" stream and we don't have a top_state
     * associated with it, it's targetted at a layer in
     * the current document. In this case, we need to get the 
     * top_state using the context and not directly from the doc_data.
     */
    if ((top_state == NULL) && (doc_data->is_inline_stream == TRUE)) {
        top_state = lo_FetchTopState(doc_id);
        doc_data->layout_state = top_state;
        if (top_state) {
            top_state->doc_data = doc_data;
            top_state->nurl = doc_data->url_struct;
        }
    }

	if (top_state == NULL)
	{
		state = NULL;
	}
	else
	{
		state = top_state->doc_state;
	}
	context = doc_data->window_id;
	
	/*
	 * if we get called with abort/complete then ignore oom condition
	 * and clean up
	 */
	if ((state != NULL)&&(state->top_state != NULL)&&
		(state->top_state->out_of_memory != FALSE)&&(tag != NULL))
	{
		lo_FinishLayout_OutOfMemory(context, state);
		return(MK_OUT_OF_MEMORY);
	}

	if ((tag != NULL)&&(context != NULL)&&(hooked_data_object == NULL)&&
		(HK_IsHook(HK_TAG, (void *)tag)))
	{
		char *tptr;
		char *in_str;
		char *out_str;
		XP_Bool delete_tag;
		XP_Bool replace_tag;

		out_str = NULL;
		delete_tag = FALSE;
		replace_tag = FALSE;
		if (tag->type == P_TEXT)
		{
			PA_LOCK (tptr, char *, tag->data);
			in_str = XP_ALLOC(tag->data_len + 1);
			if (in_str == NULL)
			{
				return(MK_OUT_OF_MEMORY);
			}
			XP_BCOPY(tptr, in_str, tag->data_len);
			in_str[tag->data_len] = '\0';
			PA_UNLOCK(tag->data);
		}
		else if (tag->type == P_UNKNOWN)
		{
			int32 start;

			PA_LOCK (tptr, char *, tag->data);
			in_str = XP_ALLOC(tag->data_len + 3);
			if (in_str == NULL)
			{
				return(MK_OUT_OF_MEMORY);
			}
			in_str[0] = '<';
			start = 1;
			if (tag->is_end != FALSE)
			{
				in_str[1] = '/';
				start = 2;
			}
			XP_BCOPY(tptr, (char *)(in_str + start), tag->data_len);
			in_str[tag->data_len + start] = '\0';
			PA_UNLOCK(tag->data);
		}
		else
		{
			XP_Bool free_tag_str;
			const char *tag_str;
			int32 start;

			free_tag_str = FALSE;
			tag_str = PA_TagString((int32)tag->type);
			if (tag_str == NULL)
			{
				tag_str = XP_STRDUP("UNKNOWN");
				if (tag_str == NULL)
				{
					return(MK_OUT_OF_MEMORY);
				}
				free_tag_str = TRUE;
			}
			start = XP_STRLEN(tag_str);

			PA_LOCK (tptr, char *, tag->data);
			in_str = XP_ALLOC(tag->data_len + start + 3);
			if (in_str == NULL)
			{
				if (free_tag_str != FALSE)
				{
					XP_FREE((char *)tag_str);
				}
				return(MK_OUT_OF_MEMORY);
			}
			in_str[0] = '<';
			in_str[1] = '\0';
			start += 1;
			if (tag->is_end != FALSE)
			{
				in_str[1] = '/';
				in_str[2] = '\0';
				start += 1;
			}
			XP_STRCAT(in_str, tag_str);
			XP_BCOPY(tptr, (char *)(in_str + start), tag->data_len);
			in_str[tag->data_len + start] = '\0';
			PA_UNLOCK(tag->data);
			if (free_tag_str != FALSE)
			{
				XP_FREE((char *)tag_str);
			}
		}
		if (HK_CallHook(HK_TAG, (void *)tag, XP_CONTEXTID(context),
			in_str, &out_str))
		{
			/*
			 * out_str of NULL means change nothing.
			 * Returning the same string passed in also
			 * changes nothing.
			 */
			if ((out_str != NULL)&&(out_str[0] == '\0'))
			{
				delete_tag = TRUE;
			}
			else if ((out_str != NULL)&&
				(XP_STRCMP(in_str, out_str) != 0))
			{
				replace_tag = TRUE;
			}
		}
		XP_FREE(in_str);

		if (delete_tag != FALSE)
		{
			PA_FreeTag(tag);
			tag = NULL;
			XP_FREE(out_str);
			return(1);
		}
		else if (replace_tag != FALSE)
		{
			intn ok;

			hooked_data_object = data_object;
			hooked_status = status;
			hooked_tag = tag;

			ok = PA_ParseStringToTags(context,
				out_str, XP_STRLEN(out_str),
				(void *)LO_ProcessTag);

			hooked_data_object = NULL;

			/*
			 * This tag has been replaced, and never happened.
			 */
			if (ok)
			{
				PA_FreeTag(tag);
				tag = NULL;
				XP_FREE(out_str);
				return(1);
			}
		}
		if (out_str != NULL)
		{
			XP_FREE(out_str);
		}
	}

	/*	
		Test to see if this is a new stream.
		If it is, kill the old one (NET_InterruptStream(url)),
		free its data, and continue.
	*/
	if (!state) { /* this is a new document being laid out */
		/* is there already a state for this context? */
		lo_TopState *top_state;
		lo_DocState *old_state;

		/* initialize PICS */
		PICS_Init(context);

		top_state = lo_FetchTopState(doc_id);
		if (top_state == NULL)
		{
			old_state = NULL;
		}
		else
		{
			old_state = top_state->doc_state;
		}

		if (old_state) {
			if (top_state->nurl) { /* is it still running? */
/* ALEKS				NET_InterruptStream (top_state->nurl); */
				/* We've done a complete "cancel" now. We shouldn't need to do any more
				work; we'll find this "old state" again and recycle it */
			}
		}
	}
		
	/*
	 * We be done.
	 */
	if (tag == NULL)
	{
        /* 
         * If this is an inline stream that is not the result of a
         * dynamic layer src change, then don't do anything. The
         * layer will be closed out by the closing </LAYER> tag
         */
        if (doc_data && doc_data->is_inline_stream && (!state || 
            (lo_InsideLayer(state) && !lo_IsCurrentLayerDynamic(state))))
            return 0;
        
		if (status == PA_ABORT)
		{
#ifdef OLD_MSGS
			FE_Progress (context, "Document Load Aborted");
#endif /* OLD_MSGS */
            lo_process_deferred_image_info(NULL);
			if (state != NULL)
			{
				state->top_state->layout_status = status;
			}
        /*
         * If this is an inline stream (the result of a dynamic
         * layer src change) close out the layer.
         */
        if (doc_data && doc_data->is_inline_stream)
            lo_FinishLayerLayout(context, state, EVENT_ABORT);
        else 
			lo_FinishLayout(context, state, EVENT_ABORT);
		}
		else if (status == PA_COMPLETE)
		{
			orig_state = state;
			state = lo_CurrentSubState(state);

            /*
             * If this is an inline stream (the result of a dynamic
             * layer src change) close out the layer.
             */
            if (doc_data && doc_data->is_inline_stream)
                lo_FinishLayerLayout(context, state, EVENT_LOAD);
            else 
			/*
			 * IF the document is complete, we need to flush
			 * out any remaining unfinished elements.
			 */
			if (state != NULL)
			{
				state->top_state->total_bytes = state->top_state->current_bytes;

				if (state->top_state->layout_blocking_element != NULL)
				{
#ifdef OLD_MSGS
				    FE_Progress(context, "Layout Phase Complete but Blocked");
#endif /* OLD_MSGS */
				}
				else
				{
				    /*
				     * makes sure we are at the bottom
				     * of everything in the document.
				     */
				    lo_CloseOutLayout(context, state);

#ifdef OLD_MSGS
				    FE_Progress(context, "Layout Phase Complete");
#endif /* OLD_MSGS */

				    lo_FinishLayout(context, state, EVENT_LOAD);
				}
				orig_state->top_state->layout_status = status;
			}
			else
			{
				LO_Element *recycle_list = NULL;
				int32 cnt;
				int32 width, height;
				int32 margin_width, margin_height;
#ifdef MEMORY_ARENAS
				lo_arena *old_arena = NULL;

				lo_GetRecycleList(context, doc_id, doc_data,
						  &recycle_list, &old_arena);
#else
				lo_GetRecycleList(context, doc_id, doc_data,
						  &recycle_list);
#endif /* MEMORY_ARENAS */
				margin_width = 0;
				margin_height = 0;
 
                /* Because Mocha is in a separate thread, the
                   complete destruction of contexts is deferred until
                   Mocha cleans up its data structures.  (See
                   ET_RemoveWindowContext()).  If this context was
                   previously a frameset context, it may still have grid
                   children that haven't been deleted yet.  Pull them
                   off the grid_children list so that we don't mistakenly
                   conclude that this context is a frameset context. */
                if (context->grid_children) {
                    XP_ListDestroy (context->grid_children);
                    context->grid_children = 0;
                }
				FE_LayoutNewDocument(context,
						     doc_data->url_struct,
						     &width, &height,
						     &margin_width,
						     &margin_height);

                if(context && doc_data && doc_data->url_struct) {
                    /*  Tell XP NavCenter API about the new document.
                     */
                    XP_SetNavCenterUrl(context, doc_data->url_struct->address);
                }

				/* see first lo_FreeRecycleList comment. safe */
				cnt = lo_FreeRecycleList(context, recycle_list);
#ifdef MEMORY_ARENAS
				cnt = lo_FreeMemoryArena(old_arena);
#endif /* MEMORY_ARENAS */

				FE_SetDocPosition(context, FE_VIEW, 0, 0);
				FE_SetDocDimension(context, FE_VIEW, 1, 1);

#ifdef OLD_MSGS
				FE_Progress(context, "Layout Phase Complete");
#endif /* OLD_MSGS */
				lo_FinishLayout(context, state, EVENT_LOAD);
			}
		}

		/*
		 * Since lo_FinishLayout make have caused state to be
		 * freed if it was a dangling sub_state, we need to
		 * reinitialize state here so it is safe to use it.
		 */
		state = lo_TopSubState(top_state);

		if (state != NULL)
		{
			/* done with netlib stream */
			state->top_state->nurl = NULL;
			state->top_state->doc_data = NULL;
		}


		/*
		 * Make sure we are done with all images now.
		 */
		lo_NoMoreImages(context);

		return(0);
	}

	/*
	 * Check if this is a new document being laid out
	 */
	if (top_state == NULL)
	{
		int32 width, height;
		int32 margin_width, margin_height;
		lo_SavedFormListData *form_list;
		lo_SavedEmbedListData *embed_list;
		lo_SavedGridData *grid_data;
		LO_Element *recycle_list = NULL;
		int32 start_element;

#ifdef MEMORY_ARENAS 
		lo_arena *old_arena = NULL;

		lo_GetRecycleList(context, doc_id, doc_data, &recycle_list, &old_arena);	/* whh */
#else
		lo_GetRecycleList(context, doc_id, doc_data, &recycle_list);	/* whh */
#endif /* MEMORY_ARENAS */

#ifdef LOCAL_DEBUG
XP_TRACE(("Initializing new doc %d\n", doc_id));
#endif /* DEBUG */
		width = 1;
		height = 1;
		margin_width = 0;
		margin_height = 0;
		/*
		 * Grid cells can set their own margin dimensions.
		 */
		if (context->is_grid_cell != FALSE)
		{
			lo_GetGridCellMargins(context,
				&margin_width, &margin_height);
		}

        /* Because Mocha is in a separate thread, the complete
           destruction of contexts is deferred until Mocha cleans up
           its data structures.  (See ET_RemoveWindowContext()).  If
           this context was previously a frameset context, it may
           still have grid children that haven't been deleted yet.
           Pull them off the grid_children list so that we don't
           mistakenly conclude that this context is a frameset
           context. */
        if (context->grid_children) {
            XP_ListDestroy (context->grid_children);
            context->grid_children = 0;
        }
		FE_LayoutNewDocument(context, doc_data->url_struct,
			&width, &height, &margin_width, &margin_height);

        if(context && doc_data && doc_data->url_struct) {
            /*  Tell XP NavCenter API about the new document.
             */
            XP_SetNavCenterUrl(context, doc_data->url_struct->address);
        }

        if (context->compositor) {
            lo_CreateDefaultLayers(context, &doc_layer, &body_layer);
            CL_ScrollCompositorWindow(context->compositor, 0, 0);
            CL_SetCompositorEnabled(context->compositor, PR_TRUE);
        }
		
		top_state = lo_NewTopState(context, doc_data->url);
		if ((context->compositor && (!doc_layer || !body_layer)) ||
                    top_state == NULL)
		{
#ifdef MEMORY_ARENAS
			if (old_arena != NULL)
			{
				int32 cnt;

				cnt = lo_FreeMemoryArena(old_arena);
			}
#endif /* MEMORY_ARENAS */
			return lo_ProcessTag_OutOfMemory(context, recycle_list, top_state);
		}
#ifdef MEMORY_ARENAS
        if ( ! EDT_IS_EDITOR(context) ) {
		    top_state->first_arena->next = old_arena;
        }
#endif /* MEMORY_ARENAS */

		/* take a special hacks from URL_Struct */
		top_state->auto_scroll =(intn)doc_data->url_struct->auto_scroll;
		top_state->is_binary = doc_data->url_struct->is_binary;
		/*
		 * Security dialogs only for FO_PRESENT docs.
		 */
		if (CLEAR_CACHE_BIT(doc_data->format_out) == FO_PRESENT)
		{
			top_state->security_level =
				doc_data->url_struct->security_on;
		}
		else
		{
			top_state->security_level = 0;
		}
        top_state->doc_layer = doc_layer;
        top_state->body_layer = body_layer;
		top_state->force_reload = doc_data->url_struct->force_reload;
		top_state->nurl = doc_data->url_struct;
		top_state->total_bytes = doc_data->url_struct->content_length;
		top_state->doc_data = doc_data;
		top_state->resize_reload = doc_data->url_struct->resize_reload;

		if (top_state->auto_scroll != 0)
		{
			top_state->scrolling_doc = TRUE;
			if (top_state->auto_scroll < 1)
			{
				top_state->auto_scroll = 1;
			}
			if (top_state->auto_scroll > 1000)
			{
				top_state->auto_scroll = 1000;
			}
		}

		/*
		 * give the parser a pointer to the layout data so we can
		 * get to it next time
		 */
		doc_data->layout_state = top_state;

		/*
		 * Add this new top state to the global state list.
		 */
		if (lo_StoreTopState(doc_id, top_state) == FALSE)
		{
			return lo_ProcessTag_OutOfMemory(context, recycle_list,
							 top_state);
		}

#ifdef EDITOR
		/* LTNOTE: maybe this should be passed into New_Layout.
		 * For now, it is not.
		 */
		top_state->edit_buffer = doc_data->edit_buffer;
#endif
		state = lo_NewLayout(context, width, height,
			margin_width, margin_height, NULL);
		if (state == NULL)
			return lo_ProcessTag_OutOfMemory(context, recycle_list, top_state);
		top_state->doc_state = state;

        /* The DOCUMENT layer's height is always the height of the
           window, at least for the purpose of computing percentage
           heights in child layers. */
        top_state->layers[LO_DOCUMENT_LAYER_ID]->height = 
            state->win_top + state->win_height + state->win_bottom;

        /* Reset the scoping for any JavaScript to be the top-level layer. */
        ET_SetActiveLayer(context, LO_DOCUMENT_LAYER_ID);

		/* RESTORE style stack 
		 */
		if(top_state->resize_reload)
		{
			History_entry *he = SHIST_GetCurrent(&context->hist);
			if (he)
			{
				if(he->savedData.style_stack)
				{
					if(top_state->style_stack)
						STYLESTACK_Delete(top_state->style_stack);
					top_state->style_stack = he->savedData.style_stack;
					he->savedData.style_stack = NULL;
				}
			}
		}
		
		/*
		 * Take care of forms.
		 * If we have been here before, we can pull the old form
		 * data out of the URL_Struct.  It may be invalid if the
		 * form was interrupted.
		 */
		RESTORE_SAVED_DATA(FormList, form_list, doc_data, recycle_list, top_state);
		if (!form_list->valid)
			lo_FreeDocumentFormListData(context, form_list);
		form_list->data_index = 0;

		/*
		 * Take care of embeds.
		 * If we have been here before, we can pull the old embed
		 * data out of the URL_Struct.
		 */
		RESTORE_SAVED_DATA(EmbedList, embed_list, doc_data, recycle_list, top_state);

		/*
		 * Take care of saved grid data.
		 * If we have been here before, we can pull the old
		 * grid data out of the URL_Struct.
		 */
		RESTORE_SAVED_DATA(Grid, grid_data, doc_data, recycle_list, top_state);

		/* XXX just in case a grid had onLoad or onUnload handlers */
		if (!doc_data->url_struct->resize_reload)
		{
			lo_RestoreContextEventHandlers(context, state, tag,
					    &doc_data->url_struct->savedData);
		}

		top_state->recycle_list = recycle_list;

		if (state->top_state->name_target != NULL)
		{
			state->display_blocking_element_id = -1;
		}

		start_element = doc_data->url_struct->position_tag;
		if (start_element == 1)
		{
			start_element = 0;
		}

		if (start_element > 0)
		{
			state->display_blocking_element_id = start_element;
		}

		if ((state->display_blocking_element_id > state->top_state->element_id)||
			(state->display_blocking_element_id  == -1))
		{
			state->display_blocked = TRUE;
		}
		else
		{
			FE_SetDocPosition(context, FE_VIEW, 0, state->base_y);
		}

		if ((Master_backdrop_url != NULL)&&(UserOverride != FALSE)
			&&(context->type != MWContextDialog))
		{
			lo_load_user_backdrop(context, state);
		}
	}

	if (state == NULL)
	{
		return(-1);
	}

    /* 
     * If we're processing an inline stream, we need to make a note of it
     * so that the tag goes into the correct place.
     */
    if (doc_data && doc_data->is_inline_stream && 
        (top_state->doc_data != doc_data)) 
    {
	    if (top_state->input_write_level >= MAX_INPUT_WRITE_LEVEL-1) {
		    top_state->out_of_memory = TRUE;
			return (MK_OUT_OF_MEMORY);
		}
        top_state->input_write_level++;
        top_state->tag_from_inline_stream = TRUE;
    }
    
	if (tag->data != NULL)
	{
		state->top_state->current_bytes += tag->data_len;
	}

	/*
	 * Divert all tags to the current sub-document if there is one.
	 */
	up_state = NULL;
	orig_state = state;
	while (state->sub_state != NULL)
	{
		up_state = state;
		state = state->sub_state;
	}

	orig_state->top_state->layout_status = status;

	/*
	 * The filter checks for all the NOFRAMES, NOEMBED, NOSCRIPT, and
	 * the APPLET ignore setup.
	 */
	if (top_state->layout_blocking_element != NULL)
	{
		lo_BlockTag(context, state, tag);
	}
	else if (lo_FilterTag(context, state, tag) == FALSE)
	{
		PA_FreeTag(tag);
	}
	else
	{
		lo_DocState *tmp_state;
		Bool may_save;
		int32 state_diff;

		if ((state->is_a_subdoc == SUBDOC_CELL)||
			(state->is_a_subdoc == SUBDOC_CAPTION))
		{
			may_save = TRUE;
		}
		else
		{
			may_save = FALSE;
		}

		/*
		 * Reset these so we can tell if anything happened
		 */
		top_state->diff_state = FALSE;
		top_state->state_pushes = 0;
		top_state->state_pops = 0;
		
		lo_LayoutTag(context, state, tag);

		if (top_state->wedged_on_mocha) {
			top_state->wedged_on_mocha = FALSE;
			return(1);
		}

		tmp_state = lo_CurrentSubState(orig_state);

		/* how has our state level changed? */
		state_diff = top_state->state_pushes - top_state->state_pops;
		
		if (may_save != FALSE)
		{
			/*
			 * Store the tag in the correct subdoc.
			 */
			if (state_diff == -1)
			{
				/*
				 * That tag popped us up one state level.  If this new
				 * state is still a subdoc, save the tag there.
				*/
				if ((tmp_state->is_a_subdoc == SUBDOC_CELL)||
				    (tmp_state->is_a_subdoc == SUBDOC_CAPTION))
				{
                    /* if we just popped a table we need to insert
                     * a dummy end tag to pop the dummy start tag
                     * we shove on the stack after createing a table
                     */
   					PA_Tag *new_tag = LO_CreateStyleSheetDummyTag(tag);
					if(new_tag)
					{
						lo_SaveSubdocTags(context, tmp_state, new_tag);
					}

				    lo_SaveSubdocTags(context, tmp_state, tag);
				}
				else
				{
				    PA_FreeTag(tag);
				}
			}
			else if (( up_state != NULL ) &&
				( top_state->diff_state != FALSE ) &&
				( state_diff == 0 ))
			{
				/*
				 * We have returned to the same state level but
				 * we have a different subdoc.
				 */
				if ((up_state->is_a_subdoc == SUBDOC_CELL)||
				     (up_state->is_a_subdoc == SUBDOC_CAPTION))
				{
				    lo_SaveSubdocTags(context, up_state, tag);
				}
				else
				{
				    PA_FreeTag(tag);
				}
			}
			else if (( top_state->diff_state == FALSE ) &&
				( state_diff == 0 ))
			{
				/*
				 * We are still in the same subdoc
				 */
				lo_SaveSubdocTags(context, state, tag);
			}
			else if (( state_diff == 1 ))
			{
				PA_Tag *new_tag;

				/*
				 * That tag started a new, nested subdoc.
				 * Add the starting tag to the parent.
				 */
				lo_SaveSubdocTags(context, state, tag);
				/*
				 * Since we have extended the parent chain,
				 * we need to reset the child to the new
				 * parent end-chain.
				 */
				if ((tmp_state->is_a_subdoc == SUBDOC_CELL)||
				    (tmp_state->is_a_subdoc == SUBDOC_CAPTION))
				{
					tmp_state->subdoc_tags =
						state->subdoc_tags_end;

					/* add an aditional dummy tag so that style sheets
					 * can use it to query styles from for this entry
					 * that created a table
					 */
					new_tag = LO_CreateStyleSheetDummyTag(tag);
					if(new_tag)
					{
						lo_SaveSubdocTags(context, tmp_state, new_tag);
					}
				}
			}
			/*
			 * This can never happen.
			 */
			else
			{
				PA_FreeTag(tag);
			}

			state = tmp_state;
		}
		else
		{
			if(state_diff == 1)
			{
				/* everytime a table is started we need to save the
				 * tag that created it so that style sheets can apply
				 * styles to it.  So we will save even in the "dont save"
				 * case
				 */
				PA_Tag *new_tag = LO_CreateStyleSheetDummyTag(tag);
				if(new_tag)
				{
					/* We just pushed another table into a previous table 
					 * we need to do some table magic to make the subdoc
					 * list work right
					 */
					lo_SaveSubdocTags(context, tmp_state, new_tag);
				}
			}                                            
            else if(state_diff == -1)
            {
                /* if we just popped a table we need to insert
                 * a dummy end tag to pop the dummy start tag
                 * we shove on the stack after createing a table
                 */
   				PA_Tag *new_tag = LO_CreateStyleSheetDummyTag(tag);
				if(new_tag)
				{
					lo_SaveSubdocTags(context, tmp_state, new_tag);
				}
            }

			PA_FreeTag(tag);
		}
	}

    if (doc_data && doc_data->is_inline_stream && 
        (top_state->doc_data != doc_data))
    {
        top_state->input_write_level--;
    }
    top_state->tag_from_inline_stream = FALSE;

	if (state->top_state->out_of_memory != FALSE)
	{
		lo_FinishLayout_OutOfMemory(context, state);
		return(MK_OUT_OF_MEMORY);
	}

	/*
	 * The parser (libparse/pa_parse.c) only calls us with these status
	 * codes when tag is null, and we handled that case far above (in fact,
	 * we will crash less far above on a null tag pointer, so we can't get
	 * here when aborting or completing).
	 */
	XP_ASSERT(status != PA_ABORT && status != PA_COMPLETE);

	return(1);
}

void
lo_RefreshDocumentArea(MWContext *context, lo_DocState *state, int32 x, int32 y,
	uint32 width, uint32 height)
{
	LO_Element *eptr;
	int32 i;
	int32 top, bottom;

	if (state->display_blocked != FALSE)
	{
		return;
	}
	
	/*	
	 * Find which lines intersect the drawable region.
	 */
	lo_RegionToLines (context, state, x, y, width, height, FALSE,
		&top, &bottom);

	/*
	 * Display all these lines, if there are any.
	 */
	if (bottom >= 0)
	{
		for (i=top; i <= bottom; i++)
		{
			lo_DisplayLine(context, state, i,
				x, y, width, height);
		}
	}

	/*
	 * Check the floating element list.
	 */
	eptr = state->float_list;
	while (eptr != NULL)
	{
		/* I'm not sure exactly why we need to do this, but this code
           has been here, in some form or another, since this file was
           first created -- fur. */
	    if (eptr->type == LO_SUBDOC)
	    {
			LO_SubDocStruct *subdoc;
			lo_DocState *sub_state;
		
			subdoc = (LO_SubDocStruct *)eptr;
			sub_state = (lo_DocState *)subdoc->state;

			if (sub_state == NULL)
				break;

			if (sub_state->base_y < 0)
				sub_state->display_blocked = FALSE;
	    }
		lo_DisplayElement(context, eptr,
						  state->base_x, state->base_y,
						  x, y, width, height);
	    eptr = eptr->lo_any.next;
	}
}


/*************************************
 * Function: LO_RefreshArea
 *
 * Description: This is a public function that allows the front end
 *	to ask that a particulat portion of the document be refreshed.
 *	The layout then finds the relevant elements, and calls
 *	the appropriate front end functions to display those
 *	elements.
 *
 * Params: Window context, x, y position of upper left corner,
 *	and width X height of the rectangle.
 *
 * Returns: Nothing
 *************************************/
void
LO_RefreshArea(MWContext *context, int32 x, int32 y,
	uint32 width, uint32 height)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	
	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return;
	}
	state = top_state->doc_state;

	lo_RefreshDocumentArea(context, state, x, y, width, height);
}


/*************************************
 * Function: lo_GetLineEnds
 *
 * Description: This is a private function that returns the line ends for
 * a particular line.
 *
 * Params: Window context, line number.
 *
 * Returns: A pointer to the first and last elements in the line at that location,
 *	or NULLs if there is no matching line.
 *************************************/
void
lo_GetLineEnds(MWContext *context, lo_DocState *state,
	int32 line, LO_Element** retBegin, LO_Element** retEnd)
{
	LO_Element **line_array;

	*retBegin = NULL;
	*retEnd = NULL;

	/*
	 * No document state yet.
	 */
	if (state == NULL)
	{
		return;
	}

	if (line >= 0)
#ifdef XP_WIN16
	{
		intn a_size;
		intn a_indx;
		intn a_line;
		XP_Block *larray_array;

		a_size = SIZE_LIMIT / sizeof(LO_Element *);
		a_indx = (intn)(line / a_size);
		a_line = (intn)(line - (a_indx * a_size));

		XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
		state->line_array = larray_array[a_indx];
		XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
		*retBegin = line_array[a_line];

		if (line >= (state->line_num - 2))
		{
			*retEnd = NULL;
		}
		else
		{
			if ((a_line + 1) == a_size)
			{
				XP_UNLOCK_BLOCK(state->line_array);
				state->line_array = larray_array[a_indx + 1];
				XP_LOCK_BLOCK(line_array, LO_Element **,
					state->line_array);
				*retEnd = line_array[0];
			}
			else
			{
				*retEnd = line_array[a_line + 1];
			}
		}
		XP_UNLOCK_BLOCK(state->line_array);
		XP_UNLOCK_BLOCK(state->larray_array);
	}
#else
	{
		XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
		*retBegin = line_array[line];
		if (line == (state->line_num - 2))
		{
			*retEnd = NULL;
		}
		else
		{
			*retEnd = line_array[line + 1];
		}
		XP_UNLOCK_BLOCK(state->line_array);
	}
#endif /* XP_WIN16 */
}

Bool lo_IsDummyLayoutElement(LO_Element *ele)
{
	Bool isDummy = FALSE;
	switch(ele->lo_any.type)
	{
		case LO_PARAGRAPH:
		case LO_CENTER:
		case LO_MULTICOLUMN:
		case LO_FLOAT:
		case LO_TEXTBLOCK:
		case LO_LIST:
		case LO_DESCTITLE:
		case LO_DESCTEXT:
		case LO_BLOCKQUOTE:
		case LO_LAYER:
		case LO_HEADING:
		case LO_SPAN:
		case LO_DIV:
			isDummy = TRUE;
			break;
	}
	return isDummy;
}

/*************************************
 * Function: lo_XYToDocumentElement
 *
 * Description: This is a public function that allows the front end
 *	to ask for the particular element residing at the passed
 *	(x, y) position in the window.  This is very important
 *	for resolving button clicks on anchors.
 *
 * Params: Window context, x, y position of the window location.
 *
 * Returns: A pointer to the element record at that location,
 *	or NULL if there is no matching element.
 *************************************/
LO_Element *
lo_XYToDocumentElement2(MWContext *context, lo_DocState *state,
	int32 x, int32 y, Bool check_float, Bool into_cells, Bool into_ilayers,
    Bool editMode, int32 *ret_x, int32 *ret_y)
{
	Bool in_table, is_inflow_layer;
	int32 line;
	LO_Element *tptr, *eptr;
	LO_Element *end_ptr;
	LO_Element *tptrTable = NULL;
	LO_Element *tptrCell = NULL;

	/*
	 * No document state yet.
	 */
	if (state == NULL)
	{
		return(NULL);
	}

	tptr = NULL;

	line = lo_PointToLine(context, state, x, y);

#ifdef LOCAL_DEBUG
XP_TRACE(("lo_PointToLine says line %d\n", line));
#endif /* LOCAL_DEBUG */

    lo_GetLineEnds(context, state, line, &tptr, &end_ptr);

	in_table = FALSE;
	while (tptr != end_ptr)
	{
		int32 y2;
        int32 x1;
		int32 t_width, t_height;

		if (lo_IsDummyLayoutElement(tptr))
		{
			tptr = tptr->lo_any.next;
			continue;
		}

        x1 = tptr->lo_any.x + tptr->lo_any.x_offset;
 
		t_width = tptr->lo_any.width;
		/*
		 * Images need to account for border width
		 */
		if (tptr->type == LO_IMAGE)
		{
			t_width = t_width + (2 * tptr->lo_image.border_width);
            /*
             * If we're editing, images also account for their border space
             */
            if ( editMode )
            {
                t_width += 2 * tptr->lo_image.border_horiz_space;
                x1 -= tptr->lo_image.border_horiz_space;
            }
		}
		if (t_width <= 0)
		{
			t_width = 1;
		}

		t_height = tptr->lo_any.height;
		/*
		 * Images need to account for border width
		 */
		if (tptr->type == LO_IMAGE)
		{
			t_height = t_height + (2 * tptr->lo_image.border_width);

            /*
             * If we're editing, images also account for their border space
             */
            if ( editMode )
            {
                t_height += 2 * tptr->lo_image.border_vert_space;
            }
		}

        is_inflow_layer =
            (tptr->type == LO_CELL) && tptr->lo_cell.cell_inflow_layer;
                
		if (!in_table && !is_inflow_layer)
		{
			y2 = tptr->lo_any.y + tptr->lo_any.line_height;
			if (tptr->lo_any.line_height <= 0)
			{
				y2 = tptr->lo_any.y + tptr->lo_any.y_offset + t_height;
			}
		}
		else
		{
			y2 = tptr->lo_any.y + tptr->lo_any.y_offset +
				t_height;
		}
		if ((y >= tptr->lo_any.y)&&(y < y2)&&
			(x >= tptr->lo_any.x)&&
			(x < (x1 + t_width)))
		{
                        
			/*
			 * Tables are containers.  Don't stop on them,
			 * look inside them.
			 */
			if (tptr->type != LO_TABLE)
			{
                /* Inflow layers look just like table cells, but we
                   always look inside them, as if the layout elements
                   were part of the line's list of element's. */
                if (is_inflow_layer && into_ilayers) {
                    eptr = lo_XYToCellElement(context, state,
                                              (LO_CellStruct *)tptr, x, y,
                                              TRUE, into_cells, into_ilayers);
                    if (eptr) {
                        tptr = eptr;
                        break;
                    }

                    /* No matching element found inside cell, keep
                       looking on rest of line. */
                } else
                    break;
			}
			else
			{
				in_table = TRUE;
                /*
                 * Save the table element
                 *  to return if no other element is found
                */
                tptrTable = tptr;
			}
		}
		tptr = tptr->lo_any.next;
	}
	if (tptr == end_ptr)
	{
		tptr = NULL;
	}

	/*
	 * Check the floating element list.
	 */
	if ((tptr == NULL)&&(check_float != FALSE))
	{
		LO_Element *eptr;

		eptr = state->float_list;
		while (eptr != NULL)
		{
			int32 t_width, t_height;

			t_width = eptr->lo_any.width;
			/*
			 * Images need to account for border width
			 */
			if (eptr->type == LO_IMAGE)
			{
				t_width = t_width +
					(2 * eptr->lo_image.border_width);
			}
			if (t_width <= 0)
			{
				t_width = 1;
			}

			t_height = eptr->lo_any.height;
			/*
			 * Images need to account for border width
			 */
			if (eptr->type == LO_IMAGE)
			{
				t_height = t_height +
					(2 * eptr->lo_image.border_width);
			}

			if ((y >= eptr->lo_any.y)&&
				(y < (eptr->lo_any.y + eptr->lo_any.y_offset +
					t_height)))
			{
				if ((x >= eptr->lo_any.x)&&
					(x < (eptr->lo_any.x +
						eptr->lo_any.x_offset +
						t_width)))
				{
					/*
					 * Tables are containers.
					 * Don't stop on them,
					 * look inside them.
					 */
					if (eptr->type != LO_TABLE)
					{
						break;
					}
				}
			}
			eptr = eptr->lo_any.next;
		}
		tptr = eptr;
	}

	if ((tptr != NULL)&&(tptr->type == LO_SUBDOC))
	{
		LO_SubDocStruct *subdoc;
		int32 new_x, new_y;
		int32 ret_new_x, ret_new_y;

		subdoc = (LO_SubDocStruct *)tptr;

		new_x = x - (subdoc->x + subdoc->x_offset +
			subdoc->border_width);
		new_y = y - (subdoc->y + subdoc->y_offset +
			subdoc->border_width);
		tptr = lo_XYToDocumentElement(context,
			(lo_DocState *)subdoc->state, new_x, new_y, check_float,
			into_cells, into_ilayers, &ret_new_x, &ret_new_y);
		x = ret_new_x;
		y = ret_new_y;
	}
	else if ((tptr != NULL)&&(tptr->type == LO_CELL)&&(into_cells != FALSE))
	{
        /*
         * Save the cell element
         *  to return if no other element is found
        */
        tptrCell = tptr;
		tptr = lo_XYToCellElement(context, state,
			(LO_CellStruct *)tptr, x, y, TRUE, into_cells, into_ilayers);
	}

	*ret_x = x;
	*ret_y = y;

	return(tptr);
}

LO_Element *
lo_XYToDocumentElement(MWContext *context, lo_DocState *state,
    int32 x, int32 y, Bool check_float, Bool into_cells, Bool into_ilayers,
	int32 *ret_x, int32 *ret_y)
{
    return lo_XYToDocumentElement2(context, state, x, y, check_float, into_cells, into_ilayers, FALSE, ret_x, ret_y);
}

/*************************************
 * Function: LO_XYToElement
 *
 * Description: This is a public function that allows the front end
 *	to ask for the particular element residing at the passed
 *	(x, y) position in the window.  This is very important
 *	for resolving button clicks on anchors.
 *
 * Params: Window context, x, y position of the window location.
 *
 * Returns: A pointer to the element record at that location,
 *	or NULL if there is no matching element.
 *************************************/
LO_Element *
LO_XYToElement(MWContext *context, int32 x, int32 y, CL_Layer *layer)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_Element *tptr = NULL;
	int32 ret_x, ret_y;
    LO_CellStruct *layer_cell;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return(NULL);
	}
	state = top_state->doc_state;

    layer_cell = lo_GetCellFromLayer(context, layer);
    
    if (layer_cell != NULL) {
        tptr = lo_XYToCellElement(context,
                                  state,
                                  layer_cell,
                                  x, y, TRUE, TRUE, TRUE);
    }
    else
        tptr = lo_XYToDocumentElement(context, state, x, y, TRUE, TRUE, TRUE,
                                      &ret_x, &ret_y);

	if ((tptr == NULL)&&(top_state->is_grid != FALSE))
	{
		tptr = lo_XYToGridEdge(context, state, x, y);
	}

	/*
	 * Make sure we do not return an invisible edge.
	 */
	if ((tptr != NULL)&&(tptr->type == LO_EDGE)&&
		(tptr->lo_edge.visible == FALSE))
	{
		tptr = NULL;
	}
	return(tptr);
}

static void
lo_set_image_info(MWContext *context, int32 ele_id, int32 width, int32 height)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *main_doc_state;
	lo_DocState *state;
	LO_ImageStruct *image;

#ifdef EDITOR
	if ( ele_id == ED_IMAGE_LOAD_HACK_ID ){
		EDT_SetImageInfo( context, ele_id, width, height );
		return;
	}
#endif


	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return;
	}
	main_doc_state = top_state->doc_state;
	state = lo_CurrentSubState(main_doc_state);

	/*
	 * If the font stack has already been freed, we are too far gone
	 * to deal with delayed images coming in.  Ignore them.
	 */
	if (state->font_stack == NULL)
	{
		top_state->layout_blocking_element = NULL;
		return;
	}

	if ((top_state->layout_blocking_element != NULL)&&
		(top_state->layout_blocking_element->lo_any.ele_id == ele_id))
	{
		image = (LO_ImageStruct *)top_state->layout_blocking_element;

		if (image->image_attr->attrmask & LO_ATTR_BACKDROP)
		{
			return;
		}

		image->width = width;
		image->height = height;
		lo_FinishImage(context, state, image);
		lo_FlushBlockage(context, state, main_doc_state);
	}
	else if (top_state->tags != NULL)
	{
		PA_Tag *tag_ptr;

		tag_ptr = top_state->tags;
		while (tag_ptr != NULL)
		{
			if (((tag_ptr->type == P_IMAGE)||
				(tag_ptr->type == P_NEW_IMAGE))&&
				(tag_ptr->lo_data != NULL)&&
				(tag_ptr->is_end == FALSE))
			{
				image = (LO_ImageStruct *)tag_ptr->lo_data;
				if (image->ele_id == ele_id)
				{
					image->width = width;
					image->height = height;
					break;
				}
			}
			tag_ptr = tag_ptr->next;
		}
	}
}

typedef struct setImageInfoClosure_struct
{
    MWContext      *context;
    int32           ele_id;
    int32           width;
    int32           height;
    struct setImageInfoClosure_struct
                   *next;
} setImageInfoClosure;

static int8 deferred_list_busy = 0;
static int8 destroy_deferred_list = 0;

/* List of images for which image size info is available,
   but for which the layout process is deferred */                             
static setImageInfoClosure *image_info_deferred_list;
static void *deferred_image_info_timeout = NULL;

static void
lo_discard_unprocessed_deferred_images(MWContext *context)
{                                    
    setImageInfoClosure *c, *prev_c;

	prev_c = NULL;
	c = image_info_deferred_list;
    if (deferred_list_busy)
    {
        /* set a flag to cause the list to get destroyed
         * as soon as we are not reentrant
         */
        destroy_deferred_list = TRUE;
        return;
    }
    else
    {
        destroy_deferred_list = FALSE;
    }

    while (c) {
		if(c->context == context) {
			setImageInfoClosure *free_tmp;

			if(prev_c)
				prev_c->next = c->next;
			else
				image_info_deferred_list = c->next;

			free_tmp = c;
			c = c->next;
            XP_FREE(free_tmp);
		}
		else
		{
			prev_c = c;
			c = c->next;
		}
    }
}

/* Handle image dimension information that has been reported to
   layout since the last time we were called. */
static void
lo_process_deferred_image_info(void *closure)
{
    setImageInfoClosure *c, *next_c;

	/* Don't allow reentrant calls. */
	if (deferred_list_busy)
		return;

    deferred_list_busy = 1;
    for (c = image_info_deferred_list; c; c = next_c) {
        next_c = c->next;
        lo_set_image_info(c->context, c->ele_id, c->width, c->height);

        if(destroy_deferred_list)
        {
            /* discared the images for the context that just tried to
             * call lo_discard_unprocessed_deferred_images.  After
             * that continue to pump this list until empty.
             */
            image_info_deferred_list = c;
            deferred_list_busy = FALSE;
            lo_discard_unprocessed_deferred_images(image_info_deferred_list->context);
            deferred_list_busy = TRUE;
            next_c = image_info_deferred_list;
        }
        else
        {
            XP_FREE(c);
        }
    }
    deferred_list_busy = 0;
    image_info_deferred_list = NULL;
    deferred_image_info_timeout = NULL;
}

/* Tell layout the dimensions of an image.  Actually, to avoid subtle
   bugs involving mutual recursion between the imagelib and the
   netlib, we don't process this information immediately, but rather
   thread it on a list to be processed later. */
void
LO_SetImageInfo(MWContext *context, int32 ele_id, int32 width, int32 height)
{
    setImageInfoClosure *c = XP_NEW(setImageInfoClosure);

    if (! c)
        return;
    c->context  = context;
    c->ele_id   = ele_id;
    c->width    = width;
    c->height   = height;
    c->next     = image_info_deferred_list;
    image_info_deferred_list = c;

    if (! deferred_image_info_timeout) {
        deferred_image_info_timeout = 
            FE_SetTimeout(lo_process_deferred_image_info, NULL, 0);
    }
}

void
lo_DeleteDocLists(MWContext *context, lo_DocLists *doc_lists)
{
	if ( doc_lists->url_list != NULL)
	{
		int i;
		LO_AnchorData **anchor_array;

#ifdef XP_WIN16
		{
		XP_Block *ulist_array;
		int32 j;
		intn a_size, a_indx, a_url;

		a_size = SIZE_LIMIT / sizeof(XP_Block *);
		a_indx = doc_lists->url_list_len / a_size;
		a_url = doc_lists->url_list_len - (a_indx * a_size);

		XP_LOCK_BLOCK(ulist_array, XP_Block *, doc_lists->ulist_array);
		for (j=0; j < (doc_lists->ulist_array_size - 1); j++)
		{
			doc_lists->url_list = ulist_array[j];
			XP_LOCK_BLOCK(anchor_array, LO_AnchorData **,
				doc_lists->url_list);
			for (i=0; i < a_size; i++)
			{
				if (anchor_array[i] != NULL)
				{
					lo_DestroyAnchor(anchor_array[i]);
				}
			}
			XP_UNLOCK_BLOCK(doc_lists->url_list);
			XP_FREE_BLOCK(doc_lists->url_list);
		}
		doc_lists->url_list = ulist_array[doc_lists->ulist_array_size - 1];
		XP_LOCK_BLOCK(anchor_array, LO_AnchorData **,
			doc_lists->url_list);
		for (i=0; i < a_url; i++)
		{
			if (anchor_array[i] != NULL)
			{
				lo_DestroyAnchor(anchor_array[i]);
			}
		}
		XP_UNLOCK_BLOCK(doc_lists->url_list);
		XP_FREE_BLOCK(doc_lists->url_list);

		XP_UNLOCK_BLOCK(doc_lists->ulist_array);
		XP_FREE_BLOCK(doc_lists->ulist_array);
		}
#else
		XP_LOCK_BLOCK(anchor_array, LO_AnchorData **,
			doc_lists->url_list);
		for (i=0; i < doc_lists->url_list_len; i++)
		{
			if (anchor_array[i] != NULL)
			{
				lo_DestroyAnchor(anchor_array[i]);
			}
		}
		XP_UNLOCK_BLOCK(doc_lists->url_list);
		XP_FREE_BLOCK(doc_lists->url_list);
#endif /* XP_WIN16 */
		doc_lists->url_list = NULL;
		doc_lists->url_list_len = 0;
	}

	if (doc_lists->name_list != NULL)
	{
		lo_NameList *nptr;

		nptr = doc_lists->name_list;
		while (nptr != NULL)
		{
			lo_NameList *name_rec;

			name_rec = nptr;
			nptr = nptr->next;
			if (name_rec->name != NULL)
			{
				PA_FREE(name_rec->name);
			}
			XP_DELETE(name_rec);
		}
		doc_lists->name_list = NULL;
        doc_lists->anchor_count = 0;
	}

	if ( doc_lists->form_list != NULL)
	{
		lo_FormData *fptr;

		fptr = doc_lists->form_list;
		while (fptr != NULL)
		{
			lo_FormData *form;

			form = fptr;
			fptr = fptr->next;
			if (form->name != NULL)
			{
				PA_FREE(form->name);
			}
			if (form->action != NULL)
			{
				PA_FREE(form->action);
			}
			if (form->form_elements != NULL)
			{
				PA_FREE(form->form_elements);
			}
			XP_DELETE(form);
		}
		doc_lists->form_list = NULL;
        doc_lists->current_form_num = 0;
	}

    doc_lists->embed_list = NULL;
    doc_lists->embed_list_count = 0;
    doc_lists->applet_list = NULL;
    doc_lists->applet_list_count = 0;
    doc_lists->image_list = doc_lists->last_image = NULL;
    doc_lists->image_list_count = 0;
}

/* XXX need to pass resize_reload through FE_FreeGridWindow */
static pa_DocData *lo_internal_doc_data;

LO_Element *
lo_InternalDiscardDocument(MWContext *context, lo_DocState *state,
						   pa_DocData *doc_data, Bool bWholeDoc)
{
	int32 doc_id;
	LO_Element *recycle_list;
	LO_Element **line_array;
	LO_TextAttr **text_attr_hash;

	/* Look in doc_data->url_struct for the *new* document's reload flag. */
	JSBool resize_reload = 
		(JSBool)(doc_data != NULL && doc_data->url_struct->resize_reload);

    /* XXX We're not getting rid of stuff on the deferred image list when
     * the editor does a relayout. The problem is that there might still be
     * entries on the list that corresponds to the old state of the document.
     */
    if (bWholeDoc)
	    lo_discard_unprocessed_deferred_images(context);

	/*
	 * Order is crucial here: run the onunload handlers in a frames tree
	 * from the top down, but destroy frames bottom-up.  Look for the
	 * LM_ReleaseDocument call below, almost at the end of this function.
	 * This way, onunload handlers get to use their kids' properties, and
	 * kids get to use their parent's properties and functions.
	 */
	if (bWholeDoc)
	{
		pa_DocData *old_doc_data;
		
		old_doc_data = state->top_state->doc_data;
		if (old_doc_data != NULL)
		{
			NET_StreamClass *parser_stream;
			parser_stream = old_doc_data->parser_stream;
			parser_stream->abort(parser_stream, 0);
			state->top_state->doc_data = NULL;
		}
	}

	if ( state->top_state->trash != NULL)
	{
		lo_RecycleElements(context, state,
			state->top_state->trash);
		state->top_state->trash = NULL;
	}

	if (state->float_list != NULL)
	{
		lo_RecycleElements(context, state, state->float_list);
		state->float_list = NULL;
	}

	if (state->line_array != NULL)
	{
		LO_Element *eptr;
		LO_Element **line_array_tmp;

#ifdef XP_WIN16
		{
			XP_Block *larray_array;
			intn cnt;

			XP_LOCK_BLOCK(larray_array, XP_Block *,
					state->larray_array);
			cnt = state->larray_array_size - 1;
			while (cnt > 0)
			{
				XP_FREE_BLOCK(larray_array[cnt]);
				cnt--;
			}
			state->line_array = larray_array[0];
			XP_UNLOCK_BLOCK(state->larray_array);
			XP_FREE_BLOCK(state->larray_array);
		}
#endif /* XP_WIN16 */

		eptr = NULL;
		XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
		eptr = line_array[0];
		line_array_tmp = (LO_Element **)state->line_array;
		state->line_array = NULL;

        /* If this is a relayout delete, the elements are already owned by the document. So
         * don't recycle them.
         */
		if ( bWholeDoc && eptr != NULL)
		{
			lo_RecycleElements(context, state, eptr);
		}
		XP_UNLOCK_BLOCK(line_array_tmp);
		XP_FREE_BLOCK(line_array_tmp);
	}

	if ( bWholeDoc && state->top_state->text_attr_hash != NULL)
	{
		LO_TextAttr *attr;
		LO_TextAttr *attr_ptr;
		int i;

		XP_LOCK_BLOCK(text_attr_hash, LO_TextAttr **,
			state->top_state->text_attr_hash);
		for (i=0; i<FONT_HASH_SIZE; i++)
		{
			attr_ptr = text_attr_hash[i];
			while (attr_ptr != NULL)
			{
				attr = attr_ptr;
				if (attr->FE_Data != NULL)
				{
					FE_ReleaseTextAttrFeData(context, attr);
					attr->FE_Data = NULL;
				}
				attr_ptr = attr_ptr->next;
				XP_DELETE(attr);
			}
			text_attr_hash[i] = NULL;
		}
		XP_UNLOCK_BLOCK(state->top_state->text_attr_hash);
		XP_FREE_BLOCK(state->top_state->text_attr_hash);
		state->top_state->text_attr_hash = NULL;
	}

	if ( bWholeDoc && state->top_state->font_face_array != NULL)
	{
		int i;
		char **face_list;

		PA_LOCK(face_list, char **, state->top_state->font_face_array);
		for (i=0; i < state->top_state->font_face_array_len; i++)
		{
			XP_FREE(face_list[i]);
		}
		PA_UNLOCK(state->top_state->font_face_array);
		PA_FREE(state->top_state->font_face_array);
		state->top_state->font_face_array = NULL;
		state->top_state->font_face_array_len = 0;
		state->top_state->font_face_array_size = 0;
	}

	if ( bWholeDoc && state->top_state->map_list != NULL)
	{
		lo_MapRec *map_list;

		map_list = state->top_state->map_list;
		while (map_list != NULL)
		{
			map_list = lo_FreeMap(map_list);
		}
		state->top_state->map_list = NULL;
	}

    if (bWholeDoc)
        lo_DeleteDocLists(context, &state->top_state->doc_lists);
    else if (state->top_state->doc_lists.name_list != NULL)
	{
		lo_NameList *nptr;

		nptr = state->top_state->doc_lists.name_list;
		while (nptr != NULL)
		{
			lo_NameList *name_rec;

			name_rec = nptr;
			nptr = nptr->next;
			if (name_rec->name != NULL)
			{
				PA_FREE(name_rec->name);
			}
			XP_DELETE(name_rec);
		}
		state->top_state->doc_lists.name_list = NULL;
	}

	if ( bWholeDoc && state->top_state->the_grid != NULL)
	{
		pa_DocData *save;
		lo_GridRec *grid;
		lo_GridCellRec *cell_ptr;
		Bool incomplete;

		save = lo_internal_doc_data;
		lo_internal_doc_data = doc_data;
		grid = state->top_state->the_grid;
		cell_ptr = grid->cell_list;
		/*
		 * If this grid document finished loading, save
		 * its grid data to be restored when we return through
		 * history.
		 * We still have to free the FE grid windows.
		 */
		incomplete = (state->top_state->layout_status != PA_COMPLETE);
		if (incomplete == FALSE)
		{
			while (cell_ptr != NULL)
			{
				intn cell_status;
				char *old_url;

				cell_status =
					lo_GetCurrentGridCellStatus(cell_ptr);

				/*
				 * Only save cell history for COMPLETE cells.
				 */
				if (cell_status == PA_COMPLETE)
				{
				    /*
				     * free the original url for this cell.
				     */
				    old_url = cell_ptr->url;
				    if (cell_ptr->url != NULL)
				    {
						XP_FREE(cell_ptr->url);
				    }

				    /*
				     * Fetch the url for the current contents
				     * of the cell, and save that.
				     */
				    cell_ptr->url =
					lo_GetCurrentGridCellUrl(cell_ptr);

				    cell_ptr->hist_list = NULL;
				    if (cell_ptr->context != NULL)
				    {
					cell_ptr->hist_list = FE_FreeGridWindow(
						cell_ptr->context, TRUE);
				    }
				}
				else
				{
				    incomplete = TRUE;
				    cell_ptr->hist_list = NULL;
				    if (cell_ptr->context != NULL)
				    {
					(void)FE_FreeGridWindow(
						cell_ptr->context, FALSE);
				    }
				}
				cell_ptr->context = NULL;

				cell_ptr = cell_ptr->next;
			}
		}
		/*
		 * Check again in case one or more frames were incomplete.
		 * Also, some special windows have no history (such as
		 * the document info window).  We can't save grid
		 * data for those windows and should free it instead.
		 */
		if ((incomplete == FALSE)&&
			(state->top_state->savedData.Grid != NULL))
		{
			state->top_state->savedData.Grid->the_grid =
				state->top_state->the_grid;
			state->top_state->savedData.Grid->main_width =
				grid->main_width;
			state->top_state->savedData.Grid->main_height =
				grid->main_height;
		}
		/*
		 * Else free up everything now.
		 */
		else
		{
			cell_ptr = grid->cell_list;
			/*
			 * It is important to NULL out the cell_list
			 * now because FE_FreeGridWindow() could lead to
			 * somone trying to traverse it while we are in
			 * the middle of deleting it.
			 */
			grid->cell_list = NULL;
			while (cell_ptr != NULL)
			{
				lo_GridCellRec *tmp_cell;

				if (cell_ptr->context != NULL)
				{
					/*
					 * The context still owns its history
					 * list, so null the cell's pointer to
					 * be clear.  If this cell doesn't have
					 * a context, then it owns the history
					 * list and lo_FreeGridCellRec() will
					 * free it.
					 */
					cell_ptr->hist_list = NULL;
					(void)FE_FreeGridWindow(
						cell_ptr->context, FALSE);
				}
				tmp_cell = cell_ptr;
				cell_ptr = cell_ptr->next;
				lo_FreeGridCellRec(context, grid, tmp_cell);
			}
			lo_FreeGridRec(grid);
		}
		state->top_state->the_grid = NULL;
		lo_internal_doc_data = save;
	}

	if ( bWholeDoc && state->top_state->url != NULL)
	{
		XP_FREE(state->top_state->url);
		state->top_state->url = NULL;
	}

	if ( bWholeDoc )
	{
		XP_FREEIF(state->top_state->inline_stream_blocked_base_url);
        XP_FREEIF(state->top_state->main_stream_blocked_base_url);
        XP_FREEIF(state->top_state->base_url);
	}

	if ( bWholeDoc && state->top_state->base_target != NULL)
	{
		XP_FREE(state->top_state->base_target);
		state->top_state->base_target = NULL;
	}

	if ( bWholeDoc && state->top_state->name_target != NULL)
	{
		XP_FREE(state->top_state->name_target);
		state->top_state->name_target = NULL;
	}

#ifdef HTML_CERTIFICATE_SUPPORT
	if ( bWholeDoc && state->top_state->cert_list != NULL)
	{
		lo_Certificate *cptr;

		cptr = state->top_state->cert_list;
		while (cptr != NULL)
		{
			lo_Certificate *lo_cert;

			lo_cert = cptr;
			cptr = cptr->next;
			if (lo_cert->name != NULL)
			{
				PA_FREE(lo_cert->name);
			}
			if (lo_cert->cert != NULL)
			{
				CERT_DestroyCertificate(lo_cert->cert);
			}
			XP_DELETE(lo_cert);
		}
		state->top_state->cert_list = NULL;
	}
#endif /* HTML_CERTIFICATE_SUPPORT */

	if (bWholeDoc && state->top_state->unknown_head_tag != NULL)
	{
		XP_FREE(state->top_state->unknown_head_tag);
		state->top_state->unknown_head_tag = NULL;
	}

	if(resize_reload)
	{
		/* LJM: Save the JSS state in the hist so that we can get it
	 	 * once the resize is complete
		 */
		History_entry *he = SHIST_GetCurrent(&context->hist);
		if (he)
		{
			he->savedData.style_stack = state->top_state->style_stack;
			state->top_state->style_stack = 0;
		}
		else
		{
			if (state->top_state->style_stack != 0)
				STYLESTACK_Delete(state->top_state->style_stack);
			state->top_state->style_stack = 0;
		}
	}
	else
	{
		if (state->top_state->style_stack != 0)
			STYLESTACK_Delete(state->top_state->style_stack);
		state->top_state->style_stack = 0;
	}

	/* XXX all this because grid docs are never re-parsed after first load. */
	if (!resize_reload &&
		((state->top_state->savedData.OnLoad != NULL)||
		 (state->top_state->savedData.OnUnload != NULL)||
		 (state->top_state->savedData.OnFocus != NULL)||
		 (state->top_state->savedData.OnBlur != NULL)||
		 (state->top_state->savedData.OnHelp != NULL)||
		 (state->top_state->savedData.OnMouseOver != NULL)||
		 (state->top_state->savedData.OnMouseOut != NULL)||
		 (state->top_state->savedData.OnDragDrop != NULL)||
		 (state->top_state->savedData.OnMove != NULL)||
		 (state->top_state->savedData.OnResize != NULL)))
	{
		History_entry *he = SHIST_GetCurrent(&context->hist);
		if (he)
		{
			he->savedData.OnLoad = state->top_state->savedData.OnLoad;
			he->savedData.OnUnload = state->top_state->savedData.OnUnload;
			he->savedData.OnFocus = state->top_state->savedData.OnFocus;
			he->savedData.OnBlur = state->top_state->savedData.OnBlur;
 			he->savedData.OnHelp = state->top_state->savedData.OnHelp;
 			he->savedData.OnMouseOver = state->top_state->savedData.OnMouseOver;
 			he->savedData.OnMouseOut = state->top_state->savedData.OnMouseOut;
 			he->savedData.OnDragDrop = state->top_state->savedData.OnDragDrop;
 			he->savedData.OnMove = state->top_state->savedData.OnMove;
 			he->savedData.OnResize = state->top_state->savedData.OnResize;
		}
		else
		{
			if (state->top_state->savedData.OnLoad != NULL)
				PA_FREE(state->top_state->savedData.OnLoad);
			if (state->top_state->savedData.OnUnload != NULL)
				PA_FREE(state->top_state->savedData.OnUnload);
			if (state->top_state->savedData.OnFocus != NULL)
				PA_FREE(state->top_state->savedData.OnFocus);
			if (state->top_state->savedData.OnBlur != NULL)
				PA_FREE(state->top_state->savedData.OnBlur);
			if (state->top_state->savedData.OnHelp != NULL)
				PA_FREE(state->top_state->savedData.OnHelp);
			if (state->top_state->savedData.OnMouseOver != NULL)
				PA_FREE(state->top_state->savedData.OnMouseOver);
			if (state->top_state->savedData.OnMouseOut != NULL)
				PA_FREE(state->top_state->savedData.OnMouseOut);
			if (state->top_state->savedData.OnDragDrop != NULL)
				PA_FREE(state->top_state->savedData.OnDragDrop);
			if (state->top_state->savedData.OnMove != NULL)
				PA_FREE(state->top_state->savedData.OnMove);
			if (state->top_state->savedData.OnResize != NULL)
				PA_FREE(state->top_state->savedData.OnResize);
		}
		state->top_state->savedData.OnLoad = NULL;
		state->top_state->savedData.OnUnload = NULL;
		state->top_state->savedData.OnFocus = NULL;
		state->top_state->savedData.OnBlur = NULL;
		state->top_state->savedData.OnHelp = NULL;
		state->top_state->savedData.OnMouseOver = NULL;
		state->top_state->savedData.OnMouseOut = NULL;
		state->top_state->savedData.OnDragDrop = NULL;
		state->top_state->savedData.OnMove = NULL;
		state->top_state->savedData.OnResize = NULL;
	}


	if ( bWholeDoc )
	{
#ifdef WEBFONTS
	    /* Release webfonts loaded by this document */
	    if (WF_fbu && !resize_reload)
	    {
		    nffbu_ReleaseWebfonts(WF_fbu, context, NULL);
	    }
#endif /* WEBFONTS */

        recycle_list = state->top_state->recycle_list;

        /* Some lo_ImageStructs may have been reclaimed by the Memory Arenas
           code without actually calling IL_DestroyImage first.  This call
           frees up any remaining image lib requests. */
        IL_DestroyImageGroup(context->img_cx);

        lo_DestroyBlinkers(context);
        if (state->top_state->layers) 
        {
            int i;
            lo_TopState *top_state = state->top_state;

            /* If we're resizing, prevent the GC'ing of the JS object
             * associated with this layer when the layer destructor is
             * called, since we want to preserve it across a resize,
             * except for those layers created with JS's new operator,
             * which aren't preserved across resize.  */
            if (resize_reload)
            {
                for (i = 1; i <= state->top_state->max_layer_num; i++)
                {
                    lo_LayerDocState *layer_state = state->top_state->layers[i];
                    if (!layer_state || layer_state->is_constructed_layer)
                        continue;
                
                    layer_state->mocha_object = NULL;
                    state->top_state->layers[i] = NULL;
                }
            }

            /* Delete the main document layer_state */
            top_state->layers[0]->layer = NULL;
            lo_DeleteLayerState(context, state, top_state->layers[0]);
            
            top_state->num_layers_allocated = 0;
            top_state->max_layer_num = 0;

            lo_DeleteLayerStack(state);
        }

        if (context->compositor){
            lo_DestroyLayers(context);

            /* Reset the compositor to do on-screen compositing by default. */
            CL_SetCompositorOffscreenDrawing(context->compositor,
                                             CL_OFFSCREEN_AUTO);
            CL_SetCompositorEnabled(context->compositor, PR_FALSE);
        }
        XP_FREEIF(state->top_state->layers);
    }

    if ( bWholeDoc ) {
    	/* throw out mocha cruft */
    	ET_ReleaseDocument(context, resize_reload);

	    XP_DELETE(state->top_state);
	    XP_DELETE(state);
	    /*
	     * Get the unique document ID, and remove this
	     * documents layout state.
	     */
	    doc_id = XP_DOCID(context);
	    lo_RemoveTopState(doc_id);

    }
    else
    {
        XP_DELETE(state);
        recycle_list = NULL;
    }
	return(recycle_list);
}


void
LO_DiscardDocument(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_Element *recycle_list;
	int32 cnt;
#ifdef MEMORY_ARENAS
	lo_arena *old_arena;
#endif /* MEMORY_ARENAS */

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return;
	}
	state = top_state->doc_state;
	
#ifdef MEMORY_ARENAS
    if ( ! EDT_IS_EDITOR(context) ) {
	    old_arena = top_state->first_arena;
    }
#endif /* MEMORY_ARENAS */

	/*
	 * Order is crucial here: run the onunload handlers in a frames tree
	 * from the top down, but destroy frames bottom-up.  Look for the
	 * LM_ReleaseDocument call below, almost at the end of this function.
	 * This way, onunload handlers get to use their kids' properties, and
	 * kids get to use their parent's properties and functions.
	 */
	ET_SendLoadEvent(context, EVENT_UNLOAD, NULL, NULL,
			 LO_DOCUMENT_LAYER_ID, FALSE);

	/* make sure no other threads are looking at our structures */
	LO_LockLayout();

	lo_SaveFormElementState(context, state, TRUE);
	/*
	 * If this document has no session history
	 * this data is not saved.
	 * It will be freed by history cleanup.
	 */
	if (SHIST_GetCurrent(&context->hist) == NULL)
	{
		top_state->savedData.FormList = NULL;
		top_state->savedData.EmbedList = NULL;
		top_state->savedData.Grid = NULL;
	}
	recycle_list =
		lo_InternalDiscardDocument(context, state, lo_internal_doc_data, TRUE);
	/* see first lo_FreeRecycleList comment. safe */
	cnt = lo_FreeRecycleList(context, recycle_list);
#ifdef MEMORY_ARENAS
    if ( ! EDT_IS_EDITOR(context) ) {
	    cnt = lo_FreeMemoryArena(old_arena);
    }
#endif /* MEMORY_ARENAS */

	/* we currently have no document loaded */
	context->doc_id = 0;

	/* the danger has passed.  let others look up our shorts again */
	LO_UnlockLayout();

}


lo_SavedFormListData *
lo_NewDocumentFormListData(void)
{
	lo_SavedFormListData *fptr;

	fptr = XP_NEW(lo_SavedFormListData);
	if (fptr == NULL)
	{
		return(NULL);
	}

	fptr->valid = FALSE;
	fptr->data_index = 0;
	fptr->data_count = 0;
	fptr->data_list = NULL;
	fptr->next = NULL;

	return(fptr);
}


void
LO_FreeDocumentFormListData(MWContext *context, void *data)
{
	lo_SavedFormListData *forms = (lo_SavedFormListData *)data;

	if (forms != NULL)
	{
		lo_FreeDocumentFormListData(context, forms);
		XP_DELETE(forms);
	}
}


lo_SavedGridData *
lo_NewDocumentGridData(void)
{
	lo_SavedGridData *sgptr;

	sgptr = XP_NEW(lo_SavedGridData);
	if (sgptr == NULL)
	{
		return(NULL);
	}

	sgptr->main_width = 0;
	sgptr->main_height = 0;
	sgptr->the_grid = NULL;

	return(sgptr);
}


void
LO_FreeDocumentGridData(MWContext *context, void *data)
{
	lo_SavedGridData *sgptr = (lo_SavedGridData *)data;

	if (sgptr != NULL)
	{
		lo_FreeDocumentGridData(context, sgptr);
		XP_DELETE(sgptr);
	}
}


lo_SavedEmbedListData *
lo_NewDocumentEmbedListData(void)
{
	lo_SavedEmbedListData *eptr;

	eptr = XP_NEW(lo_SavedEmbedListData);
	if (eptr == NULL)
	{
		return(NULL);
	}

	eptr->embed_count = 0;
	eptr->embed_data_list = NULL;

	return(eptr);
}


void
LO_FreeDocumentEmbedListData(MWContext *context, void *data)
{
	lo_SavedEmbedListData *eptr = (lo_SavedEmbedListData *)data;

	if (eptr != NULL)
	{
		lo_FreeDocumentEmbedListData(context, eptr);
		XP_DELETE(eptr);
	}
}

void
LO_HighlightAnchor(MWContext *context, LO_Element *element, Bool on)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_Element *start, *end;
	LO_AnchorData *anchor;

	/*
	 * Get the unique document ID, and retrieve this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return;
	}
	state = top_state->doc_state;

	if (element == NULL)
		return;

	anchor = lo_GetElementAnchor(element);
	if (anchor == NULL)
		return;

	/* Find all the preceding layout elements that share the same anchor. */
	start = element;
	while (start->lo_any.prev)
	{
		LO_AnchorData *anchor2 = lo_GetElementAnchor(start->lo_any.prev);
		if (anchor != anchor2)
			break;
		start = start->lo_any.prev;
	}

	/* Find all the following layout elements that share the same anchor. */
	end = element;
	while (end->lo_any.next)
	{
		LO_AnchorData *anchor2 = lo_GetElementAnchor(end->lo_any.next);
		if (anchor != anchor2)
			break;
		end = end->lo_any.next;
	}

	end = end->lo_any.next;
	element = start;
	while (element != end)
	{
        Bool restoreUnselected;
		LO_Color save_fg;

		element->lo_any.x += state->base_x;
		element->lo_any.y += state->base_y;

        restoreUnselected = FALSE;
		switch (element->type)
		{
		case LO_IMAGE:
			/* Save image selection state */
			if (! (element->lo_image.ele_attrmask & LO_ELE_SELECTED))
				restoreUnselected = TRUE;
			
			/* Force display of selection rectangle. */
			element->lo_image.ele_attrmask = LO_ELE_SELECTED;

			/* Fall through ... */

		case LO_TEXT:
		case LO_SUBDOC:

			/* Temporarily set element color to selection color. */
			if (on) {
				lo_GetElementFGColor(element, &save_fg);
				lo_SetElementFGColor(element, &state->active_anchor_color);
			}

			/* Use the compositor to synchronously update the element's area. */
			lo_RefreshElement(element, anchor->layer, TRUE);

			/* Restore the element's color. */
			if (on)
				lo_SetElementFGColor(element, &save_fg);

			/* Restore image selection state */
			if (restoreUnselected)
				element->lo_image.ele_attrmask &= ~(LO_ELE_SELECTED);

		default:
			/* No highlighting for other types of elements */
			break;
		}

		element->lo_any.x -= state->base_x;
		element->lo_any.y -= state->base_y;

		element = element->lo_any.next;
	}
}

LO_Element *
LO_XYToNearestElement(MWContext *context, int32 x, int32 y, CL_Layer *layer)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_Element *eptr;
	LO_Element **line_array;
	int32 ret_x, ret_y;
    LO_CellStruct *layer_cell;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return(NULL);
	}
	state = top_state->doc_state;

    layer_cell = lo_GetCellFromLayer(context, layer);
    if (layer_cell != NULL)
    {
        int32 new_x, new_y;

        new_y = y;
        if (new_y > layer_cell->height)
            new_y = layer_cell->height - 1;
        if (new_y < 0)
            new_y = 0;
        
        new_x = x;
        if (new_x > layer_cell->width)
            new_x = layer_cell->width - 1;
        if (new_x < 0)
            new_x = 0;
        eptr = lo_XYToNearestCellElement(context, 
                                         state,
                                         layer_cell, new_x, new_y);
        return eptr;
    }

	if (x <= state->win_left)
	{
		x = state->win_left + 1;
	}

	if (y < state->win_top)
	{
		y = state->win_top + 1;
	}

	eptr = lo_XYToDocumentElement(context, state, x, y, TRUE, TRUE, TRUE,
		&ret_x, &ret_y);

	if (eptr == NULL)
	{
		int32 top, bottom;

		/*	
		 * Find the nearest line.
		 */
		lo_RegionToLines (context, state, x, y, 1, 1, FALSE,
			&top, &bottom);

		if (top >= 0)
#ifdef XP_WIN16
		{
			intn a_size;
			intn a_indx;
			intn a_line;
			XP_Block *larray_array;

			a_size = SIZE_LIMIT / sizeof(LO_Element *);
			a_indx = (intn)(top / a_size);
			a_line = (intn)(top - (a_indx * a_size));
			XP_LOCK_BLOCK(larray_array, XP_Block *,
					state->larray_array);
			state->line_array = larray_array[a_indx];
			XP_LOCK_BLOCK(line_array, LO_Element **,
					state->line_array);
			eptr = line_array[a_line];
			XP_UNLOCK_BLOCK(state->line_array);
			XP_UNLOCK_BLOCK(state->larray_array);
		}
#else
		{
			XP_LOCK_BLOCK(line_array, LO_Element **,
					state->line_array);
			eptr = line_array[top];
			XP_UNLOCK_BLOCK(state->line_array);
		}
#endif /* XP_WIN16 */

		/*
		 * The nearest line may be a table with cells.  In which case
		 * we really need to move into that table to find the nearest
		 * cell, and possibly nearest element in that cell.
		 */
		if ((eptr != NULL)&&(eptr->type == LO_TABLE)&&
		    (eptr->lo_any.next->type == LO_CELL))
		{
		    LO_Element *new_eptr;

		    new_eptr = eptr->lo_any.next;
		    /*
		     * Find the cell that overlaps in Y.  Move to the lower
		     * cell if between cells.
		     */
		    while ((new_eptr != NULL)&&(new_eptr->type == LO_CELL))
		    {
			int32 y2;

			y2 = new_eptr->lo_any.y + new_eptr->lo_any.y_offset +
				new_eptr->lo_any.height +
				(2 * new_eptr->lo_cell.border_width);
			if (y <= y2)
			{
				break;
			}
			new_eptr = new_eptr->lo_any.next;
		    }
		    /*
		     * If we walked through the table to a non-cell element
		     * and still didn't match, match to the last cell in
		     * the table.
		     */
		    if ((new_eptr != NULL)&&(new_eptr->type != LO_CELL))
		    {
			new_eptr = new_eptr->lo_any.prev;
		    }

		    /*
		     * If new_eptr is not NULL it is the cell we matched.
		     * Move us just into that cell and see if we can match
		     * an element inside it.
		     */
		    if ((new_eptr != NULL)&&(new_eptr->type == LO_CELL))
		    {
			LO_Element *cell_eptr;
			int32 new_x, new_y;

			new_y = y;
			if (new_y >= (new_eptr->lo_any.y +
					new_eptr->lo_any.y_offset +
					new_eptr->lo_cell.border_width +
					new_eptr->lo_any.height))
			{
				new_y = new_eptr->lo_any.y +
					new_eptr->lo_any.y_offset +
					new_eptr->lo_cell.border_width +
					new_eptr->lo_any.height - 1;
			}
			if (new_y < (new_eptr->lo_any.y +
					new_eptr->lo_any.y_offset +
					new_eptr->lo_cell.border_width))
			{
				new_y = new_eptr->lo_any.y +
					new_eptr->lo_any.y_offset +
					new_eptr->lo_cell.border_width;
			}
			new_x = x;
			if (new_x >= (new_eptr->lo_any.x +
					new_eptr->lo_any.x_offset +
					new_eptr->lo_cell.border_width +
					new_eptr->lo_any.width))
			{
				new_x = new_eptr->lo_any.x +
					new_eptr->lo_any.x_offset +
					new_eptr->lo_cell.border_width +
					new_eptr->lo_any.width - 1;
			}
			if (new_x < (new_eptr->lo_any.x +
					new_eptr->lo_any.x_offset +
					new_eptr->lo_cell.border_width))
			{
				new_x = new_eptr->lo_any.x +
					new_eptr->lo_any.x_offset +
					new_eptr->lo_cell.border_width;
			}
			cell_eptr = lo_XYToNearestCellElement(context, state,
				(LO_CellStruct *)new_eptr, new_x, new_y);
			if (cell_eptr != NULL)
			{
				new_eptr = cell_eptr;
			}
		    }
		    if (new_eptr != NULL)
		    {
			eptr = new_eptr;
		    }
		}
	}

	return(eptr);
}

static void
lo_RefreshCellAnchors(lo_DocState *state, LO_CellStruct *cell);

/* return TRUE if the color of the current anchor is
 * one of the two default visted or unvisited colors
 * it will be FALSE in the case that style sheets sets
 * a specific color for the anchor
 */
static Bool
lo_is_default_anchor_color(lo_DocState *state, LO_TextAttr *tmp_attr)
{
	if(  (tmp_attr->fg.red      == STATE_VISITED_ANCHOR_RED(state)
	      && tmp_attr->fg.green == STATE_VISITED_ANCHOR_GREEN(state)
	      && tmp_attr->fg.blue  == STATE_VISITED_ANCHOR_BLUE(state))
          || (tmp_attr->fg.red      == STATE_UNVISITED_ANCHOR_RED(state)
	      && tmp_attr->fg.green == STATE_UNVISITED_ANCHOR_GREEN(state)
	      && tmp_attr->fg.blue  == STATE_UNVISITED_ANCHOR_BLUE(state)))
		return TRUE;

	return FALSE;
}

static void
lo_RefreshElementAnchor(lo_DocState *state, LO_Element *element)
{
	LO_TextAttr tmp_attr, *new_attr, *attr;
	LO_AnchorData *anchor_href;
	char *url;

	if (element->type == LO_CELL) {
		lo_RefreshCellAnchors(state, (LO_CellStruct *)element);
		return;
	}

	attr = lo_GetElementTextAttr(element);
	if (! attr)
		return;
	if (! (attr->attrmask & LO_ATTR_ANCHOR))
	    return;

	anchor_href = lo_GetElementAnchor(element);
	if (!anchor_href)
		return;

	if(!lo_is_default_anchor_color(state, attr))
		return;

	lo_CopyTextAttr(attr, &tmp_attr);

	url = (char*)anchor_href->anchor;
	if (GH_CheckGlobalHistory(url) != -1)
	{
		tmp_attr.fg.red   = STATE_VISITED_ANCHOR_RED(state);
		tmp_attr.fg.green = STATE_VISITED_ANCHOR_GREEN(state);
		tmp_attr.fg.blue  = STATE_VISITED_ANCHOR_BLUE(state);
	}
	else
	{
		tmp_attr.fg.red   = STATE_UNVISITED_ANCHOR_RED(state);
		tmp_attr.fg.green = STATE_UNVISITED_ANCHOR_GREEN(state);
		tmp_attr.fg.blue  = STATE_UNVISITED_ANCHOR_BLUE(state);
	}

	new_attr = lo_FetchTextAttr(state, &tmp_attr);
	
	if (new_attr != attr) {
		lo_SetElementTextAttr(element, new_attr);
		if (element->type == LO_TEXT || element->type == LO_IMAGE) {
			element->lo_any.x += state->base_x;
			element->lo_any.y += state->base_y;
			lo_RefreshElement(element, anchor_href->layer, TRUE);
			element->lo_any.x -= state->base_x;
			element->lo_any.y -= state->base_y;
		}
	}
}

static void
lo_RefreshCellAnchors(lo_DocState *state, LO_CellStruct *cell)
{
	LO_Element *eptr;

	eptr = cell->cell_list;
	while (eptr)
	{
		lo_RefreshElementAnchor(state, eptr);
		eptr = eptr->lo_any.next;
	}

	eptr = cell->cell_float_list;
	while (eptr)
	{
		lo_RefreshElementAnchor(state, eptr);
		eptr = eptr->lo_any.next;
	}
}

void
LO_RefreshAnchors(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_Element *eptr;
	LO_Element **line_array;

        if (!context->compositor)
            return;

	/*
	 * Get the unique document ID, and retrieve this
	 * document's layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
		return;
	state = top_state->doc_state;

	/*
	 * line_array could be NULL when discarding the document, so
	 * check it and exit early to avoid dereferencing it below.
	 */
	if (state->line_array == NULL)
		return;

#ifdef XP_WIN16
	{
		XP_Block *larray_array;

		XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
		state->line_array = larray_array[0];
		XP_UNLOCK_BLOCK(state->larray_array);
	}
#endif /* XP_WIN16 */

	XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
	eptr = line_array[0];
	XP_UNLOCK_BLOCK(state->line_array);

	for (; eptr; eptr = eptr->lo_any.next)
		lo_RefreshElementAnchor(state, eptr);
}

static void
lo_split_named_anchor(char *url, char **cmp_url, char **cmp_name)
{
	char tchar, *tptr, *tname;

	*cmp_url = NULL;
	
	if ( cmp_name )
		*cmp_name = NULL;

	if (url == NULL)
	{
		return;
	}

	tptr = url;
	while (((tchar = *tptr) != '\0')&&(tchar != '#'))
	{
		tptr++;
	}
	if (tchar == '\0')
	{
		*cmp_url = (char *)XP_STRDUP(url);
		return;
	}

	*tptr = '\0';
	*cmp_url = (char *)XP_STRDUP(url);
	*tptr = tchar;
	tname = ++tptr;
	while (((tchar = *tptr) != '\0')&&(tchar != '?'))
	{
		tptr++;
	}
	*tptr = '\0';
	if ( cmp_name )
		*cmp_name = (char *)XP_STRDUP(tname);
	*tptr = tchar;
	if (tchar == '?')
	{
		StrAllocCat(*cmp_url, tptr);
	}
}


/*
 * Is this anchor local to this document?
 */
Bool
lo_IsAnchorInDoc(lo_DocState *state, char *url)
{
	char *cmp_url;
	char *doc_url;
	Bool local;

	local = FALSE;
	/*
	 * Extract the base from the name part.
	 */
	lo_split_named_anchor(url, &cmp_url, NULL);

	/*
	 * Extract the base from the name part.
	 */
	doc_url = NULL;
	lo_split_named_anchor(state->top_state->url, &doc_url, NULL);

	/*
	 * If the document itself has no base URL (probably an error)
	 * then the only way we are local is if the passed in url has no
	 * base either.
	 */
	if (doc_url == NULL)
	{
		if (cmp_url == NULL)
		{
			local = TRUE;
		}
	}
	/*
	 * Else if the passed in url has no base, and we have a bas, you must
	 * be local because you are relative to our base.
	 */
	else if (cmp_url == NULL)
	{
		local = TRUE;
	}
	/*
	 * Else if the base of the two urls is equal, you are local.
	 */
	else if (XP_STRCMP(cmp_url, doc_url) == 0)
	{
		local = TRUE;
	}

	if (cmp_url != NULL)
	{
		XP_FREE(cmp_url);
	}
	if (doc_url != NULL)
	{
		XP_FREE(doc_url);
	}
	return(local);
}


static LO_Element *
lo_cell_id_to_element(lo_DocState *state, int32 ele_id, LO_CellStruct *cell)
{
	LO_Element *eptr;

	if (cell == NULL)
	{
		return(NULL);
	}

	eptr = cell->cell_list;
	while (eptr != NULL)
	{
		if (eptr->lo_any.ele_id == ele_id)
		{
			break;
		}
		if (eptr->type == LO_CELL)
		{
			LO_Element *cell_eptr;

			cell_eptr = lo_cell_id_to_element(state, ele_id,
						(LO_CellStruct *)eptr);
			if (cell_eptr != NULL)
			{
				eptr = cell_eptr;
				break;
			}
		}
		if (eptr->lo_any.ele_id > ele_id)
		{
			break;
		}
		eptr = eptr->lo_any.next;
	}
	return(eptr);
}


/*
 * Find the x, y, coords of the element id passed in, and return them.
 * If can't find the exact element, return the closest (next greater)
 * element id's position.
 * We need to walk into cells.
 * On severe error return 0, 0.
 */
static void
lo_element_id_to_xy(lo_DocState *state, int32 ele_id, int32 *xpos, int32 *ypos)
{
	LO_Element *eptr;
	LO_Element **line_array;

	*xpos = 0;
	*ypos = 0;

	/*
	 * On error return the top
	 */
	if (state == NULL)
	{
		return;
	}

#ifdef XP_WIN16
	{
		XP_Block *larray_array;

		XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
		state->line_array = larray_array[0];
		XP_UNLOCK_BLOCK(state->larray_array);
	}
#endif /* XP_WIN16 */

	XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
	eptr = line_array[0];
	XP_UNLOCK_BLOCK(state->line_array);

	while (eptr != NULL)
	{
		/*
		 * Exact match?
		 */
		if (eptr->lo_any.ele_id == ele_id)
		{
			*xpos = eptr->lo_any.x;
			*ypos = eptr->lo_any.y;
			break;
		}

		/*
		 * Look for a match in this cell
		 */
		if (eptr->type == LO_CELL)
		{
			LO_Element *cell_eptr;

			cell_eptr = lo_cell_id_to_element(state, ele_id,
						(LO_CellStruct *)eptr);
			if (cell_eptr != NULL)
			{
				*xpos = cell_eptr->lo_any.x;
				*ypos = cell_eptr->lo_any.y;
				break;
			}
		}

		/*
		 * If we passed it, fake a match.
		 */
		if (eptr->lo_any.ele_id > ele_id)
		{
			*xpos = eptr->lo_any.x;
			*ypos = eptr->lo_any.y;
			break;
		}
		eptr = eptr->lo_any.next;
	}
}


/*
 * Check the current URL to see if it is the URL for this doc, and if
 * so return the position of the named anchor referenced after the '#' in
 * the URL.  If there is no name or no match, return 0,0.  In comparing URLs
 * both NULL are considered equal
 */
Bool
LO_LocateNamedAnchor(MWContext *context, URL_Struct *url_struct,
			int32 *xpos, int32 *ypos)
{
    int i;
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	lo_NameList *nptr;
	char *url;
	char *cmp_url;
	char *cmp_name;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return(FALSE);
	}
	state = top_state->doc_state;

	/*
	 * obvious error
	 */
	if ((url_struct == NULL)||(url_struct->address == NULL))
	{
		return(FALSE);
	}

	/*
	 * Extract the URL we are going to.
	 */
	url = url_struct->address;


	/*
	 * Split the passed url into the real url part
	 * and the name part.  The parts (if non-NULL) must
	 * be freed.
	 */
	cmp_url = NULL;
	cmp_name = NULL;
	lo_split_named_anchor(url, &cmp_url, &cmp_name);

	/*
	 * can't strcmp on NULL strings.
	 */
	if (state->top_state->url == NULL)
	{
		/*
		 * 2 NULLs are considered equal.
		 * If not equal fail here
		 */
		if (cmp_url != NULL)
		{
			if (cmp_name != NULL)
			{
				XP_FREE(cmp_name);
			}
			return(FALSE);
		}
	}
	/*
	 * Else lets compare the 2 URLs now
	 */
	else
	{
		/*
		 * The current URL might itself have a name part.  Lose it.
		 */
		char *this_url;
		int same_p;

		lo_split_named_anchor(state->top_state->url, &this_url, NULL);
		/*
		 * If the 2 URLs are not equal fail out here
		 */
		same_p = ((this_url)&&(XP_STRCMP(cmp_url, this_url) == 0));

		if (this_url != NULL)
		{
			XP_FREE (this_url);
		}

		if (!same_p)
		{
			XP_FREE(cmp_url);
			if (cmp_name != NULL)
			{
				XP_FREE(cmp_name);
			}
			return(FALSE);
		}
	}

	/*
	 * If we got here the URLs are considered equal.
	 * Free this one.
	 */
	if (cmp_url != NULL)
	{
		XP_FREE(cmp_url);
	}

	/*
	 * Do we have a saved element ID that is not the start
	 * of the document to return to because we are moving through history?
	 * If so, go there and return.
	 */
	if (url_struct->position_tag > 1)
	{
		lo_element_id_to_xy(state, url_struct->position_tag,
			xpos, ypos);
		return(TRUE);
	}

	/*
	 * Special case for going to a NULL or empty
	 * name to return the beginning of the
	 * document.
	 */
	if (cmp_name == NULL)
	{
		*xpos = 0;
		*ypos = 0;

		if (state->top_state->url != NULL)
		{
			XP_FREE(state->top_state->url);
		}
		state->top_state->url = XP_STRDUP(url);
		return(TRUE);
	}
	else if (*cmp_name == '\0')
	{
		*xpos = 0;
		*ypos = 0;
		XP_FREE(cmp_name);

		if (state->top_state->url != NULL)
		{
			XP_FREE(state->top_state->url);
		}
		state->top_state->url = XP_STRDUP(url);
		return(TRUE);
	}

	/*
	 * Actually search the name list for a matching name.
	 */
    for (i = 0; i <= state->top_state->max_layer_num; i++)
    {
        lo_LayerDocState *layer_state = state->top_state->layers[i];
        CL_Layer *layer;
        if (!layer_state)
            continue;
        layer = layer_state->layer;
        nptr = layer_state->doc_lists->name_list;
        
        while (nptr != NULL)
        {
            char *name;

            PA_LOCK(name, char *, nptr->name);
            /*
             * If this is a match, return success
             * here.
             */
            if (XP_STRCMP(cmp_name, name) == 0)
            {
                PA_UNLOCK(nptr->name);
                if (nptr->element == NULL)
                {
                    *xpos = 0;
                    *ypos = 0;
                }
                else
                {
                    /* Convert from element coordinates to document coordinates */
                    lo_GetLayerXYShift(layer, xpos, ypos);
                    *xpos = - *xpos;
                    *ypos = - *ypos;
                    *xpos += nptr->element->lo_any.x + CL_GetLayerXOrigin(layer);
                    *ypos += nptr->element->lo_any.y + CL_GetLayerYOrigin(layer);
                }
                XP_FREE(cmp_name);

                if (state->top_state->url != NULL)
                {
                    XP_FREE(state->top_state->url);
                }
                state->top_state->url = XP_STRDUP(url);
                LM_SendOnLocate(context, nptr);
                return(TRUE);
            }
            PA_UNLOCK(nptr->name);
            nptr = nptr->next;
        }
    }

	/*
	 * Failed to find the matchng name.
	 * If no match return the top of the doc.
	 */
	XP_FREE(cmp_name);
	*xpos = 0;
	*ypos = 0;

	if (state->top_state->url != NULL)
	{
		XP_FREE(state->top_state->url);
	}
	state->top_state->url = XP_STRDUP(url);
	return(TRUE);
}


Bool
LO_HasBGImage(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if (top_state == NULL)
		return(FALSE);

#ifdef XP_WIN
	/*
	 * Evil hack alert.
	 * We don't want print context's under windows to use backdrops
	 *  currently as it causes bad display (black in the transparent
	 *  area due to drawing problems under windows).
	 */
	 if(context->type == MWContextPrint || context->type == MWContextMetaFile)
	 {
	 	return(FALSE);
	 }
#endif

     return (LO_GetLayerBackdropURL(top_state->doc_layer) != NULL);
}

void
LO_GetDocumentMargins(MWContext *context,
	int32 *margin_width, int32 *margin_height)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return;
	}
	state = top_state->doc_state;

	*margin_width = state->win_left;
	*margin_height = state->win_top;
}


/*
 * Provide a way for the parser to see if the currently loaded
 *   document who might need a call to mocha to unload
 */
Bool
LO_CheckForUnload(MWContext *context)
{
	lo_TopState *top_state;

	top_state = lo_FetchTopState(XP_DOCID(context));
	if (!top_state)
		return FALSE;

	return top_state->mocha_has_onunload;
}

/*
 * netlib is done sending data to the parser
 */
void 
LO_NetlibComplete(MWContext * context)
{
	lo_TopState *top_state;

	top_state = lo_FetchTopState(XP_DOCID(context));
	if (!top_state)
		return;
	top_state->nurl = NULL;
}

/*
 * Even though netlib is idle someone could be shoving data
 *   through layout/libparse.  Provide a way for people on
 *   the outside to tell if this is the case or not.
 */

Bool
LO_LayingOut(MWContext * context)
{
	lo_TopState *top_state;

	top_state = lo_FetchTopState(XP_DOCID(context));
	if (!top_state)
		return FALSE;
	return (top_state->doc_data != NULL);
}


void lo_UpdateStateWhileFlushingLine( MWContext *context, lo_DocState *state )
{
  int32 justification_remainder=0;

  if (state->top_state->nothing_displayed != FALSE) {
    /*
     * If we are displaying elements we are
     * no longer in the HEAD section of the HTML
     * we are in the BODY section.
     */
    state->top_state->in_head = FALSE;
    state->top_state->in_body = TRUE;

    lo_use_default_doc_background(context, state);
    state->top_state->nothing_displayed = FALSE;
  }

  /*
	 * There is a break at the end of this line.
	 * this may change min_width.
	 */
  {
    int32 new_break_holder;
    int32 min_width;
    int32 indent;

    new_break_holder = state->x;
    min_width = new_break_holder - state->break_holder;
    indent = state->list_stack->old_left_margin - state->win_left;
    min_width += indent;
    if (min_width > state->min_width)
      {
	state->min_width = min_width;
      }
    /* If we are not within <NOBR> content, allow break_holder
     * to be set to the new position where a line break can occur.
     * This fixes BUG #70782
     */
    if (state->breakable != FALSE) {
      state->break_holder = new_break_holder;
    }
  }

  /*
	 * If in a division centering or right aligning this line
	 */
  if ((state->align_stack != NULL)&&(state->delay_align == FALSE))
    {
      int32 push_right;
      LO_Element *tptr;

      if (state->align_stack->alignment == LO_ALIGN_CENTER)
	{
	  push_right = (state->right_margin - state->x) / 2;
	}
      else if (state->align_stack->alignment == LO_ALIGN_RIGHT)
	{
	  push_right = state->right_margin - state->x;
	}
      else if(state->align_stack->alignment == LO_ALIGN_JUSTIFY)
	{
	  push_right = lo_calc_push_right_for_justify(state, &justification_remainder);
	}
      else
	{
	  push_right = 0;
	}

      if ((push_right > 0 || justification_remainder)
	  &&(state->line_list != NULL))
	{
	  int32 count = 0;
	  int32 move_delta;
	  tptr = state->line_list;

	  if(state->align_stack->alignment == LO_ALIGN_JUSTIFY)
	    move_delta = 0;
	  else
	    move_delta = push_right;

	  while (tptr != NULL)
	    {
	      /* 
	       * We don't shift over inflow layers, since their contents
	       * have already been shifted over.
	       */
	      /*
	       * We also don't shift bullets of type BULLET_MQUOTE.
	       */
	      if (((tptr->type != LO_CELL)||
		   (!tptr->lo_cell.cell_inflow_layer))&&
		  ((tptr->type != LO_BULLET)||
		   ((tptr->type == LO_BULLET)&&
		    (tptr->lo_bullet.bullet_type != BULLET_MQUOTE))))
		{
		  tptr->lo_any.x += move_delta;
		}

	      {
		CL_Layer *layer = NULL;
		int32 x, y;
		int32 border_width;
		int32 x_offset, y_offset;

		border_width = 0;
		x = tptr->lo_any.x;
		y = tptr->lo_any.y;
		x_offset = tptr->lo_any.x_offset;
		y_offset = tptr->lo_any.y_offset;

		switch (tptr->type)
		  {
		  case LO_IMAGE:
		    layer = tptr->lo_image.layer;
		    border_width = tptr->lo_image.border_width;
		    break;
#ifdef JAVA
		  case LO_JAVA:
		    layer = tptr->lo_java.layer;
		    border_width = tptr->lo_java.border_width;
		    break;
#ifdef SHACK
		  case LO_BUILTIN:
		    layer = tptr->lo_builtin.layer;
		    border_width = tptr->lo_builtin.border_width;
		    break;
#endif
#endif
		  case LO_EMBED:
		    layer = tptr->lo_embed.layer;
		    border_width = tptr->lo_embed.border_width;
		    break;
		  case LO_FORM_ELE:
		    layer = tptr->lo_form.layer;
		    break;
		  default:
		    layer = NULL;
		    break;
		  }

		if (layer)
		  CL_MoveLayer(layer,
			       x + x_offset + border_width,
			       y + y_offset + border_width);
	      }

				/* justification push_rights are additive */
	      if(state->align_stack->alignment == LO_ALIGN_JUSTIFY)
		{
		  move_delta += push_right;

		  /* if there is a justification remainder, add a pixel
		   * to the first n word breaks
		   */
		  if(count < justification_remainder)
		    move_delta++;
		}

	      /* 
	       * Note that if this is an inflow layer, we don't want
	       * to shift it since the alignment has already propogated
	       * to its contents.
	       */
	      if ((tptr->type == LO_CELL) &&
		  !tptr->lo_cell.cell_inflow_layer)
		{
		  lo_ShiftCell((LO_CellStruct *)tptr, move_delta, 0);
		}
	      tptr = tptr->lo_any.next;

	      count++;
	    }

	  if(state->align_stack->alignment == LO_ALIGN_JUSTIFY)
	    state->x = state->right_margin;
	  else
	    state->x += push_right;
	}
    }
}

/* Add line feed to line list, update doc state, and set line height of
   other elements on that line */
void lo_AppendLineFeed( MWContext *context, lo_DocState *state, LO_LinefeedStruct *linefeed, int32 breaking, Bool updateFE )
{
	LO_Element *tptr;

	if (breaking != FALSE)
	{
		linefeed->ele_attrmask |= LO_ELE_BREAKING;
	}

	/*
	 * Horrible bitflag overuse!!!  For multicolumn text
	 * we need to know if a line of text can be used for
	 * a column break, or if it cannot because it is wrapped
	 * around some object in the margin.  For lines that can be
	 * used for column breaks, we will set the BREAKABLE
	 * flag in their element mask.
	 */
	if ((state->left_margin_stack == NULL)&&
		(state->right_margin_stack == NULL))
	{
		linefeed->ele_attrmask |= LO_ELE_BREAKABLE;
	}

	if ((state->align_stack != NULL)&&
		(state->delay_align != FALSE)&&
		(state->align_stack->alignment != LO_ALIGN_LEFT))
	{
		if (state->align_stack->alignment == LO_ALIGN_CENTER)
		{
			linefeed->ele_attrmask |= LO_ELE_DELAY_CENTER;
		}
		else if (state->align_stack->alignment == LO_ALIGN_RIGHT)
		{
			linefeed->ele_attrmask |= LO_ELE_DELAY_RIGHT;
		}
	}

	tptr = state->line_list;

	if (tptr == NULL)
	{
		state->line_list = (LO_Element *)linefeed;
		linefeed->prev = NULL;
	}
	else
	{
		LO_Element *next_tptr = tptr;
		do
		{
			tptr = next_tptr;
			if (updateFE) 
			{
				/*
				 * If the display is blocked for an element
				 * we havn't reached yet, check to see if
				 * it is in this line, and if so, save its
				 * y position.
				 */
				if ((state->display_blocked != FALSE)&&
	#ifdef EDITOR
					(!state->edit_relayout_display_blocked)&&
	#endif
					(state->is_a_subdoc == SUBDOC_NOT)&&
					(state->display_blocking_element_y == 0)&&
					(state->display_blocking_element_id != -1)&&
					(tptr->lo_any.ele_id >=
					state->display_blocking_element_id))
				{
					state->display_blocking_element_y =
						state->y;
					/*
					 * Special case, if the blocking element
					 * is on the first line, no blocking
					 * at all needs to happen.
					 */
					if (state->y == state->win_top)
					{
						state->display_blocked = FALSE;
						FE_SetDocPosition(context,
							FE_VIEW, 0, state->base_y);
						if (context->compositor)
						{
						  XP_Rect rect;
						  
						  rect.left = 0;
						  rect.top = 0;
						  rect.right = state->win_width;
						  rect.bottom = state->win_height;
						  CL_UpdateDocumentRect(context->compositor,
												&rect, (PRBool)FALSE);
						}
					}
				}
			}

			tptr->lo_any.line_height = state->line_height;
			/*
			 * Special for bullets of type BULLET_MQUOTE
			 * They should always be as tall as the line.
			 */
			if ((tptr->type == LO_BULLET)&&
				(tptr->lo_bullet.bullet_type ==
					BULLET_MQUOTE))
			{
				tptr->lo_any.y_offset = 0;
				tptr->lo_any.height =
					tptr->lo_any.line_height;
			}
			
			if ( (next_tptr = tptr->lo_any.next) == NULL )
            {
				tptr->lo_any.next = (LO_Element*)linefeed;
				linefeed->prev = tptr;
            }
		} while( next_tptr != NULL );
	}
	state->x += linefeed->width;
}

void lo_AppendLineListToLineArray( MWContext *context, lo_DocState *state, LO_Element *lastElementOnLineList)
{
	LO_Element **line_array;
#ifdef XP_WIN16
	int32 a_size;
	int32 a_indx;
	int32 a_line;
	XP_Block *larray_array;
#endif /* XP_WIN16 */

	/*
	 * If necessary, grow the line array to hold more lines.
	 */
#ifdef XP_WIN16
	a_size = SIZE_LIMIT / sizeof(LO_Element *);
	a_indx = (state->line_num - 1) / a_size;
	a_line = state->line_num - (a_indx * a_size);

	XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
	state->line_array = larray_array[a_indx];

	if (a_line == a_size)
	{
		state->line_array = XP_ALLOC_BLOCK(LINE_INC *
					sizeof(LO_Element *));
		if (state->line_array == NULL)
		{
			XP_UNLOCK_BLOCK(state->larray_array);
			state->top_state->out_of_memory = TRUE;
			return;
		}
		XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
		line_array[0] = NULL;
		XP_UNLOCK_BLOCK(state->line_array);
		state->line_array_size = LINE_INC;

		state->larray_array_size++;
		XP_UNLOCK_BLOCK(state->larray_array);
		state->larray_array = XP_REALLOC_BLOCK(
			state->larray_array, (state->larray_array_size
			* sizeof(XP_Block)));
		if (state->larray_array == NULL)
		{
			state->top_state->out_of_memory = TRUE;
			return;
		}
		XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
		larray_array[state->larray_array_size - 1] = state->line_array;
		state->line_array = larray_array[a_indx];
	}
	else if (a_line >= state->line_array_size)
	{
		state->line_array_size += LINE_INC;
		if (state->line_array_size > a_size)
		{
			state->line_array_size = (intn)a_size;
		}
		state->line_array = XP_REALLOC_BLOCK(state->line_array,
		    (state->line_array_size * sizeof(LO_Element *)));
		if (state->line_array == NULL)
		{
			XP_UNLOCK_BLOCK(state->larray_array);
			state->top_state->out_of_memory = TRUE;
			return;
		}
		larray_array[a_indx] = state->line_array;
	}

	/*
	 * Place this line of elements into the line array.
	 */
	XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
	line_array[a_line - 1] = state->line_list;
	XP_UNLOCK_BLOCK(state->line_array);

	XP_UNLOCK_BLOCK(state->larray_array);
#else
	if (state->line_num > state->line_array_size)
	{
		int32 line_inc;

		if (state->line_array_size > (LINE_INC * 10))
		{
			line_inc = state->line_array_size / 10;
		}
		else
		{
			line_inc = LINE_INC;
		}

		state->line_array = XP_REALLOC_BLOCK(state->line_array,
			((state->line_array_size + line_inc) *
			sizeof(LO_Element *)));
		if (state->line_array == NULL)
		{
			state->top_state->out_of_memory = TRUE;
			return;
		}
		state->line_array_size += line_inc;
	}

	/*
	 * Place this line of elements into the line array.
	 */
	XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
	line_array[state->line_num - 1] = state->line_list;
	XP_UNLOCK_BLOCK(state->line_array);
#endif /* XP_WIN16 */

	/*
	 * connect the complete doubly linked list between this line
	 * and the last one.
	 */
	if (state->end_last_line != NULL)
	{
		state->end_last_line->lo_any.next = state->line_list;
		state->line_list->lo_any.prev = state->end_last_line;
	}
	state->end_last_line = lastElementOnLineList;

	state->line_list = NULL;
	state->line_num++;
}

void lo_UpdateStateAfterFlushingLine( MWContext *context, lo_DocState *state, LO_LinefeedStruct *linefeed, Bool inRelayout )
{
	/* LO_LinefeedStruct *linefeed; */
	
	lo_AppendLineListToLineArray( context, state, (LO_Element *) linefeed );

	/* 
	 *Don't draw if we're doing layers...we'll just refresh when the 
	 * the layer size increases.
	 */
	/*
	 * Have the front end display this line.
	 */
    /* For layers, only draw if a compositor doesn't exist */
    if (!context->compositor && !inRelayout)
        /* We need to supply a display rectangle that is guaranteed to
           encompass all elements on the line.  The special 0x3fffffff
           value is approximately half the range of a 32-bit int, so
           it leaves room for overflow if arithmetic is done on these
           values. */
        lo_DisplayLine(context, state, (state->line_num - 2),
                       0, 0, 0x3fffffffL, 0x3fffffffL);

	/*
	 * We are starting a new line.  Clear any old break
	 * positions left over, clear the line_list, and increase
	 * the line number.
	 */
	state->old_break = NULL;
	state->old_break_block = NULL;
	state->old_break_pos = -1;
	state->old_break_width = 0;
	state->baseline = 0;
}

#ifdef TEST_16BIT
#undef XP_WIN16
#endif /* TEST_16BIT */

#ifdef PROFILE
#pragma profile off
#endif
