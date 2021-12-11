/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Intel Corporation
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
 * Authors:
 *    Chris Wilson <chris@chris-wilson.co.uk>
 */


#include "cairoint.h"

#include "cairo-xcb-private.h"
#include "cairo-hash-private.h"
#include "cairo-freelist-private.h"
#include "cairo-list-inline.h"

#include <xcb/xcbext.h>
#include <xcb/bigreq.h>
#include <errno.h>

#if CAIRO_HAS_XCB_SHM_FUNCTIONS
#include <sys/ipc.h>
#include <sys/shm.h>
#include <xcb/shm.h>
#endif

typedef struct _cairo_xcb_xrender_format {
    cairo_hash_entry_t key;
    xcb_render_pictformat_t xrender_format;
} cairo_xcb_xrender_format_t;

typedef struct _cairo_xcb_xid {
    cairo_list_t link;
    uint32_t xid;
} cairo_xcb_xid_t;

#define XCB_RENDER_AT_LEAST(V, major, minor)	\
	(((V)->major_version > major) ||			\
	 (((V)->major_version == major) && ((V)->minor_version >= minor)))

#define XCB_RENDER_HAS_CREATE_PICTURE(surface)		XCB_RENDER_AT_LEAST((surface), 0, 0)
#define XCB_RENDER_HAS_COMPOSITE(surface)		XCB_RENDER_AT_LEAST((surface), 0, 0)
#define XCB_RENDER_HAS_COMPOSITE_TEXT(surface)		XCB_RENDER_AT_LEAST((surface), 0, 0)

#define XCB_RENDER_HAS_FILL_RECTANGLES(surface)		XCB_RENDER_AT_LEAST((surface), 0, 1)

#define XCB_RENDER_HAS_DISJOINT(surface)		XCB_RENDER_AT_LEAST((surface), 0, 2)
#define XCB_RENDER_HAS_CONJOINT(surface)		XCB_RENDER_AT_LEAST((surface), 0, 2)

#define XCB_RENDER_HAS_TRAPEZOIDS(surface)		XCB_RENDER_AT_LEAST((surface), 0, 4)
#define XCB_RENDER_HAS_TRIANGLES(surface)		XCB_RENDER_AT_LEAST((surface), 0, 4)
#define XCB_RENDER_HAS_TRISTRIP(surface)		XCB_RENDER_AT_LEAST((surface), 0, 4)
#define XCB_RENDER_HAS_TRIFAN(surface)			XCB_RENDER_AT_LEAST((surface), 0, 4)

#define XCB_RENDER_HAS_PICTURE_TRANSFORM(surface)	XCB_RENDER_AT_LEAST((surface), 0, 6)
#define XCB_RENDER_HAS_FILTERS(surface)			XCB_RENDER_AT_LEAST((surface), 0, 6)
#define XCB_RENDER_HAS_FILTER_GOOD(surface) FALSE
#define XCB_RENDER_HAS_FILTER_BEST(surface) FALSE
#define XCB_RENDER_HAS_SUBPIXEL_ORDER(surface)		XCB_RENDER_AT_LEAST((surface), 0, 6)

#define XCB_RENDER_HAS_EXTENDED_REPEAT(surface)	XCB_RENDER_AT_LEAST((surface), 0, 10)
#define XCB_RENDER_HAS_GRADIENTS(surface)	XCB_RENDER_AT_LEAST((surface), 0, 10)

#define XCB_RENDER_HAS_PDF_OPERATORS(surface)	XCB_RENDER_AT_LEAST((surface), 0, 11)

static cairo_list_t connections;

static cairo_status_t
_cairo_xcb_connection_find_visual_formats (cairo_xcb_connection_t *connection,
					  const xcb_render_query_pict_formats_reply_t *formats)
{
    xcb_render_pictscreen_iterator_t screens;
    xcb_render_pictdepth_iterator_t depths;
    xcb_render_pictvisual_iterator_t visuals;

    for (screens = xcb_render_query_pict_formats_screens_iterator (formats);
	 screens.rem;
	 xcb_render_pictscreen_next (&screens))
    {
	for (depths = xcb_render_pictscreen_depths_iterator (screens.data);
	     depths.rem;
	     xcb_render_pictdepth_next (&depths))
	{
	    for (visuals = xcb_render_pictdepth_visuals_iterator (depths.data);
		 visuals.rem;
		 xcb_render_pictvisual_next (&visuals))
	    {
		cairo_xcb_xrender_format_t *f;
		cairo_status_t status;

		f = _cairo_malloc (sizeof (cairo_xcb_xrender_format_t));
		if (unlikely (f == NULL))
		    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

		f->key.hash = visuals.data->visual;
		f->xrender_format = visuals.data->format;
		status = _cairo_hash_table_insert (connection->visual_to_xrender_format,
						   &f->key);
		if (unlikely (status))
		    return status;
	    }
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

#if 0
static xcb_format_t *
find_format_for_depth (const xcb_setup_t *setup, uint8_t depth)
{
    xcb_format_t *fmt = xcb_setup_pixmap_formats (setup);
    xcb_format_t *fmtend = fmt + xcb_setup_pixmap_formats_length (setup);

    for (; fmt != fmtend; ++fmt)
	if (fmt->depth == depth)
	    return fmt;

    return 0;
}
#endif

static cairo_status_t
_cairo_xcb_connection_parse_xrender_formats (cairo_xcb_connection_t *connection,
					     const xcb_render_query_pict_formats_reply_t *formats)
{
    xcb_render_pictforminfo_iterator_t i;
    cairo_status_t status;

    for (i = xcb_render_query_pict_formats_formats_iterator (formats);
	 i.rem;
	 xcb_render_pictforminfo_next (&i))
    {
	cairo_format_masks_t masks;
	pixman_format_code_t pixman_format;

	if (i.data->type != XCB_RENDER_PICT_TYPE_DIRECT)
	    continue;

	masks.alpha_mask =
	    (unsigned long) i.data->direct.alpha_mask << i.data->direct.alpha_shift;
	masks.red_mask =
	    (unsigned long) i.data->direct.red_mask << i.data->direct.red_shift;
	masks.green_mask =
	    (unsigned long) i.data->direct.green_mask << i.data->direct.green_shift;
	masks.blue_mask =
	    (unsigned long) i.data->direct.blue_mask << i.data->direct.blue_shift;
	masks.bpp = i.data->depth;

	if (_pixman_format_from_masks (&masks, &pixman_format)) {
	    cairo_hash_entry_t key;

	    key.hash = pixman_format;
	    if (! _cairo_hash_table_lookup (connection->xrender_formats, &key)) {
		cairo_xcb_xrender_format_t *f;

		f = _cairo_malloc (sizeof (cairo_xcb_xrender_format_t));
		if (unlikely (f == NULL))
		    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

		f->key.hash = pixman_format;
		f->xrender_format = i.data->id;
		status = _cairo_hash_table_insert (connection->xrender_formats,
						   &f->key);
		if (unlikely (status))
		    return status;

#if 0
		printf ("xrender %x -> (%lx, %lx, %lx, %lx, %d) %x [%d, %d]\n",
			i.data->id,
			masks.alpha_mask,
			masks.red_mask,
			masks.green_mask,
			masks.blue_mask,
			masks.bpp,
			pixman_format,
			PIXMAN_FORMAT_DEPTH(pixman_format),
			PIXMAN_FORMAT_BPP(pixman_format));
#endif
	    }
	}
    }

    status = _cairo_xcb_connection_find_visual_formats (connection, formats);
    if (unlikely (status))
	return status;

    connection->standard_formats[CAIRO_FORMAT_A1] =
	_cairo_xcb_connection_get_xrender_format (connection, PIXMAN_a1);

    connection->standard_formats[CAIRO_FORMAT_A8] =
	_cairo_xcb_connection_get_xrender_format (connection, PIXMAN_a8);

    connection->standard_formats[CAIRO_FORMAT_RGB24] =
	_cairo_xcb_connection_get_xrender_format (connection,
						  PIXMAN_FORMAT (24,
								 PIXMAN_TYPE_ARGB,
								 0, 8, 8, 8));
    if (connection->standard_formats[CAIRO_FORMAT_RGB24] == XCB_NONE) {
	connection->standard_formats[CAIRO_FORMAT_RGB24] =
	    _cairo_xcb_connection_get_xrender_format (connection,
						      PIXMAN_FORMAT (24, PIXMAN_TYPE_ABGR,
								     0, 8, 8, 8));
    }

    connection->standard_formats[CAIRO_FORMAT_ARGB32] =
	_cairo_xcb_connection_get_xrender_format (connection, PIXMAN_a8r8g8b8);
    if (connection->standard_formats[CAIRO_FORMAT_ARGB32] == XCB_NONE) {
	connection->standard_formats[CAIRO_FORMAT_ARGB32] =
	    _cairo_xcb_connection_get_xrender_format (connection, PIXMAN_a8b8g8r8);
    }

    return CAIRO_STATUS_SUCCESS;
}

/*
 * We require support for depth 1, 8, 24 and 32 pixmaps
 */
#define DEPTH_MASK(d)	(1 << ((d) - 1))
#define REQUIRED_DEPTHS	(DEPTH_MASK(1) | \
			 DEPTH_MASK(8) | \
			 DEPTH_MASK(24) | \
			 DEPTH_MASK(32))
static cairo_bool_t
pixmap_depths_usable (cairo_xcb_connection_t *connection,
		      uint32_t missing,
		      xcb_drawable_t root)
{
    xcb_connection_t *c = connection->xcb_connection;
    xcb_void_cookie_t create_cookie[32];
    xcb_pixmap_t pixmap;
    cairo_bool_t success = TRUE;
    int depth, i, j;

    pixmap = _cairo_xcb_connection_get_xid (connection);

    for (depth = 1, i = 0; depth <= 32; depth++) {
	if (missing & DEPTH_MASK(depth)) {
	    create_cookie[i] = xcb_create_pixmap_checked (c, depth, pixmap, root, 1, 1);
	    xcb_free_pixmap (c, pixmap);
	    if (!create_cookie[i].sequence) {
		success = FALSE;
		break;
	    }
	    i++;
	}
    }

    for (j = 0; j < i; j++) {
	xcb_generic_error_t *create_error = xcb_request_check (c, create_cookie[j]);
	success &= create_error == NULL;
	free (create_error);
    }

    _cairo_xcb_connection_put_xid (connection, pixmap);

    return success;
}

static cairo_bool_t
has_required_depths (cairo_xcb_connection_t *connection)
{
    xcb_screen_iterator_t screens;

    for (screens = xcb_setup_roots_iterator (connection->root);
	 screens.rem;
	 xcb_screen_next (&screens))
    {
	xcb_depth_iterator_t depths;
	uint32_t missing = REQUIRED_DEPTHS;

	for (depths = xcb_screen_allowed_depths_iterator (screens.data);
	     depths.rem;
	     xcb_depth_next (&depths))
	{
	    missing &= ~DEPTH_MASK (depths.data->depth);
	}
	if (missing == 0)
	    continue;

	/*
	 * Ok, this is ugly.  It should be sufficient at this
	 * point to just return false, but Xinerama is broken at
	 * this point and only advertises depths which have an
	 * associated visual.  Of course, the other depths still
	 * work, but the only way to find out is to try them.
	 */
	if (! pixmap_depths_usable (connection, missing, screens.data->root))
	    return FALSE;
    }

    return TRUE;
}

static xcb_render_query_version_reply_t *
_render_restrict_env(xcb_render_query_version_reply_t *version)
{
    const char *env;

    if (version == NULL)
	return NULL;

    env = getenv ("CAIRO_DEBUG");
    if (env != NULL)
	env = strstr (env, "xrender-version=");
    if (env != NULL) {
	int max_render_major, max_render_minor;

	env += sizeof ("xrender-version=") - 1;
	if (sscanf (env, "%d.%d", &max_render_major, &max_render_minor) != 2)
	    max_render_major = max_render_minor = -1;

	if (max_render_major < 0 || max_render_minor < 0) {
	    free (version);
	    return NULL;
	}

	if (max_render_major < (int) version->major_version ||
	    (max_render_major == (int) version->major_version &&
	     max_render_minor < (int) version->minor_version))
	{
	    version->major_version = max_render_major;
	    version->minor_version = max_render_minor;
	}
    }

    return version;
}

static cairo_status_t
_cairo_xcb_connection_query_render (cairo_xcb_connection_t *connection)
{
    xcb_connection_t *c = connection->xcb_connection;
    xcb_render_query_version_cookie_t version_cookie;
    xcb_render_query_pict_formats_cookie_t formats_cookie;
    xcb_render_query_version_reply_t *version;
    xcb_render_query_pict_formats_reply_t *formats;
    cairo_status_t status;
    cairo_bool_t present;

    version_cookie = xcb_render_query_version (c, XCB_RENDER_MAJOR_VERSION, XCB_RENDER_MINOR_VERSION);
    formats_cookie = xcb_render_query_pict_formats (c);

    present = has_required_depths (connection);
    version = xcb_render_query_version_reply (c, version_cookie, 0);
    formats = xcb_render_query_pict_formats_reply (c, formats_cookie, 0);

    version = _render_restrict_env (version);

    if (! present || version == NULL || formats == NULL) {
	free (version);
	free (formats);
	return CAIRO_STATUS_SUCCESS;
    }

    /* always true if the extension is present (i.e. >= 0.0) */
    connection->flags |= CAIRO_XCB_HAS_RENDER;
    connection->flags |= CAIRO_XCB_RENDER_HAS_COMPOSITE;
    connection->flags |= CAIRO_XCB_RENDER_HAS_COMPOSITE_GLYPHS;

    if (XCB_RENDER_HAS_FILL_RECTANGLES (version))
	connection->flags |= CAIRO_XCB_RENDER_HAS_FILL_RECTANGLES;

    if (XCB_RENDER_HAS_TRAPEZOIDS (version))
	connection->flags |= CAIRO_XCB_RENDER_HAS_COMPOSITE_TRAPEZOIDS;

    if (XCB_RENDER_HAS_PICTURE_TRANSFORM (version))
	connection->flags |= CAIRO_XCB_RENDER_HAS_PICTURE_TRANSFORM;

    if (XCB_RENDER_HAS_FILTERS (version))
	connection->flags |= CAIRO_XCB_RENDER_HAS_FILTERS;

    if (XCB_RENDER_HAS_FILTER_GOOD (version))
	connection->flags |= CAIRO_XCB_RENDER_HAS_FILTER_GOOD;

    if (XCB_RENDER_HAS_FILTER_BEST (version))
	connection->flags |= CAIRO_XCB_RENDER_HAS_FILTER_BEST;

    if (XCB_RENDER_HAS_PDF_OPERATORS (version))
	connection->flags |= CAIRO_XCB_RENDER_HAS_PDF_OPERATORS;

    if (XCB_RENDER_HAS_EXTENDED_REPEAT (version))
	connection->flags |= CAIRO_XCB_RENDER_HAS_EXTENDED_REPEAT;

    if (XCB_RENDER_HAS_GRADIENTS (version))
	connection->flags |= CAIRO_XCB_RENDER_HAS_GRADIENTS;

    if (XCB_RENDER_HAS_SUBPIXEL_ORDER (version)) {
	uint32_t screen;
	uint32_t *subpixel = xcb_render_query_pict_formats_subpixels(formats);

	/* The spec explicitly allows to have too few entries in the reply... */
	for (screen = 0; screen < formats->num_subpixel && screen < connection->root->roots_len; screen++)
	    connection->subpixel_orders[screen] = subpixel[screen];
    }

    free (version);

    status = _cairo_xcb_connection_parse_xrender_formats (connection, formats);
    free (formats);

    return status;
}

#if 0
static void
_cairo_xcb_connection_query_cairo (cairo_xcb_connection_t *connection)
{
    xcb_connection_t *c = connection->xcb_connection;
    xcb_cairo_query_version_reply_t *version;

    version = xcb_cairo_query_version_reply (c,
					     xcb_cairo_query_version (c, 0, 0),
					     0);

    free (version);
}
#endif

#if CAIRO_HAS_XCB_SHM_FUNCTIONS
static cairo_bool_t
can_use_shm (cairo_xcb_connection_t *connection)
{
    cairo_bool_t success = TRUE;
    xcb_connection_t *c = connection->xcb_connection;
    xcb_void_cookie_t cookie[2];
    xcb_generic_error_t *error;
    int shmid;
    uint32_t shmseg;
    void *ptr;

    shmid = shmget (IPC_PRIVATE, 0x1000, IPC_CREAT | 0600);
    if (shmid == -1)
	return FALSE;

    ptr = shmat (shmid, NULL, 0);
    if (ptr == (char *) -1) {
	shmctl (shmid, IPC_RMID, NULL);
	return FALSE;
    }

    shmseg = _cairo_xcb_connection_get_xid (connection);
    cookie[0] = xcb_shm_attach_checked (c, shmseg, shmid, FALSE);
    cookie[1] = xcb_shm_detach_checked (c, shmseg);
    _cairo_xcb_connection_put_xid (connection, shmseg);

    error = xcb_request_check (c, cookie[0]);
    if (error != NULL)
	success = FALSE;

    error = xcb_request_check (c, cookie[1]);
    if (error != NULL)
	success = FALSE;

    shmctl (shmid, IPC_RMID, NULL);
    shmdt (ptr);

    return success;
}

static void
_cairo_xcb_connection_query_shm (cairo_xcb_connection_t *connection)
{
    xcb_connection_t *c = connection->xcb_connection;
    xcb_shm_query_version_reply_t *version;

    version = xcb_shm_query_version_reply (c, xcb_shm_query_version (c), 0);
    if (version == NULL)
	return;

    free (version);

    if (can_use_shm (connection))
	connection->flags |= CAIRO_XCB_HAS_SHM;
}
#endif

static cairo_status_t
_device_flush (void *device)
{
    cairo_xcb_connection_t *connection = device;
    cairo_status_t status;

    status = cairo_device_acquire (&connection->device);
    if (unlikely (status))
	return status;

#if CAIRO_HAS_XCB_SHM_FUNCTIONS
    _cairo_xcb_connection_shm_mem_pools_flush (connection);
#endif

    xcb_flush (connection->xcb_connection);

    cairo_device_release (&connection->device);
    return CAIRO_STATUS_SUCCESS;
}

static void
_pluck_xrender_format (void *entry,
		       void *closure)
{
    _cairo_hash_table_remove (closure, entry);
    free (entry);
}

static void
_device_finish (void *device)
{
    cairo_xcb_connection_t *connection = device;
    cairo_bool_t was_cached = FALSE;

    if (! cairo_list_is_empty (&connection->link)) {
	CAIRO_MUTEX_LOCK (_cairo_xcb_connections_mutex);
	cairo_list_del (&connection->link);
	CAIRO_MUTEX_UNLOCK (_cairo_xcb_connections_mutex);
	was_cached = TRUE;
    }

    while (! cairo_list_is_empty (&connection->fonts)) {
	cairo_xcb_font_t *font;

	font = cairo_list_first_entry (&connection->fonts,
				       cairo_xcb_font_t,
				       link);
	_cairo_xcb_font_close (font);
    }

    while (! cairo_list_is_empty (&connection->screens)) {
	cairo_xcb_screen_t *screen;

	screen = cairo_list_first_entry (&connection->screens,
					 cairo_xcb_screen_t,
					 link);
	_cairo_xcb_screen_finish (screen);
    }

#if CAIRO_HAS_XCB_SHM_FUNCTIONS
    /* _cairo_xcb_screen_finish finishes surfaces. If any of those surfaces had
     * a fallback image, we might have done a SHM PutImage. */
    _cairo_xcb_connection_shm_mem_pools_flush (connection);
#endif

    if (was_cached)
	cairo_device_destroy (device);
}

static void
_device_destroy (void *device)
{
    cairo_xcb_connection_t *connection = device;

    _cairo_hash_table_foreach (connection->xrender_formats,
			       _pluck_xrender_format, connection->xrender_formats);
    _cairo_hash_table_destroy (connection->xrender_formats);

    _cairo_hash_table_foreach (connection->visual_to_xrender_format,
			       _pluck_xrender_format,
			       connection->visual_to_xrender_format);
    _cairo_hash_table_destroy (connection->visual_to_xrender_format);

#if CAIRO_HAS_XCB_SHM_FUNCTIONS
    _cairo_xcb_connection_shm_mem_pools_fini (connection);
#endif
    _cairo_freepool_fini (&connection->shm_info_freelist);

    _cairo_freepool_fini (&connection->xid_pool);

    CAIRO_MUTEX_FINI (connection->shm_mutex);
    CAIRO_MUTEX_FINI (connection->screens_mutex);

    free (connection->subpixel_orders);
    free (connection);
}

static const cairo_device_backend_t _cairo_xcb_device_backend = {
    CAIRO_DEVICE_TYPE_XCB,

    NULL, NULL, /* lock, unlock */

    _device_flush,
    _device_finish,
    _device_destroy,
};

cairo_xcb_connection_t *
_cairo_xcb_connection_get (xcb_connection_t *xcb_connection)
{
    cairo_xcb_connection_t *connection;
    const xcb_query_extension_reply_t *ext;
    cairo_status_t status;

    CAIRO_MUTEX_INITIALIZE ();

    CAIRO_MUTEX_LOCK (_cairo_xcb_connections_mutex);
    if (connections.next == NULL) {
	/* XXX _cairo_init () */
	cairo_list_init (&connections);
    }

    cairo_list_foreach_entry (connection,
			      cairo_xcb_connection_t,
			      &connections,
			      link)
    {
	if (connection->xcb_connection == xcb_connection) {
	    /* Maintain MRU order. */
	    if (connections.next != &connection->link)
		cairo_list_move (&connection->link, &connections);

	    goto unlock;
	}
    }

    connection = _cairo_malloc (sizeof (cairo_xcb_connection_t));
    if (unlikely (connection == NULL))
	goto unlock;

    _cairo_device_init (&connection->device, &_cairo_xcb_device_backend);

    connection->xcb_connection = xcb_connection;

    cairo_list_init (&connection->fonts);
    cairo_list_init (&connection->screens);
    cairo_list_init (&connection->link);
    connection->xrender_formats = _cairo_hash_table_create (NULL);
    if (connection->xrender_formats == NULL) {
	CAIRO_MUTEX_FINI (connection->device.mutex);
	free (connection);
	connection = NULL;
	goto unlock;
    }

    connection->visual_to_xrender_format = _cairo_hash_table_create (NULL);
    if (connection->visual_to_xrender_format == NULL) {
	_cairo_hash_table_destroy (connection->xrender_formats);
	CAIRO_MUTEX_FINI (connection->device.mutex);
	free (connection);
	connection = NULL;
	goto unlock;
    }

    cairo_list_init (&connection->free_xids);
    _cairo_freepool_init (&connection->xid_pool,
			  sizeof (cairo_xcb_xid_t));

    cairo_list_init (&connection->shm_pools);
    cairo_list_init (&connection->shm_pending);
    _cairo_freepool_init (&connection->shm_info_freelist,
			  sizeof (cairo_xcb_shm_info_t));

    connection->maximum_request_length =
	xcb_get_maximum_request_length (xcb_connection);

    CAIRO_MUTEX_INIT (connection->shm_mutex);
    CAIRO_MUTEX_INIT (connection->screens_mutex);

    CAIRO_MUTEX_LOCK (connection->device.mutex);

    connection->flags = 0;
    connection->force_precision = -1;

    xcb_prefetch_extension_data (xcb_connection, &xcb_big_requests_id);
    xcb_prefetch_extension_data (xcb_connection, &xcb_render_id);
#if CAIRO_HAS_XCB_SHM_FUNCTIONS
    xcb_prefetch_extension_data (xcb_connection, &xcb_shm_id);
#endif
#if 0
    xcb_prefetch_extension_data (xcb_connection, &xcb_cairo_id);
#endif

    xcb_prefetch_maximum_request_length (xcb_connection);

    connection->root = xcb_get_setup (xcb_connection);
    connection->render = NULL;
    connection->subpixel_orders = calloc (connection->root->roots_len, sizeof(*connection->subpixel_orders));
    if (unlikely (connection->subpixel_orders == NULL)) {
	CAIRO_MUTEX_UNLOCK (connection->device.mutex);
	_cairo_xcb_connection_destroy (connection);
	connection = NULL;
	goto unlock;
    }

    ext = xcb_get_extension_data (xcb_connection, &xcb_render_id);
    if (ext != NULL && ext->present) {
	status = _cairo_xcb_connection_query_render (connection);
	if (unlikely (status)) {
	    CAIRO_MUTEX_UNLOCK (connection->device.mutex);
	    _cairo_xcb_connection_destroy (connection);
	    connection = NULL;
	    goto unlock;
	}

	connection->render = ext;
    }

#if 0
    ext = xcb_get_extension_data (connection, &xcb_cairo_id);
    if (ext != NULL && ext->present)
	_cairo_xcb_connection_query_cairo (connection);
#endif

    connection->shm = NULL;
#if CAIRO_HAS_XCB_SHM_FUNCTIONS
    ext = xcb_get_extension_data (xcb_connection, &xcb_shm_id);
    if (ext != NULL && ext->present) {
	_cairo_xcb_connection_query_shm (connection);
	connection->shm = ext;
    }
#endif

    connection->original_flags = connection->flags;

    CAIRO_MUTEX_UNLOCK (connection->device.mutex);

    cairo_list_add (&connection->link, &connections);
unlock:
    CAIRO_MUTEX_UNLOCK (_cairo_xcb_connections_mutex);

    return connection;
}

xcb_render_pictformat_t
_cairo_xcb_connection_get_xrender_format (cairo_xcb_connection_t *connection,
					  pixman_format_code_t pixman_format)
{
    cairo_hash_entry_t key;
    cairo_xcb_xrender_format_t *format;

    key.hash = pixman_format;
    format = _cairo_hash_table_lookup (connection->xrender_formats, &key);
    return format ? format->xrender_format : XCB_NONE;
}

xcb_render_pictformat_t
_cairo_xcb_connection_get_xrender_format_for_visual (cairo_xcb_connection_t *connection,
						     const xcb_visualid_t visual)
{
    cairo_hash_entry_t key;
    cairo_xcb_xrender_format_t *format;

    key.hash = visual;
    format = _cairo_hash_table_lookup (connection->visual_to_xrender_format, &key);
    return format ? format->xrender_format : XCB_NONE;
}

void
_cairo_xcb_connection_put_xid (cairo_xcb_connection_t *connection,
			       uint32_t xid)
{
    cairo_xcb_xid_t *cache;

    assert (CAIRO_MUTEX_IS_LOCKED (connection->device.mutex));
    cache = _cairo_freepool_alloc (&connection->xid_pool);
    if (likely (cache != NULL)) {
	cache->xid = xid;
	cairo_list_add (&cache->link, &connection->free_xids);
    }
}

uint32_t
_cairo_xcb_connection_get_xid (cairo_xcb_connection_t *connection)
{
    uint32_t xid;

    assert (CAIRO_MUTEX_IS_LOCKED (connection->device.mutex));
    if (! cairo_list_is_empty (&connection->free_xids)) {
	cairo_xcb_xid_t *cache;

	cache = cairo_list_first_entry (&connection->free_xids,
					cairo_xcb_xid_t,
					link);
	xid = cache->xid;

	cairo_list_del (&cache->link);
	_cairo_freepool_free (&connection->xid_pool, cache);
    } else {
	xid = xcb_generate_id (connection->xcb_connection);
    }

    return xid;
}

/**
 * cairo_xcb_device_get_connection:
 * @device: a #cairo_device_t for the XCB backend
 *
 * Get the connection for the XCB device.
 *
 * Returns: the #xcb_connection_t for the connection
 *
 * Since: 1.12
 **/
xcb_connection_t *
cairo_xcb_device_get_connection (cairo_device_t *device)
{
    if (device->backend->type != CAIRO_DEVICE_TYPE_XCB)
	    return NULL;

    return ((cairo_xcb_connection_t *)device)->xcb_connection;
}

/* public (debug) interface */

/**
 * cairo_xcb_device_debug_cap_xshm_version:
 * @device: a #cairo_device_t for the XCB backend
 * @major_version: major version to restrict to
 * @minor_version: minor version to restrict to
 *
 * Restricts all future XCB surfaces for this devices to the specified version
 * of the SHM extension. This function exists solely for debugging purpose.
 * It let's you find out how cairo would behave with an older version of
 * the SHM extension.
 *
 * Use the special values -1 and -1 for disabling the SHM extension.
 *
 * Since: 1.12
 **/
void
cairo_xcb_device_debug_cap_xshm_version (cairo_device_t *device,
                                         int major_version,
                                         int minor_version)
{
    cairo_xcb_connection_t *connection = (cairo_xcb_connection_t *) device;

    if (device->backend->type != CAIRO_DEVICE_TYPE_XCB) {
	cairo_status_t status;

	status = _cairo_device_set_error (device, CAIRO_STATUS_DEVICE_TYPE_MISMATCH);
	(void) status;
	return;
    }

    /* First reset all the SHM flags to their original value. This works
     * because we only ever clear bits after the connection was created.
     */
    connection->flags |= (connection->original_flags & CAIRO_XCB_SHM_MASK);

    /* clear any flags that are inappropriate for the desired version */
    if (major_version < 0 && minor_version < 0) {
	connection->flags &= ~(CAIRO_XCB_HAS_SHM);
    }
}

/**
 * cairo_xcb_device_debug_cap_xrender_version:
 * @device: a #cairo_device_t for the XCB backend
 * @major_version: major version to restrict to
 * @minor_version: minor version to restrict to
 *
 * Restricts all future XCB surfaces for this devices to the specified version
 * of the RENDER extension. This function exists solely for debugging purpose.
 * It let's you find out how cairo would behave with an older version of
 * the RENDER extension.
 *
 * Use the special values -1 and -1 for disabling the RENDER extension.
 *
 * Since: 1.12
 **/
void
cairo_xcb_device_debug_cap_xrender_version (cairo_device_t *device,
                                            int major_version,
                                            int minor_version)
{
    cairo_xcb_connection_t *connection = (cairo_xcb_connection_t *) device;

    if (device->backend->type != CAIRO_DEVICE_TYPE_XCB) {
	cairo_status_t status;

	status = _cairo_device_set_error (device, CAIRO_STATUS_DEVICE_TYPE_MISMATCH);
	(void) status;
	return;
    }

    /* First reset all the RENDER flags to their original value. This works
     * because we only ever clear bits after the connection was created.
     */
    connection->flags |= (connection->original_flags & CAIRO_XCB_RENDER_MASK);

    /* clear any flags that are inappropriate for the desired version */
    if (major_version < 0 && minor_version < 0) {
	connection->flags &= ~(CAIRO_XCB_HAS_RENDER |
			       CAIRO_XCB_RENDER_HAS_COMPOSITE |
			       CAIRO_XCB_RENDER_HAS_COMPOSITE_GLYPHS |
			       CAIRO_XCB_RENDER_HAS_FILL_RECTANGLES |
			       CAIRO_XCB_RENDER_HAS_COMPOSITE_TRAPEZOIDS |
			       CAIRO_XCB_RENDER_HAS_PICTURE_TRANSFORM |
			       CAIRO_XCB_RENDER_HAS_FILTERS |
			       CAIRO_XCB_RENDER_HAS_FILTER_GOOD |
			       CAIRO_XCB_RENDER_HAS_FILTER_BEST |
			       CAIRO_XCB_RENDER_HAS_PDF_OPERATORS |
			       CAIRO_XCB_RENDER_HAS_EXTENDED_REPEAT |
			       CAIRO_XCB_RENDER_HAS_GRADIENTS);
    } else {
	xcb_render_query_version_reply_t version;

	version.major_version = major_version;
	version.minor_version = minor_version;

	if (! XCB_RENDER_HAS_FILL_RECTANGLES (&version))
	    connection->flags &= ~CAIRO_XCB_RENDER_HAS_FILL_RECTANGLES;

	if (! XCB_RENDER_HAS_TRAPEZOIDS (&version))
	    connection->flags &= ~CAIRO_XCB_RENDER_HAS_COMPOSITE_TRAPEZOIDS;

	if (! XCB_RENDER_HAS_PICTURE_TRANSFORM (&version))
	    connection->flags &= ~CAIRO_XCB_RENDER_HAS_PICTURE_TRANSFORM;

	if (! XCB_RENDER_HAS_FILTERS (&version))
	    connection->flags &= ~CAIRO_XCB_RENDER_HAS_FILTERS;

	if (! XCB_RENDER_HAS_PDF_OPERATORS (&version))
	    connection->flags &= ~CAIRO_XCB_RENDER_HAS_PDF_OPERATORS;

	if (! XCB_RENDER_HAS_EXTENDED_REPEAT (&version))
	    connection->flags &= ~CAIRO_XCB_RENDER_HAS_EXTENDED_REPEAT;

	if (! XCB_RENDER_HAS_GRADIENTS (&version))
	    connection->flags &= ~CAIRO_XCB_RENDER_HAS_GRADIENTS;
    }
}
#if CAIRO_HAS_XLIB_XCB_FUNCTIONS
slim_hidden_def (cairo_xcb_device_debug_cap_xrender_version);
#endif

/**
 * cairo_xcb_device_debug_set_precision:
 * @device: a #cairo_device_t for the XCB backend
 * @precision: the precision to use
 *
 * Render supports two modes of precision when rendering trapezoids. Set
 * the precision to the desired mode.
 *
 * Since: 1.12
 **/
void
cairo_xcb_device_debug_set_precision (cairo_device_t *device,
				      int precision)
{
    if (device == NULL || device->status)
	return;
    if (device->backend->type != CAIRO_DEVICE_TYPE_XCB) {
	cairo_status_t status;

	status = _cairo_device_set_error (device, CAIRO_STATUS_DEVICE_TYPE_MISMATCH);
	(void) status;
	return;
    }

    ((cairo_xcb_connection_t *) device)->force_precision = precision;
}
#if CAIRO_HAS_XLIB_XCB_FUNCTIONS
slim_hidden_def (cairo_xcb_device_debug_set_precision);
#endif

/**
 * cairo_xcb_device_debug_get_precision:
 * @device: a #cairo_device_t for the XCB backend
 *
 * Get the Xrender precision mode.
 *
 * Returns: the render precision mode
 *
 * Since: 1.12
 **/
int
cairo_xcb_device_debug_get_precision (cairo_device_t *device)
{
    if (device == NULL || device->status)
	return -1;
    if (device->backend->type != CAIRO_DEVICE_TYPE_XCB) {
	cairo_status_t status;

	status = _cairo_device_set_error (device, CAIRO_STATUS_DEVICE_TYPE_MISMATCH);
	(void) status;
	return -1;
    }

    return ((cairo_xcb_connection_t *) device)->force_precision;
}
#if CAIRO_HAS_XLIB_XCB_FUNCTIONS
slim_hidden_def (cairo_xcb_device_debug_get_precision);
#endif
