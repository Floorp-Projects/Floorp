/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2014 Lukas Lalinsky
 * Copyright © 2005 Red Hat, Inc.
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
 *    Lukas Lalinsky <lukas@oxygene.sk>
 *
 * Partially on code from xftdpy.c
 *
 * Copyright © 2000 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "cairoint.h"

#include "cairo-xcb-private.h"

#include "cairo-fontconfig-private.h"

static void
parse_boolean (const char *v, cairo_bool_t *out)
{
    char c0, c1;

    c0 = *v;
    if (c0 == 't' || c0 == 'T' || c0 == 'y' || c0 == 'Y' || c0 == '1')
	*out = TRUE;
    if (c0 == 'f' || c0 == 'F' || c0 == 'n' || c0 == 'N' || c0 == '0')
	*out = FALSE;
    if (c0 == 'o') {
	c1 = v[1];
	if (c1 == 'n' || c1 == 'N')
	    *out = TRUE;
	if (c1 == 'f' || c1 == 'F')
	    *out = FALSE;
    }
}

static void
parse_integer (const char *v, int *out)
{
    char *e;
    int value;

#if CAIRO_HAS_FC_FONT
    if (FcNameConstant ((FcChar8 *) v, out))
	return;
#endif

    value = strtol (v, &e, 0);
    if (e != v)
	*out = value;
}

static char *
skip_spaces(char *str)
{
    while (*str == ' ' || *str == '\t' || *str == '\n')
	str++;
    return str;
}

struct resource_parser {
    int buffer_size;
    int bytes_in_buffer;
    char* buffer;
    cairo_xcb_resources_t *resources;
};

static cairo_bool_t
resource_parse_line (char *name, cairo_xcb_resources_t *resources)
{
    char *value;

    value = strchr (name, ':');
    if (value == NULL)
	return FALSE;

    *value++ = 0;

    name = skip_spaces (name);
    value = skip_spaces (value);

    if (strcmp (name, "Xft.antialias") == 0)
	parse_boolean (value, &(resources->xft_antialias));
    else if (strcmp (name, "Xft.lcdfilter") == 0)
	parse_integer (value, &(resources->xft_lcdfilter));
    else if (strcmp (name, "Xft.rgba") == 0)
	parse_integer (value, &(resources->xft_rgba));
    else if (strcmp (name, "Xft.hinting") == 0)
	parse_boolean (value, &(resources->xft_hinting));
    else if (strcmp (name, "Xft.hintstyle") == 0)
	parse_integer (value, &(resources->xft_hintstyle));

    return TRUE;
}

static int
resource_parse_lines (struct resource_parser *parser)
{
    char *line, *newline;

    line = parser->buffer;
    while (1) {
	newline = strchr (line, '\n');
	if (newline == NULL)
	    break;

	*newline++ = 0;

	if (! resource_parse_line (line, parser->resources))
	    break;

	line = newline;
    }

    return line - parser->buffer;
}

static void
resource_parser_init (struct resource_parser *parser, cairo_xcb_resources_t *resources)
{
    parser->buffer_size = 0;
    parser->bytes_in_buffer = 0;
    parser->buffer = NULL;
    parser->resources = resources;
}

static cairo_bool_t
resource_parser_update (struct resource_parser *parser, const char *data, int length)
{
    int bytes_parsed;

    if (parser->bytes_in_buffer + length + 1 > parser->buffer_size) {
	parser->buffer_size = parser->bytes_in_buffer + length + 1;
	parser->buffer = realloc(parser->buffer, parser->buffer_size);
	if (! parser->buffer) {
	    parser->buffer_size = 0;
	    parser->bytes_in_buffer = 0;
	    return FALSE;
	}
    }

    memmove (parser->buffer + parser->bytes_in_buffer, data, length);
    parser->bytes_in_buffer += length;
    parser->buffer[parser->bytes_in_buffer] = 0;

    bytes_parsed = resource_parse_lines (parser);

    if (parser->bytes_in_buffer > bytes_parsed) {
	memmove (parser->buffer, parser->buffer + bytes_parsed, parser->bytes_in_buffer - bytes_parsed);
	parser->bytes_in_buffer -= bytes_parsed;
    } else {
	parser->bytes_in_buffer = 0;
    }

    return TRUE;
}

static void
resource_parser_done (struct resource_parser *parser)
{
    if (parser->bytes_in_buffer > 0) {
	parser->buffer[parser->bytes_in_buffer] = 0;
	resource_parse_line (parser->buffer, parser->resources);
    }

    free (parser->buffer);
}

static void
get_resources(xcb_connection_t *connection, xcb_screen_t *screen, cairo_xcb_resources_t *resources)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    struct resource_parser parser;
    int offset;
    cairo_bool_t has_more_data;

    resources->xft_antialias = TRUE;
    resources->xft_lcdfilter = -1;
    resources->xft_hinting = TRUE;
    resources->xft_hintstyle = FC_HINT_FULL;
    resources->xft_rgba = FC_RGBA_UNKNOWN;

    resource_parser_init (&parser, resources);

    offset = 0;
    has_more_data = FALSE;
    do {
	cookie = xcb_get_property (connection, 0, screen->root, XCB_ATOM_RESOURCE_MANAGER, XCB_ATOM_STRING, offset, 1024);
	reply = xcb_get_property_reply (connection, cookie, NULL);

	if (reply) {
	    if (reply->format == 8 && reply->type == XCB_ATOM_STRING) {
		char *value = (char *) xcb_get_property_value (reply);
		int length = xcb_get_property_value_length (reply);

		offset += length / 4; /* X needs the offset in 'long' units */
		has_more_data = reply->bytes_after > 0;

		if (! resource_parser_update (&parser, value, length))
		    has_more_data = FALSE; /* early exit on error */
	    }

	    free (reply);
	}
    } while (has_more_data);

    resource_parser_done (&parser);
}

void
_cairo_xcb_resources_get (cairo_xcb_screen_t *screen, cairo_xcb_resources_t *resources)
{
    get_resources (screen->connection->xcb_connection, screen->xcb_screen, resources);

    if (resources->xft_rgba == FC_RGBA_UNKNOWN) {
	switch (screen->subpixel_order) {
	case XCB_RENDER_SUB_PIXEL_UNKNOWN:
	    resources->xft_rgba = FC_RGBA_UNKNOWN;
	    break;
	case XCB_RENDER_SUB_PIXEL_HORIZONTAL_RGB:
	    resources->xft_rgba = FC_RGBA_RGB;
	    break;
	case XCB_RENDER_SUB_PIXEL_HORIZONTAL_BGR:
	    resources->xft_rgba = FC_RGBA_BGR;
	    break;
	case XCB_RENDER_SUB_PIXEL_VERTICAL_RGB:
	    resources->xft_rgba = FC_RGBA_VRGB;
	    break;
	case XCB_RENDER_SUB_PIXEL_VERTICAL_BGR:
	    resources->xft_rgba = FC_RGBA_VBGR;
	    break;
	case XCB_RENDER_SUB_PIXEL_NONE:
	    resources->xft_rgba = FC_RGBA_NONE;
	    break;
	}
    }
}
