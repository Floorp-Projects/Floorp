/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc
 * Copyright © 2005-2006 Emmanuel Pacaud <emmanuel.pacaud@free.fr>
 * Copyright © 2006 Red Hat, Inc
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Kristian Høgsberg <krh@redhat.com>
 * 	Emmanuel Pacaud <emmanuel.pacaud@univ-poitiers.fr>
 *	Carl Worth <cworth@cworth.org>
 */

#ifndef CAIRO_SVG_SURFACE_PRIVATE_H
#define CAIRO_SVG_SURFACE_PRIVATE_H

#include "cairo-svg.h"

#include "cairo-surface-private.h"

typedef struct cairo_svg_document cairo_svg_document_t;

typedef struct cairo_svg_surface {
    cairo_surface_t base;

    cairo_content_t content;

    unsigned int id;

    double width;
    double height;

    cairo_svg_document_t *document;

    cairo_output_stream_t *xml_node;
    cairo_array_t	   page_set;

    unsigned int clip_level;
    unsigned int base_clip;
    cairo_bool_t is_base_clip_emitted;

    cairo_paginated_mode_t paginated_mode;

    cairo_bool_t force_fallbacks;
} cairo_svg_surface_t;

#endif /* CAIRO_SVG_SURFACE_PRIVATE_H */
