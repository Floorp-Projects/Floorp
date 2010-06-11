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

#ifndef HB_OT_LAYOUT_PRIVATE_H
#define HB_OT_LAYOUT_PRIVATE_H

#include "hb-private.h"

#include "hb-ot-layout.h"

#include "hb-font-private.hh"
#include "hb-buffer-private.hh"


HB_BEGIN_DECLS

typedef unsigned int hb_ot_layout_class_t;

/*
 * hb_ot_layout_t
 */

struct hb_ot_layout_t
{
  hb_blob_t *gdef_blob;
  hb_blob_t *gsub_blob;
  hb_blob_t *gpos_blob;

  const struct GDEF *gdef;
  const struct GSUB *gsub;
  const struct GPOS *gpos;

  struct
  {
    unsigned char *klasses;
    unsigned int len;
  } new_gdef;
};

struct hb_ot_layout_context_t
{
  hb_face_t *face;
  hb_font_t *font;

  union info_t
  {
    struct gpos_t
    {
      unsigned int last;        /* the last valid glyph--used with cursive positioning */
      hb_position_t anchor_x;   /* the coordinates of the anchor point */
      hb_position_t anchor_y;   /* of the last valid glyph */
    } gpos;
  } info;

  /* Convert from font-space to user-space */
  /* XXX div-by-zero / speed up */
  inline hb_position_t scale_x (int16_t v) { return (int64_t) this->font->x_scale * v / this->face->head_table->unitsPerEm; }
  inline hb_position_t scale_y (int16_t v) { return (int64_t) this->font->y_scale * v / this->face->head_table->unitsPerEm; }
};


HB_INTERNAL hb_ot_layout_t *
_hb_ot_layout_new (hb_face_t *face);

HB_INTERNAL void
_hb_ot_layout_free (hb_ot_layout_t *layout);


/*
 * GDEF
 */

HB_INTERNAL hb_bool_t
_hb_ot_layout_has_new_glyph_classes (hb_face_t *face);

HB_INTERNAL void
_hb_ot_layout_set_glyph_property (hb_face_t      *face,
				  hb_codepoint_t  glyph,
				  unsigned int    property);

HB_INTERNAL void
_hb_ot_layout_set_glyph_class (hb_face_t                  *face,
			       hb_codepoint_t              glyph,
			       hb_ot_layout_glyph_class_t  klass);

HB_INTERNAL hb_bool_t
_hb_ot_layout_check_glyph_property (hb_face_t    *face,
				    hb_internal_glyph_info_t *ginfo,
				    unsigned int  lookup_flags,
				    unsigned int *property);

HB_INTERNAL hb_bool_t
_hb_ot_layout_skip_mark (hb_face_t    *face,
			 hb_internal_glyph_info_t *ginfo,
			 unsigned int  lookup_flags,
			 unsigned int *property);

HB_END_DECLS

#endif /* HB_OT_LAYOUT_PRIVATE_H */
