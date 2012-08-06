/*
 * Copyright © 2009,2010  Red Hat, Inc.
 * Copyright © 2010,2011  Google, Inc.
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

#define HB_SHAPER ot
#define hb_ot_shaper_face_data_t hb_ot_layout_t
#define hb_ot_shaper_shape_plan_data_t hb_ot_shape_plan_t
#include "hb-shaper-impl-private.hh"

#include "hb-ot-shape-private.hh"
#include "hb-ot-shape-normalize-private.hh"
#include "hb-ot-shape-complex-private.hh"

#include "hb-ot-layout-private.hh"
#include "hb-set-private.hh"


hb_tag_t common_features[] = {
  HB_TAG('c','c','m','p'),
  HB_TAG('l','i','g','a'),
  HB_TAG('l','o','c','l'),
  HB_TAG('m','a','r','k'),
  HB_TAG('m','k','m','k'),
  HB_TAG('r','l','i','g'),
};


hb_tag_t horizontal_features[] = {
  HB_TAG('c','a','l','t'),
  HB_TAG('c','l','i','g'),
  HB_TAG('c','u','r','s'),
  HB_TAG('k','e','r','n'),
};

/* Note:
 * Technically speaking, vrt2 and vert are mutually exclusive.
 * According to the spec, valt and vpal are also mutually exclusive.
 * But we apply them all for now.
 */
hb_tag_t vertical_features[] = {
  HB_TAG('v','a','l','t'),
  HB_TAG('v','e','r','t'),
  HB_TAG('v','k','r','n'),
  HB_TAG('v','p','a','l'),
  HB_TAG('v','r','t','2'),
};



static void
hb_ot_shape_collect_features (hb_ot_shape_planner_t          *planner,
			      const hb_segment_properties_t  *props,
			      const hb_feature_t             *user_features,
			      unsigned int                    num_user_features)
{
  hb_ot_map_builder_t *map = &planner->map;

  switch (props->direction) {
    case HB_DIRECTION_LTR:
      map->add_bool_feature (HB_TAG ('l','t','r','a'));
      map->add_bool_feature (HB_TAG ('l','t','r','m'));
      break;
    case HB_DIRECTION_RTL:
      map->add_bool_feature (HB_TAG ('r','t','l','a'));
      map->add_bool_feature (HB_TAG ('r','t','l','m'), false);
      break;
    case HB_DIRECTION_TTB:
    case HB_DIRECTION_BTT:
    case HB_DIRECTION_INVALID:
    default:
      break;
  }

#define ADD_FEATURES(array) \
  HB_STMT_START { \
    for (unsigned int i = 0; i < ARRAY_LENGTH (array); i++) \
      map->add_bool_feature (array[i]); \
  } HB_STMT_END

  if (planner->shaper->collect_features)
    planner->shaper->collect_features (planner);

  ADD_FEATURES (common_features);

  if (HB_DIRECTION_IS_HORIZONTAL (props->direction))
    ADD_FEATURES (horizontal_features);
  else
    ADD_FEATURES (vertical_features);

  if (planner->shaper->override_features)
    planner->shaper->override_features (planner);

#undef ADD_FEATURES

  for (unsigned int i = 0; i < num_user_features; i++) {
    const hb_feature_t *feature = &user_features[i];
    map->add_feature (feature->tag, feature->value, (feature->start == 0 && feature->end == (unsigned int) -1));
  }
}


/*
 * shaper face data
 */

hb_ot_shaper_face_data_t *
_hb_ot_shaper_face_data_create (hb_face_t *face)
{
  return _hb_ot_layout_create (face);
}

void
_hb_ot_shaper_face_data_destroy (hb_ot_shaper_face_data_t *data)
{
  _hb_ot_layout_destroy (data);
}


/*
 * shaper font data
 */

struct hb_ot_shaper_font_data_t {};

hb_ot_shaper_font_data_t *
_hb_ot_shaper_font_data_create (hb_font_t *font)
{
  return (hb_ot_shaper_font_data_t *) HB_SHAPER_DATA_SUCCEEDED;
}

void
_hb_ot_shaper_font_data_destroy (hb_ot_shaper_font_data_t *data)
{
}


/*
 * shaper shape_plan data
 */

hb_ot_shaper_shape_plan_data_t *
_hb_ot_shaper_shape_plan_data_create (hb_shape_plan_t    *shape_plan,
				      const hb_feature_t *user_features,
				      unsigned int        num_user_features)
{
  hb_ot_shape_plan_t *plan = (hb_ot_shape_plan_t *) calloc (1, sizeof (hb_ot_shape_plan_t));
  if (unlikely (!plan))
    return NULL;

  hb_ot_shape_planner_t planner (shape_plan);

  planner.shaper = hb_ot_shape_complex_categorize (&shape_plan->props);

  hb_ot_shape_collect_features (&planner, &shape_plan->props, user_features, num_user_features);

  planner.compile (*plan);

  if (plan->shaper->data_create) {
    plan->data = plan->shaper->data_create (plan);
    if (unlikely (!plan->data))
      return NULL;
  }

  return plan;
}

void
_hb_ot_shaper_shape_plan_data_destroy (hb_ot_shaper_shape_plan_data_t *plan)
{
  if (plan->shaper->data_destroy)
    plan->shaper->data_destroy (const_cast<void *> (plan->data));

  plan->finish ();

  free (plan);
}


/*
 * shaper
 */

struct hb_ot_shape_context_t
{
  /* Input to hb_ot_shape_internal() */
  hb_ot_shape_plan_t *plan;
  hb_font_t *font;
  hb_face_t *face;
  hb_buffer_t  *buffer;
  const hb_feature_t *user_features;
  unsigned int        num_user_features;

  /* Transient stuff */
  hb_direction_t target_direction;
  hb_bool_t applied_position_complex;
};

static void
hb_ot_shape_setup_masks (hb_ot_shape_context_t *c)
{
  hb_ot_map_t *map = &c->plan->map;

  hb_mask_t global_mask = map->get_global_mask ();
  c->buffer->reset_masks (global_mask);

  if (c->plan->shaper->setup_masks)
    c->plan->shaper->setup_masks (c->plan, c->buffer, c->font);

  for (unsigned int i = 0; i < c->num_user_features; i++)
  {
    const hb_feature_t *feature = &c->user_features[i];
    if (!(feature->start == 0 && feature->end == (unsigned int)-1)) {
      unsigned int shift;
      hb_mask_t mask = map->get_mask (feature->tag, &shift);
      c->buffer->set_masks (feature->value << shift, mask, feature->start, feature->end);
    }
  }
}


/* Main shaper */

/* Prepare */

static void
hb_set_unicode_props (hb_buffer_t *buffer)
{
  unsigned int count = buffer->len;
  for (unsigned int i = 0; i < count; i++)
    _hb_glyph_info_set_unicode_props (&buffer->info[i], buffer->unicode);
}

static void
hb_form_clusters (hb_buffer_t *buffer)
{
  unsigned int count = buffer->len;
  for (unsigned int i = 1; i < count; i++)
    if (FLAG (_hb_glyph_info_get_general_category (&buffer->info[i])) &
	(FLAG (HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK) |
	 FLAG (HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK) |
	 FLAG (HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK)))
      buffer->merge_clusters (i - 1, i + 1);
}

static void
hb_ensure_native_direction (hb_buffer_t *buffer)
{
  hb_direction_t direction = buffer->props.direction;

  /* TODO vertical:
   * The only BTT vertical script is Ogham, but it's not clear to me whether OpenType
   * Ogham fonts are supposed to be implemented BTT or not.  Need to research that
   * first. */
  if ((HB_DIRECTION_IS_HORIZONTAL (direction) && direction != hb_script_get_horizontal_direction (buffer->props.script)) ||
      (HB_DIRECTION_IS_VERTICAL   (direction) && direction != HB_DIRECTION_TTB))
  {
    hb_buffer_reverse_clusters (buffer);
    buffer->props.direction = HB_DIRECTION_REVERSE (buffer->props.direction);
  }
}


/* Substitute */

static void
hb_mirror_chars (hb_ot_shape_context_t *c)
{
  hb_unicode_funcs_t *unicode = c->buffer->unicode;

  if (HB_DIRECTION_IS_FORWARD (c->target_direction))
    return;

  hb_mask_t rtlm_mask = c->plan->map.get_1_mask (HB_TAG ('r','t','l','m'));

  unsigned int count = c->buffer->len;
  for (unsigned int i = 0; i < count; i++) {
    hb_codepoint_t codepoint = unicode->mirroring (c->buffer->info[i].codepoint);
    if (likely (codepoint == c->buffer->info[i].codepoint))
      c->buffer->info[i].mask |= rtlm_mask; /* XXX this should be moved to before setting user-feature masks */
    else
      c->buffer->info[i].codepoint = codepoint;
  }
}

static void
hb_map_glyphs (hb_font_t    *font,
	       hb_buffer_t  *buffer)
{
  hb_codepoint_t glyph;

  if (unlikely (!buffer->len))
    return;

  buffer->clear_output ();

  unsigned int count = buffer->len - 1;
  for (buffer->idx = 0; buffer->idx < count;) {
    if (unlikely (buffer->unicode->is_variation_selector (buffer->cur(+1).codepoint))) {
      font->get_glyph (buffer->cur().codepoint, buffer->cur(+1).codepoint, &glyph);
      buffer->replace_glyphs (2, 1, &glyph);
    } else {
      font->get_glyph (buffer->cur().codepoint, 0, &glyph);
      buffer->replace_glyph (glyph);
    }
  }
  if (likely (buffer->idx < buffer->len)) {
    font->get_glyph (buffer->cur().codepoint, 0, &glyph);
    buffer->replace_glyph (glyph);
  }
  buffer->swap_buffers ();
}

static void
hb_substitute_default (hb_ot_shape_context_t *c)
{
  hb_mirror_chars (c);

  hb_map_glyphs (c->font, c->buffer);
}

static void
hb_synthesize_glyph_classes (hb_ot_shape_context_t *c)
{
  unsigned int count = c->buffer->len;
  for (unsigned int i = 0; i < count; i++)
    c->buffer->info[i].glyph_props() = _hb_glyph_info_get_general_category (&c->buffer->info[i]) == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK ?
				       HB_OT_LAYOUT_GLYPH_CLASS_MARK :
				       HB_OT_LAYOUT_GLYPH_CLASS_BASE_GLYPH;
}


static void
hb_ot_substitute_complex (hb_ot_shape_context_t *c)
{
  hb_ot_layout_substitute_start (c->font, c->buffer);

  if (!hb_ot_layout_has_glyph_classes (c->face))
    hb_synthesize_glyph_classes (c);

  if (hb_ot_layout_has_substitution (c->face))
    c->plan->substitute (c->font, c->buffer);

  hb_ot_layout_substitute_finish (c->font, c->buffer);

  return;
}


/* Position */

static void
hb_position_default (hb_ot_shape_context_t *c)
{
  hb_ot_layout_position_start (c->font, c->buffer);

  unsigned int count = c->buffer->len;
  for (unsigned int i = 0; i < count; i++) {
    c->font->get_glyph_advance_for_direction (c->buffer->info[i].codepoint,
					      c->buffer->props.direction,
					      &c->buffer->pos[i].x_advance,
					      &c->buffer->pos[i].y_advance);
    c->font->subtract_glyph_origin_for_direction (c->buffer->info[i].codepoint,
						  c->buffer->props.direction,
						  &c->buffer->pos[i].x_offset,
						  &c->buffer->pos[i].y_offset);
  }
}

static void
hb_zero_mark_advances (hb_ot_shape_context_t *c)
{
  unsigned int count = c->buffer->len;
  for (unsigned int i = 0; i < count; i++)
    if (_hb_glyph_info_get_general_category (&c->buffer->info[i]) == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK)
    {
      c->buffer->pos[i].x_advance = 0;
      c->buffer->pos[i].y_advance = 0;
    }
}

static void
hb_ot_position_complex (hb_ot_shape_context_t *c)
{

  if (hb_ot_layout_has_positioning (c->face))
  {
    /* Change glyph origin to what GPOS expects, apply GPOS, change it back. */

    unsigned int count = c->buffer->len;
    for (unsigned int i = 0; i < count; i++) {
      c->font->add_glyph_origin_for_direction (c->buffer->info[i].codepoint,
					       HB_DIRECTION_LTR,
					       &c->buffer->pos[i].x_offset,
					       &c->buffer->pos[i].y_offset);
    }

    c->plan->position (c->font, c->buffer);

    for (unsigned int i = 0; i < count; i++) {
      c->font->subtract_glyph_origin_for_direction (c->buffer->info[i].codepoint,
						    HB_DIRECTION_LTR,
						    &c->buffer->pos[i].x_offset,
						    &c->buffer->pos[i].y_offset);
    }

    c->applied_position_complex = true;
  } else
    hb_zero_mark_advances (c);

  hb_ot_layout_position_finish (c->font, c->buffer, c->plan->shaper->zero_width_attached_marks);

  return;
}

static void
hb_position_complex_fallback (hb_ot_shape_context_t *c HB_UNUSED)
{
  /* TODO Mark pos */
}

static void
hb_truetype_kern (hb_ot_shape_context_t *c)
{
  /* TODO Check for kern=0 */
  unsigned int count = c->buffer->len;
  for (unsigned int i = 1; i < count; i++) {
    hb_position_t x_kern, y_kern, kern1, kern2;
    c->font->get_glyph_kerning_for_direction (c->buffer->info[i - 1].codepoint, c->buffer->info[i].codepoint,
					      c->buffer->props.direction,
					      &x_kern, &y_kern);

    kern1 = x_kern >> 1;
    kern2 = x_kern - kern1;
    c->buffer->pos[i - 1].x_advance += kern1;
    c->buffer->pos[i].x_advance += kern2;
    c->buffer->pos[i].x_offset += kern2;

    kern1 = y_kern >> 1;
    kern2 = y_kern - kern1;
    c->buffer->pos[i - 1].y_advance += kern1;
    c->buffer->pos[i].y_advance += kern2;
    c->buffer->pos[i].y_offset += kern2;
  }
}

static void
hb_position_complex_fallback_visual (hb_ot_shape_context_t *c)
{
  hb_truetype_kern (c);
}

static void
hb_hide_zerowidth (hb_ot_shape_context_t *c)
{
  hb_codepoint_t space;
  if (!c->font->get_glyph (' ', 0, &space))
    return; /* No point! */

  unsigned int count = c->buffer->len;
  for (unsigned int i = 0; i < count; i++)
    if (unlikely (!is_a_ligature (c->buffer->info[i]) &&
		  _hb_glyph_info_is_zero_width (&c->buffer->info[i]))) {
      c->buffer->info[i].codepoint = space;
      c->buffer->pos[i].x_advance = 0;
      c->buffer->pos[i].y_advance = 0;
    }
}


/* Do it! */

static void
hb_ot_shape_internal (hb_ot_shape_context_t *c)
{
  c->buffer->deallocate_var_all ();

  /* Save the original direction, we use it later. */
  c->target_direction = c->buffer->props.direction;

  HB_BUFFER_ALLOCATE_VAR (c->buffer, unicode_props0);
  HB_BUFFER_ALLOCATE_VAR (c->buffer, unicode_props1);

  c->buffer->clear_output ();

  hb_set_unicode_props (c->buffer);

  hb_form_clusters (c->buffer);

  hb_ensure_native_direction (c->buffer);

  _hb_ot_shape_normalize (c->font, c->buffer,
			  c->plan->shaper->normalization_preference ?
			  c->plan->shaper->normalization_preference (c->plan) :
			  HB_OT_SHAPE_NORMALIZATION_MODE_DEFAULT);

  hb_ot_shape_setup_masks (c);

  /* SUBSTITUTE */
  {
    hb_substitute_default (c);

    hb_ot_substitute_complex (c);
  }

  /* POSITION */
  {
    hb_position_default (c);

    hb_ot_position_complex (c);

    hb_bool_t position_fallback = !c->applied_position_complex;
    if (position_fallback)
      hb_position_complex_fallback (c);

    if (HB_DIRECTION_IS_BACKWARD (c->buffer->props.direction))
      hb_buffer_reverse (c->buffer);

    if (position_fallback)
      hb_position_complex_fallback_visual (c);
  }

  hb_hide_zerowidth (c);

  HB_BUFFER_DEALLOCATE_VAR (c->buffer, unicode_props1);
  HB_BUFFER_DEALLOCATE_VAR (c->buffer, unicode_props0);

  c->buffer->props.direction = c->target_direction;

  c->buffer->deallocate_var_all ();
}


hb_bool_t
_hb_ot_shape (hb_shape_plan_t    *shape_plan,
	      hb_font_t          *font,
	      hb_buffer_t        *buffer,
	      const hb_feature_t *features,
	      unsigned int        num_features)
{
  hb_ot_shape_context_t c = {HB_SHAPER_DATA_GET (shape_plan), font, font->face, buffer, features, num_features};
  hb_ot_shape_internal (&c);

  return true;
}


void
hb_ot_shape_glyphs_closure (hb_font_t          *font,
			    hb_buffer_t        *buffer,
			    const hb_feature_t *features,
			    unsigned int        num_features,
			    hb_set_t           *glyphs)
{
  hb_ot_shape_plan_t plan;

  buffer->guess_properties ();

  /* TODO cache / ensure correct backend, etc. */
  hb_shape_plan_t *shape_plan = hb_shape_plan_create (font->face, &buffer->props, features, num_features, NULL);

  /* TODO: normalization? have shapers do closure()? */
  /* TODO: Deal with mirrored chars? */
  hb_map_glyphs (font, buffer);

  /* Seed it.  It's user's responsibility to have cleard glyphs
   * if that's what they desire. */
  unsigned int count = buffer->len;
  for (unsigned int i = 0; i < count; i++)
    hb_set_add (glyphs, buffer->info[i].codepoint);

  /* And find transitive closure. */
  hb_set_t copy;
  copy.init ();

  do {
    copy.set (glyphs);
    HB_SHAPER_DATA_GET (shape_plan)->substitute_closure (font->face, glyphs);
  } while (!copy.equal (glyphs));

  hb_shape_plan_destroy (shape_plan);
}
