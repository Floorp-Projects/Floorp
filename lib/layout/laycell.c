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
#include "laylayer.h"
#include "layers.h"

#ifdef PROFILE
#pragma profile on
#endif

#ifdef TEST_16BIT
#define XP_WIN16
#endif /* TEST_16BIT */

void
lo_DisplayCellContents(MWContext *context, LO_CellStruct *cell,
					   int32 base_x, int32 base_y,
					   int32 x, int32 y,
					   uint32 width, uint32 height)
{
	LO_Element *eptr;

	eptr = cell->cell_list;
	while (eptr)
	{
		lo_DisplayElement(context, eptr, base_x, base_y, x, y, width, height);
		eptr = eptr->lo_any.next;
	}

	eptr = cell->cell_float_list;
	while (eptr)
	{
		lo_DisplayElement(context, eptr, base_x, base_y, x, y, width, height);
		eptr = eptr->lo_any.next;
	}
}


PRIVATE
LO_Element *
lo_search_element_list(MWContext *context, LO_Element *eptr, int32 x, int32 y)
{
	while (eptr != NULL)
	{
		int32 width, height;

		width = eptr->lo_any.width;
		/*
		 * Images need to account for border width
		 */
		if (eptr->type == LO_IMAGE)
		{
			width = width + (2 * eptr->lo_image.border_width);
		}
		if (width <= 0)
		{
			width = 1;
		}

		height = eptr->lo_any.height;
		/*
		 * Images need to account for border width
		 */
		if (eptr->type == LO_IMAGE)
		{
			height = height + (2 * eptr->lo_image.border_width);
		}

		if ((y < (eptr->lo_any.y + eptr->lo_any.y_offset + height))&&
			(y >= eptr->lo_any.y))
		{
			if ((x < (eptr->lo_any.x + eptr->lo_any.x_offset +
				width))&&(x >= eptr->lo_any.x))
			{
				/*
				 * Don't stop on tables, they are containers
				 * we must look inside of.
				 */
				if (eptr->type != LO_TABLE)
				{
					break;
				}
			}
		}
		eptr = eptr->lo_any.next;
	}
	return(eptr);
}


int32
lo_GetCellBaseline(LO_CellStruct *cell)
{
	LO_Element *eptr;

	if (cell == NULL)
	{
		return(0);
	}

	/*
	 * Make eptr point to the start of the element chain
	 * for this subdoc.
	 */
	if (cell->cell_list == NULL)
	{
		return(0);
	}

	eptr = cell->cell_list;

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


LO_Element *
lo_XYToCellElement(MWContext *context, lo_DocState *state,
	LO_CellStruct *cell, int32 x, int32 y,
	Bool check_float, Bool into_cells, Bool into_ilayers)
{
	LO_Element *eptr;
	LO_Element *element;

	eptr = cell->cell_list;
	element = lo_search_element_list(context, eptr, x, y);

	if ((element == NULL)&&(check_float != FALSE))
	{
		eptr = cell->cell_float_list;
		element = lo_search_element_list(context, eptr, x, y);
	}

	if ((element != NULL)&&(element->type == LO_SUBDOC))
	{
		LO_SubDocStruct *subdoc;
		int32 new_x, new_y;
		int32 ret_new_x, ret_new_y;

		subdoc = (LO_SubDocStruct *)element;

		new_x = x - (subdoc->x + subdoc->x_offset +
			subdoc->border_width);
		new_y = y - (subdoc->y + subdoc->y_offset +
			subdoc->border_width);
		element = lo_XYToDocumentElement(context,
			(lo_DocState *)subdoc->state, new_x, new_y, 
                        check_float, into_cells, into_ilayers, 
                        &ret_new_x, &ret_new_y);
		x = ret_new_x;
		y = ret_new_y;
	}
	else if ((element != NULL)&&(element->type == LO_CELL)&&
		 (into_cells != FALSE))
	{
        if (!element->lo_cell.cell_inflow_layer || into_ilayers)
            element = lo_XYToCellElement(context, state,
                         (LO_CellStruct *)element, x, y,
                         check_float, into_cells, into_ilayers);
	}

	return(element);
}


LO_Element *
lo_XYToNearestCellElement(MWContext *context, lo_DocState *state,
	LO_CellStruct *cell, int32 x, int32 y)
{
	LO_Element *eptr, *eptrsave;
    int32 x2;
	/* int32 ysave; */

	eptr = lo_XYToCellElement(context, state, cell, x, y, TRUE, TRUE, TRUE);
    eptrsave = NULL;

	/*
	 * If no exact hit, find nearest in Y (and then X) overlap.
	 */
	if (eptr == NULL)
	{
		eptr = cell->cell_list;
		while (eptr != NULL)
		{
			int32 y2;

			/*
			 * Skip tables, we just walk into them.
			 */
			if (eptr->type == LO_TABLE)
			{
				eptr = eptr->lo_any.next;
				continue;
			}

			y2 = eptr->lo_any.y + eptr->lo_any.y_offset +
				eptr->lo_any.height;
			/*
			 * Images and cells need to account for border width
			 */
			if (eptr->type == LO_IMAGE)
			{
				y2 = y2 + (2 * eptr->lo_image.border_width);
			}
			else if (eptr->type == LO_CELL)
			{
				y2 = y2 + (2 * eptr->lo_cell.border_width);
			}

            /* 
             * If we've already found something that's greater in y,
             * let's look for something that's closer in x.
             */
            if (eptrsave) 
            {
                /* 
                 * If the current y is larger than that of the saved element 
                 * we'll just take the saved element.
                 */
                /* if (y2 > ysave)
                    break; */

                eptrsave = eptr;
                /* ysave = y2; */

                /* Calculate the x extent of the element */
                x2 = eptr->lo_any.x + eptr->lo_any.x_offset +
                    eptr->lo_any.width;
                if (eptr->type == LO_IMAGE)
                    x2 = x2 + (2 * eptr->lo_image.border_width);
                else if (eptr->type == LO_CELL)
                    x2 = x2 + (2 * eptr->lo_cell.border_width);

                /* 
                 * If we've gone too far in x, we'll take the currently
                 * saved element.
                 */
                if (x <= x2) 
                    break;
            }
			else if (y <= y2)
			{
                /* 
                 * This is the first element that's greater in y, so
                 * save it and look out for others that are closer in
                 * x, but identical in y.
                 */
                eptrsave = eptr;
                /* ysave = y2; */

				/* Calculate the x extent of the element */
                x2 = eptr->lo_any.x + eptr->lo_any.x_offset +
                    eptr->lo_any.width;
                if (eptr->type == LO_IMAGE)
                    x2 = x2 + (2 * eptr->lo_image.border_width);
                else if (eptr->type == LO_CELL)
                    x2 = x2 + (2 * eptr->lo_cell.border_width);

                /* 
                 * If we've gone too far in x, we'll take the currently
                 * saved element.
                 */
                if (x <= x2) 
                    break;
			}
			eptr = eptr->lo_any.next;
		}

        eptr = eptrsave;
        
		/*
		 * If nothing overlaps y, match the last element in the cell.
		 */
		if (eptr == NULL)
		{
			eptr = cell->cell_list_end;
		}

		/*
		 * If we matched on a cell, recurse into it.
		 */
		if ((eptr != NULL)&&(eptr->type == LO_CELL))
		{
			eptr = lo_XYToNearestCellElement(context, state,
				(LO_CellStruct *)eptr, x, y);
		}
	}
	return(eptr);
}


static void lo_MoveElementLayers( LO_Element *eptr )
{
	if (eptr->type == LO_IMAGE)
		
	{
		CL_MoveLayer(eptr->lo_image.layer, 
			eptr->lo_any.x + eptr->lo_any.x_offset + eptr->lo_image.border_width, 
			eptr->lo_any.y + eptr->lo_any.y_offset + eptr->lo_image.border_width);
	}
	else if (eptr->type == LO_EMBED)
	{
		CL_MoveLayer(eptr->lo_embed.layer, 
			eptr->lo_any.x + eptr->lo_any.x_offset + eptr->lo_embed.border_width, 
			eptr->lo_any.y + eptr->lo_any.y_offset + eptr->lo_embed.border_width);

	}
#ifdef JAVA
	else if (eptr->type == LO_JAVA)
	{
		CL_MoveLayer(eptr->lo_java.layer, 
			eptr->lo_any.x + eptr->lo_any.x_offset + eptr->lo_java.border_width, 
			eptr->lo_any.y + eptr->lo_any.y_offset + eptr->lo_java.border_width);
		
	}
#endif
	else if (eptr->type == LO_FORM_ELE)
	{
		CL_MoveLayer(eptr->lo_form.layer, 
			eptr->lo_any.x + eptr->lo_any.x_offset + eptr->lo_form.border_horiz_space, 
			eptr->lo_any.y + eptr->lo_any.y_offset + eptr->lo_form.border_vert_space);
	}
}

void
lo_ShiftCell(LO_CellStruct *cell, int32 dx, int32 dy)
{
	LO_Element *eptr;

	if (cell == NULL)
	{
		return;
	}

        if (cell->cell_bg_layer) {
            int32 x_offset, y_offset;
            lo_GetLayerXYShift(CL_GetLayerParent(cell->cell_bg_layer),
                               &x_offset, &y_offset);
            CL_MoveLayer(cell->cell_bg_layer,
                         cell->x - x_offset, cell->y - y_offset);
			CL_ResizeLayer(cell->cell_bg_layer, cell->width, cell->height);
        }
        if (cell->cell_inflow_layer)
            lo_OffsetInflowLayer(cell->cell_inflow_layer, dx, dy);

	eptr = cell->cell_list;
	while (eptr != NULL)
	{
		eptr->lo_any.x += dx;
		eptr->lo_any.y += dy;
		if (eptr->type == LO_CELL)
		{
			/*
			 * This would cause an infinite loop.
			 */
			if ((LO_CellStruct *)eptr == cell)
			{
				XP_ASSERT(FALSE);
				break;
			}
			lo_ShiftCell((LO_CellStruct *)eptr, dx, dy);
		}

		lo_MoveElementLayers( eptr );

		eptr = eptr->lo_any.next;
	}

	eptr = cell->cell_float_list;
	while (eptr != NULL)
	{
		eptr->lo_any.x += dx;
		eptr->lo_any.y += dy;
		if (eptr->type == LO_CELL)
		{
			lo_ShiftCell((LO_CellStruct *)eptr, dx, dy);
		}

		lo_MoveElementLayers( eptr );

		eptr = eptr->lo_any.next;
	}
}


static void
lo_recolor_backgrounds(lo_DocState *state, LO_CellStruct *cell,
	LO_Element *eptr)
{
	LO_TextAttr *old_attr;
	LO_TextAttr *attr;
	LO_TextAttr tmp_attr;

	if (cell->backdrop.bg_color == NULL)
	{
		return;
	}

	attr = NULL;
	old_attr = NULL;
	if (eptr->type == LO_TEXT)
	{
		old_attr = eptr->lo_text.text_attr;
	}
	else if (eptr->type == LO_LINEFEED)
	{
		old_attr = eptr->lo_linefeed.text_attr;
	}
	else if (eptr->type == LO_IMAGE)
	{
		old_attr = eptr->lo_image.text_attr;
	}
	else if (eptr->type == LO_FORM_ELE)
	{
		old_attr = eptr->lo_form.text_attr;
	}

	if (old_attr != NULL)
	{
		lo_CopyTextAttr(old_attr, &tmp_attr);

        /* don't recolor the background if it was
         * explicitly set to another color 
         */
        if(tmp_attr.no_background == TRUE)
		    tmp_attr.bg = *cell->backdrop.bg_color;

		/* removed by Lou 4-3-97 to allow overlapping
		 * text in tables to work right.  Windows
		 * never before payed attention to this attr
		 * so I'm guessing that it won't break anything :)
		 */
		/* tmp_attr.no_background = FALSE; */

		attr = lo_FetchTextAttr(state, &tmp_attr);
	}

	if (eptr->type == LO_TEXT)
	{
		eptr->lo_text.text_attr = attr;
	}
	else if (eptr->type == LO_LINEFEED)
	{
		eptr->lo_linefeed.text_attr = attr;
	}
	else if (eptr->type == LO_IMAGE)
	{
		eptr->lo_image.text_attr = attr;
	}
	else if (eptr->type == LO_FORM_ELE)
	{
		eptr->lo_form.text_attr = attr;
	}
}


/*
 * In addition to renumbering the cell, we now
 * also make sure that text in cell with a bg_color
 * have their text_attr->bg set correctly.
 */
void
lo_RenumberCell(lo_DocState *state, LO_CellStruct *cell)
{
	LO_Element *eptr;

	if (cell == NULL)
	{
		return;
	}

	eptr = cell->cell_list;
	while (eptr != NULL)
	{
		/* Keep element IDs in order */
		eptr->lo_any.ele_id = NEXT_ELEMENT;

		/*
		 * If the display is blocked for an element
		 * we havn't reached yet, check to see if
		 * this element is it, and if so, save its
		 * y position.
		 */
		if ((state->display_blocked != FALSE)&&
		    (state->is_a_subdoc == SUBDOC_NOT)&&
		    (state->display_blocking_element_y == 0)&&
		    (state->display_blocking_element_id != -1)&&
		    (eptr->lo_any.ele_id >=
			state->display_blocking_element_id))
		{
			state->display_blocking_element_y = eptr->lo_any.y;
		}

		/*
		 * If this is a cell with a BGCOLOR attribute, pass
		 * that color on to the bg of all text in this cell.
		 */
		if ((cell->backdrop.bg_color != NULL)&&((eptr->type == LO_TEXT)||
		    (eptr->type == LO_LINEFEED)||(eptr->type == LO_IMAGE)||
		    (eptr->type == LO_FORM_ELE)))
		{
			lo_recolor_backgrounds(state, cell, eptr);
		}

		if (eptr->type == LO_CELL)
		{
			lo_RenumberCell(state, (LO_CellStruct *)eptr);
		}

		eptr = eptr->lo_any.next;
	}

	eptr = cell->cell_float_list;
	while (eptr != NULL)
	{
		/* Keep element IDs in order */
		eptr->lo_any.ele_id = NEXT_ELEMENT;

		/*
		 * If the display is blocked for an element
		 * we havn't reached yet, check to see if
		 * this element is it, and if so, save its
		 * y position.
		 */
		if ((state->display_blocked != FALSE)&&
		    (state->is_a_subdoc == SUBDOC_NOT)&&
		    (state->display_blocking_element_y == 0)&&
		    (state->display_blocking_element_id != -1)&&
		    (eptr->lo_any.ele_id >=
			state->display_blocking_element_id))
		{
			state->display_blocking_element_y = eptr->lo_any.y;
		}

		/*
		 * If this is a cell with a BGCOLOR attribute, pass
		 * that color on to the bg of all text in this cell.
		 */
		if ((cell->backdrop.bg_color != NULL)&&((eptr->type == LO_TEXT)||
		    (eptr->type == LO_LINEFEED)||(eptr->type == LO_IMAGE)||
		    (eptr->type == LO_FORM_ELE)))
		{
			lo_recolor_backgrounds(state, cell, eptr);
		}

		if (eptr->type == LO_CELL)
		{
			lo_RenumberCell(state, (LO_CellStruct *)eptr);
		}

		eptr = eptr->lo_any.next;
	}
}


static void
lo_free_subdoc_state(MWContext *context, lo_DocState *parent_state,
			lo_DocState *state, Bool free_all)
{
	LO_Element **line_array;

	if (state == NULL)
	{
		return;
	}

	/*
	 * If this subdoc is not nested inside another subdoc,
	 * free any re-parse tags stored on this subdoc.
	 */
	if ((state->subdoc_tags != NULL)&&
		(parent_state->is_a_subdoc != SUBDOC_CELL)&&
		(parent_state->is_a_subdoc != SUBDOC_CAPTION))
	{
		PA_Tag *tptr;
		PA_Tag *tag;

		tptr = state->subdoc_tags;
		while ((tptr != state->subdoc_tags_end)&&(tptr != NULL))
		{
			tag = tptr;
			tptr = tptr->next;
			tag->next = NULL;
			PA_FreeTag(tag);
		}
		if (tptr != NULL)
		{
			tptr->next = NULL;
			PA_FreeTag(tptr);
		}
		state->subdoc_tags = NULL;
		state->subdoc_tags_end = NULL;
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
		lo_FreeElementList(context, state->line_list);
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

	if (state->line_buf != NULL)
	{
		PA_FREE(state->line_buf);
		state->line_buf = NULL;
	}

	if (state->float_list != NULL)
	{
		/*
		 * Don't free these, they are squished into the cell.
		 * Only free them if this is a free_all.
		 */

		if (free_all != FALSE)
		{
			lo_FreeElementList(context, state->float_list);
		}

		state->float_list = NULL;
	}

	if (state->line_array != NULL)
	{
		LO_Element *eptr;

#ifdef XP_WIN16
{
		XP_Block *larray_array;
		intn cnt;

		XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
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
		if (eptr != NULL)
		{
			/*
			 * Don't free these, they are squished into the cell.
			 * Only free them if this is a free_all.
			 */
			if (free_all != FALSE)
			{
				lo_FreeElementList(context, eptr);
			}

		}
		XP_UNLOCK_BLOCK(state->line_array);
		XP_FREE_BLOCK(state->line_array);
	}

	XP_DELETE(state);
}



void
lo_FreePartialSubDoc(MWContext *context, lo_DocState *state,
	LO_SubDocStruct *subdoc)
{
	lo_DocState *subdoc_state;

	if (subdoc == NULL)
	{
		return;
	}

	subdoc_state = (lo_DocState *)subdoc->state;
	if (subdoc_state != NULL)
	{
		lo_free_subdoc_state(context, state, subdoc_state, TRUE);
	}

	subdoc->state = NULL;
	lo_FreeElement(context, (LO_Element *)subdoc, TRUE);
}


void
lo_FreePartialCell(MWContext *context, lo_DocState *state,
	LO_CellStruct *cell)
{
	if (cell == NULL)
	{
		return;
	}

	lo_FreeElement(context, (LO_Element *)cell, TRUE);
}


LO_CellStruct *
lo_SmallSquishSubDocToCell(MWContext *context, lo_DocState *state,
	LO_SubDocStruct *subdoc, int32 *ptr_dx, int32 *ptr_dy)
{
	LO_CellStruct *cell;
	/* lo_DocState *subdoc_state; */

	if (subdoc == NULL)
	{
		return(NULL);
	}

	cell = (LO_CellStruct *)lo_NewElement(context, state, LO_CELL, NULL, 0);
	cell->isCaption = FALSE;

    cell->cell_bg_layer = NULL;
    cell->cell_inflow_layer = NULL;

	lo_CreateCellFromSubDoc (context, state, subdoc, cell, ptr_dx, ptr_dy );

	return(cell);


	/* Copied to lo_CreateCellFromSubDoc()
	 *
	*ptr_dx = 0;
	*ptr_dy = 0;

	if (cell == NULL)
	{
		return(NULL);
	}

	cell->type = LO_CELL;

	cell->ele_id = NEXT_ELEMENT;
	cell->x = subdoc->x;
	cell->x_offset = subdoc->x_offset;
	cell->y = subdoc->y;
	cell->y_offset = subdoc->y_offset;
	cell->width = subdoc->width;
	cell->height = subdoc->height;
	cell->next = subdoc->next;
	cell->prev = subdoc->prev;
	cell->FE_Data = subdoc->FE_Data;

	cell->backdrop = subdoc->backdrop;
	*/

	/*
	 * Clear out the bg_color struct from the subdocs so it doesn't
	 * get freed twice.
	 */
	/*
	subdoc->backdrop.bg_color = NULL;
        subdoc->backdrop.url = NULL;

        cell->cell_bg_layer = NULL;
        cell->cell_inflow_layer = NULL;

	/*
	cell->border_width = subdoc->border_width;
	cell->border_vert_space = subdoc->border_vert_space;
	cell->border_horiz_space = subdoc->border_horiz_space;

	cell->ele_attrmask = 0;
	cell->ele_attrmask |= (subdoc->ele_attrmask & LO_ELE_SECURE);
	cell->ele_attrmask |= (subdoc->ele_attrmask & LO_ELE_SELECTED);

	cell->sel_start = subdoc->sel_start;
	cell->sel_end = subdoc->sel_end;

	cell->cell_list = NULL;
	cell->cell_list_end = NULL;
	cell->cell_float_list = NULL;
	subdoc_state = (lo_DocState *)subdoc->state;
	if (subdoc_state != NULL)
	{
		LO_Element **line_array;
		LO_Element *eptr;
		int32 base_x, base_y;
	*/
		/*
		 * Make eptr point to the start of the element chain
		 * for this subdoc.
		 */
	/*
#ifdef XP_WIN16
		{
			XP_Block *larray_array;

			XP_LOCK_BLOCK(larray_array, XP_Block *,
				subdoc_state->larray_array);
			subdoc_state->line_array = larray_array[0];
			XP_UNLOCK_BLOCK(subdoc_state->larray_array);
		}
#endif */ /* XP_WIN16 */
	/*
		XP_LOCK_BLOCK(line_array, LO_Element **,
			subdoc_state->line_array);
		eptr = line_array[0];
		XP_UNLOCK_BLOCK(subdoc_state->line_array);

		cell->cell_list = eptr;

		base_x = subdoc->x + subdoc->x_offset + subdoc->border_width;
		base_y = subdoc->y + subdoc->y_offset + subdoc->border_width;

		cell->cell_list_end = subdoc_state->end_last_line;
		if (cell->cell_list_end == NULL)
		{
			cell->cell_list_end = cell->cell_list;
		}

		cell->cell_float_list = subdoc_state->float_list;
                        
		*ptr_dx = base_x;
		*ptr_dy = base_y;

	*/
/*
 * MOCHA needs to reuse mocha objects allocated in the namelist
 * when a cell is relaid out.  This means we must always squish
 * back up to the top level state, instead of propogating up
 * like we used to.
 */

	/*
#ifdef MOCHA
		lo_AddNameList(state->top_state->doc_state, subdoc_state);
#else
		lo_AddNameList(state, subdoc_state);
#endif */ /* MOCHA */
	/*
	}

	return(cell);
	*/
}


LO_CellStruct *
lo_SquishSubDocToCell(MWContext *context, lo_DocState *state,
	LO_SubDocStruct *subdoc, Bool free_subdoc)
{
	LO_CellStruct *cell;
	int32 dx, dy;

	dx = 0;
	dy = 0;
	
	cell = lo_SmallSquishSubDocToCell(context, state, subdoc, &dx, &dy);

	if (cell == NULL)
	{
		return(NULL);
	}

	lo_ShiftCell(cell, dx, dy);
	lo_RenumberCell(state, cell);

	if ((subdoc->state != NULL)&&(free_subdoc != FALSE))
	{
		lo_DocState *subdoc_state;

		subdoc_state = (lo_DocState *)subdoc->state;
		lo_free_subdoc_state(context, state, subdoc_state, FALSE);
		subdoc->state = NULL;
	}
	if (free_subdoc != FALSE)
	{
		lo_FreeElement(context, (LO_Element *)subdoc, free_subdoc);
	}

	return(cell);
}


/* Used during layout and relayout and called from lo_SmallSquishSubdocToCell() */
void lo_CreateCellFromSubDoc( MWContext *context, lo_DocState *state,
	LO_SubDocStruct *subdoc, LO_CellStruct *cell, int32 *ptr_dx, int32 *ptr_dy )
{
	lo_DocState *subdoc_state;

	/* Fix this later */
	if (subdoc == NULL)
	{
		return;
	}

	*ptr_dx = 0;
	*ptr_dy = 0;

	/* Fix this later */
	if (cell == NULL)
	{
		return;
	}

	cell->type = LO_CELL;

	cell->ele_id = NEXT_ELEMENT;
	cell->x = subdoc->x;
	cell->x_offset = subdoc->x_offset;
	cell->y = subdoc->y;
	cell->y_offset = subdoc->y_offset;
	cell->width = subdoc->width;
	cell->height = subdoc->height;
	cell->next = subdoc->next;
	cell->prev = subdoc->prev;
	cell->FE_Data = subdoc->FE_Data;

	cell->backdrop = subdoc->backdrop;

	/*
	 * Clear out the bg_color struct from the subdocs so it doesn't
	 * get freed twice.
	 */

	subdoc->backdrop.bg_color = NULL;
        subdoc->backdrop.url = NULL;

	cell->border_width = subdoc->border_width;
	cell->border_vert_space = subdoc->border_vert_space;
	cell->border_horiz_space = subdoc->border_horiz_space;

	cell->ele_attrmask = 0;
	cell->ele_attrmask |= (subdoc->ele_attrmask & LO_ELE_SECURE);
	cell->ele_attrmask |= (subdoc->ele_attrmask & LO_ELE_SELECTED);

	cell->sel_start = subdoc->sel_start;
	cell->sel_end = subdoc->sel_end;

	cell->cell_list = NULL;
	cell->cell_list_end = NULL;
	cell->cell_float_list = NULL;
	subdoc_state = (lo_DocState *)subdoc->state;
	if (subdoc_state != NULL)
	{
		LO_Element **line_array;
		LO_Element *eptr;
		int32 base_x, base_y;

		/*
		 * Make eptr point to the start of the element chain
		 * for this subdoc.
		 */
#ifdef XP_WIN16
		{
			XP_Block *larray_array;

			XP_LOCK_BLOCK(larray_array, XP_Block *,
				subdoc_state->larray_array);
			subdoc_state->line_array = larray_array[0];
			XP_UNLOCK_BLOCK(subdoc_state->larray_array);
		}
#endif /* XP_WIN16 */
		XP_LOCK_BLOCK(line_array, LO_Element **,
			subdoc_state->line_array);
		eptr = line_array[0];
		XP_UNLOCK_BLOCK(subdoc_state->line_array);

		cell->cell_list = eptr;

		base_x = subdoc->x + subdoc->x_offset + subdoc->border_width;
		base_y = subdoc->y + subdoc->y_offset + subdoc->border_width;

		cell->cell_list_end = subdoc_state->end_last_line;
		if (cell->cell_list_end == NULL)
		{
			cell->cell_list_end = cell->cell_list;
		}

		cell->cell_float_list = subdoc_state->float_list;
                        
		*ptr_dx = base_x;
		*ptr_dy = base_y;

/*
 * MOCHA needs to reuse mocha objects allocated in the namelist
 * when a cell is relaid out.  This means we must always squish
 * back up to the top level state, instead of propogating up
 * like we used to.
 */
#ifdef MOCHA
		lo_AddNameList(state->top_state->doc_state, subdoc_state);
#else
		lo_AddNameList(state, subdoc_state);
#endif /* MOCHA */
	}
}

#ifdef TEST_16BIT
#undef XP_WIN16
#endif /* TEST_16BIT */

#ifdef PROFILE
#pragma profile off
#endif
