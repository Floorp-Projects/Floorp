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

#ifndef HB_OT_SHAPE_COMPLEX_PRIVATE_HH
#define HB_OT_SHAPE_COMPLEX_PRIVATE_HH

#include "hb-private.h"

#include "hb-ot-shape-private.hh"

HB_BEGIN_DECLS


static inline hb_ot_complex_shaper_t
hb_ot_shape_complex_categorize (const hb_segment_properties_t *props)
{
  switch ((int) props->script) {
    case HB_SCRIPT_ARABIC:
    case HB_SCRIPT_NKO:
    case HB_SCRIPT_SYRIAC:
      return hb_ot_complex_shaper_arabic;

    default:
      return hb_ot_complex_shaper_none;
  }
}



/*
 * collect_features()
 *
 * Called during shape_plan().
 *
 * Shapers should use plan->map to add their features.
 */

HB_INTERNAL void _hb_ot_shape_complex_collect_features_arabic	(hb_ot_shape_plan_t *plan, const hb_segment_properties_t  *props);

static inline void
hb_ot_shape_complex_collect_features (hb_ot_shape_plan_t *plan,
				      const hb_segment_properties_t  *props)
{
  switch (plan->shaper) {
    case hb_ot_complex_shaper_arabic:	_hb_ot_shape_complex_collect_features_arabic (plan, props);	return;
    case hb_ot_complex_shaper_none:	default:							return;
  }
}


/* setup_masks()
 *
 * Called during shape_execute().
 *
 * Shapers should use c->plan.map to get feature masks and set on buffer.
 */

HB_INTERNAL void _hb_ot_shape_complex_setup_masks_arabic	(hb_ot_shape_context_t *c);

static inline void
hb_ot_shape_complex_setup_masks (hb_ot_shape_context_t *c)
{
  switch (c->plan->shaper) {
    case hb_ot_complex_shaper_arabic:	_hb_ot_shape_complex_setup_masks_arabic (c);	return;
    case hb_ot_complex_shaper_none:	default:					return;
  }
}


HB_END_DECLS

#endif /* HB_OT_SHAPE_COMPLEX_PRIVATE_HH */
