/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   layrelay.c --- reflow of layout elements.
   Created: Nisheeth Ranjan <nisheeth@netscape.com>, 7-Nov-1997.
 */
#include "xp.h"
#include "net.h"
#include "pa_parse.h"
#include "layout.h"
#include "laylayer.h"
#include "layers.h"
#include "layrelay.h"
#include "laytrav.h"
#ifdef EDITOR
#include "edt.h"
#endif

#include "libmocha.h"
#include "libevent.h"

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

static lo_DocState *lo_rl_InitDocState( MWContext *context, lo_DocState *state,
					int32 width, int32 height,
					int32 margin_width, int32 margin_height);
static void lo_rl_PreLayoutElement ( MWContext *context, lo_DocState * state,
				     LO_Element *lo_ele );
static void lo_rl_CopyCellToState( LO_CellStruct *cell, lo_DocState *state );
static void lo_rl_CopyStateToCell( lo_DocState *state, LO_CellStruct *cell);

/* Loops through layout elements and calls their fitting functions */
static void lo_rl_FitLayoutElements( lo_RelayoutState *relay_state, LO_Element *start_ele );


static lo_DocState *
lo_rl_InitDocState( MWContext *context, lo_DocState *state, int32 width, int32 height, int32 margin_width, int32 margin_height );
static void lo_UpdateHeightSpanForBeginRow(lo_TableRec *table);
static void lo_rl_BeginTableRelayout( MWContext *context, lo_DocState *state, lo_TableRec *table);
static LO_LinefeedStruct * lo_rl_RecreateElementListsFromCells( lo_DocState *state, LO_TableStruct *table, 
															   LO_Element **lastElementInLastCell);

typedef LO_Element * (*lo_FitFunction)( lo_RelayoutState *relay_state, LO_Element *lo_ele );

static LO_Element * lo_rl_FitIgnore( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitError( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitText( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitLineFeed( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitHRule( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitImage( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitBullet( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitFormEle( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitTable( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitCell( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitEmbed( lo_RelayoutState *relay_state, LO_Element *lo_ele );
#ifdef SHACK
static LO_Element * lo_rl_FitBuiltin( lo_RelayoutState *relay_state, LO_Element *lo_ele );
#endif /* SHACK */
static LO_Element * lo_rl_FitJava( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitObject( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitParagraph( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitCenter( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitMulticolumn( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitFloat( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitTextBlock( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitList( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitDescTitle( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitDescText( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitBlockQuote( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitLayer( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitHeading( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitSpan( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitDiv( lo_RelayoutState *relay_state, LO_Element *lo_ele );
static LO_Element * lo_rl_FitSpacer( lo_RelayoutState *relay_state, LO_Element *lo_ele );

/* The table of fitting functions for each layout element type... */
static lo_FitFunction lo_rl_FitFunctionTable[] = {
	lo_rl_FitIgnore,		/* LO_NONE */
	lo_rl_FitText,			/* LO_TEXT */
	lo_rl_FitLineFeed,		/* LO_LINEFEED */
	lo_rl_FitHRule,			/* LO_HRULE */
	lo_rl_FitImage,			/* LO_IMAGE */
	lo_rl_FitBullet,		/* LO_BULLET */
	lo_rl_FitFormEle,		/* LO_FORM_ELE */
	lo_rl_FitError,			/* LO_SUBDOC elements should not occur in line list */
	lo_rl_FitTable,			/* LO_TABLE */
	lo_rl_FitCell,			/* LO_CELL */
	lo_rl_FitEmbed,			/* LO_EMBED */
	lo_rl_FitError,			/* LO_EDGE element: Either an error or we should ignore. ??? */
	lo_rl_FitJava,			/* LO_JAVA */
	lo_rl_FitError,			/* LO_SCRIPT elements should not occur in list of layout elements */
	lo_rl_FitObject,		/* LO_OBJECT */
	lo_rl_FitParagraph,		/* LO_PARAGRAPH */
	lo_rl_FitCenter,		/* LO_CENTER */
	lo_rl_FitMulticolumn,	/* LO_MULTICOLUMN */
	lo_rl_FitFloat,			/* LO_FLOAT */
	lo_rl_FitTextBlock,		/* LO_TEXTBLOCK */
	lo_rl_FitList,			/* LO_LIST */
	lo_rl_FitDescTitle,		/* LO_DESCTITLE */
	lo_rl_FitDescText,		/* LO_DESCTEXT */
	lo_rl_FitBlockQuote,	/* LO_BLOCKQUOTE */
	lo_rl_FitLayer,			/* LO_LAYER */
	lo_rl_FitHeading,		/* LO_HEADING */
	lo_rl_FitSpan,			/* LO_SPAN */
	lo_rl_FitDiv,			/* LO_DIV */
#ifdef SHACK
	lo_rl_FitBuiltin,		/* LO_BUILTIN */
#endif /* SHACK */
	lo_rl_FitSpacer 		/* LO_SPACER */
};


/*	NRA - 10/3/97: Main Relayout Entry Point
 *  This function walks through the layout element list and re-lays out
 *  the elements to fit into a window of the specified width and height.
 *
 *	It needs to be called on each resize of the window.
 *
 *	Issues to be careful about: 1) Synchronization with Javascript that is running concurrently while this
 *	re-layout is going on.
 *
 *	2) Keep a copy of elements that change.  Will come in handy later to figure out the redraw rectangle.
 *	For now, we dirty the entire document
 *
 *	3) FE needs to tell us the position in the current document.  We will need to determine the element id
 *	at that position and call FE_SetDocPosition to set the document to the saved position after relayout
 */
void LO_RelayoutOnResize(MWContext *context, int32 width, int32 height, int32 leftMargin, int32 topMargin)
{
	lo_RelayoutState *relay_state;
	lo_DocState *state;
	LO_Element *lo_ele;
	int32 docX, docY;
	lo_TopState *top_state;

	/* check to see if the context we've been passed is a grid context.  If so, branch to the grid
	   specific resize routine, which will call this one again on its grid cells. */
	top_state = lo_FetchTopState(XP_DOCID(context));
	if (top_state == NULL)  {
	  return;
	}

	LO_LockLayout();

	/* Abort if the document is currently being laid out */
	if (LO_LayingOut( context ))
	{
		LO_UnlockLayout();
		return;
	}

	if (top_state->the_grid) {
	  /* the document doesn't have any elements.  It's a grid document */
	  lo_RelayoutGridDocumentOnResize(context, top_state, width, height);
	  LO_UnlockLayout();
	  return;
	}

	relay_state = lo_rl_CreateRelayoutState();
	if (relay_state == NULL) {
		LO_UnlockLayout();
		return;  /* Better error handling later */
	}

	/* Reset the scoping for any JavaScript to be the top-level layer. */
    /* ET_SetActiveLayer(context, LO_DOCUMENT_LAYER_ID); */

	/* remember where we are in the document */
	FE_GetDocPosition ( context, FE_VIEW, &docX, &docY );

	/* shrink the layout document in case we get smaller */
	LO_SetDocumentDimensions(context, 0, 0);

	if (lo_rl_InitRelayoutState(context, relay_state, width, height, leftMargin, topMargin) != NULL) 
	{
		lo_ele = lo_tv_GetFirstLayoutElement(relay_state->doc_state);
		lo_rl_FitLayoutElements(relay_state, lo_ele);
			
		/* Flush out any remaining elements on the line list into the line array. */		
		lo_rl_AddSoftBreakAndFlushLine(relay_state->context, relay_state->doc_state);
		lo_EndLayoutDuringReflow( relay_state->context, relay_state->doc_state );
	}		
	else 
	{
		lo_rl_DestroyRelayoutState(relay_state);
		LO_UnlockLayout();
		return;
	}

	state = relay_state->doc_state;

	/* Send JS load event.  JS will treat it as a resize by looking at the resize_reload param */
	ET_SendLoadEvent(context, EVENT_LOAD, NULL, NULL, 
                             LO_DOCUMENT_LAYER_ID, 
                             state->top_state->resize_reload);

	/* Reset state for force loading images. */
	LO_SetForceLoadImage(NULL, FALSE);

	/* position the document */
	FE_SetDocPosition ( context, FE_VIEW, 0, docY );

	/* update the final document dimensions */
	state->y += state->win_bottom;
	LO_SetDocumentDimensions(context, state->max_width, state->y);
	
	if(!state->top_state->is_binary)
		FE_SetProgressBarPercent(context, 100);

	FE_FinishedLayout(context);

	/* Tell FE to redraw window */
    if (context->compositor)
    {
		CL_Compositor *compositor = context->compositor;
        XP_Rect rect;
		CL_OffscreenMode save_offscreen_mode;

		rect.left = rect.top = 0;
		rect.right = state->win_width;
		rect.bottom = state->y;
        
        CL_UpdateDocumentRect(compositor,
                              &rect, (PRBool)FALSE);

		/* Temporarily force drawing to use the offscreen buffering area to reduce
		   flicker when resizing. (If no offscreen store is allocated, this code will
		   have no effect, but it will do no harm.) */
		save_offscreen_mode = CL_GetCompositorOffscreenDrawing(compositor);
		CL_SetCompositorOffscreenDrawing(compositor, CL_OFFSCREEN_ENABLED);
		CL_CompositeNow(compositor);
		CL_SetCompositorOffscreenDrawing(compositor, save_offscreen_mode);
	}

	lo_rl_DestroyRelayoutState(relay_state);

	LO_UnlockLayout();
	/* Postion is hard-coded to top of document for now.*/
	/* FE_SetDocPosition(context, 0, 0, 0); */
	/* 2nd parameter for FE_ClearView is iView which is ignored. */
	/* FE_ClearView(context, 0); */

}

/*
** Relayout a document given a change in one LO_Element*.  This is of
** primary use for plugins (since there is now an API to change the
** size of a plugin), but could also be used once the LO_Elements are
** reflected in a DOM and style information affecting the element's
** size is changed.
*/
void
LO_RelayoutFromElement(MWContext *context, LO_Element *element)
{
  /* toshok - we don't have all the plumbing in place yet to handle
     something like this, so we punt and make it a wrapper for
     LO_RelayoutOnResize.  Given that all drawing is done through a
     compositor, the user shouldn't be aware of everything being
     relayed out anyway.  */
  int32 leftMargin, topMargin;
  int32 width, height;
  lo_TopState *top_state = lo_FetchTopState(XP_DOCID(context));

  if (!top_state)
    return;

  width = LO_GetLayerScrollWidth(top_state->body_layer);
  height = LO_GetLayerScrollHeight(top_state->body_layer);

  LO_GetDocumentMargins(context, &leftMargin, &topMargin);

  LO_RelayoutOnResize(context, width, height, leftMargin, topMargin);
}

/*	
 *  This function reflows a document from a given element. It's primary function is for the editor.
 *	The caller must provide a state record which is correctly initialized to the position the reflow
 *	will start.
 *
 *	It currently only works for text elements.
 */
void LO_Reflow(MWContext *context, lo_DocState * state, LO_Element *startElement, LO_Element *endElement)
{
	LO_Element *lo_ele;
	lo_TopState *top_state;
	lo_RelayoutState relay_state;
	lo_DocState * save_state;
	LO_TextBlock * block;
	Bool firstElement;
	
#if defined( DEBUG_nisheeth ) || defined( DEBUG_toshok ) || defined( DEBUG_shannon )
	if (context) {
		lo_PrintLayout(context);
	}
#endif

	/* check to see if the context we've been passed is a grid context.  If so, branch to the grid
	   specific resize routine, which will call this one again on its grid cells. */
	top_state = lo_FetchTopState(XP_DOCID(context));
	if (top_state == NULL)  {
	  return;
	}
	
	LO_LockLayout();

	/* init this by hand as we don't want to create a new state */
	relay_state.context = context;
	relay_state.top_state = lo_FetchTopState(XP_DOCID(context));
	relay_state.doc_state = state;

	firstElement = TRUE;
	lo_ele = startElement;
	
	while ( lo_ele != endElement ) {
		lo_rl_PreLayoutElement ( context, state, lo_ele );
		if (state->top_state->out_of_memory)
		{
			LO_UnlockLayout();
			return;
		}
		
		state->edit_force_offset = TRUE;
	
		save_state = top_state->doc_state;
		top_state->doc_state = state;
		
		block = NULL;
		
		/*
		 * If we get a text element, we also need to retrieve the text block
		 */
		if ( lo_ele->type == LO_TEXT )
		{
/* I'm pretty sure this ifdef is ok since EDT_GetTextBlock should always return NULL
   if we're not in an editor context. hardts */
#ifdef EDITOR
			block = EDT_GetTextBlock ( context, lo_ele );
#endif /* EDITOR */
		}
		
		/*
		 * if we found a text block, then call the reflow routine with this element, otherwise
		 * use the default reflow code. Remember sometimes we get text elements without a text block
		 * (such as list elements)
		 */
		if ( block != NULL ) {
			lo_ele = lo_RelayoutTextBlock ( context, state, block, &lo_ele->lo_text );
		}
		else {
			/*
			 * HACK: Until I find a better way to deal with this.
			 *
			 * The above lo_LayoutTag loop will handle setup for things like lists. This is fine
			 * when we're inside a list (ie past element 1) as the first thing the reflow will
			 * process is the bullet. However, (for a reason I don't entirely understand yet), if
			 * we're on the first line of the document and the list is the first element, startElement
			 * will point at the LO_LIST element. This means that reflow will reexecute the start list
			 * and we'll do the wrong thing.
			 *
			 * So, for now only, if the first element is a LO_LIST element, then skip it!
			 */
			if ( firstElement && ( lo_ele->type == LO_LIST ) )
			{
				LO_Element * next;
				
				next = lo_tv_GetNextLayoutElement ( state, lo_ele, FALSE );
				
				lo_ele->lo_any.ele_id = NEXT_ELEMENT;
				lo_ele->lo_any.x = state->x;
				lo_ele->lo_any.y = state->y;

				state->x += lo_ele->lo_text.width;
				lo_AppendToLineList( context, state, lo_ele, 0);
				
				lo_ele = next;
			}
			else
			{
				/* Dispatch the layout element to its fitting routine. 
				   Fitting routine returns the next layout element to process */
				lo_ele = (*lo_rl_FitFunctionTable[lo_ele->type])( &relay_state, lo_ele);
			}
		}
		
		firstElement = FALSE;
		
		top_state->doc_state = save_state;

		if (state->top_state->out_of_memory)
		{
			LO_UnlockLayout();
			return;
		}
		
		if ( lo_ele == NULL )
		{
			/* LO_UnlockLayout(); */
			break;
		}
		
		state->edit_force_offset = FALSE;
	}

#if defined( DEBUG_nisheeth ) || defined( DEBUG_toshok ) || defined( DEBUG_shannon )
	if (context) {
		lo_PrintLayout(context);
	}
#endif

	LO_UnlockLayout();
}

/* Selectively cut and pasted from lo_InitDocState().  Should we separate this stuff
out into a common function rather than duplicating code?  */
static lo_DocState *
lo_rl_InitDocState( MWContext *context, lo_DocState *state, int32 width, int32 height, int32 margin_width, int32 margin_height )
{
	lo_TopState *top_state = state->top_state;

	state->subdoc_tags = NULL;
	state->subdoc_tags_end = NULL;

	state->win_top = margin_height;
	state->win_bottom = margin_height;
	state->win_width = width;
	state->win_height = height;

	state->base_x = 0;
	state->base_y = 0;
	state->x = 0;
	state->y = 0;
	state->width = 0;

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

/* XXX - No font information is reset upon entering a layer, nor
       is it restored when the layer ends.  Is this the right thing to do ? */
    if (!state->font_stack) {
        state->base_font_size = DEFAULT_BASE_FONT_SIZE;
        state->font_stack = lo_DefaultFont(state, context);
        if (state->font_stack == NULL)
        {
			/*
            XP_FREE_BLOCK(state->line_array);
#ifdef XP_WIN16
            XP_FREE_BLOCK(state->larray_array);
#endif 
			*/
            return(NULL);
        }
        state->font_stack->text_attr->size = state->base_font_size;

        state->text_info.max_width = 0;
        state->text_info.ascent = 0;
        state->text_info.descent = 0;
        state->text_info.lbearing = 0;
        state->text_info.rbearing = 0;
    }

	/*	XXXX NRA: Possible memory leak here.  What if these guys aren't freed at the end of initial layout? 
	 *	Check before assigning to NULL 
	 */
	state->align_stack = NULL;
	state->line_height_stack = NULL;
	state->list_stack = lo_DefaultList(state);
	if (state->list_stack == NULL)
	{
		/*
		XP_FREE_BLOCK(state->line_array);
#ifdef XP_WIN16
		XP_FREE_BLOCK(state->larray_array);
#endif  /* XP_WIN16 */
		/*
		XP_DELETE(state->font_stack);
		*/
		return(NULL);
	}

	state->line_buf_size = 0;
	state->line_buf = PA_ALLOC(LINE_BUF_INC * sizeof(char));
	if (state->line_buf == NULL)
	{	/*
		XP_FREE_BLOCK(state->line_array);
#ifdef XP_WIN16
		XP_FREE_BLOCK(state->larray_array);
#endif /* XP_WIN16 */
		/*
		XP_DELETE(state->font_stack);
		*/
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

	state->must_relayout_subdoc = FALSE;
	state->allow_percent_width = TRUE;
	state->allow_percent_height = TRUE;
	state->is_a_subdoc = SUBDOC_NOT;
	state->current_subdoc = 0;
	state->sub_documents = NULL;
	state->sub_state = NULL;

	state->current_cell = NULL;

	state->extending_start = FALSE;
    state->selection_start = NULL;
    state->selection_start_pos = 0;
    state->selection_end = NULL;
    state->selection_end_pos = 0;
    state->selection_new = NULL;
    state->selection_new_pos = 0;
    state->selection_layer = NULL;

	state->in_relayout = FALSE;
	state->tab_stop = DEF_TAB_WIDTH;
	state->beginning_tag_count = top_state->tag_count;
	return(state);

}

lo_RelayoutState * lo_rl_CreateRelayoutState( void )
{
	lo_RelayoutState * relay_state = (lo_RelayoutState *) XP_ALLOC(sizeof(lo_RelayoutState));
	return relay_state;
}

void lo_rl_DestroyRelayoutState( lo_RelayoutState *relay_state )
{
	if (relay_state) {
		XP_FREE(relay_state);
	}
}

lo_RelayoutState *
lo_rl_InitRelayoutState( MWContext *context, lo_RelayoutState *blank_state, int32 newWidth, int32 newHeight, int32 margin_width, int32 margin_height )
{
	lo_TopState *top_state;
	lo_DocState *state;

	if (!context || !blank_state) {
		return NULL;
	}

	/* Initialize context */
	blank_state->context = context;

	/* Initialize top state */
	top_state = lo_FetchTopState(XP_DOCID(context));
	if (top_state == NULL) {
		return NULL;
	}
	
	top_state->element_id = 0;

	blank_state->top_state = top_state;

	/* Initialize document state */
	state = top_state->doc_state;
    if (!lo_rl_InitDocState(context, state, newWidth, newHeight, margin_width, margin_height))
    {
		top_state->out_of_memory = TRUE;
        return NULL;
    }
	blank_state->doc_state = state;

	return blank_state;
}


static void lo_rl_PreLayoutElement ( MWContext *context, lo_DocState * state, LO_Element *lo_ele )
{
	/*
	 * If we get a non-text tag, and we have some previous text
	 * elements we have been merging into state, flush those
	 * before processing the new tag.
	 */
	if ( lo_ele->type != LO_TEXTBLOCK )
	{
		lo_FlushLineBuffer(context, state);
		if (state->top_state->out_of_memory)
		{
			return;
		}
	}
}

static void lo_rl_FitLayoutElements( lo_RelayoutState *relay_state, LO_Element *start_ele )
{
	LO_Element *lo_ele = start_ele;

	/* While not end of layout element list */
	while ( lo_ele != NULL) {
		lo_rl_PreLayoutElement ( relay_state->context, relay_state->doc_state, lo_ele );
		if (relay_state->doc_state->top_state->out_of_memory)
		{
			return;
		}
		
		/* Dispatch the layout element to its fitting routine. 
		   Fitting routine returns the next layout element to process */
		lo_ele = (*lo_rl_FitFunctionTable[lo_ele->type])(relay_state, lo_ele);
		if (relay_state->doc_state->top_state->out_of_memory)
		{
			return;
		}

	} /* End While */
}

static LO_Element *
lo_rl_FitIgnore( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
	XP_TRACE(("lo_rl_FitIgnore called...\n"));
	return lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, FALSE);
}

static LO_Element *
lo_rl_FitError( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
	XP_TRACE(("lo_rl_FitError called...\n"));
	return lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, FALSE);
}

static LO_Element *
lo_rl_FitText( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
	LO_Element *next = lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, FALSE);
	lo_DocState *state = relay_state->doc_state;

	if (lo_ele->lo_text.bullet_type == BULLET_NONE)
	  {
	    /* move this element to the correct position and add it to the line list */
	    lo_ele->lo_any.ele_id = NEXT_ELEMENT;
	    lo_ele->lo_any.x = state->x;
	    lo_ele->lo_any.y = state->y;
	
	    state->x += lo_ele->lo_text.width;
	    lo_AppendToLineList(relay_state->context, state, lo_ele, 0);
	  }
	else if (lo_ele->lo_text.bullet_type == BULLET_MQUOTE)
	  {
	    /* Add to recycle list */
	    lo_ele->lo_any.prev = NULL;
	    lo_ele->lo_any.next = NULL;
	    lo_RecycleElements( relay_state->context, relay_state->top_state->doc_state, lo_ele );
	  }
	else if (lo_ele->lo_text.bullet_type == WORDBREAK)
	{
		/* Do nothing.  Just return the next layout element */
	}
	else /* it's some other form of bullet, most probably resulting from an ordered list. */
	  {
	    int32 line_height, baseline;

	    /* move this element to the correct position and add it to the line list */
	    lo_ele->lo_any.ele_id = NEXT_ELEMENT;

	    lo_FormatBulletStr(relay_state->context, state, (LO_TextStruct*)lo_ele, &line_height, &baseline);

	    lo_AppendToLineList(relay_state->context, state, lo_ele, 0);

	    lo_UpdateStateAfterBulletStr(relay_state->context, state,
					 (LO_TextStruct*)lo_ele, line_height, baseline);
	  }

	return next;
}

static LO_Element *
lo_rl_FitLineFeed( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
	LO_Element *next = lo_tv_GetNextLayoutElement( relay_state->doc_state, lo_ele, TRUE);

	if (lo_ele->lo_linefeed.break_type == LO_LINEFEED_BREAK_SOFT) {		
		/* Add linefeed to recycle list */
		lo_ele->lo_any.prev = NULL;
		lo_ele->lo_any.next = NULL;
		lo_RecycleElements( relay_state->context, relay_state->top_state->doc_state, lo_ele );
	}
	else if (lo_ele->lo_linefeed.break_type == LO_LINEFEED_BREAK_HARD) {
	  LO_LinefeedStruct *linefeed = (LO_LinefeedStruct*)lo_ele;

	  lo_rl_AppendLinefeedAndFlushLine(relay_state->context, relay_state->doc_state,
					   linefeed, linefeed->break_type, linefeed->clear_type, TRUE, FALSE);


	  switch(linefeed->clear_type)
	    {
	    case LO_CLEAR_TO_LEFT:
	      lo_ClearToLeftMargin(relay_state->context, relay_state->doc_state);
	      break;
	    case LO_CLEAR_TO_RIGHT:
	      lo_ClearToRightMargin(relay_state->context, relay_state->doc_state);
	      break;
	    case LO_CLEAR_TO_ALL:
	    case LO_CLEAR_TO_BOTH:
	      lo_ClearToBothMargins(relay_state->context, relay_state->doc_state);
	      break;
	    case LO_CLEAR_NONE:
	    default:
	      break;
	    }

	  /*
	   * Reset the margins properly in case
	   * we are inside a list.
	   */
	  lo_FindLineMargins(relay_state->context, relay_state->doc_state, TRUE);
	  relay_state->doc_state->x = relay_state->doc_state->left_margin;
	}
	else if (lo_ele->lo_linefeed.break_type == LO_LINEFEED_BREAK_PARAGRAPH) {
	  LO_LinefeedStruct *linefeed = (LO_LinefeedStruct*)lo_ele;
	  
	  lo_rl_AppendLinefeedAndFlushLine(relay_state->context, relay_state->doc_state,
					   linefeed, linefeed->break_type, linefeed->clear_type, TRUE, FALSE);
	}

	return next;
}

static LO_Element *
lo_rl_FitHRule( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
  LO_Element *next = lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, FALSE);

  /* need this or the hrule pops up to the previous line. */
  lo_rl_AddSoftBreakAndFlushLine(relay_state->context, relay_state->doc_state);
  
  lo_StartHorizontalRuleLayout(relay_state->context, relay_state->doc_state, (LO_HorizRuleStruct*)lo_ele);

  lo_AppendToLineList(relay_state->context, relay_state->doc_state, lo_ele, 0);

  lo_UpdateStateAfterHorizontalRule(relay_state->doc_state, (LO_HorizRuleStruct*)lo_ele);

  lo_rl_AddSoftBreakAndFlushLine(relay_state->context, relay_state->doc_state);

  lo_FinishHorizontalRuleLayout(relay_state->context, relay_state->doc_state, (LO_HorizRuleStruct*)lo_ele);

  /* lo_DisplayHR(relay_state->context, (LO_HorizRuleStruct*)lo_ele); */

  return next;
}

static LO_Element *
lo_rl_FitImage( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
	int32 line_inc, baseline_inc;
	LO_Element *next = lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, FALSE);
	
	
	lo_FillInImageGeometry( relay_state->doc_state, (LO_ImageStruct *) lo_ele);
	lo_LayoutInflowImage( relay_state->context, relay_state->doc_state, (LO_ImageStruct *) lo_ele, TRUE, &line_inc, &baseline_inc);
	lo_AppendToLineList(relay_state->context, relay_state->doc_state, lo_ele, baseline_inc);
	lo_UpdateStateAfterImageLayout( relay_state->doc_state, (LO_ImageStruct *)lo_ele, line_inc, baseline_inc );

	return next;
}

static LO_Element *
lo_rl_FitBullet( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{

  LO_Element *next = lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, FALSE);
  int32 line_height, baseline;

  if (lo_ele->lo_bullet.bullet_type == BULLET_MQUOTE) 
    {
      /* Add bullet to recycle list */
      lo_ele->lo_any.prev = NULL;
      lo_ele->lo_any.next = NULL;
      lo_RecycleElements( relay_state->context, relay_state->top_state->doc_state, lo_ele );
    }
  else
    {
      /*
       * Artificially setting state to 2 here
       * allows us to put headers on list items
       * even if they aren't double spaced.
       */
      relay_state->doc_state->linefeed_state = 2;
      
      lo_FormatBullet(relay_state->context, relay_state->doc_state, (LO_BulletStruct*)lo_ele, &line_height, &baseline);
      
      lo_AppendToLineList(relay_state->context, relay_state->doc_state, lo_ele, 0);
      
      lo_UpdateStateAfterBullet(relay_state->context, relay_state->doc_state, (LO_BulletStruct*)lo_ele, line_height, baseline);
      
      /* lo_DisplayBullet(relay_state->context, (LO_BulletStruct*)lo_ele); */
      
      /*  XP_ASSERT(0);*/
    }	    
  
  return next;
}

static LO_Element *
lo_rl_FitList( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
  LO_Element *next = lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, FALSE);
  LO_ListStruct *list = (LO_ListStruct*)lo_ele;
  lo_DocState *state = relay_state->doc_state;
  
  list->lo_any.ele_id = NEXT_ELEMENT;
  list->lo_any.x = state->x;
  list->lo_any.y = state->y;

  lo_AppendToLineList(relay_state->context, relay_state->doc_state, lo_ele, 0);

  if (list->is_end == FALSE)
	lo_SetupStateForList(relay_state->context, relay_state->doc_state,
			     list, TRUE);
  else
	lo_UpdateStateAfterList(relay_state->context, relay_state->doc_state,
				list, TRUE);

  return next;
}

static LO_Element *
lo_rl_FitDescTitle( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
  LO_Element *next = lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, FALSE);
  LO_DescTitleStruct *title = (LO_DescTitleStruct*)lo_ele;
  lo_DocState *state = relay_state->doc_state;
  
  title->lo_any.ele_id = NEXT_ELEMENT;
  title->lo_any.x = state->x;
  title->lo_any.y = state->y;

  lo_AppendToLineList(relay_state->context, relay_state->doc_state, lo_ele, 0);

  lo_ProcessDescTitleElement(relay_state->context, relay_state->doc_state, title, TRUE);

  return next;
}

static LO_Element *
lo_rl_FitDescText( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
  LO_Element *next = lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, FALSE);
  LO_DescTextStruct *text = (LO_DescTextStruct*)lo_ele;
  lo_DocState *state = relay_state->doc_state;
  
  text->lo_any.ele_id = NEXT_ELEMENT;
  text->lo_any.x = state->x;
  text->lo_any.y = state->y;

  lo_AppendToLineList(relay_state->context, relay_state->doc_state, lo_ele, 0);

  lo_ProcessDescTextElement(relay_state->context, relay_state->doc_state, text, TRUE);

  return next;
}

static LO_Element *
lo_rl_FitBlockQuote( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
  LO_Element *next = lo_tv_GetNextLayoutElement( relay_state->doc_state, lo_ele, TRUE);
  lo_DocState *state = relay_state->doc_state;

  lo_ele->lo_any.x = state->x;
  lo_ele->lo_any.y = state->y;
  lo_ele->lo_any.ele_id = NEXT_ELEMENT;

  lo_AppendToLineList(relay_state->context, relay_state->doc_state, lo_ele, 0);
  lo_ProcessBlockQuoteElement(relay_state->context,
			      relay_state->doc_state,
			      (LO_BlockQuoteStruct*)lo_ele,
			      TRUE);

  return next;
}

static LO_Element *
lo_rl_FitFormEle( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
  LO_Element *next = lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, FALSE);
  LO_FormElementStruct *form = (LO_FormElementStruct*)lo_ele;
  int32 baseline_inc = 0;

  lo_LayoutInflowFormElement(relay_state->context, relay_state->doc_state, form, &baseline_inc, TRUE);

  lo_AppendToLineList(relay_state->context, relay_state->doc_state, lo_ele, baseline_inc);

  lo_UpdateStateAfterFormElement(relay_state->context, relay_state->doc_state, form, baseline_inc);

  CL_MoveLayer(form->layer,
			   form->x + form->x_offset + form->border_horiz_space,
			   form->y + form->y_offset + form->border_vert_space);

  return next;
}

/* Only called during relayout.  Sets up current height span struct. */
static void lo_UpdateHeightSpanForBeginRow(lo_TableRec *table)
{
	lo_table_span *span_rec;

	/* Point to current height span struct. */
	if (table->height_span_ptr == NULL)
	{
		table->height_span_ptr = table->height_spans;
	}
	else
	{
		table->height_span_ptr = table->height_span_ptr->next;
	}

	/* Init. current height span struct */
	span_rec = table->height_span_ptr;
	span_rec->dim = 1;
	
	/*
	 * Since min_dim on the heights is never used.
	 * I am appropriating it to do baseline aligning.  It will
	 * start as 0, and eventually be the amount of baseline needed
	 * to align all these cells in this row
	 * by their baselines.
	 */
	span_rec->min_dim = 0;
	span_rec->span = 0;
}


static LO_Element *
lo_rl_FitTable( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
	int32 cols, i;
	lo_TableRec *table = (lo_TableRec *) lo_ele->lo_table.table;
	MWContext *context = relay_state->context;
	lo_DocState *state = relay_state->doc_state;
	LO_Element *next = lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, TRUE);

	XP_ASSERT(table != NULL);
	
	if (table == NULL)
		return next;

#ifndef FAST_TABLES
	/* Skip over top aligned caption element */
	if ((table->caption != NULL) &&
		(table->caption->vert_alignment == LO_ALIGN_TOP))
	{
		next = lo_tv_GetNextLayoutElement(state, next, TRUE);
	}

	/* Call common functions that set up document state and table state
	   for a new table.  All these functions are called from lo_BeginTableAttributes()
	   also. */
	lo_rl_BeginTableRelayout(context, state, table );

	/* Reset row ptr to first row */
	table->row_ptr = table->row_list;

	/* Fit each row */
	do {
		cols = table->row_ptr->cells_in_row;
		lo_UpdateTableStateForBeginRow( table, table->row_ptr );
		lo_UpdateHeightSpanForBeginRow( table );
		/* Fit each cell in the row */
		for (i = 0; i < cols; i++)
		{
			next = lo_rl_FitCell(relay_state, next);
		}
		lo_EndTableRow(context, state, table);
		table->row_ptr = table->row_ptr->next;
	} while (table->row_ptr != NULL);
	
	/* Skip over bottom aligned caption element */
	if ((table->caption != NULL) &&
		(table->caption->vert_alignment != LO_ALIGN_TOP))
	{
		next = lo_tv_GetNextLayoutElement(state, next, TRUE);
	}
	
	lo_EndTable(context, state, table, TRUE);

	
#else
	lo_rl_ReHookupCellStructs(table);
	lo_EndTable(context, state, table, TRUE);
#endif /* FAST_TABLES */
	
	return next;
}

static void lo_rl_ReHookupCellStructs(lo_TableRec *table)
{
	
}

static void lo_rl_BeginTableRelayout( MWContext *context, lo_DocState *state, lo_TableRec *table)
{
	lo_PositionTableElement( state, table->table_ele );
	lo_InitTableRecord( table );
	lo_SetTableDimensions( state, table, state->allow_percent_width, state->allow_percent_height );
	lo_CalcFixedColWidths( context, state, table );
	lo_UpdateStateAfterBeginTable( state, table );
}

static void lo_rl_CopyCellToState( LO_CellStruct *cell, lo_DocState *state )
{
	LO_Element **	line_array;
	
	PA_LOCK(line_array, LO_Element **, state->line_array );
	line_array[0] = cell->cell_list;
	state->float_list = cell->cell_float_list;
}

static void lo_rl_CopyStateToCell( lo_DocState *state, LO_CellStruct *cell)
{
	LO_Element **	line_array;
	
	PA_LOCK(line_array, LO_Element **, state->line_array );

	cell->cell_list = (LO_Element *) line_array[0];
	/* cell->cell_list_end = state->end_last_line; */
	cell->cell_float_list = state->float_list;
}

LO_Element * lo_rl_FitCell( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
	LO_CellStruct *cell = (LO_CellStruct *)lo_ele;
	lo_TableRec *table = (lo_TableRec *) cell->table;
	MWContext *context = relay_state->context;
	lo_DocState *state = relay_state->doc_state;
	LO_Element *first;
	LO_Element *next = lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, TRUE);

	XP_ASSERT( lo_ele->lo_any.type == LO_CELL );
	
	lo_InitForBeginCell((lo_TableRow *)cell->table_row, (lo_TableCell *)cell->table_cell);
	lo_InitSubDocForBeginCell(context, state, table );

	/* Copy over line lists, float lists to substate */
	lo_rl_CopyCellToState( cell, state->sub_state );

	/* Replace current doc. state with substate */
	relay_state->doc_state = state->sub_state;

	/* Fit layout elements on substate */
	first = lo_tv_GetFirstLayoutElement(relay_state->doc_state);
	lo_rl_FitLayoutElements(relay_state, first);

	/* Dummy Floating elements on a new line at the end of a document were not getting put into
	   the line_array.  This code should ensure that anything left on the line_list gets flushed
	   to the line_array. */
	if (relay_state->doc_state->line_list != NULL)
		lo_AppendLineListToLineArray( relay_state->context, relay_state->doc_state, NULL);

	/* Restore relay state to original state */
	lo_rl_CopyStateToCell( relay_state->doc_state, cell );
	relay_state->doc_state = state;

	lo_EndTableCell( context, state->sub_state, TRUE );

	return next;
}

static LO_Element *
lo_rl_FitEmbed( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
  int32 line_inc, baseline_inc;
  LO_Element *next = lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, FALSE);
  LO_EmbedStruct *embed = (LO_EmbedStruct*)lo_ele;

  lo_FillInEmbedGeometry(relay_state->doc_state, embed, TRUE);
  lo_LayoutInflowEmbed(relay_state->context, relay_state->doc_state, embed, TRUE, &line_inc, &baseline_inc);
  lo_AppendToLineList(relay_state->context, relay_state->doc_state, lo_ele, baseline_inc);
  lo_UpdateStateAfterEmbedLayout(relay_state->doc_state, embed, line_inc, baseline_inc);

  return next;
}

#ifdef SHACK
static LO_Element *
lo_rl_FitBuiltin( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
  int32 line_inc, baseline_inc;
  LO_Element *next = lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, FALSE);
  LO_BuiltinStruct *builtin = (LO_BuiltinStruct*)lo_ele;

  lo_FillInBuiltinGeometry(relay_state->doc_state, builtin, TRUE);
  lo_LayoutInflowBuiltin(relay_state->context, relay_state->doc_state, builtin, TRUE, &line_inc, &baseline_inc);
  lo_AppendToLineList(relay_state->context, relay_state->doc_state, lo_ele, baseline_inc);
  lo_UpdateStateAfterBuiltinLayout(relay_state->doc_state, builtin, line_inc, baseline_inc);

  return next;
}
#endif /* SHACK */

static LO_Element *
lo_rl_FitJava( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
  LO_Element *next = lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, FALSE);
#ifdef JAVA
  LO_JavaAppStruct *java_app = (LO_JavaAppStruct*)lo_ele;

  int32 line_inc, baseline_inc;

  lo_FillInJavaAppGeometry( relay_state->doc_state, java_app, TRUE);
  lo_LayoutInflowJavaApp( relay_state->context, relay_state->doc_state, java_app, TRUE, &line_inc, &baseline_inc);
  lo_AppendToLineList(relay_state->context, relay_state->doc_state, lo_ele, baseline_inc);
  lo_UpdateStateAfterJavaAppLayout( relay_state->doc_state, java_app, line_inc, baseline_inc );

  CL_MoveLayer(java_app->objTag.layer,
			   java_app->objTag.x + java_app->objTag.x_offset + java_app->objTag.border_horiz_space,
			   java_app->objTag.y + java_app->objTag.y_offset + java_app->objTag.border_vert_space);
#endif

  return next;
}

static LO_Element *
lo_rl_FitObject( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
  XP_ASSERT(0);
  return lo_tv_GetNextLayoutElement(relay_state->doc_state, lo_ele, FALSE);
}

static LO_Element *
lo_rl_FitParagraph( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
  LO_Element *next = lo_tv_GetNextLayoutElement( relay_state->doc_state, lo_ele, TRUE);
  lo_DocState *state = relay_state->doc_state;

  lo_ele->lo_any.x = state->x;
  lo_ele->lo_any.y = state->y;
  lo_ele->lo_any.ele_id = NEXT_ELEMENT;

  lo_AppendToLineList(relay_state->context, relay_state->doc_state, lo_ele, 0);
  lo_ProcessParagraphElement(relay_state->context,
			     &relay_state->doc_state,
			     (LO_ParagraphStruct*)lo_ele, TRUE);
  return next;
}

static LO_Element *
lo_rl_FitCenter( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
  LO_Element *next = lo_tv_GetNextLayoutElement( relay_state->doc_state, lo_ele, TRUE);
  lo_DocState *state = relay_state->doc_state;

  lo_ele->lo_any.x = state->x;
  lo_ele->lo_any.y = state->y;
  lo_ele->lo_any.ele_id = NEXT_ELEMENT;

  lo_AppendToLineList(relay_state->context, relay_state->doc_state, lo_ele, 0);
  lo_ProcessCenterElement(relay_state->context,
			  &relay_state->doc_state,
			  (LO_CenterStruct*)lo_ele,
			  TRUE);

  return next;
}

static LO_Element *
lo_rl_FitMulticolumn( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{	
	lo_DocState *state = relay_state->doc_state;
	MWContext *context = relay_state->context;
	LO_MulticolumnStruct *multicolElement = (LO_MulticolumnStruct *) lo_ele;
	LO_Element *next = lo_tv_GetNextLayoutElement(  state, lo_ele, TRUE );

	if (multicolElement->is_end == FALSE )
	{
		int32 doc_width;		

		lo_AppendMultiColToLineList(context, state, multicolElement);

		/* if (line will not be flushed by lo_SetSoftLineBreak) and (there exist some elements on the line list) */
		if (state->linefeed_state >= 2 && state->line_list != NULL)
		{
			/* Append zero width and height line feed to the line list and flush the line list into
			   the line array. This forces the layout of elements contained within the MULTICOL tags
			   to start on a blank line_list and hence on a new line.  lo_EndMultiColumn needs this to
			   do its line array hacking properly. */
			lo_AppendZeroWidthAndHeightLF(context, state);			
		}

		lo_SetLineBreakState(context, state, FALSE, LO_LINEFEED_BREAK_SOFT, 2, TRUE);
		if (multicolElement->multicol != NULL)
		{
			LO_LinefeedStruct *linefeedAfterTable = NULL;
			LO_LinefeedStruct *linefeedBeforeTable = NULL;
			LO_Element *lastElementInLastCell;
			LO_CellStruct *lastCellElement;

			/*	Call functions to set up doc state and the multicol structure state that
				get called during initial layout also. */
			lo_StartMultiColInit( state, multicolElement->multicol ) ;
			doc_width = state->right_margin - state->left_margin;
			lo_SetupStateForBeginMulticol( state, multicolElement->multicol, doc_width );

			/*	Traverse all cells in the line list and concatenate their line lists and
				float lists.  Return the first element of the first cell's line list to be
				fitted by the lo_rl_FitLayoutElements() loop */
			XP_ASSERT(next->lo_any.type == LO_LINEFEED);
			linefeedBeforeTable = (LO_LinefeedStruct *) next;
			next = next->lo_any.next;
			
			XP_ASSERT(next->lo_any.type == LO_TABLE);
			if (next->lo_any.type == LO_TABLE)
				linefeedAfterTable = (LO_LinefeedStruct *) lo_rl_RecreateElementListsFromCells(state, (LO_TableStruct *) next,
					&lastElementInLastCell);

			/* Trash all the LO_TABLE and LO_CELL elements that were created to hold MULTICOL contents
			   for the earlier window size. */
			lastCellElement = (struct LO_CellStruct_struct*) linefeedAfterTable->prev;
			lastCellElement->next = NULL;
			next->lo_any.prev = NULL;
			lo_RecycleElements( context, state, next );

			
			/* Insert the state->linelist constructed above in between the linefeeds before and after
			   the table. */
			linefeedBeforeTable->next = state->line_list;
			state->line_list->lo_any.prev = (LO_Element *) linefeedBeforeTable;
			lastElementInLastCell->lo_any.next = (LO_Element *) linefeedAfterTable;
			linefeedAfterTable->prev = lastElementInLastCell;

			/* Set the next layout element to be fitted to the linefeed before the table.  This
			   will fit all the elements that were on the cells of the table */
			next = (LO_Element *) linefeedBeforeTable;

			/* The linelist should be NULL because we the LO_Elements
			   within the multicol tag get laid out on a new line in the line_array */
			state->line_list = NULL;
		}
	}
	else if ( multicolElement->is_end == TRUE )
	{
		if (state->current_multicol != NULL)
		{
			lo_EndMulticolumn(context, state, multicolElement->tag,
				state->current_multicol, TRUE);
			
		}
		lo_AppendMultiColToLineList(context, state, multicolElement);
	}

	return next;
}

static LO_LinefeedStruct * lo_rl_RecreateElementListsFromCells( lo_DocState *state, LO_TableStruct *table, 
															   LO_Element **lastElementInLastCell)
{	
	LO_Element *lineList = state->line_list;
	LO_Element *floatList = state->float_list;
	LO_Element *ele;
		
	/* Point to the first cell element */
	ele = table->next;

	/* Go to last element on line list and float list */
	lineList = lo_GetLastElementInList(lineList);
	floatList = lo_GetLastElementInList(floatList);

	/*	For all cell elements, keep appending the cell line lists and float lists to the doc state's
		line lists and float lists.  Keeps resetting each LO_CELL's line list and float list 
		pointers to NULL. */
	for (  ; ele->lo_any.type == LO_CELL; ele = ele->lo_any.next )
	{
		/* Preconditions: 1) lineList points to the element to which the cell list has to be appended,
						  2) floatList points to the element to which the cell float list has to be appended */
		if (lineList == NULL)
		{
			lineList = ele->lo_cell.cell_list;			
			state->line_list = lineList;
		}
		else
			lineList->lo_any.next = ele->lo_cell.cell_list;

		ele->lo_cell.cell_list = NULL;
		
		if (floatList == NULL)
		{
			floatList = ele->lo_cell.cell_float_list;
			state->float_list = floatList;
		}
		else
			floatList->lo_any.next = ele->lo_cell.cell_float_list;

		ele->lo_cell.cell_float_list = NULL;

		/* Go to last element on line list and float list */
		lineList = lo_GetLastElementInList(lineList);
		floatList = lo_GetLastElementInList(floatList);
	}

	*lastElementInLastCell = lineList;

	/* ele is the first element after the last cell in the table and should be a linefeed*/
	XP_ASSERT(ele->lo_any.type == LO_LINEFEED);
	return (LO_LinefeedStruct *) ele;
}


static LO_Element *
lo_rl_FitFloat( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
	LO_Element *next = lo_tv_GetNextLayoutElement( relay_state->doc_state, lo_ele, TRUE);
	int32 x, y;
	CL_Layer *layer = NULL;

	if (lo_ele->lo_float.float_ele->lo_any.type == LO_IMAGE) {
		LO_ImageStruct *image = (LO_ImageStruct *)lo_ele->lo_float.float_ele;

		lo_LayoutFloatImage( relay_state->context, relay_state->doc_state, image, FALSE );

		/* Determine the new position of the layer. */
		x = image->x + image->x_offset + image->border_width;
		y = image->y + image->y_offset + image->border_width;

		layer = image->layer;
	}
#ifdef JAVA
	else if (lo_ele->lo_float.float_ele->lo_any.type == LO_JAVA) {
		LO_JavaAppStruct *java_app = (LO_JavaAppStruct *)lo_ele->lo_float.float_ele;

		lo_LayoutFloatJavaApp( relay_state->context, relay_state->doc_state, java_app, FALSE );

		/* Determine the new position of the layer. */
		x = java_app->objTag.x + java_app->objTag.x_offset + java_app->objTag.border_width;
		y = java_app->objTag.y + java_app->objTag.y_offset + java_app->objTag.border_width;

		layer = java_app->objTag.layer;
	}
#endif
	else if (lo_ele->lo_float.float_ele->lo_any.type == LO_EMBED) {
		LO_EmbedStruct *embed = (LO_EmbedStruct *)lo_ele->lo_float.float_ele;
		
		lo_LayoutFloatEmbed( relay_state->context, relay_state->doc_state, embed, FALSE );

		/* Determine the new position of the layer. */
		x = embed->objTag.x + embed->objTag.x_offset + embed->objTag.border_width;
		y = embed->objTag.y + embed->objTag.y_offset + embed->objTag.border_width;

		layer = embed->objTag.layer;
	}
#ifdef SHACK
	else if (lo_ele->lo_float.float_ele->lo_any.type == LO_BUILTIN) {
		LO_BuiltinStruct *builtin = (LO_BuiltinStruct *)lo_ele->lo_float.float_ele;
		
		lo_LayoutFloatEmbed( relay_state->context, relay_state->doc_state, 
							 (LO_EmbedStruct *)builtin, FALSE );

		/* Determine the new position of the layer. */
		x = builtin->x + builtin->x_offset + builtin->border_width;
		y = builtin->y + builtin->y_offset + builtin->border_width;

		layer = builtin->layer;
	}
#endif /* SHACK */
	else if (lo_ele->lo_float.float_ele->lo_any.type == LO_TABLE) {
		LO_TableStruct *table_ele = (LO_TableStruct *)lo_ele->lo_float.float_ele;
		lo_DocState *state = relay_state->doc_state;
		LO_Element *save_float_list = state->float_list;
		LO_Element *save_line_list = state->line_list;
		LO_Element *save_elements_after_table = NULL;
		LO_Element *ele = NULL;

		/* We want to layout the table that is in the float list on the doc state in
		   relay_state.  So we need to save the float list and non-TABLE and non-CELL
		   layout elements on the float list.  Then we layout the floating table on
		   the doc state.  Then we restore the float list. */

		/* Table elements are followed by CELL elements.  The end of the table occurs
		   at the first element that is not a CELL.  So step through the element list until 
		   the first non-CELL is encountered */
		LO_Element *prev_ele = (LO_Element *) table_ele;
		for ( ele = table_ele->next; ele->lo_any.type == LO_CELL; ele = ele->lo_any.next )
		{
			prev_ele = ele;
			if (ele->lo_any.next == NULL)
			{
				ele = NULL;
				break;
			}
		}
		
		if (ele != NULL)
		{
			/* Temporarily NULL out the last CELL's next pointer. */
			save_elements_after_table = ele;
			prev_ele->lo_any.next = NULL;
		}
		state->float_list = NULL;
		state->line_list = NULL;
		
		/* Layout the table onto relay_state->doc_state */
		lo_rl_FitTable( relay_state, (LO_Element *)table_ele );

		/* Restore line list, float list */
		state->line_list = save_line_list;
		state->float_list = save_float_list;
		if (save_elements_after_table != NULL)
		{
			prev_ele->lo_any.next = save_elements_after_table;
		}
	}

	/* Reset dummy float element's position and element id before inserting back into the line list */
	lo_ele->lo_any.x = relay_state->doc_state->x;
	lo_ele->lo_any.y = relay_state->doc_state->y;
	lo_ele->lo_any.ele_id = relay_state->top_state->element_id++;

	lo_AppendToLineList(relay_state->context, relay_state->doc_state, lo_ele, 0);

	/* Move layer to new position */
	if (layer != NULL)
	{
		CL_MoveLayer(layer, x, y);
	}

	return next;
}

static LO_Element *
lo_rl_FitTextBlock( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
	LO_Element *	next;
	
	next = lo_RelayoutTextBlock ( relay_state->context, relay_state->doc_state, &lo_ele->lo_textBlock, NULL );
	return next;
}

static LO_Element *
lo_rl_FitLayer( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
	LO_Element *next;
	
	XP_ASSERT(lo_ele->lo_layer.is_end == FALSE);

	/* Skip all elements between the start and end LO_LAYER elements */
	next = lo_ele;
	do 
	{
		next = lo_tv_GetNextLayoutElement( relay_state->doc_state, next, TRUE);
	} while (next != NULL && next->lo_any.type != LO_LAYER);

	if (next->lo_any.type == LO_LAYER) {
		XP_ASSERT(next->lo_layer.is_end == TRUE);
		next = lo_tv_GetNextLayoutElement( relay_state->doc_state, next, TRUE);
	}

	return next;
}


static LO_Element *
lo_rl_FitHeading( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
	XP_ASSERT(0);
	return lo_tv_GetNextLayoutElement( relay_state->doc_state, lo_ele, TRUE);
}

static LO_Element * 
lo_rl_FitSpacer( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
	LO_Element *next = lo_tv_GetNextLayoutElement( relay_state->doc_state, lo_ele, TRUE);
	LO_SpacerStruct *spacer = (LO_SpacerStruct *) lo_ele;
	lo_DocState *state = relay_state->doc_state;
	MWContext *context = relay_state->context;

	/* Put the LO_SPACER element back on the line list */
	spacer->lo_any.x = state->x;
	spacer->lo_any.y = state->y;
	spacer->lo_any.ele_id = state->top_state->element_id++;
	lo_AppendToLineList(context, state, lo_ele, 0);

	/* Relayout the SPACER element */
	lo_LayoutSpacerElement(context, state, spacer, TRUE);

	return next;
}

static LO_Element * lo_rl_FitSpan( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{	
#ifndef DOM
	XP_ASSERT(0);
	return lo_tv_GetNextLayoutElement( relay_state->doc_state, lo_ele, TRUE);
#else
	MWContext *context = relay_state->context;
	lo_DocState *state = relay_state->doc_state;
	LO_Element *next = lo_tv_GetNextLayoutElement( relay_state->doc_state, lo_ele, TRUE);

	/* Skip the SPAN element */
	if (lo_ele->lo_span.is_end == FALSE)
	{				
		lo_AppendToLineList(context, state, lo_ele, 0);
		state->in_span = TRUE;
	}
	else
	{
		state->in_span = FALSE;
		lo_AppendToLineList(context, state, lo_ele, 0);
	}

	return next;
#endif
}

static LO_Element * lo_rl_FitDiv( lo_RelayoutState *relay_state, LO_Element *lo_ele )
{
	XP_ASSERT(0);
	return lo_tv_GetNextLayoutElement( relay_state->doc_state, lo_ele, TRUE);
}

/* 
 * Adds a soft line break to the end of the line list and adds the line list to the line array.  Resets line list to NULL. 
 * Should be used during relayout only.  It is the relayout version of lo_SetSoftLineBreak().
 */
void lo_rl_AddSoftBreakAndFlushLine( MWContext *context, lo_DocState *state )
{
	lo_rl_AddBreakAndFlushLine( context, state, LO_LINEFEED_BREAK_SOFT, LO_CLEAR_NONE, TRUE);
}


void lo_rl_AddBreakAndFlushLine( MWContext *context, lo_DocState *state, int32 break_type, uint32 clear_type, Bool breaking)
{
	LO_LinefeedStruct *linefeed;
	
	linefeed = NULL;
	
	lo_rl_AppendLinefeedAndFlushLine(context, state, linefeed, break_type, clear_type, breaking, TRUE);
}

void lo_rl_AppendLinefeedAndFlushLine( MWContext *context, lo_DocState *state, LO_LinefeedStruct *linefeed, int32 break_type, uint32 clear_type,
									  Bool breaking, Bool createNewLF)
{
	/* flush any text */
	lo_FlushLineBuffer(context, state);

	lo_UpdateStateWhileFlushingLine( context, state );
	
	if (createNewLF)
		linefeed = lo_NewLinefeed(state, context, break_type, clear_type);
	else
		lo_FillInLineFeed( context, state, break_type, clear_type, linefeed );

	if (linefeed != NULL)
	{
		lo_AppendLineFeed( context, state, linefeed, breaking, FALSE );
	}

	lo_UpdateStateAfterFlushingLine( context, state, linefeed, TRUE );
	lo_UpdateStateAfterLineBreak( context, state, FALSE );
	lo_UpdateFEProgressBar(context, state);
}

/* 
 * Append a dummy layout element in the line list.  When the relayout engine
 * will see this dummy element, it will call lo_LayoutFloat{Image,JavaApp,Embed}()
 */
void lo_AppendFloatInLineList( lo_DocState *state, LO_Element *ele, LO_Element *restOfLine)
{
	LO_Element *eptr;
	LO_FloatStruct *float_dummy = XP_NEW_ZAP(LO_FloatStruct);
	float_dummy->float_ele = ele;
	float_dummy->lo_any.type = LO_FLOAT;
	float_dummy->lo_any.ele_id = NEXT_ELEMENT;
	float_dummy->lo_any.x = state->x;
	float_dummy->lo_any.y = state->y;
	float_dummy->lo_any.next = restOfLine;
	float_dummy->lo_any.prev = NULL;

	if (state->line_list == NULL ) {
		state->line_list = (LO_Element *) float_dummy;
	}
	else {
		for (eptr = state->line_list; eptr->lo_any.next != NULL; eptr = eptr->lo_any.next)
			;
		/* eptr now points to the last element in state->line_list */
		eptr->lo_any.next = (LO_Element *) float_dummy;
		float_dummy->lo_any.prev = eptr;
	}
}

void lo_rl_ReflowCell( MWContext *context, lo_DocState *state, LO_CellStruct *cell )
{
	lo_RelayoutState *relay_state;
	LO_Element *first;

	LO_LockLayout();

	/* Copy over cell line lists, float lists to state */
	lo_rl_CopyCellToState( cell, state );

	/* Create relayout state */
	relay_state = lo_rl_CreateRelayoutState();
	relay_state->doc_state = state;
	relay_state->context = context;
	relay_state->top_state = state->top_state;

	/* Fit cell's layout elements */
	first = lo_tv_GetFirstLayoutElement(relay_state->doc_state);
	lo_rl_FitLayoutElements(relay_state, first);

	/* Dummy Floating elements on a new line at the end of a document were not getting put into
	   the line_array.  This code should ensure that anything left on the line_list gets flushed
	   to the line_array. */
	if (state->line_list != NULL)
		lo_AppendLineListToLineArray( context, state, NULL);

	/* Restore cell line list and float list */
	lo_rl_CopyStateToCell( relay_state->doc_state, cell );
	
	/* Done with relayout.  Destroy relayout state. */
	lo_rl_DestroyRelayoutState( relay_state );

	LO_UnlockLayout();

}

void lo_rl_ReflowDocState( MWContext *context, lo_DocState *state )
{
	lo_RelayoutState *relay_state;
	LO_Element *first;

	LO_LockLayout();

	/* Create relayout state */
	relay_state = lo_rl_CreateRelayoutState();
	relay_state->doc_state = state;
	relay_state->context = context;
	relay_state->top_state = state->top_state;

	/* Fit cell's layout elements */
	first = lo_tv_GetFirstLayoutElement(relay_state->doc_state);
	
	lo_rl_FitLayoutElements(relay_state, first);

	/* Dummy Floating elements on a new line at the end of a document were not getting put into
	   the line_array.  This code should ensure that anything left on the line_list gets flushed
	   to the line_array. */
	if (state->line_list != NULL)
		lo_AppendLineListToLineArray( context, state, NULL);

	/* Done with relayout.  Destroy relayout state. */
	lo_rl_DestroyRelayoutState( relay_state );

	LO_UnlockLayout();
}

void lo_PrepareElementForReuse( MWContext *context, lo_DocState *state, LO_Element * eptr,
		ED_Element *edit_element, intn edit_offset)
{
#ifdef EDITOR
	eptr->lo_any.edit_element = NULL;
	eptr->lo_any.edit_offset = 0;

	if( EDT_IS_EDITOR(context) )
    {
        if( lo_EditableElement( eptr->type ) )
	    {
		    if( edit_element == 0 ){
			    edit_element = state->edit_current_element;
			    edit_offset = state->edit_current_offset;
		    }
		    EDT_SetLayoutElement( edit_element, 
								    edit_offset,
								    eptr->type,
								    eptr );
        } else if( edit_element && (eptr->type == LO_TABLE || eptr->type == LO_CELL) )
        {
            /* _TABLE_EXPERIMENT_  SAVE POINTER TO EDIT ELEMENT IN LO STRUCT */
            eptr->lo_any.edit_element = edit_element;

        }
    }
	state->edit_force_offset = FALSE;
#endif
}

