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

#include "hb-ot-map-private.hh"


void
hb_ot_map_t::add_lookups (hb_face_t    *face,
			  unsigned int  table_index,
			  unsigned int  feature_index,
			  hb_mask_t     mask)
{
  unsigned int lookup_indices[32];
  unsigned int offset, len;

  offset = 0;
  do {
    len = ARRAY_LENGTH (lookup_indices);
    hb_ot_layout_feature_get_lookup_indexes (face,
					     table_tags[table_index],
					     feature_index,
					     offset, &len,
					     lookup_indices);

    for (unsigned int i = 0; i < len; i++) {
      hb_ot_map_t::lookup_map_t *lookup = lookups[table_index].push ();
      if (unlikely (!lookup))
        return;
      lookup->mask = mask;
      lookup->index = lookup_indices[i];
    }

    offset += len;
  } while (len == ARRAY_LENGTH (lookup_indices));
}


void hb_ot_map_builder_t::add_feature (hb_tag_t tag, unsigned int value, bool global)
{
  feature_info_t *info = feature_infos.push();
  if (unlikely (!info)) return;
  info->tag = tag;
  info->seq = feature_infos.len;
  info->max_value = value;
  info->global = global;
  info->default_value = global ? value : 0;
  info->stage[0] = current_stage[0];
  info->stage[1] = current_stage[1];
}

/* Keep the next two functions in sync. */

void hb_ot_map_t::substitute (const hb_ot_shape_plan_t *plan, hb_font_t *font, hb_buffer_t *buffer) const
{
  const unsigned int table_index = 0;
  unsigned int i = 0;

  for (unsigned int pause_index = 0; pause_index < pauses[table_index].len; pause_index++) {
    const pause_map_t *pause = &pauses[table_index][pause_index];
    for (; i < pause->num_lookups; i++)
      hb_ot_layout_substitute_lookup (font, buffer, lookups[table_index][i].index, lookups[table_index][i].mask);

    buffer->clear_output ();

    if (pause->callback)
      pause->callback (plan, font, buffer);
  }

  for (; i < lookups[table_index].len; i++)
    hb_ot_layout_substitute_lookup (font, buffer, lookups[table_index][i].index, lookups[table_index][i].mask);
}

void hb_ot_map_t::position (const hb_ot_shape_plan_t *plan, hb_font_t *font, hb_buffer_t *buffer) const
{
  const unsigned int table_index = 1;
  unsigned int i = 0;

  for (unsigned int pause_index = 0; pause_index < pauses[table_index].len; pause_index++) {
    const pause_map_t *pause = &pauses[table_index][pause_index];
    for (; i < pause->num_lookups; i++)
      hb_ot_layout_position_lookup (font, buffer, lookups[table_index][i].index, lookups[table_index][i].mask);

    if (pause->callback)
      pause->callback (plan, font, buffer);
  }

  for (; i < lookups[table_index].len; i++)
    hb_ot_layout_position_lookup (font, buffer, lookups[table_index][i].index, lookups[table_index][i].mask);
}

void hb_ot_map_t::substitute_closure (const hb_ot_shape_plan_t *plan, hb_face_t *face, hb_set_t *glyphs) const
{
  unsigned int table_index = 0;
  unsigned int i = 0;

  for (unsigned int pause_index = 0; pause_index < pauses[table_index].len; pause_index++) {
    const pause_map_t *pause = &pauses[table_index][pause_index];
    for (; i < pause->num_lookups; i++)
      hb_ot_layout_substitute_closure_lookup (face, glyphs, lookups[table_index][i].index);
  }

  for (; i < lookups[table_index].len; i++)
    hb_ot_layout_substitute_closure_lookup (face, glyphs, lookups[table_index][i].index);
}

void hb_ot_map_builder_t::add_pause (unsigned int table_index, hb_ot_map_t::pause_func_t pause_func)
{
  pause_info_t *p = pauses[table_index].push ();
  if (likely (p)) {
    p->stage = current_stage[table_index];
    p->callback = pause_func;
  }

  current_stage[table_index]++;
}

void
hb_ot_map_builder_t::compile (hb_face_t *face,
			      const hb_segment_properties_t *props,
			      hb_ot_map_t &m)
{
 m.global_mask = 1;

  if (!feature_infos.len)
    return;


  /* Fetch script/language indices for GSUB/GPOS.  We need these later to skip
   * features not available in either table and not waste precious bits for them. */

  hb_tag_t script_tags[3] = {HB_TAG_NONE};
  hb_tag_t language_tag;

  hb_ot_tags_from_script (props->script, &script_tags[0], &script_tags[1]);
  language_tag = hb_ot_tag_from_language (props->language);

  unsigned int script_index[2], language_index[2];
  for (unsigned int table_index = 0; table_index < 2; table_index++) {
    hb_tag_t table_tag = table_tags[table_index];
    hb_ot_layout_table_choose_script (face, table_tag, script_tags, &script_index[table_index], &m.chosen_script[table_index]);
    hb_ot_layout_script_find_language (face, table_tag, script_index[table_index], language_tag, &language_index[table_index]);
  }


  /* Sort features and merge duplicates */
  {
    feature_infos.sort ();
    unsigned int j = 0;
    for (unsigned int i = 1; i < feature_infos.len; i++)
      if (feature_infos[i].tag != feature_infos[j].tag)
	feature_infos[++j] = feature_infos[i];
      else {
	if (feature_infos[i].global) {
	  feature_infos[j].global = true;
	  feature_infos[j].max_value = feature_infos[i].max_value;
	  feature_infos[j].default_value = feature_infos[i].default_value;
	} else {
	  feature_infos[j].global = false;
	  feature_infos[j].max_value = MAX (feature_infos[j].max_value, feature_infos[i].max_value);
	}
	feature_infos[j].stage[0] = MIN (feature_infos[j].stage[0], feature_infos[i].stage[0]);
	feature_infos[j].stage[1] = MIN (feature_infos[j].stage[1], feature_infos[i].stage[1]);
	/* Inherit default_value from j */
      }
    feature_infos.shrink (j + 1);
  }


  /* Allocate bits now */
  unsigned int next_bit = 1;
  for (unsigned int i = 0; i < feature_infos.len; i++) {
    const feature_info_t *info = &feature_infos[i];

    unsigned int bits_needed;

    if (info->global && info->max_value == 1)
      /* Uses the global bit */
      bits_needed = 0;
    else
      bits_needed = _hb_bit_storage (info->max_value);

    if (!info->max_value || next_bit + bits_needed > 8 * sizeof (hb_mask_t))
      continue; /* Feature disabled, or not enough bits. */


    bool found = false;
    unsigned int feature_index[2];
    for (unsigned int table_index = 0; table_index < 2; table_index++)
      found |= hb_ot_layout_language_find_feature (face,
						   table_tags[table_index],
						   script_index[table_index],
						   language_index[table_index],
						   info->tag,
						   &feature_index[table_index]);
    if (!found)
      continue;


    hb_ot_map_t::feature_map_t *map = m.features.push ();
    if (unlikely (!map))
      break;

    map->tag = info->tag;
    map->index[0] = feature_index[0];
    map->index[1] = feature_index[1];
    map->stage[0] = info->stage[0];
    map->stage[1] = info->stage[1];
    if (info->global && info->max_value == 1) {
      /* Uses the global bit */
      map->shift = 0;
      map->mask = 1;
    } else {
      map->shift = next_bit;
      map->mask = (1 << (next_bit + bits_needed)) - (1 << next_bit);
      next_bit += bits_needed;
      if (info->global)
	m.global_mask |= (info->default_value << map->shift) & map->mask;
    }
    map->_1_mask = (1 << map->shift) & map->mask;

  }
  feature_infos.shrink (0); /* Done with these */


  add_gsub_pause (NULL);
  add_gpos_pause (NULL);

  for (unsigned int table_index = 0; table_index < 2; table_index++) {
    hb_tag_t table_tag = table_tags[table_index];

    /* Collect lookup indices for features */

    unsigned int required_feature_index;
    if (hb_ot_layout_language_get_required_feature_index (face,
							  table_tag,
							  script_index[table_index],
							  language_index[table_index],
							  &required_feature_index))
      m.add_lookups (face, table_index, required_feature_index, 1);

    unsigned int pause_index = 0;
    unsigned int last_num_lookups = 0;
    for (unsigned stage = 0; stage < current_stage[table_index]; stage++)
    {
      for (unsigned i = 0; i < m.features.len; i++)
        if (m.features[i].stage[table_index] == stage)
	  m.add_lookups (face, table_index, m.features[i].index[table_index], m.features[i].mask);

      /* Sort lookups and merge duplicates */
      if (last_num_lookups < m.lookups[table_index].len)
      {
	m.lookups[table_index].sort (last_num_lookups, m.lookups[table_index].len);

	unsigned int j = last_num_lookups;
	for (unsigned int i = j + 1; i < m.lookups[table_index].len; i++)
	  if (m.lookups[table_index][i].index != m.lookups[table_index][j].index)
	    m.lookups[table_index][++j] = m.lookups[table_index][i];
	  else
	    m.lookups[table_index][j].mask |= m.lookups[table_index][i].mask;
	m.lookups[table_index].shrink (j + 1);
      }

      last_num_lookups = m.lookups[table_index].len;

      if (pause_index < pauses[table_index].len && pauses[table_index][pause_index].stage == stage) {
	hb_ot_map_t::pause_map_t *pause_map = m.pauses[table_index].push ();
	if (likely (pause_map)) {
	  pause_map->num_lookups = last_num_lookups;
	  pause_map->callback = pauses[table_index][pause_index].callback;
	}

	pause_index++;
      }
    }
  }
}
