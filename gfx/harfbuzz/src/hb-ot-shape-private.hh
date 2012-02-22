/*
 * Copyright © 2010  Google, Inc.
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

#ifndef HB_OT_SHAPE_PRIVATE_HH
#define HB_OT_SHAPE_PRIVATE_HH

#include "hb-private.hh"

#include "hb-ot-shape.h"

#include "hb-ot-map-private.hh"
#include "hb-ot-shape-complex-private.hh"



enum hb_ot_complex_shaper_t;

struct hb_ot_shape_plan_t
{
  friend struct hb_ot_shape_planner_t;

  hb_ot_map_t map;
  hb_ot_complex_shaper_t shaper;

  hb_ot_shape_plan_t (void) : map () {}
  ~hb_ot_shape_plan_t (void) { map.finish (); }

  private:
  NO_COPY (hb_ot_shape_plan_t);
};

struct hb_ot_shape_planner_t
{
  hb_ot_map_builder_t map;
  hb_ot_complex_shaper_t shaper;

  hb_ot_shape_planner_t (void) : map () {}
  ~hb_ot_shape_planner_t (void) { map.finish (); }

  inline void compile (hb_face_t *face,
		       const hb_segment_properties_t *props,
		       struct hb_ot_shape_plan_t &plan)
  {
    plan.shaper = shaper;
    map.compile (face, props, plan.map);
  }

  private:
  NO_COPY (hb_ot_shape_planner_t);
};


struct hb_ot_shape_context_t
{
  /* Input to hb_ot_shape_execute() */
  hb_ot_shape_plan_t *plan;
  hb_font_t *font;
  hb_face_t *face;
  hb_buffer_t  *buffer;
  const hb_feature_t *user_features;
  unsigned int        num_user_features;

  /* Transient stuff */
  hb_direction_t target_direction;
  hb_bool_t applied_substitute_complex;
  hb_bool_t applied_position_complex;
};


static inline hb_bool_t
is_variation_selector (hb_codepoint_t unicode)
{
  return unlikely ((unicode >=  0x180B && unicode <=  0x180D) || /* MONGOLIAN FREE VARIATION SELECTOR ONE..THREE */
		   (unicode >=  0xFE00 && unicode <=  0xFE0F) || /* VARIATION SELECTOR-1..16 */
		   (unicode >= 0xE0100 && unicode <= 0xE01EF));  /* VARIATION SELECTOR-17..256 */
}

static inline unsigned int
_hb_unicode_modified_combining_class (hb_unicode_funcs_t *ufuncs,
				      hb_codepoint_t      unicode)
{
  int c = hb_unicode_combining_class (ufuncs, unicode);

  /* For Hebrew, we permute the "fixed-position" classes 10-25 into the order
   * described in the SBL Hebrew manual http://www.sbl-site.org/Fonts/SBLHebrewUserManual1.5x.pdf
   * (as recommended by http://forum.fontlab.com/archive-old-microsoft-volt-group/vista-and-diacritic-ordering-t6751.0.html)
   */
  static const int permuted_hebrew_classes[25 - 10 + 1] = {
    /* 10 sheva */        22,
    /* 11 hataf segol */  15,
    /* 12 hataf patah */  16,
    /* 13 hataf qamats */ 17,
    /* 14 hiriq */        23,
    /* 15 tsere */        18,
    /* 16 segol */        19,
    /* 17 patah */        20,
    /* 18 qamats */       21,
    /* 19 holam */        14,
    /* 20 qubuts */       24,
    /* 21 dagesh */       12,
    /* 22 meteg */        25,
    /* 23 rafe */         13,
    /* 24 shin dot */     10,
    /* 25 sin dot */      11,
  };

  /* Modify the combining-class to suit Arabic better.  See:
   * http://unicode.org/faq/normalization.html#8
   * http://unicode.org/faq/normalization.html#9
   */
  if (unlikely (hb_in_range<int> (c, 27, 33)))
    c = c == 33 ? 27 : c + 1;
  /* The equivalent fix for Hebrew is more complex,
   * see the SBL Hebrew manual.
   */
  else if (unlikely (hb_in_range<int> (c, 10, 25)))
    c = permuted_hebrew_classes[c - 10];

  return c;
}

static inline void
hb_glyph_info_set_unicode_props (hb_glyph_info_t *info, hb_unicode_funcs_t *unicode)
{
  info->general_category() = hb_unicode_general_category (unicode, info->codepoint);
  info->combining_class() = _hb_unicode_modified_combining_class (unicode, info->codepoint);
}

HB_INTERNAL void _hb_set_unicode_props (hb_buffer_t *buffer);

HB_INTERNAL void _hb_ot_shape_normalize (hb_ot_shape_context_t *c);


#endif /* HB_OT_SHAPE_PRIVATE_HH */
