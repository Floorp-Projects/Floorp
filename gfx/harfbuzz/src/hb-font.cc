/*
 * Copyright © 2009  Red Hat, Inc.
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

#include "hb-private.hh"

#include "hb-ot-layout-private.hh"

#include "hb-font-private.hh"
#include "hb-blob.h"
#include "hb-open-file-private.hh"
#include "hb-ot-head-table.hh"

#include "hb-cache-private.hh"

#include <string.h>



/*
 * hb_font_funcs_t
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


hb_font_funcs_t *
hb_font_funcs_create (void)
{
  hb_font_funcs_t *ffuncs;

  if (!(ffuncs = hb_object_create<hb_font_funcs_t> ()))
    return &_hb_font_funcs_nil;

  ffuncs->get = _hb_font_funcs_nil.get;

  return ffuncs;
}

hb_font_funcs_t *
hb_font_funcs_get_empty (void)
{
  return &_hb_font_funcs_nil;
}

hb_font_funcs_t *
hb_font_funcs_reference (hb_font_funcs_t *ffuncs)
{
  return hb_object_reference (ffuncs);
}

void
hb_font_funcs_destroy (hb_font_funcs_t *ffuncs)
{
  if (!hb_object_destroy (ffuncs)) return;

#define HB_FONT_FUNC_IMPLEMENT(name) if (ffuncs->destroy.name) \
  ffuncs->destroy.name (ffuncs->user_data.name);
  HB_FONT_FUNCS_IMPLEMENT_CALLBACKS
#undef HB_FONT_FUNC_IMPLEMENT

  free (ffuncs);
}

hb_bool_t
hb_font_funcs_set_user_data (hb_font_funcs_t    *ffuncs,
			     hb_user_data_key_t *key,
			     void *              data,
			     hb_destroy_func_t   destroy,
			     hb_bool_t           replace)
{
  return hb_object_set_user_data (ffuncs, key, data, destroy, replace);
}

void *
hb_font_funcs_get_user_data (hb_font_funcs_t    *ffuncs,
			     hb_user_data_key_t *key)
{
  return hb_object_get_user_data (ffuncs, key);
}


void
hb_font_funcs_make_immutable (hb_font_funcs_t *ffuncs)
{
  if (hb_object_is_inert (ffuncs))
    return;

  ffuncs->immutable = TRUE;
}

hb_bool_t
hb_font_funcs_is_immutable (hb_font_funcs_t *ffuncs)
{
  return ffuncs->immutable;
}


#define HB_FONT_FUNC_IMPLEMENT(name) \
                                                                         \
void                                                                     \
hb_font_funcs_set_##name##_func (hb_font_funcs_t             *ffuncs,    \
                                 hb_font_get_##name##_func_t  func,      \
                                 void                        *user_data, \
                                 hb_destroy_func_t            destroy)   \
{                                                                        \
  if (ffuncs->immutable) {                                               \
    if (destroy)                                                         \
      destroy (user_data);                                               \
    return;                                                              \
  }                                                                      \
                                                                         \
  if (ffuncs->destroy.name)                                              \
    ffuncs->destroy.name (ffuncs->user_data.name);                       \
                                                                         \
  if (func) {                                                            \
    ffuncs->get.name = func;                                             \
    ffuncs->user_data.name = user_data;                                  \
    ffuncs->destroy.name = destroy;                                      \
  } else {                                                               \
    ffuncs->get.name = hb_font_get_##name##_nil;                         \
    ffuncs->user_data.name = NULL;                                       \
    ffuncs->destroy.name = NULL;                                         \
  }                                                                      \
}

HB_FONT_FUNCS_IMPLEMENT_CALLBACKS
#undef HB_FONT_FUNC_IMPLEMENT


hb_bool_t
hb_font_get_glyph (hb_font_t *font,
		   hb_codepoint_t unicode, hb_codepoint_t variation_selector,
		   hb_codepoint_t *glyph)
{
  *glyph = 0;
  return font->klass->get.glyph (font, font->user_data,
				 unicode, variation_selector, glyph,
				 font->klass->user_data.glyph);
}

hb_position_t
hb_font_get_glyph_h_advance (hb_font_t *font,
			     hb_codepoint_t glyph)
{
  return font->klass->get.glyph_h_advance (font, font->user_data,
					   glyph,
					   font->klass->user_data.glyph_h_advance);
}

hb_position_t
hb_font_get_glyph_v_advance (hb_font_t *font,
			     hb_codepoint_t glyph)
{
  return font->klass->get.glyph_v_advance (font, font->user_data,
					   glyph,
					   font->klass->user_data.glyph_v_advance);
}

hb_bool_t
hb_font_get_glyph_h_origin (hb_font_t *font,
			    hb_codepoint_t glyph,
			    hb_position_t *x, hb_position_t *y)
{
  *x = *y = 0;
  return font->klass->get.glyph_h_origin (font, font->user_data,
					   glyph, x, y,
					   font->klass->user_data.glyph_h_origin);
}

hb_bool_t
hb_font_get_glyph_v_origin (hb_font_t *font,
			    hb_codepoint_t glyph,
			    hb_position_t *x, hb_position_t *y)
{
  *x = *y = 0;
  return font->klass->get.glyph_v_origin (font, font->user_data,
					   glyph, x, y,
					   font->klass->user_data.glyph_v_origin);
}

hb_position_t
hb_font_get_glyph_h_kerning (hb_font_t *font,
			     hb_codepoint_t left_glyph, hb_codepoint_t right_glyph)
{
  return font->klass->get.glyph_h_kerning (font, font->user_data,
					   left_glyph, right_glyph,
					   font->klass->user_data.glyph_h_kerning);
}

hb_position_t
hb_font_get_glyph_v_kerning (hb_font_t *font,
			     hb_codepoint_t left_glyph, hb_codepoint_t right_glyph)
{
  return font->klass->get.glyph_v_kerning (font, font->user_data,
				     left_glyph, right_glyph,
				     font->klass->user_data.glyph_v_kerning);
}

hb_bool_t
hb_font_get_glyph_extents (hb_font_t *font,
			   hb_codepoint_t glyph,
			   hb_glyph_extents_t *extents)
{
  memset (extents, 0, sizeof (*extents));
  return font->klass->get.glyph_extents (font, font->user_data,
					 glyph,
					 extents,
					 font->klass->user_data.glyph_extents);
}

hb_bool_t
hb_font_get_glyph_contour_point (hb_font_t *font,
				 hb_codepoint_t glyph, unsigned int point_index,
				 hb_position_t *x, hb_position_t *y)
{
  *x = *y = 0;
  return font->klass->get.glyph_contour_point (font, font->user_data,
					       glyph, point_index,
					       x, y,
					       font->klass->user_data.glyph_contour_point);
}


/* A bit higher-level, and with fallback */

void
hb_font_get_glyph_advance_for_direction (hb_font_t *font,
					 hb_codepoint_t glyph,
					 hb_direction_t direction,
					 hb_position_t *x, hb_position_t *y)
{
  if (likely (HB_DIRECTION_IS_HORIZONTAL (direction))) {
    *x = hb_font_get_glyph_h_advance (font, glyph);
    *y = 0;
  } else {
    *x = 0;
    *y = hb_font_get_glyph_v_advance (font, glyph);
  }
}

static void
guess_v_origin_minus_h_origin (hb_font_t *font,
			       hb_codepoint_t glyph,
			       hb_position_t *x, hb_position_t *y)
{
  *x = hb_font_get_glyph_h_advance (font, glyph) / 2;

  /* TODO use font_metics.ascent */
  *y = font->y_scale;
}


void
hb_font_get_glyph_origin_for_direction (hb_font_t *font,
					hb_codepoint_t glyph,
					hb_direction_t direction,
					hb_position_t *x, hb_position_t *y)
{
  if (likely (HB_DIRECTION_IS_HORIZONTAL (direction))) {
    hb_bool_t ret = hb_font_get_glyph_h_origin (font, glyph, x, y);
    if (!ret && (ret = hb_font_get_glyph_v_origin (font, glyph, x, y))) {
      hb_position_t dx, dy;
      guess_v_origin_minus_h_origin (font, glyph, &dx, &dy);
      *x -= dx; *y -= dy;
    }
  } else {
    hb_bool_t ret = hb_font_get_glyph_v_origin (font, glyph, x, y);
    if (!ret && (ret = hb_font_get_glyph_h_origin (font, glyph, x, y))) {
      hb_position_t dx, dy;
      guess_v_origin_minus_h_origin (font, glyph, &dx, &dy);
      *x += dx; *y += dy;
    }
  }
}

void
hb_font_add_glyph_origin_for_direction (hb_font_t *font,
					hb_codepoint_t glyph,
					hb_direction_t direction,
					hb_position_t *x, hb_position_t *y)
{
  hb_position_t origin_x, origin_y;

  hb_font_get_glyph_origin_for_direction (font, glyph, direction, &origin_x, &origin_y);

  *x += origin_x;
  *y += origin_y;
}

void
hb_font_subtract_glyph_origin_for_direction (hb_font_t *font,
					     hb_codepoint_t glyph,
					     hb_direction_t direction,
					     hb_position_t *x, hb_position_t *y)
{
  hb_position_t origin_x, origin_y;

  hb_font_get_glyph_origin_for_direction (font, glyph, direction, &origin_x, &origin_y);

  *x -= origin_x;
  *y -= origin_y;
}

void
hb_font_get_glyph_kerning_for_direction (hb_font_t *font,
					 hb_codepoint_t first_glyph, hb_codepoint_t second_glyph,
					 hb_direction_t direction,
					 hb_position_t *x, hb_position_t *y)
{
  if (likely (HB_DIRECTION_IS_HORIZONTAL (direction))) {
    *x = hb_font_get_glyph_h_kerning (font, first_glyph, second_glyph);
    *y = 0;
  } else {
    *x = 0;
    *y = hb_font_get_glyph_v_kerning (font, first_glyph, second_glyph);
  }
}

hb_bool_t
hb_font_get_glyph_extents_for_origin (hb_font_t *font,
				      hb_codepoint_t glyph,
				      hb_direction_t direction,
				      hb_glyph_extents_t *extents)
{
  hb_bool_t ret = hb_font_get_glyph_extents (font, glyph, extents);

  if (ret)
    hb_font_subtract_glyph_origin_for_direction (font, glyph, direction, &extents->x_bearing, &extents->y_bearing);

  return ret;
}

hb_bool_t
hb_font_get_glyph_contour_point_for_origin (hb_font_t *font,
					    hb_codepoint_t glyph, unsigned int point_index,
					    hb_direction_t direction,
					    hb_position_t *x, hb_position_t *y)
{
  hb_bool_t ret = hb_font_get_glyph_contour_point (font, glyph, point_index, x, y);

  if (ret)
    hb_font_subtract_glyph_origin_for_direction (font, glyph, direction, x, y);

  return ret;
}


/*
 * hb_face_t
 */

static hb_face_t _hb_face_nil = {
  HB_OBJECT_HEADER_STATIC,

  TRUE, /* immutable */

  NULL, /* reference_table */
  NULL, /* user_data */
  NULL, /* destroy */

  NULL, /* ot_layout */

  0,    /* index */
  1000  /* upem */
};


hb_face_t *
hb_face_create_for_tables (hb_reference_table_func_t  reference_table,
			   void                      *user_data,
			   hb_destroy_func_t          destroy)
{
  hb_face_t *face;

  if (!reference_table || !(face = hb_object_create<hb_face_t> ())) {
    if (destroy)
      destroy (user_data);
    return &_hb_face_nil;
  }

  face->reference_table = reference_table;
  face->user_data = user_data;
  face->destroy = destroy;

  face->ot_layout = _hb_ot_layout_create (face);

  face->upem = 0;

  return face;
}


typedef struct _hb_face_for_data_closure_t {
  hb_blob_t *blob;
  unsigned int  index;
} hb_face_for_data_closure_t;

static hb_face_for_data_closure_t *
_hb_face_for_data_closure_create (hb_blob_t *blob, unsigned int index)
{
  hb_face_for_data_closure_t *closure;

  closure = (hb_face_for_data_closure_t *) malloc (sizeof (hb_face_for_data_closure_t));
  if (unlikely (!closure))
    return NULL;

  closure->blob = blob;
  closure->index = index;

  return closure;
}

static void
_hb_face_for_data_closure_destroy (hb_face_for_data_closure_t *closure)
{
  hb_blob_destroy (closure->blob);
  free (closure);
}

static hb_blob_t *
_hb_face_for_data_reference_table (hb_face_t *face HB_UNUSED, hb_tag_t tag, void *user_data)
{
  hb_face_for_data_closure_t *data = (hb_face_for_data_closure_t *) user_data;

  if (tag == HB_TAG_NONE)
    return hb_blob_reference (data->blob);

  const OpenTypeFontFile &ot_file = *Sanitizer<OpenTypeFontFile>::lock_instance (data->blob);
  const OpenTypeFontFace &ot_face = ot_file.get_face (data->index);

  const OpenTypeTable &table = ot_face.get_table_by_tag (tag);

  hb_blob_t *blob = hb_blob_create_sub_blob (data->blob, table.offset, table.length);

  return blob;
}

hb_face_t *
hb_face_create (hb_blob_t    *blob,
		unsigned int  index)
{
  hb_face_t *face;

  if (unlikely (!blob || !hb_blob_get_length (blob)))
    return &_hb_face_nil;

  hb_face_for_data_closure_t *closure = _hb_face_for_data_closure_create (Sanitizer<OpenTypeFontFile>::sanitize (hb_blob_reference (blob)), index);

  if (unlikely (!closure))
    return &_hb_face_nil;

  face = hb_face_create_for_tables (_hb_face_for_data_reference_table,
				    closure,
				    (hb_destroy_func_t) _hb_face_for_data_closure_destroy);

  hb_face_set_index (face, index);

  return face;
}

hb_face_t *
hb_face_get_empty (void)
{
  return &_hb_face_nil;
}


hb_face_t *
hb_face_reference (hb_face_t *face)
{
  return hb_object_reference (face);
}

void
hb_face_destroy (hb_face_t *face)
{
  if (!hb_object_destroy (face)) return;

  _hb_ot_layout_destroy (face->ot_layout);

  if (face->destroy)
    face->destroy (face->user_data);

  free (face);
}

hb_bool_t
hb_face_set_user_data (hb_face_t          *face,
		       hb_user_data_key_t *key,
		       void *              data,
		       hb_destroy_func_t   destroy,
		       hb_bool_t           replace)
{
  return hb_object_set_user_data (face, key, data, destroy, replace);
}

void *
hb_face_get_user_data (hb_face_t          *face,
		       hb_user_data_key_t *key)
{
  return hb_object_get_user_data (face, key);
}

void
hb_face_make_immutable (hb_face_t *face)
{
  if (hb_object_is_inert (face))
    return;

  face->immutable = true;
}

hb_bool_t
hb_face_is_immutable (hb_face_t *face)
{
  return face->immutable;
}


hb_blob_t *
hb_face_reference_table (hb_face_t *face,
			 hb_tag_t   tag)
{
  hb_blob_t *blob;

  if (unlikely (!face || !face->reference_table))
    return hb_blob_get_empty ();

  blob = face->reference_table (face, tag, face->user_data);
  if (unlikely (!blob))
    return hb_blob_get_empty ();

  return blob;
}

hb_blob_t *
hb_face_reference_blob (hb_face_t *face)
{
  return hb_face_reference_table (face, HB_TAG_NONE);
}

void
hb_face_set_index (hb_face_t    *face,
		   unsigned int  index)
{
  if (hb_object_is_inert (face))
    return;

  face->index = index;
}

unsigned int
hb_face_get_index (hb_face_t    *face)
{
  return face->index;
}

void
hb_face_set_upem (hb_face_t    *face,
		  unsigned int  upem)
{
  if (hb_object_is_inert (face))
    return;

  face->upem = upem;
}

unsigned int
hb_face_get_upem (hb_face_t *face)
{
  if (unlikely (!face->upem)) {
    hb_blob_t *head_blob = Sanitizer<head>::sanitize (hb_face_reference_table (face, HB_OT_TAG_head));
    const head *head_table = Sanitizer<head>::lock_instance (head_blob);
    face->upem = head_table->get_upem ();
    hb_blob_destroy (head_blob);
  }
  return face->upem;
}


/*
 * hb_font_t
 */

static hb_font_t _hb_font_nil = {
  HB_OBJECT_HEADER_STATIC,

  TRUE, /* immutable */

  NULL, /* parent */
  &_hb_face_nil,

  0, /* x_scale */
  0, /* y_scale */

  0, /* x_ppem */
  0, /* y_ppem */

  &_hb_font_funcs_nil, /* klass */
  NULL, /* user_data */
  NULL  /* destroy */
};

hb_font_t *
hb_font_create (hb_face_t *face)
{
  hb_font_t *font;

  if (unlikely (!face))
    face = &_hb_face_nil;
  if (unlikely (hb_object_is_inert (face)))
    return &_hb_font_nil;
  if (!(font = hb_object_create<hb_font_t> ()))
    return &_hb_font_nil;

  hb_face_make_immutable (face);
  font->face = hb_face_reference (face);
  font->klass = &_hb_font_funcs_nil;

  return font;
}

hb_font_t *
hb_font_create_sub_font (hb_font_t *parent)
{
  if (unlikely (!parent))
    return &_hb_font_nil;

  hb_font_t *font = hb_font_create (parent->face);

  if (unlikely (hb_object_is_inert (font)))
    return font;

  hb_font_make_immutable (parent);
  font->parent = hb_font_reference (parent);

  font->x_scale = parent->x_scale;
  font->y_scale = parent->y_scale;
  font->x_ppem = parent->x_ppem;
  font->y_ppem = parent->y_ppem;

  font->klass = &_hb_font_funcs_nil;

  return font;
}

hb_font_t *
hb_font_get_empty (void)
{
  return &_hb_font_nil;
}

hb_font_t *
hb_font_reference (hb_font_t *font)
{
  return hb_object_reference (font);
}

void
hb_font_destroy (hb_font_t *font)
{
  if (!hb_object_destroy (font)) return;

  hb_font_destroy (font->parent);
  hb_face_destroy (font->face);
  hb_font_funcs_destroy (font->klass);
  if (font->destroy)
    font->destroy (font->user_data);

  free (font);
}

hb_bool_t
hb_font_set_user_data (hb_font_t          *font,
		       hb_user_data_key_t *key,
		       void *              data,
		       hb_destroy_func_t   destroy,
		       hb_bool_t           replace)
{
  return hb_object_set_user_data (font, key, data, destroy, replace);
}

void *
hb_font_get_user_data (hb_font_t          *font,
		       hb_user_data_key_t *key)
{
  return hb_object_get_user_data (font, key);
}

void
hb_font_make_immutable (hb_font_t *font)
{
  if (hb_object_is_inert (font))
    return;

  font->immutable = true;
}

hb_bool_t
hb_font_is_immutable (hb_font_t *font)
{
  return font->immutable;
}

hb_font_t *
hb_font_get_parent (hb_font_t *font)
{
  return font->parent;
}

hb_face_t *
hb_font_get_face (hb_font_t *font)
{
  return font->face;
}


void
hb_font_set_funcs (hb_font_t         *font,
		   hb_font_funcs_t   *klass,
		   void              *user_data,
		   hb_destroy_func_t  destroy)
{
  if (font->immutable) {
    if (destroy)
      destroy (user_data);
    return;
  }

  if (font->destroy)
    font->destroy (font->user_data);

  if (!klass)
    klass = &_hb_font_funcs_nil;

  hb_font_funcs_reference (klass);
  hb_font_funcs_destroy (font->klass);
  font->klass = klass;
  font->user_data = user_data;
  font->destroy = destroy;
}

void
hb_font_set_funcs_data (hb_font_t         *font,
		        void              *user_data,
		        hb_destroy_func_t  destroy)
{
  /* Destroy user_data? */
  if (font->immutable) {
    if (destroy)
      destroy (user_data);
    return;
  }

  if (font->destroy)
    font->destroy (font->user_data);

  font->user_data = user_data;
  font->destroy = destroy;
}


void
hb_font_set_scale (hb_font_t *font,
		   int x_scale,
		   int y_scale)
{
  if (font->immutable)
    return;

  font->x_scale = x_scale;
  font->y_scale = y_scale;
}

void
hb_font_get_scale (hb_font_t *font,
		   int *x_scale,
		   int *y_scale)
{
  if (x_scale) *x_scale = font->x_scale;
  if (y_scale) *y_scale = font->y_scale;
}

void
hb_font_set_ppem (hb_font_t *font,
		  unsigned int x_ppem,
		  unsigned int y_ppem)
{
  if (font->immutable)
    return;

  font->x_ppem = x_ppem;
  font->y_ppem = y_ppem;
}

void
hb_font_get_ppem (hb_font_t *font,
		  unsigned int *x_ppem,
		  unsigned int *y_ppem)
{
  if (x_ppem) *x_ppem = font->x_ppem;
  if (y_ppem) *y_ppem = font->y_ppem;
}


