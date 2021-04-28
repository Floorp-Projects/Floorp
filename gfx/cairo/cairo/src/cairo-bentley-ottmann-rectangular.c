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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
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

#include "cairo-boxes-private.h"
#include "cairo-error-private.h"
#include "cairo-combsort-private.h"
#include "cairo-list-private.h"

#include <setjmp.h>

typedef struct _rectangle rectangle_t;
typedef struct _edge edge_t;

struct _edge {
    edge_t *next, *prev;
    edge_t *right;
    cairo_fixed_t x, top;
    int dir;
};

struct _rectangle {
    edge_t left, right;
    int32_t top, bottom;
};

#define UNROLL3(x) x x x

/* the parent is always given by index/2 */
#define PQ_PARENT_INDEX(i) ((i) >> 1)
#define PQ_FIRST_ENTRY 1

/* left and right children are index * 2 and (index * 2) +1 respectively */
#define PQ_LEFT_CHILD_INDEX(i) ((i) << 1)

typedef struct _pqueue {
    int size, max_size;

    rectangle_t **elements;
    rectangle_t *elements_embedded[1024];
} pqueue_t;

typedef struct _sweep_line {
    rectangle_t **rectangles;
    pqueue_t pq;
    edge_t head, tail;
    edge_t *insert_left, *insert_right;
    int32_t current_y;
    int32_t last_y;

    cairo_fill_rule_t fill_rule;

    jmp_buf unwind;
} sweep_line_t;

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
rectangle_compare_start (const rectangle_t *a,
			 const rectangle_t *b)
{
    return a->top - b->top;
}

static inline int
rectangle_compare_stop (const rectangle_t *a,
			 const rectangle_t *b)
{
    return a->bottom - b->bottom;
}

static inline void
pqueue_init (pqueue_t *pq)
{
    pq->max_size = ARRAY_LENGTH (pq->elements_embedded);
    pq->size = 0;

    pq->elements = pq->elements_embedded;
    pq->elements[PQ_FIRST_ENTRY] = NULL;
}

static inline void
pqueue_fini (pqueue_t *pq)
{
    if (pq->elements != pq->elements_embedded)
	free (pq->elements);
}

static cairo_bool_t
pqueue_grow (pqueue_t *pq)
{
    rectangle_t **new_elements;
    pq->max_size *= 2;

    if (pq->elements == pq->elements_embedded) {
	new_elements = _cairo_malloc_ab (pq->max_size,
					 sizeof (rectangle_t *));
	if (unlikely (new_elements == NULL))
	    return FALSE;

	memcpy (new_elements, pq->elements_embedded,
		sizeof (pq->elements_embedded));
    } else {
	new_elements = _cairo_realloc_ab (pq->elements,
					  pq->max_size,
					  sizeof (rectangle_t *));
	if (unlikely (new_elements == NULL))
	    return FALSE;
    }

    pq->elements = new_elements;
    return TRUE;
}

static inline void
pqueue_push (sweep_line_t *sweep, rectangle_t *rectangle)
{
    rectangle_t **elements;
    int i, parent;

    if (unlikely (sweep->pq.size + 1 == sweep->pq.max_size)) {
	if (unlikely (! pqueue_grow (&sweep->pq))) {
	    longjmp (sweep->unwind,
		     _cairo_error (CAIRO_STATUS_NO_MEMORY));
	}
    }

    elements = sweep->pq.elements;
    for (i = ++sweep->pq.size;
	 i != PQ_FIRST_ENTRY &&
	 rectangle_compare_stop (rectangle,
				 elements[parent = PQ_PARENT_INDEX (i)]) < 0;
	 i = parent)
    {
	elements[i] = elements[parent];
    }

    elements[i] = rectangle;
}

static inline void
pqueue_pop (pqueue_t *pq)
{
    rectangle_t **elements = pq->elements;
    rectangle_t *tail;
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
	    rectangle_compare_stop (elements[child+1],
				    elements[child]) < 0)
	{
	    child++;
	}

	if (rectangle_compare_stop (elements[child], tail) >= 0)
	    break;

	elements[i] = elements[child];
    }
    elements[i] = tail;
}

static inline rectangle_t *
rectangle_pop_start (sweep_line_t *sweep_line)
{
    return *sweep_line->rectangles++;
}

static inline rectangle_t *
rectangle_peek_stop (sweep_line_t *sweep_line)
{
    return sweep_line->pq.elements[PQ_FIRST_ENTRY];
}

CAIRO_COMBSORT_DECLARE (_rectangle_sort,
			rectangle_t *,
			rectangle_compare_start)

static void
sweep_line_init (sweep_line_t	 *sweep_line,
		 rectangle_t	**rectangles,
		 int		  num_rectangles,
		 cairo_fill_rule_t fill_rule)
{
    _rectangle_sort (rectangles, num_rectangles);
    rectangles[num_rectangles] = NULL;
    sweep_line->rectangles = rectangles;

    sweep_line->head.x = INT32_MIN;
    sweep_line->head.right = NULL;
    sweep_line->head.dir = 0;
    sweep_line->head.next = &sweep_line->tail;
    /* we need to initialize prev so that we can check
     * if this edge is the left most and make sure
     * we always insert to the right of it, even if
     * our x coordinate matches */
    sweep_line->head.prev = NULL;

    sweep_line->tail.x = INT32_MAX;
    sweep_line->tail.right = NULL;
    sweep_line->tail.dir = 0;
    sweep_line->tail.prev = &sweep_line->head;
    sweep_line->tail.next = NULL;

    sweep_line->insert_left = &sweep_line->tail;
    sweep_line->insert_right = &sweep_line->tail;

    sweep_line->current_y = INT32_MIN;
    sweep_line->last_y = INT32_MIN;

    sweep_line->fill_rule = fill_rule;

    pqueue_init (&sweep_line->pq);
}

static void
sweep_line_fini (sweep_line_t *sweep_line)
{
    pqueue_fini (&sweep_line->pq);
}

static void
edge_end_box (sweep_line_t *sweep_line,
	      edge_t	*left,
	      int32_t		 bot,
	      cairo_bool_t	 do_traps,
	      void	        *container)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    /* Only emit (trivial) non-degenerate trapezoids with positive height. */
    if (likely (left->top < bot)) {
	if (do_traps) {
	    cairo_line_t _left = {
		{ left->x, left->top },
		{ left->x, bot },
	    }, _right = {
		{ left->right->x, left->top },
		{ left->right->x, bot },
	    };
	    _cairo_traps_add_trap (container, left->top, bot, &_left, &_right);
	    status = _cairo_traps_status ((cairo_traps_t *) container);
	} else {
	    cairo_box_t box;

	    box.p1.x = left->x;
	    box.p1.y = left->top;
	    box.p2.x = left->right->x;
	    box.p2.y = bot;

	    status = _cairo_boxes_add (container, &box);
	}
    }
    if (unlikely (status))
	longjmp (sweep_line->unwind, status);

    left->right = NULL;
}

/* Start a new trapezoid at the given top y coordinate, whose edges
 * are `edge' and `edge->next'. If `edge' already has a trapezoid,
 * then either add it to the traps in `traps', if the trapezoid's
 * right edge differs from `edge->next', or do nothing if the new
 * trapezoid would be a continuation of the existing one. */
static inline void
edge_start_or_continue_box (sweep_line_t *sweep_line,
			    edge_t	*left,
			    edge_t	*right,
			    int		 top,
			    cairo_bool_t	 do_traps,
			    void		*container)
{
    if (left->right == right)
	return;

    if (left->right != NULL) {
	if (right != NULL && left->right->x == right->x) {
	    /* continuation on right, so just swap edges */
	    left->right = right;
	    return;
	}

	edge_end_box (sweep_line,
		      left, top, do_traps, container);
    }

    if (right != NULL && left->x != right->x) {
	left->top = top;
	left->right = right;
    }
}

static inline void
active_edges_to_traps (sweep_line_t	*sweep,
		       cairo_bool_t	 do_traps,
		       void		*container)
{
    int top = sweep->current_y;
    edge_t *pos;

    if (sweep->last_y == sweep->current_y)
	return;

    pos = sweep->head.next;
    if (pos == &sweep->tail)
	return;

    if (sweep->fill_rule == CAIRO_FILL_RULE_WINDING) {
	do {
	    edge_t *left, *right;
	    int winding;

	    left = pos;
	    winding = left->dir;

	    right = left->next;

	    /* Check if there is a co-linear edge with an existing trap */
	    if (left->right == NULL) {
		while (unlikely (right->x == left->x)) {
		    winding += right->dir;
		    if (right->right != NULL) {
			/* continuation on left */
			left->top = right->top;
			left->right = right->right;
			right->right = NULL;
			winding -= right->dir;
			break;
		    }

		    right = right->next;
		}

		if (winding == 0) {
		    pos = right;
		    continue;
		}
	    }

	    /* Greedily search for the closing edge, so that we generate the
	     * maximal span width with the minimal number of trapezoids.
	     */

	    do {
		/* End all subsumed traps */
		if (unlikely (right->right != NULL)) {
		    edge_end_box (sweep,
				  right, top, do_traps, container);
		}

		winding += right->dir;
		if (winding == 0) {
		    /* skip co-linear edges */
		    if (likely (right->x != right->next->x))
			break;
		}

		right = right->next;
	    } while (TRUE);

	    edge_start_or_continue_box (sweep,
					left, right, top,
					do_traps, container);

	    pos = right->next;
	} while (pos != &sweep->tail);
    } else {
	do {
	    edge_t *right = pos->next;
	    int count = 0;

	    do {
		/* End all subsumed traps */
		if (unlikely (right->right != NULL)) {
		    edge_end_box (sweep,
				  right, top, do_traps, container);
		}

		if (++count & 1) {
		    /* skip co-linear edges */
		    if (likely (right->x != right->next->x))
			break;
		}

		right = right->next;
	    } while (TRUE);

	    edge_start_or_continue_box (sweep,
					pos, right, top,
					do_traps, container);

	    pos = right->next;
	} while (pos != &sweep->tail);
    }

    sweep->last_y = sweep->current_y;
}

static inline void
sweep_line_delete_edge (sweep_line_t *sweep_line,
			edge_t *edge,
			cairo_bool_t do_traps,
			void *container)
{
    if (edge->right != NULL) {
	edge_t *next = edge->next;
	if (next->x == edge->x) {
	    next->top = edge->top;
	    next->right = edge->right;
	} else {
	    edge_end_box (sweep_line,
			  edge,
			  sweep_line->current_y,
			  do_traps, container);
	}
    }

    if (sweep_line->insert_left == edge)
	sweep_line->insert_left = edge->next;
    if (sweep_line->insert_right == edge)
	sweep_line->insert_right = edge->next;

    edge->prev->next = edge->next;
    edge->next->prev = edge->prev;
}

static inline cairo_bool_t
sweep_line_delete (sweep_line_t	*sweep,
		   rectangle_t	*rectangle,
		   cairo_bool_t		 do_traps,
		   void			*container)
{
    cairo_bool_t update;

    update = TRUE;
    if (sweep->fill_rule == CAIRO_FILL_RULE_WINDING &&
	rectangle->left.prev->dir == rectangle->left.dir)
    {
	update = rectangle->left.next != &rectangle->right;
    }

    sweep_line_delete_edge (sweep,
			    &rectangle->left,
			    do_traps, container);

    sweep_line_delete_edge (sweep,
			    &rectangle->right,
			    do_traps, container);

    pqueue_pop (&sweep->pq);
    return update;
}

static inline void
insert_edge (edge_t *edge, edge_t *pos)
{
    if (pos->x != edge->x) {
	if (pos->x > edge->x) {
	    do {
		UNROLL3({
		    if (pos->prev->x <= edge->x)
			break;
		    pos = pos->prev;
		})
	    } while (TRUE);
	} else {
	    do {
		UNROLL3({
		    pos = pos->next;
		    if (pos->x >= edge->x)
			break;
		})
	    } while (TRUE);
	}
    }
    if (pos->prev) {
        pos->prev->next = edge;
        edge->prev = pos->prev;
        edge->next = pos;
        pos->prev = edge;
    } else {
        /* we have edge that shares an x coordinate with the left most sentinal.
         * instead of inserting before pos and ruining our sentinal we insert after pos. */
        pos->next->prev = edge;
        edge->next = pos->next;
        edge->prev = pos;
        pos->next = edge;
    }
}

static inline cairo_bool_t
sweep_line_insert (sweep_line_t	*sweep,
		   rectangle_t	*rectangle)
{
    edge_t *pos;

    /* right edge */
    pos = sweep->insert_right;
    insert_edge (&rectangle->right, pos);
    sweep->insert_right = &rectangle->right;

    /* left edge */
    pos = sweep->insert_left;
    if (pos->x > sweep->insert_right->x)
	pos = sweep->insert_right->prev;
    insert_edge (&rectangle->left, pos);
    sweep->insert_left = &rectangle->left;

    pqueue_push (sweep, rectangle);

    if (sweep->fill_rule == CAIRO_FILL_RULE_WINDING &&
	rectangle->left.prev->dir == rectangle->left.dir)
    {
	return rectangle->left.next != &rectangle->right;
    }

    return TRUE;
}

static cairo_status_t
_cairo_bentley_ottmann_tessellate_rectangular (rectangle_t	**rectangles,
					       int			  num_rectangles,
					       cairo_fill_rule_t	  fill_rule,
					       cairo_bool_t		 do_traps,
					       void			*container)
{
    sweep_line_t sweep_line;
    rectangle_t *rectangle;
    cairo_status_t status;
    cairo_bool_t update = FALSE;

    sweep_line_init (&sweep_line, rectangles, num_rectangles, fill_rule);
    if ((status = setjmp (sweep_line.unwind)))
	goto unwind;

    rectangle = rectangle_pop_start (&sweep_line);
    do {
	if (rectangle->top != sweep_line.current_y) {
	    rectangle_t *stop;

	    stop = rectangle_peek_stop (&sweep_line);
	    while (stop != NULL && stop->bottom < rectangle->top) {
		if (stop->bottom != sweep_line.current_y) {
		    if (update) {
			active_edges_to_traps (&sweep_line,
					       do_traps, container);
			update = FALSE;
		    }

		    sweep_line.current_y = stop->bottom;
		}

		update |= sweep_line_delete (&sweep_line, stop, do_traps, container);

		stop = rectangle_peek_stop (&sweep_line);
	    }

	    if (update) {
		active_edges_to_traps (&sweep_line, do_traps, container);
		update = FALSE;
	    }

	    sweep_line.current_y = rectangle->top;
	}

	update |= sweep_line_insert (&sweep_line, rectangle);
    } while ((rectangle = rectangle_pop_start (&sweep_line)) != NULL);

    while ((rectangle = rectangle_peek_stop (&sweep_line)) != NULL) {
	if (rectangle->bottom != sweep_line.current_y) {
	    if (update) {
		active_edges_to_traps (&sweep_line, do_traps, container);
		update = FALSE;
	    }

	    sweep_line.current_y = rectangle->bottom;
	}

	update |= sweep_line_delete (&sweep_line, rectangle, do_traps, container);
    }

unwind:
    sweep_line_fini (&sweep_line);
    return status;
}

cairo_status_t
_cairo_bentley_ottmann_tessellate_rectangular_traps (cairo_traps_t *traps,
						     cairo_fill_rule_t fill_rule)
{
    rectangle_t stack_rectangles[CAIRO_STACK_ARRAY_LENGTH (rectangle_t)];
    rectangle_t *rectangles;
    rectangle_t *stack_rectangles_ptrs[ARRAY_LENGTH (stack_rectangles) + 1];
    rectangle_t **rectangles_ptrs;
    cairo_status_t status;
    int i;

    assert (traps->is_rectangular);

    if (unlikely (traps->num_traps <= 1)) {
        if (traps->num_traps == 1) {
            cairo_trapezoid_t *trap = traps->traps;
            if (trap->left.p1.x > trap->right.p1.x) {
                cairo_line_t tmp = trap->left;
                trap->left = trap->right;
                trap->right = tmp;
          }
        }
        return CAIRO_STATUS_SUCCESS;
    }

    dump_traps (traps, "bo-rects-traps-in.txt");

    rectangles = stack_rectangles;
    rectangles_ptrs = stack_rectangles_ptrs;
    if (traps->num_traps > ARRAY_LENGTH (stack_rectangles)) {
	rectangles = _cairo_malloc_ab_plus_c (traps->num_traps,
					  sizeof (rectangle_t) +
					  sizeof (rectangle_t *),
					  sizeof (rectangle_t *));
	if (unlikely (rectangles == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	rectangles_ptrs = (rectangle_t **) (rectangles + traps->num_traps);
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

	rectangles[i].left.right = NULL;
	rectangles[i].right.right = NULL;

	rectangles[i].top = traps->traps[i].top;
	rectangles[i].bottom = traps->traps[i].bottom;

	rectangles_ptrs[i] = &rectangles[i];
    }

    _cairo_traps_clear (traps);
    status = _cairo_bentley_ottmann_tessellate_rectangular (rectangles_ptrs, i,
							    fill_rule,
							    TRUE, traps);
    traps->is_rectilinear = TRUE;
    traps->is_rectangular = TRUE;

    if (rectangles != stack_rectangles)
	free (rectangles);

    dump_traps (traps, "bo-rects-traps-out.txt");

    return status;
}

cairo_status_t
_cairo_bentley_ottmann_tessellate_boxes (const cairo_boxes_t *in,
					 cairo_fill_rule_t fill_rule,
					 cairo_boxes_t *out)
{
    rectangle_t stack_rectangles[CAIRO_STACK_ARRAY_LENGTH (rectangle_t)];
    rectangle_t *rectangles;
    rectangle_t *stack_rectangles_ptrs[ARRAY_LENGTH (stack_rectangles) + 1];
    rectangle_t **rectangles_ptrs;
    const struct _cairo_boxes_chunk *chunk;
    cairo_status_t status;
    int i, j;

    if (unlikely (in->num_boxes == 0)) {
	_cairo_boxes_clear (out);
	return CAIRO_STATUS_SUCCESS;
    }

    if (in->num_boxes == 1) {
	if (in == out) {
	    cairo_box_t *box = &in->chunks.base[0];

	    if (box->p1.x > box->p2.x) {
		cairo_fixed_t tmp = box->p1.x;
		box->p1.x = box->p2.x;
		box->p2.x = tmp;
	    }
	} else {
	    cairo_box_t box = in->chunks.base[0];

	    if (box.p1.x > box.p2.x) {
		cairo_fixed_t tmp = box.p1.x;
		box.p1.x = box.p2.x;
		box.p2.x = tmp;
	    }

	    _cairo_boxes_clear (out);
	    status = _cairo_boxes_add (out, &box);
	    assert (status == CAIRO_STATUS_SUCCESS);
	}
	return CAIRO_STATUS_SUCCESS;
    }

    rectangles = stack_rectangles;
    rectangles_ptrs = stack_rectangles_ptrs;
    if (in->num_boxes > ARRAY_LENGTH (stack_rectangles)) {
	rectangles = _cairo_malloc_ab_plus_c (in->num_boxes,
					  sizeof (rectangle_t) +
					  sizeof (rectangle_t *),
					  sizeof (rectangle_t *));
	if (unlikely (rectangles == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	rectangles_ptrs = (rectangle_t **) (rectangles + in->num_boxes);
    }

    j = 0;
    for (chunk = &in->chunks; chunk != NULL; chunk = chunk->next) {
	const cairo_box_t *box = chunk->base;
	for (i = 0; i < chunk->count; i++) {
	    if (box[i].p1.x < box[i].p2.x) {
		rectangles[j].left.x = box[i].p1.x;
		rectangles[j].left.dir = 1;

		rectangles[j].right.x = box[i].p2.x;
		rectangles[j].right.dir = -1;
	    } else {
		rectangles[j].right.x = box[i].p1.x;
		rectangles[j].right.dir = 1;

		rectangles[j].left.x = box[i].p2.x;
		rectangles[j].left.dir = -1;
	    }

	    rectangles[j].left.right = NULL;
	    rectangles[j].right.right = NULL;

	    rectangles[j].top = box[i].p1.y;
	    rectangles[j].bottom = box[i].p2.y;

	    rectangles_ptrs[j] = &rectangles[j];
	    j++;
	}
    }

    _cairo_boxes_clear (out);
    status = _cairo_bentley_ottmann_tessellate_rectangular (rectangles_ptrs, j,
							    fill_rule,
							    FALSE, out);
    if (rectangles != stack_rectangles)
	free (rectangles);

    return status;
}
