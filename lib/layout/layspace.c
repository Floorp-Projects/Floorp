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
#include "layout.h"


#define SPACE_WORD	1
#define SPACE_LINE	2
#define SPACE_BLOCK	3

static void lo_AppendSpacerElementToLineList( MWContext *context, lo_DocState *state, 
											 LO_SpacerStruct *spacer );
static void lo_ParseAttributesForBlockSpacer( MWContext *context, lo_DocState *state, 
											 LO_SpacerStruct *spacer );
static void lo_LayoutBlockSpacer(MWContext *context, lo_DocState *state, 
								 LO_SpacerStruct *spacer, Bool relayout);

void
lo_ProcessSpacerTag(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	int32 type;
	int32 size;
	PA_Block buff;
	LO_SpacerStruct *spacer;
	
	spacer = (LO_SpacerStruct*)lo_NewElement(context, state, LO_SPACER, NULL, 0);
	XP_ASSERT(spacer);
	if (!spacer) 
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}

	type = SPACE_WORD;
	buff = lo_FetchParamValue(context, tag, PARAM_TYPE);
	if (buff != NULL)
	{
		char *type_str;

		PA_LOCK(type_str, char *, buff);
		if (pa_TagEqual("line", type_str))
		{
			type = SPACE_LINE;
		}
		else if (pa_TagEqual("vert", type_str))
		{
			type = SPACE_LINE;
		}
		else if (pa_TagEqual("vertical", type_str))
		{
			type = SPACE_LINE;
		}
		else if (pa_TagEqual("block", type_str))
		{
			type = SPACE_BLOCK;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	size = 0;
	buff = lo_FetchParamValue(context, tag, PARAM_SIZE);
	if (buff != NULL)
	{
		char *str;

		PA_LOCK(str, char *, buff);
		size = XP_ATOI(str);
		PA_UNLOCK(buff);
		PA_FREE(buff);
		if (size < 1)
		{
			size = 0;
		}
	}	

	spacer->size = size;
	spacer->type = (int8) type;
	spacer->tag = tag;

	lo_AppendSpacerElementToLineList( context, state, spacer );
	lo_LayoutSpacerElement(context, state, spacer, FALSE);

	/*
	 * Spacers of size 0 do nothing.
	 * Unless they are block spacers that use WIDTH and HEIGHT.
	 */
	/*
	if ((size == 0)&&(type != SPACE_BLOCK))
	{
		return;
	}

	if (type == SPACE_WORD)
	{
		lo_InsertWordBreak(context, state);
		size = FEUNITS_X(size, context);
		state->x += size;
	}
	else if (type == SPACE_LINE)
	{
		lo_SetSoftLineBreakState(context, state, FALSE, 1);
		size = FEUNITS_Y(size, context);
		state->y += size;
	}
	else if (type == SPACE_BLOCK)
	{
		lo_format_block_spacer(context, state, tag);
	}
	*/
}

static void lo_AppendSpacerElementToLineList( MWContext *context, lo_DocState *state, LO_SpacerStruct *spacer )
{
	spacer->lo_any.type = LO_SPACER;
	spacer->lo_any.x = state->x;
	spacer->lo_any.y = state->y;
	spacer->lo_any.x_offset = 0;
	spacer->lo_any.y_offset = 0;
	spacer->lo_any.width = 0;
	spacer->lo_any.height = 0;
	spacer->lo_any.line_height = 0;
	spacer->lo_any.ele_id = NEXT_ELEMENT;
	
	lo_AppendToLineList(context, state, (LO_Element*)spacer, 0);
}


void lo_LayoutSpacerElement(MWContext *context, lo_DocState *state, LO_SpacerStruct *spacer, Bool relayout)
{
	int32 size = spacer->size;
	int8 type = spacer->type;

	/*
	 * Spacers of size 0 do nothing.
	 * Unless they are block spacers that use WIDTH and HEIGHT.
	 */
	if ((size == 0)&&(type != SPACE_BLOCK))
	{
		return;
	}

	if (type == SPACE_WORD)
	{
		if (!relayout)
			lo_InsertWordBreak(context, state);

		size = FEUNITS_X(size, context);
		state->x += size;
	}
	else if (type == SPACE_LINE)
	{
		/* lo_SetSoftLineBreakState(context, state, FALSE, 1); */
		lo_SetLineBreakState(context, state, FALSE, LO_LINEFEED_BREAK_SOFT, 1, relayout);
		size = FEUNITS_Y(size, context);
		state->y += size;
	}
	else if (type == SPACE_BLOCK)
	{
		if (!relayout)
		{
			lo_ParseAttributesForBlockSpacer(context, state, spacer);
		}
		
		lo_LayoutBlockSpacer(context, state, spacer, relayout);
	}

	if (!relayout)
	{
		/* We do not want to keep a pointer to the tag around for relayout
		   because we have all the information we need in the LO_SPACER
		   layout element */
		spacer->tag = NULL;
	}
}


static void lo_ParseAttributesForBlockSpacer( MWContext *context, lo_DocState *state, LO_SpacerStruct *spacer )
{
	Bool floating;
	int32 alignment;
	int32 val;
	PA_Block buff;
	PA_Tag *tag = spacer->tag;
	char *str;

	alignment = LO_ALIGN_BASELINE;
	floating = FALSE;

	/*
	 * Check for an align parameter
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_ALIGN);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		alignment = lo_EvalAlignParam(str, &floating);
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	spacer->alignment = alignment;
	spacer->floating = floating;	

	/*
	 * Get the width parameter, in absolute or percentage.
	 * If percentage, make it absolute.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_WIDTH);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			spacer->is_percent_width = TRUE;
			spacer->width = val;			
		}
		else
		{
			spacer->is_percent_width = FALSE;
			val = FEUNITS_X(val, context);
			if (val < 1)
			{
				val = 1;
			}
			spacer->width = val;
		}		
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	/*
	 * Get the height parameter, in absolute or percentage.
	 * If percentage, make it absolute.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_HEIGHT);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			spacer->is_percent_height = TRUE;
			spacer->height = val;
		}
		else
		{
			spacer->is_percent_height = FALSE;
			val = FEUNITS_Y(val, context);
			if (val < 1)
			{
				val = 1;
			}
			spacer->height = val;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
}

static void lo_LayoutBlockSpacer(MWContext *context, lo_DocState *state, LO_SpacerStruct *spacer, Bool relayout)
{
	int32 width = 0;
	int32 height = 0;
	int32 doc_width;
	int32 alignment = spacer->alignment;
	Bool floating = spacer->floating;
	Bool line_break;
	char *str;
	int32 x, y;
	int16 x_offset;
	int32 y_offset;
	int32 baseline_inc, line_inc;	
	LO_TextStruct tmp_text;
	LO_TextInfo text_info;
	LO_Element *eptr;
	PA_Block buff;

	x = state->x;
	y = state->y;	
	doc_width = state->right_margin - state->left_margin;
	
	if (spacer->is_percent_width)
	{
		int32 val;
		
		if (state->allow_percent_width == FALSE)
		{
			val = 0;
		}
		else
		{
			val = spacer->width;
			val = doc_width * val / 100;
			if (val < 1)
			{
				val = 1;
			}
		}

		width = val;
	}
	else
	{
		width = spacer->width;
	}

	if (spacer->is_percent_height)
	{
		int32 val;

		if (state->allow_percent_height == FALSE)
		{
			val = 0;
		}
		else
		{
			val = state->win_height * val / 100;
			if (val < 1)
			{
				val = 1;
			}
		}
		height = val;
	}
	else
	{
		height = spacer->height;
	}


	/*
     * All this work is to get the text_info filled in for the current
     * font in the font stack. Yuck, there must be a better way.
     */
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
	tmp_text.text_attr = state->font_stack->text_attr;
	FE_GetTextInfo(context, &tmp_text, &text_info);
	PA_FREE(buff);

	/*
	 * If both dimentions are bogus, no spacer.
	 * If only one dimension bogus, then make it 1.
	 */
	if ((width < 1)&&(height < 1))
	{
		return;
	}
	else if (width < 1)
	{
		width = 1;
	}
	else if (height < 1)
	{
		height = 1;
	}


	/*
	 * SEVERE FLOW BREAK!  This may be a floating image,
	 * which means at this point we go do something completely
	 * different.
	 */
	if (floating != FALSE)
	{
		if (alignment == LO_ALIGN_RIGHT)
		{
			if (state->right_margin_stack == NULL)
			{
				x = state->right_margin - width;
			}
			else
			{
				x = state->right_margin_stack->margin - width;
			}
			if (x < 0)
			{
				x = 0;
			}
		}
		else
		{
			x = state->left_margin;
		}

		y = -1;

		lo_AddMarginStack(state, x, y,
			width, height, 0, 0, 0, alignment);

		if (state->at_begin_line != FALSE)
		{
			lo_FindLineMargins(context, state, !relayout);
			state->x = state->left_margin;
		}

		return;
	}

	/*
	 * Will this spacer make the line too wide.
	 */
	if ((state->x + width) > state->right_margin)
	{
		line_break = TRUE;
	}
	else
	{
		line_break = FALSE;
	}

	/*
	 * if we are at the beginning of the line.  There is
	 * no point in breaking, we are just too wide.
	 * Also don't break in unmapped preformatted text.
	 * Also can't break inside a NOBR section.
	 */
	if ((state->at_begin_line != FALSE)||
		(state->preformatted == PRE_TEXT_YES)||
		(state->breakable == FALSE))
	{
		line_break = FALSE;
	}

	/*
	 * break on the spacer if we have
	 * a break.
	 */
	if (line_break != FALSE)
	{
		if (!relayout)
			lo_SoftLineBreak(context, state, TRUE);
		else
			lo_rl_AddSoftBreakAndFlushLine(context, state);

		x = state->x;
		y = state->y;
	}

	/*
	 * Figure out how to align this spacer.
	 * baseline_inc is how much to increase the baseline
	 * of previous element of this line.  line_inc is how
	 * much to increase the line height below the baseline.
	 */
	baseline_inc = 0;
	line_inc = 0;
	x_offset = 0;
	y_offset = 0;

	lo_CalcAlignOffsets(state, &text_info, alignment,
		width, height,
		&x_offset, &y_offset, &line_inc, &baseline_inc);

	width += x_offset;
	height += y_offset;

	/*
	 * Change the rest of the line the way lo_AppendToLineList()
	 * would if we were adding a real element.
	 */
	eptr = state->line_list;
        if (eptr != NULL)
        {
                while (eptr->lo_any.next != NULL)
                {
                        eptr->lo_any.y_offset += baseline_inc;
                        eptr = eptr->lo_any.next;
                }
                eptr->lo_any.y_offset += baseline_inc;
        }

	state->baseline += (intn) baseline_inc;
	state->line_height += (intn) (baseline_inc + line_inc);

	/*
	 * Clean up state
	 */
	state->x = state->x + width;
	state->linefeed_state = 0;
	state->at_begin_line = FALSE;
	state->trailing_space = FALSE;
	state->cur_ele_type = LO_NONE;
}

#if 0
void
lo_format_block_spacer(MWContext *context, lo_DocState *state, PA_Tag *tag, Bool relayout)
{
	Bool line_break;
	Bool floating;
	int32 baseline_inc, line_inc;
	int32 alignment;
	int16 x_offset;
	int32 y_offset;
	int32 x, y;
	int32 width, height;
	int32 doc_width;
	int32 val;
	PA_Block buff;
	char *str;
	LO_TextStruct tmp_text;
	LO_TextInfo text_info;
	LO_Element *eptr;

	x = state->x;
	y = state->y;
	width = 0;
	height = 0;
	alignment = LO_ALIGN_BASELINE;
	floating = FALSE;

	/*
	 * Check for an align parameter
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_ALIGN);
	if (buff != NULL)
	{

		PA_LOCK(str, char *, buff);
		alignment = lo_EvalAlignParam(str, &floating);
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	doc_width = state->right_margin - state->left_margin;

	/*
	 * Get the width parameter, in absolute or percentage.
	 * If percentage, make it absolute.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_WIDTH);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			if (state->allow_percent_width == FALSE)
			{
				val = 0;
			}
			else
			{
				val = doc_width * val / 100;
				if (val < 1)
				{
					val = 1;
				}
			}
		}
		else
		{
			val = FEUNITS_X(val, context);
			if (val < 1)
			{
				val = 1;
			}
		}
		width = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	/*
	 * Get the height parameter, in absolute or percentage.
	 * If percentage, make it absolute.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_HEIGHT);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			if (state->allow_percent_height == FALSE)
			{
				val = 0;
			}
			else
			{
				val = state->win_height * val / 100;
				if (val < 1)
				{
					val = 1;
				}
			}
		}
		else
		{
			val = FEUNITS_Y(val, context);
			if (val < 1)
			{
				val = 1;
			}
		}
		height = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	/*
         * All this work is to get the text_info filled in for the current
         * font in the font stack. Yuck, there must be a better way.
         */
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
	tmp_text.text_attr = state->font_stack->text_attr;
	FE_GetTextInfo(context, &tmp_text, &text_info);
	PA_FREE(buff);

	/*
	 * If both dimentions are bogus, no spacer.
	 * If only one dimension bogus, then make it 1.
	 */
	if ((width < 1)&&(height < 1))
	{
		return;
	}
	else if (width < 1)
	{
		width = 1;
	}
	else if (height < 1)
	{
		height = 1;
	}


	/*
	 * SEVERE FLOW BREAK!  This may be a floating image,
	 * which means at this point we go do something completely
	 * different.
	 */
	if (floating != FALSE)
	{
		if (alignment == LO_ALIGN_RIGHT)
		{
			if (state->right_margin_stack == NULL)
			{
				x = state->right_margin - width;
			}
			else
			{
				x = state->right_margin_stack->margin - width;
			}
			if (x < 0)
			{
				x = 0;
			}
		}
		else
		{
			x = state->left_margin;
		}

		y = -1;

		lo_AddMarginStack(state, x, y,
			width, height, 0, 0, 0, alignment);

		if (state->at_begin_line != FALSE)
		{
			lo_FindLineMargins(context, state, TRUE);
			state->x = state->left_margin;
		}

		return;
	}

	/*
	 * Will this spacer make the line too wide.
	 */
	if ((state->x + width) > state->right_margin)
	{
		line_break = TRUE;
	}
	else
	{
		line_break = FALSE;
	}

	/*
	 * if we are at the beginning of the line.  There is
	 * no point in breaking, we are just too wide.
	 * Also don't break in unmapped preformatted text.
	 * Also can't break inside a NOBR section.
	 */
	if ((state->at_begin_line != FALSE)||
		(state->preformatted == PRE_TEXT_YES)||
		(state->breakable == FALSE))
	{
		line_break = FALSE;
	}

	/*
	 * break on the spacer if we have
	 * a break.
	 */
	if (line_break != FALSE)
	{
		lo_SoftLineBreak(context, state, TRUE);
		x = state->x;
		y = state->y;
	}

	/*
	 * Figure out how to align this spacer.
	 * baseline_inc is how much to increase the baseline
	 * of previous element of this line.  line_inc is how
	 * much to increase the line height below the baseline.
	 */
	baseline_inc = 0;
	line_inc = 0;
	x_offset = 0;
	y_offset = 0;

	lo_CalcAlignOffsets(state, &text_info, alignment,
		width, height,
		&x_offset, &y_offset, &line_inc, &baseline_inc);

	width += x_offset;
	height += y_offset;

	/*
	 * Change the rest of the line the way lo_AppendToLineList()
	 * would if we were adding a real element.
	 */
	eptr = state->line_list;
        if (eptr != NULL)
        {
                while (eptr->lo_any.next != NULL)
                {
                        eptr->lo_any.y_offset += baseline_inc;
                        eptr = eptr->lo_any.next;
                }
                eptr->lo_any.y_offset += baseline_inc;
        }

	state->baseline += (intn) baseline_inc;
	state->line_height += (intn) (baseline_inc + line_inc);

	/*
	 * Clean up state
	 */
	state->x = state->x + width;
	state->linefeed_state = 0;
	state->at_begin_line = FALSE;
	state->trailing_space = FALSE;
	state->cur_ele_type = LO_NONE;
}
#endif
