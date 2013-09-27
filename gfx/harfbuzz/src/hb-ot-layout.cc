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

#include <stdlib.h>
#include <string.h>


HB_SHAPER_DATA_ENSURE_DECLARE(ot, face)

hb_ot_layout_t *
_hb_ot_layout_create (hb_face_t *face)
{
  hb_ot_layout_t *layout = (hb_ot_layout_t *) calloc (1, sizeof (hb_ot_layout_t));
  if (unlikely (!layout))
    return NULL;

  layout->gdef_blob = OT::Sanitizer<OT::GDEF>::sanitize (face->reference_table (HB_OT_TAG_GDEF));
  layout->gdef = OT::Sanitizer<OT::GDEF>::lock_instance (layout->gdef_blob);

  layout->gsub_blob = OT::Sanitizer<OT::GSUB>::sanitize (face->reference_table (HB_OT_TAG_GSUB));
  layout->gsub = OT::Sanitizer<OT::GSUB>::lock_instance (layout->gsub_blob);

  layout->gpos_blob = OT::Sanitizer<OT::GPOS>::sanitize (face->reference_table (HB_OT_TAG_GPOS));
  layout->gpos = OT::Sanitizer<OT::GPOS>::lock_instance (layout->gpos_blob);

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
  {
    layout->gsub_digests[i].init ();
    layout->gsub->get_lookup (i).add_coverage (&layout->gsub_digests[i]);
  }
  for (unsigned int i = 0; i < layout->gpos_lookup_count; i++)
  {
    layout->gpos_digests[i].init ();
    layout->gpos->get_lookup (i).add_coverage (&layout->gpos_digests[i]);
  }

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

static inline const OT::GDEF&
_get_gdef (hb_face_t *face)
{
  if (unlikely (!hb_ot_shaper_face_data_ensure (face))) return OT::Null(OT::GDEF);
  return *hb_ot_layout_from_face (face)->gdef;
}
static inline const OT::GSUB&
_get_gsub (hb_face_t *face)
{
  if (unlikely (!hb_ot_shaper_face_data_ensure (face))) return OT::Null(OT::GSUB);
  return *hb_ot_layout_from_face (face)->gsub;
}
static inline const OT::GPOS&
_get_gpos (hb_face_t *face)
{
  if (unlikely (!hb_ot_shaper_face_data_ensure (face))) return OT::Null(OT::GPOS);
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

hb_ot_layout_glyph_class_t
hb_ot_layout_get_glyph_class (hb_face_t      *face,
			      hb_codepoint_t  glyph)
{
  return (hb_ot_layout_glyph_class_t) _get_gdef (face).get_glyph_class (glyph);
}

void
hb_ot_layout_get_glyphs_in_class (hb_face_t                  *face,
				  hb_ot_layout_glyph_class_t  klass,
				  hb_set_t                   *glyphs /* OUT */)
{
  return _get_gdef (face).get_glyphs_in_class (klass, glyphs);
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

static const OT::GSUBGPOS&
get_gsubgpos_table (hb_face_t *face,
		    hb_tag_t   table_tag)
{
  switch (table_tag) {
    case HB_OT_TAG_GSUB: return _get_gsub (face);
    case HB_OT_TAG_GPOS: return _get_gpos (face);
    default:             return OT::Null(OT::GSUBGPOS);
  }
}


unsigned int
hb_ot_layout_table_get_script_tags (hb_face_t    *face,
				    hb_tag_t      table_tag,
				    unsigned int  start_offset,
				    unsigned int *script_count /* IN/OUT */,
				    hb_tag_t     *script_tags /* OUT */)
{
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

  return g.get_script_tags (start_offset, script_count, script_tags);
}

#define HB_OT_TAG_LATIN_SCRIPT		HB_TAG ('l', 'a', 't', 'n')

hb_bool_t
hb_ot_layout_table_find_script (hb_face_t    *face,
				hb_tag_t      table_tag,
				hb_tag_t      script_tag,
				unsigned int *script_index)
{
  ASSERT_STATIC (OT::Index::NOT_FOUND_INDEX == HB_OT_LAYOUT_NO_SCRIPT_INDEX);
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

  if (g.find_script_index (script_tag, script_index))
    return true;

  /* try finding 'DFLT' */
  if (g.find_script_index (HB_OT_TAG_DEFAULT_SCRIPT, script_index))
    return false;

  /* try with 'dflt'; MS site has had typos and many fonts use it now :(.
   * including many versions of DejaVu Sans Mono! */
  if (g.find_script_index (HB_OT_TAG_DEFAULT_LANGUAGE, script_index))
    return false;

  /* try with 'latn'; some old fonts put their features there even though
     they're really trying to support Thai, for example :( */
  if (g.find_script_index (HB_OT_TAG_LATIN_SCRIPT, script_index))
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
  ASSERT_STATIC (OT::Index::NOT_FOUND_INDEX == HB_OT_LAYOUT_NO_SCRIPT_INDEX);
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

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
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

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
  const OT::Script &s = get_gsubgpos_table (face, table_tag).get_script (script_index);

  return s.get_lang_sys_tags (start_offset, language_count, language_tags);
}

hb_bool_t
hb_ot_layout_script_find_language (hb_face_t    *face,
				   hb_tag_t      table_tag,
				   unsigned int  script_index,
				   hb_tag_t      language_tag,
				   unsigned int *language_index)
{
  ASSERT_STATIC (OT::Index::NOT_FOUND_INDEX == HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX);
  const OT::Script &s = get_gsubgpos_table (face, table_tag).get_script (script_index);

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
  const OT::LangSys &l = get_gsubgpos_table (face, table_tag).get_script (script_index).get_lang_sys (language_index);

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
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  const OT::LangSys &l = g.get_script (script_index).get_lang_sys (language_index);

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
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  const OT::LangSys &l = g.get_script (script_index).get_lang_sys (language_index);

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
  ASSERT_STATIC (OT::Index::NOT_FOUND_INDEX == HB_OT_LAYOUT_NO_FEATURE_INDEX);
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  const OT::LangSys &l = g.get_script (script_index).get_lang_sys (language_index);

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
hb_ot_layout_feature_get_lookups (hb_face_t    *face,
				  hb_tag_t      table_tag,
				  unsigned int  feature_index,
				  unsigned int  start_offset,
				  unsigned int *lookup_count /* IN/OUT */,
				  unsigned int *lookup_indexes /* OUT */)
{
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  const OT::Feature &f = g.get_feature (feature_index);

  return f.get_lookup_indexes (start_offset, lookup_count, lookup_indexes);
}

static void
_hb_ot_layout_collect_lookups_lookups (hb_face_t      *face,
				       hb_tag_t        table_tag,
				       unsigned int    feature_index,
				       hb_set_t       *lookup_indexes /* OUT */)
{
  unsigned int lookup_indices[32];
  unsigned int offset, len;

  offset = 0;
  do {
    len = ARRAY_LENGTH (lookup_indices);
    hb_ot_layout_feature_get_lookups (face,
				      table_tag,
				      feature_index,
				      offset, &len,
				      lookup_indices);

    for (unsigned int i = 0; i < len; i++)
      lookup_indexes->add (lookup_indices[i]);

    offset += len;
  } while (len == ARRAY_LENGTH (lookup_indices));
}

static void
_hb_ot_layout_collect_lookups_features (hb_face_t      *face,
					hb_tag_t        table_tag,
					unsigned int    script_index,
					unsigned int    language_index,
					const hb_tag_t *features,
					hb_set_t       *lookup_indexes /* OUT */)
{
  unsigned int required_feature_index;
  if (hb_ot_layout_language_get_required_feature_index (face,
							table_tag,
							script_index,
							language_index,
							&required_feature_index))
    _hb_ot_layout_collect_lookups_lookups (face,
					   table_tag,
					   required_feature_index,
					   lookup_indexes);

  if (!features)
  {
    /* All features */
    unsigned int feature_indices[32];
    unsigned int offset, len;

    offset = 0;
    do {
      len = ARRAY_LENGTH (feature_indices);
      hb_ot_layout_language_get_feature_indexes (face,
						 table_tag,
						 script_index,
						 language_index,
						 offset, &len,
						 feature_indices);

      for (unsigned int i = 0; i < len; i++)
	_hb_ot_layout_collect_lookups_lookups (face,
					       table_tag,
					       feature_indices[i],
					       lookup_indexes);

      offset += len;
    } while (len == ARRAY_LENGTH (feature_indices));
  }
  else
  {
    for (; *features; features++)
    {
      unsigned int feature_index;
      if (hb_ot_layout_language_find_feature (face,
					      table_tag,
					      script_index,
					      language_index,
					      *features,
					      &feature_index))
        _hb_ot_layout_collect_lookups_lookups (face,
					       table_tag,
					       feature_index,
					       lookup_indexes);
    }
  }
}

static void
_hb_ot_layout_collect_lookups_languages (hb_face_t      *face,
					 hb_tag_t        table_tag,
					 unsigned int    script_index,
					 const hb_tag_t *languages,
					 const hb_tag_t *features,
					 hb_set_t       *lookup_indexes /* OUT */)
{
  _hb_ot_layout_collect_lookups_features (face,
					  table_tag,
					  script_index,
					  HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX,
					  features,
					  lookup_indexes);

  if (!languages)
  {
    /* All languages */
    unsigned int count = hb_ot_layout_script_get_language_tags (face,
								table_tag,
								script_index,
								0, NULL, NULL);
    for (unsigned int language_index = 0; language_index < count; language_index++)
      _hb_ot_layout_collect_lookups_features (face,
					      table_tag,
					      script_index,
					      language_index,
					      features,
					      lookup_indexes);
  }
  else
  {
    for (; *languages; languages++)
    {
      unsigned int language_index;
      if (hb_ot_layout_script_find_language (face,
					     table_tag,
					     script_index,
					     *languages,
					     &language_index))
        _hb_ot_layout_collect_lookups_features (face,
						table_tag,
						script_index,
						language_index,
						features,
						lookup_indexes);
    }
  }
}

void
hb_ot_layout_collect_lookups (hb_face_t      *face,
			      hb_tag_t        table_tag,
			      const hb_tag_t *scripts,
			      const hb_tag_t *languages,
			      const hb_tag_t *features,
			      hb_set_t       *lookup_indexes /* OUT */)
{
  if (!scripts)
  {
    /* All scripts */
    unsigned int count = hb_ot_layout_table_get_script_tags (face,
							     table_tag,
							     0, NULL, NULL);
    for (unsigned int script_index = 0; script_index < count; script_index++)
      _hb_ot_layout_collect_lookups_languages (face,
					       table_tag,
					       script_index,
					       languages,
					       features,
					       lookup_indexes);
  }
  else
  {
    for (; *scripts; scripts++)
    {
      unsigned int script_index;
      if (hb_ot_layout_table_find_script (face,
					  table_tag,
					  *scripts,
					  &script_index))
        _hb_ot_layout_collect_lookups_languages (face,
						 table_tag,
						 script_index,
						 languages,
						 features,
						 lookup_indexes);
    }
  }
}

void
hb_ot_layout_lookup_collect_glyphs (hb_face_t    *face,
				    hb_tag_t      table_tag,
				    unsigned int  lookup_index,
				    hb_set_t     *glyphs_before, /* OUT. May be NULL */
				    hb_set_t     *glyphs_input,  /* OUT. May be NULL */
				    hb_set_t     *glyphs_after,  /* OUT. May be NULL */
				    hb_set_t     *glyphs_output  /* OUT. May be NULL */)
{
  if (unlikely (!hb_ot_shaper_face_data_ensure (face))) return;

  OT::hb_collect_glyphs_context_t c (face,
				     glyphs_before,
				     glyphs_input,
				     glyphs_after,
				     glyphs_output);

  switch (table_tag)
  {
    case HB_OT_TAG_GSUB:
    {
      const OT::SubstLookup& l = hb_ot_layout_from_face (face)->gsub->get_lookup (lookup_index);
      l.collect_glyphs_lookup (&c);
      return;
    }
    case HB_OT_TAG_GPOS:
    {
      const OT::PosLookup& l = hb_ot_layout_from_face (face)->gpos->get_lookup (lookup_index);
      l.collect_glyphs_lookup (&c);
      return;
    }
  }
}


/*
 * OT::GSUB
 */

hb_bool_t
hb_ot_layout_has_substitution (hb_face_t *face)
{
  return &_get_gsub (face) != &OT::Null(OT::GSUB);
}

hb_bool_t
hb_ot_layout_lookup_would_substitute (hb_face_t            *face,
				      unsigned int          lookup_index,
				      const hb_codepoint_t *glyphs,
				      unsigned int          glyphs_length,
				      hb_bool_t             zero_context)
{
  if (unlikely (!hb_ot_shaper_face_data_ensure (face))) return false;
  return hb_ot_layout_lookup_would_substitute_fast (face, lookup_index, glyphs, glyphs_length, zero_context);
}

hb_bool_t
hb_ot_layout_lookup_would_substitute_fast (hb_face_t            *face,
					   unsigned int          lookup_index,
					   const hb_codepoint_t *glyphs,
					   unsigned int          glyphs_length,
					   hb_bool_t             zero_context)
{
  if (unlikely (lookup_index >= hb_ot_layout_from_face (face)->gsub_lookup_count)) return false;
  OT::hb_would_apply_context_t c (face, glyphs, glyphs_length, zero_context);

  const OT::SubstLookup& l = hb_ot_layout_from_face (face)->gsub->get_lookup (lookup_index);

  return l.would_apply (&c, &hb_ot_layout_from_face (face)->gsub_digests[lookup_index]);
}

void
hb_ot_layout_substitute_start (hb_font_t *font, hb_buffer_t *buffer)
{
  OT::GSUB::substitute_start (font, buffer);
}

hb_bool_t
hb_ot_layout_substitute_lookup (hb_font_t    *font,
				hb_buffer_t  *buffer,
				unsigned int  lookup_index,
				hb_mask_t     mask,
				hb_bool_t     auto_zwj)
{
  if (unlikely (lookup_index >= hb_ot_layout_from_face (font->face)->gsub_lookup_count)) return false;

  OT::hb_apply_context_t c (0, font, buffer, mask, auto_zwj);

  const OT::SubstLookup& l = hb_ot_layout_from_face (font->face)->gsub->get_lookup (lookup_index);

  return l.apply_string (&c, &hb_ot_layout_from_face (font->face)->gsub_digests[lookup_index]);
}

void
hb_ot_layout_substitute_finish (hb_font_t *font, hb_buffer_t *buffer)
{
  OT::GSUB::substitute_finish (font, buffer);
}

void
hb_ot_layout_lookup_substitute_closure (hb_face_t    *face,
				        unsigned int  lookup_index,
				        hb_set_t     *glyphs)
{
  OT::hb_closure_context_t c (face, glyphs);

  const OT::SubstLookup& l = _get_gsub (face).get_lookup (lookup_index);

  l.closure (&c);
}

/*
 * OT::GPOS
 */

hb_bool_t
hb_ot_layout_has_positioning (hb_face_t *face)
{
  return &_get_gpos (face) != &OT::Null(OT::GPOS);
}

void
hb_ot_layout_position_start (hb_font_t *font, hb_buffer_t *buffer)
{
  OT::GPOS::position_start (font, buffer);
}

hb_bool_t
hb_ot_layout_position_lookup (hb_font_t    *font,
			      hb_buffer_t  *buffer,
			      unsigned int  lookup_index,
			      hb_mask_t     mask,
			      hb_bool_t     auto_zwj)
{
  if (unlikely (lookup_index >= hb_ot_layout_from_face (font->face)->gpos_lookup_count)) return false;

  OT::hb_apply_context_t c (1, font, buffer, mask, auto_zwj);

  const OT::PosLookup& l = hb_ot_layout_from_face (font->face)->gpos->get_lookup (lookup_index);

  return l.apply_string (&c, &hb_ot_layout_from_face (font->face)->gpos_digests[lookup_index]);
}

void
hb_ot_layout_position_finish (hb_font_t *font, hb_buffer_t *buffer)
{
  OT::GPOS::position_finish (font, buffer);
}

hb_bool_t
hb_ot_layout_get_size_params (hb_face_t    *face,
			      unsigned int *design_size,       /* OUT.  May be NULL */
			      unsigned int *subfamily_id,      /* OUT.  May be NULL */
			      unsigned int *subfamily_name_id, /* OUT.  May be NULL */
			      unsigned int *range_start,       /* OUT.  May be NULL */
			      unsigned int *range_end          /* OUT.  May be NULL */)
{
  const OT::GPOS &gpos = _get_gpos (face);
  const hb_tag_t tag = HB_TAG ('s','i','z','e');

  unsigned int num_features = gpos.get_feature_count ();
  for (unsigned int i = 0; i < num_features; i++)
  {
    if (tag == gpos.get_feature_tag (i))
    {
      const OT::Feature &f = gpos.get_feature (i);
      const OT::FeatureParamsSize &params = f.get_feature_params ().get_size_params (tag);

      if (params.designSize)
      {
#define PARAM(a, A) if (a) *a = params.A
	PARAM (design_size, designSize);
	PARAM (subfamily_id, subfamilyID);
	PARAM (subfamily_name_id, subfamilyNameID);
	PARAM (range_start, rangeStart);
	PARAM (range_end, rangeEnd);
#undef PARAM

	return true;
      }
    }
  }

#define PARAM(a, A) if (a) *a = 0
  PARAM (design_size, designSize);
  PARAM (subfamily_id, subfamilyID);
  PARAM (subfamily_name_id, subfamilyNameID);
  PARAM (range_start, rangeStart);
  PARAM (range_end, rangeEnd);
#undef PARAM

  return false;
}
