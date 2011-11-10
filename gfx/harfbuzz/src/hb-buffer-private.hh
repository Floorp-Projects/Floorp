/*
 * Copyright (C) 1998-2004  David Turner and Werner Lemberg
 * Copyright (C) 2004,2007,2009,2010  Red Hat, Inc.
 *
 * This is part of HarfBuzz, a text shaping library.
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
 * Red Hat Author(s): Owen Taylor, Behdad Esfahbod
 */

#ifndef HB_BUFFER_PRIVATE_HH
#define HB_BUFFER_PRIVATE_HH

#include "hb-private.h"
#include "hb-buffer.h"
#include "hb-unicode-private.h"

HB_BEGIN_DECLS


ASSERT_STATIC (sizeof (hb_glyph_info_t) == 20);
ASSERT_STATIC (sizeof (hb_glyph_info_t) == sizeof (hb_glyph_position_t));

typedef struct _hb_segment_properties_t {
    hb_direction_t      direction;
    hb_script_t         script;
    hb_language_t       language;
} hb_segment_properties_t;


HB_INTERNAL void
_hb_buffer_swap (hb_buffer_t *buffer);

HB_INTERNAL void
_hb_buffer_clear_output (hb_buffer_t *buffer);

HB_INTERNAL void
_hb_buffer_replace_glyphs_be16 (hb_buffer_t *buffer,
				unsigned int num_in,
				unsigned int num_out,
				const uint16_t *glyph_data_be);

HB_INTERNAL void
_hb_buffer_replace_glyph (hb_buffer_t *buffer,
			  hb_codepoint_t glyph_index);

HB_INTERNAL void
_hb_buffer_next_glyph (hb_buffer_t *buffer);


HB_INTERNAL void
_hb_buffer_reset_masks (hb_buffer_t *buffer,
			hb_mask_t    mask);

HB_INTERNAL void
_hb_buffer_add_masks (hb_buffer_t *buffer,
		      hb_mask_t    mask);

HB_INTERNAL void
_hb_buffer_set_masks (hb_buffer_t *buffer,
		      hb_mask_t    value,
		      hb_mask_t    mask,
		      unsigned int cluster_start,
		      unsigned int cluster_end);


struct _hb_buffer_t {
  hb_reference_count_t ref_count;

  /* Information about how the text in the buffer should be treated */

  hb_unicode_funcs_t *unicode; /* Unicode functions */
  hb_segment_properties_t props; /* Script, language, direction */

  /* Buffer contents */

  unsigned int allocated; /* Length of allocated arrays */

  hb_bool_t have_output; /* Whether we have an output buffer going on */
  hb_bool_t have_positions; /* Whether we have positions */
  hb_bool_t in_error; /* Allocation failed */

  unsigned int i; /* Cursor into ->info and ->pos arrays */
  unsigned int len; /* Length of ->info and ->pos arrays */
  unsigned int out_len; /* Length of ->out array if have_output */

  hb_glyph_info_t     *info;
  hb_glyph_info_t     *out_info;
  hb_glyph_position_t *pos;

  /* Other stuff */

  unsigned int serial;


  /* Methods */
  inline unsigned int backtrack_len (void) const
  { return this->have_output? this->out_len : this->i; }
  inline unsigned int next_serial (void) { return serial++; }
  inline void swap (void) { _hb_buffer_swap (this); }
  inline void clear_output (void) { _hb_buffer_clear_output (this); }
  inline void next_glyph (void) { _hb_buffer_next_glyph (this); }
  inline void replace_glyphs_be16 (unsigned int num_in,
				   unsigned int num_out,
				   const uint16_t *glyph_data_be)
  { _hb_buffer_replace_glyphs_be16 (this, num_in, num_out, glyph_data_be); }
  inline void replace_glyph (hb_codepoint_t glyph_index)
  { _hb_buffer_replace_glyph (this, glyph_index); }

  inline void reset_masks (hb_mask_t mask)
  {
    for (unsigned int i = 0; i < len; i++)
      info[i].mask = mask;
  }
  inline void add_masks (hb_mask_t mask)
  {
    for (unsigned int i = 0; i < len; i++)
      info[i].mask |= mask;
  }
  inline void set_masks (hb_mask_t value,
			 hb_mask_t mask,
			 unsigned int cluster_start,
			 unsigned int cluster_end)
  { _hb_buffer_set_masks (this, value, mask, cluster_start, cluster_end); }
};


HB_END_DECLS

#endif /* HB_BUFFER_PRIVATE_HH */
