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
#include "net.h"
#include "pa_parse.h"
#include "layout.h"
#include "laylayer.h"
#include "np.h"
#include "laystyle.h"
#include "layers.h"

#define BUILTIN_DEF_DIM			50
#define BUILTIN_DEF_BORDER		0
#define BUILTIN_DEF_VERTICAL_SPACE	0
#define BUILTIN_DEF_HORIZONTAL_SPACE	0

void lo_FinishBuiltin(MWContext *, lo_DocState *, LO_BuiltinStruct *);
static void lo_FormatBuiltinInternal(MWContext *, lo_DocState *, PA_Tag *,
								   LO_BuiltinStruct *, Bool, Bool);
void lo_FillInBuiltinGeometry(lo_DocState *state, LO_BuiltinStruct *builtin,
							  Bool relayout);
void lo_UpdateStateAfterBuiltinLayout (lo_DocState *state, LO_BuiltinStruct *builtin,
									   int32 line_inc, int32 baseline_inc);

void
lo_FormatBuiltin (MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	LO_BuiltinStruct *builtin;
	PA_Block buff;
	char *str;
	uint32 src_len;

#ifdef DEBUG_SPENCE
	printf ("lo_FormatBuiltin\n");
#endif

	/* get SRC */
	buff = lo_FetchParamValue(context, tag, PARAM_SRC);
	PA_LOCK(str, char *, buff);

#ifdef DEBUG_SPENCE
	printf ("FormatBuiltin: str = %s\n", str);
#endif

	builtin = (LO_BuiltinStruct *) lo_NewElement (context, state, LO_BUILTIN, NULL, 0);
	if (builtin == NULL) {
		return;
	}

	builtin->type = LO_BUILTIN;
	builtin->ele_id = NEXT_ELEMENT;
	builtin->x = state->x;
	builtin->x_offset = 0;
	builtin->y = state->y;
	builtin->y_offset = 0;
	builtin->width = 0;
	builtin->height = 0;
	builtin->next = NULL;
	builtin->prev = NULL;
	builtin->attribute_cnt = 0;
	builtin->attribute_list = NULL;
	builtin->value_list = NULL;

	builtin->attribute_cnt = PA_FetchAllNameValues (tag,
		&(builtin->attribute_list), &(builtin->value_list), CS_FE_ASCII);

	lo_FormatBuiltinInternal (context, state, tag, builtin, FALSE, FALSE);
}				  

void
lo_FormatBuiltinObject (MWContext *context, lo_DocState* state,
						PA_Tag* tag , LO_BuiltinStruct *builtin, Bool streamStarted,
						uint32 param_count, char **param_names, char **param_values)
{
	uint32 count;
	int32 typeIndex = -1;
	int32 classidIndex = -1;

#ifdef DEBUG_SPENCE
	printf ("lo_FormatBuiltinObject\n");
#endif

	builtin->attribute_cnt = 0;
	builtin->attribute_list = NULL;
	builtin->value_list = NULL;

	builtin->attribute_cnt = PA_FetchAllNameValues (tag,
		&(builtin->attribute_list), &(builtin->value_list), CS_FE_ASCII);

	lo_FormatBuiltinInternal (context, state, tag, builtin, TRUE, streamStarted);
}

void
lo_FillInBuiltinGeometry(lo_DocState *state,
						 LO_BuiltinStruct *builtin,
						 Bool relayout)
{
	int32 doc_width;

	if (relayout == TRUE)
	{
		builtin->ele_id = NEXT_ELEMENT;
	}
	builtin->x = state->x;
	builtin->y = state->y;

	doc_width = state->right_margin - state->left_margin;

	/*
	 * Get the width parameter, in absolute or percentage.
	 * If percentage, make it absolute.
	 */

	if (builtin->percent_width > 0) {
		int32 val = builtin->percent_width;
		if (state->allow_percent_width == FALSE) {
			val = 0;
		}
		else {
			val = doc_width * val / 100;
		}
		builtin->width = val;
	}

	/* Set builtin->height if builtn has a % height specified */
	if (builtin->percent_height > 0) {
		int32 val = builtin->percent_height;
		if (state->allow_percent_height == FALSE) {
			val = 0;
		}
		else {
			val = state->win_height * val / 100;
		}
		builtin->height = val;
	}
}

static void
lo_FormatBuiltinInternal (MWContext *context, lo_DocState *state, PA_Tag *tag,
						  LO_BuiltinStruct *builtin, Bool isObject, Bool streamStarted)
{
	PA_Block buff;
	char *str;
	int32 val;
	int32 doc_width;
	XP_Bool widthSpecified = FALSE;
	XP_Bool heightSpecified = FALSE;

#ifdef DEBUG_SPENCE
	printf ("lo_FormatBuiltinInternal\n");
#endif

    builtin->nextBuiltin = NULL;
#ifdef MOCHA
    builtin->mocha_object = NULL;
#endif

	builtin->FE_Data = NULL;
	builtin->session_data = NULL;
	builtin->line_height = state->line_height;
	builtin->builtin_src = NULL;
	builtin->alignment = LO_ALIGN_BASELINE;
	builtin->border_width = BUILTIN_DEF_BORDER;
	builtin->border_vert_space = BUILTIN_DEF_VERTICAL_SPACE;
	builtin->border_horiz_space = BUILTIN_DEF_HORIZONTAL_SPACE;
	builtin->tag = tag;
	builtin->ele_attrmask = 0;

	if (streamStarted)
		builtin->ele_attrmask |= LO_ELE_STREAM_STARTED;

	/* Convert any js in the values */
	lo_ConvertAllValues (context, builtin->value_list, builtin->attribute_cnt,
						 tag->newline_count);

	/* double-counting builtins? XXX */

#if 0
	/* Assign a unique index and inc the master index */
	builtin->builtin_index = state->top_state->builtin_count++;
#endif

	/*
	 * Check for an align parameter
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_ALIGN);
	if (buff != NULL)
	{
		Bool floating;

		floating = FALSE;
		PA_LOCK(str, char *, buff);
		builtin->alignment = lo_EvalAlignParam(str, &floating);
		if (floating != FALSE)
		{
			builtin->ele_attrmask |= LO_ELE_FLOATING;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	/*
	 * Get the optional src parameter.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_SRC);
	if (buff != NULL)
	{
		PA_Block new_buff;
		char *new_str;
		int32 len;

		len = 1;
		PA_LOCK(str, char *, buff);
		if (str != NULL)
		{

			len = lo_StripTextWhitespace(str, XP_STRLEN(str));
		}
		if (str != NULL)
		{
			/* bing: make sure this deals with "data:" URLs */
			new_str = NET_MakeAbsoluteURL(
				state->top_state->base_url, str);
		}
		else
		{
			new_str = NULL;
		}

		if ((new_str == NULL)||(len == 0))
		{
			new_buff = NULL;
		}
		else
		{
			char *object_src;

			new_buff = PA_ALLOC(XP_STRLEN(new_str) + 1);
			if (new_buff != NULL)
			{
				PA_LOCK(object_src, char *, new_buff);
				XP_STRCPY(object_src, new_str);
				PA_UNLOCK(new_buff);
			}
			else
			{
				state->top_state->out_of_memory = TRUE;
			}
			XP_FREE(new_str);
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		buff = new_buff;
	}
	builtin->builtin_src = buff;

	doc_width = state->right_margin - state->left_margin;

	/*
	 * Get the width parameter, in absolute or percentage.
	 * If percentage, make it absolute.  If the height is
	 * zero, make the builtin hidden so the FE knows that the
	 * size really is zero (zero size normally means that
	 * we're blocking).
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_WIDTH);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			builtin->percent_width = val;
		}
		else
		{
			builtin->percent_width = 0;
			builtin->width = val;
			val = FEUNITS_X(val, context);
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		widthSpecified = TRUE;
	}
	val = LO_GetWidthFromStyleSheet(context, state);
	if(val)
	  {
		builtin->width = val;
		builtin->percent_width = 0;
		widthSpecified = TRUE;
	  }

	/*
	 * Get the height parameter, in absolute or percentage.
	 * If percentage, make it absolute.  If the height is
	 * zero, make the builtin hidden so the FE knows that the
	 * size really is zero (zero size normally means that
	 * we're blocking).
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_HEIGHT);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			builtin->percent_height = val;
		}
		else
		{
			builtin->percent_height = 0;
			val = FEUNITS_Y(val, context);
		}
		builtin->height = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
		heightSpecified = TRUE;
	}

	val = LO_GetHeightFromStyleSheet(context, state);
	if(val)
	  {
		builtin->height = val;
		builtin->percent_height = 0;
		heightSpecified = TRUE;
	  }

	/*
	 * If they forgot to specify a width or height, make one up.
	 */
	if (!widthSpecified)
	{
		if (heightSpecified)
			builtin->width = builtin->height;
		else
			builtin->width = BUILTIN_DEF_DIM;
	}
	
	if (!heightSpecified)
	{
		if (widthSpecified)
			builtin->height = builtin->width;
		else
			builtin->height = BUILTIN_DEF_DIM;
	}

	lo_FillInBuiltinGeometry(state, builtin, FALSE);

	/*
	 * Get the extra vertical space parameter.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_VSPACE);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		val = XP_ATOI(str);
		if (val < 0)
		{
			val = 0;
		}
		builtin->border_vert_space = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	builtin->border_vert_space = FEUNITS_Y(builtin->border_vert_space, context);

	/*
	 * Get the extra horizontal space parameter.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_HSPACE);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		val = XP_ATOI(str);
		if (val < 0)
		{
			val = 0;
		}
		builtin->border_horiz_space = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	builtin->border_horiz_space = FEUNITS_X(builtin->border_horiz_space,
						context);

	/* builtin_list for reflection? XXX */

	lo_FinishBuiltin (context, state, builtin);
}

void
lo_FinishBuiltin (MWContext *context, lo_DocState *state, LO_BuiltinStruct *builtin)
{
	int32 baseline_inc;
	int32 line_inc;

#ifdef DEBUG_SPENCE
	printf ("lo_FinishBuiltin\n");
#endif

	/*
	 * Figure out how to align this builtin.
	 * baseline_inc is how much to increase the baseline
	 * of previous element of this line.  line_inc is how
	 * much to increase the line height below the baseline.
	 */
	baseline_inc = 0;
	line_inc = 0;
	/*
	 * If we are at the beginning of a line, with no baseline,
	 * we first set baseline and line_height based on the current
	 * font, then place the builtin.
	 */
	if (state->baseline == 0) {
		state->baseline = 0;
	}

	builtin->x_offset += (int16)builtin->border_horiz_space;
	builtin->y_offset += (int32)builtin->border_vert_space;

	lo_AppendToLineList(context, state,
						(LO_Element *)builtin, baseline_inc);

	lo_UpdateStateAfterBuiltinLayout(state, builtin, line_inc, baseline_inc);
}

void
lo_UpdateStateAfterBuiltinLayout (lo_DocState *state, LO_BuiltinStruct *builtin,
								  int32 line_inc, int32 baseline_inc)
{
	int32 builtin_width;
	int32 x, y;

#ifdef DEBUG_SPENCE
	printf ("lo_UpdateStateAfterBuiltinLayout\n");
#endif

	builtin->width = builtin->width + (2 * builtin->border_width) +
		(2 * builtin->border_horiz_space);

	state->baseline += (intn) baseline_inc;
	state->line_height += (intn) (baseline_inc + line_inc);

	/*
	 * Clean up state
	 */
	state->x = state->x + builtin->x_offset +
		builtin_width - builtin->border_horiz_space;
	state->linefeed_state = 0;
	state->at_begin_line = FALSE;
	state->trailing_space = FALSE;
	state->cur_ele_type = LO_BUILTIN;

	/* Determine the new position of the layer. */
	x = builtin->x + builtin->x_offset + builtin->border_width;
	y = builtin->y + builtin->y_offset + builtin->border_width;
}


void
lo_LayoutInflowBuiltin(MWContext *context,
					 lo_DocState *state,
					 LO_BuiltinStruct *builtin,
					 Bool inRelayout,
					 int32 *line_inc,
					 int32 *baseline_inc)
{
  int32 builtin_width, builtin_height;
  Bool line_break;
  PA_Block buff;
  char *str;
  LO_TextStruct tmp_text;
  LO_TextInfo text_info;
  lo_TopState *top_state;
  
  top_state = state->top_state;
  
  /*
   * All this work is to get the text_info filled in for the current
   * font in the font stack. Yuck, there must be a better way.
   */
  memset (&tmp_text, 0, sizeof (tmp_text));
  buff = PA_ALLOC(1);
  if (buff == NULL)
	{
	  top_state->out_of_memory = TRUE;
	  return;
	}
  PA_LOCK(str, char *, buff);
  str[0] = ' ';
  PA_UNLOCK(buff);
  tmp_text.text = buff;
  tmp_text.text_len = 1;
  tmp_text.text_attr =
	state->font_stack->text_attr;
  FE_GetTextInfo(context, &tmp_text, &text_info);
  PA_FREE(buff);
  
  builtin_width = builtin->width + (2 * builtin->border_width) +
	(2 * builtin->border_horiz_space);
  builtin_height = builtin->height + (2 * builtin->border_width) +
	(2 * builtin->border_vert_space);
  
  /*
   * Will this builtin make the line too wide.
   */
  if ((state->x + builtin_width) > state->right_margin)
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
   * Also don't break in unwrapped preformatted text.
   * Also can't break inside a NOBR section.
   */
  if ((state->at_begin_line != FALSE)||
	  (state->preformatted == PRE_TEXT_YES)||
	  (state->breakable == FALSE))
	{
	  line_break = FALSE;
	}
  
  /*
   * break on the builtin if we have
   * a break.
   */
  if (line_break != FALSE)
	{
	  /*
	   * We need to make the elements sequential, linefeed
	   * before builtin.
	   */
		top_state->element_id = builtin->ele_id;	  

		if (!inRelayout)
		{
			lo_SoftLineBreak(context, state, TRUE);
		}
		else 
		{
			lo_rl_AddSoftBreakAndFlushLine(context, state);
		}
		builtin->x = state->x;
		builtin->y = state->y;
		builtin->ele_id = NEXT_ELEMENT;
	}

  lo_CalcAlignOffsets(state, &text_info, (intn)builtin->alignment,
					  builtin_width, builtin_height,
					  &builtin->x_offset, &builtin->y_offset, line_inc, baseline_inc);
}

