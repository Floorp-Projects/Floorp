/*
 * Copyright © 1998-2004  David Turner and Werner Lemberg
 * Copyright © 2006  Behdad Esfahbod
 * Copyright © 2007,2008,2009  Red Hat, Inc.
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

#include "hb-ot-layout-private.hh"

#include "hb-ot-layout-gdef-table.hh"
#include "hb-ot-layout-gsub-table.hh"
#include "hb-ot-layout-gpos-table.hh"
#include "hb-ot-maxp-table.hh"


#include <stdlib.h>
#include <string.h>


HB_SHAPER_DATA_ENSURE_DECLARE(ot, face)

hb_ot_layout_t *
_hb_ot_layout_create (hb_face_t *face)
{
  hb_ot_layout_t *layout = (hb_ot_layout_t *) calloc (1, sizeof (hb_ot_layout_t));
  if (unlikely (!layout))
    return NULL;

  layout->gdef_blob = Sanitizer<GDEF>::sanitize (hb_face_reference_table (face, HB_OT_TAG_GDEF));
  layout->gdef = Sanitizer<GDEF>::lock_instance (layout->gdef_blob);

  layout->gsub_blob = Sanitizer<GSUB>::sanitize (hb_face_reference_table (face, HB_OT_TAG_GSUB));
  layout->gsub = Sanitizer<GSUB>::lock_instance (layout->gsub_blob);

  layout->gpos_blob = Sanitizer<GPOS>::sanitize (hb_face_reference_table (face, HB_OT_TAG_GPOS));
  layout->gpos = Sanitizer<GPOS>::lock_instance (layout->gpos_blob);

  layout->gsub_lookup_count = layout->gsub->get_lookup_count ();
  layout->gpos_lookup_count = layout->gpos->get_lookup_count ();

  layout->gsub_digests = (hb_set_digest_t *) calloc (layout->gsub->get_lookup_count (), sizeof (hb_set_digest_t));
  layout->gpos_digests = (hb_set_digest_t *) calloc (layout->gpos->get_lookup_count (), sizeof (hb_set_digest_t));

  if (unlikely ((layout->gsub_lookup_count && !layout->gsub_digests) ||
		(layout->gpos_lookup_count && !layout->gpos_digests)))
  {
    _hb_ot_layout_destroy (layout);
    return NULL;
  }

  for (unsigned int i = 0; i < layout->gsub_lookup_count; i++)
    layout->gsub->add_coverage (&layout->gsub_digests[i], i);
  for (unsigned int i = 0; i < layout->gpos_lookup_count; i++)
    layout->gpos->add_coverage (&layout->gpos_digests[i], i);

  return layout;
}

void
_hb_ot_layout_destroy (hb_ot_layout_t *layout)
{
  hb_blob_destroy (layout->gdef_blob);
  hb_blob_destroy (layout->gsub_blob);
  hb_blob_destroy (layout->gpos_blob);

  free (layout->gsub_digests);
  free (layout->gpos_digests);

  free (layout);
}

static inline const GDEF&
_get_gdef (hb_face_t *face)
{
  if (unlikely (!hb_ot_shaper_face_data_ensure (face))) return Null(GDEF);
  return *hb_ot_layout_from_face (face)->gdef;
}
static inline const GSUB&
_get_gsub (hb_face_t *face)
{
  if (unlikely (!hb_ot_shaper_face_data_ensure (face))) return Null(GSUB);
  return *hb_ot_layout_from_face (face)->gsub;
}
static inline const GPOS&
_get_gpos (hb_face_t *face)
{
  if (unlikely (!hb_ot_shaper_face_data_ensure (face))) return Null(GPOS);
  return *hb_ot_layout_from_face (face)->gpos;
}


/*
 * GDEF
 */

hb_bool_t
hb_ot_layout_has_glyph_classes (hb_face_t *face)
{
  return _get_gdef (face).has_glyph_classes ();
}


unsigned int
hb_ot_layout_get_attach_points (hb_face_t      *face,
				hb_codepoint_t  glyph,
				unsigned int    start_offset,
				unsigned int   *point_count /* IN/OUT */,
				unsigned int   *point_array /* OUT */)
{
  return _get_gdef (face).get_attach_points (glyph, start_offset, point_count, point_array);
}

unsigned int
hb_ot_layout_get_ligature_carets (hb_font_t      *font,
				  hb_direction_t  direction,
				  hb_codepoint_t  glyph,
				  unsigned int    start_offset,
				  unsigned int   *caret_count /* IN/OUT */,
				  int            *caret_array /* OUT */)
{
  return _get_gdef (font->face).get_lig_carets (font, direction, glyph, start_offset, caret_count, caret_array);
}


/*
 * GSUB/GPOS
 */

static const GSUBGPOS&
get_gsubgpos_table (hb_face_t *face,
		    hb_tag_t   table_tag)
{
  switch (table_tag) {
    case HB_OT_TAG_GSUB: return _get_gsub (face);
    case HB_OT_TAG_GPOS: return _get_gpos (face);
    default:             return Null(GSUBGPOS);
  }
}


unsigned int
hb_ot_layout_table_get_script_tags (hb_face_t    *face,
				    hb_tag_t      table_tag,
				    unsigned int  start_offset,
				    unsigned int *script_count /* IN/OUT */,
				    hb_tag_t     *script_tags /* OUT */)
{
  const GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

  return g.get_script_tags (start_offset, script_count, script_tags);
}

hb_bool_t
hb_ot_layout_table_find_script (hb_face_t    *face,
				hb_tag_t      table_tag,
				hb_tag_t      script_tag,
				unsigned int *script_index)
{
  ASSERT_STATIC (Index::NOT_FOUND_INDEX == HB_OT_LAYOUT_NO_SCRIPT_INDEX);
  const GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

  if (g.find_script_index (script_tag, script_index))
    return true;

  /* try finding 'DFLT' */
  if (g.find_script_index (HB_OT_TAG_DEFAULT_SCRIPT, script_index))
    return false;

  /* try with 'dflt'; MS site has had typos and many fonts use it now :(.
   * including many versions of DejaVu Sans Mono! */
  if (g.find_script_index (HB_OT_TAG_DEFAULT_LANGUAGE, script_index))
    return false;

  if (script_index) *script_index = HB_OT_LAYOUT_NO_SCRIPT_INDEX;
  return false;
}

hb_bool_t
hb_ot_layout_table_choose_script (hb_face_t      *face,
				  hb_tag_t        table_tag,
				  const hb_tag_t *script_tags,
				  unsigned int   *script_index,
				  hb_tag_t       *chosen_script)
{
  ASSERT_STATIC (Index::NOT_FOUND_INDEX == HB_OT_LAYOUT_NO_SCRIPT_INDEX);
  const GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

  while (*script_tags)
  {
    if (g.find_script_index (*script_tags, script_index)) {
      if (chosen_script)
        *chosen_script = *script_tags;
      return true;
    }
    script_tags++;
  }

  /* try finding 'DFLT' */
  if (g.find_script_index (HB_OT_TAG_DEFAULT_SCRIPT, script_index)) {
    if (chosen_script)
      *chosen_script = HB_OT_TAG_DEFAULT_SCRIPT;
    return false;
  }

  /* try with 'dflt'; MS site has had typos and many fonts use it now :( */
  if (g.find_script_index (HB_OT_TAG_DEFAULT_LANGUAGE, script_index)) {
    if (chosen_script)
      *chosen_script = HB_OT_TAG_DEFAULT_LANGUAGE;
    return false;
  }

  /* try with 'latn'; some old fonts put their features there even though
     they're really trying to support Thai, for example :( */
#define HB_OT_TAG_LATIN_SCRIPT		HB_TAG ('l', 'a', 't', 'n')
  if (g.find_script_index (HB_OT_TAG_LATIN_SCRIPT, script_index)) {
    if (chosen_script)
      *chosen_script = HB_OT_TAG_LATIN_SCRIPT;
    return false;
  }

  if (script_index) *script_index = HB_OT_LAYOUT_NO_SCRIPT_INDEX;
  if (chosen_script)
    *chosen_script = HB_OT_LAYOUT_NO_SCRIPT_INDEX;
  return false;
}

unsigned int
hb_ot_layout_table_get_feature_tags (hb_face_t    *face,
				     hb_tag_t      table_tag,
				     unsigned int  start_offset,
				     unsigned int *feature_count /* IN/OUT */,
				     hb_tag_t     *feature_tags /* OUT */)
{
  const GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

  return g.get_feature_tags (start_offset, feature_count, feature_tags);
}


unsigned int
hb_ot_layout_script_get_language_tags (hb_face_t    *face,
				       hb_tag_t      table_tag,
				       unsigned int  script_index,
				       unsigned int  start_offset,
				       unsigned int *language_count /* IN/OUT */,
				       hb_tag_t     *language_tags /* OUT */)
{
  const Script &s = get_gsubgpos_table (face, table_tag).get_script (script_index);

  return s.get_lang_sys_tags (start_offset, language_count, language_tags);
}

hb_bool_t
hb_ot_layout_script_find_language (hb_face_t    *face,
				   hb_tag_t      table_tag,
				   unsigned int  script_index,
				   hb_tag_t      language_tag,
				   unsigned int *language_index)
{
  ASSERT_STATIC (Index::NOT_FOUND_INDEX == HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX);
  const Script &s = get_gsubgpos_table (face, table_tag).get_script (script_index);

  if (s.find_lang_sys_index (language_tag, language_index))
    return true;

  /* try with 'dflt'; MS site has had typos and many fonts use it now :( */
  if (s.find_lang_sys_index (HB_OT_TAG_DEFAULT_LANGUAGE, language_index))
    return false;

  if (language_index) *language_index = HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX;
  return false;
}

hb_bool_t
hb_ot_layout_language_get_required_feature_index (hb_face_t    *face,
						  hb_tag_t      table_tag,
						  unsigned int  script_index,
						  unsigned int  language_index,
						  unsigned int *feature_index)
{
  const LangSys &l = get_gsubgpos_table (face, table_tag).get_script (script_index).get_lang_sys (language_index);

  if (feature_index) *feature_index = l.get_required_feature_index ();

  return l.has_required_feature ();
}

unsigned int
hb_ot_layout_language_get_feature_indexes (hb_face_t    *face,
					   hb_tag_t      table_tag,
					   unsigned int  script_index,
					   unsigned int  language_index,
					   unsigned int  start_offset,
					   unsigned int *feature_count /* IN/OUT */,
					   unsigned int *feature_indexes /* OUT */)
{
  const GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  const LangSys &l = g.get_script (script_index).get_lang_sys (language_index);

  return l.get_feature_indexes (start_offset, feature_count, feature_indexes);
}

unsigned int
hb_ot_layout_language_get_feature_tags (hb_face_t    *face,
					hb_tag_t      table_tag,
					unsigned int  script_index,
					unsigned int  language_index,
					unsigned int  start_offset,
					unsigned int *feature_count /* IN/OUT */,
					hb_tag_t     *feature_tags /* OUT */)
{
  const GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  const LangSys &l = g.get_script (script_index).get_lang_sys (language_index);

  ASSERT_STATIC (sizeof (unsigned int) == sizeof (hb_tag_t));
  unsigned int ret = l.get_feature_indexes (start_offset, feature_count, (unsigned int *) feature_tags);

  if (feature_tags) {
    unsigned int count = *feature_count;
    for (unsigned int i = 0; i < count; i++)
      feature_tags[i] = g.get_feature_tag ((unsigned int) feature_tags[i]);
  }

  return ret;
}


hb_bool_t
hb_ot_layout_language_find_feature (hb_face_t    *face,
				    hb_tag_t      table_tag,
				    unsigned int  script_index,
				    unsigned int  language_index,
				    hb_tag_t      feature_tag,
				    unsigned int *feature_index)
{
  ASSERT_STATIC (Index::NOT_FOUND_INDEX == HB_OT_LAYOUT_NO_FEATURE_INDEX);
  const GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  const LangSys &l = g.get_script (script_index).get_lang_sys (language_index);

  unsigned int num_features = l.get_feature_count ();
  for (unsigned int i = 0; i < num_features; i++) {
    unsigned int f_index = l.get_feature_index (i);

    if (feature_tag == g.get_feature_tag (f_index)) {
      if (feature_index) *feature_index = f_index;
      return true;
    }
  }

  if (feature_index) *feature_index = HB_OT_LAYOUT_NO_FEATURE_INDEX;
  return false;
}

unsigned int
hb_ot_layout_feature_get_lookup_indexes (hb_face_t    *face,
					 hb_tag_t      table_tag,
					 unsigned int  feature_index,
					 unsigned int  start_offset,
					 unsigned int *lookup_count /* IN/OUT */,
					 unsigned int *lookup_indexes /* OUT */)
{
  const GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  const Feature &f = g.get_feature (feature_index);

  return f.get_lookup_indexes (start_offset, lookup_count, lookup_indexes);
}


/*
 * GSUB
 */

hb_bool_t
hb_ot_layout_has_substitution (hb_face_t *face)
{
  return &_get_gsub (face) != &Null(GSUB);
}

hb_bool_t
hb_ot_layout_would_substitute_lookup (hb_face_t            *face,
				      const hb_codepoint_t *glyphs,
				      unsigned int          glyphs_length,
				      unsigned int          lookup_index)
{
  if (unlikely (!hb_ot_shaper_face_data_ensure (face))) return false;
  return hb_ot_layout_would_substitute_lookup_fast (face, glyphs, glyphs_length, lookup_index);
}

hb_bool_t
hb_ot_layout_would_substitute_lookup_fast (hb_face_t            *face,
					   const hb_codepoint_t *glyphs,
					   unsigned int          glyphs_length,
					   unsigned int          lookup_index)
{
  if (unlikely (glyphs_length < 1 || glyphs_length > 2)) return false;
  if (unlikely (lookup_index >= hb_ot_layout_from_face (face)->gsub_lookup_count)) return false;
  hb_would_apply_context_t c (face, glyphs[0], glyphs_length == 2 ? glyphs[1] : -1, &hb_ot_layout_from_face (face)->gsub_digests[lookup_index]);
  return hb_ot_layout_from_face (face)->gsub->would_substitute_lookup (&c, lookup_index);
}

void
hb_ot_layout_substitute_start (hb_font_t *font, hb_buffer_t *buffer)
{
  GSUB::substitute_start (font, buffer);
}

hb_bool_t
hb_ot_layout_substitute_lookup (hb_font_t    *font,
				hb_buffer_t  *buffer,
				unsigned int  lookup_index,
				hb_mask_t     mask)
{
  if (unlikely (lookup_index >= hb_ot_layout_from_face (font->face)->gsub_lookup_count)) return false;
  hb_apply_context_t c (font, buffer, mask, &hb_ot_layout_from_face (font->face)->gsub_digests[lookup_index]);
  return hb_ot_layout_from_face (font->face)->gsub->substitute_lookup (&c, lookup_index);
}

void
hb_ot_layout_substitute_finish (hb_font_t *font, hb_buffer_t *buffer)
{
  GSUB::substitute_finish (font, buffer);
}

void
hb_ot_layout_substitute_closure_lookup (hb_face_t    *face,
				        hb_set_t     *glyphs,
				        unsigned int  lookup_index)
{
  hb_closure_context_t c (face, glyphs);
  _get_gsub (face).closure_lookup (&c, lookup_index);
}

/*
 * GPOS
 */

hb_bool_t
hb_ot_layout_has_positioning (hb_face_t *face)
{
  return &_get_gpos (face) != &Null(GPOS);
}

void
hb_ot_layout_position_start (hb_font_t *font, hb_buffer_t *buffer)
{
  GPOS::position_start (font, buffer);
}

hb_bool_t
hb_ot_layout_position_lookup (hb_font_t    *font,
			      hb_buffer_t  *buffer,
			      unsigned int  lookup_index,
			      hb_mask_t     mask)
{
  if (unlikely (lookup_index >= hb_ot_layout_from_face (font->face)->gpos_lookup_count)) return false;
  hb_apply_context_t c (font, buffer, mask, &hb_ot_layout_from_face (font->face)->gpos_digests[lookup_index]);
  return hb_ot_layout_from_face (font->face)->gpos->position_lookup (&c, lookup_index);
}

void
hb_ot_layout_position_finish (hb_font_t *font, hb_buffer_t *buffer, hb_bool_t zero_width_attached_marks)
{
  GPOS::position_finish (font, buffer, zero_width_attached_marks);
}
