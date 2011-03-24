/*
 * Copyright (C) 1998-2004  David Turner and Werner Lemberg
 * Copyright (C) 2006  Behdad Esfahbod
 * Copyright (C) 2007,2008,2009  Red Hat, Inc.
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

#define HB_OT_LAYOUT_CC

#include "hb-ot-layout-private.hh"

#include "hb-ot-layout-gdef-private.hh"
#include "hb-ot-layout-gsub-private.hh"
#include "hb-ot-layout-gpos-private.hh"
#include "hb-ot-shape-private.hh"


#include <stdlib.h>
#include <string.h>

HB_BEGIN_DECLS


hb_ot_layout_t *
_hb_ot_layout_new (hb_face_t *face)
{
  /* Remove this object altogether */
  hb_ot_layout_t *layout = (hb_ot_layout_t *) calloc (1, sizeof (hb_ot_layout_t));

  layout->gdef_blob = Sanitizer<GDEF>::sanitize (hb_face_get_table (face, HB_OT_TAG_GDEF));
  layout->gdef = Sanitizer<GDEF>::lock_instance (layout->gdef_blob);

  layout->gsub_blob = Sanitizer<GSUB>::sanitize (hb_face_get_table (face, HB_OT_TAG_GSUB));
  layout->gsub = Sanitizer<GSUB>::lock_instance (layout->gsub_blob);

  layout->gpos_blob = Sanitizer<GPOS>::sanitize (hb_face_get_table (face, HB_OT_TAG_GPOS));
  layout->gpos = Sanitizer<GPOS>::lock_instance (layout->gpos_blob);

  return layout;
}

void
_hb_ot_layout_free (hb_ot_layout_t *layout)
{
  hb_blob_unlock (layout->gdef_blob);
  hb_blob_unlock (layout->gsub_blob);
  hb_blob_unlock (layout->gpos_blob);

  hb_blob_destroy (layout->gdef_blob);
  hb_blob_destroy (layout->gsub_blob);
  hb_blob_destroy (layout->gpos_blob);

  free (layout);
}

static inline const GDEF&
_get_gdef (hb_face_t *face)
{
  return likely (face->ot_layout && face->ot_layout->gdef) ? *face->ot_layout->gdef : Null(GDEF);
}

static inline const GSUB&
_get_gsub (hb_face_t *face)
{
  return likely (face->ot_layout && face->ot_layout->gsub) ? *face->ot_layout->gsub : Null(GSUB);
}

static inline const GPOS&
_get_gpos (hb_face_t *face)
{
  return likely (face->ot_layout && face->ot_layout->gpos) ? *face->ot_layout->gpos : Null(GPOS);
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
_hb_ot_layout_get_glyph_property (hb_face_t       *face,
				  hb_glyph_info_t *info)
{
  if (!info->props_cache())
  {
    const GDEF &gdef = _get_gdef (face);
    info->props_cache() = gdef.get_glyph_props (info->codepoint);
  }

  return info->props_cache();
}

static hb_bool_t
_hb_ot_layout_match_properties (hb_face_t      *face,
				hb_codepoint_t  codepoint,
				unsigned int    glyph_props,
				unsigned int    lookup_props)
{
  /* Not covered, if, for example, glyph class is ligature and
   * lookup_props includes LookupFlags::IgnoreLigatures
   */
  if (glyph_props & lookup_props & LookupFlag::IgnoreFlags)
    return false;

  if (glyph_props & HB_OT_LAYOUT_GLYPH_CLASS_MARK)
  {
    /* If using mark filtering sets, the high short of
     * lookup_props has the set index.
     */
    if (lookup_props & LookupFlag::UseMarkFilteringSet)
      return _get_gdef (face).mark_set_covers (lookup_props >> 16, codepoint);

    /* The second byte of lookup_props has the meaning
     * "ignore marks of attachment type different than
     * the attachment type specified."
     */
    if (lookup_props & LookupFlag::MarkAttachmentType && glyph_props & LookupFlag::MarkAttachmentType)
      return (lookup_props & LookupFlag::MarkAttachmentType) == (glyph_props & LookupFlag::MarkAttachmentType);
  }

  return true;
}

hb_bool_t
_hb_ot_layout_check_glyph_property (hb_face_t    *face,
				    hb_glyph_info_t *ginfo,
				    unsigned int  lookup_props,
				    unsigned int *property_out)
{
  unsigned int property;

  property = _hb_ot_layout_get_glyph_property (face, ginfo);
  (void) (property_out && (*property_out = property));

  return _hb_ot_layout_match_properties (face, ginfo->codepoint, property, lookup_props);
}

hb_bool_t
_hb_ot_layout_skip_mark (hb_face_t    *face,
			 hb_glyph_info_t *ginfo,
			 unsigned int  lookup_props,
			 unsigned int *property_out)
{
  unsigned int property;

  property = _hb_ot_layout_get_glyph_property (face, ginfo);
  (void) (property_out && (*property_out = property));

  /* If it's a mark, skip it we don't accept it. */
  if (unlikely (property & HB_OT_LAYOUT_GLYPH_CLASS_MARK))
    return !_hb_ot_layout_match_properties (face, ginfo->codepoint, property, lookup_props);

  /* If not a mark, don't skip. */
  return false;
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
				  hb_face_t      *face,
				  hb_direction_t  direction,
				  hb_codepoint_t  glyph,
				  unsigned int    start_offset,
				  unsigned int   *caret_count /* IN/OUT */,
				  int            *caret_array /* OUT */)
{
  hb_ot_layout_context_t c;
  c.font = font;
  c.face = face;
  return _get_gdef (face).get_lig_carets (&c, direction, glyph, start_offset, caret_count, caret_array);
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
    return TRUE;

  /* try finding 'DFLT' */
  if (g.find_script_index (HB_OT_TAG_DEFAULT_SCRIPT, script_index))
    return FALSE;

  /* try with 'dflt'; MS site has had typos and many fonts use it now :(.
   * including many versions of DejaVu Sans Mono! */
  if (g.find_script_index (HB_OT_TAG_DEFAULT_LANGUAGE, script_index))
    return FALSE;

  if (script_index) *script_index = HB_OT_LAYOUT_NO_SCRIPT_INDEX;
  return FALSE;
}

hb_bool_t
hb_ot_layout_table_choose_script (hb_face_t      *face,
				  hb_tag_t        table_tag,
				  const hb_tag_t *script_tags,
				  unsigned int   *script_index)
{
  ASSERT_STATIC (Index::NOT_FOUND_INDEX == HB_OT_LAYOUT_NO_SCRIPT_INDEX);
  const GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

  while (*script_tags)
  {
    if (g.find_script_index (*script_tags, script_index))
      return TRUE;
    script_tags++;
  }

  /* try finding 'DFLT' */
  if (g.find_script_index (HB_OT_TAG_DEFAULT_SCRIPT, script_index))
    return FALSE;

  /* try with 'dflt'; MS site has had typos and many fonts use it now :( */
  if (g.find_script_index (HB_OT_TAG_DEFAULT_LANGUAGE, script_index))
    return FALSE;

  if (script_index) *script_index = HB_OT_LAYOUT_NO_SCRIPT_INDEX;
  return FALSE;
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
    return TRUE;

  /* try with 'dflt'; MS site has had typos and many fonts use it now :( */
  if (s.find_lang_sys_index (HB_OT_TAG_DEFAULT_LANGUAGE, language_index))
    return FALSE;

  if (language_index) *language_index = HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX;
  return FALSE;
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
      return TRUE;
    }
  }

  if (feature_index) *feature_index = HB_OT_LAYOUT_NO_FEATURE_INDEX;
  return FALSE;
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
hb_ot_layout_substitute_lookup (hb_face_t    *face,
				hb_buffer_t  *buffer,
				unsigned int  lookup_index,
				hb_mask_t     mask)
{
  hb_ot_layout_context_t c;
  c.font = NULL;
  c.face = face;
  return _get_gsub (face).substitute_lookup (&c, buffer, lookup_index, mask);
}


/*
 * GPOS
 */

hb_bool_t
hb_ot_layout_has_positioning (hb_face_t *face)
{
  return &_get_gpos (face) != &Null(GPOS);
}

hb_bool_t
hb_ot_layout_position_lookup   (hb_font_t    *font,
				hb_face_t    *face,
				hb_buffer_t  *buffer,
				unsigned int  lookup_index,
				hb_mask_t     mask)
{
  hb_ot_layout_context_t c;
  c.font = font;
  c.face = face;
  return _get_gpos (face).position_lookup (&c, buffer, lookup_index, mask);
}

void
hb_ot_layout_position_finish (hb_face_t *face, hb_buffer_t *buffer)
{
  /* force diacritics to have zero width */
  unsigned int count = buffer->len;
  if (hb_ot_layout_has_glyph_classes (face)) {
    const GDEF& gdef = _get_gdef (face);
    if (buffer->props.direction == HB_DIRECTION_RTL) {
      for (unsigned int i = 1; i < count; i++) {
        if (gdef.get_glyph_class (buffer->info[i].codepoint) == GDEF::MarkGlyph) {
          buffer->pos[i].x_advance = 0;
        }
      }
    } else {
      for (unsigned int i = 1; i < count; i++) {
        if (gdef.get_glyph_class (buffer->info[i].codepoint) == GDEF::MarkGlyph) {
          hb_glyph_position_t& pos = buffer->pos[i];
          pos.x_offset -= pos.x_advance;
          pos.x_advance = 0;
        }
      }
    }
  } else {
    /* no GDEF classes available, so use General Category as a fallback */
    if (buffer->props.direction == HB_DIRECTION_RTL) {
      for (unsigned int i = 1; i < count; i++) {
        if (buffer->info[i].general_category() == HB_CATEGORY_NON_SPACING_MARK) {
          buffer->pos[i].x_advance = 0;
        }
      }
    } else {
      for (unsigned int i = 1; i < count; i++) {
        if (buffer->info[i].general_category() == HB_CATEGORY_NON_SPACING_MARK) {
          hb_glyph_position_t& pos = buffer->pos[i];
          pos.x_offset -= pos.x_advance;
          pos.x_advance = 0;
        }
      }
    }
  }

  GPOS::position_finish (buffer);
}


HB_END_DECLS
