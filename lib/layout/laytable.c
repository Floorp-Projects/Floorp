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
#include "np.h"
#include "laystyle.h"
#include "edt.h"

#ifdef TEST_16BIT
#define XP_WIN16
#endif /* TEST_16BIT */


#define	TABLE_BORDERS_ON		1
#define	TABLE_BORDERS_OFF		0
#define	TABLE_BORDERS_GONE		-1

#define	TABLE_DEF_INNER_CELL_PAD	1
#define	TABLE_DEF_INTER_CELL_PAD	2
#define	TABLE_DEF_CELL_BORDER		1

#define	TABLE_DEF_BORDER		0
#define TABLE_DEF_BORDER_STYLE		BORDER_SOLID
#define	TABLE_DEF_VERTICAL_SPACE	0
#define	TABLE_DEF_HORIZONTAL_SPACE	0

#define	TABLE_MAX_COLSPAN		1000
#define	TABLE_MAX_ROWSPAN		10000

typedef struct lo_cell_data_struct {
#ifdef XP_WIN16
	/*
	 * This struct must be a power of 2 if we are going to allocate an
	 * array of more than 128K
	 */
	unsigned width, height;
#else
	int32 width, height;
#endif
	lo_TableCell *cell;
} lo_cell_data;


LO_SubDocStruct *
	lo_EndCellSubDoc(MWContext *context, lo_DocState *state, lo_DocState *old_state,
		LO_Element *element, Bool relayout);
LO_SubDocStruct *
	lo_RelayoutCaptionSubdoc(MWContext *context, lo_DocState *state, lo_TableCaption *caption,
		LO_SubDocStruct *subdoc, int32 width, Bool is_a_header);
LO_CellStruct *
lo_RelayoutCell(MWContext *context, lo_DocState *state,
	LO_SubDocStruct *subdoc, lo_TableCell *cell_ptr,
	LO_CellStruct *cell, int32 width, Bool is_a_header,
	Bool relayout);
void
	lo_BeginCaptionSubDoc(MWContext *context, lo_DocState *state,
		lo_TableCaption *caption, PA_Tag *tag);
void
	lo_BeginCellSubDoc(MWContext *context, 
 					lo_DocState *state,
 					lo_TableRec *table,
 					char *bgcolor_attr,
 					char *background_attr,
                    lo_TileMode tile_mode,
 					char *valign_attr,
 					char *halign_attr,
 					char *width_attr,
 					char *height_attr,
 					Bool is_a_header, 
 					intn draw_borders,
					Bool relayout);
Bool
	lo_subdoc_has_elements(lo_DocState *sub_state);
Bool
	lo_cell_has_elements(LO_CellStruct *cell);
int32
	lo_align_subdoc(MWContext *context, lo_DocState *state, lo_DocState *old_state,
		LO_SubDocStruct *subdoc, lo_TableRec *table, lo_table_span *row_max);
int32
	lo_align_cell(MWContext *context, lo_DocState *state, lo_TableCell *cell_ptr,
		LO_CellStruct *cell, lo_TableRec *table, lo_table_span *row_max);

#if 0	/* No longer used after Mariner */
void
	lo_RelayoutTags(MWContext *context, lo_DocState *state, PA_Tag * tag_ptr,
		PA_Tag * tag_end_ptr, LO_Element * elem_list);
#endif
	
PA_Tag *
	lo_FindReuseableElement(MWContext *context, lo_DocState *state,
		LO_Element ** elem_list);

static void
lo_reuse_current_state(MWContext *context, lo_DocState *state,
	int32 width, int32 height, int32 margin_width, int32 margin_height,
	Bool destroyLineLists);
/*
 ********************************************************************************
 * Some Helper Functions
 */
 
static int32 lo_CalculateCellPercentWidth( lo_TableRec *table, int32 percent);
static int32 lo_CalculateCellPercentHeight( lo_TableRec *table, lo_TableCell *table_cell);

static int32
	lo_ComputeCellEmptySpace(MWContext *context,lo_TableRec *table,
		LO_SubDocStruct *subdoc);
static int32
	lo_FindDefaultCellWidth(MWContext * context, lo_TableRec *table, lo_TableCell *table_cell,
		int32 desired_width, int32 cell_empty_space);
static int32
	lo_ComputeInternalTableWidth(MWContext *context, lo_TableRec *table,
		lo_DocState *state);
static void
	lo_SetDefaultCellWidth(MWContext *context, lo_TableRec *table, LO_SubDocStruct *subdoc,
		lo_TableCell *table_cell, int32 cell_width);


static void lo_ResetWidthSpans( lo_TableRec *table );

static void lo_UpdateCaptionCellFromSubDoc( MWContext *context, lo_DocState *state, LO_SubDocStruct *subdoc, LO_CellStruct *cell_ele);
static void lo_FreeCaptionCell( MWContext *context, lo_DocState *state, LO_CellStruct *cell_ele);
static void lo_FreeTableCaption( MWContext *context, lo_DocState *state, lo_TableRec *table );

static int32
lo_ComputeCellEmptySpace(MWContext *context, lo_TableRec *table,
	LO_SubDocStruct *subdoc)
{
	int32 left_inner_pad;
	int32 right_inner_pad;
	int32 cell_empty_space;
	
	left_inner_pad = FEUNITS_X(table->inner_left_pad, context);
	right_inner_pad = FEUNITS_X(table->inner_right_pad, context);
	
	cell_empty_space =	(2 * subdoc->border_width) +
						(2 * subdoc->border_horiz_space) +
						(left_inner_pad + right_inner_pad);
	
	return cell_empty_space;
}


/*
 * Handle Fixed Table Layout. If the table has a COLS attribute
 * then the width of this cell may already be specified. The
 * trick is to know if this is the first row of the table. If
 * so, then this cell can choose it's width.
 */
static int32
lo_FindDefaultCellWidth(MWContext * context, lo_TableRec *table, lo_TableCell *table_cell,
	int32 desired_width, int32 cell_empty_space)
{
	lo_TableRow * row_ptr;
	int32 cell_num;
	int32 cell_width;
	int32 count;
	int32 cell_pad;
		
	cell_pad = FEUNITS_X(table->inter_cell_pad, context);
	
	row_ptr = table->row_ptr;
	cell_num = row_ptr->cells;
	
	/* if we don't have a width table or we're out of bounds, bail */
	if ( table->fixed_col_widths == NULL ||
		((cell_num+table_cell->colspan) > table->fixed_cols) )
	{
		table->fixed_cols = 0;
		table->default_cell_width = 0;
		return desired_width;
	}
	
	/* does this cell already have a width defined? */
	cell_width = table->fixed_col_widths[ cell_num ];
	if ( cell_width == 0 )
	{
		int32 col_width;
		
		/* use it's desired width or the default */
		cell_width = desired_width;
		if ( cell_width == 0 )
		{
			/*
			 * Get our size from the default cell width. We
			 * may need to clip it to the table width remaining.
			 */
			cell_width = table->default_cell_width * table_cell->colspan;
			
			/*
			 * If this is bigger than the width remaining the the table, then
			 * clip to the remaining space. If the table is already full, then
			 * we're going for broke and the cell gets all of it.
			 */
			if ( ( table->fixed_width_remaining > 0 ) &&
				( cell_width > table->fixed_width_remaining ) )
			{
				cell_width = table->fixed_width_remaining;
			}
	
			table->fixed_width_remaining -= cell_width;
			
			/*
			 * Remove any cell spacing so the cell will internally
			 * layout to the correct size.
			 */
			cell_width -= cell_empty_space * table_cell->colspan;
		}
		else
		{
			/*
			 * The cell has a specified size. Allocate all of it. If we
			 * don't have enough space, then we'll keep doing fixed layout,
			 * but the table may end up larger than expected.
			 */
			table->fixed_width_remaining -= cell_width + cell_empty_space;
		}
		
		/* make sure this stays reasonable */
		if ( table->fixed_width_remaining < 0 )
		{
			table->fixed_width_remaining = 0;
		}
					
		/* fill out the apropriate number of col widths */
		col_width = cell_width / table_cell->colspan;
		for ( count = table_cell->colspan; count > 0 ; --count )
		{
			table->fixed_col_widths[ cell_num ] = col_width;
			cell_num++;
		}
		
		/* add any odd amount to the last column */
		table->fixed_col_widths[ cell_num-1 ] += (cell_width-
			(col_width * table_cell->colspan));
	}
	else
	{		
		/* add up widths of each column our cell spans */
		for ( count = 1; count < table_cell->colspan; ++count )
		{
			cell_width += table->fixed_col_widths[ cell_num+count ];
		}
	}
			
	/*
	 * Add the empty space this cell spans
	 */
	cell_width += ( table_cell->colspan - 1 ) * ( cell_empty_space + cell_pad );
	
	return cell_width;
}


/*
 * For fixed layout, set the cell width for a cell after layout. This is used
 * for cases where we don't know how wide a cell is until after it's been layed
 * out (for example, when NOWRAP is set).
 *
 * We will have already chosen a default width for this cell. If our new width
 * is larger, then we need to resize the column/s.
 */
static void
lo_SetDefaultCellWidth(MWContext *context, lo_TableRec *table, LO_SubDocStruct *subdoc,
	lo_TableCell *table_cell, int32 cell_width)
{
	lo_TableRow * row_ptr;
	int32 cell_num;
	int32 count;
	int32 cur_width;
	int32 add_width;
	int32 cell_empty_space;
	int32 cell_pad;
			
	row_ptr = table->row_ptr;
	cell_num = row_ptr->cells;
	
	/* if we don't have a width table or we're out of bounds, bail */
	if ( table->fixed_col_widths == NULL ||
		((cell_num+table_cell->colspan) > table->fixed_cols) )
	{
		table->fixed_cols = 0;
		table->default_cell_width = 0;
		return;
	}
	
	/*
	 * Figure out the current width for these cells
	 */
	cur_width = 0;
	
	for ( count = table_cell->colspan-1; count >= 0 ; --count )
	{
		cur_width += table->fixed_col_widths[ cell_num+count ];
	}
	
	/*
	 * Add space for the empty space that this cell spans
	 */
	cell_empty_space = lo_ComputeCellEmptySpace(context, table, subdoc );
	cell_pad = FEUNITS_X(table->inter_cell_pad, context);

	cur_width += ( table_cell->colspan - 1 ) * ( cell_empty_space + cell_pad );
	
	/*
	 * If our new width is bigger, we need to grow the default width
	 */
	if ( cell_width > cur_width )
	{
		cell_width -= cur_width;
		add_width = cell_width / table_cell->colspan;
		
		table->fixed_width_remaining -= add_width;
		if ( table->fixed_width_remaining < 0 )
		{
			table->fixed_width_remaining = 0;
		}
		
		for ( count = table_cell->colspan; count > 0 ; --count )
		{
			table->fixed_col_widths[ cell_num ] += add_width;
			cell_width -= add_width;
			cell_num++;
		}
		
		/* add any odd amount to the last column */
		table->fixed_col_widths[ cell_num-1 ] += cell_width;
	}
}

static int32
lo_ComputeInternalTableWidth(MWContext *context,lo_TableRec *table,
	lo_DocState *state)
{
	int32 table_width;
	int32 cell_pad;
	int32 table_pad;
	
	cell_pad = FEUNITS_X(table->inter_cell_pad, context);
	
	table_width = table->width;
	if ( table_width == 0 )
	{
		/* Compute our starting width as the window width */
		table_width = (state->win_width - state->win_left -
			state->win_right);
	}
	
	/* remove space for the table borders */
	table_width -= ( table->table_ele->border_left_width
		+ table->table_ele->border_right_width );

	if (table->draw_borders == TABLE_BORDERS_OFF)
	{
		table_pad = FEUNITS_X(TABLE_DEF_CELL_BORDER, context);
		table_width -= 2 * table_pad;
	}
	
	if ( table->fixed_cols > 0 )
	{
		table_width -= ( table->fixed_cols + 1 ) * cell_pad;
	}
	else
	{
		table_width -= ( table->cols + 1 ) * cell_pad;
	}

	return table_width;
}

/*
 ********************************************************************************
 * Core Table Layout Code
 */
 
void
lo_BeginCaptionSubDoc(MWContext *context, lo_DocState *state,
	lo_TableCaption *caption, PA_Tag *tag)
{
	LO_SubDocStruct *subdoc;
	lo_DocState *new_state;
	int32 first_width, first_height;
	Bool allow_percent_width;
	Bool allow_percent_height;

	/*
	 * Fill in the subdoc structure with default data
	 */
	subdoc = (LO_SubDocStruct *)lo_NewElement(context, state, LO_SUBDOC, NULL, 0);
	if (subdoc == NULL)
	{
		return;
	}

	subdoc->type = LO_SUBDOC;
	subdoc->ele_id = NEXT_ELEMENT;
	subdoc->x = state->x;
	subdoc->x_offset = 0;
	subdoc->y = state->y;
	subdoc->y_offset = 0;
	subdoc->width = 0;
	subdoc->height = 0;
	subdoc->next = NULL;
	subdoc->prev = NULL;

	subdoc->anchor_href = state->current_anchor;

	if (state->font_stack == NULL)
	{
		subdoc->text_attr = NULL;
	}
	else
	{
		subdoc->text_attr = state->font_stack->text_attr;
	}

	subdoc->FE_Data = NULL;
	subdoc->backdrop.bg_color = NULL;
    subdoc->backdrop.url = NULL;
    subdoc->backdrop.tile_mode = LO_TILE_BOTH;
	subdoc->state = NULL;

	subdoc->vert_alignment = LO_ALIGN_CENTER;
	subdoc->horiz_alignment = caption->horiz_alignment;

	subdoc->alignment = LO_ALIGN_BOTTOM;

	subdoc->border_width = 0;
	subdoc->border_vert_space = 0;
	subdoc->border_horiz_space = 0;

	subdoc->ele_attrmask = 0;

	subdoc->sel_start = -1;
	subdoc->sel_end = -1;

	subdoc->border_width = FEUNITS_X(subdoc->border_width, context);

	if (subdoc->width == 0)
	{
		first_width = FEUNITS_X(5000, context);
		allow_percent_width = FALSE;
	}
	else
	{
		first_width = subdoc->width;
		allow_percent_width = TRUE;
	}

	if (subdoc->height == 0)
	{
		first_height = FEUNITS_Y(10000, context);
		allow_percent_height = FALSE;
	}
	else
	{
		first_height = subdoc->height;
		allow_percent_height = TRUE;
	}

	new_state = lo_NewLayout(context, first_width, first_height, 0, 0, NULL);
	if (new_state == NULL)
	{
		lo_FreeElement(context, (LO_Element *)subdoc, FALSE);
		return;
	}
	new_state->is_a_subdoc = SUBDOC_CAPTION;
	lo_InheritParentState(context, new_state, state);
	new_state->allow_percent_width = allow_percent_width;
	new_state->allow_percent_height = allow_percent_height;

	new_state->base_x = 0;
	new_state->base_y = 0;
	new_state->display_blocked = TRUE;
	if (subdoc->width == 0)
	{
		new_state->delay_align = TRUE;
	}

	new_state->win_left = 0;
	new_state->win_right = 0;
	new_state->x = new_state->win_left;
	new_state->y = 0;
	new_state->max_width = new_state->x + 1;
	new_state->left_margin = new_state->win_left;
	new_state->right_margin = new_state->win_width - new_state->win_right;
	new_state->list_stack->old_left_margin = new_state->left_margin;
	new_state->list_stack->old_right_margin = new_state->right_margin;

	/*
	 * Initialize the alignment stack for this state
	 * to match the caption alignment.
	 */
	if (subdoc->horiz_alignment != LO_ALIGN_LEFT)
	{
		lo_PushAlignment(new_state, tag->type, subdoc->horiz_alignment);
	}

	state->sub_state = new_state;

	state->current_ele = (LO_Element *)subdoc;
	
	/*
	 * We added a new state.
	 */
	lo_PushStateLevel ( context );
}


static void
lo_reuse_current_state(MWContext *context, lo_DocState *state,
	int32 width, int32 height, int32 margin_width, int32 margin_height,
	Bool destroyLineLists)
{
	lo_TopState *top_state;
	int32 doc_id;
    lo_DocLists *doc_lists;

	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(state == NULL))
	{
		return;
	}

	state->subdoc_tags = NULL;
	state->subdoc_tags_end = NULL;

	state->text_fg.red = lo_master_colors[LO_COLOR_FG].red;
	state->text_fg.green = lo_master_colors[LO_COLOR_FG].green;
	state->text_fg.blue = lo_master_colors[LO_COLOR_FG].blue;

	state->text_bg.red = lo_master_colors[LO_COLOR_BG].red;
	state->text_bg.green = lo_master_colors[LO_COLOR_BG].green;
	state->text_bg.blue = lo_master_colors[LO_COLOR_BG].blue;

	state->anchor_color.red = lo_master_colors[LO_COLOR_LINK].red;
	state->anchor_color.green = lo_master_colors[LO_COLOR_LINK].green;
	state->anchor_color.blue = lo_master_colors[LO_COLOR_LINK].blue;

	state->visited_anchor_color.red = lo_master_colors[LO_COLOR_VLINK].red;
	state->visited_anchor_color.green = lo_master_colors[LO_COLOR_VLINK].green;
	state->visited_anchor_color.blue = lo_master_colors[LO_COLOR_VLINK].blue;

	state->active_anchor_color.red = lo_master_colors[LO_COLOR_ALINK].red;
	state->active_anchor_color.green = lo_master_colors[LO_COLOR_ALINK].green;
	state->active_anchor_color.blue = lo_master_colors[LO_COLOR_ALINK].blue;

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

	state->x = state->win_left;
	state->y = state->win_top;

	state->left_margin = state->win_left;
	state->right_margin = state->win_width - state->win_right;
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

	if (destroyLineLists == TRUE)
	{
		if (state->line_list != NULL)
		{
			lo_FreeElementList(context, state->line_list);
			state->line_list = NULL;
		}
		state->end_last_line = NULL;
		if (state->float_list != NULL)
		{	
			lo_FreeElementList(context, state->float_list);
			state->float_list = NULL;
		}
	}

	state->base_font_size = DEFAULT_BASE_FONT_SIZE;
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

	state->font_stack->text_attr->size = state->base_font_size;

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
	state->list_stack = lo_DefaultList(state);

	state->text_info.max_width = 0;
	state->text_info.ascent = 0;
	state->text_info.descent = 0;
	state->text_info.lbearing = 0;
	state->text_info.rbearing = 0;

	state->line_buf_len = 0;

	state->baseline = 0;
	state->line_height = 0;
	state->break_pos = -1;
	state->break_width = 0;
	state->last_char_CR = FALSE;
	state->trailing_space = FALSE;
	state->at_begin_line = TRUE;

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

	if (state->top_state->doc_state)
		doc_lists = lo_GetCurrentDocLists(state->top_state->doc_state);
	else
		doc_lists = lo_GetCurrentDocLists(state);
	
	/*
	 * These values are saved into the (sub)doc here
	 * so that if it gets Relayout() later, we know where
	 * to reset the from counts to.
	 */
	if (doc_lists->form_list != NULL)
	{
		lo_FormData *form_list;

		form_list = doc_lists->form_list;
		state->form_ele_cnt = form_list->form_ele_cnt;
		state->form_id = form_list->id;
	}
	else
	{
		state->form_ele_cnt = 0;
		state->form_id = 0;
	}
	state->start_in_form = top_state->in_form;
	if (top_state->savedData.FormList != NULL)
	{
		lo_SavedFormListData *all_form_ele;

		all_form_ele = top_state->savedData.FormList;
		state->form_data_index = all_form_ele->data_index;
	}
	else
	{
		state->form_data_index = 0;
	}

	/*
	 * Remember number of embeds we had before beginning this
	 * (sub)doc. This information is used in laytable.c so
	 * it can preserve embed indices across a relayout.
	 */
	state->embed_count_base = top_state->embed_count;

	/*
	 * Ditto for anchors, images, applets and embeds
	 */
	state->url_count_base = doc_lists->url_list_len;
	state->image_list_count_base = doc_lists->image_list_count;
        state->applet_list_count_base = doc_lists->applet_list_count;
        state->embed_list_count_base = doc_lists->embed_list_count;
	state->current_layer_num_base = top_state->current_layer_num;
	state->current_layer_num_max = top_state->current_layer_num;

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

#ifdef EDITOR
	state->edit_force_offset = FALSE;
	state->edit_current_element = 0;
	state->edit_current_offset = 0;
	state->edit_relayout_display_blocked = FALSE;
#endif
#ifdef MOCHA
	state->in_relayout = FALSE;
#endif

	state->cur_text_block = NULL;
	
	/* we need min widths during the initial table layout pass */
	state->need_min_width = TRUE;
}


static void
lo_reuse_current_subdoc(MWContext *context, LO_SubDocStruct *subdoc)
{
	if (subdoc == NULL)
	{
		return;
	}

	subdoc->type = LO_SUBDOC;
	subdoc->ele_id = 0;
	subdoc->x = 0;
	subdoc->x_offset = 0;
	subdoc->y = 0;
	subdoc->y_offset = 0;
	subdoc->width = 0;
	subdoc->height = 0;
	subdoc->next = NULL;
	subdoc->prev = NULL;

	subdoc->FE_Data = NULL;

	/*
	 * We leave subdoc->state alone as it will get reused later.
	 */

    XP_FREEIF(subdoc->backdrop.bg_color);
    XP_FREEIF(subdoc->backdrop.url);

	/*
	 * These two come from lists of shared pointers, so
	 * we NULL them but don't free them.
	 */
	subdoc->anchor_href = NULL;
	subdoc->text_attr = NULL;

	subdoc->alignment = LO_ALIGN_LEFT;
	subdoc->vert_alignment = LO_ALIGN_TOP;
	subdoc->horiz_alignment = LO_ALIGN_LEFT;
	subdoc->border_width = 0;
	subdoc->border_vert_space = 0;
	subdoc->border_horiz_space = 0;
	subdoc->ele_attrmask = 0;
	subdoc->sel_start = -1;
	subdoc->sel_end = -1;
}


static void
lo_cleanup_old_state(lo_DocState *state)
{
	LO_Element **line_array;

	if (state == NULL)
	{
		return;
	}

	/*
	 * Clear out the start of the line_list.
	 */
#ifdef XP_WIN16
	{
		XP_Block *larray_array;

		XP_LOCK_BLOCK(larray_array, XP_Block *, state->larray_array);
		state->line_array = larray_array[0];
		XP_UNLOCK_BLOCK(state->larray_array);
	}
#endif /* XP_WIN16 */
	XP_LOCK_BLOCK(line_array, LO_Element **, state->line_array);
	line_array[0] = NULL;
	XP_UNLOCK_BLOCK(state->line_array);

	state->end_last_line = NULL;

	state->float_list = NULL;
}


static int32 lo_CalculateCellPercentWidth( lo_TableRec *table, int32 percent)
{
	int32 val = percent;

	if ( table->table_width_fixed != FALSE )
	{
		val = table->width * val / 100;
	}
	else
	{
		val = 0;
	}
	return val;
}

static int32 lo_CalculateCellPercentHeight( lo_TableRec *table, lo_TableCell *table_cell)
{
	int32 val;

	if ((table->height > 0)&&
		(table_cell->percent_height > 0))
	{
		val = table->height *
			table_cell->percent_height / 100;
	}
	else
	{
		val = 0;
	}
	return val;
}

void
lo_BeginCellSubDoc(MWContext *context, 
 					lo_DocState *state,
 					lo_TableRec *table,
 					char *bgcolor_attr,
 					char *background_attr,
                    lo_TileMode tile_mode,
 					char *valign_attr,
 					char *halign_attr,
 					char *width_attr,
 					char *height_attr,
 					Bool is_a_header, 
 					intn draw_borders,
					Bool relayout /* Is true during relayout without reload */)
{
	LO_SubDocStruct *subdoc;
	lo_DocState *new_state;
	lo_TableRow *table_row;
	lo_TableCell *table_cell;
	int32 val;
	int32 first_width, first_height;
	Bool allow_percent_width;
	Bool allow_percent_height;
	StyleStruct *style_struct=0;
#ifdef LOCAL_DEBUG
 fprintf(stderr, "lo_BeginCellSubDoc called\n");
#endif /* LOCAL_DEBUG */

	table_row = table->row_ptr;
	table_cell = table_row->cell_ptr;

	if (relayout == FALSE) 
	{
		if(state->top_state && state->top_state->style_stack)
			style_struct = STYLESTACK_GetStyleByIndex(
											state->top_state->style_stack,
											0);
	}

	/*
	 * Fill in the subdoc structure with default data
	 */
	lo_reuse_current_subdoc(context, table->current_subdoc);
	subdoc = table->current_subdoc;
	if (subdoc == NULL)
	{
		return;
	}

	subdoc->type = LO_SUBDOC;
	subdoc->ele_id = NEXT_ELEMENT;
	subdoc->x = state->x;
	subdoc->x_offset = 0;
	subdoc->y = state->y;
	subdoc->y_offset = 0;
	subdoc->width = 0;
	subdoc->height = 0;
	subdoc->next = NULL;
	subdoc->prev = NULL;

	subdoc->anchor_href = state->current_anchor;

	if (state->font_stack == NULL)
	{
		subdoc->text_attr = NULL;
	}
	else
	{
		subdoc->text_attr = state->font_stack->text_attr;
	}

	subdoc->FE_Data = NULL;

	subdoc->vert_alignment = table_row->vert_alignment;
	if (subdoc->vert_alignment == LO_ALIGN_DEFAULT)
	{
		subdoc->vert_alignment = LO_ALIGN_CENTER;
	}
	subdoc->horiz_alignment = table_row->horiz_alignment;
	if (subdoc->horiz_alignment == LO_ALIGN_DEFAULT)
	{
		if (is_a_header == FALSE)
		{
			subdoc->horiz_alignment = LO_ALIGN_LEFT;
		}
		else
		{
			subdoc->horiz_alignment = LO_ALIGN_CENTER;
		}
	}
	subdoc->alignment = LO_ALIGN_BOTTOM;

	if (draw_borders == TABLE_BORDERS_OFF)
	{
		subdoc->border_width = 0;
		subdoc->border_vert_space = TABLE_DEF_CELL_BORDER;
		subdoc->border_horiz_space = TABLE_DEF_CELL_BORDER;
	}
	else if (draw_borders == TABLE_BORDERS_ON)
	{
		subdoc->border_width = TABLE_DEF_CELL_BORDER;
		subdoc->border_vert_space = 0;
		subdoc->border_horiz_space = 0;
	}
	else
	{
		subdoc->border_width = 0;
		subdoc->border_vert_space = 0;
		subdoc->border_horiz_space = 0;
	}

	subdoc->ele_attrmask = 0;

	subdoc->sel_start = -1;
	subdoc->sel_end = -1;

	subdoc->backdrop.bg_color = NULL;
    subdoc->backdrop.url = NULL;
    subdoc->backdrop.tile_mode = LO_TILE_BOTH;

	/*
	 * May inherit a bg_color from your row.
	 */
	if (table_row->backdrop.bg_color != NULL)
	{
		subdoc->backdrop.bg_color = XP_NEW(LO_Color);
		if (subdoc->backdrop.bg_color != NULL)
			*subdoc->backdrop.bg_color = *table_row->backdrop.bg_color;
	}

	if (relayout == FALSE)
	{
		subdoc->backdrop.bg_color = NULL;
		subdoc->backdrop.url = NULL;
		subdoc->backdrop.tile_mode = LO_TILE_BOTH;

		/*
		 * May inherit a bg_color from your row.
		 */
		if (table_row->backdrop.bg_color != NULL)
		{
			subdoc->backdrop.bg_color = XP_NEW(LO_Color);
			if (subdoc->backdrop.bg_color != NULL)
				*subdoc->backdrop.bg_color = *table_row->backdrop.bg_color;
		}

		/*
		 * Else may inherit a bg_color from your table.
		 */
		else if (table->backdrop.bg_color != NULL)
		{
			subdoc->backdrop.bg_color = XP_NEW(LO_Color);
			if (subdoc->backdrop.bg_color != NULL)
				*subdoc->backdrop.bg_color = *table->backdrop.bg_color;
		}
		/*
		 * Else possibly inherit the background color attribute
		 * of a parent table cell.
		 */
		else if (state->is_a_subdoc == SUBDOC_CELL)
		{
			lo_DocState *up_state;

			/*
			 * Find the parent subdoc's state.
			 */
			up_state = state->top_state->doc_state;
			while ((up_state->sub_state != NULL)&&
				(up_state->sub_state != state))
			{
				up_state = up_state->sub_state;
			}
			if ((up_state->sub_state != NULL)&&
				(up_state->current_ele != NULL)&&
				(up_state->current_ele->type == LO_SUBDOC)&&
				(up_state->current_ele->lo_subdoc.backdrop.bg_color != NULL))
			{
				LO_Color *old_bg;

				old_bg = up_state->current_ele->lo_subdoc.backdrop.bg_color;
				subdoc->backdrop.bg_color = XP_NEW(LO_Color);
				if (subdoc->backdrop.bg_color != NULL)
					*subdoc->backdrop.bg_color = *old_bg;
			}
		}


		/* Use backdrop URL, if supplied. */
		if (background_attr)
		{
			subdoc->backdrop.url = XP_STRDUP(background_attr);
			subdoc->backdrop.tile_mode = tile_mode;
		}
    
		/*
		 * May inherit a backdrop_url from parent row.
		 */
		else if (table_row->backdrop.url)
		{
			subdoc->backdrop.url = XP_STRDUP(table_row->backdrop.url);
			subdoc->backdrop.tile_mode = table_row->backdrop.tile_mode;
		}

		/*
		 * Else may inherit a backdrop_url from parent table.
		 */
		else if (table->backdrop.url != NULL)
		{
			subdoc->backdrop.url = XP_STRDUP(table->backdrop.url);
			subdoc->backdrop.tile_mode = table->backdrop.tile_mode;
		}
		/*
		 * Else possibly inherit the backdrop_url
		 * of a parent table cell.
		 */
		else if (state->is_a_subdoc == SUBDOC_CELL)
		{
			lo_DocState *up_state;

			/*
			 * Find the parent subdoc's state.
			 */
			up_state = state->top_state->doc_state;
			while ((up_state->sub_state != NULL)&&
				(up_state->sub_state != state))
			{
				up_state = up_state->sub_state;
			}
			if ((up_state->sub_state != NULL)&&
				(up_state->current_ele != NULL)&&
				(up_state->current_ele->type == LO_SUBDOC)&&
				(up_state->current_ele->lo_subdoc.backdrop.url != NULL))
			{
				lo_Backdrop *src_backdrop = &up_state->current_ele->lo_subdoc.backdrop;
				subdoc->backdrop.url = XP_STRDUP(src_backdrop->url);
				subdoc->backdrop.tile_mode = src_backdrop->tile_mode;
			}
		}

		/*
		 * Check for a background color attribute
		 */
		if (bgcolor_attr)
		{
			uint8 red, green, blue;

			LO_ParseRGB(bgcolor_attr, &red, &green, &blue);
			XP_FREEIF(subdoc->backdrop.bg_color);
			subdoc->backdrop.bg_color = XP_NEW(LO_Color);
			if (subdoc->backdrop.bg_color != NULL)
			{
				subdoc->backdrop.bg_color->red = red;
				subdoc->backdrop.bg_color->green = green;
				subdoc->backdrop.bg_color->blue = blue;
			}
		}

		/* check for a style sheet directive for bgcolor */
		if(style_struct)
		{
			char *property = STYLESTRUCT_GetString(style_struct, BG_COLOR_STYLE);
		
			if(property)
			{
				uint8 red, green, blue;

				LO_ParseStyleSheetRGB(property, &red, &green, &blue);

				XP_FREEIF(subdoc->backdrop.bg_color);
				subdoc->backdrop.bg_color = XP_NEW(LO_Color);
				if (subdoc->backdrop.bg_color != NULL)
				{
					subdoc->backdrop.bg_color->red = red;
					subdoc->backdrop.bg_color->green = green;
					subdoc->backdrop.bg_color->blue = blue;
				}
				XP_FREE(property);
			}

			property = STYLESTRUCT_GetString(style_struct, BG_IMAGE_STYLE);
			if(property)
			{
				if(subdoc->backdrop.url)
					XP_FREE(subdoc->backdrop.url);
				subdoc->backdrop.url = NULL;

				if(strcasecomp(property, "none"))
			{
					subdoc->backdrop.url = XP_STRDUP(lo_ParseStyleSheetURL(property));
				}
				XP_FREE(property);
			}

			property = STYLESTRUCT_GetString(style_struct, BG_REPEAT_STYLE);
			if(property)
			{
				if(!strcasecomp(property, BG_REPEAT_NONE_STYLE))
					subdoc->backdrop.tile_mode = LO_NO_TILE;
				else if(!strcasecomp(property, BG_REPEAT_X_STYLE))
					subdoc->backdrop.tile_mode = LO_TILE_HORIZ;
				else if(!strcasecomp(property, BG_REPEAT_Y_STYLE))
					subdoc->backdrop.tile_mode = LO_TILE_VERT;
				else 
					subdoc->backdrop.tile_mode = LO_TILE_BOTH;
			}
		}
	}	
	else  /* If relayout is TRUE */
	{
		/*
		subdoc->backdrop.tile_mode = table_cell->cell->backdrop.tile_mode;		
		if (table_cell->cell->backdrop.bg_color != NULL)
		{
			subdoc->backdrop.bg_color = XP_NEW(LO_Color);
			subdoc->backdrop.bg_color->red = table_cell->cell->backdrop.bg_color->red;
			subdoc->backdrop.bg_color->green = table_cell->cell->backdrop.bg_color->green;
			subdoc->backdrop.bg_color->blue = table_cell->cell->backdrop.bg_color->blue;
		}
		if (table_cell->cell->backdrop.url != NULL)
		{
			subdoc->backdrop.url = XP_STRDUP(table_cell->cell->backdrop.url);
		}
		*/
		subdoc->backdrop = table_cell->cell->backdrop;
	}

	/*
	 * Check for a vertical align parameter
	 */
	if (valign_attr)
	{
		subdoc->vert_alignment = lo_EvalVAlignParam(valign_attr);
	}



	if(style_struct)
	{
		char *property = STYLESTRUCT_GetString(style_struct, VERTICAL_ALIGN_STYLE);
	
		if(property)
		{
			subdoc->vert_alignment = lo_EvalVAlignParam(property);
			XP_FREE(property);
		}
	}

	if (relayout)
	{
		subdoc->vert_alignment = table_cell->vert_alignment;
	}

	/*
	 * Check for a horizontal align parameter
	 */
	if (halign_attr)
	{
		subdoc->horiz_alignment = lo_EvalCellAlignParam(halign_attr);
	}

	if (relayout)
	{
		subdoc->horiz_alignment = table_cell->horiz_alignment;
	}

	subdoc->border_width = FEUNITS_X(subdoc->border_width, context);
	subdoc->border_vert_space =
		FEUNITS_Y(subdoc->border_vert_space, context);
	subdoc->border_horiz_space =
		FEUNITS_X(subdoc->border_horiz_space, context);

	/*
	 * Allow individual setting of cell
	 * width and height.
	 */
	if (width_attr)
	{
		Bool is_percent;

		val = lo_ValueOrPercent(width_attr, &is_percent);
		if (is_percent != FALSE)
		{
			/*
			 * Percentage width cells must be handled
			 * later after at least the whole row is
			 * processed.
			 */
			table_cell->percent_width = val;
			table_row->has_percent_width_cells = TRUE;
			table->has_percent_width_cells = TRUE;
			val = lo_CalculateCellPercentWidth( table, val );

			/* Copied to lo_CalculateCellPercentWidth() */
			/*
			if ( table->table_width_fixed != FALSE )
			{
				val = table->width * val / 100;
			}
			else
			{
				val = 0;
			}
			*/
		}
		else
		{
			table_cell->percent_width = 0;
			val = FEUNITS_X(val, context);
		}
		if (val < 0)
		{
			val = 0;
		}
		
		subdoc->width = val;
		table_cell->specified_width = subdoc->width;
	}

	/* During relayout without reload, recalculate width of subdoc */
	if (relayout)
	{
		if (table_cell->percent_width > 0)
		{
			subdoc->width = lo_CalculateCellPercentWidth( table, table_cell->percent_width);
			table_cell->specified_width = subdoc->width;
		}
		else {
			subdoc->width = table_cell->specified_width;
			if (subdoc->width < 0) {
				subdoc->width = 0;
			}
		}
		
	}
							
	/*
	 * If we're in the fixed layout case, figure out what our cell
	 * width should be.
	 */
	if ( table->fixed_cols > 0 )
	{
		int32 cell_empty_space;
		int32 def_cell_width;
		int32 desired_width;
		
		cell_empty_space = lo_ComputeCellEmptySpace ( context, table, subdoc);

		desired_width = subdoc->width;
		/*
		 * Obey percentage cell width in fixed columns if we
		 * already know table width.
		 */
		if ((table_cell->percent_width > 0)&&(table->width > 0))
		{
			int32 avail_width;
			int32 cell_pad;

			/*
			 * Percentage width is a percentage of the available
			 * cell space.
			 */
			cell_pad = FEUNITS_X(table->inter_cell_pad, context);
			avail_width = table->width -
				((table->fixed_cols + 1) * cell_pad);
			avail_width -= (table->table_ele->border_left_width +
					table->table_ele->border_right_width);
			desired_width = avail_width *
				table_cell->percent_width / 100;
			/*
			 * Pecentage width takes into account the
			 * cell padding/spacing already.
			 */
			desired_width -= cell_empty_space;
		}
		def_cell_width = lo_FindDefaultCellWidth (context, table,
			table_cell, desired_width, cell_empty_space);
		
		/*
		 * If NOWRAP is off, we can go ahead and layout to this width. Otherwise
		 * we want to layout to a dynamic width and then use the bigger of the
		 * default or computed width for the cell.
		 */
		if ( table_cell->nowrap == FALSE )
		{
			subdoc->width = def_cell_width;
		}
	}
		
	if (height_attr)
	{
		Bool is_percent;

		val = lo_ValueOrPercent(height_attr, &is_percent);
		if (is_percent != FALSE)
		{
			/*
			 * We can process percentage heights here if
			 * we have a known table height.
			 */
			table_cell->percent_height = val;
			table->has_percent_height_cells = TRUE;
			val = lo_CalculateCellPercentHeight(table, table_cell);

			/* Copied to lo_CalculateCellPercentHeight() */
			/*
			if ((table->height > 0)&&
				(table_cell->percent_height > 0))
			{
				val = table->height *
					table_cell->percent_height / 100;
			}
			else
			{
				val = 0;
			}
			*/
		}
		else
		{
			table_cell->percent_height = 0;
			val = FEUNITS_X(val, context);
		}
		subdoc->height = val;
		if (subdoc->height < 0)
		{
			subdoc->height = 0;
		}
		table_cell->specified_height = subdoc->height;
	}

	/* During relayout without reload, recalculate height of subdoc */
	if (relayout)
	{
		if (table_cell->percent_height > 0)
		{
			subdoc->height = lo_CalculateCellPercentHeight( table, table_cell);
			table_cell->specified_height = subdoc->height;
		}
		else {
			subdoc->height = table_cell->specified_height;
			if (subdoc->height < 0) {
				subdoc->height = 0;
			}
		}
		
	}

	if (subdoc->width == 0)
	{
		/*
		 * Choose a default size that will fit within the current table. If NOWRAP is
		 * on, we just want to go ahead and choose a big width so that we're sure
		 * to get the correct nonwrapping text layout.
		 */
		if ( table_cell->nowrap == FALSE )
		{
			first_width = lo_ComputeInternalTableWidth ( context, table, state );
			first_width -= lo_ComputeCellEmptySpace ( context, table, subdoc);
		}
		else
		{
			first_width = FEUNITS_X(5000, context);
		}
		
		allow_percent_width = FALSE;
	}
	else
	{
		first_width = subdoc->width;
		allow_percent_width = TRUE;
	}

	if (subdoc->height == 0)
	{
		first_height = FEUNITS_Y(10000, context);
		allow_percent_height = FALSE;
	}
	else
	{
		first_height = subdoc->height;
		allow_percent_height = TRUE;
	}

	lo_reuse_current_state(context, subdoc->state,
		first_width, first_height, 0, 0, TRUE);
	new_state = subdoc->state;
	subdoc->state = NULL;

	if (new_state == NULL)
	{
		lo_FreeElement(context, (LO_Element *)subdoc, FALSE);
		return;
	}
	new_state->is_a_subdoc = SUBDOC_CELL;
	lo_InheritParentState(context, new_state, state);
	new_state->allow_percent_width = allow_percent_width;
	new_state->allow_percent_height = allow_percent_height;

	new_state->base_x = 0;
	new_state->base_y = 0;
	new_state->display_blocked = TRUE;
	if (subdoc->width == 0)
	{
		new_state->delay_align = TRUE;
	}

	new_state->win_left = 0;
	new_state->win_right = 0;
	new_state->x = new_state->win_left;
	new_state->y = 0;
	new_state->max_width = new_state->x + 1;
	new_state->left_margin = new_state->win_left;
	new_state->right_margin = new_state->win_width - new_state->win_right;
	new_state->list_stack->old_left_margin = new_state->left_margin;
	new_state->list_stack->old_right_margin = new_state->right_margin;

	/*
	 * Initialize the alignment stack for this state
	 * to match the caption alignment.
	 */
	if (subdoc->horiz_alignment != LO_ALIGN_LEFT)
	{
		lo_PushAlignment(new_state, P_TABLE_DATA, subdoc->horiz_alignment);
	}

	state->sub_state = new_state;

	state->current_ele = (LO_Element *)subdoc;

	if (is_a_header != FALSE)
	{
		LO_TextAttr *old_attr;
		LO_TextAttr *attr;
		LO_TextAttr tmp_attr;

		old_attr = new_state->font_stack->text_attr;
		lo_CopyTextAttr(old_attr, &tmp_attr);
		tmp_attr.fontmask |= LO_FONT_BOLD;
		attr = lo_FetchTextAttr(new_state, &tmp_attr);

		lo_PushFont(new_state, P_TABLE_DATA, attr);
	}
}


Bool
lo_subdoc_has_elements(lo_DocState *sub_state)
{
	if (sub_state->end_last_line != NULL)
	{
		return(TRUE);
	}
	if (sub_state->float_list != NULL)
	{
		return(TRUE);
	}
	return(FALSE);
}


Bool
lo_cell_has_elements(LO_CellStruct *cell)
{
	if (cell->cell_list != NULL)
	{
		return(TRUE);
	}
	if (cell->cell_float_list != NULL)
	{
		return(TRUE);
	}
	return(FALSE);
}


int32
lo_align_subdoc(MWContext *context, lo_DocState *state, lo_DocState *old_state,
	LO_SubDocStruct *subdoc, lo_TableRec *table, lo_table_span *row_max)
{
	int32 ele_cnt;
	LO_Element *eptr;
	int32 vert_inc;
	int32 top_inner_pad;
	int32 bottom_inner_pad;
	int32 left_inner_pad;
	int32 right_inner_pad;
	LO_Element **line_array;

	top_inner_pad = FEUNITS_X(table->inner_top_pad, context);
	bottom_inner_pad = FEUNITS_X(table->inner_bottom_pad, context);
	left_inner_pad = FEUNITS_X(table->inner_left_pad, context);
	right_inner_pad = FEUNITS_X(table->inner_right_pad, context);

	ele_cnt = 0;

	/*
	 * Mess with vertical alignment.
	 */
	vert_inc = subdoc->height - (2 * subdoc->border_width) - 
		(top_inner_pad + bottom_inner_pad) - old_state->y;
	if (vert_inc > 0)
	{
		if (subdoc->vert_alignment == LO_ALIGN_CENTER)
		{
			vert_inc = vert_inc / 2;
		}
		else if (subdoc->vert_alignment == LO_ALIGN_BOTTOM)
		{
			/* vert_inc unchanged */
		}
		else if ((subdoc->vert_alignment == LO_ALIGN_BASELINE)&&
			(row_max != NULL))
		{
			int32 baseline;
			int32 tmp_val;

			baseline = lo_GetSubDocBaseline(subdoc);
			tmp_val = row_max->min_dim - baseline;
			if (tmp_val > vert_inc)
			{
				tmp_val = vert_inc;
			}
			if (tmp_val > 0)
			{
				vert_inc = tmp_val;
			}
			else
			{
				vert_inc = 0;
			}
		}
		else
		{
			vert_inc = 0;
		}
	}
	else
	{
		vert_inc = 0;
	}

	/*
	 * Make eptr point to the start of the element chain
	 * for this subdoc.
	 */
#ifdef XP_WIN16
{
	XP_Block *larray_array;

	XP_LOCK_BLOCK(larray_array, XP_Block *, old_state->larray_array);
	old_state->line_array = larray_array[0];
	XP_UNLOCK_BLOCK(old_state->larray_array);
}
#endif /* XP_WIN16 */
	XP_LOCK_BLOCK(line_array, LO_Element **, old_state->line_array);
	eptr = line_array[0];
	XP_UNLOCK_BLOCK(old_state->line_array);

	while (eptr != NULL)
	{
		eptr->lo_any.x += left_inner_pad;

		switch (eptr->type)
		{
		    case LO_LINEFEED:
			if (eptr->lo_linefeed.ele_attrmask &
				LO_ELE_DELAY_ALIGNED)
			{
			    if (eptr->lo_linefeed.ele_attrmask &
				LO_ELE_DELAY_CENTER)
			    {
				int32 side_inc;
				int32 inside_width;
				LO_Element *c_eptr;

				c_eptr = eptr;
				inside_width = subdoc->width -
				    (2 * subdoc->border_width) -
				    (2 * subdoc->border_horiz_space) -
				    (left_inner_pad + right_inner_pad);

				side_inc = inside_width -
					(c_eptr->lo_linefeed.x +
					c_eptr->lo_linefeed.x_offset -
					left_inner_pad);
				side_inc = side_inc / 2;
				if (side_inc > 0)
				{
					c_eptr->lo_linefeed.width -= side_inc;
					if (c_eptr->lo_linefeed.width < 0)
					{
						c_eptr->lo_linefeed.width = 0;
					}
					while ((c_eptr->lo_any.prev != NULL)&&
						(c_eptr->lo_any.prev->type !=
							LO_LINEFEED))
					{
						c_eptr->lo_any.x += side_inc;
						if (c_eptr->type == LO_CELL)
						{
						    lo_ShiftCell(
							(LO_CellStruct *)c_eptr,
							side_inc, 0);
						}
						c_eptr = c_eptr->lo_any.prev;
					}
					c_eptr->lo_any.x += side_inc;
					if (c_eptr->type == LO_CELL)
					{
					    lo_ShiftCell(
						(LO_CellStruct *)c_eptr,
						side_inc, 0);
					}
				}
			    }
			    else if (eptr->lo_linefeed.ele_attrmask &
				LO_ELE_DELAY_RIGHT)
			    {
				int32 side_inc;
				int32 inside_width;
				LO_Element *c_eptr;

				c_eptr = eptr;
				inside_width = subdoc->width -
				    (2 * subdoc->border_width) -
				    (2 * subdoc->border_horiz_space) -
				    (left_inner_pad + right_inner_pad);

				side_inc = inside_width -
					(c_eptr->lo_linefeed.x +
					c_eptr->lo_linefeed.x_offset -
					left_inner_pad);
				if (side_inc > 0)
				{
					c_eptr->lo_linefeed.width -= side_inc;
					if (c_eptr->lo_linefeed.width < 0)
					{
						c_eptr->lo_linefeed.width = 0;
					}
					while ((c_eptr->lo_any.prev != NULL)&&
						(c_eptr->lo_any.prev->type !=
							LO_LINEFEED))
					{
						c_eptr->lo_any.x += side_inc;
						if (c_eptr->type == LO_CELL)
						{
						    lo_ShiftCell(
							(LO_CellStruct *)c_eptr,
							side_inc, 0);
						}
						c_eptr = c_eptr->lo_any.prev;
					}
					c_eptr->lo_any.x += side_inc;
					if (c_eptr->type == LO_CELL)
					{
					    lo_ShiftCell(
						(LO_CellStruct *)c_eptr,
						side_inc, 0);
					}
				}
			    }
			    eptr->lo_linefeed.ele_attrmask &=
					(~(LO_ELE_DELAY_ALIGNED));
			}
			break;
		    default:
			break;
		}
		eptr->lo_any.y += (vert_inc + top_inner_pad);
		if (eptr->type == LO_CELL)
		{
			lo_ShiftCell((LO_CellStruct *)eptr, left_inner_pad,
				(vert_inc + top_inner_pad));
		}
		eptr = eptr->lo_any.next;
		ele_cnt++;
	}

	eptr = old_state->float_list;
	while (eptr != NULL)
	{
		eptr->lo_any.x += left_inner_pad;
		eptr->lo_any.y += (vert_inc + top_inner_pad);
		if (eptr->type == LO_CELL)
		{
			lo_ShiftCell((LO_CellStruct *)eptr, left_inner_pad,
				(vert_inc + top_inner_pad));
		}
		eptr = eptr->lo_any.next;
		ele_cnt++;
	}

	return(ele_cnt);
}


int32
lo_align_cell(MWContext *context, lo_DocState *state, lo_TableCell *cell_ptr,
	LO_CellStruct *cell, lo_TableRec *table, lo_table_span *row_max)
{
	int32 ele_cnt;
	LO_Element *eptr;
	int32 vert_inc;
	int32 top_inner_pad;
	int32 bottom_inner_pad;
	int32 left_inner_pad;
	int32 right_inner_pad;

	top_inner_pad = FEUNITS_X(table->inner_top_pad, context);
	bottom_inner_pad = FEUNITS_X(table->inner_bottom_pad, context);
	left_inner_pad = FEUNITS_X(table->inner_left_pad, context);
	right_inner_pad = FEUNITS_X(table->inner_right_pad, context);

	ele_cnt = 0;

	/*
	 * Mess with vertical alignment.
	 */
	vert_inc = cell->height - (2 * cell->border_width) - 
		(top_inner_pad + bottom_inner_pad) - cell_ptr->max_y;
	if (vert_inc > 0)
	{
		if (cell_ptr->vert_alignment == LO_ALIGN_CENTER)
		{
			vert_inc = vert_inc / 2;
		}
		else if (cell_ptr->vert_alignment == LO_ALIGN_BOTTOM)
		{
			/* vert_inc unchanged */
		}
		else if ((cell_ptr->vert_alignment == LO_ALIGN_BASELINE)&&
			(row_max != NULL))
		{
			int32 baseline;
			int32 tmp_val;

			baseline = lo_GetCellBaseline(cell);
			tmp_val = row_max->min_dim - baseline;
			if (tmp_val > vert_inc)
			{
				tmp_val = vert_inc;
			}
			if (tmp_val > 0)
			{
				vert_inc = tmp_val;
			}
			else
			{
				vert_inc = 0;
			}
		}
		else
		{
			vert_inc = 0;
		}
	}
	else
	{
		vert_inc = 0;
	}

	/*
	 * Make eptr point to the start of the element chain
	 * for this cell.
	 */
	eptr = cell->cell_list;

	while (eptr != NULL)
	{
		eptr->lo_any.x += left_inner_pad;

		switch (eptr->type)
		{
		    case LO_LINEFEED:
			if (eptr->lo_linefeed.ele_attrmask &
				LO_ELE_DELAY_ALIGNED)
			{
			    if (eptr->lo_linefeed.ele_attrmask &
				LO_ELE_DELAY_CENTER)
			    {
				int32 side_inc;
				int32 inside_width;
				LO_Element *c_eptr;

				c_eptr = eptr;
				inside_width = cell->width -
				    (2 * cell->border_width) -
				    (2 * cell->border_horiz_space) -
				    (left_inner_pad + right_inner_pad);

				side_inc = inside_width -
					(c_eptr->lo_linefeed.x +
					c_eptr->lo_linefeed.x_offset -
					left_inner_pad);
				side_inc = side_inc / 2;
				if (side_inc > 0)
				{
					c_eptr->lo_linefeed.width -= side_inc;
					if (c_eptr->lo_linefeed.width < 0)
					{
						c_eptr->lo_linefeed.width = 0;
					}
					while ((c_eptr->lo_any.prev != NULL)&&
						(c_eptr->lo_any.prev->type !=
							LO_LINEFEED))
					{
						c_eptr->lo_any.x += side_inc;
						if (c_eptr->type == LO_CELL)
						{
						    lo_ShiftCell(
							(LO_CellStruct *)c_eptr,
							side_inc, 0);
						}
						c_eptr = c_eptr->lo_any.prev;
					}
					c_eptr->lo_any.x += side_inc;
					if (c_eptr->type == LO_CELL)
					{
					    lo_ShiftCell(
						(LO_CellStruct *)c_eptr,
						side_inc, 0);
					}
				}
			    }
			    else if (eptr->lo_linefeed.ele_attrmask &
				LO_ELE_DELAY_RIGHT)
			    {
				int32 side_inc;
				int32 inside_width;
				LO_Element *c_eptr;

				c_eptr = eptr;
				inside_width = cell->width -
				    (2 * cell->border_width) -
				    (2 * cell->border_horiz_space) -
				    (left_inner_pad + right_inner_pad);

				side_inc = inside_width -
					(c_eptr->lo_linefeed.x +
					c_eptr->lo_linefeed.x_offset -
					left_inner_pad);
				if (side_inc > 0)
				{
					c_eptr->lo_linefeed.width -= side_inc;
					if (c_eptr->lo_linefeed.width < 0)
					{
						c_eptr->lo_linefeed.width = 0;
					}
					while ((c_eptr->lo_any.prev != NULL)&&
						(c_eptr->lo_any.prev->type !=
							LO_LINEFEED))
					{
						c_eptr->lo_any.x += side_inc;
						if (c_eptr->type == LO_CELL)
						{
						    lo_ShiftCell(
							(LO_CellStruct *)c_eptr,
							side_inc, 0);
						}
						c_eptr = c_eptr->lo_any.prev;
					}
					c_eptr->lo_any.x += side_inc;
					if (c_eptr->type == LO_CELL)
					{
					    lo_ShiftCell(
						(LO_CellStruct *)c_eptr,
						side_inc, 0);
					}
				}
			    }
			    eptr->lo_linefeed.ele_attrmask &=
					(~(LO_ELE_DELAY_ALIGNED));
			}
			break;
		    default:
			break;
		}
		eptr->lo_any.y += (vert_inc + top_inner_pad);
		if (eptr->type == LO_CELL)
		{
			lo_ShiftCell((LO_CellStruct *)eptr, left_inner_pad,
				(vert_inc + top_inner_pad));
		}
		eptr = eptr->lo_any.next;
		ele_cnt++;
	}

	eptr = cell->cell_float_list;
	while (eptr != NULL)
	{
		eptr->lo_any.x += left_inner_pad;
		eptr->lo_any.y += (vert_inc + top_inner_pad);
		if (eptr->type == LO_CELL)
		{
			lo_ShiftCell((LO_CellStruct *)eptr, left_inner_pad,
				(vert_inc + top_inner_pad));
		}
		eptr = eptr->lo_any.next;
		ele_cnt++;
	}

	return(ele_cnt);
}


LO_SubDocStruct *
lo_EndCellSubDoc(MWContext *context, lo_DocState *state, lo_DocState *old_state,
	LO_Element *element, Bool relayout)
{
	LO_Element *eptr;
	LO_SubDocStruct *subdoc;
	PA_Block buff;
	char *str;
	Bool line_break;
	int32 baseline_inc;
	int32 line_inc;
	int32 subdoc_height, subdoc_width;
	int32 doc_height;
	LO_TextStruct tmp_text;
	LO_TextInfo text_info;
	LO_Element **line_array;
	Bool dynamic_width;
	Bool dynamic_height;
#ifdef LOCAL_DEBUG
 fprintf(stderr, "lo_EndCellSubDoc called\n");
#endif /* LOCAL_DEBUG */

    /* Close any layers (blocks) that were opened in this cell, in case
	   the document author forgot to. */
    while (old_state->layer_nest_level > 0)
	{
		lo_EndLayer(context, old_state, PR_TRUE);
	}

	subdoc = (LO_SubDocStruct *)element;
	subdoc->state = (void *)old_state;

	/*
	 * All this work is to get the text_info filled in for the current
	 * font in the font stack. Yuck, there must be a better way.
	 */
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
	tmp_text.text_attr =
		state->font_stack->text_attr;
	FE_GetTextInfo(context, &tmp_text, &text_info);
	PA_FREE(buff);

	if (relayout == FALSE)
	{
		/*
		 * We don't want to leave any end paragraph linbreaks in the subdoc
		 * as they give us a really ugly blank line at the bottom of a cell. So,
		 * if the last element in the list is paragraph break linefeed, then
		 * we kill it.
		 */
		eptr = old_state->end_last_line;
		if ( (eptr != NULL) && (eptr->type == LO_LINEFEED) &&
			(eptr->lo_linefeed.break_type == LO_LINEFEED_BREAK_PARAGRAPH) )
		{
			LO_Element *prev;
			
			prev = eptr->lo_any.prev;

			/*
			 * Kill this layout element and reset the subdoc height
			 */
			if ( prev != NULL )
			{
				prev->lo_any.next = NULL;
				old_state->end_last_line = prev;
			}
			else
			{
				old_state->line_list = NULL;
				old_state->end_last_line = NULL;
			}
			
			/* The if condition checks to make sure that the linefeed that we just killed
			 * was sticking out of the bottom of the cell.  This fixes bug 81282.
			 */
			if (eptr->lo_linefeed.y + eptr->lo_linefeed.line_height >= old_state->y) {
				/* shrink the document */
				old_state->y -= eptr->lo_linefeed.line_height;
			}

			lo_RecycleElements ( context, old_state, eptr );
		}
	}
	
	dynamic_width = FALSE;
	dynamic_height = FALSE;
	if (subdoc->width == 0)
	{
		dynamic_width = TRUE;
		/*
		 * Subdocs that were dynamically widthed, and which
		 * contained right aligned items, must be relaid out
		 * once their width is determined.
		 */
		eptr = old_state->float_list;
		while (eptr != NULL)
		{
			if ((eptr->type == LO_IMAGE)&&
				(eptr->lo_image.image_attr->alignment == LO_ALIGN_RIGHT))
			{
				old_state->must_relayout_subdoc = TRUE;
				break;
			}
			else if ((eptr->type == LO_SUBDOC)&&
				(eptr->lo_subdoc.alignment == LO_ALIGN_RIGHT))
			{
				old_state->must_relayout_subdoc = TRUE;
				break;
			}
			eptr = eptr->lo_any.next;
		}

		subdoc->width = old_state->max_width;
	}

	/*
	 * Add in the size of the subdoc's borders
	 */
	subdoc->width += (2 * subdoc->border_width);

	/*
	 * Figure the height the subdoc laid out in.
	 * Use this if we were passed no height, or if it is greater
	 * than the passed height.
	 */
	doc_height = old_state->y + (2 * subdoc->border_width);
	if (subdoc->height == 0)
	{
		dynamic_height = TRUE;
		subdoc->height = doc_height;
	}
	else if (subdoc->height < doc_height)
	{
		subdoc->height = doc_height;
	}

	/*
	 * Make eptr point to the start of the element chain
	 * for this subdoc.
	 */
#ifdef XP_WIN16
{
	XP_Block *larray_array;

	XP_LOCK_BLOCK(larray_array, XP_Block *, old_state->larray_array);
	old_state->line_array = larray_array[0];
	XP_UNLOCK_BLOCK(old_state->larray_array);
}
#endif /* XP_WIN16 */
	XP_LOCK_BLOCK(line_array, LO_Element **, old_state->line_array);
	eptr = line_array[0];
	XP_UNLOCK_BLOCK(old_state->line_array);

	while (eptr != NULL)
	{
		switch (eptr->type)
		{
		    case LO_LINEFEED:
			if ((eptr->lo_linefeed.x + eptr->lo_linefeed.x_offset +
				eptr->lo_linefeed.width) >= (subdoc->width - 1 -
				(2 * subdoc->border_width)))
			{
				eptr->lo_linefeed.width = (subdoc->width - 1 -
					(2 * subdoc->border_width)) -
					(eptr->lo_linefeed.x +
					eptr->lo_linefeed.x_offset);
				if (eptr->lo_linefeed.width < 0)
				{
					eptr->lo_linefeed.width = 0;
				}
			}
#ifdef OLD_DELAY_CENTERED
			if (eptr->lo_linefeed.ele_attrmask &
				LO_ELE_DELAY_CENTERED)
			{
				int32 side_inc;
				LO_Element *c_eptr;

				c_eptr = eptr;

				side_inc = subdoc->width - 1 -
					(2 * subdoc->border_width) -
					(eptr->lo_linefeed.x +
					eptr->lo_linefeed.x_offset);
				side_inc = side_inc / 2;
				if (side_inc > 0)
				{
					c_eptr->lo_linefeed.width -= side_inc;
					if (c_eptr->lo_linefeed.width < 0)
					{
						c_eptr->lo_linefeed.width = 0;
					}
					while ((c_eptr->lo_any.prev != NULL)&&
						(c_eptr->lo_any.prev->type !=
							LO_LINEFEED))
					{
						c_eptr->lo_any.x += side_inc;
						if (c_eptr->type == LO_CELL)
						{
						    lo_ShiftCell(
							(LO_CellStruct *)c_eptr,
							side_inc, 0);
						}
						c_eptr = c_eptr->lo_any.prev;
					}
					c_eptr->lo_any.x += side_inc;
					if (c_eptr->type == LO_CELL)
					{
					    lo_ShiftCell(
						(LO_CellStruct *)c_eptr,
						side_inc, 0);
					}
				}
				eptr->lo_linefeed.ele_attrmask &=
					(~(LO_ELE_DELAY_CENTERED));
			}
#endif /* OLD_DELAY_CENTERED */
			break;
		    /*
		     * This is awful, but any dynamically dimenstioned
		     * subdoc that contains one of these elements
		     * must be relayed out to take care of window
		     * width and height dependencies.
		     */

			/* case LO_HRULE: */
		    case LO_SUBDOC:
		    case LO_TABLE:
				old_state->must_relayout_subdoc = TRUE;

			break;
		    case LO_IMAGE:
/* WILL NEED THIS LATER FOR PERCENT WIDTH IMAGES
			if ((old_state->allow_percent_width == FALSE)||
				(old_state->allow_percent_height == FALSE))
			{
				old_state->must_relayout_subdoc = TRUE;
			}
*/
			break;
		    default:
			break;
		}
		eptr = eptr->lo_any.next;
	}

	/*
	 * Remember: (2 * subdoc->border_width) has already been added to the subdoc
	 * width above...
	 */
	subdoc_width = subdoc->width + (2 * subdoc->border_horiz_space);
	subdoc_height = subdoc->height + (2 * subdoc->border_width) +
		(2 * subdoc->border_vert_space);

	if (old_state->display_blocked != FALSE)
	{
		old_state->display_blocked = FALSE;
	}

	/*
	 * Will this subdoc make the line too wide.
	 */
	if ((state->x + subdoc_width) > state->right_margin)
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
	 * Also don't break in unwrappedpreformatted text.
	 * Also can't break inside a NOBR section.
	 */
	if ((state->at_begin_line != FALSE)||
		(state->preformatted == PRE_TEXT_YES)||
		(state->breakable == FALSE))
	{
		line_break = FALSE;
	}

	/*
         * break on the subdoc if we have
         * a break.
         */
/* DOn't break in cell
        if (line_break != FALSE)
        {
                lo_SoftLineBreak(context, state, TRUE);
                subdoc->x = state->x;
                subdoc->y = state->y;
        }
*/

	/*
	 * Figure out how to align this subdoc.
	 * baseline_inc is how much to increase the baseline
	 * of previous element of this line.  line_inc is how
	 * much to increase the line height below the baseline.
	 */
	baseline_inc = 0;
	line_inc = 0;
	/*
	 * If we are at the beginning of a line, with no baseline,
	 * we first set baseline and line_height based on the current
	 * font, then place the subdoc.
	 */
	if (state->baseline == 0)
	{
		state->baseline = 0;
	}

	lo_CalcAlignOffsets(state, &text_info, (intn)subdoc->alignment,
		subdoc_width, subdoc_height,
		&subdoc->x_offset, &subdoc->y_offset, &line_inc, &baseline_inc);

	subdoc->x_offset += (int16)subdoc->border_horiz_space;
	subdoc->y_offset += (int32)subdoc->border_vert_space;

	old_state->base_x = subdoc->x + subdoc->x_offset +
		subdoc->border_width;
	old_state->base_y = subdoc->y + subdoc->y_offset +
		subdoc->border_width;

	/*
	 * Clean up state
	state->x = state->x + subdoc->x_offset +
		subdoc_width - subdoc->border_horiz_space;
	state->linefeed_state = 0;
	state->at_begin_line = FALSE;
	state->trailing_space = FALSE;
	state->cur_ele_type = LO_SUBDOC;
	 */

	return(subdoc);
}


void
lo_relayout_recycle(MWContext *context, lo_DocState *state, LO_Element *elist)
{
	LO_LockLayout();
    
	while (elist != NULL)
	{
		LO_Element *eptr;

		eptr = elist;
		elist = elist->lo_any.next;
		eptr->lo_any.next = NULL;

		if (eptr->type == LO_IMAGE)
		{
			eptr->lo_any.next = state->top_state->trash;
			state->top_state->trash = eptr;
		}
		else if (eptr->type == LO_CELL)
		{
			if (eptr->lo_cell.cell_list != NULL)
			{
			    if (eptr->lo_cell.cell_list_end != NULL)
			    {
				eptr->lo_cell.cell_list_end->lo_any.next = NULL;
			    }
			    lo_relayout_recycle(context, state,
					eptr->lo_cell.cell_list);
				eptr->lo_cell.cell_list = NULL;
				eptr->lo_cell.cell_list_end = NULL;
			}
			if (eptr->lo_cell.cell_float_list != NULL)
			{
			    lo_relayout_recycle(context, state,
					eptr->lo_cell.cell_float_list);
				eptr->lo_cell.cell_float_list = NULL;
			}
			lo_RecycleElements(context, state, eptr);
		}
		else
		{
			/*
			 * If this is an embed, tell it that it should
			 * recycle rather than delete itself.
			 */
			if (eptr->type == LO_EMBED)
			{
				NPL_SameElement((LO_EmbedStruct*)eptr);
			}
			lo_RecycleElements(context, state, eptr);
		}
	}
	LO_UnlockLayout();
}


static void
lo_cleanup_state(MWContext *context, lo_DocState *state)
{
	LO_Element **line_array;

	if (state == NULL)
	{
		return;
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
		lo_relayout_recycle(context, state, state->line_list);
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
		lo_relayout_recycle(context, state, state->float_list);
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
			lo_relayout_recycle(context, state, eptr);
		}
		XP_UNLOCK_BLOCK(state->line_array);
		XP_FREE_BLOCK(state->line_array);
	}

	XP_DELETE(state);
}


static lo_FormData *
lo_merge_form_data(lo_FormData *new_form_list, lo_FormData *hold_form_list)
{
	intn i;
	LO_FormElementStruct **new_ele_list;
	LO_FormElementStruct **hold_ele_list;

	PA_LOCK(new_ele_list, LO_FormElementStruct **,
			new_form_list->form_elements);
	PA_LOCK(hold_ele_list, LO_FormElementStruct **,
			hold_form_list->form_elements);
	for (i=0; i<new_form_list->form_ele_cnt; i++)
	{
		if (hold_ele_list[i] != new_ele_list[i])
		{
#ifdef MOCHA
			if (new_ele_list[i]->mocha_object == NULL)
			{
				new_ele_list[i]->mocha_object
				    = hold_ele_list[i]->mocha_object;
			}
#endif
			hold_ele_list[i] = new_ele_list[i];
		}
	}
	PA_UNLOCK(hold_form_list->form_elements);
	PA_UNLOCK(new_form_list->form_elements);

#ifdef MOCHA
	/* XXX sometimes the second pass does not reflect a form object! */
	if (new_form_list->mocha_object != NULL)
	{
		hold_form_list->mocha_object = new_form_list->mocha_object;
	}
#endif
	return(hold_form_list);
}


/*
 * Serious table magic.  Given a table cell subdoc,
 * redo the layout to the new width from the stored tag list,
 * and create a whole new subdoc, which is returned.
 */
LO_SubDocStruct *
lo_RelayoutCaptionSubdoc(MWContext *context, lo_DocState *state, lo_TableCaption *caption,
	LO_SubDocStruct *subdoc, int32 width, Bool is_a_header)
{
	int32 first_height;
	lo_TopState *top_state;
	lo_DocState *old_state;
	lo_DocState *new_state;
	/* ifdef-ed out of service: 
	PA_Tag *tag_list; */
	/* Commented and ifdef-ed out of service: 
	PA_Tag *tag_ptr; */
	/* Commented out of service: 
	PA_Tag *tag_end_ptr; */
	int32 doc_id;
	Bool allow_percent_width;
	Bool allow_percent_height;
	int32 hold_form_ele_cnt;
	int32 hold_form_data_index;
	intn hold_form_id;
	intn hold_current_form_num;
	Bool hold_in_form;
	lo_FormData *hold_form_list;
	int32 hold_embed_global_index;
	int32 original_tag_count;
	intn hold_url_list_len;
	NET_ReloadMethod save_force;
	/* ifdef-ed out of service: 
	Bool save_diff_state; */
	/* ifdef-ed out of service: 
	int32 save_state_pushes; */
	/* ifdef-ed out of service: 
	int32 save_state_pops; */
	lo_DocLists *doc_lists;
	int32 hold_image_list_count;
        int32 hold_applet_list_count;
        int32 hold_embed_list_count;
	int32 hold_current_layer_num;

	/*
	 * Relayout is NOT allowed to block on images, so never
	 * have the forced reload flag set.
	 */
	save_force = state->top_state->force_reload;
	state->top_state->force_reload = NET_CACHE_ONLY_RELOAD;

#ifdef LOCAL_DEBUG
 fprintf(stderr, "lo_RelayoutSubdoc called\n");
#endif /* LOCAL_DEBUG */

	top_state = state->top_state;
	old_state = (lo_DocState *) subdoc->state;
	
	/*
	 * We are going to reuse this subdoc element instead
	 * of allocating a new one.
	 */
	/* subdoc->state = NULL; */
	subdoc->width = width;
	subdoc->height = 0;

	doc_lists = lo_GetCurrentDocLists(state);
	
	/*
	 * If this subdoc contained form elements.  We want to reuse
	 * them without messing up the order of later form elements in
	 * other subdocs.
	 *
	 * We assume that relaying out the doc will create the exact
	 * same number of form elements in the same order as the first time.
	 * so we reset global form counters to before this subdoc
	 * was ever laid out, saving the current counts to restore later.
	 */
	if (doc_lists->form_list != NULL)
	{
		lo_FormData *form_list;
		lo_FormData *tmp_fptr;

		hold_form_id = old_state->form_id;
		hold_current_form_num = doc_lists->current_form_num;
		form_list = doc_lists->form_list;
		hold_form_list = NULL;
		while ((form_list != NULL)&&
			(form_list->id > hold_form_id))
		{
			tmp_fptr = form_list;
			form_list = form_list->next;
			tmp_fptr->next = hold_form_list;
			hold_form_list = tmp_fptr;
		}
		doc_lists->form_list = form_list;
		doc_lists->current_form_num = hold_form_id;
		if (form_list != NULL)
		{
			hold_form_ele_cnt = form_list->form_ele_cnt;
			form_list->form_ele_cnt = old_state->form_ele_cnt;
		}
		else
		{
			hold_form_ele_cnt = 0;
		}
	}
	else
	{
		hold_form_id = 0;
		hold_form_ele_cnt = 0;
		hold_form_list = NULL;
		hold_current_form_num = 0;
	}

	/*
	 * We're going to be deleting and then recreating the contents 
	 * of this subdoc.  We want the newly-created embeds to have
	 * the same embed indices as their recycled counterparts, so
	 * we reset the master embed index in the top_state to be the
	 * value it held at the time the old subdoc was created. When
	 * we're finished creating the new subdoc, the master index
	 * should be back to its original value (but just in case we
	 * save that value in a local variable here).
	 */
	hold_embed_global_index = top_state->embed_count;
	top_state->embed_count = old_state->embed_count_base;

	/* ditto here for the last comment.  We are trying to keep
	 * a tag count that reflects the order of tags coming in
 	 * on the stream.  We need that count to be correct across
	 * multiple relayout instances so we restore the top state
	 * count with the saved state
	 */
	original_tag_count = top_state->tag_count;
	top_state->tag_count = state->beginning_tag_count;

	/*
	 * Same goes for anchors and images
	 */
	hold_url_list_len = doc_lists->url_list_len;
	doc_lists->url_list_len = old_state->url_count_base;

	hold_image_list_count = doc_lists->image_list_count;
	doc_lists->image_list_count = old_state->image_list_count_base;

	hold_applet_list_count = doc_lists->applet_list_count;
	doc_lists->applet_list_count = old_state->applet_list_count_base;
	
	hold_embed_list_count = doc_lists->embed_list_count;
	doc_lists->embed_list_count = old_state->embed_list_count_base;

	hold_current_layer_num = top_state->current_layer_num;
	top_state->current_layer_num = old_state->current_layer_num_base;
	
	/*
	 * Save whether we are in a form or not, and restore
	 * to what we were when the subdoc was started.
	 */
	hold_in_form = top_state->in_form;
	top_state->in_form = old_state->start_in_form;

	if (top_state->savedData.FormList != NULL)
	{
		lo_SavedFormListData *all_form_ele;

		all_form_ele = top_state->savedData.FormList;
		hold_form_data_index = all_form_ele->data_index;
		all_form_ele->data_index = old_state->form_data_index;
	}
	else
	{
		hold_form_data_index = 0;
	}

	/*
	 * Still variable height, but we know the width now.
	 */
	first_height = FEUNITS_Y(10000, context);
	allow_percent_height = FALSE;
	allow_percent_width = TRUE;

	/*
	 * Get a new sub-state to create the new subdoc in.
	 */
	/*
	new_state = lo_NewLayout(context, width, first_height, 0, 0, NULL);
	*/
	lo_reuse_current_state(context, subdoc->state, width,
		first_height, 0, 0, FALSE);

	new_state = subdoc->state;
	subdoc->state = NULL;

	
	if (new_state == NULL)
	{
		state->top_state->force_reload = save_force;
		return(subdoc);
	}

	new_state->end_last_line = NULL;
	/*
	if (old_state->is_a_subdoc == SUBDOC_CAPTION)
	{
		new_state->is_a_subdoc = SUBDOC_CAPTION;
	}
	else
	{
		new_state->is_a_subdoc = SUBDOC_CELL;
	}
	*/
	new_state->is_a_subdoc = SUBDOC_CAPTION;

	lo_InheritParentColors(context, new_state, state);
	new_state->allow_percent_width = allow_percent_width;
	new_state->allow_percent_height = allow_percent_height;
	/*
	 * In relaying out subdocs, we NEVER link back up subdoc tags
	 */
	new_state->subdoc_tags = NULL;
	new_state->subdoc_tags_end = NULL;

	/*
	 * Initialize other new state sundries.
	 */
	new_state->base_x = 0;
	new_state->base_y = 0;
	new_state->display_blocked = TRUE;
	if (subdoc->width == 0)
	{
		new_state->delay_align = TRUE;
	}

	new_state->win_left = 0;
	new_state->win_right = 0;
	new_state->x = new_state->win_left;
	new_state->y = 0;
	new_state->max_width = new_state->x + 1;
	new_state->left_margin = new_state->win_left;
	new_state->right_margin = new_state->win_width - new_state->win_right;
	new_state->list_stack->old_left_margin = new_state->left_margin;
	new_state->list_stack->old_right_margin = new_state->right_margin;

	/* we don't need min widths during relayout */
	new_state->need_min_width = FALSE;

	/* reset line_height stack */
	while (new_state->line_height_stack)
		lo_PopLineHeight(new_state);

	/* 
	 * Initialize the alignment stack for this state
	 * to match the caption alignment.
	 */
	if (subdoc->horiz_alignment != LO_ALIGN_LEFT)
	{
		lo_PushAlignment(new_state, P_TABLE_DATA, subdoc->horiz_alignment);
	}

	state->sub_state = new_state;

	state->current_ele = (LO_Element *)subdoc;
	
	if (is_a_header != FALSE)
	{
		LO_TextAttr *old_attr;
		LO_TextAttr *attr;
		LO_TextAttr tmp_attr;

		old_attr = new_state->font_stack->text_attr;
		lo_CopyTextAttr(old_attr, &tmp_attr);
		tmp_attr.fontmask |= LO_FONT_BOLD;
		attr = lo_FetchTextAttr(new_state, &tmp_attr);

		lo_PushFont(new_state, P_TABLE_HEADER, attr);
	}

	/*
	 * Move our current state down into the new sub-state.
	 * Fetch the tag list from the old subdoc's state in preparation
	 * for relayout.
	 */
	state = new_state;

	/*
	tag_ptr = old_state->subdoc_tags;
	tag_end_ptr = old_state->subdoc_tags_end;
	old_state->subdoc_tags = NULL;
	old_state->subdoc_tags_end = NULL;
	if (tag_end_ptr != NULL)
	{
		tag_end_ptr = tag_end_ptr->next;
	}
	*/

	/*
	 * Clean up memory used by old subdoc state.
	 */
	/*
	lo_cleanup_state(context, old_state);
	*/

#if 0    /* Doesn't need to happen any more because we do non-destructive reflow
	/*
	 * Save our parent's state levels
	 */
	save_diff_state = top_state->diff_state;
	save_state_pushes = top_state->state_pushes;
	save_state_pops = top_state->state_pops;

	while (tag_ptr != tag_end_ptr)
	{
		PA_Tag *tag;
		lo_DocState *sub_state;
		lo_DocState *up_state;
		lo_DocState *tmp_state;
		Bool may_save;

		tag = tag_ptr;
		tag_ptr = tag_ptr->next;
		tag->next = NULL;
		tag_list = tag_ptr;

		up_state = NULL;
		sub_state = state;
		while (sub_state->sub_state != NULL)
		{
			up_state = sub_state;
			sub_state = sub_state->sub_state;
		}

		if ((sub_state->is_a_subdoc == SUBDOC_CELL)||
			(sub_state->is_a_subdoc == SUBDOC_CAPTION))
		{
			may_save = TRUE;
		}
		else
		{
			may_save = FALSE;
		}

		/*
		 * Reset these so we can tell if anything happened
		 */
		top_state->diff_state = FALSE;
		top_state->state_pushes = 0;
		top_state->state_pops = 0;

#ifdef MOCHA
		sub_state->in_relayout = TRUE;
#endif
		lo_LayoutTag(context, sub_state, tag);
		tmp_state = lo_CurrentSubState(state);
#ifdef MOCHA
		if (tmp_state == sub_state)
		{
			sub_state->in_relayout = FALSE;
		}
#endif

		if (may_save != FALSE)
		{
			int32 state_diff;
			
			/* how has our state level changed? */
			state_diff = top_state->state_pushes - top_state->state_pops;
			
			/*
			 * That tag popped us up one state level.  If this new
			 * state is still a subdoc, save the tag there.
			 */
			if (state_diff == -1)
			{
				if ((tmp_state->is_a_subdoc == SUBDOC_CELL)||
				    (tmp_state->is_a_subdoc == SUBDOC_CAPTION))
				{
                    /* if we just popped a table we need to insert
                     * a dummy end tag to pop the dummy start tag
                     * we shove on the stack after createing a table
                     */
   					PA_Tag *new_tag = LO_CreateStyleSheetDummyTag(tag);
					if(new_tag)
					{
						lo_SaveSubdocTags(context, tmp_state, new_tag);
					}

				    lo_SaveSubdocTags(context, tmp_state, tag);
				}
				else
				{
				    PA_FreeTag(tag);
				}
			}
			/*
			 * Else that tag put us in a new subdoc on the same
			 * level.  It needs to be saved one level up,
			 * if the parent is also a subdoc.
			 */
			else if (( up_state != NULL ) &&
				( top_state->diff_state != FALSE ) &&
				( state_diff == 0 ))
			{
				if ((up_state->is_a_subdoc == SUBDOC_CELL)||
				     (up_state->is_a_subdoc == SUBDOC_CAPTION))
				{
				    lo_SaveSubdocTags(context, up_state, tag);
				}
				else
				{
				    PA_FreeTag(tag);
				}
			}
			/*
			 * Else we are still in the same subdoc
			 */
			else if (( top_state->diff_state == FALSE ) &&
				( state_diff == 0 ))
			{
				lo_SaveSubdocTags(context, sub_state, tag);
			}
			/*
			 * Else that tag started a new, nested subdoc.
			 * Add the starting tag to the parent.
			 */
			else if (( state_diff == 1 ))
			{
				lo_SaveSubdocTags(context, sub_state, tag);
				/*
				 * Since we have extended the parent chain,
				 * we need to reset the child to the new
				 * parent end-chain.
				 */
				if ((tmp_state->is_a_subdoc == SUBDOC_CELL)||
				    (tmp_state->is_a_subdoc == SUBDOC_CAPTION))
				{
					PA_Tag *new_tag;
					
					tmp_state->subdoc_tags =
						sub_state->subdoc_tags_end;

					/* add an aditional dummy tag so that style sheets
					 * can use it to query styles from for this entry
					 * that created a table
					 */
					new_tag = LO_CreateStyleSheetDummyTag(tag);
					if(new_tag)
					{
						lo_SaveSubdocTags(context, tmp_state, new_tag);
					}
				}
			}
			/*
			 * This can never happen.
			 */
			else
			{
				PA_FreeTag(tag);
			}
		}
		tag_ptr = tag_list;
	}

	/*
	 * Restore our parent's state levels
	 */
	top_state->diff_state = save_diff_state;
	top_state->state_pushes = save_state_pushes;
	top_state->state_pops = save_state_pops;
#endif

	/*
	 * Link the end of this subdoc's tag chain back to
	 * its parent.  The beginning should have never been
	 * unlinked from the parent.
	 */
	/*
	if (state->subdoc_tags_end != NULL)
	{
		state->subdoc_tags_end->next = tag_end_ptr;
	}
	*/

	lo_rl_ReflowDocState( context, state );

	/*
	 * makes sure we are at the bottom
	 * of everything in the document.
	 */
	/*
	lo_CloseOutLayout(context, state);
	*/

	lo_EndLayoutDuringReflow( context, state );

	old_state = state;
	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	state = top_state->doc_state;

	while ((state->sub_state != NULL)&&
		(state->sub_state != old_state))
	{
		state = state->sub_state;
	}

	/*
	 * Ok, now reset the global form element counters
	 * to their original values.
	 */
	if ((doc_lists->form_list != NULL)||(hold_form_list != NULL))
	{
		lo_FormData *form_list;
		lo_FormData *new_form_list;
		lo_FormData *tmp_fptr;

		/*
		 * Remove any new forms created in this subdocs to a
		 * separate list.
		 */
		form_list = doc_lists->form_list;
		new_form_list = NULL;
		while ((form_list != NULL)&&
			(form_list->id > hold_form_id))
		{
			tmp_fptr = form_list;
			form_list = form_list->next;
			tmp_fptr->next = new_form_list;
			new_form_list = tmp_fptr;
		}

		/*
		 * Restore the element count in the form we started in
		 * because the subdoc may have left it half finished.
		 */
		if (form_list != NULL)
		{
			form_list->form_ele_cnt = hold_form_ele_cnt;
		}

		/*
		 * Merge the newly created forms with the old ones
		 * since the new ones may have partially complete
		 * data.
		 */
		while ((new_form_list != NULL)&&(hold_form_list != NULL))
		{
			tmp_fptr = lo_merge_form_data(new_form_list,
					hold_form_list);
			new_form_list = new_form_list->next;
			hold_form_list = hold_form_list->next;
			tmp_fptr->next = form_list;
			form_list = tmp_fptr;
		}

		/*
		 * Add back on forms that are from later subdocs.
		 */
		while (hold_form_list != NULL)
		{
			tmp_fptr = hold_form_list;
			hold_form_list = hold_form_list->next;
			tmp_fptr->next = form_list;
			form_list = tmp_fptr;
		}

		doc_lists->form_list = form_list;
		doc_lists->current_form_num = hold_current_form_num;
	}

	/*
	 * Restore in form status to what is
	 * was before this relayout.
	 */
	top_state->in_form = hold_in_form;

	if (top_state->savedData.FormList != NULL)
	{
		lo_SavedFormListData *all_form_ele;

		all_form_ele = top_state->savedData.FormList;
		all_form_ele->data_index = hold_form_data_index;
	}

	/*
	 * The master embed index is typically now be back
	 * to its original value, but might not be if we
	 * only had to relayout some (not all) of the embeds
	 * in this subdoc.  So, make sure it's set back to
	 * its original value here.
	 */
	top_state->embed_count = hold_embed_global_index;

	/* verify that the tag count is still correct */
	top_state->tag_count = original_tag_count;

	/*
	 * Likewise for the anchor list.
	 */
	doc_lists->url_list_len = hold_url_list_len;
	doc_lists->image_list_count = hold_image_list_count;
        doc_lists->applet_list_count = hold_applet_list_count;
        doc_lists->embed_list_count = hold_embed_list_count;
	top_state->current_layer_num = hold_current_layer_num;

	subdoc = NULL;
	if ((state != old_state)&&
		(state->current_ele != NULL))
	{
		subdoc = lo_EndCellSubDoc(context, state,
				old_state, state->current_ele, TRUE);
		state->sub_state = NULL;
		state->current_ele = NULL;
/*
		if ((state->is_a_subdoc == SUBDOC_CELL)||
			(state->is_a_subdoc == SUBDOC_CAPTION))
		{
			if (old_state->subdoc_tags_end != NULL)
			{
				state->subdoc_tags_end =
					old_state->subdoc_tags_end;
			}
		}
*/
	}

	old_state->must_relayout_subdoc = FALSE;

	state->top_state->force_reload = save_force;
	return(subdoc);
}


LO_CellStruct *
lo_RelayoutCell(MWContext *context, lo_DocState *state,
	LO_SubDocStruct *subdoc, lo_TableCell *cell_ptr,
	LO_CellStruct *cell, int32 width, Bool is_a_header,
	Bool relayout)
{
	LO_CellStruct *cell_ele;
	int32 first_height;
	lo_TopState *top_state;
	lo_DocState *old_state;
	lo_DocState *new_state;
	/* Unused: 
	PA_Tag *tag_list; */
	/* COmmented out of service: 
	PA_Tag *tag_ptr; */
	/* Commented out of service: 
	PA_Tag *tag_end_ptr; */
	int32 doc_id;
	Bool allow_percent_width;
	Bool allow_percent_height;
	int32 hold_form_ele_cnt;
	int32 hold_form_data_index;
	intn hold_form_id;
	intn hold_current_form_num;
	Bool hold_in_form;
	lo_FormData *hold_form_list;
	int32 hold_embed_global_index;
	int32 original_tag_count;
	intn hold_url_list_len;
	NET_ReloadMethod save_force;
	/* Unused: 
	Bool save_diff_state; */
	/* Unused: 
	int32 save_state_pushes; */
	/* Unused: 
	;int32 save_state_pops; */
	lo_DocLists *doc_lists;
	int32 hold_image_list_count;
        int32 hold_applet_list_count;
        int32 hold_embed_list_count;
	int32 hold_current_layer_num;
	/* Commented out of service: 
	LO_Element * elem_list; */

	/*
	 * Relayout is NOT allowed to block on images, so never
	 * have the forced reload flag set.
	 */
	save_force = state->top_state->force_reload;
	state->top_state->force_reload = NET_CACHE_ONLY_RELOAD;

	top_state = state->top_state;
	
	/*
	 * We are going to reuse this subdoc element instead
	 * of allocating a new one.
	lo_reuse_current_subdoc(context, subdoc);
	subdoc->state = NULL;
	 */
	subdoc->width = width;
	subdoc->height = 0;

	subdoc->horiz_alignment = cell_ptr->horiz_alignment;

	doc_lists = lo_GetCurrentDocLists(state);
	
	/*
	 * If this subdoc contained form elements.  We want to reuse
	 * them without messing up the order of later form elements in
	 * other subdocs.
	 *
	 * We assume that relaying out the doc will create the exact
	 * same number of form elements in the same order as the first time.
	 * so we reset global form counters to before this subdoc
	 * was ever laid out, saving the current counts to restore later.
	 */
	if (doc_lists->form_list != NULL)
	{
		lo_FormData *form_list;
		lo_FormData *tmp_fptr;

		hold_form_id = cell_ptr->form_id;
		hold_current_form_num = doc_lists->current_form_num;
		form_list = doc_lists->form_list;
		hold_form_list = NULL;
		while ((form_list != NULL)&&
			(form_list->id > hold_form_id))
		{
			tmp_fptr = form_list;
			form_list = form_list->next;
			tmp_fptr->next = hold_form_list;
			hold_form_list = tmp_fptr;
		}
		doc_lists->form_list = form_list;
		doc_lists->current_form_num = hold_form_id;
		if (form_list != NULL)
		{
			hold_form_ele_cnt = form_list->form_ele_cnt;
			form_list->form_ele_cnt = cell_ptr->form_ele_cnt;
		}
		else
		{
			hold_form_ele_cnt = 0;
		}
	}
	else
	{
		hold_form_id = 0;
		hold_form_ele_cnt = 0;
		hold_form_list = NULL;
		hold_current_form_num = 0;
	}

	/*
	 * We're going to be deleting and then recreating the contents 
	 * of this subdoc.  We want the newly-created embeds to have
	 * the same embed indices as their recycled counterparts, so
	 * we reset the master embed index in the top_state to be the
	 * value it held at the time the old subdoc was created. When
	 * we're finished creating the new subdoc, the master index
	 * should be back to its original value (but just in case we
	 * save that value in a local variable here).
	 */
	hold_embed_global_index = top_state->embed_count;
	top_state->embed_count = cell_ptr->embed_count_base;

	/* same for tag count */
	original_tag_count = top_state->tag_count;
	top_state->tag_count = cell_ptr->beginning_tag_count;

	/*
	 * Same goes for anchors and images
	 */
	hold_url_list_len = doc_lists->url_list_len;
	doc_lists->url_list_len = cell_ptr->url_count_base;

	hold_image_list_count = doc_lists->image_list_count;
	doc_lists->image_list_count = cell_ptr->image_list_count_base;

	hold_applet_list_count = doc_lists->applet_list_count;
	doc_lists->applet_list_count = cell_ptr->applet_list_count_base;
	
	hold_embed_list_count = doc_lists->embed_list_count;
	doc_lists->embed_list_count = cell_ptr->embed_list_count_base;

	hold_current_layer_num = top_state->current_layer_num;
	top_state->current_layer_num = cell_ptr->current_layer_num_base;

	/*
	 * Save whether we are in a form or not, and restore
	 * to what we were when the subdoc was started.
	 */
	hold_in_form = top_state->in_form;
	top_state->in_form = cell_ptr->start_in_form;

	if (top_state->savedData.FormList != NULL)
	{
		lo_SavedFormListData *all_form_ele;

		all_form_ele = top_state->savedData.FormList;
		hold_form_data_index = all_form_ele->data_index;
		all_form_ele->data_index = cell_ptr->form_data_index;
	}
	else
	{
		hold_form_data_index = 0;
	}

	/*
	 * Still variable height, but we know the width now.
	 */
	first_height = FEUNITS_Y(10000, context);
	allow_percent_height = FALSE;
	allow_percent_width = TRUE;

	/*
	 * Get a new sub-state to create the new subdoc in.
	 */
	lo_reuse_current_state(context, subdoc->state,
		width, first_height, 0, 0, TRUE);
	new_state = subdoc->state;
	subdoc->state = NULL;


	if (new_state == NULL)
	{
		state->top_state->force_reload = save_force;
		return(cell);
	}
	if (cell_ptr->is_a_subdoc == SUBDOC_CAPTION)
	{
		new_state->is_a_subdoc = SUBDOC_CAPTION;
	}
	else
	{
		new_state->is_a_subdoc = SUBDOC_CELL;
	}

	lo_InheritParentState(context, new_state, state);
	new_state->allow_percent_width = allow_percent_width;
	new_state->allow_percent_height = allow_percent_height;
	/*
	 * In relaying out subdocs, we NEVER link back up subdoc tags
	 */
	new_state->subdoc_tags = NULL;
	new_state->subdoc_tags_end = NULL;

	/*
	 * Initialize other new state sundries.
	 */
	new_state->base_x = 0;
	new_state->base_y = 0;
	new_state->display_blocked = TRUE;
	if (subdoc->width == 0)
	{
		new_state->delay_align = TRUE;
	}

	new_state->win_left = 0;
	new_state->win_right = 0;
	new_state->x = new_state->win_left;
	new_state->y = 0;
	new_state->max_width = new_state->x + 1;
	new_state->left_margin = new_state->win_left;
	new_state->right_margin = new_state->win_width - new_state->win_right;
	new_state->list_stack->old_left_margin = new_state->left_margin;
	new_state->list_stack->old_right_margin = new_state->right_margin;
	
	/* we don't need min widths during relayout */
	new_state->need_min_width = FALSE;
    
    /* reset line_height stack */
    while(new_state->line_height_stack)
        lo_PopLineHeight(new_state);
    
    /*
	 * Initialize the alignment stack for this state
	 * to match the caption alignment.
	 */
	if (subdoc->horiz_alignment != LO_ALIGN_LEFT)
	{
		lo_PushAlignment(new_state, P_TABLE_DATA, subdoc->horiz_alignment);
	}

	state->sub_state = new_state;

	state->current_ele = (LO_Element *)subdoc;

	if (is_a_header != FALSE)
	{
		LO_TextAttr *old_attr;
		LO_TextAttr *attr;
		LO_TextAttr tmp_attr;

		old_attr = new_state->font_stack->text_attr;
		lo_CopyTextAttr(old_attr, &tmp_attr);
		tmp_attr.fontmask |= LO_FONT_BOLD;
		attr = lo_FetchTextAttr(new_state, &tmp_attr);

		lo_PushFont(new_state, P_TABLE_HEADER, attr);
	}

	/*
	 * Move our current state down into the new sub-state.
	 * Fetch the tag list from the old subdoc's state in preparation
	 * for relayout.
	 */
	
	state = new_state;
	/*
	tag_ptr = cell_ptr->subdoc_tags;
	tag_end_ptr = cell_ptr->subdoc_tags_end;
	cell_ptr->subdoc_tags = NULL;
	cell_ptr->subdoc_tags_end = NULL;
	
	if (tag_end_ptr != NULL)
	{
		tag_end_ptr = tag_end_ptr->next;
	}
	*/

	/*
	 * Copy some cell attribute state from the old cell to the subdoc
	 * so that we retain the correct state. Make sure that the bg_color
	 * doesn't get freed.
	 */
	subdoc->horiz_alignment = cell_ptr->horiz_alignment;
	subdoc->vert_alignment = cell_ptr->vert_alignment;
	subdoc->backdrop = cell->backdrop;
	/*
	cell->backdrop.bg_color = NULL;
	cell->backdrop.url = NULL;
	*/
	
	/*
	 * Clean up memory used by old cell.
	 */
	/*
	elem_list = cell->cell_list;
	cell->next = NULL;
	cell->cell_list = NULL;
	cell->cell_list_end = NULL;
	lo_relayout_recycle(context, state, (LO_Element *)cell);
	*/

	/*
	 * Relayout all the goodies
	 */
	/*
	lo_RelayoutTags(context, state, tag_ptr, tag_end_ptr, elem_list);
	*/

	/*
	 * Link the end of this subdoc's tag chain back to
	 * its parent.  The beginning should have never been
	 * unlinked from the parent.
	 */
	/*
	if (state->subdoc_tags_end != NULL)
	{
		state->subdoc_tags_end->next = tag_end_ptr;
	}
	*/

	/*
	 * makes sure we are at the bottom
	 * of everything in the document.
	 */

	lo_rl_ReflowCell( context, state, cell );
	
	/* Replaced with lo_EndLayoutDuringReflow()
	lo_CloseOutLayout(context, state);
	*/
	
	lo_EndLayoutDuringReflow( context, state );
	

	old_state = state;
	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	state = top_state->doc_state;

	while ((state->sub_state != NULL)&&
		(state->sub_state != old_state))
	{
		state = state->sub_state;
	}

	/*
	 * Ok, now reset the global form element counters
	 * to their original values.
	 */
	if ((doc_lists->form_list != NULL)||(hold_form_list != NULL))
	{
		lo_FormData *form_list;
		lo_FormData *new_form_list;
		lo_FormData *tmp_fptr;

		/*
		 * Remove any new forms created in this subdocs to a
		 * separate list.
		 */
		form_list = doc_lists->form_list;
		new_form_list = NULL;
		while ((form_list != NULL)&&
			(form_list->id > hold_form_id))
		{
			tmp_fptr = form_list;
			form_list = form_list->next;
			tmp_fptr->next = new_form_list;
			new_form_list = tmp_fptr;
		}

		/*
		 * Restore the element count in the form we started in
		 * because the subdoc may have left it half finished.
		 */
		if (form_list != NULL)
		{
			form_list->form_ele_cnt = hold_form_ele_cnt;
		}

		/*
		 * Merge the newly created forms with the old ones
		 * since the new ones may have partially complete
		 * data.
		 */
		while ((new_form_list != NULL)&&(hold_form_list != NULL))
		{
			tmp_fptr = lo_merge_form_data(new_form_list,
					hold_form_list);
			new_form_list = new_form_list->next;
			hold_form_list = hold_form_list->next;
			tmp_fptr->next = form_list;
			form_list = tmp_fptr;
		}

		/*
		 * Add back on forms that are from later subdocs.
		 */
		while (hold_form_list != NULL)
		{
			tmp_fptr = hold_form_list;
			hold_form_list = hold_form_list->next;
			tmp_fptr->next = form_list;
			form_list = tmp_fptr;
		}

		doc_lists->form_list = form_list;
		doc_lists->current_form_num = hold_current_form_num;
	}

	/*
	 * Restore in form status to what is
	 * was before this relayout.
	 */
	top_state->in_form = hold_in_form;

	if (top_state->savedData.FormList != NULL)
	{
		lo_SavedFormListData *all_form_ele;

		all_form_ele = top_state->savedData.FormList;
		all_form_ele->data_index = hold_form_data_index;
	}

	/*
	 * The master embed index is typically now be back
	 * to its original value, but might not be if we
	 * only had to relayout some (not all) of the embeds
	 * in this subdoc.  So, make sure it's set back to
	 * its original value here.
	 */
	top_state->embed_count = hold_embed_global_index;
	
	/* ditto for tag_count */
	top_state->tag_count = original_tag_count;

	/*
	 * Likewise for the anchor list and image list.
	 */
	doc_lists->url_list_len = hold_url_list_len;
	doc_lists->image_list_count = hold_image_list_count;
        doc_lists->applet_list_count = hold_applet_list_count;
        doc_lists->embed_list_count = hold_embed_list_count;
	top_state->current_layer_num = hold_current_layer_num;
	
	subdoc = NULL;
	cell_ele = NULL;
	if ((state != old_state)&&
		(state->current_ele != NULL))
	{
		lo_DocState *subdoc_state;
		int32 dx, dy;
		int32 base_x, base_y;

		subdoc = lo_EndCellSubDoc(context, state,
				old_state, state->current_ele, TRUE);
		state->sub_state = NULL;
		state->current_ele = NULL;
/*
		if ((state->is_a_subdoc == SUBDOC_CELL)||
			(state->is_a_subdoc == SUBDOC_CAPTION))
		{
			if (old_state->subdoc_tags_end != NULL)
			{
				state->subdoc_tags_end =
					old_state->subdoc_tags_end;
			}
		}
*/
		subdoc_state = (lo_DocState *)(subdoc->state);
		cell_ptr->start_in_form = subdoc_state->start_in_form;
		cell_ptr->form_id = subdoc_state->form_id;
		cell_ptr->form_ele_cnt = subdoc_state->form_ele_cnt;
		cell_ptr->form_data_index = subdoc_state->form_data_index;
		cell_ptr->embed_count_base = subdoc_state->embed_count_base;
		cell_ptr->url_count_base = subdoc_state->url_count_base;
		cell_ptr->image_list_count_base = subdoc_state->image_list_count_base;
		cell_ptr->applet_list_count_base = subdoc_state->applet_list_count_base;
		cell_ptr->embed_list_count_base = subdoc_state->embed_list_count_base;                
		cell_ptr->current_layer_num_base = subdoc_state->current_layer_num_base;
		cell_ptr->must_relayout = subdoc_state->must_relayout_subdoc;
		cell_ptr->is_a_subdoc = subdoc_state->is_a_subdoc;
		/*
		cell_ptr->subdoc_tags = subdoc_state->subdoc_tags;
		cell_ptr->subdoc_tags_end = subdoc_state->subdoc_tags_end;
		*/
		subdoc_state->subdoc_tags = NULL;
		subdoc_state->subdoc_tags_end = NULL;
		cell_ptr->max_y = subdoc_state->y;
		cell_ptr->horiz_alignment = subdoc->horiz_alignment;
		cell_ptr->vert_alignment = subdoc->vert_alignment;
			
		/*
		cell_ele = lo_SmallSquishSubDocToCell(context, state, subdoc,
				&dx, &dy);
		*/

		cell_ele = cell_ptr->cell;
		lo_CreateCellFromSubDoc(context, state, subdoc, cell_ele, &dx, &dy);

		lo_cleanup_old_state(subdoc->state);
		base_x = cell_ele->x + cell_ele->x_offset + cell_ele->border_width;
		base_y = cell_ele->y + cell_ele->y_offset + cell_ele->border_width;
		dx -= base_x;
		dy -= base_y;
		cell_ptr->cell_base_x = dx;
		cell_ptr->cell_base_y = dy;
	}

	XP_ASSERT ( cell_ele != NULL );

	old_state->must_relayout_subdoc = FALSE;

	state->top_state->force_reload = save_force;

	/* pop the style stack once to get rid of the extra dummy tag
	 * that we added to the stack to force styles on the begin
	 * subdoc in relayout
	 */
	if (relayout == FALSE)
	{
		LO_PopStyleTagByIndex(context, &state, P_TABLE_DATA, 0);
	}

	return(cell_ele);
}

/*
 * if normal borders is TRUE draw cell borders if the rest of
 * the table has borders.  If FALSE, never draw cell borders
 */
void
lo_BeginTableCellAttributes(MWContext *context, 
 							lo_DocState *state, 
 							lo_TableRec *table,
 							char * colspan_attr,
							char * rowspan_attr,
 							char * nowrap_attr,
 							char * bgcolor_attr,
 							char * background_attr,
                            lo_TileMode tile_mode,
 							char * valign_attr,
 							char * halign_attr,
  							char * width_attr,
							char * height_attr,
							Bool is_a_header,
							Bool normal_borders)
{
	lo_TableRow *table_row;
	lo_TableCell *table_cell;
	int32 val;
#ifdef LOCAL_DEBUG
 fprintf(stderr, "lo_BeginTableCell called\n");
#endif /* LOCAL_DEBUG */

	table_row = table->row_ptr;

	table_cell = XP_NEW(lo_TableCell);
	if (table_cell == NULL)
	{
		return;
	}

	if (state->is_a_subdoc != SUBDOC_NOT) 
	{
		table_cell->in_nested_table = TRUE;
	}
	else 
	{
		table_cell->in_nested_table = FALSE;
	}	

	/* Copied to lo_InitForBeginCell()
	table_cell->must_relayout = FALSE;
	*/
	table_cell->subdoc_tags = NULL;
	table_cell->subdoc_tags_end = NULL;

	/* Copied to lo_InitForBeginCell()
	table_cell->cell_done = FALSE;
	*/
	table_cell->nowrap = FALSE;
	table_cell->is_a_header = is_a_header;
	table_cell->percent_width = 0;
	table_cell->percent_height = 0;
	table_cell->specified_width = 0;
	table_cell->specified_height = 0;
	table_cell->min_width = 0;
	table_cell->max_width = 0;
	table_cell->height = 0;
	table_cell->baseline = 0;
	table_cell->rowspan = 1;
	table_cell->colspan = 1;
	table_cell->cell = NULL;
	table_cell->next = NULL;
	table_cell->beginning_tag_count = state->top_state->tag_count;

	lo_InitForBeginCell( table_row, table_cell );
	
    if (table->current_subdoc == NULL)
	{
		XP_DELETE(table_cell);
		return;
	}

	if (colspan_attr)
	{
		val = XP_ATOI(colspan_attr);
		if (val < 1)
		{
			val = 1;
		}
		/*
		 * Prevent huge colspan
		 */
		if (val > TABLE_MAX_COLSPAN)
		{
			val = TABLE_MAX_COLSPAN;
		}
		table_cell->colspan = val;
	}

	if (rowspan_attr)
	{
		val = XP_ATOI(rowspan_attr);
		if (val < 1)
		{
			val = 1;
		}
		/*
		 * Prevent huge rowspan
		 */
		if (val > TABLE_MAX_ROWSPAN)
		{
			val = TABLE_MAX_ROWSPAN;
		}
		table_cell->rowspan = val;
	}

	if (nowrap_attr)
	{
		table_cell->nowrap = TRUE;
	}

	/* Copied to lo_InitForBeginCell()
	if (table_row->cell_list == NULL)
	{
		table_row->cell_list = table_cell;
		table_row->cell_ptr = table_cell;
	}
	else
	{
		table_row->cell_ptr->next = table_cell;
		table_row->cell_ptr = table_cell;
	}
	*/

	lo_BeginCellSubDoc(context, 
						state, 
						table,
						bgcolor_attr,
                        background_attr,
                        tile_mode,
						valign_attr,
						halign_attr,
						width_attr,
						height_attr,
 						is_a_header, 
						normal_borders ? table->draw_borders : 0,
						FALSE);

	/*
	 * We added a new state.
	 */
	lo_PushStateLevel ( context );
}


void
lo_BeginTableCell(MWContext *context, lo_DocState *state, lo_TableRec *table,
	PA_Tag *tag, Bool is_a_header)
{
	char *colspan_attr = (char*)lo_FetchParamValue(context, tag, PARAM_COLSPAN);
	char *rowspan_attr = (char*)lo_FetchParamValue(context, tag, PARAM_ROWSPAN);
	char *nowrap_attr  = (char*)lo_FetchParamValue(context, tag, PARAM_NOWRAP);
	char *bgcolor_attr = (char*)lo_FetchParamValue(context, tag, PARAM_BGCOLOR);
	char *background_attr= (char*)lo_FetchParamValue(context, tag, PARAM_BACKGROUND);
	char *valign_attr  = (char*)lo_FetchParamValue(context, tag, PARAM_VALIGN);
	char *halign_attr  = (char*)lo_FetchParamValue(context, tag, PARAM_ALIGN);
	char *width_attr   = (char*)lo_FetchParamValue(context, tag, PARAM_WIDTH);
	char *height_attr  = (char*)lo_FetchParamValue(context, tag, PARAM_HEIGHT);

	/* remove the PA_LOCK stuff */

	lo_BeginTableCellAttributes(context, 
								state, 
								table,
								colspan_attr,
								rowspan_attr,
								nowrap_attr,
								bgcolor_attr,
                                background_attr,
                                LO_TILE_BOTH, /* Default backdrop tile mode */
								valign_attr,
								halign_attr,
								width_attr,
								height_attr,
								is_a_header,
								TRUE);

	if(colspan_attr)
		PA_FREE(colspan_attr);
	if(rowspan_attr)
		PA_FREE(rowspan_attr);
	if(nowrap_attr)
		PA_FREE(nowrap_attr);
	if(bgcolor_attr)
		PA_FREE(bgcolor_attr);
	if(background_attr)
		PA_FREE(background_attr);
	if(valign_attr)
		PA_FREE(valign_attr);
	if(halign_attr)
		PA_FREE(halign_attr);
	if(width_attr)
		PA_FREE(width_attr);
	if(height_attr)
		PA_FREE(height_attr);
}


void
lo_EndTableCell(MWContext *context, lo_DocState *state, Bool relayout )
{
	LO_SubDocStruct *subdoc;
	LO_CellStruct *cell_ele;
	lo_TableRec *table;
	lo_TableRow *table_row;
	lo_TableCell *table_cell;
	lo_table_span *span_rec;
	int32 top_inner_pad;
	int32 bottom_inner_pad;
	int32 left_inner_pad;
	int32 right_inner_pad;
	int32 i;
	lo_TopState *top_state;
	lo_DocState *old_state;
	int32 doc_id;
#ifdef LOCAL_DEBUG
 fprintf(stderr, "lo_EndTableCell called\n");
#endif /* LOCAL_DEBUG */

	/*
	 * makes sure we are at the bottom
	 * of everything in the document.
	 */
	if (relayout == FALSE) 
	{
		lo_CloseOutLayout(context, state);
	}
	else
	{
		lo_EndLayoutDuringReflow( context, state );
	}

	old_state = state;
	/*
	 * Get the unique document ID, and retrieve this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	state = top_state->doc_state;

	while ((state->sub_state != NULL)&&
		(state->sub_state != old_state))
	{
		state = state->sub_state;
	}

	subdoc = NULL;
	cell_ele = NULL;
	if ((state != old_state)&&
		(state->current_ele != NULL))
	{
		subdoc = lo_EndCellSubDoc(context, state,
				old_state, state->current_ele, relayout);
		state->sub_state = NULL;
		state->current_ele = NULL;
		if ((state->is_a_subdoc == SUBDOC_CELL)||
			(state->is_a_subdoc == SUBDOC_CAPTION))
		{
			if (old_state->subdoc_tags_end != NULL)
			{
				state->subdoc_tags_end =
					old_state->subdoc_tags_end;
			}
		}
		else
		{
		}
	}

	/*
	 * We popped a state level.
	 */
	lo_PopStateLevel ( context );

	table = state->current_table;
	table_row = table->row_ptr;
	table_cell = table_row->cell_ptr;
	
	top_inner_pad = FEUNITS_X(table->inner_top_pad, context);
	bottom_inner_pad = FEUNITS_X(table->inner_bottom_pad, context);
	left_inner_pad = FEUNITS_X(table->inner_left_pad, context);
	right_inner_pad = FEUNITS_X(table->inner_right_pad, context);

	if (subdoc != NULL)
	{
		int32 subdoc_width;
		int32 subdoc_height;
		int32 min_cellwidth;
		int32 base_x, base_y;
		int32 dx, dy;
		int32 cell_internal_width;
		lo_DocState *subdoc_state;
		Bool reset_min_width;
		
		reset_min_width = FALSE;
		
		cell_internal_width = old_state->max_width;
		
		/* how big do we think this cell should be? */
		min_cellwidth = old_state->min_width;

		/*
		* If the cell is smaller than this, we need to grow it so that
		* any internal elements don't overwrite the cell borders.
		*/
		if (subdoc->width < min_cellwidth)
		{
			/*
			* We need to lay things out again as we now have more space.
			* Do we want to be smart and see if we contain things that
			* will change?
			 * Be sure to add in the width of the borders as later code expects it
			*/
			subdoc->width = min_cellwidth + (2 * subdoc->border_width);
			old_state->must_relayout_subdoc = TRUE;
			
			cell_internal_width = min_cellwidth;
		}

		subdoc_width = subdoc->width +
			(2 * subdoc->border_horiz_space) +
			(left_inner_pad + right_inner_pad);
		subdoc_height = subdoc->height +
			(2 * subdoc->border_vert_space) +
			(top_inner_pad + bottom_inner_pad);
		table_cell->max_width = subdoc_width;
		table_cell->min_width = old_state->min_width +
			(2 * subdoc->border_width) +
			(2 * subdoc->border_horiz_space) +
			(left_inner_pad + right_inner_pad);

		/*
		 * This is some nasty logic. We need to reset the min_width
		 * of the cell in cases where we're certain about how large
		 * the cell is.
		 */
		if ( (table_cell->min_width < table_cell->max_width) )
		{
			/*
			 * If we're a fixed width cell and fixed layout (COLS) is on.
			 * BRAIN DAMAGE: It's arguable that we should always do this - a width for
			 * the cell was specified, we should respect it. However, certain popular
			 * sites don't look too good if we do...
			 */
			if ( (table_cell->specified_width > 0) &&
				 (table_cell->percent_width == 0) &&
				 (table->fixed_cols > 0) )
			{
				reset_min_width = TRUE;
			}
			else
			/*
			 * If we're in a fixed width table who's width does not
			 * depend on it's parent and are using fixed layout (COLS).
			 */
			if ( (table->fixed_cols > 0) &&
				 (table->table_width_fixed != FALSE) )
			{
				reset_min_width = TRUE;
			}
		}
		
		/*
		 * If nowrap is on or min_width ended up being bigger than
		 * max_width
		 */
		if ((table_cell->nowrap != FALSE)||
			(table_cell->min_width > table_cell->max_width))
		{
			reset_min_width = TRUE;
		}
		
		if ( reset_min_width != FALSE )
		{
			table_cell->min_width = table_cell->max_width;
		}

		/*
		 * If we're in the fixed layout case, then we may need to grow this
		 * column to fit this new cell width. This will allow any subsequent
		 * columns to be layed out to the correct size and not have to be
		 * relayed out.
		 */
		if ( table->fixed_cols > 0 )
		{
			lo_SetDefaultCellWidth( context, table, subdoc, table_cell, cell_internal_width );
		}
				
		table_cell->height = subdoc_height;
		table_cell->baseline = lo_GetSubDocBaseline(subdoc);

		subdoc_state = (lo_DocState *)(subdoc->state);
		table_cell->start_in_form = subdoc_state->start_in_form;
		table_cell->form_id = subdoc_state->form_id;
		table_cell->form_ele_cnt = subdoc_state->form_ele_cnt;
		table_cell->form_data_index = subdoc_state->form_data_index;
		table_cell->embed_count_base = subdoc_state->embed_count_base;
		table_cell->url_count_base = subdoc_state->url_count_base;
		table_cell->image_list_count_base = subdoc_state->image_list_count_base;
		table_cell->applet_list_count_base = subdoc_state->applet_list_count_base;
		table_cell->embed_list_count_base = subdoc_state->embed_list_count_base;

		table_cell->current_layer_num_base = subdoc_state->current_layer_num_base;
		table_cell->must_relayout = subdoc_state->must_relayout_subdoc;
		table_cell->is_a_subdoc = subdoc_state->is_a_subdoc;
		if (relayout == FALSE)
		{
			table_cell->subdoc_tags = subdoc_state->subdoc_tags;
			table_cell->subdoc_tags_end = subdoc_state->subdoc_tags_end;
		}
		subdoc_state->subdoc_tags = NULL;
		subdoc_state->subdoc_tags_end = NULL;
		table_cell->max_y = subdoc_state->y;
		table_cell->horiz_alignment = subdoc->horiz_alignment;
		table_cell->vert_alignment = subdoc->vert_alignment;

		if (relayout == FALSE)
		{
			cell_ele = lo_SmallSquishSubDocToCell(context, state, subdoc,
				&dx, &dy);
			/* Keep ptr. back to table state structs for relayout */
			cell_ele->table_cell = table_cell;
			cell_ele->table_row = table_row;
			cell_ele->table = table;
		}
		else 
		{
			cell_ele = table_cell->cell;
			lo_CreateCellFromSubDoc(context, state, subdoc, cell_ele, &dx, &dy);
		}

		lo_cleanup_old_state(subdoc->state);
		base_x = cell_ele->x + cell_ele->x_offset + cell_ele->border_width;
		base_y = cell_ele->y + cell_ele->y_offset + cell_ele->border_width;
		dx -= base_x;
		dy -= base_y;
		table_cell->cell_base_x = dx;
		table_cell->cell_base_y = dy;
	}

	table_cell->cell = cell_ele;
	table_cell->cell_done = TRUE;

	i = 0;
	while (i < table_cell->colspan)
	{

		if ((table->width_spans == NULL)||
		    ((table->width_span_ptr != NULL)&&
		     (table->width_span_ptr->next == NULL)))
		{
			span_rec = XP_NEW(lo_table_span);
			if (span_rec == NULL)
			{
				return;
			}
			span_rec->dim = 1;
			span_rec->min_dim = 1;
			span_rec->span = 0;
			span_rec->next = NULL;

			if (table->width_spans == NULL)
			{
				table->width_spans = span_rec;
				table->width_span_ptr = span_rec;
			}
			else
			{
				table->width_span_ptr->next = span_rec;
				table->width_span_ptr = span_rec;
			}
		}
		else
		{
			if (table->width_span_ptr == NULL)
			{
				table->width_span_ptr = table->width_spans;
			}
			else
			{
				table->width_span_ptr =
					table->width_span_ptr->next;
			}
			span_rec = table->width_span_ptr;
		}

		if (span_rec->span > 0)
		{
			span_rec->span--;
			table_row->cells++;

			/*
			 * This only happens if a colspan on this
			 * row overlaps a rowspan from a previous row.
			 */
			if (i > 0)
			{
				int32 new_span;

				new_span = table_cell->rowspan - 1;
				if (new_span > span_rec->span)
				{
					span_rec->span = new_span;
				}
				i++;
				/*
				 * Don't count this cell twice.
				 */
				table_row->cells--;
			}
			continue;
		}
		else
		{
			span_rec->span = table_cell->rowspan - 1;
			i++;
			if (table_cell->colspan == 1)
			{
				if (table_cell->max_width > span_rec->dim)
				{
				    span_rec->dim = table_cell->max_width;
				}
				if (table_cell->min_width > span_rec->min_dim)
				{
				    span_rec->min_dim = table_cell->min_width;
				}
			}
		}
	}

	span_rec = table->height_span_ptr;
	if (table_cell->rowspan == 1)
	{
		int32 tmp_val;

		/*
		 * If the user specified a height for the cell,
		 * that becomes our min height.
		 */
		if ((table_cell->specified_height > 0)&&
			(table_cell->specified_height > span_rec->min_dim))
		{
			span_rec->min_dim = table_cell->specified_height;
		}

		tmp_val = table_cell->baseline - span_rec->min_dim;
		if (tmp_val > 0)
		{
			span_rec->min_dim += tmp_val;
			if (table_row->vert_alignment == LO_ALIGN_BASELINE)
			{
				span_rec->dim += tmp_val;
			}
		}

		if (table_row->vert_alignment == LO_ALIGN_BASELINE)
		{
			tmp_val = (table_cell->height - table_cell->baseline) -
				(span_rec->dim - span_rec->min_dim);
			if (tmp_val > 0)
			{
				span_rec->dim += tmp_val;
			}
		}
		else
		{
			if (table_cell->height > span_rec->dim)
			{
				span_rec->dim = table_cell->height;
			}
		}
	}

	table_row->cells += table_cell->colspan;
}


void
lo_BeginTableCaption(MWContext *context, lo_DocState *state, lo_TableRec *table,
	PA_Tag *tag)
{
	lo_TableCaption *caption;
	PA_Block buff;
	char *str;

	caption = XP_NEW(lo_TableCaption);
	if (caption == NULL)
	{
		return;
	}

	caption->vert_alignment = LO_ALIGN_TOP;
	caption->horiz_alignment = LO_ALIGN_CENTER;
	caption->min_width = 0;
	caption->max_width = 0;
	caption->height = 0;
	caption->subdoc = NULL;
	caption->cell_ele = NULL;

	/*
	 * Check for an align parameter
 	 */
	buff = lo_FetchParamValue(context, tag, PARAM_ALIGN);
	if (buff != NULL)
	{
		PA_LOCK(str, char *, buff);
		if (pa_TagEqual("bottom", str))
		{
			caption->vert_alignment = LO_ALIGN_BOTTOM;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}

	table->caption = caption;

	lo_BeginCaptionSubDoc(context, state, caption, tag);
}


void
lo_EndTableCaption(MWContext *context, lo_DocState *state)
{
	LO_SubDocStruct *subdoc;
	lo_TableRec *table;
	lo_TableCaption *caption;
	lo_TopState *top_state;
	lo_DocState *old_state;
	int32 doc_id;
	int32 top_inner_pad;
	int32 bottom_inner_pad;
	int32 left_inner_pad;
	int32 right_inner_pad;

	/*
	 * makes sure we are at the bottom
	 * of everything in the document.
	 */
	lo_CloseOutLayout(context, state);

	old_state = state;
	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	state = top_state->doc_state;

	while ((state->sub_state != NULL)&&
		(state->sub_state != old_state))
	{
		state = state->sub_state;
	}

	subdoc = NULL;
	if ((state != old_state)&&
		(state->current_ele != NULL))
	{
		subdoc = lo_EndCellSubDoc(context, state,
				old_state, state->current_ele, FALSE);
		state->sub_state = NULL;
		state->current_ele = NULL;
		if ((state->is_a_subdoc == SUBDOC_CELL)||
			(state->is_a_subdoc == SUBDOC_CAPTION))
		{
			if (old_state->subdoc_tags_end != NULL)
			{
				state->subdoc_tags_end =
					old_state->subdoc_tags_end;
			}
		}
	}

	table = state->current_table;
	caption = table->caption;

	top_inner_pad = FEUNITS_X(table->inner_top_pad, context);
	bottom_inner_pad = FEUNITS_X(table->inner_bottom_pad, context);
	left_inner_pad = FEUNITS_X(table->inner_left_pad, context);
	right_inner_pad = FEUNITS_X(table->inner_right_pad, context);

	if (subdoc != NULL)
	{
		int32 subdoc_width;
		int32 subdoc_height;

		subdoc_width = subdoc->width +
			(2 * subdoc->border_horiz_space) +
			(left_inner_pad + right_inner_pad);
		subdoc_height = subdoc->height +
			(2 * subdoc->border_vert_space) + 
			(top_inner_pad + bottom_inner_pad);
		caption->max_width = subdoc_width;
		caption->min_width = old_state->min_width +
			(2 * subdoc->border_width) +
			(2 * subdoc->border_horiz_space) +
			(left_inner_pad + right_inner_pad);
		if (caption->min_width > caption->max_width)
		{
			caption->min_width = caption->max_width;
		}
		caption->height = subdoc_height;
	}

	caption->subdoc = subdoc;

	/* If the caption is empty, free the memory used by it */
	if (lo_subdoc_has_elements(caption->subdoc->state) == FALSE)
	{
		lo_FreeTableCaption( context, state, table );
	}
	
	/*
	 * We popped a state level.
	 */
	lo_PopStateLevel ( context );
}


void
lo_BeginTableRowAttributes(MWContext *context, 
				 			lo_DocState *state, 
				 			lo_TableRec *table,
				 			char *bgcolor_attr,
                            char *background_attr,
				 			char *valign_attr,
				 			char *halign_attr)
{
	lo_TableRow *table_row;
	lo_table_span *span_rec;
	char *bgcolor_from_style=NULL;
#ifdef LOCAL_DEBUG
 fprintf(stderr, "lo_BeginTableRow called\n");
#endif /* LOCAL_DEBUG */

	table_row = XP_NEW(lo_TableRow);
	if (table_row == NULL)
	{
		return;
	}

	/* copied to lo_UpdateTableStateForBeginRow()
	table_row->row_done = FALSE; 
	*/
	table_row->has_percent_width_cells = FALSE;
	table_row->backdrop.bg_color = NULL;
    table_row->backdrop.url = NULL;
    table_row->backdrop.tile_mode = LO_TILE_BOTH;
	/* copied to lo_UpdateTableStateForBeginRow()
	table_row->cells = 0;
	*/
	table_row->vert_alignment = LO_ALIGN_DEFAULT;
	table_row->horiz_alignment = LO_ALIGN_DEFAULT;

	/* copied to lo_UpdateTableStateForBeginRow()
	table_row->cell_list = NULL;
	table_row->cell_ptr = NULL;
	*/
	table_row->next = NULL;
	
	
	lo_UpdateTableStateForBeginRow( table, table_row );

	/* check for style sheet color and override if necessary */
	if(state->top_state && state->top_state->style_stack)
	{
		StyleStruct *style_struct;

		style_struct = STYLESTACK_GetStyleByIndex(
											state->top_state->style_stack,
											0);

		if(style_struct)
		{
			bgcolor_from_style = STYLESTRUCT_GetString(style_struct, BG_COLOR_STYLE);

			if(bgcolor_from_style)
			{
				bgcolor_attr = bgcolor_from_style;
			}
		}
	}

	/*
	 * Check for a background color attribute
	 */
	if (bgcolor_attr)
	{
		uint8 red, green, blue;
		XP_Bool rv;

		if(bgcolor_from_style)
			rv = LO_ParseStyleSheetRGB(bgcolor_attr, &red, &green, &blue);
		else
			rv = LO_ParseRGB(bgcolor_attr, &red, &green, &blue);

		if(rv)
		{
			table_row->backdrop.bg_color = XP_NEW(LO_Color);
			if (table_row->backdrop.bg_color != NULL)
			{
				table_row->backdrop.bg_color->red = red;
				table_row->backdrop.bg_color->green = green;
				table_row->backdrop.bg_color->blue = blue;
			}
		}
	}

    if (background_attr)
        table_row->backdrop.url = XP_STRDUP(background_attr);

	XP_FREEIF(bgcolor_from_style);


	/*
	 * Check for a vertical align parameter
	 */
	if (valign_attr)
	{
		table_row->vert_alignment = lo_EvalVAlignParam(valign_attr);
	}

	/*
	 * Check for a horizontal align parameter
	 */
	if (halign_attr)
	{
		table_row->horiz_alignment = lo_EvalCellAlignParam(halign_attr);
	}

	
	if (table->row_list == NULL)
	{
		table->row_list = table_row;
		table->row_ptr = table_row;
	}
	else
	{
		table->row_ptr->next = table_row;
		table->row_ptr = table_row;
	}

	/* copied to lo_UpdateTableStateForBeginRow()
	table->width_span_ptr = NULL;
	*/

	span_rec = XP_NEW(lo_table_span);
	if (span_rec == NULL)
	{
		return;
	}
	
	if (table->height_spans == NULL)
	{
		table->height_spans = span_rec;
		table->height_span_ptr = span_rec;
	}
	else
	{
		table->height_span_ptr->next = span_rec;
		table->height_span_ptr = span_rec;
	}

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
	span_rec->next = NULL;
}


void
lo_BeginTableRow(MWContext *context, lo_DocState *state, lo_TableRec *table,
	PA_Tag *tag)
{
	char *bgcolor_attr = (char*)lo_FetchParamValue(context, tag, PARAM_BGCOLOR);
	char *background_attr = (char*)lo_FetchParamValue(context, tag, PARAM_BACKGROUND);
	char *valign_attr  = (char*)lo_FetchParamValue(context, tag, PARAM_VALIGN);
	char *halign_attr  = (char*)lo_FetchParamValue(context, tag, PARAM_ALIGN);

	/* remove the PA_LOCK stuff */

	lo_BeginTableRowAttributes(context,
								state, 
								table,
								bgcolor_attr,
                                background_attr,
								valign_attr,
								halign_attr);

	if(bgcolor_attr)
		PA_FREE(bgcolor_attr);
	if(valign_attr)
		PA_FREE(valign_attr);
	if(halign_attr)
		PA_FREE(halign_attr);
}


void
lo_EndTableRow(MWContext *context, lo_DocState *state, lo_TableRec *table)
{
	lo_TableRow *table_row;
#ifdef LOCAL_DEBUG
 fprintf(stderr, "lo_EndTableRow called\n");
#endif /* LOCAL_DEBUG */

	table_row = table->row_ptr;
	table_row->row_done = TRUE;
	table->rows++;

	while (table->width_span_ptr != NULL)
	{
		table->width_span_ptr = table->width_span_ptr->next;
		if (table->width_span_ptr != NULL)
		{
			table->width_span_ptr->span--;
			table_row->cells++;
		}
	}

	if (table_row->cells > table->cols)
	{
		table->cols = table_row->cells;
	}
	
	/*
	 * We've hit the end of a row, if it's the first row and
	 * we haven't allocated all columns yet and table width
	 * is remaining, then set their size now.
	 */
	if ( table->fixed_width_remaining > 0 )
	{
		if ( (table_row->cells < table->fixed_cols) && (table->fixed_col_widths != NULL) )
		{
			int32 count;
			int32 colwidth;
			int32 cell_extra_space;
			int32 left_inner_pad;
			int32 right_inner_pad;
			
			/* how much border/pad space do we need per cell */
			if (table->draw_borders == TABLE_BORDERS_OFF)
			{
				cell_extra_space = 2 * TABLE_DEF_CELL_BORDER;
			}
			else if (table->draw_borders == TABLE_BORDERS_ON)
			{
				cell_extra_space = 2 * TABLE_DEF_CELL_BORDER;
			}
			else
			{
				cell_extra_space = 0;
			}
			
			left_inner_pad = FEUNITS_X(table->inner_left_pad, context);
			right_inner_pad = FEUNITS_X(table->inner_right_pad, context);
			cell_extra_space += left_inner_pad + right_inner_pad;
			
			/* divide space up to the remaining cols */
			colwidth = ( table->fixed_width_remaining / 
				( table->fixed_cols - table_row->cells )) - cell_extra_space;
			
			if ( colwidth < 0 )
			{
				colwidth = 0;
			}
			
			for ( count = table_row->cells; count < table->fixed_cols; ++count )
			{
				table->fixed_col_widths[ count ] = colwidth;
				table->fixed_width_remaining -= colwidth;
			}
			
			/* add any leftover space to the last row */
			table->fixed_col_widths[ table->fixed_cols - 1 ] += 
				table->fixed_width_remaining;
		}
		table->fixed_width_remaining = 0;
	}
}

void
lo_BeginTableAttributes(MWContext *context, 
						lo_DocState *state, 
						char *align_attr,
						char *border_attr,
						char *border_top_attr,
						char *border_bottom_attr,
						char *border_left_attr,
						char *border_right_attr,
						char *border_color_attr,
						char *border_style_attr,
						char *vspace_attr,
						char *hspace_attr,
						char *bgcolor_attr,
						char *background_attr,
						char *width_attr,
						char *height_attr,
						char *cellpad_attr,
						char *toppad_attr,
						char *bottompad_attr,
						char *leftpad_attr,
						char *rightpad_attr,
						char *cellspace_attr,
						char *cols_attr)
{
	lo_TableRec *table;
	LO_TableStruct *table_ele;
	int32 val;
	char *bgcolor_from_style = NULL;
	Bool allow_percent_width;
	Bool allow_percent_height;
#ifdef LOCAL_DEBUG
 fprintf(stderr, "lo_BeginTable called\n");
#endif /* LOCAL_DEBUG */

	if (state == NULL)
	{
		return;
	}

	/* Increment table nesting level (used for passing into lo_CreateCellBackGroundLayer() */
	state->top_state->table_nesting_level++;

	table_ele = (LO_TableStruct *)lo_NewElement(context, state, LO_TABLE, NULL, 0);
	if (table_ele == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}

	table_ele->type = LO_TABLE;
	/* Copied into lo_PositionTableElement() */
	/*
	table_ele->ele_id = NEXT_ELEMENT;
	table_ele->x = state->x;
	table_ele->x_offset = 0;
	table_ele->y = state->y;
	table_ele->y_offset = 0;
	table_ele->width = 0;
	table_ele->height = 0;
	table_ele->line_height = 0;
	*/
	
	table_ele->FE_Data = NULL;
	table_ele->anchor_href = state->current_anchor;

	/*
	 * Default to the current alignment
	 */
	if ( state->align_stack != NULL )
	{
		table_ele->alignment = state->align_stack->alignment;
	}
	else
	{
		table_ele->alignment = LO_ALIGN_LEFT;
	}

	table_ele->border_width = TABLE_DEF_BORDER;
	table_ele->border_top_width = TABLE_DEF_BORDER;
	table_ele->border_bottom_width = TABLE_DEF_BORDER;
	table_ele->border_left_width = TABLE_DEF_BORDER;
	table_ele->border_right_width = TABLE_DEF_BORDER;
	table_ele->border_style = TABLE_DEF_BORDER_STYLE;

	table_ele->border_vert_space = TABLE_DEF_VERTICAL_SPACE;
	table_ele->border_horiz_space = TABLE_DEF_HORIZONTAL_SPACE;

	table_ele->border_color.red = 0;
	table_ele->border_color.green = 0;
	table_ele->border_color.blue = 0;

	table_ele->ele_attrmask = 0;

	table_ele->sel_start = -1;
	table_ele->sel_end = -1;

	/* Copied into lo_PositionTableElement() */
	/*
	table_ele->next = NULL;
	table_ele->prev = NULL;
	*/

	/*
	 * Check for an align parameter
	 */
	if (align_attr)
	{
		Bool floating;

		floating = FALSE;
		table_ele->alignment = lo_EvalAlignParam(align_attr, &floating);
		/*
		 * Only allow left and right (floating) and center.
		 */
		if (floating != FALSE)
		{
			table_ele->ele_attrmask |= LO_ELE_FLOATING;
		}
		else
		{
			/*
			 * Tables can be left, center, right, justify or char alignment.
			 * We only do left, center and right. True left and right will
			 * hit the floating case above. So, we set all alignments to
			 * be either center or left.
			 */
			switch ( table_ele->alignment )
			{
				case LO_ALIGN_LEFT:
				case LO_ALIGN_RIGHT:
				case LO_ALIGN_CENTER:
					break;
					
				case LO_ALIGN_NCSA_CENTER:
					table_ele->alignment = LO_ALIGN_CENTER;
					break;
				
				default:
					table_ele->alignment = LO_ALIGN_LEFT;
					break;
			}
		}
	}

	lo_PositionTableElement(state, table_ele);

	/*
	 * Push our alignment state if we're not floating
	 */

	/* Copied into lo_PositionTableElement() */
	/*
	if ( !(table_ele->ele_attrmask & LO_ELE_FLOATING) )
	{
		lo_PushAlignment(state, P_TABLE_DATA, table_ele->alignment);
	}
	*/

	/*
	 * Get the border parameter.
	 */
	if (border_attr)
	{
		val = XP_ATOI(border_attr);
		if ((val == 0)&&(*border_attr == '0'))
		{
			val = -1;
		}
		else if (val < 1)
		{
			val = 1;
		}
		table_ele->border_width = val;
		table_ele->border_top_width = val;
		table_ele->border_bottom_width = val;
		table_ele->border_left_width = val;
		table_ele->border_right_width = val;
	}
	table_ele->border_width = FEUNITS_X(table_ele->border_width, context);

	/*
	 * Get the top border parameter.
	 */
	if (border_top_attr)
	{
		val = XP_ATOI(border_top_attr);
		if ((val == 0)&&(*border_top_attr == '0'))
		{
			val = -1;
		}
		else if (val < 1)
		{
			val = 1;
		}
		table_ele->border_top_width = val;
	}
	table_ele->border_top_width = FEUNITS_Y(table_ele->border_top_width, context);
	
	/*
	 * Get the bottom border parameter.
	 */
	if (border_bottom_attr)
	{
		val = XP_ATOI(border_bottom_attr);
		if ((val == 0)&&(*border_bottom_attr == '0'))
		{
			val = -1;
		}
		else if (val < 1)
		{
			val = 1;
		}
		table_ele->border_bottom_width = val;
	}
	table_ele->border_bottom_width = FEUNITS_Y(table_ele->border_bottom_width, context);
	
	/*
	 * Get the left border parameter.
	 */
	if (border_left_attr)
	{
		val = XP_ATOI(border_left_attr);
		if ((val == 0)&&(*border_left_attr == '0'))
		{
			val = -1;
		}
		else if (val < 1)
		{
			val = 1;
		}
		table_ele->border_left_width = val;
	}
	table_ele->border_left_width = FEUNITS_X(table_ele->border_left_width, context);
	
	/*
	 * Get the right border parameter.
	 */
	if (border_right_attr)
	{
		val = XP_ATOI(border_right_attr);
		if ((val == 0)&&(*border_right_attr == '0'))
		{
			val = -1;
		}
		else if (val < 1)
		{
			val = 1;
		}
		table_ele->border_right_width = val;
	}
	table_ele->border_right_width = FEUNITS_X(table_ele->border_right_width, context);

	/*
	 * Get the border style parameter.
	 */
	if (border_style_attr)
	{
		int32 border_style;
		
		border_style = BORDER_OUTSET;
		if ( pa_TagEqual("none", border_style_attr) )
		{
			border_style = BORDER_NONE;
		}
		else if ( pa_TagEqual("dotted", border_style_attr) )
		{
			border_style = BORDER_DOTTED;
		}
		else if ( pa_TagEqual("dashed", border_style_attr) )
		{
			border_style = BORDER_DASHED;
		}
		else if ( pa_TagEqual("solid", border_style_attr) )
		{
			border_style = BORDER_SOLID;
		}
		else if ( pa_TagEqual("double", border_style_attr) )
		{
			border_style = BORDER_DOUBLE;
		}
		else if ( pa_TagEqual("groove", border_style_attr) )
		{
			border_style = BORDER_GROOVE;
		}
		else if ( pa_TagEqual("ridge", border_style_attr) )
		{
			border_style = BORDER_RIDGE;
		}
		else if ( pa_TagEqual("inset", border_style_attr) )
		{
			border_style = BORDER_INSET;
		}
		else if ( pa_TagEqual("outset", border_style_attr) )
		{
			border_style = BORDER_OUTSET;
		}
		
		table_ele->border_style = border_style;
	}

	/*
	 * Get the border color parameter.
	 */
	if (border_color_attr)
	{
		uint8 red, green, blue;

		LO_ParseStyleSheetRGB(border_color_attr, &red, &green, &blue);
		table_ele->border_color.red = red;
		table_ele->border_color.green = green;
		table_ele->border_color.blue = blue;
	}
	
	/*
	 * Get the extra vertical space parameter.
	 */
	if (vspace_attr)
	{
		val = XP_ATOI(vspace_attr);
		if (val < 0)
		{
			val = 0;
		}
		table_ele->border_vert_space = val;
	}
	table_ele->border_vert_space = FEUNITS_Y(table_ele->border_vert_space,
						context);

	/*
	 * Get the extra horizontal space parameter.
	 */
	if (hspace_attr)
	{
		val = XP_ATOI(hspace_attr);
		if (val < 0)
		{
			val = 0;
		}
		table_ele->border_horiz_space = val;
	}
	table_ele->border_horiz_space = FEUNITS_X(table_ele->border_horiz_space,
						context);

	table = XP_NEW(lo_TableRec);
	
	/* Keep ptr to table state structure.  Will be needed during relayout */
	table_ele->table = table;

	if (table == NULL)
	{
		return;
	}

	if(table_ele->border_top_width < 0)
			table_ele->border_top_width = 0;
 	if(table_ele->border_bottom_width < 0)
			table_ele->border_bottom_width = 0;
	if(table_ele->border_right_width < 0)
			table_ele->border_right_width = 0;
	if(table_ele->border_left_width < 0)
			table_ele->border_left_width = 0;

	if(table_ele->border_width < 0)
	{
		/* backwards compatibility */
		table->draw_borders = TABLE_BORDERS_GONE;			
		table_ele->border_width = 0;
		table_ele->border_top_width = 0;
		table_ele->border_bottom_width = 0;
		table_ele->border_right_width = 0;
		table_ele->border_left_width = 0;
	}
	else if (table_ele->border_top_width == 0
		&& table_ele->border_bottom_width == 0
		&& table_ele->border_left_width == 0
		&& table_ele->border_right_width == 0)
	{
		table->draw_borders = TABLE_BORDERS_OFF;
	}
	else
	{
		table->draw_borders = TABLE_BORDERS_ON;
	}
	table->has_percent_width_cells = FALSE;
	table->has_percent_height_cells = FALSE;
	table->backdrop.bg_color = NULL;
    table->backdrop.url = NULL;
    table->backdrop.tile_mode = LO_TILE_BOTH;

	/* Copied to lo_InitTableRecord()
	/*
	table->rows = 0;
	table->cols = 0;
	*/

	table->width_spans = NULL;
	table->width_span_ptr = NULL;
	table->height_spans = NULL;
	/* Copied to lo_InitTableRecord()
	table->height_span_ptr = NULL;
	*/
	table->caption = NULL;
	table->table_ele = table_ele;
	table->current_subdoc = NULL;
	table->row_list = NULL;
	table->row_ptr = NULL;
	table->width = 0;
	table->height = 0;
	
	lo_InitTableRecord( table );

	/*
	 * Percent width, height added for relayout
	 */
	table->percent_width = 0;
	table->percent_height = 0;

	table->inner_top_pad = TABLE_DEF_INNER_CELL_PAD;
	table->inner_bottom_pad = TABLE_DEF_INNER_CELL_PAD;
	table->inner_left_pad = TABLE_DEF_INNER_CELL_PAD;
	table->inner_right_pad = TABLE_DEF_INNER_CELL_PAD;
	table->inter_cell_pad = TABLE_DEF_INTER_CELL_PAD;

	table->current_subdoc = (LO_SubDocStruct *)lo_NewElement(context, state, LO_SUBDOC, NULL, 0);
	table->current_subdoc->type = LO_SUBDOC;
	
	table->current_subdoc->backdrop.bg_color = NULL;
    table->current_subdoc->backdrop.url = NULL;
    table->current_subdoc->backdrop.tile_mode = LO_TILE_BOTH;
	table->current_subdoc->state = lo_NewLayout(context,
		state->win_width, state->win_height, 0, 0, NULL);

	table->default_cell_width = 0;
	table->fixed_width_remaining = 0;
	table->fixed_col_widths = NULL;
	table->table_width_fixed = FALSE;
	table->fixed_cols = 0;	
	
	/*
	 * You can't do percentage widths if the parent's
	 * width is still undecided.
	 */
	allow_percent_width = TRUE;
	allow_percent_height = TRUE;
	if ((state->is_a_subdoc == SUBDOC_CELL)||
		(state->is_a_subdoc == SUBDOC_CAPTION))
	{
		lo_TopState *top_state;
		lo_DocState *new_state;
		int32 doc_id;

		/*
		 * Get the unique document ID, and retreive this
		 * documents layout state.
		 */
		doc_id = XP_DOCID(context);
		top_state = lo_FetchTopState(doc_id);
		new_state = top_state->doc_state;

		while ((new_state->sub_state != NULL)&&
			(new_state->sub_state != state))
		{
			new_state = new_state->sub_state;
		}

		if ((new_state->sub_state == state)&&
			(new_state->current_ele != NULL))
		{
			LO_SubDocStruct *subdoc;

			subdoc = (LO_SubDocStruct *)new_state->current_ele;
			if (subdoc->width == 0)
			{
				allow_percent_width = FALSE;
#ifdef LOCAL_DEBUG
 fprintf(stderr, "Percent width not allowed in this subdoc!\n");
#endif /* LOCAL_DEBUG */
			}
			else
			{
#ifdef LOCAL_DEBUG
 fprintf(stderr, "Percent width IS allowed in this subdoc!\n");
#endif /* LOCAL_DEBUG */
			}
			if (subdoc->height == 0)
			{
				allow_percent_height = FALSE;
			}
		}
	}

	
	/* check for style sheet color and override if necessary */
	if(state->top_state && state->top_state->style_stack)
	{
		StyleStruct *style_struct;

		style_struct = STYLESTACK_GetStyleByIndex(
											state->top_state->style_stack,
											0);

		if(style_struct)
		{
			bgcolor_from_style = STYLESTRUCT_GetString(style_struct, BG_COLOR_STYLE);

			if(bgcolor_from_style)
			{
				bgcolor_attr = bgcolor_from_style;
			}
		}
	}

	/*
	 * Check for a background color attribute
	 */
	if (bgcolor_attr)
	{
		uint8 red, green, blue;
		XP_Bool rv;

		if(bgcolor_from_style)
			rv = LO_ParseStyleSheetRGB(bgcolor_attr, &red, &green, &blue);
		else
			rv = LO_ParseRGB(bgcolor_attr, &red, &green, &blue);
		if(rv)
		{
			table->backdrop.bg_color = XP_NEW(LO_Color);
			if (table->backdrop.bg_color != NULL)
			{
				table->backdrop.bg_color->red = red;
				table->backdrop.bg_color->green = green;
				table->backdrop.bg_color->blue = blue;
			}
		}
	}

    if (background_attr)
        table->backdrop.url = XP_STRDUP(background_attr);
	XP_FREEIF(bgcolor_from_style);


	/*
	 * Get the width and height parameters, in absolute or percentage.
	 * If percentage, make it absolute.
	 */
	if (width_attr)
	{
		Bool is_percent;

		val = lo_ValueOrPercent(width_attr, &is_percent);
		if (is_percent != FALSE)
		{
			table->percent_width = val;
			/*
			if (allow_percent_width == FALSE)
			{
				val = 0;
			}
			else
			{
				val = (state->win_width - state->win_left -
					state->win_right) * val / 100;
			}
			*/
			
		}
		else
		{
			table->percent_width = 0;
			table->table_width_fixed = TRUE;
			val = FEUNITS_X(val, context);
		}
		if (val < 0)
		{
			val = 0;
		}
		table->width = val;
	}
	
	if (height_attr)
	{
		Bool is_percent;

		val = lo_ValueOrPercent(height_attr, &is_percent);
		if (is_percent != FALSE)
		{
			table->percent_height = val;
			/*
			if (allow_percent_height == FALSE)
			{
				val = 0;
			}
			else
			{
				val = (state->win_height - state->win_top -
					state->win_bottom) * val / 100;
			}
			*/
		}
		else
		{
			table->percent_height = 0;
			val = FEUNITS_X(val, context);
		}
		if (val < 0)
		{
			val = 0;
		}
		table->height = val;
	}

	lo_SetTableDimensions(state, table, allow_percent_width, allow_percent_height);

	if (cellpad_attr)
	{
		val = XP_ATOI(cellpad_attr);
		if (val < 0)
		{
			val = 0;
		}
		/* set all our pads to this value */
		table->inner_top_pad = val;
		table->inner_bottom_pad = val;
		table->inner_left_pad = val;
		table->inner_right_pad = val;
	}

	if (toppad_attr)
	{
		val = XP_ATOI(toppad_attr);
		if (val < 0)
		{
			val = 0;
		}
		table->inner_top_pad = val;
	}

	if (bottompad_attr)
	{
		val = XP_ATOI(bottompad_attr);
		if (val < 0)
		{
			val = 0;
		}
		table->inner_bottom_pad = val;
	}

	if (leftpad_attr)
	{
		val = XP_ATOI(leftpad_attr);
		if (val < 0)
		{
			val = 0;
		}
		table->inner_left_pad = val;
	}

	if (rightpad_attr)
	{
		val = XP_ATOI(rightpad_attr);
		if (val < 0)
		{
			val = 0;
		}
		table->inner_right_pad = val;
	}
	
	if (cellspace_attr)
	{
		val = XP_ATOI(cellspace_attr);
		if (val < 0)
		{
			val = 0;
		}
		table->inter_cell_pad = val;
	}

	/*
	 * Get the COLS parameter.
	 */
	if (cols_attr)
	{
		val = XP_ATOI(cols_attr);
		if (val < 0)
		{
			val = 0;
		}
		table_ele->border_horiz_space = val;
		
		/*
		 * If we have a specified number of columns, then
		 * we first need to make sure we have a real table
		 * width and then compute our default column width.
		 */
		table->fixed_cols = val;		
		if ( val > 0 )
		{
		
			/* Copied to lo_CalcFixedColWidths() */
			/*
			int32 count;
			int32 table_width;
			
			table->fixed_cols = val;
			
			table_width = lo_ComputeInternalTableWidth ( context, table, state );
			*/

			/* Split the space up evenly */
			/*
			table->default_cell_width = table_width / val;
			*/

			/* and we have all the table width left to play with */
			/*
			table->fixed_width_remaining = table_width;
			*/
			
			/* allocate and initialize our width array */			
			table->fixed_col_widths = XP_ALLOC(val * sizeof(int32));
			if (table->fixed_col_widths == NULL)
			{
				state->top_state->out_of_memory = TRUE;
				table->fixed_cols = 0;
				table->default_cell_width = 0;
				return;
			}


			/*
			for ( count = 0; count < val; ++count )
			{
				table->fixed_col_widths[ count ] = 0;
			}
			*/

			lo_CalcFixedColWidths( context, state, table );
		}
	}

	/* Copied to lo_UpdateStateAfterBeginTable() */
	/* 
	state->current_table = table;
	*/
	
	lo_UpdateStateAfterBeginTable( state, table );
}


/*
 * Preparse the tag attributes and call the real begin table
 */
void
lo_BeginTable(MWContext *context, lo_DocState *state, PA_Tag *tag)
{

	/* style sheets variables */
	StyleStruct *style_struct=NULL;
	SS_Number *top_padding, *bottom_padding, *left_padding, *right_padding;

    char *align_attr =    	(char*)lo_FetchParamValue(context, tag, PARAM_ALIGN);
    char *border_attr =   	(char*)lo_FetchParamValue(context, tag, PARAM_BORDER);
    char *border_top_attr =		NULL;
    char *border_bottom_attr =	NULL;
    char *border_left_attr =	NULL;
    char *border_right_attr =	NULL;
    char *border_color_attr =	(char*)lo_FetchParamValue(context, tag, PARAM_BORDERCOLOR);
    char *border_style_attr =	NULL;
    char *vspace_attr =   	(char*)lo_FetchParamValue(context, tag, PARAM_VSPACE);
    char *hspace_attr =   	(char*)lo_FetchParamValue(context, tag, PARAM_HSPACE);
	char *bgcolor_attr =   	(char*)lo_FetchParamValue(context, tag, PARAM_BGCOLOR);
	char *background_attr =  	(char*)lo_FetchParamValue(context, tag, PARAM_BACKGROUND);
	char *width_attr =     	(char*)lo_FetchParamValue(context, tag, PARAM_WIDTH);
	char *height_attr =   	(char*)lo_FetchParamValue(context, tag, PARAM_HEIGHT);
	char *cellpad_attr =   	(char*)lo_FetchParamValue(context, tag, PARAM_CELLPAD);
	char *toppad_attr =   	(char*)lo_FetchParamValue(context, tag, PARAM_TOPPAD);
	char *bottompad_attr =  (char*)lo_FetchParamValue(context, tag, PARAM_BOTTOMPAD);
	char *leftpad_attr =   	(char*)lo_FetchParamValue(context, tag, PARAM_LEFTPAD);
	char *rightpad_attr =   (char*)lo_FetchParamValue(context, tag, PARAM_RIGHTPAD);
	char *cellspace_attr = 	(char*)lo_FetchParamValue(context, tag, PARAM_CELLSPACE);
	char *cols_attr = 		(char*)lo_FetchParamValue(context, tag, PARAM_COLS);

	if(!border_style_attr)
	{
		border_style_attr = XP_STRDUP("outset"); 
	}

	if(!border_color_attr)
	{
		XP_ASSERT(state);
		if(state)
			border_color_attr = PR_smprintf("#%2x%2x%2x", 
								   state->text_bg.red,
								   state->text_bg.green,
								   state->text_bg.blue);
	}

	if(state->top_state->style_stack)
		style_struct = STYLESTACK_GetStyleByIndex(state->top_state->style_stack, 0);

	if(style_struct)
	{
		left_padding = STYLESTRUCT_GetNumber(style_struct, LEFTPADDING_STYLE);
		if(left_padding)
		{
			LO_AdjustSSUnits(left_padding, LEFTPADDING_STYLE, context, state);
			XP_FREEIF(leftpad_attr);
			leftpad_attr = PR_smprintf("%ld", (int32)left_padding->value);
		}

		right_padding = STYLESTRUCT_GetNumber(style_struct, RIGHTPADDING_STYLE);
		if(right_padding)
		{
			LO_AdjustSSUnits(right_padding, RIGHTPADDING_STYLE, context, state);
			XP_FREEIF(rightpad_attr);
			rightpad_attr = PR_smprintf("%ld", (int32)right_padding->value);
		}

		top_padding = STYLESTRUCT_GetNumber(style_struct, TOPPADDING_STYLE);
		if(top_padding)
		{
			LO_AdjustSSUnits(top_padding, TOPPADDING_STYLE, context, state);
			XP_FREEIF(toppad_attr);
			toppad_attr = PR_smprintf("%ld", (int32)top_padding->value);
		}

		bottom_padding = STYLESTRUCT_GetNumber(style_struct, BOTTOMPADDING_STYLE);
		if(bottom_padding)
		{
			LO_AdjustSSUnits(bottom_padding, BOTTOMPADDING_STYLE, context, state);
			XP_FREEIF(bottompad_attr);
			bottompad_attr = PR_smprintf("%ld", (int32)bottom_padding->value);
		}
	}

	/* remove the PA_LOCK stuff, it does nothing anyways */

	lo_BeginTableAttributes(context, 
							state, 
							align_attr,
							border_attr,
							border_top_attr,
							border_bottom_attr,
							border_left_attr,
							border_right_attr,
							border_color_attr,
							border_style_attr,
							vspace_attr,
							hspace_attr,
							bgcolor_attr,
                            background_attr,
							width_attr,
							height_attr,
							cellpad_attr,
							toppad_attr,
							bottompad_attr,
							leftpad_attr,
							rightpad_attr,
							cellspace_attr,
							cols_attr);

	if(align_attr)
		PA_FREE(align_attr);
	if(border_attr)
		PA_FREE(border_attr);
	if(border_top_attr)
		PA_FREE(border_top_attr);
	if(border_bottom_attr)
		PA_FREE(border_bottom_attr);
	if(border_left_attr)
		PA_FREE(border_left_attr);
	if(border_right_attr)
		PA_FREE(border_right_attr);
	if(border_color_attr)
		PA_FREE(border_color_attr);
	if(border_style_attr)
		PA_FREE(border_style_attr);
	if(vspace_attr)
		PA_FREE(vspace_attr);
	if(hspace_attr)
		PA_FREE(hspace_attr);
	if(bgcolor_attr)
		PA_FREE(bgcolor_attr);
	if(background_attr)
		PA_FREE(background_attr);
	if(width_attr)
		PA_FREE(width_attr);
	if(height_attr)
		PA_FREE(height_attr);
	if(cellpad_attr)
		PA_FREE(cellpad_attr);
	if(toppad_attr)
		PA_FREE(toppad_attr);
	if(bottompad_attr)
		PA_FREE(bottompad_attr);
	if(leftpad_attr)
		PA_FREE(leftpad_attr);
	if(rightpad_attr)
		PA_FREE(rightpad_attr);
	if(cellspace_attr)
		PA_FREE(cellspace_attr);
	if(cols_attr)
		PA_FREE(cols_attr);
}


static void
lo_fill_cell_array(lo_TableRec *table, lo_cell_data XP_HUGE *cell_array,
	lo_TableCell *blank_cell, int32 cell_pad, Bool *relayout_pass)
{
	int32 x, y;
	int32 indx;
	lo_table_span *row_max;
	lo_table_span *col_max;
	lo_TableRow *row_ptr;
	lo_TableCell *cell_ptr;

	row_max = table->height_spans;
	row_ptr = table->row_list;
	for (y = 0; y < table->rows; y++)
	{
		x = 0;
		col_max = table->width_spans;
		cell_ptr = row_ptr->cell_list;
		row_ptr->cells_in_row = 0;
		while (cell_ptr != NULL)
		{
			/*
			 * Also on this pass check if any of the cells
			 * NEED to be relaid out later.
			 */
			if (cell_ptr->must_relayout != FALSE)
			{
				*relayout_pass = TRUE;
			}

			/*
			 * "fix" up badly specified row spans.
			 */
			if ((y + cell_ptr->rowspan) > table->rows)
			{
				cell_ptr->rowspan = table->rows - y;
				if (cell_ptr->rowspan == 1)
				{
					if (cell_ptr->height > row_max->dim)
					{
						row_max->dim = cell_ptr->height;
					}
				}
			}

			indx = (y * table->cols) + x;

			if (cell_array[indx].cell == blank_cell)
			{
				x++;
				col_max = col_max->next;
				continue;
			}

			/*
			 * If the cell is not an empty cell, count it for this row
			 */
			/* if (cell_ptr->cell->cell_list != NULL || cell_ptr->cell->cell_float_list != NULL) */
				row_ptr->cells_in_row++;

			cell_array[indx].cell = cell_ptr;
			cell_array[indx].width = cell_ptr->max_width;
			cell_array[indx].height = cell_ptr->height;

			if (cell_ptr->colspan > 1)
			{
				int32 i;
				int32 width, min_width;
				lo_table_span *max_ptr;

				max_ptr = col_max;

				width = max_ptr->dim;
				min_width = max_ptr->min_dim;
				for (i=1; i < cell_ptr->colspan; i++)
				{
					cell_array[indx + i].cell = blank_cell;
					max_ptr = max_ptr->next;
					width = width + cell_pad + max_ptr->dim;
					min_width = min_width + cell_pad +
						max_ptr->min_dim;
				}

				if (width < cell_ptr->max_width)
				{
					int32 add_width;
					int32 add;
					lo_table_span *add_ptr;

					add_ptr = col_max;
					add_width = cell_ptr->max_width -
						width;
					add = 0;
					while (add_ptr != max_ptr)
					{
						int32 newWidth;

						newWidth = add_width *
							add_ptr->dim / width;
						add_ptr->dim += newWidth;
						add += newWidth;
						add_ptr = add_ptr->next;
					}
					add_ptr->dim += (add_width - add);
				}

				if (min_width < cell_ptr->min_width)
				{
					int32 add_width;
					int32 add;
					lo_table_span *add_ptr;

					add_ptr = col_max;
					add_width = cell_ptr->min_width -
						min_width;
					add = 0;
					while (add_ptr != max_ptr)
					{
						int32 newWidth;

						newWidth = add_width *
							add_ptr->min_dim /
							min_width;
						/*
						 * We are not allowed to add
						 * enough to put min_dim > dim.
						 */
						if ((add_ptr->min_dim +
						     newWidth) > add_ptr->dim)
						{
						    newWidth = add_ptr->dim -
							add_ptr->min_dim;
						    /*
						     * don't let newWidth become
						     * negative.
						     */
						    if (newWidth < 0)
						    {
								newWidth = 0;
						    }
						}
						add_ptr->min_dim += newWidth;
						add += newWidth;
						add_ptr = add_ptr->next;
					}
					add_ptr->min_dim += (add_width - add);
				}
				col_max = max_ptr;
			}

			if (cell_ptr->rowspan > 1)
			{
				int32 i;
				int32 height;
				int32 tmp_val;
				lo_table_span *max_ptr;

				max_ptr = row_max;

				height = max_ptr->dim;
				for (i=1; i < cell_ptr->rowspan; i++)
				{
				    cell_array[indx + (i * table->cols)].cell =
						blank_cell;
				    if (cell_ptr->colspan > 1)
				    {
						int32 j;

						for (j=1; j < cell_ptr->colspan; j++)
						{
						    cell_array[indx +
							(i * table->cols) + j].cell =
							blank_cell;
						}
				    }
				    max_ptr = max_ptr->next;
				    height = height + cell_pad + max_ptr->dim;
				}

				tmp_val = cell_ptr->baseline - row_max->min_dim;
				if (tmp_val > 0)
				{
					row_max->min_dim += tmp_val;
					if (row_ptr->vert_alignment ==
						LO_ALIGN_BASELINE)
					{
						row_max->dim += tmp_val;
						height += tmp_val;
					}
					/*
					 * Baseline spacing shouldn't grow the
					 * max height, except for baseline
					 * aligned rows as above.
					 */
					if (row_max->min_dim > row_max->dim)
					{
						row_max->min_dim = row_max->dim;
					}
				}

				if (height < cell_ptr->height)
				{
					int32 add_height;
					int32 add;
					lo_table_span *add_ptr;

					add_ptr = row_max;
					add_height = cell_ptr->height -
						height;
					add = 0;
					while (add_ptr != max_ptr)
					{
						int32 newHeight;

						newHeight = add_height *
							add_ptr->dim / height;
						add_ptr->dim += newHeight;
						add += newHeight;
						add_ptr = add_ptr->next;
					}
					add_ptr->dim += (add_height - add);
				}
			}

			x += cell_ptr->colspan;

			col_max = col_max->next;
			cell_ptr = cell_ptr->next;
			cell_array[indx].cell->next = NULL;
		}
		row_ptr = row_ptr->next;
		row_max = row_max->next;
	}
}

static void
lo_percent_width_cells(lo_TableRec *table, lo_cell_data XP_HUGE *cell_array,
	lo_TableCell *blank_cell, int32 cell_pad, int32 table_pad,
	int32 *table_width, int32 *min_table_width,
	int32 *base_table_width, int32 *min_base_table_width)
{
	int32 x, y;
	int32 indx;
	int32 new_table_width;
	int32 new_min_table_width;
	int32 new_base_table_width;
	int32 new_min_base_table_width;
	Bool need_pass_two;
	lo_table_span *row_max;
	lo_table_span *col_max;
	lo_TableRow *row_ptr;
	lo_TableCell *cell_ptr;

	new_table_width = 0;
	row_ptr = table->row_list;
	row_max = table->height_spans;
	for (y=0; y < table->rows; y++)
	{
	    if (row_ptr->has_percent_width_cells != FALSE)
	    {
			int32 reserve;
			int32 unknown, unknown_base;

			unknown = 0;
			unknown_base = 0;
			reserve = 100 - table->cols;
			col_max = table->width_spans;
			for (x=0; x < table->cols; x++)
			{
				indx = (y * table->cols) + x;
				cell_ptr = cell_array[indx].cell;
				if ((cell_ptr == blank_cell)||
					(cell_ptr == NULL))
				{
					col_max = col_max->next;
					continue;
				}
				if (cell_ptr->percent_width > 0)
				{
					int32 width;

					reserve += cell_ptr->colspan;
					if (cell_ptr->percent_width > reserve)
					{
					    cell_ptr->percent_width = reserve;
					    reserve = 0;
					}
					else
					{
					    reserve -= cell_ptr->percent_width;
					}
					width = cell_ptr->max_width * 100 /
						cell_ptr->percent_width;
					if (width > new_table_width)
					{
						new_table_width = width;
					}
				}
				else
				{
					unknown++;
					unknown_base += cell_ptr->max_width;
				}
				col_max = col_max->next;
			}
			if (unknown)
			{
			    col_max = table->width_spans;
			    for (x=0; x < table->cols; x++)
			    {
				indx = (y * table->cols) + x;
				cell_ptr = cell_array[indx].cell;
				if ((cell_ptr == blank_cell)||
					(cell_ptr == NULL))
				{
					col_max = col_max->next;
					continue;
				}
				if (cell_ptr->percent_width == 0)
				{
					int32 width;
					int32 percent;

					if (unknown == 1)
					{
					    percent = reserve;
					}
					else
					{
					    percent = reserve *
							cell_ptr->max_width /
							unknown_base;
					}
					reserve -= percent;
					if (reserve < 0)
					{
					    reserve = 0;
					}
					percent += cell_ptr->colspan;
					cell_ptr->percent_width = percent;

					width = cell_ptr->max_width * 100 /
						cell_ptr->percent_width;
					if (width > new_table_width)
					{
						new_table_width = width;
					}

					unknown--;
				}
				col_max = col_max->next;
			    }
			}
	    }
	    else
	    {
			int32 width;

			width = 0;
			col_max = table->width_spans;
			for (x=0; x < table->cols; x++)
			{
				indx = (y * table->cols) + x;
				cell_ptr = cell_array[indx].cell;
				if ((cell_ptr == blank_cell)||
					(cell_ptr == NULL))
				{
					col_max = col_max->next;
					continue;
				}
				width += cell_ptr->max_width;
				col_max = col_max->next;
			}
			if (width > new_table_width)
			{
				new_table_width = width;
			}
	    }
	    row_ptr = row_ptr->next;
	    row_max = row_max->next;
	}

	if (*table_width > new_table_width)
	{
		new_table_width = *table_width;
	}

	/*
	 * If we already know how wide this table must be
	 * Use that width when calculate percentage cell widths.
	 */
	if ((table->width > 0)&&(table->width >= *min_table_width))
	{
		new_table_width = table->width;
	}

	need_pass_two = FALSE;

	col_max = table->width_spans;
	for (x=0; x < table->cols; x++)
	{
		int32 current_max;

		current_max = col_max->dim;
		col_max->dim = 1;
		row_max = table->height_spans;
		for (y=0; y < table->rows; y++)
		{
			indx = (y * table->cols) + x;
			cell_ptr = cell_array[indx].cell;
			if ((cell_ptr == blank_cell)||
				(cell_ptr == NULL))
			{
				row_max = row_max->next;
				continue;
			}
			if ((cell_ptr->percent_width > 0)&&
				(cell_ptr->colspan == 1))
			{
				int32 p_width;

				p_width = new_table_width *
					cell_ptr->percent_width / 100;
				if (p_width < cell_ptr->min_width)
				{
					p_width = cell_ptr->min_width;
				}
				if (p_width > col_max->dim)
				{
					col_max->dim = p_width;
				}
			}
			else if (cell_ptr->colspan > 1)
			{
				need_pass_two = TRUE;
			}
			else
			{
				if (cell_ptr->max_width > col_max->dim)
				{
					col_max->dim =
						cell_ptr->max_width;
				}
			}
			row_max = row_max->next;
		}
		if (col_max->dim < col_max->min_dim)
		{
			col_max->dim = col_max->min_dim;
		}
		col_max = col_max->next;
	}
	/*
	 * Take care of spanning columns if any
	 */
	if (need_pass_two != FALSE)
	{
	    row_max = table->height_spans;
	    for (y=0; y < table->rows; y++)
	    {
			col_max = table->width_spans;
			for (x=0; x < table->cols; x++)
			{
				indx = (y * table->cols) + x;
				cell_ptr = cell_array[indx].cell;
				if ((cell_ptr == blank_cell)||
					(cell_ptr == NULL))
				{
					col_max = col_max->next;
					continue;
				}
				if (cell_ptr->colspan > 1)
				{
					int32 i;
					int32 width;
					lo_table_span *max_ptr;
					int32 p_width;
					int32 new_width;

					new_width = cell_ptr->max_width;
				    if (cell_ptr->percent_width > 0)
				    {
						p_width = new_table_width *
							cell_ptr->percent_width / 100;
						if (p_width >= cell_ptr->min_width)
						{
							new_width = p_width;
						}
				    }

					max_ptr = col_max;

					width = max_ptr->dim;
					for (i=1; i < cell_ptr->colspan; i++)
					{
						max_ptr = max_ptr->next;
						width = width + cell_pad +
							max_ptr->dim;
					}

					if (width < new_width)
					{
						int32 add_width;
						int32 add;
						lo_table_span *add_ptr;

						add_ptr = col_max;
						add_width = new_width - width;
						add = 0;
						while (add_ptr != max_ptr)
						{
							int32 newWidth;

							newWidth = add_width *
								add_ptr->dim /
								width;
							add_ptr->dim +=newWidth;
							add += newWidth;
							add_ptr = add_ptr->next;
						}
						add_ptr->dim += (add_width -
							add);
					}
				}
				col_max = col_max->next;
			}
			row_max = row_max->next;
	    }
	}

	new_table_width = 0;
	new_min_table_width = 0;
	new_base_table_width = 0;
	new_min_base_table_width = 0;
	col_max = table->width_spans;
	while (col_max != NULL)
	{
		new_base_table_width += col_max->dim;
		new_min_base_table_width += col_max->min_dim;
		new_table_width = new_table_width + cell_pad +
			col_max->dim;
		new_min_table_width = new_min_table_width + cell_pad +
			col_max->min_dim;
		col_max = col_max->next;
	}
	new_table_width += cell_pad;
	new_table_width += (table->table_ele->border_left_width + 
		table->table_ele->border_right_width);
	new_min_table_width += cell_pad;
	new_min_table_width += (table->table_ele->border_left_width + 
		table->table_ele->border_right_width);
	if (table->draw_borders == TABLE_BORDERS_OFF)
	{
		new_table_width += (2 * table_pad);
		new_min_table_width += (2 * table_pad);
	}

	*table_width = new_table_width;
	*min_table_width = new_min_table_width;
	*base_table_width = new_base_table_width;
	*min_base_table_width = new_min_base_table_width;
}

static void
lo_cell_relayout_pass(MWContext *context, lo_DocState *state,
	lo_TableRec *table, lo_cell_data XP_HUGE *cell_array, lo_TableCell *blank_cell,
	int32 cell_pad, Bool *rowspan_pass, Bool relayout)
{
	int32 x, y;
	int32 indx;
	lo_table_span *row_max;
	lo_table_span *col_max;
	lo_TableRow *row_ptr;
	lo_TableCell *cell_ptr;
	int32 top_inner_pad;
	int32 bottom_inner_pad;
	int32 left_inner_pad;
	int32 right_inner_pad;

	top_inner_pad = FEUNITS_X(table->inner_top_pad, context);
	bottom_inner_pad = FEUNITS_X(table->inner_bottom_pad, context);
	left_inner_pad = FEUNITS_X(table->inner_left_pad, context);
	right_inner_pad = FEUNITS_X(table->inner_right_pad, context);

	row_ptr = table->row_list;
	row_max = table->height_spans;
	for (y=0; y < table->rows; y++)
	{
		Bool max_height_valid;
		Bool changed_row_height;
		int32 max_row_height;
			
		max_height_valid = FALSE;
		changed_row_height = FALSE;
		
		max_row_height = 0;
		
		col_max = table->width_spans;

		for (x=0; x < table->cols; x++)
		{
			LO_CellStruct *cell_struct;
			int32 width;
			int32 inside_width;
			Bool has_elements;

			indx = (y * table->cols) + x;
			cell_ptr = cell_array[indx].cell;
			if ((cell_ptr == blank_cell)||
				(cell_ptr == NULL))
			{
				col_max = col_max->next;
				continue;
			}
			cell_struct = cell_ptr->cell;
			inside_width = col_max->dim;
			width = inside_width;
			if (cell_ptr->colspan > 1)
			{
				int32 i;
				lo_table_span *max_ptr;

				max_ptr = col_max;

				/*
				 * We need to add some cellpads for the spanned cells
				 * to our inside width
				 */
				inside_width += ( cell_ptr->colspan - 1 ) * cell_pad;
				
				for (i=1; i < cell_ptr->colspan; i++)
				{
					max_ptr = max_ptr->next;
					inside_width += max_ptr->dim;
					width += cell_pad + max_ptr->dim;
				}
			}
									
			/*
			 * We want to relayout the cell if it's been tagged as
			 * needing it or if it's been layed out to a different size.
			 */
			if ( (cell_ptr->must_relayout != FALSE) || 
				((width != cell_ptr->max_width)) )
			{
				cell_ptr->must_relayout = FALSE;
				
				inside_width = inside_width -
				    (2 * cell_struct->border_width) -
				    (2 * cell_struct->border_horiz_space) -
				    (left_inner_pad + right_inner_pad);
				    
				/*
				 * Don't relayout documents that have no
				 * elements, it causes errors.
				 */
				has_elements = lo_cell_has_elements(cell_ptr->cell);
				if ( has_elements == FALSE )
				{
					/*
					 * Simply record our new width in this case.
					 */
					 cell_struct->width = inside_width;
				}
				else
				{

					cell_struct = lo_RelayoutCell(context, state,
					    table->current_subdoc, cell_ptr,
					    cell_struct, inside_width,
					    cell_ptr->is_a_header, relayout);

					if (!cell_struct)	/* temporary fix for crash when cell_struct is null */
					{
						break;
					}
					cell_ptr->cell = cell_struct;
					cell_ptr->baseline = lo_GetCellBaseline(cell_struct);
					
					if (cell_ptr->rowspan == 1)
					{
						int32 tmp_val;

						/*
						 * We have changed the height of a single row cell, so we may need
						 * to update the row height later.
						 */
						changed_row_height = TRUE;
						
					    tmp_val = cell_ptr->baseline - row_max->min_dim;
					    if (tmp_val > 0)
					    {
							row_max->min_dim += tmp_val;
							if (row_ptr->vert_alignment ==
								LO_ALIGN_BASELINE)
							{
								row_max->dim += tmp_val;
							}
							/*
							 * Baseline spacing shouldn't grow the
							 * max height, except for baseline
							 * aligned rows as above.
							 */
							if (row_max->min_dim > row_max->dim)
							{
								row_max->min_dim = row_max->dim;
							}
					    }
					}
					else
					{
						*rowspan_pass = TRUE;
					}
				}
			}
			else
			{
				if (cell_ptr->rowspan > 1)
				{
					*rowspan_pass = TRUE;
				}
			}
			
			/*
			 * If this cell has anything inside it, see if it's height
			 * is the biggest for the row.
			 */
			has_elements = lo_cell_has_elements(cell_ptr->cell);
			if ( has_elements != FALSE )
			{
				int32 cell_row_height;

				/*
				 * What row height does this cell want?
				 */
			    cell_row_height = cell_struct->height +
					(top_inner_pad + bottom_inner_pad) +
					(2 * cell_struct->border_vert_space);
				
				/*
				 * Munge the height if the row is vertically aligned along
				 * the baseline and the cell only spans one row.
				 */
			    if (cell_ptr->rowspan == 1)
			    {
			    	if (row_ptr->vert_alignment == LO_ALIGN_BASELINE)
			    	{
				    	cell_row_height = (cell_row_height -
						    cell_ptr->baseline) + row_max->min_dim;
					}
				
					if ( cell_row_height > max_row_height )
					{
						/*
						 * Update to our new maximum and flag that it
						 * contains valid data.
						 */
						max_height_valid = TRUE;
						max_row_height = cell_row_height;
					}
			    }
			}
			col_max = col_max->next;
		}
		
		/*
		 * If we did relayout anything on that row which gave us a new
		 * valid max row height which is different to what we currently
		 * have, then use that.
		 */
		if ( max_height_valid != FALSE && changed_row_height != FALSE &&
			max_row_height != row_max->dim )
		{
			/*
			 * Can't reset height to be smaller than
			 * the minimum height.
			 */
			if (max_row_height >= row_max->min_dim)
			{
				row_max->dim = max_row_height;
			}
			else
			{
				row_max->dim = row_max->min_dim;
			}
		}
		row_max = row_max->next;
		row_ptr = row_ptr->next;
	}

}


static void
lo_cell_rowspan_pass(MWContext *context, lo_TableRec *table, lo_cell_data XP_HUGE *cell_array,
	lo_TableCell *blank_cell, int32 cell_pad)
{
	int32 x, y;
	int32 indx;
	lo_table_span *row_max;
	lo_table_span *col_max;
	lo_TableRow *row_ptr;
	lo_TableCell *cell_ptr;
	int32 top_inner_pad;
	int32 bottom_inner_pad;

	top_inner_pad = FEUNITS_X(table->inner_top_pad, context);
	bottom_inner_pad = FEUNITS_X(table->inner_bottom_pad, context);

	row_max = table->height_spans;
	row_ptr = table->row_list;
	for (y=0; y < table->rows; y++)
	{
		col_max = table->width_spans;
		for (x=0; x < table->cols; x++)
		{
			LO_CellStruct *cell_struct;

			indx = (y * table->cols) + x;
			cell_ptr = cell_array[indx].cell;
			if ((cell_ptr == blank_cell)||
				(cell_ptr == NULL))
			{
				col_max = col_max->next;
				continue;
			}
			cell_struct = cell_ptr->cell;

			if (cell_ptr->rowspan > 1)
			{
				int32 i;
				int32 height;
				int32 tmp_val;
				int32 cell_height;
				lo_table_span *max_ptr;

				max_ptr = row_max;

				height = max_ptr->dim;
				for (i=1; i < cell_ptr->rowspan; i++)
				{
				    max_ptr = max_ptr->next;
				    height = height + cell_pad +
					max_ptr->dim;
				}
				tmp_val = cell_ptr->baseline -
				    row_max->min_dim;
				if (tmp_val > 0)
				{
				    row_max->min_dim += tmp_val;
				    if (row_ptr->vert_alignment ==
						LO_ALIGN_BASELINE)
				    {
						row_max->dim += tmp_val;
						height += tmp_val;
				    }
				    /*
				     * Baseline spacing shouldn't grow the
				     * max height, except for baseline
				     * aligned rows as above.
				     */
				    if (row_max->min_dim > row_max->dim)
				    {
						row_max->min_dim = row_max->dim;
				    }
				}

				cell_height = cell_struct->height +
					(top_inner_pad + bottom_inner_pad) +
					(2 * cell_struct->border_vert_space);

				if (height < cell_height)
				{
					int32 add_height;
					int32 add;
					lo_table_span *add_ptr;

					add_ptr = row_max;
					add_height = cell_height -
						height;
					add = 0;
					while (add_ptr != max_ptr)
					{
						int32 newHeight;

						newHeight = add_height *
							add_ptr->dim /
							height;
						add_ptr->dim +=
							newHeight;
						add += newHeight;
						add_ptr->min_dim = add_ptr->dim;
						add_ptr = add_ptr->next;
					}
					add_ptr->dim +=
						(add_height - add);
					add_ptr->min_dim = add_ptr->dim;
				}
			}
			col_max = col_max->next;
		}
		row_max = row_max->next;
		row_ptr = row_ptr->next;
	}
}


static void
lo_free_cell_record(MWContext *context, lo_DocState *state, lo_TableCell *cell)
{
	if (cell == NULL)
	{
		return;
	}

	/*
	 * If this table cell is not nested inside another table cell
	 * or caption, free any re-parse tags stored on this cell.
	 */
	if ((cell->in_nested_table == FALSE) &&
		(cell->subdoc_tags != NULL)&&
		(state->is_a_subdoc != SUBDOC_CELL)&&
		(state->is_a_subdoc != SUBDOC_CAPTION))
	{
		PA_Tag *tptr;
		PA_Tag *tag;

		tptr = cell->subdoc_tags;
		while ((tptr != cell->subdoc_tags_end)&&(tptr != NULL))
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
		cell->subdoc_tags = NULL;
		cell->subdoc_tags_end = NULL;
	}

	if (cell->cell != NULL) {
		cell->cell->table = NULL;
		cell->cell->table_row = NULL;
		cell->cell->table_cell = NULL;
	}	

	XP_DELETE(cell);
}


static void

lo_free_row_record(MWContext *context, lo_DocState *state,
			lo_TableRow *row, Bool partial)
{
	lo_TableCell *cell_ptr;
	lo_TableCell *cell;

	if (row->cell_list != NULL)
	{
		/*
		 * These are already freed on a completed table, and
		 * need to be freed on a partial table.
		 */

		if (partial != FALSE)
		{
			cell_ptr = row->cell_list;
			while (cell_ptr != NULL)
			{
				cell = cell_ptr;
				cell_ptr = cell_ptr->next;
				lo_free_cell_record(context, state, cell);
			}
		}

		row->cell_list = NULL;
		row->cell_ptr = NULL;
	}

    XP_FREEIF(row->backdrop.bg_color);
    XP_FREEIF(row->backdrop.url);

	XP_DELETE(row);
}


static void lo_FreeTableCaption( MWContext *context, lo_DocState *state, lo_TableRec *table )
{
	if (table->caption != NULL)
	{
		if (table->caption->cell_ele != NULL)
		{
			lo_FreeCaptionCell( context, state, table->caption->cell_ele );
			table->caption->cell_ele = NULL;
		}

		if ( table->caption->subdoc != NULL )
		{
			lo_FreePartialSubDoc ( context, state, table->caption->subdoc );
			table->caption->subdoc = NULL;
		}
		
		XP_DELETE(table->caption);
		table->caption = NULL;
	}
}

void lo_free_table_record(MWContext *context, lo_DocState *state,
			lo_TableRec *table, Bool partial)
{
	if (table->row_list != NULL)
	{
		lo_TableRow *row_ptr;
		lo_TableRow *row;

		row_ptr = table->row_list;
		while (row_ptr != NULL)
		{
			row = row_ptr;
			row_ptr = row_ptr->next;
			lo_free_row_record(context, state, row, partial);
		}
		table->row_list = NULL;
		table->row_ptr = NULL;
	}

	lo_FreeTableCaption( context, state, table );

	if (table->width_spans != NULL)
	{
		lo_table_span *span_ptr;
		lo_table_span *span;

		span_ptr = table->width_spans;
		while (span_ptr != NULL)
		{
			span = span_ptr;
			span_ptr = span_ptr->next;
			XP_DELETE(span);
		}
		table->width_spans = NULL;
		table->width_span_ptr = NULL;
	}

	if (table->height_spans != NULL)
	{
		lo_table_span *span_ptr;
		lo_table_span *span;

		span_ptr = table->height_spans;
		while (span_ptr != NULL)
		{
			span = span_ptr;
			span_ptr = span_ptr->next;
			XP_DELETE(span);
		}
		table->height_spans = NULL;
		table->height_span_ptr = NULL;
	}

    XP_FREEIF(table->backdrop.bg_color);
    XP_FREEIF(table->backdrop.url);

	/*
	 * Whether the partial flag is set or not, we always
	 * want to eliminate this subdoc and all its contents
	 * when the table is done.
	 */
	if (table->current_subdoc != NULL)
	{
		lo_FreePartialSubDoc(context, state, table->current_subdoc);
		table->current_subdoc = NULL;
	}

	if ( table->fixed_col_widths != NULL )
	{
		XP_FREE(table->fixed_col_widths);
	}
	
	/*
	 * Reset back pointer in table layout element
	 */
	if (table->table_ele != NULL) {
		table->table_ele->table = NULL;
	}

	XP_DELETE(table);
}



void
lo_FreePartialTable(MWContext *context, lo_DocState *state, lo_TableRec *table)
{
	lo_TableRow *row_ptr;
	lo_TableCell *cell_ptr;
	LO_SubDocStruct *subdoc;
	LO_CellStruct *cell_struct;

	if (table == NULL)
	{
		return;
	}

	row_ptr = table->row_list;
	while (row_ptr != NULL)
	{
		cell_ptr = row_ptr->cell_list;
		while (cell_ptr != NULL)
		{
			cell_struct = cell_ptr->cell;
			lo_FreePartialCell(context, state, cell_struct);
			cell_ptr->cell = NULL;
			cell_ptr = cell_ptr->next;
		}
		row_ptr = row_ptr->next;
	}

	if (table->caption != NULL)
	{
		subdoc = table->caption->subdoc;
		lo_FreePartialSubDoc(context, state, subdoc);
		table->caption->subdoc = NULL;
	}

	lo_free_table_record(context, state, table, TRUE);
}



void
lo_EndTable(MWContext *context, lo_DocState *state, lo_TableRec *table, Bool relayout)
{
	int32 save_doc_min_width;
	int32 x, y;
	int32 cell_x, cell_y;
	int32 indx;
	int32 cell_cnt;
	int32 ele_cnt;
	int32 table_width, min_table_width;
	int32 base_table_width, min_base_table_width;
	int32 table_height;
	int32 min_table_height;
	int32 width_limit;
	Bool relayout_pass;
	Bool rowspan_pass;
	Bool cut_to_window_width;
	lo_TableCell blank_cell;
	lo_cell_data XP_HUGE *cell_array;
	XP_Block cell_array_buff;
	lo_table_span *row_max;
	lo_table_span *col_max;
	lo_TableCell *cell_ptr;
	int32 cell_pad;
	int32 top_inner_pad;
	int32 bottom_inner_pad;
	int32 left_inner_pad;
	int32 right_inner_pad;
	int32 table_pad;
	Bool floating;
	int32 save_state_x, save_state_y;
	LO_Element *save_line_list;
#ifdef LOCAL_DEBUG
fprintf(stderr, "lo_EndTable called\n");
#endif /* LOCAL_DEBUG */

	cell_pad = FEUNITS_X(table->inter_cell_pad, context);
	table_pad = FEUNITS_X(TABLE_DEF_CELL_BORDER, context);
	
	top_inner_pad = FEUNITS_X(table->inner_top_pad, context);
	bottom_inner_pad = FEUNITS_X(table->inner_bottom_pad, context);
	left_inner_pad = FEUNITS_X(table->inner_left_pad, context);
	right_inner_pad = FEUNITS_X(table->inner_right_pad, context);
	
	relayout_pass = FALSE;
	cut_to_window_width = TRUE;
	floating = FALSE;

	/*
	 * So gcc won't complain
	 */
	save_state_x = 0;
	save_state_y = 0;
	save_line_list = NULL;
/*
	state->current_table = NULL;
*/

	cell_cnt = table->rows * table->cols;
	/*
	 * Empty tables are completely ignored!
	 */
	if (cell_cnt <= 0)
	{
		/*
		 * Clear table state, and free up this structure.
		 */
		state->current_table = NULL;

		/* 
		 * Don't wanna free table record any more because this information
		 * is used during relayout.
		 */
		/*
		if (table != NULL)
		{
			lo_free_table_record(context, state, table, FALSE);
		}		  
		*/
		return;
	}
#ifdef XP_WIN16
	/*
	 * It had better be the case that the size of the struct is a
	 * power of 2
	 */
	XP_ASSERT(sizeof(lo_cell_data) == 8);
	cell_array = (lo_cell_data XP_HUGE *)_halloc(cell_cnt, sizeof(lo_cell_data));

	/*
	 * There isn't a runtime routine that can initialize a huge array
	 */
	if (cell_cnt * sizeof(lo_cell_data) <= 0xFFFFL)
		memset(cell_array, 0, (cell_cnt * sizeof(lo_cell_data)));
	else {
		BYTE XP_HUGE *tmp = (BYTE XP_HUGE *)cell_array;

		for (indx = 0; indx < cell_cnt * sizeof(lo_cell_data); indx++)
			tmp[indx] = 0;
	}
#else
	cell_array_buff = XP_ALLOC_BLOCK(cell_cnt * sizeof(lo_cell_data));
	XP_LOCK_BLOCK(cell_array, lo_cell_data *, cell_array_buff);
	memset(cell_array, 0, (cell_cnt * sizeof(lo_cell_data)));
#endif

	lo_fill_cell_array(table, cell_array, &blank_cell, cell_pad,
		&relayout_pass);

	table_width = 0;
	min_table_width = 0;
	base_table_width = 0;
	min_base_table_width = 0;
	col_max = table->width_spans;
	while (col_max != NULL)
	{
		base_table_width += col_max->dim;
		min_base_table_width += col_max->min_dim;
		table_width = table_width + cell_pad + col_max->dim;
		min_table_width = min_table_width + cell_pad + col_max->min_dim;
		col_max = col_max->next;
	}
	table_width += cell_pad;
	min_table_width += cell_pad;
	table_width += (table->table_ele->border_left_width + 
		table->table_ele->border_right_width);
	min_table_width += (table->table_ele->border_left_width + 
		table->table_ele->border_right_width);
	if (table->draw_borders == TABLE_BORDERS_OFF)
	{
		table_width += (2 * table_pad);
		min_table_width += (2 * table_pad);
	}

	/*
	 * Take care of cells with percentage widths unless we're in
	 * a fixed layout table (has the COLS attribute) in which case
	 * the cell widths are already set.
	 */
	if ((table->has_percent_width_cells != FALSE) &&
		(table->fixed_cols == 0))
	{
		lo_percent_width_cells(table, cell_array, &blank_cell,
			cell_pad, table_pad, &table_width, &min_table_width,
			&base_table_width, &min_base_table_width);
		relayout_pass = TRUE;
	}

	/*
	 * Use state->left_margin here instead of state->win_left to take
	 * indent from lists into account.
	 */
	width_limit = (state->win_width - state->left_margin - state->win_right);
	if (table->width > 0)
	{
		width_limit = table->width;
	}
	/*
	 * Else, if we are a nested table in an infinite width
	 * layout space, we don't want to cut to "window width"
	 */
	else if (state->allow_percent_width == FALSE)
	{
		cut_to_window_width = FALSE;
	}

	/*
	 * If we're in a fixed layout table, then we never
	 * want to cut to "window width" 
	 */
	if ( table->fixed_cols > 0 )
	{
		cut_to_window_width = FALSE;
	}

	/*
	 * If the table is too small, we always need to relayout.
	 */
	if ((min_table_width >= width_limit))
	{
		lo_table_span *sub_ptr;

		relayout_pass = TRUE;
		sub_ptr = table->width_spans;
		while (sub_ptr != NULL)
		{
			sub_ptr->dim = sub_ptr->min_dim;
			sub_ptr = sub_ptr->next;
		}
	}
	else if ((table_width > width_limit)&&(cut_to_window_width != FALSE))
	{
		intn pass;
		Bool expand_failed;
		int32 add_width;
		int32 div_width;
		int32 div_width_next;
		int32 add;
		int32 total_to_add;
		lo_table_span *add_ptr;

		relayout_pass = TRUE;

		expand_failed = TRUE;
		pass = 0;
		total_to_add = width_limit - min_table_width;
		div_width = table_width;
		while ((total_to_add > 0)&&(pass < 10)&&(expand_failed !=FALSE))
		{
			int32 extra;
			int32 min_extra;

			expand_failed = FALSE;
			add_ptr = table->width_spans;
			add_width = total_to_add;
			total_to_add = 0;
			div_width_next = (table->cols + 1) * cell_pad;
			add = 0;
			while (add_ptr->next != NULL)
			{
				if ((pass > 0)&&
					(add_ptr->min_dim == add_ptr->dim))
				{
					add_ptr = add_ptr->next;
					continue;
				}
				extra = add_width * add_ptr->dim /
					div_width;
				if ((add_ptr->min_dim + extra) > add_ptr->dim)
				{
					expand_failed = TRUE;
					min_extra = add_ptr->dim -
						add_ptr->min_dim;
					extra = min_extra;
				}

				add_ptr->min_dim += extra;
				if (add_ptr->min_dim < add_ptr->dim)
				{
					div_width_next += add_ptr->dim;
				}
				add += extra;
				add_ptr = add_ptr->next;
			}
			if ((pass == 0)||(add_ptr->min_dim < add_ptr->dim))
			{
				if (expand_failed == FALSE)
				{
					extra = add_width - add;
				}
				else
				{
					extra = add_width * add_ptr->dim /
						div_width;
				}
				if ((add_ptr->min_dim + extra) > add_ptr->dim)
				{
					expand_failed = TRUE;
					min_extra = add_ptr->dim -
						add_ptr->min_dim;
					extra = min_extra;
				}
				add_ptr->min_dim += extra;
				if (add_ptr->min_dim < add_ptr->dim)
				{
					div_width_next += add_ptr->dim;
				}
				add += extra;
			}
			total_to_add = add_width - add;
			div_width = div_width_next;
			pass++;
		}
		if (total_to_add > 0)
		{
			add = 0;
			add_ptr = table->width_spans;
			while ((add_ptr->next != NULL)&&(total_to_add > 0))
			{
				if ((add_ptr->min_dim + total_to_add) >
					add_ptr->dim)
				{
					add = add_ptr->dim - add_ptr->min_dim;
					total_to_add = total_to_add - add;
					add_ptr->min_dim += add;
				}
				else
				{
					add_ptr->min_dim += total_to_add;
					total_to_add = 0;
				}
				add_ptr = add_ptr->next;
			}
			add_ptr->min_dim += total_to_add;
		}

		add_ptr = table->width_spans;
		while (add_ptr != NULL)
		{
			add_ptr->dim = add_ptr->min_dim;
			add_ptr = add_ptr->next;
		}
	}

	/*
	 * If the user specified a wider width than the minimum
	 * width then we grow the sucker.
	 */
	if ((table->width > 0)&&(width_limit > table_width))
	{
		int32 add_width;
		int32 add;
		int32 newWidth;
		int32 min_add_width;
		int32 min_add;
		lo_table_span *add_ptr;

		/* relayout to fill all the space we're adding */
		relayout_pass = TRUE;
		
		add_ptr = table->width_spans;
		add_width = width_limit - table_width;
		if ( add_width < 0 )
		{
			add_width = 0;
		}
		add = 0;
		min_add_width = width_limit - min_table_width;
		min_add = 0;
		while ((add_ptr != NULL)&&(add_ptr->next != NULL))
		{
			newWidth = add_width * add_ptr->dim / base_table_width;
			add_ptr->dim += newWidth;
			add += newWidth;

			/*
			 * We only want to resize the min_dim's of the spans
			 * if we're a fixed width table.
			 */
			if ( table->table_width_fixed )
			{
				newWidth = min_add_width * add_ptr->min_dim / min_base_table_width;
				add_ptr->min_dim += newWidth;
				min_add += newWidth;
			}
			add_ptr = add_ptr->next;
		}
		if ( table->table_width_fixed )
		{
			add_ptr->min_dim += (min_add_width - min_add);
		}
		add_ptr->dim += (add_width - add);
	}

	rowspan_pass = FALSE;

	if ((state->top_state->doc_state == state) ||
		(state->top_state->in_cell_relayout == TRUE))
	{

		if (state->top_state->doc_state == state)
		{
			state->top_state->in_cell_relayout = TRUE;
		}		

		if (relayout_pass != FALSE)
		{	
			lo_cell_relayout_pass(context, state, table, cell_array,
				&blank_cell, cell_pad, &rowspan_pass, relayout);	
		}

		if (rowspan_pass != FALSE)
		{
			lo_cell_rowspan_pass(context, table, cell_array, &blank_cell,
				cell_pad);
		}

		if (state->top_state->doc_state == state)
		{
			state->top_state->in_cell_relayout = FALSE;
		}
	}

	table_width = 0;
	min_table_width = 0;
	col_max = table->width_spans;
	while (col_max != NULL)
	{
		table_width = table_width + cell_pad + col_max->dim;
		min_table_width = min_table_width + cell_pad + col_max->min_dim;
		col_max = col_max->next;
	}
	table_width += cell_pad;
	table_width += (table->table_ele->border_left_width + 
		table->table_ele->border_right_width);
	min_table_width += cell_pad;
	min_table_width += (table->table_ele->border_left_width + 
		table->table_ele->border_right_width);

	table_height = 0;
	min_table_height = 0;
	row_max = table->height_spans;
	while (row_max != NULL)
	{
		table_height = table_height + cell_pad + row_max->dim;
		min_table_height = min_table_height + cell_pad + row_max->min_dim;
		row_max = row_max->next;
	}
	table_height += cell_pad;
	min_table_height += cell_pad;
	table_height += (table->table_ele->border_top_width + 
		table->table_ele->border_bottom_width);
	min_table_height += (table->table_ele->border_top_width + 
		table->table_ele->border_bottom_width);
	if (table->draw_borders == TABLE_BORDERS_OFF)
	{
		table_width += (2 * table_pad);
		min_table_width += (2 * table_pad);
		table_height += (2 * table_pad);
		min_table_height += (2 * table_pad);
	}


	/*
	 * If the user specified a taller table than the min
	 * height we need to adjust the heights of all the rows.
	 */
	if ((table->height > 0)&&(table->height > min_table_height))
	{
		int32 add_height;
		int32 add;
		lo_table_span *add_ptr;
		int32 newHeight;
		int32 min_add_height;
		int32 min_add;

		add_ptr = table->height_spans;
		add_height = table->height - table_height;
		if ( add_height < 0 )
		{
			add_height = 0;
		}
		min_add_height = table->height - min_table_height;
		add = 0;
		min_add = 0;
		while((add_ptr != NULL) && (add_ptr->next != NULL))
		{
			newHeight = add_height * add_ptr->dim / table_height;
			add_ptr->dim += newHeight;
			add += newHeight;
			
			newHeight = min_add_height * add_ptr->min_dim / table_height;
			add_ptr->min_dim += newHeight;
			min_add += newHeight;
			
			add_ptr = add_ptr->next;
		}
		add_ptr->dim += (add_height - add);
		add_ptr->min_dim += (min_add_height - min_add);

		/*
		 * We can't make the height be less than the sum
		 * of the row heights.
		 */
		if (table_height < table->height)
		{
			table_height = table->height;
		}
		min_table_height = table->height;
	}

	table->table_ele->width = table_width;
	table->table_ele->height = table_height;

	/*
	 * Here, if we are floating we don't do the linebreak.
	 * We do save some state info and then "fake" a linebreak
	 * so we can use common code to place the rest of the table.
	 */
	if (table->table_ele->ele_attrmask & LO_ELE_FLOATING)
	{
		save_line_list = state->line_list;
		save_state_x = state->x;
		save_state_y = state->y;
		floating = TRUE;
		state->x = state->left_margin;
		state->y = state->y + state->line_height;
		state->line_list = NULL;
	}
	else
	{
		/*
		lo_SetSoftLineBreakState(context, state, FALSE, 1);
		*/
		lo_SetLineBreakState ( context, state, FALSE, LO_LINEFEED_BREAK_SOFT, 1, relayout );
	}

	table->table_ele->x = state->x;
	table->table_ele->y = state->y;
	/* Keep the element IDs in order */
	table->table_ele->ele_id = NEXT_ELEMENT;
    
    /*cmanske - Save spacing param for drawing table selection in Composer */
    table->table_ele->inter_cell_space = table->inter_cell_pad;

	lo_AppendToLineList(context, state,
		(LO_Element *)table->table_ele, 0);

#ifdef EDITOR
    /*cmanske - build list of tables we are laying out
     * so Editor can readjust it's data after all is finished 
     */
    EDT_AddToRelayoutTables(context, table->table_ele);
#endif

	state->x += (cell_pad + table->table_ele->border_left_width);
	state->y += (cell_pad + table->table_ele->border_top_width);
	if (table->draw_borders == TABLE_BORDERS_OFF)
	{
		state->x += table_pad;
		state->y += table_pad;
	}

	if (table->caption != NULL)
	{
		int32 new_width;
		LO_SubDocStruct *subdoc;
		Bool has_elements;

		subdoc = table->caption->subdoc;

		col_max = table->width_spans;
		new_width = col_max->dim;
		while (col_max->next != NULL)
		{
			col_max = col_max->next;
			new_width = new_width + cell_pad + col_max->dim;
		}
		/*
		 * Don't relayout documents that have no
		 * elements, it causes errors.
		 */
		has_elements = lo_subdoc_has_elements(subdoc->state);
		if ((new_width < table->caption->max_width)&&
			(has_elements == FALSE))
		{
			int32 inside_width;

			inside_width = new_width -
			    (2 * subdoc->border_width) -
			    (2 * subdoc->border_horiz_space) -
			    (left_inner_pad + right_inner_pad);
			subdoc->width = inside_width;
		}
		else /* if (new_width < table->caption->max_width) */
		{
			int32 inside_width;

			inside_width = new_width -
			    (2 * subdoc->border_width) -
			    (2 * subdoc->border_horiz_space) -
			    (left_inner_pad + right_inner_pad);
			table->caption->subdoc = lo_RelayoutCaptionSubdoc(context,
				state, table->caption, subdoc, inside_width, FALSE);
			subdoc = table->caption->subdoc;

			table->caption->height = subdoc->height +
				(2 * subdoc->border_vert_space) +
				(top_inner_pad + bottom_inner_pad);
			table->caption->max_width = new_width;
		}
		/*
		else
		{
			table->caption->max_width = new_width;
		}
		*/
	}

	state->current_table = NULL;
	cell_x = state->x;
	cell_y = state->y;

	if ((table->caption != NULL)&&
		(table->caption->vert_alignment == LO_ALIGN_TOP))
	{
		LO_SubDocStruct *subdoc;

		cell_x = state->x;
		subdoc = table->caption->subdoc;
		subdoc->x = cell_x;
		subdoc->y = table->table_ele->y;
		subdoc->x_offset = (int16)subdoc->border_horiz_space;
		subdoc->y_offset = (int32)subdoc->border_vert_space;
		subdoc->width = table->caption->max_width -
			(2 * subdoc->border_horiz_space);
		subdoc->height = table->caption->height -
			(2 * subdoc->border_vert_space);
		ele_cnt = lo_align_subdoc(context, state,
			(lo_DocState *)subdoc->state, subdoc, table, NULL);
		if (ele_cnt > 0)
		{
			LO_CellStruct *cell_ele;
			
			if (relayout == FALSE)
			{
				/*
				cell_ele = lo_SquishSubDocToCell(context, state,
					subdoc, TRUE);
				*/
				cell_ele = lo_SquishSubDocToCell(context, state,
					subdoc, FALSE);
				cell_ele->isCaption = TRUE;
				table->caption->cell_ele = cell_ele;
			}
			else
			{
				cell_ele = table->caption->cell_ele;
				lo_UpdateCaptionCellFromSubDoc(context, state, subdoc, cell_ele);
			}
			/* table->caption->subdoc = NULL; */

			if (cell_ele == NULL)
			{
				lo_AppendToLineList(context, state,
					(LO_Element *)subdoc, 0);
			}
			else
			{
                /*cmanske - Save spacing param for drawing cell selection by FEs */
                /* should we use cell_pad (converted to FE units) instead? */
                cell_ele->inter_cell_space = table->inter_cell_pad;
				lo_AppendToLineList(context, state,
					(LO_Element *)cell_ele, 0);
			}
			cell_x = state->x;
			cell_y = cell_y + table->caption->height + cell_pad;

			table->table_ele->y_offset = cell_y -
				table->table_ele->y;
			cell_y += (cell_pad + table->table_ele->border_top_width);
			if (table->draw_borders == TABLE_BORDERS_OFF)
			{
				cell_y += table_pad;
			}
		}
		else
		{
			LO_CellStruct *cell_ele;

			/*
			 * Free up the useless subdoc
			 */
			cell_ele = lo_SquishSubDocToCell(context, state,
						subdoc, TRUE);
			table->caption->subdoc = NULL;

			if (cell_ele != NULL)
			{
				lo_FreeElement(context,
					(LO_Element *)cell_ele, TRUE);
			}
		}
	}

	row_max = table->height_spans;
	for (y=0; y < table->rows; y++)
	{
		cell_x = state->x;
		col_max = table->width_spans;
		for (x=0; x < table->cols; x++)
		{
			LO_CellStruct *cell_struct;
			int32 new_x, new_y;

			indx = (y * table->cols) + x;
			cell_ptr = cell_array[indx].cell;
			if ((cell_ptr == &blank_cell)||(cell_ptr == NULL))
			{
				cell_x = cell_x + col_max->dim + cell_pad;
				col_max = col_max->next;
				continue;
			}
			cell_struct = cell_ptr->cell;
/*
			cell_struct->x = cell_x;
			cell_struct->y = cell_y;
			cell_struct->x_offset = (int16)cell_struct->border_horiz_space;
			cell_struct->y_offset = (int32)cell_struct->border_vert_space;
*/

			new_x = cell_x + (int16)cell_struct->border_horiz_space + cell_struct->border_width;
			new_y = cell_y + (int32)cell_struct->border_vert_space + cell_struct->border_width;

			cell_struct->width = col_max->dim;
			if (cell_ptr->colspan > 1)
			{
				int32 i;
				lo_table_span *max_ptr;

				max_ptr = col_max;

				for (i=1; i < cell_ptr->colspan; i++)
				{
					max_ptr = max_ptr->next;
					cell_struct->width = cell_struct->width +
						cell_pad + max_ptr->dim;
				}
			}
			cell_struct->width = cell_struct->width -
				(2 * cell_struct->border_horiz_space);
			cell_struct->height = row_max->dim;
			if (cell_ptr->rowspan > 1)
			{
				int32 i;
				lo_table_span *max_ptr;

				max_ptr = row_max;

				for (i=1; i < cell_ptr->rowspan; i++)
				{
					max_ptr = max_ptr->next;
					cell_struct->height = cell_struct->height +
						cell_pad + max_ptr->dim;
				}
			}
			cell_struct->height = cell_struct->height -
				(2 * cell_struct->border_vert_space);

			lo_ShiftCell(cell_struct,
				cell_ptr->cell_base_x, cell_ptr->cell_base_y);
			cell_ptr->cell_base_x = 0;
			cell_ptr->cell_base_y = 0;
			ele_cnt = lo_align_cell(context, state, cell_ptr,
					cell_struct, table, row_max);
			/*
			if (ele_cnt > 0)
			*/
			{
				LO_CellStruct *cell_ele;
				int32 shift_x, shift_y;



				shift_x = new_x - cell_struct->x -
					cell_struct->x_offset -
					cell_struct->border_width;
				shift_y = new_y - cell_struct->y -
					cell_struct->y_offset -
					cell_struct->border_width;
				cell_struct->x = cell_x;
				cell_struct->y = cell_y;
				cell_struct->x_offset = (int16)cell_struct->border_horiz_space;
				cell_struct->y_offset = (int32)cell_struct->border_vert_space;
				cell_ele = cell_struct;

/*
				lo_ShiftCell(cell_ele, shift_x, shift_y);
*/
				lo_ShiftCell(cell_ele, new_x, new_y);
				/*
				 * Keep element ids sequential.
				 */
				if (cell_ele != NULL)
				{
					cell_ele->ele_id = NEXT_ELEMENT;
					lo_RenumberCell(state, cell_ele);
				}

				if (cell_ele == NULL)
				{
					lo_AppendToLineList(context, state,
						(LO_Element *)cell_struct, 0);
				}
				else
				{
                    /*cmanske - Save spacing param for drawing cell selection by FEs */
                    cell_ele->inter_cell_space = table->inter_cell_pad;
					lo_AppendToLineList(context, state,
						(LO_Element *)cell_ele, 0);
				}

				if ( relayout == FALSE )
				{
					/*
					 * Cell backgrounds now sit in their own layer. This is needed
					 * for selection to work correctly.
					 * Cell backgrounds can exist in the main _BODY layer,
					 * so we need to special case the parent layer of the
					 * cell background,
					 */
					if (context->compositor &&
						(cell_ele->backdrop.bg_color || cell_ele->backdrop.url))
					{
						lo_TopState *top_state = state->top_state;
						CL_Layer *parent_layer = lo_CurrentLayer(state);
						if (parent_layer == top_state->doc_layer)
							parent_layer = top_state->body_layer;
						cell_ele->cell_bg_layer =
							lo_CreateCellBackgroundLayer(context, cell_ele,
														 parent_layer, top_state->table_nesting_level);
					}
					else
					{
						cell_ele->cell_bg_layer = NULL;
					}
				}
			}
			/*
			else
			{
			*/
				/*
				LO_CellStruct *cell_ele;
				*/
				/*
				 * Free up the useless cell
				 */			
			/*
				cell_ele = cell_struct;
				if (cell_ele != NULL)
				{
					lo_FreeElement(context,
						(LO_Element *)cell_ele, TRUE);
				}
				
			}
			*/
			cell_x = cell_x + col_max->dim + cell_pad;
			col_max = col_max->next;
		}
		cell_y = cell_y + row_max->dim + cell_pad;
		row_max = row_max->next;
	}
	cell_x = cell_x + table->table_ele->border_left_width;
	cell_y = cell_y + table->table_ele->border_top_width;
	if (table->draw_borders == TABLE_BORDERS_OFF)
	{
		cell_x += table_pad;
		cell_y += table_pad;
	}

	if ((table->caption != NULL)&&
		(table->caption->vert_alignment != LO_ALIGN_TOP))
	{
		LO_SubDocStruct *subdoc;

		subdoc = table->caption->subdoc;
		subdoc->x = state->x;
		subdoc->y = cell_y;
		subdoc->x_offset = (int16)subdoc->border_horiz_space;
		subdoc->y_offset = (int32)subdoc->border_vert_space;
		subdoc->width = table->caption->max_width -
			(2 * subdoc->border_horiz_space);
		subdoc->height = table->caption->height -
			(2 * subdoc->border_vert_space);
		ele_cnt = lo_align_subdoc(context, state,
			(lo_DocState *)subdoc->state, subdoc, table, NULL);
		if (ele_cnt > 0)
		{
			LO_CellStruct *cell_ele;

			if (relayout == FALSE)
			{
				/*
				cell_ele = lo_SquishSubDocToCell(context, state,
						subdoc, TRUE);
				*/
				cell_ele = lo_SquishSubDocToCell(context, state,
						subdoc, FALSE);
				cell_ele->isCaption = TRUE;
				table->caption->cell_ele = cell_ele;
			}
			else 
			{
				cell_ele = table->caption->cell_ele;
				lo_UpdateCaptionCellFromSubDoc(context, state, subdoc, cell_ele);
			}
			
			/* table->caption->subdoc = NULL; */

			if (cell_ele == NULL)
			{
				lo_AppendToLineList(context, state,
					(LO_Element *)subdoc, 0);
			}
			else
			{
                /*cmanske - Save spacing param for drawing cell selection by FEs */
                cell_ele->inter_cell_space = table->inter_cell_pad;
				lo_AppendToLineList(context, state,
					(LO_Element *)cell_ele, 0);
			}
			cell_y = cell_y + table->caption->height + cell_pad;
		}
		else
		{
			LO_CellStruct *cell_ele;

			/*
			 * Free up the useless subdoc
			 */
			cell_ele = lo_SquishSubDocToCell(context, state,
						subdoc, TRUE);
			table->caption->subdoc = NULL;
			
			if (cell_ele != NULL)
			{
				lo_FreeElement(context,
					(LO_Element *)cell_ele, TRUE);
			}
		}
	}

	/*
	 * This table may have contained named anchors.
	 * This will correct their positions in the name_list,
	 * and unblock us if we were blocked on them.
	 */
	lo_CheckNameList(context, state, table->table_ele->ele_id);

	/*
	 * The usual stuff for a table.
	 */
	if (floating == FALSE)
	{
		lo_AlignStack *aptr;
		int32 indent;

		state->x = cell_x;
		state->baseline = 0;
		state->line_height = cell_y - state->y;
		state->linefeed_state = 0;
		state->at_begin_line = FALSE;
		state->trailing_space = FALSE;
		state->cur_ele_type = LO_SUBDOC;

		table->table_ele->line_height = state->line_height;

		/*
		 * Find how far the table might be indented from
		 * being inside a list.
		 */
		indent = state->list_stack->old_left_margin - state->win_left;
		if (indent < 0)
		{
			indent = 0;
		}

		save_doc_min_width = state->min_width;
		if (relayout == FALSE) 
		{
			lo_SoftLineBreak(context, state, FALSE);
		}
		else
		{
			lo_rl_AddBreakAndFlushLine( context, state, LO_LINEFEED_BREAK_SOFT, LO_CLEAR_NONE, FALSE );
		}

		if ((min_table_width + indent) > save_doc_min_width)
		{
			save_doc_min_width = min_table_width + indent;
		}
		state->min_width = save_doc_min_width;
		
		/*
		 * Pop the table's alignment.
		 */
		aptr = lo_PopAlignment(state);
		if (aptr != NULL)
		{
			XP_DELETE(aptr);
		}
	}
	/*
	 * Else this is a floating table, rip it out of the "faked"
	 * line list, stuff itin the margin, and restore our state
	 * to where we left off.
	 */
	else
	{
		int32 push_right;
		int32 line_height;
                LO_Element *tptr;
                LO_Element *last;

		line_height = cell_y - state->y;
		push_right = 0;
		if (table->table_ele->alignment == LO_ALIGN_RIGHT)
		{
			push_right = state->right_margin - cell_x;
		}

		table->table_ele->x_offset +=
			(int16)table->table_ele->border_horiz_space;
		table->table_ele->y_offset +=
			(int32)table->table_ele->border_vert_space;

		last = NULL;
		tptr = state->line_list;
		while (tptr != NULL)
		{
			tptr->lo_any.x += push_right;
			if (tptr->type == LO_CELL)
			{
				tptr->lo_any.x +=
					table->table_ele->border_horiz_space;
				tptr->lo_any.y +=
					table->table_ele->border_vert_space;
				lo_ShiftCell((LO_CellStruct *)tptr,
					(push_right +
					table->table_ele->border_horiz_space),
					table->table_ele->border_vert_space);
			}
			last = tptr;
			tptr->lo_any.line_height = line_height;
			tptr = tptr->lo_any.next;
		}

		/*
		 * Stuff the whole line list into the float list, and
		 * restore the line list to its pre-table state.
		 */
		if (state->line_list != NULL)
		{
			last->lo_any.next = state->float_list;
			state->float_list = state->line_list;
			state->line_list = NULL;
		}

		if (relayout == FALSE)
		{
			lo_AppendFloatInLineList(state, (LO_Element *)table->table_ele, save_line_list );
		}
		else
		{
			state->line_list = save_line_list;
		}
		

		table->table_ele->line_height = line_height;
		table->table_ele->expected_y = table->table_ele->y;
		table->table_ele->y = -1;

		lo_AddMarginStack(state,
			table->table_ele->x, table->table_ele->y,
                        table->table_ele->width, table->table_ele->line_height,
                        table->table_ele->border_left_width + 
                        	table->table_ele->border_right_width,
                        table->table_ele->border_vert_space,
			table->table_ele->border_horiz_space,
                        (intn)table->table_ele->alignment);

		/*
		 * Restore state to pre-table values.
		 */
		state->x = save_state_x;
		state->y = save_state_y;

		/*
		 * All the doc_min_width stuff makes state->min_width
		 * be correct for tables nested inside tables.
		 */
		save_doc_min_width = state->min_width;

		/*
		 * Standard float stuff, if we happen to be at the start
		 * of a line, place the table now.
		 */
		if (state->at_begin_line != FALSE)
		{
			lo_FindLineMargins(context, state, (! relayout));
			state->x = state->left_margin;
		}

		if (min_table_width > save_doc_min_width)
		{
			save_doc_min_width = min_table_width;
		}
		state->min_width = save_doc_min_width;

	}

	/*  Now cell records get freed when the LO_TABLE element in the line list gets recycled
	for (y=0; y < table->rows; y++)
	{
		for (x=0; x < table->cols; x++)
		{
			indx = (y * table->cols) + x;
			cell_ptr = cell_array[indx].cell;
			if ((cell_ptr != &blank_cell)&&(cell_ptr != NULL))
			{
				lo_free_cell_record(context, state, cell_ptr);
			}
		}
	}
	*/

	/* Decrement table nesting level (used for passing into lo_CreateCellBackGroundLayer() */
	if (!relayout)
	{
 		state->top_state->table_nesting_level--;
	}

#ifdef XP_WIN16
	_hfree(cell_array);
#else
	XP_UNLOCK_BLOCK(cell_array_buff);
	XP_FREE_BLOCK(cell_array_buff);
#endif

	/* 
	 * Don't wanna free the table record any more because this information
	 * is used during relayout.
	 */
	/*

	lo_free_table_record(context, state, table, FALSE);

	*/
}


#if 0
/*
 * This function relaysout the tags in a table cell. It knows how to handle
 * certain types of elements that can be resused directly rather than thrown
 * away and relayedout from scratch (the default case).
 */
void
lo_RelayoutTags(MWContext *context, lo_DocState *state, PA_Tag * tag_ptr,
	PA_Tag * tag_end_ptr, LO_Element * elem_list)
{
	PA_Tag * next_tag;
	Bool save_diff_state;
	int32 save_state_pushes;
	int32 save_state_pops;
	lo_TopState *top_state;
	PA_Tag *tag_list;
	
	next_tag = NULL;
	top_state = state->top_state;

	/*
	 * Save our parent's state levels
	 */
	save_diff_state = top_state->diff_state;
	save_state_pushes = top_state->state_pushes;
	save_state_pops = top_state->state_pops;

	while (tag_ptr != tag_end_ptr)
	{
		PA_Tag *tag;
		lo_DocState *sub_state;
		lo_DocState *up_state;
		lo_DocState *tmp_state;
		Bool may_save;

		tag = tag_ptr;
		tag_ptr = tag_ptr->next;
		tag->next = NULL;
		tag_list = tag_ptr;

		up_state = NULL;
		sub_state = state;
		while (sub_state->sub_state != NULL)
		{
			up_state = sub_state;
			sub_state = sub_state->sub_state;
		}

		if ((sub_state->is_a_subdoc == SUBDOC_CELL)||
			(sub_state->is_a_subdoc == SUBDOC_CAPTION))
		{
			may_save = TRUE;
		}
		else
		{
			may_save = FALSE;
		}

		/*
		 * Reset these so we can tell if anything happened
		 */
		top_state->diff_state = FALSE;
		top_state->state_pushes = 0;
		top_state->state_pops = 0;

#ifdef MOCHA
		sub_state->in_relayout = TRUE;
#endif
		/*
		 * If we're not currently searching for a tag, then find the next
		 * one to look for.
		 */
		if ( next_tag == NULL && elem_list != NULL )
		{
			next_tag = lo_FindReuseableElement ( context, state, &elem_list );
		}
				
		/*
		 * Can we reuse this current element?
		 */
		if ( (tag == next_tag) && (elem_list != NULL) )
		{			
			LO_Element *eptr;
			LO_Element *enext;
			
			/*
			 * Yup, remove it from the list and call the object layout code to
			 * do the relayout.
			 */
			eptr = elem_list;
			enext = eptr->lo_any.next;
			
			elem_list = enext;
			
			if ( enext != NULL )
			{
				enext->lo_any.prev = NULL;
			}
			
			eptr->lo_any.next = 0L;
			eptr->lo_any.prev = 0L;
			
			switch ( eptr->type )
			{
				case LO_EMBED:
					lo_RelayoutEmbed ( context, sub_state, &eptr->lo_embed, tag );
					break;
#ifdef JAVA
				case LO_JAVA:
					lo_RelayoutJavaApp ( context, sub_state, tag, &eptr->lo_java );
					break;
#endif /* JAVA */
				
				default:
					/*
					 * We goofed and got something we don't know how to relayout
					 */
					lo_relayout_recycle ( context, state, eptr );
					lo_LayoutTag(context, sub_state, tag);
					break;
			}
			
			/*
			 * Force us to look for the next reusable element.
			 */
			next_tag = NULL;
		}
		else
		{
			/*
			 * Nope, call the standard layout code.
			 */
			lo_LayoutTag(context, sub_state, tag);
		}

		tmp_state = lo_CurrentSubState(state);
#ifdef MOCHA
		if (tmp_state == sub_state)
		{
			sub_state->in_relayout = FALSE;
		}
#endif

		tmp_state = lo_CurrentSubState(state);
#ifdef MOCHA
		if (tmp_state == sub_state)
		{
			sub_state->in_relayout = FALSE;
		}
#endif

		if (may_save != FALSE)
		{
			int32 state_diff;
			
			/* how has our state level changed? */
			state_diff = top_state->state_pushes - top_state->state_pops;
			
			/*
			 * That tag popped us up one state level.  If this new
			 * state is still a subdoc, save the tag there.
			 */
			if (state_diff == -1)
			{
				if ((tmp_state->is_a_subdoc == SUBDOC_CELL)||
				    (tmp_state->is_a_subdoc == SUBDOC_CAPTION))
				{
                    /* if we just popped a table we need to insert
                     * a dummy end tag to pop the dummy start tag
                     * we shove on the stack after createing a table
                     */
   					PA_Tag *new_tag = LO_CreateStyleSheetDummyTag(tag);
					if(new_tag)
					{
						lo_SaveSubdocTags(context, tmp_state, new_tag);
					}

				    lo_SaveSubdocTags(context, tmp_state, tag);
				}
				else
				{
				    PA_FreeTag(tag);
				}
			}
			/*
			 * Else that tag put us in a new subdoc on the same
			 * level.  It needs to be saved one level up,
			 * if the parent is also a subdoc.
			 */
			else if (( up_state != NULL ) &&
				( top_state->diff_state != FALSE ) &&
				( state_diff == 0 ))
			{
				if ((up_state->is_a_subdoc == SUBDOC_CELL)||
				     (up_state->is_a_subdoc == SUBDOC_CAPTION))
				{
				    lo_SaveSubdocTags(context, up_state, tag);
				}
				else
				{
				    PA_FreeTag(tag);
				}
			}
			/*
			 * Else we are still in the same subdoc
			 */
			else if (( top_state->diff_state == FALSE ) &&
				( state_diff == 0 ))
			{
				lo_SaveSubdocTags(context, sub_state, tag);
			}
			/*
			 * Else that tag started a new, nested subdoc.
			 * Add the starting tag to the parent.
			 */
			else if (( state_diff == 1 ))
			{
				lo_SaveSubdocTags(context, sub_state, tag);
				/*
				 * Since we have extended the parent chain,
				 * we need to reset the child to the new
				 * parent end-chain.
				 */
				if ((tmp_state->is_a_subdoc == SUBDOC_CELL)||
				    (tmp_state->is_a_subdoc == SUBDOC_CAPTION))
				{
					PA_Tag *new_tag;
					
					tmp_state->subdoc_tags =
						sub_state->subdoc_tags_end;

					/* add an aditional dummy tag so that style sheets
					 * can use it to query styles from for this entry
					 * that created a table
					 */
					new_tag = LO_CreateStyleSheetDummyTag(tag);
					if(new_tag)
					{
						lo_SaveSubdocTags(context, tmp_state, new_tag);
					}
				}
			}
			/*
			 * This can never happen.
			 */
			else
			{
				PA_FreeTag(tag);
			}
		}
		tag_ptr = tag_list;
	}

	/*
	 * Restore our parent's state levels
	 */
	top_state->diff_state = save_diff_state;
	top_state->state_pushes = save_state_pushes;
	top_state->state_pops = save_state_pops;
}
#endif

/*
 * Walk the element list, throwing them away until we find one
 * that can be reused.
 */
PA_Tag *
lo_FindReuseableElement(MWContext *context, lo_DocState *state, LO_Element ** elem_list)
{
	LO_Element * eptr;
	LO_Element * enext;
	PA_Tag * tag;
	
	tag = NULL;
	eptr = *elem_list;
	enext = NULL;
	
	/*
	 * Keep going until we've gone through all the elements
	 * or found one that we can reuse.
	 */
	while ( (eptr != NULL) && (tag == NULL) )
	{		
		enext = eptr->lo_any.next;
		
		/*
		 * Any element found by this switch statement will attempt to be relayed out
		 * using the same element structure. DO NOT put anything in here that can
		 * have atributes set by style sheets until the lo_PreLayoutTag function
		 * can correctly handle stylesheets.
		 */
		switch ( eptr->type )
		{
			case LO_EMBED:
				tag = eptr->lo_embed.objTag.tag;
				break;
#ifdef JAVA
			case LO_JAVA:
				tag = eptr->lo_java.objTag.tag;
				break;
#endif /* JAVA */
		}
		
		/*
		 * If we didn't find a tag from that element, then dispose of it and
		 * keep looking.
		 */
		if ( tag == NULL )
		{
			/*
			 * We dunno how to reuse this type of element, so throw
			 * it away and someone else will relay it out from scratch.
			 */				
			eptr->lo_any.prev = NULL;
			eptr->lo_any.next = NULL;
			lo_relayout_recycle ( context, state, eptr );
			if ( enext != NULL )
			{
				enext->lo_any.prev = NULL;
			}
			eptr = enext;
		}
	}
	/*
	 * Advance the element list to the new head
	 */
	*elem_list = eptr;
	
	return tag;
}

/* 
 * Functions separated out of lo_BeginTableAttributes
 */

void lo_PositionTableElement(lo_DocState *state, LO_TableStruct *table_ele) 
{
	table_ele->ele_id = NEXT_ELEMENT;
	table_ele->x = state->x;
	table_ele->x_offset = 0;
	table_ele->y = state->y;
	table_ele->y_offset = 0;
	table_ele->width = 0;
	table_ele->height = 0;
	table_ele->line_height = 0;

	table_ele->next = NULL;
	table_ele->prev = NULL;

	/*
	 * Push our alignment state if we're not floating
	 */
	
	if ( !(table_ele->ele_attrmask & LO_ELE_FLOATING) )
	{
		lo_PushAlignment(state, P_TABLE_DATA, table_ele->alignment);
	}
	
}


void lo_InitTableRecord( lo_TableRec *table )
{
	/* Reset row and col counters */
	table->rows = 0;
	table->cols = 0;

	/* Reset width span array */
	lo_ResetWidthSpans(table);

	table->height_span_ptr = NULL;
}

static void lo_ResetWidthSpans( lo_TableRec *table )
{
	lo_table_span *span = table->width_spans;

	while (span != NULL)
	{
		span->dim = 1;
		span->min_dim = 1;
		span->span = 0;		
		span = span->next;
	}

}

void lo_SetTableDimensions( lo_DocState *state, lo_TableRec *table, int32 allow_percent_width, int32 allow_percent_height)
{
	int32 val;

	if (table->percent_width > 0) 
	{
		val = table->percent_width;
		if (allow_percent_width == FALSE)
		{
			val = 0;
		}
		else
		{
			val = (state->win_width - state->win_left -
				state->win_right) * val / 100;
		}
		if (val < 0)
		{
			val = 0;
		}
		table->width = val;
	}
	
	if (table->percent_height > 0)
	{
		val = table->percent_height;
		if (allow_percent_height == FALSE)
		{
			val = 0;
		}
		else
		{
			val = (state->win_height - state->win_top -
				state->win_bottom) * val / 100;
		}
		if (val < 0)
		{
			val = 0;
		}
		table->height = val;
	}
}

void lo_CalcFixedColWidths( MWContext *context, lo_DocState *state, lo_TableRec *table)
{
	int32 cols = table->fixed_cols;
	if (cols > 0) {
		int32 count;
		int32 table_width;
		
		table_width = lo_ComputeInternalTableWidth ( context, table, state );
		
		/* Split the space up evenly */
		table->default_cell_width = table_width / cols;
		
		/* and we have all the table width left to play with */
		table->fixed_width_remaining = table_width;
		
		/* Initialize our width array */
		for ( count = 0; count < cols; ++count )
		{
			table->fixed_col_widths[ count ] = 0;
		}
	}
}

void lo_UpdateStateAfterBeginTable( lo_DocState *state, lo_TableRec *table)
{
	state->current_table = table;
}

/* Relayout version of lo_BeginTableRowAttributes()  */
void lo_UpdateTableStateForBeginRow(lo_TableRec *table, lo_TableRow *table_row)
{
	table_row->row_done = FALSE;
	table_row->cells = 0;
	table_row->cell_list = NULL;
	table_row->cell_ptr = NULL;
	/* table_row->next = NULL;

	if (table->row_list == NULL)
	{
		table->row_list = table_row;
		table->row_ptr = table_row;
	}
	else
	{
		table->row_ptr->next = table_row;
		table->row_ptr = table_row;
	}
	*/

	table->width_span_ptr = NULL;
	
}

/* 
 * Functions for breaking up lo_BeginTableCellAttributes()
 */
void lo_InitForBeginCell(lo_TableRow *table_row, lo_TableCell *table_cell)
{
	table_cell->must_relayout = FALSE;
	table_cell->cell_done = FALSE;
	
	if (table_row->cell_list == NULL)
	{
		table_row->cell_list = table_cell;
		table_row->cell_ptr = table_cell;
	}
	else
	{
		table_row->cell_ptr->next = table_cell;
		table_row->cell_ptr = table_cell;
	}
}

void lo_InitSubDocForBeginCell( MWContext *context, lo_DocState *state, lo_TableRec *table )
{
	lo_TableCell *table_cell = table->row_ptr->cell_ptr;
	lo_BeginCellSubDoc(context, 
						state, 
						table,
						NULL,
                        NULL,
                        LO_TILE_BOTH,
						NULL,
						NULL,
						NULL,
						NULL,
 						table_cell->is_a_header, 
						table->draw_borders,
						TRUE);

	/*
	 * We added a new state.
	 */
	lo_PushStateLevel ( context );
}

static void lo_UpdateCaptionCellFromSubDoc( MWContext *context, lo_DocState *state, LO_SubDocStruct *subdoc, LO_CellStruct *cell_ele)
{
	int32 dx = 0;
	int32 dy = 0;

	lo_CreateCellFromSubDoc(context, state, subdoc, cell_ele, &dx, &dy);
	lo_ShiftCell(cell_ele, dx, dy);
	lo_RenumberCell(state, cell_ele);
}

/* Only deletes the cell itself.  Assumes that stuff the cell points to will get deleted
   elsewhere */
static void lo_FreeCaptionCell( MWContext *context, lo_DocState *state, LO_CellStruct *cell_ele)
{
	cell_ele->next = NULL;
	cell_ele->prev = NULL;
	cell_ele->cell_list = NULL;
	cell_ele->cell_list_end = NULL;
	cell_ele->cell_float_list = NULL;
	cell_ele->table_cell = NULL;
	cell_ele->table_row = NULL;
	cell_ele->table = NULL;

	lo_FreeElement(context, (LO_Element *)cell_ele, TRUE);

}
#ifdef TEST_16BIT
#undef XP_WIN16
#endif /* TEST_16BIT */
