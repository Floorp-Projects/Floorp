/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2008 Adrian Johnson
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
 * The Initial Developer of the Original Code is Adrian Johnson.
 *
 * Contributor(s):
 *	Adrian Johnson <ajohnson@redneon.com>
 */

#include "cairoint.h"
#include "cairo-image-info-private.h"

static uint32_t
_get_be32 (const unsigned char *p)
{
    return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

/* JPEG (image/jpeg)
 *
 * http://www.w3.org/Graphics/JPEG/itu-t81.pdf
 */

/* Markers with no parameters. All other markers are followed by a two
 * byte length of the parameters. */
#define TEM       0x01
#define RST_begin 0xd0
#define RST_end   0xd7
#define SOI       0xd8
#define EOI       0xd9

/* Start of frame markers. */
#define SOF0  0xc0
#define SOF1  0xc1
#define SOF2  0xc2
#define SOF3  0xc3
#define SOF5  0xc5
#define SOF6  0xc6
#define SOF7  0xc7
#define SOF9  0xc9
#define SOF10 0xca
#define SOF11 0xcb
#define SOF13 0xcd
#define SOF14 0xce
#define SOF15 0xcf

static const unsigned char *
_jpeg_skip_segment (const unsigned char *p)
{
    int len;

    p++;
    len = (p[0] << 8) | p[1];

    return p + len;
}

static void
_jpeg_extract_info (cairo_image_info_t *info, const unsigned char *p)
{
    info->width = (p[6] << 8) + p[7];
    info->height = (p[4] << 8) + p[5];
    info->num_components = p[8];
    info->bits_per_component = p[3];
}

cairo_int_status_t
_cairo_image_info_get_jpeg_info (cairo_image_info_t	*info,
				 const unsigned char	*data,
				 long			 length)
{
    const unsigned char *p = data;

    while (p + 1 < data + length) {
	if (*p != 0xff)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	p++;

	switch (*p) {
	    /* skip fill bytes */
	case 0xff:
	    p++;
	    break;

	case TEM:
	case SOI:
	case EOI:
	    p++;
	    break;

	case SOF0:
	case SOF1:
	case SOF2:
	case SOF3:
	case SOF5:
	case SOF6:
	case SOF7:
	case SOF9:
	case SOF10:
	case SOF11:
	case SOF13:
	case SOF14:
	case SOF15:
	    /* Start of frame found. Extract the image parameters. */
	    if (p + 8 > data + length)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	    _jpeg_extract_info (info, p);
	    return CAIRO_STATUS_SUCCESS;

	default:
	    if (*p >= RST_begin && *p <= RST_end) {
		p++;
		break;
	    }

	    if (p + 2 > data + length)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	    p = _jpeg_skip_segment (p);
	    break;
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

/* JPEG 2000 (image/jp2)
 *
 * http://www.jpeg.org/public/15444-1annexi.pdf
 */

#define JPX_FILETYPE 0x66747970
#define JPX_JP2_HEADER 0x6A703268
#define JPX_IMAGE_HEADER 0x69686472

static const unsigned char _jpx_signature[] = {
    0x00, 0x00, 0x00, 0x0c, 0x6a, 0x50, 0x20, 0x20, 0x0d, 0x0a, 0x87, 0x0a
};

static const unsigned char *
_jpx_next_box (const unsigned char *p)
{
    return p + _get_be32 (p);
}

static const unsigned char *
_jpx_get_box_contents (const unsigned char *p)
{
    return p + 8;
}

static cairo_bool_t
_jpx_match_box (const unsigned char *p, const unsigned char *end, uint32_t type)
{
    uint32_t length;

    if (p + 8 < end) {
	length = _get_be32 (p);
	if (_get_be32 (p + 4) == type &&  p + length < end)
	    return TRUE;
    }

    return FALSE;
}

static const unsigned char *
_jpx_find_box (const unsigned char *p, const unsigned char *end, uint32_t type)
{
    while (p < end) {
	if (_jpx_match_box (p, end, type))
	    return p;
	p = _jpx_next_box (p);
    }

    return NULL;
}

static void
_jpx_extract_info (const unsigned char *p, cairo_image_info_t *info)
{
    info->height = _get_be32 (p);
    info->width = _get_be32 (p + 4);
    info->num_components = (p[8] << 8) + p[9];
    info->bits_per_component = p[10];
}

cairo_int_status_t
_cairo_image_info_get_jpx_info (cairo_image_info_t	*info,
				const unsigned char	*data,
				unsigned long		 length)
{
    const unsigned char *p = data;
    const unsigned char *end = data + length;

    /* First 12 bytes must be the JPEG 2000 signature box. */
    if (length < ARRAY_LENGTH(_jpx_signature) ||
	memcmp(p, _jpx_signature, ARRAY_LENGTH(_jpx_signature)) != 0)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    p += ARRAY_LENGTH(_jpx_signature);

    /* Next box must be a File Type Box */
    if (! _jpx_match_box (p, end, JPX_FILETYPE))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    p = _jpx_next_box (p);

    /* Locate the JP2 header box. */
    p = _jpx_find_box (p, end, JPX_JP2_HEADER);
    if (!p)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* Step into the JP2 header box. First box must be the Image
     * Header */
    p = _jpx_get_box_contents (p);
    if (! _jpx_match_box (p, end, JPX_IMAGE_HEADER))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* Get the image info */
    p = _jpx_get_box_contents (p);
    _jpx_extract_info (p, info);

    return CAIRO_STATUS_SUCCESS;
}

/* PNG (image/png)
 *
 * http://www.w3.org/TR/2003/REC-PNG-20031110/
 */

#define PNG_IHDR 0x49484452

static const unsigned char _png_magic[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };

cairo_int_status_t
_cairo_image_info_get_png_info (cairo_image_info_t     *info,
                               const unsigned char     *data,
                               unsigned long            length)
{
    const unsigned char *p = data;
    const unsigned char *end = data + length;

    if (length < 8 || memcmp (data, _png_magic, 8) != 0)
       return CAIRO_INT_STATUS_UNSUPPORTED;

    p += 8;

    /* The first chunk must be IDHR. IDHR has 13 bytes of data plus
     * the 12 bytes of overhead for the chunk. */
    if (p + 13 + 12 > end)
       return CAIRO_INT_STATUS_UNSUPPORTED;

    p += 4;
    if (_get_be32 (p) != PNG_IHDR)
       return CAIRO_INT_STATUS_UNSUPPORTED;

    p += 4;
    info->width = _get_be32 (p);
    p += 4;
    info->height = _get_be32 (p);

    return CAIRO_STATUS_SUCCESS;
}
