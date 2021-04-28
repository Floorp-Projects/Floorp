/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
 * Copyright © 2005 Red Hat, Inc
 * Copyright © 2007,2008 Adrian Johnson
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
 *	Carl D. Worth <cworth@cworth.org>
 *	Kristian Høgsberg <krh@redhat.com>
 *	Keith Packard <keithp@keithp.com>
 *	Adrian Johnson <ajohnson@redneon.com>
 */


/*
 * Design of the PS output:
 *
 * The PS output is harmonised with the PDF operations using PS procedures
 * to emulate the PDF operators.
 *
 * This has a number of advantages:
 *   1. A large chunk of code is shared between the PDF and PS backends.
 *      See cairo-pdf-operators.
 *   2. Using gs to do PS -> PDF and PDF -> PS will always work well.
 */

#define _BSD_SOURCE /* for ctime_r(), snprintf(), strdup() */
#include "cairoint.h"
#include "cairo-ps.h"
#include "cairo-ps-surface-private.h"
#include "cairo-pdf-operators-private.h"
#include "cairo-composite-rectangles-private.h"
#include "cairo-error-private.h"
#include "cairo-scaled-font-subsets-private.h"
#include "cairo-paginated-private.h"
#include "cairo-recording-surface-private.h"
#include "cairo-surface-clipper-private.h"
#include "cairo-surface-subsurface-private.h"
#include "cairo-output-stream-private.h"
#include "cairo-type3-glyph-surface-private.h"
#include "cairo-image-info-private.h"

#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <zlib.h>
#include <errno.h>

#define DEBUG_PS 0

#if DEBUG_PS
#define DEBUG_FALLBACK(s) \
    fprintf (stderr, "%s::%d -- %s\n", __FUNCTION__, __LINE__, (s))
#else
#define DEBUG_FALLBACK(s)
#endif

#ifndef HAVE_CTIME_R
#define ctime_r(T, BUF) ctime (T)
#endif

/**
 * SECTION:cairo-ps
 * @Title: PostScript Surfaces
 * @Short_Description: Rendering PostScript documents
 * @See_Also: #cairo_surface_t
 *
 * The PostScript surface is used to render cairo graphics to Adobe
 * PostScript files and is a multi-page vector surface backend.
 */

/**
 * CAIRO_HAS_PS_SURFACE:
 * 
 * Defined if the PostScript surface backend is available.
 * This macro can be used to conditionally compile backend-specific code.
 */

static const cairo_surface_backend_t cairo_ps_surface_backend;
static const cairo_paginated_surface_backend_t cairo_ps_surface_paginated_backend;

static void
_cairo_ps_surface_release_surface (cairo_ps_surface_t      *surface,
				   cairo_surface_pattern_t *pattern);

static const cairo_ps_level_t _cairo_ps_levels[] =
{
    CAIRO_PS_LEVEL_2,
    CAIRO_PS_LEVEL_3
};

#define CAIRO_PS_LEVEL_LAST ARRAY_LENGTH (_cairo_ps_levels)

static const char * _cairo_ps_level_strings[CAIRO_PS_LEVEL_LAST] =
{
    "PS Level 2",
    "PS Level 3"
};

typedef struct _cairo_page_standard_media {
    const char *name;
    int width;
    int height;
} cairo_page_standard_media_t;

static const cairo_page_standard_media_t _cairo_page_standard_media[] =
{
    { "A0",       2384, 3371 },
    { "A1",       1685, 2384 },
    { "A2",       1190, 1684 },
    { "A3",        842, 1190 },
    { "A4",        595,  842 },
    { "A5",        420,  595 },
    { "B4",        729, 1032 },
    { "B5",        516,  729 },
    { "Letter",    612,  792 },
    { "Tabloid",   792, 1224 },
    { "Ledger",   1224,  792 },
    { "Legal",     612, 1008 },
    { "Statement", 396,  612 },
    { "Executive", 540,  720 },
    { "Folio",     612,  936 },
    { "Quarto",    610,  780 },
    { "10x14",     720, 1008 },
};

typedef struct _cairo_page_media {
    char *name;
    int width;
    int height;
    cairo_list_t link;
} cairo_page_media_t;

static void
_cairo_ps_surface_emit_header (cairo_ps_surface_t *surface)
{
    char ctime_buf[26];
    time_t now;
    char **comments;
    int i, num_comments;
    int level;
    const char *eps_header = "";

    if (surface->has_creation_date)
	now = surface->creation_date;
    else
	now = time (NULL);

    if (surface->ps_level_used == CAIRO_PS_LEVEL_2)
	level = 2;
    else
	level = 3;

    if (surface->eps)
	eps_header = " EPSF-3.0";

    _cairo_output_stream_printf (surface->final_stream,
				 "%%!PS-Adobe-3.0%s\n"
				 "%%%%Creator: cairo %s (http://cairographics.org)\n"
				 "%%%%CreationDate: %s"
				 "%%%%Pages: %d\n"
				 "%%%%BoundingBox: %d %d %d %d\n",
				 eps_header,
				 cairo_version_string (),
				 ctime_r (&now, ctime_buf),
				 surface->num_pages,
				 surface->bbox_x1,
				 surface->bbox_y1,
				 surface->bbox_x2,
				 surface->bbox_y2);

    _cairo_output_stream_printf (surface->final_stream,
				 "%%%%DocumentData: Clean7Bit\n"
				 "%%%%LanguageLevel: %d\n",
				 level);

    if (!cairo_list_is_empty (&surface->document_media)) {
	cairo_page_media_t *page;
	cairo_bool_t first = TRUE;

	cairo_list_foreach_entry (page, cairo_page_media_t, &surface->document_media, link) {
	    if (first) {
		_cairo_output_stream_printf (surface->final_stream,
					     "%%%%DocumentMedia: ");
		first = FALSE;
	    } else {
		_cairo_output_stream_printf (surface->final_stream,
					     "%%%%+ ");
	    }
	    _cairo_output_stream_printf (surface->final_stream,
					 "%s %d %d 0 () ()\n",
					 page->name,
					 page->width,
					 page->height);
	}
    }

    num_comments = _cairo_array_num_elements (&surface->dsc_header_comments);
    comments = _cairo_array_index (&surface->dsc_header_comments, 0);
    for (i = 0; i < num_comments; i++) {
	_cairo_output_stream_printf (surface->final_stream,
				     "%s\n", comments[i]);
	free (comments[i]);
	comments[i] = NULL;
    }

    _cairo_output_stream_printf (surface->final_stream,
				 "%%%%EndComments\n");

    _cairo_output_stream_printf (surface->final_stream,
				 "%%%%BeginProlog\n");

    if (surface->eps) {
	_cairo_output_stream_printf (surface->final_stream,
				     "/cairo_eps_state save def\n"
				     "/dict_count countdictstack def\n"
				     "/op_count count 1 sub def\n"
				     "userdict begin\n");
    } else {
	_cairo_output_stream_printf (surface->final_stream,
				     "/languagelevel where\n"
				     "{ pop languagelevel } { 1 } ifelse\n"
				     "%d lt { /Helvetica findfont 12 scalefont setfont 50 500 moveto\n"
				     "  (This print job requires a PostScript Language Level %d printer.) show\n"
				     "  showpage quit } if\n",
				     level,
				     level);
    }

    _cairo_output_stream_printf (surface->final_stream,
				 "/q { gsave } bind def\n"
				 "/Q { grestore } bind def\n"
				 "/cm { 6 array astore concat } bind def\n"
				 "/w { setlinewidth } bind def\n"
				 "/J { setlinecap } bind def\n"
				 "/j { setlinejoin } bind def\n"
				 "/M { setmiterlimit } bind def\n"
				 "/d { setdash } bind def\n"
				 "/m { moveto } bind def\n"
				 "/l { lineto } bind def\n"
				 "/c { curveto } bind def\n"
				 "/h { closepath } bind def\n"
				 "/re { exch dup neg 3 1 roll 5 3 roll moveto 0 rlineto\n"
				 "      0 exch rlineto 0 rlineto closepath } bind def\n"
				 "/S { stroke } bind def\n"
				 "/f { fill } bind def\n"
				 "/f* { eofill } bind def\n"
				 "/n { newpath } bind def\n"
				 "/W { clip } bind def\n"
				 "/W* { eoclip } bind def\n"
				 "/BT { } bind def\n"
				 "/ET { } bind def\n"
				 "/pdfmark where { pop globaldict /?pdfmark /exec load put }\n"
				 "    { globaldict begin /?pdfmark /pop load def /pdfmark\n"
				 "    /cleartomark load def end } ifelse\n"
				 "/BDC { mark 3 1 roll /BDC pdfmark } bind def\n"
				 "/EMC { mark /EMC pdfmark } bind def\n"
				 "/cairo_store_point { /cairo_point_y exch def /cairo_point_x exch def } def\n"
				 "/Tj { show currentpoint cairo_store_point } bind def\n"
				 "/TJ {\n"
				 "  {\n"
				 "    dup\n"
				 "    type /stringtype eq\n"
				 "    { show } { -0.001 mul 0 cairo_font_matrix dtransform rmoveto } ifelse\n"
				 "  } forall\n"
				 "  currentpoint cairo_store_point\n"
				 "} bind def\n"
				 "/cairo_selectfont { cairo_font_matrix aload pop pop pop 0 0 6 array astore\n"
				 "    cairo_font exch selectfont cairo_point_x cairo_point_y moveto } bind def\n"
				 "/Tf { pop /cairo_font exch def /cairo_font_matrix where\n"
				 "      { pop cairo_selectfont } if } bind def\n"
				 "/Td { matrix translate cairo_font_matrix matrix concatmatrix dup\n"
				 "      /cairo_font_matrix exch def dup 4 get exch 5 get cairo_store_point\n"
				 "      /cairo_font where { pop cairo_selectfont } if } bind def\n"
				 "/Tm { 2 copy 8 2 roll 6 array astore /cairo_font_matrix exch def\n"
				 "      cairo_store_point /cairo_font where { pop cairo_selectfont } if } bind def\n"
				 "/g { setgray } bind def\n"
				 "/rg { setrgbcolor } bind def\n"
				 "/d1 { setcachedevice } bind def\n");

    _cairo_output_stream_printf (surface->final_stream,
				 "%%%%EndProlog\n");

    num_comments = _cairo_array_num_elements (&surface->dsc_setup_comments);
    if (num_comments) {
	_cairo_output_stream_printf (surface->final_stream,
				     "%%%%BeginSetup\n");

	comments = _cairo_array_index (&surface->dsc_setup_comments, 0);
	for (i = 0; i < num_comments; i++) {
	    _cairo_output_stream_printf (surface->final_stream,
					 "%s\n", comments[i]);
	    free (comments[i]);
	    comments[i] = NULL;
	}

	_cairo_output_stream_printf (surface->final_stream,
				     "%%%%EndSetup\n");
    }
}

#if CAIRO_HAS_FT_FONT
static cairo_status_t
_cairo_ps_surface_emit_type1_font_subset (cairo_ps_surface_t		*surface,
					  cairo_scaled_font_subset_t	*font_subset)


{
    cairo_type1_subset_t subset;
    cairo_status_t status;
    int length;
    char name[64];

    snprintf (name, sizeof name, "f-%d-%d",
	      font_subset->font_id, font_subset->subset_id);
    status = _cairo_type1_subset_init (&subset, name, font_subset, TRUE);
    if (unlikely (status))
	return status;

    /* FIXME: Figure out document structure convention for fonts */

#if DEBUG_PS
    _cairo_output_stream_printf (surface->final_stream,
				 "%% _cairo_ps_surface_emit_type1_font_subset\n");
#endif

    length = subset.header_length + subset.data_length + subset.trailer_length;
    _cairo_output_stream_write (surface->final_stream, subset.data, length);

    _cairo_type1_subset_fini (&subset);

    return CAIRO_STATUS_SUCCESS;
}
#endif

static cairo_status_t
_cairo_ps_surface_emit_type1_font_fallback (cairo_ps_surface_t		*surface,
                                            cairo_scaled_font_subset_t	*font_subset)
{
    cairo_type1_subset_t subset;
    cairo_status_t status;
    int length;
    char name[64];

    snprintf (name, sizeof name, "f-%d-%d",
	      font_subset->font_id, font_subset->subset_id);
    status = _cairo_type1_fallback_init_hex (&subset, name, font_subset);
    if (unlikely (status))
	return status;

    /* FIXME: Figure out document structure convention for fonts */

#if DEBUG_PS
    _cairo_output_stream_printf (surface->final_stream,
				 "%% _cairo_ps_surface_emit_type1_font_fallback\n");
#endif

    length = subset.header_length + subset.data_length + subset.trailer_length;
    _cairo_output_stream_write (surface->final_stream, subset.data, length);

    _cairo_type1_fallback_fini (&subset);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_emit_truetype_font_subset (cairo_ps_surface_t		*surface,
					     cairo_scaled_font_subset_t	*font_subset)


{
    cairo_truetype_subset_t subset;
    cairo_status_t status;
    unsigned int i, begin, end;

    status = _cairo_truetype_subset_init (&subset, font_subset);
    if (unlikely (status))
	return status;

    /* FIXME: Figure out document structure convention for fonts */

#if DEBUG_PS
    _cairo_output_stream_printf (surface->final_stream,
				 "%% _cairo_ps_surface_emit_truetype_font_subset\n");
#endif

    _cairo_output_stream_printf (surface->final_stream,
				 "11 dict begin\n"
				 "/FontType 42 def\n"
				 "/FontName /%s def\n"
				 "/PaintType 0 def\n"
				 "/FontMatrix [ 1 0 0 1 0 0 ] def\n"
				 "/FontBBox [ 0 0 0 0 ] def\n"
				 "/Encoding 256 array def\n"
				 "0 1 255 { Encoding exch /.notdef put } for\n",
				 subset.ps_name);

    /* FIXME: Figure out how subset->x_max etc maps to the /FontBBox */

    for (i = 1; i < font_subset->num_glyphs; i++) {
	if (font_subset->glyph_names != NULL) {
	    _cairo_output_stream_printf (surface->final_stream,
					 "Encoding %d /%s put\n",
					 i, font_subset->glyph_names[i]);
	} else {
	    _cairo_output_stream_printf (surface->final_stream,
					 "Encoding %d /g%d put\n", i, i);
	}
    }

    _cairo_output_stream_printf (surface->final_stream,
				 "/CharStrings %d dict dup begin\n"
				 "/.notdef 0 def\n",
				 font_subset->num_glyphs);

    for (i = 1; i < font_subset->num_glyphs; i++) {
	if (font_subset->glyph_names != NULL) {
	    _cairo_output_stream_printf (surface->final_stream,
					 "/%s %d def\n",
					 font_subset->glyph_names[i], i);
	} else {
	    _cairo_output_stream_printf (surface->final_stream,
					 "/g%d %d def\n", i, i);
	}
    }

    _cairo_output_stream_printf (surface->final_stream,
				 "end readonly def\n");

    _cairo_output_stream_printf (surface->final_stream,
				 "/sfnts [\n");
    begin = 0;
    end = 0;
    for (i = 0; i < subset.num_string_offsets; i++) {
        end = subset.string_offsets[i];
        _cairo_output_stream_printf (surface->final_stream,"<");
        _cairo_output_stream_write_hex_string (surface->final_stream,
                                               subset.data + begin, end - begin);
        _cairo_output_stream_printf (surface->final_stream,"00>\n");
        begin = end;
    }
    if (subset.data_length > end) {
        _cairo_output_stream_printf (surface->final_stream,"<");
        _cairo_output_stream_write_hex_string (surface->final_stream,
                                               subset.data + end, subset.data_length - end);
        _cairo_output_stream_printf (surface->final_stream,"00>\n");
    }

    _cairo_output_stream_printf (surface->final_stream,
				 "] def\n"
				 "/f-%d-%d currentdict end definefont pop\n",
				 font_subset->font_id,
				 font_subset->subset_id);

    _cairo_truetype_subset_fini (&subset);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_emit_imagemask (cairo_image_surface_t *image,
			  cairo_output_stream_t *stream)
{
    uint8_t *row, *byte;
    int rows, cols;

    /* The only image type supported by Type 3 fonts are 1-bit image
     * masks */
    assert (image->format == CAIRO_FORMAT_A1);

    _cairo_output_stream_printf (stream,
				 "<<\n"
				 "   /ImageType 1\n"
				 "   /Width %d\n"
				 "   /Height %d\n"
				 "   /ImageMatrix [%d 0 0 %d 0 %d]\n"
				 "   /Decode [1 0]\n"
				 "   /BitsPerComponent 1\n",
				 image->width,
				 image->height,
				 image->width,
				 -image->height,
				 image->height);

    _cairo_output_stream_printf (stream,
				 "   /DataSource {<\n   ");
    for (row = image->data, rows = image->height; rows; row += image->stride, rows--) {
	for (byte = row, cols = (image->width + 7) / 8; cols; byte++, cols--) {
	    uint8_t output_byte = CAIRO_BITSWAP8_IF_LITTLE_ENDIAN (*byte);
	    _cairo_output_stream_printf (stream, "%02x ", output_byte);
	}
	_cairo_output_stream_printf (stream, "\n   ");
    }
    _cairo_output_stream_printf (stream, ">}\n>>\n");

    _cairo_output_stream_printf (stream,
				 "imagemask\n");

    return _cairo_output_stream_get_status (stream);
}

static cairo_status_t
_cairo_ps_surface_analyze_user_font_subset (cairo_scaled_font_subset_t *font_subset,
					    void		       *closure)
{
    cairo_ps_surface_t *surface = closure;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    unsigned int i;
    cairo_surface_t *type3_surface;

    type3_surface = _cairo_type3_glyph_surface_create (font_subset->scaled_font,
						       NULL,
						       _cairo_ps_emit_imagemask,
						       surface->font_subsets);

    for (i = 0; i < font_subset->num_glyphs; i++) {
	status = _cairo_type3_glyph_surface_analyze_glyph (type3_surface,
							   font_subset->glyphs[i]);
	if (unlikely (status))
	    break;

    }
    cairo_surface_finish (type3_surface);
    cairo_surface_destroy (type3_surface);

    return status;
}

static cairo_status_t
_cairo_ps_surface_emit_type3_font_subset (cairo_ps_surface_t		*surface,
					  cairo_scaled_font_subset_t	*font_subset)


{
    cairo_status_t status;
    unsigned int i;
    cairo_box_t font_bbox = {{0,0},{0,0}};
    cairo_box_t bbox = {{0,0},{0,0}};
    cairo_surface_t *type3_surface;
    double width;

    if (font_subset->num_glyphs == 0)
	return CAIRO_STATUS_SUCCESS;

#if DEBUG_PS
    _cairo_output_stream_printf (surface->final_stream,
				 "%% _cairo_ps_surface_emit_type3_font_subset\n");
#endif

    _cairo_output_stream_printf (surface->final_stream,
				 "8 dict begin\n"
				 "/FontType 3 def\n"
				 "/FontMatrix [1 0 0 1 0 0] def\n"
				 "/Encoding 256 array def\n"
				 "0 1 255 { Encoding exch /.notdef put } for\n");

    type3_surface = _cairo_type3_glyph_surface_create (font_subset->scaled_font,
						       NULL,
						       _cairo_ps_emit_imagemask,
						       surface->font_subsets);
    status = type3_surface->status;
    if (unlikely (status))
	return status;

    for (i = 0; i < font_subset->num_glyphs; i++) {
	if (font_subset->glyph_names != NULL) {
	    _cairo_output_stream_printf (surface->final_stream,
					 "Encoding %d /%s put\n",
					 i, font_subset->glyph_names[i]);
	} else {
	    _cairo_output_stream_printf (surface->final_stream,
					 "Encoding %d /g%d put\n", i, i);
	}
    }

    _cairo_output_stream_printf (surface->final_stream,
				 "/Glyphs [\n");

    for (i = 0; i < font_subset->num_glyphs; i++) {
	_cairo_output_stream_printf (surface->final_stream,
				     "    { %% %d\n", i);
	status = _cairo_type3_glyph_surface_emit_glyph (type3_surface,
							surface->final_stream,
							font_subset->glyphs[i],
							&bbox,
							&width);
	if (unlikely (status))
	    break;

	_cairo_output_stream_printf (surface->final_stream,
				     "    }\n");
        if (i == 0) {
            font_bbox.p1.x = bbox.p1.x;
            font_bbox.p1.y = bbox.p1.y;
            font_bbox.p2.x = bbox.p2.x;
            font_bbox.p2.y = bbox.p2.y;
        } else {
            if (bbox.p1.x < font_bbox.p1.x)
                font_bbox.p1.x = bbox.p1.x;
            if (bbox.p1.y < font_bbox.p1.y)
                font_bbox.p1.y = bbox.p1.y;
            if (bbox.p2.x > font_bbox.p2.x)
                font_bbox.p2.x = bbox.p2.x;
            if (bbox.p2.y > font_bbox.p2.y)
                font_bbox.p2.y = bbox.p2.y;
        }
    }
    cairo_surface_finish (type3_surface);
    cairo_surface_destroy (type3_surface);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->final_stream,
				 "] def\n"
				 "/FontBBox [%f %f %f %f] def\n"
				 "/BuildChar {\n"
				 "  exch /Glyphs get\n"
				 "  exch get\n"
				 "  10 dict begin exec end\n"
				 "} bind def\n"
				 "currentdict\n"
				 "end\n"
				 "/f-%d-%d exch definefont pop\n",
				 _cairo_fixed_to_double (font_bbox.p1.x),
				 - _cairo_fixed_to_double (font_bbox.p2.y),
				 _cairo_fixed_to_double (font_bbox.p2.x),
				 - _cairo_fixed_to_double (font_bbox.p1.y),
				 font_subset->font_id,
				 font_subset->subset_id);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_emit_unscaled_font_subset (cairo_scaled_font_subset_t	*font_subset,
				            void			*closure)
{
    cairo_ps_surface_t *surface = closure;
    cairo_status_t status;


    status = _cairo_scaled_font_subset_create_glyph_names (font_subset);
    if (_cairo_status_is_error (status))
	return status;

#if CAIRO_HAS_FT_FONT
    status = _cairo_ps_surface_emit_type1_font_subset (surface, font_subset);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;
#endif

    status = _cairo_ps_surface_emit_truetype_font_subset (surface, font_subset);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    status = _cairo_ps_surface_emit_type1_font_fallback (surface, font_subset);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    ASSERT_NOT_REACHED;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_emit_scaled_font_subset (cairo_scaled_font_subset_t *font_subset,
                                           void			      *closure)
{
    cairo_ps_surface_t *surface = closure;
    cairo_status_t status;

    status = _cairo_scaled_font_subset_create_glyph_names (font_subset);
    if (_cairo_status_is_error (status))
	return status;

    status = _cairo_ps_surface_emit_type3_font_subset (surface, font_subset);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    ASSERT_NOT_REACHED;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_emit_font_subsets (cairo_ps_surface_t *surface)
{
    cairo_status_t status;

#if DEBUG_PS
    _cairo_output_stream_printf (surface->final_stream,
				 "%% _cairo_ps_surface_emit_font_subsets\n");
#endif

    status = _cairo_scaled_font_subsets_foreach_user (surface->font_subsets,
						      _cairo_ps_surface_analyze_user_font_subset,
						      surface);
    if (unlikely (status))
	return status;

    status = _cairo_scaled_font_subsets_foreach_unscaled (surface->font_subsets,
                                                          _cairo_ps_surface_emit_unscaled_font_subset,
                                                          surface);
    if (unlikely (status))
	return status;

    status = _cairo_scaled_font_subsets_foreach_scaled (surface->font_subsets,
                                                        _cairo_ps_surface_emit_scaled_font_subset,
                                                        surface);
    if (unlikely (status))
	return status;

    return _cairo_scaled_font_subsets_foreach_user (surface->font_subsets,
						    _cairo_ps_surface_emit_scaled_font_subset,
						    surface);
}

static cairo_status_t
_cairo_ps_surface_emit_body (cairo_ps_surface_t *surface)
{
    char    buf[4096];
    int	    n;

    if (ferror (surface->tmpfile) != 0)
	return _cairo_error (CAIRO_STATUS_TEMP_FILE_ERROR);

    rewind (surface->tmpfile);
    while ((n = fread (buf, 1, sizeof (buf), surface->tmpfile)) > 0)
	_cairo_output_stream_write (surface->final_stream, buf, n);

    if (ferror (surface->tmpfile) != 0)
	return _cairo_error (CAIRO_STATUS_TEMP_FILE_ERROR);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_ps_surface_emit_footer (cairo_ps_surface_t *surface)
{
    _cairo_output_stream_printf (surface->final_stream,
				 "%%%%Trailer\n");

    if (surface->eps) {
	_cairo_output_stream_printf (surface->final_stream,
				     "count op_count sub {pop} repeat\n"
				     "countdictstack dict_count sub {end} repeat\n"
				     "cairo_eps_state restore\n");
    }

    _cairo_output_stream_printf (surface->final_stream,
				 "%%%%EOF\n");
}

static cairo_bool_t
_path_covers_bbox (cairo_ps_surface_t *surface,
		   cairo_path_fixed_t *path)
{
    cairo_box_t box;

    if (_cairo_path_fixed_is_box (path, &box)) {
	cairo_rectangle_int_t rect;

	_cairo_box_round_to_rectangle (&box, &rect);

	/* skip trivial whole-page clips */
	if (_cairo_rectangle_intersect (&rect, &surface->page_bbox)) {
	    if (rect.x == surface->page_bbox.x &&
		rect.width == surface->page_bbox.width &&
		rect.y == surface->page_bbox.y &&
		rect.height == surface->page_bbox.height)
	    {
		return TRUE;
	    }
	}
    }

    return FALSE;
}

static cairo_status_t
_cairo_ps_surface_clipper_intersect_clip_path (cairo_surface_clipper_t *clipper,
					       cairo_path_fixed_t *path,
					       cairo_fill_rule_t   fill_rule,
					       double		    tolerance,
					       cairo_antialias_t   antialias)
{
    cairo_ps_surface_t *surface = cairo_container_of (clipper,
						      cairo_ps_surface_t,
						      clipper);
    cairo_output_stream_t *stream = surface->stream;
    cairo_status_t status;

    assert (surface->paginated_mode != CAIRO_PAGINATED_MODE_ANALYZE);

#if DEBUG_PS
    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_intersect_clip_path\n");
#endif

    if (path == NULL) {
	status = _cairo_pdf_operators_flush (&surface->pdf_operators);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (stream, "Q q\n");

	surface->current_pattern_is_solid_color = FALSE;
	_cairo_pdf_operators_reset (&surface->pdf_operators);

	return CAIRO_STATUS_SUCCESS;
    }

    if (_path_covers_bbox (surface, path))
	return CAIRO_STATUS_SUCCESS;

    return _cairo_pdf_operators_clip (&surface->pdf_operators,
				      path,
				      fill_rule);
}

/* PLRM specifies a tolerance of 5 points when matching page sizes */
static cairo_bool_t
_ps_page_dimension_equal (int a, int b)
{
    return (abs (a - b) < 5);
}

static const char *
_cairo_ps_surface_get_page_media (cairo_ps_surface_t     *surface)
{
    int width, height, i;
    char buf[50];
    cairo_page_media_t *page;
    const char *page_name;

    width = _cairo_lround (surface->width);
    height = _cairo_lround (surface->height);

    /* search previously used page sizes */
    cairo_list_foreach_entry (page, cairo_page_media_t, &surface->document_media, link) {
	if (_ps_page_dimension_equal (width, page->width) &&
	    _ps_page_dimension_equal (height, page->height))
	    return page->name;
    }

    /* search list of standard page sizes */
    page_name = NULL;
    for (i = 0; i < ARRAY_LENGTH (_cairo_page_standard_media); i++) {
	if (_ps_page_dimension_equal (width, _cairo_page_standard_media[i].width) &&
	    _ps_page_dimension_equal (height, _cairo_page_standard_media[i].height))
	{
	    page_name = _cairo_page_standard_media[i].name;
	    width = _cairo_page_standard_media[i].width;
	    height = _cairo_page_standard_media[i].height;
	    break;
	}
    }

    page = malloc (sizeof (cairo_page_media_t));
    if (unlikely (page == NULL)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    if (page_name) {
	page->name = strdup (page_name);
    } else {
	snprintf (buf, sizeof (buf), "%dx%dmm",
		  (int) _cairo_lround (surface->width * 25.4/72),
		  (int) _cairo_lround (surface->height * 25.4/72));
	page->name = strdup (buf);
    }

    if (unlikely (page->name == NULL)) {
	free (page);
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    page->width = width;
    page->height = height;
    cairo_list_add_tail (&page->link, &surface->document_media);

    return page->name;
}

static cairo_surface_t *
_cairo_ps_surface_create_for_stream_internal (cairo_output_stream_t *stream,
					      double		     width,
					      double		     height)
{
    cairo_status_t status, status_ignored;
    cairo_ps_surface_t *surface;

    surface = malloc (sizeof (cairo_ps_surface_t));
    if (unlikely (surface == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP;
    }

    _cairo_surface_init (&surface->base,
			 &cairo_ps_surface_backend,
			 NULL, /* device */
			 CAIRO_CONTENT_COLOR_ALPHA);

    surface->final_stream = stream;

    surface->tmpfile = tmpfile ();
    if (surface->tmpfile == NULL) {
	switch (errno) {
	case ENOMEM:
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    break;
	default:
	    status = _cairo_error (CAIRO_STATUS_TEMP_FILE_ERROR);
	    break;
	}
	goto CLEANUP_SURFACE;
    }

    surface->stream = _cairo_output_stream_create_for_file (surface->tmpfile);
    status = _cairo_output_stream_get_status (surface->stream);
    if (unlikely (status))
	goto CLEANUP_OUTPUT_STREAM;

    surface->font_subsets = _cairo_scaled_font_subsets_create_simple ();
    if (unlikely (surface->font_subsets == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_OUTPUT_STREAM;
    }

    surface->has_creation_date = FALSE;
    surface->eps = FALSE;
    surface->ps_level = CAIRO_PS_LEVEL_3;
    surface->ps_level_used = CAIRO_PS_LEVEL_2;
    surface->width  = width;
    surface->height = height;
    cairo_matrix_init (&surface->cairo_to_ps, 1, 0, 0, -1, 0, height);
    surface->paginated_mode = CAIRO_PAGINATED_MODE_ANALYZE;
    surface->force_fallbacks = FALSE;
    surface->content = CAIRO_CONTENT_COLOR_ALPHA;
    surface->use_string_datasource = FALSE;
    surface->current_pattern_is_solid_color = FALSE;

    surface->page_bbox.x = 0;
    surface->page_bbox.y = 0;
    surface->page_bbox.width  = width;
    surface->page_bbox.height = height;

    _cairo_surface_clipper_init (&surface->clipper,
				 _cairo_ps_surface_clipper_intersect_clip_path);

    _cairo_pdf_operators_init (&surface->pdf_operators,
			       surface->stream,
			       &surface->cairo_to_ps,
			       surface->font_subsets);
    surface->num_pages = 0;

    cairo_list_init (&surface->document_media);
    _cairo_array_init (&surface->dsc_header_comments, sizeof (char *));
    _cairo_array_init (&surface->dsc_setup_comments, sizeof (char *));
    _cairo_array_init (&surface->dsc_page_setup_comments, sizeof (char *));

    surface->dsc_comment_target = &surface->dsc_header_comments;

    surface->paginated_surface = _cairo_paginated_surface_create (
	                                   &surface->base,
					   CAIRO_CONTENT_COLOR_ALPHA,
					   &cairo_ps_surface_paginated_backend);
    status = surface->paginated_surface->status;
    if (status == CAIRO_STATUS_SUCCESS) {
	/* paginated keeps the only reference to surface now, drop ours */
	cairo_surface_destroy (&surface->base);
	return surface->paginated_surface;
    }

    _cairo_scaled_font_subsets_destroy (surface->font_subsets);
 CLEANUP_OUTPUT_STREAM:
    status_ignored = _cairo_output_stream_destroy (surface->stream);
    fclose (surface->tmpfile);
 CLEANUP_SURFACE:
    free (surface);
 CLEANUP:
    /* destroy stream on behalf of caller */
    status_ignored = _cairo_output_stream_destroy (stream);

    return _cairo_surface_create_in_error (status);
}

/**
 * cairo_ps_surface_create:
 * @filename: a filename for the PS output (must be writable), %NULL may be
 *            used to specify no output. This will generate a PS surface that
 *            may be queried and used as a source, without generating a
 *            temporary file.
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 *
 * Creates a PostScript surface of the specified size in points to be
 * written to @filename. See cairo_ps_surface_create_for_stream() for
 * a more flexible mechanism for handling the PostScript output than
 * simply writing it to a named file.
 *
 * Note that the size of individual pages of the PostScript output can
 * vary. See cairo_ps_surface_set_size().
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy() when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 *
 * Since: 1.2
 **/
cairo_surface_t *
cairo_ps_surface_create (const char		*filename,
			 double			 width_in_points,
			 double			 height_in_points)
{
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create_for_filename (filename);
    if (_cairo_output_stream_get_status (stream))
	return _cairo_surface_create_in_error (_cairo_output_stream_destroy (stream));

    return _cairo_ps_surface_create_for_stream_internal (stream,
							 width_in_points,
							 height_in_points);
}

/**
 * cairo_ps_surface_create_for_stream:
 * @write_func: a #cairo_write_func_t to accept the output data, may be %NULL
 *              to indicate a no-op @write_func. With a no-op @write_func,
 *              the surface may be queried or used as a source without
 *              generating any temporary files.
 * @closure: the closure argument for @write_func
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 *
 * Creates a PostScript surface of the specified size in points to be
 * written incrementally to the stream represented by @write_func and
 * @closure. See cairo_ps_surface_create() for a more convenient way
 * to simply direct the PostScript output to a named file.
 *
 * Note that the size of individual pages of the PostScript
 * output can vary. See cairo_ps_surface_set_size().
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy() when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 *
 * Since: 1.2
 */
cairo_surface_t *
cairo_ps_surface_create_for_stream (cairo_write_func_t	write_func,
				    void	       *closure,
				    double		width_in_points,
				    double		height_in_points)
{
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create (write_func, NULL, closure);
    if (_cairo_output_stream_get_status (stream))
	return _cairo_surface_create_in_error (_cairo_output_stream_destroy (stream));

    return _cairo_ps_surface_create_for_stream_internal (stream,
							 width_in_points,
							 height_in_points);
}

static cairo_bool_t
_cairo_surface_is_ps (cairo_surface_t *surface)
{
    return surface->backend == &cairo_ps_surface_backend;
}

/* If the abstract_surface is a paginated surface, and that paginated
 * surface's target is a ps_surface, then set ps_surface to that
 * target. Otherwise return FALSE.
 */
static cairo_bool_t
_extract_ps_surface (cairo_surface_t	 *surface,
                     cairo_bool_t         set_error_on_failure,
		     cairo_ps_surface_t **ps_surface)
{
    cairo_surface_t *target;
    cairo_status_t status_ignored;

    if (surface->status)
	return FALSE;
    if (surface->finished) {
        if (set_error_on_failure)
            status_ignored = _cairo_surface_set_error (surface,
                                                       _cairo_error (CAIRO_STATUS_SURFACE_FINISHED));
	return FALSE;
    }

    if (! _cairo_surface_is_paginated (surface)) {
        if (set_error_on_failure)
            status_ignored = _cairo_surface_set_error (surface,
                                                       _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));
	return FALSE;
    }

    target = _cairo_paginated_surface_get_target (surface);
    if (target->status) {
        if (set_error_on_failure)
            status_ignored = _cairo_surface_set_error (surface, target->status);
	return FALSE;
    }
    if (target->finished) {
        if (set_error_on_failure)
            status_ignored = _cairo_surface_set_error (surface,
                                                       _cairo_error (CAIRO_STATUS_SURFACE_FINISHED));
	return FALSE;
    }

    if (! _cairo_surface_is_ps (target)) {
        if (set_error_on_failure)
            status_ignored = _cairo_surface_set_error (surface,
                                                       _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));
	return FALSE;
    }

    *ps_surface = (cairo_ps_surface_t *) target;
    return TRUE;
}

/**
 * cairo_ps_surface_restrict_to_level:
 * @surface: a PostScript #cairo_surface_t
 * @level: PostScript level
 *
 * Restricts the generated PostSript file to @level. See
 * cairo_ps_get_levels() for a list of available level values that
 * can be used here.
 *
 * This function should only be called before any drawing operations
 * have been performed on the given surface. The simplest way to do
 * this is to call this function immediately after creating the
 * surface.
 *
 * Since: 1.6
 **/
void
cairo_ps_surface_restrict_to_level (cairo_surface_t  *surface,
                                    cairo_ps_level_t  level)
{
    cairo_ps_surface_t *ps_surface = NULL;

    if (! _extract_ps_surface (surface, TRUE, &ps_surface))
	return;

    if (level < CAIRO_PS_LEVEL_LAST)
	ps_surface->ps_level = level;
}

/**
 * cairo_ps_get_levels:
 * @levels: supported level list
 * @num_levels: list length
 *
 * Used to retrieve the list of supported levels. See
 * cairo_ps_surface_restrict_to_level().
 *
 * Since: 1.6
 **/
void
cairo_ps_get_levels (cairo_ps_level_t const	**levels,
                     int                     	 *num_levels)
{
    if (levels != NULL)
	*levels = _cairo_ps_levels;

    if (num_levels != NULL)
	*num_levels = CAIRO_PS_LEVEL_LAST;
}

/**
 * cairo_ps_level_to_string:
 * @level: a level id
 *
 * Get the string representation of the given @level id. This function
 * will return %NULL if @level id isn't valid. See cairo_ps_get_levels()
 * for a way to get the list of valid level ids.
 *
 * Return value: the string associated to given level.
 *
 * Since: 1.6
 **/
const char *
cairo_ps_level_to_string (cairo_ps_level_t level)
{
    if (level >= CAIRO_PS_LEVEL_LAST)
	return NULL;

    return _cairo_ps_level_strings[level];
}

/**
 * cairo_ps_surface_set_eps:
 * @surface: a PostScript #cairo_surface_t
 * @eps: %TRUE to output EPS format PostScript
 *
 * If @eps is %TRUE, the PostScript surface will output Encapsulated
 * PostScript.
 *
 * This function should only be called before any drawing operations
 * have been performed on the current page. The simplest way to do
 * this is to call this function immediately after creating the
 * surface. An Encapsulated PostScript file should never contain more
 * than one page.
 *
 * Since: 1.6
 **/
void
cairo_ps_surface_set_eps (cairo_surface_t	*surface,
			  cairo_bool_t           eps)
{
    cairo_ps_surface_t *ps_surface = NULL;

    if (! _extract_ps_surface (surface, TRUE, &ps_surface))
	return;

    ps_surface->eps = eps;
}

/**
 * cairo_ps_surface_get_eps:
 * @surface: a PostScript #cairo_surface_t
 *
 * Check whether the PostScript surface will output Encapsulated PostScript.
 *
 * Return value: %TRUE if the surface will output Encapsulated PostScript.
 *
 * Since: 1.6
 **/
cairo_public cairo_bool_t
cairo_ps_surface_get_eps (cairo_surface_t	*surface)
{
    cairo_ps_surface_t *ps_surface = NULL;

    if (! _extract_ps_surface (surface, FALSE, &ps_surface))
	return FALSE;

    return ps_surface->eps;
}

/**
 * cairo_ps_surface_set_size:
 * @surface: a PostScript #cairo_surface_t
 * @width_in_points: new surface width, in points (1 point == 1/72.0 inch)
 * @height_in_points: new surface height, in points (1 point == 1/72.0 inch)
 *
 * Changes the size of a PostScript surface for the current (and
 * subsequent) pages.
 *
 * This function should only be called before any drawing operations
 * have been performed on the current page. The simplest way to do
 * this is to call this function immediately after creating the
 * surface or immediately after completing a page with either
 * cairo_show_page() or cairo_copy_page().
 *
 * Since: 1.2
 **/
void
cairo_ps_surface_set_size (cairo_surface_t	*surface,
			   double		 width_in_points,
			   double		 height_in_points)
{
    cairo_ps_surface_t *ps_surface = NULL;

    if (! _extract_ps_surface (surface, TRUE, &ps_surface))
	return;

    ps_surface->width = width_in_points;
    ps_surface->height = height_in_points;
    cairo_matrix_init (&ps_surface->cairo_to_ps, 1, 0, 0, -1, 0, height_in_points);
    _cairo_pdf_operators_set_cairo_to_pdf_matrix (&ps_surface->pdf_operators,
						  &ps_surface->cairo_to_ps);
}

/**
 * cairo_ps_surface_dsc_comment:
 * @surface: a PostScript #cairo_surface_t
 * @comment: a comment string to be emitted into the PostScript output
 *
 * Emit a comment into the PostScript output for the given surface.
 *
 * The comment is expected to conform to the PostScript Language
 * Document Structuring Conventions (DSC). Please see that manual for
 * details on the available comments and their meanings. In
 * particular, the %%IncludeFeature comment allows a
 * device-independent means of controlling printer device features. So
 * the PostScript Printer Description Files Specification will also be
 * a useful reference.
 *
 * The comment string must begin with a percent character (%) and the
 * total length of the string (including any initial percent
 * characters) must not exceed 255 characters. Violating either of
 * these conditions will place @surface into an error state. But
 * beyond these two conditions, this function will not enforce
 * conformance of the comment with any particular specification.
 *
 * The comment string should not have a trailing newline.
 *
 * The DSC specifies different sections in which particular comments
 * can appear. This function provides for comments to be emitted
 * within three sections: the header, the Setup section, and the
 * PageSetup section.  Comments appearing in the first two sections
 * apply to the entire document while comments in the BeginPageSetup
 * section apply only to a single page.
 *
 * For comments to appear in the header section, this function should
 * be called after the surface is created, but before a call to
 * cairo_ps_surface_begin_setup().
 *
 * For comments to appear in the Setup section, this function should
 * be called after a call to cairo_ps_surface_begin_setup() but before
 * a call to cairo_ps_surface_begin_page_setup().
 *
 * For comments to appear in the PageSetup section, this function
 * should be called after a call to cairo_ps_surface_begin_page_setup().
 *
 * Note that it is only necessary to call cairo_ps_surface_begin_page_setup()
 * for the first page of any surface. After a call to
 * cairo_show_page() or cairo_copy_page() comments are unambiguously
 * directed to the PageSetup section of the current page. But it
 * doesn't hurt to call this function at the beginning of every page
 * as that consistency may make the calling code simpler.
 *
 * As a final note, cairo automatically generates several comments on
 * its own. As such, applications must not manually generate any of
 * the following comments:
 *
 * Header section: %!PS-Adobe-3.0, %%Creator, %%CreationDate, %%Pages,
 * %%BoundingBox, %%DocumentData, %%LanguageLevel, %%EndComments.
 *
 * Setup section: %%BeginSetup, %%EndSetup
 *
 * PageSetup section: %%BeginPageSetup, %%PageBoundingBox,
 * %%EndPageSetup.
 *
 * Other sections: %%BeginProlog, %%EndProlog, %%Page, %%Trailer, %%EOF
 *
 * Here is an example sequence showing how this function might be used:
 *
 * <informalexample><programlisting>
 * #cairo_surface_t *surface = cairo_ps_surface_create (filename, width, height);
 * ...
 * cairo_ps_surface_dsc_comment (surface, "%%Title: My excellent document");
 * cairo_ps_surface_dsc_comment (surface, "%%Copyright: Copyright (C) 2006 Cairo Lover")
 * ...
 * cairo_ps_surface_dsc_begin_setup (surface);
 * cairo_ps_surface_dsc_comment (surface, "%%IncludeFeature: *MediaColor White");
 * ...
 * cairo_ps_surface_dsc_begin_page_setup (surface);
 * cairo_ps_surface_dsc_comment (surface, "%%IncludeFeature: *PageSize A3");
 * cairo_ps_surface_dsc_comment (surface, "%%IncludeFeature: *InputSlot LargeCapacity");
 * cairo_ps_surface_dsc_comment (surface, "%%IncludeFeature: *MediaType Glossy");
 * cairo_ps_surface_dsc_comment (surface, "%%IncludeFeature: *MediaColor Blue");
 * ... draw to first page here ..
 * cairo_show_page (cr);
 * ...
 * cairo_ps_surface_dsc_comment (surface, "%%IncludeFeature: *PageSize A5");
 * ...
 * </programlisting></informalexample>
 *
 * Since: 1.2
 **/
void
cairo_ps_surface_dsc_comment (cairo_surface_t	*surface,
			      const char	*comment)
{
    cairo_ps_surface_t *ps_surface = NULL;
    cairo_status_t status;
    char *comment_copy;

    if (! _extract_ps_surface (surface, TRUE, &ps_surface))
	return;

    /* A couple of sanity checks on the comment value. */
    if (comment == NULL) {
	status = _cairo_surface_set_error (surface, CAIRO_STATUS_NULL_POINTER);
	return;
    }

    if (comment[0] != '%' || strlen (comment) > 255) {
	status = _cairo_surface_set_error (surface, CAIRO_STATUS_INVALID_DSC_COMMENT);
	return;
    }

    /* Then, copy the comment and store it in the appropriate array. */
    comment_copy = strdup (comment);
    if (unlikely (comment_copy == NULL)) {
	status = _cairo_surface_set_error (surface, CAIRO_STATUS_NO_MEMORY);
	return;
    }

    status = _cairo_array_append (ps_surface->dsc_comment_target, &comment_copy);
    if (unlikely (status)) {
	free (comment_copy);
	status = _cairo_surface_set_error (surface, status);
	return;
    }
}

/**
 * cairo_ps_surface_dsc_begin_setup:
 * @surface: a PostScript #cairo_surface_t
 *
 * This function indicates that subsequent calls to
 * cairo_ps_surface_dsc_comment() should direct comments to the Setup
 * section of the PostScript output.
 *
 * This function should be called at most once per surface, and must
 * be called before any call to cairo_ps_surface_dsc_begin_page_setup()
 * and before any drawing is performed to the surface.
 *
 * See cairo_ps_surface_dsc_comment() for more details.
 *
 * Since: 1.2
 **/
void
cairo_ps_surface_dsc_begin_setup (cairo_surface_t *surface)
{
    cairo_ps_surface_t *ps_surface = NULL;

    if (! _extract_ps_surface (surface, TRUE, &ps_surface))
	return;

    if (ps_surface->dsc_comment_target == &ps_surface->dsc_header_comments)
	ps_surface->dsc_comment_target = &ps_surface->dsc_setup_comments;
}

/**
 * cairo_ps_surface_dsc_begin_page_setup:
 * @surface: a PostScript #cairo_surface_t
 *
 * This function indicates that subsequent calls to
 * cairo_ps_surface_dsc_comment() should direct comments to the
 * PageSetup section of the PostScript output.
 *
 * This function call is only needed for the first page of a
 * surface. It should be called after any call to
 * cairo_ps_surface_dsc_begin_setup() and before any drawing is
 * performed to the surface.
 *
 * See cairo_ps_surface_dsc_comment() for more details.
 *
 * Since: 1.2
 **/
void
cairo_ps_surface_dsc_begin_page_setup (cairo_surface_t *surface)
{
    cairo_ps_surface_t *ps_surface = NULL;

    if (! _extract_ps_surface (surface, TRUE, &ps_surface))
	return;

    if (ps_surface->dsc_comment_target == &ps_surface->dsc_header_comments ||
	ps_surface->dsc_comment_target == &ps_surface->dsc_setup_comments)
    {
	ps_surface->dsc_comment_target = &ps_surface->dsc_page_setup_comments;
    }
}

static cairo_status_t
_cairo_ps_surface_finish (void *abstract_surface)
{
    cairo_status_t status, status2;
    cairo_ps_surface_t *surface = abstract_surface;
    int i, num_comments;
    char **comments;

    status = surface->base.status;
    if (unlikely (status))
	goto CLEANUP;

    _cairo_ps_surface_emit_header (surface);

    status = _cairo_ps_surface_emit_font_subsets (surface);
    if (unlikely (status))
	goto CLEANUP;

    status = _cairo_ps_surface_emit_body (surface);
    if (unlikely (status))
	goto CLEANUP;

    _cairo_ps_surface_emit_footer (surface);

CLEANUP:
    _cairo_scaled_font_subsets_destroy (surface->font_subsets);

    status2 = _cairo_output_stream_destroy (surface->stream);
    if (status == CAIRO_STATUS_SUCCESS)
	status = status2;

    fclose (surface->tmpfile);

    status2 = _cairo_output_stream_destroy (surface->final_stream);
    if (status == CAIRO_STATUS_SUCCESS)
	status = status2;

    while (! cairo_list_is_empty (&surface->document_media)) {
        cairo_page_media_t *page;

        page = cairo_list_first_entry (&surface->document_media,
                                       cairo_page_media_t,
                                       link);
        cairo_list_del (&page->link);
	free (page->name);
	free (page);
    }

    num_comments = _cairo_array_num_elements (&surface->dsc_header_comments);
    comments = _cairo_array_index (&surface->dsc_header_comments, 0);
    for (i = 0; i < num_comments; i++)
	free (comments[i]);
    _cairo_array_fini (&surface->dsc_header_comments);

    num_comments = _cairo_array_num_elements (&surface->dsc_setup_comments);
    comments = _cairo_array_index (&surface->dsc_setup_comments, 0);
    for (i = 0; i < num_comments; i++)
	free (comments[i]);
    _cairo_array_fini (&surface->dsc_setup_comments);

    num_comments = _cairo_array_num_elements (&surface->dsc_page_setup_comments);
    comments = _cairo_array_index (&surface->dsc_page_setup_comments, 0);
    for (i = 0; i < num_comments; i++)
	free (comments[i]);
    _cairo_array_fini (&surface->dsc_page_setup_comments);

    _cairo_surface_clipper_reset (&surface->clipper);

    return status;
}

static cairo_int_status_t
_cairo_ps_surface_start_page (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

    /* Increment before print so page numbers start at 1. */
    surface->num_pages++;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_end_page (cairo_ps_surface_t *surface)
{
    cairo_int_status_t status;

    status = _cairo_pdf_operators_flush (&surface->pdf_operators);
    if (unlikely (status))
	return status;

    if (surface->clipper.clip.path != NULL) {
	_cairo_output_stream_printf (surface->stream, "Q Q\n");
	_cairo_surface_clipper_reset (&surface->clipper);
    } else
	_cairo_output_stream_printf (surface->stream, "Q\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_show_page (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_int_status_t status;

    status = _cairo_ps_surface_end_page (surface);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->stream, "showpage\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
color_is_gray (double red, double green, double blue)
{
    const double epsilon = 0.00001;

    return (fabs (red - green) < epsilon &&
	    fabs (red - blue) < epsilon);
}

static cairo_int_status_t
_cairo_ps_surface_analyze_surface_pattern_transparency (cairo_ps_surface_t      *surface,
						       cairo_surface_pattern_t *pattern)
{
    cairo_image_surface_t  *image;
    void		   *image_extra;
    cairo_int_status_t      status;
    cairo_image_transparency_t transparency;

    status = _cairo_surface_acquire_source_image (pattern->surface,
						  &image,
						  &image_extra);
    if (unlikely (status))
	return status;

    if (image->base.status)
	return image->base.status;

    transparency = _cairo_image_analyze_transparency (image);
    switch (transparency) {
    case CAIRO_IMAGE_IS_OPAQUE:
	status = CAIRO_STATUS_SUCCESS;
	break;

    case CAIRO_IMAGE_HAS_BILEVEL_ALPHA:
	if (surface->ps_level == CAIRO_PS_LEVEL_2) {
	    status = CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY;
	} else {
	    surface->ps_level_used = CAIRO_PS_LEVEL_3;
	    status = CAIRO_STATUS_SUCCESS;
	}
	break;

    case CAIRO_IMAGE_HAS_ALPHA:
	status = CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY;
	break;

    case CAIRO_IMAGE_UNKNOWN:
	ASSERT_NOT_REACHED;
    }

    _cairo_surface_release_source_image (pattern->surface, image, image_extra);

    return status;
}

static cairo_bool_t
surface_pattern_supported (const cairo_surface_pattern_t *pattern)
{
    if (pattern->surface->type == CAIRO_SURFACE_TYPE_RECORDING)
	return TRUE;

    if (pattern->surface->backend->acquire_source_image == NULL)
	return FALSE;

    /* Does an ALPHA-only source surface even make sense? Maybe, but I
     * don't think it's worth the extra code to support it. */

/* XXX: Need to write this function here...
    content = pattern->surface->content;
    if (content == CAIRO_CONTENT_ALPHA)
	return FALSE;
*/

    return TRUE;
}

static cairo_bool_t
_gradient_pattern_supported (cairo_ps_surface_t    *surface,
			     const cairo_pattern_t *pattern)
{
    const cairo_gradient_pattern_t *gradient = (const cairo_gradient_pattern_t *) pattern;
    uint16_t alpha;
    cairo_extend_t extend;
    unsigned int i;

    if (surface->ps_level == CAIRO_PS_LEVEL_2)
	return FALSE;

    if (gradient->n_stops == 0)
	return TRUE;

    /* Alpha gradients are only supported (by flattening the alpha)
     * if there is no variation in the alpha across the gradient. */
    alpha = gradient->stops[0].color.alpha_short;
    for (i = 0; i < gradient->n_stops; i++) {
	if (gradient->stops[i].color.alpha_short != alpha)
	    return FALSE;
    }

    extend = cairo_pattern_get_extend ((cairo_pattern_t *) pattern);

    /* Radial gradients are currently only supported when one circle
     * is inside the other. */
    if (pattern->type == CAIRO_PATTERN_TYPE_RADIAL) {
        double x1, y1, x2, y2, r1, r2, d;
        cairo_radial_pattern_t *radial = (cairo_radial_pattern_t *) pattern;

	if (extend == CAIRO_EXTEND_REPEAT ||
	    extend == CAIRO_EXTEND_REFLECT) {
	    return FALSE;
	}

        x1 = _cairo_fixed_to_double (radial->c1.x);
        y1 = _cairo_fixed_to_double (radial->c1.y);
        r1 = _cairo_fixed_to_double (radial->r1);
        x2 = _cairo_fixed_to_double (radial->c2.x);
        y2 = _cairo_fixed_to_double (radial->c2.y);
        r2 = _cairo_fixed_to_double (radial->r2);

        d = sqrt((x2 - x1)*(x2 - x1) + (y2 - y1)*(y2 - y1));
        if (d > fabs(r2 - r1)) {
            return FALSE;
        }
    }

    surface->ps_level_used = CAIRO_PS_LEVEL_3;

    return TRUE;
}

static cairo_bool_t
pattern_supported (cairo_ps_surface_t *surface, const cairo_pattern_t *pattern)
{
    if (pattern->type == CAIRO_PATTERN_TYPE_SOLID)
	return TRUE;

    if (pattern->type == CAIRO_PATTERN_TYPE_LINEAR ||
	pattern->type == CAIRO_PATTERN_TYPE_RADIAL)
	return _gradient_pattern_supported (surface, pattern);

    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE)
	return surface_pattern_supported ((cairo_surface_pattern_t *) pattern);

    return FALSE;
}

static cairo_int_status_t
_cairo_ps_surface_analyze_operation (cairo_ps_surface_t    *surface,
				     cairo_operator_t       op,
				     const cairo_pattern_t       *pattern,
				     const cairo_rectangle_int_t *extents)
{
    if (surface->force_fallbacks &&
	surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (! pattern_supported (surface, pattern))
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (! (op == CAIRO_OPERATOR_SOURCE || op == CAIRO_OPERATOR_OVER))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t *) pattern;

	if (surface_pattern->surface->type == CAIRO_SURFACE_TYPE_RECORDING) {
	    if (pattern->extend == CAIRO_EXTEND_PAD)
		return CAIRO_INT_STATUS_UNSUPPORTED;
	    else
		return CAIRO_INT_STATUS_ANALYZE_RECORDING_SURFACE_PATTERN;
	}
    }

    if (op == CAIRO_OPERATOR_SOURCE)
	return CAIRO_STATUS_SUCCESS;

    /* CAIRO_OPERATOR_OVER is only supported for opaque patterns. If
     * the pattern contains transparency, we return
     * CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY to the analysis
     * surface. If the analysis surface determines that there is
     * anything drawn under this operation, a fallback image will be
     * used. Otherwise the operation will be replayed during the
     * render stage and we blend the transparency into the white
     * background to convert the pattern to opaque.
     */
    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t *) pattern;

	return _cairo_ps_surface_analyze_surface_pattern_transparency (surface,
								       surface_pattern);
    }

    if (_cairo_pattern_is_opaque (pattern, extents))
	return CAIRO_STATUS_SUCCESS;

    return CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY;
}

static cairo_bool_t
_cairo_ps_surface_operation_supported (cairo_ps_surface_t    *surface,
				       cairo_operator_t       op,
				       const cairo_pattern_t       *pattern,
				       const cairo_rectangle_int_t *extents)
{
    return _cairo_ps_surface_analyze_operation (surface, op, pattern, extents) != CAIRO_INT_STATUS_UNSUPPORTED;
}

/* The "standard" implementation limit for PostScript string sizes is
 * 65535 characters (see PostScript Language Reference, Appendix
 * B). We go one short of that because we sometimes need two
 * characters in a string to represent a single ASCII85 byte, (for the
 * escape sequences "\\", "\(", and "\)") and we must not split these
 * across two strings. So we'd be in trouble if we went right to the
 * limit and one of these escape sequences just happened to land at
 * the end.
 */
#define STRING_ARRAY_MAX_STRING_SIZE (65535-1)
#define STRING_ARRAY_MAX_COLUMN	     72

typedef struct _string_array_stream {
    cairo_output_stream_t base;
    cairo_output_stream_t *output;
    int column;
    int string_size;
    cairo_bool_t use_strings;
} string_array_stream_t;

static cairo_status_t
_string_array_stream_write (cairo_output_stream_t *base,
			    const unsigned char   *data,
			    unsigned int	   length)
{
    string_array_stream_t *stream = (string_array_stream_t *) base;
    unsigned char c;
    const unsigned char backslash = '\\';

    if (length == 0)
	return CAIRO_STATUS_SUCCESS;

    while (length--) {
	if (stream->string_size == 0 && stream->use_strings) {
	    _cairo_output_stream_printf (stream->output, "(");
	    stream->column++;
	}

	c = *data++;
	if (stream->use_strings) {
	    switch (c) {
	    case '\\':
	    case '(':
	    case ')':
		_cairo_output_stream_write (stream->output, &backslash, 1);
		stream->column++;
		stream->string_size++;
		break;
	    }
	}
	/* Have to be careful to never split the final ~> sequence. */
        if (c == '~') {
	    _cairo_output_stream_write (stream->output, &c, 1);
	    stream->column++;
	    stream->string_size++;

	    if (length-- == 0)
		break;

	    c = *data++;
	}
	_cairo_output_stream_write (stream->output, &c, 1);
	stream->column++;
	stream->string_size++;

	if (stream->use_strings &&
	    stream->string_size >= STRING_ARRAY_MAX_STRING_SIZE)
	{
	    _cairo_output_stream_printf (stream->output, ")\n");
	    stream->string_size = 0;
	    stream->column = 0;
	}
	if (stream->column >= STRING_ARRAY_MAX_COLUMN) {
	    _cairo_output_stream_printf (stream->output, "\n ");
	    stream->string_size += 2;
	    stream->column = 1;
	}
    }

    return _cairo_output_stream_get_status (stream->output);
}

static cairo_status_t
_string_array_stream_close (cairo_output_stream_t *base)
{
    cairo_status_t status;
    string_array_stream_t *stream = (string_array_stream_t *) base;

    if (stream->use_strings)
	_cairo_output_stream_printf (stream->output, ")\n");

    status = _cairo_output_stream_get_status (stream->output);

    return status;
}

/* A string_array_stream wraps an existing output stream. It takes the
 * data provided to it and output one or more consecutive string
 * objects, each within the standard PostScript implementation limit
 * of 65k characters.
 *
 * The strings are each separated by a space character for easy
 * inclusion within an array object, (but the array delimiters are not
 * added by the string_array_stream).
 *
 * The string array stream is also careful to wrap the output within
 * STRING_ARRAY_MAX_COLUMN columns (+/- 1). The stream also adds
 * necessary escaping for special characters within a string,
 * (specifically '\', '(', and ')').
 */
static cairo_output_stream_t *
_string_array_stream_create (cairo_output_stream_t *output)
{
    string_array_stream_t *stream;

    stream = malloc (sizeof (string_array_stream_t));
    if (unlikely (stream == NULL)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return (cairo_output_stream_t *) &_cairo_output_stream_nil;
    }

    _cairo_output_stream_init (&stream->base,
			       _string_array_stream_write,
			       NULL,
			       _string_array_stream_close);
    stream->output = output;
    stream->column = 0;
    stream->string_size = 0;
    stream->use_strings = TRUE;

    return &stream->base;
}

/* A base85_array_stream wraps an existing output stream. It wraps the
 * output within STRING_ARRAY_MAX_COLUMN columns (+/- 1). The output
 * is not enclosed in strings like string_array_stream.
 */
static cairo_output_stream_t *
_base85_array_stream_create (cairo_output_stream_t *output)
{
    string_array_stream_t *stream;

    stream = malloc (sizeof (string_array_stream_t));
    if (unlikely (stream == NULL)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return (cairo_output_stream_t *) &_cairo_output_stream_nil;
    }

    _cairo_output_stream_init (&stream->base,
			       _string_array_stream_write,
			       NULL,
			       _string_array_stream_close);
    stream->output = output;
    stream->column = 0;
    stream->string_size = 0;
    stream->use_strings = FALSE;

    return &stream->base;
}


/* PS Output - this section handles output of the parts of the recording
 * surface we can render natively in PS. */

static cairo_status_t
_cairo_ps_surface_flatten_image_transparency (cairo_ps_surface_t    *surface,
					      cairo_image_surface_t *image,
					      cairo_image_surface_t **opaque_image)
{
    cairo_surface_t *opaque;
    cairo_surface_pattern_t pattern;
    cairo_status_t status;

    opaque = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
					 image->width,
					 image->height);
    if (unlikely (opaque->status))
	return opaque->status;

    if (surface->content == CAIRO_CONTENT_COLOR_ALPHA) {
	status = _cairo_surface_paint (opaque,
				       CAIRO_OPERATOR_SOURCE,
				       &_cairo_pattern_white.base,
				       NULL);
	if (unlikely (status)) {
	    cairo_surface_destroy (opaque);
	    return status;
	}
    }

    _cairo_pattern_init_for_surface (&pattern, &image->base);
    pattern.base.filter = CAIRO_FILTER_NEAREST;
    status = _cairo_surface_paint (opaque, CAIRO_OPERATOR_OVER, &pattern.base, NULL);
    _cairo_pattern_fini (&pattern.base);
    if (unlikely (status)) {
	cairo_surface_destroy (opaque);
	return status;
    }

    *opaque_image = (cairo_image_surface_t *) opaque;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_emit_base85_string (cairo_ps_surface_t    *surface,
				      const unsigned char   *data,
				      unsigned long	     length,
				      cairo_bool_t           use_strings)
{
    cairo_output_stream_t *base85_stream, *string_array_stream;
    cairo_status_t status, status2;

    if (use_strings)
	string_array_stream = _string_array_stream_create (surface->stream);
    else
	string_array_stream = _base85_array_stream_create (surface->stream);

    status = _cairo_output_stream_get_status (string_array_stream);
    if (unlikely (status))
	return _cairo_output_stream_destroy (string_array_stream);

    base85_stream = _cairo_base85_stream_create (string_array_stream);
    status = _cairo_output_stream_get_status (base85_stream);
    if (unlikely (status)) {
	status2 = _cairo_output_stream_destroy (string_array_stream);
	return _cairo_output_stream_destroy (base85_stream);
    }

    _cairo_output_stream_write (base85_stream, data, length);

    status = _cairo_output_stream_destroy (base85_stream);

    /* Mark end of base85 data */
    _cairo_output_stream_printf (string_array_stream, "~>");
    status2 = _cairo_output_stream_destroy (string_array_stream);
    if (status == CAIRO_STATUS_SUCCESS)
	status = status2;


    return status;
}

static cairo_status_t
_cairo_ps_surface_emit_image (cairo_ps_surface_t    *surface,
			      cairo_image_surface_t *image,
			      cairo_operator_t	     op,
			      cairo_filter_t         filter)
{
    cairo_status_t status;
    unsigned char *data, *data_compressed;
    unsigned long data_size, data_compressed_size;
    cairo_image_surface_t *opaque_image = NULL;
    int x, y, i;
    cairo_image_transparency_t transparency;
    cairo_bool_t use_mask;
    uint32_t *pixel;
    int bit;
    const char *interpolate;

    if (image->base.status)
	return image->base.status;

    switch (filter) {
    default:
    case CAIRO_FILTER_GOOD:
    case CAIRO_FILTER_BEST:
    case CAIRO_FILTER_BILINEAR:
	interpolate = "true";
	break;
    case CAIRO_FILTER_FAST:
    case CAIRO_FILTER_NEAREST:
    case CAIRO_FILTER_GAUSSIAN:
	interpolate = "false";
	break;
    }

    transparency = _cairo_image_analyze_transparency (image);

    /* PostScript can not represent the alpha channel, so we blend the
       current image over a white (or black for CONTENT_COLOR
       surfaces) RGB surface to eliminate it. */

    if (op == CAIRO_OPERATOR_SOURCE ||
	transparency == CAIRO_IMAGE_HAS_ALPHA ||
	(transparency == CAIRO_IMAGE_HAS_BILEVEL_ALPHA &&
	 surface->ps_level == CAIRO_PS_LEVEL_2))
    {
	status = _cairo_ps_surface_flatten_image_transparency (surface,
							       image,
							       &opaque_image);
	if (unlikely (status))
	    return status;

	use_mask = FALSE;
    } else if (transparency == CAIRO_IMAGE_IS_OPAQUE) {
	opaque_image = image;
	use_mask = FALSE;
    } else {
	use_mask = TRUE;
    }

    if (use_mask) {
	/* Type 2 (mask and image interleaved) has the mask and image
	 * samples interleaved by row.  The mask row is first, one bit
	 * per pixel with (bit 7 first). The row is padded to byte
	 * boundaries. The image data is 3 bytes per pixel RGB
	 * format. */
	data_size = image->height * ((image->width + 7)/8 + 3*image->width);
    } else {
	data_size = image->height * image->width * 3;
    }
    data = malloc (data_size);
    if (unlikely (data == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto bail1;
    }

    if (use_mask) {
	i = 0;
	for (y = 0; y < image->height; y++) {
	    /* mask row */
	    pixel = (uint32_t *) (image->data + y * image->stride);
	    bit = 7;
	    for (x = 0; x < image->width; x++, pixel++) {
		if (bit == 7)
		    data[i] = 0;
		if (((*pixel & 0xff000000) >> 24) > 0x80)
		    data[i] |= (1 << bit);
		bit--;
		if (bit < 0) {
		    bit = 7;
		    i++;
		}
	    }
	    if (bit != 7)
		i++;

	    /* image row*/
	    pixel = (uint32_t *) (image->data + y * image->stride);
	    for (x = 0; x < image->width; x++, pixel++) {
		data[i++] = (*pixel & 0x00ff0000) >> 16;
		data[i++] = (*pixel & 0x0000ff00) >>  8;
		data[i++] = (*pixel & 0x000000ff) >>  0;
	    }
	}
    } else {
	i = 0;
	for (y = 0; y < opaque_image->height; y++) {
	    pixel = (uint32_t *) (opaque_image->data + y * opaque_image->stride);
	    for (x = 0; x < opaque_image->width; x++, pixel++) {
		data[i++] = (*pixel & 0x00ff0000) >> 16;
		data[i++] = (*pixel & 0x0000ff00) >>  8;
		data[i++] = (*pixel & 0x000000ff) >>  0;
	    }
	}
    }

    /* XXX: Should fix cairo-lzw to provide a stream-based interface
     * instead. */
    data_compressed_size = data_size;
    data_compressed = _cairo_lzw_compress (data, &data_compressed_size);
    if (unlikely (data_compressed == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto bail2;
    }

    if (surface->use_string_datasource) {
	/* Emit the image data as a base85-encoded string which will
	 * be used as the data source for the image operator later. */
	_cairo_output_stream_printf (surface->stream,
				     "/CairoImageData [\n");

	status = _cairo_ps_surface_emit_base85_string (surface,
						       data_compressed,
						       data_compressed_size,
						       TRUE);
	if (unlikely (status))
	    goto bail3;

	_cairo_output_stream_printf (surface->stream,
				     "] def\n");
	_cairo_output_stream_printf (surface->stream,
				     "/CairoImageDataIndex 0 def\n");
    }

    if (use_mask) {
	_cairo_output_stream_printf (surface->stream,
				     "/DeviceRGB setcolorspace\n"
				     "5 dict dup begin\n"
				     "  /ImageType 3 def\n"
				     "  /InterleaveType 2 def\n"
				     "  /DataDict 8 dict def\n"
				     "    DataDict begin\n"
				     "    /ImageType 1 def\n"
				     "    /Width %d def\n"
				     "    /Height %d def\n"
				     "    /Interpolate %s def\n"
				     "    /BitsPerComponent 8 def\n"
				     "    /Decode [ 0 1 0 1 0 1 ] def\n",
				     image->width,
				     image->height,
				     interpolate);

	if (surface->use_string_datasource) {
	    _cairo_output_stream_printf (surface->stream,
					 "    /DataSource {\n"
					 "      CairoImageData CairoImageDataIndex get\n"
					 "	/CairoImageDataIndex CairoImageDataIndex 1 add def\n"
					 "	CairoImageDataIndex CairoImageData length 1 sub gt\n"
					 "       { /CairoImageDataIndex 0 def } if\n"
					 "    } /ASCII85Decode filter /LZWDecode filter def\n");
	} else {
	    _cairo_output_stream_printf (surface->stream,
					 "    /DataSource currentfile /ASCII85Decode filter /LZWDecode filter def\n");
	}

	_cairo_output_stream_printf (surface->stream,
				     "    /ImageMatrix [ 1 0 0 -1 0 %d ] def\n"
				     "  end\n"
				     "  /MaskDict 8 dict def\n"
				     "     MaskDict begin\n"
				     "    /ImageType 1 def\n"
				     "    /Width %d def\n"
				     "    /Height %d def\n"
				     "    /Interpolate %s def\n"
				     "    /BitsPerComponent 1 def\n"
				     "    /Decode [ 1 0 ] def\n"
				     "    /ImageMatrix [ 1 0 0 -1 0 %d ] def\n"
				     "  end\n"
				     "end\n"
				     "image\n",
				     image->height,
				     image->width,
				     image->height,
				     interpolate,
				     image->height);
    } else {
	_cairo_output_stream_printf (surface->stream,
				     "/DeviceRGB setcolorspace\n"
				     "8 dict dup begin\n"
				     "  /ImageType 1 def\n"
				     "  /Width %d def\n"
				     "  /Height %d def\n"
				     "  /BitsPerComponent 8 def\n"
				     "  /Decode [ 0 1 0 1 0 1 ] def\n",
				     opaque_image->width,
				     opaque_image->height);
	if (surface->use_string_datasource) {
	    _cairo_output_stream_printf (surface->stream,
					 "  /DataSource {\n"
					 "    CairoImageData CairoImageDataIndex get\n"
					 "    /CairoImageDataIndex CairoImageDataIndex 1 add def\n"
					 "    CairoImageDataIndex CairoImageData length 1 sub gt\n"
					 "     { /CairoImageDataIndex 0 def } if\n"
					 "  } /ASCII85Decode filter /LZWDecode filter def\n");
	} else {
	    _cairo_output_stream_printf (surface->stream,
					 "  /DataSource currentfile /ASCII85Decode filter /LZWDecode filter def\n");
	}

	_cairo_output_stream_printf (surface->stream,
				     "  /Interpolate %s def\n"
				     "  /ImageMatrix [ 1 0 0 -1 0 %d ] def\n"
				     "end\n"
				     "image\n",
				     interpolate,
				     opaque_image->height);
    }

    if (!surface->use_string_datasource) {
	/* Emit the image data as a base85-encoded string which will
	 * be used as the data source for the image operator. */
	status = _cairo_ps_surface_emit_base85_string (surface,
						       data_compressed,
						       data_compressed_size,
						       FALSE);
	_cairo_output_stream_printf (surface->stream, "\n");
    } else {
	status = CAIRO_STATUS_SUCCESS;
    }

bail3:
    free (data_compressed);

bail2:
    free (data);

bail1:
    if (!use_mask && opaque_image != image)
	cairo_surface_destroy (&opaque_image->base);

    return status;
}

static cairo_status_t
_cairo_ps_surface_emit_jpeg_image (cairo_ps_surface_t    *surface,
				   cairo_surface_t	 *source,
				   int                    width,
				   int                    height)
{
    cairo_status_t status;
    const unsigned char *mime_data;
    unsigned long mime_data_length;
    cairo_image_info_t info;

    cairo_surface_get_mime_data (source, CAIRO_MIME_TYPE_JPEG,
				 &mime_data, &mime_data_length);
    if (unlikely (source->status))
	return source->status;
    if (mime_data == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_image_info_get_jpeg_info (&info, mime_data, mime_data_length);
    if (unlikely (status))
	return status;

    if (info.num_components != 1 && info.num_components != 3)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (surface->use_string_datasource) {
	/* Emit the image data as a base85-encoded string which will
	 * be used as the data source for the image operator later. */
	_cairo_output_stream_printf (surface->stream,
				     "/CairoImageData [\n");

	status = _cairo_ps_surface_emit_base85_string (surface,
						       mime_data,
						       mime_data_length,
						       TRUE);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (surface->stream,
				     "] def\n");
	_cairo_output_stream_printf (surface->stream,
				     "/CairoImageDataIndex 0 def\n");
    }

    _cairo_output_stream_printf (surface->stream,
				 "/%s setcolorspace\n"
				 "8 dict dup begin\n"
				 "  /ImageType 1 def\n"
				 "  /Width %d def\n"
				 "  /Height %d def\n"
				 "  /BitsPerComponent %d def\n"
				 "  /Decode [ 0 1 0 1 0 1 ] def\n",
				 info.num_components == 1 ? "DeviceGray" : "DeviceRGB",
				 info.width,
				 info.height,
				 info.bits_per_component);

    if (surface->use_string_datasource) {
	_cairo_output_stream_printf (surface->stream,
				     "  /DataSource {\n"
				     "    CairoImageData CairoImageDataIndex get\n"
				     "    /CairoImageDataIndex CairoImageDataIndex 1 add def\n"
				     "    CairoImageDataIndex CairoImageData length 1 sub gt\n"
				     "     { /CairoImageDataIndex 0 def } if\n"
				     "  } /ASCII85Decode filter /DCTDecode filter def\n");
    } else {
	_cairo_output_stream_printf (surface->stream,
				     "  /DataSource currentfile /ASCII85Decode filter /DCTDecode filter def\n");
    }

    _cairo_output_stream_printf (surface->stream,
				 "  /ImageMatrix [ 1 0 0 -1 0 %d ] def\n"
				 "end\n"
				 "image\n",
				 info.height);

    if (!surface->use_string_datasource) {
	/* Emit the image data as a base85-encoded string which will
	 * be used as the data source for the image operator. */
	status = _cairo_ps_surface_emit_base85_string (surface,
						       mime_data,
						       mime_data_length,
						       FALSE);
    }

    return status;
}

static cairo_status_t
_cairo_ps_surface_emit_recording_surface (cairo_ps_surface_t *surface,
					  cairo_surface_t    *recording_surface)
{
    double old_width, old_height;
    cairo_matrix_t old_cairo_to_ps;
    cairo_content_t old_content;
    cairo_rectangle_int_t old_page_bbox;
    cairo_box_t bbox;
    cairo_status_t status;

    old_content = surface->content;
    old_width = surface->width;
    old_height = surface->height;
    old_page_bbox = surface->page_bbox;
    old_cairo_to_ps = surface->cairo_to_ps;

    status =
	_cairo_recording_surface_get_bbox ((cairo_recording_surface_t *) recording_surface,
					   &bbox,
					   NULL);
    if (unlikely (status))
	return status;

#if DEBUG_PS
    _cairo_output_stream_printf (surface->stream,
				 "%% _cairo_ps_surface_emit_recording_surface (%f, %f), (%f, %f)\n",
				 _cairo_fixed_to_double (bbox.p1.x),
				 _cairo_fixed_to_double (bbox.p1.y),
				 _cairo_fixed_to_double (bbox.p2.x),
				 _cairo_fixed_to_double (bbox.p2.y));
#endif

    surface->width = _cairo_fixed_to_double (bbox.p2.x - bbox.p1.x);
    surface->height = _cairo_fixed_to_double (bbox.p2.y - bbox.p1.y);
    _cairo_box_round_to_rectangle (&bbox, &surface->page_bbox);

    surface->current_pattern_is_solid_color = FALSE;
    _cairo_pdf_operators_reset (&surface->pdf_operators);
    cairo_matrix_init (&surface->cairo_to_ps, 1, 0, 0, -1, 0, surface->height);
    _cairo_pdf_operators_set_cairo_to_pdf_matrix (&surface->pdf_operators,
						  &surface->cairo_to_ps);
    _cairo_output_stream_printf (surface->stream, "  q\n");

    if (recording_surface->content == CAIRO_CONTENT_COLOR) {
	surface->content = CAIRO_CONTENT_COLOR;
	_cairo_output_stream_printf (surface->stream,
				     "  0 g %d %d %d %d rectfill\n",
				     surface->page_bbox.x,
				     surface->page_bbox.y,
				     surface->page_bbox.width,
				     surface->page_bbox.height);
    }

    status = _cairo_recording_surface_replay_region (recording_surface,
						     NULL,
						     &surface->base,
						     CAIRO_RECORDING_REGION_NATIVE);
    assert (status != CAIRO_INT_STATUS_UNSUPPORTED);
    if (unlikely (status))
	return status;

    status = _cairo_pdf_operators_flush (&surface->pdf_operators);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->stream, "  Q\n");
    surface->content = old_content;
    surface->width = old_width;
    surface->height = old_height;
    surface->page_bbox = old_page_bbox;
    surface->current_pattern_is_solid_color = FALSE;
    _cairo_pdf_operators_reset (&surface->pdf_operators);
    surface->cairo_to_ps = old_cairo_to_ps;

    _cairo_pdf_operators_set_cairo_to_pdf_matrix (&surface->pdf_operators,
						  &surface->cairo_to_ps);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_emit_recording_subsurface (cairo_ps_surface_t *surface,
					     cairo_surface_t    *recording_surface,
					     const cairo_rectangle_int_t *extents)
{
    double old_width, old_height;
    cairo_matrix_t old_cairo_to_ps;
    cairo_content_t old_content;
    cairo_rectangle_int_t old_page_bbox;
    cairo_status_t status;

    old_content = surface->content;
    old_width = surface->width;
    old_height = surface->height;
    old_page_bbox = surface->page_bbox;
    old_cairo_to_ps = surface->cairo_to_ps;

#if DEBUG_PS
    _cairo_output_stream_printf (surface->stream,
				 "%% _cairo_ps_surface_emit_recording_subsurface (%d, %d), (%d, %d)\n",
				 extents->x, extents->y,
				 extents->width, extents->height);
#endif

    surface->page_bbox.x = surface->page_bbox.y = 0;
    surface->page_bbox.width = surface->width  = extents->width;
    surface->page_bbox.height = surface->height = extents->height;

    surface->current_pattern_is_solid_color = FALSE;
    _cairo_pdf_operators_reset (&surface->pdf_operators);
    cairo_matrix_init (&surface->cairo_to_ps, 1, 0, 0, -1, 0, surface->height);
    _cairo_pdf_operators_set_cairo_to_pdf_matrix (&surface->pdf_operators,
						  &surface->cairo_to_ps);
    _cairo_output_stream_printf (surface->stream, "  q\n");

    if (recording_surface->content == CAIRO_CONTENT_COLOR) {
	surface->content = CAIRO_CONTENT_COLOR;
	_cairo_output_stream_printf (surface->stream,
				     "  0 g %d %d %d %d rectfill\n",
				     surface->page_bbox.x,
				     surface->page_bbox.y,
				     surface->page_bbox.width,
				     surface->page_bbox.height);
    }

    status = _cairo_recording_surface_replay_region (recording_surface,
						     extents,
						     &surface->base,
						     CAIRO_RECORDING_REGION_NATIVE);
    assert (status != CAIRO_INT_STATUS_UNSUPPORTED);
    if (unlikely (status))
	return status;

    status = _cairo_pdf_operators_flush (&surface->pdf_operators);
    if (unlikely (status))
	return status;

    _cairo_output_stream_printf (surface->stream, "  Q\n");
    surface->content = old_content;
    surface->width = old_width;
    surface->height = old_height;
    surface->page_bbox = old_page_bbox;
    surface->current_pattern_is_solid_color = FALSE;
    _cairo_pdf_operators_reset (&surface->pdf_operators);
    surface->cairo_to_ps = old_cairo_to_ps;

    _cairo_pdf_operators_set_cairo_to_pdf_matrix (&surface->pdf_operators,
						  &surface->cairo_to_ps);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_ps_surface_flatten_transparency (cairo_ps_surface_t	*surface,
					const cairo_color_t	*color,
					double			*red,
					double			*green,
					double			*blue)
{
    *red   = color->red;
    *green = color->green;
    *blue  = color->blue;

    if (! CAIRO_COLOR_IS_OPAQUE (color)) {
	*red   *= color->alpha;
	*green *= color->alpha;
	*blue  *= color->alpha;
	if (surface->content == CAIRO_CONTENT_COLOR_ALPHA) {
	    double one_minus_alpha = 1. - color->alpha;
	    *red   += one_minus_alpha;
	    *green += one_minus_alpha;
	    *blue  += one_minus_alpha;
	}
    }
}

static void
_cairo_ps_surface_emit_solid_pattern (cairo_ps_surface_t    *surface,
				      cairo_solid_pattern_t *pattern)
{
    double red, green, blue;

    _cairo_ps_surface_flatten_transparency (surface, &pattern->color, &red, &green, &blue);

    if (color_is_gray (red, green, blue))
	_cairo_output_stream_printf (surface->stream,
				     "%f g\n",
				     red);
    else
	_cairo_output_stream_printf (surface->stream,
				     "%f %f %f rg\n",
				     red, green, blue);
}

static cairo_status_t
_cairo_ps_surface_acquire_surface (cairo_ps_surface_t      *surface,
				   cairo_surface_pattern_t *pattern,
				   cairo_rectangle_int_t   *extents,
				   int                     *width,
				   int                     *height,
				   int			   *origin_x,
				   int			   *origin_y)
{
    cairo_status_t          status;
    cairo_surface_t	   *pad_image;
    int x = 0;
    int y = 0;

    surface->acquired_image = NULL;
    surface->image = NULL;

    if (pattern->surface->type == CAIRO_SURFACE_TYPE_RECORDING) {
	if (pattern->surface->backend->type == CAIRO_SURFACE_TYPE_SUBSURFACE) {
	    cairo_surface_subsurface_t *sub = (cairo_surface_subsurface_t *) pattern->surface;

	    *width  = sub->extents.width;
	    *height = sub->extents.height;
	} else {
	    cairo_recording_surface_t *recording_surface = (cairo_recording_surface_t *) pattern->surface;
	    cairo_box_t bbox;
	    cairo_rectangle_int_t extents;

	    status = _cairo_recording_surface_get_bbox (recording_surface, &bbox, NULL);
	    if (unlikely (status))
		return status;

	    _cairo_box_round_to_rectangle (&bbox, &extents);
	    *width  = extents.width;
	    *height = extents.height;
	}
	return CAIRO_STATUS_SUCCESS;
    } else {
	status = _cairo_surface_acquire_source_image (pattern->surface,
						      &surface->acquired_image,
						      &surface->image_extra);
	if (unlikely (status))
	    return status;

	pad_image = &surface->acquired_image->base;
	if (cairo_pattern_get_extend (&pattern->base) == CAIRO_EXTEND_PAD) {
	    cairo_box_t box;
	    cairo_rectangle_int_t rect;
	    cairo_surface_pattern_t pad_pattern;

	    /* get the operation extents in pattern space */
	    _cairo_box_from_rectangle (&box, extents);
	    _cairo_matrix_transform_bounding_box_fixed (&pattern->base.matrix, &box, NULL);
	    _cairo_box_round_to_rectangle (&box, &rect);
	    x = -rect.x;
	    y = -rect.y;

	    pad_image =
		_cairo_image_surface_create_with_pixman_format (NULL,
								surface->acquired_image->pixman_format,
								rect.width, rect.height,
								0);
	    if (pad_image->status) {
		status = pad_image->status;
		goto BAIL;
	    }

	    _cairo_pattern_init_for_surface (&pad_pattern, &surface->acquired_image->base);
	    cairo_matrix_init_translate (&pad_pattern.base.matrix, -x, -y);
	    pad_pattern.base.extend = CAIRO_EXTEND_PAD;
	    status = _cairo_surface_paint (pad_image,
					   CAIRO_OPERATOR_SOURCE,
					   &pad_pattern.base,
					   NULL);
	    _cairo_pattern_fini (&pad_pattern.base);
	    if (unlikely (status)) {
		if (pad_image != &surface->acquired_image->base)
		    cairo_surface_destroy (pad_image);

		goto BAIL;
	    }
	}

	surface->image = (cairo_image_surface_t *) pad_image;
	*width = surface->image->width;
	*height = surface->image->height;
	*origin_x = x;
	*origin_y = y;
	return CAIRO_STATUS_SUCCESS;
    }

BAIL:
    _cairo_ps_surface_release_surface (surface, pattern);
    return status;
}

static cairo_status_t
_cairo_ps_surface_emit_surface (cairo_ps_surface_t      *surface,
				cairo_surface_pattern_t *pattern,
				cairo_operator_t	 op,
				int                      width,
				int                      height)
{
    cairo_status_t status;

    if (pattern->surface->type == CAIRO_SURFACE_TYPE_RECORDING) {
	cairo_surface_t *source = pattern->surface;

	if (source->backend->type == CAIRO_SURFACE_TYPE_SUBSURFACE) {
	    cairo_surface_subsurface_t *sub = (cairo_surface_subsurface_t *) source;
	    status = _cairo_ps_surface_emit_recording_subsurface (surface, sub->target, &sub->extents);
	} else {
	    status = _cairo_ps_surface_emit_recording_surface (surface, source);
	}
    } else {
	if (pattern->base.extend != CAIRO_EXTEND_PAD) {
	    status = _cairo_ps_surface_emit_jpeg_image (surface, pattern->surface,
							width, height);
	    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
		return status;
	}

	status = _cairo_ps_surface_emit_image (surface, surface->image,
					       op, pattern->base.filter);
    }

    return status;
}

static void
_cairo_ps_surface_release_surface (cairo_ps_surface_t      *surface,
				   cairo_surface_pattern_t *pattern)
{
    if (surface->image != surface->acquired_image)
	cairo_surface_destroy (&surface->image->base);

    if (pattern->surface->type != CAIRO_SURFACE_TYPE_RECORDING) {
	_cairo_surface_release_source_image (pattern->surface,
					     surface->acquired_image,
					     surface->image_extra);
    }

    surface->acquired_image = NULL;
    surface->image = NULL;
}

static void
_path_fixed_init_rectangle (cairo_path_fixed_t *path,
			    cairo_rectangle_int_t *rect)
{
    cairo_status_t status;

    _cairo_path_fixed_init (path);

    status = _cairo_path_fixed_move_to (path,
					_cairo_fixed_from_int (rect->x),
					_cairo_fixed_from_int (rect->y));
    assert (status == CAIRO_STATUS_SUCCESS);
    status = _cairo_path_fixed_rel_line_to (path,
					    _cairo_fixed_from_int (rect->width),
					    _cairo_fixed_from_int (0));
    assert (status == CAIRO_STATUS_SUCCESS);
    status = _cairo_path_fixed_rel_line_to (path,
					    _cairo_fixed_from_int (0),
					    _cairo_fixed_from_int (rect->height));
    assert (status == CAIRO_STATUS_SUCCESS);
    status = _cairo_path_fixed_rel_line_to (path,
					    _cairo_fixed_from_int (-rect->width),
					    _cairo_fixed_from_int (0));
    assert (status == CAIRO_STATUS_SUCCESS);

    status = _cairo_path_fixed_close_path (path);
    assert (status == CAIRO_STATUS_SUCCESS);
}

static cairo_status_t
_cairo_ps_surface_paint_surface (cairo_ps_surface_t      *surface,
				 cairo_surface_pattern_t *pattern,
				 cairo_rectangle_int_t   *extents,
				 cairo_operator_t	  op)
{
    cairo_status_t status;
    int width, height;
    cairo_matrix_t cairo_p2d, ps_p2d;
    cairo_path_fixed_t path;
    int origin_x = 0;
    int origin_y = 0;

    status = _cairo_pdf_operators_flush (&surface->pdf_operators);
    if (unlikely (status))
	return status;

    status = _cairo_ps_surface_acquire_surface (surface,
						pattern,
						extents,
						&width, &height,
						&origin_x, &origin_y);
    if (unlikely (status))
	return status;

    _path_fixed_init_rectangle (&path, extents);
    status = _cairo_pdf_operators_clip (&surface->pdf_operators,
					&path,
					CAIRO_FILL_RULE_WINDING);
    _cairo_path_fixed_fini (&path);
    if (unlikely (status))
	return status;

    cairo_p2d = pattern->base.matrix;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_FALLBACK) {
	double scale = cairo_p2d.xx;

	_cairo_output_stream_printf (surface->stream,
				     "%% Fallback Image: x=%f, y=%f, w=%d, h=%d res=%fdpi size=%ld\n",
				     -cairo_p2d.x0/scale,
				     -cairo_p2d.y0/scale,
				     (int)(width/scale),
				     (int)(height/scale),
				     scale*72,
				     (long)width*height*3);
    } else {
	if (op == CAIRO_OPERATOR_SOURCE) {
	    _cairo_output_stream_printf (surface->stream,
					 "%d g 0 0 %f %f rectfill\n",
					 surface->content == CAIRO_CONTENT_COLOR ? 0 : 1,
					 surface->width,
					 surface->height);
	}
    }

    status = cairo_matrix_invert (&cairo_p2d);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    ps_p2d = surface->cairo_to_ps;
    cairo_matrix_multiply (&ps_p2d, &cairo_p2d, &ps_p2d);
    cairo_matrix_translate (&ps_p2d, -origin_x, -origin_y);
    cairo_matrix_translate (&ps_p2d, 0.0, height);
    cairo_matrix_scale (&ps_p2d, 1.0, -1.0);

    if (! _cairo_matrix_is_identity (&ps_p2d)) {
	_cairo_output_stream_printf (surface->stream,
				     "[ %f %f %f %f %f %f ] concat\n",
				     ps_p2d.xx, ps_p2d.yx,
				     ps_p2d.xy, ps_p2d.yy,
				     ps_p2d.x0, ps_p2d.y0);
    }

    status = _cairo_ps_surface_emit_surface (surface, pattern, op, width, height);
    _cairo_ps_surface_release_surface (surface, pattern);

    return status;
}

static cairo_status_t
_cairo_ps_surface_emit_surface_pattern (cairo_ps_surface_t      *surface,
					cairo_surface_pattern_t *pattern,
					cairo_rectangle_int_t   *extents,
					cairo_operator_t	 op)
{
    cairo_status_t status;
    int pattern_width = 0; /* squelch bogus compiler warning */
    int pattern_height = 0; /* squelch bogus compiler warning */
    double xstep, ystep;
    cairo_matrix_t cairo_p2d, ps_p2d;
    cairo_bool_t old_use_string_datasource;
    int origin_x = 0;
    int origin_y = 0;

    cairo_p2d = pattern->base.matrix;
    status = cairo_matrix_invert (&cairo_p2d);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    status = _cairo_ps_surface_acquire_surface (surface,
						pattern,
						extents,
						&pattern_width, &pattern_height,
						&origin_x, &origin_y);
    if (unlikely (status))
	return status;

    switch (pattern->base.extend) {
    case CAIRO_EXTEND_PAD:
    case CAIRO_EXTEND_NONE:
    {
	/* In PS/PDF, (as far as I can tell), all patterns are
	 * repeating. So we support cairo's EXTEND_NONE semantics
	 * by setting the repeat step size to a size large enough
	 * to guarantee that no more than a single occurrence will
	 * be visible.
	 *
	 * First, map the surface extents into pattern space (since
	 * xstep and ystep are in pattern space).  Then use an upper
	 * bound on the length of the diagonal of the pattern image
	 * and the surface as repeat size.  This guarantees to never
	 * repeat visibly.
	 */
	double x1 = 0.0, y1 = 0.0;
	double x2 = surface->width, y2 = surface->height;
	_cairo_matrix_transform_bounding_box (&pattern->base.matrix,
					      &x1, &y1, &x2, &y2,
					      NULL);

	/* Rather than computing precise bounds of the union, just
	 * add the surface extents unconditionally. We only
	 * required an answer that's large enough, we don't really
	 * care if it's not as tight as possible.*/
	xstep = ystep = ceil ((x2 - x1) + (y2 - y1) +
			      pattern_width + pattern_height);
	break;
    }
    case CAIRO_EXTEND_REPEAT:
	xstep = pattern_width;
	ystep = pattern_height;
	break;
    case CAIRO_EXTEND_REFLECT:
	xstep = pattern_width*2;
	ystep = pattern_height*2;
	break;
	/* All the rest (if any) should have been analyzed away, so these
	 * cases should be unreachable. */
    default:
	ASSERT_NOT_REACHED;
	xstep = 0;
	ystep = 0;
    }

    _cairo_output_stream_printf (surface->stream,
				 "/CairoPattern {\n");

    old_use_string_datasource = surface->use_string_datasource;
    surface->use_string_datasource = TRUE;
    if (op == CAIRO_OPERATOR_SOURCE) {
	_cairo_output_stream_printf (surface->stream,
				     "%d g 0 0 %f %f rectfill\n",
				     surface->content == CAIRO_CONTENT_COLOR ? 0 : 1,
				     xstep, ystep);
    }
    status = _cairo_ps_surface_emit_surface (surface, pattern, op,
					     pattern_width, pattern_height);
    if (unlikely (status))
	return status;

    surface->use_string_datasource = old_use_string_datasource;
    _cairo_output_stream_printf (surface->stream,
				 "} bind def\n");

    _cairo_output_stream_printf (surface->stream,
				 "<< /PatternType 1\n"
				 "   /PaintType 1\n"
				 "   /TilingType 1\n");
    _cairo_output_stream_printf (surface->stream,
				 "   /XStep %f /YStep %f\n",
				 xstep, ystep);

    if (pattern->base.extend == CAIRO_EXTEND_REFLECT) {
	_cairo_output_stream_printf (surface->stream,
				     "   /BBox [0 0 %d %d]\n"
				     "   /PaintProc {\n"
				     "      CairoPattern\n"
				     "      [-1 0 0  1 %d 0] concat CairoPattern\n"
				     "      [ 1 0 0 -1 0 %d] concat CairoPattern\n"
				     "      [-1 0 0  1 %d 0] concat CairoPattern\n"
				     "      CairoPattern\n"
				     "   } bind\n",
				     pattern_width*2, pattern_height*2,
				     pattern_width*2,
				     pattern_height*2,
				     pattern_width*2);
    } else {
	if (op == CAIRO_OPERATOR_SOURCE) {
	    _cairo_output_stream_printf (surface->stream,
					 "   /BBox [0 0 %f %f]\n",
					 xstep, ystep);
	} else {
	    _cairo_output_stream_printf (surface->stream,
					 "   /BBox [0 0 %d %d]\n",
					 pattern_width, pattern_height);
	}
	_cairo_output_stream_printf (surface->stream,
				     "   /PaintProc { CairoPattern }\n");
    }

    _cairo_output_stream_printf (surface->stream,
				 ">>\n");

    cairo_p2d = pattern->base.matrix;
    status = cairo_matrix_invert (&cairo_p2d);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    cairo_matrix_init_identity (&ps_p2d);
    cairo_matrix_translate (&ps_p2d, 0.0, surface->height);
    cairo_matrix_scale (&ps_p2d, 1.0, -1.0);
    cairo_matrix_multiply (&ps_p2d, &cairo_p2d, &ps_p2d);
    cairo_matrix_translate (&ps_p2d, 0.0, pattern_height);
    cairo_matrix_scale (&ps_p2d, 1.0, -1.0);

    _cairo_output_stream_printf (surface->stream,
				 "[ %f %f %f %f %f %f ]\n",
				 ps_p2d.xx, ps_p2d.yx,
				 ps_p2d.xy, ps_p2d.yy,
				 ps_p2d.x0, ps_p2d.y0);
    _cairo_output_stream_printf (surface->stream,
				 "makepattern setpattern\n");

    return CAIRO_STATUS_SUCCESS;
}

typedef struct _cairo_ps_color_stop {
    double offset;
    double color[4];
} cairo_ps_color_stop_t;

static void
_cairo_ps_surface_emit_linear_colorgradient (cairo_ps_surface_t     *surface,
					     cairo_ps_color_stop_t  *stop1,
					     cairo_ps_color_stop_t  *stop2)
{
    _cairo_output_stream_printf (surface->stream,
				 "   << /FunctionType 2\n"
				 "      /Domain [ 0 1 ]\n"
				 "      /C0 [ %f %f %f ]\n"
				 "      /C1 [ %f %f %f ]\n"
				 "      /N 1\n"
				 "   >>\n",
				 stop1->color[0],
				 stop1->color[1],
				 stop1->color[2],
				 stop2->color[0],
				 stop2->color[1],
				 stop2->color[2]);
}

static void
_cairo_ps_surface_emit_stitched_colorgradient (cairo_ps_surface_t    *surface,
					       unsigned int 	      n_stops,
					       cairo_ps_color_stop_t  stops[])
{
    unsigned int i;

    _cairo_output_stream_printf (surface->stream,
				 "<< /FunctionType 3\n"
				 "   /Domain [ 0 1 ]\n"
				 "   /Functions [\n");
    for (i = 0; i < n_stops - 1; i++)
	_cairo_ps_surface_emit_linear_colorgradient (surface, &stops[i], &stops[i+1]);

    _cairo_output_stream_printf (surface->stream, "   ]\n");

    _cairo_output_stream_printf (surface->stream, "   /Bounds [ ");
    for (i = 1; i < n_stops-1; i++)
	_cairo_output_stream_printf (surface->stream, "%f ", stops[i].offset);
    _cairo_output_stream_printf (surface->stream, "]\n");

    _cairo_output_stream_printf (surface->stream, "   /Encode [ 1 1 %d { pop 0 1 } for ]\n",
				 n_stops - 1);

    _cairo_output_stream_printf (surface->stream, ">>\n");
}

static void
calc_gradient_color (cairo_ps_color_stop_t *new_stop,
		     cairo_ps_color_stop_t *stop1,
		     cairo_ps_color_stop_t *stop2)
{
    int i;
    double offset = stop1->offset / (stop1->offset + 1.0 - stop2->offset);

    for (i = 0; i < 4; i++)
	new_stop->color[i] = stop1->color[i] + offset*(stop2->color[i] - stop1->color[i]);
}

#define COLOR_STOP_EPSILON 1e-6

static cairo_status_t
_cairo_ps_surface_emit_pattern_stops (cairo_ps_surface_t       *surface,
				      cairo_gradient_pattern_t *pattern)
{
    cairo_ps_color_stop_t *allstops, *stops;
    unsigned int i, n_stops;

    allstops = _cairo_malloc_ab ((pattern->n_stops + 2), sizeof (cairo_ps_color_stop_t));
    if (unlikely (allstops == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    stops = &allstops[1];
    n_stops = pattern->n_stops;

    for (i = 0; i < n_stops; i++) {
	cairo_gradient_stop_t *stop = &pattern->stops[i];

	stops[i].color[0] = stop->color.red;
	stops[i].color[1] = stop->color.green;
	stops[i].color[2] = stop->color.blue;
	stops[i].color[3] = stop->color.alpha;
	stops[i].offset = pattern->stops[i].offset;
    }

    if (pattern->base.extend == CAIRO_EXTEND_REPEAT ||
	pattern->base.extend == CAIRO_EXTEND_REFLECT) {
	if (stops[0].offset > COLOR_STOP_EPSILON) {
	    if (pattern->base.extend == CAIRO_EXTEND_REFLECT)
		memcpy (allstops, stops, sizeof (cairo_ps_color_stop_t));
	    else
		calc_gradient_color (&allstops[0], &stops[0], &stops[n_stops-1]);
	    stops = allstops;
	    n_stops++;
	}
	stops[0].offset = 0.0;

	if (stops[n_stops-1].offset < 1.0 - COLOR_STOP_EPSILON) {
	    if (pattern->base.extend == CAIRO_EXTEND_REFLECT) {
		memcpy (&stops[n_stops],
			&stops[n_stops - 1],
			sizeof (cairo_ps_color_stop_t));
	    } else {
		calc_gradient_color (&stops[n_stops], &stops[0], &stops[n_stops-1]);
	    }
	    n_stops++;
	}
	stops[n_stops-1].offset = 1.0;
    }

    for (i = 0; i < n_stops; i++) {
	double red, green, blue;
	cairo_color_t color;

	_cairo_color_init_rgba (&color,
				stops[i].color[0],
				stops[i].color[1],
				stops[i].color[2],
				stops[i].color[3]);
	_cairo_ps_surface_flatten_transparency (surface, &color,
						&red, &green, &blue);
	stops[i].color[0] = red;
	stops[i].color[1] = green;
	stops[i].color[2] = blue;
    }

    _cairo_output_stream_printf (surface->stream,
				 "/CairoFunction\n");
    if (n_stops == 1) {
	/* work around single stop gradients */
	_cairo_ps_surface_emit_linear_colorgradient (surface, &stops[0], &stops[0]);
    } else if (n_stops == 2) {
	/* no need for stitched function */
	_cairo_ps_surface_emit_linear_colorgradient (surface, &stops[0], &stops[1]);
    } else {
	/* multiple stops: stitch. XXX possible optimization: regulary spaced
	 * stops do not require stitching. XXX */
	_cairo_ps_surface_emit_stitched_colorgradient (surface, n_stops,stops);
    }
    _cairo_output_stream_printf (surface->stream,
				 "def\n");

    free (allstops);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_emit_repeating_function (cairo_ps_surface_t       *surface,
					   cairo_gradient_pattern_t *pattern,
					   int                       begin,
					   int                       end)
{
    _cairo_output_stream_printf (surface->stream,
				 "/CairoFunction\n"
				 "<< /FunctionType 3\n"
				 "   /Domain [ %d %d ]\n"
				 "   /Functions [ %d {CairoFunction} repeat ]\n"
				 "   /Bounds [ %d 1 %d {} for ]\n",
				 begin,
                                 end,
				 end - begin,
				 begin + 1,
				 end - 1);

    if (pattern->base.extend == CAIRO_EXTEND_REFLECT) {
	_cairo_output_stream_printf (surface->stream, "   /Encode [ %d 1 %d { 2 mod 0 eq {0 1} {1 0} ifelse } for ]\n",
				     begin,
				     end - 1);
    } else {
	_cairo_output_stream_printf (surface->stream, "   /Encode [ %d 1 %d { pop 0 1 } for ]\n",
				     begin,
				     end - 1);
    }

    _cairo_output_stream_printf (surface->stream, ">> def\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_emit_linear_pattern (cairo_ps_surface_t     *surface,
				       cairo_linear_pattern_t *pattern)
{
    double x1, y1, x2, y2;
    double _x1, _y1, _x2, _y2;
    cairo_matrix_t pat_to_ps;
    cairo_extend_t extend;
    cairo_status_t status;
    cairo_gradient_pattern_t *gradient = &pattern->base;
    double first_stop, last_stop;
    int repeat_begin = 0, repeat_end = 1;

    extend = cairo_pattern_get_extend (&pattern->base.base);

    pat_to_ps = pattern->base.base.matrix;
    status = cairo_matrix_invert (&pat_to_ps);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    cairo_matrix_multiply (&pat_to_ps, &pat_to_ps, &surface->cairo_to_ps);
    first_stop = gradient->stops[0].offset;
    last_stop = gradient->stops[gradient->n_stops - 1].offset;

    if (pattern->base.base.extend == CAIRO_EXTEND_REPEAT ||
	pattern->base.base.extend == CAIRO_EXTEND_REFLECT) {
	double dx, dy;
	int x_rep = 0, y_rep = 0;

	x1 = _cairo_fixed_to_double (pattern->p1.x);
	y1 = _cairo_fixed_to_double (pattern->p1.y);
	cairo_matrix_transform_point (&pat_to_ps, &x1, &y1);

	x2 = _cairo_fixed_to_double (pattern->p2.x);
	y2 = _cairo_fixed_to_double (pattern->p2.y);
	cairo_matrix_transform_point (&pat_to_ps, &x2, &y2);

	dx = fabs (x2 - x1);
	dy = fabs (y2 - y1);
	if (dx > 1e-6)
	    x_rep = ceil (surface->width/dx);
	if (dy > 1e-6)
	    y_rep = ceil (surface->height/dy);

	repeat_end = MAX (x_rep, y_rep);
	repeat_begin = -repeat_end;
	first_stop = repeat_begin;
	last_stop = repeat_end;
    }

    /* PS requires the first and last stop to be the same as the line
     * coordinates. For repeating patterns this moves the line
     * coordinates out to the begin/end of the repeating function. For
     * non repeating patterns this may move the line coordinates in if
     * there are not stops at offset 0 and 1. */
    x1 = _cairo_fixed_to_double (pattern->p1.x);
    y1 = _cairo_fixed_to_double (pattern->p1.y);
    x2 = _cairo_fixed_to_double (pattern->p2.x);
    y2 = _cairo_fixed_to_double (pattern->p2.y);

    _x1 = x1 + (x2 - x1)*first_stop;
    _y1 = y1 + (y2 - y1)*first_stop;
    _x2 = x1 + (x2 - x1)*last_stop;
    _y2 = y1 + (y2 - y1)*last_stop;

    x1 = _x1;
    x2 = _x2;
    y1 = _y1;
    y2 = _y2;

    /* For EXTEND_NONE and EXTEND_PAD if there are only two stops a
     * Type 2 function is used by itself without a stitching
     * function. Type 2 functions always have the domain [0 1] */
    if ((pattern->base.base.extend == CAIRO_EXTEND_NONE ||
	 pattern->base.base.extend == CAIRO_EXTEND_PAD) &&
	gradient->n_stops == 2) {
	first_stop = 0.0;
	last_stop = 1.0;
    }

    status = _cairo_ps_surface_emit_pattern_stops (surface,
						   &pattern->base);
    if (unlikely (status))
	return status;

    if (pattern->base.base.extend == CAIRO_EXTEND_REPEAT ||
	pattern->base.base.extend == CAIRO_EXTEND_REFLECT) {
	status = _cairo_ps_surface_emit_repeating_function (surface,
							    &pattern->base,
							    repeat_begin,
							    repeat_end);
	if (unlikely (status))
	    return status;
    }

    _cairo_output_stream_printf (surface->stream,
				 "<< /PatternType 2\n"
				 "   /Shading\n"
				 "   << /ShadingType 2\n"
				 "      /ColorSpace /DeviceRGB\n"
				 "      /Coords [ %f %f %f %f ]\n"
                                 "      /Domain [ %f %f ]\n"
				 "      /Function CairoFunction\n",
				 x1, y1, x2, y2,
				 first_stop, last_stop);

    if (extend == CAIRO_EXTEND_PAD) {
	_cairo_output_stream_printf (surface->stream,
                                     "      /Extend [ true true ]\n");
    } else {
	_cairo_output_stream_printf (surface->stream,
                                     "      /Extend [ false false ]\n");
    }

    _cairo_output_stream_printf (surface->stream,
				 "   >>\n"
				 ">>\n");
    _cairo_output_stream_printf (surface->stream,
				 "[ %f %f %f %f %f %f ]\n",
                                 pat_to_ps.xx, pat_to_ps.yx,
                                 pat_to_ps.xy, pat_to_ps.yy,
                                 pat_to_ps.x0, pat_to_ps.y0);
    _cairo_output_stream_printf (surface->stream,
				 "makepattern setpattern\n");

    return status;
}

static cairo_status_t
_cairo_ps_surface_emit_radial_pattern (cairo_ps_surface_t     *surface,
				       cairo_radial_pattern_t *pattern)
{
    double x1, y1, x2, y2, r1, r2;
    cairo_matrix_t pat_to_ps;
    cairo_extend_t extend;
    cairo_status_t status;

    extend = cairo_pattern_get_extend (&pattern->base.base);

    pat_to_ps = pattern->base.base.matrix;
    status = cairo_matrix_invert (&pat_to_ps);
    /* cairo_pattern_set_matrix ensures the matrix is invertible */
    assert (status == CAIRO_STATUS_SUCCESS);

    cairo_matrix_multiply (&pat_to_ps, &pat_to_ps, &surface->cairo_to_ps);
    x1 = _cairo_fixed_to_double (pattern->c1.x);
    y1 = _cairo_fixed_to_double (pattern->c1.y);
    r1 = _cairo_fixed_to_double (pattern->r1);
    x2 = _cairo_fixed_to_double (pattern->c2.x);
    y2 = _cairo_fixed_to_double (pattern->c2.y);
    r2 = _cairo_fixed_to_double (pattern->r2);

   status = _cairo_ps_surface_emit_pattern_stops (surface, &pattern->base);
   if (unlikely (status))
      return status;

   _cairo_output_stream_printf (surface->stream,
				 "<< /PatternType 2\n"
				 "   /Shading\n"
				 "   << /ShadingType 3\n"
				 "      /ColorSpace /DeviceRGB\n"
				 "      /Coords [ %f %f %f %f %f %f ]\n"
				 "      /Function CairoFunction\n",
				 x1, y1, r1, x2, y2, r2);

    if (extend == CAIRO_EXTEND_PAD) {
	_cairo_output_stream_printf (surface->stream,
                                     "      /Extend [ true true ]\n");
    } else {
	_cairo_output_stream_printf (surface->stream,
                                     "      /Extend [ false false ]\n");
    }

    _cairo_output_stream_printf (surface->stream,
				 "   >>\n"
				 ">>\n");

    _cairo_output_stream_printf (surface->stream,
				 "[ %f %f %f %f %f %f ]\n",
				 pat_to_ps.xx, pat_to_ps.yx,
                                 pat_to_ps.xy, pat_to_ps.yy,
                                 pat_to_ps.x0, pat_to_ps.y0);
    _cairo_output_stream_printf (surface->stream,
				 "makepattern setpattern\n");

    return status;
}

static cairo_status_t
_cairo_ps_surface_emit_pattern (cairo_ps_surface_t *surface,
				const cairo_pattern_t *pattern,
				cairo_rectangle_int_t *extents,
				cairo_operator_t       op)
{
    cairo_status_t status;

    status = _cairo_pdf_operators_flush (&surface->pdf_operators);
    if (unlikely (status))
	return status;

    if (pattern->type == CAIRO_PATTERN_TYPE_SOLID) {
	cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *) pattern;

	if (surface->current_pattern_is_solid_color == FALSE ||
	    ! _cairo_color_equal (&surface->current_color, &solid->color))
	{
	    status = _cairo_pdf_operators_flush (&surface->pdf_operators);
	    if (unlikely (status))
		return status;

	    _cairo_ps_surface_emit_solid_pattern (surface, (cairo_solid_pattern_t *) pattern);

	    surface->current_pattern_is_solid_color = TRUE;
	    surface->current_color = solid->color;
	}

	return CAIRO_STATUS_SUCCESS;
    }

    surface->current_pattern_is_solid_color = FALSE;
    status = _cairo_pdf_operators_flush (&surface->pdf_operators);
    if (unlikely (status))
	    return status;

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:

	_cairo_ps_surface_emit_solid_pattern (surface, (cairo_solid_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_TYPE_SURFACE:
	status = _cairo_ps_surface_emit_surface_pattern (surface,
							 (cairo_surface_pattern_t *) pattern,
							 extents,
							 op);
	if (unlikely (status))
	    return status;
	break;

    case CAIRO_PATTERN_TYPE_LINEAR:
	status = _cairo_ps_surface_emit_linear_pattern (surface,
					  (cairo_linear_pattern_t *) pattern);
	if (unlikely (status))
	    return status;
	break;

    case CAIRO_PATTERN_TYPE_RADIAL:
	status = _cairo_ps_surface_emit_radial_pattern (surface,
					  (cairo_radial_pattern_t *) pattern);
	if (unlikely (status))
	    return status;
	break;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_ps_surface_get_extents (void		       *abstract_surface,
			       cairo_rectangle_int_t   *rectangle)
{
    cairo_ps_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;

    /* XXX: The conversion to integers here is pretty bogus, (not to
     * mention the aribitray limitation of width to a short(!). We
     * may need to come up with a better interface for get_extents.
     */
    rectangle->width  = ceil (surface->width);
    rectangle->height = ceil (surface->height);

    return TRUE;
}

static void
_cairo_ps_surface_get_font_options (void                  *abstract_surface,
				    cairo_font_options_t  *options)
{
    _cairo_font_options_init_default (options);

    cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_OFF);
    cairo_font_options_set_antialias (options, CAIRO_ANTIALIAS_GRAY);
}

static cairo_int_status_t
_cairo_ps_surface_paint (void			*abstract_surface,
			 cairo_operator_t	 op,
			 const cairo_pattern_t	*source,
			 cairo_clip_t		*clip)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    cairo_composite_rectangles_t extents;
    cairo_status_t status;

    cairo_rectangle_int_t rect;
    rect.x = rect.y = 0;
    rect.width = surface->width;
    rect.height = surface->height;

    status = _cairo_composite_rectangles_init_for_paint (&extents,
							 &rect,
							 op, source, clip);
    if (unlikely (status))
	return status;

    if (! _cairo_rectangle_intersect (&extents.bounded, &surface->page_bbox))
	return CAIRO_STATUS_SUCCESS;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_ps_surface_analyze_operation (surface, op, source, &extents.bounded);

    assert (_cairo_ps_surface_operation_supported (surface, op, source, &extents.bounded));

#if DEBUG_PS
    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_paint\n");
#endif

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status))
	return status;

    if (source->type == CAIRO_PATTERN_TYPE_SURFACE &&
	(source->extend == CAIRO_EXTEND_NONE ||
	 source->extend == CAIRO_EXTEND_PAD))
    {
	status = _cairo_pdf_operators_flush (&surface->pdf_operators);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (stream, "q\n");
	status = _cairo_ps_surface_paint_surface (surface,
						 (cairo_surface_pattern_t *) source,
						  &extents.bounded, op);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (stream, "Q\n");
    } else {
	status = _cairo_ps_surface_emit_pattern (surface, source, &extents.bounded, op);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (stream, "%d %d %d %d rectfill\n",
				     extents.bounded.x, extents.bounded.y,
				     extents.bounded.width, extents.bounded.height);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_stroke (void			*abstract_surface,
			  cairo_operator_t	 op,
			  const cairo_pattern_t	*source,
			  cairo_path_fixed_t	*path,
			  const cairo_stroke_style_t	*style,
			  const cairo_matrix_t	*ctm,
			  const cairo_matrix_t	*ctm_inverse,
			  double		 tolerance,
			  cairo_antialias_t	 antialias,
			  cairo_clip_t		*clip)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_composite_rectangles_t extents;
    cairo_int_status_t status;

    cairo_rectangle_int_t rect;
    rect.x = rect.y = 0;
    rect.width = surface->width;
    rect.height = surface->height;

    status = _cairo_composite_rectangles_init_for_stroke (&extents,
							  &rect,
							  op, source,
							  path, style, ctm,
							  clip);
    if (unlikely (status))
	return status;

    if (! _cairo_rectangle_intersect (&extents.bounded, &surface->page_bbox))
	return CAIRO_STATUS_SUCCESS;

    /* use the more accurate extents */
    if (extents.is_bounded) {
	status = _cairo_path_fixed_stroke_extents (path, style,
						   ctm, ctm_inverse,
						   tolerance,
						   &extents.mask);
	if (unlikely (status))
	    return status;

	if (! _cairo_rectangle_intersect (&extents.bounded, &extents.mask))
	    return CAIRO_STATUS_SUCCESS;
    }


    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_ps_surface_analyze_operation (surface, op, source, &extents.bounded);

    assert (_cairo_ps_surface_operation_supported (surface, op, source, &extents.bounded));

#if DEBUG_PS
    _cairo_output_stream_printf (surface->stream,
				 "%% _cairo_ps_surface_stroke\n");
#endif

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status))
	return status;

    status = _cairo_ps_surface_emit_pattern (surface, source, &extents.bounded, op);
    if (unlikely (status))
	return status;

    return _cairo_pdf_operators_stroke (&surface->pdf_operators,
					path,
					style,
					ctm,
					ctm_inverse);
}

static cairo_int_status_t
_cairo_ps_surface_fill (void		*abstract_surface,
			cairo_operator_t	 op,
			const cairo_pattern_t	*source,
			cairo_path_fixed_t	*path,
			cairo_fill_rule_t	 fill_rule,
			double			 tolerance,
			cairo_antialias_t	 antialias,
			cairo_clip_t		*clip)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_composite_rectangles_t extents;
    cairo_int_status_t status;

    cairo_rectangle_int_t rect;
    rect.x = rect.y = 0;
    rect.width = surface->width;
    rect.height = surface->height;

    status = _cairo_composite_rectangles_init_for_fill (&extents,
							&rect,
							op, source, path,
							clip);
    if (unlikely (status))
	return status;

    if (! _cairo_rectangle_intersect (&extents.bounded, &surface->page_bbox))
	return CAIRO_STATUS_SUCCESS;

    /* use the more accurate extents */
    if (extents.is_bounded) {
	_cairo_path_fixed_fill_extents (path,
					fill_rule,
					tolerance,
					&extents.mask);

	if (! _cairo_rectangle_intersect (&extents.bounded, &extents.mask))
	    return CAIRO_STATUS_SUCCESS;
    }

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_ps_surface_analyze_operation (surface, op, source, &extents.bounded);

    assert (_cairo_ps_surface_operation_supported (surface, op, source, &extents.bounded));

#if DEBUG_PS
    _cairo_output_stream_printf (surface->stream,
				 "%% _cairo_ps_surface_fill\n");
#endif

    status = _cairo_pdf_operators_flush (&surface->pdf_operators);
    if (unlikely (status))
	return status;

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status))
	return status;

    if (source->type == CAIRO_PATTERN_TYPE_SURFACE &&
	(source->extend == CAIRO_EXTEND_NONE ||
	 source->extend == CAIRO_EXTEND_PAD))
    {
	_cairo_output_stream_printf (surface->stream, "q\n");

	status =  _cairo_pdf_operators_clip (&surface->pdf_operators,
					     path,
					     fill_rule);
	if (unlikely (status))
	    return status;

	status = _cairo_ps_surface_paint_surface (surface,
						 (cairo_surface_pattern_t *) source,
						  &extents.bounded, op);
	if (unlikely (status))
	    return status;

	_cairo_output_stream_printf (surface->stream, "Q\n");
	_cairo_pdf_operators_reset (&surface->pdf_operators);
    } else {
	status = _cairo_ps_surface_emit_pattern (surface, source, &extents.bounded, op);
	if (unlikely (status))
	    return status;

	status = _cairo_pdf_operators_fill (&surface->pdf_operators,
					    path,
					    fill_rule);
    }

    return status;
}

static cairo_int_status_t
_cairo_ps_surface_show_glyphs (void		     *abstract_surface,
			       cairo_operator_t	      op,
			       const cairo_pattern_t *source,
			       cairo_glyph_t         *glyphs,
			       int		      num_glyphs,
			       cairo_scaled_font_t   *scaled_font,
			       cairo_clip_t	     *clip,
			       int		     *remaining_glyphs)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_composite_rectangles_t extents;
    cairo_bool_t overlap;
    cairo_status_t status;

    cairo_rectangle_int_t rect;
    rect.x = rect.y = 0;
    rect.width = surface->width;
    rect.height = surface->height;

    status = _cairo_composite_rectangles_init_for_glyphs (&extents,
							  &rect,
							  op, source,
							  scaled_font,
							  glyphs, num_glyphs,
							  clip,
							  &overlap);
    if (unlikely (status))
	return status;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _cairo_ps_surface_analyze_operation (surface, op, source, &extents.bounded);

    assert (_cairo_ps_surface_operation_supported (surface, op, source, &extents.bounded));

#if DEBUG_PS
    _cairo_output_stream_printf (surface->stream,
				 "%% _cairo_ps_surface_show_glyphs\n");
#endif

    status = _cairo_surface_clipper_set_clip (&surface->clipper, clip);
    if (unlikely (status))
	return status;

    status = _cairo_ps_surface_emit_pattern (surface, source, &extents.bounded, op);
    if (unlikely (status))
	return status;

    return _cairo_pdf_operators_show_text_glyphs (&surface->pdf_operators,
						  NULL, 0,
						  glyphs, num_glyphs,
						  NULL, 0,
						  FALSE,
						  scaled_font);
}

static void
_cairo_ps_surface_set_paginated_mode (void			*abstract_surface,
				      cairo_paginated_mode_t	 paginated_mode)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_status_t status;

    surface->paginated_mode = paginated_mode;

    if (surface->clipper.clip.path != NULL) {
	status = _cairo_pdf_operators_flush (&surface->pdf_operators);

	_cairo_output_stream_printf (surface->stream, "Q q\n");
	_cairo_surface_clipper_reset (&surface->clipper);
    }
}

static cairo_int_status_t
_cairo_ps_surface_set_bounding_box (void		*abstract_surface,
				    cairo_box_t		*bbox)
{
    cairo_ps_surface_t *surface = abstract_surface;
    int i, num_comments;
    char **comments;
    int x1, y1, x2, y2;
    cairo_bool_t has_page_media;
    const char *page_media;

    if (surface->eps) {
	x1 = floor (_cairo_fixed_to_double (bbox->p1.x));
	y1 = floor (surface->height - _cairo_fixed_to_double (bbox->p2.y));
	x2 = ceil (_cairo_fixed_to_double (bbox->p2.x));
	y2 = ceil (surface->height - _cairo_fixed_to_double (bbox->p1.y));
    } else {
	x1 = 0;
	y1 = 0;
	x2 = ceil (surface->width);
	y2 = ceil (surface->height);
    }

    surface->page_bbox.x = x1;
    surface->page_bbox.y = y1;
    surface->page_bbox.width  = x2 - x1;
    surface->page_bbox.height = y2 - y1;

    _cairo_output_stream_printf (surface->stream,
				 "%%%%Page: %d %d\n",
				 surface->num_pages,
				 surface->num_pages);

    _cairo_output_stream_printf (surface->stream,
				 "%%%%BeginPageSetup\n");

    has_page_media = FALSE;
    num_comments = _cairo_array_num_elements (&surface->dsc_page_setup_comments);
    comments = _cairo_array_index (&surface->dsc_page_setup_comments, 0);
    for (i = 0; i < num_comments; i++) {
	_cairo_output_stream_printf (surface->stream,
				     "%s\n", comments[i]);
	if (strncmp (comments[i], "%%PageMedia:", 11) == 0)
	    has_page_media = TRUE;
	free (comments[i]);
	comments[i] = NULL;
    }
    _cairo_array_truncate (&surface->dsc_page_setup_comments, 0);

    if (!has_page_media && !surface->eps) {
	page_media = _cairo_ps_surface_get_page_media (surface);
	if (unlikely (page_media == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	_cairo_output_stream_printf (surface->stream,
				     "%%%%PageMedia: %s\n",
				     page_media);
    }

    _cairo_output_stream_printf (surface->stream,
				 "%%%%PageBoundingBox: %d %d %d %d\n",
				 x1, y1, x2, y2);

    _cairo_output_stream_printf (surface->stream,
                                 "%%%%EndPageSetup\n"
				 "q %d %d %d %d rectclip q\n",
				 surface->page_bbox.x,
				 surface->page_bbox.y,
				 surface->page_bbox.width,
				 surface->page_bbox.height);

    if (surface->num_pages == 1) {
	surface->bbox_x1 = x1;
	surface->bbox_y1 = y1;
	surface->bbox_x2 = x2;
	surface->bbox_y2 = y2;
    } else {
	if (x1 < surface->bbox_x1)
	    surface->bbox_x1 = x1;
	if (y1 < surface->bbox_y1)
	    surface->bbox_y1 = y1;
	if (x2 > surface->bbox_x2)
	    surface->bbox_x2 = x2;
	if (y2 > surface->bbox_y2)
	    surface->bbox_y2 = y2;
    }
    surface->current_pattern_is_solid_color = FALSE;
    _cairo_pdf_operators_reset (&surface->pdf_operators);

    return _cairo_output_stream_get_status (surface->stream);
}

static cairo_bool_t
_cairo_ps_surface_supports_fine_grained_fallbacks (void	    *abstract_surface)
{
    return TRUE;
}

static const cairo_surface_backend_t cairo_ps_surface_backend = {
    CAIRO_SURFACE_TYPE_PS,
    NULL, /* create similar: handled by wrapper */
    _cairo_ps_surface_finish,
    NULL, /* acquire_source_image */
    NULL, /* release_source_image */
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* create_span_renderer */
    NULL, /* check_span_renderer */
    NULL, /* cairo_ps_surface_copy_page */
    _cairo_ps_surface_show_page,
    _cairo_ps_surface_get_extents,
    NULL, /* old_show_glyphs */
    _cairo_ps_surface_get_font_options,
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    /* Here are the drawing functions */

    _cairo_ps_surface_paint, /* paint */
    NULL, /* mask */
    _cairo_ps_surface_stroke,
    _cairo_ps_surface_fill,
    _cairo_ps_surface_show_glyphs,
    NULL, /* snapshot */
};

static const cairo_paginated_surface_backend_t cairo_ps_surface_paginated_backend = {
    _cairo_ps_surface_start_page,
    _cairo_ps_surface_set_paginated_mode,
    _cairo_ps_surface_set_bounding_box,
    NULL, /* _cairo_ps_surface_has_fallback_images, */
    _cairo_ps_surface_supports_fine_grained_fallbacks,
};
