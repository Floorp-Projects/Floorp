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
#include "pa_parse.h"

static LO_Element *lo_duplicate_element(MWContext *context, lo_DocState *state,
					int32 block_watch, LO_Element *eptr);
         
#ifdef XP_WIN16
#define SIZE_LIMIT 32000
#endif


static LO_Element *
lo_line_to_element(lo_DocState *state, int32 line)
{
	LO_Element *eptr;
	LO_Element **line_array;
#ifdef XP_WIN16
        intn a_size;
        intn a_indx;
        intn a_line;
        XP_Block *larray_array;
#endif /* XP_WIN16 */

	if (line >= (state->line_num - 1))
	{
		return(NULL);
	}

#ifdef XP_WIN16
        a_size = SIZE_LIMIT / sizeof(LO_Element *);
        a_indx = (intn)(line / a_size);
        a_line = (intn)(line - (a_indx * a_size));

        XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
        state->line_array = larray_array[a_indx];
        XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
        eptr = line_array[a_line];

        XP_UNLOCK_BLOCK(state->line_array);
        XP_UNLOCK_BLOCK(state->larray_array);
#else
        XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
        eptr = line_array[line];
        XP_UNLOCK_BLOCK(state->line_array);
#endif /* XP_WIN16 */

	return(eptr);
}


static void
lo_set_element_to_line(lo_DocState *state, LO_Element *eptr, int32 line)
{
	LO_Element **line_array;
#ifdef XP_WIN16
        intn a_size;
        intn a_indx;
        intn a_line;
        XP_Block *larray_array;
#endif /* XP_WIN16 */

	if (line >= (state->line_num - 1))
	{
		return;
	}

#ifdef XP_WIN16
        a_size = SIZE_LIMIT / sizeof(LO_Element *);
        a_indx = (intn)(line / a_size);
        a_line = (intn)(line - (a_indx * a_size));

        XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
        state->line_array = larray_array[a_indx];
        XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
	line_array[a_line] = eptr;

        XP_UNLOCK_BLOCK(state->line_array);
        XP_UNLOCK_BLOCK(state->larray_array);
#else
        XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
	line_array[line] = eptr;
        XP_UNLOCK_BLOCK(state->line_array);
#endif /* XP_WIN16 */
}

static void
lo_duplicate_cell(MWContext *context, lo_DocState *state, int32 block_watch,
		LO_CellStruct *old_cell, LO_CellStruct *new_cell)
{
	LO_Element *list_ptr;
	LO_Element *tail_ptr;
	LO_Element *eptr;

	/*
	 * Duplicate the cells normal list of elements.
	 */
	list_ptr = NULL;
	eptr = old_cell->cell_list;
	if (eptr != NULL)
	{
		list_ptr = lo_duplicate_element(context, state,
					block_watch, eptr);
		eptr = eptr->lo_any.next;
	}
	tail_ptr = list_ptr;
	while (eptr != NULL)
	{
		tail_ptr->lo_any.next = lo_duplicate_element(context, state,
							block_watch, eptr);
		tail_ptr = tail_ptr->lo_any.next;
		eptr = eptr->lo_any.next;
	}
	if (tail_ptr != NULL)
	{
		tail_ptr->lo_any.next = NULL;
	}
	new_cell->cell_list = list_ptr;
	new_cell->cell_list_end = tail_ptr;

	/*
	 * Duplicate the cells floating list of elements.
	 */
	list_ptr = NULL;
	eptr = old_cell->cell_float_list;
	if (eptr != NULL)
	{
		list_ptr = lo_duplicate_element(context, state,
						block_watch, eptr);
		eptr = eptr->lo_any.next;
	}
	tail_ptr = list_ptr;
	while (eptr != NULL)
	{
		tail_ptr->lo_any.next = lo_duplicate_element(context, state,
						block_watch, eptr);
		tail_ptr = tail_ptr->lo_any.next;
		eptr = eptr->lo_any.next;
	}
	if (tail_ptr != NULL)
	{
		tail_ptr->lo_any.next = NULL;
	}
	new_cell->cell_float_list = list_ptr;
}


static LO_Element *
lo_duplicate_element(MWContext *context, lo_DocState *state,
			int32 block_watch, LO_Element *eptr)
{
	LO_Element *ret_eptr;

	if (eptr == NULL)
	{
		return(NULL);
	}

	ret_eptr = lo_NewElement(context, state, eptr->type, NULL, 0);
	if (ret_eptr == NULL)
	{
		return(NULL);
	}

	switch(eptr->type)
	{
		case LO_TEXT:
			XP_BCOPY((char *)eptr, (char *)(ret_eptr),
				sizeof(LO_TextStruct));
			break;
		case LO_LINEFEED:
			XP_BCOPY((char *)eptr, (char *)(ret_eptr),
				sizeof(LO_LinefeedStruct));
			break;
		case LO_HRULE:
			XP_BCOPY((char *)eptr, (char *)(ret_eptr),
				sizeof(LO_HorizRuleStruct));
			break;
		case LO_IMAGE:
			XP_BCOPY((char *)eptr, (char *)(ret_eptr),
				sizeof(LO_ImageStruct));
#ifndef M12N            /* XXXM12N  */
			(void)IL_ReplaceImage(context,
				(LO_ImageStruct *)ret_eptr,
				(LO_ImageStruct *)eptr);
#else
            ((LO_ImageStruct *)ret_eptr)->image_req =
                ((LO_ImageStruct *)eptr)->image_req;
#endif /* M12N */
			FE_ShiftImage(context, (LO_ImageStruct *)ret_eptr);
			break;
		case LO_BULLET:
			XP_BCOPY((char *)eptr, (char *)(ret_eptr),
				sizeof(LO_BulletStruct));
			break;
		case LO_FORM_ELE:
			XP_BCOPY((char *)eptr, (char *)(ret_eptr),
				sizeof(LO_FormElementStruct));
			break;
		case LO_SUBDOC:
			XP_BCOPY((char *)eptr, (char *)(ret_eptr),
				sizeof(LO_SubDocStruct));
			break;
		case LO_TABLE:
			XP_BCOPY((char *)eptr, (char *)(ret_eptr),
				sizeof(LO_TableStruct));
			break;
		case LO_CELL:
			XP_BCOPY((char *)eptr, (char *)(ret_eptr),
				sizeof(LO_CellStruct));
			lo_duplicate_cell(context, state, block_watch,
				(LO_CellStruct *)eptr,
				(LO_CellStruct *)ret_eptr);
			break;
		case LO_EMBED:
			XP_BCOPY((char *)eptr, (char *)(ret_eptr),
				sizeof(LO_EmbedStruct));
			break;
#ifdef JAVA
		case LO_JAVA:
			XP_BCOPY((char *)eptr, (char *)(ret_eptr),
				sizeof(LO_JavaAppStruct));
			break;
#endif
		case LO_NONE:
		case LO_UNKNOWN:
			ret_eptr = NULL;
			break;
	}

	/*
	 * If we have duplicated the blocking element, set its
	 * new pointer into state.
	 */
	if ((ret_eptr != NULL)&&(block_watch != -1))
	{
		if (ret_eptr->lo_any.ele_id == block_watch)
		{
			state->top_state->layout_blocking_element = 
				ret_eptr;
			block_watch = -1;
		}
	}

	return(ret_eptr);
}


void
lo_ClipLines(MWContext *context, lo_DocState *state, int32 line)
{
	int32 i;
	int32 dy;
	int32 block_watch;
	LO_Element *start_clipping;
	LO_Element *end_clipping;
	LO_Element *eptr;
	LO_Element *end_ptr;
	LO_Element *end_last_line;
	LO_Element *float_list;
	LO_Element *float_ptr;

#ifdef MEMORY_ARENAS	
	lo_arena *old_first_arena;
	lo_arena *old_current_arena;

	/*
	 * Save off the current memory arena, and make a new one.
	 */
	old_first_arena = state->top_state->first_arena;
	old_current_arena = state->top_state->current_arena;
	state->top_state->first_arena = NULL;
	state->top_state->current_arena = NULL;
	lo_InitializeMemoryArena(state->top_state);
	if (state->top_state->first_arena == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		state->top_state->first_arena = old_first_arena;
		state->top_state->current_arena = old_current_arena;
		return;
	}


#endif /* MEMORY_ARENAS */
	
	/*
	 * Clear the selection (if any).
	 */
	LO_HighlightSelection(context, FALSE);
	state->selection_start = NULL;
	state->selection_start_pos = 0;
	state->selection_end = NULL;
	state->selection_end_pos = 0;

	/*
	 * If there is a blocked element that changes, watch it and reset
	 * it properly.
	 */
	block_watch = -1;
	if (state->top_state->layout_blocking_element != NULL)
	{
		block_watch =
		    state->top_state->layout_blocking_element->lo_any.ele_id;
	}

	dy = 0;

	/*
	 * Delete all elements in the lines being clipped off.
	 */
	start_clipping = lo_line_to_element(state, 0);
	end_clipping = lo_line_to_element(state, line);
	if ((end_clipping != NULL)&&(start_clipping != NULL))
	{
		dy = end_clipping->lo_any.y - start_clipping->lo_any.y;
	}
	while ((start_clipping != NULL)&&(start_clipping != end_clipping))
	{
		lo_ScrapeElement(context, start_clipping);
		start_clipping = start_clipping->lo_any.next;
	}

	/*
	 * Loop through all the lines after the clipped line.
	 */
	end_last_line = NULL;
	for (i=0; i < (state->line_num - line - 1); i++)
	{
		LO_Element *line_start;
		LO_Element *lptr;

		/*
		 * Find the start of this line, and the start of
		 * the next line.
		 */
		eptr = lo_line_to_element(state, (line + i));
		end_ptr = lo_line_to_element(state, (line + 1 + i));
		if (eptr != NULL)
		{
			line_start = lo_duplicate_element(context, state,
						block_watch, eptr);
			lptr = line_start;
			eptr = eptr->lo_any.next;
		}
		else
		{
			line_start = NULL;
			lptr = NULL;
		}

		/*
		 * Set the start of this new line into the line array.
		 */
		lo_set_element_to_line(state, line_start, i);

		/*
		 * Copy all the elements in the line.
		 */
		while (eptr != end_ptr)
		{
			lptr->lo_any.y -= dy;
			if (lptr->type == LO_CELL)
			{
				lo_ShiftCell((LO_CellStruct *)lptr, 0, -dy);
			}
			else if (lptr->type == LO_IMAGE)
			{
				FE_ShiftImage(context, (LO_ImageStruct *)lptr);
			}

			lptr->lo_any.next = lo_duplicate_element(context, state,
							block_watch, eptr);
			if (lptr->lo_any.next == NULL)
			{
				break;
			}
			lptr->lo_any.next->lo_any.prev = lptr;
			lptr = lptr->lo_any.next;
			eptr = eptr->lo_any.next;
		}
		if (lptr != NULL)
		{
			lptr->lo_any.y -= dy;
			if (lptr->type == LO_CELL)
			{
				lo_ShiftCell((LO_CellStruct *)lptr, 0, -dy);
			}
			else if (lptr->type == LO_IMAGE)
			{
				FE_ShiftImage(context, (LO_ImageStruct *)lptr);
			}
			lptr->lo_any.next = NULL;
		}

		/*
		 * Link the beginning of the line we just copied to
		 * the end of the last line.
		 */
		line_start->lo_any.prev = end_last_line;
		if (end_last_line != NULL)
		{
			end_last_line->lo_any.next = line_start;
		}
		end_last_line = lptr;
		lptr->lo_any.next = NULL;
	}

	/*
	 * Clear the extra slots at the end.
	 */
	for (i = (state->line_num - line - 1); i < (state->line_num - 1); i++)
	{
		lo_set_element_to_line(state, NULL, i);
	}

	float_list = NULL;
	float_ptr = NULL;
	eptr = state->float_list;
	while (eptr != NULL)
	{
		/*
		 * Floating elements that move up off the top are deleted.
		 */
		if ((eptr->lo_any.y - dy) < 0)
		{
			lo_ScrapeElement(context, eptr);
			eptr = eptr->lo_any.next;
			continue;
		}

		if (float_list == NULL)
		{
			float_list = lo_duplicate_element(context, state,
					block_watch, eptr);
			float_ptr = float_list;
		}
		else
		{
			float_ptr->lo_any.next = lo_duplicate_element(context,
						state, block_watch, eptr);
			float_ptr = float_ptr->lo_any.next;
		}
		float_ptr->lo_any.y -= dy;
		if (float_ptr->type == LO_CELL)
		{
			lo_ShiftCell((LO_CellStruct *)float_ptr, 0, -dy);
		}
		else if (float_ptr->type == LO_IMAGE)
		{
			FE_ShiftImage(context, (LO_ImageStruct *)float_ptr);
		}

		eptr = eptr->lo_any.next;
	}
	if (float_ptr != NULL)
	{
		float_ptr->lo_any.next = NULL;
	}

	/*
	 * If there are blocked tags, one of them may be holded
	 * a partially fetched blocked image element, duplicate it.
	 */
	if (state->top_state->tags != NULL)
	{
		PA_Tag *tag_ptr;

		tag_ptr = state->top_state->tags;
		while (tag_ptr != NULL)
		{
			if ((tag_ptr->type == P_IMAGE)&&
				(tag_ptr->lo_data != NULL))
			{
				LO_Element *new_eptr;
				LO_Element *eptr;

				eptr = (LO_Element *)tag_ptr->lo_data;
				new_eptr = lo_duplicate_element(context, state,
                                                block_watch, eptr);
				tag_ptr->lo_data = (void *)new_eptr;
			}
			tag_ptr = tag_ptr->next;
		}
	}

	state->line_num -= line;
	state->y -= dy;
	state->end_last_line = end_last_line;
	state->float_list = float_list;
	state->current_ele = NULL;

	lo_ShiftMarginsUp(context, state, dy);

	/*
	 * If we had a blocking element that didn't get duplicated, we don't
	 * know what happened to it.
	 */
	if (block_watch != -1)
	{
		state->top_state->layout_blocking_element = NULL;
	}

#ifdef MEMORY_ARENAS	
	(void)lo_FreeMemoryArena(old_first_arena);
#endif /* MEMORY_ARENAS	*/

}

