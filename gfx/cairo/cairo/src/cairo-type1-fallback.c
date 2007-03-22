/* cairo - a vector graphics library with display and print output
 *
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Adrian Johnson <ajohnson@redneon.com>
 */

#include "cairoint.h"
#include "cairo-scaled-font-subsets-private.h"
#include "cairo-path-fixed-private.h"
#include "cairo-output-stream-private.h"

typedef struct _cairo_type1_font {
    int *widths;

    cairo_scaled_font_subset_t *scaled_font_subset;
    cairo_scaled_font_t        *type1_scaled_font;

    cairo_array_t contents;

    double x_min, y_min, x_max, y_max;

    const char    *data;
    unsigned long  header_size;
    unsigned long  data_size;
    unsigned long  trailer_size;
    int            bbox_position;
    int            bbox_max_chars;

    cairo_output_stream_t *output;

    unsigned short eexec_key;
    cairo_bool_t hex_encode;
    int hex_column;
} cairo_type1_font_t;

static cairo_status_t
cairo_type1_font_create (cairo_scaled_font_subset_t  *scaled_font_subset,
                         cairo_type1_font_t         **subset_return,
                         cairo_bool_t                 hex_encode)
{
    cairo_type1_font_t *font;
    cairo_font_face_t *font_face;
    cairo_matrix_t font_matrix;
    cairo_matrix_t ctm;
    cairo_font_options_t font_options;

    font = calloc (1, sizeof (cairo_type1_font_t));
    if (font == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    font->widths = calloc (scaled_font_subset->num_glyphs,
                           sizeof (int));
    if (font->widths == NULL) {
	free (font);
	return CAIRO_STATUS_NO_MEMORY;
    }

    font->scaled_font_subset = scaled_font_subset;
    font->hex_encode = hex_encode;

    font_face = cairo_scaled_font_get_font_face (scaled_font_subset->scaled_font);

    cairo_matrix_init_scale (&font_matrix, 1000, 1000);
    cairo_matrix_init_identity (&ctm);

    _cairo_font_options_init_default (&font_options);
    cairo_font_options_set_hint_style (&font_options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics (&font_options, CAIRO_HINT_METRICS_OFF);

    font->type1_scaled_font = cairo_scaled_font_create (font_face,
							&font_matrix,
							&ctm,
							&font_options);
    if (font->type1_scaled_font == NULL)
        goto fail;

    _cairo_array_init (&font->contents, sizeof (unsigned char));

    *subset_return = font;

    return CAIRO_STATUS_SUCCESS;

fail:
    free (font->widths);
    free (font);

    return CAIRO_STATUS_NO_MEMORY;
}

/* Magic constants for the type1 eexec encryption */
static const unsigned short encrypt_c1 = 52845, encrypt_c2 = 22719;
static const unsigned short private_dict_key = 55665;
static const unsigned short charstring_key = 4330;

/* Charstring commands. If the high byte is 0 the command is encoded
 * with a single byte. */
#define CHARSTRING_sbw        0x0c07
#define CHARSTRING_rmoveto    0x0015
#define CHARSTRING_rlineto    0x0005
#define CHARSTRING_rcurveto   0x0008
#define CHARSTRING_closepath  0x0009
#define CHARSTRING_endchar    0x000e

/* Before calling this function, the caller must allocate sufficient
 * space in data (see _cairo_array_grow_by). The maximum number of
 * bytes that will be used is 2.
 */
static void
charstring_encode_command (cairo_array_t *data, int command)
{
    cairo_status_t status;
    int orig_size;
    unsigned char buf[5];
    unsigned char *p = buf;

    if (command & 0xff00)
        *p++ = command >> 8;
    *p++ = command & 0x00ff;

    /* Ensure the array doesn't grow, which allows this function to
     * have no possibility of failure. */
    orig_size = _cairo_array_size (data);
    status = _cairo_array_append_multiple (data, buf, p - buf);

    assert (status == CAIRO_STATUS_SUCCESS);
    assert (_cairo_array_size (data) == orig_size);
}

/* Before calling this function, the caller must allocate sufficient
 * space in data (see _cairo_array_grow_by). The maximum number of
 * bytes that will be used is 5.
 */
static void
charstring_encode_integer (cairo_array_t *data, int i)
{
    cairo_status_t status;
    int orig_size;
    unsigned char buf[10];
    unsigned char *p = buf;

    if (i >= -107 && i <= 107) {
        *p++ = i + 139;
    } else if (i >= 108 && i <= 1131) {
        i -= 108;
        *p++ = (i >> 8)+ 247;
        *p++ = i & 0xff;
    } else if (i >= -1131 && i <= -108) {
        i = -i - 108;
        *p++ = (i >> 8)+ 251;
        *p++ = i & 0xff;
    } else {
        *p++ = 0xff;
        *p++ = i >> 24;
        *p++ = (i >> 16) & 0xff;
        *p++ = (i >> 8)  & 0xff;
        *p++ = i & 0xff;
    }

    /* Ensure the array doesn't grow, which allows this function to
     * have no possibility of failure. */
    orig_size = _cairo_array_size (data);
    status = _cairo_array_append_multiple (data, buf, p - buf);

    assert (status == CAIRO_STATUS_SUCCESS);
    assert (_cairo_array_size (data) == orig_size);
}

typedef struct _ps_path_info {
    cairo_array_t *data;
    int current_x, current_y;
} t1_path_info_t;

static cairo_status_t
_charstring_move_to (void          *closure,
                     cairo_point_t *point)
{
    t1_path_info_t *path_info = (t1_path_info_t *) closure;
    int dx, dy;
    cairo_status_t status;

    status = _cairo_array_grow_by (path_info->data, 12);
    if (status)
        return status;

    dx = _cairo_fixed_integer_part (point->x) - path_info->current_x;
    dy = _cairo_fixed_integer_part (point->y) - path_info->current_y;
    charstring_encode_integer (path_info->data, dx);
    charstring_encode_integer (path_info->data, dy);
    path_info->current_x += dx;
    path_info->current_y += dy;

    charstring_encode_command (path_info->data, CHARSTRING_rmoveto);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_charstring_line_to (void          *closure,
                     cairo_point_t *point)
{
    t1_path_info_t *path_info = (t1_path_info_t *) closure;
    int dx, dy;
    cairo_status_t status;

    status = _cairo_array_grow_by (path_info->data, 12);
    if (status)
        return status;

    dx = _cairo_fixed_integer_part (point->x) - path_info->current_x;
    dy = _cairo_fixed_integer_part (point->y) - path_info->current_y;
    charstring_encode_integer (path_info->data, dx);
    charstring_encode_integer (path_info->data, dy);
    path_info->current_x += dx;
    path_info->current_y += dy;

    charstring_encode_command (path_info->data, CHARSTRING_rlineto);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_charstring_curve_to (void	    *closure,
                      cairo_point_t *point1,
                      cairo_point_t *point2,
                      cairo_point_t *point3)
{
    t1_path_info_t *path_info = (t1_path_info_t *) closure;
    int dx1, dy1, dx2, dy2, dx3, dy3;
    cairo_status_t status;

    status = _cairo_array_grow_by (path_info->data, 32);
    if (status)
        return status;

    dx1 = _cairo_fixed_integer_part (point1->x) - path_info->current_x;
    dy1 = _cairo_fixed_integer_part (point1->y) - path_info->current_y;
    dx2 = _cairo_fixed_integer_part (point2->x) - path_info->current_x - dx1;
    dy2 = _cairo_fixed_integer_part (point2->y) - path_info->current_y - dy1;
    dx3 = _cairo_fixed_integer_part (point3->x) - path_info->current_x - dx1 - dx2;
    dy3 = _cairo_fixed_integer_part (point3->y) - path_info->current_y - dy1 - dy2;
    charstring_encode_integer (path_info->data, dx1);
    charstring_encode_integer (path_info->data, dy1);
    charstring_encode_integer (path_info->data, dx2);
    charstring_encode_integer (path_info->data, dy2);
    charstring_encode_integer (path_info->data, dx3);
    charstring_encode_integer (path_info->data, dy3);
    path_info->current_x += dx1 + dx2 + dx3;
    path_info->current_y += dy1 + dy2 + dy3;
    charstring_encode_command (path_info->data, CHARSTRING_rcurveto);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_charstring_close_path (void *closure)
{
    cairo_status_t status;
    t1_path_info_t *path_info = (t1_path_info_t *) closure;

    status = _cairo_array_grow_by (path_info->data, 2);
    if (status)
	return status;

    charstring_encode_command (path_info->data, CHARSTRING_closepath);

    return CAIRO_STATUS_SUCCESS;
}

static void
charstring_encrypt (cairo_array_t *data)
{
    unsigned char *d, *end;
    uint16_t c, p, r;

    r = charstring_key;
    d = (unsigned char *) _cairo_array_index (data, 0);
    end = d + _cairo_array_num_elements (data);
    while (d < end) {
	p = *d;
	c = p ^ (r >> 8);
	r = (c + r) * encrypt_c1 + encrypt_c2;
        *d++ = c;
    }
}

static cairo_int_status_t
create_notdef_charstring (cairo_array_t *data)
{
    cairo_status_t status;

    /* We're passing constants below, so we know the 0 values will
     * only use 1 byte each, and the 500 values will use 2 bytes
     * each. Then 2 more for each of the commands is 10 total. */
    status = _cairo_array_grow_by (data, 10);
    if (status)
        return status;

    charstring_encode_integer (data, 0);
    charstring_encode_integer (data, 0);

    /* The width and height is arbitrary. */
    charstring_encode_integer (data, 500);
    charstring_encode_integer (data, 500);
    charstring_encode_command (data, CHARSTRING_sbw);

    charstring_encode_command (data, CHARSTRING_endchar);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_type1_font_create_charstring (cairo_type1_font_t *font,
                                    int                 subset_index,
                                    int                 glyph_index,
                                    cairo_array_t      *data)
{
    cairo_int_status_t status;
    cairo_scaled_glyph_t *scaled_glyph;
    t1_path_info_t path_info;
    cairo_text_extents_t *metrics;

    /* This call may return CAIRO_INT_STATUS_UNSUPPORTED for bitmap fonts. */
    status = _cairo_scaled_glyph_lookup (font->type1_scaled_font,
					 glyph_index,
					 CAIRO_SCALED_GLYPH_INFO_METRICS|
					 CAIRO_SCALED_GLYPH_INFO_PATH,
					 &scaled_glyph);
    if (status)
        return status;

    metrics = &scaled_glyph->metrics;
    if (subset_index == 0) {
        font->x_min = metrics->x_bearing;
        font->y_min = metrics->y_bearing;
        font->x_max = metrics->x_bearing + metrics->width;
        font->y_max = metrics->y_bearing + metrics->height;
    } else {
        if (metrics->x_bearing < font->x_min)
            font->x_min = metrics->x_bearing;
        if (metrics->y_bearing < font->y_min)
            font->y_min = metrics->y_bearing;
        if (metrics->x_bearing + metrics->width > font->x_max)
            font->x_max = metrics->x_bearing + metrics->width;
        if (metrics->y_bearing + metrics->height > font->y_max)
            font->y_max = metrics->y_bearing + metrics->height;
    }
    font->widths[subset_index] = metrics->width;

    status = _cairo_array_grow_by (data, 30);
    if (status)
        return status;

    charstring_encode_integer (data, (int) scaled_glyph->metrics.x_bearing);
    charstring_encode_integer (data, (int) scaled_glyph->metrics.y_bearing);
    charstring_encode_integer (data, (int) scaled_glyph->metrics.width);
    charstring_encode_integer (data, (int) scaled_glyph->metrics.height);
    charstring_encode_command (data, CHARSTRING_sbw);

    path_info.data = data;
    path_info.current_x = (int) scaled_glyph->metrics.x_bearing;
    path_info.current_y = (int) scaled_glyph->metrics.y_bearing;
    status = _cairo_path_fixed_interpret (scaled_glyph->path,
                                          CAIRO_DIRECTION_FORWARD,
                                          _charstring_move_to,
                                          _charstring_line_to,
                                          _charstring_curve_to,
                                          _charstring_close_path,
                                          &path_info);
    if (status)
        return status;

    status = _cairo_array_grow_by (data, 1);
    if (status)
        return status;
    charstring_encode_command (path_info.data, CHARSTRING_endchar);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_type1_font_write_charstrings (cairo_type1_font_t    *font,
                                    cairo_output_stream_t *encrypted_output)
{
    cairo_status_t status;
    unsigned char zeros[] = { 0, 0, 0, 0 };
    cairo_array_t data;
    unsigned int i;
    int length;

    _cairo_array_init (&data, sizeof (unsigned char));
    status = _cairo_array_grow_by (&data, 1024);
    if (status)
        goto fail;

    _cairo_output_stream_printf (encrypted_output,
                                 "2 index /CharStrings %d dict dup begin\n",
                                 font->scaled_font_subset->num_glyphs + 1);

    for (i = 0; i < font->scaled_font_subset->num_glyphs; i++) {
        _cairo_array_truncate (&data, 0);
        /* four "random" bytes required by encryption algorithm */
        status = _cairo_array_append_multiple (&data, zeros, 4);
        if (status)
            goto fail;
        status = cairo_type1_font_create_charstring (font, i,
						     font->scaled_font_subset->glyphs[i],
						     &data);
        if (status)
            goto fail;
        charstring_encrypt (&data);
        length = _cairo_array_num_elements (&data);
        _cairo_output_stream_printf (encrypted_output, "/g%d %d RD ", i, length);
        _cairo_output_stream_write (encrypted_output,
                                    _cairo_array_index (&data, 0),
                                    length);
        _cairo_output_stream_printf (encrypted_output, " ND\n");
    }

    /* All type 1 fonts must have a /.notdef charstring */

    _cairo_array_truncate (&data, 0);
    /* four "random" bytes required by encryption algorithm */
    status = _cairo_array_append_multiple (&data, zeros, 4);
    if (status)
        goto fail;
    status = create_notdef_charstring (&data);
    if (status)
        goto fail;
    charstring_encrypt (&data);
    length = _cairo_array_num_elements (&data);
    _cairo_output_stream_printf (encrypted_output, "/.notdef %d RD ", length);
    _cairo_output_stream_write (encrypted_output,
                                _cairo_array_index (&data, 0),
                                length);
    _cairo_output_stream_printf (encrypted_output, " ND\n");

fail:
    _cairo_array_fini (&data);
    return status;
}

static void
cairo_type1_font_write_header (cairo_type1_font_t *font,
                               const char         *name)
{
    cairo_matrix_t matrix;
    unsigned int i;
    const char spaces[50] = "                                                  ";

    matrix = font->type1_scaled_font->scale;
    matrix.xy = -matrix.xy;
    matrix.yy = -matrix.yy;
    cairo_matrix_invert (&matrix);

    _cairo_output_stream_printf (font->output,
                                 "%%!FontType1-1.1 %s 1.0\n"
                                 "11 dict begin\n"
                                 "/FontName /%s def\n"
                                 "/PaintType 0 def\n"
                                 "/FontType 1 def\n"
                                 "/FontMatrix [%f %f %f %f 0 0] readonly def\n",
                                 name,
                                 name,
                                 matrix.xx,
				 matrix.yx,
				 matrix.xy,
				 matrix.yy);

    /* We don't know the bbox values until after the charstrings have
     * been generated.  Reserve some space and fill in the bbox
     * later. */

    /* Worst case for four signed ints with spaces between each number */
    font->bbox_max_chars = 50;

    _cairo_output_stream_printf (font->output, "/FontBBox {");
    font->bbox_position = _cairo_output_stream_get_position (font->output);
    _cairo_output_stream_write (font->output, spaces, font->bbox_max_chars);

    _cairo_output_stream_printf (font->output,
                                 "} readonly def\n"
                                 "/Encoding 256 array\n"
				 "0 1 255 {1 index exch /.notdef put} for\n");
    for (i = 0; i < font->scaled_font_subset->num_glyphs; i++)
        _cairo_output_stream_printf (font->output, "dup %d /g%d put\n", i, i);
    _cairo_output_stream_printf (font->output,
                                 "readonly def\n"
                                 "currentdict end\n"
                                 "currentfile eexec\n");
}

static cairo_status_t
cairo_type1_write_stream_encrypted (void                *closure,
                                    const unsigned char *data,
                                    unsigned int         length)
{
    const unsigned char *in, *end;
    uint16_t c, p;
    static const char hex_digits[16] = "0123456789abcdef";
    char digits[3];
    cairo_type1_font_t *font = closure;

    in = (const unsigned char *) data;
    end = (const unsigned char *) data + length;
    while (in < end) {
	p = *in++;
	c = p ^ (font->eexec_key >> 8);
	font->eexec_key = (c + font->eexec_key) * encrypt_c1 + encrypt_c2;

	if (font->hex_encode) {
	    digits[0] = hex_digits[c >> 4];
	    digits[1] = hex_digits[c & 0x0f];
	    digits[2] = '\n';
	    font->hex_column += 2;

	    if (font->hex_column == 78) {
		_cairo_output_stream_write (font->output, digits, 3);
		font->hex_column = 0;
	    } else {
		_cairo_output_stream_write (font->output, digits, 2);
	    }
	} else {
	    digits[0] = c;
	    _cairo_output_stream_write (font->output, digits, 1);
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_type1_font_write_private_dict (cairo_type1_font_t *font,
                                     const char         *name)
{
    cairo_int_status_t status;
    cairo_output_stream_t *encrypted_output;

    font->eexec_key = private_dict_key;
    font->hex_column = 0;
    encrypted_output = _cairo_output_stream_create (
        cairo_type1_write_stream_encrypted,
        NULL,
        font);
    if (encrypted_output == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto fail;
    }

    /* Note: the first four spaces at the start of this private dict
     * are the four "random" bytes of plaintext required by the
     * encryption algorithm */
    _cairo_output_stream_printf (encrypted_output,
                                 "    dup /Private 9 dict dup begin\n"
                                 "/RD {string currentfile exch readstring pop}"
                                 " executeonly def\n"
                                 "/ND {noaccess def} executeonly def\n"
                                 "/NP {noaccess put} executeonly def\n"
                                 "/BlueValues [] def\n"
                                 "/MinFeature {16 16} def\n"
                                 "/lenIV 4 def\n"
                                 "/password 5839 def\n");

    status = cairo_type1_font_write_charstrings (font, encrypted_output);
    if (status)
	goto fail;

    _cairo_output_stream_printf (encrypted_output,
                                 "end\n"
                                 "end\n"
                                 "readonly put\n"
                                 "noaccess put\n"
                                 "dup /FontName get exch definefont pop\n"
                                 "mark currentfile closefile\n");

    if (status == CAIRO_STATUS_SUCCESS)
	status = _cairo_output_stream_get_status (encrypted_output);
  fail:
    _cairo_output_stream_destroy (encrypted_output);

    return status;
}

static void
cairo_type1_font_write_trailer(cairo_type1_font_t *font)
{
    int i;
    static const char zeros[65] =
	"0000000000000000000000000000000000000000000000000000000000000000\n";

    for (i = 0; i < 8; i++)
	_cairo_output_stream_write (font->output, zeros, sizeof zeros);

    _cairo_output_stream_printf (font->output, "cleartomark\n");
}

static cairo_status_t
cairo_type1_write_stream (void *closure,
                         const unsigned char *data,
                         unsigned int length)
{
    cairo_type1_font_t *font = closure;

    return _cairo_array_append_multiple (&font->contents, data, length);
}

static cairo_int_status_t
cairo_type1_font_write (cairo_type1_font_t *font,
                        const char *name)
{
    cairo_int_status_t status;

    cairo_type1_font_write_header (font, name);
    font->header_size = _cairo_output_stream_get_position (font->output);

    status = cairo_type1_font_write_private_dict (font, name);
    if (status)
	return status;

    font->data_size = _cairo_output_stream_get_position (font->output) -
	font->header_size;

    cairo_type1_font_write_trailer (font);
    font->trailer_size =
	_cairo_output_stream_get_position (font->output) -
	font->header_size - font->data_size;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
cairo_type1_font_generate (cairo_type1_font_t *font, const char *name)
{
    cairo_int_status_t status;

    status = _cairo_array_grow_by (&font->contents, 4096);
    if (status)
	return status;

    font->output = _cairo_output_stream_create (cairo_type1_write_stream, NULL, font);

    status = cairo_type1_font_write (font, name);
    if (status)
	return status;

    font->data = _cairo_array_index (&font->contents, 0);

    return CAIRO_STATUS_SUCCESS;
}

static void
cairo_type1_font_destroy (cairo_type1_font_t *font)
{
    free (font->widths);
    _cairo_array_fini (&font->contents);
    cairo_scaled_font_destroy (font->type1_scaled_font);
    free (font);
}

static cairo_status_t
_cairo_type1_fallback_init_internal (cairo_type1_subset_t	*type1_subset,
                                     const char			*name,
                                     cairo_scaled_font_subset_t	*scaled_font_subset,
                                     cairo_bool_t                hex_encode)
{
    cairo_type1_font_t *font;
    cairo_status_t status;
    unsigned long length;
    unsigned int i, len;

    status = cairo_type1_font_create (scaled_font_subset, &font, hex_encode);
    if (status)
	return status;

    status = cairo_type1_font_generate (font, name);
    if (status)
	goto fail1;

    type1_subset->base_font = strdup (name);
    if (type1_subset->base_font == NULL) {
        status = CAIRO_STATUS_NO_MEMORY;
        goto fail1;
    }

    type1_subset->widths = calloc (sizeof (int), font->scaled_font_subset->num_glyphs);
    if (type1_subset->widths == NULL) {
        status = CAIRO_STATUS_NO_MEMORY;
        goto fail2;
    }
    for (i = 0; i < font->scaled_font_subset->num_glyphs; i++)
	type1_subset->widths[i] = font->widths[i];

    type1_subset->x_min   = (int) font->x_min;
    type1_subset->y_min   = (int) font->y_min;
    type1_subset->x_max   = (int) font->x_max;
    type1_subset->y_max   = (int) font->y_max;
    type1_subset->ascent  = (int) font->y_max;
    type1_subset->descent = (int) font->y_min;

    length = font->header_size + font->data_size +
	font->trailer_size;
    type1_subset->data = malloc (length);
    if (type1_subset->data == NULL) {
        status = CAIRO_STATUS_NO_MEMORY;
	goto fail3;
    }
    memcpy (type1_subset->data,
	    _cairo_array_index (&font->contents, 0), length);

    len = snprintf(type1_subset->data + font->bbox_position,
                   font->bbox_max_chars,
                   "%d %d %d %d",
                   (int)type1_subset->x_min,
                   (int)type1_subset->y_min,
                   (int)type1_subset->x_max,
                   (int)type1_subset->y_max);
    type1_subset->data[font->bbox_position + len] = ' ';

    type1_subset->header_length = font->header_size;
    type1_subset->data_length = font->data_size;
    type1_subset->trailer_length = font->trailer_size;

    cairo_type1_font_destroy (font);
    return CAIRO_STATUS_SUCCESS;

 fail3:
    free (type1_subset->widths);
 fail2:
    free (type1_subset->base_font);
 fail1:
    cairo_type1_font_destroy (font);

    return status;
}

cairo_status_t
_cairo_type1_fallback_init_binary (cairo_type1_subset_t	      *type1_subset,
                                   const char		      *name,
                                   cairo_scaled_font_subset_t *scaled_font_subset)
{
    return _cairo_type1_fallback_init_internal (type1_subset,
                                                name,
                                                scaled_font_subset, FALSE);
}

cairo_status_t
_cairo_type1_fallback_init_hex (cairo_type1_subset_t	   *type1_subset,
                                const char		   *name,
                                cairo_scaled_font_subset_t *scaled_font_subset)
{
    return _cairo_type1_fallback_init_internal (type1_subset,
                                                name,
                                                scaled_font_subset, TRUE);
}

void
_cairo_type1_fallback_fini (cairo_type1_subset_t *subset)
{
    free (subset->base_font);
    free (subset->widths);
    free (subset->data);
}
