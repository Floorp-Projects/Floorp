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

#include "hb-ot-shape-complex-private.hh"


/* TODO Add kana, hangul, and other small sahpers here */

/* When adding trivial shapers, eg. kana, hangul, etc, we can either
 * add a full shaper enum value for them, or switch on the script in
 * the default complex shaper.  The former is faster, so I think that's
 * what we would do, and hence the default complex shaper shall remain
 * empty.
 */

void
_hb_ot_shape_complex_collect_features_default (hb_ot_map_builder_t *map, const hb_segment_properties_t  *props)
{
}

bool
_hb_ot_shape_complex_prefer_decomposed_default (void)
{
  return FALSE;
}

void
_hb_ot_shape_complex_setup_masks_default (hb_ot_map_t *map, hb_buffer_t *buffer)
{
}


