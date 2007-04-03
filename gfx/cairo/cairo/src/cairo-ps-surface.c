/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
 * Copyright © 2005 Red Hat, Inc
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
 *	Carl D. Worth <cworth@cworth.org>
 *	Kristian Høgsberg <krh@redhat.com>
 *	Keith Packard <keithp@keithp.com>
 */

#include "cairoint.h"
#include "cairo-ps.h"
#include "cairo-ps-test.h"
#include "cairo-scaled-font-subsets-private.h"
#include "cairo-paginated-surface-private.h"
#include "cairo-meta-surface-private.h"
#include "cairo-output-stream-private.h"

#include <ctype.h>
#include <time.h>
#include <zlib.h>

static const cairo_surface_backend_t cairo_ps_surface_backend;
static const cairo_paginated_surface_backend_t cairo_ps_surface_paginated_backend;

typedef struct cairo_ps_surface {
    cairo_surface_t base;

    /* Here final_stream corresponds to the stream/file passed to
     * cairo_ps_surface_create surface is built. Meanwhile stream is a
     * temporary stream in which the file output is built, (so that
     * the header can be built and inserted into the target stream
     * before the contents of the temporary stream are copied). */
    cairo_output_stream_t *final_stream;

    FILE *tmpfile;
    cairo_output_stream_t *stream;

    double width;
    double height;
    double max_width;
    double max_height;

    int num_pages;

    cairo_paginated_mode_t paginated_mode;

    cairo_scaled_font_subsets_t *font_subsets;

    cairo_array_t dsc_header_comments;
    cairo_array_t dsc_setup_comments;
    cairo_array_t dsc_page_setup_comments;

    cairo_array_t *dsc_comment_target;

} cairo_ps_surface_t;

#define PS_SURFACE_MAX_GLYPHS_PER_FONT	256

/* A word wrap stream can be used as a filter to do word wrapping on
 * top of an existing output stream. The word wrapping is quite
 * simple, using isspace to determine characters that separate
 * words. Any word that will cause the column count exceed the given
 * max_column will have a '\n' character emitted before it.
 *
 * The stream is careful to maintain integrity for words that cross
 * the boundary from one call to write to the next.
 *
 * Note: This stream does not guarantee that the output will never
 * exceed max_column. In particular, if a single word is larger than
 * max_column it will not be broken up.
 */
typedef struct _word_wrap_stream {
    cairo_output_stream_t base;
    cairo_output_stream_t *output;
    int max_column;
    int column;
    cairo_bool_t last_write_was_space;
} word_wrap_stream_t;

static int
_count_word_up_to (const unsigned char *s, int length)
{
    int word = 0;

    while (length--) {
	if (! isspace (*s++))
	    word++;
	else
	    return word;
    }

    return word;
}

static cairo_status_t
_word_wrap_stream_write (cairo_output_stream_t  *base,
			 const unsigned char	*data,
			 unsigned int		 length)
{
    word_wrap_stream_t *stream = (word_wrap_stream_t *) base;
    cairo_bool_t newline;
    int word;

    while (length) {
	if (isspace (*data)) {
	    newline =  (*data == '\n' || *data == '\r');
	    if (! newline && stream->column >= stream->max_column) {
		_cairo_output_stream_printf (stream->output, "\n");
		stream->column = 0;
	    }
	    _cairo_output_stream_write (stream->output, data, 1);
	    data++;
	    length--;
	    if (newline)
		stream->column = 0;
	    else
		stream->column++;
	    stream->last_write_was_space = TRUE;
	} else {
	    word = _count_word_up_to (data, length);
	    /* Don't wrap if this word is a continuation of a word
	     * from a previous call to write. */
	    if (stream->column + word >= stream->max_column &&
		stream->last_write_was_space)
	    {
		_cairo_output_stream_printf (stream->output, "\n");
		stream->column = 0;
	    }
	    _cairo_output_stream_write (stream->output, data, word);
	    data += word;
	    length -= word;
	    stream->column += word;
	    stream->last_write_was_space = FALSE;
	}
    }

    return _cairo_output_stream_get_status (stream->output);
}

static cairo_status_t
_word_wrap_stream_close (cairo_output_stream_t *base)
{
    word_wrap_stream_t *stream = (word_wrap_stream_t *) base;

    return _cairo_output_stream_get_status (stream->output);
}

static cairo_output_stream_t *
_word_wrap_stream_create (cairo_output_stream_t *output, int max_column)
{
    word_wrap_stream_t *stream;

    stream = malloc (sizeof (word_wrap_stream_t));
    if (stream == NULL)
	return (cairo_output_stream_t *) &cairo_output_stream_nil;

    _cairo_output_stream_init (&stream->base,
			       _word_wrap_stream_write,
			       _word_wrap_stream_close);
    stream->output = output;
    stream->max_column = max_column;
    stream->column = 0;
    stream->last_write_was_space = FALSE;

    return &stream->base;
}

typedef struct _ps_path_info {
    cairo_ps_surface_t *surface;
    cairo_output_stream_t *stream;
    cairo_line_cap_t line_cap;
    cairo_point_t last_move_to_point;
    cairo_bool_t has_sub_path;
} ps_path_info_t;

static cairo_status_t
_cairo_ps_surface_path_move_to (void *closure, cairo_point_t *point)
{
    ps_path_info_t *path_info = closure;

    path_info->last_move_to_point = *point;
    path_info->has_sub_path = FALSE;

    _cairo_output_stream_printf (path_info->stream,
				 "%f %f moveto ",
				 _cairo_fixed_to_double (point->x),
				 _cairo_fixed_to_double (point->y));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_path_line_to (void *closure, cairo_point_t *point)
{
    ps_path_info_t *path_info = closure;

    if (path_info->line_cap != CAIRO_LINE_CAP_ROUND &&
	! path_info->has_sub_path &&
	point->x == path_info->last_move_to_point.x &&
	point->y == path_info->last_move_to_point.y)
    {
	return CAIRO_STATUS_SUCCESS;
    }

    path_info->has_sub_path = TRUE;

    _cairo_output_stream_printf (path_info->stream,
				 "%f %f lineto ",
				 _cairo_fixed_to_double (point->x),
				 _cairo_fixed_to_double (point->y));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_path_curve_to (void          *closure,
				 cairo_point_t *b,
				 cairo_point_t *c,
				 cairo_point_t *d)
{
    ps_path_info_t *path_info = closure;

    path_info->has_sub_path = TRUE;

    _cairo_output_stream_printf (path_info->stream,
				 "%f %f %f %f %f %f curveto ",
				 _cairo_fixed_to_double (b->x),
				 _cairo_fixed_to_double (b->y),
				 _cairo_fixed_to_double (c->x),
				 _cairo_fixed_to_double (c->y),
				 _cairo_fixed_to_double (d->x),
				 _cairo_fixed_to_double (d->y));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_path_close_path (void *closure)
{
    ps_path_info_t *path_info = closure;

    if (path_info->line_cap != CAIRO_LINE_CAP_ROUND &&
	! path_info->has_sub_path)
    {
	return CAIRO_STATUS_SUCCESS;
    }

    _cairo_output_stream_printf (path_info->stream,
				 "closepath\n");

    return CAIRO_STATUS_SUCCESS;
}

/* The line cap value is needed to workaround the fact that PostScript
 * semantics for stroking degenerate sub-paths do not match cairo
 * semantics. (PostScript draws something for any line cap value,
 * while cairo draws something only for round caps).
 *
 * When using this function to emit a path to be filled, rather than
 * stroked, simply pass CAIRO_LINE_CAP_ROUND which will guarantee that
 * the stroke workaround will not modify the path being emitted.
 */
static cairo_status_t
_cairo_ps_surface_emit_path (cairo_ps_surface_t	   *surface,
			     cairo_output_stream_t *stream,
			     cairo_path_fixed_t    *path,
			     cairo_line_cap_t	    line_cap)
{
    cairo_output_stream_t *word_wrap;
    cairo_status_t status;
    ps_path_info_t path_info;

    word_wrap = _word_wrap_stream_create (stream, 79);

    path_info.surface = surface;
    path_info.stream = word_wrap;
    path_info.line_cap = line_cap;
    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_ps_surface_path_move_to,
					  _cairo_ps_surface_path_line_to,
					  _cairo_ps_surface_path_curve_to,
					  _cairo_ps_surface_path_close_path,
					  &path_info);

    if (status == CAIRO_STATUS_SUCCESS)
	status = _cairo_output_stream_get_status (word_wrap);
    _cairo_output_stream_destroy (word_wrap);

    return status;
}

static void
_cairo_ps_surface_emit_header (cairo_ps_surface_t *surface)
{
    time_t now;
    char **comments;
    int i, num_comments;

    now = time (NULL);

    _cairo_output_stream_printf (surface->final_stream,
				 "%%!PS-Adobe-3.0\n"
				 "%%%%Creator: cairo %s (http://cairographics.org)\n"
				 "%%%%CreationDate: %s"
				 "%%%%Pages: %d\n"
				 "%%%%BoundingBox: %d %d %d %d\n",
                                 cairo_version_string (),
				 ctime (&now),
				 surface->num_pages,
				 0, 0,
				 (int) ceil (surface->max_width),
				 (int) ceil (surface->max_height));

    _cairo_output_stream_printf (surface->final_stream,
				 "%%%%DocumentData: Clean7Bit\n"
				 "%%%%LanguageLevel: 2\n");

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
				 "%%%%BeginProlog\n"
				 "/C{curveto}bind def\n"
				 "/F{fill}bind def\n"
				 "/G{setgray}bind def\n"
				 "/L{lineto}bind def\n"
				 "/M{moveto}bind def\n"
				 "/P{closepath}bind def\n"
				 "/R{setrgbcolor}bind def\n"
				 "/S{show}bind def\n"
				 "/xS{xshow}bind def\n"
				 "/yS{yshow}bind def\n"
				 "/xyS{xyshow}bind def\n"
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

    snprintf (name, sizeof name, "CairoFont-%d-%d",
	      font_subset->font_id, font_subset->subset_id);
    status = _cairo_type1_subset_init (&subset, name, font_subset, TRUE);
    if (status)
	return status;

    /* FIXME: Figure out document structure convention for fonts */

    _cairo_output_stream_printf (surface->final_stream,
				 "%% _cairo_ps_surface_emit_type1_font_subset\n");

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

    snprintf (name, sizeof name, "CairoFont-%d-%d",
	      font_subset->font_id, font_subset->subset_id);
    status = _cairo_type1_fallback_init_hex (&subset, name, font_subset);
    if (status)
	return status;

    /* FIXME: Figure out document structure convention for fonts */

    _cairo_output_stream_printf (surface->final_stream,
				 "%% _cairo_ps_surface_emit_type1_font_fallback\n");

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
    if (status)
	return status;

    /* FIXME: Figure out document structure convention for fonts */

    _cairo_output_stream_printf (surface->final_stream,
				 "%% _cairo_ps_surface_emit_truetype_font_subset\n");

    _cairo_output_stream_printf (surface->final_stream,
				 "11 dict begin\n"
				 "/FontType 42 def\n"
				 "/FontName /CairoFont-%d-%d def\n"
				 "/PaintType 0 def\n"
				 "/FontMatrix [ 1 0 0 1 0 0 ] def\n"
				 "/FontBBox [ 0 0 0 0 ] def\n"
				 "/Encoding 256 array def\n"
				 "0 1 255 { Encoding exch /.notdef put } for\n",
				 font_subset->font_id,
				 font_subset->subset_id);

    /* FIXME: Figure out how subset->x_max etc maps to the /FontBBox */

    for (i = 1; i < font_subset->num_glyphs; i++)
	_cairo_output_stream_printf (surface->final_stream,
				     "Encoding %d /g%d put\n", i, i);

    _cairo_output_stream_printf (surface->final_stream,
				 "/CharStrings %d dict dup begin\n"
				 "/.notdef 0 def\n",
				 font_subset->num_glyphs);

    for (i = 1; i < font_subset->num_glyphs; i++)
	_cairo_output_stream_printf (surface->final_stream,
				     "/g%d %d def\n", i, i);

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
				 "FontName currentdict end definefont pop\n");

    _cairo_truetype_subset_fini (&subset);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_emit_outline_glyph_data (cairo_ps_surface_t	*surface,
					   cairo_scaled_font_t	*scaled_font,
					   unsigned long	 glyph_index)
{
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_status_t status;

    status = _cairo_scaled_glyph_lookup (scaled_font,
					 glyph_index,
					 CAIRO_SCALED_GLYPH_INFO_METRICS|
					 CAIRO_SCALED_GLYPH_INFO_PATH,
					 &scaled_glyph);
    if (status)
	return status;

    _cairo_output_stream_printf (surface->final_stream,
				 "0 0 %f %f %f %f setcachedevice\n",
				 _cairo_fixed_to_double (scaled_glyph->bbox.p1.x),
				 -_cairo_fixed_to_double (scaled_glyph->bbox.p2.y),
				 _cairo_fixed_to_double (scaled_glyph->bbox.p2.x),
				 -_cairo_fixed_to_double (scaled_glyph->bbox.p1.y));

    /* We're filling not stroking, so we pass CAIRO_LINE_CAP_ROUND. */
    status = _cairo_ps_surface_emit_path (surface,
					  surface->final_stream,
					  scaled_glyph->path,
					  CAIRO_LINE_CAP_ROUND);

    _cairo_output_stream_printf (surface->final_stream,
				 "F\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_emit_bitmap_glyph_data (cairo_ps_surface_t	*surface,
					  cairo_scaled_font_t	*scaled_font,
					  unsigned long	 glyph_index)
{
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_status_t status;
    cairo_image_surface_t *image;
    unsigned char *row, *byte;
    int rows, cols;

    status = _cairo_scaled_glyph_lookup (scaled_font,
					 glyph_index,
					 CAIRO_SCALED_GLYPH_INFO_METRICS|
					 CAIRO_SCALED_GLYPH_INFO_SURFACE,
					 &scaled_glyph);

    image = scaled_glyph->surface;
    if (image->format != CAIRO_FORMAT_A1) {
	image = _cairo_image_surface_clone (image, CAIRO_FORMAT_A1);
	if (cairo_surface_status (&image->base))
	    return cairo_surface_status (&image->base);
    }

    _cairo_output_stream_printf (surface->final_stream,
				 "0 0 %f %f %f %f setcachedevice\n",
				 _cairo_fixed_to_double (scaled_glyph->bbox.p1.x),
				 - _cairo_fixed_to_double (scaled_glyph->bbox.p2.y),
				 _cairo_fixed_to_double (scaled_glyph->bbox.p2.x),
				 - _cairo_fixed_to_double (scaled_glyph->bbox.p1.y));

    _cairo_output_stream_printf (surface->final_stream,
				 "<<\n"
				 "   /ImageType 1\n"
				 "   /Width %d\n"
				 "   /Height %d\n"
				 "   /ImageMatrix [%f %f %f %f %f %f]\n"
				 "   /Decode [1 0]\n"
				 "   /BitsPerComponent 1\n",
				 image->width,
				 image->height,
				 image->base.device_transform_inverse.xx,
				 image->base.device_transform_inverse.yx,
				 image->base.device_transform_inverse.xy,
				 image->base.device_transform_inverse.yy,
				 image->base.device_transform_inverse.x0,
				 image->base.device_transform_inverse.y0);

    _cairo_output_stream_printf (surface->final_stream,
				 "   /DataSource   {<");
    for (row = image->data, rows = image->height; rows; row += image->stride, rows--) {
	for (byte = row, cols = (image->width + 7) / 8; cols; byte++, cols--) {
	    unsigned char output_byte = CAIRO_BITSWAP8_IF_LITTLE_ENDIAN (*byte);
	    _cairo_output_stream_printf (surface->final_stream, "%02x ", output_byte);
	}
	_cairo_output_stream_printf (surface->final_stream, "\n   ");
    }
    _cairo_output_stream_printf (surface->final_stream,
				 "   >}\n");
    _cairo_output_stream_printf (surface->final_stream,
				 ">>\n");

    _cairo_output_stream_printf (surface->final_stream,
				 "imagemask\n");

    if (image != scaled_glyph->surface)
	cairo_surface_destroy (&image->base);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_ps_surface_emit_glyph (cairo_ps_surface_t	*surface,
			      cairo_scaled_font_t	*scaled_font,
			      unsigned long		 scaled_font_glyph_index,
			      unsigned int		 subset_glyph_index)
{
    cairo_status_t	    status;

    _cairo_output_stream_printf (surface->final_stream,
				 "\t\t{ %% %d\n", subset_glyph_index);

    status = _cairo_ps_surface_emit_outline_glyph_data (surface,
							scaled_font,
							scaled_font_glyph_index);
    if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	status = _cairo_ps_surface_emit_bitmap_glyph_data (surface,
							   scaled_font,
							   scaled_font_glyph_index);

    _cairo_output_stream_printf (surface->final_stream,
				 "\t\t}\n");

    if (status)
	_cairo_surface_set_error (&surface->base, status);
}

static cairo_status_t
_cairo_ps_surface_emit_type3_font_subset (cairo_ps_surface_t		*surface,
					  cairo_scaled_font_subset_t	*font_subset)


{
    cairo_matrix_t matrix;
    unsigned int i;

    _cairo_output_stream_printf (surface->final_stream,
				 "%% _cairo_ps_surface_emit_type3_font_subset\n");

    _cairo_output_stream_printf (surface->final_stream,
				 "/CairoFont-%d-%d <<\n",
				 font_subset->font_id,
				 font_subset->subset_id);

    matrix = font_subset->scaled_font->scale;
    cairo_matrix_invert (&matrix);
    _cairo_output_stream_printf (surface->final_stream,
				 "\t/FontType\t3\n"
				 "\t/FontMatrix\t[%f %f %f %f 0 0]\n"
				 "\t/Encoding\t[0]\n"
				 "\t/FontBBox\t[0 0 10 10]\n"
				 "\t/Glyphs [\n",
				 matrix.xx,
				 matrix.yx,
				 -matrix.xy,
				 -matrix.yy);

    for (i = 0; i < font_subset->num_glyphs; i++) {
	_cairo_ps_surface_emit_glyph (surface,
				      font_subset->scaled_font,
				      font_subset->glyphs[i], i);
    }

    _cairo_output_stream_printf (surface->final_stream,
				 "\t]\n"
				 "\t/BuildChar {\n"
				 "\t\texch /Glyphs get\n"
				 "\t\texch get exec\n"
				 "\t}\n"
				 ">> definefont pop\n");

    return CAIRO_STATUS_SUCCESS;
}


static void
_cairo_ps_surface_emit_font_subset (cairo_scaled_font_subset_t	*font_subset,
				    void			*closure)
{
    cairo_ps_surface_t *surface = closure;
    cairo_status_t status;

#if CAIRO_HAS_FT_FONT
    status = _cairo_ps_surface_emit_type1_font_subset (surface, font_subset);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return;
#endif

    status = _cairo_ps_surface_emit_truetype_font_subset (surface, font_subset);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return;

    status = _cairo_ps_surface_emit_type1_font_fallback (surface, font_subset);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return;

    status = _cairo_ps_surface_emit_type3_font_subset (surface, font_subset);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return;
}

static cairo_status_t
_cairo_ps_surface_emit_font_subsets (cairo_ps_surface_t *surface)
{
    cairo_status_t status;

    _cairo_output_stream_printf (surface->final_stream,
				 "%% _cairo_ps_surface_emit_font_subsets\n");

    status = _cairo_scaled_font_subsets_foreach (surface->font_subsets,
						 _cairo_ps_surface_emit_font_subset,
						 surface);
    _cairo_scaled_font_subsets_destroy (surface->font_subsets);
    surface->font_subsets = NULL;

    return status;
}

static void
_cairo_ps_surface_emit_body (cairo_ps_surface_t *surface)
{
    char    buf[4096];
    int	    n;

    rewind (surface->tmpfile);
    while ((n = fread (buf, 1, sizeof (buf), surface->tmpfile)) > 0)
	_cairo_output_stream_write (surface->final_stream, buf, n);
}

static void
_cairo_ps_surface_emit_footer (cairo_ps_surface_t *surface)
{
    _cairo_output_stream_printf (surface->final_stream,
				 "%%%%Trailer\n"
				 "%%%%EOF\n");
}

static cairo_surface_t *
_cairo_ps_surface_create_for_stream_internal (cairo_output_stream_t *stream,
					      double		     width,
					      double		     height)
{
    cairo_status_t status;
    cairo_ps_surface_t *surface = NULL;

    surface = malloc (sizeof (cairo_ps_surface_t));
    if (surface == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto CLEANUP;
    }

    _cairo_surface_init (&surface->base, &cairo_ps_surface_backend,
			 CAIRO_CONTENT_COLOR_ALPHA);

    surface->final_stream = stream;

    surface->tmpfile = tmpfile ();
    if (surface->tmpfile == NULL)
	goto CLEANUP_SURFACE;

    surface->stream = _cairo_output_stream_create_for_file (surface->tmpfile);
    status = _cairo_output_stream_get_status (surface->stream);
    if (status)
	goto CLEANUP_TMPFILE;

    surface->font_subsets = _cairo_scaled_font_subsets_create (PS_SURFACE_MAX_GLYPHS_PER_FONT);
    if (! surface->font_subsets)
	goto CLEANUP_OUTPUT_STREAM;

    surface->width  = width;
    surface->height = height;
    surface->max_width = width;
    surface->max_height = height;
    surface->paginated_mode = CAIRO_PAGINATED_MODE_ANALYZE;

    surface->num_pages = 0;

    _cairo_array_init (&surface->dsc_header_comments, sizeof (char *));
    _cairo_array_init (&surface->dsc_setup_comments, sizeof (char *));
    _cairo_array_init (&surface->dsc_page_setup_comments, sizeof (char *));

    surface->dsc_comment_target = &surface->dsc_header_comments;

    return _cairo_paginated_surface_create (&surface->base,
					    CAIRO_CONTENT_COLOR_ALPHA,
					    width, height,
					    &cairo_ps_surface_paginated_backend);

 CLEANUP_OUTPUT_STREAM:
    _cairo_output_stream_destroy (surface->stream);
 CLEANUP_TMPFILE:
    fclose (surface->tmpfile);
 CLEANUP_SURFACE:
    free (surface);
 CLEANUP:
    _cairo_error (CAIRO_STATUS_NO_MEMORY);
    return (cairo_surface_t*) &_cairo_surface_nil;
}

/**
 * cairo_ps_surface_create:
 * @filename: a filename for the PS output (must be writable)
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
 * owns the surface and should call cairo_surface_destroy when done
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
    cairo_status_t status;
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create_for_filename (filename);
    status = _cairo_output_stream_get_status (stream);
    if (status) {
	_cairo_error (status);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    return _cairo_ps_surface_create_for_stream_internal (stream,
							 width_in_points,
							 height_in_points);
}

/**
 * cairo_ps_surface_create_for_stream:
 * @write_func: a #cairo_write_func_t to accept the output data
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
 * owns the surface and should call cairo_surface_destroy when done
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
    cairo_status_t status;
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create (write_func, NULL, closure);
    status = _cairo_output_stream_get_status (stream);
    if (status) {
	_cairo_error (status);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

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
 * target. Otherwise return CAIRO_STATUS_SURFACE_TYPE_MISMATCH.
 */
static cairo_status_t
_extract_ps_surface (cairo_surface_t	 *surface,
		     cairo_ps_surface_t **ps_surface)
{
    cairo_surface_t *target;

    if (! _cairo_surface_is_paginated (surface))
	return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;

    target = _cairo_paginated_surface_get_target (surface);

    if (! _cairo_surface_is_ps (target))
	return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;

    *ps_surface = (cairo_ps_surface_t *) target;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_ps_surface_set_size:
 * @surface: a PostScript cairo_surface_t
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
    cairo_ps_surface_t *ps_surface;
    cairo_status_t status;

    status = _extract_ps_surface (surface, &ps_surface);
    if (status) {
	_cairo_surface_set_error (surface, CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    ps_surface->width = width_in_points;
    ps_surface->height = height_in_points;
}

/**
 * cairo_ps_surface_dsc_comment:
 * @surface: a PostScript cairo_surface_t
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
 * cairo_surface_t *surface = cairo_ps_surface_create (filename, width, height);
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
    cairo_ps_surface_t *ps_surface;
    cairo_status_t status;
    char *comment_copy;

    status = _extract_ps_surface (surface, &ps_surface);
    if (status) {
	_cairo_surface_set_error (surface, CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    /* A couple of sanity checks on the comment value. */
    if (comment == NULL) {
	_cairo_surface_set_error (surface, CAIRO_STATUS_NULL_POINTER);
	return;
    }

    if (comment[0] != '%' || strlen (comment) > 255) {
	_cairo_surface_set_error (surface, CAIRO_STATUS_INVALID_DSC_COMMENT);
	return;
    }

    /* Then, copy the comment and store it in the appropriate array. */
    comment_copy = strdup (comment);
    if (comment_copy == NULL) {
	_cairo_surface_set_error (surface, CAIRO_STATUS_NO_MEMORY);
	return;
    }

    status = _cairo_array_append (ps_surface->dsc_comment_target, &comment_copy);
    if (status) {
	free (comment_copy);
	_cairo_surface_set_error (surface, status);
	return;
    }
}

/**
 * cairo_ps_surface_dsc_begin_setup:
 * @surface: a PostScript cairo_surface_t
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
    cairo_ps_surface_t *ps_surface;
    cairo_status_t status;

    status = _extract_ps_surface (surface, &ps_surface);
    if (status) {
	_cairo_surface_set_error (surface, CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    if (ps_surface->dsc_comment_target == &ps_surface->dsc_header_comments)
    {
	ps_surface->dsc_comment_target = &ps_surface->dsc_setup_comments;
    }
}

/**
 * cairo_ps_surface_dsc_begin_page_setup:
 * @surface: a PostScript cairo_surface_t
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
    cairo_ps_surface_t *ps_surface;
    cairo_status_t status;

    status = _extract_ps_surface (surface, &ps_surface);
    if (status) {
	_cairo_surface_set_error (surface, CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    if (ps_surface->dsc_comment_target == &ps_surface->dsc_header_comments ||
	ps_surface->dsc_comment_target == &ps_surface->dsc_setup_comments)
    {
	ps_surface->dsc_comment_target = &ps_surface->dsc_page_setup_comments;
    }
}

static cairo_status_t
_cairo_ps_surface_finish (void *abstract_surface)
{
    cairo_status_t status;
    cairo_ps_surface_t *surface = abstract_surface;
    int i, num_comments;
    char **comments;

    _cairo_ps_surface_emit_header (surface);

    _cairo_ps_surface_emit_font_subsets (surface);

    _cairo_ps_surface_emit_body (surface);

    _cairo_ps_surface_emit_footer (surface);

    _cairo_output_stream_close (surface->stream);
    status = _cairo_output_stream_get_status (surface->stream);
    _cairo_output_stream_destroy (surface->stream);

    fclose (surface->tmpfile);

    _cairo_output_stream_close (surface->final_stream);
    if (status == CAIRO_STATUS_SUCCESS)
	status = _cairo_output_stream_get_status (surface->final_stream);
    _cairo_output_stream_destroy (surface->final_stream);

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

    return status;
}

static cairo_int_status_t
_cairo_ps_surface_start_page (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;
    int i, num_comments;
    char **comments;

    /* Increment before print so page numbers start at 1. */
    surface->num_pages++;
    _cairo_output_stream_printf (surface->stream,
				 "%%%%Page: %d %d\n",
				 surface->num_pages,
				 surface->num_pages);

    _cairo_output_stream_printf (surface->stream,
				 "%%%%BeginPageSetup\n");

    num_comments = _cairo_array_num_elements (&surface->dsc_page_setup_comments);
    comments = _cairo_array_index (&surface->dsc_page_setup_comments, 0);
    for (i = 0; i < num_comments; i++) {
	_cairo_output_stream_printf (surface->stream,
				     "%s\n", comments[i]);
	free (comments[i]);
	comments[i] = NULL;
    }
    _cairo_array_truncate (&surface->dsc_page_setup_comments, 0);

    _cairo_output_stream_printf (surface->stream,
				 "%%%%PageBoundingBox: %d %d %d %d\n",
				 0, 0,
				 (int) ceil (surface->width),
				 (int) ceil (surface->height));

    _cairo_output_stream_printf (surface->stream,
				 "gsave %f %f translate 1.0 -1.0 scale \n",
				 0.0, surface->height);

    _cairo_output_stream_printf (surface->stream,
				 "%%%%EndPageSetup\n");

    if (surface->width > surface->max_width)
	surface->max_width = surface->width;
    if (surface->height > surface->max_height)
	surface->max_height = surface->height;

    return _cairo_output_stream_get_status (surface->stream);
}

static void
_cairo_ps_surface_end_page (cairo_ps_surface_t *surface)
{
    _cairo_output_stream_printf (surface->stream,
				 "grestore\n");
}

static cairo_int_status_t
_cairo_ps_surface_copy_page (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

    _cairo_ps_surface_end_page (surface);

    _cairo_output_stream_printf (surface->stream, "copypage\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_show_page (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

    _cairo_ps_surface_end_page (surface);

    _cairo_output_stream_printf (surface->stream, "showpage\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
color_is_gray (cairo_color_t *color)
{
    const double epsilon = 0.00001;

    return (fabs (color->red - color->green) < epsilon &&
	    fabs (color->red - color->blue) < epsilon);
}

static cairo_bool_t
surface_pattern_supported (const cairo_surface_pattern_t *pattern)
{
    if (pattern->surface->backend->acquire_source_image != NULL)
	return TRUE;

    return FALSE;
}

static cairo_bool_t
pattern_supported (const cairo_pattern_t *pattern)
{
    if (pattern->type == CAIRO_PATTERN_TYPE_SOLID)
	return TRUE;

    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE)
	return surface_pattern_supported ((const cairo_surface_pattern_t *) pattern);

    return FALSE;
}

static cairo_bool_t cairo_ps_force_fallbacks = FALSE;

/**
 * _cairo_ps_test_force_fallbacks
 *
 * Force the PS surface backend to use image fallbacks for every
 * operation.
 *
 * <note>
 * This function is <emphasis>only</emphasis> intended for internal
 * testing use within the cairo distribution. It is not installed in
 * any public header file.
 * </note>
 **/
void
_cairo_ps_test_force_fallbacks (void)
{
    cairo_ps_force_fallbacks = TRUE;
}

static cairo_int_status_t
operation_supported (cairo_ps_surface_t *surface,
		      cairo_operator_t op,
		      const cairo_pattern_t *pattern)
{
    if (cairo_ps_force_fallbacks)
	return FALSE;

    if (! pattern_supported (pattern))
	return FALSE;

    if (_cairo_operator_always_opaque (op))
	return TRUE;

    if (_cairo_operator_always_translucent (op))
	return FALSE;

    return _cairo_pattern_is_opaque (pattern);
}

static cairo_int_status_t
_analyze_operation (cairo_ps_surface_t *surface,
		    cairo_operator_t op,
		    const cairo_pattern_t *pattern)
{
    if (operation_supported (surface, op, pattern))
	return CAIRO_STATUS_SUCCESS;
    else
	return CAIRO_INT_STATUS_UNSUPPORTED;
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
	if (stream->string_size == 0) {
	    _cairo_output_stream_printf (stream->output, "(");
	    stream->column++;
	}

	c = *data++;
	switch (c) {
	case '\\':
	case '(':
	case ')':
	    _cairo_output_stream_write (stream->output, &backslash, 1);
	    stream->column++;
	    stream->string_size++;
	    break;
	/* Have to also be careful to never split the final ~> sequence. */
	case '~':
	    _cairo_output_stream_write (stream->output, &c, 1);
	    stream->column++;
	    stream->string_size++;
	    length--;
	    c = *data++;
	    break;
	}
	_cairo_output_stream_write (stream->output, &c, 1);
	stream->column++;
	stream->string_size++;

	if (stream->string_size >= STRING_ARRAY_MAX_STRING_SIZE) {
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
    if (stream == NULL)
	return (cairo_output_stream_t *) &cairo_output_stream_nil;

    _cairo_output_stream_init (&stream->base,
			       _string_array_stream_write,
			       _string_array_stream_close);
    stream->output = output;
    stream->column = 0;
    stream->string_size = 0;

    return &stream->base;
}

/* PS Output - this section handles output of the parts of the meta
 * surface we can render natively in PS. */

static cairo_status_t
emit_image (cairo_ps_surface_t    *surface,
	    cairo_image_surface_t *image,
	    cairo_matrix_t	  *matrix,
	    const char		  *name)
{
    cairo_status_t status;
    unsigned char *rgb, *compressed;
    unsigned long rgb_size, compressed_size;
    cairo_surface_t *opaque;
    cairo_image_surface_t *opaque_image;
    cairo_pattern_union_t pattern;
    int x, y, i;
    cairo_output_stream_t *base85_stream, *string_array_stream;

    /* PostScript can not represent the alpha channel, so we blend the
       current image over a white RGB surface to eliminate it. */

    if (image->base.status)
	return image->base.status;

    if (image->format != CAIRO_FORMAT_RGB24) {
	opaque = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
					     image->width,
					     image->height);
	if (opaque->status) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto bail0;
	}

	_cairo_pattern_init_for_surface (&pattern.surface, &image->base);

	_cairo_surface_fill_rectangle (opaque,
				       CAIRO_OPERATOR_SOURCE,
				       CAIRO_COLOR_WHITE,
				       0, 0, image->width, image->height);

	_cairo_surface_composite (CAIRO_OPERATOR_OVER,
				  &pattern.base,
				  NULL,
				  opaque,
				  0, 0,
				  0, 0,
				  0, 0,
				  image->width,
				  image->height);

	_cairo_pattern_fini (&pattern.base);
	opaque_image = (cairo_image_surface_t *) opaque;
    } else {
	opaque = &image->base;
	opaque_image = image;
    }

    rgb_size = 3 * opaque_image->width * opaque_image->height;
    rgb = malloc (rgb_size);
    if (rgb == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto bail1;
    }

    i = 0;
    for (y = 0; y < opaque_image->height; y++) {
	pixman_bits_t *pixel = (pixman_bits_t *) (opaque_image->data + y * opaque_image->stride);
	for (x = 0; x < opaque_image->width; x++, pixel++) {
	    rgb[i++] = (*pixel & 0x00ff0000) >> 16;
	    rgb[i++] = (*pixel & 0x0000ff00) >>  8;
	    rgb[i++] = (*pixel & 0x000000ff) >>  0;
	}
    }

    /* XXX: Should fix cairo-lzw to provide a stream-based interface
     * instead. */
    compressed_size = rgb_size;
    compressed = _cairo_lzw_compress (rgb, &compressed_size);
    if (compressed == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto bail2;
    }

    /* First emit the image data as a base85-encoded string which will
     * be used as the data source for the image operator later. */
    _cairo_output_stream_printf (surface->stream,
				 "/%sData [\n", name);

    string_array_stream = _string_array_stream_create (surface->stream);
    base85_stream = _cairo_base85_stream_create (string_array_stream);

    _cairo_output_stream_write (base85_stream, compressed, compressed_size);

    _cairo_output_stream_destroy (base85_stream);
    _cairo_output_stream_destroy (string_array_stream);

    _cairo_output_stream_printf (surface->stream,
				 "] def\n");
    _cairo_output_stream_printf (surface->stream,
				 "/%sDataIndex 0 def\n", name);

    _cairo_output_stream_printf (surface->stream,
				 "/%s {\n"
				 "    /DeviceRGB setcolorspace\n"
				 "    <<\n"
				 "	/ImageType 1\n"
				 "	/Width %d\n"
				 "	/Height %d\n"
				 "	/BitsPerComponent 8\n"
				 "	/Decode [ 0 1 0 1 0 1 ]\n"
				 "	/DataSource {\n"
				 "	    %sData %sDataIndex get\n"
				 "	    /%sDataIndex %sDataIndex 1 add def\n"
				 "	    %sDataIndex %sData length 1 sub gt { /%sDataIndex 0 def } if\n"
				 "	} /ASCII85Decode filter /LZWDecode filter\n"
				 "	/ImageMatrix [ %f %f %f %f %f %f ]\n"
				 "    >>\n"
				 "    image\n"
				 "} def\n",
				 name,
				 opaque_image->width,
				 opaque_image->height,
				 name, name, name, name, name, name, name,
				 matrix->xx, matrix->yx,
				 matrix->xy, matrix->yy,
				 0.0, 0.0);

    status = CAIRO_STATUS_SUCCESS;

    free (compressed);
 bail2:
    free (rgb);
 bail1:
    if (opaque_image != image)
	cairo_surface_destroy (opaque);
 bail0:
    return status;
}

static void
emit_solid_pattern (cairo_ps_surface_t *surface,
		    cairo_solid_pattern_t *pattern)
{
    if (color_is_gray (&pattern->color))
	_cairo_output_stream_printf (surface->stream,
				     "%f G\n",
				     pattern->color.red);
    else
	_cairo_output_stream_printf (surface->stream,
				     "%f %f %f R\n",
				     pattern->color.red,
				     pattern->color.green,
				     pattern->color.blue);
}

static void
emit_surface_pattern (cairo_ps_surface_t *surface,
		      cairo_surface_pattern_t *pattern)
{
    double bbox_width, bbox_height;
    int xstep, ystep;
    cairo_matrix_t inverse = pattern->base.matrix;
    cairo_matrix_invert (&inverse);

    if (_cairo_surface_is_meta (pattern->surface)) {
	_cairo_output_stream_printf (surface->stream, "/MyPattern {\n");
	_cairo_meta_surface_replay (pattern->surface, &surface->base);
	bbox_width = surface->width;
	bbox_height = surface->height;
	xstep = surface->width;
	ystep = surface->height;
	_cairo_output_stream_printf (surface->stream, "} bind def\n");
    } else {
	cairo_image_surface_t	*image;
	void			*image_extra;
	cairo_status_t		status;

	status = _cairo_surface_acquire_source_image (pattern->surface,
						      &image,
						      &image_extra);
	assert (status == CAIRO_STATUS_SUCCESS);

	emit_image (surface, image, &pattern->base.matrix, "MyPattern");

	bbox_width = image->width;
	bbox_height = image->height;
	cairo_matrix_transform_distance (&inverse,
					 &bbox_width, &bbox_height);

	/* In PostScript, (as far as I can tell), all patterns are
	 * repeating. So we support cairo's EXTEND_NONE semantics by
	 * setting the repeat step size to the larger of the image size
	 * and the extents of the destination surface. That way we
	 * guarantee the pattern will not repeat.
	 */
	switch (pattern->base.extend) {
	case CAIRO_EXTEND_NONE:
	    xstep = MAX (image->width, surface->width);
	    ystep = MAX (image->height, surface->height);
	    break;
	case CAIRO_EXTEND_REPEAT:
	    xstep = image->width;
	    ystep = image->height;
	    break;
	/* All the rest should have been analyzed away, so these cases
	 * should be unreachable. */
	case CAIRO_EXTEND_REFLECT:
	case CAIRO_EXTEND_PAD:
	default:
	    ASSERT_NOT_REACHED;
	    xstep = 0;
	    ystep = 0;
	}

	_cairo_surface_release_source_image (pattern->surface, image,
					     image_extra);
    }
    _cairo_output_stream_printf (surface->stream,
				 "<< /PatternType 1\n"
				 "   /PaintType 1\n"
				 "   /TilingType 1\n");
    _cairo_output_stream_printf (surface->stream,
				 "   /BBox [0 0 %d %d]\n",
				 (int) bbox_width, (int) bbox_height);
    _cairo_output_stream_printf (surface->stream,
				 "   /XStep %d /YStep %d\n",
				 xstep, ystep);
    _cairo_output_stream_printf (surface->stream,
				 "   /PaintProc { MyPattern } bind\n"
				 ">>\n");
    _cairo_output_stream_printf (surface->stream,
				 "[ 1 0 0 1 %f %f ]\n",
				 inverse.x0, inverse.y0);
    _cairo_output_stream_printf (surface->stream,
				 "makepattern setpattern\n");
}

static void
emit_linear_pattern (cairo_ps_surface_t *surface,
		     cairo_linear_pattern_t *pattern)
{
    /* XXX: NYI */
}

static void
emit_radial_pattern (cairo_ps_surface_t *surface,
		     cairo_radial_pattern_t *pattern)
{
    /* XXX: NYI */
}

static void
emit_pattern (cairo_ps_surface_t *surface, cairo_pattern_t *pattern)
{
    /* FIXME: We should keep track of what pattern is currently set in
     * the postscript file and only emit code if we're setting a
     * different pattern. */

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	emit_solid_pattern (surface, (cairo_solid_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_TYPE_SURFACE:
	emit_surface_pattern (surface, (cairo_surface_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_TYPE_LINEAR:
	emit_linear_pattern (surface, (cairo_linear_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_TYPE_RADIAL:
	emit_radial_pattern (surface, (cairo_radial_pattern_t *) pattern);
	break;
    }
}

static cairo_int_status_t
_cairo_ps_surface_intersect_clip_path (void		   *abstract_surface,
				cairo_path_fixed_t *path,
				cairo_fill_rule_t   fill_rule,
				double		    tolerance,
				cairo_antialias_t   antialias)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    cairo_status_t status;
    const char *ps_operator;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return CAIRO_STATUS_SUCCESS;

    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_intersect_clip_path\n");

    if (path == NULL) {
	_cairo_output_stream_printf (stream, "initclip\n");
	return CAIRO_STATUS_SUCCESS;
    }

    /* We're "filling" not stroking, so we pass CAIRO_LINE_CAP_ROUND. */
    status = _cairo_ps_surface_emit_path (surface, stream, path,
					  CAIRO_LINE_CAP_ROUND);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	ps_operator = "clip";
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	ps_operator = "eoclip";
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    _cairo_output_stream_printf (stream,
				 "%s newpath\n",
				 ps_operator);

    return status;
}

static cairo_int_status_t
_cairo_ps_surface_get_extents (void		       *abstract_surface,
			       cairo_rectangle_int16_t *rectangle)
{
    cairo_ps_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;

    /* XXX: The conversion to integers here is pretty bogus, (not to
     * mention the aribitray limitation of width to a short(!). We
     * may need to come up with a better interface for get_extents.
     */
    rectangle->width  = (int) ceil (surface->width);
    rectangle->height = (int) ceil (surface->height);

    return CAIRO_STATUS_SUCCESS;
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
			 cairo_pattern_t	*source)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    cairo_rectangle_int16_t extents, pattern_extents;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _analyze_operation (surface, op, source);

    /* XXX: It would be nice to be able to assert this condition
     * here. But, we actually allow one 'cheat' that is used when
     * painting the final image-based fallbacks. The final fallbacks
     * do have alpha which we support by blending with white. This is
     * possible only because there is nothing between the fallback
     * images and the paper, nor is anything painted above. */
    /*
    assert (_operation_supported (op, source));
    */

    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_paint\n");

    _cairo_surface_get_extents (&surface->base, &extents);
    _cairo_pattern_get_extents (source, &pattern_extents);
    _cairo_rectangle_intersect (&extents, &pattern_extents);

    emit_pattern (surface, source);

    _cairo_output_stream_printf (stream, "%d %d M\n",
				 extents.x, extents.y);
    _cairo_output_stream_printf (stream, "%d %d L\n",
				 extents.x + extents.width,
				 extents.y);
    _cairo_output_stream_printf (stream, "%d %d L\n",
				 extents.x + extents.width,
				 extents.y + extents.height);
    _cairo_output_stream_printf (stream, "%d %d L\n",
				 extents.x,
				 extents.y + extents.height);
    _cairo_output_stream_printf (stream, "P F\n");
    return CAIRO_STATUS_SUCCESS;
}

static int
_cairo_ps_line_cap (cairo_line_cap_t cap)
{
    switch (cap) {
    case CAIRO_LINE_CAP_BUTT:
	return 0;
    case CAIRO_LINE_CAP_ROUND:
	return 1;
    case CAIRO_LINE_CAP_SQUARE:
	return 2;
    default:
	ASSERT_NOT_REACHED;
	return 0;
    }
}

static int
_cairo_ps_line_join (cairo_line_join_t join)
{
    switch (join) {
    case CAIRO_LINE_JOIN_MITER:
	return 0;
    case CAIRO_LINE_JOIN_ROUND:
	return 1;
    case CAIRO_LINE_JOIN_BEVEL:
	return 2;
    default:
	ASSERT_NOT_REACHED;
	return 0;
    }
}

static cairo_int_status_t
_cairo_ps_surface_stroke (void			*abstract_surface,
			  cairo_operator_t	 op,
			  cairo_pattern_t	*source,
			  cairo_path_fixed_t	*path,
			  cairo_stroke_style_t	*style,
			  cairo_matrix_t	*ctm,
			  cairo_matrix_t	*ctm_inverse,
			  double		 tolerance,
			  cairo_antialias_t	 antialias)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    cairo_int_status_t status;
    double *dash = style->dash;
    int num_dashes = style->num_dashes;
    double dash_offset = style->dash_offset;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _analyze_operation (surface, op, source);

    assert (operation_supported (surface, op, source));


    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_stroke\n");

    /* PostScript has "special needs" when it comes to zero-length
     * dash segments with butt caps. It apparently (at least
     * according to ghostscript) draws hairlines for this
     * case. That's not what the cairo semantics want, so we first
     * touch up the array to eliminate any 0.0 values that will
     * result in "on" segments.
     */
    if (num_dashes && style->line_cap == CAIRO_LINE_CAP_BUTT) {
	int i;

	/* If there's an odd number of dash values they will each get
	 * interpreted as both on and off. So we first explicitly
	 * expand the array to remove the duplicate usage so that we
	 * can modify some of the values.
	 */
	if (num_dashes % 2) {
	    dash = malloc (2 * num_dashes * sizeof (double));
	    if (dash == NULL)
		return CAIRO_STATUS_NO_MEMORY;

	    memcpy (dash, style->dash, num_dashes * sizeof (double));
	    memcpy (dash + num_dashes, style->dash, num_dashes * sizeof (double));

	    num_dashes *= 2;
	}

	for (i = 0; i < num_dashes; i += 2) {
	    if (dash[i] == 0.0) {
		/* If we're at the front of the list, we first rotate
		 * two elements from the end of the list to the front
		 * of the list before folding away the 0.0. Or, if
		 * there are only two dash elements, then there is
		 * nothing at all to draw.
		 */
		if (i == 0) {
		    double last_two[2];

		    if (num_dashes == 2) {
			if (dash != style->dash)
			    free (dash);
			return CAIRO_STATUS_SUCCESS;
		    }
		    /* The cases of num_dashes == 0, 1, or 3 elements
		     * cannot exist, so the rotation of 2 elements
		     * will always be safe */
		    memcpy (last_two, dash + num_dashes - 2, sizeof (last_two));
		    memmove (dash + 2, dash, (num_dashes - 2) * sizeof (double));
		    memcpy (dash, last_two, sizeof (last_two));
		    dash_offset += dash[0] + dash[1];
		    i = 2;
		}
		dash[i-1] += dash[i+1];
		num_dashes -= 2;
		memmove (dash + i, dash + i + 2, (num_dashes - i) * sizeof (double));
		/* If we might have just rotated, it's possible that
		 * we rotated a 0.0 value to the front of the list.
		 * Set i to -2 so it will get incremented to 0. */
		if (i == 2)
		    i = -2;
	    }
	}
    }

    emit_pattern (surface, source);

    _cairo_output_stream_printf (stream,
				 "gsave\n");
    status = _cairo_ps_surface_emit_path (surface, stream, path,
					  style->line_cap);

    /*
     * Switch to user space to set line parameters
     */
    _cairo_output_stream_printf (stream,
				 "[%f %f %f %f 0 0] concat\n",
				 ctm->xx, ctm->yx, ctm->xy, ctm->yy);
    /* line width */
    _cairo_output_stream_printf (stream, "%f setlinewidth\n",
				 style->line_width);
    /* line cap */
    _cairo_output_stream_printf (stream, "%d setlinecap\n",
				 _cairo_ps_line_cap (style->line_cap));
    /* line join */
    _cairo_output_stream_printf (stream, "%d setlinejoin\n",
				 _cairo_ps_line_join (style->line_join));
    /* dashes */
    if (num_dashes) {
	int d;

	_cairo_output_stream_printf (stream, "[");
	for (d = 0; d < num_dashes; d++)
	    _cairo_output_stream_printf (stream, " %f", dash[d]);
	_cairo_output_stream_printf (stream, "] %f setdash\n",
				     dash_offset);
    }
    if (dash != style->dash)
	free (dash);

    /* miter limit */
    _cairo_output_stream_printf (stream, "%f setmiterlimit\n",
				 style->miter_limit);
    _cairo_output_stream_printf (stream,
				 "stroke\n");
    _cairo_output_stream_printf (stream,
				 "grestore\n");
    return status;
}

static cairo_int_status_t
_cairo_ps_surface_fill (void		*abstract_surface,
		 cairo_operator_t	 op,
		 cairo_pattern_t	*source,
		 cairo_path_fixed_t	*path,
		 cairo_fill_rule_t	 fill_rule,
		 double			 tolerance,
		 cairo_antialias_t	 antialias)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    cairo_int_status_t status;
    const char *ps_operator;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _analyze_operation (surface, op, source);

    assert (operation_supported (surface, op, source));

    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_fill\n");

    emit_pattern (surface, source);

    /* We're filling not stroking, so we pass CAIRO_LINE_CAP_ROUND. */
    status = _cairo_ps_surface_emit_path (surface, stream, path,
					  CAIRO_LINE_CAP_ROUND);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	ps_operator = "F";
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	ps_operator = "eofill";
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    _cairo_output_stream_printf (stream,
				 "%s\n", ps_operator);

    return status;
}

/* This size keeps the length of the hex encoded string of glyphs
 * within 80 columns. */
#define MAX_GLYPHS_PER_SHOW  36

typedef struct _cairo_ps_glyph_id {
    unsigned int subset_id;
    unsigned int glyph_id;
} cairo_ps_glyph_id_t;

static cairo_int_status_t
_cairo_ps_surface_show_glyphs (void		     *abstract_surface,
			       cairo_operator_t	      op,
			       cairo_pattern_t	     *source,
			       cairo_glyph_t         *glyphs,
			       int		      num_glyphs,
			       cairo_scaled_font_t   *scaled_font)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    unsigned int current_subset_id = -1;
    unsigned int font_id;
    cairo_ps_glyph_id_t *glyph_ids;
    cairo_status_t status;
    unsigned int num_glyphs_unsigned, i, j, last, end;
    cairo_bool_t vertical, horizontal;
    cairo_output_stream_t *word_wrap;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return _analyze_operation (surface, op, source);

    assert (operation_supported (surface, op, source));

    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_show_glyphs\n");

    if (num_glyphs <= 0)
        return CAIRO_STATUS_SUCCESS;

    num_glyphs_unsigned = num_glyphs;

    emit_pattern (surface, source);
    glyph_ids = malloc (num_glyphs_unsigned*sizeof (cairo_ps_glyph_id_t));
    if (glyph_ids == NULL)
        return CAIRO_STATUS_NO_MEMORY;

    for (i = 0; i < num_glyphs_unsigned; i++) {
        status = _cairo_scaled_font_subsets_map_glyph (surface->font_subsets,
                                                       scaled_font, glyphs[i].index,
                                                       &font_id,
                                                       &(glyph_ids[i].subset_id),
                                                       &(glyph_ids[i].glyph_id));
        if (status)
            goto fail;
    }

    i = 0;
    while (i < num_glyphs_unsigned) {
        if (glyph_ids[i].subset_id != current_subset_id) {
            _cairo_output_stream_printf (surface->stream,
                                         "/CairoFont-%d-%d findfont\n"
                                         "[ %f %f %f %f 0 0 ] makefont\n"
                                         "setfont\n",
                                         font_id,
                                         glyph_ids[i].subset_id,
                                         scaled_font->scale.xx,
                                         scaled_font->scale.yx,
                                         -scaled_font->scale.xy,
                                         -scaled_font->scale.yy);
            current_subset_id = glyph_ids[i].subset_id;
        }

        if (i == 0)
            _cairo_output_stream_printf (stream,
                                         "%f %f M\n",
                                         glyphs[i].x,
                                         glyphs[i].y);

        horizontal = TRUE;
        vertical = TRUE;
        end = num_glyphs_unsigned;
        if (end - i > MAX_GLYPHS_PER_SHOW)
            end = i + MAX_GLYPHS_PER_SHOW;
        last = end - 1;
        for (j = i; j < end - 1; j++) {
            if ((glyphs[j].y != glyphs[j + 1].y))
                horizontal = FALSE;
            if ((glyphs[j].x != glyphs[j + 1].x))
                vertical = FALSE;
            if (glyph_ids[j].subset_id != glyph_ids[j + 1].subset_id) {
                last = j;
                break;
            }
        }

        if (i == last) {
            _cairo_output_stream_printf (surface->stream, "<%02x> S\n", glyph_ids[i].glyph_id);
        } else {
            word_wrap = _word_wrap_stream_create (surface->stream, 79);
            _cairo_output_stream_printf (word_wrap, "<");
            for (j = i; j < last+1; j++)
                _cairo_output_stream_printf (word_wrap, "%02x", glyph_ids[j].glyph_id);
            _cairo_output_stream_printf (word_wrap, ">\n[");

            if (horizontal) {
                for (j = i; j < last+1; j++) {
                    if (j == num_glyphs_unsigned - 1)
                        _cairo_output_stream_printf (word_wrap, "0 ");
                    else
                        _cairo_output_stream_printf (word_wrap,
                                                     "%f ", glyphs[j + 1].x - glyphs[j].x);
                }
                _cairo_output_stream_printf (word_wrap, "] xS\n");
            } else if (vertical) {
                for (j = i; j < last+1; j++) {
                    if (j == num_glyphs_unsigned - 1)
                        _cairo_output_stream_printf (word_wrap, "0 ");
                    else
                        _cairo_output_stream_printf (word_wrap,
                                                     "%f ", glyphs[j + 1].y - glyphs[j].y);
                }
                _cairo_output_stream_printf (word_wrap, "] yS\n");
            } else {
                for (j = i; j < last+1; j++) {
                    if (j == num_glyphs_unsigned - 1)
                        _cairo_output_stream_printf (word_wrap, "0 ");
                    else
                        _cairo_output_stream_printf (word_wrap,
                                                     "%f %f ",
                                                     glyphs[j + 1].x - glyphs[j].x,
                                                     glyphs[j + 1].y - glyphs[j].y);
                }
                _cairo_output_stream_printf (word_wrap, "] xyS\n");
            }

            status = _cairo_output_stream_destroy (word_wrap);
            if (status)
                goto fail;
        }
        i = last + 1;
    }

    status = _cairo_output_stream_get_status (surface->stream);
fail:
    free (glyph_ids);

    return status;
}

static void
_cairo_ps_surface_set_paginated_mode (void			*abstract_surface,
				      cairo_paginated_mode_t	 paginated_mode)
{
    cairo_ps_surface_t *surface = abstract_surface;

    surface->paginated_mode = paginated_mode;
}

static const cairo_surface_backend_t cairo_ps_surface_backend = {
    CAIRO_SURFACE_TYPE_PS,
    NULL, /* create_similar */
    _cairo_ps_surface_finish,
    NULL, /* acquire_source_image */
    NULL, /* release_source_image */
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    _cairo_ps_surface_copy_page,
    _cairo_ps_surface_show_page,
    NULL, /* set_clip_region */
    _cairo_ps_surface_intersect_clip_path,
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
    _cairo_ps_surface_set_paginated_mode
};
