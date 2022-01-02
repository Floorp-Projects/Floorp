/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2012 Intel Corporation
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#if !CAIRO_HAS_XLIB_XCB_FUNCTIONS

#include "cairo-xlib-private.h"
#include "cairo-xlib-surface-private.h"

#if !HAVE_X11_EXTENSIONS_XSHM_H || !(HAVE_X11_EXTENSIONS_SHMPROTO_H || HAVE_X11_EXTENSIONS_SHMSTR_H)
void _cairo_xlib_display_init_shm (cairo_xlib_display_t *display)
{
    display->shm = NULL;
}

cairo_surface_t *
_cairo_xlib_surface_get_shm (cairo_xlib_surface_t *surface,
			     cairo_bool_t overwrite)
{
    return NULL;
}

cairo_int_status_t
_cairo_xlib_surface_put_shm (cairo_xlib_surface_t *surface)
{
    assert (!surface->fallback);
    return CAIRO_INT_STATUS_SUCCESS;
}

cairo_surface_t *
_cairo_xlib_surface_create_shm (cairo_xlib_surface_t *other,
				pixman_format_code_t format,
				int width, int height)
{
    return NULL;
}

cairo_surface_t *
_cairo_xlib_surface_create_shm__image (cairo_xlib_surface_t *surface,
				       pixman_format_code_t format,
				       int width, int height)
{
    return NULL;
}

cairo_surface_t *
_cairo_xlib_surface_create_similar_shm (void *other,
					cairo_format_t format,
					int width, int height)
{
    return cairo_image_surface_create (format, width, height);
}

void
_cairo_xlib_shm_surface_mark_active (cairo_surface_t *_shm)
{
    ASSERT_NOT_REACHED;
}

void
_cairo_xlib_shm_surface_get_ximage (cairo_surface_t *surface,
				    XImage *ximage)
{
    ASSERT_NOT_REACHED;
}

void *
_cairo_xlib_shm_surface_get_obdata (cairo_surface_t *surface)
{
    ASSERT_NOT_REACHED;
    return NULL;
}

Pixmap
_cairo_xlib_shm_surface_get_pixmap (cairo_surface_t *surface)
{
    ASSERT_NOT_REACHED;
    return 0;
}

XRenderPictFormat *
_cairo_xlib_shm_surface_get_xrender_format (cairo_surface_t *surface)
{
    ASSERT_NOT_REACHED;
    return NULL;
}

cairo_bool_t
_cairo_xlib_shm_surface_is_active (cairo_surface_t *surface)
{
    ASSERT_NOT_REACHED;
    return FALSE;
}

cairo_bool_t
_cairo_xlib_shm_surface_is_idle (cairo_surface_t *surface)
{
    ASSERT_NOT_REACHED;
    return TRUE;
}

void _cairo_xlib_display_fini_shm (cairo_xlib_display_t *display) {}

#else

#include "cairo-damage-private.h"
#include "cairo-default-context-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-list-inline.h"
#include "cairo-mempool-private.h"

#include <X11/Xlibint.h>
#include <X11/Xproto.h>
#include <X11/extensions/XShm.h>
#if HAVE_X11_EXTENSIONS_SHMPROTO_H
#include <X11/extensions/shmproto.h>
#elif HAVE_X11_EXTENSIONS_SHMSTR_H
#include <X11/extensions/shmstr.h>
#endif
#include <sys/ipc.h>
#include <sys/shm.h>

#define MIN_PIXMAP_SIZE 4096

#define MIN_BITS 8
#define MIN_SIZE (1<<(MIN_BITS-1))

typedef struct _cairo_xlib_shm cairo_xlib_shm_t;
typedef struct _cairo_xlib_shm_info cairo_xlib_shm_info_t;
typedef struct _cairo_xlib_shm_surface cairo_xlib_shm_surface_t;

struct _cairo_xlib_shm {
    cairo_mempool_t mem;

    XShmSegmentInfo shm;
    unsigned long attached;
    cairo_list_t link;
};

struct _cairo_xlib_shm_info {
    unsigned long last_request;
    void *mem;
    size_t size;
    cairo_xlib_shm_t *pool;
};

struct _cairo_xlib_shm_surface {
    cairo_image_surface_t image;

    cairo_list_t link;
    cairo_xlib_shm_info_t *info;
    Pixmap pixmap;
    unsigned long active;
    int idle;
};

/* the parent is always given by index/2 */
#define PQ_PARENT_INDEX(i) ((i) >> 1)
#define PQ_FIRST_ENTRY 1

/* left and right children are index * 2 and (index * 2) +1 respectively */
#define PQ_LEFT_CHILD_INDEX(i) ((i) << 1)

#define PQ_TOP(pq) ((pq)->elements[PQ_FIRST_ENTRY])

struct pqueue {
    int size, max_size;
    cairo_xlib_shm_info_t **elements;
};

struct _cairo_xlib_shm_display {
    int has_pixmaps;
    int opcode;
    int event;

    Window window;
    unsigned long last_request;
    unsigned long last_event;

    cairo_list_t surfaces;

    cairo_list_t pool;
    struct pqueue info;
};

static inline cairo_bool_t
seqno_passed (unsigned long a, unsigned long b)
{
    return (long)(b - a) >= 0;
}

static inline cairo_bool_t
seqno_before (unsigned long a, unsigned long b)
{
    return (long)(b - a) > 0;
}

static inline cairo_bool_t
seqno_after (unsigned long a, unsigned long b)
{
    return (long)(a - b) > 0;
}

static inline cairo_status_t
_pqueue_init (struct pqueue *pq)
{
    pq->max_size = 32;
    pq->size = 0;

    pq->elements = _cairo_malloc_ab (pq->max_size,
				     sizeof (cairo_xlib_shm_info_t *));
    if (unlikely (pq->elements == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    PQ_TOP(pq) = NULL;
    return CAIRO_STATUS_SUCCESS;
}

static inline void
_pqueue_fini (struct pqueue *pq)
{
    free (pq->elements);
}

static cairo_status_t
_pqueue_grow (struct pqueue *pq)
{
    cairo_xlib_shm_info_t **new_elements;

    new_elements = _cairo_realloc_ab (pq->elements,
				      2 * pq->max_size,
				      sizeof (cairo_xlib_shm_info_t *));
    if (unlikely (new_elements == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    pq->elements = new_elements;
    pq->max_size *= 2;
    return CAIRO_STATUS_SUCCESS;
}

static void
_pqueue_shrink (struct pqueue *pq, int min_size)
{
    cairo_xlib_shm_info_t **new_elements;

    if (min_size > pq->max_size)
	return;

    new_elements = _cairo_realloc_ab (pq->elements,
				      min_size,
				      sizeof (cairo_xlib_shm_info_t *));
    if (unlikely (new_elements == NULL))
	return;

    pq->elements = new_elements;
    pq->max_size = min_size;
}

static inline cairo_status_t
_pqueue_push (struct pqueue *pq, cairo_xlib_shm_info_t *info)
{
    cairo_xlib_shm_info_t **elements;
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
	 info->last_request < elements[parent = PQ_PARENT_INDEX (i)]->last_request;
	 i = parent)
    {
	elements[i] = elements[parent];
    }

    elements[i] = info;

    return CAIRO_STATUS_SUCCESS;
}

static inline void
_pqueue_pop (struct pqueue *pq)
{
    cairo_xlib_shm_info_t **elements = pq->elements;
    cairo_xlib_shm_info_t *tail;
    int child, i;

    tail = elements[pq->size--];
    if (pq->size == 0) {
	elements[PQ_FIRST_ENTRY] = NULL;
	_pqueue_shrink (pq, 32);
	return;
    }

    for (i = PQ_FIRST_ENTRY;
	 (child = PQ_LEFT_CHILD_INDEX (i)) <= pq->size;
	 i = child)
    {
	if (child != pq->size &&
	    elements[child+1]->last_request < elements[child]->last_request)
	{
	    child++;
	}

	if (elements[child]->last_request >= tail->last_request)
	    break;

	elements[i] = elements[child];
    }
    elements[i] = tail;
}

static cairo_bool_t _x_error_occurred;

static int
_check_error_handler (Display     *display,
		     XErrorEvent *event)
{
    _x_error_occurred = TRUE;
    return False; /* ignored */
}

static cairo_bool_t
can_use_shm (Display *dpy, int *has_pixmap)
{
    XShmSegmentInfo shm;
    int (*old_handler) (Display *display, XErrorEvent *event);
    Status success;
    int major, minor;

    if (! XShmQueryExtension (dpy))
	return FALSE;

    XShmQueryVersion (dpy, &major, &minor, has_pixmap);

    shm.shmid = shmget (IPC_PRIVATE, 0x1000, IPC_CREAT | 0600);
    if (shm.shmid == -1)
	return FALSE;

    shm.readOnly = FALSE;
    shm.shmaddr = shmat (shm.shmid, NULL, 0);
    if (shm.shmaddr == (char *) -1) {
	shmctl (shm.shmid, IPC_RMID, NULL);
	return FALSE;
    }

    assert (CAIRO_MUTEX_IS_LOCKED (_cairo_xlib_display_mutex));
    _x_error_occurred = FALSE;

    XLockDisplay (dpy);
    XSync (dpy, False);
    old_handler = XSetErrorHandler (_check_error_handler);

    success = XShmAttach (dpy, &shm);
    if (success)
	XShmDetach (dpy, &shm);

    XSync (dpy, False);
    XSetErrorHandler (old_handler);
    XUnlockDisplay (dpy);

    shmctl (shm.shmid, IPC_RMID, NULL);
    shmdt (shm.shmaddr);

    return success && ! _x_error_occurred;
}

static inline Display *
peek_display (cairo_device_t *device)
{
    return ((cairo_xlib_display_t *)device)->display;
}

static inline unsigned long
peek_processed (cairo_device_t *device)
{
    return LastKnownRequestProcessed (peek_display(device));
}

static void
_cairo_xlib_display_shm_pool_destroy (cairo_xlib_display_t *display,
				      cairo_xlib_shm_t *pool)
{
    shmdt (pool->shm.shmaddr);
    if (display->display) /* may be called after CloseDisplay */
	XShmDetach (display->display, &pool->shm);

    _cairo_mempool_fini (&pool->mem);

    cairo_list_del (&pool->link);
    free (pool);
}

static void send_event(cairo_xlib_display_t *display,
		       cairo_xlib_shm_info_t *info,
		       unsigned long seqno)
{
    XShmCompletionEvent ev;

    if (! seqno_after (seqno, display->shm->last_event))
	return;

    ev.type = display->shm->event;
    ev.send_event = 1; /* XXX or lie? */
    ev.serial = XNextRequest (display->display);
    ev.drawable = display->shm->window;
    ev.major_code = display->shm->opcode;
    ev.minor_code = X_ShmPutImage;
    ev.shmseg = info->pool->shm.shmid;
    ev.offset = (char *)info->mem - (char *)info->pool->shm.shmaddr;

    XSendEvent (display->display, ev.drawable, False, 0, (XEvent *)&ev);

    display->shm->last_event = ev.serial;
}

static void _cairo_xlib_display_sync (cairo_xlib_display_t *display)
{
    cairo_xlib_shm_info_t *info;
    struct pqueue *pq = &display->shm->info;

    XSync (display->display, False);

    while ((info = PQ_TOP(pq))) {
	_cairo_mempool_free (&info->pool->mem, info->mem);
	_pqueue_pop (&display->shm->info);
	free (info);
    }
}

static void
_cairo_xlib_shm_info_cleanup (cairo_xlib_display_t *display)
{
    cairo_xlib_shm_info_t *info;
    Display *dpy = display->display;
    struct pqueue *pq = &display->shm->info;
    unsigned long processed;

    if (PQ_TOP(pq) == NULL)
	return;

    XEventsQueued (dpy, QueuedAfterReading);
    processed = LastKnownRequestProcessed (dpy);

    info = PQ_TOP(pq);
    do {
	if (! seqno_passed (info->last_request, processed)) {
	    send_event (display, info, display->shm->last_request);
	    return;
	}

	_cairo_mempool_free (&info->pool->mem, info->mem);
	_pqueue_pop (&display->shm->info);
	free (info);
    } while ((info = PQ_TOP(pq)));
}

static cairo_xlib_shm_t *
_cairo_xlib_shm_info_find (cairo_xlib_display_t *display, size_t size,
			   void **ptr, unsigned long *last_request)
{
    cairo_xlib_shm_info_t *info;
    struct pqueue *pq = &display->shm->info;

    if (PQ_TOP(pq) == NULL)
	return NULL;

    info = PQ_TOP(pq);
    do {
	cairo_xlib_shm_t *pool = info->pool;

	*last_request = info->last_request;

	_pqueue_pop (&display->shm->info);
	_cairo_mempool_free (&pool->mem, info->mem);
	free (info);

	if (pool->mem.free_bytes >= size) {
	    void *mem = _cairo_mempool_alloc (&pool->mem, size);
	    if (mem != NULL) {
		*ptr = mem;
		return pool;
	    }
	}
    } while ((info = PQ_TOP(pq)));

    return NULL;
}

static cairo_xlib_shm_t *
_cairo_xlib_shm_pool_find (cairo_xlib_display_t *display,
			   size_t size,
			   void **ptr)
{
    cairo_xlib_shm_t *pool;

    cairo_list_foreach_entry (pool, cairo_xlib_shm_t, &display->shm->pool, link) {
	if (pool->mem.free_bytes >= size) {
	    void *mem = _cairo_mempool_alloc (&pool->mem, size);
	    if (mem != NULL) {
		*ptr = mem;
		return pool;
	    }
	}
    }

    return NULL;
}

static void
_cairo_xlib_shm_pool_cleanup (cairo_xlib_display_t *display)
{
    cairo_xlib_shm_t *pool, *next;
    unsigned long processed;

    processed = LastKnownRequestProcessed (display->display);

    cairo_list_foreach_entry_safe (pool, next, cairo_xlib_shm_t,
				   &display->shm->pool, link) {
	if (! seqno_passed (pool->attached, processed))
	    break;

	if (pool->mem.free_bytes == pool->mem.max_bytes)
	    _cairo_xlib_display_shm_pool_destroy (display, pool);
    }
}

static cairo_xlib_shm_t *
_cairo_xlib_shm_pool_create(cairo_xlib_display_t *display,
			    size_t size, void **ptr)
{
    Display *dpy = display->display;
    cairo_xlib_shm_t *pool;
    size_t bytes, maxbits = 16, minbits = MIN_BITS;
    Status success;

    pool = _cairo_malloc (sizeof (cairo_xlib_shm_t));
    if (pool == NULL)
	return NULL;

    bytes = 1 << maxbits;
    while (bytes <= size)
	bytes <<= 1, maxbits++;
    bytes <<= 3;

    minbits += (maxbits - 16) / 2;

    pool->shm.shmid = shmget (IPC_PRIVATE, bytes, IPC_CREAT | 0600);
    while (pool->shm.shmid == -1 && bytes >= 2*size) {
	bytes >>= 1;
	pool->shm.shmid = shmget (IPC_PRIVATE, bytes, IPC_CREAT | 0600);
    }
    if (pool->shm.shmid == -1)
	goto cleanup;

    pool->shm.readOnly = FALSE;
    pool->shm.shmaddr = shmat (pool->shm.shmid, NULL, 0);
    if (pool->shm.shmaddr == (char *) -1) {
	shmctl (pool->shm.shmid, IPC_RMID, NULL);
	goto cleanup;
    }

    pool->attached = XNextRequest (dpy);
    success = XShmAttach (dpy, &pool->shm);
#if !IPC_RMID_DEFERRED_RELEASE
    XSync (dpy, FALSE);
#endif
    shmctl (pool->shm.shmid, IPC_RMID, NULL);

    if (! success)
	goto cleanup_shm;

    if (_cairo_mempool_init (&pool->mem, pool->shm.shmaddr, bytes,
			     minbits, maxbits - minbits + 1))
	goto cleanup_detach;

    cairo_list_add (&pool->link, &display->shm->pool);

    *ptr = _cairo_mempool_alloc (&pool->mem, size);
    assert (*ptr != NULL);
    return pool;

cleanup_detach:
    XShmDetach (dpy, &pool->shm);
cleanup_shm:
    shmdt (pool->shm.shmaddr);
cleanup:
    free (pool);
    return NULL;
}

static cairo_xlib_shm_info_t *
_cairo_xlib_shm_info_create (cairo_xlib_display_t *display,
			     size_t size, cairo_bool_t will_sync)
{
    cairo_xlib_shm_info_t *info;
    cairo_xlib_shm_t *pool;
    unsigned long last_request = 0;
    void *mem = NULL;

    _cairo_xlib_shm_info_cleanup (display);
    pool = _cairo_xlib_shm_pool_find (display, size, &mem);
    _cairo_xlib_shm_pool_cleanup (display);

    if (pool == NULL && will_sync)
	pool = _cairo_xlib_shm_info_find (display, size, &mem, &last_request);
    if (pool == NULL)
	pool = _cairo_xlib_shm_pool_create (display, size, &mem);
    if (pool == NULL)
	return NULL;

    assert (mem != NULL);

    info = _cairo_malloc (sizeof (*info));
    if (info == NULL) {
	_cairo_mempool_free (&pool->mem, mem);
	return NULL;
    }

    info->pool = pool;
    info->mem = mem;
    info->size = size;
    info->last_request = last_request;

    return info;
}

static cairo_status_t
_cairo_xlib_shm_surface_flush (void *abstract_surface, unsigned flags)
{
    cairo_xlib_shm_surface_t *shm = abstract_surface;
    cairo_xlib_display_t *display;
    Display *dpy;
    cairo_status_t status;

    if (shm->active == 0)
	return CAIRO_STATUS_SUCCESS;

    if (shm->image.base._finishing)
	return CAIRO_STATUS_SUCCESS;

    if (seqno_passed (shm->active, peek_processed (shm->image.base.device))) {
	shm->active = 0;
	return CAIRO_STATUS_SUCCESS;
    }

    status = _cairo_xlib_display_acquire (shm->image.base.device, &display);
    if (unlikely (status))
	return status;

    send_event (display, shm->info, shm->active);

    dpy = display->display;
    XEventsQueued (dpy, QueuedAfterReading);
    while (! seqno_passed (shm->active, LastKnownRequestProcessed (dpy))) {
	LockDisplay(dpy);
	_XReadEvents(dpy);
	UnlockDisplay(dpy);
    }

    cairo_device_release (&display->base);
    shm->active = 0;

    return CAIRO_STATUS_SUCCESS;
}

static inline cairo_bool_t
active (cairo_xlib_shm_surface_t *shm, Display *dpy)
{
    return (shm->active &&
	    ! seqno_passed (shm->active, LastKnownRequestProcessed (dpy)));
}

static cairo_status_t
_cairo_xlib_shm_surface_finish (void *abstract_surface)
{
    cairo_xlib_shm_surface_t *shm = abstract_surface;
    cairo_xlib_display_t *display;
    cairo_status_t status;

    if (shm->image.base.damage) {
	_cairo_damage_destroy (shm->image.base.damage);
	shm->image.base.damage = _cairo_damage_create_in_error (CAIRO_STATUS_SURFACE_FINISHED);
    }

    status = _cairo_xlib_display_acquire (shm->image.base.device, &display);
    if (unlikely (status))
	return status;

    if (shm->pixmap)
	XFreePixmap (display->display, shm->pixmap);

    if (active (shm, display->display)) {
	shm->info->last_request = shm->active;
	_pqueue_push (&display->shm->info, shm->info);
	if (seqno_before (display->shm->last_request, shm->active))
	    display->shm->last_request = shm->active;
    } else {
	_cairo_mempool_free (&shm->info->pool->mem, shm->info->mem);
	free (shm->info);

	_cairo_xlib_shm_pool_cleanup (display);
    }

    cairo_list_del (&shm->link);

    cairo_device_release (&display->base);
    return _cairo_image_surface_finish (abstract_surface);
}

static const cairo_surface_backend_t cairo_xlib_shm_surface_backend = {
    CAIRO_SURFACE_TYPE_IMAGE,
    _cairo_xlib_shm_surface_finish,

    _cairo_default_context_create,

    _cairo_image_surface_create_similar,
    NULL, /* create similar image */
    _cairo_image_surface_map_to_image,
    _cairo_image_surface_unmap_image,

    _cairo_image_surface_source,
    _cairo_image_surface_acquire_source_image,
    _cairo_image_surface_release_source_image,
    _cairo_image_surface_snapshot,

    NULL, /* copy_page */
    NULL, /* show_page */

    _cairo_image_surface_get_extents,
    _cairo_image_surface_get_font_options,

    _cairo_xlib_shm_surface_flush,
    NULL,

    _cairo_image_surface_paint,
    _cairo_image_surface_mask,
    _cairo_image_surface_stroke,
    _cairo_image_surface_fill,
    NULL, /* fill-stroke */
    _cairo_image_surface_glyphs,
};

static cairo_bool_t
has_shm (cairo_xlib_surface_t *surface)
{
    cairo_xlib_display_t *display = (cairo_xlib_display_t *)surface->base.device;
    return display->shm != NULL;
}

static int
has_shm_pixmaps (cairo_xlib_surface_t *surface)
{
    cairo_xlib_display_t *display = (cairo_xlib_display_t *)surface->base.device;
    if (!display->shm)
	return 0;

    return display->shm->has_pixmaps;
}

static cairo_xlib_shm_surface_t *
_cairo_xlib_shm_surface_create (cairo_xlib_surface_t *other,
				pixman_format_code_t format,
				int width, int height,
				cairo_bool_t will_sync,
				int create_pixmap)
{
    cairo_xlib_shm_surface_t *shm;
    cairo_xlib_display_t *display;
    pixman_image_t *image;
    int stride, size;

    if (width > XLIB_COORD_MAX || height > XLIB_COORD_MAX)
	return NULL;

    stride = CAIRO_STRIDE_FOR_WIDTH_BPP (width, PIXMAN_FORMAT_BPP(format));
    size = stride * height;
    if (size < MIN_SIZE)
	return NULL;

    shm = _cairo_malloc (sizeof (*shm));
    if (unlikely (shm == NULL))
	return (cairo_xlib_shm_surface_t *)_cairo_surface_create_in_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_surface_init (&shm->image.base,
			 &cairo_xlib_shm_surface_backend,
			 other->base.device,
			 _cairo_content_from_pixman_format (format),
			 FALSE); /* is_vector */

    if (_cairo_xlib_display_acquire (other->base.device, &display))
	goto cleanup_shm;

    shm->info = _cairo_xlib_shm_info_create (display, size, will_sync);
    if (shm->info == NULL)
	goto cleanup_display;

    image = pixman_image_create_bits (format, width, height,
				      (uint32_t *) shm->info->mem, stride);
    if (image == NULL)
	goto cleanup_info;

    _cairo_image_surface_init (&shm->image, image, format);

    shm->pixmap = 0;
    if (create_pixmap && size >= create_pixmap) {
	shm->pixmap = XShmCreatePixmap (display->display,
					other->drawable,
					shm->info->mem,
					&shm->info->pool->shm,
					shm->image.width,
					shm->image.height,
					shm->image.depth);
    }
    shm->active = shm->info->last_request;
    shm->idle = -5;

    assert (shm->active == 0 || will_sync);

    cairo_list_add (&shm->link, &display->shm->surfaces);

    cairo_device_release (&display->base);

    return shm;

cleanup_info:
    _cairo_mempool_free (&shm->info->pool->mem, shm->info->mem);
    free(shm->info);
cleanup_display:
    cairo_device_release (&display->base);
cleanup_shm:
    free (shm);
    return NULL;
}

static void
_cairo_xlib_surface_update_shm (cairo_xlib_surface_t *surface)
{
    cairo_xlib_shm_surface_t *shm = (cairo_xlib_shm_surface_t *)surface->shm;
    cairo_xlib_display_t *display;
    cairo_damage_t *damage;
    GC gc;

    damage = _cairo_damage_reduce (surface->base.damage);
    surface->base.damage = _cairo_damage_create();

    if (_cairo_xlib_display_acquire (surface->base.device, &display))
	goto cleanup_damage;

    if (_cairo_xlib_surface_get_gc (display, surface, &gc))
	goto cleanup_display;

    if (! surface->owns_pixmap) {
	XGCValues gcv;

	gcv.subwindow_mode = IncludeInferiors;
	XChangeGC (display->display, gc, GCSubwindowMode, &gcv);
    }

    if (damage->region) {
	XRectangle stack_rects[CAIRO_STACK_ARRAY_LENGTH (XRectangle)];
	XRectangle *rects = stack_rects;
	cairo_rectangle_int_t r;
	int n_rects, i;

	n_rects = cairo_region_num_rectangles (damage->region);
	if (n_rects == 0) {
	} else if (n_rects == 1) {
	    cairo_region_get_rectangle (damage->region, 0, &r);
	    XCopyArea (display->display,
		       surface->drawable, shm->pixmap, gc,
		       r.x, r.y,
		       r.width, r.height,
		       r.x, r.y);
	} else {
	    if (n_rects > ARRAY_LENGTH (stack_rects)) {
		rects = _cairo_malloc_ab (n_rects, sizeof (XRectangle));
		if (unlikely (rects == NULL)) {
		    rects = stack_rects;
		    n_rects = ARRAY_LENGTH (stack_rects);
		}
	    }
	    for (i = 0; i < n_rects; i++) {
		cairo_region_get_rectangle (damage->region, i, &r);

		rects[i].x = r.x;
		rects[i].y = r.y;
		rects[i].width  = r.width;
		rects[i].height = r.height;
	    }
	    XSetClipRectangles (display->display, gc, 0, 0, rects, i, YXBanded);

	    XCopyArea (display->display,
		       surface->drawable, shm->pixmap, gc,
		       0, 0,
		       shm->image.width, shm->image.height,
		       0, 0);

	    if (damage->status == CAIRO_STATUS_SUCCESS && damage->region)
		XSetClipMask (display->display, gc, None);
	}
    } else {
	XCopyArea (display->display,
		   surface->drawable, shm->pixmap, gc,
		   0, 0,
		   shm->image.width, shm->image.height,
		   0, 0);
    }

    if (! surface->owns_pixmap) {
	XGCValues gcv;

	gcv.subwindow_mode = ClipByChildren;
	XChangeGC (display->display, gc, GCSubwindowMode, &gcv);
    }

    _cairo_xlib_display_sync (display);
    shm->active = 0;
    shm->idle--;

    _cairo_xlib_surface_put_gc (display, surface, gc);
cleanup_display:
    cairo_device_release (&display->base);
cleanup_damage:
    _cairo_damage_destroy (damage);
}

static void
_cairo_xlib_surface_clear_shm (cairo_xlib_surface_t *surface)
{
    cairo_xlib_shm_surface_t *shm = (cairo_xlib_shm_surface_t *)surface->shm;

    assert (shm->active == 0);

    _cairo_damage_destroy (surface->base.damage);
    surface->base.damage = _cairo_damage_create();

    memset (shm->image.data, 0, shm->image.stride * shm->image.height);
    shm->image.base.is_clear = TRUE;
}

static void inc_idle (cairo_surface_t *surface)
{
    cairo_xlib_shm_surface_t *shm = (cairo_xlib_shm_surface_t *)surface;
    shm->idle++;
}

static void dec_idle (cairo_surface_t *surface)
{
    cairo_xlib_shm_surface_t *shm = (cairo_xlib_shm_surface_t *)surface;
    shm->idle--;
}

cairo_surface_t *
_cairo_xlib_surface_get_shm (cairo_xlib_surface_t *surface,
			     cairo_bool_t overwrite)
{
    if (surface->fallback) {
	assert (surface->base.damage);
	assert (surface->shm);
	assert (surface->shm->damage);
	goto done;
    }

    if (surface->shm == NULL) {
	pixman_format_code_t pixman_format;
	cairo_bool_t will_sync;

	if (! has_shm_pixmaps (surface))
	    return NULL;

	if ((surface->width | surface->height) < 32)
	    return NULL;

	pixman_format = _pixman_format_for_xlib_surface (surface);
	if (pixman_format == 0)
	    return NULL;

	will_sync = !surface->base.is_clear && !overwrite;

	surface->shm =
	    &_cairo_xlib_shm_surface_create (surface, pixman_format,
					     surface->width, surface->height,
					     will_sync, 1)->image.base;
	if (surface->shm == NULL)
	    return NULL;

	assert (surface->base.damage == NULL);
	if (surface->base.serial || !surface->owns_pixmap) {
	    cairo_rectangle_int_t rect;

	    rect.x = rect.y = 0;
	    rect.width = surface->width;
	    rect.height = surface->height;

	    surface->base.damage =
		_cairo_damage_add_rectangle (NULL, &rect);
	} else
	    surface->base.damage = _cairo_damage_create ();

	surface->shm->damage = _cairo_damage_create ();
    }

    if (overwrite) {
	_cairo_damage_destroy (surface->base.damage);
	surface->base.damage = _cairo_damage_create ();
    }

    if (!surface->base.is_clear && surface->base.damage->dirty)
	_cairo_xlib_surface_update_shm (surface);

    _cairo_xlib_shm_surface_flush (surface->shm, 1);

    if (surface->base.is_clear && surface->base.damage->dirty)
	_cairo_xlib_surface_clear_shm (surface);

done:
    dec_idle(surface->shm);
    return surface->shm;
}

cairo_int_status_t
_cairo_xlib_surface_put_shm (cairo_xlib_surface_t *surface)
{
    cairo_int_status_t status = CAIRO_INT_STATUS_SUCCESS;

    if (!surface->fallback) {
	if (surface->shm)
	    inc_idle (surface->shm);
	return CAIRO_INT_STATUS_SUCCESS;
    }

    if (surface->shm->damage->dirty) {
	cairo_xlib_shm_surface_t *shm = (cairo_xlib_shm_surface_t *) surface->shm;
	cairo_xlib_display_t *display;
	cairo_damage_t *damage;
	GC gc;

	status = _cairo_xlib_display_acquire (surface->base.device, &display);
	if (unlikely (status))
	    return status;

	damage = _cairo_damage_reduce (shm->image.base.damage);
	shm->image.base.damage = _cairo_damage_create ();

	TRACE ((stderr, "%s: flushing damage x %d\n", __FUNCTION__,
		damage->region ? cairo_region_num_rectangles (damage->region) : 0));
	if (damage->status == CAIRO_STATUS_SUCCESS && damage->region) {
	    XRectangle stack_rects[CAIRO_STACK_ARRAY_LENGTH (XRectangle)];
	    XRectangle *rects = stack_rects;
	    cairo_rectangle_int_t r;
	    int n_rects, i;

	    n_rects = cairo_region_num_rectangles (damage->region);
	    if (n_rects == 0)
		goto out;

	    status = _cairo_xlib_surface_get_gc (display, surface, &gc);
	    if (unlikely (status))
		goto out;

	    if (n_rects == 1) {
		cairo_region_get_rectangle (damage->region, 0, &r);
		_cairo_xlib_shm_surface_mark_active (surface->shm);
		XCopyArea (display->display,
			   shm->pixmap, surface->drawable, gc,
			   r.x, r.y,
			   r.width, r.height,
			   r.x, r.y);
	    } else {
		if (n_rects > ARRAY_LENGTH (stack_rects)) {
		    rects = _cairo_malloc_ab (n_rects, sizeof (XRectangle));
		    if (unlikely (rects == NULL)) {
			status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
			_cairo_xlib_surface_put_gc (display, surface, gc);
			goto out;
		    }
		}
		for (i = 0; i < n_rects; i++) {
		    cairo_region_get_rectangle (damage->region, i, &r);

		    rects[i].x = r.x;
		    rects[i].y = r.y;
		    rects[i].width  = r.width;
		    rects[i].height = r.height;
		}
		XSetClipRectangles (display->display, gc, 0, 0, rects, i, YXBanded);

		_cairo_xlib_shm_surface_mark_active (surface->shm);
		XCopyArea (display->display,
			   shm->pixmap, surface->drawable, gc,
			   0, 0,
			   shm->image.width, shm->image.height,
			   0, 0);

		if (damage->status == CAIRO_STATUS_SUCCESS && damage->region)
		    XSetClipMask (display->display, gc, None);
	    }

	    _cairo_xlib_surface_put_gc (display, surface, gc);
	}

out:
	_cairo_damage_destroy (damage);
	cairo_device_release (&display->base);
    }

    return status;
}

cairo_surface_t *
_cairo_xlib_surface_create_shm (cairo_xlib_surface_t *other,
				pixman_format_code_t format,
				int width, int height)
{
    cairo_surface_t *surface;

    surface = NULL;
    if (has_shm (other))
	surface = &_cairo_xlib_shm_surface_create (other, format, width, height,
						   FALSE, has_shm_pixmaps (other))->image.base;

    return surface;
}

cairo_surface_t *
_cairo_xlib_surface_create_shm__image (cairo_xlib_surface_t *surface,
				       pixman_format_code_t format,
				       int width, int height)
{
    if (! has_shm(surface))
	return NULL;

    return &_cairo_xlib_shm_surface_create (surface, format, width, height,
					    FALSE, 0)->image.base;
}

cairo_surface_t *
_cairo_xlib_surface_create_similar_shm (void *other,
					cairo_format_t format,
					int width, int height)
{
    cairo_surface_t *surface;

    surface = _cairo_xlib_surface_create_shm (other,
					      _cairo_format_to_pixman_format_code (format),
					      width, height);
    if (surface) {
	if (! surface->is_clear) {
	    cairo_xlib_shm_surface_t *shm = (cairo_xlib_shm_surface_t *) surface;
	    assert (shm->active == 0);
	    memset (shm->image.data, 0, shm->image.stride * shm->image.height);
	    shm->image.base.is_clear = TRUE;
	}
    } else
	surface = cairo_image_surface_create (format, width, height);

    return surface;
}

void
_cairo_xlib_shm_surface_mark_active (cairo_surface_t *_shm)
{
    cairo_xlib_shm_surface_t *shm = (cairo_xlib_shm_surface_t *) _shm;
    cairo_xlib_display_t *display = (cairo_xlib_display_t *) _shm->device;

    shm->active = XNextRequest (display->display);
}

void
_cairo_xlib_shm_surface_get_ximage (cairo_surface_t *surface,
				    XImage *ximage)
{
    cairo_xlib_shm_surface_t *shm = (cairo_xlib_shm_surface_t *) surface;
    int native_byte_order = _cairo_is_little_endian () ? LSBFirst : MSBFirst;
    cairo_format_masks_t image_masks;
    int ret;

    ret = _pixman_format_to_masks (shm->image.pixman_format, &image_masks);
    assert (ret);

    ximage->width = shm->image.width;
    ximage->height = shm->image.height;
    ximage->format = ZPixmap;
    ximage->data = (char *) shm->image.data;
    ximage->obdata = (char *)&shm->info->pool->shm;
    ximage->byte_order = native_byte_order;
    ximage->bitmap_unit = 32;	/* always for libpixman */
    ximage->bitmap_bit_order = native_byte_order;
    ximage->bitmap_pad = 32;	/* always for libpixman */
    ximage->depth = shm->image.depth;
    ximage->bytes_per_line = shm->image.stride;
    ximage->bits_per_pixel = image_masks.bpp;
    ximage->red_mask = image_masks.red_mask;
    ximage->green_mask = image_masks.green_mask;
    ximage->blue_mask = image_masks.blue_mask;
    ximage->xoffset = 0;

    ret = XInitImage (ximage);
    assert (ret != 0);
}

void *
_cairo_xlib_shm_surface_get_obdata (cairo_surface_t *surface)
{
    cairo_xlib_display_t *display = (cairo_xlib_display_t *) surface->device;
    cairo_xlib_shm_surface_t *shm = (cairo_xlib_shm_surface_t *) surface;

    display->shm->last_event = shm->active = XNextRequest (display->display);
    return &shm->info->pool->shm;
}

Pixmap
_cairo_xlib_shm_surface_get_pixmap (cairo_surface_t *surface)
{
    cairo_xlib_shm_surface_t *shm;

    shm = (cairo_xlib_shm_surface_t *) surface;
    return shm->pixmap;
}

XRenderPictFormat *
_cairo_xlib_shm_surface_get_xrender_format (cairo_surface_t *surface)
{
    cairo_xlib_shm_surface_t *shm;

    shm = (cairo_xlib_shm_surface_t *) surface;
    if (shm->image.format != CAIRO_FORMAT_INVALID)
	return _cairo_xlib_display_get_xrender_format ((cairo_xlib_display_t *)surface->device,
						       shm->image.format);

    return _cairo_xlib_display_get_xrender_format_for_pixman((cairo_xlib_display_t *)surface->device,
							     shm->image.pixman_format);
}

cairo_bool_t
_cairo_xlib_shm_surface_is_active (cairo_surface_t *surface)
{
    cairo_xlib_shm_surface_t *shm;

    shm = (cairo_xlib_shm_surface_t *) surface;
    if (shm->active == 0)
	return FALSE;

    if (seqno_passed (shm->active, peek_processed (shm->image.base.device))) {
	shm->active = 0;
	return FALSE;
    }

    return TRUE;
}

cairo_bool_t
_cairo_xlib_shm_surface_is_idle (cairo_surface_t *surface)
{
    cairo_xlib_shm_surface_t *shm;

    shm = (cairo_xlib_shm_surface_t *) surface;
    return shm->idle > 0;
}

#define XORG_VERSION_ENCODE(major,minor,patch,snap) \
    (((major) * 10000000) + ((minor) * 100000) + ((patch) * 1000) + snap)

static cairo_bool_t
has_broken_send_shm_event (cairo_xlib_display_t *display,
			   cairo_xlib_shm_display_t *shm)
{
    Display *dpy = display->display;
    int (*old_handler) (Display *display, XErrorEvent *event);
    XShmCompletionEvent ev;
    XShmSegmentInfo info;

    info.shmid = shmget (IPC_PRIVATE, 0x1000, IPC_CREAT | 0600);
    if (info.shmid == -1)
	return TRUE;

    info.readOnly = FALSE;
    info.shmaddr = shmat (info.shmid, NULL, 0);
    if (info.shmaddr == (char *) -1) {
	shmctl (info.shmid, IPC_RMID, NULL);
	return TRUE;
    }

    ev.type = shm->event;
    ev.send_event = 1;
    ev.serial = 1;
    ev.drawable = shm->window;
    ev.major_code = shm->opcode;
    ev.minor_code = X_ShmPutImage;

    ev.shmseg = info.shmid;
    ev.offset = 0;

    assert (CAIRO_MUTEX_IS_LOCKED (_cairo_xlib_display_mutex));
    _x_error_occurred = FALSE;

    XLockDisplay (dpy);
    XSync (dpy, False);
    old_handler = XSetErrorHandler (_check_error_handler);

    XShmAttach (dpy, &info);
    XSendEvent (dpy, ev.drawable, False, 0, (XEvent *)&ev);
    XShmDetach (dpy, &info);

    XSync (dpy, False);
    XSetErrorHandler (old_handler);
    XUnlockDisplay (dpy);

    shmctl (info.shmid, IPC_RMID, NULL);
    shmdt (info.shmaddr);

    return _x_error_occurred;
}

static cairo_bool_t
xorg_has_buggy_send_shm_completion_event(cairo_xlib_display_t *display,
					 cairo_xlib_shm_display_t *shm)
{
    Display *dpy = display->display;

    /* As libXext sets the SEND_EVENT bit in the ShmCompletionEvent,
     * the Xserver may crash if it does not take care when processing
     * the event type. For instance versions of Xorg prior to 1.11.1
     * exhibited this bug, and was fixed by:
     *
     * commit 2d2dce558d24eeea0eb011ec9ebaa6c5c2273c39
     * Author: Sam Spilsbury <sam.spilsbury@canonical.com>
     * Date:   Wed Sep 14 09:58:34 2011 +0800
     *
     * Remove the SendEvent bit (0x80) before doing range checks on event type.
     */
    if (_cairo_xlib_vendor_is_xorg (dpy) &&
	VendorRelease (dpy) < XORG_VERSION_ENCODE(1,11,0,1))
	return TRUE;

    /* For everyone else check that no error is generated */
    return has_broken_send_shm_event (display, shm);
}

void
_cairo_xlib_display_init_shm (cairo_xlib_display_t *display)
{
    cairo_xlib_shm_display_t *shm;
    XSetWindowAttributes attr;
    XExtCodes *codes;
    int has_pixmap, scr;

    display->shm = NULL;

    if (!can_use_shm (display->display, &has_pixmap))
	return;

    shm = _cairo_malloc (sizeof (*shm));
    if (unlikely (shm == NULL))
	return;

    codes = XInitExtension (display->display, SHMNAME);
    if (codes == NULL) {
	free (shm);
	return;
    }

    shm->opcode = codes ->major_opcode;
    shm->event = codes->first_event;

    if (unlikely (_pqueue_init (&shm->info))) {
	free (shm);
	return;
    }

    scr = DefaultScreen (display->display);
    attr.override_redirect = 1;
    shm->window = XCreateWindow (display->display,
				 DefaultRootWindow (display->display), -1, -1,
				 1, 1, 0,
				 DefaultDepth (display->display, scr),
				 InputOutput,
				 DefaultVisual (display->display, scr),
				 CWOverrideRedirect, &attr);
    shm->last_event = 0;
    shm->last_request = 0;

    if (xorg_has_buggy_send_shm_completion_event(display, shm))
	has_pixmap = 0;

    shm->has_pixmaps = has_pixmap ? MIN_PIXMAP_SIZE : 0;
    cairo_list_init (&shm->pool);

    cairo_list_init (&shm->surfaces);

    display->shm = shm;
}

void
_cairo_xlib_display_fini_shm (cairo_xlib_display_t *display)
{
    cairo_xlib_shm_display_t *shm = display->shm;

    if (shm == NULL)
	return;

    while (!cairo_list_is_empty (&shm->surfaces))
	cairo_surface_finish (&cairo_list_first_entry (&shm->surfaces,
						       cairo_xlib_shm_surface_t,
						       link)->image.base);

    _pqueue_fini (&shm->info);

    while (!cairo_list_is_empty (&shm->pool)) {
	cairo_xlib_shm_t *pool;

	pool = cairo_list_first_entry (&shm->pool, cairo_xlib_shm_t, link);
	_cairo_xlib_display_shm_pool_destroy (display, pool);
    }

    if (display->display)
	XDestroyWindow (display->display, shm->window);

    free (shm);
    display->shm = NULL;
}
#endif
#endif
