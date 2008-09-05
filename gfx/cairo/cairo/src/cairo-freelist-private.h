/*
 * Copyright © 2006 Joonas Pihlaja
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */
#ifndef CAIRO_FREELIST_H
#define CAIRO_FREELIST_H

#include "cairo-types-private.h"
#include "cairo-compiler-private.h"

/* Opaque implementation types. */
typedef struct _cairo_freelist cairo_freelist_t;
typedef struct _cairo_freelist_node cairo_freelist_node_t;

struct _cairo_freelist_node {
    cairo_freelist_node_t *next;
};

struct _cairo_freelist {
    cairo_freelist_node_t *first_free_node;
    unsigned nodesize;
};


/* Initialise a freelist that will be responsible for allocating
 * nodes of size nodesize. */
cairo_private void
_cairo_freelist_init (cairo_freelist_t *freelist, unsigned nodesize);

/* Deallocate any nodes in the freelist. */
cairo_private void
_cairo_freelist_fini (cairo_freelist_t *freelist);

/* Allocate a new node from the freelist.  If the freelist contains no
 * nodes, a new one will be allocated using malloc().  The caller is
 * responsible for calling _cairo_freelist_free() or free() on the
 * returned node.  Returns %NULL on memory allocation error. */
cairo_private void *
_cairo_freelist_alloc (cairo_freelist_t *freelist);

/* Allocate a new node from the freelist.  If the freelist contains no
 * nodes, a new one will be allocated using calloc().  The caller is
 * responsible for calling _cairo_freelist_free() or free() on the
 * returned node.  Returns %NULL on memory allocation error. */
cairo_private void *
_cairo_freelist_calloc (cairo_freelist_t *freelist);

/* Return a node to the freelist. This does not deallocate the memory,
 * but makes it available for later reuse by
 * _cairo_freelist_alloc(). */
cairo_private void
_cairo_freelist_free (cairo_freelist_t *freelist, void *node);

#endif /* CAIRO_FREELIST_H */
