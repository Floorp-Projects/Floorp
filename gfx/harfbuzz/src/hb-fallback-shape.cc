/*
 * Copyright © 2011  Google, Inc.
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

#include "hb-fallback-shape-private.hh"

#include "hb-buffer-private.hh"

hb_bool_t
_hb_fallback_shape (hb_font_t          *font,
		    hb_buffer_t        *buffer,
		    const hb_feature_t *features,
		    unsigned int        num_features)
{
  buffer->guess_properties ();

  unsigned int count = buffer->len;

  for (unsigned int i = 0; i < count; i++)
    hb_font_get_glyph (font, buffer->info[i].codepoint, 0, &buffer->info[i].codepoint);

  buffer->clear_positions ();

  for (unsigned int i = 0; i < count; i++) {
    hb_font_get_glyph_advance_for_direction (font, buffer->info[i].codepoint,
					     buffer->props.direction,
					     &buffer->pos[i].x_advance,
					     &buffer->pos[i].y_advance);
    hb_font_subtract_glyph_origin_for_direction (font, buffer->info[i].codepoint,
						 buffer->props.direction,
						 &buffer->pos[i].x_offset,
						 &buffer->pos[i].y_offset);
  }

  if (HB_DIRECTION_IS_BACKWARD (buffer->props.direction))
    hb_buffer_reverse (buffer);

  return TRUE;
}
