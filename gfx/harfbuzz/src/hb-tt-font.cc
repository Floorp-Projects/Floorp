/*
 * Copyright © 2011  Google, Inc.
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
 * Google Author(s): Behdad Esfahbod
 */

#include "hb-font-private.hh" /* Shall be first since may include windows.h */

#include "hb-open-type-private.hh"

#include "hb-ot-hhea-table.hh"
#include "hb-ot-hmtx-table.hh"

#include "hb-blob.h"

#include <string.h>



#if 0
struct hb_tt_font_t
{
  const struct hhea *hhea;
  hb_blob_t *hhea_blob;
};


static hb_tt_font_t *
_hb_tt_font_create (hb_font_t *font)
{
  /* TODO Remove this object altogether */
  hb_tt_font_t *tt = (hb_tt_font_t *) calloc (1, sizeof (hb_tt_font_t));

  tt->hhea_blob = Sanitizer<hhea>::sanitize (hb_face_reference_table (font->face, HB_OT_TAG_hhea));
  tt->hhea = Sanitizer<hhea>::lock_instance (tt->hhea_blob);

  return tt;
}

static void
_hb_tt_font_destroy (hb_tt_font_t *tt)
{
  hb_blob_destroy (tt->hhea_blob);

  free (tt);
}

static inline const hhea&
_get_hhea (hb_face_t *face)
{
//  return likely (face->tt && face->tt->hhea) ? *face->tt->hhea : Null(hhea);
}


/*
 * hb_tt_font_funcs_t
 */

static hb_bool_t
hb_font_get_glyph_nil (hb_font_t *font HB_UNUSED,
		       void *font_data HB_UNUSED,
		       hb_codepoint_t unicode,
		       hb_codepoint_t variation_selector,
		       hb_codepoint_t *glyph,
		       void *user_data HB_UNUSED)
{
  if (font->parent)
    return hb_font_get_glyph (font->parent, unicode, variation_selector, glyph);

  *glyph = 0;
  return FALSE;
}

static hb_position_t
hb_font_get_glyph_h_advance_nil (hb_font_t *font HB_UNUSED,
				 void *font_data HB_UNUSED,
				 hb_codepoint_t glyph,
				 void *user_data HB_UNUSED)
{
  if (font->parent)
    return font->parent_scale_x_distance (hb_font_get_glyph_h_advance (font->parent, glyph));

  return font->x_scale;
}

static hb_position_t
hb_font_get_glyph_v_advance_nil (hb_font_t *font HB_UNUSED,
				 void *font_data HB_UNUSED,
				 hb_codepoint_t glyph,
				 void *user_data HB_UNUSED)
{
  if (font->parent)
    return font->parent_scale_y_distance (hb_font_get_glyph_v_advance (font->parent, glyph));

  return font->y_scale;
}

static hb_bool_t
hb_font_get_glyph_h_origin_nil (hb_font_t *font HB_UNUSED,
				void *font_data HB_UNUSED,
				hb_codepoint_t glyph,
				hb_position_t *x,
				hb_position_t *y,
				void *user_data HB_UNUSED)
{
  if (font->parent) {
    hb_bool_t ret = hb_font_get_glyph_h_origin (font->parent,
						glyph,
						x, y);
    if (ret)
      font->parent_scale_position (x, y);
    return ret;
  }

  *x = *y = 0;
  return FALSE;
}

static hb_bool_t
hb_font_get_glyph_v_origin_nil (hb_font_t *font HB_UNUSED,
				void *font_data HB_UNUSED,
				hb_codepoint_t glyph,
				hb_position_t *x,
				hb_position_t *y,
				void *user_data HB_UNUSED)
{
  if (font->parent) {
    hb_bool_t ret = hb_font_get_glyph_v_origin (font->parent,
						glyph,
						x, y);
    if (ret)
      font->parent_scale_position (x, y);
    return ret;
  }

  *x = *y = 0;
  return FALSE;
}

static hb_position_t
hb_font_get_glyph_h_kerning_nil (hb_font_t *font HB_UNUSED,
				 void *font_data HB_UNUSED,
				 hb_codepoint_t left_glyph,
				 hb_codepoint_t right_glyph,
				 void *user_data HB_UNUSED)
{
  if (font->parent)
    return font->parent_scale_x_distance (hb_font_get_glyph_h_kerning (font->parent, left_glyph, right_glyph));

  return 0;
}

static hb_position_t
hb_font_get_glyph_v_kerning_nil (hb_font_t *font HB_UNUSED,
				 void *font_data HB_UNUSED,
				 hb_codepoint_t top_glyph,
				 hb_codepoint_t bottom_glyph,
				 void *user_data HB_UNUSED)
{
  if (font->parent)
    return font->parent_scale_y_distance (hb_font_get_glyph_v_kerning (font->parent, top_glyph, bottom_glyph));

  return 0;
}

static hb_bool_t
hb_font_get_glyph_extents_nil (hb_font_t *font HB_UNUSED,
			       void *font_data HB_UNUSED,
			       hb_codepoint_t glyph,
			       hb_glyph_extents_t *extents,
			       void *user_data HB_UNUSED)
{
  if (font->parent) {
    hb_bool_t ret = hb_font_get_glyph_extents (font->parent,
					       glyph,
					       extents);
    if (ret) {
      font->parent_scale_position (&extents->x_bearing, &extents->y_bearing);
      font->parent_scale_distance (&extents->width, &extents->height);
    }
    return ret;
  }

  memset (extents, 0, sizeof (*extents));
  return FALSE;
}

static hb_bool_t
hb_font_get_glyph_contour_point_nil (hb_font_t *font HB_UNUSED,
				     void *font_data HB_UNUSED,
				     hb_codepoint_t glyph,
				     unsigned int point_index,
				     hb_position_t *x,
				     hb_position_t *y,
				     void *user_data HB_UNUSED)
{
  if (font->parent) {
    hb_bool_t ret = hb_font_get_glyph_contour_point (font->parent,
						     glyph, point_index,
						     x, y);
    if (ret)
      font->parent_scale_position (x, y);
    return ret;
  }

  *x = *y = 0;
  return FALSE;
}


static hb_font_funcs_t _hb_font_funcs_nil = {
  HB_OBJECT_HEADER_STATIC,

  TRUE, /* immutable */

  {
#define HB_FONT_FUNC_IMPLEMENT(name) hb_font_get_##name##_nil,
    HB_FONT_FUNCS_IMPLEMENT_CALLBACKS
#undef HB_FONT_FUNC_IMPLEMENT
  }
};
#endif

