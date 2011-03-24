/*
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

#ifndef HB_OT_LAYOUT_H
#define HB_OT_LAYOUT_H

#include "hb-common.h"
#include "hb-buffer.h"
#include "hb-font.h"

#include "hb-ot-tag.h"

HB_BEGIN_DECLS


#define HB_OT_TAG_GDEF HB_TAG('G','D','E','F')
#define HB_OT_TAG_GSUB HB_TAG('G','S','U','B')
#define HB_OT_TAG_GPOS HB_TAG('G','P','O','S')

/*
 * GDEF
 */

hb_bool_t
hb_ot_layout_has_glyph_classes (hb_face_t *face);

/* Not that useful.  Provides list of attach points for a glyph that a
 * client may want to cache */
unsigned int
hb_ot_layout_get_attach_points (hb_face_t      *face,
				hb_codepoint_t  glyph,
				unsigned int    start_offset,
				unsigned int   *point_count /* IN/OUT */,
				unsigned int   *point_array /* OUT */);

/* Ligature caret positions */
unsigned int
hb_ot_layout_get_ligature_carets (hb_font_t      *font,
				  hb_face_t      *face,
				  hb_direction_t  direction,
				  hb_codepoint_t  glyph,
				  unsigned int    start_offset,
				  unsigned int   *caret_count /* IN/OUT */,
				  int            *caret_array /* OUT */);


/*
 * GSUB/GPOS feature query and enumeration interface
 */

#define HB_OT_LAYOUT_NO_SCRIPT_INDEX		((unsigned int) 0xFFFF)
#define HB_OT_LAYOUT_NO_FEATURE_INDEX		((unsigned int) 0xFFFF)
#define HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX	((unsigned int) 0xFFFF)

unsigned int
hb_ot_layout_table_get_script_tags (hb_face_t    *face,
				    hb_tag_t      table_tag,
				    unsigned int  start_offset,
				    unsigned int *script_count /* IN/OUT */,
				    hb_tag_t     *script_tags /* OUT */);

hb_bool_t
hb_ot_layout_table_find_script (hb_face_t    *face,
				hb_tag_t      table_tag,
				hb_tag_t      script_tag,
				unsigned int *script_index);

/* Like find_script, but takes zero-terminated array of scripts to test */
hb_bool_t
hb_ot_layout_table_choose_script (hb_face_t      *face,
				  hb_tag_t        table_tag,
				  const hb_tag_t *script_tags,
				  unsigned int   *script_index);

unsigned int
hb_ot_layout_table_get_feature_tags (hb_face_t    *face,
				     hb_tag_t      table_tag,
				     unsigned int  start_offset,
				     unsigned int *feature_count /* IN/OUT */,
				     hb_tag_t     *feature_tags /* OUT */);

unsigned int
hb_ot_layout_script_get_language_tags (hb_face_t    *face,
				       hb_tag_t      table_tag,
				       unsigned int  script_index,
				       unsigned int  start_offset,
				       unsigned int *language_count /* IN/OUT */,
				       hb_tag_t     *language_tags /* OUT */);

hb_bool_t
hb_ot_layout_script_find_language (hb_face_t    *face,
				   hb_tag_t      table_tag,
				   unsigned int  script_index,
				   hb_tag_t      language_tag,
				   unsigned int *language_index);

hb_bool_t
hb_ot_layout_language_get_required_feature_index (hb_face_t    *face,
						  hb_tag_t      table_tag,
						  unsigned int  script_index,
						  unsigned int  language_index,
						  unsigned int *feature_index);

unsigned int
hb_ot_layout_language_get_feature_indexes (hb_face_t    *face,
					   hb_tag_t      table_tag,
					   unsigned int  script_index,
					   unsigned int  language_index,
					   unsigned int  start_offset,
					   unsigned int *feature_count /* IN/OUT */,
					   unsigned int *feature_indexes /* OUT */);

unsigned int
hb_ot_layout_language_get_feature_tags (hb_face_t    *face,
					hb_tag_t      table_tag,
					unsigned int  script_index,
					unsigned int  language_index,
					unsigned int  start_offset,
					unsigned int *feature_count /* IN/OUT */,
					hb_tag_t     *feature_tags /* OUT */);

hb_bool_t
hb_ot_layout_language_find_feature (hb_face_t    *face,
				    hb_tag_t      table_tag,
				    unsigned int  script_index,
				    unsigned int  language_index,
				    hb_tag_t      feature_tag,
				    unsigned int *feature_index);

unsigned int
hb_ot_layout_feature_get_lookup_indexes (hb_face_t    *face,
					 hb_tag_t      table_tag,
					 unsigned int  feature_index,
					 unsigned int  start_offset,
					 unsigned int *lookup_count /* IN/OUT */,
					 unsigned int *lookup_indexes /* OUT */);


/*
 * GSUB
 */

hb_bool_t
hb_ot_layout_has_substitution (hb_face_t *face);

hb_bool_t
hb_ot_layout_substitute_lookup (hb_face_t    *face,
				hb_buffer_t  *buffer,
				unsigned int  lookup_index,
				hb_mask_t     mask);

/*
 * GPOS
 */

hb_bool_t
hb_ot_layout_has_positioning (hb_face_t *face);

hb_bool_t
hb_ot_layout_position_lookup (hb_font_t    *font,
			      hb_face_t    *face,
			      hb_buffer_t  *buffer,
			      unsigned int  lookup_index,
			      hb_mask_t     mask);

/* Should be called after all the position_lookup's are done */
void
hb_ot_layout_position_finish (hb_face_t *face, hb_buffer_t *buffer);


HB_END_DECLS

#endif /* HB_OT_LAYOUT_H */
