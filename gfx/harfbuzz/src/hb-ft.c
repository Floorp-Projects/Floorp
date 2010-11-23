/*
 * Copyright (C) 2009  Red Hat, Inc.
 * Copyright (C) 2009  Keith Stribley <devel@thanlwinsoft.org>
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 */

#include "hb-private.h"

#include "hb-ft.h"

#include "hb-font-private.h"

#include FT_TRUETYPE_TABLES_H

HB_BEGIN_DECLS


static hb_codepoint_t
hb_ft_get_glyph (hb_font_t *font HB_UNUSED,
		 hb_face_t *face HB_UNUSED,
		 const void *user_data,
		 hb_codepoint_t unicode,
		 hb_codepoint_t variation_selector)
{
  FT_Face ft_face = (FT_Face) user_data;

#ifdef HAVE_FT_FACE_GETCHARVARIANTINDEX
  if (unlikely (variation_selector)) {
    hb_codepoint_t glyph = FT_Face_GetCharVariantIndex (ft_face, unicode, variation_selector);
    if (glyph)
      return glyph;
  }
#endif

  return FT_Get_Char_Index (ft_face, unicode);
}

static void
hb_ft_get_glyph_advance (hb_font_t *font HB_UNUSED,
			 hb_face_t *face HB_UNUSED,
			 const void *user_data,
			 hb_codepoint_t glyph,
			 hb_position_t *x_advance,
			 hb_position_t *y_advance)
{
  FT_Face ft_face = (FT_Face) user_data;
  int load_flags = FT_LOAD_DEFAULT;

  /* TODO: load_flags, embolden, etc */

  if (likely (!FT_Load_Glyph (ft_face, glyph, load_flags)))
  {
    *x_advance = ft_face->glyph->advance.x;
    *y_advance = ft_face->glyph->advance.y;
  }
}

static void
hb_ft_get_glyph_extents (hb_font_t *font HB_UNUSED,
			 hb_face_t *face HB_UNUSED,
			 const void *user_data,
			 hb_codepoint_t glyph,
			 hb_glyph_extents_t *extents)
{
  FT_Face ft_face = (FT_Face) user_data;
  int load_flags = FT_LOAD_DEFAULT;

  /* TODO: load_flags, embolden, etc */

  if (likely (!FT_Load_Glyph (ft_face, glyph, load_flags)))
  {
    /* XXX: A few negations should be in order here, not sure. */
    extents->x_bearing = ft_face->glyph->metrics.horiBearingX;
    extents->y_bearing = ft_face->glyph->metrics.horiBearingY;
    extents->width = ft_face->glyph->metrics.width;
    extents->height = ft_face->glyph->metrics.height;
  }
}

static hb_bool_t
hb_ft_get_contour_point (hb_font_t *font HB_UNUSED,
			 hb_face_t *face HB_UNUSED,
			 const void *user_data,
			 unsigned int point_index,
			 hb_codepoint_t glyph,
			 hb_position_t *x,
			 hb_position_t *y)
{
  FT_Face ft_face = (FT_Face) user_data;
  int load_flags = FT_LOAD_DEFAULT;

  /* TODO: load_flags, embolden, etc */

  if (unlikely (FT_Load_Glyph (ft_face, glyph, load_flags)))
      return FALSE;

  if (unlikely (ft_face->glyph->format != FT_GLYPH_FORMAT_OUTLINE))
      return FALSE;

  if (unlikely (point_index >= (unsigned int) ft_face->glyph->outline.n_points))
      return FALSE;

  *x = ft_face->glyph->outline.points[point_index].x;
  *y = ft_face->glyph->outline.points[point_index].y;

  return TRUE;
}

static hb_position_t
hb_ft_get_kerning (hb_font_t *font HB_UNUSED,
		   hb_face_t *face HB_UNUSED,
		   const void *user_data,
		   hb_codepoint_t first_glyph,
		   hb_codepoint_t second_glyph)
{
  FT_Face ft_face = (FT_Face) user_data;
  FT_Vector kerning;

  /* TODO: Kern type? */
  if (FT_Get_Kerning (ft_face, first_glyph, second_glyph, FT_KERNING_DEFAULT, &kerning))
      return 0;

  return kerning.x;
}

static hb_font_funcs_t ft_ffuncs = {
  HB_REFERENCE_COUNT_INVALID, /* ref_count */
  TRUE, /* immutable */
  {
    hb_ft_get_glyph,
    hb_ft_get_glyph_advance,
    hb_ft_get_glyph_extents,
    hb_ft_get_contour_point,
    hb_ft_get_kerning
  }
};

hb_font_funcs_t *
hb_ft_get_font_funcs (void)
{
  return &ft_ffuncs;
}


static hb_blob_t *
get_table  (hb_tag_t tag, void *user_data)
{
  FT_Face ft_face = (FT_Face) user_data;
  FT_Byte *buffer;
  FT_ULong  length = 0;
  FT_Error error;

  if (unlikely (tag == HB_TAG_NONE))
    return NULL;

  error = FT_Load_Sfnt_Table (ft_face, tag, 0, NULL, &length);
  if (error)
    return NULL;

  /* TODO Use FT_Memory? */
  buffer = (FT_Byte *) malloc (length);
  if (buffer == NULL)
    return NULL;

  error = FT_Load_Sfnt_Table (ft_face, tag, 0, buffer, &length);
  if (error)
    return NULL;

  return hb_blob_create ((const char *) buffer, length,
			 HB_MEMORY_MODE_WRITABLE,
			 free, buffer);
}


hb_face_t *
hb_ft_face_create (FT_Face           ft_face,
		   hb_destroy_func_t destroy)
{
  hb_face_t *face;

  if (ft_face->stream->read == NULL) {
    hb_blob_t *blob;

    blob = hb_blob_create ((const char *) ft_face->stream->base,
			   (unsigned int) ft_face->stream->size,
			   /* TODO: Check FT_FACE_FLAG_EXTERNAL_STREAM? */
			   HB_MEMORY_MODE_READONLY_MAY_MAKE_WRITABLE,
			   destroy, ft_face);
    face = hb_face_create_for_data (blob, ft_face->face_index);
    hb_blob_destroy (blob);
  } else {
    face = hb_face_create_for_tables (get_table, destroy, ft_face);
  }

  return face;
}

static void
hb_ft_face_finalize (FT_Face ft_face)
{
  hb_face_destroy ((hb_face_t *) ft_face->generic.data);
}

hb_face_t *
hb_ft_face_create_cached (FT_Face ft_face)
{
  if (unlikely (!ft_face->generic.data || ft_face->generic.finalizer != (FT_Generic_Finalizer) hb_ft_face_finalize))
  {
    if (ft_face->generic.finalizer)
      ft_face->generic.finalizer (ft_face);

    ft_face->generic.data = hb_ft_face_create (ft_face, NULL);
    ft_face->generic.finalizer = (FT_Generic_Finalizer) hb_ft_face_finalize;
  }

  return hb_face_reference ((hb_face_t *) ft_face->generic.data);
}


hb_font_t *
hb_ft_font_create (FT_Face           ft_face,
		   hb_destroy_func_t destroy)
{
  hb_font_t *font;

  font = hb_font_create ();
  hb_font_set_funcs (font,
		     hb_ft_get_font_funcs (),
		     destroy, ft_face);
  hb_font_set_scale (font,
		     ((uint64_t) ft_face->size->metrics.x_scale * (uint64_t) ft_face->units_per_EM) >> 16,
		     ((uint64_t) ft_face->size->metrics.y_scale * (uint64_t) ft_face->units_per_EM) >> 16);
  hb_font_set_ppem (font,
		    ft_face->size->metrics.x_ppem,
		    ft_face->size->metrics.y_ppem);

  return font;
}


HB_END_DECLS
