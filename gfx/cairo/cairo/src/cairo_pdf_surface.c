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
 */

#include "cairoint.h"
#include "cairo-pdf.h"
/* XXX: Eventually, we need to handle other font backends */
#include "cairo-ft-private.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_TAGS_H
#include FT_TRUETYPE_TABLES_H

#include <time.h>
#include <zlib.h>

/* Issues:
 *
 * - Why doesn't pages inherit /alpha%d GS dictionaries from the Pages
 *   object?
 *
 * - We embed an image in the stream each time it's composited.  We
 *   could add generation counters to surfaces and remember the stream
 *   ID for a particular generation for a particular surface.
 *
 * - Multi stop gradients.  What are the exponential interpolation
 *   functions, could they be used for gradients?
 *
 * - Clipping: must be able to reset clipping
 *
 * - Images of other formats than 8 bit RGBA.
 *
 * - Backend specific meta data.
 *
 * - Surface patterns.
 *
 * - Alpha channels in gradients.
 *
 * - Should/does cairo support drawing into a scratch surface and then
 *   using that as a fill pattern?  For this backend, that would involve
 *   using a tiling pattern (4.6.2).  How do you create such a scratch
 *   surface?  cairo_surface_create_similar() ?
 *
 * - What if you create a similiar surface and does show_page and then
 *   does show_surface on another surface?
 *
 * - Output TM so page scales to the right size - PDF default user
 *   space has 1 unit = 1 / 72 inch.
 *
 * - Add test case for RGBA images.
 *
 * - Add test case for RGBA gradients.
 *
 * - Pattern extend isn't honoured by image backend.
 *
 * - Coordinate space for create_similar() args?
 *
 * - Investigate /Matrix entry in content stream dicts for pages
 *   instead of outputting the cm operator in every page.
 */

typedef struct ft_subset_glyph ft_subset_glyph_t;
struct ft_subset_glyph {
    int parent_index;
    unsigned long location;
};

typedef struct cairo_pdf_font_backend cairo_pdf_font_backend_t;
struct cairo_pdf_font_backend {
    int			(*use_glyph)	(void *abstract_font,
					 int glyph);
    cairo_status_t	(*generate)	(void *abstract_font,
					 const char **data,
					 unsigned long *length);
    void		(*destroy)	(void *abstract_font);
};

typedef struct cairo_pdf_font cairo_pdf_font_t;
struct cairo_pdf_font {
    cairo_pdf_font_backend_t *backend;
    cairo_unscaled_font_t *unscaled_font;
    unsigned int font_id;
    char *base_font;
    int num_glyphs;
    int *widths;
    long x_min, y_min, x_max, y_max;
    long ascent, descent;
};

typedef struct cairo_pdf_ft_font cairo_pdf_ft_font_t;
struct cairo_pdf_ft_font {
    cairo_pdf_font_t base;
    ft_subset_glyph_t *glyphs;
    FT_Face face;
    unsigned long *checksum_location;
    cairo_array_t output;
    int *parent_to_subset;
    cairo_status_t status;
};

typedef struct cairo_pdf_object cairo_pdf_object_t;
typedef struct cairo_pdf_resource cairo_pdf_resource_t;
typedef struct cairo_pdf_stream cairo_pdf_stream_t;
typedef struct cairo_pdf_document cairo_pdf_document_t;
typedef struct cairo_pdf_surface cairo_pdf_surface_t;

struct cairo_pdf_object {
    long offset;
};

struct cairo_pdf_resource {
    unsigned int id;
};

struct cairo_pdf_stream {
    unsigned int id;
    unsigned int length_id;
    long start_offset;
};

struct cairo_pdf_document {
    FILE *file;
    unsigned long refcount;

    double width_inches;
    double height_inches;
    double x_ppi;
    double y_ppi;

    unsigned int next_available_id;
    unsigned int pages_id;

    cairo_pdf_stream_t *current_stream;

    cairo_array_t objects;
    cairo_array_t pages;

    cairo_array_t fonts;
};

struct cairo_pdf_surface {
    cairo_surface_t base;

    double width_inches;
    double height_inches;

    cairo_pdf_document_t *document;
    cairo_pdf_stream_t *current_stream;

    cairo_array_t patterns;
    cairo_array_t xobjects;
    cairo_array_t streams;
    cairo_array_t alphas;
    cairo_array_t fonts;
};


static cairo_pdf_document_t *
_cairo_pdf_document_create (FILE	*file,
			    double	width_inches,
			    double	height_inches,
			    double	x_pixels_per_inch,
			    double	y_pixels_per_inch);

static void
_cairo_pdf_document_destroy (cairo_pdf_document_t *document);

static void
_cairo_pdf_document_reference (cairo_pdf_document_t *document);

static unsigned int
_cairo_pdf_document_new_object (cairo_pdf_document_t *document);

static cairo_status_t
_cairo_pdf_document_add_page (cairo_pdf_document_t *document,
			      cairo_pdf_surface_t *surface);

static void
_cairo_pdf_surface_clear (cairo_pdf_surface_t *surface);

static cairo_pdf_stream_t *
_cairo_pdf_document_open_stream (cairo_pdf_document_t	*document,
				 const char		*extra_entries);
static cairo_surface_t *
_cairo_pdf_surface_create_for_document (cairo_pdf_document_t	*document,
					double			width_inches,
					double			height_inches);
static void
_cairo_pdf_surface_add_stream (cairo_pdf_surface_t	*surface,
			       cairo_pdf_stream_t	*stream);
static void
_cairo_pdf_surface_ensure_stream (cairo_pdf_surface_t	*surface);

static const cairo_surface_backend_t cairo_pdf_surface_backend;

/* Truetype font subsetting code */

#define ARRAY_LENGTH(a) ( (sizeof (a)) / (sizeof ((a)[0])) )

#define SFNT_VERSION			0x00010000

#ifdef WORDS_BIGENDIAN

#define cpu_to_be16(v) (v)
#define be16_to_cpu(v) (v)
#define cpu_to_be32(v) (v)
#define be32_to_cpu(v) (v)

#else

static inline unsigned short
cpu_to_be16(unsigned short v)
{
    return (v << 8) | (v >> 8);
}

static inline unsigned short
be16_to_cpu(unsigned short v)
{
    return cpu_to_be16 (v);
}

static inline unsigned long
cpu_to_be32(unsigned long v)
{
    return (cpu_to_be16 (v) << 16) | cpu_to_be16 (v >> 16);
}

static inline unsigned long
be32_to_cpu(unsigned long v)
{
    return cpu_to_be32 (v);
}

#endif

static cairo_pdf_font_backend_t cairo_pdf_ft_font_backend;

static int
cairo_pdf_font_use_glyph (cairo_pdf_font_t *font, int glyph)
{
    return font->backend->use_glyph (font, glyph);
}

static cairo_status_t
cairo_pdf_font_generate (cairo_pdf_font_t *font,
			 const char **data, unsigned long *length)
{
    return font->backend->generate (font, data, length);
}

static void
cairo_pdf_font_destroy (cairo_pdf_font_t *font)
{
    font->backend->destroy (font);
}

static cairo_pdf_font_t *
cairo_pdf_ft_font_create (cairo_pdf_document_t  *document,
			  cairo_unscaled_font_t *unscaled_font)
{
    FT_Face face;
    cairo_pdf_ft_font_t *font;
    unsigned long size;
    int i, j;

    face = _cairo_ft_unscaled_font_lock_face (unscaled_font);

    /* We currently only support freetype truetype fonts. */
    size = 0;
    if (!FT_IS_SFNT (face) ||
	FT_Load_Sfnt_Table (face, TTAG_glyf, 0, NULL, &size) != 0)
	return NULL;

    font = malloc (sizeof (cairo_pdf_ft_font_t));
    if (font == NULL)
	return NULL;

    font->base.unscaled_font = unscaled_font;
    _cairo_unscaled_font_reference (unscaled_font);
    font->base.backend = &cairo_pdf_ft_font_backend;
    font->base.font_id = _cairo_pdf_document_new_object (document);

    _cairo_array_init (&font->output, sizeof (char));
    if (_cairo_array_grow_by (&font->output, 4096) != CAIRO_STATUS_SUCCESS)
	goto fail1;

    font->base.unscaled_font = unscaled_font;
    _cairo_unscaled_font_reference (unscaled_font);
    font->glyphs = calloc (face->num_glyphs + 1, sizeof (ft_subset_glyph_t));
    if (font->glyphs == NULL)
	goto fail2;

    font->parent_to_subset = calloc (face->num_glyphs, sizeof (int));
    if (font->parent_to_subset == NULL)
	goto fail3;

    font->base.num_glyphs = 1;
    font->base.x_min = face->bbox.xMin;
    font->base.y_min = face->bbox.yMin;
    font->base.x_max = face->bbox.xMax;
    font->base.y_max = face->bbox.yMax;
    font->base.ascent = face->ascender;
    font->base.descent = face->descender;
    font->base.base_font = strdup (face->family_name);
    if (font->base.base_font == NULL)
	goto fail4;

    for (i = 0, j = 0; font->base.base_font[j]; j++) {
	if (font->base.base_font[j] == ' ')
	    continue;
	font->base.base_font[i++] = font->base.base_font[j];
    }
    font->base.base_font[i] = '\0';

    font->base.widths = calloc (face->num_glyphs, sizeof (int));
    if (font->base.widths == NULL)
	goto fail5;

    _cairo_ft_unscaled_font_unlock_face (unscaled_font);

    font->status = CAIRO_STATUS_SUCCESS;

    return &font->base;

 fail5:
    free (font->base.base_font);
 fail4:
    free (font->parent_to_subset);
 fail3:
    free (font->glyphs);
 fail2:
    _cairo_array_fini (&font->output);
 fail1:
    free (font);
    return NULL;
}

static void
cairo_pdf_ft_font_destroy (void *abstract_font)
{
    cairo_pdf_ft_font_t *font = abstract_font;

    _cairo_unscaled_font_destroy (font->base.unscaled_font);
    free (font->base.base_font);
    free (font->parent_to_subset);
    free (font->glyphs);
    _cairo_array_fini (&font->output);
    free (font);
}

static void *
cairo_pdf_ft_font_write (cairo_pdf_ft_font_t *font,
			 const void *data, size_t length)
{
    void *p;

    p = _cairo_array_append (&font->output, data, length);
    if (p == NULL)
	font->status = CAIRO_STATUS_NO_MEMORY;

    return p;
}

static void
cairo_pdf_ft_font_write_be16 (cairo_pdf_ft_font_t *font,
			      unsigned short value)
{
    unsigned short be16_value;

    be16_value = cpu_to_be16 (value);
    cairo_pdf_ft_font_write (font, &be16_value, sizeof be16_value);
}

static void
cairo_pdf_ft_font_write_be32 (cairo_pdf_ft_font_t *font, unsigned long value)
{
    unsigned long be32_value;

    be32_value = cpu_to_be32 (value);
    cairo_pdf_ft_font_write (font, &be32_value, sizeof be32_value);
}

static unsigned long
cairo_pdf_ft_font_align_output (cairo_pdf_ft_font_t *font)
{
    int length, aligned;
    static const char pad[4];

    length = _cairo_array_num_elements (&font->output);
    aligned = (length + 3) & ~3;
    cairo_pdf_ft_font_write (font, pad, aligned - length);

    return aligned;
}

static int
cairo_pdf_ft_font_write_cmap_table (cairo_pdf_ft_font_t *font, unsigned long tag)
{
    int i;

    cairo_pdf_ft_font_write_be16 (font, 0);
    cairo_pdf_ft_font_write_be16 (font, 1);

    cairo_pdf_ft_font_write_be16 (font, 1);
    cairo_pdf_ft_font_write_be16 (font, 0);
    cairo_pdf_ft_font_write_be32 (font, 12);

    /* Output a format 6 encoding table. */

    cairo_pdf_ft_font_write_be16 (font, 6);
    cairo_pdf_ft_font_write_be16 (font, 10 + 2 * (font->base.num_glyphs - 1));
    cairo_pdf_ft_font_write_be16 (font, 0);
    cairo_pdf_ft_font_write_be16 (font, 1); /* First glyph */
    cairo_pdf_ft_font_write_be16 (font, font->base.num_glyphs - 1);
    for (i = 1; i < font->base.num_glyphs; i++)
	cairo_pdf_ft_font_write_be16 (font, i);

    return font->status;
}

static int
cairo_pdf_ft_font_write_generic_table (cairo_pdf_ft_font_t *font,
				       unsigned long tag)
{
    char *buffer;
    unsigned long size;

    size = 0;
    FT_Load_Sfnt_Table (font->face, tag, 0, NULL, &size);
    buffer = cairo_pdf_ft_font_write (font, NULL, size);
    FT_Load_Sfnt_Table (font->face, tag, 0, buffer, &size);
    
    return 0;
}

static int
cairo_pdf_ft_font_write_glyf_table (cairo_pdf_ft_font_t *font,
				    unsigned long tag)
{
    unsigned long start_offset, index, size;
    TT_Header *header;
    unsigned long begin, end;
    char *buffer;
    int i;
    union {
	unsigned char *bytes;
	unsigned short *short_offsets;
	unsigned long *long_offsets;
    } u;

    header = FT_Get_Sfnt_Table (font->face, ft_sfnt_head);
    if (header->Index_To_Loc_Format == 0)
	size = sizeof (short) * (font->face->num_glyphs + 1);
    else
	size = sizeof (long) * (font->face->num_glyphs + 1);

    u.bytes = malloc (size);
    if (u.bytes == NULL) {
	font->status = CAIRO_STATUS_NO_MEMORY;
	return font->status;
    }
    FT_Load_Sfnt_Table (font->face, TTAG_loca, 0, u.bytes, &size);

    start_offset = _cairo_array_num_elements (&font->output);
    for (i = 0; i < font->base.num_glyphs; i++) {
	index = font->glyphs[i].parent_index;
	if (header->Index_To_Loc_Format == 0) {
	    begin = be16_to_cpu (u.short_offsets[index]) * 2;
	    end = be16_to_cpu (u.short_offsets[index + 1]) * 2;
	}
	else {
	    begin = be32_to_cpu (u.long_offsets[index]);
	    end = be32_to_cpu (u.long_offsets[index + 1]);
	}

	size = end - begin;

	font->glyphs[i].location =
	    cairo_pdf_ft_font_align_output (font) - start_offset;
	buffer = cairo_pdf_ft_font_write (font, NULL, size);
	if (buffer == NULL)
	    break;
	FT_Load_Sfnt_Table (font->face, TTAG_glyf, begin, buffer, &size);
	/* FIXME: remap composite glyphs */
    }

    font->glyphs[i].location =
	cairo_pdf_ft_font_align_output (font) - start_offset;

    free (u.bytes);

    return font->status;
}

static int
cairo_pdf_ft_font_write_head_table (cairo_pdf_ft_font_t *font,
				    unsigned long tag)
{
    TT_Header *head;

    head = FT_Get_Sfnt_Table (font->face, ft_sfnt_head);

    cairo_pdf_ft_font_write_be32 (font, head->Table_Version);
    cairo_pdf_ft_font_write_be32 (font, head->Font_Revision);

    font->checksum_location =
	(unsigned long *) _cairo_array_index (&font->output, 0) +
	_cairo_array_num_elements (&font->output) / sizeof (long);
    cairo_pdf_ft_font_write_be32 (font, 0);
    cairo_pdf_ft_font_write_be32 (font, head->Magic_Number);

    cairo_pdf_ft_font_write_be16 (font, head->Flags);
    cairo_pdf_ft_font_write_be16 (font, head->Units_Per_EM);

    cairo_pdf_ft_font_write_be32 (font, head->Created[0]);
    cairo_pdf_ft_font_write_be32 (font, head->Created[1]);
    cairo_pdf_ft_font_write_be32 (font, head->Modified[0]);
    cairo_pdf_ft_font_write_be32 (font, head->Modified[1]);

    cairo_pdf_ft_font_write_be16 (font, head->xMin);
    cairo_pdf_ft_font_write_be16 (font, head->yMin);
    cairo_pdf_ft_font_write_be16 (font, head->xMax);
    cairo_pdf_ft_font_write_be16 (font, head->yMax);

    cairo_pdf_ft_font_write_be16 (font, head->Mac_Style);
    cairo_pdf_ft_font_write_be16 (font, head->Lowest_Rec_PPEM);

    cairo_pdf_ft_font_write_be16 (font, head->Font_Direction);
    cairo_pdf_ft_font_write_be16 (font, head->Index_To_Loc_Format);
    cairo_pdf_ft_font_write_be16 (font, head->Glyph_Data_Format);

    return font->status;
}

static int cairo_pdf_ft_font_write_hhea_table (cairo_pdf_ft_font_t *font, unsigned long tag)
{
    TT_HoriHeader *hhea;

    hhea = FT_Get_Sfnt_Table (font->face, ft_sfnt_hhea);

    cairo_pdf_ft_font_write_be32 (font, hhea->Version);
    cairo_pdf_ft_font_write_be16 (font, hhea->Ascender);
    cairo_pdf_ft_font_write_be16 (font, hhea->Descender);
    cairo_pdf_ft_font_write_be16 (font, hhea->Line_Gap);

    cairo_pdf_ft_font_write_be16 (font, hhea->advance_Width_Max);

    cairo_pdf_ft_font_write_be16 (font, hhea->min_Left_Side_Bearing);
    cairo_pdf_ft_font_write_be16 (font, hhea->min_Right_Side_Bearing);
    cairo_pdf_ft_font_write_be16 (font, hhea->xMax_Extent);
    cairo_pdf_ft_font_write_be16 (font, hhea->caret_Slope_Rise);
    cairo_pdf_ft_font_write_be16 (font, hhea->caret_Slope_Run);
    cairo_pdf_ft_font_write_be16 (font, hhea->caret_Offset);

    cairo_pdf_ft_font_write_be16 (font, 0);
    cairo_pdf_ft_font_write_be16 (font, 0);
    cairo_pdf_ft_font_write_be16 (font, 0);
    cairo_pdf_ft_font_write_be16 (font, 0);

    cairo_pdf_ft_font_write_be16 (font, hhea->metric_Data_Format);
    cairo_pdf_ft_font_write_be16 (font, font->base.num_glyphs);

    return font->status;
}

static int
cairo_pdf_ft_font_write_hmtx_table (cairo_pdf_ft_font_t *font,
				    unsigned long tag)
{
    unsigned long entry_size;
    short *p;
    int i;

    for (i = 0; i < font->base.num_glyphs; i++) {
	entry_size = 2 * sizeof (short);
	p = cairo_pdf_ft_font_write (font, NULL, entry_size);
	FT_Load_Sfnt_Table (font->face, TTAG_hmtx, 
			    font->glyphs[i].parent_index * entry_size,
			    (FT_Byte *) p, &entry_size);
	font->base.widths[i] = be16_to_cpu (p[0]);
    }

    return font->status;
}

static int
cairo_pdf_ft_font_write_loca_table (cairo_pdf_ft_font_t *font,
				    unsigned long tag)
{
    int i;
    TT_Header *header;

    header = FT_Get_Sfnt_Table (font->face, ft_sfnt_head);

    if (header->Index_To_Loc_Format == 0) {
	for (i = 0; i < font->base.num_glyphs + 1; i++)
	    cairo_pdf_ft_font_write_be16 (font, font->glyphs[i].location / 2);
    }
    else {
	for (i = 0; i < font->base.num_glyphs + 1; i++)
	    cairo_pdf_ft_font_write_be32 (font, font->glyphs[i].location);
    }

    return font->status;
}

static int
cairo_pdf_ft_font_write_maxp_table (cairo_pdf_ft_font_t *font,
				    unsigned long tag)
{
    TT_MaxProfile *maxp;

    maxp = FT_Get_Sfnt_Table (font->face, ft_sfnt_maxp);
    
    cairo_pdf_ft_font_write_be32 (font, maxp->version);
    cairo_pdf_ft_font_write_be16 (font, font->base.num_glyphs);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxPoints);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxContours);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxCompositePoints);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxCompositeContours);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxZones);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxTwilightPoints);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxStorage);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxFunctionDefs);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxInstructionDefs);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxStackElements);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxSizeOfInstructions);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxComponentElements);
    cairo_pdf_ft_font_write_be16 (font, maxp->maxComponentDepth);

    return font->status;
}

typedef struct table table_t;
struct table {
    unsigned long tag;
    int (*write) (cairo_pdf_ft_font_t *font, unsigned long tag);
};

static const table_t truetype_tables[] = {
    { TTAG_cmap, cairo_pdf_ft_font_write_cmap_table },
    { TTAG_cvt,  cairo_pdf_ft_font_write_generic_table },
    { TTAG_fpgm, cairo_pdf_ft_font_write_generic_table },
    { TTAG_glyf, cairo_pdf_ft_font_write_glyf_table },
    { TTAG_head, cairo_pdf_ft_font_write_head_table },
    { TTAG_hhea, cairo_pdf_ft_font_write_hhea_table },
    { TTAG_hmtx, cairo_pdf_ft_font_write_hmtx_table },
    { TTAG_loca, cairo_pdf_ft_font_write_loca_table },
    { TTAG_maxp, cairo_pdf_ft_font_write_maxp_table },
    { TTAG_name, cairo_pdf_ft_font_write_generic_table },
    { TTAG_prep, cairo_pdf_ft_font_write_generic_table },
};

static cairo_status_t
cairo_pdf_ft_font_write_offset_table (cairo_pdf_ft_font_t *font)
{
    unsigned short search_range, entry_selector, range_shift;
    int num_tables;

    num_tables = ARRAY_LENGTH (truetype_tables);
    search_range = 1;
    entry_selector = 0;
    while (search_range * 2 <= num_tables) {
	search_range *= 2;
	entry_selector++;
    }
    search_range *= 16;
    range_shift = num_tables * 16 - search_range;

    cairo_pdf_ft_font_write_be32 (font, SFNT_VERSION);
    cairo_pdf_ft_font_write_be16 (font, num_tables);
    cairo_pdf_ft_font_write_be16 (font, search_range);
    cairo_pdf_ft_font_write_be16 (font, entry_selector);
    cairo_pdf_ft_font_write_be16 (font, range_shift);

    cairo_pdf_ft_font_write (font, NULL, ARRAY_LENGTH (truetype_tables) * 16);

    return font->status;
}    

static unsigned long
cairo_pdf_ft_font_calculate_checksum (cairo_pdf_ft_font_t *font,
			   unsigned long start, unsigned long end)
{
    unsigned long *padded_end;
    unsigned long *p;
    unsigned long checksum;
    char *data;

    checksum = 0; 
    data = _cairo_array_index (&font->output, 0);
    p = (unsigned long *) (data + start);
    padded_end = (unsigned long *) (data + ((end + 3) & ~3));
    while (p < padded_end)
	checksum += *p++;

    return checksum;
}

static void
cairo_pdf_ft_font_update_entry (cairo_pdf_ft_font_t *font, int index, unsigned long tag,
			unsigned long start, unsigned long end)
{
    unsigned long *entry;

    entry = _cairo_array_index (&font->output, 12 + 16 * index);
    entry[0] = cpu_to_be32 (tag);
    entry[1] = cpu_to_be32 (cairo_pdf_ft_font_calculate_checksum (font, start, end));
    entry[2] = cpu_to_be32 (start);
    entry[3] = cpu_to_be32 (end - start);
}

static cairo_status_t
cairo_pdf_ft_font_generate (void *abstract_font,
			    const char **data, unsigned long *length)
{
    cairo_pdf_ft_font_t *font = abstract_font;
    unsigned long start, end, next, checksum;
    int i;

    font->face = _cairo_ft_unscaled_font_lock_face (font->base.unscaled_font);

    if (cairo_pdf_ft_font_write_offset_table (font))
	goto fail;

    start = cairo_pdf_ft_font_align_output (font);
    end = start;

    end = 0;
    for (i = 0; i < ARRAY_LENGTH (truetype_tables); i++) {
	if (truetype_tables[i].write (font, truetype_tables[i].tag))
	    goto fail;

	end = _cairo_array_num_elements (&font->output);
	next = cairo_pdf_ft_font_align_output (font);
	cairo_pdf_ft_font_update_entry (font, i, truetype_tables[i].tag,
					start, end);
	start = next;
    }

    checksum =
	0xb1b0afba - cairo_pdf_ft_font_calculate_checksum (font, 0, end);
    *font->checksum_location = cpu_to_be32 (checksum);

    *data = _cairo_array_index (&font->output, 0);
    *length = _cairo_array_num_elements (&font->output);

 fail:
    _cairo_ft_unscaled_font_unlock_face (font->base.unscaled_font);      
    font->face = NULL;

    return font->status;
}

static int
cairo_pdf_ft_font_use_glyph (void *abstract_font, int glyph)
{
    cairo_pdf_ft_font_t *font = abstract_font;

    if (font->parent_to_subset[glyph] == 0) {
	font->parent_to_subset[glyph] = font->base.num_glyphs;
	font->glyphs[font->base.num_glyphs].parent_index = glyph;
	font->base.num_glyphs++;
    }

    return font->parent_to_subset[glyph];
}

static cairo_pdf_font_backend_t cairo_pdf_ft_font_backend = {
    cairo_pdf_ft_font_use_glyph,
    cairo_pdf_ft_font_generate,
    cairo_pdf_ft_font_destroy
};

/* PDF Generation */

static unsigned int
_cairo_pdf_document_new_object (cairo_pdf_document_t *document)
{
    cairo_pdf_object_t object;

    object.offset = ftell (document->file);
    if (_cairo_array_append (&document->objects, &object, 1) == NULL)
	return 0;

    return document->next_available_id++;
}

static void
_cairo_pdf_document_update_object (cairo_pdf_document_t *document,
				   unsigned int id)
{
    cairo_pdf_object_t *object;

    object = _cairo_array_index (&document->objects, id - 1);
    object->offset = ftell (document->file);
}

static void
_cairo_pdf_surface_add_stream (cairo_pdf_surface_t	*surface,
			       cairo_pdf_stream_t	*stream)
{
    _cairo_array_append (&surface->streams, &stream, 1);
    surface->current_stream = stream;
}

static void
_cairo_pdf_surface_add_pattern (cairo_pdf_surface_t *surface, unsigned int id)
{
    cairo_pdf_resource_t resource;

    resource.id = id;
    _cairo_array_append (&surface->patterns, &resource, 1);
}

static void
_cairo_pdf_surface_add_xobject (cairo_pdf_surface_t *surface, unsigned int id)
{
    cairo_pdf_resource_t resource;
    int i, num_resources;

    num_resources = _cairo_array_num_elements (&surface->xobjects);
    for (i = 0; i < num_resources; i++) {
	_cairo_array_copy_element (&surface->xobjects, i, &resource);
	if (resource.id == id)
	    return;
    }

    resource.id = id;
    _cairo_array_append (&surface->xobjects, &resource, 1);
}

static unsigned int
_cairo_pdf_surface_add_alpha (cairo_pdf_surface_t *surface, double alpha)
{
    int num_alphas, i;
    double other;

    num_alphas = _cairo_array_num_elements (&surface->alphas);
    for (i = 0; i < num_alphas; i++) {
	_cairo_array_copy_element (&surface->alphas, i, &other);
	if (alpha == other)
	    return i;
    }

    _cairo_array_append (&surface->alphas, &alpha, 1);
    return _cairo_array_num_elements (&surface->alphas) - 1;
}

static void
_cairo_pdf_surface_add_font (cairo_pdf_surface_t *surface, unsigned int id)
{
    cairo_pdf_resource_t resource;
    int i, num_fonts;

    num_fonts = _cairo_array_num_elements (&surface->fonts);
    for (i = 0; i < num_fonts; i++) {
	_cairo_array_copy_element (&surface->fonts, i, &resource);
	if (resource.id == id)
	    return;
    }

    resource.id = id;
    _cairo_array_append (&surface->fonts, &resource, 1);
}

cairo_surface_t *
cairo_pdf_surface_create (FILE		*file,
			  double	width_inches,
			  double	height_inches,
			  double	x_pixels_per_inch,
			  double	y_pixels_per_inch)
{
    cairo_pdf_document_t *document;
    cairo_surface_t *surface;

    document = _cairo_pdf_document_create (file,
					   width_inches,
					   height_inches,
					   x_pixels_per_inch,
					   y_pixels_per_inch);
    if (document == NULL)
      return NULL;

    surface = _cairo_pdf_surface_create_for_document (document,
						      width_inches,
						      height_inches);

    _cairo_pdf_document_destroy (document);

    return surface;
}

static cairo_surface_t *
_cairo_pdf_surface_create_for_document (cairo_pdf_document_t	*document,
					double			width_inches,
					double			height_inches)
{
    cairo_pdf_surface_t *surface;

    surface = malloc (sizeof (cairo_pdf_surface_t));
    if (surface == NULL)
	return NULL;

    _cairo_surface_init (&surface->base, &cairo_pdf_surface_backend);

    surface->width_inches = width_inches;
    surface->height_inches = height_inches;

    _cairo_pdf_document_reference (document);
    surface->document = document;
    _cairo_array_init (&surface->streams, sizeof (cairo_pdf_stream_t *));
    _cairo_array_init (&surface->patterns, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&surface->xobjects, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&surface->alphas, sizeof (double));
    _cairo_array_init (&surface->fonts, sizeof (cairo_pdf_resource_t));

    return &surface->base;
}

static void
_cairo_pdf_surface_clear (cairo_pdf_surface_t *surface)
{
    int num_streams, i;
    cairo_pdf_stream_t *stream;

    num_streams = _cairo_array_num_elements (&surface->streams);
    for (i = 0; i < num_streams; i++) {
	_cairo_array_copy_element (&surface->streams, i, &stream);
	free (stream);
    }

    _cairo_array_truncate (&surface->streams, 0);
    _cairo_array_truncate (&surface->patterns, 0);
    _cairo_array_truncate (&surface->xobjects, 0);
    _cairo_array_truncate (&surface->alphas, 0);
    _cairo_array_truncate (&surface->fonts, 0);
}

static cairo_surface_t *
_cairo_pdf_surface_create_similar (void			*abstract_src,
				   cairo_format_t	format,
				   int			drawable,
				   int			width,
				   int			height)
{
    cairo_pdf_surface_t *template = abstract_src;

    return _cairo_pdf_surface_create_for_document (template->document,
						   width, height);
}

static cairo_pdf_stream_t *
_cairo_pdf_document_open_stream (cairo_pdf_document_t	*document,
				 const char		*extra_entries)
{
    FILE *file = document->file;
    cairo_pdf_stream_t *stream;

    stream = malloc (sizeof (cairo_pdf_stream_t));
    if (stream == NULL) {
	return NULL;
    }

    stream->id = _cairo_pdf_document_new_object (document);
    stream->length_id = _cairo_pdf_document_new_object (document);

    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /Length %d 0 R\r\n"
	     "%s"
	     ">>\r\n"
	     "stream\r\n",
	     stream->id,
	     stream->length_id,
	     extra_entries);

    stream->start_offset = ftell (file);

    document->current_stream = stream;

    return stream;
}

static void
_cairo_pdf_document_close_stream (cairo_pdf_document_t	*document)
{
    FILE *file = document->file;
    long length;
    cairo_pdf_stream_t *stream;

    stream = document->current_stream;
    if (stream == NULL)
	return;

    length = ftell(file) - stream->start_offset;
    fprintf (file, 
	     "\r\n"
	     "endstream\r\n"
	     "endobj\r\n");

    _cairo_pdf_document_update_object (document, stream->length_id);
    fprintf (file, 
	     "%d 0 obj\r\n"
	     "   %ld\r\n"
	     "endobj\r\n",
	     stream->length_id,
	     length);

    document->current_stream = NULL;
}

static void
_cairo_pdf_surface_destroy (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;

    if (surface->current_stream == document->current_stream)
	_cairo_pdf_document_close_stream (document);

    _cairo_pdf_document_destroy (document);

    free (surface);
}

/* XXX: We should re-work this interface to return both X/Y ppi values. */
static double
_cairo_pdf_surface_pixels_per_inch (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
 
    return surface->document->y_ppi;
}

static void
_cairo_pdf_surface_ensure_stream (cairo_pdf_surface_t *surface)
{
    cairo_pdf_document_t *document = surface->document;
    cairo_pdf_stream_t *stream;
    FILE *file = document->file;
    char extra[200];

    if (document->current_stream == NULL ||
	document->current_stream != surface->current_stream) {
	_cairo_pdf_document_close_stream (document);
	snprintf (extra, sizeof extra,
		  "   /Type /XObject\r\n"
		  "   /Subtype /Form\r\n"
		  "   /BBox [ 0 0 %f %f ]\r\n",
		  surface->width_inches * document->x_ppi,
		  surface->height_inches * document->y_ppi);
	stream = _cairo_pdf_document_open_stream (document, extra);
	_cairo_pdf_surface_add_stream (surface, stream);

	/* If this is the first stream we open for this surface,
	 * output the cairo to PDF transformation matrix. */
	if (_cairo_array_num_elements (&surface->streams) == 1)
	    fprintf (file, "1 0 0 -1 0 %f cm\r\n",
		     document->height_inches * document->y_ppi);
    }
}

static cairo_status_t
_cairo_pdf_surface_acquire_source_image (void                    *abstract_surface,
					 cairo_image_surface_t  **image_out,
					 void                   **image_extra)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static void
_cairo_pdf_surface_release_source_image (void                   *abstract_surface,
					 cairo_image_surface_t  *image,
					 void                   *image_extra)
{
}

static cairo_status_t
_cairo_pdf_surface_acquire_dest_image (void                    *abstract_surface,
				       cairo_rectangle_t       *interest_rect,
				       cairo_image_surface_t  **image_out,
				       cairo_rectangle_t       *image_rect,
				       void                   **image_extra)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static void
_cairo_pdf_surface_release_dest_image (void                   *abstract_surface,
				       cairo_rectangle_t      *interest_rect,
				       cairo_image_surface_t  *image,
				       cairo_rectangle_t      *image_rect,
				       void                   *image_extra)
{
}

static cairo_status_t
_cairo_pdf_surface_clone_similar (void			*abstract_surface,
				  cairo_surface_t	*src,
				  cairo_surface_t     **clone_out)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static void *
compress_dup (const void *data, unsigned long data_size,
	      unsigned long *compressed_size)
{
    void *compressed;

    /* Bound calculation taken from zlib. */
    *compressed_size = data_size + (data_size >> 12) + (data_size >> 14) + 11;
    compressed = malloc (*compressed_size);
    if (compressed == NULL)
	return NULL;

    compress (compressed, compressed_size, data, data_size);

    return compressed;
}

static unsigned int
emit_image_data (cairo_pdf_document_t *document,
		 cairo_image_surface_t *image)
{
    FILE *file = document->file;
    cairo_pdf_stream_t *stream;
    char entries[200];
    char *rgb, *compressed;
    int i, x, y;
    unsigned long rgb_size, compressed_size;
    pixman_bits_t *pixel;

    rgb_size = image->height * image->width * 3;
    rgb = malloc (rgb_size);
    if (rgb == NULL)
	return 0;

    i = 0;
    for (y = 0; y < image->height; y++) {
	pixel = (pixman_bits_t *) (image->data + y * image->stride);

	for (x = 0; x < image->width; x++, pixel++) {
	    rgb[i++] = (*pixel & 0x00ff0000) >> 16;
	    rgb[i++] = (*pixel & 0x0000ff00) >>  8;
	    rgb[i++] = (*pixel & 0x000000ff) >>  0;
	}
    }

    compressed = compress_dup (rgb, rgb_size, &compressed_size);
    if (compressed == NULL) {
	free (rgb);
	return 0;
    }

    _cairo_pdf_document_close_stream (document);

    snprintf (entries, sizeof entries, 
	      "   /Type /XObject\r\n"
	      "   /Subtype /Image\r\n"
	      "   /Width %d\r\n"
	      "   /Height %d\r\n"
	      "   /ColorSpace /DeviceRGB\r\n"
	      "   /BitsPerComponent 8\r\n"
	      "   /Filter /FlateDecode\r\n",
	      image->width, image->height);

    stream = _cairo_pdf_document_open_stream (document, entries);
    fwrite (compressed, 1, compressed_size, file);
    _cairo_pdf_document_close_stream (document);

    free (rgb);
    free (compressed);

    return stream->id;
}

static cairo_int_status_t
_cairo_pdf_surface_composite_image (cairo_pdf_surface_t	*dst,
				    cairo_surface_pattern_t *pattern)
{
    cairo_pdf_document_t *document = dst->document;
    FILE *file = document->file;
    unsigned id;
    cairo_matrix_t i2u;
    cairo_status_t status;
    cairo_image_surface_t *image;
    cairo_surface_t *src;
    void *image_extra;

    src = pattern->surface;
    status = _cairo_surface_acquire_source_image (src, &image, &image_extra);
    if (!CAIRO_OK (status))
	return status;

    id = emit_image_data (dst->document, image);
    if (id == 0) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto bail;
    }

    _cairo_pdf_surface_add_xobject (dst, id);

    _cairo_pdf_surface_ensure_stream (dst);

    cairo_matrix_copy (&i2u, &pattern->base.matrix);
    cairo_matrix_invert (&i2u);
    cairo_matrix_translate (&i2u, 0, image->height);
    cairo_matrix_scale (&i2u, image->width, -image->height);

    fprintf (file,
	     "q %f %f %f %f %f %f cm /res%d Do Q\r\n",
	     i2u.m[0][0], i2u.m[0][1],
	     i2u.m[1][0], i2u.m[1][1],
	     i2u.m[2][0], i2u.m[2][1],
	     id);

 bail:
    _cairo_surface_release_source_image (src, image, image_extra);

    return status;
}

/* The contents of the surface is already transformed into PDF units,
 * but when we composite the surface we may want to use a different
 * space.  The problem I see now is that the show_surface snippet
 * creates a surface 1x1, which in the snippet environment is the
 * entire surface.  When compositing the surface, cairo gives us the
 * 1x1 to 256x256 matrix.  This would be fine if cairo didn't actually
 * also transform the drawing to the surface.  Should the CTM be part
 * of the current target surface?
 */

static cairo_int_status_t
_cairo_pdf_surface_composite_pdf (cairo_pdf_surface_t *dst,
				  cairo_surface_pattern_t *pattern)
{
    cairo_pdf_document_t *document = dst->document;
    FILE *file = document->file;
    cairo_matrix_t i2u;
    cairo_pdf_stream_t *stream;
    int num_streams, i;
    cairo_pdf_surface_t *src;

    _cairo_pdf_surface_ensure_stream (dst);

    src = (cairo_pdf_surface_t *) pattern->surface;

    cairo_matrix_copy (&i2u, &src->base.matrix);
    cairo_matrix_invert (&i2u);
    cairo_matrix_scale (&i2u, 
			1.0 / (src->width_inches * document->x_ppi),
			1.0 / (src->height_inches * document->y_ppi));

    fprintf (file,
	     "q %f %f %f %f %f %f cm",
	     i2u.m[0][0], i2u.m[0][1],
	     i2u.m[1][0], i2u.m[1][1],
	     i2u.m[2][0], i2u.m[2][1]);

    num_streams = _cairo_array_num_elements (&src->streams);
    for (i = 0; i < num_streams; i++) {
	_cairo_array_copy_element (&src->streams, i, &stream);
	fprintf (file,
		 " /res%d Do",
		 stream->id);

	_cairo_pdf_surface_add_xobject (dst, stream->id);

    }
	
    fprintf (file, " Q\r\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_composite (cairo_operator_t	operator,
			      cairo_pattern_t	*src_pattern,
			      cairo_pattern_t	*mask_pattern,
			      void		*abstract_dst,
			      int		src_x,
			      int		src_y,
			      int		mask_x,
			      int		mask_y,
			      int		dst_x,
			      int		dst_y,
			      unsigned int	width,
			      unsigned int	height)
{
    cairo_pdf_surface_t *dst = abstract_dst;
    cairo_surface_pattern_t *src = (cairo_surface_pattern_t *) src_pattern;

    if (mask_pattern)
 	return CAIRO_STATUS_SUCCESS;
    
    if (src_pattern->type != CAIRO_PATTERN_SURFACE)
	return CAIRO_STATUS_SUCCESS;

    if (src->surface->backend == &cairo_pdf_surface_backend)
	return _cairo_pdf_surface_composite_pdf (dst, src);
    else
	return _cairo_pdf_surface_composite_image (dst, src);
}

static cairo_int_status_t
_cairo_pdf_surface_fill_rectangles (void		*abstract_surface,
				    cairo_operator_t	operator,
				    const cairo_color_t	*color,
				    cairo_rectangle_t	*rects,
				    int			num_rects)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;
    FILE *file = document->file;
    int i;

    _cairo_pdf_surface_ensure_stream (surface);

    fprintf (file,
	     "%f %f %f rg\r\n",
	     color->red, color->green, color->blue);

    for (i = 0; i < num_rects; i++) {
	fprintf (file, 
		 "%d %d %d %d re f\r\n",
		 rects[i].x, rects[i].y,
		 rects[i].width, rects[i].height);
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
emit_solid_pattern (cairo_pdf_surface_t *surface,
		    cairo_solid_pattern_t *pattern)
{
    cairo_pdf_document_t *document = surface->document;
    FILE *file = document->file;
    unsigned int alpha;
    
    alpha = _cairo_pdf_surface_add_alpha (surface, pattern->base.alpha);
    _cairo_pdf_surface_ensure_stream (surface);
    fprintf (file, 
	     "%f %f %f rg /a%d gs\r\n",
	     pattern->red,
	     pattern->green,
	     pattern->blue,
	     alpha);
}

static void
emit_surface_pattern (cairo_pdf_surface_t	*dst,
		      cairo_surface_pattern_t		*pattern)
{
    cairo_pdf_document_t *document = dst->document;
    FILE *file = document->file;
    cairo_pdf_stream_t *stream;
    cairo_image_surface_t *image;
    void *image_extra;
    cairo_status_t status;
    char entries[250];
    unsigned int id, alpha;
    cairo_matrix_t pm;

    if (pattern->surface->backend == &cairo_pdf_surface_backend) {
	return;
    }

    status = _cairo_surface_acquire_source_image (pattern->surface, &image, &image_extra);
    if (!CAIRO_OK (status))
	return;

    _cairo_pdf_document_close_stream (document);

    id = emit_image_data (dst->document, image);

    /* BBox must be smaller than XStep by YStep or acroread wont
     * display the pattern. */

    cairo_matrix_set_identity (&pm);
    cairo_matrix_scale (&pm, image->width, image->height);
    cairo_matrix_copy (&pm, &pattern->base.matrix);
    cairo_matrix_invert (&pm);

    snprintf (entries, sizeof entries,
	      "   /BBox [ 0 0 256 256 ]\r\n"
	      "   /XStep 256\r\n"
	      "   /YStep 256\r\n"
	      "   /PatternType 1\r\n"
	      "   /TilingType 1\r\n"
	      "   /PaintType 1\r\n"
	      "   /Resources << /XObject << /res%d %d 0 R >> >>\r\n"
	      "   /Matrix [ %f %f %f %f %f %f ]\r\n",
	      id, id,
	      pm.m[0][0], pm.m[0][1],
	      pm.m[1][0], pm.m[1][1],
	      pm.m[2][0], pm.m[2][1]);

    stream = _cairo_pdf_document_open_stream (document, entries);

    /* FIXME: emit code to show surface here. */

    _cairo_pdf_surface_add_pattern (dst, stream->id);

    _cairo_pdf_surface_ensure_stream (dst);
    alpha = _cairo_pdf_surface_add_alpha (dst, 1.0);
    fprintf (file,
	     "/Pattern cs /res%d scn /a%d gs\r\n",
	     stream->id, alpha);

    _cairo_surface_release_source_image (pattern->surface, image, image_extra);
}

static unsigned int
emit_pattern_stops (cairo_pdf_surface_t *surface, cairo_gradient_pattern_t *pattern)
{
    cairo_pdf_document_t *document = surface->document;
    FILE *file = document->file;
    unsigned int function_id;

    function_id = _cairo_pdf_document_new_object (document);
    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /FunctionType 0\r\n"
	     "   /Domain [ 0.0 1.0 ]\r\n"
	     "   /Size [ 2 ]\r\n"
	     "   /BitsPerSample 8\r\n"
	     "   /Range [ 0.0 1.0 0.0 1.0 0.0 1.0 ]\r\n"
	     "   /Length 6\r\n"
	     ">>\r\n"
	     "stream\r\n",
	     function_id);

    fputc (pattern->stops[0].color.red   * 0xff, file);
    fputc (pattern->stops[0].color.green * 0xff, file);
    fputc (pattern->stops[0].color.blue  * 0xff, file);
    fputc (pattern->stops[1].color.red   * 0xff, file);
    fputc (pattern->stops[1].color.green * 0xff, file);
    fputc (pattern->stops[1].color.blue  * 0xff, file);

    fprintf (file,
	     "\r\n"
	     "endstream\r\n"
	     "endobj\r\n");

    return function_id;
}

static void
emit_linear_pattern (cairo_pdf_surface_t *surface, cairo_linear_pattern_t *pattern)
{
    cairo_pdf_document_t *document = surface->document;
    FILE *file = document->file;
    unsigned int function_id, pattern_id, alpha;
    double x0, y0, x1, y1;
    cairo_matrix_t p2u;

    _cairo_pdf_document_close_stream (document);

    function_id = emit_pattern_stops (surface, &pattern->base);

    cairo_matrix_copy (&p2u, &pattern->base.base.matrix);
    cairo_matrix_invert (&p2u);

    x0 = pattern->point0.x;
    y0 = pattern->point0.y;
    cairo_matrix_transform_point (&p2u, &x0, &y0);
    x1 = pattern->point1.x;
    y1 = pattern->point1.y;
    cairo_matrix_transform_point (&p2u, &x1, &y1);

    pattern_id = _cairo_pdf_document_new_object (document);
    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /Type /Pattern\r\n"
	     "   /PatternType 2\r\n"
	     "   /Matrix [ 1 0 0 -1 0 %f ]\r\n"
	     "   /Shading\r\n"
	     "      << /ShadingType 2\r\n"
	     "         /ColorSpace /DeviceRGB\r\n"
	     "         /Coords [ %f %f %f %f ]\r\n"
	     "         /Function %d 0 R\r\n"
	     "         /Extend [ true true ]\r\n"
	     "      >>\r\n"
	     ">>\r\n"
	     "endobj\r\n",
	     pattern_id,
	     document->height_inches * document->y_ppi,
	     x0, y0, x1, y1,
	     function_id);
    
    _cairo_pdf_surface_add_pattern (surface, pattern_id);

    _cairo_pdf_surface_ensure_stream (surface);
    alpha = _cairo_pdf_surface_add_alpha (surface, 1.0);

    /* Use pattern */
    fprintf (file,
	     "/Pattern cs /res%d scn /a%d gs\r\n",
	     pattern_id, alpha);
}
	
static void
emit_radial_pattern (cairo_pdf_surface_t *surface, cairo_radial_pattern_t *pattern)
{
    cairo_pdf_document_t *document = surface->document;
    FILE *file = document->file;
    unsigned int function_id, pattern_id, alpha;
    double x0, y0, x1, y1, r0, r1;
    cairo_matrix_t p2u;

    _cairo_pdf_document_close_stream (document);

    function_id = emit_pattern_stops (surface, &pattern->base);

    cairo_matrix_copy (&p2u, &pattern->base.base.matrix);
    cairo_matrix_invert (&p2u);

    x0 = pattern->center0.x;
    y0 = pattern->center0.y;
    r0 = pattern->radius0;
    cairo_matrix_transform_point (&p2u, &x0, &y0);
    x1 = pattern->center1.x;
    y1 = pattern->center1.y;
    r1 = pattern->radius1;
    cairo_matrix_transform_point (&p2u, &x1, &y1);

    /* FIXME: This is surely crack, but how should you scale a radius
     * in a non-orthogonal coordinate system? */
    cairo_matrix_transform_distance (&p2u, &r0, &r1);

    /* FIXME: There is a difference between the cairo gradient extend
     * semantics and PDF extend semantics. PDFs extend=false means
     * that nothing is painted outside the gradient boundaries,
     * whereas cairo takes this to mean that the end color is padded
     * to infinity. Setting extend=true in PDF gives the cairo default
     * behavoir, not yet sure how to implement the cairo mirror and
     * repeat behaviour. */
    pattern_id = _cairo_pdf_document_new_object (document);
    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /Type /Pattern\r\n"
	     "   /PatternType 2\r\n"
	     "   /Matrix [ 1 0 0 -1 0 %f ]\r\n"
	     "   /Shading\r\n"
	     "      << /ShadingType 3\r\n"
	     "         /ColorSpace /DeviceRGB\r\n"
	     "         /Coords [ %f %f %f %f %f %f ]\r\n"
	     "         /Function %d 0 R\r\n"
	     "         /Extend [ true true ]\r\n"
	     "      >>\r\n"
	     ">>\r\n"
	     "endobj\r\n",
	     pattern_id,
	     document->height_inches * document->y_ppi,
	     x0, y0, r0, x1, y1, r1,
	     function_id);
    
    _cairo_pdf_surface_add_pattern (surface, pattern_id);

    _cairo_pdf_surface_ensure_stream (surface);
    alpha = _cairo_pdf_surface_add_alpha (surface, 1.0);

    /* Use pattern */
    fprintf (file,
	     "/Pattern cs /res%d scn /a%d gs\r\n",
	     pattern_id, alpha);
}
	
static void
emit_pattern (cairo_pdf_surface_t *surface, cairo_pattern_t *pattern)
{
    switch (pattern->type) {
    case CAIRO_PATTERN_SOLID:	
	emit_solid_pattern (surface, (cairo_solid_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_SURFACE:
	emit_surface_pattern (surface, (cairo_surface_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_LINEAR:
	emit_linear_pattern (surface, (cairo_linear_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_RADIAL:
	emit_radial_pattern (surface, (cairo_radial_pattern_t *) pattern);
	break;	    
    }
}

static double
intersect (cairo_line_t *line, cairo_fixed_t y)
{
    return _cairo_fixed_to_double (line->p1.x) +
	_cairo_fixed_to_double (line->p2.x - line->p1.x) *
	_cairo_fixed_to_double (y - line->p1.y) /
	_cairo_fixed_to_double (line->p2.y - line->p1.y);
}

static cairo_int_status_t
_cairo_pdf_surface_composite_trapezoids (cairo_operator_t	operator,
					 cairo_pattern_t	*pattern,
					 void			*abstract_dst,
					 int			x_src,
					 int			y_src,
					 int			x_dst,
					 int			y_dst,
					 unsigned int		width,
					 unsigned int		height,
					 cairo_trapezoid_t	*traps,
					 int			num_traps)
{
    cairo_pdf_surface_t *surface = abstract_dst;
    cairo_pdf_document_t *document = surface->document;
    FILE *file = document->file;
    int i;

    emit_pattern (surface, pattern);

    /* After the above switch the current stream should belong to this
     * surface, so no need to _cairo_pdf_surface_ensure_stream() */
    assert (document->current_stream != NULL &&
	    document->current_stream == surface->current_stream);

    for (i = 0; i < num_traps; i++) {
	double left_x1, left_x2, right_x1, right_x2;

	left_x1  = intersect (&traps[i].left, traps[i].top);
	left_x2  = intersect (&traps[i].left, traps[i].bottom);
	right_x1 = intersect (&traps[i].right, traps[i].top);
	right_x2 = intersect (&traps[i].right, traps[i].bottom);

	fprintf (file, 
		 "%f %f m %f %f l %f %f l %f %f l h\r\n",
		 left_x1, _cairo_fixed_to_double (traps[i].top),
		 left_x2, _cairo_fixed_to_double (traps[i].bottom),
		 right_x2, _cairo_fixed_to_double (traps[i].bottom),
		 right_x1, _cairo_fixed_to_double (traps[i].top));
    }

    fprintf (file, 
	     "f\r\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_copy_page (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;

    return _cairo_pdf_document_add_page (document, surface);
}

static cairo_int_status_t
_cairo_pdf_surface_show_page (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;
    cairo_int_status_t status;

    status = _cairo_pdf_document_add_page (document, surface);
    if (status == CAIRO_STATUS_SUCCESS)
	_cairo_pdf_surface_clear (surface);

    return status;
}

static cairo_int_status_t
_cairo_pdf_surface_set_clip_region (void *abstract_surface,
				    pixman_region16_t *region)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_pdf_font_t *
_cairo_pdf_document_get_font (cairo_pdf_document_t	*document,
			      cairo_font_t	        *font)
{
    cairo_unscaled_font_t *unscaled_font;
    cairo_pdf_font_t *pdf_font;
    unsigned int num_fonts, i;

    unscaled_font = _cairo_ft_font_get_unscaled_font (font);

    num_fonts = _cairo_array_num_elements (&document->fonts);
    for (i = 0; i < num_fonts; i++) {
	_cairo_array_copy_element (&document->fonts, i, &pdf_font);
	if (pdf_font->unscaled_font == unscaled_font)
	    return pdf_font;
    }

    /* FIXME: Figure out here which font backend is in use and call
     * the appropriate constructor. */
    pdf_font = cairo_pdf_ft_font_create (document, unscaled_font);
    if (pdf_font == NULL)
	return NULL;

    if (_cairo_array_append (&document->fonts, &pdf_font, 1) == NULL) {
	cairo_pdf_font_destroy (pdf_font);
	return NULL;
    }

    return pdf_font;
}

static cairo_status_t
_cairo_pdf_surface_show_glyphs (cairo_font_t	        *font,
				cairo_operator_t	operator,
				cairo_pattern_t		*pattern,
				void			*abstract_surface,
				int			source_x,
				int			source_y,
				int			dest_x,
				int			dest_y,
				unsigned int		width,
				unsigned int		height,
				const cairo_glyph_t	*glyphs,
				int			num_glyphs)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;
    FILE *file = document->file;
    cairo_pdf_font_t *pdf_font;
    int i, index;

    pdf_font = _cairo_pdf_document_get_font (document, font);
    if (pdf_font == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    emit_pattern (surface, pattern);

    fprintf (file, "BT /res%u 1 Tf", pdf_font->font_id);
    for (i = 0; i < num_glyphs; i++) {

	index = cairo_pdf_font_use_glyph (pdf_font, glyphs[i].index);

	fprintf (file,
		 " %f %f %f %f %f %f Tm (\\%o) Tj",
		 font->scale.matrix[0][0],
		 font->scale.matrix[0][1],
		 font->scale.matrix[1][0],
		 -font->scale.matrix[1][1],
		 glyphs[i].x,
		 glyphs[i].y,
                 index);
    }
    fprintf (file, " ET\r\n");

    _cairo_pdf_surface_add_font (surface, pdf_font->font_id);

    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t cairo_pdf_surface_backend = {
    _cairo_pdf_surface_create_similar,
    _cairo_pdf_surface_destroy,
    _cairo_pdf_surface_pixels_per_inch,
    _cairo_pdf_surface_acquire_source_image,
    _cairo_pdf_surface_release_source_image,
    _cairo_pdf_surface_acquire_dest_image,
    _cairo_pdf_surface_release_dest_image,
    _cairo_pdf_surface_clone_similar,
    _cairo_pdf_surface_composite,
    _cairo_pdf_surface_fill_rectangles,
    _cairo_pdf_surface_composite_trapezoids,
    _cairo_pdf_surface_copy_page,
    _cairo_pdf_surface_show_page,
    _cairo_pdf_surface_set_clip_region,
    _cairo_pdf_surface_show_glyphs
};

static cairo_pdf_document_t *
_cairo_pdf_document_create (FILE	*file,
			   double	width_inches,
			   double	height_inches,
			   double	x_pixels_per_inch,
			   double	y_pixels_per_inch)
{
    cairo_pdf_document_t *document;

    document = malloc (sizeof (cairo_pdf_document_t));
    if (document == NULL)
	return NULL;

    document->file = file;
    document->refcount = 1;
    document->width_inches = width_inches;
    document->height_inches = height_inches;
    document->x_ppi = x_pixels_per_inch;
    document->y_ppi = y_pixels_per_inch;

    _cairo_array_init (&document->objects, sizeof (cairo_pdf_object_t));
    _cairo_array_init (&document->pages, sizeof (unsigned int));
    document->next_available_id = 1;

    document->current_stream = NULL;

    document->pages_id = _cairo_pdf_document_new_object (document);

    _cairo_array_init (&document->fonts, sizeof (cairo_pdf_font_t *));

    /* Document header */
    fprintf (file, "%%PDF-1.4\r\n");

    return document;
}

static unsigned int
_cairo_pdf_document_write_info (cairo_pdf_document_t *document)
{
    FILE *file = document->file;
    unsigned int id;

    id = _cairo_pdf_document_new_object (document);
    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /Creator (cairographics.org)\r\n"
	     "   /Producer (cairographics.org)\r\n"
	     ">>\r\n"
	     "endobj\r\n",
	     id);

    return id;
}

static void
_cairo_pdf_document_write_pages (cairo_pdf_document_t *document)
{
    FILE *file = document->file;
    unsigned int page_id;
    int num_pages, i;

    _cairo_pdf_document_update_object (document, document->pages_id);
    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /Type /Pages\r\n"
	     "   /Kids [ ",
	     document->pages_id);
    
    num_pages = _cairo_array_num_elements (&document->pages);
    for (i = 0; i < num_pages; i++) {
	_cairo_array_copy_element (&document->pages, i, &page_id);
	fprintf (file, "%d 0 R ", page_id);
    }

    fprintf (file, "]\r\n"); 
    fprintf (file, "   /Count %d\r\n", num_pages);

    /* TODO: Figure out wich other defaults to be inherited by /Page
     * objects. */
    fprintf (file,
	     "   /MediaBox [ 0 0 %f %f ]\r\n"
	     ">>\r\n"
	     "endobj\r\n",
	     document->width_inches * document->x_ppi,
	     document->height_inches * document->y_ppi);
}

static cairo_status_t
_cairo_pdf_document_write_fonts (cairo_pdf_document_t *document)
{
    FILE *file = document->file;
    cairo_pdf_font_t *font;
    int num_fonts, i, j;
    const char *data;
    char *compressed;
    unsigned long data_size, compressed_size;
    unsigned int stream_id, descriptor_id;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    num_fonts = _cairo_array_num_elements (&document->fonts);
    for (i = 0; i < num_fonts; i++) {
	_cairo_array_copy_element (&document->fonts, i, &font);

	status = cairo_pdf_font_generate (font, &data, &data_size);
	if (status)
	    goto fail;

	compressed = compress_dup (data, data_size, &compressed_size);
	if (compressed == NULL) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto fail;
	}

	stream_id = _cairo_pdf_document_new_object (document);
	fprintf (file,
		 "%d 0 obj\r\n"
		 "<< /Filter /FlateDecode\r\n"
		 "   /Length %lu\r\n"
		 "   /Length1 %lu\r\n"
		 ">>\r\n"
		 "stream\r\n",
		 stream_id,
		 compressed_size,
		 data_size);
	fwrite (compressed, 1, compressed_size, file);
	fprintf (file,
		 "\r\n"
		 "endstream\r\n"
		 "endobj\r\n");
	free (compressed);

	descriptor_id = _cairo_pdf_document_new_object (document);
	fprintf (file,
		 "%d 0 obj\r\n"
		 "<< /Type /FontDescriptor\r\n"
		 "   /FontName /7%s\r\n"
		 "   /Flags 4\r\n"
		 "   /FontBBox [ %ld %ld %ld %ld ]\r\n"
		 "   /ItalicAngle 0\r\n"
		 "   /Ascent %ld\r\n"
		 "   /Descent %ld\r\n"
		 "   /CapHeight 500\r\n"
		 "   /StemV 80\r\n"
		 "   /StemH 80\r\n"
		 "   /FontFile2 %u 0 R\r\n"
		 ">>\r\n"
		 "endobj\r\n",
		 descriptor_id,
		 font->base_font,
		 font->x_min,
		 font->y_min,
		 font->x_max,
		 font->y_max,
		 font->ascent,
		 font->descent,
		 stream_id);

	_cairo_pdf_document_update_object (document, font->font_id);
	fprintf (file,
		 "%d 0 obj\r\n"
		 "<< /Type /Font\r\n"
		 "   /Subtype /TrueType\r\n"
		 "   /BaseFont /%s\r\n"
		 "   /FirstChar 0\r\n"
		 "   /LastChar %d\r\n"
		 "   /FontDescriptor %d 0 R\r\n"
		 "   /Widths ",
		 font->font_id,
		 font->base_font,
		 font->num_glyphs,
		 descriptor_id);

	fprintf (file,
		 "[");

	for (j = 0; j < font->num_glyphs; j++)
	    fprintf (file,
		     " %d",
		     font->widths[j]);

	fprintf (file,
		 " ]\r\n"
		 ">>\r\n"
		 "endobj\r\n");

    fail:
	cairo_pdf_ft_font_destroy (font);
    }

    return status;
}

static unsigned int
_cairo_pdf_document_write_catalog (cairo_pdf_document_t *document)
{
    FILE *file = document->file;
    unsigned int id;

    id = _cairo_pdf_document_new_object (document);
    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /Type /Catalog\r\n"
	     "   /Pages %d 0 R\r\n" 
	     ">>\r\n"
	     "endobj\r\n",
	     id, document->pages_id);

    return id;
}

static long
_cairo_pdf_document_write_xref (cairo_pdf_document_t *document)
{
    FILE *file = document->file;
    cairo_pdf_object_t *object;
    int num_objects, i;
    long offset;

    num_objects = _cairo_array_num_elements (&document->objects);

    offset = ftell(file);
    fprintf (document->file,
	     "xref\r\n"
	     "%d %d\r\n",
	     0, num_objects + 1);

    fprintf (file, "0000000000 65535 f\r\n");
    for (i = 0; i < num_objects; i++) {
	object = _cairo_array_index (&document->objects, i);
	fprintf (file, "%010ld 00000 n\r\n", object->offset);
    }

    return offset;
}

static void
_cairo_pdf_document_reference (cairo_pdf_document_t *document)
{
    document->refcount++;
}

static void
_cairo_pdf_document_destroy (cairo_pdf_document_t *document)
{
    FILE *file = document->file;
    long offset;
    unsigned int info_id, catalog_id;

    document->refcount--;
    if (document->refcount > 0)
      return;

    _cairo_pdf_document_close_stream (document);
    _cairo_pdf_document_write_pages (document);
    _cairo_pdf_document_write_fonts (document);
    info_id = _cairo_pdf_document_write_info (document);
    catalog_id = _cairo_pdf_document_write_catalog (document);
    offset = _cairo_pdf_document_write_xref (document);
    
    fprintf (file,
	     "trailer\r\n"
	     "<< /Size %d\r\n"
	     "   /Root %d 0 R\r\n"
	     "   /Info %d 0 R\r\n"
	     ">>\r\n",
	     document->next_available_id,
	     catalog_id,
	     info_id);

    fprintf (file,
	     "startxref\r\n"
	     "%ld\r\n"
	     "%%%%EOF\r\n",
	     offset);

    free (document);
}

static cairo_status_t
_cairo_pdf_document_add_page (cairo_pdf_document_t	*document,
			      cairo_pdf_surface_t	*surface)
{
    cairo_pdf_stream_t *stream;
    cairo_pdf_resource_t *res;
    FILE *file = document->file;
    unsigned int page_id;
    double alpha;
    int num_streams, num_alphas, num_resources, i;

    _cairo_pdf_document_close_stream (document);

    page_id = _cairo_pdf_document_new_object (document);
    fprintf (file,
	     "%d 0 obj\r\n"
	     "<< /Type /Page\r\n"
	     "   /Parent %d 0 R\r\n"
	     "   /Contents [",
	     page_id,
	     document->pages_id);

    num_streams = _cairo_array_num_elements (&surface->streams);
    for (i = 0; i < num_streams; i++) {
	_cairo_array_copy_element (&surface->streams, i, &stream);	
	fprintf (file,
		 " %d 0 R",
		 stream->id);
    }

    fprintf (file, 
	     " ]\r\n"
	     "   /Resources <<\r\n");

    num_resources =  _cairo_array_num_elements (&surface->fonts);
    if (num_resources > 0) {
	fprintf (file,
		 "      /Font <<");

	for (i = 0; i < num_resources; i++) {
	    res = _cairo_array_index (&surface->fonts, i);
	    fprintf (file,
		     " /res%d %d 0 R",
		     res->id, res->id);
	}

	fprintf (file,
		 " >>\r\n");
    }
    
    num_alphas =  _cairo_array_num_elements (&surface->alphas);
    if (num_alphas > 0) {
	fprintf (file,
		 "      /ExtGState <<\r\n");

	for (i = 0; i < num_alphas; i++) {
	    _cairo_array_copy_element (&surface->alphas, i, &alpha);
	    fprintf (file,
		     "         /a%d << /ca %f >>\r\n",
		     i, alpha);
	}

	fprintf (file,
		 "      >>\r\n");
    }
    
    num_resources = _cairo_array_num_elements (&surface->patterns);
    if (num_resources > 0) {
	fprintf (file,
		 "      /Pattern <<");
	for (i = 0; i < num_resources; i++) {
	    res = _cairo_array_index (&surface->patterns, i);
	    fprintf (file,
		     " /res%d %d 0 R",
		     res->id, res->id);
	}

	fprintf (file,
		 " >>\r\n");
    }

    num_resources = _cairo_array_num_elements (&surface->xobjects);
    if (num_resources > 0) {
	fprintf (file,
		 "      /XObject <<");

	for (i = 0; i < num_resources; i++) {
	    res = _cairo_array_index (&surface->xobjects, i);
	    fprintf (file,
		     " /res%d %d 0 R",
		     res->id, res->id);
	}

	fprintf (file,
		 " >>\r\n");
    }

    fprintf (file,
	     "   >>\r\n"
	     ">>\r\n"
	     "endobj\r\n");

    _cairo_array_append (&document->pages, &page_id, 1);

    return CAIRO_STATUS_SUCCESS;
}

void
cairo_set_target_pdf (cairo_t	*cr,
		      FILE	*file,
		      double	width_inches,
		      double	height_inches,
		      double	x_pixels_per_inch,
		      double	y_pixels_per_inch)
{
    cairo_surface_t *surface;

    surface = cairo_pdf_surface_create (file,
					width_inches,
					height_inches,
					x_pixels_per_inch,
					y_pixels_per_inch);

    if (surface == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	return;
    }

    cairo_set_target_surface (cr, surface);

    /* cairo_set_target_surface takes a reference, so we must destroy ours */
    cairo_surface_destroy (surface);
}
