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
#include "laystyle.h"

#ifdef PROFILE
#pragma profile on
#endif

#ifdef XP_MAC
# ifdef XP_TRACE
#  undef XP_TRACE
# endif
# define XP_TRACE(X)
#else
#ifndef XP_TRACE
# define XP_TRACE(X) fprintf X
#endif
#endif /* XP_MAC */


#define	DEFAULT_HR_THICKNESS	2


/* This function is now only called during relayout */
void
lo_FillInHorizontalRuleGeometry(MWContext *context, lo_DocState *state, LO_HorizRuleStruct *hrule)
{
	int32 doc_width;

	hrule->ele_id = NEXT_ELEMENT;
	hrule->x = state->x;
	hrule->x_offset = 0;
	hrule->y = state->y;
	hrule->y_offset = 0;

	state->line_height = FEUNITS_Y(hrule->thickness, context) +
		FEUNITS_Y(2, context);
	/*
	 * horizontal rules are always at least as tall
	 * as the default line height.
	 */
	if (state->line_height < state->default_line_height)
	{
		state->line_height = state->default_line_height;
	}
	hrule->height = FEUNITS_Y(hrule->thickness, context);		

	/* Set hrule->width  */
	doc_width = state->right_margin - state->left_margin;
	if (hrule->width_in_initial_layout > 0) 
	{
		/* Absolute width was specified in initial layout*/
		hrule->width = hrule->width_in_initial_layout;
	}
	else if (hrule->percent_width > 0 ) 
	{
		/* Percent width was specified in initial layout */
		int32 val = hrule->percent_width;
		if (state->allow_percent_width == FALSE) 
		{
			val = 1;
		}
		else 
		{
			val = doc_width * val / 100;
		}
		hrule->width = val;
	}
	else 
	{
		/* No width was specified in initial layout */
		if (state->allow_percent_width == FALSE) 
		{
			hrule->width = 1;
		}
		else
		{
			hrule->width = doc_width;
		}
	}

	hrule->y_offset = (state->line_height - hrule->thickness) / 2;
}

void
lo_UpdateStateAfterHorizontalRule(lo_DocState *state, LO_HorizRuleStruct *hrule)
{
	state->x = state->x + hrule->x_offset + hrule->width;
	state->linefeed_state = 0;
	state->at_begin_line = FALSE;
	state->cur_ele_type = LO_HRULE;
}

void
lo_StartHorizontalRuleLayout(MWContext *context,
							 lo_DocState *state,
							 LO_HorizRuleStruct *hrule)
{
  lo_PushAlignment(state, P_HRULE, hrule->alignment);

  lo_FillInHorizontalRuleGeometry(context, state, hrule);
}

void
lo_FinishHorizontalRuleLayout(MWContext *context,
							  lo_DocState *state,
							  LO_HorizRuleStruct *hrule)
{
  lo_AlignStack *aptr;

  aptr = lo_PopAlignment(state);
  if (aptr != NULL)
	{
	  XP_DELETE(aptr);
	}
}

void
lo_HorizontalRule(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	LO_HorizRuleStruct *hrule;
	PA_Block buff;
	char *str;
	int32 val;

	hrule = (LO_HorizRuleStruct *)lo_NewElement(context, state, LO_HRULE, NULL, 0);
	if (hrule == NULL)
	{
		return;
	}

	hrule->type = LO_HRULE;	
	hrule->width = state->right_margin - state->left_margin;
	hrule->height = 0;
	hrule->next = NULL;
	hrule->prev = NULL;

	hrule->end_x = 0;
	hrule->end_y = 0;

	hrule->FE_Data = NULL;
	hrule->alignment = LO_ALIGN_CENTER;
	hrule->thickness = DEFAULT_HR_THICKNESS;

	hrule->ele_attrmask = LO_ELE_SHADED;

	hrule->sel_start = -1;
	hrule->sel_end = -1;
	hrule->percent_width = 0;
	hrule->width_in_initial_layout = 0;

	buff = lo_FetchParamValue(context, tag, PARAM_ALIGN);
	if (buff != NULL)
	  {
		PA_LOCK(str, char *, buff);
		if (pa_TagEqual("left", str))
		  {
			hrule->alignment = LO_ALIGN_LEFT;
		  }
		else if (pa_TagEqual("right", str))
		  {
			hrule->alignment = LO_ALIGN_RIGHT;
		  }
		PA_UNLOCK(buff);
		PA_FREE(buff);
	  }
	else if (state->align_stack)
	  {
		hrule->alignment = state->align_stack->alignment;
	  }
	
	buff = lo_FetchParamValue(context, tag, PARAM_SIZE);
	if (buff != NULL)
	  {
		PA_LOCK(str, char *, buff);
		val = XP_ATOI(str);
		if (val < 1)
		{
			val = 1;
		}
		else if (val > 100)
		{
			val = 100;
		}
		hrule->thickness = (intn) val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	buff = lo_FetchParamValue(context, tag, PARAM_WIDTH);
	if (buff != NULL)
	{
		Bool is_percent;

		PA_LOCK(str, char *, buff);
		val = lo_ValueOrPercent(str, &is_percent);
		if (is_percent != FALSE)
		{
			hrule->percent_width = val;
		}
		else
		{
			hrule->percent_width = 0;
			hrule->width_in_initial_layout = val;   /* Store absolute width for use in relayout*/
			val = FEUNITS_X(val, context);
		}
		hrule->width = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	/* Possible bug here if LO_GetWidthFromStyleSheet() is converting % widths to
	   absolute widths */
	val = LO_GetWidthFromStyleSheet(context, state);
	if(val)
	  {
		hrule->percent_width = 0;
		hrule->width = val;
		hrule->width_in_initial_layout = val;		/* Store absolute width for use in relayout */
	  }	

	buff = lo_FetchParamValue(context, tag, PARAM_NOSHADE);
	if (buff != NULL)
	{
		hrule->ele_attrmask &= (~(LO_ELE_SHADED));
		PA_FREE(buff);
	}

	/*
	 * Alignment of HRULE now done before we ever get here.
	if (state->allow_percent_width != FALSE)
	{
		if (hrule->alignment == LO_ALIGN_CENTER)
		{
			hrule->x_offset = (doc_width - hrule->width) / 2;
		}
		else if (hrule->alignment == LO_ALIGN_RIGHT)
		{
			hrule->x_offset = doc_width - hrule->width;
		}
	}
	if (hrule->x_offset < 0)
	{
		hrule->x_offset = state->left_margin;
	}
	 */

	lo_StartHorizontalRuleLayout(context, state, hrule);

	lo_AppendToLineList(context, state, (LO_Element *)hrule, 0);

	lo_UpdateStateAfterHorizontalRule(state, hrule);

	lo_SoftLineBreak(context, state, FALSE);

	lo_FinishHorizontalRuleLayout(context, state, hrule);
}

#ifdef PROFILE
#pragma profile off
#endif
