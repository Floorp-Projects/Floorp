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
#include "layout.h"
#include "intl_csi.h"

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

#ifdef TEST_16BIT
#define XP_WIN16
#endif /* TEST_16BIT */

#define INTL_FIND	1	/* ftang */
#include "libi18n.h"

/*************************
 * The following is to speed up case conversion
 * to allow faster checking of caseless equal among strings.
 *************************/
#ifdef NON_ASCII_STRINGS
# define TOLOWER(x) (tolower((int)(x)))
#else /* ASCII TABLE LOOKUP */
static unsigned char lower_lookup[256]={
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,
    27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,
    51,52,53,54,55,56,57,58,59,60,61,62,63,64,
        97,98,99,100,101,102,103,104,105,106,107,108,109,
        110,111,112,113,114,115,116,117,118,119,120,121,122,
    91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,
    111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
    129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,
    147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,
    165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,
    183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,
    201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,
    219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,
    237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,
    255};
# define TOLOWER(x)      (lower_lookup[(unsigned int)(x)])
#endif /* NON_ASCII_STRINGS */


static void
lo_next_character(MWContext *context, lo_DocState *state, LO_Element **ele_loc,
	int32 *pos, Bool forward)
{
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
	int16 win_csid = INTL_GetCSIWinCSID(c);
	LO_Element *eptr;
	int32 position;

	eptr = *ele_loc;
	position = *pos;

	/*
	 * If our current element is text, we may be able to just
	 * move inside of it.
	 */
	if (eptr->type == LO_TEXT)
	{
		if ((forward != FALSE)&&
			(position < (eptr->lo_text.text_len - 1)))
		{
#ifdef INTL_FIND	
			/*	Add by ftang to provide international find */
			position = INTL_NextCharIdxInText(win_csid, (unsigned char*)eptr->lo_text.text, position );
#else
			position++;
#endif
			*ele_loc = eptr;
			*pos = position;
			return;
		}
		else if ((forward == FALSE)&&(position > 0))
		{
#ifdef INTL_FIND
			/*	Add by ftang to provide international find */
			position = INTL_PrevCharIdxInText(win_csid, (unsigned char*)eptr->lo_text.text, position );
#else
			position--;
#endif
			*ele_loc = eptr;
			*pos = position;
			return;
		}
	}

	/*
	 * If we didn't return above, we need to move to a new element.
	 */
	if (forward != FALSE)
	{
		/*
		 * If no next element, see if this is a CELL we can
		 * hop out of.
		 */
		if (eptr->lo_any.next == NULL)
		{
			int32 no_loop_id;

			no_loop_id = eptr->lo_any.ele_id;

			/*
			 * Jump cell boundries if there is one
			 * between here and the next element.
			 */
			eptr = lo_JumpCellWall(context, state, eptr);

			/*
			 * If non-null eptr is the cell we were in, move
			 * to the next cell/element.
			 */
			if (eptr != NULL)
			{
				eptr = eptr->lo_any.next;
			}

			/*
			 * infinite loop prevention
			 */
			if ((eptr != NULL)&&(eptr->lo_any.ele_id <= no_loop_id))
			{
#ifdef DEBUG
XP_TRACE(("Find loop avoidance 1\n"));
#endif /* DEBUG */
				eptr = NULL;
			}
		}
		else
		{
			eptr = eptr->lo_any.next;
		}
	}
	else
	{
		/*
		 * If no previous element, see if this is a CELL we can
		 * hop out of.
		 */
		if (eptr->lo_any.prev == NULL)
		{
			int32 no_loop_id;

			no_loop_id = eptr->lo_any.ele_id;

			/*
			 * Jump cell boundries if there is one
			 * between here and the previous element.
			 */
			eptr = lo_JumpCellWall(context, state, eptr);

			/*
			 * If non-null eptr is the cell we were in, move
			 * to the previous cell/element.
			 */
			if (eptr != NULL)
			{
				eptr = eptr->lo_any.prev;
			}

			/*
			 * infinite loop prevention
			 */
			if ((eptr != NULL)&&(eptr->lo_any.ele_id >= no_loop_id))
			{
#ifdef DEBUG
XP_TRACE(("Find loop avoidance 2\n"));
#endif /* DEBUG */
				eptr = NULL;
			}
		}
		else
		{
			eptr = eptr->lo_any.prev;
		}
	}

	while (eptr != NULL)
	{
		if (eptr->type == LO_LINEFEED)
		{
			break;
		}
		else if ((eptr->type == LO_TEXT)&&(eptr->lo_text.text != NULL))
		{
			break;
		}
		else if (eptr->type == LO_CELL)
		{
			/*
			 * When we walk onto a cell, we need
			 * to walk into it if it isn't empty.
			 */
			if ((forward != FALSE)&&
			    (eptr->lo_cell.cell_list != NULL))
			{
				eptr = eptr->lo_cell.cell_list;
				continue;
			}
			else if ((forward == FALSE)&&
			         (eptr->lo_cell.cell_list_end != NULL))
			{
				eptr = eptr->lo_cell.cell_list_end;
				continue;
			}
		}

		/*
		 * Move forward or back to the next element
		 */
		if (forward != FALSE)
		{
			/*
			 * If no next element, see if this is a CELL we can
			 * hop out of.
			 */
			if (eptr->lo_any.next == NULL)
			{
				int32 no_loop_id;

				no_loop_id = eptr->lo_any.ele_id;

				/*
				 * Jump cell boundries if there is one
				 * between here and the next element.
				 */
				eptr = lo_JumpCellWall(context, state, eptr);

				/*
				 * If non-null eptr is the cell we were in, move
				 * to the next cell/element.
				 */
				if (eptr != NULL)
				{
					eptr = eptr->lo_any.next;
				}

				/*
				 * infinite loop prevention
				 */
				if ((eptr != NULL)&&(
					eptr->lo_any.ele_id <= no_loop_id))
				{
#ifdef DEBUG
XP_TRACE(("Find loop avoidance 3\n"));
#endif /* DEBUG */
					eptr = NULL;
				}
			}
			else
			{
				eptr = eptr->lo_any.next;
			}
		}
		else
		{
			/*
			 * If no previous element, see if this is a CELL we can
			 * hop out of.
			 */
			if (eptr->lo_any.prev == NULL)
			{
				int32 no_loop_id;

				no_loop_id = eptr->lo_any.ele_id;

				/*
				 * Jump cell boundries if there is one
				 * between here and the previous element.
				 */
				eptr = lo_JumpCellWall(context, state, eptr);

				/*
				 * If non-null eptr is the cell we were in, move
				 * to the previous cell/element.
				 */
				if (eptr != NULL)
				{
					eptr = eptr->lo_any.prev;
				}

				/*
				 * infinite loop prevention
				 */
				if ((eptr != NULL)&&
					(eptr->lo_any.ele_id >= no_loop_id))
				{
#ifdef DEBUG
XP_TRACE(("Find loop avoidance 4\n"));
#endif /* DEBUG */
					eptr = NULL;
				}
			}
			else
			{
				eptr = eptr->lo_any.prev;
			}
		}
	}
	if (eptr == NULL)
	{
		*ele_loc = NULL;
		*pos = 0;
	}
	else if (eptr->type == LO_TEXT)
	{
		*ele_loc = eptr;
		if (forward != FALSE)
		{
			*pos = 0;
		}
		else
		{
#ifdef INTL_FIND
			/*	Add by ftang to provide international find */
			if(eptr->lo_text.text_len == 0)
				position = 0;
			else
				position = INTL_PrevCharIdxInText(win_csid, (unsigned char*)eptr->lo_text.text, eptr->lo_text.text_len );
#else
			position = eptr->lo_text.text_len - 1;
			if (position < 0)
			{
				position = 0;
			}
#endif
			*pos = position;
		}
	}
	else if (eptr->type == LO_LINEFEED)
	{
		*ele_loc = eptr;
		*pos = 0;
	}
}


static Bool
lo_find_in_list(MWContext *context, lo_DocState *state,
	LO_Element *eptr, char *cmp_text, int32 len, int32 position,
	LO_Element **start_ele_loc, int32 *start_position,
	LO_Element **end_ele_loc, int32 *end_position,
	Bool use_case, Bool forward)
{
	int32 cnt;
	LO_Element *start_element, *end_element;
	int32 start_pos, end_pos;
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
	int16 win_csid = INTL_GetCSIWinCSID(c);

	while (eptr != NULL)
	{
		Bool have_start, not_equal;
		int charlen = 1;

		have_start = FALSE;
		while ((eptr != NULL)&&(have_start == FALSE))
		{
			unsigned char *tptr;
			unsigned char *str;

			switch (eptr->type)
			{
#ifndef INTL_FIND
			    char tchar;
#endif
			    case LO_TEXT:
				if (eptr->lo_text.text != NULL)
				{
					PA_LOCK(str, unsigned char *,
						eptr->lo_text.text);
					tptr = (unsigned char *)(str + position);
#ifdef INTL_FIND
					if (use_case)
						have_start = INTL_MatchOneCaseChar(win_csid, (unsigned char*)cmp_text,tptr,&charlen);
					else
						have_start = INTL_MatchOneChar(win_csid, (unsigned char*)cmp_text,tptr,&charlen);
#else
					if (use_case == FALSE)
					{
						tchar = TOLOWER(*tptr);
					}
					else
					{
						tchar = *tptr;
					}
					if (cmp_text[0] == tchar)
					{
						have_start = TRUE;
					}
#endif
					PA_UNLOCK(eptr->lo_text.text);
				}
				break;
			    case LO_LINEFEED:
				if (cmp_text[0] == ' ')
				{
					have_start = TRUE;
				}
				break;
			    case LO_HRULE:
			    case LO_FORM_ELE:
			    case LO_BULLET:
			    case LO_IMAGE:
			    case LO_SUBDOC:
			    case LO_TABLE:
			    default:
				break;
			}
			if (have_start == FALSE)
			{
				lo_next_character(context, state,
					&eptr, &position, forward);
			}
		}
		if (have_start == FALSE)
		{
			return(FALSE);
		}

		start_element = eptr;
		start_pos = position;
#ifdef INTL_FIND
		if (len == charlen)
#else
		if (len == 1)
#endif
		{
			end_element = eptr;
			end_pos = position;
			*start_ele_loc = start_element;
			*start_position = start_pos;
			*end_ele_loc = end_element;
			*end_position = end_pos;
			return(TRUE);
		}

#ifdef INTL_FIND
		cnt = charlen;
#else
		cnt = 1;
#endif	
		not_equal = FALSE;
		lo_next_character(context, state, &eptr, &position, TRUE);
		while ((eptr != NULL)&&(cnt < len)&&(not_equal == FALSE))
		{
			unsigned char *tptr; /* this needs to be an unsigned quantity!  chouck 3-Nov-94 */
			char *str;

			switch (eptr->type)
			{
#ifndef INTL_FIND
			    char tchar;
#endif
			    case LO_TEXT:
				if (eptr->lo_text.text != NULL)
				{
					PA_LOCK(str, char *,
						eptr->lo_text.text);
					tptr = (unsigned char *)(str + position);
#ifdef INTL_FIND
					if (use_case)
						not_equal = ! INTL_MatchOneCaseChar(win_csid,(unsigned char *) cmp_text+cnt,tptr,&charlen);
					else
						not_equal = ! INTL_MatchOneChar(win_csid,(unsigned char *) cmp_text+cnt,tptr,&charlen);
#else
					if (use_case == FALSE)
					{                          
					    /* this needs to be an unsigned quantity!  chouck 3-Nov-94 */
						tchar = TOLOWER(*tptr);
					}
					else
					{
						tchar = (char) *tptr;
					}
					if (tchar != cmp_text[cnt])
					{
						not_equal = TRUE;
					}
#endif
					PA_UNLOCK(eptr->lo_text.text);
				}
				break;
			    case LO_LINEFEED:
				if (cmp_text[cnt] != ' ')
				{
					not_equal = TRUE;
				}
				break;
			    case LO_HRULE:
			    case LO_FORM_ELE:
			    case LO_BULLET:
			    case LO_IMAGE:
			    case LO_SUBDOC:
			    case LO_TABLE:
			    default:
				break;
			}
#ifdef INTL_FIND
		cnt += charlen;
#else
		cnt++;
#endif	
			if ((not_equal == FALSE)&&(cnt < len))
			{
				lo_next_character(context, state,
					&eptr, &position, TRUE);
			}
		}

		if ((cnt == len)&&(not_equal == FALSE))
		{
			end_element = eptr;
			end_pos = position;
			*start_ele_loc = start_element;
			*start_position = start_pos;
			*end_ele_loc = end_element;
			*end_position = end_pos;
			return(TRUE);
		}

		eptr = start_element;
		position = start_pos;
		lo_next_character(context, state, &eptr, &position, forward);
	}
	return(FALSE);
}


static Bool
lo_element_in_floating_cell(MWContext *context, lo_DocState *state,
					LO_Element *eptr)
{
	LO_Element *cell;
	LO_Element *f_ptr;
	int32 x, y;
	int32 ret_x, ret_y;

	/*
	 * Error test.
	 */
	if (eptr == NULL)
	{
		return(FALSE);
	}

	x = eptr->lo_any.x + eptr->lo_any.x_offset;
        y = eptr->lo_any.y + eptr->lo_any.y_offset;

	/*
	 * This wouldn't work if the element had zero width, but since
	 * we are assuming we are calling this on an element returned
	 * as the result of a FIND operation, it must be a text item
	 * with a non-zero width.
	 */
    cell = lo_XYToDocumentElement(context, state, x, y, TRUE, FALSE, TRUE,
					&ret_x, &ret_y);
	if ((cell == NULL)||(cell->type != LO_CELL))
	{
		return(FALSE);
	}

	/*
	 * Now look for this cell in the float list.
	 */
	f_ptr = state->float_list;
	while (f_ptr != NULL)
	{
		if (f_ptr == cell)
		{
			return(TRUE);
		}
		f_ptr = f_ptr->lo_any.next;
	}
	return(FALSE);
}


Bool
LO_FindText(MWContext *context, char *text,
	LO_Element **start_ele_loc, int32 *start_position,
	LO_Element **end_ele_loc, int32 *end_position,
	Bool use_case, Bool forward)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	LO_Element *eptr;
	LO_Element *start_element, *end_element;
	int32 start_pos, end_pos;
	int32 position;
	int32 i, len;
	char *cmp_text;
	Bool start_at_top;
	Bool search_float_list;
	Bool ret_val;

	start_at_top = FALSE;
	search_float_list = TRUE;

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

	eptr = *start_ele_loc;
	position = *start_position;

	if (eptr == NULL)
	{
		start_at_top = TRUE;
		if (forward != FALSE)
		{
			if (state->line_num <= 1)
			{
				return(FALSE);
			}
			else
			{
				LO_Element **array;
#ifdef XP_WIN16
{
				XP_Block *larray_array;

				XP_LOCK_BLOCK(larray_array, XP_Block *,
					state->larray_array);
				state->line_array = larray_array[0];
				XP_UNLOCK_BLOCK(state->larray_array);
}
#endif /* XP_WIN16 */

				XP_LOCK_BLOCK(array, LO_Element **,
					state->line_array);
				eptr = array[0];
				XP_UNLOCK_BLOCK(state->line_array);
				if (eptr == NULL)
				{
					return(FALSE);
				}
			}
			position = 0;
		}
		else
		{
			if (state->line_num <= 1)
			{
				return(FALSE);
			}
			else
			{
				eptr = state->end_last_line;
				if (eptr == NULL)
				{
					return(FALSE);
				}
			}
			position = 0;
		}
	}
	/*
	 * Else, if we were passed a start position, we need to know if
	 * that position is in a floating element in the margin, or is
	 * in one of the normal document lines.  If we starting in the
	 * float list, we don't want to search it a second time.
	 */
	else if (lo_element_in_floating_cell(context, state, eptr))
	{
		search_float_list = FALSE;
	}

	len = XP_STRLEN(text);
	cmp_text = (char*) XP_ALLOC(len + 1);
	if (cmp_text == NULL)
	{
		return(FALSE);
	}
#ifdef INTL_FIND
	/* If we use INTL_FIND. Do the conversion for the cmp_text later in INTL_MatchOneChar() routine */
	for (i=0; i<len; i++)
	{
		cmp_text[i] = text[i];
	}	
#else
	if (use_case != FALSE)
	{
		for (i=0; i<len; i++)
		{
			cmp_text[i] = text[i];
		}
	}
	else
	{
		for (i=0; i<len; i++)
		{
			cmp_text[i] = TOLOWER((unsigned char) text[i]);
		}
	}
#endif
	cmp_text[len] = '\0';

	if (start_at_top == FALSE)
	{
		lo_next_character(context, state, &eptr, &position, forward);
	}

	ret_val = lo_find_in_list(context, state, eptr, cmp_text, len,
			position, &start_element, &start_pos,
			&end_element, &end_pos, use_case, forward);
	if (ret_val != FALSE)
	{
		*start_ele_loc = start_element;
		*start_position = start_pos;
		*end_ele_loc = end_element;
		*end_position = end_pos;

		if (cmp_text != NULL)
		{
			XP_FREE(cmp_text);
		}
		return(TRUE);
	}

	/*
	 * While we failed to find anything, we still havn't checked the
	 * contents of table in the margins.  It will appear out of
	 * order, but we should check them now.
	 */
	if (search_float_list != FALSE)
	{
		eptr = state->float_list;
		position = 0;
		ret_val = lo_find_in_list(context, state, eptr, cmp_text, len,
				position, &start_element, &start_pos,
				&end_element, &end_pos, use_case, forward);
		if (ret_val != FALSE)
		{
			*start_ele_loc = start_element;
			*start_position = start_pos;
			*end_ele_loc = end_element;
			*end_position = end_pos;

			if (cmp_text != NULL)
			{
				XP_FREE(cmp_text);
			}
			return(TRUE);
		}
	}

	if (cmp_text != NULL)
	{
		XP_FREE(cmp_text);
	}

	return(FALSE);
}


static Bool
lo_child_search(MWContext *context, MWContext **ret_context, char *text,
	LO_Element **start_ele_loc, int32 *start_position,
	LO_Element **end_ele_loc, int32 *end_position,
	Bool use_case, Bool forward)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	Bool ret_val;

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
	ret_val = FALSE;

	if (top_state->is_grid == FALSE)
	{
		ret_val = LO_FindText(context, text,
			start_ele_loc, start_position,
			end_ele_loc, end_position,
			use_case, forward);
		if (ret_val != FALSE)
		{
			*ret_context = context;
		}
	}
	else
	{
		lo_GridCellRec *cell_ptr;

		cell_ptr = top_state->the_grid->cell_list;
		while (cell_ptr != NULL)
		{
			if (cell_ptr->context != NULL)
			{
				/*
				 * Always start at the beginning of
				 * child docs.
				 */
				*start_ele_loc = NULL;
				*end_ele_loc = NULL;
				*start_position = 0;
				*end_position = 0;
				ret_val = lo_child_search(cell_ptr->context,
					ret_context, text,
					start_ele_loc, start_position,
					end_ele_loc, end_position,
					use_case, forward);
				if (ret_val != FALSE)
				{
					break;
				}
			}
			cell_ptr = cell_ptr->next;
		}
	}

	return(ret_val);
}


static Bool
lo_sibling_search(MWContext *child, MWContext **ret_context, char *text,
	LO_Element **start_ele_loc, int32 *start_position,
	LO_Element **end_ele_loc, int32 *end_position,
	Bool use_case, Bool forward)
{
	MWContext *parent;
	int32 doc_id;
	lo_TopState *top_state;
	Bool ret_val;
	lo_GridCellRec *cell_ptr;

	/*
	 * We must have a grid parent
	 */
	parent = child->grid_parent;
	if (parent == NULL)
	{
		return(FALSE);
	}

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(parent);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->doc_state == NULL))
	{
		return(FALSE);
	}

	/*
	 * We must have a grid parent
	 */
	if (top_state->is_grid == FALSE)
	{
		return(FALSE);
	}

	ret_val = FALSE;

	/*
	 * Find the sibling after the child
	 */
	cell_ptr = top_state->the_grid->cell_list;
	while (cell_ptr != NULL)
	{
		if (cell_ptr->context == child)
		{
			cell_ptr = cell_ptr->next;
			break;
		}
		cell_ptr = cell_ptr->next;
	}

	/*
	 * Search all siblings (if any).
	 */
	while (cell_ptr != NULL)
	{
		if (cell_ptr->context != NULL)
		{
			/*
			 * Always start at the beginning of sibling docs.
			 */
			*start_ele_loc = NULL;
			*end_ele_loc = NULL;
			*start_position = 0;
			*end_position = 0;
			ret_val = lo_child_search(cell_ptr->context,
				ret_context, text,
				start_ele_loc, start_position,
				end_ele_loc, end_position,
				use_case, forward);
			if (ret_val != FALSE)
			{
				break;
			}
		}
		cell_ptr = cell_ptr->next;
	}

	return(ret_val);
}


Bool
LO_FindGridText(MWContext *context, MWContext **ret_context, char *text,
	LO_Element **start_ele_loc, int32 *start_position,
	LO_Element **end_ele_loc, int32 *end_position,
	Bool use_case, Bool forward)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	Bool ret_val;
	LO_Element *start_ele;
	LO_Element *end_ele;
	int32 start_pos;
	int32 end_pos;

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
	ret_val = FALSE;
	start_ele = *start_ele_loc;
	end_ele = *end_ele_loc;
	start_pos = *start_position;
	end_pos = *end_position;

	/*
	 * If no starting context we start at the top level grid context.
	 */
	if (*ret_context == NULL)
	{
		MWContext *parent;

		/*
		 * Find the top of the nested grid tree
		 */
		parent = context;
		while ((parent->is_grid_cell != FALSE)&&
			(parent->grid_parent != NULL))
		{
			parent = parent->grid_parent;
		}
		/*
		 * Search the tree completely.
		 */
		ret_val = lo_child_search(parent, ret_context, text,
					start_ele_loc, start_position,
					end_ele_loc, end_position,
					use_case, forward);
	}
	else
	{
		MWContext *child;

		/*
		 * Search the starting context at the starting position.
		 */
		child = *ret_context;
		ret_val = lo_child_search(child, ret_context, text,
					start_ele_loc, start_position,
					end_ele_loc, end_position,
					use_case, forward);

		/*
		 * If it wasn't in the starting context, walk up the tree
		 * searching all siblings (after the parent sibling)
		 * at each level.
		 */
		while ((ret_val == FALSE)&&(child->is_grid_cell != FALSE)&&
			(child->grid_parent != NULL))
		{
			/*
			 * Always start at the beginning of sibling docs.
			 */
			*start_ele_loc = NULL;
			*end_ele_loc = NULL;
			*start_position = 0;
			*end_position = 0;
			ret_val = lo_sibling_search(child, ret_context, text,
					start_ele_loc, start_position,
					end_ele_loc, end_position,
					use_case, forward);
			child = child->grid_parent;
		}
	}

	/*
	 * If we didn't find it, restore starting values.
	 */
	if (ret_val == FALSE)
	{
		*start_ele_loc = start_ele;
		*end_ele_loc = end_ele;
		*start_position = start_pos;
		*end_position = end_pos;
	}

	return(ret_val);
}


void
LO_SelectText(MWContext *context, LO_Element *start, int32 start_pos,
	LO_Element *end, int32 end_pos, int32 *x, int32 *y)
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

	if ((start == NULL)||(end == NULL))
	{
		return;
	}

	LO_HighlightSelection(context, FALSE);

	state->selection_start = start;
        state->selection_start_pos = start_pos;
        state->selection_end = end;
        state->selection_end_pos = end_pos;

	LO_HighlightSelection(context, TRUE);

	*x = (start->lo_any.x + state->base_x);
	*y = (start->lo_any.y + state->base_y);
}

#ifdef TEST_16BIT
#undef XP_WIN16
#endif /* TEST_16BIT */

#ifdef PROFILE
#pragma profile off
#endif
