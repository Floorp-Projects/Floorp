/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Kristian Høgsberg <krh@redhat.com>
 *	Adrian Johnson <ajohnson@redneon.com>
 */

/*
 * Useful links:
 * http://developer.apple.com/textfonts/TTRefMan/RM06/Chap6.html
 * http://www.microsoft.com/typography/specs/default.htm
 */

#define _BSD_SOURCE /* for snprintf(), strdup() */
#include "cairoint.h"
#include "cairo-error-private.h"

#if CAIRO_HAS_FONT_SUBSET

#include "cairo-scaled-font-subsets-private.h"
#include "cairo-truetype-subset-private.h"


typedef struct subset_glyph subset_glyph_t;
struct subset_glyph {
    int parent_index;
    unsigned long location;
};

typedef struct _cairo_truetype_font cairo_truetype_font_t;

typedef struct table table_t;
struct table {
    unsigned long tag;
    cairo_status_t (*write) (cairo_truetype_font_t *font, unsigned long tag);
    int pos; /* position in the font directory */
};

struct _cairo_truetype_font {

    cairo_scaled_font_subset_t *scaled_font_subset;

    table_t truetype_tables[10];
    int num_tables;

    struct {
	char *font_name;
	char *ps_name;
	unsigned int num_glyphs;
	int *widths;
	long x_min, y_min, x_max, y_max;
	long ascent, descent;
        int  units_per_em;
    } base;

    subset_glyph_t *glyphs;
    const cairo_scaled_font_backend_t *backend;
    int num_glyphs_in_face;
    int checksum_index;
    cairo_array_t output;
    cairo_array_t string_offsets;
    unsigned long last_offset;
    unsigned long last_boundary;
    int *parent_to_subset;
    cairo_status_t status;

};

/*
 * Test that the structs we define for TrueType tables have the
 * correct size, ie. they are not padded.
 */
#define check(T, S) COMPILE_TIME_ASSERT (sizeof (T) == (S))
check (tt_head_t,	54);
check (tt_hhea_t,	36);
check (tt_maxp_t,	32);
check (tt_name_record_t, 12);
check (tt_name_t,	18);
check (tt_name_t,	18);
check (tt_composite_glyph_t, 16);
check (tt_glyph_data_t,	26);
#undef check

static cairo_status_t
cairo_truetype_font_use_glyph (cairo_truetype_font_t	    *font,
	                       unsigned short		     glyph,
			       unsigned short		    *out);

#define SFNT_VERSION			0x00010000
#define SFNT_STRING_MAX_LENGTH  65535

static cairo_status_t
_cairo_truetype_font_set_error (cairo_truetype_font_t *font,
			        cairo_status_t status)
{
    if (status == CAIRO_STATUS_SUCCESS || status == CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    _cairo_status_set_error (&font->status, status);

    return _cairo_error (status);
}

static cairo_status_t
_cairo_truetype_font_create (cairo_scaled_font_subset_t  *scaled_font_subset,
			     cairo_truetype_font_t      **font_return)
{
    cairo_status_t status;
    cairo_truetype_font_t *font;
    const cairo_scaled_font_backend_t *backend;
    tt_head_t head;
    tt_hhea_t hhea;
    tt_maxp_t maxp;
    unsigned long size;

    backend = scaled_font_subset->scaled_font->backend;
    if (!backend->load_truetype_table)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* FIXME: We should either support subsetting vertical fonts, or fail on
     * vertical.  Currently font_options_t doesn't have vertical flag, but
     * it should be added in the future.  For now, the freetype backend
     * returns UNSUPPORTED in load_truetype_table if the font is vertical.
     *
     *  if (cairo_font_options_get_vertical_layout (scaled_font_subset->scaled_font))
     *   return CAIRO_INT_STATUS_UNSUPPORTED;
     */

    size = sizeof (tt_head_t);
    status = backend->load_truetype_table (scaled_font_subset->scaled_font,
                                          TT_TAG_head, 0,
					  (unsigned char *) &head,
                                          &size);
    if (unlikely (status))
	return status;

    size = sizeof (tt_maxp_t);
    status = backend->load_truetype_table (scaled_font_subset->scaled_font,
                                           TT_TAG_maxp, 0,
					   (unsigned char *) &maxp,
					   &size);
    if (unlikely (status))
	return status;

    size = sizeof (tt_hhea_t);
    status = backend->load_truetype_table (scaled_font_subset->scaled_font,
                                           TT_TAG_hhea, 0,
					   (unsigned char *) &hhea,
					   &size);
    if (unlikely (status))
	return status;

    font = malloc (sizeof (cairo_truetype_font_t));
    if (unlikely (font == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    font->backend = backend;
    font->num_glyphs_in_face = be16_to_cpu (maxp.num_glyphs);
    font->scaled_font_subset = scaled_font_subset;

    font->last_offset = 0;
    font->last_boundary = 0;
    _cairo_array_init (&font->output, sizeof (char));
    status = _cairo_array_grow_by (&font->output, 4096);
    if (unlikely (status))
	goto fail1;

    font->glyphs = calloc (font->num_glyphs_in_face + 1, sizeof (subset_glyph_t));
    if (unlikely (font->glyphs == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail1;
    }

    font->parent_to_subset = calloc (font->num_glyphs_in_face, sizeof (int));
    if (unlikely (font->parent_to_subset == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail2;
    }

    font->base.num_glyphs = 0;
    font->base.x_min = (int16_t) be16_to_cpu (head.x_min);
    font->base.y_min = (int16_t) be16_to_cpu (head.y_min);
    font->base.x_max = (int16_t) be16_to_cpu (head.x_max);
    font->base.y_max = (int16_t) be16_to_cpu (head.y_max);
    font->base.ascent = (int16_t) be16_to_cpu (hhea.ascender);
    font->base.descent = (int16_t) be16_to_cpu (hhea.descender);
    font->base.units_per_em = (int16_t) be16_to_cpu (head.units_per_em);
    if (font->base.units_per_em == 0)
        font->base.units_per_em = 2048;

    font->base.ps_name = NULL;
    font->base.font_name = NULL;
    status = _cairo_truetype_read_font_name (scaled_font_subset->scaled_font,
					     &font->base.ps_name,
					     &font->base.font_name);
    if (_cairo_status_is_error (status))
	goto fail3;

    /* If the PS name is not found, create a CairoFont-x-y name. */
    if (font->base.ps_name == NULL) {
        font->base.ps_name = malloc (30);
        if (unlikely (font->base.ps_name == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
            goto fail3;
	}

        snprintf(font->base.ps_name, 30, "CairoFont-%u-%u",
                 scaled_font_subset->font_id,
                 scaled_font_subset->subset_id);
    }

    font->base.widths = calloc (font->num_glyphs_in_face, sizeof (int));
    if (unlikely (font->base.widths == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail4;
    }

    _cairo_array_init (&font->string_offsets, sizeof (unsigned long));
    status = _cairo_array_grow_by (&font->string_offsets, 10);
    if (unlikely (status))
	goto fail5;

    font->status = CAIRO_STATUS_SUCCESS;

    *font_return = font;

    return CAIRO_STATUS_SUCCESS;

 fail5:
    _cairo_array_fini (&font->string_offsets);
    free (font->base.widths);
 fail4:
    free (font->base.ps_name);
 fail3:
    free (font->parent_to_subset);
    if (font->base.font_name)
	free (font->base.font_name);
 fail2:
    free (font->glyphs);
 fail1:
    _cairo_array_fini (&font->output);
    free (font);

    return status;
}

static void
cairo_truetype_font_destroy (cairo_truetype_font_t *font)
{
    _cairo_array_fini (&font->string_offsets);
    free (font->base.widths);
    free (font->base.ps_name);
    if (font->base.font_name)
	free (font->base.font_name);
    free (font->parent_to_subset);
    free (font->glyphs);
    _cairo_array_fini (&font->output);
    free (font);
}

static cairo_status_t
cairo_truetype_font_allocate_write_buffer (cairo_truetype_font_t  *font,
					   size_t		   length,
					   unsigned char	 **buffer)
{
    cairo_status_t status;

    if (font->status)
	return font->status;

    status = _cairo_array_allocate (&font->output, length, (void **) buffer);
    if (unlikely (status))
	return _cairo_truetype_font_set_error (font, status);

    return CAIRO_STATUS_SUCCESS;
}

static void
cairo_truetype_font_write (cairo_truetype_font_t *font,
			   const void            *data,
			   size_t                 length)
{
    cairo_status_t status;

    if (font->status)
	return;

    status = _cairo_array_append_multiple (&font->output, data, length);
    if (unlikely (status))
	status = _cairo_truetype_font_set_error (font, status);
}

static void
cairo_truetype_font_write_be16 (cairo_truetype_font_t *font,
				uint16_t               value)
{
    uint16_t be16_value;

    if (font->status)
	return;

    be16_value = cpu_to_be16 (value);
    cairo_truetype_font_write (font, &be16_value, sizeof be16_value);
}

static void
cairo_truetype_font_write_be32 (cairo_truetype_font_t *font,
				uint32_t               value)
{
    uint32_t be32_value;

    if (font->status)
	return;

    be32_value = cpu_to_be32 (value);
    cairo_truetype_font_write (font, &be32_value, sizeof be32_value);
}

static cairo_status_t
cairo_truetype_font_align_output (cairo_truetype_font_t	    *font,
	                          unsigned long		    *aligned)
{
    int length, pad;
    unsigned char *padding;

    length = _cairo_array_num_elements (&font->output);
    *aligned = (length + 3) & ~3;
    pad = *aligned - length;

    if (pad) {
	cairo_status_t status;

	status = cairo_truetype_font_allocate_write_buffer (font, pad,
		                                            &padding);
	if (unlikely (status))
	    return status;

	memset (padding, 0, pad);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_truetype_font_check_boundary (cairo_truetype_font_t *font,
				    unsigned long          boundary)
{
    cairo_status_t status;

    if (font->status)
	return font->status;

    if (boundary - font->last_offset > SFNT_STRING_MAX_LENGTH)
    {
        status = _cairo_array_append (&font->string_offsets,
				      &font->last_boundary);
	if (unlikely (status))
	    return _cairo_truetype_font_set_error (font, status);

        font->last_offset = font->last_boundary;
    }
    font->last_boundary = boundary;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_truetype_font_write_cmap_table (cairo_truetype_font_t *font,
				      unsigned long          tag)
{
    unsigned int i;

    cairo_truetype_font_write_be16 (font, 0);  /* Table version */
    cairo_truetype_font_write_be16 (font, 2);  /* Num tables */

    cairo_truetype_font_write_be16 (font, 3);  /* Platform */
    cairo_truetype_font_write_be16 (font, 0);  /* Encoding */
    cairo_truetype_font_write_be32 (font, 20); /* Offset to start of table */

    cairo_truetype_font_write_be16 (font, 1);  /* Platform */
    cairo_truetype_font_write_be16 (font, 0);  /* Encoding */
    cairo_truetype_font_write_be32 (font, 52); /* Offset to start of table */

    /* Output a format 4 encoding table. */

    cairo_truetype_font_write_be16 (font, 4);  /* Format */
    cairo_truetype_font_write_be16 (font, 32); /* Length */
    cairo_truetype_font_write_be16 (font, 0);  /* Version */
    cairo_truetype_font_write_be16 (font, 4);  /* 2*segcount */
    cairo_truetype_font_write_be16 (font, 4);  /* searchrange */
    cairo_truetype_font_write_be16 (font, 1);  /* entry selector */
    cairo_truetype_font_write_be16 (font, 0);  /* rangeshift */
    cairo_truetype_font_write_be16 (font, 0xf000 + font->base.num_glyphs - 1); /* end count[0] */
    cairo_truetype_font_write_be16 (font, 0xffff);  /* end count[1] */
    cairo_truetype_font_write_be16 (font, 0);       /* reserved */
    cairo_truetype_font_write_be16 (font, 0xf000);  /* startCode[0] */
    cairo_truetype_font_write_be16 (font, 0xffff);  /* startCode[1] */
    cairo_truetype_font_write_be16 (font, 0x1000);  /* delta[0] */
    cairo_truetype_font_write_be16 (font, 1);       /* delta[1] */
    cairo_truetype_font_write_be16 (font, 0);       /* rangeOffset[0] */
    cairo_truetype_font_write_be16 (font, 0);       /* rangeOffset[1] */

    /* Output a format 6 encoding table. */

    cairo_truetype_font_write_be16 (font, 6);
    cairo_truetype_font_write_be16 (font, 10 + 2 * font->base.num_glyphs);
    cairo_truetype_font_write_be16 (font, 0);
    cairo_truetype_font_write_be16 (font, 0); /* First character */
    cairo_truetype_font_write_be16 (font, font->base.num_glyphs);
    for (i = 0; i < font->base.num_glyphs; i++)
	cairo_truetype_font_write_be16 (font, i);

    return font->status;
}

static cairo_status_t
cairo_truetype_font_write_generic_table (cairo_truetype_font_t *font,
					 unsigned long          tag)
{
    cairo_status_t status;
    unsigned char *buffer;
    unsigned long size;

    if (font->status)
	return font->status;

    size = 0;
    status = font->backend->load_truetype_table(font->scaled_font_subset->scaled_font,
					        tag, 0, NULL, &size);
    if (unlikely (status))
        return _cairo_truetype_font_set_error (font, status);

    status = cairo_truetype_font_allocate_write_buffer (font, size, &buffer);
    if (unlikely (status))
	return _cairo_truetype_font_set_error (font, status);

    status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
						 tag, 0, buffer, &size);
    if (unlikely (status))
	return _cairo_truetype_font_set_error (font, status);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_truetype_font_remap_composite_glyph (cairo_truetype_font_t	*font,
					   unsigned char		*buffer,
					   unsigned long		 size)
{
    tt_glyph_data_t *glyph_data;
    tt_composite_glyph_t *composite_glyph;
    int num_args;
    int has_more_components;
    unsigned short flags;
    unsigned short index;
    cairo_status_t status;
    unsigned char *end = buffer + size;

    if (font->status)
	return font->status;

    glyph_data = (tt_glyph_data_t *) buffer;
    if ((unsigned char *)(&glyph_data->data) >= end)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if ((int16_t)be16_to_cpu (glyph_data->num_contours) >= 0)
        return CAIRO_STATUS_SUCCESS;

    composite_glyph = &glyph_data->glyph;
    do {
	if ((unsigned char *)(&composite_glyph->args[1]) > end)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	flags = be16_to_cpu (composite_glyph->flags);
        has_more_components = flags & TT_MORE_COMPONENTS;
        status = cairo_truetype_font_use_glyph (font, be16_to_cpu (composite_glyph->index), &index);
	if (unlikely (status))
	    return status;

        composite_glyph->index = cpu_to_be16 (index);
        num_args = 1;
        if (flags & TT_ARG_1_AND_2_ARE_WORDS)
            num_args += 1;

	if (flags & TT_WE_HAVE_A_SCALE)
            num_args += 1;
        else if (flags & TT_WE_HAVE_AN_X_AND_Y_SCALE)
            num_args += 2;
        else if (flags & TT_WE_HAVE_A_TWO_BY_TWO)
            num_args += 4;

	composite_glyph = (tt_composite_glyph_t *) &(composite_glyph->args[num_args]);
    } while (has_more_components);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_truetype_font_write_glyf_table (cairo_truetype_font_t *font,
				      unsigned long          tag)
{
    unsigned long start_offset, index, size, next;
    tt_head_t header;
    unsigned long begin, end;
    unsigned char *buffer;
    unsigned int i;
    union {
	unsigned char *bytes;
	uint16_t      *short_offsets;
	uint32_t      *long_offsets;
    } u;
    cairo_status_t status;

    if (font->status)
	return font->status;

    size = sizeof (tt_head_t);
    status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
						 TT_TAG_head, 0,
						 (unsigned char*) &header, &size);
    if (unlikely (status))
	return _cairo_truetype_font_set_error (font, status);

    if (be16_to_cpu (header.index_to_loc_format) == 0)
	size = sizeof (int16_t) * (font->num_glyphs_in_face + 1);
    else
	size = sizeof (int32_t) * (font->num_glyphs_in_face + 1);

    u.bytes = malloc (size);
    if (unlikely (u.bytes == NULL))
	return _cairo_truetype_font_set_error (font, CAIRO_STATUS_NO_MEMORY);

    status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
                                                 TT_TAG_loca, 0, u.bytes, &size);
    if (unlikely (status))
	return _cairo_truetype_font_set_error (font, status);

    start_offset = _cairo_array_num_elements (&font->output);
    for (i = 0; i < font->base.num_glyphs; i++) {
	index = font->glyphs[i].parent_index;
	if (be16_to_cpu (header.index_to_loc_format) == 0) {
	    begin = be16_to_cpu (u.short_offsets[index]) * 2;
	    end = be16_to_cpu (u.short_offsets[index + 1]) * 2;
	}
	else {
	    begin = be32_to_cpu (u.long_offsets[index]);
	    end = be32_to_cpu (u.long_offsets[index + 1]);
	}

	/* quick sanity check... */
	if (end < begin) {
	    status = CAIRO_INT_STATUS_UNSUPPORTED;
	    goto FAIL;
	}

	size = end - begin;
        status = cairo_truetype_font_align_output (font, &next);
	if (unlikely (status))
	    goto FAIL;

        status = cairo_truetype_font_check_boundary (font, next);
	if (unlikely (status))
	    goto FAIL;

        font->glyphs[i].location = next - start_offset;

	status = cairo_truetype_font_allocate_write_buffer (font, size, &buffer);
	if (unlikely (status))
	    goto FAIL;

        if (size != 0) {
            status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
							 TT_TAG_glyf, begin, buffer, &size);
	    if (unlikely (status))
		goto FAIL;

            status = cairo_truetype_font_remap_composite_glyph (font, buffer, size);
	    if (unlikely (status))
		goto FAIL;
        }
    }

    status = cairo_truetype_font_align_output (font, &next);
    if (unlikely (status))
	goto FAIL;

    font->glyphs[i].location = next - start_offset;

    status = font->status;
FAIL:
    free (u.bytes);

    return _cairo_truetype_font_set_error (font, status);
}

static cairo_status_t
cairo_truetype_font_write_head_table (cairo_truetype_font_t *font,
                                      unsigned long          tag)
{
    unsigned char *buffer;
    unsigned long size;
    cairo_status_t status;

    if (font->status)
	return font->status;

    size = 0;
    status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
						 tag, 0, NULL, &size);
    if (unlikely (status))
	return _cairo_truetype_font_set_error (font, status);

    font->checksum_index = _cairo_array_num_elements (&font->output) + 8;
    status = cairo_truetype_font_allocate_write_buffer (font, size, &buffer);
    if (unlikely (status))
	return _cairo_truetype_font_set_error (font, status);

    status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
						 tag, 0, buffer, &size);
    if (unlikely (status))
	return _cairo_truetype_font_set_error (font, status);

    /* set checkSumAdjustment to 0 for table checksum calculation */
    *(uint32_t *)(buffer + 8) = 0;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_truetype_font_write_hhea_table (cairo_truetype_font_t *font, unsigned long tag)
{
    tt_hhea_t *hhea;
    unsigned long size;
    cairo_status_t status;

    if (font->status)
	return font->status;

    size = sizeof (tt_hhea_t);
    status = cairo_truetype_font_allocate_write_buffer (font, size, (unsigned char **) &hhea);
    if (unlikely (status))
	return _cairo_truetype_font_set_error (font, status);

    status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
						 tag, 0, (unsigned char *) hhea, &size);
    if (unlikely (status))
	return _cairo_truetype_font_set_error (font, status);

    hhea->num_hmetrics = cpu_to_be16 ((uint16_t)(font->base.num_glyphs));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_truetype_font_write_hmtx_table (cairo_truetype_font_t *font,
				      unsigned long          tag)
{
    unsigned long size;
    unsigned long long_entry_size;
    unsigned long short_entry_size;
    short *p;
    unsigned int i;
    tt_hhea_t hhea;
    int num_hmetrics;
    cairo_status_t status;

    if (font->status)
	return font->status;

    size = sizeof (tt_hhea_t);
    status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
						 TT_TAG_hhea, 0,
						 (unsigned char*) &hhea, &size);
    if (unlikely (status))
	return _cairo_truetype_font_set_error (font, status);

    num_hmetrics = be16_to_cpu(hhea.num_hmetrics);

    for (i = 0; i < font->base.num_glyphs; i++) {
        long_entry_size = 2 * sizeof (int16_t);
        short_entry_size = sizeof (int16_t);
        status = cairo_truetype_font_allocate_write_buffer (font,
		                                            long_entry_size,
							    (unsigned char **) &p);
	if (unlikely (status))
	    return _cairo_truetype_font_set_error (font, status);

        if (font->glyphs[i].parent_index < num_hmetrics) {
            status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
                                                         TT_TAG_hmtx,
                                                         font->glyphs[i].parent_index * long_entry_size,
                                                         (unsigned char *) p, &long_entry_size);
	    if (unlikely (status))
		return _cairo_truetype_font_set_error (font, status);
        }
        else
        {
            status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
                                                         TT_TAG_hmtx,
							 (num_hmetrics - 1) * long_entry_size,
							 (unsigned char *) p, &short_entry_size);
	    if (unlikely (status))
		return _cairo_truetype_font_set_error (font, status);

            status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
							 TT_TAG_hmtx,
							 num_hmetrics * long_entry_size +
							 (font->glyphs[i].parent_index - num_hmetrics) * short_entry_size,
							 (unsigned char *) (p + 1), &short_entry_size);
	    if (unlikely (status))
		return _cairo_truetype_font_set_error (font, status);
        }
        font->base.widths[i] = be16_to_cpu (p[0]);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_truetype_font_write_loca_table (cairo_truetype_font_t *font,
				      unsigned long          tag)
{
    unsigned int i;
    tt_head_t header;
    unsigned long size;
    cairo_status_t status;

    if (font->status)
	return font->status;

    size = sizeof(tt_head_t);
    status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
						 TT_TAG_head, 0,
						 (unsigned char*) &header, &size);
    if (unlikely (status))
	return _cairo_truetype_font_set_error (font, status);

    if (be16_to_cpu (header.index_to_loc_format) == 0)
    {
	for (i = 0; i < font->base.num_glyphs + 1; i++)
	    cairo_truetype_font_write_be16 (font, font->glyphs[i].location / 2);
    } else {
	for (i = 0; i < font->base.num_glyphs + 1; i++)
	    cairo_truetype_font_write_be32 (font, font->glyphs[i].location);
    }

    return font->status;
}

static cairo_status_t
cairo_truetype_font_write_maxp_table (cairo_truetype_font_t *font,
				      unsigned long          tag)
{
    tt_maxp_t *maxp;
    unsigned long size;
    cairo_status_t status;

    if (font->status)
	return font->status;

    size = sizeof (tt_maxp_t);
    status = cairo_truetype_font_allocate_write_buffer (font, size, (unsigned char **) &maxp);
    if (unlikely (status))
	return _cairo_truetype_font_set_error (font, status);

    status = font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
						 tag, 0, (unsigned char *) maxp, &size);
    if (unlikely (status))
	return _cairo_truetype_font_set_error (font, status);

    maxp->num_glyphs = cpu_to_be16 (font->base.num_glyphs);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
cairo_truetype_font_write_offset_table (cairo_truetype_font_t *font)
{
    cairo_status_t status;
    unsigned char *table_buffer;
    size_t table_buffer_length;
    unsigned short search_range, entry_selector, range_shift;

    if (font->status)
	return font->status;

    search_range = 1;
    entry_selector = 0;
    while (search_range * 2 <= font->num_tables) {
	search_range *= 2;
	entry_selector++;
    }
    search_range *= 16;
    range_shift = font->num_tables * 16 - search_range;

    cairo_truetype_font_write_be32 (font, SFNT_VERSION);
    cairo_truetype_font_write_be16 (font, font->num_tables);
    cairo_truetype_font_write_be16 (font, search_range);
    cairo_truetype_font_write_be16 (font, entry_selector);
    cairo_truetype_font_write_be16 (font, range_shift);

    /* Allocate space for the table directory. Each directory entry
     * will be filled in by cairo_truetype_font_update_entry() after
     * the table is written. */
    table_buffer_length = font->num_tables * 16;
    status = cairo_truetype_font_allocate_write_buffer (font, table_buffer_length,
						      &table_buffer);
    if (unlikely (status))
	return _cairo_truetype_font_set_error (font, status);

    return CAIRO_STATUS_SUCCESS;
}

static uint32_t
cairo_truetype_font_calculate_checksum (cairo_truetype_font_t *font,
					unsigned long          start,
					unsigned long          end)
{
    uint32_t *padded_end;
    uint32_t *p;
    uint32_t checksum;
    char *data;

    checksum = 0;
    data = _cairo_array_index (&font->output, 0);
    p = (uint32_t *) (data + start);
    padded_end = (uint32_t *) (data + ((end + 3) & ~3));
    while (p < padded_end)
	checksum += be32_to_cpu(*p++);

    return checksum;
}

static void
cairo_truetype_font_update_entry (cairo_truetype_font_t *font,
				  int                    index,
				  unsigned long          tag,
				  unsigned long          start,
				  unsigned long          end)
{
    uint32_t *entry;

    entry = _cairo_array_index (&font->output, 12 + 16 * index);
    entry[0] = cpu_to_be32 ((uint32_t)tag);
    entry[1] = cpu_to_be32 (cairo_truetype_font_calculate_checksum (font, start, end));
    entry[2] = cpu_to_be32 ((uint32_t)start);
    entry[3] = cpu_to_be32 ((uint32_t)(end - start));
}

static cairo_status_t
cairo_truetype_font_generate (cairo_truetype_font_t  *font,
			      const char            **data,
			      unsigned long          *length,
			      const unsigned long   **string_offsets,
			      unsigned long          *num_strings)
{
    cairo_status_t status;
    unsigned long start, end, next;
    uint32_t checksum, *checksum_location;
    int i;

    if (font->status)
	return font->status;

    status = cairo_truetype_font_write_offset_table (font);
    if (unlikely (status))
	goto FAIL;

    status = cairo_truetype_font_align_output (font, &start);
    if (unlikely (status))
	goto FAIL;

    end = 0;
    for (i = 0; i < font->num_tables; i++) {
	status = font->truetype_tables[i].write (font, font->truetype_tables[i].tag);
	if (unlikely (status))
	    goto FAIL;

	end = _cairo_array_num_elements (&font->output);
	status = cairo_truetype_font_align_output (font, &next);
	if (unlikely (status))
	    goto FAIL;

	cairo_truetype_font_update_entry (font, font->truetype_tables[i].pos,
                                          font->truetype_tables[i].tag, start, end);
        status = cairo_truetype_font_check_boundary (font, next);
	if (unlikely (status))
	    goto FAIL;

	start = next;
    }

    checksum =
	0xb1b0afba - cairo_truetype_font_calculate_checksum (font, 0, end);
    checksum_location = _cairo_array_index (&font->output, font->checksum_index);
    *checksum_location = cpu_to_be32 (checksum);

    *data = _cairo_array_index (&font->output, 0);
    *length = _cairo_array_num_elements (&font->output);
    *num_strings = _cairo_array_num_elements (&font->string_offsets);
    if (*num_strings != 0)
	*string_offsets = _cairo_array_index (&font->string_offsets, 0);
    else
	*string_offsets = NULL;

 FAIL:
    return _cairo_truetype_font_set_error (font, status);
}

static cairo_status_t
cairo_truetype_font_use_glyph (cairo_truetype_font_t	    *font,
	                       unsigned short		     glyph,
			       unsigned short		    *out)
{
    if (glyph >= font->num_glyphs_in_face)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (font->parent_to_subset[glyph] == 0) {
	font->parent_to_subset[glyph] = font->base.num_glyphs;
	font->glyphs[font->base.num_glyphs].parent_index = glyph;
	font->base.num_glyphs++;
    }

    *out = font->parent_to_subset[glyph];
    return CAIRO_STATUS_SUCCESS;
}

static void
cairo_truetype_font_add_truetype_table (cairo_truetype_font_t *font,
           unsigned long tag,
           cairo_status_t (*write) (cairo_truetype_font_t *font, unsigned long tag),
           int pos)
{
    font->truetype_tables[font->num_tables].tag = tag;
    font->truetype_tables[font->num_tables].write = write;
    font->truetype_tables[font->num_tables].pos = pos;
    font->num_tables++;
}

/* cairo_truetype_font_create_truetype_table_list() builds the list of
 * truetype tables to be embedded in the subsetted font. Each call to
 * cairo_truetype_font_add_truetype_table() adds a table, the callback
 * for generating the table, and the position in the table directory
 * to the truetype_tables array.
 *
 * As we write out the glyf table we remap composite glyphs.
 * Remapping composite glyphs will reference the sub glyphs the
 * composite glyph is made up of. The "glyf" table callback needs to
 * be called first so we have all the glyphs in the subset before
 * going further.
 *
 * The order in which tables are added to the truetype_table array
 * using cairo_truetype_font_add_truetype_table() specifies the order
 * in which the callback functions will be called.
 *
 * The tables in the table directory must be listed in alphabetical
 * order.  The "cvt", "fpgm", and "prep" are optional tables. They
 * will only be embedded in the subset if they exist in the source
 * font. The pos parameter of cairo_truetype_font_add_truetype_table()
 * specifies the position of the table in the table directory.
 */
static void
cairo_truetype_font_create_truetype_table_list (cairo_truetype_font_t *font)
{
    cairo_bool_t has_cvt = FALSE;
    cairo_bool_t has_fpgm = FALSE;
    cairo_bool_t has_prep = FALSE;
    unsigned long size;
    int pos;

    size = 0;
    if (font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
                                      TT_TAG_cvt, 0, NULL,
                                      &size) == CAIRO_STATUS_SUCCESS)
        has_cvt = TRUE;

    size = 0;
    if (font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
                                      TT_TAG_fpgm, 0, NULL,
                                      &size) == CAIRO_STATUS_SUCCESS)
        has_fpgm = TRUE;

    size = 0;
    if (font->backend->load_truetype_table (font->scaled_font_subset->scaled_font,
                                      TT_TAG_prep, 0, NULL,
                                      &size) == CAIRO_STATUS_SUCCESS)
        has_prep = TRUE;

    font->num_tables = 0;
    pos = 1;
    if (has_cvt)
        pos++;
    if (has_fpgm)
        pos++;
    cairo_truetype_font_add_truetype_table (font, TT_TAG_glyf, cairo_truetype_font_write_glyf_table, pos);

    pos = 0;
    cairo_truetype_font_add_truetype_table (font, TT_TAG_cmap, cairo_truetype_font_write_cmap_table, pos++);
    if (has_cvt)
        cairo_truetype_font_add_truetype_table (font, TT_TAG_cvt, cairo_truetype_font_write_generic_table, pos++);
    if (has_fpgm)
        cairo_truetype_font_add_truetype_table (font, TT_TAG_fpgm, cairo_truetype_font_write_generic_table, pos++);
    pos++;
    cairo_truetype_font_add_truetype_table (font, TT_TAG_head, cairo_truetype_font_write_head_table, pos++);
    cairo_truetype_font_add_truetype_table (font, TT_TAG_hhea, cairo_truetype_font_write_hhea_table, pos++);
    cairo_truetype_font_add_truetype_table (font, TT_TAG_hmtx, cairo_truetype_font_write_hmtx_table, pos++);
    cairo_truetype_font_add_truetype_table (font, TT_TAG_loca, cairo_truetype_font_write_loca_table, pos++);
    cairo_truetype_font_add_truetype_table (font, TT_TAG_maxp, cairo_truetype_font_write_maxp_table, pos++);
    if (has_prep)
        cairo_truetype_font_add_truetype_table (font, TT_TAG_prep, cairo_truetype_font_write_generic_table, pos);
}

cairo_status_t
_cairo_truetype_subset_init (cairo_truetype_subset_t    *truetype_subset,
			     cairo_scaled_font_subset_t	*font_subset)
{
    cairo_truetype_font_t *font = NULL;
    cairo_status_t status;
    const char *data = NULL; /* squelch bogus compiler warning */
    unsigned long length = 0; /* squelch bogus compiler warning */
    unsigned long offsets_length;
    unsigned int i;
    const unsigned long *string_offsets = NULL;
    unsigned long num_strings = 0;

    status = _cairo_truetype_font_create (font_subset, &font);
    if (unlikely (status))
	return status;

    for (i = 0; i < font->scaled_font_subset->num_glyphs; i++) {
	unsigned short parent_glyph = font->scaled_font_subset->glyphs[i];
	status = cairo_truetype_font_use_glyph (font, parent_glyph, &parent_glyph);
	if (unlikely (status))
	    goto fail1;
    }

    cairo_truetype_font_create_truetype_table_list (font);
    status = cairo_truetype_font_generate (font, &data, &length,
                                           &string_offsets, &num_strings);
    if (unlikely (status))
	goto fail1;

    truetype_subset->ps_name = strdup (font->base.ps_name);
    if (unlikely (truetype_subset->ps_name == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail1;
    }

    if (font->base.font_name != NULL) {
	truetype_subset->font_name = strdup (font->base.font_name);
	if (unlikely (truetype_subset->font_name == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto fail2;
	}
    } else {
	truetype_subset->font_name = NULL;
    }

    /* The widths array returned must contain only widths for the
     * glyphs in font_subset. Any subglyphs appended after
     * font_subset->num_glyphs are omitted. */
    truetype_subset->widths = calloc (sizeof (double),
                                      font->scaled_font_subset->num_glyphs);
    if (unlikely (truetype_subset->widths == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto fail3;
    }
    for (i = 0; i < font->scaled_font_subset->num_glyphs; i++)
	truetype_subset->widths[i] = (double)font->base.widths[i]/font->base.units_per_em;

    truetype_subset->x_min = (double)font->base.x_min/font->base.units_per_em;
    truetype_subset->y_min = (double)font->base.y_min/font->base.units_per_em;
    truetype_subset->x_max = (double)font->base.x_max/font->base.units_per_em;
    truetype_subset->y_max = (double)font->base.y_max/font->base.units_per_em;
    truetype_subset->ascent = (double)font->base.ascent/font->base.units_per_em;
    truetype_subset->descent = (double)font->base.descent/font->base.units_per_em;

    if (length) {
	truetype_subset->data = malloc (length);
	if (unlikely (truetype_subset->data == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto fail4;
	}

	memcpy (truetype_subset->data, data, length);
    } else
	truetype_subset->data = NULL;
    truetype_subset->data_length = length;

    if (num_strings) {
	offsets_length = num_strings * sizeof (unsigned long);
	truetype_subset->string_offsets = malloc (offsets_length);
	if (unlikely (truetype_subset->string_offsets == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto fail5;
	}

	memcpy (truetype_subset->string_offsets, string_offsets, offsets_length);
	truetype_subset->num_string_offsets = num_strings;
    } else {
	truetype_subset->string_offsets = NULL;
	truetype_subset->num_string_offsets = 0;
    }

    cairo_truetype_font_destroy (font);

    return CAIRO_STATUS_SUCCESS;

 fail5:
    free (truetype_subset->data);
 fail4:
    free (truetype_subset->widths);
 fail3:
    if (truetype_subset->font_name)
	free (truetype_subset->font_name);
 fail2:
    free (truetype_subset->ps_name);
 fail1:
    cairo_truetype_font_destroy (font);

    return status;
}

void
_cairo_truetype_subset_fini (cairo_truetype_subset_t *subset)
{
    free (subset->ps_name);
    if (subset->font_name)
	free (subset->font_name);
    free (subset->widths);
    free (subset->data);
    free (subset->string_offsets);
}

static cairo_int_status_t
_cairo_truetype_reverse_cmap (cairo_scaled_font_t *scaled_font,
			      unsigned long        table_offset,
			      unsigned long        index,
			      uint32_t            *ucs4)
{
    cairo_status_t status;
    const cairo_scaled_font_backend_t *backend;
    tt_segment_map_t *map;
    char buf[4];
    unsigned int num_segments, i;
    unsigned long size;
    uint16_t *start_code;
    uint16_t *end_code;
    uint16_t *delta;
    uint16_t *range_offset;
    uint16_t *glyph_array;
    uint16_t  c;

    backend = scaled_font->backend;
    size = 4;
    status = backend->load_truetype_table (scaled_font,
                                           TT_TAG_cmap, table_offset,
					   (unsigned char *) &buf,
					   &size);
    if (unlikely (status))
	return status;

    /* All table formats have the same first two words */
    map = (tt_segment_map_t *) buf;
    if (be16_to_cpu (map->format) != 4)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    size = be16_to_cpu (map->length);
    map = malloc (size);
    if (unlikely (map == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = backend->load_truetype_table (scaled_font,
                                           TT_TAG_cmap, table_offset,
                                           (unsigned char *) map,
                                           &size);
    if (unlikely (status))
	goto fail;

    num_segments = be16_to_cpu (map->segCountX2)/2;

    /* A Format 4 cmap contains 8 uint16_t numbers and 4 arrays of
     * uint16_t each num_segments long. */
    if (size < (8 + 4*num_segments)*sizeof(uint16_t))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    end_code = map->endCount;
    start_code = &(end_code[num_segments + 1]);
    delta = &(start_code[num_segments]);
    range_offset = &(delta[num_segments]);
    glyph_array = &(range_offset[num_segments]);

    /* search for glyph in segments with rangeOffset=0 */
    for (i = 0; i < num_segments; i++) {
	c = index - be16_to_cpu (delta[i]);
	if (range_offset[i] == 0 &&
	    c >= be16_to_cpu (start_code[i]) &&
	    c <= be16_to_cpu (end_code[i]))
	{
	    *ucs4 = c;
	    goto found;
	}
    }

    /* search for glyph in segments with rangeOffset=1 */
    for (i = 0; i < num_segments; i++) {
	if (range_offset[i] != 0) {
	    uint16_t *glyph_ids = &range_offset[i] + be16_to_cpu (range_offset[i])/2;
	    int range_size = be16_to_cpu (end_code[i]) - be16_to_cpu (start_code[i]) + 1;
	    uint16_t g_id_be = cpu_to_be16 (index);
	    int j;

	    if (range_size > 0) {
		if ((char*)glyph_ids + 2*range_size > (char*)map + size)
		    return CAIRO_INT_STATUS_UNSUPPORTED;

		for (j = 0; j < range_size; j++) {
		    if (glyph_ids[j] == g_id_be) {
			*ucs4 = be16_to_cpu (start_code[i]) + j;
			goto found;
		    }
		}
	    }
	}
    }

    /* glyph not found */
    *ucs4 = -1;

found:
    status = CAIRO_STATUS_SUCCESS;

fail:
    free (map);

    return status;
}

cairo_int_status_t
_cairo_truetype_index_to_ucs4 (cairo_scaled_font_t *scaled_font,
                               unsigned long        index,
                               uint32_t            *ucs4)
{
    cairo_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;
    const cairo_scaled_font_backend_t *backend;
    tt_cmap_t *cmap;
    char buf[4];
    int num_tables, i;
    unsigned long size;

    backend = scaled_font->backend;
    if (!backend->load_truetype_table)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    size = 4;
    status = backend->load_truetype_table (scaled_font,
                                           TT_TAG_cmap, 0,
					   (unsigned char *) &buf,
					   &size);
    if (unlikely (status))
	return status;

    cmap = (tt_cmap_t *) buf;
    num_tables = be16_to_cpu (cmap->num_tables);
    size = 4 + num_tables*sizeof(tt_cmap_index_t);
    cmap = _cairo_malloc_ab_plus_c (num_tables, sizeof (tt_cmap_index_t), 4);
    if (unlikely (cmap == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = backend->load_truetype_table (scaled_font,
	                                   TT_TAG_cmap, 0,
					   (unsigned char *) cmap,
					   &size);
    if (unlikely (status))
        goto cleanup;

    /* Find a table with Unicode mapping */
    for (i = 0; i < num_tables; i++) {
        if (be16_to_cpu (cmap->index[i].platform) == 3 &&
            be16_to_cpu (cmap->index[i].encoding) == 1) {
            status = _cairo_truetype_reverse_cmap (scaled_font,
						   be32_to_cpu (cmap->index[i].offset),
						   index,
						   ucs4);
            if (status != CAIRO_INT_STATUS_UNSUPPORTED)
                break;
        }
    }

cleanup:
    free (cmap);

    return status;
}

cairo_int_status_t
_cairo_truetype_read_font_name (cairo_scaled_font_t  	 *scaled_font,
				char 	       		**ps_name_out,
				char 	       		**font_name_out)
{
    cairo_status_t status;
    const cairo_scaled_font_backend_t *backend;
    tt_name_t *name;
    tt_name_record_t *record;
    unsigned long size;
    int i, j;
    char *ps_name = NULL;
    char *font_name = NULL;

    backend = scaled_font->backend;
    if (!backend->load_truetype_table)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    size = 0;
    status = backend->load_truetype_table (scaled_font,
	                                   TT_TAG_name, 0,
					   NULL,
					   &size);
    if (status)
	return status;

    name = malloc (size);
    if (name == NULL)
        return _cairo_error (CAIRO_STATUS_NO_MEMORY);

   status = backend->load_truetype_table (scaled_font,
					   TT_TAG_name, 0,
					   (unsigned char *) name,
					   &size);
    if (status)
	goto fail;

    /* Extract the font name and PS name from the name table. At
     * present this just looks for the Mac platform/Roman encoded font
     * name. It should be extended to use any suitable font name in
     * the name table.
     */
    for (i = 0; i < be16_to_cpu(name->num_records); i++) {
        record = &(name->records[i]);
        if ((be16_to_cpu (record->platform) == 1) &&
            (be16_to_cpu (record->encoding) == 0)) {

	    if (be16_to_cpu (record->name) == 4) {
		font_name = malloc (be16_to_cpu(record->length) + 1);
		if (font_name == NULL) {
		    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
		    goto fail;
		}
		strncpy(font_name,
			((char*)name) + be16_to_cpu (name->strings_offset) + be16_to_cpu (record->offset),
			be16_to_cpu (record->length));
		font_name[be16_to_cpu (record->length)] = 0;
	    }

	    if (be16_to_cpu (record->name) == 6) {
		ps_name = malloc (be16_to_cpu(record->length) + 1);
		if (ps_name == NULL) {
		    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
		    goto fail;
		}
		strncpy(ps_name,
			((char*)name) + be16_to_cpu (name->strings_offset) + be16_to_cpu (record->offset),
			be16_to_cpu (record->length));
		ps_name[be16_to_cpu (record->length)] = 0;
	    }

	    if (font_name && ps_name)
		break;
        }
    }

    free (name);

    /* Ensure PS name does not contain any spaces */
    if (ps_name) {
	for (i = 0, j = 0; ps_name[j]; j++) {
	    if (ps_name[j] == ' ')
		continue;
	    ps_name[i++] = ps_name[j];
	}
	ps_name[i] = '\0';
    }

    *ps_name_out = ps_name;
    *font_name_out = font_name;

    return CAIRO_STATUS_SUCCESS;

fail:
    free (name);

    if (ps_name != NULL)
	free (ps_name);

    if (font_name != NULL)
	free (font_name);

    *ps_name_out = NULL;
    *font_name_out = NULL;

    return status;
}

#endif /* CAIRO_HAS_FONT_SUBSET */
