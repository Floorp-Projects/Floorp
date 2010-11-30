/*
 * Copyright (C) 2010  Google, Inc.
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

#include "hb-private.h"

#include "hb-ot-shape.h"

#include "hb-ot-map-private.hh"

HB_BEGIN_DECLS


/* buffer var allocations */
#define general_category() var1.u8[0] /* unicode general_category (hb_category_t) */
#define combining_class() var1.u8[1] /* unicode combining_class (uint8_t) */


enum hb_ot_complex_shaper_t {
  hb_ot_complex_shaper_none,
  hb_ot_complex_shaper_arabic
};


struct hb_ot_shape_plan_t
{
  hb_ot_map_t map;
  hb_ot_complex_shaper_t shaper;
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
  hb_direction_t original_direction;
  hb_bool_t applied_substitute_complex;
  hb_bool_t applied_position_complex;
};


HB_END_DECLS

#endif /* HB_OT_SHAPE_PRIVATE_HH */
