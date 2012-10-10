/*
 * Copyright © 2009  Red Hat, Inc.
 * Copyright © 2012  Google, Inc.
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
 * Google Author(s): Behdad Esfahbod
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
hb_font_get_glyph_nil (hb_font_t *font,
		       void *font_data HB_UNUSED,
		       hb_codepoint_t unicode,
		       hb_codepoint_t variation_selector,
		       hb_codepoint_t *glyph,
		       void *user_data HB_UNUSED)
{
  if (font->parent)
    return font->parent->get_glyph (unicode, variation_selector, glyph);

  *glyph = 0;
  return false;
}

static hb_position_t
hb_font_get_glyph_h_advance_nil (hb_font_t *font,
				 void *font_data HB_UNUSED,
				 hb_codepoint_t glyph,
				 void *user_data HB_UNUSED)
{
  if (font->parent)
    return font->parent_scale_x_distance (font->parent->get_glyph_h_advance (glyph));

  return font->x_scale;
}

static hb_position_t
hb_font_get_glyph_v_advance_nil (hb_font_t *font,
				 void *font_data HB_UNUSED,
				 hb_codepoint_t glyph,
				 void *user_data HB_UNUSED)
{
  if (font->parent)
    return font->parent_scale_y_distance (font->parent->get_glyph_v_advance (glyph));

  return font->y_scale;
}

static hb_bool_t
hb_font_get_glyph_h_origin_nil (hb_font_t *font,
				void *font_data HB_UNUSED,
				hb_codepoint_t glyph,
				hb_position_t *x,
				hb_position_t *y,
				void *user_data HB_UNUSED)
{
  if (font->parent) {
    hb_bool_t ret = font->parent->get_glyph_h_origin (glyph, x, y);
    if (ret)
      font->parent_scale_position (x, y);
    return ret;
  }

  *x = *y = 0;
  return false;
}

static hb_bool_t
hb_font_get_glyph_v_origin_nil (hb_font_t *font,
				void *font_data HB_UNUSED,
				hb_codepoint_t glyph,
				hb_position_t *x,
				hb_position_t *y,
				void *user_data HB_UNUSED)
{
  if (font->parent) {
    hb_bool_t ret = font->parent->get_glyph_v_origin (glyph, x, y);
    if (ret)
      font->parent_scale_position (x, y);
    return ret;
  }

  *x = *y = 0;
  return false;
}

static hb_position_t
hb_font_get_glyph_h_kerning_nil (hb_font_t *font,
				 void *font_data HB_UNUSED,
				 hb_codepoint_t left_glyph,
				 hb_codepoint_t right_glyph,
				 void *user_data HB_UNUSED)
{
  if (font->parent)
    return font->parent_scale_x_distance (font->parent->get_glyph_h_kerning (left_glyph, right_glyph));

  return 0;
}

static hb_position_t
hb_font_get_glyph_v_kerning_nil (hb_font_t *font,
				 void *font_data HB_UNUSED,
				 hb_codepoint_t top_glyph,
				 hb_codepoint_t bottom_glyph,
				 void *user_data HB_UNUSED)
{
  if (font->parent)
    return font->parent_scale_y_distance (font->parent->get_glyph_v_kerning (top_glyph, bottom_glyph));

  return 0;
}

static hb_bool_t
hb_font_get_glyph_extents_nil (hb_font_t *font,
			       void *font_data HB_UNUSED,
			       hb_codepoint_t glyph,
			       hb_glyph_extents_t *extents,
			       void *user_data HB_UNUSED)
{
  if (font->parent) {
    hb_bool_t ret = font->parent->get_glyph_extents (glyph, extents);
    if (ret) {
      font->parent_scale_position (&extents->x_bearing, &extents->y_bearing);
      font->parent_scale_distance (&extents->width, &extents->height);
    }
    return ret;
  }

  memset (extents, 0, sizeof (*extents));
  return false;
}

static hb_bool_t
hb_font_get_glyph_contour_point_nil (hb_font_t *font,
				     void *font_data HB_UNUSED,
				     hb_codepoint_t glyph,
				     unsigned int point_index,
				     hb_position_t *x,
				     hb_position_t *y,
				     void *user_data HB_UNUSED)
{
  if (font->parent) {
    hb_bool_t ret = font->parent->get_glyph_contour_point (glyph, point_index, x, y);
    if (ret)
      font->parent_scale_position (x, y);
    return ret;
  }

  *x = *y = 0;
  return false;
}

static hb_bool_t
hb_font_get_glyph_name_nil (hb_font_t *font,
			    void *font_data HB_UNUSED,
			    hb_codepoint_t glyph,
			    char *name, unsigned int size,
			    void *user_data HB_UNUSED)
{
  if (font->parent)
    return font->parent->get_glyph_name (glyph, name, size);

  if (size) *name = '\0';
  return false;
}

static hb_bool_t
hb_font_get_glyph_from_name_nil (hb_font_t *font,
				 void *font_data HB_UNUSED,
				 const char *name, int len, /* -1 means nul-terminated */
				 hb_codepoint_t *glyph,
				 void *user_data HB_UNUSED)
{
  if (font->parent)
    return font->parent->get_glyph_from_name (name, len, glyph);

  *glyph = 0;
  return false;
}


static const hb_font_funcs_t _hb_font_funcs_nil = {
  HB_OBJECT_HEADER_STATIC,

  true, /* immutable */

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
    return hb_font_funcs_get_empty ();

  ffuncs->get = _hb_font_funcs_nil.get;

  return ffuncs;
}

hb_font_funcs_t *
hb_font_funcs_get_empty (void)
{
  return const_cast<hb_font_funcs_t *> (&_hb_font_funcs_nil);
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

  ffuncs->immutable = true;
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


/* Public getters */

hb_bool_t
hb_font_get_glyph (hb_font_t *font,
		   hb_codepoint_t unicode, hb_codepoint_t variation_selector,
		   hb_codepoint_t *glyph)
{
  return font->get_glyph (unicode, variation_selector, glyph);
}

hb_position_t
hb_font_get_glyph_h_advance (hb_font_t *font,
			     hb_codepoint_t glyph)
{
  return font->get_glyph_h_advance (glyph);
}

hb_position_t
hb_font_get_glyph_v_advance (hb_font_t *font,
			     hb_codepoint_t glyph)
{
  return font->get_glyph_v_advance (glyph);
}

hb_bool_t
hb_font_get_glyph_h_origin (hb_font_t *font,
			    hb_codepoint_t glyph,
			    hb_position_t *x, hb_position_t *y)
{
  return font->get_glyph_h_origin (glyph, x, y);
}

hb_bool_t
hb_font_get_glyph_v_origin (hb_font_t *font,
			    hb_codepoint_t glyph,
			    hb_position_t *x, hb_position_t *y)
{
  return font->get_glyph_v_origin (glyph, x, y);
}

hb_position_t
hb_font_get_glyph_h_kerning (hb_font_t *font,
			     hb_codepoint_t left_glyph, hb_codepoint_t right_glyph)
{
  return font->get_glyph_h_kerning (left_glyph, right_glyph);
}

hb_position_t
hb_font_get_glyph_v_kerning (hb_font_t *font,
			     hb_codepoint_t left_glyph, hb_codepoint_t right_glyph)
{
  return font->get_glyph_v_kerning (left_glyph, right_glyph);
}

hb_bool_t
hb_font_get_glyph_extents (hb_font_t *font,
			   hb_codepoint_t glyph,
			   hb_glyph_extents_t *extents)
{
  return font->get_glyph_extents (glyph, extents);
}

hb_bool_t
hb_font_get_glyph_contour_point (hb_font_t *font,
				 hb_codepoint_t glyph, unsigned int point_index,
				 hb_position_t *x, hb_position_t *y)
{
  return font->get_glyph_contour_point (glyph, point_index, x, y);
}

hb_bool_t
hb_font_get_glyph_name (hb_font_t *font,
			hb_codepoint_t glyph,
			char *name, unsigned int size)
{
  return font->get_glyph_name (glyph, name, size);
}

hb_bool_t
hb_font_get_glyph_from_name (hb_font_t *font,
			     const char *name, int len, /* -1 means nul-terminated */
			     hb_codepoint_t *glyph)
{
  return font->get_glyph_from_name (name, len, glyph);
}


/* A bit higher-level, and with fallback */

void
hb_font_get_glyph_advance_for_direction (hb_font_t *font,
					 hb_codepoint_t glyph,
					 hb_direction_t direction,
					 hb_position_t *x, hb_position_t *y)
{
  return font->get_glyph_advance_for_direction (glyph, direction, x, y);
}

void
hb_font_get_glyph_origin_for_direction (hb_font_t *font,
					hb_codepoint_t glyph,
					hb_direction_t direction,
					hb_position_t *x, hb_position_t *y)
{
  return font->get_glyph_origin_for_direction (glyph, direction, x, y);
}

void
hb_font_add_glyph_origin_for_direction (hb_font_t *font,
					hb_codepoint_t glyph,
					hb_direction_t direction,
					hb_position_t *x, hb_position_t *y)
{
  return font->add_glyph_origin_for_direction (glyph, direction, x, y);
}

void
hb_font_subtract_glyph_origin_for_direction (hb_font_t *font,
					     hb_codepoint_t glyph,
					     hb_direction_t direction,
					     hb_position_t *x, hb_position_t *y)
{
  return font->subtract_glyph_origin_for_direction (glyph, direction, x, y);
}

void
hb_font_get_glyph_kerning_for_direction (hb_font_t *font,
					 hb_codepoint_t first_glyph, hb_codepoint_t second_glyph,
					 hb_direction_t direction,
					 hb_position_t *x, hb_position_t *y)
{
  return font->get_glyph_kerning_for_direction (first_glyph, second_glyph, direction, x, y);
}

hb_bool_t
hb_font_get_glyph_extents_for_origin (hb_font_t *font,
				      hb_codepoint_t glyph,
				      hb_direction_t direction,
				      hb_glyph_extents_t *extents)
{
  return font->get_glyph_extents_for_origin (glyph, direction, extents);
}

hb_bool_t
hb_font_get_glyph_contour_point_for_origin (hb_font_t *font,
					    hb_codepoint_t glyph, unsigned int point_index,
					    hb_direction_t direction,
					    hb_position_t *x, hb_position_t *y)
{
  return font->get_glyph_contour_point_for_origin (glyph, point_index, direction, x, y);
}

/* Generates gidDDD if glyph has no name. */
void
hb_font_glyph_to_string (hb_font_t *font,
			 hb_codepoint_t glyph,
			 char *s, unsigned int size)
{
  font->glyph_to_string (glyph, s, size);
}

/* Parses gidDDD and uniUUUU strings automatically. */
hb_bool_t
hb_font_glyph_from_string (hb_font_t *font,
			   const char *s, int len, /* -1 means nul-terminated */
			   hb_codepoint_t *glyph)
{
  return font->glyph_from_string (s, len, glyph);
}


/*
 * hb_face_t
 */

static const hb_face_t _hb_face_nil = {
  HB_OBJECT_HEADER_STATIC,

  true, /* immutable */

  NULL, /* reference_table_func */
  NULL, /* user_data */
  NULL, /* destroy */

  0,    /* index */
  1000, /* upem */

  {
#define HB_SHAPER_IMPLEMENT(shaper) HB_SHAPER_DATA_INVALID,
#include "hb-shaper-list.hh"
#undef HB_SHAPER_IMPLEMENT
  },

  NULL, /* shape_plans */
};


hb_face_t *
hb_face_create_for_tables (hb_reference_table_func_t  reference_table_func,
			   void                      *user_data,
			   hb_destroy_func_t          destroy)
{
  hb_face_t *face;

  if (!reference_table_func || !(face = hb_object_create<hb_face_t> ())) {
    if (destroy)
      destroy (user_data);
    return hb_face_get_empty ();
  }

  face->reference_table_func = reference_table_func;
  face->user_data = user_data;
  face->destroy = destroy;

  face->upem = 0;

  return face;
}


typedef struct hb_face_for_data_closure_t {
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

  const OT::OpenTypeFontFile &ot_file = *OT::Sanitizer<OT::OpenTypeFontFile>::lock_instance (data->blob);
  const OT::OpenTypeFontFace &ot_face = ot_file.get_face (data->index);

  const OT::OpenTypeTable &table = ot_face.get_table_by_tag (tag);

  hb_blob_t *blob = hb_blob_create_sub_blob (data->blob, table.offset, table.length);

  return blob;
}

hb_face_t *
hb_face_create (hb_blob_t    *blob,
		unsigned int  index)
{
  hb_face_t *face;

  if (unlikely (!blob || !hb_blob_get_length (blob)))
    return hb_face_get_empty ();

  hb_face_for_data_closure_t *closure = _hb_face_for_data_closure_create (OT::Sanitizer<OT::OpenTypeFontFile>::sanitize (hb_blob_reference (blob)), index);

  if (unlikely (!closure))
    return hb_face_get_empty ();

  face = hb_face_create_for_tables (_hb_face_for_data_reference_table,
				    closure,
				    (hb_destroy_func_t) _hb_face_for_data_closure_destroy);

  hb_face_set_index (face, index);

  return face;
}

hb_face_t *
hb_face_get_empty (void)
{
  return const_cast<hb_face_t *> (&_hb_face_nil);
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

  for (hb_face_t::plan_node_t *node = face->shape_plans; node; )
  {
    hb_face_t::plan_node_t *next = node->next;
    hb_shape_plan_destroy (node->shape_plan);
    free (node);
    node = next;
  }

#define HB_SHAPER_IMPLEMENT(shaper) HB_SHAPER_DATA_DESTROY(shaper, face);
#include "hb-shaper-list.hh"
#undef HB_SHAPER_IMPLEMENT

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
  return face->reference_table (tag);
}

hb_blob_t *
hb_face_reference_blob (hb_face_t *face)
{
  return face->reference_table (HB_TAG_NONE);
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
  return face->get_upem ();
}


void
hb_face_t::load_upem (void) const
{
  hb_blob_t *head_blob = OT::Sanitizer<OT::head>::sanitize (reference_table (HB_OT_TAG_head));
  const OT::head *head_table = OT::Sanitizer<OT::head>::lock_instance (head_blob);
  upem = head_table->get_upem ();
  hb_blob_destroy (head_blob);
}


/*
 * hb_font_t
 */

hb_font_t *
hb_font_create (hb_face_t *face)
{
  hb_font_t *font;

  if (unlikely (!face))
    face = hb_face_get_empty ();
  if (unlikely (hb_object_is_inert (face)))
    return hb_font_get_empty ();
  if (!(font = hb_object_create<hb_font_t> ()))
    return hb_font_get_empty ();

  hb_face_make_immutable (face);
  font->face = hb_face_reference (face);
  font->klass = hb_font_funcs_get_empty ();

  return font;
}

hb_font_t *
hb_font_create_sub_font (hb_font_t *parent)
{
  if (unlikely (!parent))
    return hb_font_get_empty ();

  hb_font_t *font = hb_font_create (parent->face);

  if (unlikely (hb_object_is_inert (font)))
    return font;

  hb_font_make_immutable (parent);
  font->parent = hb_font_reference (parent);

  font->x_scale = parent->x_scale;
  font->y_scale = parent->y_scale;
  font->x_ppem = parent->x_ppem;
  font->y_ppem = parent->y_ppem;

  return font;
}

hb_font_t *
hb_font_get_empty (void)
{
  static const hb_font_t _hb_font_nil = {
    HB_OBJECT_HEADER_STATIC,

    true, /* immutable */

    NULL, /* parent */
    const_cast<hb_face_t *> (&_hb_face_nil),

    0, /* x_scale */
    0, /* y_scale */

    0, /* x_ppem */
    0, /* y_ppem */

    const_cast<hb_font_funcs_t *> (&_hb_font_funcs_nil), /* klass */
    NULL, /* user_data */
    NULL, /* destroy */

    {
#define HB_SHAPER_IMPLEMENT(shaper) HB_SHAPER_DATA_INVALID,
#include "hb-shaper-list.hh"
#undef HB_SHAPER_IMPLEMENT
    }
  };

  return const_cast<hb_font_t *> (&_hb_font_nil);
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

#define HB_SHAPER_IMPLEMENT(shaper) HB_SHAPER_DATA_DESTROY(shaper, font);
#include "hb-shaper-list.hh"
#undef HB_SHAPER_IMPLEMENT

  if (font->destroy)
    font->destroy (font->user_data);

  hb_font_destroy (font->parent);
  hb_face_destroy (font->face);
  hb_font_funcs_destroy (font->klass);

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
    klass = hb_font_funcs_get_empty ();

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
