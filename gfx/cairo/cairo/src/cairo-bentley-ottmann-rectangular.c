/*
 * Copyright © 2004 Carl Worth
 * Copyright © 2006 Red Hat, Inc.
 * Copyright © 2009 Chris Wilson
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Carl Worth
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

/* Provide definitions for standalone compilation */
#include "cairoint.h"

#include "cairo-combsort-private.h"
#include "cairo-list-private.h"

typedef struct _cairo_bo_rectangle cairo_bo_rectangle_t;
typedef struct _cairo_bo_edge cairo_bo_edge_t;

/* A deferred trapezoid of an edge */
typedef struct _cairo_bo_trap {
    cairo_bo_edge_t *right;
    int32_t top;
} cairo_bo_trap_t;

struct _cairo_bo_edge {
    int x;
    int dir;
    cairo_bo_trap_t deferred_trap;
    cairo_list_t link;
};

struct _cairo_bo_rectangle {
    cairo_bo_edge_t left, right;
    int top, bottom;
};

/* the parent is always given by index/2 */
#define PQ_PARENT_INDEX(i) ((i) >> 1)
#define PQ_FIRST_ENTRY 1

/* left and right children are index * 2 and (index * 2) +1 respectively */
#define PQ_LEFT_CHILD_INDEX(i) ((i) << 1)

typedef struct _pqueue {
    int size, max_size;

    cairo_bo_rectangle_t **elements;
    cairo_bo_rectangle_t *elements_embedded[1024];
} pqueue_t;

typedef struct _cairo_bo_sweep_line {
    cairo_bo_rectangle_t **rectangles;
    pqueue_t stop;
    cairo_list_t sweep;
    cairo_list_t *current_left, *current_right;
    int32_t current_y;
    int32_t last_y;
} cairo_bo_sweep_line_t;

#define DEBUG_TRAPS 0

#if DEBUG_TRAPS
static void
dump_traps (cairo_traps_t *traps, const char *filename)
{
    FILE *file;
    int n;

    if (getenv ("CAIRO_DEBUG_TRAPS") == NULL)
	return;

    file = fopen (filename, "a");
    if (file != NULL) {
	for (n = 0; n < traps->num_traps; n++) {
	    fprintf (file, "%d %d L:(%d, %d), (%d, %d) R:(%d, %d), (%d, %d)\n",
		     traps->traps[n].top,
		     traps->traps[n].bottom,
		     traps->traps[n].left.p1.x,
		     traps->traps[n].left.p1.y,
		     traps->traps[n].left.p2.x,
		     traps->traps[n].left.p2.y,
		     traps->traps[n].right.p1.x,
		     traps->traps[n].right.p1.y,
		     traps->traps[n].right.p2.x,
		     traps->traps[n].right.p2.y);
	}
	fprintf (file, "\n");
	fclose (file);
    }
}
#else
#define dump_traps(traps, filename)
#endif

static inline int
cairo_bo_rectangle_compare_start (const cairo_bo_rectangle_t *a,
				  const cairo_bo_rectangle_t *b)
{
    return a->top - b->top;
}

static inline int
_cairo_bo_rectangle_compare_stop (const cairo_bo_rectangle_t *a,
				  const cairo_bo_rectangle_t *b)
{
    return a->bottom - b->bottom;
}

static inline void
_pqueue_init (pqueue_t *pq)
{
    pq->max_size = ARRAY_LENGTH (pq->elements_embedded);
    pq->size = 0;

    pq->elements = pq->elements_embedded;
    pq->elements[PQ_FIRST_ENTRY] = NULL;
}

static inline void
_pqueue_fini (pqueue_t *pq)
{
    if (pq->elements != pq->elements_embedded)
	free (pq->elements);
}

static cairo_status_t
_pqueue_grow (pqueue_t *pq)
{
    cairo_bo_rectangle_t **new_elements;
    pq->max_size *= 2;

    if (pq->elements == pq->elements_embedded) {
	new_elements = _cairo_malloc_ab (pq->max_size,
					 sizeof (cairo_bo_rectangle_t *));
	if (unlikely (new_elements == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	memcpy (new_elements, pq->elements_embedded,
		sizeof (pq->elements_embedded));
    } else {
	new_elements = _cairo_realloc_ab (pq->elements,
					  pq->max_size,
					  sizeof (cairo_bo_rectangle_t *));
	if (unlikely (new_elements == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    pq->elements = new_elements;
    return CAIRO_STATUS_SUCCESS;
}

static inline cairo_status_t
_pqueue_push (pqueue_t *pq, cairo_bo_rectangle_t *rectangle)
{
    cairo_bo_rectangle_t **elements;
    int i, parent;

    if (unlikely (pq->size + 1 == pq->max_size)) {
	cairo_status_t status;

	status = _pqueue_grow (pq);
	if (unlikely (status))
	    return status;
    }

    elements = pq->elements;

    for (i = ++pq->size;
	 i != PQ_FIRST_ENTRY &&
	 _cairo_bo_rectangle_compare_stop (rectangle,
					   elements[parent = PQ_PARENT_INDEX (i)]) < 0;
	 i = parent)
    {
	elements[i] = elements[parent];
    }

    elements[i] = rectangle;

    return CAIRO_STATUS_SUCCESS;
}

static inline void
_pqueue_pop (pqueue_t *pq)
{
    cairo_bo_rectangle_t **elements = pq->elements;
    cairo_bo_rectangle_t *tail;
    int child, i;

    tail = elements[pq->size--];
    if (pq->size == 0) {
	elements[PQ_FIRST_ENTRY] = NULL;
	return;
    }

    for (i = PQ_FIRST_ENTRY;
	 (child = PQ_LEFT_CHILD_INDEX (i)) <= pq->size;
	 i = child)
    {
	if (child != pq->size &&
	    _cairo_bo_rectangle_compare_stop (elements[child+1],
					      elements[child]) < 0)
	{
	    child++;
	}

	if (_cairo_bo_rectangle_compare_stop (elements[child], tail) >= 0)
	    break;

	elements[i] = elements[child];
    }
    elements[i] = tail;
}

static inline cairo_bo_rectangle_t *
_cairo_bo_rectangle_pop_start (cairo_bo_sweep_line_t *sweep_line)
{
    return *sweep_line->rectangles++;
}

static inline cairo_bo_rectangle_t *
_cairo_bo_rectangle_peek_stop (cairo_bo_sweep_line_t *sweep_line)
{
    return sweep_line->stop.elements[PQ_FIRST_ENTRY];
}

CAIRO_COMBSORT_DECLARE (_cairo_bo_rectangle_sort,
			cairo_bo_rectangle_t *,
			cairo_bo_rectangle_compare_start)

static void
_cairo_bo_sweep_line_init (cairo_bo_sweep_line_t *sweep_line,
			   cairo_bo_rectangle_t	**rectangles,
			   int			  num_rectangles)
{
    _cairo_bo_rectangle_sort (rectangles, num_rectangles);
    rectangles[num_rectangles] = NULL;
    sweep_line->rectangles = rectangles;

    cairo_list_init (&sweep_line->sweep);
    sweep_line->current_left = &sweep_line->sweep;
    sweep_line->current_right = &sweep_line->sweep;
    sweep_line->current_y = INT32_MIN;
    sweep_line->last_y = INT32_MIN;

    _pqueue_init (&sweep_line->stop);
}

static void
_cairo_bo_sweep_line_fini (cairo_bo_sweep_line_t *sweep_line)
{
    _pqueue_fini (&sweep_line->stop);
}

static inline cairo_bo_edge_t *
link_to_edge (cairo_list_t *elt)
{
    return cairo_container_of (elt, cairo_bo_edge_t, link);
}

static cairo_status_t
_cairo_bo_edge_end_trap (cairo_bo_edge_t	*left,
			 int32_t		 bot,
			 cairo_traps_t	        *traps)
{
    cairo_bo_trap_t *trap = &left->deferred_trap;

    /* Only emit (trivial) non-degenerate trapezoids with positive height. */
    if (likely (trap->top < bot)) {
	cairo_line_t _left = {
	    { left->x, trap->top },
	    { left->x, bot },
	}, _right = {
	    { trap->right->x, trap->top },
	    { trap->right->x, bot },
	};
	_cairo_traps_add_trap (traps, trap->top, bot, &_left, &_right);
    }

    trap->right = NULL;

    return _cairo_traps_status (traps);
}

/* Start a new trapezoid at the given top y coordinate, whose edges
 * are `edge' and `edge->next'. If `edge' already has a trapezoid,
 * then either add it to the traps in `traps', if the trapezoid's
 * right edge differs from `edge->next', or do nothing if the new
 * trapezoid would be a continuation of the existing one. */
static inline cairo_status_t
_cairo_bo_edge_start_or_continue_trap (cairo_bo_edge_t	    *left,
				       cairo_bo_edge_t	    *right,
				       int		     top,
				       cairo_traps_t	    *traps)
{
    cairo_status_t status;

    if (left->deferred_trap.right == right)
	return CAIRO_STATUS_SUCCESS;

    if (left->deferred_trap.right != NULL) {
	if (right != NULL && left->deferred_trap.right->x == right->x) {
	    /* continuation on right, so just swap edges */
	    left->deferred_trap.right = right;
	    return CAIRO_STATUS_SUCCESS;
	}

	status = _cairo_bo_edge_end_trap (left, top, traps);
	if (unlikely (status))
	    return status;
    }

    if (right != NULL && left->x != right->x) {
	left->deferred_trap.top = top;
	left->deferred_trap.right = right;
    }

    return CAIRO_STATUS_SUCCESS;
}

static inline cairo_status_t
_active_edges_to_traps (cairo_bo_sweep_line_t	*sweep,
			cairo_fill_rule_t	 fill_rule,
			cairo_traps_t	        *traps)
{
    int top = sweep->current_y;
    cairo_list_t *pos = &sweep->sweep;
    cairo_status_t status;

    if (sweep->last_y == sweep->current_y)
	return CAIRO_STATUS_SUCCESS;

    if (fill_rule == CAIRO_FILL_RULE_WINDING) {
	do {
	    cairo_bo_edge_t *left, *right;
	    int in_out;

	    pos = pos->next;
	    if (pos == &sweep->sweep)
		break;

	    left = link_to_edge (pos);
	    in_out = left->dir;

	    /* Check if there is a co-linear edge with an existing trap */
	    if (left->deferred_trap.right == NULL) {
		right = link_to_edge (pos->next);
		while (unlikely (right->x == left->x)) {
		    if (right->deferred_trap.right != NULL) {
			/* continuation on left */
			left->deferred_trap = right->deferred_trap;
			right->deferred_trap.right = NULL;
			break;
		    }

		    if (right->link.next == &sweep->sweep)
			break;
		    right = link_to_edge (right->link.next);
		}
	    }

	    /* Greedily search for the closing edge, so that we generate the
	     * maximal span width with the minimal number of trapezoids.
	     */

	    right = link_to_edge (left->link.next);
	    do {
		/* End all subsumed traps */
		if (right->deferred_trap.right != NULL) {
		    status = _cairo_bo_edge_end_trap (right, top, traps);
		    if (unlikely (status))
			return status;
		}

		in_out += right->dir;
		if (in_out == 0) {
		    /* skip co-linear edges */
		    if (likely (right->link.next == &sweep->sweep ||
				right->x != link_to_edge (right->link.next)->x))
		    {
			break;
		    }
		}

		right = link_to_edge (right->link.next);
	    } while (TRUE);

	    status = _cairo_bo_edge_start_or_continue_trap (left, right,
							    top, traps);
	    if (unlikely (status))
		return status;

	    pos = &right->link;
	} while (TRUE);
    } else {
	cairo_bo_edge_t *left, *right;
	do {
	    int in_out = 0;

	    pos = pos->next;
	    if (pos == &sweep->sweep)
		break;

	    left = link_to_edge (pos);

	    pos = pos->next;
	    do {
		right = link_to_edge (pos);
		if (right->deferred_trap.right != NULL) {
		    status = _cairo_bo_edge_end_trap (right, top, traps);
		    if (unlikely (status))
			return status;
		}

		if ((in_out++ & 1) == 0) {
		    cairo_list_t *next;
		    cairo_bool_t skip = FALSE;

		    /* skip co-linear edges */
		    next = pos->next;
		    if (next != &sweep->sweep)
			skip = right->x == link_to_edge (next)->x;

		    if (! skip)
			break;
		}
		pos = pos->next;
	    } while (TRUE);

	    right = pos == &sweep->sweep ? NULL : link_to_edge (pos);
	    status = _cairo_bo_edge_start_or_continue_trap (left, right,
							    top, traps);
	    if (unlikely (status))
		return status;
	} while (right != NULL);
    }

    sweep->last_y = sweep->current_y;
    return CAIRO_STATUS_SUCCESS;
}

static inline cairo_status_t
_cairo_bo_sweep_line_delete_edge (cairo_bo_sweep_line_t *sweep_line,
	                          cairo_bo_edge_t *edge,
				  cairo_traps_t *traps)
{
    if (edge->deferred_trap.right != NULL) {
	cairo_bo_edge_t *next = link_to_edge (edge->link.next);
	if (&next->link != &sweep_line->sweep && next->x == edge->x) {
	    next->deferred_trap = edge->deferred_trap;
	} else {
	    cairo_status_t status;

	    status = _cairo_bo_edge_end_trap (edge,
		                              sweep_line->current_y,
					      traps);
	    if (unlikely (status))
		return status;
	}
    }

    if (sweep_line->current_left == &edge->link)
	sweep_line->current_left = edge->link.prev;

    if (sweep_line->current_right == &edge->link)
	sweep_line->current_right = edge->link.next;

    cairo_list_del (&edge->link);

    return CAIRO_STATUS_SUCCESS;
}

static inline cairo_status_t
_cairo_bo_sweep_line_delete (cairo_bo_sweep_line_t	*sweep_line,
			     cairo_bo_rectangle_t	*rectangle,
			     cairo_fill_rule_t		 fill_rule,
			     cairo_traps_t		*traps)
{
    cairo_status_t status;

    if (rectangle->bottom != sweep_line->current_y) {
	status = _active_edges_to_traps (sweep_line, fill_rule, traps);
	if (unlikely (status))
	    return status;

	sweep_line->current_y = rectangle->bottom;
    }

    status = _cairo_bo_sweep_line_delete_edge (sweep_line,
	                                       &rectangle->left,
					       traps);
    if (unlikely (status))
	return status;

    status = _cairo_bo_sweep_line_delete_edge (sweep_line,
	                                       &rectangle->right,
					       traps);
    if (unlikely (status))
	return status;

    _pqueue_pop (&sweep_line->stop);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
validate_sweep_line (cairo_bo_sweep_line_t *sweep_line)
{
    int32_t last_x = INT32_MIN;
    cairo_bo_edge_t *edge;
    cairo_list_foreach_entry (edge, cairo_bo_edge_t, &sweep_line->sweep, link) {
	if (edge->x < last_x)
	    return FALSE;
	last_x = edge->x;
    }
    return TRUE;
}
static inline cairo_status_t
_cairo_bo_sweep_line_insert (cairo_bo_sweep_line_t	*sweep_line,
			     cairo_bo_rectangle_t	*rectangle,
			     cairo_fill_rule_t		 fill_rule,
			     cairo_traps_t		*traps)
{
    cairo_list_t *pos;
    cairo_status_t status;

    if (rectangle->top != sweep_line->current_y) {
	cairo_bo_rectangle_t *stop;

	stop = _cairo_bo_rectangle_peek_stop (sweep_line);
	while (stop != NULL && stop->bottom < rectangle->top) {
	    status = _cairo_bo_sweep_line_delete (sweep_line, stop,
						  fill_rule, traps);
	    if (unlikely (status))
		return status;

	    stop = _cairo_bo_rectangle_peek_stop (sweep_line);
	}

	status = _active_edges_to_traps (sweep_line, fill_rule, traps);
	if (unlikely (status))
	    return status;

	sweep_line->current_y = rectangle->top;
    }

    /* right edge */
    pos = sweep_line->current_right;
    if (pos == &sweep_line->sweep)
	pos = sweep_line->sweep.prev;
    if (pos != &sweep_line->sweep) {
	int cmp;

	cmp = link_to_edge (pos)->x - rectangle->right.x;
	if (cmp < 0) {
	    while (pos->next != &sweep_line->sweep &&
		   link_to_edge (pos->next)->x - rectangle->right.x < 0)
	    {
		pos = pos->next;
	    }
	} else if (cmp > 0) {
	    do {
		pos = pos->prev;
	    } while (pos != &sweep_line->sweep &&
		     link_to_edge (pos)->x - rectangle->right.x > 0);
	}

	cairo_list_add (&rectangle->right.link, pos);
    } else {
	cairo_list_add_tail (&rectangle->right.link, pos);
    }
    sweep_line->current_right = &rectangle->right.link;
    assert (validate_sweep_line (sweep_line));

    /* left edge */
    pos = sweep_line->current_left;
    if (pos == &sweep_line->sweep)
	pos = sweep_line->sweep.next;
    if (pos != &sweep_line->sweep) {
	int cmp;

	if (link_to_edge (pos)->x >= rectangle->right.x) {
	    pos = rectangle->right.link.prev;
	    if (pos == &sweep_line->sweep)
		goto left_done;
	}

	cmp = link_to_edge (pos)->x - rectangle->left.x;
	if (cmp < 0) {
	    while (pos->next != &sweep_line->sweep &&
		   link_to_edge (pos->next)->x - rectangle->left.x < 0)
	    {
		pos = pos->next;
	    }
	} else if (cmp > 0) {
	    do {
		pos = pos->prev;
	    } while (pos != &sweep_line->sweep &&
		    link_to_edge (pos)->x - rectangle->left.x > 0);
	}
    }
  left_done:
    cairo_list_add (&rectangle->left.link, pos);
    sweep_line->current_left = &rectangle->left.link;
    assert (validate_sweep_line (sweep_line));

    return _pqueue_push (&sweep_line->stop, rectangle);
}

static cairo_status_t
_cairo_bentley_ottmann_tessellate_rectangular (cairo_bo_rectangle_t	**rectangles,
					       int			  num_rectangles,
					       cairo_fill_rule_t	  fill_rule,
					       cairo_traps_t		 *traps)
{
    cairo_bo_sweep_line_t sweep_line;
    cairo_bo_rectangle_t *rectangle;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    _cairo_bo_sweep_line_init (&sweep_line, rectangles, num_rectangles);

    while ((rectangle = _cairo_bo_rectangle_pop_start (&sweep_line)) != NULL) {
	status = _cairo_bo_sweep_line_insert (&sweep_line, rectangle,
					      fill_rule, traps);
	if (unlikely (status))
	    goto BAIL;
    }

    while ((rectangle = _cairo_bo_rectangle_peek_stop (&sweep_line)) != NULL) {
	status = _cairo_bo_sweep_line_delete (&sweep_line, rectangle,
					      fill_rule, traps);
	if (unlikely (status))
	    goto BAIL;
    }

BAIL:
    _cairo_bo_sweep_line_fini (&sweep_line);
    return status;
}

cairo_status_t
_cairo_bentley_ottmann_tessellate_rectangular_traps (cairo_traps_t *traps,
						     cairo_fill_rule_t fill_rule)
{
    cairo_bo_rectangle_t stack_rectangles[CAIRO_STACK_ARRAY_LENGTH (cairo_bo_rectangle_t)];
    cairo_bo_rectangle_t *rectangles;
    cairo_bo_rectangle_t *stack_rectangles_ptrs[ARRAY_LENGTH (stack_rectangles) + 1];
    cairo_bo_rectangle_t **rectangles_ptrs;
    cairo_status_t status;
    int i;

    if (unlikely (traps->num_traps == 0))
	return CAIRO_STATUS_SUCCESS;

    assert (traps->is_rectangular);

    dump_traps (traps, "bo-rects-traps-in.txt");

    rectangles = stack_rectangles;
    rectangles_ptrs = stack_rectangles_ptrs;
    if (traps->num_traps > ARRAY_LENGTH (stack_rectangles)) {
	rectangles = _cairo_malloc_ab_plus_c (traps->num_traps,
					  sizeof (cairo_bo_rectangle_t) +
					  sizeof (cairo_bo_rectangle_t *),
					  sizeof (cairo_bo_rectangle_t *));
	if (unlikely (rectangles == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	rectangles_ptrs = (cairo_bo_rectangle_t **) (rectangles + traps->num_traps);
    }

    for (i = 0; i < traps->num_traps; i++) {
	if (traps->traps[i].left.p1.x < traps->traps[i].right.p1.x) {
	    rectangles[i].left.x = traps->traps[i].left.p1.x;
	    rectangles[i].left.dir = 1;

	    rectangles[i].right.x = traps->traps[i].right.p1.x;
	    rectangles[i].right.dir = -1;
	} else {
	    rectangles[i].right.x = traps->traps[i].left.p1.x;
	    rectangles[i].right.dir = 1;

	    rectangles[i].left.x = traps->traps[i].right.p1.x;
	    rectangles[i].left.dir = -1;
	}

	rectangles[i].left.deferred_trap.right = NULL;
	cairo_list_init (&rectangles[i].left.link);

	rectangles[i].right.deferred_trap.right = NULL;
	cairo_list_init (&rectangles[i].right.link);

	rectangles[i].top = traps->traps[i].top;
	rectangles[i].bottom = traps->traps[i].bottom;

	rectangles_ptrs[i] = &rectangles[i];
    }

    _cairo_traps_clear (traps);
    status = _cairo_bentley_ottmann_tessellate_rectangular (rectangles_ptrs, i,
							    fill_rule,
							    traps);
    traps->is_rectilinear = TRUE;
    traps->is_rectangular = TRUE;

    if (rectangles != stack_rectangles)
	free (rectangles);

    dump_traps (traps, "bo-rects-traps-out.txt");


    return status;
}
