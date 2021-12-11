/* cairo - a vector graphics library with display and print output
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
 * Contributor(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#if CAIRO_HAS_XCB_SHM_FUNCTIONS

#include "cairo-xcb-private.h"

#include <xcb/xcbext.h>
#include <xcb/shm.h>

uint32_t
_cairo_xcb_connection_shm_attach (cairo_xcb_connection_t *connection,
				  uint32_t id,
				  cairo_bool_t readonly)
{
    uint32_t segment = _cairo_xcb_connection_get_xid (connection);
    assert (connection->flags & CAIRO_XCB_HAS_SHM);
    xcb_shm_attach (connection->xcb_connection, segment, id, readonly);
    return segment;
}

void
_cairo_xcb_connection_shm_put_image (cairo_xcb_connection_t *connection,
				     xcb_drawable_t dst,
				     xcb_gcontext_t gc,
				     uint16_t total_width,
				     uint16_t total_height,
				     int16_t src_x,
				     int16_t src_y,
				     uint16_t width,
				     uint16_t height,
				     int16_t dst_x,
				     int16_t dst_y,
				     uint8_t depth,
				     uint32_t shm,
				     uint32_t offset)
{
    assert (connection->flags & CAIRO_XCB_HAS_SHM);
    xcb_shm_put_image (connection->xcb_connection, dst, gc, total_width, total_height,
		       src_x, src_y, width, height, dst_x, dst_y, depth,
		       XCB_IMAGE_FORMAT_Z_PIXMAP, 0, shm, offset);
}

cairo_status_t
_cairo_xcb_connection_shm_get_image (cairo_xcb_connection_t *connection,
				     xcb_drawable_t src,
				     int16_t src_x,
				     int16_t src_y,
				     uint16_t width,
				     uint16_t height,
				     uint32_t shmseg,
				     uint32_t offset)
{
    xcb_shm_get_image_reply_t *reply;

    assert (connection->flags & CAIRO_XCB_HAS_SHM);
    reply = xcb_shm_get_image_reply (connection->xcb_connection,
				     xcb_shm_get_image (connection->xcb_connection,
							src,
							src_x, src_y,
							width, height,
							(uint32_t) -1,
							XCB_IMAGE_FORMAT_Z_PIXMAP,
							shmseg, offset),
				     NULL);
    free (reply);

    if (!reply) {
	/* an error here should be impossible */
	return _cairo_error (CAIRO_STATUS_READ_ERROR);
    }

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_xcb_connection_shm_detach (cairo_xcb_connection_t *connection,
				  uint32_t segment)
{
    assert (connection->flags & CAIRO_XCB_HAS_SHM);
    xcb_shm_detach (connection->xcb_connection, segment);
    _cairo_xcb_connection_put_xid (connection, segment);
}

#endif /* CAIRO_HAS_XCB_SHM_FUNCTIONS */
