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

#ifdef TEST_16BIT
#define XP_WIN16
#endif /* TEST_16BIT */

#define	SUBDOC_DEF_ANCHOR_BORDER		1
#define	SUBDOC_DEF_VERTICAL_SPACE		5
#define	SUBDOC_DEF_HORIZONTAL_SPACE		5

#define	CELL_LINE_INC		10


void
lo_InheritParentState(MWContext *context,
	lo_DocState *child_state, lo_DocState *parent_state)
{
	/*
	 * Instead of the default of a new doc assuming 100 lines,
	 * we will start assuming a cell is 10 lines.
	 * This save lots of memory in table processing.
	 */
	if ((child_state->is_a_subdoc == SUBDOC_CELL)||
		(child_state->is_a_subdoc == SUBDOC_CAPTION))
	{
		XP_Block line_array_block;
		LO_Element **line_array;

		line_array_block = XP_ALLOC_BLOCK(CELL_LINE_INC *
					sizeof(LO_Element *));
		if (line_array_block != NULL)
		{
			XP_FREE_BLOCK(child_state->line_array);
			child_state->line_array = line_array_block;
			XP_LOCK_BLOCK(line_array, LO_Element **,
				child_state->line_array);
			line_array[0] = NULL;
			XP_UNLOCK_BLOCK(child_state->line_array);
			child_state->line_array_size = CELL_LINE_INC;
#ifdef XP_WIN16
{
			XP_Block *larray_array;

			XP_LOCK_BLOCK(larray_array, XP_Block *,
				child_state->larray_array);
			larray_array[0] = child_state->line_array;
			XP_UNLOCK_BLOCK(child_state->larray_array);
}
#endif /* XP_WIN16 */
		}
	}

	if (((child_state->is_a_subdoc == SUBDOC_CELL)||
		(child_state->is_a_subdoc == SUBDOC_CAPTION))&&
	    ((parent_state->is_a_subdoc == SUBDOC_CELL)||
		(parent_state->is_a_subdoc == SUBDOC_CAPTION)))
	{
		child_state->subdoc_tags = parent_state->subdoc_tags_end;
		child_state->subdoc_tags_end = NULL;
	}

	lo_InheritParentColors(context, child_state, parent_state);
}

void
lo_InheritParentColors(MWContext *context,
	lo_DocState *child_state, lo_DocState *parent_state)
{
	child_state->text_fg.red = STATE_DEFAULT_FG_RED(parent_state);
	child_state->text_fg.green = STATE_DEFAULT_FG_GREEN(parent_state);
	child_state->text_fg.blue = STATE_DEFAULT_FG_BLUE(parent_state);

	child_state->text_bg.red = STATE_DEFAULT_BG_RED(parent_state);
	child_state->text_bg.green = STATE_DEFAULT_BG_GREEN(parent_state);
	child_state->text_bg.blue = STATE_DEFAULT_BG_BLUE(parent_state);
	lo_ResetFontStack(context, child_state);

	child_state->anchor_color.red =
		STATE_UNVISITED_ANCHOR_RED(parent_state);
	child_state->anchor_color.green =
		STATE_UNVISITED_ANCHOR_GREEN(parent_state);
	child_state->anchor_color.blue =
		STATE_UNVISITED_ANCHOR_BLUE(parent_state);

	child_state->visited_anchor_color.red =
		STATE_VISITED_ANCHOR_RED(parent_state);
	child_state->visited_anchor_color.green =
		STATE_VISITED_ANCHOR_GREEN(parent_state);
	child_state->visited_anchor_color.blue =
		STATE_VISITED_ANCHOR_BLUE(parent_state);

	child_state->active_anchor_color.red =
		STATE_SELECTED_ANCHOR_RED(parent_state);
	child_state->active_anchor_color.green =
		STATE_SELECTED_ANCHOR_GREEN(parent_state);
	child_state->active_anchor_color.blue =
		STATE_SELECTED_ANCHOR_BLUE(parent_state);

	child_state->hide_content = parent_state->hide_content;
}
int32
lo_GetSubDocBaseline(LO_SubDocStruct *subdoc)
{
	LO_Element **line_array;
	LO_Element *eptr;
	lo_DocState *subdoc_state;

	subdoc_state = (lo_DocState *)subdoc->state;
	if (subdoc_state == NULL)
	{
		return(0);
	}

	/*
	 * Make eptr point to the start of the element chain
	 * for this subdoc.
	 */
#ifdef XP_WIN16
{
	XP_Block *larray_array;

	if (subdoc_state->larray_array == NULL)
	{
		return(0);
	}
	XP_LOCK_BLOCK(larray_array, XP_Block *, subdoc_state->larray_array);
	subdoc_state->line_array = larray_array[0];
	XP_UNLOCK_BLOCK(subdoc_state->larray_array);
}
#endif /* XP_WIN16 */
	if (subdoc_state->line_array == NULL)
	{
		return(0);
	}
	XP_LOCK_BLOCK(line_array, LO_Element **, subdoc_state->line_array);
	eptr = line_array[0];
	XP_UNLOCK_BLOCK(subdoc_state->line_array);

	while (eptr != NULL)
	{
		if (eptr->type == LO_LINEFEED)
		{
			break;
		}
		eptr = eptr->lo_any.next;
	}
	if (eptr == NULL)
	{
		return(0);
	}
	return(eptr->lo_linefeed.baseline);
}


#ifdef TEST_16BIT
#undef XP_WIN16
#endif /* TEST_16BIT */

