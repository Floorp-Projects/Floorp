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
#include "shist.h"
#include "pa_parse.h"
#include "layout.h"
#include "libmocha.h"


#define TOP_EDGE	0
#define BOTTOM_EDGE	1
#define LEFT_EDGE	2
#define RIGHT_EDGE	3


static lo_GridPercent *
lo_parse_percent_list(MWContext *context, char *str, int32 *rows)
{
	char *tptr;
	int32 i, cnt;
	lo_GridPercent *percent_list;

	if ((str == NULL)||(*str == '\0'))
	{
		return((lo_GridPercent *)NULL);
	}

	cnt = 0;
	tptr = strchr(str, ',');
	while (tptr != NULL)
	{
		cnt++;
		tptr++;
		tptr = strchr(tptr, ',');
	}
	cnt++;
	*rows = cnt;

	percent_list = (lo_GridPercent *)XP_ALLOC(cnt * sizeof(lo_GridPercent));
	if (percent_list == NULL)
	{
		return((lo_GridPercent *)NULL);
	}

	tptr = str;
	for (i=0; i<cnt; i++)
	{
		char *ptr;
		char *end_ptr;

		ptr = strchr(tptr, ',');
		if (ptr != NULL)
		{
			*ptr = '\0';
		}

		/*
		 * Need the end of the string to check if the number
		 * ends in '%', '*', or 'p'.
		 */
		if (ptr != NULL)
		{
			end_ptr = (char *)(ptr - 1);
			if (end_ptr < tptr)
			{
				end_ptr = tptr;
			}
		}
		else
		{
			end_ptr = (char *)(str + XP_STRLEN(str) - 1);
		}

		if (*end_ptr == '*')
		{
			percent_list[i].type = LO_PERCENT_FREE;
			percent_list[i].original_value = (int32)XP_ATOI(tptr);
			if (percent_list[i].original_value < 1)
			{
				percent_list[i].original_value = 1;
			}
		}
		else if (*end_ptr == '%')
		{
			percent_list[i].type = LO_PERCENT_NORMAL;
			percent_list[i].original_value = (int32)XP_ATOI(tptr);
			if (percent_list[i].original_value <= 0)
			{
				percent_list[i].original_value = 100 / cnt;
				if (percent_list[i].original_value == 0)
				{
					percent_list[i].original_value = 1;
				}
			}
		}
		/*
		 * Default is in "pixels"
		 */
		else
		{
			int32 val;

			percent_list[i].type = LO_PERCENT_FIXED;
			val = (int32)XP_ATOI(tptr);
			val = FEUNITS_X(val, context);
			if (val < 1)
			{
				val = 1;
			}
			percent_list[i].original_value = val;
		}

		if (ptr != NULL)
		{
			*ptr = ',';
			tptr = (char *)(ptr + 1);
		}
	}
	return(percent_list);
}


void
lo_FreeGridEdge(lo_GridEdge *edge)
{
	if (edge->cell_array != NULL)
	{
		XP_FREE(edge->cell_array);
	}
	XP_DELETE(edge);
}


void
lo_FreeGridRec(lo_GridRec *grid)
{
	if (grid == NULL)
	{
		return;
	}

	if (grid->row_percents != NULL)
	{
		XP_FREE(grid->row_percents);
	}
	if (grid->col_percents != NULL)
	{
		XP_FREE(grid->col_percents);
	}

	if (grid->edge_list != NULL)
	{
		lo_GridEdge *tmp_edge;
		lo_GridEdge *edge_list;

		edge_list = grid->edge_list;
		while (edge_list != NULL)
		{
			tmp_edge = edge_list;
			edge_list = edge_list->next;
			lo_FreeGridEdge(tmp_edge);
		}
	}

	XP_DELETE(grid);
}


void
lo_FreeGridCellRec(MWContext *context, lo_GridRec *grid, lo_GridCellRec *cell)
{
	History_entry *hist;

	if (cell == NULL)
	{
		return;
	}

	if (cell->url != NULL)
	{
		XP_FREE(cell->url);
	}
	if (cell->name != NULL)
	{
		XP_FREE(cell->name);
	}
	if (cell->context != NULL)
	{
		/*
		 * If the cell has a context, use it instead of the
		 * parent's context.  Otherwise use the parent context.
		 * This is bogus but needed to keep the winfe from
		 * dumping because it blindly dereferences the context
		 * even though it doesn't use it.
		 */
		context = cell->context;
	}
	if (cell->hist_list != NULL)
	{
		XP_List *list_ptr;

		list_ptr = (XP_List *)cell->hist_list;
		hist = (History_entry *)XP_ListRemoveTopObject(list_ptr);
		while (hist != NULL)
		{
			SHIST_DropEntry(context, hist);
			hist =(History_entry *)XP_ListRemoveTopObject(list_ptr);
		}
		XP_ListDestroy(list_ptr);
	}
	if (cell->hist_array != NULL)
	{
		int32 i;

		for (i = 0; i < grid->hist_size; i++)
		{
			hist = (History_entry *)cell->hist_array[i].hist;
			SHIST_DropEntry(context, hist);
		}
		XP_FREE(cell->hist_array);
	}
	XP_DELETE(cell);
}


static lo_GridEdge *
lo_new_horizontal_edge(MWContext *context, lo_DocState *state,
			int32 cols, int32 x, int32 y, int32 width, int32 height)
{
	lo_GridEdge *edge;
	LO_EdgeStruct *fe_edge;

	edge = XP_NEW(lo_GridEdge);
	if (edge == NULL)
	{
		return(NULL);
	}

	fe_edge = (LO_EdgeStruct *)lo_NewElement(context, state, LO_EDGE, NULL, 0);
	if (fe_edge == NULL)
	{
		XP_DELETE(edge);
		return(NULL);
	}

	edge->x = x;
	edge->y = y;
	edge->width = width;
	edge->height = height;
	edge->is_vertical = FALSE;
	edge->left_top_bound = 0;
	edge->right_bottom_bound = 0;
	edge->cell_cnt = 0;
	edge->cell_max = 0;
	edge->cell_array = NULL;
	edge->next = NULL;

	if (cols > 0)
	{
		int32 cells;

		cells = 2 * cols;
		edge->cell_array = (lo_GridCellRec **)XP_ALLOC(cells *
			sizeof(lo_GridCellRec *));
		if (edge->cell_array != NULL)
		{
			edge->cell_max = cells;
		}
	}

	fe_edge->type = LO_EDGE;
	fe_edge->ele_id = NEXT_ELEMENT;
	fe_edge->x = x;
	fe_edge->x_offset = 0;
	fe_edge->y = y;
	fe_edge->y_offset = 0;
	fe_edge->width = edge->width;
	fe_edge->height = edge->height;
	fe_edge->line_height = 0;
	fe_edge->next = NULL;
	fe_edge->prev = NULL;

	fe_edge->FE_Data = NULL;
	fe_edge->lo_edge_info = (void *)edge;
	fe_edge->is_vertical = edge->is_vertical;
	fe_edge->visible = TRUE;
	fe_edge->movable = TRUE;
	fe_edge->left_top_bound = 0;
	fe_edge->right_bottom_bound = 0;

	edge->fe_edge = fe_edge;

	return(edge);
}


static lo_GridEdge *
lo_new_vertical_edge(MWContext *context, lo_DocState *state,
			int32 rows, int32 x, int32 y, int32 width, int32 height)
{
	lo_GridEdge *edge;
	LO_EdgeStruct *fe_edge;

	edge = XP_NEW(lo_GridEdge);
	if (edge == NULL)
	{
		return(NULL);
	}

	fe_edge = (LO_EdgeStruct *)lo_NewElement(context, state, LO_EDGE, NULL, 0);
	if (fe_edge == NULL)
	{
		XP_DELETE(edge);
		return(NULL);
	}

	edge->x = x;
	edge->y = y;
	edge->width = width;
	edge->height = height;
	edge->is_vertical = TRUE;
	edge->left_top_bound = 0;
	edge->right_bottom_bound = 0;
	edge->cell_cnt = 0;
	edge->cell_max = 0;
	edge->cell_array = NULL;
	edge->next = NULL;

	if (rows > 0)
	{
		int32 cells;

		cells = 2 * rows;
		edge->cell_array = (lo_GridCellRec **)XP_ALLOC(cells *
			sizeof(lo_GridCellRec *));
		if (edge->cell_array != NULL)
		{
			edge->cell_max = cells;
		}
	}

	fe_edge->type = LO_EDGE;
	fe_edge->ele_id = NEXT_ELEMENT;
	fe_edge->x = x;
	fe_edge->x_offset = 0;
	fe_edge->y = y;
	fe_edge->y_offset = 0;
	fe_edge->width = edge->width;
	fe_edge->height = edge->height;
	fe_edge->line_height = 0;
	fe_edge->next = NULL;
	fe_edge->prev = NULL;

	fe_edge->FE_Data = NULL;
	fe_edge->lo_edge_info = (void *)edge;
	fe_edge->is_vertical = edge->is_vertical;
	fe_edge->visible = TRUE;
	fe_edge->movable = TRUE;
	fe_edge->left_top_bound = 0;
	fe_edge->right_bottom_bound = 0;

	edge->fe_edge = fe_edge;

	return(edge);
}


static void
lo_grow_edge_cell_array(lo_GridEdge *edge, int32 more)
{
	lo_GridCellRec **old_array;
	lo_GridCellRec **new_array;
	int32 new_size;

	/*
	 * Error condition, punt.
	 */
	if (more <= 0)
	{
		return;
	}

	/*
	 * If we are not growing the array, but instead allocating a new
	 * array.
	 */
	if (edge->cell_max == 0)
	{
		edge->cell_array = (lo_GridCellRec **)XP_ALLOC(more *
			sizeof(lo_GridCellRec *));
		if (edge->cell_array != NULL)
		{
			edge->cell_max = more;
		}
		return;
	}

	/*
	 * Attempt to grow the old array.
	 */
	new_size = edge->cell_max + more;
	old_array = edge->cell_array;
	new_array = (lo_GridCellRec **)XP_REALLOC(old_array, (new_size *
			sizeof(lo_GridCellRec *)));
	/*
	 * If realloc fails, punt.
	 */
	if (new_array == NULL)
	{
		return;
	}

	edge->cell_array = new_array;
	edge->cell_max = new_size;
}


static void
lo_add_cell_to_edge(lo_GridEdge *edge, lo_GridCellRec *cell)
{
	/*
	 * Error.
	 */
	if ((cell == NULL)||(edge == NULL))
	{
		return;
	}

	/*
	 * If I did my math right earlier, this will never happen.
	 */
	if (edge->cell_cnt >= edge->cell_max)
	{
		lo_grow_edge_cell_array(edge, 1);
		/*
		 * This may fail
		 */
		if (edge->cell_cnt >= edge->cell_max)
		{
			return;
		}
	}

	edge->cell_array[edge->cell_cnt] = cell;
	edge->cell_cnt++;
}


static void
lo_adjust_percents(int32 count, lo_GridPercent *percents, int32 max)
{
	int32 i;
	int32 fixed_percent;
	int32 free_percent;
	int32 total;

	if (max == 0)
		max = 1;

	/*
	 * Total up the three different type of grid cell percentages.
	 */
	fixed_percent = 0;
	free_percent = 0;
	total = 0;
	for (i=0; i < count; i++)
	{
		if (percents[i].type == LO_PERCENT_FIXED)
		{
			/*
			 * Now that we know the max dimension turn
			 * fixed pixel dimensions into percents.
			 */
			percents[i].value = 100 * percents[i].original_value / max;
			if (percents[i].value < 1)
			{
				percents[i].value = 1;
			}
			fixed_percent += percents[i].value;
		}
		else if (percents[i].type == LO_PERCENT_FREE)
		{
			percents[i].value = percents[i].original_value;
			free_percent += percents[i].original_value;
		}
		else
		{
			percents[i].value = percents[i].original_value;
			total += percents[i].original_value;
		}
	}

	/*
	 * If the user didn't explicitly use up all the space.
	 */
	if ((total + fixed_percent) < 100)
	{
		int32 val;

		/*
		 * We have some free percentage cells that want to
		 * soak up the excess space.
		 */
		if (free_percent > 0)
		{
			int32 used;
			int32 last_i;

			last_i = -1;
			used = 0;
			val = 100 - (total + fixed_percent);
			for (i=0; i < count; i++)
			{
				if (percents[i].type == LO_PERCENT_FREE)
				{
					percents[i].value *= val;
					percents[i].value /= free_percent;
					if (percents[i].value < 1)
					{
						percents[i].value = 1;
					}
					used += percents[i].value;
					last_i = i;
				}
			}
			/*
			 * Slop the extra into the last qualifying cell.
			 */
			if (((used + total + fixed_percent) < 100)&&
				(last_i >= 0))
			{
				percents[last_i].value +=
					(100 - total - fixed_percent - used);
			}
		}
		/*
		 * Else we have no free cells, but have some normal
		 * percentage cells, distribute the extra space among them.
		 */
		else if (total > 0)
		{
			int32 used;
			int32 last_i;

			last_i = -1;
			used = 0;
			val = (100 - fixed_percent);
			for (i=0; i < count; i++)
			{
				if (percents[i].type == LO_PERCENT_NORMAL)
				{
					percents[i].value *= val;
					percents[i].value /= total;
					used += percents[i].value;
					last_i = i;
				}
			}
			/*
			 * Slop the extra into the last qualifying cell.
			 */
			if ((used < val)&&(last_i >= 0))
			{
				percents[last_i].value += (val - used);
			}
		}
		/*
		 * Else all we have are fixed percentage cells, we will
		 * have to grow them.
		 */
		else
		{
			int32 used;

			used = 0;
			for (i=0; i < count; i++)
			{
				percents[i].value *= 100;
				percents[i].value /= (total + fixed_percent);
				used += percents[i].value;
			}
			/*
			 * Slop the extra into the last cell.
			 */
			if ((used < 100)&&(count > 0))
			{
				percents[count - 1].value += (100 - used);
			}
		}
	}
	/*
	 * Else if the user allocated too much space, we need to shrink
	 * something.
	 */
	else if ((total + fixed_percent) > 100)
	{
		int32 val;

		/*
		 * If there is not too much fixed percentage
		 * added, we can just shrink the normal percentage
		 * cells to make things fit.
		 */
		if (fixed_percent <= 100)
		{
			int32 used;
			int32 last_i;

			last_i = -1;
			used = 0;
			val = (100 - fixed_percent);
			for (i=0; i < count; i++)
			{
				if (percents[i].type == LO_PERCENT_NORMAL)
				{
					percents[i].value *= val;
					percents[i].value /= total;
					used += percents[i].value;
				}
			}
			/*
			 * Since integer division always truncates, we either
			 * made it fit exactly, or overcompensated and made
			 * it too small.
			 */
			if ((used < val)&&(last_i >= 0))
			{
				percents[last_i].value += (val - used);
			}
		}
		/*
		 * Else there is too much fixed percentage as well, we will
		 * just shrink all the cells.
		 */
		else
		{
			int32 used;

			used = 0;
			for (i=0; i < count; i++)
			{
				if (percents[i].type == LO_PERCENT_FREE)
				{
					percents[i].value = 0;
				}
				else
				{
					percents[i].value *= 100;
					percents[i].value /=
						(total + fixed_percent);
				}
				used += percents[i].value;
			}
			/*
			 * Since integer division always truncates, we either
			 * made it fit exactly, or overcompensated and made
			 * it too small.
			 */
			if (used < 100)
			{
				percents[count - 1].value += (100 - used);
			}
		}
	}
}


PRIVATE
void
lo_LayoutGridCells(MWContext *context, lo_DocState *state,
	int32 x, int32 y, int32 width, int32 height,
	lo_GridRec *grid, lo_GridEdge **edges)
{
	int32 cols, rows;
	int32 col_cnt, row_cnt;
	int32 orig_x, orig_y;
	lo_GridCellRec *cell_parent;
	lo_GridCellRec *cell_list;
	lo_GridEdge **col_edges;
	lo_GridEdge *top_edge;
	lo_GridEdge *new_edges[4];	/* top, bottom, left, right */

	new_edges[TOP_EDGE] = NULL;
	new_edges[BOTTOM_EDGE] = NULL;
	new_edges[LEFT_EDGE] = NULL;
	new_edges[RIGHT_EDGE] = NULL;

	if (grid == NULL)
	{
		return;
	}

	cols = grid->cols;
	rows = grid->rows;
	cell_list = grid->cell_list;

	/*
	 * Allocate space for the temporary array of column
	 * edge pointer we will need here.
	 */
	col_edges = (lo_GridEdge **)XP_ALLOC(sizeof(lo_GridEdge *) * cols);
	if (col_edges == NULL)
	{
		return;
	}
	top_edge = NULL;

	/*
	 * If we came in with bounding edges, we are a sub-grid, and we need
	 * to increase the size of the cell arrays in those edges to reflect
	 * the extra cells now bordering them.
	 */
	if (edges[TOP_EDGE] != NULL)
	{
		int32 more;

		more = (cols * 2) - 1;
		lo_grow_edge_cell_array(edges[TOP_EDGE], more);
	}
	if (edges[BOTTOM_EDGE] != NULL)
	{
		int32 more;

		more = (cols * 2) - 1;
		lo_grow_edge_cell_array(edges[BOTTOM_EDGE], more);
	}
	if (edges[LEFT_EDGE] != NULL)
	{
		int32 more;

		more = (rows * 2) - 1;
		lo_grow_edge_cell_array(edges[LEFT_EDGE], more);
	}
	if (edges[RIGHT_EDGE] != NULL)
	{
		int32 more;

		more = (rows * 2) - 1;
		lo_grow_edge_cell_array(edges[RIGHT_EDGE], more);
	}

	orig_x = x;
	orig_y = y;
	col_cnt = 0;
	row_cnt = 0;
	cell_parent = NULL;
	while (cell_list != NULL)
	{
		lo_GridEdge *tmp_edge;

		tmp_edge = NULL; /* make gcc happy */

		cell_list->width_percent =
			(intn)grid->col_percents[col_cnt].value;
		cell_list->height_percent =
			(intn)grid->row_percents[row_cnt].value;

		cell_list->x = x;
		if (col_cnt != 0)
		{
			cell_list->x += grid->grid_cell_border;
		}
		cell_list->y = y;
		if (row_cnt != 0)
		{
			cell_list->y += grid->grid_cell_border;
		}

		cell_list->width = width * cell_list->width_percent / 100;
		if (cell_list->width < grid->grid_cell_min_dim)
		{
			cell_list->width = grid->grid_cell_min_dim;
		}
		if (col_cnt > 0)
		{
			cell_list->width -= grid->grid_cell_border;
		}
		if (col_cnt < (cols - 1))
		{
			cell_list->width -= grid->grid_cell_border;
		}

		/*
		 * Make sure the last column uses all the remaining space.
		 * We subtract orig_x to get cell_list->x into the same coord
		 * space as width.
		 */
		if (col_cnt == (cols - 1))
		{
			cell_list->width = width - (cell_list->x - orig_x);
		}

		cell_list->height = height * cell_list->height_percent / 100;
		if (cell_list->height < grid->grid_cell_min_dim)
		{
			cell_list->height = grid->grid_cell_min_dim;
		}
		if (row_cnt > 0)
		{
			cell_list->height -= grid->grid_cell_border;
		}
		if (row_cnt < (rows - 1))
		{
			cell_list->height -= grid->grid_cell_border;
		}

		/*
		 * Make sure the last row uses all the remaining space.
		 * We subtract orig_y to get cell_list->y into the same coord
		 * space as height.
		 */
		if (row_cnt == (rows - 1))
		{
			cell_list->height = height - (cell_list->y - orig_y);
		}

		/*
		 * If this is the first column, for any row but the
		 * last row, create a new edge to be the bottom of this
		 * row.
		 */
		if ((col_cnt == 0)&&(row_cnt < (rows - 1)))
		{
			tmp_edge = lo_new_horizontal_edge(context, state,
				cols, x, (cell_list->y + cell_list->height),
				width, (2 * grid->grid_cell_border));
			if (tmp_edge != NULL)
			{
				tmp_edge->next = grid->edge_list;
				grid->edge_list = tmp_edge;
			}
		}

		/*
		 * When in the first row, create a new side edge on
		 * each column but the last one.
		 */
		if ((row_cnt == 0)&&(col_cnt < (cols - 1)))
		{
			lo_GridEdge *v_edge;

			v_edge = lo_new_vertical_edge(context, state,
				rows, (cell_list->x + cell_list->width), y,
				(2 * grid->grid_cell_border), height);
			if (v_edge != NULL)
			{
				v_edge->next = grid->edge_list;
				grid->edge_list = v_edge;
			}
			col_edges[col_cnt] = v_edge;
		}

		/*
		 * If this is the first column in the row, we need to
		 * set top and bottom edges, for later columns in
		 * the same row, they remain unchanged.
		 */
		if (col_cnt == 0)
		{
			new_edges[TOP_EDGE] = top_edge;
			if (row_cnt == 0)
			{
				new_edges[TOP_EDGE] = edges[TOP_EDGE];
			}
			new_edges[BOTTOM_EDGE] = tmp_edge;
			if (row_cnt == (rows - 1))
			{
				new_edges[BOTTOM_EDGE] = edges[BOTTOM_EDGE];
			}
			/*
			 * Set top_edge for next time around.
			 */
			top_edge = tmp_edge;
		}

		/*
		 * Set the left and right edges for this column.
		 */
		if (col_cnt == 0)
		{
			new_edges[LEFT_EDGE] = edges[LEFT_EDGE];
		}
		else
		{
			new_edges[LEFT_EDGE] = col_edges[(col_cnt - 1)];
		}
		if (col_cnt == (cols - 1))
		{
			new_edges[RIGHT_EDGE] = edges[RIGHT_EDGE];
		}
		else
		{
			new_edges[RIGHT_EDGE] = col_edges[col_cnt];
		}

		x += cell_list->width;
		if (col_cnt > 0)
		{
			x += grid->grid_cell_border;
		}
		if (col_cnt < (cols - 1))
		{
			x += grid->grid_cell_border;
		}
		col_cnt++;
		if (col_cnt >= cols)
		{
			x = orig_x;
			y += cell_list->height;
			if (row_cnt > 0)
			{
				y += grid->grid_cell_border;
			}
			if (row_cnt < (rows - 1))
			{
				y += grid->grid_cell_border;
			}
			col_cnt = 0;
			row_cnt++;
		}

		if (cell_list->subgrid != NULL)
		{
			lo_GridRec *subgrid;
			lo_GridCellRec *dummy_cell;

			subgrid = cell_list->subgrid;
			dummy_cell = cell_list;

			subgrid->grid_cell_border = grid->grid_cell_border;
			subgrid->grid_cell_min_dim = grid->grid_cell_min_dim;

			/*
			 * Make sure all the row percentages total up
			 * to 100%
			 */
			lo_adjust_percents(subgrid->rows, subgrid->row_percents,
				cell_list->height);

			/*
			 * Make sure all the col percentages total up
			 * to 100%
			 */
			lo_adjust_percents(subgrid->cols, subgrid->col_percents,
				cell_list->width);

			lo_LayoutGridCells(context, state,
				cell_list->x, cell_list->y,
				cell_list->width, cell_list->height,
				subgrid, new_edges);

			/*
			 * Merge the edge list of the sub-grid with that of
			 * the parent grid.
			 */
			if (subgrid->edge_list != NULL)
			{
				lo_GridEdge *a_edge;

				a_edge = subgrid->edge_list;
				while (a_edge->next != NULL)
				{
					a_edge = a_edge->next;
				}
				a_edge->next = grid->edge_list;
				grid->edge_list = subgrid->edge_list;
				subgrid->edge_list = NULL;
			}

			/*
			 * Replace the subgrid cell in the list with its
			 * own list of member cells.
			 */
			/*
			 * If the cell to be replace is not the head of the
			 * list.
			 */
			if (cell_parent != NULL)
			{
			    /*
			     * If the cell is being replaced with a list
			     * of one or more cells.
			     */
			    if (subgrid->cell_list != NULL)
			    {
				cell_parent->next = subgrid->cell_list;
				subgrid->cell_list_last->next = cell_list->next;
				/*
				 * If the cell being replaced was the end of
				 * the list, set the new end properly.
				 */
				if (grid->cell_list_last == cell_list)
				{
				    grid->cell_list_last =
					subgrid->cell_list_last;
				}
				cell_parent = subgrid->cell_list_last;
				cell_list = cell_list->next;
			    }
			    /*
			     * If the cell is being replaced with
			     * no cells (just remove it.)
			     */
			    else
			    {
				cell_parent->next = cell_list->next;
				/*
				 * If the cell being replaced was the end of
				 * the list, set the new end properly.
				 */
				if (grid->cell_list_last == cell_list)
				{
				    grid->cell_list_last = cell_parent;
				}
				cell_parent = cell_parent;
				cell_list = cell_list->next;
			    }
			}
			/*
			 * Else we are replacing the head of the list.
			 */
			else
			{
			    /*
			     * If the cell is being replaced with a list
			     * of one or more cells.
			     */
			    if (subgrid->cell_list != NULL)
			    {
				grid->cell_list = subgrid->cell_list;
				subgrid->cell_list_last->next = cell_list->next;
				/*
				 * If the cell being replaced was the end of
				 * the list, set the new end properly.
				 */
				if (grid->cell_list_last == cell_list)
				{
				    grid->cell_list_last =
					subgrid->cell_list_last;
				}
				cell_parent = subgrid->cell_list_last;
				cell_list = cell_list->next;
			    }
			    /*
			     * If the cell is being replaced with
			     * no cells (just remove it.)
			     */
			    else
			    {
				grid->cell_list = cell_list->next;
				/*
				 * If the cell being replaced was the end of
				 * the list, set the new end properly.
				 */
				if (grid->cell_list_last == cell_list)
				{
				    grid->cell_list_last = cell_list->next;
				}
				cell_parent = cell_parent;
				cell_list = cell_list->next;
			    }
			}

			lo_FreeGridRec(subgrid);
			lo_FreeGridCellRec(context, grid, dummy_cell);
		}
		/*
		 * Else this was a normal cell.
		 */
		else
		{
			/*
			 * Add this cell to the cell array for each edge
			 * it borders.
			 */
			if (new_edges[TOP_EDGE] != NULL)
			{
				lo_add_cell_to_edge(new_edges[TOP_EDGE],
					cell_list);
			}
			if (new_edges[BOTTOM_EDGE] != NULL)
			{
				lo_add_cell_to_edge(new_edges[BOTTOM_EDGE],
					cell_list);
			}
			if (new_edges[LEFT_EDGE] != NULL)
			{
				lo_add_cell_to_edge(new_edges[LEFT_EDGE],
					cell_list);
			}
			if (new_edges[RIGHT_EDGE] != NULL)
			{
				lo_add_cell_to_edge(new_edges[RIGHT_EDGE],
					cell_list);
			}

			cell_list->side_edges[TOP_EDGE] =
				new_edges[TOP_EDGE];
			cell_list->side_edges[BOTTOM_EDGE] =
				new_edges[BOTTOM_EDGE];
			cell_list->side_edges[LEFT_EDGE] =
				new_edges[LEFT_EDGE];
			cell_list->side_edges[RIGHT_EDGE] =
				new_edges[RIGHT_EDGE];

			cell_parent = cell_list;
			cell_list = cell_list->next;
		}

	}

	if (col_edges != NULL)
	{
		XP_FREE(col_edges);
	}
}

static void
lo_set_edge_bounds(lo_GridEdge *edge)
{
	int32 i;
	int32 min, max;
	Bool visible;
	Bool movable;
	int16 size;
	int8 color_priority;
	LO_Color *bg_color;

	if (edge == NULL)
	{
		return;
	}

	visible = FALSE;
	movable = TRUE;
	size = -1;
	color_priority = 0;
	bg_color = NULL;

	min = -1;
	max = -1;
	/*
	 * For horizontal edges
	 */
	if (edge->is_vertical == FALSE)
	{
		for (i=0; i < edge->cell_cnt; i++)
		{
			lo_GridCellRec *cell;

			cell = edge->cell_array[i];
			if (cell == NULL)
			{
				continue;
			}

			/*
			 * If a cell on this edge is not resizable,
			 * this edge is not movable.
			 */
			if (cell->resizable == FALSE)
			{
				movable = FALSE;
			}

			/*
			 * If a cell on this edge has visible edges,
			 * this edge is visible.
			 */
			if (cell->no_edges == FALSE)
			{
				visible = TRUE;
			}

			/*
			 * A cell's size is the max of the edge_size
			 * of all cells next to it.
			 */
			if (cell->edge_size > size)
			{
				size = cell->edge_size;
			}

			/*
			 * A cell sets a color on an edge that doesn't
			 * already have a color, or has a lower
			 * priority, it wins.
			 */
			if ((cell->border_color != NULL)&&
			    ((bg_color == NULL)||
			     (cell->color_priority > color_priority)))
			{
				bg_color = XP_NEW(LO_Color);
				if (bg_color != NULL)
				{
				    bg_color->red = cell->border_color->red;
				    bg_color->green = cell->border_color->green;
				    bg_color->blue = cell->border_color->blue;
				    color_priority = cell->color_priority;
				}
			}

			/*
			 * If the cell is above the edge.
			 */
			if (cell->y < edge->y)
			{
				if ((cell->y > min)||(min == -1))
				{
					min = cell->y;
				}
			}
			/*
			 * Else the cell is below the edge.
			 */
			else
			{
				int32 bottom;

				bottom = cell->y + cell->height - 1;
				if ((bottom < max)||(max == -1))
				{
					max = bottom;
				}
			}
		}

		/*
		 * Make min and max be the inclusive bounds through
		 * which edge->y can move.
		 * We should never move an edge so that cell->y
		 * becomes equal to edge->y or else in the future we
		 * will thing that cell is on the wrong side of the
		 * edge.  To prevent this, it min is set by a cell's
		 * bounds, we increment it by one.
		 */
		if (min == -1)
		{
			min = edge->y;
		}
		else
		{
			min = min + 1;
		}
		if (max == -1)
		{
			max = edge->y + edge->height - 1;
		}
		else
		{
			max = max - edge->height + 1;
		}
		/*
		 * Sanity check bounds
		 */
		if (max < min)
		{
			max = min;
		}
		edge->left_top_bound = min;
		edge->right_bottom_bound = max;
	}
	/*
	 * Else for vertical edges.
	 */
	else
	{
		for (i=0; i < edge->cell_cnt; i++)
		{
			lo_GridCellRec *cell;

			cell = edge->cell_array[i];
			if (cell == NULL)
			{
				continue;
			}

			/*
			 * If a cell on this edge is not resizable,
			 * this edge is not movable.
			 */
			if (cell->resizable == FALSE)
			{
				movable = FALSE;
			}

			/*
			 * If a cell on this edge has visible edges,
			 * this edge is visible.
			 */
			if (cell->no_edges == FALSE)
			{
				visible = TRUE;
			}

			/*
			 * A cells size is the max of the edge_size
			 * of all cells next to it.
			 */
			if (cell->edge_size > size)
			{
				size = cell->edge_size;
			}

			/*
			 * A cell sets a color on an edge that doesn't
			 * already have a color, or has a lower
			 * priority, it wins.
			 */
			if ((cell->border_color != NULL)&&
			    ((bg_color == NULL)||
			     (cell->color_priority > color_priority)))
			{
				bg_color = XP_NEW(LO_Color);
				if (bg_color != NULL)
				{
				    bg_color->red = cell->border_color->red;
				    bg_color->green = cell->border_color->green;
				    bg_color->blue = cell->border_color->blue;
				    color_priority = cell->color_priority;
				}
			}

			/*
			 * If the cell is left of the edge.
			 */
			if (cell->x < edge->x)
			{
				if ((cell->x > min)||(min == -1))
				{
					min = cell->x;
				}
			}
			/*
			 * Else the cell is right of the edge.
			 */
			else
			{
				int32 right;

				right = cell->x + cell->width - 1;
				if ((right < max)||(max == -1))
				{
					max = right;
				}
			}
		}

		/*
		 * Make min and max be the inclusive bounds through
		 * which edge->x can move.
		 * We should never move an edge so that cell->x
		 * becomes equal to edge->x or else in the future we
		 * will thing that cell is on the wrong side of the
		 * edge.  To prevent this, it min is set by a cell's
		 * bounds, we increment it by one.
		 */
		if (min == -1)
		{
			min = edge->x;
		}
		else
		{
			min = min + 1;
		}
		if (max == -1)
		{
			max = edge->x + edge->width - 1;
		}
		else
		{
			max = max - edge->width + 1;
		}
		/*
		 * Sanity check bounds
		 */
		if (max < min)
		{
			max = min;
		}
		edge->left_top_bound = min;
		edge->right_bottom_bound = max;
	}

	edge->fe_edge->left_top_bound = edge->left_top_bound;
	edge->fe_edge->right_bottom_bound = edge->right_bottom_bound;
	edge->fe_edge->visible = visible;
	edge->fe_edge->movable = movable;
	edge->fe_edge->size = size;
	edge->fe_edge->bg_color = bg_color;
}


static void
lo_set_all_edge_bounds(MWContext *context, lo_DocState *state, lo_GridRec *grid)
{
	lo_GridEdge *edge_list;

	if ((grid == NULL)||(grid->edge_list == NULL))
	{
		return;
	}

	edge_list = grid->edge_list;
	while (edge_list != NULL)
	{
		lo_set_edge_bounds(edge_list);

		/*
		 * Insert this edge into the float list.
		 */
		edge_list->fe_edge->next = state->float_list;
		state->float_list = (LO_Element *)edge_list->fe_edge;

		lo_DisplayEdge(context, edge_list->fe_edge);
		edge_list = edge_list->next;
	}
}


/*
 * Use the cell's history index to reload the document
 * it should now be currently displaying.
 * (Note, history index starts at 1, and array starts at 0).
 */
static void
lo_reload_cell_from_history(lo_GridCellRec *cell_ptr,
	NET_ReloadMethod force_reload)
{
	void *hist;

	hist = cell_ptr->hist_array[cell_ptr->hist_indx - 1].hist;

	/*
	 * If we have a history entry, and a context to load
	 * it into, have the FE load it now.
	 */
	if ((hist != NULL)&&(cell_ptr->context != NULL))
	{
		FE_LoadGridCellFromHistory(cell_ptr->context, hist,
			force_reload);
	}
}


static lo_GridCellRec *reconnecting_cell = NULL;


PRIVATE
void
lo_MakeGrid(MWContext *context, lo_DocState *state, lo_GridRec *grid)
{
	int32 x, y;
	int32 edge_size;
	int32 width, height;
	lo_GridCellRec *cell_ptr;
	lo_GridEdge *edges[4];	/* top, bottom, left, right */

	width = state->win_width;
	height = state->win_height;
	FE_GetFullWindowSize(context, &width, &height);

	if (width <= 0)
		width = 1;

	if (height <= 0)
		height = 1;

	grid->main_width = width;
	grid->main_height = height;

#ifdef NO_FRAMESET_BORDER
	edge_size = 1;
	FE_GetEdgeMinSize(context, &edge_size);
	if (edge_size < 1)
	{
		edge_size = 1;
	}
#else
	if (grid->edge_size < 0)
	{
		edge_size = 1;
		FE_GetEdgeMinSize(context, &edge_size
#if defined(XP_WIN) || defined(XP_OS2)
		/* need this information here under windows */
		, grid->no_edges
#endif
            );
	}
	else
	{
		edge_size = grid->edge_size;
	}
	if (edge_size < 0)
	{
		edge_size = 0;
	}
#endif /* NO_FRAMESET_BORDER */
	grid->grid_cell_border = (edge_size + 1) / 2;
	grid->grid_cell_min_dim = (2 * grid->grid_cell_border) + 1;

	x = 0;
	y = 0;

	/*
	 * Make sure all the row percentages total up
	 * to 100%
	 */
	lo_adjust_percents(grid->rows, grid->row_percents, height);

	/*
	 * Make sure all the col percentages total up
	 * to 100%
	 */
	lo_adjust_percents(grid->cols, grid->col_percents, width);

	edges[TOP_EDGE] = NULL;
	edges[BOTTOM_EDGE] = NULL;
	edges[LEFT_EDGE] = NULL;
	edges[RIGHT_EDGE] = NULL;
	lo_LayoutGridCells(context, state, x, y, width, height, grid, edges);
	lo_set_all_edge_bounds(context, state, grid);

	/*
	 * If there is an old_grid in the top_state, that means we restored
	 * from history with the sizes changed.  Go through that old_grid
	 * now, and put those urls back into the cells, then free the old_grid.
	 */
	if (state->top_state->old_grid != NULL)
	{
		lo_GridRec *old_grid;
		lo_GridCellRec *old_cell_ptr;
		lo_SavedGridData tmp_saved_grid;

		old_grid = state->top_state->old_grid;
		grid->current_hist = old_grid->current_hist;
		grid->max_hist = old_grid->max_hist;
		grid->hist_size = old_grid->hist_size;
		old_cell_ptr = old_grid->cell_list;
		cell_ptr = grid->cell_list;
		while ((cell_ptr != NULL)&&(old_cell_ptr != NULL))
		{
			/*
			 * Free any initial URL loaded, and move the old
			 * url in place.  Make sure to clear the old URL
			 * so we don't accidentally free it later.
			 */
			if (cell_ptr->url != NULL)
			{
				XP_FREE(cell_ptr->url);
				cell_ptr->url = NULL;
			}
			cell_ptr->url = old_cell_ptr->url;
			old_cell_ptr->url = NULL;

			/*
			 * Also copy over the old history.
			 */
			cell_ptr->hist_indx = old_cell_ptr->hist_indx;
			cell_ptr->hist_array = old_cell_ptr->hist_array;
			old_cell_ptr->hist_array = NULL;
			cell_ptr->hist_list = old_cell_ptr->hist_list;
			old_cell_ptr->hist_list = NULL;

			cell_ptr = cell_ptr->next;
			old_cell_ptr = old_cell_ptr->next;
		}

		/*
		 * Free up the old saved grid.
		 */
		tmp_saved_grid.main_width = 0;
		tmp_saved_grid.main_height = 0;
		tmp_saved_grid.the_grid = state->top_state->old_grid;
		state->top_state->old_grid = NULL;
		lo_FreeDocumentGridData(context, &tmp_saved_grid);
	}

	/*
	 * Go through all the cells creating them where specified.
	 */
	cell_ptr = grid->cell_list;
	while (cell_ptr != NULL)
	{
		/*
		 * Have the front end create the grid cell.
		 */
		if (cell_ptr->url != NULL)
		{
			void *history;
			void *hist_list;

			history = NULL;
			hist_list = NULL;
			if (cell_ptr->hist_list != NULL)
			{
				hist_list = cell_ptr->hist_list;
				cell_ptr->hist_indx = grid->current_hist;
				history = cell_ptr->hist_array[cell_ptr->hist_indx - 1].hist;
				if (history == NULL)
				{
					hist_list = NULL;
				}
			}
			reconnecting_cell = cell_ptr;
			cell_ptr->context = FE_MakeGridWindow (context,
				hist_list,
				history,
				cell_ptr->x, cell_ptr->y,
				cell_ptr->width, cell_ptr->height,
				(char *)cell_ptr->url, (char *)cell_ptr->name,
				cell_ptr->scrolling,
				FORCE_RELOAD_FLAG(state->top_state),
				cell_ptr->no_edges);
			reconnecting_cell = NULL;
			if (cell_ptr->context)
			{
			    XP_ASSERT(cell_ptr->context->forceJSEnabled ==
				context->forceJSEnabled);
			    if (history)
			    {
				cell_ptr->context->hist.cur_doc_ptr = history;
			    }
			}
		}
		else
		{
			cell_ptr->context = NULL;
		}
		cell_ptr = cell_ptr->next;
	}
}



void
lo_BeginGrid(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	PA_Block buff;
	char *str;
	int32 rows, cols;
	lo_GridPercent *row_percents;
	lo_GridPercent *col_percents;
	lo_GridRec *grid;

	rows = 1;
	cols = 1;

	grid = XP_NEW(lo_GridRec);
	if (grid == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}

	grid->no_edges = FALSE;
	grid->edge_size = -1;
	grid->color_priority = 0;
	grid->border_color = NULL;
	grid->edge_list = NULL;
	grid->cell_list = NULL;
	grid->cell_list_last = NULL;
	grid->subgrid = NULL;

	buff = lo_FetchParamValue(context, tag, PARAM_FRAMEBORDER);
	if (buff != NULL)
	{
		Bool no_edges;

		no_edges = FALSE;
		PA_LOCK(str, char *, buff);
		if (pa_TagEqual("no", str))
		{
			no_edges = TRUE;
		}
		else if (pa_TagEqual("0", str))
		{
			no_edges = TRUE;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		grid->no_edges = no_edges;
	}

	buff = lo_FetchParamValue(context, tag, PARAM_BORDER);
	if (buff != NULL)
	{
		int32 border;

		PA_LOCK(str, char *, buff);
		border = XP_ATOI(str);
		PA_UNLOCK(buff);
		PA_FREE(buff);
		if (border < 0)
		{
			border = 0;
		}
		if (border > 100)
		{
			border = 100;
		}
		grid->edge_size = (int16)border;
	}

	/*
	 * A borderwidth of zero automatically turns off edges.
	 */
	if (grid->edge_size == 0)
	{
		grid->no_edges = TRUE;
	}

	/*
	 * Check for a border color attribute
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_BORDERCOLOR);
	if (buff != NULL)
	{
		uint8 red, green, blue;

		PA_LOCK(str, char *, buff);
		LO_ParseRGB(str, &red, &green, &blue);
		PA_UNLOCK(buff);
		PA_FREE(buff);
		grid->border_color = XP_NEW(LO_Color);
		if (grid->border_color != NULL)
		{
			grid->border_color->red = red;
			grid->border_color->green = green;
			grid->border_color->blue = blue;
			grid->color_priority = 1;
		}
	}

	buff = lo_FetchParamValue(context, tag, PARAM_ROWS);
	if (buff != NULL)
	{
		int32 len;

		PA_LOCK(str, char *, buff);
		len = lo_StripTextWhitespace(str, XP_STRLEN(str));
		row_percents = lo_parse_percent_list(context, str, &rows);
		if ((row_percents == NULL)&&(rows >= 1))
		{
			PA_UNLOCK(buff);
			PA_FREE(buff);
			XP_DELETE(grid);
			state->top_state->out_of_memory = TRUE;
			return;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		if (rows <= 0)
		{
			row_percents = (lo_GridPercent *)
				 XP_ALLOC(sizeof(lo_GridPercent));
			if (row_percents == NULL)
			{
				XP_DELETE(grid);
				state->top_state->out_of_memory = TRUE;
				return;
			}
			row_percents[0].type = LO_PERCENT_NORMAL;
			row_percents[0].original_value = 100;
			rows = 1;
		}
	}
	else
	{
		row_percents = (lo_GridPercent *)
			XP_ALLOC(sizeof(lo_GridPercent));
		if (row_percents == NULL)
		{
			XP_DELETE(grid);
			state->top_state->out_of_memory = TRUE;
			return;
		}
		row_percents[0].type = LO_PERCENT_NORMAL;
		row_percents[0].original_value = 100;
		rows = 1;
	}

	buff = lo_FetchParamValue(context, tag, PARAM_COLS);
	if (buff != NULL)
	{
		int32 len;

		PA_LOCK(str, char *, buff);
		len = lo_StripTextWhitespace(str, XP_STRLEN(str));
		col_percents = lo_parse_percent_list(context, str, &cols);
		if ((col_percents == NULL)&&(cols >= 1))
		{
			PA_UNLOCK(buff);
			PA_FREE(buff);
			if (row_percents != NULL)
			{
				XP_FREE(row_percents);
			}
			XP_DELETE(grid);
			state->top_state->out_of_memory = TRUE;
			return;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		if (cols <= 0)
		{
			col_percents = (lo_GridPercent *)
				XP_ALLOC(sizeof(lo_GridPercent));
			if (col_percents == NULL)
			{
				if (row_percents != NULL)
				{
					XP_FREE(row_percents);
				}
				XP_DELETE(grid);
				state->top_state->out_of_memory = TRUE;
				return;
			}
			col_percents[0].type = LO_PERCENT_NORMAL;
			col_percents[0].original_value = 100;
			cols = 1;
		}
	}
	else
	{
		col_percents = (lo_GridPercent *)
			XP_ALLOC(sizeof(lo_GridPercent));
		if (col_percents == NULL)
		{
			if (row_percents != NULL)
			{
				XP_FREE(row_percents);
			}
			XP_DELETE(grid);
			state->top_state->out_of_memory = TRUE;
			return;
		}
		col_percents[0].type = LO_PERCENT_NORMAL;
		col_percents[0].original_value = 100;
		cols = 1;
	}

	if ((rows == 1)&&(cols == 1))
	{
		if (row_percents != NULL)
		{
			XP_FREE(row_percents);
		}
		if (col_percents != NULL)
		{
			XP_FREE(col_percents);
		}
		XP_DELETE(grid);
		return;
	}

#ifdef MOCHA
	(void)lo_ProcessContextEventHandlers(context, state, tag);
#endif

	grid->rows = rows;
	grid->row_percents = row_percents;
	grid->cols = cols;
	grid->col_percents = col_percents;
	grid->cell_count = 0;

	grid->current_hist = 0;
	grid->max_hist = 0;
	grid->hist_size = 10;

	grid->main_width = state->win_width;
	grid->main_height = state->win_height;
	grid->current_cell_margin_width = 0;
	grid->current_cell_margin_height = 0;

	state->current_grid = grid;
}


void
lo_EndGrid(MWContext *context, lo_DocState *state, lo_GridRec *grid)
{
	int32 cell_cnt;

	if (grid == NULL)
	{
		return;
	}

	cell_cnt = grid->rows * grid->cols;

	state->top_state->the_grid = grid;

	lo_MakeGrid(context, state, grid);
}


static Bool
lo_is_grid_recursion(MWContext *context, lo_DocState *state, char *url)
{
	MWContext *cptr;

	/*
	 * If this cell has its parent's URL, it is recursion.
	 */
	if ((state->top_state->url != NULL)&&
	    (XP_STRCMP(url, state->top_state->url) == 0))
	{
		return(TRUE);
	}

	cptr = context->grid_parent;
	while (cptr != NULL)
	{
		int32 doc_id;
		lo_TopState *top_state;
		char *anc_url;

		/*
		 * Get the unique document ID, and retreive this
		 * documents layout state.
		 */
		doc_id = XP_DOCID(cptr);
		top_state = lo_FetchTopState(doc_id);
		/*
		 * We have to get the ancenstor URL from the state because
		 * context->url doesn't seem to be reliable.
		 */
		if (top_state == NULL)
		{
			anc_url = NULL;
		}
		else
		{
			anc_url = top_state->url;
		}

		/*
		 * if this cell has the same URL as any of its ancestors
		 * this is recursion.
		 */
		if ((anc_url != NULL)&&(XP_STRCMP(url, anc_url) == 0))
		{
			return(TRUE);
		}
		cptr = cptr->grid_parent;
	}
	return(FALSE);
}


void
lo_BeginGridCell(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	PA_Block buff;
	char *str;
	lo_GridRec *grid;
	int32 col_cnt, row_cnt;
	lo_GridCellRec *cell;

	/*
	 * Get the current grid, if there is none, error out.
	 */
	grid = state->current_grid;
	if (grid == NULL)
	{
		return;
	}

	/*
	 * If we have subgrids, the deepest one is at the TOP of the
	 * subgrid stack.
	 */
	if (grid->subgrid != NULL)
	{
		grid = grid->subgrid;
	}

	/*
	 * If this grid has its full complement of cells, ignore
	 * this cell.
	 */
	if (grid->cell_count >= (grid->rows * grid->cols))
	{
		return;
	}

	cell = XP_NEW(lo_GridCellRec);
	if (cell == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}

	row_cnt = grid->cell_count / grid->cols;
	col_cnt = grid->cell_count - (row_cnt *  grid->cols);

	cell->x = 0;
	cell->y = 0;
	cell->width = 0;
	cell->height = 0;
	cell->margin_width = 0;
	cell->margin_height = 0;
	cell->width_percent = 0;
	cell->height_percent = 0;
	cell->url = NULL;
	cell->name = NULL;
	cell->context = NULL;
	cell->hist_list = NULL;
	cell->hist_array = (lo_GridHistory *)XP_ALLOC(grid->hist_size *
				sizeof(lo_GridHistory));
	if (cell->hist_array == NULL)
	{
		XP_DELETE(cell);
		state->top_state->out_of_memory = TRUE;
		return;
	}
	XP_MEMSET(cell->hist_array, 0, grid->hist_size *
				sizeof(lo_GridHistory));
	cell->hist_indx = 0;
	cell->next = NULL;
	cell->subgrid = NULL;
	cell->side_edges[TOP_EDGE] = NULL;
	cell->side_edges[BOTTOM_EDGE] = NULL;
	cell->side_edges[LEFT_EDGE] = NULL;
	cell->side_edges[RIGHT_EDGE] = NULL;
	cell->scrolling = LO_SCROLL_AUTO;
	cell->no_edges = grid->no_edges;
	cell->resizable = TRUE;
	cell->edge_size = -1;
	cell->color_priority = 0;
	cell->border_color = NULL;

	buff = lo_FetchParamValue(context, tag, PARAM_FRAMEBORDER);
	if (buff != NULL)
	{
		Bool no_edges;

		no_edges = FALSE;
		PA_LOCK(str, char *, buff);
		if (pa_TagEqual("no", str))
		{
			no_edges = TRUE;
		}
		else if (pa_TagEqual("0", str))
		{
			no_edges = TRUE;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		cell->no_edges = no_edges;
	}

	buff = lo_FetchParamValue(context, tag, PARAM_BORDER);
	if (buff != NULL)
	{
		int32 border;

		PA_LOCK(str, char *, buff);
		border = XP_ATOI(str);
		PA_UNLOCK(buff);
		PA_FREE(buff);
		if (border < 0)
		{
			border = 0;
		}
		if (border > 100)
		{
			border = 100;
		}
		cell->edge_size = (int16)border;
	}

	/*
	 * A borderwidth of zero automatically turns off edges.
	 */
	if (cell->edge_size == 0)
	{
		cell->no_edges = TRUE;
	}

	/*
	 * Check for a border color attribute
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_BORDERCOLOR);
	if (buff != NULL)
	{
		uint8 red, green, blue;

		PA_LOCK(str, char *, buff);
		LO_ParseRGB(str, &red, &green, &blue);
		PA_UNLOCK(buff);
		PA_FREE(buff);
		cell->border_color = XP_NEW(LO_Color);
		if (cell->border_color != NULL)
		{
			cell->border_color->red = red;
			cell->border_color->green = green;
			cell->border_color->blue = blue;
			cell->color_priority = 3;
		}
	}
	/*
	 * Else we might inherit a border color from parent.
	 */
	else if (grid->border_color != NULL)
	{
		cell->border_color = XP_NEW(LO_Color);
		if (cell->border_color != NULL)
		{
		    cell->border_color->red = grid->border_color->red;
		    cell->border_color->green = grid->border_color->green;
		    cell->border_color->blue = grid->border_color->blue;
		    cell->color_priority = grid->color_priority;
		}
	}

	buff = lo_FetchParamValue(context, tag, PARAM_NORESIZE);
	if (buff != NULL)
	{
		PA_FREE(buff);
		cell->resizable = FALSE;
	}

	buff = lo_FetchParamValue(context, tag, PARAM_SCROLLING);
	if (buff != NULL)
	{
		int8 scrolling;

		scrolling = LO_SCROLL_AUTO;
		PA_LOCK(str, char *, buff);
		if (pa_TagEqual("yes", str))
		{
			scrolling = LO_SCROLL_YES;
		}
		else if (pa_TagEqual("scroll", str))
		{
			scrolling = LO_SCROLL_YES;
		}
		else if (pa_TagEqual("no", str))
		{
			scrolling = LO_SCROLL_NO;
		}
		else if (pa_TagEqual("noscroll", str))
		{
			scrolling = LO_SCROLL_NO;
		}
		else if (pa_TagEqual("on", str))
		{
			scrolling = LO_SCROLL_YES;
		}
		else if (pa_TagEqual("off", str))
		{
			scrolling = LO_SCROLL_NO;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		cell->scrolling = scrolling;
	}

	buff = lo_FetchParamValue(context, tag, PARAM_SRC);
	if (buff != NULL)
	{
		char *new_str;

		PA_LOCK(str, char *, buff);
		if (str != NULL)
		{
			int32 len;

			len = lo_StripTextWhitespace(str, XP_STRLEN(str));
		}
		new_str = NET_MakeAbsoluteURL(state->top_state->base_url, str);

		PA_UNLOCK(buff);
		PA_FREE(buff);
		cell->url = new_str;
		/*
		 * Don't allow cells to have the same URL as their parent
		 * or any of their ancestors as that would cause infinite
		 * recursion.
		 */
		if (lo_is_grid_recursion(context, state, cell->url) != FALSE)
		{
			XP_FREE(new_str);
			cell->url = NULL;
		}
	}
	else
	{
		cell->url = NULL;
	}

	buff = lo_FetchParamValue(context, tag, PARAM_NAME);
	if (buff != NULL)
	{
		char *name;

		PA_LOCK(str, char *, buff);
		if (str != NULL)
		{
			int32 len;

			len = lo_StripTextWhitespace(str, XP_STRLEN(str));
		}
		name = (char *)XP_ALLOC(XP_STRLEN(str) + 1);
		if (name == NULL)
		{
			cell->name = NULL;
		}
		else
		{
			XP_STRCPY(name, str);
			cell->name = name;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	else
	{
		cell->name = NULL;
	}

	/*
	 * Get the margin width.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_MARGINWIDTH);
	if (buff != NULL)
	{
		int32 val;

		PA_LOCK(str, char *, buff);
		val = XP_ATOI(str);
		if (val < 1)
		{
			val = 1;
		}
		cell->margin_width = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	cell->margin_width = FEUNITS_X(cell->margin_width, context);

	/*
	 * Get the margin height.
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_MARGINHEIGHT);
	if (buff != NULL)
	{
		int32 val;

		PA_LOCK(str, char *, buff);
		val = XP_ATOI(str);
		if (val < 1)
		{
			val = 1;
		}
		cell->margin_height = val;
		PA_UNLOCK(buff);
		PA_FREE(buff);
	}
	cell->margin_height = FEUNITS_Y(cell->margin_height, context);

	grid->cell_count++;

	if (grid->cell_list_last == NULL)
	{
		grid->cell_list = cell;
		grid->cell_list_last = cell;
	}
	else
	{
		grid->cell_list_last->next = cell;
		grid->cell_list_last = cell;
	}
}


void
lo_BeginSubgrid(MWContext *context, lo_DocState *state, PA_Tag *tag)
{
	PA_Block buff;
	char *str;
	lo_GridRec *grid;
	int32 col_cnt, row_cnt;
	lo_GridRec *subgrid;
	int32 rows, cols;
	lo_GridPercent *row_percents;
	lo_GridPercent *col_percents;
	lo_GridCellRec *cell;

	/*
	 * Get the current grid, if there is none, error out.
	 */
	grid = state->current_grid;
	if (grid == NULL)
	{
		return;
	}

	/*
	 * If we have subgrids, the deepest one is at the TOP of the
	 * subgrid stack.
	 */
	if (grid->subgrid != NULL)
	{
		grid = grid->subgrid;
	}

	/*
	 * If this grid has its full complement of cells, ignore
	 * this subgrid cell.
	 */
	if (grid->cell_count >= (grid->rows * grid->cols))
	{
		return;
	}


	/*
	 ********************************
	 * Create and parse the subgrid *
	 ********************************
	 */
	rows = 1;
	cols = 1;

	subgrid = XP_NEW(lo_GridRec);
	if (subgrid == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}

	subgrid->no_edges = grid->no_edges;
	subgrid->edge_size = grid->edge_size;
	subgrid->color_priority = 0;
	subgrid->border_color = NULL;
	subgrid->edge_list = NULL;
	subgrid->cell_list = NULL;
	subgrid->cell_list_last = NULL;
	subgrid->subgrid = NULL;

	/*
	 * Check for a border color attribute
	 */
	buff = lo_FetchParamValue(context, tag, PARAM_BORDERCOLOR);
	if (buff != NULL)
	{
		uint8 red, green, blue;

		PA_LOCK(str, char *, buff);
		LO_ParseRGB(str, &red, &green, &blue);
		PA_UNLOCK(buff);
		PA_FREE(buff);
		subgrid->border_color = XP_NEW(LO_Color);
		if (subgrid->border_color != NULL)
		{
			subgrid->border_color->red = red;
			subgrid->border_color->green = green;
			subgrid->border_color->blue = blue;
			subgrid->color_priority = 2;
		}
	}
	/*
	 * Else we might inherit a border color from parent.
	 */
	else if (grid->border_color != NULL)
	{
		subgrid->border_color = XP_NEW(LO_Color);
		if (subgrid->border_color != NULL)
		{
		    subgrid->border_color->red = grid->border_color->red;
		    subgrid->border_color->green = grid->border_color->green;
		    subgrid->border_color->blue = grid->border_color->blue;
		    subgrid->color_priority = grid->color_priority;
		}
	}

	buff = lo_FetchParamValue(context, tag, PARAM_FRAMEBORDER);
	if (buff != NULL)
	{
		Bool no_edges;

		no_edges = FALSE;
		PA_LOCK(str, char *, buff);
		if (pa_TagEqual("no", str))
		{
			no_edges = TRUE;
		}
		else if (pa_TagEqual("0", str))
		{
			no_edges = TRUE;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		subgrid->no_edges = no_edges;
	}

	buff = lo_FetchParamValue(context, tag, PARAM_ROWS);
	if (buff != NULL)
	{
		int32 len;

		PA_LOCK(str, char *, buff);
		len = lo_StripTextWhitespace(str, XP_STRLEN(str));
		row_percents = lo_parse_percent_list(context, str, &rows);
		if ((row_percents == NULL)&&(rows >= 1))
		{
			PA_UNLOCK(buff);
			PA_FREE(buff);
			XP_DELETE(subgrid);
			state->top_state->out_of_memory = TRUE;
			return;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		if (rows <= 0)
		{
			row_percents = (lo_GridPercent *)
				XP_ALLOC(sizeof(lo_GridPercent));
			if (row_percents == NULL)
			{
				XP_DELETE(subgrid);
				state->top_state->out_of_memory = TRUE;
				return;
			}
			row_percents[0].type = LO_PERCENT_NORMAL;
			row_percents[0].original_value = 100;
			rows = 1;
		}
	}
	else
	{
		row_percents = (lo_GridPercent *)
			XP_ALLOC(sizeof(lo_GridPercent));
		if (row_percents == NULL)
		{
			XP_DELETE(subgrid);
			state->top_state->out_of_memory = TRUE;
			return;
		}
		row_percents[0].type = LO_PERCENT_NORMAL;
		row_percents[0].original_value = 100;
		rows = 1;
	}

	buff = lo_FetchParamValue(context, tag, PARAM_COLS);
	if (buff != NULL)
	{
		int32 len;

		PA_LOCK(str, char *, buff);
		len = lo_StripTextWhitespace(str, XP_STRLEN(str));
		col_percents = lo_parse_percent_list(context, str, &cols);
		if ((col_percents == NULL)&&(cols >= 1))
		{
			PA_UNLOCK(buff);
			PA_FREE(buff);
			if (row_percents != NULL)
			{
				XP_FREE(row_percents);
			}
			XP_DELETE(subgrid);
			state->top_state->out_of_memory = TRUE;
			return;
		}
		PA_UNLOCK(buff);
		PA_FREE(buff);
		if (cols <= 0)
		{
			col_percents = (lo_GridPercent *)
				XP_ALLOC(sizeof(lo_GridPercent));
			if (col_percents == NULL)
			{
				if (row_percents != NULL)
				{
					XP_FREE(row_percents);
				}
				XP_DELETE(subgrid);
				state->top_state->out_of_memory = TRUE;
				return;
			}
			col_percents[0].type = LO_PERCENT_NORMAL;
			col_percents[0].original_value = 100;
			cols = 1;
		}
	}
	else
	{
		col_percents = (lo_GridPercent *)
			XP_ALLOC(sizeof(lo_GridPercent));
		if (col_percents == NULL)
		{
			if (row_percents != NULL)
			{
				XP_FREE(row_percents);
			}
			XP_DELETE(subgrid);
			state->top_state->out_of_memory = TRUE;
			return;
		}
		col_percents[0].type = LO_PERCENT_NORMAL;
		col_percents[0].original_value = 100;
		cols = 1;
	}

	if ((rows == 1)&&(cols == 1))
	{
		if (row_percents != NULL)
		{
			XP_FREE(row_percents);
		}
		if (col_percents != NULL)
		{
			XP_FREE(col_percents);
		}
		XP_DELETE(subgrid);
		return;
	}

#ifdef MOCHA
	(void)lo_ProcessContextEventHandlers(context, state, tag);
#endif

	subgrid->rows = rows;
	subgrid->row_percents = row_percents;
	subgrid->cols = cols;
	subgrid->col_percents = col_percents;
	subgrid->cell_count = 0;

	subgrid->current_hist = 0;
	subgrid->max_hist = 0;
	subgrid->hist_size = 10;

	subgrid->main_width = state->win_width;
	subgrid->main_height = state->win_height;
	subgrid->current_cell_margin_width = 0;
	subgrid->current_cell_margin_height = 0;

	subgrid->subgrid = state->current_grid->subgrid;
	state->current_grid->subgrid = subgrid;

	/*
	 ******************************************
	 * Subgrid parsing done.                  *
	 * Create a dummy cell to hang it off of. *
	 ******************************************
	 */

	cell = XP_NEW(lo_GridCellRec);
	if (cell == NULL)
	{
		state->top_state->out_of_memory = TRUE;
		return;
	}

	row_cnt = grid->cell_count / grid->cols;
	col_cnt = grid->cell_count - (row_cnt *  grid->cols);

	cell->x = 0;
	cell->y = 0;
	cell->width = 0;
	cell->height = 0;
	cell->margin_width = 0;
	cell->margin_height = 0;
	cell->width_percent = 0;
	cell->height_percent = 0;
	cell->url = NULL;
	cell->name = NULL;
	cell->context = NULL;
	cell->hist_list = NULL;
	cell->hist_array = (lo_GridHistory *)XP_ALLOC(grid->hist_size *
				sizeof(lo_GridHistory));
	if (cell->hist_array == NULL)
	{
		XP_DELETE(cell);
		state->top_state->out_of_memory = TRUE;
		return;
	}
	XP_MEMSET(cell->hist_array, 0, grid->hist_size *
				sizeof(lo_GridHistory));
	cell->hist_indx = 0;
	cell->next = NULL;
	cell->subgrid = subgrid;
	cell->side_edges[TOP_EDGE] = NULL;
	cell->side_edges[BOTTOM_EDGE] = NULL;
	cell->side_edges[LEFT_EDGE] = NULL;
	cell->side_edges[RIGHT_EDGE] = NULL;
	cell->scrolling = LO_SCROLL_AUTO;
	cell->no_edges = subgrid->no_edges;
	cell->resizable = TRUE;
	cell->edge_size = -1;
	cell->color_priority = 0;
	cell->border_color = NULL;

	grid->cell_count++;

	if (grid->cell_list_last == NULL)
	{
		grid->cell_list = cell;
		grid->cell_list_last = cell;
	}
	else
	{
		grid->cell_list_last->next = cell;
		grid->cell_list_last = cell;
	}
}


void
lo_EndSubgrid(MWContext *context, lo_DocState *state, lo_GridRec *grid)
{
	lo_GridRec *subgrid;

	if ((grid == NULL)||(grid->subgrid == NULL))
	{
		return;
	}

	subgrid = grid->subgrid;
	grid->subgrid = grid->subgrid->subgrid;
	subgrid->subgrid = NULL;
}


#ifdef DEBUG_EDGES
static void
lo_print_edge_info(lo_GridEdge *edge)
{
	int32 i;

	fprintf(stderr, "\nEdge %ld,%ld %ldx%ld; Range is %ld to %ld\n",
		(long) edge->x, (long) edge->y, (long) edge->width,
		(long) edge->height, (long) edge->left_top_bound,
		(long) edge->right_bottom_bound);
	fprintf(stderr, "\tBounds %ld cells\n", (long) edge->cell_cnt);
	for (i=0; i < edge->cell_cnt; i++)
	{
		lo_GridCellRec *cell;

		cell = edge->cell_array[i];
		fprintf(stderr, "\t(%ld) %ld,%ld %ldx%ld\n",
			(long) i, (long) cell->x, (long) cell->y,
			(long) cell->width, (long) cell->height);
	}
	fprintf(stderr, "\n");
}
#endif /* DEBUG_EDGES */


LO_Element *
lo_XYToGridEdge(MWContext *context, lo_DocState *state, int32 x, int32 y)
{
	lo_GridRec *grid;
	lo_GridEdge *edge_list;

	if ((state->top_state->is_grid == FALSE)||
	    (state->top_state->the_grid == NULL))
	{
		return(NULL);
	}

	grid = state->top_state->the_grid;
	edge_list = grid->edge_list;
	while (edge_list != NULL)
	{
		/*
		 * You cannot grab invisible edges.
		 */
		if (edge_list->fe_edge->visible == FALSE)
		{
			edge_list = edge_list->next;
			continue;
		}

		if (edge_list->is_vertical == FALSE)
		{
			if ((y >= edge_list->y)&&
			    (y < (edge_list->y + edge_list->height))&&
			    (x >= edge_list->x)&&
			    (x < (edge_list->x + edge_list->width)))
			{
				break;
			}
		}
		else
		{
			if ((x >= edge_list->x)&&
			    (x < (edge_list->x + edge_list->width))&&
			    (y >= edge_list->y)&&
			    (y < (edge_list->y + edge_list->height)))
			{
				break;
			}
		}
		edge_list = edge_list->next;
	}

	if (edge_list != NULL)
	{
#ifdef DEBUG_EDGES
		lo_print_edge_info(edge_list);
#endif /* DEBUG_EDGES */
		return((LO_Element *)edge_list->fe_edge);
	}
	else
	{
		return(NULL);
	}
}


/*
 * Edges left of a moved vertical edge may need to be shrunk
 * or grown with the edge.
 */
static void
lo_change_left_edges(MWContext *context, lo_GridEdge *edge,
		     int32 bound, int32 diff, Bool call_display_edge)
{
	if (edge != NULL)
	{
		/*
		 * If this edge abuts the edge moved,
		 * change this edges width just like
		 * the cell.
		 */
		if ((edge->x + edge->width) == bound)
		{
			edge->width += diff;
			edge->fe_edge->width += diff;

			if (call_display_edge)
			  /*
			   * Display the edge that was just changed.
			   */
			  lo_DisplayEdge(context, edge->fe_edge);
		}
	}
}


/*
 * Edges right of a moved vertical edge may need to be moved and shrunk
 * or grown with the edge.
 */
static void
lo_change_right_edges(MWContext *context, lo_GridEdge *edge,
		      int32 bound, int32 diff, Bool call_display_edge)
{
	if (edge != NULL)
	{
		/*
		 * If this edge abuts the edge moved,
		 * change this edges position and width just like
		 * the cell.
		 */
		if ((edge->x - 1) == bound)
		{
			edge->x += diff;
			edge->width -= diff;
			edge->fe_edge->x += diff;
			edge->fe_edge->width -= diff;
			if (call_display_edge)
			  /*
			   * Display the edge that was just changed.
			   */
			  lo_DisplayEdge(context, edge->fe_edge);
		}
	}
}


/*
 * Edges above a moved vertical edge may need to be shrunk
 * or grown with the edge.
 */
static void
lo_change_top_edges(MWContext *context, lo_GridEdge *edge,
		    int32 bound, int32 diff, Bool call_display_edge)
{
	if (edge != NULL)
	{
		/*
		 * If this edge abuts the edge moved,
		 * change this edges width just like
		 * the cell.
		 */
		if ((edge->y + edge->height) == bound)
		{
			edge->height += diff;
			edge->fe_edge->height += diff;
			if (call_display_edge)
			  /*
			   * Display the edge that was just changed.
			   */
			  lo_DisplayEdge(context, edge->fe_edge);
		}
	}
}


/*
 * Edges below a moved horizontal edge may need to be moved and shrunk
 * or grown with the edge.
 */
static void
lo_change_bottom_edges(MWContext *context, lo_GridEdge *edge,
		       int32 bound, int32 diff, Bool call_display_edge)
{
	if (edge != NULL)
	{
		/*
		 * If this edge abuts the edge moved,
		 * change this edges position and width just like
		 * the cell.
		 */
		if ((edge->y - 1) == bound)
		{
			edge->y += diff;
			edge->height -= diff;
			edge->fe_edge->y += diff;
			edge->fe_edge->height -= diff;
			if (call_display_edge)
			  /*
			   * Display the edge that was just changed.
			   */
			  lo_DisplayEdge(context, edge->fe_edge);
		}
	}
}


void
LO_MoveGridEdge(MWContext *context, LO_EdgeStruct *fe_edge, int32 x, int32 y)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	int32 i;
	lo_GridEdge *edge;

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

	edge = (lo_GridEdge *)fe_edge->lo_edge_info;
	if (edge == NULL)
	{
		return;
	}

	/*
	 * For horizontal edges
	 */
	if (edge->is_vertical == FALSE)
	{
		int32 diff;

		diff = y - edge->y;

		for (i=0; i < edge->cell_cnt; i++)
		{
			lo_GridCellRec *cell;

			cell = edge->cell_array[i];
			if (cell == NULL)
			{
				continue;
			}
			/*
			 * If the cell is above the edge.
			 * Grow the cell and any abutting
			 * edges.
			 */
			if (cell->y < edge->y)
			{
				cell->height += diff;
				lo_change_top_edges(context,
					cell->side_edges[LEFT_EDGE],
					edge->y, diff, TRUE);
				lo_change_top_edges(context,
					cell->side_edges[RIGHT_EDGE],
					edge->y, diff, TRUE);
				/*
				 * The bounds of this edge are effected
				 */
				lo_set_edge_bounds(cell->side_edges[TOP_EDGE]);
			}
			/*
			 * Else the cell is below the edge.
			 * Move and shrink the cell and any abutting
			 * edges.
			 */
			else
			{
				cell->y += diff;
				cell->height -= diff;
				lo_change_bottom_edges(context,
					cell->side_edges[LEFT_EDGE],
					(edge->y + edge->height - 1), diff, TRUE);
				lo_change_bottom_edges(context,
					cell->side_edges[RIGHT_EDGE],
					(edge->y + edge->height - 1), diff, TRUE);
				/*
				 * The bounds of this edge are effected
				 */
				lo_set_edge_bounds(cell->side_edges[BOTTOM_EDGE]);
			}
			/*
			 * Only restructure cells that have windows(contexts)
			 */
			if (cell->context != NULL)
			{
				FE_RestructureGridWindow (cell->context,
					cell->x, cell->y,
					cell->width, cell->height);
			}
		}
		edge->y = y;
		edge->fe_edge->y = y;
	}
	/*
	 * Else for vertical edges.
	 */
	else
	{
		int32 diff;

		diff = x - edge->x;

		for (i=0; i < edge->cell_cnt; i++)
		{
			lo_GridCellRec *cell;

			cell = edge->cell_array[i];
			if (cell == NULL)
			{
				continue;
			}
			/*
			 * If the cell is left of the edge.
			 * Grow the cell and any abutting
			 * edges.
			 */
			if (cell->x < edge->x)
			{
				cell->width += diff;
				lo_change_left_edges(context,
					cell->side_edges[TOP_EDGE],
					edge->x, diff, TRUE);
				lo_change_left_edges(context,
					cell->side_edges[BOTTOM_EDGE],
					edge->x, diff, TRUE);
				/*
				 * The bounds of this edge are effected
				 */
				lo_set_edge_bounds(cell->side_edges[LEFT_EDGE]);
			}
			/*
			 * Else the cell is right of the edge.
			 * Move and shrink the cell and any abutting
			 * edges.
			 */
			else
			{
				cell->x += diff;
				cell->width -= diff;
				lo_change_right_edges(context,
					cell->side_edges[TOP_EDGE],
					(edge->x + edge->width - 1), diff, TRUE);
				lo_change_right_edges(context,
					cell->side_edges[BOTTOM_EDGE],
					(edge->x + edge->width - 1), diff, TRUE);
				/*
				 * The bounds of this edge are effected
				 */
				lo_set_edge_bounds(cell->side_edges[RIGHT_EDGE]);
			}
			/*
			 * Only restructure cells that have windows(contexts)
			 */
			if (cell->context != NULL)
			{
				FE_RestructureGridWindow (cell->context,
					cell->x, cell->y,
					cell->width, cell->height);
			}
		}
		edge->x = x;
		edge->fe_edge->x = x;
	}
	/*
	 * Display the edge that was just moved.
	 */
	lo_DisplayEdge(context, edge->fe_edge);
}

intn
lo_GetCurrentGridCellStatus(lo_GridCellRec *cell)
{
	int32 doc_id;
	lo_TopState *top_state;

	/*
	 * This is a blank cell, always considered complete.
	 */
	if ((cell == NULL)||(cell->context == NULL))
	{
		return(PA_COMPLETE);
	}

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(cell->context);
	top_state = lo_FetchTopState(doc_id);
	if (top_state == NULL)
	{
		return(-1);
	}
	return(top_state->layout_status);
}


char *
lo_GetCurrentGridCellUrl(lo_GridCellRec *cell)
{
	int32 doc_id;
	lo_TopState *top_state;
	char *url;

	if ((cell == NULL)||(cell->context == NULL))
	{
		return(NULL);
	}

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(cell->context);
	top_state = lo_FetchTopState(doc_id);
	if (top_state == NULL)
	{
		return(NULL);
	}

	if (top_state->url == NULL)
	{
		url = NULL;
	}
	else
	{
		url = XP_STRDUP(top_state->url);
	}
	return(url);
}


void
lo_GetGridCellMargins(MWContext *context,
	int32 *margin_width, int32 *margin_height)
{
	int32 doc_id;
	lo_TopState *top_state;
	MWContext *parent;
	lo_GridCellRec *cell_ptr;

	if ((context == NULL)||(context->grid_parent == NULL))
	{
		return;
	}

	parent = context->grid_parent;
	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(parent);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->the_grid == NULL))
	{
		return;
	}

	/*
	 * Find the cell whose context matches the passed in context.
	 */
	cell_ptr = top_state->the_grid->cell_list;
	while (cell_ptr != NULL)
	{
		if (cell_ptr->context == context)
		{
			break;
		}
		cell_ptr = cell_ptr->next;
	}

	/*
	 * If we found the cell, set our margins.
	 */
	if (cell_ptr != NULL)
	{
		*margin_width = cell_ptr->margin_width;
		*margin_height = cell_ptr->margin_height;
	}
	/*
	 * If we didn't find the cell, it is
	 * in the middle of creation, get the
	 * margins from the parent grid.
	 */
	else
	{
		*margin_width = top_state->the_grid->current_cell_margin_width;
		*margin_height = top_state->the_grid->current_cell_margin_height;
	}
}


static void
lo_reset_all_edge_bounds(MWContext *context, lo_DocState *state,
				lo_GridRec *grid)
{
	lo_GridEdge *edge_list;

	if ((grid == NULL)||(grid->edge_list == NULL))
	{
		return;
	}

	edge_list = grid->edge_list;
	while (edge_list != NULL)
	{
		LO_EdgeStruct *fe_edge;

		fe_edge = (LO_EdgeStruct *)lo_NewElement(context, state,
				LO_EDGE, NULL, 0);
		if (fe_edge == NULL)
		{
		}

		fe_edge->type = LO_EDGE;
		fe_edge->ele_id = NEXT_ELEMENT;
		fe_edge->x = edge_list->x;
		fe_edge->x_offset = 0;
		fe_edge->y = edge_list->y;
		fe_edge->y_offset = 0;
		fe_edge->width = edge_list->width;
		fe_edge->height = edge_list->height;
		fe_edge->line_height = 0;
		fe_edge->next = NULL;
		fe_edge->prev = NULL;

		fe_edge->FE_Data = NULL;
		fe_edge->lo_edge_info = (void *)edge_list;
		fe_edge->is_vertical = edge_list->is_vertical;
		fe_edge->movable = TRUE;
		fe_edge->left_top_bound = 0;
		fe_edge->right_bottom_bound = 0;

		edge_list->fe_edge = fe_edge;

		lo_set_edge_bounds(edge_list);

		/*
		 * Insert this edge into the float list.
		 */
		edge_list->fe_edge->next = state->float_list;
		state->float_list = (LO_Element *)edge_list->fe_edge;

		lo_DisplayEdge(context, edge_list->fe_edge);
		edge_list = edge_list->next;
	}
}


void
lo_RecreateGrid(MWContext *context, lo_DocState *state, lo_GridRec *grid)
{
	lo_GridCellRec *cell_ptr;
	int32 width, height;

	width = state->win_width;
	height = state->win_height;
	FE_GetFullWindowSize(context, &width, &height);

	if (width <= 0)
		width = 1;

	if (height <= 0)
		height = 1;

/* already set.
	grid->main_width = width;
	grid->main_height = height;
*/

	lo_reset_all_edge_bounds(context, state, grid);

	state->top_state->the_grid = grid;

	/*
	 * Go through all the cells creating them where specified.
	 */
	cell_ptr = grid->cell_list;
	while (cell_ptr != NULL)
	{
		/*
		 * Have the front end create the grid cell.
		 */
		if (cell_ptr->url != NULL)
		{
			void *history;
			void *hist_list;

			history = NULL;
			hist_list = NULL;
			if (cell_ptr->hist_list != NULL)
			{
				hist_list = cell_ptr->hist_list;
				cell_ptr->hist_indx = grid->current_hist;
				history = cell_ptr->hist_array[cell_ptr->hist_indx - 1].hist;
				if (history == NULL)
				{
					hist_list = NULL;
				}
			}
			grid->current_cell_margin_width = cell_ptr->margin_width;
			grid->current_cell_margin_height = cell_ptr->margin_height;
			reconnecting_cell = cell_ptr;
			cell_ptr->context = FE_MakeGridWindow (context,
				hist_list,
				history,
				cell_ptr->x, cell_ptr->y,
				cell_ptr->width, cell_ptr->height,
				(char *)cell_ptr->url, (char *)cell_ptr->name,
				cell_ptr->scrolling,
				FORCE_RELOAD_FLAG(state->top_state),
				cell_ptr->no_edges);
			reconnecting_cell = NULL;
			grid->current_cell_margin_width = 0;
			grid->current_cell_margin_height = 0;
			if (cell_ptr->context)
			{
			    XP_ASSERT(cell_ptr->context->forceJSEnabled ==
				context->forceJSEnabled);
			    if (history)
			    {
				cell_ptr->context->hist.cur_doc_ptr = history;
			    }
			}
		}
		else
		{
			cell_ptr->context = NULL;
		}
		cell_ptr = cell_ptr->next;
	}
}



lo_GridCellRec *
lo_ContextToCell(MWContext *context, Bool reconnect, lo_GridRec **grid_ptr)
{
	int32 doc_id;
	lo_TopState *top_state;
	MWContext *parent;
	lo_GridCellRec *cell_ptr;

	*grid_ptr = NULL;
	if ((context == NULL)||(context->grid_parent == NULL))
	{
		return((lo_GridCellRec *)NULL);
	}

	parent = context->grid_parent;
	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(parent);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->the_grid == NULL))
	{
		return((lo_GridCellRec *)NULL);
	}

	*grid_ptr = top_state->the_grid;

	/*
	 * Find the cell whose context matches the passed in context.
	 */
	cell_ptr = top_state->the_grid->cell_list;
	while (cell_ptr != NULL)
	{
		if (cell_ptr->context == context)
		{
			return(cell_ptr);
		}
		cell_ptr = cell_ptr->next;
	}

	if (reconnect && (cell_ptr = reconnecting_cell) != NULL)
	{
		cell_ptr->context = context;
		if (context->grid_parent)
		{
			context->forceJSEnabled =
				context->grid_parent->forceJSEnabled;
		}

		return(cell_ptr);
	}
	return(NULL);
}


static void
lo_set_hist(MWContext *context, void **hep, History_entry *hist)
{
	History_entry *oldhist = *hep;

	if (oldhist == hist)
		return;
	*hep = SHIST_HoldEntry(hist);
	SHIST_DropEntry(context, oldhist);
}


PRIVATE
void
lo_UpdateOtherCells(lo_GridRec *grid, lo_GridCellRec *the_cell)
{
	lo_GridCellRec *cell_ptr;

	cell_ptr = grid->cell_list;
	while (cell_ptr != NULL)
	{
		if ((cell_ptr != the_cell)&&
			(cell_ptr->hist_indx < grid->current_hist))
		{
		    int32 i;
		    int32 old;

		    /*
		     * Make sure se don't go backwards off the beginning
		     * of the array.
		     */
		    old = cell_ptr->hist_indx - 1;
		    if (old < 0)
		    {
			old = 0;
		    }

		    for (i = cell_ptr->hist_indx; i < grid->current_hist; i++)
		    {
			lo_set_hist(cell_ptr->context,
				    &cell_ptr->hist_array[i].hist,
				    cell_ptr->hist_array[old].hist);
			cell_ptr->hist_array[i].position =
			    cell_ptr->hist_array[old].position;
		    }

		    /*
		     * If we got here and hist_indx is zero, that means
		     * we are trying to update this cell, before it ever
		     * had its initializing load.  In that case, we
		     * want to leave hist_indx zero so we will be able to
		     * detect that initializing load when it occurs.
		     */
		    if (cell_ptr->hist_indx > 0)
		    {
			cell_ptr->hist_indx = grid->current_hist;
		    }
		}
		cell_ptr = cell_ptr->next;
	}
}


static void
lo_grow_grid_hist_arrays(lo_GridRec *grid)
{
	int32 new_size;
	lo_GridCellRec *cell_ptr;

	if (grid == NULL)
	{
		return;
	}

	new_size = grid->hist_size + 10;
	cell_ptr = grid->cell_list;
	while (cell_ptr != NULL)
	{
		lo_GridHistory *new_array;

		new_array = (lo_GridHistory *)XP_REALLOC(cell_ptr->hist_array,
				(new_size * sizeof(lo_GridHistory)));
		if (new_array == NULL)
		{
			/* XXX should set top_state->out_of_memory somehow */
			return;
		}
		XP_MEMSET(new_array + grid->hist_size, 0,
			  (10 * sizeof(lo_GridHistory)));
		cell_ptr->hist_array = new_array;
		cell_ptr = cell_ptr->next;
	}
	grid->hist_size  = new_size;
}


void
LO_UpdateGridHistory(MWContext *context)
{
	lo_GridRec *grid;
	lo_GridCellRec *cell_ptr;
	History_entry *hist;

	cell_ptr = lo_ContextToCell(context, TRUE, &grid);
	if (cell_ptr == NULL)
	{
		return;
	}

	hist = SHIST_GetCurrent(&context->hist);
	if (hist == NULL)
	{
		return;
	}

	if (cell_ptr->hist_indx >= grid->current_hist)
	{
		if (grid->current_hist >= grid->hist_size)
		{
			lo_grow_grid_hist_arrays(grid);
		}
		grid->current_hist = cell_ptr->hist_indx + 1;
		if (grid->current_hist > grid->max_hist)
		{
			grid->max_hist = grid->current_hist;
		}
		else if (grid->current_hist < grid->max_hist)
		{
			grid->max_hist = grid->current_hist;
		}
		if (grid->current_hist > 1)
		{
			lo_UpdateOtherCells(grid, cell_ptr);
			if ((context->grid_parent != NULL)&&
				(context->grid_parent->is_grid_cell != FALSE))
			{
				LO_UpdateGridHistory(context->grid_parent);
			}
		}
	}
	lo_set_hist(context,
		    &cell_ptr->hist_array[cell_ptr->hist_indx].hist,
		    hist);
	if (cell_ptr->hist_indx == 0)
	{
		cell_ptr->hist_array[cell_ptr->hist_indx].position = 1;
		/*
		 * This means some other grid cell already progressed before
		 * we got our first load, now that we are loaded, catch up.
		 */
		if (grid->current_hist > 1)
		{
			int32 i;

			for (i = 1; i < grid->current_hist; i++)
			{
				lo_set_hist(context,
					    &cell_ptr->hist_array[i].hist,
					    cell_ptr->hist_array[0].hist);
				cell_ptr->hist_array[i].position =
					cell_ptr->hist_array[0].position;
			}
			cell_ptr->hist_indx = grid->current_hist - 1;
		}
	}
	else
	{
		/*
		 * Drop held MochaDecoder if any, we needed it only during
		 * resize-reload and don't now that we've moved forward in
		 * this frame context's history.
		 */
		hist = cell_ptr->hist_array[cell_ptr->hist_indx - 1].hist;
		if (hist && hist->savedData.Window)
		{
			LM_DropSavedWindow(context, hist->savedData.Window);
			hist->savedData.Window = NULL;
		}

		cell_ptr->hist_array[cell_ptr->hist_indx].position =
		    cell_ptr->hist_array[cell_ptr->hist_indx - 1].position + 1;
	}
	cell_ptr->hist_indx++;
}


PRIVATE
void
lo_BackupGridHistory(lo_GridRec *grid)
{
	lo_GridCellRec *cell_ptr;

	cell_ptr = grid->cell_list;
	while (cell_ptr != NULL)
	{
		if (cell_ptr->hist_indx > 1)
		{
			int32 indx;

			cell_ptr->hist_indx--;
			indx = cell_ptr->hist_indx - 1;

			if (cell_ptr->hist_array[indx].hist !=
				cell_ptr->hist_array[cell_ptr->hist_indx].hist)
			{
				lo_reload_cell_from_history(cell_ptr,
						NET_DONT_RELOAD);
			}
			else if ((cell_ptr->hist_array[indx].position !=
			   cell_ptr->hist_array[cell_ptr->hist_indx].position)&&
			   (cell_ptr->context != NULL)&&
			   (cell_ptr->context->grid_children != NULL))
			{
				(void)LO_BackInGrid(cell_ptr->context);
			}
		}
		cell_ptr = cell_ptr->next;
	}
}


PRIVATE
void
lo_ForwardGridHistory(lo_GridRec *grid)
{
	lo_GridCellRec *cell_ptr;

	cell_ptr = grid->cell_list;
	while (cell_ptr != NULL)
	{
		if (cell_ptr->hist_indx < grid->max_hist)
		{
			int32 indx;

			cell_ptr->hist_indx++;
			indx = cell_ptr->hist_indx - 1;

			if (cell_ptr->hist_array[indx].hist !=
				cell_ptr->hist_array[indx - 1].hist)
			{
				lo_reload_cell_from_history(cell_ptr,
						NET_DONT_RELOAD);
			}
			else if ((cell_ptr->hist_array[indx].position !=
			   cell_ptr->hist_array[indx - 1].position)&&
			   (cell_ptr->context != NULL)&&
			   (cell_ptr->context->grid_children != NULL))
			{
				(void)LO_ForwardInGrid(cell_ptr->context);
			}
		}
		cell_ptr = cell_ptr->next;
	}
}


Bool
LO_BackInGrid(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_GridRec *grid;

	if ((context == NULL)||(context->grid_children == NULL))
	{
		return(FALSE);
	}

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->the_grid == NULL))
	{
		return(FALSE);
	}

	grid = top_state->the_grid;

	if (grid->current_hist <= 1)
	{
		return(FALSE);
	}

	lo_BackupGridHistory(grid);
	grid->current_hist--;
	return(TRUE);
}


Bool
LO_ForwardInGrid(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_GridRec *grid;

	if ((context == NULL)||(context->grid_children == NULL))
	{
		return(FALSE);
	}

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->the_grid == NULL))
	{
		return(FALSE);
	}

	grid = top_state->the_grid;

	if (grid->current_hist >= grid->max_hist)
	{
		return(FALSE);
	}

	lo_ForwardGridHistory(grid);
	grid->current_hist++;
	return(TRUE);
}


Bool
LO_GridCanGoForward(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_GridRec *grid;

	if ((context == NULL)||(context->grid_children == NULL))
	{
		return(FALSE);
	}

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->the_grid == NULL))
	{
		return(FALSE);
	}

	grid = top_state->the_grid;

	if (grid->current_hist < grid->max_hist)
	{
		return(TRUE);
	}
	return(FALSE);
}


Bool
LO_GridCanGoBackward(MWContext *context)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_GridRec *grid;

	if ((context == NULL)||(context->grid_children == NULL))
	{
		return(FALSE);
	}

	/*
	 * Get the unique document ID, and retreive this
	 * documents layout state.
	 */
	doc_id = XP_DOCID(context);
	top_state = lo_FetchTopState(doc_id);
	if ((top_state == NULL)||(top_state->the_grid == NULL))
	{
		return(FALSE);
	}

	grid = top_state->the_grid;

	if (grid->current_hist > 1)
	{
		return(TRUE);
	}
	return(FALSE);
}


static void
lo_cleanup_grid_history(MWContext *context, lo_GridRec *grid)
{
	lo_GridCellRec *cell_ptr;

	if (grid == NULL)
	{
		return;
	}

	/*
	 * Set the current history to be the new maximum.
	 */
	grid->max_hist = grid->current_hist;

	/*
	 * Go through all the cells, truncating their history lists
	 */
	cell_ptr = grid->cell_list;
	while (cell_ptr != NULL)
	{
		int32 indx, count;
		XP_List *list_ptr;
		History_entry *cell_hist;

		list_ptr = (XP_List *)cell_ptr->hist_list;
		cell_hist = cell_ptr->hist_array[grid->current_hist - 1].hist;
		if ((list_ptr != NULL)&&(cell_hist != NULL))
		{
		    indx = XP_ListGetNumFromObject(list_ptr, cell_hist);
		    for (count = XP_ListCount(list_ptr); count > indx; count--)
		    {
			History_entry *old_entry;

			old_entry =
			    (History_entry *)XP_ListRemoveEndObject(list_ptr);
			SHIST_DropEntry(context, old_entry);
		    }
		}

		/*
		 * If the last history entry in the cells chain
		 * is itself a grid, it may need to be cleaned
		 * up recursively.
		 */
		if (cell_hist != NULL)
		{
		    lo_SavedGridData *saved_grid;

		    saved_grid =(lo_SavedGridData *)(cell_hist->savedData.Grid);
		    if ((saved_grid != NULL)&&(saved_grid->the_grid != NULL))
		    {
			lo_cleanup_grid_history(context, saved_grid->the_grid);
		    }
		}

		cell_ptr = cell_ptr->next;
	}
}


/*
 * When we have just added a session history after
 * this one, clean up the grid's history chains if
 * it is the parent we just left.
 */
void
LO_CleanupGridHistory(MWContext *context)
{
	History_entry *hist;
	lo_SavedGridData *saved_grid;
	lo_GridRec *grid;

	/*
	 * Look back one, if the is no back, return.
	 */
	hist = SHIST_GetPrevious(context);
	if (hist == NULL)
	{
		return;
	}

	/*
	 * Is the document back one in history a valid grid,
	 * if not, return.
	 */
	saved_grid = (lo_SavedGridData *)(hist->savedData.Grid);
	if ((saved_grid == NULL)||(saved_grid->the_grid == NULL))
	{
		return;
	}

	/*
	 * Since we are appending after this history stage,
	 * the current history chains must be
	 * clipped so this is the new max.
	 */
	grid = saved_grid->the_grid;
	lo_cleanup_grid_history(context, grid);
}

static void
lo_MoveGridEdgeForRelayout(MWContext *context, LO_EdgeStruct *fe_edge, int32 x, int32 y)
{
	int32 doc_id;
	lo_TopState *top_state;
	lo_DocState *state;
	int32 i;
	lo_GridEdge *edge;

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

	edge = (lo_GridEdge *)fe_edge->lo_edge_info;
	if (edge == NULL)
	{
		return;
	}

	/*
	 * For horizontal edges
	 */
	if (edge->is_vertical == FALSE)
	{
		int32 diff;

		diff = y - edge->y;

		for (i=0; i < edge->cell_cnt; i++)
		{
			lo_GridCellRec *cell;

			cell = edge->cell_array[i];
			if (cell == NULL)
			{
				continue;
			}
			/*
			 * If the cell is above the edge.
			 * Grow the cell and any abutting
			 * edges.
			 */
			if (cell->y < edge->y)
			{
#if 0
				cell->height += diff;
#endif
				lo_change_top_edges(context,
					cell->side_edges[LEFT_EDGE],
					edge->y, diff, FALSE);
				lo_change_top_edges(context,
					cell->side_edges[RIGHT_EDGE],
					edge->y, diff, FALSE);
				/*
				 * The bounds of this edge are effected
				 */
				lo_set_edge_bounds(cell->side_edges[TOP_EDGE]);
			}
			/*
			 * Else the cell is below the edge.
			 * Move and shrink the cell and any abutting
			 * edges.
			 */
			else
			{
#if 0
				cell->y += diff;
				cell->height -= diff;
#endif
				lo_change_bottom_edges(context,
					cell->side_edges[LEFT_EDGE],
					(edge->y + edge->height - 1), diff, FALSE);
				lo_change_bottom_edges(context,
					cell->side_edges[RIGHT_EDGE],
					(edge->y + edge->height - 1), diff, FALSE);
				/*
				 * The bounds of this edge are effected
				 */
				lo_set_edge_bounds(cell->side_edges[BOTTOM_EDGE]);
			}
			/*
			 * Only restructure cells that have windows(contexts)
			 */
			if (cell->context != NULL)
			{
			  cell->needs_restructuring = TRUE;
			}
		}
		edge->y = y;
		edge->fe_edge->y = y;
	}
	/*
	 * Else for vertical edges.
	 */
	else
	{
		int32 diff;

		diff = x - edge->x;

		for (i=0; i < edge->cell_cnt; i++)
		{
			lo_GridCellRec *cell;

			cell = edge->cell_array[i];
			if (cell == NULL)
			{
				continue;
			}
			/*
			 * If the cell is left of the edge.
			 * Grow the cell and any abutting
			 * edges.
			 */
			if (cell->x < edge->x)
			{
#if 0
				cell->width += diff;
#endif
				lo_change_left_edges(context,
					cell->side_edges[TOP_EDGE],
					edge->x, diff, FALSE);
				lo_change_left_edges(context,
					cell->side_edges[BOTTOM_EDGE],
					edge->x, diff, FALSE);
				/*
				 * The bounds of this edge are effected
				 */
				lo_set_edge_bounds(cell->side_edges[LEFT_EDGE]);
			}
			/*
			 * Else the cell is right of the edge.
			 * Move and shrink the cell and any abutting
			 * edges.
			 */
			else
			{
#if 0
				cell->x += diff;
				cell->width -= diff;
#endif
				lo_change_right_edges(context,
					cell->side_edges[TOP_EDGE],
					(edge->x + edge->width - 1), diff, FALSE);
				lo_change_right_edges(context,
					cell->side_edges[BOTTOM_EDGE],
					(edge->x + edge->width - 1), diff, FALSE);
				/*
				 * The bounds of this edge are effected
				 */
				lo_set_edge_bounds(cell->side_edges[RIGHT_EDGE]);
			}
			/*
			 * Only restructure cells that have windows(contexts)
			 */
			if (cell->context != NULL)
			{
			  cell->needs_restructuring = TRUE;
			}
		}
		edge->x = x;
		edge->fe_edge->x = x;
	}
}

PRIVATE
void
lo_ReLayoutGridCells(MWContext *context,
		     int32 x, int32 y, int32 width, int32 height,
		     lo_GridRec *grid)
{
	int32 cols, rows;
	int32 col_cnt, row_cnt;
	int32 orig_x, orig_y;
	lo_GridCellRec *cell_list;
	int32 new_cell_width, new_cell_height;

	if (grid == NULL)
	{
		return;
	}

	/*
	 * Make sure all the row percentages total up
	 * to 100%
	 */
	lo_adjust_percents(grid->rows, grid->row_percents, height);
	
	/*
	 * Make sure all the col percentages total up
	 * to 100%
	 */
	lo_adjust_percents(grid->cols, grid->col_percents, width);

	cols = grid->cols;
	rows = grid->rows;
	cell_list = grid->cell_list;

	orig_x = x;
	orig_y = y;
	col_cnt = 0;
	row_cnt = 0;
	while (cell_list != NULL)
	{
		cell_list->width_percent =
			(intn)grid->col_percents[col_cnt].value;
		cell_list->height_percent =
			(intn)grid->row_percents[row_cnt].value;

		cell_list->x = x;
		if (col_cnt != 0)
		{
			cell_list->x += grid->grid_cell_border;
		}
		cell_list->y = y;
		if (row_cnt != 0)
		{
			cell_list->y += grid->grid_cell_border;
		}

		new_cell_width = width * cell_list->width_percent / 100;
		if (new_cell_width < grid->grid_cell_min_dim)
		{
			new_cell_width = grid->grid_cell_min_dim;
		}
		if (col_cnt > 0)
		{
			new_cell_width -= grid->grid_cell_border;
		}
		if (col_cnt < (cols - 1))
		{
			new_cell_width -= grid->grid_cell_border;
		}

		/*
		 * Make sure the last column uses all the remaining space.
		 * We subtract orig_x to get cell_list->x into the same coord
		 * space as width.
		 */
		if (col_cnt == (cols - 1))
		{
			new_cell_width = width - (cell_list->x - orig_x);
		}

		new_cell_height = height * cell_list->height_percent / 100;
		if (new_cell_height < grid->grid_cell_min_dim)
		{
			new_cell_height = grid->grid_cell_min_dim;
		}
		if (row_cnt > 0)
		{
			new_cell_height -= grid->grid_cell_border;
		}
		if (row_cnt < (rows - 1))
		{
			new_cell_height -= grid->grid_cell_border;
		}

		/*
		 * Make sure the last row uses all the remaining space.
		 * We subtract orig_y to get cell_list->y into the same coord
		 * space as height.
		 */
		if (row_cnt == (rows - 1))
		{
			new_cell_height = height - (cell_list->y - orig_y);
		}

		cell_list->width = new_cell_width;
		cell_list->height = new_cell_height;

		if (cell_list->side_edges[RIGHT_EDGE])
		  {
		    if (cell_list->side_edges[RIGHT_EDGE]->dealt_with_in_relayout == FALSE)
		      {
			lo_MoveGridEdgeForRelayout(context,
						   cell_list->side_edges[RIGHT_EDGE]->fe_edge,
						   cell_list->x + cell_list->width,
						   cell_list->side_edges[RIGHT_EDGE]->y);
			cell_list->side_edges[RIGHT_EDGE]->dealt_with_in_relayout = TRUE;
		      }
		  }

		if (cell_list->side_edges[BOTTOM_EDGE])
		  {
		    if (cell_list->side_edges[BOTTOM_EDGE]->dealt_with_in_relayout == FALSE)
		      {
			lo_MoveGridEdgeForRelayout(context,
						   cell_list->side_edges[BOTTOM_EDGE]->fe_edge,
						   cell_list->side_edges[BOTTOM_EDGE]->x,
						   cell_list->y + cell_list->height);
			cell_list->side_edges[BOTTOM_EDGE]->dealt_with_in_relayout = TRUE;
		      }
		  }

		x += new_cell_width;
		if (col_cnt > 0)
		{
			x += grid->grid_cell_border;
		}
		if (col_cnt < (cols - 1))
		{
			x += grid->grid_cell_border;
		}
		col_cnt++;
		if (col_cnt >= cols)
		{
			x = orig_x;
			y += new_cell_height;
			if (row_cnt > 0)
			{
				y += grid->grid_cell_border;
			}
			if (row_cnt < (rows - 1))
			{
				y += grid->grid_cell_border;
			}
			col_cnt = 0;
			row_cnt++;
		}

		cell_list = cell_list->next;
	}
}

static void
lo_PrepareGridForRelayout(MWContext *context,
			  lo_GridRec *grid)
{
  lo_GridEdge *edge_list;
  lo_GridCellRec *cell_list;

  cell_list = grid->cell_list;
  while(cell_list)
    {
      cell_list->needs_restructuring = FALSE;
      cell_list = cell_list->next;
    }

  edge_list = grid->edge_list;
  while (edge_list != NULL)
    {
      /*      FE_HideEdge(context, FE_VIEW, edge_list->fe_edge);*/
      edge_list->dealt_with_in_relayout = FALSE;
      edge_list = edge_list->next;
    }

}

static void
lo_ResyncEdgeSizes(MWContext *context,
		   lo_GridRec *grid)
{
  lo_GridEdge *edge_list;

  edge_list = grid->edge_list;
  while (edge_list != NULL)
    {
      int i;

      lo_set_edge_bounds(edge_list);

      if (edge_list->is_vertical == FALSE) /* horizontal edge */
	{
	  int32 width = 0;
	  for (i = 0; i < edge_list->cell_cnt; i ++)
	    {
	      lo_GridCellRec *cell;
	      
	      cell = edge_list->cell_array[i];
	      if (cell == NULL)
		{
		  continue;
		}

	      if (cell->x + cell->width > edge_list->x + width)
		width = cell->x + cell->width - edge_list->x;
	    }

	  edge_list->width = width;
	  edge_list->fe_edge->width = width;
	}
      else /* vertical edge */
	{
	  int32 height = 0;
	  for (i = 0; i < edge_list->cell_cnt; i ++)
	    {
	      lo_GridCellRec *cell;
	      
	      cell = edge_list->cell_array[i];
	      if (cell == NULL)
		{
		  continue;
		}

	      if (cell->y + cell->height > edge_list->y + height)
		height = cell->y + cell->height - edge_list->y;
	    }

	  edge_list->height = height;
	  edge_list->fe_edge->height = height;
	}

      lo_DisplayEdge(context, edge_list->fe_edge);

      edge_list = edge_list->next;
    }
}

static void
lo_RestructureCells(MWContext *context,
		    lo_GridRec *grid)
{
  lo_GridCellRec *cell_list;
  cell_list = grid->cell_list;
  while(cell_list)
    {
      if (cell_list->needs_restructuring &&
	  cell_list->context != NULL)
	{
	  FE_RestructureGridWindow (cell_list->context,
				    cell_list->x, cell_list->y,
				    cell_list->width, cell_list->height);
	}

      cell_list = cell_list->next;
    }
}

void
lo_RelayoutGridDocumentOnResize(MWContext *context,
				lo_TopState *top_state,
				int32 width, int32 height)
{
  lo_GridRec *grid = top_state->the_grid;

  lo_PrepareGridForRelayout(context, grid);

  lo_ReLayoutGridCells(context, 0, 0, width, height, grid);

  lo_RestructureCells(context, grid);

  lo_ResyncEdgeSizes(context, grid);
}

