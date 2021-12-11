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

#include "cairo-xcb-private.h"

#include <xcb/xcbext.h>

xcb_pixmap_t
_cairo_xcb_connection_create_pixmap (cairo_xcb_connection_t *connection,
				     uint8_t depth,
				     xcb_drawable_t drawable,
				     uint16_t width,
				     uint16_t height)
{
    xcb_pixmap_t pixmap = _cairo_xcb_connection_get_xid (connection);

    assert (width > 0);
    assert (height > 0);
    xcb_create_pixmap (connection->xcb_connection,
		       depth, pixmap, drawable,
		       width, height);
    return pixmap;
}

void
_cairo_xcb_connection_free_pixmap (cairo_xcb_connection_t *connection,
				   xcb_pixmap_t pixmap)
{
    xcb_free_pixmap (connection->xcb_connection, pixmap);
    _cairo_xcb_connection_put_xid (connection, pixmap);
}

xcb_gcontext_t
_cairo_xcb_connection_create_gc (cairo_xcb_connection_t *connection,
				 xcb_drawable_t drawable,
				 uint32_t value_mask,
				 uint32_t *values)
{
    xcb_gcontext_t gc = _cairo_xcb_connection_get_xid (connection);
    xcb_create_gc (connection->xcb_connection, gc, drawable,
		   value_mask, values);
    return gc;
}

void
_cairo_xcb_connection_free_gc (cairo_xcb_connection_t *connection,
			       xcb_gcontext_t gc)
{
    xcb_free_gc (connection->xcb_connection, gc);
    _cairo_xcb_connection_put_xid (connection, gc);
}

void
_cairo_xcb_connection_change_gc (cairo_xcb_connection_t *connection,
				 xcb_gcontext_t gc,
				 uint32_t value_mask,
				 uint32_t *values)
{
    xcb_change_gc (connection->xcb_connection, gc,
		   value_mask, values);
}

void
_cairo_xcb_connection_copy_area (cairo_xcb_connection_t *connection,
				 xcb_drawable_t src,
				 xcb_drawable_t dst,
				 xcb_gcontext_t gc,
				 int16_t src_x,
				 int16_t src_y,
				 int16_t dst_x,
				 int16_t dst_y,
				 uint16_t width,
				 uint16_t height)
{
    xcb_copy_area (connection->xcb_connection, src, dst, gc,
		   src_x, src_y, dst_x, dst_y, width, height);
}

void
_cairo_xcb_connection_poly_fill_rectangle (cairo_xcb_connection_t *connection,
					   xcb_drawable_t dst,
					   xcb_gcontext_t gc,
					   uint32_t num_rectangles,
					   xcb_rectangle_t *rectangles)
{
    xcb_poly_fill_rectangle (connection->xcb_connection, dst, gc,
			     num_rectangles, rectangles);
}

void
_cairo_xcb_connection_put_image (cairo_xcb_connection_t *connection,
				 xcb_drawable_t dst,
				 xcb_gcontext_t gc,
				 uint16_t width,
				 uint16_t height,
				 int16_t dst_x,
				 int16_t dst_y,
				 uint8_t depth,
				 uint32_t stride,
				 void *data)
{
    const uint32_t req_size = 18;
    uint32_t length = height * stride;
    uint32_t len = (req_size + length) >> 2;

    if (len < connection->maximum_request_length) {
	xcb_put_image (connection->xcb_connection, XCB_IMAGE_FORMAT_Z_PIXMAP,
		       dst, gc, width, height, dst_x, dst_y, 0, depth,
		       length, data);
    } else {
	int rows = (connection->maximum_request_length - req_size - 4) / stride;
	if (rows > 0) {
	    do {
		if (rows > height)
		    rows = height;

		length = rows * stride;

		xcb_put_image (connection->xcb_connection, XCB_IMAGE_FORMAT_Z_PIXMAP,
			       dst, gc, width, rows, dst_x, dst_y, 0, depth, length, data);

		height -= rows;
		dst_y += rows;
		data = (char *) data + length;
	    } while (height);
	} else {
	    ASSERT_NOT_REACHED;
	}
    }
}

static void
_cairo_xcb_connection_do_put_subimage (cairo_xcb_connection_t *connection,
				       xcb_drawable_t dst,
				       xcb_gcontext_t gc,
				       int16_t src_x,
				       int16_t src_y,
				       uint16_t width,
				       uint16_t height,
				       uint16_t cpp,
				       int stride,
				       int16_t dst_x,
				       int16_t dst_y,
				       uint8_t depth,
				       void *_data)
{
    xcb_protocol_request_t xcb_req = {
	0 /* count */,
	0 /* ext */,
	XCB_PUT_IMAGE /* opcode */,
	1 /* isvoid (doesn't cause a reply) */
    };
    xcb_put_image_request_t req;
    struct iovec vec_stack[CAIRO_STACK_ARRAY_LENGTH (struct iovec)];
    struct iovec *vec = vec_stack;
    uint32_t len = 0;
    uint8_t *data = _data;
    int n = 3;
    /* Two extra entries are needed for xcb, two for us */
    int entries_needed = height + 2 + 2;

    req.format = XCB_IMAGE_FORMAT_Z_PIXMAP;
    req.drawable = dst;
    req.gc = gc;
    req.width = width;
    req.height = height;
    req.dst_x = dst_x;
    req.dst_y = dst_y;
    req.left_pad = 0;
    req.depth = depth;
    req.pad0[0] = 0;
    req.pad0[1] = 0;

    if (entries_needed > ARRAY_LENGTH (vec_stack)) {
	vec = _cairo_malloc_ab (entries_needed, sizeof (struct iovec));
	if (unlikely (vec == NULL)) {
	    /* XXX loop over ARRAY_LENGTH (vec_stack) */
	    return;
	}
    }

    data += src_y * stride + src_x * cpp;
    /* vec[1] will be used in XCB if it has to use BigRequests or insert a sync,
     * vec[0] is used if the internal queue needs to be flushed. */
    vec[2].iov_base = (char *) &req;
    vec[2].iov_len = sizeof (req);

    /* Now comes the actual data */
    while (height--) {
	vec[n].iov_base = data;
	vec[n].iov_len = cpp * width;
	len += cpp * width;
	data += stride;
	n++;
    }

    /* And again some padding */
    vec[n].iov_base = 0;
    vec[n].iov_len = -len & 3;
    n++;

    /* For efficiency reasons, this functions writes the request "directly" to
     * the xcb connection to avoid having to copy the data around. */
    assert (n == entries_needed);
    xcb_req.count = n - 2;
    xcb_send_request (connection->xcb_connection, 0, &vec[2], &xcb_req);

    if (vec != vec_stack)
	free (vec);
}

void
_cairo_xcb_connection_put_subimage (cairo_xcb_connection_t *connection,
				    xcb_drawable_t dst,
				    xcb_gcontext_t gc,
				    int16_t src_x,
				    int16_t src_y,
				    uint16_t width,
				    uint16_t height,
				    uint16_t cpp,
				    int stride,
				    int16_t dst_x,
				    int16_t dst_y,
				    uint8_t depth,
				    void *_data)
{
    const uint32_t req_size = sizeof(xcb_put_image_request_t);
    uint32_t length = height * cpp * width;
    uint32_t len = (req_size + length) >> 2;

    if (len < connection->maximum_request_length) {
	_cairo_xcb_connection_do_put_subimage (connection, dst, gc, src_x, src_y,
			width, height, cpp, stride, dst_x, dst_y, depth, _data);
    } else {
	int rows = (connection->maximum_request_length - req_size - 4) / (cpp * width);
	if (rows > 0) {
	    do {
		if (rows > height)
		    rows = height;

		_cairo_xcb_connection_do_put_subimage (connection, dst, gc, src_x, src_y,
			width, rows, cpp, stride, dst_x, dst_y, depth, _data);

		height -= rows;
		dst_y += rows;
		_data = (char *) _data + stride * rows;
	    } while (height);
	} else {
	    ASSERT_NOT_REACHED;
	}
    }
}

xcb_get_image_reply_t *
_cairo_xcb_connection_get_image (cairo_xcb_connection_t *connection,
				 xcb_drawable_t src,
				 int16_t src_x,
				 int16_t src_y,
				 uint16_t width,
				 uint16_t height)
{
    return xcb_get_image_reply (connection->xcb_connection,
				xcb_get_image (connection->xcb_connection,
					       XCB_IMAGE_FORMAT_Z_PIXMAP,
					       src,
					       src_x, src_y,
					       width, height,
					       (uint32_t) -1),
				NULL);
}
