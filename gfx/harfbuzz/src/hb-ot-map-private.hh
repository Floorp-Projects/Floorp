/*
 * Copyright (C) 2009,2010  Red Hat, Inc.
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
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_MAP_PRIVATE_HH
#define HB_OT_MAP_PRIVATE_HH

#include "hb-buffer-private.hh"

#include "hb-ot-layout.h"

HB_BEGIN_DECLS


#define MAX_FEATURES 100 /* FIXME */
#define MAX_LOOKUPS 1000 /* FIXME */

/* some constants for feature-ordering priorities; intermediate values can also be
   used if required for shapers that want precise control. */
#define FIRST_PRIORITY   100
#define EARLY_PRIORITY   300
#define DEFAULT_PRIORITY 500
#define LATE_PRIORITY    700
#define LAST_PRIORITY    900

static const hb_tag_t table_tags[2] = {HB_OT_TAG_GSUB, HB_OT_TAG_GPOS};

struct hb_ot_map_t {

  private:

  struct feature_info_t {
    hb_tag_t tag;
    unsigned int seq; /* sequence#, used for stable sorting only */
    unsigned int max_value;
    bool global; /* whether the feature applies value to every glyph in the buffer */
    unsigned short default_value; /* for non-global features, what should the unset glyphs take */
    unsigned short priority; /* feature ordering priority, for cases such as Arabic */

    static int cmp (const feature_info_t *a, const feature_info_t *b)
    { return (a->tag != b->tag) ?  (a->tag < b->tag ? -1 : 1) : (a->seq < b->seq ? -1 : 1); }
  };

  struct feature_map_t {
    hb_tag_t tag; /* should be first for our bsearch to work */
    unsigned int index[2]; /* GSUB, GPOS */
    unsigned short shift;
    unsigned short priority;
    hb_mask_t mask;
    hb_mask_t _1_mask; /* mask for value=1, for quick access */

    static int cmp (const feature_map_t *a, const feature_map_t *b)
    { return a->tag < b->tag ? -1 : a->tag > b->tag ? 1 : 0; }
  };

  struct lookup_map_t {
    unsigned short index;
    unsigned short priority;
    hb_mask_t mask;

    static int cmp (const lookup_map_t *a, const lookup_map_t *b)
    {
      unsigned int a_key = (a->priority << 16) + a->index;
      unsigned int b_key = (b->priority << 16) + b->index;
      return a_key < b_key ? -1 : a_key > b_key ? 1 : 0;
    }
  };

  HB_INTERNAL void add_lookups (hb_face_t    *face,
				unsigned int  table_index,
				unsigned int  feature_index,
				unsigned short priority,
				hb_mask_t     mask);


  public:

  hb_ot_map_t (void) : feature_count (0) {}

  void add_feature (hb_tag_t tag, unsigned int value, unsigned short priority, bool global)
  {
    feature_info_t *info = &feature_infos[feature_count++];
    info->tag = tag;
    info->seq = feature_count;
    info->max_value = value;
    info->global = global;
    info->default_value = global ? value : 0;
    info->priority = priority;
  }

  inline void add_bool_feature (hb_tag_t tag, unsigned short priority, bool global = true)
  { add_feature (tag, 1, priority, global); }

  HB_INTERNAL void compile (hb_face_t *face,
			    const hb_segment_properties_t *props);

  inline hb_mask_t get_global_mask (void) const { return global_mask; }

  inline hb_mask_t get_mask (hb_tag_t tag, unsigned short *shift = NULL) const {
    const feature_map_t *map = (const feature_map_t *) bsearch (&tag, feature_maps, feature_count, sizeof (feature_maps[0]), (hb_compare_func_t) feature_map_t::cmp);
    if (shift) *shift = map ? map->shift : 0;
    return map ? map->mask : 0;
  }

  inline hb_mask_t get_1_mask (hb_tag_t tag) const {
    const feature_map_t *map = (const feature_map_t *) bsearch (&tag, feature_maps, feature_count, sizeof (feature_maps[0]), (hb_compare_func_t) feature_map_t::cmp);
    return map ? map->_1_mask : 0;
  }

  inline void substitute (hb_face_t *face, hb_buffer_t *buffer) const {
    for (unsigned int i = 0; i < lookup_count[0]; i++)
      hb_ot_layout_substitute_lookup (face, buffer, lookup_maps[0][i].index, lookup_maps[0][i].mask);
  }

  inline void position (hb_font_t *font, hb_face_t *face, hb_buffer_t *buffer) const {
    for (unsigned int i = 0; i < lookup_count[1]; i++)
      hb_ot_layout_position_lookup (font, face, buffer, lookup_maps[1][i].index, lookup_maps[1][i].mask);
  }

  private:

  hb_mask_t global_mask;

  unsigned int feature_count;
  feature_info_t feature_infos[MAX_FEATURES]; /* used before compile() only */
  feature_map_t feature_maps[MAX_FEATURES];

  lookup_map_t lookup_maps[2][MAX_LOOKUPS]; /* GSUB/GPOS */
  unsigned int lookup_count[2];
};



HB_END_DECLS

#endif /* HB_OT_MAP_PRIVATE_HH */
