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

#ifdef PROFILE
#pragma profile on
#endif

lo_ListStack *
lo_DefaultList(lo_DocState *state)
{
	lo_ListStack *lptr;

	lptr = XP_NEW(lo_ListStack);
	if (lptr == NULL)
	{
		return(NULL);
	}

	lptr->type = P_UNKNOWN;
	lptr->level = 0;
	lptr->value = 1;
	lptr->compact = FALSE;
	lptr->bullet_type = BULLET_BASIC;
	lptr->quote_type = QUOTE_NONE;
	lptr->old_left_margin = state->win_left;
	lptr->old_right_margin = state->win_width - state->win_right;
	lptr->next = NULL;

	return(lptr);
}


/* mquote means quoted mail message */
void
lo_PushList(lo_DocState *state, PA_Tag *tag, int8 quote_type)
{
	lo_ListStack *lptr;
	intn bullet_type;
	int32 val;
	Bool no_level;
    int32 mquote_line_num = 0;
    int32 mquote_x = 0;

	val = 1;
	no_level = FALSE;
	switch (tag->type)
	{
		/*
		 * Blockquotes and multicolumns pretend to be the current
		 * list type, unless the current list is nothing.
		 * Now we have DIV tags that can act like MULTICOL tags.
		 */
		case P_MULTICOLUMN:
		case P_DIVISION:
		case P_BLOCKQUOTE:
			bullet_type = state->list_stack->bullet_type;
			no_level = TRUE;
			if (state->list_stack->type != P_UNKNOWN)
			{
				tag->type = state->list_stack->type;
			}
			val = state->list_stack->value;
			break;
		case P_NUM_LIST:
			bullet_type = BULLET_NUM;
			break;
		case P_UNUM_LIST:
		case P_MENU:
		case P_DIRECTORY:
			bullet_type = BULLET_BASIC;
			break;
		default:
			bullet_type = BULLET_NONE;
			break;
	}

    /* Support for mail compose quoting. */
    if (quote_type != QUOTE_NONE) 
    {
        mquote_line_num = state->line_num;
        mquote_x = state->left_margin;
    }

	lptr = XP_NEW(lo_ListStack);
	if (lptr == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}

	lptr->type = tag->type;
	if (no_level != FALSE)
	{
		lptr->level = state->list_stack->level;
	}
	else
	{
		lptr->level = state->list_stack->level + 1;
	}
	lptr->compact = FALSE;
	lptr->value = val;
	lptr->bullet_type = bullet_type;
	lptr->old_left_margin = state->left_margin;
	lptr->old_right_margin = state->right_margin;
    lptr->quote_type = quote_type;
    lptr->mquote_line_num = mquote_line_num;
    lptr->mquote_x = mquote_x;

	lptr->next = state->list_stack;
	state->list_stack = lptr;
}


lo_ListStack *
lo_PopList(lo_DocState *state, PA_Tag *tag)
{
	lo_ListStack *lptr;

	lptr = state->list_stack;
	if ((lptr->type == P_UNKNOWN)||(lptr->next == NULL))
	{
		return(NULL);
	}

	state->list_stack = lptr->next;
	lptr->next = NULL;
	return(lptr);
}

#ifdef PROFILE
#pragma profile off
#endif
