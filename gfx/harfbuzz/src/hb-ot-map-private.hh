/*
 * Copyright © 2009,2010  Red Hat, Inc.
 * Copyright © 2010,2011  Google, Inc.
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

#ifndef HB_OT_MAP_PRIVATE_HH
#define HB_OT_MAP_PRIVATE_HH

#include "hb-buffer-private.hh"

#include "hb-ot-layout.h"



static const hb_tag_t table_tags[2] = {HB_OT_TAG_GSUB, HB_OT_TAG_GPOS};

struct hb_ot_map_t
{
  friend struct hb_ot_map_builder_t;

  public:

  hb_ot_map_t (void) { memset (this, 0, sizeof (*this)); }

  typedef void (*gsub_pause_func_t) (const hb_ot_map_t *map, hb_face_t *face, hb_buffer_t *buffer, void *user_data);
  typedef void (*gpos_pause_func_t) (const hb_ot_map_t *map, hb_font_t *font, hb_buffer_t *buffer, void *user_data);

  inline hb_mask_t get_global_mask (void) const { return global_mask; }

  inline hb_mask_t get_mask (hb_tag_t tag, unsigned int *shift = NULL) const {
    const feature_map_t *map = features.bsearch (&tag);
    if (shift) *shift = map ? map->shift : 0;
    return map ? map->mask : 0;
  }

  inline hb_mask_t get_1_mask (hb_tag_t tag) const {
    const feature_map_t *map = features.bsearch (&tag);
    return map ? map->_1_mask : 0;
  }

  inline hb_tag_t get_chosen_script (unsigned int table_index) const
  { return chosen_script[table_index]; }

  inline void substitute (hb_face_t *face, hb_buffer_t *buffer) const
  { apply (0, (hb_ot_map_t::apply_lookup_func_t) hb_ot_layout_substitute_lookup, face, buffer); }
  inline void position (hb_font_t *font, hb_buffer_t *buffer) const
  { apply (1, (hb_ot_map_t::apply_lookup_func_t) hb_ot_layout_position_lookup, font, buffer); }

  HB_INTERNAL void substitute_closure (hb_face_t *face,
				       hb_set_t *glyphs) const;


  inline void finish (void) {
    features.finish ();
    lookups[0].finish ();
    lookups[1].finish ();
    pauses[0].finish ();
    pauses[1].finish ();
  }

  private:

  struct feature_map_t {
    hb_tag_t tag; /* should be first for our bsearch to work */
    unsigned int index[2]; /* GSUB/GPOS */
    unsigned int stage[2]; /* GSUB/GPOS */
    unsigned int shift;
    hb_mask_t mask;
    hb_mask_t _1_mask; /* mask for value=1, for quick access */

    static int cmp (const feature_map_t *a, const feature_map_t *b)
    { return a->tag < b->tag ? -1 : a->tag > b->tag ? 1 : 0; }
  };

  struct lookup_map_t {
    unsigned int index;
    hb_mask_t mask;

    static int cmp (const lookup_map_t *a, const lookup_map_t *b)
    { return a->index < b->index ? -1 : a->index > b->index ? 1 : 0; }
  };

  typedef void (*pause_func_t) (const hb_ot_map_t *map, void *face_or_font, hb_buffer_t *buffer, void *user_data);
  typedef struct {
    pause_func_t func;
    void *user_data;
  } pause_callback_t;

  struct pause_map_t {
    unsigned int num_lookups; /* Cumulative */
    pause_callback_t callback;
  };

  typedef hb_bool_t (*apply_lookup_func_t) (void *face_or_font,
					    hb_buffer_t  *buffer,
					    unsigned int  lookup_index,
					    hb_mask_t     mask);

  HB_INTERNAL void add_lookups (hb_face_t    *face,
				unsigned int  table_index,
				unsigned int  feature_index,
				hb_mask_t     mask);

  HB_INTERNAL void apply (unsigned int table_index,
			  hb_ot_map_t::apply_lookup_func_t apply_lookup_func,
			  void *face_or_font,
			  hb_buffer_t *buffer) const;

  hb_mask_t global_mask;

  hb_tag_t chosen_script[2];
  hb_prealloced_array_t<feature_map_t, 8> features;
  hb_prealloced_array_t<lookup_map_t, 32> lookups[2]; /* GSUB/GPOS */
  hb_prealloced_array_t<pause_map_t, 1> pauses[2]; /* GSUB/GPOS */
};


struct hb_ot_map_builder_t
{
  public:

  hb_ot_map_builder_t (void) { memset (this, 0, sizeof (*this)); }

  HB_INTERNAL void add_feature (hb_tag_t tag, unsigned int value, bool global);

  inline void add_bool_feature (hb_tag_t tag, bool global = true)
  { add_feature (tag, 1, global); }

  inline void add_gsub_pause (hb_ot_map_t::gsub_pause_func_t pause_func, void *user_data)
  { add_pause (0, (hb_ot_map_t::pause_func_t) pause_func, user_data); }
  inline void add_gpos_pause (hb_ot_map_t::gpos_pause_func_t pause_func, void *user_data)
  { add_pause (1, (hb_ot_map_t::pause_func_t) pause_func, user_data); }

  HB_INTERNAL void compile (hb_face_t *face,
			    const hb_segment_properties_t *props,
			    struct hb_ot_map_t &m);

  inline void finish (void) {
    feature_infos.finish ();
    pauses[0].finish ();
    pauses[1].finish ();
  }

  private:

  struct feature_info_t {
    hb_tag_t tag;
    unsigned int seq; /* sequence#, used for stable sorting only */
    unsigned int max_value;
    bool global; /* whether the feature applies value to every glyph in the buffer */
    unsigned int default_value; /* for non-global features, what should the unset glyphs take */
    unsigned int stage[2]; /* GSUB/GPOS */

    static int cmp (const feature_info_t *a, const feature_info_t *b)
    { return (a->tag != b->tag) ?  (a->tag < b->tag ? -1 : 1) : (a->seq < b->seq ? -1 : 1); }
  };

  struct pause_info_t {
    unsigned int stage;
    hb_ot_map_t::pause_callback_t callback;
  };

  HB_INTERNAL void add_pause (unsigned int table_index, hb_ot_map_t::pause_func_t pause_func, void *user_data);

  unsigned int current_stage[2]; /* GSUB/GPOS */
  hb_prealloced_array_t<feature_info_t,16> feature_infos;
  hb_prealloced_array_t<pause_info_t, 1> pauses[2]; /* GSUB/GPOS */
};



#endif /* HB_OT_MAP_PRIVATE_HH */
