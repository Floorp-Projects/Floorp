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
#include "libi18n.h"
#include "edt.h"
#include "laystyle.h"
#include "laytrav.h"

/*
 * Turn this define on to get the new multibyte parsing code
#define	FAST_MULTI
*/

#define	FAST_EDITOR

#ifdef TEST_16BIT
#define XP_WIN16
#endif /* TEST_16BIT */

#ifdef XP_WIN16
#define SIZE_LIMIT              32000
#endif /* XP_WIN16 */

#ifdef XP_UNIX
#define TEXT_CHUNK_LIMIT	500
#else
#define TEXT_CHUNK_LIMIT	1600
#endif

#ifdef XP_MAC
#define NON_BREAKING_SPACE	0x07
#else
#define NON_BREAKING_SPACE	160
#endif

#ifdef PROFILE
#pragma profile on
#endif

int32 lo_correct_text_element_width(LO_TextInfo *);

static void lo_insert_quote_characters(MWContext *context,
                                       lo_DocState *state);
static LO_TextStruct * lo_new_text_element(MWContext *context,
                                           lo_DocState *state,
                                           ED_Element *edit_element,
                                           intn edit_offset );

void lo_LayoutFormattedText(MWContext *context,
                            lo_DocState *state,
                            LO_TextBlock * block);
void lo_LayoutPreformattedText(MWContext *context,
                               lo_DocState *state,
                               LO_TextBlock * block);
LO_TextBlock * lo_NewTextBlock (MWContext * context,
                                lo_DocState * state,
                                char * text,
                                uint16 formatMode );
int32 lo_compute_text_basline_inc (lo_DocState * state,
                                   LO_TextBlock * block,
                                   LO_TextStruct * text_data );

uint32 lo_FindBlockOffset (LO_TextBlock * block,
                           LO_TextStruct * fromElement );
void lo_RelayoutTextElements (MWContext * context,
                              lo_DocState * state,
                              LO_TextBlock * block,
                              LO_TextStruct * fromElement );
Bool lo_CanUseBreakTable (lo_DocState * state );
Bool lo_UseBreakTable (LO_TextBlock * block );

void lo_AppendTextToBlock (MWContext *context,
                           lo_DocState *state,
                           LO_TextBlock * block,
                           char *text );
void lo_LayoutTextBlock (MWContext * context,
                         lo_DocState * state,
                         Bool flushLastLine );
static void lo_FlushText (MWContext * context,
                          lo_DocState * state );
static void lo_SetupBreakState (LO_TextBlock * block );

Bool lo_GrowTextBlock (LO_TextBlock * block,
                       uint32 length );
static void lo_GetTextParseAtributes (lo_DocState * state,
                                      Bool * multiByte );


#ifdef XP_MAC
#define	kStaticMeasureTextBufferSize	512
static uint16 gMeasureTextBuffer[ kStaticMeasureTextBufferSize ];

/* This needs to go in fe_proto.h - will move it there once we're out
   of the branch */
#define FE_MeasureText(context, text, charLocs) \
			(*context->funcs->MeasureText)(context, text, charLocs)
#endif

#ifdef XP_MAC
Bool gCallNewText = TRUE;
#endif

LO_TextBlock * lo_NewTextBlock ( MWContext * context,
                                 lo_DocState * state,
                                 char * text,
                                 uint16 formatMode )
{
	LO_TextBlock *	block;
	uint32			length;
	
	length = strlen ( text );
	
	block = (LO_TextBlock *)lo_NewElement ( context, state,
                                            LO_TEXTBLOCK, NULL, 0 );
	if ( block == NULL )
		{
		state->top_state->out_of_memory = TRUE;
		return NULL;
		}
		
	block->type = LO_TEXTBLOCK;
	block->x_offset = 0;
	block->ele_id = NEXT_ELEMENT;
	block->x = state->x;
	block->y = state->y;
	block->y_offset = 0;
	block->width = 0;
	block->height = 0;
	block->line_height = 0;
	block->next = NULL;
	block->prev = NULL;
	block->text_attr = NULL;
	block->anchor_href = NULL;
	block->ele_attrmask = 0;
	block->format_mode = 0;
	
	block->startTextElement = NULL;
	block->endTextElement = NULL;
	
	block->text_buffer = NULL;
	block->buffer_length = 0;
	block->buffer_write_index = 0;
	block->last_buffer_write_index = 0;
	block->buffer_read_index = 0;
	block->last_line_break = 0;
	
	block->break_table = NULL;
	block->break_length = 0;
	block->break_write_index = 0;
	block->break_read_index = 0;
	block->last_break_offset = 0;
	
	block->multibyte_index = 0;
	block->multibyte_length = 0;
	
	block->old_break = NULL;
	block->old_break_pos = 0;
	block->old_break_width = 0;
	
	block->totalWidth = 0;
	block->totalChars = 0;
	block->break_pending = 0;
	block->last_char_is_whitespace = 0;
	
	block->ascent = 0;
	block->descent = 0;
	
	block->text_buffer = XP_ALLOC ( length + 1 );
	if ( block->text_buffer == NULL )
		{
		XP_FREE ( block );
		state->top_state->out_of_memory = TRUE;
		return NULL;
		}
	
	block->buffer_length = length + 1;
	XP_BCOPY ( text, block->text_buffer, length );
	block->text_buffer[ length ] = '\0';
	block->buffer_write_index = length;
	
	/* default text does not have a break table */
	block->break_table = NULL;
	block->break_length = 0;
	
	block->format_mode = formatMode;
	
	/* 
     * Since we're creating a new text block, grab some of the text
	 * state out of the layout state.  
     */
	block->anchor_href = state->current_anchor;
	
	if ( state->font_stack != NULL )
		{
		block->text_attr = state->font_stack->text_attr;
		}
		
	if (state->breakable != FALSE)
		{
		block->ele_attrmask |= LO_ELE_BREAKABLE;
		}
	
	state->cur_text_block = block;

	lo_AppendToLineList ( context, state, (LO_Element *) block, 0 );
	
	return block;
}

void
lo_SetDefaultFontAttr(lo_DocState *state, LO_TextAttr *tptr,
	MWContext *context)
{
	tptr->size = DEFAULT_BASE_FONT_SIZE;
	tptr->fontmask = 0;
	tptr->no_background = TRUE;
	tptr->attrmask = 0;
	tptr->fg.red =   STATE_DEFAULT_FG_RED(state);
	tptr->fg.green = STATE_DEFAULT_FG_GREEN(state);
	tptr->fg.blue =  STATE_DEFAULT_FG_BLUE(state);
	tptr->bg.red =   STATE_DEFAULT_BG_RED(state);
	tptr->bg.green = STATE_DEFAULT_BG_GREEN(state);
	tptr->bg.blue =  STATE_DEFAULT_BG_BLUE(state);
	tptr->charset = INTL_DefaultTextAttributeCharSetID(context);
	tptr->font_face = NULL;
	tptr->FE_Data = NULL;
	tptr->point_size = 0;
	tptr->font_weight = 0;
}


/*************************************
 * Function: lo_DefaultFont
 *
 * Description: This function sets up the text attribute
 *      structure for the default text drawing font.
 *      This is the font that sits at the bottom of the font
 *	stack, and can never be popped off.
 *
 * Params: Document state
 *
 * Returns: A pointer to a lo_FontStack structure.
 *	Returns a NULL on error (such as out of memory);
 *************************************/
lo_FontStack *
lo_DefaultFont(lo_DocState *state, MWContext *context)
{
	LO_TextAttr tmp_attr;
	lo_FontStack *fptr;
	LO_TextAttr *tptr;

	/*
	 * Fill in default font information.
	 */
	lo_SetDefaultFontAttr(state, &tmp_attr, context);
	tptr = lo_FetchTextAttr(state, &tmp_attr);

	fptr = XP_NEW(lo_FontStack);
	if (fptr == NULL)
	{
		return(NULL);
	}
	fptr->tag_type = P_UNKNOWN;
	fptr->text_attr = tptr;
	fptr->next = NULL;

	return(fptr);
}

/*
** lo_PushAlignment
**
** Pushes a new alignment entry onto the alignment stack.  Alignment
** entries are tagged with both the actually alignment type (CENTER,
** LEFT, RIGHT, MIDDLE, etc.) as well as the tag that contained the
** alignment attribute.  The latter is so we can accurately lookup the
** alignment for a given tag type..
*/
void
lo_PushAlignment(lo_DocState *state, intn tag_type, int32 alignment)
{
	lo_AlignStack *aptr;

	aptr = XP_NEW(lo_AlignStack);
	if (aptr == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}

	aptr->type = tag_type;
	aptr->alignment = alignment;

	aptr->next = state->align_stack;
	state->align_stack = aptr;
}


/*
** lo_PopAlignment
**
** Pops off the top entry from the alignment stack.
*/
lo_AlignStack *
lo_PopAlignment(lo_DocState *state)
{
	lo_AlignStack *aptr;

	aptr = state->align_stack;
	if (aptr != NULL)
	{
		state->align_stack = aptr->next;
		aptr->next = NULL;
	}
	return(aptr);
}

void
lo_PushLineHeight(lo_DocState *state, int32 height)
{
	lo_LineHeightStack *aptr;

	aptr = XP_NEW(lo_LineHeightStack);
	if (aptr == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}

	aptr->height = height;

	aptr->next = state->line_height_stack;
	state->line_height_stack = aptr;
}

lo_LineHeightStack *
lo_PopLineHeight(lo_DocState *state)
{
	lo_LineHeightStack *aptr;

	aptr = state->line_height_stack;
	if (aptr != NULL)
	{
		state->line_height_stack = aptr->next;
		aptr->next = NULL;
	}
	return(aptr);
}

/*************************************
 * Function: lo_FreshText
 *
 * Description: This function clears out the information from the
 *	document state that refers to text in the midst of being
 *	laid out.  It is called to clear the layout state before
 *	Laying out new text potentially in a new font.
 *
 * Params: Document state structure.
 *
 * Returns: Nothing
 *************************************/
void
lo_FreshText(lo_DocState *state)
{
	state->text_info.max_width = 0;
	state->text_info.ascent = 0;
	state->text_info.descent = 0;
	state->text_info.lbearing = 0;
	state->text_info.rbearing = 0;
	state->line_buf_len = 0;
	state->break_pos = -1;
	state->break_width = 0;
	state->last_char_CR = FALSE;
#ifdef EDITOR
	if( !state->edit_force_offset )
	{
		state->edit_current_offset = 0;
	}
#endif
}


/*************************************
 * Function: lo_new_text_element
 *
 * Description: Create a new text element structure based
 *	on the current text information stored in the document
 *	state.
 *
 * Params: Document state
 *
 * Returns: A pointer to a LO_TextStruct structure.
 *	Returns a NULL on error (such as out of memory);
 *************************************/
static LO_TextStruct *
lo_new_text_element(MWContext *context,
                    lo_DocState *state,
                    ED_Element *edit_element,
                    intn edit_offset )
{
	LO_TextStruct *text_ele = NULL;
#ifdef DEBUG
	assert (state);
#endif

	if (state == NULL)
	{
		return(NULL);
	}

	text_ele = (LO_TextStruct *)lo_NewElement(context, state, LO_TEXT, 
			edit_element, edit_offset);
	if (text_ele == NULL)
	{
#ifdef DEBUG
		assert (state->top_state->out_of_memory);
#endif
		return(NULL);
	}

	text_ele->type = LO_TEXT;
	text_ele->ele_id = NEXT_ELEMENT;
	text_ele->x = state->x;
	text_ele->x_offset = 0;
	text_ele->y = state->y;
	text_ele->y_offset = 0;
	text_ele->width = 0;
	text_ele->height = 0;
	text_ele->next = NULL;
	text_ele->prev = NULL;

	if (state->line_buf_len > 0)
	{
		text_ele->text = PA_ALLOC((state->line_buf_len + 1) *
			sizeof(char));
		if (text_ele->text != NULL)
		{
			char *text_buf;
			char *line;

			PA_LOCK(line, char *, state->line_buf);
			PA_LOCK(text_buf, char *, text_ele->text);
			XP_BCOPY(line, text_buf, state->line_buf_len);
			text_buf[state->line_buf_len] = '\0';
			PA_UNLOCK(text_ele->text);
			PA_UNLOCK(state->line_buf);
			text_ele->text_len = (int16)state->line_buf_len;
		}
		else
		{
			state->top_state->out_of_memory = TRUE;
			/*
			 * free text element && return; it's no different
			 * than if we had run out of memory allocating
			 * the text element
			 */
			lo_FreeElement(context, (LO_Element *)text_ele, FALSE);
			return(NULL);
		}
	}
	else
	{
		text_ele->text = NULL;
		text_ele->text_len = 0;
	}

	text_ele->anchor_href = state->current_anchor;

	if (state->font_stack == NULL)
	{
		text_ele->text_attr = NULL;
	}
	else
	{
		text_ele->text_attr = state->cur_text_block->text_attr;
	}

	text_ele->ele_attrmask = 0;
	if (state->breakable != FALSE)
	{
		text_ele->ele_attrmask |= LO_ELE_BREAKABLE;
	}

	text_ele->sel_start = -1;
	text_ele->sel_end = -1;

	return(text_ele);
}


/*
 * A bogus an probably too expensive function to fill in the
 * textinfo for whatever font is now on the font stack.
 */
MODULE_PRIVATE void
lo_fillin_text_info(MWContext *context, lo_DocState *state)
{
	LO_TextStruct tmp_text;
	PA_Block buff;
	char *str;

	memset (&tmp_text, 0, sizeof (tmp_text));
	buff = PA_ALLOC(1);
	if (buff == NULL)
	{
		return;
	}
	PA_LOCK(str, char *, buff);
	str[0] = ' ';
	PA_UNLOCK(buff);
	tmp_text.text = buff;
	tmp_text.text_len = 1;
	
	/* if we have a text block, use it's font info, otherwise get it
       from the state */
	if ( state->cur_text_block == NULL )
		{
		tmp_text.text_attr = state->font_stack->text_attr;
		}
	else
		{
		tmp_text.text_attr = state->cur_text_block->text_attr;
		}
		
	FE_GetTextInfo(context, &tmp_text, &(state->text_info));
	PA_FREE(buff);
}


/*************************************
 * Function: lo_NewLinefeed
 *
 * Description: Create a new linefeed element structure based
 *	on the current information stored in the document
 *	state.
 *
 * Params: Document state, linefeed type
 *
 * Returns: A pointer to a LO_LinefeedStruct structure.
 *	Returns a NULL on error (such as out of memory);
 *************************************/
LO_LinefeedStruct *
lo_NewLinefeed(lo_DocState *state,
               MWContext * context,
               uint32 break_type,
               uint32 clear_type)
{	
	LO_LinefeedStruct *linefeed = NULL;

	if (state == NULL)
	{
		return(NULL);
	}

	linefeed = (LO_LinefeedStruct *)lo_NewElement(context, state, LO_LINEFEED, NULL, 0);
#ifdef DEBUG
	assert (state);
#endif
	if (linefeed == NULL)
	{
#ifdef DEBUG
		assert (state->top_state->out_of_memory);
#endif
		state->top_state->out_of_memory = TRUE;
		return(NULL);
	}

	lo_FillInLineFeed( context, state, break_type, clear_type, linefeed );

	return(linefeed);
}



/*************************************
 * Function: lo_InsertLineBreak
 *
 * Description: This function forces a linebreak at this time.
 *	Forcing the current text insetion point down one line
 *	and back to the left margin.
 *	It also maintains the linefeed state:
 *		0 - middle of a line.
 *		1 - at left margin below a line of text.
 *		2 - at left margin below a blank line.
 *
 * Params: Window context, Document state, break type and a boolean
 *	That describes whether this is a breaking linefeed or not.
 *	(Breaking linefeed is one inserted just to break a formatted
 *	 line to the current document width.)
 *
 * Returns: Nothing
 *************************************/
void
lo_InsertLineBreak(MWContext *context,
                   lo_DocState *state,
                   uint32 break_type,
                   uint32 clear_type,
                   Bool breaking)
{
	/* int32 line_width; */
	Bool scroll_at_bottom;

	scroll_at_bottom = FALSE;
	/*
	 * If this is an auto-scrolling doc, we will be scrolling
	 * later if we are at the bottom now.
	 */
	if ((state->is_a_subdoc == SUBDOC_NOT)&&
		(state->display_blocked == FALSE)&&
		(state->top_state->auto_scroll > 0))
	{
		int32 doc_x, doc_y;

		FE_GetDocPosition(context, FE_VIEW, &doc_x, &doc_y);
		if ((doc_y + state->win_height + state->win_bottom) >= state->y)
		{
			scroll_at_bottom = TRUE;
		}
	}

	/*
	 * This line is done, flush it into the line table and
	 * display it to the front end.
	 */
	lo_FlushLineList(context, state, break_type, clear_type, breaking);
	if (state->top_state->out_of_memory != FALSE)
	{
		return;
	}

	lo_UpdateStateAfterLineBreak( context, state, TRUE );

	lo_UpdateFEProgressBar(context, state);

	lo_UpdateFEDocSize( context, state );


	/*
	 * Tell the front end how big the document is right now.
	 * Only do this for the top level document.
	 */
	if ((state->display_blocked != FALSE)&&
#ifdef EDITOR
		(!state->edit_relayout_display_blocked)&&
#endif
	   (state->display_blocking_element_y > 0)&&
	   (state->y > (state->display_blocking_element_y + state->win_height)))
	{
		int32 y;

		state->display_blocked = FALSE;
		y = state->display_blocking_element_y;
		state->display_blocking_element_y = 0;
        if (!lo_InsideLayer(state))
		{
            LO_SetDocumentDimensions(context, state->max_width, state->y);
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

	/*
	 * Reset the left and right margins
	 */
	/*
	lo_FindLineMargins(context, state, TRUE);
	state->x = state->left_margin;
	*/
	if ((state->is_a_subdoc == SUBDOC_NOT)&&
	    (state->top_state->auto_scroll > 0)&&
	    ((state->line_num - 1) > state->top_state->auto_scroll))
	{
		Bool redraw;
		int32 old_y, dy, new_y;
		int32 doc_x, doc_y;

		old_y = state->y;
		FE_GetDocPosition(context, FE_VIEW, &doc_x, &doc_y);
		lo_ClipLines(context, state, 1);
		/*
		 * Calculate how much of the top was clipped
		 * off, and if any of that area was in the users
		 * window we need to redraw their window.
		 */
		redraw = FALSE;
		dy = old_y - state->y;
		if (dy >= doc_y)
		{
			redraw = TRUE;
		}
		new_y = doc_y - dy;
		if (new_y < 0)
		{
			new_y = 0;
		}

		FE_SetDocPosition(context, FE_VIEW, doc_x, new_y);
        if (!lo_InsideLayer(state))
		{
            LO_SetDocumentDimensions(context, state->max_width, state->y);
		}

		if (redraw != FALSE)
		{
			FE_ClearView(context, FE_VIEW );
            if (context->compositor)
            {
                    XP_Rect rect;
                    
                    rect.left = 0;
                    rect.top = new_y;
                    rect.right = state->win_width;
                    rect.bottom = new_y + state->win_height;
                    CL_UpdateDocumentRect(context->compositor,
                                          &rect, (PRBool)FALSE);
			}
		}
	}

	if ((state->is_a_subdoc == SUBDOC_NOT)&&
		(state->display_blocked == FALSE)&&
		(state->top_state->auto_scroll > 0))
	{
		int32 doc_x, doc_y;

		FE_GetDocPosition(context, FE_VIEW, &doc_x, &doc_y);
		if (((doc_y + state->win_height) < state->y)&&
			(scroll_at_bottom != FALSE))
		{
			int32 y;

			y = state->y - state->win_height + state->win_bottom;
			if (y < 0)
			{
				y = 0;
			}
			else
			{
				FE_SetDocPosition(context, FE_VIEW, 0, y);
			}
		}
	}

	lo_insert_quote_characters(context, state);
}

/*************************************
 * Function: lo_HardLineBreak
 *
 * Description: This function forces a linebreak at this time.
 *	Forcing the current text insetion point down one line
 *	and back to the left margin.
 *
 * Params: Window context, Document state and a boolean
 *	That describes whether this is a breaking linefeed or not.
 *	(Breaking linefeed is one inserted just to break a formatted
 *	 line to the current document width.)
 *
 * Returns: Nothing
 *************************************/
void
lo_HardLineBreak(MWContext *context, lo_DocState *state, Bool breaking)
{
	lo_InsertLineBreak ( context, state, LO_LINEFEED_BREAK_HARD,
                         LO_CLEAR_NONE, breaking );
}

/*************************************
 * Function: lo_HardLineBreakWithBreak
 *
 * Returns: Nothing
 *************************************/
void
lo_HardLineBreakWithClearType(MWContext *context,
                              lo_DocState *state,
                              uint32 clear_type,
                              Bool breaking)
{
	lo_InsertLineBreak ( context, state, LO_LINEFEED_BREAK_HARD,
                         clear_type, breaking );
}

/*************************************
 * Function: lo_SoftLineBreak
 *
 * Description: This function adds a single soft line break.
 *
 * Params: Window context, Document state and a boolean that
 *	describes if this is a breaking linefeed.
 *
 * Returns: Nothing
 *************************************/
void
lo_SoftLineBreak(MWContext *context, lo_DocState *state, Bool breaking)
{
	lo_InsertLineBreak ( context, state, LO_LINEFEED_BREAK_SOFT,
                         LO_CLEAR_NONE, breaking);
}

/*************************************
 * Function: lo_SetSoftLineBreakState
 *
 * Description: Add soft linebreaks to bring the linefeed state to the
 *  specified level.
 *	Linefeed states:
 *		0 - middle of a line.
 *		1 - at left margin below a line of text.
 *		2 - at left margin below a blank line.
 *
 * Params: Window context, Document state, a boolean that
 *	describes if this is a breaking linefeed, and the desired
 *	linefeed state.
 *
 * Returns: Nothing
 *************************************/
void
lo_SetSoftLineBreakState(MWContext *context,
                         lo_DocState *state,
                         Bool breaking,
                         intn linefeed_state)
{
	lo_SetLineBreakState ( context, state, breaking, LO_LINEFEED_BREAK_SOFT,
                           linefeed_state, FALSE);
}

/*************************************
 * Function: lo_SetLineBreakState
 *
 * Description: Add linebreaks to bring the linefeed state to the
 *  specified level.
 * Linefeed states:
 *		0 - middle of a line.
 *		1 - at left margin below a line of text.
 *		2 - at left margin below a blank line.
 *
 * Params: Window context, Document state, a boolean that
 *	describes if this is a breaking linefeed, and the desired
 *	linefeed state.
 *
 * Returns: Nothing
 *************************************/
void
lo_SetLineBreakState(MWContext *context,
                     lo_DocState *state,
                     Bool breaking,
                     uint32 break_type,
                     intn linefeed_state,
                     Bool relayout)
{
	/*
	 * Linefeeds are partially disabled if we are placing
	 * preformatted text.
	 */
	if (state->preformatted != PRE_TEXT_NO)
	{
		if ((breaking == FALSE)&&(state->linefeed_state < 1)&&
			(state->top_state->out_of_memory == FALSE))
		{
			if (relayout == FALSE) 
			{
				lo_InsertLineBreak(context, state, break_type,
                                   LO_CLEAR_NONE, breaking);				
			}
			else 
			{
				lo_rl_AddBreakAndFlushLine( context, state,
                                            break_type, LO_CLEAR_NONE,
                                            breaking);
			}
		}
		return;
	}

	/* check for the style sheet display: inline property.
 	 * ignore newlines if it is set
	 */
	if(state->top_state && state->top_state->style_stack)
	{
		StyleStruct *style = STYLESTACK_GetStyleByIndex(
												state->top_state->style_stack,
                                                0);
		if(style)
		{
			char * property = STYLESTRUCT_GetString(style, DISPLAY_STYLE);
			if(property && !strcasecomp(property, INLINE_STYLE))
			{
				XP_FREE(property);
				return;
			}
			XP_FREEIF(property);
		}
	}

	if (linefeed_state > 2)
	{
		linefeed_state = 2;
	}

	while ((state->linefeed_state < linefeed_state)&&
		(state->top_state->out_of_memory == FALSE))
	{
		if (relayout == FALSE) 
		{
			lo_InsertLineBreak(context, state, break_type,
                               LO_CLEAR_NONE, breaking);
		}
		else 
		{
			lo_rl_AddBreakAndFlushLine( context, state, break_type,
                                        LO_CLEAR_NONE, breaking );
		}
	}
}

/*************************************
 * Function: lo_InsertWordBreak
 *
 * Description: This insert a word break into the laid out
 *	element chain for the current line.  A word break is
 *	basically an empty text element structure.  All
 *	word break elements should probably be removed
 *	from the layout chain once the line list for this
 *	line is flushed.
 *
 * Params: Window context and document state.
 *
 * Returns: Nothing
 *************************************/
void
lo_InsertWordBreak(MWContext *context, lo_DocState *state)
{
	LO_TextStruct *text_ele = NULL;

	if (state == NULL)
	{
		return;
	}

	text_ele = (LO_TextStruct *)lo_NewElement(context, state,
                                              LO_TEXT, NULL, 0);
	if (text_ele == NULL)
	{
#ifdef DEBUG
		assert (state->top_state->out_of_memory);
#endif
		return;
	}

	text_ele->type = LO_TEXT;
	text_ele->ele_id = NEXT_ELEMENT;
	text_ele->x = state->x;
	text_ele->x_offset = 0;
	text_ele->y = state->y;
	text_ele->y_offset = 0;
	text_ele->width = 0;
	text_ele->height = 0;
	text_ele->next = NULL;
	text_ele->prev = NULL;

	text_ele->block_offset = 0;
	text_ele->doc_width = 0;
	
#if 0
	text_ele->text = PA_ALLOC(sizeof(char));
	if (text_ele->text != NULL)
	{
		PA_LOCK(text_buf, char *, text_ele->text);
		text_buf[0] = '\0';
		PA_UNLOCK(text_ele->text);
	}
	else
	{
		state->top_state->out_of_memory = TRUE;
	}
#else
	text_ele->text = NULL;
#endif
	text_ele->text_len = 0;
	text_ele->anchor_href = state->current_anchor;

	if (state->font_stack == NULL)
	{
		text_ele->text_attr = NULL;
	}
	else
	{
		text_ele->text_attr = state->font_stack->text_attr;
	}

	text_ele->ele_attrmask = 0;
	if (state->breakable != FALSE)
	{
		text_ele->ele_attrmask |= LO_ELE_BREAKABLE;
	}

	text_ele->sel_start = -1;
	text_ele->sel_end = -1;
	text_ele->bullet_type = WORDBREAK;

	lo_AppendToLineList(context, state, (LO_Element *)text_ele, 0);

	state->old_break = text_ele;
	state->old_break_block = state->cur_text_block;
	state->old_break_pos = 0;
	state->old_break_width = 0;
}


int32
lo_baseline_adjust(MWContext *context,
                   lo_DocState * state,
                   LO_Element *ele_list,
                   int32 old_baseline,
                   int32 old_line_height)
{
	LO_Element *eptr;
	int32 baseline;
	int32 new_height;
	int32 baseline_adjust;
	LO_TextInfo text_info;

	baseline = 0;
	new_height = 0;
	eptr = ele_list;
	while (eptr != NULL)
	{
		switch (eptr->type)
		{
			case LO_TEXT:
				FE_GetTextInfo(context, (LO_TextStruct *)eptr,
					&text_info);
				if (text_info.ascent > baseline)
				{
					baseline = text_info.ascent;
				}
				if ((text_info.ascent + text_info.descent) >
					new_height)
				{
					new_height = text_info.ascent +
						text_info.descent;
				}
				break;
			case LO_FORM_ELE:
				if (eptr->lo_form.baseline > baseline)
				{
					baseline = eptr->lo_form.baseline;
				}
				if (eptr->lo_any.height > new_height)
				{
					new_height = eptr->lo_any.height;
				}
				break;
			case LO_IMAGE:
				if ((old_baseline - eptr->lo_any.y_offset) >
					baseline)
				{
					baseline = old_baseline -
						eptr->lo_any.y_offset;
				}
				if ((eptr->lo_image.height +
					(2 * eptr->lo_image.border_width) +
					(2 * eptr->lo_image.border_vert_space))
					> new_height)
				{
					new_height = eptr->lo_image.height +
					 (2 * eptr->lo_image.border_width) +
					 (2 * eptr->lo_image.border_vert_space);
				}
				break;
			case LO_HRULE:
			case LO_BULLET:
				if ((old_baseline - eptr->lo_any.y_offset) >
					baseline)
				{
					baseline = old_baseline -
						eptr->lo_any.y_offset;
				}
				if (eptr->lo_any.height > new_height)
				{
					new_height = eptr->lo_any.height;
				}
				break;
			default:
				break;
		}
		eptr = eptr->lo_any.next;
	}
	baseline_adjust = old_baseline - baseline;
	return(baseline_adjust);
}


/*************************************
 * Function: lo_BreakOldElement
 *
 * Description: This function goes back to a previous
 *	element in the element chain, that is still on the
 *	same line, and breaks the line there, putting the
 *	remaining elements (if any) on the next line.
 *
 * Params: Window context and document state.
 *
 * Returns: Nothing
 *************************************/
void
lo_BreakOldElement(MWContext *context, lo_DocState *state)
{
	char *text_buf;
	char *break_ptr;
	char *word_ptr;
	char *new_buf;
	PA_Block new_block;
	intn word_len;
	int32 save_width;
	int32 old_baseline;
	int32 old_line_height;
	int32 adjust;
	LO_TextStruct *text_data;
	LO_TextStruct *new_text_data;
	LO_Element *tptr;
	int16 charset;
	int multi_byte;
	LO_TextBlock * block;
#ifdef LOCAL_DEBUG
XP_TRACE(("lo_BreakOldElement, flush text.\n"));
#endif /* LOCAL_DEBUG */

	if (state == NULL)
	{
		return;
	}

	/* BRAIN DAMAGE: Make sure the correct element is at the end of
       the list for this block */
	block = state->old_break_block;
	
	if ( block == NULL )
	{
		return;
	}
	
	/* 
     * If this text block is using the new breaktable algorithm,
     * call that code 
     */
	if ( lo_UseBreakTable ( block ) )
		{
		lo_BreakOldTextBlockElement ( context, state );
		return;
		}
		
	charset = block->text_attr->charset;
	multi_byte = (INTL_CharSetType(charset) != SINGLEBYTE);

	/*
	 * Move to the element we will break
	 */
	text_data = state->old_break;

	/*
	 * If there is no text there to break
	 * it is an error.
	 */
	if ( text_data == NULL )
	{
		return;
	}

	/*
	 * Later operations will trash the width field.
	 * So save it now to restore later.
	 */
	save_width = state->width;

	/*
	 * If this buffer is just an empty string, then we are
	 * breaking at a previously inserted word break element.
	 * Knowing that, the breaking is easier, so it is special 
	 * cased here.
	 */
	if (text_data->text == NULL)
	{
		LO_Element *line_ptr;

		/*
		 * Back the state up to this element's
		 * location, break off the rest of the elements
		 * and save them for later.
		 * Flush this line, and insert a linebreak.
		 */
		state->x = text_data->x;
		state->y = text_data->y;
		tptr = text_data->next;
		text_data->next = NULL;
		PA_UNLOCK(text_data->text);
		state->width = text_data->width;
		state->x += state->width;
		lo_SoftLineBreak(context, state, TRUE);

		/*
		 * The remaining elements go on the next line.
		 * The nextline may already have special mail
		 * bullets on it.
		 */
		line_ptr = state->line_list;
		while ((line_ptr != NULL)&&(line_ptr->lo_any.next != NULL))
		{
			line_ptr = line_ptr->lo_any.next;
		}
		if (line_ptr == NULL)
		{
			state->line_list = tptr;
			if (tptr != NULL)
			{
				tptr->lo_any.prev = NULL;
			}
		}
		else
		{
			line_ptr->lo_any.next = tptr;
			if (tptr != NULL)
			{
				tptr->lo_any.prev = line_ptr;
			}
		}

		/*
		 * If there are no elements to place on the next
		 * line, and we have new text buffered,
		 * remove any spaces at the start of that new
		 * text since new lines are not allowed to begin
		 * with whitespace.
		 */
		if ((tptr == NULL)&&(state->line_buf_len != 0))
		{
			char *cptr;
			int32 wlen;

			PA_LOCK(text_buf, char *, state->line_buf);
			cptr = text_buf;
			wlen = 0;
			while ((XP_IS_SPACE(*cptr))&&(*cptr != '\0'))
			{
				cptr++;
				wlen++;
			}
			if (wlen)
			{
				LO_TextStruct tmp_text;

				memset (&tmp_text, 0, sizeof (tmp_text));

				/*
				 * We removed space, move the string up
				 * and recalculate its current width.
				 */
				XP_BCOPY(cptr, text_buf,
					(state->line_buf_len - wlen + 1));
				state->line_buf_len -= (intn) wlen;
				PA_UNLOCK(state->line_buf);
				tmp_text.text = state->line_buf;
				tmp_text.text_len = (int16)state->line_buf_len;
				tmp_text.text_attr =
					block->text_attr;
				FE_GetTextInfo(context, &tmp_text,
					&(state->text_info));

				/*
				 * Override the saved width since we did want
				 * to change the real width in this case.
				 */
				save_width = lo_correct_text_element_width(
					&(state->text_info));
			}
			else
			{
				PA_UNLOCK(state->line_buf);
			}
		}
	}
	/*
	 * We are breaking somewhere in the middle of an old
	 * text element, this will mean splitting it into 2 text
	 * elements, and putting a line break between them.
	 */
	else
	{
		LO_TextInfo text_info;
		int32 base_change;

		PA_LOCK(text_buf, char *, text_data->text);
		
		/*
		 * Locate our break location, and a pointer to
		 * the remaining word (with its preceeding space removed).
		 */
		break_ptr = (char *)(text_buf + state->old_break_pos);
		/*
		 * On multibyte, we often break on character which is 
		 * not space.
		 */ 
		if (multi_byte == FALSE || XP_IS_SPACE(*break_ptr))
		{
			*break_ptr = '\0';
			word_ptr = (char *)(break_ptr + 1);
		}
		else
		{
			word_ptr = (char *)break_ptr;
		}

		/*
		 * Copy the remaining word into its own block.
		 */
		word_len = XP_STRLEN(word_ptr);
		new_block = PA_ALLOC((word_len + 1));
		if (new_block == NULL)
		{
			state->top_state->out_of_memory = TRUE;
			return;
		}
		PA_LOCK(new_buf, char *, new_block);
		XP_BCOPY(word_ptr, new_buf, (word_len + 1));
		new_buf[word_len] = '\0';

		/*
		 * Back the state up to this element's
		 * location, break off the rest of the elements
		 * and save them for later.
		 * Flush this line, and insert a linebreak.
		 */
		state->x = text_data->x;
		state->y = text_data->y;
		tptr = text_data->next;
		text_data->next = NULL;
		if (word_ptr != break_ptr)
			text_data->text_len = text_data->text_len - word_len - 1;
		else
			text_data->text_len = text_data->text_len - word_len;
		FE_GetTextInfo(context, text_data, &text_info);
		state->width = lo_correct_text_element_width(&text_info);
		PA_UNLOCK(text_data->text);
		state->x += state->width;

		/*
		 * Make the split element know its new width.
		 */
		text_data->width = state->width;

		/*
		 * If the element that caused this break has a different
		 * baseline than the element we are breaking, we need to
		 * preserve that difference after the break.
		 */
		base_change = state->baseline -
			(text_data->y_offset + text_info.ascent);

		old_baseline = state->baseline;
		old_line_height = state->line_height;

		/*
		 * Reset element_id so they are still sequencial.
		 */
		state->top_state->element_id = text_data->ele_id + 1;

		/*
		 * If we are breaking an anchor, we need to make sure the
		 * linefeed gets its anchor href set properly.
		 */
		if (text_data->anchor_href != NULL)
		{
			LO_AnchorData *tmp_anchor;

			tmp_anchor = state->current_anchor;
			state->current_anchor = text_data->anchor_href;
			lo_SoftLineBreak(context, state, TRUE);
			state->current_anchor = tmp_anchor;
		}
		else
		{
			lo_SoftLineBreak(context, state, TRUE);
		}

		adjust = lo_baseline_adjust(context, state, tptr,
			old_baseline, old_line_height);
		state->baseline = old_baseline - adjust;
		state->line_height = (intn) old_line_height - adjust;

		/*
		 * If there was really no remaining word, free the
		 * unneeded buffer.
		 */
		if (word_len == 0)
		{
			LO_Element *eptr;
			LO_Element *line_ptr;

			PA_UNLOCK(new_block);
			PA_FREE(new_block);

			line_ptr = state->line_list;
			while ((line_ptr != NULL)&&
				(line_ptr->lo_any.next != NULL))
			{
				line_ptr = line_ptr->lo_any.next;
			}
			if (line_ptr == NULL)
			{
				state->line_list = tptr;
				if (tptr != NULL)
				{
					tptr->lo_any.prev = NULL;
				}
			}
			else
			{
				line_ptr->lo_any.next = tptr;
				if (tptr != NULL)
				{
					tptr->lo_any.prev = line_ptr;
				}
			}

			state->width = 0;

			eptr = tptr;
			while (eptr != NULL)
			{
				eptr->lo_any.ele_id = NEXT_ELEMENT;
				eptr->lo_any.y_offset -= adjust;
				eptr = eptr->lo_any.next;
			}
		}
		/*
		 * Else create a new text element for the remaining word.
		 * and stick it in the begining of the next line of
		 * text elements.
		 */
		else
		{
			LO_Element *line_ptr;
			LO_Element *eptr;
			int32 baseline_inc;
			LO_TextInfo text_info;
			baseline_inc = -1 * adjust;
			new_text_data = lo_new_text_element(context, state, 
					text_data->edit_element,
                    text_data->edit_offset+text_data->text_len+1 );
			if (new_text_data == NULL)
			{
				return;
			}
			if (new_text_data->text != NULL)
			{
				PA_FREE(new_text_data->text);
				new_text_data->text = NULL;
				new_text_data->text_len = 0;
			}
			new_text_data->anchor_href = text_data->anchor_href;
			new_text_data->text_attr = text_data->text_attr;
			new_text_data->x = state->x;
			new_text_data->y = state->y;
				
#ifdef LOCAL_DEBUG
XP_TRACE(("lo_BreakOldElement, left over word (%s)\n", new_buf));
#endif /* LOCAL_DEBUG */

			PA_UNLOCK(new_block);
			new_text_data->text = new_block;
			new_text_data->text_len = word_len;
			FE_GetTextInfo(context, new_text_data, &text_info);
			new_text_data->width =
				lo_correct_text_element_width(&text_info);

			/*
			 * Some fonts (particulatly italic ones with curly
			 * tails on letters like 'f') have a left bearing
			 * that extends back into the previous character.
			 * Since in this case the previous character is
			 * probably not in the same font, we move forward
			 * to avoid overlap.
			 */
			if (text_info.lbearing < 0)
			{
				new_text_data->x_offset =
					text_info.lbearing * -1;
			}

			/*
			 * The baseline of the text element just inserted in
			 * the line may be less than or greater than the
			 * baseline of the rest of the line due to font
			 * changes.  If the baseline is less, this is easy,
			 * we just increase y_offest to move the text down
			 * so the baselines line up.  For greater baselines,
			 * we can't move the text up to line up the baselines
			 * because we will overlay the previous line, so we
			 * have to move all rest of the elements in this line
			 * down.
			 *
			 * If the baseline is zero, we are the first element
			 * on the line, and we get to set the baseline.
			 */
			if (state->baseline == 0)
			{
				state->baseline = text_info.ascent;
			}
			else if (text_info.ascent < state->baseline)
			{
				new_text_data->y_offset = state->baseline -
						text_info.ascent;
			}
			else
			{
				baseline_inc = baseline_inc +
					(text_info.ascent - state->baseline);
				state->baseline =
					text_info.ascent;
			}

			/*
			 * Now that we have broken, and added the new
			 * element, we need to move it down to restore the
			 * baseline difference that previously existed.
			 */
			new_text_data->y_offset -= base_change;

			/*
			 * Calculate the height of this new
			 * text element.
			 */
			new_text_data->height = text_info.ascent +
				text_info.descent;
			state->x += new_text_data->width;

			/*
			 * Stick this new text element at the beginning
			 * of the remaining line elements
			 * There may be some special mail bullets already
			 * on the line that we have to insert after.
			 */
			line_ptr = state->line_list;
			while ((line_ptr != NULL)&&
				(line_ptr->lo_any.next != NULL))
			{
				line_ptr = line_ptr->lo_any.next;
			}
			if (line_ptr == NULL)
			{
				state->line_list = (LO_Element *)new_text_data;
				new_text_data->prev = NULL;
			}
			else
			{
				line_ptr->lo_any.next =
					(LO_Element *)new_text_data;
				new_text_data->prev = line_ptr;
			}
			new_text_data->next = tptr;
			if (tptr != NULL)
			{
				tptr->lo_any.prev = (LO_Element *)new_text_data;
			}

			eptr = tptr;

			while (eptr != NULL)
			{
				eptr->lo_any.ele_id = NEXT_ELEMENT;
				eptr->lo_any.y_offset += baseline_inc;
				eptr = eptr->lo_any.next;
			}

			if ((new_text_data->y_offset + new_text_data->height) > 
				state->line_height)
			{
				state->line_height = (intn) new_text_data->y_offset +
					new_text_data->height;
			}

			state->at_begin_line = FALSE;
		}
	}

	/*
	 * If we are at the beginning of a line, and there is
	 * remaining text to place here, remove leading space
	 * which is not allowed at the start of lines.
	 * ERIC, make a test case for this, I suspect right now
	 * this code is never being executed and may contain an error.
	 */
	if ((state->at_begin_line != FALSE)&&(tptr != NULL)&&
		(tptr->type == LO_TEXT))
	{
		char *cptr;
		int32 wlen;
		LO_TextStruct *tmp_text;
		LO_TextInfo text_info;

		tmp_text = (LO_TextStruct *)tptr;

		PA_LOCK(text_buf, char *, tmp_text->text);
		cptr = text_buf;
		wlen = 0;
		while ((XP_IS_SPACE(*cptr))&&(*cptr != '\0'))
		{
			cptr++;
			wlen++;
		}
		if (wlen)
		{
			XP_BCOPY(cptr, text_buf,
				(tmp_text->text_len - wlen + 1));
			tmp_text->text_len -= (intn) wlen;

			PA_UNLOCK(tmp_text->text);
			FE_GetTextInfo(context, tmp_text, &text_info);
			tmp_text->width = lo_correct_text_element_width(
				&text_info);
		}
		else
		{
			PA_UNLOCK(tmp_text->text);
		}
	}

	/*
	 * Upgrade forward the x and y text positions in the document
	 * state.
	 */
	while (tptr != NULL)
	{
		lo_UpdateElementPosition ( state, tptr );
		tptr = tptr->lo_any.next;
	}

	state->at_begin_line = FALSE;
	state->width = save_width;
}


void lo_UpdateElementPosition ( lo_DocState * state, LO_Element * element )
{
	element->lo_any.x = state->x;
	element->lo_any.y = state->y;
	state->x = state->x + element->lo_any.width;
	
	/* move any element specific items */
	switch ( element->lo_any.type )
		{
		case LO_IMAGE:
			CL_MoveLayer(element->lo_image.layer,
                         element->lo_any.x, element->lo_any.y);
			break;
			
		case LO_EMBED:
			CL_MoveLayer(element->lo_embed.layer,
                         element->lo_any.x, element->lo_any.y);
			break;
		}
}


/*************************************
 * Function: lo_correct_text_element_width
 *
 * Description: Calculate the correct width of this text element
 *	if it is a complete element surrounded by elements of potentially
 *	different fonts, so we have to take care not to truncate
 *	any slanted characters at either end of the element.
 *
 * Params: LO_TextInfo structure for this text element's text string.
 *
 * Returns: The width this element would have if it stood alone.
 *************************************/
int32
lo_correct_text_element_width(LO_TextInfo *text_info)
{
	int32 x_offset;
	int32 width;

	width = text_info->max_width;
	x_offset = 0;

	/*
	 * For text that leans into the previous character.
	 */
	if (text_info->lbearing < 0)
	{
		x_offset = text_info->lbearing * -1;
		width += x_offset;
	}

	/*
	 * For text that leans right into the following characters.
	 */
	if (text_info->rbearing > text_info->max_width)
	{
		width += (text_info->rbearing - text_info->max_width);
	}

	return(width);
}


PRIVATE
int32
lo_characters_in_line(lo_DocState *state)
{
	int32 cnt;
	LO_Element *eptr;

	cnt = 0;
	eptr = state->line_list;
	while (eptr != NULL)
	{
		if (eptr->type == LO_TEXT)
		{
			cnt += eptr->lo_text.text_len;
		}
		eptr = eptr->lo_any.next;
	}
	return(cnt);
}

void
lo_PreformatedText(MWContext *context, lo_DocState *state, char *text)
{
	LO_TextBlock *	block;
	
	block = lo_NewTextBlock ( context, state, text, state->preformatted );
	if ( !state->top_state->out_of_memory && ( block != NULL ) )
		{
		block->buffer_read_index = 0;
		lo_LayoutPreformattedText ( context, state, block );
		}
}


void
lo_LayoutPreformattedText(MWContext *context,
                          lo_DocState *state,
                          LO_TextBlock * block)
{
	char *tptr;
	char *w_start;
	char *w_end;
	char *text_buf;
	char tchar1;
	Bool have_CR;
	Bool line_break;
	Bool white_space;
	LO_TextStruct text_data;
	char *tmp_buf;
	PA_Block tmp_block;
	int32 tab_count, ignore_cnt, line_length;
	int16 charset;
	int multi_byte;
	int kinsoku_class, last_kinsoku_class;
	int i;
	int bytestocopy;
	char * text;
	Bool lineBufMeasured;
	
	/* start at the current text position in this text block */
	text = (char *) &block->text_buffer[ block->buffer_read_index ];
	
	kinsoku_class = PROHIBIT_NOWHERE;
	
	lineBufMeasured = FALSE;
	
	/*
	 * Initialize the structures to 0 (mark)
	 */
	memset (&text_data, 0, sizeof (LO_TextStruct));

	/*
	 * Error conditions
	 */
	if ((state == NULL)||(state->cur_ele_type != LO_TEXT)||(text == NULL))
	{
		return;
	}

	charset = block->text_attr->charset;
	multi_byte = (INTL_CharSetType(charset) != SINGLEBYTE);

	/*
	 * Move through this text fragment, expand tabs, honor LF/CR.
	 */
	have_CR = state->last_char_CR;
	tptr = text;
	while ((*tptr != '\0')&&(state->top_state->out_of_memory == FALSE))
	{
		Bool has_nbsp;
		Bool in_word;
		Bool wrap_break;
		Bool pre_wrap_break;

		/*
		 * white_space is a tag to tell us if the current word
		 * ends in whitespace.
		 */
		white_space = FALSE;
		line_break = FALSE;
		has_nbsp = FALSE;
		if (multi_byte)
			has_nbsp = TRUE;

		/*
		 * Find the end of the line, counting tabs.
		 */
		tab_count = 0;
		ignore_cnt = 0;
		line_length = 0;
		w_start = tptr;

		/*
		 * If the last character processed was a CR, and the next
		 * char is a LF, ignore it.  Otherwise we know we
		 * can break the line on the first CR or LF found.
		 */
		if ((have_CR != FALSE)&&(*tptr == LF))
		{
			ignore_cnt++;
			tptr++;
		}
		have_CR = FALSE;

		in_word = FALSE;
		wrap_break = FALSE;
		pre_wrap_break = FALSE;
		
#ifdef XP_WIN16
		while ((*tptr != CR)&&(*tptr != LF)&&(*tptr != '\0')&&
#ifdef TEXT_CHUNK_LIMIT
		    ((line_length + (tab_count * state->tab_stop)) < TEXT_CHUNK_LIMIT)&&
#endif /* TEXT_CHUNK_LIMIT */
		    (((state->line_buf_len + line_length + (tab_count * state->tab_stop))) < SIZE_LIMIT))
#else
		while ((*tptr != CR)&&(*tptr != LF)&&(*tptr != '\0')
#ifdef TEXT_CHUNK_LIMIT
		    &&((line_length + (tab_count * state->tab_stop)) < TEXT_CHUNK_LIMIT)
#endif /* TEXT_CHUNK_LIMIT */
			)
#endif /* XP_WIN16 */
		{
			/*
			 * In the special wrapping preformatted text
			 * we need to chunk by word instead of by
			 * line.
			 */
			if ((state->preformatted == PRE_TEXT_WRAP)||
			    (state->preformatted == PRE_TEXT_COLS))
			{
				if(multi_byte && (! (INTL_CharSetType(charset) & CS_SPACE)))
				{
					last_kinsoku_class = kinsoku_class;
					kinsoku_class = INTL_KinsokuClass(charset, (unsigned char *)tptr);
					/* We need to conser PROHIBIT_WORD_BREAK for UTF8 case */
					if(( PROHIBIT_WORD_BREAK == kinsoku_class) || (0x00 == (*tptr & 0x80)))
					{
						if ((in_word == FALSE)&&(!XP_IS_SPACE(*tptr)) )
						{
							in_word = TRUE;
						}
						else if ((in_word != FALSE)&&
							(XP_IS_SPACE(*tptr)))
						{
							wrap_break = TRUE;
							break;
						}
					}
					else
					{
						if( (line_length != 0) &&
							(PROHIBIT_END_OF_LINE != last_kinsoku_class) &&
							(PROHIBIT_BEGIN_OF_LINE != kinsoku_class)
						  )
						{
							wrap_break = TRUE;
							break;
						}
					}
				}
				else
				{
					if ((in_word == FALSE)&&(!XP_IS_SPACE(*tptr)))
					{
						in_word = TRUE;
					}
					else if ((in_word != FALSE)&&
						(XP_IS_SPACE(*tptr)))
					{
						wrap_break = TRUE;
						break;
					}
				}
			}

			if ((*tptr == FF)||(*tptr == VTAB))
			{
				/*
				 * Ignore the form feeds
				 * thrown in by some platforms.
				 * Ignore vertical tabs since we don't know
				 * what else to do with them.
				 */
				ignore_cnt++;
			}
			else if (*tptr == TAB)
			{
				tab_count++;
			}
			else if (!multi_byte && (unsigned char)*tptr == NON_BREAKING_SPACE)
			{
				/* *tptr = ' '; Replace this later */
				has_nbsp = TRUE;
				line_length++;
			}
			else
			{
				if(multi_byte)
					line_length += INTL_CharLen(charset, (unsigned char*)tptr);
				else
					line_length++;
			}

			if(multi_byte)
				tptr = INTL_NextChar(charset, tptr);
			else
				tptr++;
		}
		line_length = line_length + (state->tab_stop * tab_count);

#ifdef TEXT_CHUNK_LIMIT
		if ((state->line_buf_len + line_length) > TEXT_CHUNK_LIMIT)
		{
			lo_FlushLineBuffer(context, state);
			if (state->cur_ele_type != LO_TEXT)
			{
				lo_FreshText(state);
				state->cur_ele_type = LO_TEXT;
			}
		}
#endif /* TEXT_CHUNK_LIMIT */

#ifdef XP_WIN16
		if ((state->line_buf_len + line_length) >= SIZE_LIMIT)
		{
			line_break = TRUE;
		}
#endif /* XP_WIN16 */

		/*
		 * Terminate the word, saving the char we replaced
		 * with the terminator so it can be restored later.
		 */
		w_end = tptr;
		tchar1 = *w_end;
		*w_end = '\0';

		tmp_block = PA_ALLOC(line_length + 1);
		if (tmp_block == NULL)
		{
			*w_end = tchar1;
			state->top_state->out_of_memory = TRUE;
			break;
		}
		PA_LOCK(tmp_buf, char *, tmp_block);

		if ((tab_count)||(ignore_cnt))
		{
			char *cptr;
			char *text_ptr;
			int32 cnt;

			text_ptr = tmp_buf;
			cptr = w_start;
			cnt = lo_characters_in_line(state);
			cnt += state->line_buf_len;
			while (*cptr != '\0')
			{
				if ((*cptr == LF)||(*cptr == FF)||
					(*cptr == VTAB))
				{
					/*
					 * Ignore any linefeeds that must have
					 * been after CR, and form feeds.
					 * Ignore vertical tabs since we
					 * don't know what else to do with them.
					 */
					cptr++;
				}
				else if (*cptr == TAB)
				{
					int32 i, tab_pos;

					tab_pos = ((cnt / state->tab_stop) +
						1) * state->tab_stop;
					for (i=0; i<(tab_pos - cnt); i++)
					{
						*text_ptr++ = ' ';
					}
					cnt = tab_pos;
					cptr++;
				}
				else
				{
					/*
					 * Bug #77467
					 * If multibyte, character != char by default, so copy 
					 * the CHARACTER, not the char
					 */
					if(multi_byte) 
					{
						bytestocopy = INTL_CharLen(charset, (unsigned char*)cptr);
						for (i=0; i<bytestocopy; i++)
						{
							*text_ptr++ = *cptr++;
							cnt++;
						}
					}
					else
					{
						*text_ptr++ = *cptr++;
						cnt++;
					}
				}
			}
			*text_ptr = *cptr;
		}
		else
		{
			XP_BCOPY(w_start, tmp_buf, line_length + 1);
		}

		/*
		 * Now we catch those nasty non-breaking space special
		 * characters and make them spaces.
		 */
		if (has_nbsp != FALSE)
		{
			char *tmp_ptr;

			tmp_ptr = tmp_buf;
			while (*tmp_ptr != '\0')
			{
				if (((unsigned char)*tmp_ptr == NON_BREAKING_SPACE)
					&& (CS_USER_DEFINED_ENCODING != charset))
				{
					*tmp_ptr = ' ';
				}
				if(multi_byte)
					tmp_ptr = INTL_NextChar(charset, tmp_ptr);
				else
					tmp_ptr++;
			}
		}

		/* don't need this any more since we're converting
		 * elsewhere -- erik
		tmp_buf = FE_TranslateISOText(context, charset, tmp_buf);
		 */
		line_length = XP_STRLEN(tmp_buf);

		if ((line_length > 0)&&(XP_IS_SPACE(tmp_buf[line_length - 1])))
		{
			state->trailing_space = TRUE;
		}
#ifdef LOCAL_DEBUG
XP_TRACE(("Found Preformatted text (%s)\n", tmp_buf));
#endif /* LOCAL_DEBUG */
		PA_UNLOCK(tmp_block);

#if WHAT
		/*
		 * If this is an empty string, just throw it out
		 * and move on
		 */
		if (*w_start == '\0')
		{
			*w_end = tchar1;
#ifdef LOCAL_DEBUG
XP_TRACE(("Throwing out empty string!\n"));
#endif /* LOCAL_DEBUG */
			continue;
		}
#endif

		/*
		 * If we have extra text, Append it to the line buffer.
		 * It may be necessary to expand the line
		 * buffer.
		 */
		if (*w_start != '\0')
		{
			int32 old_len;
			Bool old_begin_line;

			old_len = state->line_buf_len;
			old_begin_line = state->at_begin_line;

			if ((state->line_buf_len + line_length + 1) >
				state->line_buf_size)
			{
				state->line_buf = PA_REALLOC(
					state->line_buf, (state->line_buf_size +
					line_length + LINE_BUF_INC));
				if (state->line_buf == NULL)
				{
					*w_end = tchar1;
					state->top_state->out_of_memory = TRUE;
					break;
				}
				state->line_buf_size += (line_length +
					LINE_BUF_INC);
			}
			PA_LOCK(text_buf, char *, state->line_buf);
			PA_LOCK(tmp_buf, char *, tmp_block);

			XP_BCOPY(tmp_buf,
			    (char *)(text_buf + state->line_buf_len),
			    (line_length + 1));
			state->line_buf_len += (intn) line_length;
			PA_UNLOCK(state->line_buf);
			PA_UNLOCK(tmp_block);

			/* we have not measured this new text yet */
			lineBufMeasured = FALSE;
			
			/*
			 * Having added text, we cannot be at the start
			 * of a line
			 */
			state->cur_ele_type = LO_TEXT;
			state->at_begin_line = FALSE;

#ifdef OLD_WAY
			/*
			 * Most common case is appending to the same line.
			 * assume that is what we are doing here.
			 */
			text_data.text = state->line_buf;
			text_data.text_len = (int16)state->line_buf_len;
			text_data.text_attr = block->text_attr;
			FE_GetTextInfo(context, &text_data,
				&(state->text_info));
			state->width =
				lo_correct_text_element_width(
				&(state->text_info));

			/*
			 * If this is a special wrapping pre, and we would
			 * wrap here, break before this, and strip all
			 * following whitespace so there is none at
			 * the start of the next line.
			 * If we were at the beginning of the line before
			 * this, then obviously trying to wrap here will
			 * be pointless, and will in fact cause an
			 * infinite loop.
			 *
			 * Also wrap here is we are in fixed column wrapping
			 * pre text, and we would pass our set column.
			 */
			if (((state->preformatted == PRE_TEXT_WRAP)&&
			    (old_begin_line == FALSE)&&
			    ((state->x + state->width) > state->right_margin))||
			   ((state->preformatted == PRE_TEXT_COLS)&&
			    (old_begin_line == FALSE)&&
			    (state->preformat_cols > 0)&&
			    (state->line_buf_len > state->preformat_cols)))
			{
				PA_LOCK(text_buf, char *, state->line_buf);
				text_buf[old_len] = '\0';
				PA_UNLOCK(state->line_buf);
				state->line_buf_len = old_len;

				*w_end = tchar1;
				tptr = w_start;
				while ((XP_IS_SPACE(*tptr))&&(*tptr != '\0'))
				{
					tptr++;
				}
				line_break = TRUE;
				w_start = tptr;
				w_end = tptr;
				tchar1 = *w_end;
			}
#else

			text_data.text = state->line_buf;
			text_data.text_len = (int16)state->line_buf_len;
			text_data.text_attr = block->text_attr;
			FE_GetTextInfo ( context, &text_data, &(state->text_info) );

			/* update the block's font info cache */
			block->ascent = state->text_info.ascent;
			block->descent = state->text_info.descent;

			/*
			 * If this is a special wrapping pre, then we need to measure this line of text to see
			 * if we need to wrap.
			 */
			if ( ( state->preformatted == PRE_TEXT_WRAP ) && ( old_begin_line == FALSE ) )
			{
				state->width = lo_correct_text_element_width ( &(state->text_info) );
				
				lineBufMeasured = TRUE;
				
				/*
				 * If this line is now too long, wrap
				 */
				if ( ( state->x + state->width ) > state->right_margin )
				{
					pre_wrap_break = TRUE;
				}
			}
			
			/*
			 * Now check to see if we need to wrap based on being too long for the special pre modes
			 */
			if ( ( pre_wrap_break != FALSE ) ||
				(	( state->preformatted == PRE_TEXT_COLS ) &&
					( old_begin_line == FALSE ) &&
					( state->preformat_cols > 0 ) &&
					( state->line_buf_len > state->preformat_cols ) ))
			{
				PA_LOCK(text_buf, char *, state->line_buf);
				text_buf[old_len] = '\0';
				PA_UNLOCK(state->line_buf);
				state->line_buf_len = old_len;

				*w_end = tchar1;
				tptr = w_start;
				while ((XP_IS_SPACE(*tptr))&&(*tptr != '\0'))
				{
					tptr++;
				}
				line_break = TRUE;
				w_start = tptr;
				w_end = tptr;
				tchar1 = *w_end;
			}
#endif
		}

		if (tchar1 == LF)
		{
			line_break = TRUE;
		}
		else if (tchar1 == CR)
		{
			line_break = TRUE;
			have_CR = TRUE;
		}

		/* update the buffer position to refleft the new word */
		block->buffer_read_index = tptr - (char *) block->text_buffer;
		
		/*
		 * If we are breaking the line here, flush the
		 * line_buf, and then insert a linebreak.
		 */
		if (line_break != FALSE)
		{
#ifdef LOCAL_DEBUG
XP_TRACE(("LineBreak, flush text.\n"));
#endif /* LOCAL_DEBUG */

			/*
			 * Flush the line and insert the linebreak.
			 */
			PA_LOCK(text_buf, char *, state->line_buf);
			text_data.text = state->line_buf;
			text_data.text_len = (int16)state->line_buf_len;
			text_data.text_attr = block->text_attr;
			FE_GetTextInfo(context, &text_data,&(state->text_info));
			PA_UNLOCK(state->line_buf);
			state->width = lo_correct_text_element_width(
				&(state->text_info));

			lo_FlushLineBuffer(context, state);

			lineBufMeasured = TRUE;
#ifdef EDITOR
	/* LTNOTE: do something here like: */
			/*state->edit_current_offset += (word_ptr - text_buf);*/
#endif
			if (state->top_state->out_of_memory != FALSE)
			{
				PA_FREE(tmp_block);
				return;
			}
			/*
			 * Put on a linefeed element.
			 * This line is finished and will be added
			 * to the line array.
			 */
			lo_SoftLineBreak(context, state, TRUE);

			state->line_buf_len = 0;
			state->width = 0;

			/*
			 * having just broken the line, we have no break
			 * position.
			 */
			state->break_pos = -1;
			state->break_width = 0;
		}

		*w_end = tchar1;
		if ((*tptr == CR)||(*tptr == LF))
		{
			tptr++;
		}
		PA_FREE(tmp_block);
	}

#ifndef OLD_WAY
	/*
	 * If we haven't measured this line of text yet, do so now
	 */
	if ( !lineBufMeasured )
	{
		text_data.text = state->line_buf;
		text_data.text_len = (int16)state->line_buf_len;
		text_data.text_attr = block->text_attr;
		FE_GetTextInfo ( context, &text_data, &(state->text_info) );
		state->width = lo_correct_text_element_width ( &(state->text_info) );
	}
#endif

	/*
	 * Because we just might get passed text broken between the
	 * CR and the LF, we need to save this state.
	 */
	if ((tptr > text)&&(*(tptr - 1) == CR))
	{
		state->last_char_CR = TRUE;
	}
	
	if ( ( state->cur_ele_type != LO_TEXT ) || ( state->line_buf_len == 0 ) )
		{
		state->cur_text_block = NULL;
		}
}

#define CAPITALIZE 0
#define UPPERCASE  1
#define LOWERCASE  2

/* transform the text inline.
 * "capitalize" : uppercase first letter of each word.
 * "uppercase" : uppercase every letter
 * "lowercase" : lowercase every letter
 * else : do nothing
 */
PRIVATE void
lo_transform_text(char *ptr, int method)
{
	XP_Bool possible_first_letter = TRUE;

	for(; *ptr; ptr++)
	{
		switch(method)
		{
		case CAPITALIZE:
				if(!(XP_IS_SPACE(*ptr)))
				{
					if(possible_first_letter)
					{
						*ptr = XP_TO_UPPER(*ptr);
						possible_first_letter = FALSE;
					}
				}
				else /* is a space */
				{
					possible_first_letter = TRUE;
				}
				break;

			case UPPERCASE:
				*ptr = XP_TO_UPPER(*ptr);
				break;

			case LOWERCASE:
				*ptr = XP_TO_LOWER(*ptr);
				break;

			default:
				XP_ASSERT(0);

		}
	}
}

/* see lo_transform_text
 *
 * This function just maps the string method to an int
 */
PRIVATE void
lo_transform_text_from_string_method(char *ptr, char *method)
{
	if(!strcasecomp(method, "capitalize"))
		lo_transform_text(ptr, CAPITALIZE);
	else if(!strcasecomp(method, "lowercase"))
		lo_transform_text(ptr, LOWERCASE);
	else if(!strcasecomp(method, "uppercase"))
		lo_transform_text(ptr, UPPERCASE);
}

/*************************************
 * Function: lo_FormatText
 *
 * Description: This function creates a text block element and then calls the
 *  format text function for it.
 *
 * Params: Window context and document state., and the text to be formatted.
 *
 * Returns: Nothing
 *************************************/
void
lo_FormatText(MWContext *context, lo_DocState *state, char *text)
{
	LO_TextBlock *	block;
	
	/* can we use the new style layout? */
	if ( lo_CanUseBreakTable ( state ) )
		{
		block = state->cur_text_block;
		
		/* flush any existing text in a partial buffer */
		if ( ( block != NULL ) && lo_UseBreakTable ( block ) )
			{
			lo_LayoutTextBlock ( context, state, TRUE );
			}
		
		/* parse the new text and flush all but any stragglers */
		lo_AppendTextToBlock ( context, state, NULL, text );
		lo_LayoutTextBlock ( context, state, FALSE );
		}
	else
		{
		block = lo_NewTextBlock ( context, state, text, state->preformatted );
		if ( !state->top_state->out_of_memory && ( block != NULL ) )
			{
			block->buffer_read_index = 0;
			lo_LayoutFormattedText ( context, state, block );
			}
		}
}


/*************************************
 * Function: lo_FormatText
 *
 * Description: This function formats text by breaking it into lines
 *	at word boundries.  Word boundries are whitespace, or special
 *	word break tags.
 *
 * Params: Window context and document state., and the text to be formatted.
 *
 * Returns: Nothing
 *************************************/
void
lo_LayoutFormattedText(MWContext *context,
                       lo_DocState *state,
                       LO_TextBlock * block)
{
	char *tptr;
	char *w_start;
	char *w_end;
	char *text_buf;
	char tchar1;
	int32 word_len;
	Bool line_break;
	Bool word_break;
	Bool prev_word_breakable;
	Bool white_space;
	int16 charset;
	Bool multi_byte;
	LO_TextStruct text_data;
	char * text;
	
	/* start at the current text position in this text block */
	text = (char *) &block->text_buffer[ block->buffer_read_index ];
	

#ifdef XP_OS2                  /* performance                            */
   int32 maxw;            /* performance - max char width for font  */
   int32 estwidth;        /* performance - estimated width for line */
   int textsw;            /* performance                            */
   maxw  = 0;
   textsw = 0;            /* performance - need to mark no width taken */
#endif


	/*
	 * Initialize the structures to 0 (mark)
	 */
	memset (&text_data, 0, sizeof (LO_TextStruct));

	/*
	 * Error conditions
	 */
	if ((state == NULL)||(state->cur_ele_type != LO_TEXT)||(text == NULL))
	{
		return;
	}

	charset = block->text_attr->charset;
	if ((INTL_CharSetType(charset) == SINGLEBYTE) ||
		(INTL_CharSetType(charset) & CS_SPACE))
	{
		multi_byte = FALSE;
	}
	else
	{
		multi_byte = TRUE;
	}

	/*
	 * Move through this text fragment, breaking it up into
	 * words, and then grouping the words into lines.
	 */
	tptr = text;
	prev_word_breakable = FALSE;
	while ((*tptr != '\0')&&(state->top_state->out_of_memory == FALSE))
	{
		PA_Block nbsp_block;
		Bool has_nbsp;
#ifdef TEXT_CHUNK_LIMIT
		int32 w_char_cnt;
#endif /* TEXT_CHUNK_LIMIT */
#ifdef XP_WIN16
		int32 ccnt;
#endif /* XP_WIN16 */
		Bool mb_sp;  /* Allow space between multibyte */
		
		/*
		 * white_space is a tag to tell us if the currenct word
		 * contains nothing but whitespace.
		 * word_break tells us if there was whitespace
		 * before this word so we know we can break it.
		 */
		white_space = FALSE;
		word_break = FALSE;
		line_break = FALSE;
		nbsp_block = NULL;
		has_nbsp = FALSE;
		mb_sp = FALSE;

		if (multi_byte == FALSE)
		{
			/* check for textTransform properties and apply them */
			if(state->top_state && state->top_state->style_stack)
			{
				char *property;
				StyleStruct *style_struct = STYLESTACK_GetStyleByIndex(
												state->top_state->style_stack, 
												0);

				if(style_struct)
				{
					property = STYLESTRUCT_GetString(style_struct, 
													 TEXT_TRANSFORM_STYLE);

					if(property)
					{
						lo_transform_text_from_string_method(tptr, property);
					}
				}
			}
		
			/*
			 * Find the start of the word, skipping whitespace.
			 */
			w_start = tptr;
			while ((XP_IS_SPACE(*tptr))&&(*tptr != '\0'))
			{
				tptr++;
			}

			/*
			 * if tptr has been moved at all, that means
			 * there was some whitespace to skip, which means
			 * we are allowed to put a linebreak before this
			 * word if we want to.
			 */
			if (tptr != w_start)
			{
				int32 new_break_holder;
				int32 min_width;
				int32 indent;

				w_start = tptr;
				word_break = TRUE;

				new_break_holder = state->x + state->width;
				min_width = new_break_holder - state->break_holder;
				indent = state->list_stack->old_left_margin -
						state->win_left;
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
   	         * If we are in text that is supposed to be
             * justified, we want each word to be in a separate
             * text element, so if we just found a word break,
             * and there is already a word in the line buffer,
             * flush that word into its own element.
             */
			if ((state->align_stack != NULL)&&
			   (state->align_stack->alignment ==LO_ALIGN_JUSTIFY)&&
			   (word_break != FALSE)&&
			   (state->cur_ele_type == LO_TEXT)&&
			   (state->line_buf_len != 0))
			{
				/* set the current text offset for this new word */
				block->buffer_read_index = tptr - (char *) block->text_buffer;
				
				lo_FlushLineBuffer(context, state);
				if (state->top_state->out_of_memory != FALSE)
				{
					return;
				}
			}

			/*
			 * Find the end of the word.
			 * Terminate the word, saving the char we replaced
			 * with the terminator so it can be restored later.
			 */
#ifdef TEXT_CHUNK_LIMIT
			w_char_cnt = 0;
#endif /* TEXT_CHUNK_LIMIT */
#ifdef XP_WIN16
			ccnt = state->line_buf_len;
			while ((!XP_IS_SPACE(*tptr))&&(*tptr != '\0')&&(ccnt < SIZE_LIMIT))
			{
				if ((unsigned char)*tptr == NON_BREAKING_SPACE)
				{
					has_nbsp = TRUE;
				}
				tptr++;
#ifdef TEXT_CHUNK_LIMIT
				w_char_cnt++;
#endif /* TEXT_CHUNK_LIMIT */
				ccnt++;
			}
			if (ccnt >= SIZE_LIMIT)
			{
				line_break = TRUE;
			}
#else
			while ((!XP_IS_SPACE(*tptr))&&(*tptr != '\0'))
			{
				if ((unsigned char)*tptr == NON_BREAKING_SPACE)
				{
					/* *tptr = ' '; Replace this later */
					has_nbsp = TRUE;
				}
				tptr++;
#ifdef TEXT_CHUNK_LIMIT
				w_char_cnt++;
#endif /* TEXT_CHUNK_LIMIT */
			}
#endif /* XP_WIN16 */
		}
		else
		{
			has_nbsp = TRUE;
			/*
			 * Find the start of the word, skipping whitespace.
			 */
			w_start = tptr;
			while ((XP_IS_SPACE(*tptr))&&(*tptr != '\0'))
			{
				tptr = INTL_NextChar(charset, tptr);
			}
			if (w_start != tptr)
				mb_sp = TRUE;

			/*
			 * if tptr has been moved at all, that means
			 * there was some whitespace to skip, which means
			 * we are allowed to put a linebreak before this
			 * word if we want to.
			 */
			/*
			 * If this char is a two-byte thing, we can break
			 * before it.
			 */
			if ((tptr != w_start)||((unsigned char)*tptr > 127))
			{
				int32 new_break_holder;
				int32 min_width;
				int32 indent;

				/* If it's multibyte character, it always be able to break */
				if (tptr == w_start)
					prev_word_breakable = TRUE;
				w_start = tptr;
				word_break = TRUE;

				new_break_holder = state->x + state->width;
				min_width = new_break_holder - state->break_holder;
				indent = state->list_stack->old_left_margin -
						state->win_left;
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
			else if (prev_word_breakable)
			{
				int32 new_break_holder;
				int32 min_width;
				int32 indent;

				prev_word_breakable = FALSE;
				w_start = tptr;
				word_break = TRUE;

				new_break_holder = state->x + state->width;
				min_width = new_break_holder - state->break_holder;
				indent = state->list_stack->old_left_margin -
						state->win_left;
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
			 * Find the end of the word.
			 * Terminate the word, saving the char we replaced
			 * with the terminator so it can be restored later.
			 */
#ifdef TEXT_CHUNK_LIMIT
			w_char_cnt = 0;
#endif /* TEXT_CHUNK_LIMIT */
#ifdef XP_WIN16
			ccnt = state->line_buf_len;
			while ((  ((unsigned char)*tptr < 128) 
						|| (INTL_KinsokuClass(charset, (unsigned char *)tptr) == PROHIBIT_WORD_BREAK )
				   ) && (!XP_IS_SPACE(*tptr)) 
					 && (*tptr != '\0')
					 && (ccnt < SIZE_LIMIT))
			{
				intn c_len;
				char *tptr2;

				tptr2 = INTL_NextChar(charset, tptr);
				c_len = (intn)(tptr2 - tptr);
				tptr = tptr2;
#ifdef TEXT_CHUNK_LIMIT
				w_char_cnt += c_len;
#endif /* TEXT_CHUNK_LIMIT */
				ccnt += c_len;
			}
			if (ccnt >= SIZE_LIMIT)
			{
				line_break = TRUE;
			}
#else

#if 0
			while ( /* Change the order so we have better performance */
					 (*tptr != '\0')
				     && (!XP_IS_SPACE(*tptr)) 
				     && (	((unsigned char)*tptr < 128)
						    || ((CS_UTF8 == charset) /* hack, since we know only CS_UTF8 have PROHIBIT_WORD_BREAK*/
						        && (INTL_KinsokuClass(charset, (unsigned char *)tptr) == PROHIBIT_WORD_BREAK ))
				        )
					 )
#else
			while (
					 (*tptr != '\0')
				     && (!XP_IS_SPACE(*tptr)) 
				     && (	((unsigned char)*tptr < 128)
						    || ((CS_UTF8 == charset) && (*(unsigned char *)tptr <= 0xE2))					
						    /* In case of CS_UTF8, some code range like CJK character baundary is breakable.
						     * While in range UCS2 < 0x2000 (roman), character baundary is not breakable. 
						     */
				        )
					 )
#endif
			{
				intn c_len;
				char *tptr2;

				tptr2 = INTL_NextChar(charset, tptr);
				c_len = (intn)(tptr2 - tptr);
				tptr = tptr2;
#ifdef TEXT_CHUNK_LIMIT
				w_char_cnt += c_len;
#endif /* TEXT_CHUNK_LIMIT */
			}
#endif /* XP_WIN16 */
		}  /* multi byte */

#ifdef TEXT_CHUNK_LIMIT
		if (w_char_cnt > TEXT_CHUNK_LIMIT)
		{
			tptr = (char *)(tptr - (w_char_cnt - TEXT_CHUNK_LIMIT));
			w_char_cnt = TEXT_CHUNK_LIMIT;
		}

		if ((state->line_buf_len + w_char_cnt) > TEXT_CHUNK_LIMIT)
		{
			lo_FlushLineBuffer(context, state);
			if (state->top_state->out_of_memory != FALSE)
			{
				return;
			}
			
			if (state->cur_ele_type != LO_TEXT)
			{
				lo_FreshText(state);
				state->cur_ele_type = LO_TEXT;
			}
		}
#endif /* TEXT_CHUNK_LIMIT */
		if (multi_byte != FALSE)
		{
			if ((w_start == tptr)&&((unsigned char)*tptr > 127))
			{
				tptr = INTL_NextChar(charset, tptr);
			}
		}
		w_end = tptr;
		tchar1 = *w_end;
		*w_end = '\0';

		/*
		 * If the "word" is just an empty string, this
		 * is just whitespace that we may wish to compress out.
		 */
		if (*w_start == '\0')
		{
			white_space = TRUE;
		}

		/*
		 * compress out whitespace if the last word added was also
		 * whitespace.
		 */
		if ((white_space != FALSE)&&(state->trailing_space != FALSE))
		{
			*w_end = tchar1;
#ifdef LOCAL_DEBUG
XP_TRACE(("Discarding(%s)\n", w_start));
#endif /* LOCAL_DEBUG */
			continue;
		}

		/*
		 * This places the preceeding space in front of
		 * separate words on a line.
		 * Unecessary if last item was trailng space.
		 *
		 * If there was a word break before this word, so we know it
		 * was supposed to be separate, and if we are not at the
		 * beginning of the line, and if the
		 * preceeding word is not already whitespace, then add
		 * a space before this word.
		 */
		if ((word_break != FALSE)&&
			(state->at_begin_line == FALSE)&&
			(state->trailing_space == FALSE))
		{
			/*
			 * Since word_break is true, we know
			 * we skipped some spaces previously
			 * so we know there is space to back up
			 * the word pointer inside the buffer.
			 */
		    if ((multi_byte == FALSE)||mb_sp)
		    {
				w_start--;
				*w_start = ' ';
		    }

			/*
			 * If we are formatting breakable text
			 * set break position to be just before this word.
			 * This is where we will break this line if the
			 * new word makes it too long.
			 */
			if (state->breakable != FALSE)
			{
				state->break_pos = state->line_buf_len;
				state->break_width = state->width;
			}
		}

#ifdef LOCAL_DEBUG
XP_TRACE(("Found Word (%s)\n", w_start));
#endif /* LOCAL_DEBUG */
		/*
		 * If this is an empty string, just throw it out
		 * and move on
		 */
		if (*w_start == '\0')
		{
			*w_end = tchar1;
#ifdef LOCAL_DEBUG
XP_TRACE(("Throwing out empty string!\n"));
#endif /* LOCAL_DEBUG */
			continue;
		}

		/*
		 * Now we catch those nasty non-breaking space special
		 * characters and make them spaces.  Yuck, so that 
		 * relayout in tables will still see the non-breaking
		 * spaces, we need to copy the buffer here.
		 */
		if (has_nbsp != FALSE)
		{
			char *tmp_ptr;
			char *to_ptr;
			char *tmp_buf;

			nbsp_block = PA_ALLOC(XP_STRLEN(w_start) + 1);
			if (nbsp_block == NULL)
			{
				*w_end = tchar1;
				state->top_state->out_of_memory = TRUE;
				break;
			}
			PA_LOCK(tmp_buf, char *, nbsp_block);

			tmp_ptr = w_start;
			to_ptr = tmp_buf;
			while (*tmp_ptr != '\0')
			{
				*to_ptr = *tmp_ptr;
				if (((unsigned char)*to_ptr == NON_BREAKING_SPACE)
					&& (CS_USER_DEFINED_ENCODING != charset))
				{
					*to_ptr = ' ';
				}
				if(multi_byte) {
					int i;
					int len = INTL_CharLen(charset,
                                           (unsigned char *)tmp_ptr);
					to_ptr++;
					tmp_ptr++;
					for (i=1; (i<len) && (*tmp_ptr != '\0'); i++) {
						*to_ptr++  = *tmp_ptr++;
					}
				}
				else {
					to_ptr++;
					tmp_ptr++;
				}
			}
			*w_end = tchar1;
			w_start = tmp_buf;
			w_end = to_ptr;
			*w_end = '\0';
		}

		/*
		 * Make this Front End specific text, and count
		 * the length of the word.
		 */
		/* don't need this any more since we're converting
		 * elsewhere -- erik
		w_start = FE_TranslateISOText(context, charset, w_start);
		 */
		word_len = XP_STRLEN(w_start);

		/*
		 * Append this word to the line buffer.
		 * It may be necessary to expand the line
		 * buffer.
		 */
		if ((state->line_buf_len + word_len + 1) >
			state->line_buf_size)
		{
			state->line_buf = PA_REALLOC( state->line_buf,
				(state->line_buf_size +
				word_len + LINE_BUF_INC));
			if (state->line_buf == NULL)
			{
				*w_end = tchar1;
				if ((has_nbsp != FALSE)&&(nbsp_block != NULL))
				{
					PA_UNLOCK(nbsp_block);
					PA_FREE(nbsp_block);
					nbsp_block = NULL;
				}
				state->top_state->out_of_memory = TRUE;
				break;
			}
			state->line_buf_size += (word_len + LINE_BUF_INC);
		}
		PA_LOCK(text_buf, char *, state->line_buf);
		XP_BCOPY(w_start,
		    (char *)(text_buf + state->line_buf_len),
		    (word_len + 1));
		state->line_buf_len += word_len;
		PA_UNLOCK(state->line_buf);

		/*
		 * Having added a word, we cannot be at the start of a line
		 */
		state->cur_ele_type = LO_TEXT;
		state->at_begin_line = FALSE;

		/*
		 * Most common case is appending to the same line.
		 * assume that is what we are doing here.
		 */
		text_data.text = state->line_buf;
		text_data.text_len = (int16)state->line_buf_len;
		text_data.text_attr = state->cur_text_block->text_attr;
		FE_GetTextInfo(context, &text_data, &(state->text_info));
		state->width =
			lo_correct_text_element_width(&(state->text_info));
		
		/* udpate the block's font info cache */
		block->ascent = state->text_info.ascent;
		block->descent = state->text_info.descent;
		
		/*
		 * Set line_break based on document window width
		 */
#ifdef XP_WIN16
		if (((state->x + state->width) > state->right_margin)||(line_break != FALSE))
#else
		if ((state->x + state->width) > state->right_margin)
#endif /* XP_WIN16 */
		{
			/*  
			 * INTL kinsoku line break, some of characters are not allowed to put 
			 * in the end of line or beginning of line
			 */
			if (multi_byte && (state->break_pos != -1))
			{
				int cur_wordtype, pre_wordtype, pre_break_pos;
				cur_wordtype = INTL_KinsokuClass(charset, (unsigned char *) w_start);

				PA_LOCK(text_buf, char *, state->line_buf);
				pre_break_pos = INTL_PrevCharIdx(charset, 
					(unsigned char *)text_buf, state->break_pos);
				pre_wordtype = INTL_KinsokuClass(charset, 
					(unsigned char *)(text_buf + pre_break_pos));

				if (pre_wordtype == PROHIBIT_END_OF_LINE ||
				    (cur_wordtype == PROHIBIT_BEGIN_OF_LINE && 
					 XP_IS_ALPHA(*(text_buf+pre_break_pos)) == FALSE))
					state->break_pos = pre_break_pos;

				PA_UNLOCK(state->line_buf);
			}
			line_break = TRUE;
		}
		else
		{
			line_break = FALSE;
		}

		/*
		 * We cannot break a line if we have no break positions.
		 * Usually happens with a single line of unbreakable text.
		 */
		if ((line_break != FALSE)&&(state->break_pos == -1))
		{
			/*
			 * It may be possible to break a previous
			 * text element on the same line.
			 */
			if (state->old_break_pos != -1)
			{
				lo_BreakOldElement(context, state);
				line_break = FALSE;
			}
#ifdef XP_WIN16
			else if (ccnt >= SIZE_LIMIT)
			{
				state->break_pos = state->line_buf_len - 1;
			}
			else
			{
				line_break = FALSE;
			}
#else
			else
			{
				line_break = FALSE;
			}
#endif /* XP_WIN16 */
		}

		/*
		 * If we are breaking the line here, flush the
		 * line_buf, and then insert a linebreak.
		 */
		if (line_break != FALSE)
		{
			char *break_ptr;
			char *word_ptr;
			char *new_buf;
			PA_Block new_block;
#ifdef LOCAL_DEBUG
XP_TRACE(("LineBreak, flush text.\n"));
#endif /* LOCAL_DEBUG */

			/*
			 * Find the breaking point, and the pointer
			 * to the remaining word without its leading
			 * space.
			 */
			PA_LOCK(text_buf, char *, state->line_buf);
			break_ptr = (char *)(text_buf + state->break_pos);
/*			word_ptr = (char *)(break_ptr + 1); */
			word_ptr = break_ptr;

		    if ((multi_byte == FALSE)||mb_sp)
		    {
				word_ptr++;
		    }

			/*
			 * Copy the remaining word into its
			 * own buffer.
			 */
			word_len = XP_STRLEN(word_ptr);
			new_block = PA_ALLOC((word_len + 1) *
				sizeof(char));
			if (new_block == NULL)
			{
				PA_UNLOCK(state->line_buf);
				*w_end = tchar1;
				if ((has_nbsp != FALSE)&&(nbsp_block != NULL))
				{
					PA_UNLOCK(nbsp_block);
					PA_FREE(nbsp_block);
					nbsp_block = NULL;
				}
				state->top_state->out_of_memory = TRUE;
				break;
			}
			PA_LOCK(new_buf, char *, new_block);
			XP_BCOPY(word_ptr, new_buf, (word_len + 1));

			*break_ptr = '\0';
			state->line_buf_len = state->line_buf_len -
				word_len;
			if ((multi_byte == FALSE)||(word_ptr != break_ptr))
		    {
				state->line_buf_len--;
		    }
			text_data.text = state->line_buf;
			text_data.text_len = (int16)state->line_buf_len;
			text_data.text_attr = state->cur_text_block->text_attr;
			FE_GetTextInfo(context, &text_data,&(state->text_info));
			PA_UNLOCK(state->line_buf);
			state->width = lo_correct_text_element_width(
				&(state->text_info));
			
			lo_FlushLineBuffer(context, state);
#ifdef EDITOR
			state->edit_current_offset += (word_ptr - text_buf);
#endif
			if (state->top_state->out_of_memory != FALSE)
			{
				PA_UNLOCK(new_block);
				PA_FREE(new_block);
				if ((has_nbsp != FALSE)&&(nbsp_block != NULL))
				{
					PA_UNLOCK(nbsp_block);
					PA_FREE(nbsp_block);
					nbsp_block = NULL;
				}
				return;
			}

			/*
			 * Put on a linefeed element.
			 * This line is finished and will be added
			 * to the line array.
			 */
			lo_SoftLineBreak(context, state, TRUE);

			/*
			 * If there was no remaining word, free up
			 * the unnecessary buffer, and empty out
			 * the line buffer.
			 */
			if (word_len == 0)
			{
				PA_UNLOCK(new_block);
				PA_FREE(new_block);
				state->line_buf_len = 0;
				state->width = 0;
			}
			else
			{
				PA_LOCK(text_buf, char *,state->line_buf);
				XP_BCOPY(new_buf, text_buf, (word_len + 1));
				PA_UNLOCK(state->line_buf);
				PA_UNLOCK(new_block);
				PA_FREE(new_block);
				state->line_buf_len = word_len;
				text_data.text = state->line_buf;
				text_data.text_len = (int16)state->line_buf_len;
				text_data.text_attr =
					state->cur_text_block->text_attr;
				FE_GetTextInfo(context, &text_data,
					&(state->text_info));
				state->width = lo_correct_text_element_width(
					&(state->text_info));

				/*
				 * Having added text, we are no longer at the
				 * start of the line.
				 */
				state->at_begin_line = FALSE;
				state->cur_ele_type = LO_TEXT;
			}


			/*
			 * having just broken the line, we have no break
			 * position.
			 */
			state->break_pos = -1;
			state->break_width = 0;
		}
		else
		{
			/* this word fits, so update the text buffer position */
			block->buffer_read_index = tptr - (char *) block->text_buffer;

			if (white_space != FALSE)
			{
				state->trailing_space = TRUE;
			}
			else
			{
				state->trailing_space = FALSE;
			}
		}

		*w_end = tchar1;
		/*
		 * Free up the extra block used for non-breaking
		 * spaces if we had to allocate one.
		 */
		if ((has_nbsp != FALSE)&&(nbsp_block != NULL))
		{
			PA_UNLOCK(nbsp_block);
			PA_FREE(nbsp_block);
			nbsp_block = NULL;
		}
	}
	/*   
     * if last char is multibyte, break position need to be set to
	 * end of string 
     */
	if (multi_byte != FALSE && *tptr == '\0' && prev_word_breakable != FALSE)
		state->break_pos = state->line_buf_len;
	
	if ( ( state->cur_ele_type != LO_TEXT ) || ( state->line_buf_len == 0 ) )
		{
		state->cur_text_block = NULL;
		}
}


/*************************************
 * Function: lo_FlushLineBuffer
 *
 * Description: Flush out the current line buffer of text
 *	into a new text element, and add that element to
 *	the end of the line list of elements.
 *
 * Params: Window context and document state.
 *
 * Returns: Nothing
 *************************************/
void
lo_FlushLineBuffer(MWContext *context, lo_DocState *state)
{
	LO_TextStruct *text_data;
	int32 baseline_inc;
	LO_TextBlock * block;
	
	baseline_inc = 0;
#ifdef DEBUG
	assert (state);
#endif
		
	block = state->cur_text_block;
	
	/* bail if we have nothing to do with text */
	if ( ( block == NULL ) || ( state->cur_ele_type != LO_TEXT ) )
		{
		return;
		}
	
	/*
	 * If we're currently using the new break table layout, then bail to it
	 */
	if ( lo_UseBreakTable ( block ) )
		{
		lo_FlushText ( context, state );
		return;
		}
	
	/*
	 * Make sure we have some text to flush
	 */
	if ( state->line_buf_len == 0 )
		{
		return;
		}
		
	/* 
	 * LTNOTE: probably should be grabbing state edit_element and offset from
	 * state.
	*/
    text_data = lo_new_text_element(context, state, NULL, 0);

	if (text_data == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}
	state->linefeed_state = 0;

	/*
	 * Some fonts (particulatly italic ones with curly tails
	 * on letters like 'f') have a left bearing that extends
	 * back into the previous character.  Since in this case the
	 * previous character is probably not in the same font, we
	 * move forward to avoid overlap.
	 *
	 * Those same funny fonts can extend past the last character,
	 * and we also have to catch that, and advance the following text
	 * to eliminate cutoff.
	 */
	if (state->text_info.lbearing < 0)
	{
		text_data->x_offset = state->text_info.lbearing * -1;
	}
	text_data->width = state->width;
	
	/* 
	 * record the current doc width and text buffer offset for use
	 * during relayout.
	 */
	text_data->doc_width = state->right_margin - state->x;
	text_data->block_offset = block->buffer_read_index;
	XP_ASSERT(block->buffer_read_index <= 65535);
	
	baseline_inc = lo_compute_text_basline_inc ( state, block, text_data );
	
	lo_AppendToLineList(context, state, (LO_Element *)text_data, baseline_inc);

	if ( block->startTextElement == NULL )
		{
		block->startTextElement = text_data;
		block->endTextElement = text_data;
		}
	else
		{
		block->endTextElement = text_data;
		}
		
	text_data->height = state->text_info.ascent +
		state->text_info.descent;

	/*
	 * If the element we just flushed had a breakable word
	 * position in it, save that position in case we have
	 * to go back and break this element before we finish
	 * the line.
	 */
	if (state->break_pos != -1)
	{
		state->old_break = text_data;
		state->old_break_block = block;
		state->old_break_pos = state->break_pos;
		state->old_break_width = state->break_width;
	}

	state->line_buf_len = 0;
	state->x += state->width;
	state->width = 0;
	state->cur_ele_type = LO_NONE;
}

void
lo_FlushTextBlock ( MWContext *context, lo_DocState *state )
{
	lo_FlushLineBuffer ( context, state );
	
	state->cur_text_block = NULL;
}

void
lo_ChangeBodyTextFGColor(MWContext *context, lo_DocState *state, 
                         LO_Color *color)
{
	lo_FontStack *fptr;
	LO_TextAttr *attr;

	if ((state->top_state->body_attr & BODY_ATTR_TEXT) != 0)
        return;
    
    state->top_state->body_attr |= BODY_ATTR_TEXT;

    state->text_fg = *color;
    fptr = state->font_stack;

    /* 
     * If we're inside a layer, then we want this color change
     * to only affect text in the layer. So, we push a font
     * (a copy of the top of the stack) onto the font stack
     * and change its color. This font will be popped in the
     * closing of the layer.
     */
    if (lo_InsideLayer(state)) {
        LO_TextAttr tmp_attr;
        
        if (fptr)
            lo_CopyTextAttr(fptr->text_attr, &tmp_attr);
        else
            lo_SetDefaultFontAttr(state, &tmp_attr, context);

        tmp_attr.fg.red =   STATE_DEFAULT_FG_RED(state);
		tmp_attr.fg.green = STATE_DEFAULT_FG_GREEN(state);
		tmp_attr.fg.blue =  STATE_DEFAULT_FG_BLUE(state);
        attr = lo_FetchTextAttr(state, &tmp_attr);
        lo_PushFont(state, P_BODY, attr);
    }
    else if (fptr != NULL)
	{
		attr = fptr->text_attr;
		attr->fg.red =   STATE_DEFAULT_FG_RED(state);
		attr->fg.green = STATE_DEFAULT_FG_GREEN(state);
		attr->fg.blue =  STATE_DEFAULT_FG_BLUE(state);
	}
}


/*
 * Something has changed (probably the default FG and BG colors)
 * since the font stack was initialized in this state.
 * We need to reinitialie it to the new default font.
 * WARNING: This function depends on the assumption that no
 *		elements have yet been placed in this state.
 */
void
lo_ResetFontStack(MWContext *context, lo_DocState *state)
{
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
	state->font_stack = lo_DefaultFont(state, context);
}


/*************************************
 * Function: lo_PushFont
 *
 * Description: Push the text attribute information for a new
 *	font onto the font stack.  Also save the type of the
 *	tag that caused the change.
 *
 * Params: Document state, tag type, and the text attribute
 *	structure for the new font.
 *
 * Returns: Nothing
 *************************************/
void
lo_PushFont(lo_DocState *state, intn tag_type, LO_TextAttr *attr)
{
	lo_FontStack *fptr;

	fptr = XP_NEW(lo_FontStack);
	if (fptr == NULL)
	{
		return;
	}
	fptr->tag_type = tag_type;
	fptr->text_attr = attr;
	fptr->next = state->font_stack;
	state->font_stack = fptr;;
}


/*************************************
 * Function: lo_PopFontStack
 *
 * Description: This function pops the next font
 *	off the font stack, and return the text attribute of the
 *	previous font.
 *	The last font on the font stack cannot be popped off.
 *
 * Params: Document state, and the tag type that caused the change.
 *
 * Returns: The LO_TextAttr structure of the font just passed.
 *************************************/
PRIVATE
LO_TextAttr *
lo_PopFontStack(lo_DocState *state, intn tag_type)
{
	LO_TextAttr *attr;
	lo_FontStack *fptr;

	if (state->font_stack->next == NULL)
	{
#ifdef LOCAL_DEBUG
XP_TRACE(("Popped too many fonts!\n"));
#endif /* LOCAL_DEBUG */
		return(NULL);
	}

	fptr = state->font_stack;
	attr = fptr->text_attr;
	if (fptr->tag_type != tag_type)
	{
#ifdef LOCAL_DEBUG
XP_TRACE(("Warning:  Font popped by different TAG than pushed it %d != %d\n", fptr->tag_type, tag_type));
#endif /* LOCAL_DEBUG */
	}
	state->font_stack = fptr->next;
	XP_DELETE(fptr);

	return(attr);
}


LO_TextAttr *
lo_PopFont(lo_DocState *state, intn tag_type)
{
	LO_TextAttr *attr;
	lo_FontStack *fptr;

	/*
	 * This should never happen, but we are patching a
	 * more serious problem that causes us to be called
	 * here after the font stack has been freed.
	 */
	if ((state->font_stack == NULL)||(state->font_stack->next == NULL))
	{
#ifdef LOCAL_DEBUG
XP_TRACE(("Popped too many fonts!\n"));
#endif /* LOCAL_DEBUG */
		return(NULL);
	}

	fptr = state->font_stack;
	attr = NULL;

	if (fptr->tag_type != P_ANCHOR)
	{
		attr = fptr->text_attr;
		if (fptr->tag_type != tag_type)
		{
#ifdef LOCAL_DEBUG
XP_TRACE(("Warning:  Font popped by different TAG than pushed it %d != %d\n", fptr->tag_type, tag_type));
#endif /* LOCAL_DEBUG */
		}
		state->font_stack = fptr->next;
		XP_DELETE(fptr);
	}
	else
	{
		while ((fptr->next != NULL)&&(fptr->next->tag_type == P_ANCHOR))
		{
			fptr = fptr->next;
		}
		if (fptr->next->next != NULL)
		{
			lo_FontStack *f_tmp;

			f_tmp = fptr->next;
			fptr->next = fptr->next->next;
			attr = f_tmp->text_attr;
			XP_DELETE(f_tmp);
		}
	}

	return(attr);
}


void
lo_PopAllAnchors(lo_DocState *state)
{
	lo_FontStack *fptr;

	if (state->font_stack->next == NULL)
	{
#ifdef LOCAL_DEBUG
XP_TRACE(("Popped too many fonts!\n"));
#endif /* LOCAL_DEBUG */
		return;
	}

	/*
	 * Remove all anchors on top of the font stack
	 */
	fptr = state->font_stack;
	while ((fptr->tag_type == P_ANCHOR)&&(fptr->next != NULL))
	{
		lo_FontStack *f_tmp;

		f_tmp = fptr;
		fptr = fptr->next;
		XP_DELETE(f_tmp);
	}
	state->font_stack = fptr;

	/*
	 * Remove all anchors buried in the stack
	 */
	while (fptr->next != NULL)
	{
		/*
		 * Reset spurrious anchor color text entries
		 */
		if ((fptr->text_attr != NULL)&&
			(fptr->text_attr->attrmask & LO_ATTR_ANCHOR))
		{
			LO_TextAttr tmp_attr;

			lo_CopyTextAttr(fptr->text_attr, &tmp_attr);
			tmp_attr.attrmask =
				tmp_attr.attrmask & (~LO_ATTR_ANCHOR);
			tmp_attr.fg.red = STATE_DEFAULT_FG_RED(state);
			tmp_attr.fg.green = STATE_DEFAULT_FG_GREEN(state);
			tmp_attr.fg.blue = STATE_DEFAULT_FG_BLUE(state);
			tmp_attr.bg.red = STATE_DEFAULT_BG_RED(state);
			tmp_attr.bg.green = STATE_DEFAULT_BG_GREEN(state);
			tmp_attr.bg.blue = STATE_DEFAULT_BG_BLUE(state);
			fptr->text_attr = lo_FetchTextAttr(state, &tmp_attr);
		}

		if (fptr->next->tag_type == P_ANCHOR)
		{
			lo_FontStack *f_tmp;

			f_tmp = fptr->next;
			fptr->next = fptr->next->next;
			XP_DELETE(f_tmp);
		}
		else
		{
			fptr = fptr->next;
		}
	}
}

void
lo_FormatBullet(MWContext *context, lo_DocState *state,
				LO_BulletStruct *bullet,
				int32 *line_height,
				int32 *baseline)
{
	LO_TextStruct tmp_text;
	LO_TextInfo text_info;
	LO_TextAttr *tptr;
	PA_Block buff;
	char *str;

#define MIN_BULLET_SIZE 5

	bullet->ele_id = NEXT_ELEMENT;

	/* bullet = (LO_BulletStruct *)lo_NewElement(context, state, LO_BULLET, NULL, 0); */
	if (bullet == NULL)
	{
#ifdef DEBUG
		assert (state->top_state->out_of_memory);
#endif
		return;
	}

	tptr = bullet->text_attr;

	memset (&tmp_text, 0, sizeof (tmp_text));
	buff = PA_ALLOC(1);
	if (buff == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}
	PA_LOCK(str, char *, buff);
	str[0] = ' ';
	PA_UNLOCK(buff);
	tmp_text.text = buff;
	tmp_text.text_len = 1;
	tmp_text.text_attr = tptr;
	FE_GetTextInfo(context, &tmp_text, &text_info);
	PA_FREE(buff);

	/* contain the bullet size so that it doesn't extend off the
	 * left side of the page since we are using a negative offset
	 * to place the bullet
	 *
	 * also subtract one to avoid the header code at the bottom
	 * from triggering and messing up the alignment
	 */
	if(bullet->bullet_size*2 >= state->x-state->win_left)
	  bullet->bullet_size = ((state->x-state->win_left)/2)-1;

    /* enforce a minumum bullet size */
    if(bullet->bullet_size < 1)
	  bullet->bullet_size = MIN_BULLET_SIZE;

	bullet->x = state->x - (2 * bullet->bullet_size);
	if (bullet->x < state->win_left)
	{
		bullet->x = state->win_left;
	}
	bullet->x_offset = 0;
	bullet->y = state->y;
	bullet->y_offset =
		(text_info.ascent + text_info.descent - bullet->bullet_size) / 2;
	bullet->width = bullet->bullet_size;
	bullet->height = bullet->bullet_size;

	*line_height = text_info.ascent + text_info.descent;
	*baseline = text_info.ascent;
}

void
lo_UpdateStateAfterBullet(MWContext * context, lo_DocState *state,
						  LO_BulletStruct *bullet,
						  int32 line_height,
						  int32 baseline)
{
	state->baseline = baseline;
	state->line_height = line_height;

	/*
	 * Clean up state
	 */
/*
 * Supporting old mistakes made in some other browsers.
 * I will put the "correct code" here, but comment it out, since
 * some other browsers allowed headers inside lists, so we should to, sigh.
	state->linefeed_state = 0;
 */
	state->at_begin_line = TRUE;
	state->cur_ele_type = LO_BULLET;
	if (bullet->x == state->win_left)
	{
		state->x += (bullet->x_offset + (2 * bullet->width));
	}

	/*
	 * Make at_begin_line be accurate
	 * so we can detect the header
	 * linefeed state deception later.
	 */
	state->at_begin_line = FALSE;
	
	/*
	 * After much soul-searching (and brow-beating
	 * by Jamie, I've agreed that really whitespace
	 * should be compressed out at the start of a
	 * list item.  They can always add non-breaking
	 * spaces if they want them.
	 * Setting trailing space true means it won't
	 * let the users add whitespace because it
	 * thinks there already is some.
	 */
	state->trailing_space = TRUE;
}

void
lo_PlaceBullet(MWContext *context, lo_DocState *state)
{
	LO_BulletStruct *bullet = NULL;
	int32 line_height, baseline;
	LO_TextAttr tmp_attr;
	LO_TextStruct tmp_text;
	LO_TextInfo text_info;
	LO_TextAttr *tptr;
	PA_Block buff;
	char *str;

	bullet = (LO_BulletStruct *)lo_NewElement(context, state,
                                              LO_BULLET, NULL, 0);
	if (bullet == NULL)
	{
#ifdef DEBUG
		assert (state->top_state->out_of_memory);
#endif
		return;
	}

	bullet->type = LO_BULLET;
	bullet->next = NULL;
	bullet->prev = NULL;

	bullet->FE_Data = NULL;

	bullet->level = state->list_stack->level;

	bullet->bullet_type = state->list_stack->bullet_type;

    /* try and get a bullet type from style sheets */
    if(state && state->top_state && state->top_state->style_stack)
    {
        StyleStruct *style_struct = STYLESTACK_GetStyleByIndex(
                                            state->top_state->style_stack, 0);

        if(style_struct)
        {
            char *list_style_prop = STYLESTRUCT_GetString(
                                            style_struct,
                                            LIST_STYLE_TYPE_STYLE);
			if(list_style_prop)
			{
				bullet->bullet_type = lo_list_bullet_type(list_style_prop,
                                                          P_UNUM_LIST);
				XP_FREE(list_style_prop);
			}
        }
    }

	bullet->ele_attrmask = 0;

	bullet->sel_start = -1;
	bullet->sel_end = -1;

	if(state->font_stack)
    {
        lo_CopyTextAttr(state->font_stack->text_attr, &tmp_attr);
    }
    else
    {
		lo_SetDefaultFontAttr(state, &tmp_attr, context);
	}
	tptr = lo_FetchTextAttr(state, &tmp_attr);

	memset (&tmp_text, 0, sizeof (tmp_text));
	buff = PA_ALLOC(1);
	if (buff == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}
	PA_LOCK(str, char *, buff);
	str[0] = ' ';
	PA_UNLOCK(buff);
	tmp_text.text = buff;
	tmp_text.text_len = 1;
	tmp_text.text_attr = tptr;
	FE_GetTextInfo(context, &tmp_text, &text_info);
	PA_FREE(buff);

	bullet->bullet_size = (text_info.ascent + text_info.descent) / 2;
	bullet->text_attr = tptr;

	lo_FormatBullet(context, state, bullet, &line_height, &baseline);

	lo_AppendToLineList(context, state, (LO_Element *)bullet, 0);

	lo_UpdateStateAfterBullet(context, state, bullet,
							  line_height,
							  baseline);
}


void
lo_FormatBulletStr(MWContext *context, lo_DocState *state,
				   LO_TextStruct *bullet_text,
				   int32 *line_height,
				   int32 *baseline)
{
	LO_TextInfo text_info;

	FE_GetTextInfo(context, bullet_text, &text_info);

	bullet_text->x = state->x - (bullet_text->height / 2) -
		bullet_text->width;
	if (bullet_text->x < state->win_left)
	{
		bullet_text->x = state->win_left;
	}
	bullet_text->x_offset = 0;
	bullet_text->y = state->y;
	bullet_text->y_offset = 0;

	state->baseline = text_info.ascent;
	state->line_height = (intn) bullet_text->height;

	*baseline = text_info.ascent;
	*line_height = bullet_text->height;
}

void
lo_UpdateStateAfterBulletStr(MWContext *context,
							 lo_DocState *state,
							 LO_TextStruct *bullet_text,
							 int32 line_height,
							 int32 baseline)
{
	state->baseline = baseline;
	state->line_height = line_height;

	/*
	 * Clean up state
	 */
/*
 * Supporting old mistakes made in some other browsers.
 * I will put the "correct code" here, but comment it out, since
 * some other browsers allowed headers inside lists, so we should to, sigh.
	state->linefeed_state = 0;
	state->at_begin_line = FALSE;
 */
	state->at_begin_line = TRUE;
	state->cur_ele_type = LO_TEXT;
}

void
lo_PlaceBulletStr(MWContext *context, lo_DocState *state)
{
	intn len;
	char str2[22];
	char *str;
	char *str3;
	PA_Block buff;
	LO_TextStruct *bullet_text = NULL;
	LO_TextInfo text_info;
    int bullet_type;
	int32 line_height, baseline;

	bullet_text = (LO_TextStruct *)lo_NewElement(context, state,
                                                 LO_TEXT, NULL, 0);
	if (bullet_text == NULL)
	{
#ifdef DEBUG
		assert (state->top_state->out_of_memory);
#endif
		return;
	}

    bullet_type = state->list_stack->bullet_type;

    /* try and get a bullet type from style sheets */
    if(state && state->top_state && state->top_state->style_stack)
    {
        StyleStruct *style_struct = STYLESTACK_GetStyleByIndex(
                                            state->top_state->style_stack, 0);

        if(style_struct)
        {
            char *list_style_prop = STYLESTRUCT_GetString(
                                            style_struct,
                                            LIST_STYLE_TYPE_STYLE);
			if(list_style_prop)
			{
				bullet_type = lo_list_bullet_type(list_style_prop, P_NUM_LIST);
				XP_FREE(list_style_prop);
			}
        }
    }


	if( EDT_IS_EDITOR( context )) 
	{
		switch( bullet_type ){
		case BULLET_ALPHA_L:
			str = "A";
			break;
		case BULLET_ALPHA_S:
			str = "a";
			break;
		case BULLET_NUM_S_ROMAN:
			str = "x";
			break;
		case BULLET_NUM_L_ROMAN:
			str = "X";
			break;
		default:
			str = "#";
			break;
		}
		len = XP_STRLEN(str);
		buff = PA_ALLOC(len + 1);
		if (buff != NULL)
		{
			PA_LOCK(str3, char *, buff);
			XP_STRCPY(str3, str);
			PA_UNLOCK(buff);
		}
	}
	else {
		if (bullet_type == BULLET_ALPHA_S)
		{
			buff = lo_ValueToAlpha(state->list_stack->value, FALSE, &len);
		}
		else if (bullet_type == BULLET_ALPHA_L)
		{
			buff = lo_ValueToAlpha(state->list_stack->value, TRUE, &len);
		}
		else if (bullet_type == BULLET_NUM_S_ROMAN)
		{
			buff = lo_ValueToRoman(state->list_stack->value, FALSE, &len);
		}
		else if (bullet_type == BULLET_NUM_L_ROMAN)
		{
			buff = lo_ValueToRoman(state->list_stack->value, TRUE, &len);
		}
		else
		{
			XP_SPRINTF(str2, "%d.", (intn)state->list_stack->value);
			len = XP_STRLEN(str2);
			buff = PA_ALLOC(len + 1);
			if (buff != NULL)
			{
				PA_LOCK(str, char *, buff);
				XP_STRCPY(str, str2);
				PA_UNLOCK(buff);
			}
			else
			{
				state->top_state->out_of_memory = TRUE;
			}
		}
	}

	if (buff == NULL)
	{
		return;
	}

	bullet_text->bullet_type = bullet_type;
	bullet_text->text = buff;
	bullet_text->text_len = len;
	bullet_text->text_attr = state->font_stack->text_attr;
	FE_GetTextInfo(context, bullet_text, &text_info);
	bullet_text->width = lo_correct_text_element_width(&text_info);
	bullet_text->height = text_info.ascent + text_info.descent;

	bullet_text->type = LO_TEXT;
	bullet_text->ele_id = NEXT_ELEMENT;

	lo_FormatBulletStr(context, state, bullet_text, &line_height, &baseline);

	bullet_text->anchor_href = state->current_anchor;

	bullet_text->ele_attrmask = 0;
	if (state->breakable != FALSE)
	{
		bullet_text->ele_attrmask |= LO_ELE_BREAKABLE;
	}

	bullet_text->sel_start = -1;
	bullet_text->sel_end = -1;

	bullet_text->next = NULL;
	bullet_text->prev = NULL;

	bullet_text->FE_Data = NULL;

	lo_AppendToLineList(context, state, (LO_Element *)bullet_text, 0);

	state->baseline = text_info.ascent;
	state->line_height = (intn) bullet_text->height;

	lo_UpdateStateAfterBulletStr(context, state, bullet_text,
                                 line_height, baseline);
}


static LO_Element *
lo_make_quote_text(MWContext *context, lo_DocState *state, int32 margin)
{
	PA_Block buff;
	char *str;
	LO_TextStruct *quote_text;
	LO_TextAttr tmp_attr;
	LO_TextInfo text_info;

	quote_text = (LO_TextStruct *)lo_NewElement(context, state, LO_TEXT,
							NULL, 0);
	if (quote_text == NULL)
	{
#ifdef DEBUG
		assert (state->top_state->out_of_memory);
#endif
		return(NULL);
	}

	buff = PA_ALLOC(2);
	if (buff == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return(NULL);
	}
	PA_LOCK(str, char *, buff);
	str[0] = '>';
	str[1] = '\0';
	PA_UNLOCK(buff);

	quote_text->text = buff;
	quote_text->text_len = 1;
	/*
	 * Fill in default fixed font information.
	 */
	lo_SetDefaultFontAttr(state, &tmp_attr, context);
	tmp_attr.fontmask |= LO_FONT_FIXED;
	quote_text->text_attr = lo_FetchTextAttr(state, &tmp_attr);
	FE_GetTextInfo(context, quote_text, &text_info);
	quote_text->width = lo_correct_text_element_width(&text_info);
	quote_text->height = text_info.ascent + text_info.descent;

	quote_text->type = LO_TEXT;
	quote_text->ele_id = 0;
	quote_text->x = margin;
	if (quote_text->x < state->win_left)
	{
		quote_text->x = state->win_left;
	}
	quote_text->x_offset = 0;
	quote_text->y = state->y;
	quote_text->y_offset = 0;

	quote_text->anchor_href = state->current_anchor;

	quote_text->ele_attrmask = 0;
	if (state->breakable != FALSE)
	{
		quote_text->ele_attrmask |= LO_ELE_BREAKABLE;
	}

	quote_text->bullet_type = BULLET_MQUOTE;

	quote_text->sel_start = -1;
	quote_text->sel_end = -1;

	quote_text->next = NULL;
	quote_text->prev = NULL;

	quote_text->FE_Data = NULL;

	state->baseline = text_info.ascent;

	return((LO_Element *)quote_text);
}


static LO_Element *
lo_make_quote_bullet(MWContext *context, lo_DocState *state, int32 margin)
{
	PA_Block buff;
	char *str;
	LO_BulletStruct *bullet = NULL;
	LO_TextAttr tmp_attr;
	LO_TextInfo text_info;
	LO_TextStruct tmp_text;
	LO_TextAttr *tptr;
	int32 bullet_size;

	bullet = (LO_BulletStruct *)lo_NewElement(context, state,
                                              LO_BULLET, NULL, 0);
	if (bullet == NULL)
	{
#ifdef DEBUG
		assert (state->top_state->out_of_memory);
#endif
		return(NULL);
	}

	lo_SetDefaultFontAttr(state, &tmp_attr, context);
	tmp_attr.fg.red = 0;
	tmp_attr.fg.green = 0;
	tmp_attr.fg.blue = 255;
	tptr = lo_FetchTextAttr(state, &tmp_attr);

	memset (&tmp_text, 0, sizeof (tmp_text));
	buff = PA_ALLOC(1);
	if (buff == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return(NULL);
	}
	PA_LOCK(str, char *, buff);
	str[0] = ' ';
	PA_UNLOCK(buff);
	tmp_text.text = buff;
	tmp_text.text_len = 1;
	tmp_text.text_attr = tptr;
	FE_GetTextInfo(context, &tmp_text, &text_info);
	PA_FREE(buff);

	bullet_size = text_info.ascent + text_info.descent;

	if (bullet_size < 5)
	{
		bullet_size = 5;
	}

	bullet->type = LO_BULLET;
	bullet->ele_id = 0;
	bullet->x = margin;
	if (bullet->x < state->win_left)
	{
		bullet->x = state->win_left;
	}
	bullet->x_offset = 0;
	bullet->y = state->y;
	bullet->y_offset = 0;
	bullet->width = 5;
	bullet->height = bullet_size;
	bullet->next = NULL;
	bullet->prev = NULL;

	bullet->FE_Data = NULL;

	bullet->level = state->list_stack->level;
	bullet->bullet_type = BULLET_MQUOTE;
	bullet->text_attr = tptr;

	bullet->ele_attrmask = 0;

	bullet->sel_start = -1;
	bullet->sel_end = -1;

	state->baseline = text_info.ascent;

	return((LO_Element *)bullet);
}


static void
lo_insert_quote_characters(MWContext *context, lo_DocState *state)
{
	LO_Element *eptr;
	LO_Element *elist;
	lo_ListStack *lptr;

	elist = NULL;
	lptr = state->list_stack;
	while (lptr != NULL)
	{
		eptr = NULL;
		if (lptr->quote_type == QUOTE_JWZ)
		{
			eptr = lo_make_quote_text(context, state,
					lptr->mquote_x);
		}
		else if (lptr->quote_type == QUOTE_CITE)
		{
			eptr = lo_make_quote_bullet(context, state,
					lptr->mquote_x);
		}
		if (eptr != NULL)
		{
			eptr->lo_any.next = elist;
			elist = eptr;
		}
		lptr = lptr->next;
	}

	eptr = elist;
	while (eptr != NULL)
	{
		LO_Element *tmp_ele;

		tmp_ele = eptr;
		eptr = eptr->lo_any.next;
		tmp_ele->lo_any.next = NULL;

		tmp_ele->lo_any.ele_id = NEXT_ELEMENT;
		lo_AppendToLineList(context, state, tmp_ele, 0);
		state->line_height = (intn)tmp_ele->lo_any.height;
		state->at_begin_line = TRUE;
		state->cur_ele_type = LO_TEXT;
	}
}


void
lo_PlaceQuoteMarker(MWContext *context, lo_DocState *state, lo_ListStack *lptr)
{
	LO_Element *eptr;

	if (lptr != NULL)
	{
		eptr = NULL;
		if (lptr->quote_type == QUOTE_JWZ)
		{
			eptr = lo_make_quote_text(context, state,
					lptr->mquote_x);
		}
		else if (lptr->quote_type == QUOTE_CITE)
		{
			eptr = lo_make_quote_bullet(context, state,
					lptr->mquote_x);
		}
		if (eptr != NULL)
		{
			eptr->lo_any.ele_id = NEXT_ELEMENT;
			lo_AppendToLineList(context, state, eptr, 0);
			state->line_height = (intn)eptr->lo_any.height;
			state->at_begin_line = TRUE;
			if (lptr->quote_type == QUOTE_JWZ)
			{
				state->cur_ele_type = LO_TEXT;
			}
			else if (lptr->quote_type == QUOTE_CITE)
			{
				state->cur_ele_type = LO_BULLET;
			}
		}
	}
}


void lo_UpdateStateAfterLineBreak( MWContext *context,
                                   lo_DocState *state,
                                   Bool updateFE )
{
	int32 line_width;

	/*
	 * if this linefeed has a zero height, make it the height
	 * of the current font.
	 */
	if (state->line_height == 0)
	{
		state->line_height = state->text_info.ascent +
			state->text_info.descent;
		if ((state->line_height <= 0)&&(state->font_stack != NULL)&&
			(state->font_stack->text_attr != NULL))
		{
			lo_fillin_text_info(context, state);
			state->line_height = state->text_info.ascent +
				state->text_info.descent;
		}
		/*
		 * This should never happen, but we have it
		 * covered just in case it does :-)
		 */
		if (state->line_height <= 0)
		{
			state->line_height = state->default_line_height;
		}
	}

	if (state->end_last_line != NULL)
	{
		line_width = state->end_last_line->lo_any.x + state->win_right;
	}
	else
	{
		line_width = state->x + state->win_right;
	}

	if (line_width > state->max_width)
	{
		state->max_width = line_width;
	}

	/* if LineHeightStack exists use it to offset the new Y value */
    if(state->cur_ele_type != LO_SUBDOC && state->line_height_stack)
	{
		state->y += state->line_height_stack->height;
	}
	else
	{
		state->y = state->y + state->line_height;
	}

	state->x = state->left_margin;
	state->width = 0;
	state->at_begin_line = TRUE;
	state->trailing_space = FALSE;
	state->line_height = 0;
	state->break_holder = state->x;

	state->linefeed_state++;
	if (state->linefeed_state > 2)
	{
		state->linefeed_state = 2;
	}

	/*
	 * Reset the left and right margins
	 */
	lo_FindLineMargins(context, state, updateFE);
	state->x = state->left_margin;

}

void lo_UpdateFEProgressBar( MWContext *context, lo_DocState *state )
{
	if (state->is_a_subdoc == SUBDOC_NOT)
	{
		int32 percent;

		if (state->top_state->total_bytes < 1)
		{
			percent = -1;
		}
		else
		{
			percent = (100 * state->top_state->layout_bytes) /
				state->top_state->total_bytes;
			if (percent > 100)
			{
				percent = 100;
			}
		}
		if ((percent == 100)||(percent < 0)||
			(percent > (state->top_state->layout_percent + 1)))
		{
			if(!state->top_state->is_binary)
				FE_SetProgressBarPercent(context, percent);
			state->top_state->layout_percent = (intn)percent;
		}
	}
}

void lo_UpdateFEDocSize( MWContext *context, lo_DocState *state )
{	
	/*
	 * Tell the front end how big the document is right now.
	 * Only do this for the top level document.
	 */
	if ((state->is_a_subdoc == SUBDOC_NOT)
        &&(state->display_blocked == FALSE)
#ifdef EDITOR
		&&(!state->edit_relayout_display_blocked)
#endif
        )
	{

        /* 
         * Don't resize the layer if we're laying out a block. This
         * will be done when the line is added to the block.
         */
        if (!lo_InsideLayer(state))
		{
            LO_SetDocumentDimensions(context, state->max_width, state->y);
		}
	}
}


void lo_FillInLineFeed( MWContext *context,
                        lo_DocState *state,
                        int32 break_type,
                        uint32 clear_type,
                        LO_LinefeedStruct *linefeed )
{
	linefeed->type = LO_LINEFEED;
	linefeed->ele_id = NEXT_ELEMENT;
	linefeed->x = state->x;
	linefeed->x_offset = 0;
	linefeed->y = state->y;
	linefeed->y_offset = 0;
    /* 
     * If we're laying out a block, we want the contents of the block
     * to determine the size of the block. The right margin is nothing
     * more than a hint for where to wrap the contents. We don't want 
     * the linefeed to extend out to the right margin, because it 
     * unnecessarily extends the block contents.
     */
	if (state->layer_nest_level > 0) {
        linefeed->width = 0;
    }
    else
	linefeed->width = state->right_margin - state->x;
	if (linefeed->width < 0)
	{
		linefeed->width = 0;
	}
	linefeed->height = state->line_height;
	/*
	 * if this linefeed has a zero height, make it the height
	 * of the current font.
	 */
	if (linefeed->height == 0)
	{
		linefeed->height = state->text_info.ascent +
			state->text_info.descent;
		if ((linefeed->height <= 0)&&(state->font_stack != NULL)&&
			(state->font_stack->text_attr != NULL))
		{
			lo_fillin_text_info(context, state);
			linefeed->height = state->text_info.ascent +
				state->text_info.descent;
		}
		/*
		 * This should never happen, but we have it
		 * covered just in case it does :-)
		 */
		if (linefeed->height <= 0)
		{
			linefeed->height = state->default_line_height;
		}
	}
	linefeed->line_height = linefeed->height;

	linefeed->FE_Data = NULL;
	linefeed->anchor_href = state->current_anchor;

	if (state->font_stack == NULL)
	{
		LO_TextAttr tmp_attr;
		LO_TextAttr *tptr;

		/*
		 * Fill in default font information.
		 */
		lo_SetDefaultFontAttr(state, &tmp_attr, context);
		tptr = lo_FetchTextAttr(state, &tmp_attr);
		linefeed->text_attr = tptr;
	}
	else
	{
		linefeed->text_attr = state->font_stack->text_attr;
	}

	linefeed->baseline = state->baseline;

	linefeed->ele_attrmask = 0;

	linefeed->sel_start = -1;
	linefeed->sel_end = -1;

	linefeed->next = NULL;
	linefeed->prev = NULL;
	linefeed->break_type = (uint8) break_type;
	linefeed->clear_type = (uint8) clear_type;
}

Bool lo_CanUseBreakTable ( lo_DocState * state )
{
	Bool	multiByte;
	Bool	useBreakTable;
	
	useBreakTable = TRUE;
	
	lo_GetTextParseAtributes ( state, &multiByte );

#ifndef FAST_MULTI	
	/* 
     * We also need some sort of check for the script - for example
	 * Arabic should go through the old algorithm for now.  
     */
	if ( multiByte )
		{
		useBreakTable = FALSE;
		}
#endif
	
	/*
	 * Justified text is currently broken, route it through old layout for now
	 */
	if ( ( state->align_stack != NULL ) && 
         ( state->align_stack->alignment == LO_ALIGN_JUSTIFY ) )
		{
		useBreakTable = FALSE;
		}
		
#ifndef FAST_EDITOR
	if ( EDT_IS_EDITOR( context ) )
		{
		useBreakTable = FALSE;
		}
#endif
		
#ifdef XP_MAC
	if ( !gCallNewText )
		{
		useBreakTable = FALSE;
		}
#endif

	return useBreakTable;
}

/*
 * Is the current text that's being layed out using the break table
 * layout algorithm?  
 */
Bool lo_UseBreakTable ( LO_TextBlock * block )
{
	Bool			useBreakTable;
	
	useBreakTable = FALSE;
	
	if ( block != NULL )
		{
		if ( block->break_table != NULL )
			{
			useBreakTable = TRUE;
			}
		}
	
	return useBreakTable;
}

int32 lo_compute_text_basline_inc ( lo_DocState * state,
                                    LO_TextBlock * block,
                                    LO_TextStruct * text_data )
{
	int32	line_inc;
	int32	baseline_inc;
	
	/*
	 * The baseline of the text element just added to the line may be
	 * less than or greater than the baseline of the rest of the line
	 * due to font changes.  If the baseline is less, this is easy,
	 * we just increase y_offest to move the text down so the baselines
	 * line up.  For greater baselines, we can't move the text up to
	 * line up the baselines because we will overlay the previous line,
	 * so we have to move all the previous elements in this line down.
	 *
	 * If the baseline is zero, we are the first element on the line,
	 * and we get to set the baseline.
	 */
	
	line_inc = 0;
	baseline_inc = 0;
	
	if (state->baseline == 0)
	{
		state->baseline = block->ascent;
		if (state->line_height < (state->baseline + block->descent))
		{
			state->line_height = state->baseline + block->descent;
		}
	}
	else if (block->ascent < state->baseline)
	{
		text_data->y_offset = state->baseline - block->ascent;
		if ((text_data->y_offset + block->ascent + block->descent) > state->line_height)
		{
			line_inc = text_data->y_offset +
				block->ascent +
				block->descent -
				state->line_height;
		}
	}
	else
	{
		baseline_inc = block->ascent - state->baseline;
		if ((text_data->y_offset + block->ascent + block->descent - baseline_inc) > state->line_height)
		{
			line_inc = text_data->y_offset +
				block->ascent +
				block->descent -
				state->line_height - baseline_inc;
		}
	}

	state->baseline += (intn) baseline_inc;
	state->line_height += (intn) (baseline_inc + line_inc);
	
	return baseline_inc;
}


void lo_FlushTextElement ( MWContext * context,
                           lo_DocState * state,
                           LO_TextBlock * block,
                           LO_TextStruct * element )
{
	int32	baseline_inc;
	
	/* update the text layout state as if we had just layed this element out */
	block->buffer_read_index = element->block_offset;

	state->width = element->width;

	element->ele_id = NEXT_ELEMENT;
	element->x = state->x;
	element->y = state->y;
	element->y_offset = 0;

	element->sel_start = -1;
	element->sel_end = -1;

	baseline_inc = lo_compute_text_basline_inc ( state, block, element );
		
	element->prev = NULL;
	element->next = NULL;
	lo_AppendToLineList ( context, state, (LO_Element *) element,
                          baseline_inc );

	state->line_buf_len = 0;
	state->x += state->width;
	state->width = 0;
	state->cur_ele_type = LO_NONE;

	/* update the element list for this block */
	if ( block->startTextElement == NULL )
		{
		block->startTextElement = element;
		}

	block->endTextElement = element;
}

uint32 lo_FindBlockOffset ( LO_TextBlock * block, LO_TextStruct * fromElement )
{
	uint32			blockOffset;
	LO_Element *	endElement;
	LO_Element *	element;
	
	blockOffset = 0;
		
	if ( fromElement != NULL )
		{
		/* run through all elements in this text block. the correct
           block offset is belongs to */
		/* the previous element in this list */
		element = (LO_Element *) block->startTextElement;
		endElement = (LO_Element *) block->endTextElement;
		
		while ( element != NULL )
			{
			if ( element == endElement )
				{
				break;
				}
			
			/* is it the one we're looking for? */
			if ( element == (LO_Element *) fromElement )
				{
				break;
				}
				
			if ( element->type == LO_TEXT )
				{
				blockOffset = element->lo_text.block_offset;
				}
			
			element = element->lo_any.next;
			}
		}
		
	return blockOffset;
}

void lo_RelayoutTextElements ( MWContext * context,
                               lo_DocState * state,
                               LO_TextBlock * block,
                               LO_TextStruct * fromElement )
{
	LO_Element *	next;
	LO_Element *	element;
	LO_Element *	startElement;
	LO_Element *	endElement;
	uint32			lineWidth;
	Bool			done;
	Bool			fastPreformat;
	
	/* start at the beginning of this text block */
	block->buffer_read_index = 0;
	block->break_read_index = 0;
	
	/* we will rebuild the element list for this block as we go */
	startElement = (LO_Element *) block->startTextElement;
	endElement = (LO_Element *) block->endTextElement;

	block->startTextElement = NULL;
	block->endTextElement = NULL;
	
	if ( fromElement == NULL )
		{
		fromElement = (LO_TextStruct *) startElement;
		}
	
	/* sanity check */
	if ( fromElement == NULL )
		{
		return;
		}
	
	/*
	 * We need to run through all elements up to start element and place them
	 * back in the line list. We also need to recycle anything that's not text
	 * (linefeeds and bullets)
	 */
	element = startElement;
	
	while ( element != (LO_Element *) fromElement )
		{
		next = lo_tv_GetNextLayoutElement ( state, element, FALSE );
		
		/* if this element is text, put it on the line list */
		switch ( element->lo_any.type )
			{
			case LO_TEXT:
				lo_PrepareElementForReuse ( context, state, element,
                                            element->lo_any.edit_element,
                                            element->lo_any.edit_offset );
				lo_FlushTextElement ( context, state, block,
                                      (LO_TextStruct *) element );
				break;
			
			case LO_LINEFEED:
				/* recycle this element */
				element->lo_any.prev = NULL;
				element->lo_any.next = NULL;
				lo_RecycleElements( context, state, element );
				
				break;
			
			default:
				element->lo_any.prev = NULL;
				element->lo_any.next = NULL;
				lo_RecycleElements( context, state, element );
				break;
			}
		
		element = next;
		}
	
	/*
	 * Now, we may not need to lay any of the following elements out as out
	 * layout environment may not have changed. So, run through the remaining
	 * elements until we find the first one that's changed.
	 *
	 * We have to layout the last element using the proper code path
	 * so that we can correctly update the state record with the last
	 * break position and other flags.
	 *
	 * Column and line wrapped preformatted text can always reuse the
	 * elements as it's wrapping will never change. Word wrapped
	 * preformatted text may change if the document width changes.  
     */
	
	element = (LO_Element *) fromElement;
	done = element == endElement;
	fastPreformat = ( block->format_mode == PRE_TEXT_YES ) || ( block->format_mode == PRE_TEXT_COLS );
	
	while ( ( !done ) && ( element != NULL ) )
		{
		next = lo_tv_GetNextLayoutElement ( state, element, FALSE );
		
		/* if this element is text, see if it will fit. otherwise recycle it */
		switch ( element->lo_any.type )
			{
			case LO_TEXT:
				/* 
                 * We only assume this element can be reused if the
				 * line width is exactly the same as last time. If the
				 * line is longer, we could potentially reuse this
				 * element (the next one may appear on this line as
				 * well) but we won't be able to set the state's
				 * old_break_position, which may be needed!
				 *
				 * We can also always flush column or line wrapped
				 * preformatted text (word wrapped preformatted text
				 * may need to be layed out again as it's wrapping may
				 * change).  
                 */
				lineWidth = state->right_margin - state->x;
				
				if ( fastPreformat || ( element->lo_text.doc_width == lineWidth ) )
					{
					lo_PrepareElementForReuse ( context, state, element, element->lo_any.edit_element,
							element->lo_any.edit_offset );
					lo_FlushTextElement ( context, state, block, (LO_TextStruct *) element );
					}
				else
					{
					/* the size has changed, we must relayout this element */
					done = TRUE;
					}
				
				break;
			
			case LO_LINEFEED:
				/* recycle this element */
				element->lo_any.prev = NULL;
				element->lo_any.next = NULL;
				lo_RecycleElements( context, state, element );

				break;
			
			default:
				element->lo_any.prev = NULL;
				element->lo_any.next = NULL;
				lo_RecycleElements( context, state, element );
				break;
			}
			
		element = next;
		
		/*
		 * if we're at the last element, bail as we always need to layout this
		 * one so that the state record is updated properly
		 */
		if ( element == endElement )
			{
			break;
			}
		}
	
	/* 
     * now run through and delete all the remaining elements in this
	 * text block.  
     */
	while ( element != NULL )
		{
		next = lo_tv_GetNextLayoutElement ( state, element, FALSE );

		element->lo_any.prev = NULL;
		element->lo_any.next = NULL;
		lo_RecycleElements( context, state, element );
		
		if ( element == endElement )
			{
			break;
			}
			
		element = next;
		}
}

LO_Element * lo_RelayoutTextBlock ( MWContext * context, lo_DocState * state, LO_TextBlock * block, LO_TextStruct * fromElement )
{
	LO_Element *	next;
	LO_Element *	endElement;
	LO_Element *	lo_ele;

	state->cur_text_block = block;
	
	/*
	 * Update some of the global state information
	 */
	state->breakable = block->ele_attrmask & LO_ELE_BREAKABLE;
	
	/* get the next element for the overall relayout process */
	if ( block->endTextElement != NULL )
		{
		next = lo_tv_GetNextLayoutElement ( state, (LO_Element *) block->endTextElement, FALSE );
		}
	else
		{
		next = lo_tv_GetNextLayoutElement ( state, (LO_Element *) block, FALSE );
		}

	/* 
     * In the relayout case we might be able to skip layout for some
	 * elements that have not changed.  This happens frequenty for
	 * typing in the editor and occasionaly in table layout (very
	 * occasionally on resizes).
	 *
	 * To do this, we run through the text elements until we come
	 * across one whose width does not match the layout width or whose
	 * text has changed. That element and all others are then
	 * recycled.  
     */
	
	if ( EDT_IS_EDITOR( context ) )
		{
		/* 
         * for the editor we don't want to relayout the text elements
		 * that precede the current one. we just want to start laying
		 * out afresh from this specified element to the end of the
		 * text block. the editor will take care of merging the
		 * elements back in 
         */
		block->buffer_read_index = lo_FindBlockOffset ( block, fromElement );

		state->edit_current_element = block->edit_element;
		state->edit_current_offset = 0;
		
		/* if we're laying this element out, then we need to reinsert
           it on the line list */
		if ( ( block->startTextElement == fromElement ) || ( fromElement == NULL ) )
			{
			/* record the current edit element for later use */
			lo_PrepareElementForReuse ( context, state, (LO_Element *) block, block->edit_element,
					block->edit_offset );
			
			block->ele_id = NEXT_ELEMENT;
			block->x = state->x;
			block->y = state->y;
			block->x_offset = 0;
			block->y_offset = 0;
			
			/* free all the text elements that we're going to reflow */
			endElement = (LO_Element *) block->endTextElement;
			lo_ele = (LO_Element *) block->startTextElement;
			while ( lo_ele != NULL )
				{
				LO_Element * next_ele;
				
				next_ele = lo_ele->lo_any.next;
				
				lo_ele->lo_any.next = NULL;
				lo_ele->lo_any.prev = NULL;
				lo_RecycleElements( context, state, lo_ele );
				
				if ( lo_ele == endElement )
					{
					break;
					}
				
				lo_ele = next_ele;
				}

			block->startTextElement = NULL;
			block->endTextElement = NULL;
			
			block->prev = NULL;
			block->next = NULL;
			lo_AppendToLineList ( context, state, (LO_Element *) block, 0 );
			}
		else
			{
			LO_TextStruct * lastText;
			Bool			hitFromElement;
			
			/* We're reflowing from somewhere within the text block
			 * (past the first element). We need to reset the
			 * endTextElement as well as recycle from the fromElement
			 * to the end of the text block */
			
			hitFromElement = FALSE;
			endElement = (LO_Element *) block->endTextElement;
			lo_ele = (LO_Element *) block->startTextElement;
			lastText = block->startTextElement;

			while ( lo_ele != NULL )
				{
				LO_Element * next_ele;
				
				next_ele = lo_ele->lo_any.next;
				
				if ( lo_ele == (LO_Element *) fromElement )
					{
					hitFromElement = TRUE;
					}
					
				/* if we've found the fromElement, we need to start
                   recycling */
				if ( hitFromElement )
					{
					lo_ele->lo_any.next = NULL;
					lo_ele->lo_any.prev = NULL;
					lo_RecycleElements( context, state, lo_ele );
					}
				
				if ( lo_ele == endElement )
					{
					break;
					}
				
				/* if we haven't hit the from element and this is a
                   text element, it may be the new end */
				/* element for the block */
				if ( !hitFromElement && ( lo_ele->type == LO_TEXT ) )
					{
					lastText = &lo_ele->lo_text;
					}
				
				lo_ele = next_ele;
				}
			
			/* reset the endElement for the block */
			block->endTextElement = lastText;
			}
		}
	else
		{
		/* put the text block back in the line list and then add any
           existing elements that we can */
		block->prev = NULL;
		block->next = NULL;
		block->ele_id = NEXT_ELEMENT;
		block->x = state->x;
		block->y = state->y;
		lo_AppendToLineList ( context, state, (LO_Element *) block, 0 );

		lo_RelayoutTextElements ( context, state, block, fromElement );
		}
		
	/* Because we're lame for now we just delete all the old linefeeds
	 * and lay the text out afresh */
	
	/* Tell everybody we're laying out text */
	if (state->cur_ele_type != LO_TEXT)
		{
		lo_FreshText(state);
		state->cur_ele_type = LO_TEXT;
		}

	/* actually layout the text */
	state->preformatted = block->format_mode;
	
	if ( lo_UseBreakTable ( block ) )
		{
		lo_SetupBreakState ( block );
		
		/* be sure to set up the editor offset */
		if ( EDT_IS_EDITOR( context ) )
			{
			state->edit_force_offset = TRUE;
			state->edit_current_offset = block->buffer_read_index;
			}
			
		lo_LayoutTextBlock ( context, state, TRUE );
		}
	else
	if ( block->format_mode == PRE_TEXT_NO )
		{
		lo_LayoutFormattedText ( context, state, block );
		}
	else
		{
		lo_LayoutPreformattedText ( context, state, block );
		}
	
	/*
	 * If there's text left in the line buffer, then flush it.
	 *
	 * BRAIN DAMAGE: We don't want to do that here - there may be a
	 * following text block that continues this same text buffer.  
     */
	lo_FlushLineBuffer(context, state);
		
	return next;
}

Bool lo_ChangeText ( LO_TextBlock * block, char * text )
{
	uint32	length;
	/*
	 * Reset the text contents for this text block. If we have a break table,
	 * then we need to rebuild it.
	 */
	
#ifdef LOCAL_DEBUG
	XP_TRACE( ("Setting text for text block %lx to %s", block, text) );
#endif
	
	length = XP_STRLEN ( text ) + 1;
	if ( length > block->buffer_write_index )
		{
		if ( !lo_GrowTextBlock ( block, length - block->buffer_write_index ) )
			{
			return FALSE;
			}
		}
		
	if ( lo_UseBreakTable ( block ) )
		{
		block->buffer_write_index = 0;
		block->last_buffer_write_index = 0;
		block->break_write_index = 0;
		block->last_break_offset = 0;
		
		lo_AppendTextToBlock ( NULL, NULL, block, text );
		}
	else
		{
		/* for old style text we just want to replace the buffer */
		XP_BCOPY ( text, (char *) block->text_buffer, length );
		block->buffer_write_index = length;
		}
	
	return TRUE;
}

/*
 *
 * ===========================================================================
 * 
 *											New text layout
 *
 *  ==========================================================================
 */

/*
 * Break Table constants
 */

#define	MAX_NATURAL_LENGTH		0xAL
#define	LINE_FEED				0xBL
#define	BYTE_LENGTH				0xCL
#define	WORD_LENGTH				0xDL
#define	LONG_LENGTH				0xEL
#define	MULTI_BYTE				0xFL

#define	MULTI_BYTE_DATA_SIZE	16

/*
 * Break Position Magic Constants
 */
#define	OVERRAN_BREAK_TABLE	-1
#define	BREAK_LINEFEED		-2

#define	TEXT_BUFFER_INC		256
#define	BREAK_TABLE_INC		64

typedef struct BreakState
{
	uint32		buffer_read_index;
	uint32		break_read_index;
	uint32		multibyte_index;
	uint32		multibyte_length;
	uint32		last_line_break;
	uint32		lineLength;
} BreakState;

static LO_TextBlock * lo_CurrentTextBlock ( MWContext * context, lo_DocState * state );

/* routines to walk through our break table */
static uint8 * lo_GetNextTextPosition ( LO_TextBlock * block, uint32 * outWordLength, uint32 * outLineLength, Bool * canBreak );
static uint8 * lo_GetPrevTextPosition ( LO_TextBlock * block, uint32 * outWordLength, uint32 * outLineLength, Bool * canBreak );
static uint8 * lo_RestoreBreakState ( LO_TextBlock * block, BreakState * state, uint32 * lineLength );
static void lo_SetLineBreak ( LO_TextBlock * block, Bool skipSpace );
static uint8 * lo_GetLineStart ( LO_TextBlock * block );
static void lo_SkipCharacter ( LO_TextBlock * block );
static Bool lo_SkipInitialSpace ( LO_TextBlock * block );

static Bool lo_SetBreakPosition ( LO_TextBlock * block );
static Bool lo_SetMultiByteRun ( LO_TextBlock * block, int32 charSize, Bool breakable, Bool eachCharBreakable );
static Bool lo_SetBreakCommand ( LO_TextBlock * block, uint32 command, uint32 commandLength );
static void lo_CopyText ( uint8 * src, uint8 * dst, uint32 length );
static void lo_CopyTextToLineBuffer ( lo_DocState * state, uint8 * src, uint32 length );

static uint32  lo_FindLineBreak ( MWContext * context, lo_DocState * state, LO_TextBlock * block, uint8 * text,
	uint16 * widthTable, uint32 * width, int32 * minWidth, Bool * allTextFits );

/* the parsers */
static void lo_ParseSingleText ( lo_DocState * state, LO_TextBlock * block, Bool parseAllText, char * text );
static void lo_ParseSinglePreformattedText ( lo_DocState * state, LO_TextBlock * block, Bool parseAllText, char * text );
static void lo_ParseDoubleText ( lo_DocState * state, LO_TextBlock * block, Bool parseAllText, char * text );
static void lo_ParseDoublePreformattedText ( lo_DocState * state, LO_TextBlock * block, Bool parseAllText, char * text );

extern int32 lo_correct_text_element_width(LO_TextInfo *text_info);

#ifdef LOG
static Bool gHaveLog = FALSE;
#endif


/*
 * Some helpful macros for code inlining
 */

#define	SAVE_BREAK_STATE(block,state,line_length)				\
	(state)->buffer_read_index = (block)->buffer_read_index;	\
	(state)->break_read_index = (block)->break_read_index;		\
	(state)->multibyte_index = (block)->multibyte_index;		\
	(state)->multibyte_length = (block)->multibyte_length;		\
	(state)->last_line_break = (block)->last_line_break;		\
	(state)->lineLength = line_length;
	

static LO_TextBlock *
lo_CurrentTextBlock ( MWContext * context, lo_DocState * state )
{
	LO_TextBlock * textBlock;
	
	textBlock = state->cur_text_block;
	if ( textBlock == NULL )
		{
		textBlock = (LO_TextBlock *)lo_NewElement ( context, state, LO_TEXTBLOCK, NULL, 0 );

		textBlock->type = LO_TEXTBLOCK;
		textBlock->x_offset = 0;
		textBlock->ele_id = NEXT_ELEMENT;
		textBlock->x = state->x;
		textBlock->y = state->y;
		textBlock->y_offset = 0;
		textBlock->width = 0;
		textBlock->height = 0;
		textBlock->line_height = 0;
		textBlock->next = NULL;
		textBlock->prev = NULL;
		textBlock->text_attr = NULL;
		textBlock->anchor_href = NULL;
		textBlock->ele_attrmask = 0;
		textBlock->format_mode = 0;
		
		textBlock->startTextElement = NULL;
		textBlock->endTextElement = NULL;
		
		textBlock->text_buffer = NULL;
		textBlock->buffer_length = 0;
		textBlock->buffer_write_index = 0;
		textBlock->last_buffer_write_index = 0;
		textBlock->buffer_read_index = 0;
		textBlock->last_line_break = 0;
		
		textBlock->break_table = NULL;
		textBlock->break_length = 0;
		textBlock->break_write_index = 0;
		textBlock->break_read_index = 0;
		textBlock->last_break_offset = 0;
		textBlock->multibyte_index = 0;
		textBlock->multibyte_length = 0;
		
		textBlock->old_break = NULL;
		textBlock->old_break_pos = 0;
		textBlock->old_break_width = 0;
		
		textBlock->totalWidth = 0;
		textBlock->totalChars = 0;
		textBlock->break_pending = 0;
		textBlock->last_char_is_whitespace = 0;
		
		textBlock->ascent = 0;
		textBlock->descent = 0;
		
		/* BRAIN DAMAGE: Add this enum to lo_ele.h! */
		textBlock->text_buffer = XP_ALLOC ( TEXT_BUFFER_INC );
		textBlock->buffer_length = TEXT_BUFFER_INC;
		
		textBlock->break_table = XP_ALLOC ( BREAK_TABLE_INC );
		textBlock->break_length = BREAK_TABLE_INC * 2;
		
		/* Since we're creating a new text block, grab some of the
		 * text state out of the layout state.  */
		textBlock->anchor_href = state->current_anchor;
		
		if ( state->font_stack != NULL )
			{
			textBlock->text_attr = state->font_stack->text_attr;
			}
			
		if (state->breakable != FALSE)
			{
			textBlock->ele_attrmask |= LO_ELE_BREAKABLE;
			}
		
		state->cur_text_block = textBlock;		
		
		lo_AppendToLineList ( context, state, (LO_Element *) textBlock, 0 );
		}
	
	return textBlock;
}

void lo_AppendTextToBlock ( MWContext *context, lo_DocState *state, LO_TextBlock * block, char *text )
{
	Bool	parseAllText;
	Bool	multiByte;
	
	/*
	 * We have several cases in which we can just bail:
	 *	1. The text string and the line buffer is empty.
	 *	2. The text string is all whitespace and we already have a trailing 
     *     space.
	 */
	
	if ( ( state != NULL ) && ( state->line_buf_len == 0 ) )
		{
		char *	t_ptr;

		/* if this string is empty, bail */
		if ( *text == '\0' )
			{
			return;
			}
		
		/* if it's only whitespace and we have a trailing space, then bail */
		if ( state->trailing_space )
			{
			t_ptr = text;
			
			while ( *t_ptr != '\0' )
				{
				if ( !XP_IS_SPACE( *t_ptr ) )
					{
					break;
					}
				
				++t_ptr;
				}
			
			if ( ( *t_ptr == '\0' ) && ( t_ptr != text ) )
				{
				return;
				}
			}
		}
	
	/* If we don't have a block, create one if we have a valid state
	 * record. Otherwise we have an error */
	
	if ( block == NULL )
		{
		/* the editor may call us with a NULL state and context
           record, in this case we must always have a block */
		XP_ASSERT(( state != NULL ) && ( context != NULL ));
		if ( ( state != NULL ) && ( context != NULL ) )
			{
			block = lo_CurrentTextBlock ( context, state );
			}
		}
	
	/* OPTIMIZATION: If the parser could tell us if we have a split
	 * buffer then we could intelligently set parseAllText here and
	 * not buffer words that we think may be split across a buffer but
	 * in reality are whole.  */
	 
	/* If we're in an editor context, then we can always parse the
	 * whole buffer of text, we never need to worry about partial
	 * buffers being passed to us.  */
	parseAllText = EDT_IS_EDITOR( context );
		
	/* Scan through the text, removing whitespace and adding words to
	 * the text buffer as we come across them.
	 *
	 * Particular things we have to deal with:
	 *		- Preformatted text (normal, word wrapped and column wrapped)
	 *		- Normal single byte text
	 *		- Multibyte text
	 *      - non-breaking spaces 
     */
	
	lo_GetTextParseAtributes ( state, &multiByte );

	if ( FALSE || multiByte )
		{
		lo_ParseDoubleText ( state, block, parseAllText, text );
		}
	else
		{
		lo_ParseSingleText ( state, block, parseAllText, text );
		}
}

static void
lo_GetTextParseAtributes ( lo_DocState * state, Bool * multiByte )
{
	int16	charset;
	
	*multiByte = FALSE;
	
	if ( state != NULL )
		{
		charset = state->font_stack->text_attr->charset;
		if ( (INTL_CharSetType ( charset ) != SINGLEBYTE ) && !( INTL_CharSetType ( charset ) & CS_SPACE ) )
			{
			*multiByte = TRUE;
			}
		}
}

static void
lo_ParseSingleText ( lo_DocState * state, LO_TextBlock * block, Bool parseAllText, char * text )
{
	uint8 *			t_ptr;
	uint8 *			w_start;
	uint8 *			w_end;
	uint8 *			line_buff;
	uint32			w_length;
	Bool			skipped_space;
	uint32			textLength;
	
	/* check for textTransform properties and aply them */
	if( ( state != NULL ) && ( state->top_state && state->top_state->style_stack ) )
		{
		char * property;
		
		StyleStruct *style_struct = STYLESTACK_GetStyleByIndex( state->top_state->style_stack, 0);

		if( style_struct )
			{
			property = STYLESTRUCT_GetString(style_struct, TEXT_TRANSFORM_STYLE);
			if(property)
				{
				lo_transform_text_from_string_method(text, property);
				}
			}
		}

	t_ptr = (uint8 *) text;
	skipped_space = FALSE;	
	
	if ( ( state != NULL ) && ( state->line_buf_len > 0 ) )
		{
		PA_LOCK(line_buff, uint8 *, state->line_buf);
		textLength = state->line_buf_len;
		
		if ( *line_buff == '\0' )
			{
			line_buff = NULL;
			textLength = 0;
			state->line_buf_len = 0;
			}
		}
	else
		{
		line_buff = NULL;
		textLength = 0;
		}
		
	/* Make sure the text block has enough space to hold this block of text */
	textLength += XP_STRLEN ( text );
	lo_GrowTextBlock ( block, textLength + 1 );
	
	/*
	 * If there's anything in the line buffer, then pull that out now
	 */
	if ( line_buff != NULL )
		{
		/* was there any trailing space left for us? */
		skipped_space = state->trailing_space;
			
		/* skip any white space at the head of our text */
		while ( ( *line_buff != '\0' ) && XP_IS_SPACE ( *line_buff ) )
			{
			line_buff++;
			state->line_buf_len--;
			skipped_space = TRUE;
			}
		
		/*
		 * if we skipped any space and we're not at the end of the buffer, 
		 * then insert a break position
		 */
		if ( *line_buff != '\0' )
			{
			if ( skipped_space )
				{
				if ( !lo_SetBreakPosition ( block ) )
					{
					state->top_state->out_of_memory = TRUE;
					return;
					}
				
				/* If the space was real whitespace and not a trailing
				 * space from a previous layout, then copy the space
				 * to the text block.  */
				if ( !state->trailing_space )
					{
					block->text_buffer[ block->buffer_write_index ] = ' ';
					block->buffer_write_index++;
					block->last_buffer_write_index++;
					skipped_space = TRUE;
					}

				skipped_space = FALSE;
				}
			
			/* copy in the line buffer. it is only allowed to be one word */
			lo_CopyText ( line_buff, &block->text_buffer[ block->buffer_write_index ], state->line_buf_len );
			block->buffer_write_index += state->line_buf_len;
			state->line_buf_len = 0;
			}
		}
	
	/* The last chunk of text may have left a single piece of
	 * whitespace at the end of the buffer. If so, we need to skip any
	 * whitespace at the front of the buffer so we don't have to worry
	 * about this inside the main loop.
	 *
	 * When called from the editor, we may not have a state
	 * record. However, we also won't need to worry about this case as
	 * it will have taken care of it for us.  */
	if ( ( state != NULL ) && ( state->trailing_space ) && ( *t_ptr != '\0' ) )
		{
		skipped_space = TRUE;
		
		while ( ( *t_ptr != '\0' ) && XP_IS_SPACE ( *t_ptr ) )
			{
			t_ptr++;
			}
		}
	
	while ( *t_ptr != '\0' )
		{
		w_start = t_ptr;
		
		/* skip past any whitespace before this word. */
		while ( ( *w_start != '\0' ) && XP_IS_SPACE ( *w_start ) )
			{
			w_start++;
			}
						
		/* run through the text and find the end of the word */
		w_end = w_start;
		w_length = 0;
		
		while ( ( *w_end != '\0' ) && !XP_IS_SPACE ( *w_end ) )
			{
			w_end++;
			w_length++;
			}

		/* If we hit the end of the buffer then we may be inside a
		 * partial word. This can cause problems with interword
		 * kerning, multibyte characters and other contextually
		 * sensitive script systems.
		 *
		 * We buffer this word in the line_buff. If this is truly the
		 * end of the text, then we'll be called to flush the last
		 * line. We'll do this by appending this word to our text
		 * block and then laying out the last of the text.
		 *
		 * If this word is just split across a buffer, then it will be
		 * inserted to the beginning of the next text block.
		 *
		 * If the caller tells us to parse the whole buffer, then we
		 * don't care.  */
		
		if ( !parseAllText && ( *w_end == '\0' ) && ( w_length > 0 ) && ( state != NULL ) )
			{
			if ( w_start != t_ptr )
				{
				/* put a space in the line buffer */
				lo_CopyTextToLineBuffer ( state, (uint8 *) " ", 1 );
				if ( state->top_state->out_of_memory )
					{
					return;
					}
				}
				
			/* put this text in the line buffer */
			lo_CopyTextToLineBuffer ( state, w_start, w_length );
			if ( state->top_state->out_of_memory )
				{
				return;
				}
				
			/* we now have text after the whitespace */
			skipped_space = FALSE;
			
			break;
			}
					
		/* If we skipped some white space, then we know that we can
		 * put a break here.  */
		if ( w_start != t_ptr )
			{
			skipped_space = TRUE;
			
			/* add the break position */
			if ( !lo_SetBreakPosition ( block ) )
				{
				if ( state != NULL )
					{
					state->top_state->out_of_memory = TRUE;
					}
				return;
				}

			if ( block->buffer_length < ( block->buffer_write_index + 1 ) )
				{
				if ( !lo_GrowTextBlock ( block, 1 ) )
					{
					if ( state != NULL )
						{
						state->top_state->out_of_memory = TRUE;
						}
					return;
					}
				}
			
			block->text_buffer[ block->buffer_write_index ] = ' ';
			block->buffer_write_index++;
			
			/* BRAIN DAMAGE: Add a new field to the text block struct
			 * to indicate how many chars to skip when calculating the
			 * length of the next run.  */
			block->last_buffer_write_index++;
			}
		else
			{
			skipped_space = FALSE;
			}
		
		/* if we found anything, add it to the buffer */
		if ( w_length > 0 )
			{
			if ( block->buffer_length < ( block->buffer_write_index + w_length ) )
				{
				if ( !lo_GrowTextBlock ( block, w_length ) )
					{
					if ( state != NULL )
						{
						state->top_state->out_of_memory = TRUE;
						}
					return;
					}
				}
			
			lo_CopyText ( w_start, &block->text_buffer[ block->buffer_write_index ], w_length );
			block->buffer_write_index += w_length;
			
			/* we now have text after the whitespace */
			skipped_space = FALSE;
			}
		
		t_ptr = w_end;
		}
		
		/*
		 * Remember whether the last thing we added was whitespace
		 */
		block->last_char_is_whitespace = skipped_space;
}


/*
 * Parse State Table for two byte text
 */

typedef enum {
	kUnprohibited = PROHIBIT_NOWHERE,
	kBeginProhibited = PROHIBIT_BEGIN_OF_LINE,
	kEndProhibited = PROHIBIT_END_OF_LINE,
	kWordBreakProhibited = PROHIBIT_WORD_BREAK,
	kSingleByte,
	kBreakableSpace,
	kFlushFinalRun,
	kNumCharTypes
} ParseState;

/*
 * Flags for the state table command
 */

#define	SET_BREAKABLE			0x01		/* dump the current run as a single breakable run */
#define	SET_MULTI_BREAKABLE		0x02		/* dump the current run as a mutibyte breakable run */
#define	DUMP_TEXT_AND_BREAK		0x04		/* dump the text to the buffer with no break point and then stop processing */
#define	CARRY_LAST_CHAR			0x08		/* move the last char of this run into the next state */
#define	INC_RUN_LENGTH			0x10		/* inc the length of this run (we could do without this one) */
#define	INSERT_WHITESPACE		0x20		/* insert a single whitespace into the text buffer */
#define	SKIP_CHAR				0x40		/* skip the current char */
#define	MAINTAIN_CHAR_TYPE		0x80		/* don't change the curCharType */

/*
 * Things to Note:
 *
 * 1. SET_BREAKABLE means to insert a normal break command. This run can 
 *    be broken at the end of the run.
 * 2. SET_MULTI_BREAKABLE means that we have a run which can be broken at 
 *    the end of every character.
 *
 */

/*
 * The mutibyte breakable run data word is 16 bits big. It is organized as:
 *
 *	BIT:	15	14	13			12	11	10	9	8	7	6	5	4	3	2	1	0
 *	FIELD:	CHAR SIZE			RUN LENGTH
 */

#define	MULTI_CHAR_SIZE_MASK	0xE000
#define	MULTI_CHAR_SIZE_SHIFT	12

#define	MULTI_LENGTH_MASK		0x1FFF

/*
 * The state table
 */

static uint8 gParseTable[ kNumCharTypes ][ kNumCharTypes ] = 
{
	/*	current char			next char					operations */
	
/* Unprohibited two byte text */
	/*	kUnprohibited,			kUnprohibited,			*/	INC_RUN_LENGTH,
	/*	kUnprohibited,			kBeginProhibited,		*/	SET_MULTI_BREAKABLE + CARRY_LAST_CHAR,
	/*	kUnprohibited,			kEndProhibited,			*/	SET_MULTI_BREAKABLE,
	/*	kUnprohibited,			kWordBreakProhibited,	*/	SET_MULTI_BREAKABLE,
	/*	kUnprohibited,			kSingleByte,			*/	SET_MULTI_BREAKABLE,
	/*	kUnprohibited,			kBreakableSpace,		*/	SET_MULTI_BREAKABLE + INSERT_WHITESPACE + SKIP_CHAR,
	/*	kUnprohibited,			kFlushFinalRun,			*/	SET_MULTI_BREAKABLE,
	
/* Begin Line Prohibited two byte text */
	/*	kBeginProhibited,		kUnprohibited,			*/	SET_BREAKABLE,
	/*	kBeginProhibited,		kBeginProhibited,		*/	INC_RUN_LENGTH,
	/*	kBeginProhibited,		kEndProhibited,			*/	SET_BREAKABLE,
	/*	kBeginProhibited,		kWordBreakProhibited,	*/	SET_BREAKABLE,
	/*	kBeginProhibited,		kSingleByte,			*/	SET_BREAKABLE,
	/*	kBeginProhibited,		kBreakableSpace,		*/	SET_BREAKABLE + INSERT_WHITESPACE + SKIP_CHAR,
	/*	kBeginProhibited,		kFlushFinalRun,			*/	DUMP_TEXT_AND_BREAK,
	
/* End Line Prohibited two byte text */
	/*	kEndProhibited,			kUnprohibited,			*/	INC_RUN_LENGTH + SET_BREAKABLE,
	/*	kEndProhibited,			kBeginProhibited,		*/	INC_RUN_LENGTH + SET_BREAKABLE,
	/*	kEndProhibited,			kEndProhibited,			*/	INC_RUN_LENGTH,
	/*	kEndProhibited,			kWordBreakProhibited,	*/	INC_RUN_LENGTH,
	/*	kEndProhibited,			kSingleByte,			*/	INC_RUN_LENGTH,
	/*	kEndProhibited,			kBreakableSpace,		*/	INC_RUN_LENGTH + MAINTAIN_CHAR_TYPE,	/* BIZZARE CASE! */
	/*	kEndProhibited,			kFlushFinalRun,			*/	DUMP_TEXT_AND_BREAK,					/* not much we can do here */
	
/* Word Break Prohibited two byte text */
	/*	kWordBreakProhibited,	kUnprohibited,			*/	SET_BREAKABLE,
	/*	kWordBreakProhibited,	kBeginProhibited,		*/	INC_RUN_LENGTH,
	/*	kWordBreakProhibited,	kEndProhibited,			*/	SET_BREAKABLE,
	/*	kWordBreakProhibited,	kWordBreakProhibited,	*/	INC_RUN_LENGTH,
	/*	kWordBreakProhibited,	kSingleByte,			*/	INC_RUN_LENGTH,
	/*	kWordBreakProhibited,	kBreakableSpace,		*/	SET_BREAKABLE + INSERT_WHITESPACE + SKIP_CHAR,
	/*	kWordBreakProhibited,	kFlushFinalRun,			*/	DUMP_TEXT_AND_BREAK,
	
/* Single byte text */
	/*	kSingleByte,			kUnprohibited,			*/	SET_BREAKABLE,
	/*	kSingleByte,			kBeginProhibited,		*/	INC_RUN_LENGTH,
	/*	kSingleByte,			kEndProhibited,			*/	SET_BREAKABLE,
	/*	kSingleByte,			kWordBreakProhibited,	*/	SET_BREAKABLE,
	/*	kSingleByte,			kSingleByte,			*/	INC_RUN_LENGTH,
	/*	kSingleByte,			kBreakableSpace,		*/	SET_BREAKABLE + INSERT_WHITESPACE + SKIP_CHAR,
	/*	kSingleByte,			kFlushFinalRun,			*/	DUMP_TEXT_AND_BREAK,
	
/* Single byte breakable space */
	/*	kBreakableSpace,		kUnprohibited,			*/	0,
	/*	kBreakableSpace,		kBeginProhibited,		*/	0,
	/*	kBreakableSpace,		kEndProhibited,			*/	0,
	/*	kBreakableSpace,		kWordBreakProhibited,	*/	0,
	/*	kBreakableSpace,		kSingleByte,			*/	0,
	/*	kBreakableSpace,		kBreakableSpace,		*/	SKIP_CHAR,
	/*	kBreakableSpace,		kFlushFinalRun,			*/	0,
	
/* These are never hit */
	/*	kFlushFinalRun,			kUnprohibited,			*/	0,
	/*	kFlushFinalRun,			kBeginProhibited,		*/	0,
	/*	kFlushFinalRun,			kEndProhibited,			*/	0,
	/*	kFlushFinalRun,			kWordBreakProhibited,	*/	0,
	/*	kFlushFinalRun,			kSingleByte,			*/	0,
	/*	kFlushFinalRun,			kBreakableSpace,		*/	0,
	/*	kFlushFinalRun,			kFlushFinalRun,			*/	0,
};

static void
lo_ParseDoubleText ( lo_DocState * state, LO_TextBlock * block, Bool parseAllText, char * text )
{
	char *			tptr;
	char *			wordStart;
	char *			nextWordStart;
	int32			runLength;
	int32			nextRunLength;
	int32			curCharBytes;
	int32			nextCharBytes;
	int16			charset;
	ParseState		curCharType;
	ParseState		nextCharType;
	uint8			parseCommand;
	uint32			textLength;
	Bool			startNewRun;
	Bool			eachCharBreakable;
	Bool			processLastRun;
	
	tptr = text;
	wordStart = text;
	runLength = 0;
	
	textLength = 0;
	
	processLastRun = FALSE;
	
	/* BRAIN DAMAGE: We need to see if there's anything in the line
       buffer for us */
	
	/* Make sure the text block has enough space to hold this block of text */
	textLength += XP_STRLEN ( text );
	lo_GrowTextBlock ( block, textLength + 1 );

	charset = block->text_attr->charset;
	curCharType = kSingleByte;
	curCharBytes = 1;
	
	eachCharBreakable = FALSE;
	
	/* if we have a trailing space, then set our state to be a space */
	if ( state->trailing_space )
		{
		curCharType = kBreakableSpace;
		runLength = curCharBytes;
		}
	
	startNewRun = FALSE;
		
	while ( ( *tptr != '\0' ) || processLastRun )
		{
		if ( processLastRun )
			{
			/* force the last run to be flushed */
			nextCharType = kFlushFinalRun;
			nextCharBytes = 0;
			}
		else
			{		
			/* do we have an ascii char? */
			if ( ( (unsigned char) *tptr ) <= 0x7F )
				{
				nextCharBytes = 1;
				
				/* is the next char a breakable space? */
				if ( XP_IS_SPACE( *tptr ) )
					{
					nextCharType = kBreakableSpace;
					}
				else
				/* it's a normal char */
					{
					nextCharType = kSingleByte;
					}
				}
			else
				{
				/* multibyte, do that international thing */
				nextCharBytes = INTL_CharLen( charset, (unsigned char *) tptr);
				nextCharType = (ParseState) INTL_KinsokuClass( charset, (unsigned char *) tptr );
				}
			}
		
		/* now get our parse command */
		parseCommand = gParseTable[ curCharType ][ nextCharType ];
		nextRunLength = nextCharBytes;
		nextWordStart = tptr;
		
		/* Unprohibited multibyte check - we need to catch cases where
		 * our byte size changes. We might want to add a command bit
		 * for this check, for now I shall do it this way.  */
		
		if ( ( curCharType == kUnprohibited ) && ( nextCharType == kUnprohibited ) )
			{
			if ( nextCharBytes != curCharBytes )
				{
				/* ok, our char size changed. we need to dump this run */
				parseCommand |= SET_BREAKABLE;
				}
			}
			
		/* process the command */
		if ( parseCommand & CARRY_LAST_CHAR )
			{
			/* move the last char of this run into the next run */
			runLength -= curCharBytes;
			nextWordStart -= curCharBytes;
			
			/* and move the carried char of the previous run to the next one */
			nextRunLength = curCharBytes + nextCharBytes;
			
			/* if the old run is empty, we don't want to do anything else  */
			if ( runLength == 0 )
				{
				parseCommand = 0;
				startNewRun = TRUE;
				}
			}
			
		if ( parseCommand & INC_RUN_LENGTH )
			{
			runLength += nextCharBytes;
			}

		if ( parseCommand & SET_BREAKABLE )
			{
			lo_CopyText ( (uint8 *) wordStart, &block->text_buffer[ block->buffer_write_index ], runLength );
			block->buffer_write_index += runLength;
			
			/* set a breakable run */
			lo_SetBreakPosition ( block );
				
			startNewRun = TRUE;
			}

		if ( parseCommand & SET_MULTI_BREAKABLE )
			{
			lo_CopyText ( (uint8 *) wordStart, &block->text_buffer[ block->buffer_write_index ], runLength );
			block->buffer_write_index += runLength;
			
			/* set an unbreakable run */
			lo_SetMultiByteRun ( block, curCharBytes, TRUE, eachCharBreakable );
			startNewRun = TRUE;
			}
		
		if ( parseCommand & DUMP_TEXT_AND_BREAK )
			{
			/* copy the text out but don't set a break (only happens
               when flushing at the end) */
			lo_CopyText ( (uint8 *) wordStart, &block->text_buffer[ block->buffer_write_index ], runLength );
			block->buffer_write_index += runLength;
			
			/* now break out of the loop - we're done */
			break;
			}
			
		if ( startNewRun )
			{
			wordStart = nextWordStart;
			runLength = nextRunLength;
			startNewRun = FALSE;
			}
		
		if ( parseCommand & SKIP_CHAR )
			{
			wordStart++;
			}
		
		if ( parseCommand & INSERT_WHITESPACE )
			{
			block->text_buffer[ block->buffer_write_index ] = ' ';
			block->buffer_write_index++;
			block->last_buffer_write_index++;
			}
		
		curCharBytes = nextCharBytes;
		
		if ( !( parseCommand & MAINTAIN_CHAR_TYPE ) )
			{
			curCharType = nextCharType;
			}
			
		eachCharBreakable = curCharType == kUnprohibited;
		
		tptr += nextCharBytes;
		
		/* if we got here with processLastRun, then we need to bail */
		if ( processLastRun )
			{
			break;
			}
			
		/* if we're on the last character, then we may have a partial
		 * run left over. if we're parsing all text then we need to
		 * dump it.  */
		if ( parseAllText && ( *tptr == 0 ) && ( runLength > 0 ) )
			{
			processLastRun = TRUE;
			}
		}
	
	/* if we've ended and we have a run, then we need to save it in
       the line buffer */
	if ( ( runLength > 0 ) && ( *wordStart != 0 ) )
		{
		lo_CopyTextToLineBuffer ( state,(uint8 *)  wordStart, runLength );
		}
}

static void
lo_FlushText ( MWContext * context, lo_DocState * state )
{
	LO_TextBlock *	block;
	Bool			multiByte;
	char *			text_buf;
	
	block = state->cur_text_block;
	
	if ( block != NULL )
		{
		lo_GetTextParseAtributes ( state, &multiByte );
		
		/* add any text to the text block that may be sitting in the
           line buffer */
		/* BRAIN DAMAGE: These should both be handled the same way */
		if ( multiByte )
			{
			if ( state->line_buf_len > 0 )
				{
				PA_LOCK(text_buf, char *, state->line_buf);
				lo_ParseDoubleText ( state, block, TRUE, text_buf );
				PA_UNLOCK(state->line_buf);
				}
			}
		else
			{
			lo_AppendTextToBlock ( context, state, block, "" );
			}
			
		lo_LayoutTextBlock ( context, state, TRUE );
		}
}

static void
lo_SetupBreakState ( LO_TextBlock * block )
{
	Bool			canBreak;
	uint32			wordLength;
	uint32			lineLength;
	uint8 *			runEnd;
	uint32			searchReadIndex;
	BreakState		breakState;
	
	searchReadIndex = block->buffer_read_index;
	
	block->buffer_read_index = 0;
	block->break_read_index = 0;
	block->last_line_break = 0;

	/* We need to update our state to be at the current text position
	 * (specified by buffer_read_index) */
	
	lineLength = 0;

	/* run through the break table until we get to the correct read index */
	while ( block->buffer_read_index < searchReadIndex )
		{
		SAVE_BREAK_STATE ( block, &breakState, lineLength );			
				
		runEnd = lo_GetNextTextPosition ( block, &wordLength, &lineLength, &canBreak );
		if ( runEnd == NULL )
			{
			/* this should not happen, but just in case, let's do
               something kinda reasonable */
			lo_RestoreBreakState ( block, &breakState, NULL );
			break;
			}
		}

	/* If we're not at the beginning of the text buffer, then we need
	 * to increment buffer_read_index so that we skip the breakable
	 * space we're currently at.  */
	if ( ( block->buffer_read_index > 0 ) && XP_IS_SPACE ( block->text_buffer[ block->buffer_read_index ] ) )
		{
		block->buffer_read_index++;
		}
		
	block->last_line_break = block->buffer_read_index;
}

void lo_LayoutTextBlock ( MWContext * context, lo_DocState * state, Bool flushLastLine )
{
	LO_TextBlock *	block;
	Bool			allTextFits;
	Bool			canBreakAtStart;
	LO_TextStruct *	text_data;
	LO_TextStruct	msTextData;
	uint32			width;
	uint32			lineLength;
	uint8 *			text;
	int32			baseline_inc;
	int32			line_inc;
	int32			minWidth;
	int32 *			minWidthPtr;
	BreakState		breakState;
	uint16 *		charLocs;
	Bool			freeMeasureBuffer;
	Bool			justify;
	
	block = state->cur_text_block;
	if ( block == NULL )
		{
		return;
		}
	
	justify = ( state->align_stack != NULL ) && ( state->align_stack->alignment == LO_ALIGN_JUSTIFY );
		
	/* bail if we're at the end of this block */
	if ( block->buffer_read_index == block->buffer_write_index )
		{
		/* if there's also no text in the line buffer, clear this block */
		if ( state->line_buf_len == 0 )
			{
			state->cur_ele_type = LO_NONE;
			state->cur_text_block = NULL;
			state->trailing_space = block->last_char_is_whitespace;
			}
		return;
		}
	
	lineLength = 0;
	
	charLocs = NULL;
	freeMeasureBuffer = FALSE;
	
	/* find the width of an average character */
	if ( block->totalWidth == 0 )
		{
		memset (&msTextData, 0, sizeof (LO_TextStruct));
		msTextData.text = (PA_Block) "aeiou";
		msTextData.text_attr = block->text_attr;
		msTextData.text_len = 5;
		
		FE_GetTextInfo ( context, &msTextData, &state->text_info );
		block->totalWidth = state->text_info.max_width;
		block->totalChars = msTextData.text_len;
		}

#ifdef XP_MAC
	/* do a quick test to see if we could overflow a UInt16 */
	if ( ( block->buffer_write_index * block->totalWidth / block->totalChars ) < 65535 )
		{
		/* measure the text using the fast measure text */
		if ( ( block->buffer_write_index + 1 ) < kStaticMeasureTextBufferSize )
			{
			charLocs = gMeasureTextBuffer;
			}
		else
			{
			charLocs = XP_ALLOC( ( block->buffer_write_index + 1 ) * sizeof(uint16) );
			freeMeasureBuffer = TRUE;
			}
			
		if ( charLocs != NULL )
			{
			memset (&msTextData, 0, sizeof (LO_TextStruct));
			msTextData.text = (PA_Block) block->text_buffer;
			msTextData.text_attr = block->text_attr;
			msTextData.text_len = block->buffer_write_index;
			FE_MeasureText ( context, &msTextData, (int16 *) charLocs );
			
			/* BRAIN DAMAGE: We need to figure out for sure if the
               width has ever exceeded a uint16! */
			/* the actual interface for MeasureText says it takes an
               array of signed ints, however */
			/* the data stuffed in there is unsigned (we quickly
               overflow a int16 with an 8Kb buffer) */
			}
		}
#endif
	
	/* Given the current layout state, run through this text block and
	 * convert all that we can into text elements.  If flushLastLine
	 * is set, we want to flush all the text, rather than keeping the
	 * last bit in case more text comes.  */
	
	allTextFits = FALSE;
	
	/* if we're laying out a table, then we need to calculate min widths */
	minWidthPtr = state->need_min_width ? &minWidth : NULL;
	
	while ( !allTextFits )
		{
		canBreakAtStart = FALSE;

		/* Save the current break state in case we need to delay on
           the last line */
		SAVE_BREAK_STATE ( block, &breakState, lineLength );
				
		/* We need to handle the case where we're at the beginning of
		 * the line and the first character is a breakable space.  */
		if ( state->at_begin_line )
			{
			canBreakAtStart = lo_SkipInitialSpace ( block );
#ifdef LOG
			PR_LogPrint ( "canBreakAtStart: %d\n", canBreakAtStart );
			PR_LogFlush();
#endif
			}
		
		/* we're looking for a new break position */
		state->break_pos = -1;
		state->break_width = -1;

		text = lo_GetLineStart ( block );
				
		lineLength = lo_FindLineBreak ( context, state, block, text, charLocs, &width, minWidthPtr, &allTextFits );

		/* update the state's min_width if we need to - this will be
           constant even if we don't flush this line */
		if ( minWidthPtr != NULL )
			{
			if ( minWidth > state->min_width )
				{
				state->min_width = minWidth;
				}
			}
		
		/* if this line is too long, we either need to break at the
		 * beginning of the run (if we can) or break at an old
		 * element. If we can't do either of those then we just have
		 * to make the line too big */
		
		if ( ( width > ( state->right_margin - state->x ) ) && ( state->x > state->left_margin ) )
			{
			/* could we have broken at the start of this line? */
			if ( canBreakAtStart )
				{
#ifdef LOG
				PR_LogPrint ( "Too long - Breaking at the start of the line\n" );
				PR_LogFlush();
#endif

				/* break the line here */
				lo_SoftLineBreak( context, state, TRUE );
				
				/* BUG BUG: We're restoring the break state to the
				 * beginning of the buffer - ie to before the space we
				 * skipped above. We need to fix the space skipping
				 * mechanism to remove this case (we can go into an
				 * infinite loop here if there's not enough space for
				 * the first word).
				 *
				 * Should be able to have a new flag "canSkipSpace"
				 * but then actually don't skip it. Then if the line
				 * does fit and it's at the beginning, we can skip the
				 * space. lo_FindLineBreak should probably be the one
				 * to do this work so that the width we get back is
				 * correct.  */
				lo_RestoreBreakState ( block, &breakState, NULL );
				
				/* if all the text fits (ie there was only an
				 * unbreakable run left in this block), then we need
				 * to stick with this break position. Otherwise we can
				 * go find a new one */
				if ( !allTextFits )
					{
					continue;
					}
				}
			else
			/* do we have an old break position we can use? */
			if ( state->old_break_pos != -1 )
				{
#ifdef LOG
				PR_LogPrint ( "Too long - Breaking at the old_break_pos: %ld, width %ld\n"
					state->old_break_pos, state->old_break_width );
				PR_LogFlush();
#endif
				lo_BreakOldElement ( context, state );
				lo_RestoreBreakState ( block, &breakState, NULL );
				
				/* if all the text fits (ie there was only an
				 * unbreakable run left in this block), then we need
				 * to stick with this break position. Otherwise we can
				 * go find a new one */
				if ( !allTextFits )
					{
					continue;
					}
				}
			
			/* we're screwed, we just have to make this line too long */
			}
						
		/* We may not necessarily want to flush the whole buffer out
		 * to layout elements (the case where were processing part of
		 * a text chunk based on what netlib has streamed to us).
		 *
		 * We know we do want to flush this next line out if we still
		 * have more text in this buffer to process or layout really
		 * does want us to flush this whole buffer (because some other
		 * element is after us).  */
		if ( !allTextFits || flushLastLine )
			{
			state->width = width;

			if ( lineLength > 0 )
				{
				text_data = (LO_TextStruct *)lo_NewElement ( context, state, LO_TEXT, NULL, 0 );
				if (text_data == NULL)
					{
#ifdef DEBUG
					assert (state->top_state->out_of_memory);
#endif
					break;
					}

				text_data->type = LO_TEXT;
				text_data->ele_id = NEXT_ELEMENT;
				text_data->x = state->x;
				text_data->x_offset = 0;
				text_data->y = state->y;
				text_data->y_offset = 0;
				text_data->width = width;
				text_data->height = 0;
				text_data->next = NULL;
				text_data->prev = NULL;

				text_data->text = (PA_Block) text;
				text_data->text_len = lineLength;

				text_data->anchor_href = block->anchor_href;
				text_data->text_attr = block->text_attr;
				text_data->ele_attrmask = block->ele_attrmask;
				
				/* BRAIN DAMAGE: Set LO_ELE_INVISIBLE to mark the
                   element as not having a valid text ptr */
				XP_ASSERT ( !(text_data->ele_attrmask & LO_ELE_INVISIBLE ) );
				text_data->ele_attrmask |= LO_ELE_INVISIBLE;
				
				text_data->sel_start = -1;
				text_data->sel_end = -1;

				text_data->doc_width = state->right_margin - state->x;
				text_data->doc_width = 0;
				text_data->block_offset = block->buffer_read_index;
				XP_ASSERT(block->buffer_read_index <= 65535);
					
				/*
				 * Some fonts (particulatly italic ones with curly tails
				 * on letters like 'f') have a left bearing that extends
				 * back into the previous character.  Since in this case the
				 * previous character is probably not in the same font, we
				 * move forward to avoid overlap.
				 *
				 * Those same funny fonts can extend past the last
				 * character, and we also have to catch that, and
				 * advance the following text to eliminate cutoff.  */
				if ( state->text_info.lbearing < 0 )
					{
					text_data->x_offset = state->text_info.lbearing * -1;
					}
					
				baseline_inc = 0;
				line_inc = 0;
				
				/* The baseline of the text element just added to the
				 * line may be less than or greater than the baseline
				 * of the rest of the line due to font changes.  If
				 * the baseline is less, this is easy, we just
				 * increase y_offest to move the text down so the
				 * baselines line up.  For greater baselines, we can't
				 * move the text up to line up the baselines because
				 * we will overlay the previous line, so we have to
				 * move all the previous elements in this line down.
				 *
				 * If the baseline is zero, we are the first element
				 * on the line, and we get to set the baseline.  */
				if ( state->baseline == 0 )
					{
					state->baseline = state->text_info.ascent;
					if (state->line_height < 
						(state->baseline + state->text_info.descent))
						{
						state->line_height = state->baseline +
							state->text_info.descent;
						}
					}
				else if ( state->text_info.ascent < state->baseline )
					{
					text_data->y_offset = state->baseline - state->text_info.ascent;
					if ( ( text_data->y_offset + state->text_info.ascent + state->text_info.descent ) > state->line_height )
						{
						line_inc = text_data->y_offset + state->text_info.ascent + state->text_info.descent -
									state->line_height;
						}
					}
				else
					{
					baseline_inc = state->text_info.ascent - state->baseline;
					if ( ( text_data->y_offset + state->text_info.ascent + state->text_info.descent - baseline_inc ) >
							state->line_height)
						{
						line_inc = text_data->y_offset + state->text_info.ascent + state->text_info.descent -
									state->line_height - baseline_inc;
						}
					}
				
				/* Append this element to layout's linelist and our
				 * own list of text elements belonging to this block */
				lo_AppendToLineList ( context, state, (LO_Element *) text_data, baseline_inc );
				if ( block->startTextElement == NULL )
					{
					block->startTextElement = text_data;
					block->endTextElement = text_data;
					}
				else
					{
					block->endTextElement = text_data;
					}
				
				/* we know we're not at the beginning of the line anymore */
				state->at_begin_line = FALSE;
				
				state->baseline += (intn) baseline_inc;
				state->line_height += (intn) (baseline_inc + line_inc);
				text_data->height = state->text_info.ascent + state->text_info.descent;

				/*
				 * If the element we just flushed had a breakable word
				 * position in it, save that position in case we have
				 * to go back and break this element before we finish
				 * the line.
				 */
				if ( state->break_pos != -1 )
					{
					state->old_break = text_data;
					state->old_break_block = block;
					state->old_break_pos = state->break_pos;
					state->old_break_width = state->break_width;
					}

				state->linefeed_state = 0;
				state->x += state->width;
				state->width = 0;
				}
				
			/* If we're still processing text in this buffer, put a
			 * linebreak out there */
			if ( !allTextFits && !justify )
				{
				lo_SoftLineBreak(context, state, TRUE);
				}
			
			/* tell the break engine that we broke the line here */
			lo_SetLineBreak ( block, !justify );


#ifdef EDITOR
			/* tell the editor where we are */
			state->edit_current_offset = block->last_line_break;
#endif
				
			if ( !( allTextFits && !flushLastLine ) )
				{
				/* Skip the break character if it's whitespace. We
				 * don't need to worry about non-breaking spaces here
				 * as if the space was non-breaking, we would not have
				 * broken the line here */

				if ( XP_IS_SPACE ( *text ) && !allTextFits )
					{
					/* BRAIN DAMAGE: We should be able to do this at
                       the start of the line */
/*					lo_SkipCharacter ( block ); */
					}
				}
			
			}
		}
	
	if ( flushLastLine )
		{
		state->cur_ele_type = LO_NONE;
		state->cur_text_block = NULL;
		state->trailing_space = block->last_char_is_whitespace;
		}
	else
	if ( allTextFits )
		{
		/* we're still inside a group of text elements */
		state->cur_ele_type = LO_TEXT;
		state->trailing_space = block->last_char_is_whitespace;
		
		/* We're not going to flush the last line, so restore our
           break state to the start of the line */
		lo_RestoreBreakState ( block, &breakState, NULL );
		}
	
	if ( freeMeasureBuffer )
		{
		XP_FREE( charLocs );
		}
}

int32 lo_ComputeTextMinWidth ( lo_DocState * state, int32 wordWidth, Bool canBreak );
int32 lo_ComputeTextMinWidth ( lo_DocState * state, int32 wordWidth, Bool canBreak )
{
	int32 new_break_holder;
	int32 min_width;
	int32 indent;

	new_break_holder = state->x + wordWidth;
	min_width = new_break_holder - state->break_holder;
	indent = state->list_stack->old_left_margin - state->win_left;
	min_width += indent;

	/* If we are not within <NOBR> content, allow break_holder
	 * to be set to the new position where a line break can occur.
	 * This fixes BUG #70782
	 */
	if ( ( state->breakable != FALSE ) && canBreak) {
		state->break_holder = new_break_holder;
	}
	
	return min_width;
}

static uint32
lo_FindLineBreak ( MWContext * context, lo_DocState * state, LO_TextBlock * block, uint8 * text,
	uint16 * widthTable, uint32 * width, int32 * minWidth, Bool * allTextFits )
{
	LO_TextStruct	text_data;
	uint32			breakCount;
	Bool			skipEndSpace;
	Bool			haveTooShort;
	Bool			canBreak;
	BreakState		tooShortBreak;
	Bool			haveTooLong;
	uint32			wordLength;
	uint32			breakChar;
	uint32			lineLength;
	uint32			prevLineLength;
	uint8 *			wordStart;
	LO_TextInfo		text_info;
	uint32			runLength;
	int32			docWidth;
	BreakState		breakState;
	uint8 *			runEnd;
	int32			oldBreakPos;
	int32			oldBreakWidth;
	int32			lineWidth;
	Bool			justify;
#ifdef BREAK_GUESS_TRACK
	uint32			numForwardMoves;
	uint32			numBackwardMoves;
	
	numForwardMoves = 0;
	numBackwardMoves = 0;
#endif
	
	*allTextFits = FALSE;
	
	memset (&text_data, 0, sizeof (LO_TextStruct));
	text_data.text = (PA_Block) text;
	text_data.text_attr = block->text_attr;
	
	if ( minWidth != NULL )
		{
		*minWidth = 0;
		}

	justify = ( state->align_stack != NULL ) && ( state->align_stack->alignment == LO_ALIGN_JUSTIFY );
	
	/* guess where we want to break this line */
	docWidth = state->right_margin - state->x;
	if ( docWidth < 0 )
		{
		/* we should never get here - the line before us needs to have been broken */
		docWidth = 0;
		}
	
	lineLength = block->buffer_write_index - block->buffer_read_index;
	if ( state->breakable )
		{
		breakChar = docWidth * block->totalChars / block->totalWidth;
		if ( breakChar > lineLength )
			{
			breakChar = lineLength;
			}
		}
	else
		{
		breakChar = lineLength;
		}
	
	/* We first need to walk through the word runs until we get to the
	 * first one before our break character.
	 *
	 * OPTIMIZATION: Make lo_GetNextTextPosition take a breakChar and
	 * have it walk forward to that position in an inner loop. This
	 * will save us a ton of calls (we currently spend about 5% if our
	 * time in lo_GetNextTextPosition - not huge but significant).  */

	lineLength = 0;
	
	breakCount = 0;
	skipEndSpace = FALSE;
	
	wordStart = text;
	
	haveTooShort = FALSE;
	haveTooLong = FALSE;

	oldBreakPos = -1;
	oldBreakWidth = -1;
	
	lineWidth = 0;
#ifdef LOG
	if ( !gHaveLog )
		{
		PR_SetLogFile ( "TextLog" );
		gHaveLog = true;
		}

	PR_LogPrint ( "Finding initial break position\n" );
#endif

	/* get the next break position */
	while ( TRUE )
		{
		prevLineLength = lineLength;
		
#ifdef LOG
		PR_LogPrint ( "Get next break position\n" );
		PR_LogFlush();
#endif
		/* Save the current break position in case it ends up being
           the one we need */
		if ( breakCount > 0 && canBreak )
			{
			oldBreakPos = lineLength;
			}
		
		SAVE_BREAK_STATE ( block, &breakState, lineLength );			
		runEnd = lo_GetNextTextPosition ( block, &wordLength, &lineLength, &canBreak );
		if ( runEnd == NULL )
			{
#ifdef LOG
			PR_LogPrint ( "End of break table\n" );
			PR_LogFlush();
#endif
			
			/* we hit the end of the break table */
			runEnd = lo_RestoreBreakState ( block, &breakState, &lineLength );
			break;
			}

		/* do we need to calculate min_width's? */
		if ( minWidth != NULL )
			{
			int32 min_width;
			
			if ( widthTable != NULL )
				{
				uint32	startWordWidth;
				uint32	endWordWidth;
				
				startWordWidth = widthTable[ block->last_line_break + prevLineLength ];
				endWordWidth = widthTable[ block->last_line_break + lineLength ];
				
				runLength = endWordWidth - startWordWidth;
				}
			else
				{
				text_data.text = (PA_Block) wordStart;
				text_data.text_len = lineLength - prevLineLength;
				FE_GetTextInfo ( context, &text_data, &text_info );
				
				runLength = text_info.max_width;
				}
			
			/* add the width of this word into our line width */
			lineWidth += runLength;
			
			/* compute the real min width based on the last break position */
			min_width = lo_ComputeTextMinWidth ( state, lineWidth, canBreak );
			if ( min_width > *minWidth )
				{
				*minWidth = min_width;
				}
			
			wordStart = runEnd;
			}

#ifdef LOG
		PR_LogPrint ( "wordlen: %lu, lineLength: %lu, canBreak: %d, runEnd: %s\n", wordLength, lineLength, canBreak, runEnd );
		PR_LogFlush();
#endif
		
		/* Are we where we want to be yet? */
		if ( lineLength >= breakChar )
			{
#ifdef LOG
			PR_LogPrint ( "Moved past, back up\n" );
			PR_LogFlush();
#endif
			
			/* if we moved past it then back up if we can */
			if ( ( lineLength > breakChar ) && ( breakCount > 0 ) )
				{
				runEnd = lo_RestoreBreakState ( block, &breakState, &lineLength );			
				--breakCount;
				}
				
			break;
			}
		
		/* if we're justifying text, then we just dump the next break
           position */
		if ( justify )
			{
			break;
			}
			
		/* we can now back up to something */
		++breakCount;
		}

	/* So now we're looking at the nearest break position to where we
	 * guessed we'd want to be.  Now we loop measuring this line of
	 * text until we find the best break position */
	
#ifdef LOG
	PR_LogPrint ( "Finding actual break position\n" );
	PR_LogFlush();
#endif
	
	while ( TRUE )
		{
		if ( widthTable != NULL )
			{
			uint32	startLineWidth;
			uint32	endLineWidth;
			
			startLineWidth = widthTable[ block->last_line_break ];
			endLineWidth = widthTable[ block->last_line_break + lineLength ];
			
			runLength = endLineWidth - startLineWidth;
			}
		else
			{
			text_data.text = (PA_Block) text;
			text_data.text_len = lineLength;
			FE_GetTextInfo ( context, &text_data, &text_info );
				
			runLength = text_info.max_width;
			}

#ifdef LOG
		PR_LogPrint ( "lineLength: %lu, width: %lu, docWidth: %lu, text: %s\n", lineLength, runLength, docWidth, text );
		PR_LogFlush();
#endif
		/* if we're justified text, then we just dump the word we have now */
		if ( justify )
			{
			/* save this break position */
			if ( canBreak )
				{
				oldBreakPos = lineLength;
				oldBreakWidth = runLength;
				
				/* if the next char along is a space, then we need to include it */
				if ( ( runEnd != NULL ) && XP_IS_SPACE ( *runEnd )  )
					{
					++lineLength;
					}
				}
				
			break;
			}
		else
		/* are we non-breakable text? */
		if ( !state->breakable )
			{
			/* We always want to move to the end of the text block. We
			 * should already be there from the loop above.  */
#ifdef LOG
			PR_LogPrint ( "Non-breakable, always get next break point\n" );
			PR_LogFlush();
#endif
			
			/* go forward one break position and measure again
               (including min width) */
			SAVE_BREAK_STATE ( block, &tooShortBreak, lineLength );
			haveTooShort = TRUE;
			
#ifdef BREAK_GUESS_TRACK
			++numForwardMoves;
#endif			
			wordStart = runEnd;
			runEnd = lo_GetNextTextPosition ( block, &wordLength, &lineLength, &canBreak );
			if ( runEnd == NULL )
				{				
				/* we've already at the end of the line */
				runEnd = lo_RestoreBreakState ( block, &tooShortBreak, &lineLength );
#ifdef LOG
				PR_LogPrint ( "Non-breakable text, hit end of block: %lu, runEnd: %s\n", lineLength, runEnd );
				PR_LogFlush();
#endif
				/* update min_width */
				if ( minWidth != NULL )
					{
					int32 min_width;
										
					/* compute the real min width based on the last
                       break position - we cannot break here */
					min_width = lo_ComputeTextMinWidth ( state, runLength, FALSE );
					if ( min_width > *minWidth )
						{
						*minWidth = min_width;
						}
					}
				break;
				}
			}
		else
		/* have we gone too far? */
		if ( runLength > docWidth )
			{
#ifdef LOG
			PR_LogPrint ( "Too long\n" );
			PR_LogFlush();
#endif
			
			/* if we found a break position before this one that was
               too short, choose the too short one */
			if ( haveTooShort )
				{
#ifdef LOG
				PR_LogPrint ( "Using too short\n" );
				PR_LogFlush();
#endif
				runEnd = lo_RestoreBreakState ( block, &tooShortBreak, &lineLength );
				break;
				}
			
			/* if we have something to back up to, then do
               so. Otherwise we have to break here */
			if ( breakCount > 0 )
				{
#ifdef LOG
				PR_LogPrint ( "Backing up to previous break position\n" );
				PR_LogFlush();
#endif
#ifdef BREAK_GUESS_TRACK
				++numBackwardMoves;
#endif			

				/* mark that we've been to far and back up one */
				haveTooLong = TRUE;
				runEnd = lo_GetPrevTextPosition ( block, &wordLength, &lineLength, &canBreak );
				if ( runEnd == NULL )
					{
					/* we need to break at a previous break position
                       on this line... */
					break;
					}
					
				wordStart = runEnd;
				--breakCount;
				continue;
				}
			else
				{
#ifdef LOG
				PR_LogPrint ( "Nothing to back up to, bailing\n" );
				PR_LogFlush();
#endif
				break;
				}
			}
		else
		/* have we not gone far enough? */
		if ( runLength < docWidth )
			{
#ifdef LOG
			PR_LogPrint ( "Too short\n" );
			PR_LogFlush();
#endif
			/* if we have a too long break position, then we know
               we're straddling the break point, choose */
			/* this one */
			if ( haveTooLong )
				{
#ifdef LOG
				PR_LogPrint ( "Using this break, next is too long\n" );
				PR_LogFlush();
#endif
				break;
				}

			/* save this break position */
			if ( canBreak )
				{
				oldBreakPos = lineLength;
				oldBreakWidth = runLength;
				}
			
			/* go forward one break position and measure again
               (including min width) */
			SAVE_BREAK_STATE ( block, &tooShortBreak, lineLength );
			haveTooShort = TRUE;
			
#ifdef BREAK_GUESS_TRACK
			++numForwardMoves;
#endif			
			
			wordStart = runEnd;
			runEnd = lo_GetNextTextPosition ( block, &wordLength, &lineLength, &canBreak );
			if ( runEnd == NULL )
				{				
				/* we've already at the end of the line */
				runEnd = lo_RestoreBreakState ( block, &tooShortBreak, &lineLength );
#ifdef LOG
				PR_LogPrint ( "No next break position, use this one. length: %lu, runEnd: %s\n", lineLength, runEnd );
				PR_LogFlush();
#endif
				break;
				}
			
			/*
			 * Update min width.
			 */
			if ( minWidth != NULL )
				{
				int32 min_width;

				if ( widthTable != NULL )
					{
					uint32	startLineWidth;
					uint32	endLineWidth;
					
					startLineWidth = widthTable[ block->last_line_break ];
					endLineWidth = widthTable[ block->last_line_break + lineLength ];
					
					runLength = endLineWidth - startLineWidth;
					}
				else
					{
					text_data.text = (PA_Block) wordStart;
					text_data.text_len = wordLength;
					FE_GetTextInfo ( context, &text_data, &text_info );
					
					/* add the length of this word into the length of
                       the line */
					runLength += text_info.max_width;
					}
									
				/* compute the real min width based on the last break
                   position */
				min_width = lo_ComputeTextMinWidth ( state, runLength, canBreak );
				if ( min_width > *minWidth )
					{
					*minWidth = min_width;
					}
				}
				
			++breakCount;
			continue;
			}
		else
			{
			/* we're spot on! */
			break;
			}
		}
	
	/* We may be in a nasty case where our current break position is
	 * before our last saved one in oldBreakPos. This can happen when
	 * we move backwards from our initial break guess. To correct
	 * this, we need to back up from our current break point, get the
	 * new position and then move forward again.  */
	if ( ( oldBreakPos != -1 ) && ( oldBreakPos >= lineLength ) )
		{
		uint32	dummyWordLength;
		uint32	prevBreakPos;
		Bool	dummyCanBreak;
		uint8 *	prevRunEnd;
			
#ifdef BREAK_GUESS_TRACK
			++numBackwardMoves;
#endif			
		prevBreakPos = lineLength;
		
		SAVE_BREAK_STATE ( block, &breakState, prevBreakPos );
		prevRunEnd = lo_GetPrevTextPosition ( block, &dummyWordLength, &prevBreakPos, &dummyCanBreak );
		if ( prevRunEnd != NULL )
			{
			/* we found a valid previous break, so use it */
			oldBreakPos = prevBreakPos;
			oldBreakWidth = -1;
			}
		else
			{
			/* nothing to back up to, so don't record any old break */
			oldBreakPos = -1;
			oldBreakWidth = -1;
			}
		
		/* restore the current break state */
		lo_RestoreBreakState ( block, &breakState, &prevBreakPos );
		}
			
	text_data.text = (PA_Block) text;

	/* If we don't have a width for the oldBreakPos, measure one now */
	if ( ( oldBreakPos != -1 ) && ( oldBreakWidth == -1 ) )
		{
		if ( widthTable != NULL )
			{
			uint32	startLineWidth;
			uint32	endLineWidth;
			
			startLineWidth = widthTable[ block->last_line_break ];
			endLineWidth = widthTable[ block->last_line_break + oldBreakPos ];
			
			oldBreakWidth = endLineWidth - startLineWidth;
			}
		else
			{
			text_data.text_len = oldBreakPos;
			FE_GetTextInfo ( context, &text_data, &text_info );
			oldBreakWidth = text_info.max_width;
			}
		}
	
	text_data.text_len = lineLength;
	
	/* if we're breaking at a space at the end of this line, don't
       measure it */
	if ( skipEndSpace )
		{
		--text_data.text_len;
		}
	
	/* BRAIN DAMAGE: We don't need this - already got all the info */
	if ( widthTable != NULL )
		{
		uint32	startLineWidth;
		uint32	endLineWidth;
		
		/* this is really lame */
		text_data.text = (PA_Block) text;
		text_data.text_len = 1;
		FE_GetTextInfo ( context, &text_data, &text_info );
		
		startLineWidth = widthTable[ block->last_line_break ];
		endLineWidth = widthTable[ block->last_line_break + lineLength ];
		
		text_info.max_width = endLineWidth - startLineWidth;
		}
	else
		{
		text_data.text = (PA_Block) text;
		text_data.text_len = lineLength;
		FE_GetTextInfo ( context, &text_data, &text_info );
		}
	
	/* update our char width average */
	block->totalWidth += text_info.max_width;
	block->totalChars += lineLength;
	
	*width = lo_correct_text_element_width( &text_info );
	
	/* BRAIN DAMAGE: Pass this into the FE call */
	state->text_info = text_info;
	
	/* check to see if we're at the end of the buffer */
	if ( block->buffer_read_index == block->buffer_write_index )
		{
		*allTextFits = TRUE;
		}
	
	/* save the last break position */
	if ( oldBreakPos != -1 )
		{
		state->break_pos = oldBreakPos;
		state->break_width = oldBreakWidth;
		}
	
#ifdef LOG
	PR_LogPrint ( "Final lineLength: %lu, allTextFits: %d, text: %s\n", text_data.text_len, *allTextFits, text );
	PR_LogFlush();
#endif

#ifdef BREAK_GUESS_TRACK
	XP_TRACE(("Num forward, backward break moves after initial guess: %ld, %ld", numForwardMoves, numBackwardMoves ));
#endif			

	return lineLength;
}


static uint8 *
lo_GetNextTextPosition ( LO_TextBlock * block, uint32 * outWordLength, uint32 * outLineLength, Bool * canBreak )
{
	uint32		breakCommand;
	uint32		breakLong;
	uint32		breakIndex;
	uint32		lineLength;
	uint32		nibbleCount;
	uint32		dataNibbles;
	int32		wordLength;
	uint32 *	breakTable;
	uint8 *		endTextRun;
	
	/*
	 * Sanity check for already being at the end of the buffer
	 */
	
	if ( block->buffer_read_index == block->buffer_write_index )
		{
		*outWordLength = 0;
		*canBreak = FALSE;
		return NULL;
		}
	
	/* are we in a run of breakable multibyte characters? */
	if ( block->multibyte_length > 0 )
		{
		uint16	charSize;
		
		/* BRAIN DAMAGE */
		/* turn this next line on when the uint16 multibyte_char_size
           field is added to LO_TextBlock */
#if 0
		charSize = block->multibyte_char_size;
#else
		charSize = 2;
#endif
		block->multibyte_index += charSize;
		
		/* are we at the end of this run? */
		if ( block->multibyte_index == block->multibyte_length )
			{
			block->multibyte_length = 0;
			block->multibyte_index = 0;
			}
		
		/* bump by one character */
		*outWordLength = charSize;
		(*outLineLength) += charSize;
		*canBreak = TRUE;

		block->buffer_read_index += 2;
		endTextRun = &block->text_buffer[ block->buffer_read_index ];
	
		return endTextRun;
		}
		
	/* assume we will be able to break */
	*canBreak = TRUE;
	
	lineLength = block->buffer_read_index;
	breakIndex = block->break_read_index;
	
	/* are we at the end of the break table? */
	if ( breakIndex < block->break_write_index )
		{
		/* nope, so grab the next break position */
		breakTable = &block->break_table[ breakIndex >> 3 ];
		
		/* cache this in the TextBlock */
		breakLong = ( *breakTable++ ) << ( ( breakIndex & 0x7 ) << 2 );
		wordLength = 0;
		
		/* get the next break command */
		breakCommand = breakLong >> 28;
		breakLong <<= 4;
		if ( ( ++breakIndex & 0x7 ) == 0 )
			{
			breakLong = *breakTable++;
			}
		
		if ( breakCommand <= MAX_NATURAL_LENGTH )
			{
			/* a nibble of length data, we already have all the info we need */
			wordLength = breakCommand;
			dataNibbles = 0;
			}
		else
		if ( breakCommand == LINE_FEED )
			{
			/* we should only get this when parsing preformatted text */
			wordLength = BREAK_LINEFEED;
			dataNibbles = 0;
			}
		else
		if ( breakCommand == BYTE_LENGTH )
			{
			dataNibbles = 2;
			}
		else
		if ( breakCommand == WORD_LENGTH )
			{
			dataNibbles = 4;
			}
		else
		if ( breakCommand == MULTI_BYTE )
			{
			dataNibbles = 4;
			}
		else
			{
			/* a 24 bits of data */
			dataNibbles = 6;
			}
		
		if ( dataNibbles > 0 )
			{
			/* read in the actual count and the tail command header */
			for ( nibbleCount = dataNibbles; nibbleCount > 0; --nibbleCount )
				{
				wordLength <<= 4;
				wordLength |= breakLong >> 28;
				
				breakLong <<= 4;
				if ( ( ++breakIndex & 0x7 ) == 0 )
					{
					breakLong = *breakTable++;
					}
				}
			
			/* now skip the tail end of the command */
			++breakIndex;
			}

		/* if multi byte, then extract the real data */
		if ( breakCommand == MULTI_BYTE )
			{
			int32	runLength;
			
			*canBreak = TRUE;

			runLength = wordLength & MULTI_LENGTH_MASK;
							
			block->multibyte_length = runLength;
			block->multibyte_index = 2;
				
			/* make sure we're not already at the end of the run */
			if ( block->multibyte_index == block->multibyte_length )
				{
				block->multibyte_index = 0;
				block->multibyte_length = 0;
				}
				
			/* extract the real word length from the command */
			wordLength = ( wordLength & MULTI_CHAR_SIZE_MASK ) >> MULTI_CHAR_SIZE_SHIFT;
				
			/* MAJOR BRAIN DAMAGE: WE NEED TO STORE THIS IN THE TEXT BLOCK AS IT WILL NOT */
			/* ALWAYS BE TWO BYTE!!!! */
			XP_ASSERT( wordLength == 2 );
			}

		lineLength += wordLength;

		/* if we actually have a word here and are not the first word
		 * on the line, then add one to the length to account for the
		 * interword space. We know we're not the first word on the
		 * line if we're not at the linebreak or if we're at the start
		 * of the buffer but have already skipped a break position
		 * (this happens when the first character of the buffer is a
		 * breaking space).  */
		if ( ( wordLength > 0 ) && ( ( block->last_line_break != block->buffer_read_index ) ||
				( ( block->buffer_read_index == 0 ) && ( block->break_read_index > 0 ) ) ) )
			{
			/* only true if there's a space here */
			if ( XP_IS_SPACE ( block->text_buffer[ block->buffer_read_index ] ) )
				{
				lineLength++;
				(*outLineLength)++;
				}
			}
		}
	else
		{
		/* We're at the end of the break table. We may have some text
		 * after this last break.  Either way, we cannot break here */
		
		*canBreak = FALSE;
		
		if ( lineLength < block->buffer_write_index )
			{
			lineLength = block->buffer_write_index;
			wordLength = lineLength - block->buffer_read_index;
			}
		}
		
	block->break_read_index = breakIndex;
	block->buffer_read_index = lineLength;

	endTextRun = &block->text_buffer[ lineLength ];
	
	*outLineLength += wordLength;
	*outWordLength = wordLength;
	
	return endTextRun;
}


static Bool
lo_ExtractPrevBreakCommand ( LO_TextBlock * block, Bool * multiByte, uint32 * command )
{
	Bool		hasPrev;
	uint32		breakCommand;
	uint32		commandData;
	uint32		breakLong;
	uint32		breakIndex;
	uint32 *	breakTable;
	uint32		nibbleCount;
	uint32		dataNibbles;
	Bool		readPrevCommand;
	
	commandData = 0;
	*multiByte = FALSE;
	
	breakIndex = block->break_read_index;

	hasPrev = block->break_read_index > 0;
	if ( hasPrev )
		{
		readPrevCommand = TRUE;
		
		/* are we at the absolute end of the buffer? */
		if ( block->buffer_read_index == block->buffer_write_index )
			{
			/* Yup, so back up to the last break offset. */
			commandData = block->buffer_read_index - block->last_break_offset;
			
			/* was there really a true ending break command? */
			if ( commandData > 0 )
				{
				readPrevCommand = FALSE;

				/* this length was before the breaking space, add it back in */
				--commandData;
				}
			}
		
		/* extract the previous command from the table if we need to */
		if ( readPrevCommand )
			{
			/* nope, so back up within the break table */
			--breakIndex;
			breakTable = &block->break_table[ breakIndex >> 3 ];
			
			breakLong = *breakTable;
			
			/* shift the command down and extract the data */
			breakCommand = ( breakLong >> ( ( 7 - ( breakIndex & 0x7 ) ) << 2 ) ) & 0xF;	
			if ( breakCommand <= MAX_NATURAL_LENGTH )
				{
				/* a nibble of length data, we already have all the
                   info we need */
				dataNibbles = 0;
				commandData = breakCommand;
				}
			else
			if ( breakCommand == LINE_FEED )
				{
				/* we should only get this when parsing preformatted text */
				dataNibbles = 0;
				commandData = breakCommand;
				}
			else
			if ( breakCommand == BYTE_LENGTH )
				{
				/* a byte of length data */
				dataNibbles = 2;
				}
			else
			if ( breakCommand == WORD_LENGTH )
				{
				/* a short of length data */
				dataNibbles = 4;
				}
			else
			if ( breakCommand == MULTI_BYTE )
				{
				/* 16 bits of data */
				dataNibbles = 4;
				*multiByte = TRUE;
				}
			else
				{
				/* a 24 bits of data */
				dataNibbles = 6;
				}

			if ( dataNibbles > 0 )
				{
				/* skip the command tail */
				if ( ( --breakIndex & 0x7 ) == 7 )
					{
					breakLong = *--breakTable;
					}
				
				/* read in the actual count and the tail command header */
				for ( nibbleCount = 0; nibbleCount < dataNibbles; ++nibbleCount )
					{
					uint32	nibble;
					
					/* grab the next nibble */
					nibble = ( breakLong >> ( ( 7 - ( breakIndex & 0x7 ) ) << 2 ) ) & 0xF;
					commandData |= nibble << ( nibbleCount << 2 );
					
					if ( ( --breakIndex & 0x7 ) == 7 )
						{
						breakLong = *--breakTable;
						}
					}
				}
			}
		}
	
	block->break_read_index = breakIndex;

	*command = commandData;
	
	return hasPrev;
}

static uint8 *
lo_GetPrevTextPosition ( LO_TextBlock * block, uint32 * outWordLength, uint32 * outLineLength, Bool * canBreak )
{
	uint32		wordLength;
	uint32		breakCommand;
	uint32		lineLength;
	uint8 *		endTextRun;
	uint32		totalSkip;
	Bool		havePrevCommand;
	Bool		multiByte;
	
	*canBreak = FALSE;
	totalSkip = 0;
	
	/*
	 * Sanity check for already being at the beginning of the line
	 */
	if ( block->buffer_read_index == block->last_line_break )
		{
		*outWordLength = 0;
		return NULL;
		}
	
	/* are we in a run of breakable multibyte characters? */
	if ( block->multibyte_length > 0 )
		{
		uint16	charSize;
		
		/* BRAIN DAMAGE */
		/* turn this next line on when the uint16 multibyte_char_size
           field is added to LO_TextBlock */
#if 0
		charSize = block->multibyte_char_size;
#else
		charSize = 2;
#endif
		block->multibyte_index -= charSize;
		
		/* are we at the end of this run? */
		if ( block->multibyte_index == 0 )
			{
			block->multibyte_length = 0;
			}
		
		/* bump by one character */
		*outWordLength = charSize;
		(*outLineLength) -= charSize;
		*canBreak = TRUE;

		block->buffer_read_index -= 2;
		endTextRun = &block->text_buffer[ block->buffer_read_index ];
	
		return endTextRun;
		}
	
	/* back up to the previous command */
	havePrevCommand = lo_ExtractPrevBreakCommand ( block, &multiByte, &breakCommand );
	if ( !havePrevCommand )
		{
		*outWordLength = 0;
		return NULL;
		}
	
	/* back ourselves up in the buffer */
	if ( multiByte )
		{
		uint32	charSize;
		
		*canBreak = TRUE;
			
		/* extract the real word length from the command */
		charSize = ( breakCommand & MULTI_CHAR_SIZE_MASK ) >> MULTI_CHAR_SIZE_SHIFT;
						
		block->multibyte_length = breakCommand & MULTI_LENGTH_MASK;
		block->multibyte_index = block->multibyte_length - charSize;
			
		/* make sure we're not already at the beginning of the run */
		if ( block->multibyte_index == 0 )
			{
			block->multibyte_length = 0;
			}
			
		/* MAJOR BRAIN DAMAGE: WE NEED TO STORE THIS IN THE TEXT BLOCK
           AS IT WILL NOT */
		/* ALWAYS BE TWO BYTE!!!! */
		XP_ASSERT( charSize == 2 );
		
		/* the length of this run is the char size */
		wordLength = charSize;
		}
	else
		{
		wordLength = breakCommand;
		}
	
	lineLength = block->buffer_read_index;
	
	/* if we actually have a word here and are not the first word on
	 * the line, then subtract one to the length to account for the
	 * interword space.  */
	if ( ( wordLength > 0 ) && ( block->last_line_break != ( lineLength - wordLength )) )
		{
		/* only true if there's a space here */
		if ( XP_IS_SPACE ( block->text_buffer[ lineLength - wordLength - 1 ] ) )
			{
			lineLength--;
			(*outLineLength)--;
			}
		}

	lineLength -= wordLength;
	
	block->buffer_read_index = lineLength;

	endTextRun = &block->text_buffer[ lineLength ];
	
	*outLineLength -= wordLength;
	*outWordLength = wordLength;

	/* We can't break if we've backed up all the way to the start of
       the buffer and there is */
	/* no break position there */
	if ( ( block->break_read_index == 0 ) && ( wordLength == 0 ) )
		{
		*canBreak = FALSE;
		}
	else
		{
		*canBreak = TRUE;
		}
				
	return endTextRun;
}

static Bool
lo_SetBreakCommand ( LO_TextBlock * block, uint32 command, uint32 commandLength )
{
	uint32		break_write_index;
	uint32 *	break_table;
	
	/* record the current break position as it may be the last entry
       in the break table */
	block->last_break_offset = block->buffer_write_index;
	block->last_buffer_write_index = block->buffer_write_index;
	
	break_write_index = block->break_write_index;
	
	/* grow the break table if we need to - always have space for one
       long of data */
	if ( ( break_write_index + 8 ) > block->break_length )
		{
		/* allocate in bytes, count in nibbles */
		block->break_length += BREAK_TABLE_INC * 2;
		block->break_table = XP_REALLOC ( block->break_table, block->break_length / 2 );
		}
	
	break_table = block->break_table;
	
	if ( break_table != NULL )
		{
		uint32	nibble_index;
		
		nibble_index = break_write_index & 0x7;		
		
		/* write the command */
		if ( nibble_index == 0 )
			{
			/* we're long aligned, write the sucker */
			break_table[ break_write_index >> 3 ] = command;
			}
		else
			{
			uint32	table_long;
			uint32	table_index;
			
			table_index = break_write_index >> 3;
			table_long = break_table[ table_index ];
			
			table_long |= command >> ( nibble_index << 2 );
			break_table[ table_index ] = table_long;
			
			/* how many nibbles did we write, and were they enough? */
			nibble_index = 0x8 - nibble_index;
			if ( commandLength > nibble_index )
				{
				/* need to write out some more data */
				break_table[ table_index + 1 ] = command << ( nibble_index << 2 );
				}
			}
		
		break_write_index += commandLength;
		}
	
	block->break_write_index = break_write_index;
	
	return break_table != NULL;
}

static Bool
lo_SetBreakPosition ( LO_TextBlock * block )
{
	uint32		w_length;
	uint32		data_long;
	uint32		command_size;
	Bool		success;
	
	/* how many characters were added for this run */
	w_length = block->buffer_write_index - block->last_buffer_write_index;
	XP_ASSERT ( w_length >= 0 );
	
	if ( w_length <= MAX_NATURAL_LENGTH )
		{
		/* one data nibble */
		command_size = 1;
		data_long = w_length << 28;
		}
	else
	if ( w_length <= 255 )
		{
		/* a command nibble, two data nibbles and then a terminator nibble */
		command_size = 4;
		data_long = ( BYTE_LENGTH << 28 ) | ( w_length << 20 ) | ( BYTE_LENGTH << 16 );
		}
	else
	if ( w_length <= 65535 )
		{
		/* a command nibble, four data nibbles and then a terminator nibble */
		command_size = 6;
		data_long = ( WORD_LENGTH << 28 ) | ( w_length << 12 ) | ( WORD_LENGTH << 8 );
		}
	else
		{
		/* we can have 24 bits of data at most... */
		XP_ASSERT ( w_length <= ( ( 1 << 24 ) - 1 ) );
		
		/* a command nibble, six data nibbles and then a terminator nibble */
		command_size = 8;
		data_long = ( LONG_LENGTH << 28 ) | ( w_length << 4 ) | ( LONG_LENGTH );
		}
	
	success = lo_SetBreakCommand ( block, data_long, command_size );

	return success;
}

static Bool
lo_SetMultiByteRun ( LO_TextBlock * block, int32 charSize, Bool breakable, Bool eachCharBreakable )
{
	uint32		w_length;
	uint32		data_long;
	uint32		command_size;
	Bool		success;
	
	/* how many characters were added for this run */
	w_length = block->buffer_write_index - block->last_buffer_write_index;
	XP_ASSERT ( w_length >= 0 );
	
	/* * There are two cases where we can use a normal simple break
	 *	entry: 1. We are not breakable on every char and the text is
	 *	breakable (the normal break table case).  2. We can break on
	 *	every char, but we only have one char in the run */
	if ( breakable )
		{
		if ( !eachCharBreakable || ( charSize == w_length ) )
			{
			return lo_SetBreakPosition ( block );
			}
		}
		
	/* BRAIN DAMAGE: We need to handle overflow of our data word */
	
	/* this break command always has 16 bits of data and so it is 24
       bits big */
	command_size = 6;
	
	data_long = w_length;
	
	data_long |= ( charSize << MULTI_CHAR_SIZE_SHIFT ) & MULTI_CHAR_SIZE_MASK;

	XP_ASSERT( w_length <= MULTI_LENGTH_MASK );
	data_long |= ( w_length & MULTI_LENGTH_MASK );
	
	/* set the command nibbles */
	data_long = ( (uint32) MULTI_BYTE << ( MULTI_BYTE_DATA_SIZE + 4 ) ) | (uint32) ( data_long << 4 ) | (uint32) MULTI_BYTE;
	data_long <<= 8;
	
	success = lo_SetBreakCommand ( block, data_long, command_size );

	return success;
}

void lo_BreakOldTextBlockElement(MWContext *context, lo_DocState *state)
{
	LO_TextBlock *	block;
	LO_TextStruct *	text_data;
	LO_TextStruct *	new_text_data;
	char *			text;
	char *			breakPtr;
	int32			save_width;
	uint32			newTextlength;
	LO_TextInfo		text_info;
	int32			base_change;
	int32			old_baseline;
	int32			old_line_height;
	int32			adjust;
	int32			baseline_inc;
	LO_Element *	tptr;
	LO_Element *	eptr;
	LO_Element *	line_ptr;
	
	/* note that the block can be null for a word break element */
	block = state->old_break_block;
		
	/* Move to the element we will break */
	text_data = state->old_break;

	/* If there is no text there to break it is an error. */
	if ( text_data == NULL )
		{
		return;
		}
	
	new_text_data = NULL;
	
	/*
	 * Later operations will trash the width field.
	 * So save it now to restore later.
	 */
	save_width = state->width;
	
	/*
	 * If this element has no text, then it's a special word break
	 * element. We can simply remove it and then add a linefeed and then
	 * insert the remaining text on the line buffer
	 */
	if ( text_data->text == NULL )
		{
		/*
		 * Back up the state to this element's location
		 */
		state->x = text_data->x;
		state->y = text_data->y;

		tptr = text_data->next;
		text_data->next = NULL;

		state->width = text_data->width;
		state->x += state->width;
				
		/* add a line feed */
		lo_SoftLineBreak(context, state, TRUE);
		}
	else
		{
		/* We're trying to break inside an actual text element. We
		 * need to shorten this element to point up to this break
		 * position, then add a linefeed, then create a new element
		 * that contains the remaining text and place it on the line
		 * buffer */
		 
		 /* if we're breaking an element, we must have a text block */
		if ( block == NULL )
			{
			return;
			}

		PA_LOCK(text, char *, text_data->text);
		
		/*
		 * Back the state up to this element's
		 * location, break off the rest of the elements
		 * and save them for later.
		 * Flush this line, and insert a linebreak.
		 */
		state->x = text_data->x;
		state->y = text_data->y;
		tptr = text_data->next;
		text_data->next = NULL;
		
		breakPtr = &text[ state->old_break_pos ];
		newTextlength = text_data->text_len - state->old_break_pos;

		text_data->block_offset = text_data->block_offset - text_data->text_len + state->old_break_pos;
		text_data->text_len = state->old_break_pos;

		FE_GetTextInfo(context, text_data, &text_info);
		state->width = lo_correct_text_element_width( &text_info );
		
		text_data->width = state->width;
		PA_UNLOCK(text_data->text);
		state->x += state->width;
		
		/* this element should point at the space before the next
           word, so skip it */
		if ( XP_IS_SPACE ( *breakPtr ) )
			{
			++breakPtr;
			--newTextlength;
			}

		/*
		 * If the element that caused this break has a different
		 * baseline than the element we are breaking, we need to
		 * preserve that difference after the break.
		 */
		base_change = state->baseline - (text_data->y_offset + text_info.ascent);

		old_baseline = state->baseline;
		old_line_height = state->line_height;

		/*
		 * Reset element_id so they are still sequencial.
		 */
		state->top_state->element_id = text_data->ele_id + 1;

		/*
		 * If we are breaking an anchor, we need to make sure the
		 * linefeed gets its anchor href set properly.
		 */
		if (text_data->anchor_href != NULL)
			{
			LO_AnchorData *tmp_anchor;

			tmp_anchor = state->current_anchor;
			state->current_anchor = text_data->anchor_href;
			lo_SoftLineBreak(context, state, TRUE);
			state->current_anchor = tmp_anchor;
			}
		else
			{
			lo_SoftLineBreak(context, state, TRUE);
			}

		adjust = lo_baseline_adjust( context, state, tptr, old_baseline, old_line_height );
		state->baseline = old_baseline - adjust;
		state->line_height = (intn) old_line_height - adjust;
		
		/* now create a new text element */
		baseline_inc = -1 * adjust;
		
		new_text_data = (LO_TextStruct *)lo_NewElement ( context, state, LO_TEXT, text_data->edit_element, text_data->edit_offset+text_data->text_len+1 );
		if (text_data == NULL)
			{
#ifdef DEBUG
			assert (state->top_state->out_of_memory);
#endif
			return;
			}

		new_text_data->type = LO_TEXT;
		new_text_data->ele_id = NEXT_ELEMENT;
		new_text_data->x = state->x;
		new_text_data->x_offset = 0;
		new_text_data->y = state->y;
		new_text_data->y_offset = 0;
		new_text_data->height = 0;
		new_text_data->next = NULL;
		new_text_data->prev = NULL;

		new_text_data->anchor_href = block->anchor_href;
		new_text_data->text_attr = block->text_attr;
		new_text_data->ele_attrmask = block->ele_attrmask;
		
		/* BRAIN DAMAGE: Set LO_ELE_INVISIBLE to mark the element as
           not having a valid text ptr */
		XP_ASSERT ( !(new_text_data->ele_attrmask & LO_ELE_INVISIBLE ) );
		new_text_data->ele_attrmask |= LO_ELE_INVISIBLE;
		
		new_text_data->sel_start = -1;
		new_text_data->sel_end = -1;

		new_text_data->doc_width = state->right_margin - state->x;
		new_text_data->doc_width = 0;
		new_text_data->block_offset = text_data->block_offset + text_data->text_len;
		XP_ASSERT(new_text_data->block_offset <= 65535);
			
		new_text_data->text = (PA_Block) breakPtr;
		new_text_data->text_len = newTextlength;
		FE_GetTextInfo(context, new_text_data, &text_info);
		new_text_data->width = lo_correct_text_element_width(&text_info);

		/*
		 * Some fonts (particulatly italic ones with curly
		 * tails on letters like 'f') have a left bearing
		 * that extends back into the previous character.
		 * Since in this case the previous character is
		 * probably not in the same font, we move forward
		 * to avoid overlap.
		 */
		if (text_info.lbearing < 0)
			{
			new_text_data->x_offset = text_info.lbearing * -1;
			}

		/*
		 * The baseline of the text element just inserted in
		 * the line may be less than or greater than the
		 * baseline of the rest of the line due to font
		 * changes.  If the baseline is less, this is easy,
		 * we just increase y_offest to move the text down
		 * so the baselines line up.  For greater baselines,
		 * we can't move the text up to line up the baselines
		 * because we will overlay the previous line, so we
		 * have to move all rest of the elements in this line
		 * down.
		 *
		 * If the baseline is zero, we are the first element
		 * on the line, and we get to set the baseline.
		 */
		if (state->baseline == 0)
			{
			state->baseline = text_info.ascent;
			}
		else
		if (text_info.ascent < state->baseline)
			{
			new_text_data->y_offset = state->baseline - text_info.ascent;
			}
		else
			{
			baseline_inc = baseline_inc + (text_info.ascent - state->baseline);
			state->baseline = text_info.ascent;
			}

		/*
		 * Now that we have broken, and added the new
		 * element, we need to move it down to restore the
		 * baseline difference that previously existed.
		 */
		new_text_data->y_offset -= base_change;

		/*
		 * Calculate the height of this new
		 * text element.
		 */
		new_text_data->height = text_info.ascent + text_info.descent;
		state->x += new_text_data->width;
		}
	
	/* if our previous element was the last text element for this
	 * block, then our new element is the new end */
	if ( block->endTextElement == text_data )
		{
		block->endTextElement = new_text_data;
		}
		
	/*
	 * Now add the remaining elements to the line list
	 */
	 
	/* first find the end of the line list */
	line_ptr = state->line_list;
	while ((line_ptr != NULL)&&(line_ptr->lo_any.next != NULL))
		{
		line_ptr = line_ptr->lo_any.next;
		}
	
	/* now add the new_text_data if there is one */
	if ( new_text_data != NULL )
		{
		if (line_ptr == NULL)
			{
			state->line_list = (LO_Element *) new_text_data;
			new_text_data->prev = NULL;
			line_ptr = (LO_Element *) new_text_data;
			}
		else
			{
			line_ptr->lo_any.next = (LO_Element *) new_text_data;
			new_text_data->prev = line_ptr;
			line_ptr = (LO_Element *) new_text_data;
			}
		}
	
	/* and then add tptr */
	if ( tptr != NULL )
		{
		if (line_ptr == NULL)
			{
			state->line_list = tptr;
			tptr->lo_any.prev = NULL;
			line_ptr = tptr;
			}
		else
			{
			line_ptr->lo_any.next = tptr;
			tptr->lo_any.prev = line_ptr;
			line_ptr = tptr;
			}
		}
	
	/* if we've added a new element then increment the element id's of
       the following elements */
	if ( new_text_data != NULL )
		{
		eptr = tptr;

		while (eptr != NULL)
			{
			eptr->lo_any.ele_id = NEXT_ELEMENT;
			eptr->lo_any.y_offset += baseline_inc;
			eptr = eptr->lo_any.next;
			}
		
		/* and bump the line height if we need to */
		if ( ( new_text_data->y_offset + new_text_data->height ) > state->line_height )
			{
			state->line_height = (intn) new_text_data->y_offset + new_text_data->height;
			}
		}

	/*
	 * Upgrade forward the x and y text positions in the document
	 * state.
	 */
	while ( tptr != NULL )
		{
		lo_UpdateElementPosition ( state, tptr );
		tptr = tptr->lo_any.next;
		}

	state->at_begin_line = FALSE;
	state->width = save_width;
}

static uint8 *
lo_GetLineStart ( LO_TextBlock * block )
{
	return &block->text_buffer[ block->last_line_break ];
}


static void
lo_SkipCharacter ( LO_TextBlock * block )
{
	++block->buffer_read_index;
}


static void
lo_SetLineBreak ( LO_TextBlock * block, Bool skipSpace )
{
	/* if the current character is a space, we can skip it as we're
       breaking here */
	/* NOT FOR PREFORMATTED TEXT THOUGH */
	if ( skipSpace && ( XP_IS_SPACE ( block->text_buffer[ block->buffer_read_index ] ) ) )
		{
		++block->buffer_read_index;
		}

	block->last_line_break = block->buffer_read_index;
}

static uint8 *
lo_RestoreBreakState ( LO_TextBlock * block, BreakState * state, uint32 * lineLength )
{
	uint8 *	runEnd;
	
	block->buffer_read_index = state->buffer_read_index;
	block->break_read_index = state->break_read_index;
	block->multibyte_index = state->multibyte_index;
	block->multibyte_length = state->multibyte_length;
	block->last_line_break = state->last_line_break;
	
	if ( lineLength != NULL )
		{
		*lineLength = state->lineLength;
		}
	
	runEnd = &block->text_buffer[ block->buffer_read_index ];
	
	return runEnd;
}


static Bool
lo_SkipInitialSpace ( LO_TextBlock * block )
{
	Bool	canBreak;
	uint32	breakLong;
	uint32	breakIndex;
	
	canBreak = FALSE;
	breakIndex = block->break_read_index;
	
	/* do we have a break point at the current text offset */
	if ( ( breakIndex == 0 ) && ( breakIndex < block->break_write_index ) && XP_IS_SPACE ( block->text_buffer[ 0 ] ) )
		{
		breakLong = block->break_table[ breakIndex >> 3 ];
		breakLong >>= ( 7 - ( breakIndex & 0x7 ) ) << 2;
		
		if ( breakLong == 0 )
			{
			block->buffer_read_index++;
			block->break_read_index++;
			block->last_line_break++;
			canBreak = TRUE;
			}
		}
	
	return canBreak;
}


Bool lo_GrowTextBlock ( LO_TextBlock * block, uint32 length )
{
	Bool			success = TRUE;
	uint32			growBy;
	uint32			oldTextBase;
	uint32			offset;
	LO_TextStruct *	textElement;
	LO_TextStruct *	endElement;
	
	if ( block->buffer_length < ( block->buffer_write_index + length ) )
		{
		/* need to make sure that the new size is enough to contain
           the new data */
		growBy = TEXT_BUFFER_INC;
		if ( growBy < length )
			{
			growBy = length;
			}
		
		oldTextBase = (uint32) block->text_buffer;
		
		growBy += block->buffer_length;
		block->text_buffer = XP_REALLOC ( block->text_buffer, growBy );
		
		block->buffer_length = growBy;
		success = block->text_buffer != NULL;
		
		/* update any break table related information */
		if ( success && ( block->break_table != NULL ) )
			{
			/* Run through all the text elements and adjust their text
			 * addresses to point to the newly relocated block */
			
			textElement = block->startTextElement;
			endElement = block->endTextElement;
			
			while ( textElement != NULL )
				{
				if ( textElement->type == LO_TEXT )
					{
					offset = (uint32) textElement->text - oldTextBase;
					textElement->text = (PA_Block) ( (uint32) block->text_buffer + offset );
					}
				
				if ( textElement == endElement )
					{
					break;
					}
				
				textElement = (LO_TextStruct *) textElement->next;
				}
			}
		}
	
	return success;
}

static void
lo_CopyText ( uint8 * src, uint8 * dst, uint32 length )
{
	uint8	c;
	
	/* copy a text string, converting non breaking spaces to normal
       spaces as we go */
	while ( length-- )
		{
		c = *src++;
		
		if ( c == NON_BREAKING_SPACE )
			{
			c = ' ';
			}
		
		*dst++ = c;
		}
}

static void
lo_CopyTextToLineBuffer ( lo_DocState * state, uint8 * src, uint32 length )
{
	char *	text_buf;

	/* do we need to grow the buffer? */
	if ( ( state->line_buf_len + length + 1 ) > state->line_buf_size )
		{
		state->line_buf = PA_REALLOC ( state->line_buf, ( state->line_buf_size + length + LINE_BUF_INC ) );
		if ( state->line_buf == NULL )
			{
			state->top_state->out_of_memory = TRUE;
			return;
			}
		}
	
	PA_LOCK(text_buf, char *, state->line_buf);
	
	XP_BCOPY ( (char *) src, (char *) ( text_buf + state->line_buf_len ), ( length + 1 ) );
	state->line_buf_len += length;
	
	text_buf[ state->line_buf_len ] = 0;
	
	PA_UNLOCK(state->line_buf);
}

#ifdef TEST_16BIT
#undef XP_WIN16
#endif /* TEST_16BIT */

#ifdef PROFILE
#pragma profile off
#endif
