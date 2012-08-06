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
#include "hb-ot-shape-private.hh"



/* buffer var allocations */
#define arabic_shaping_action() complex_var_temporary_u8() /* arabic shaping action */


/*
 * Bits used in the joining tables
 */
enum {
  JOINING_TYPE_U		= 0,
  JOINING_TYPE_R		= 1,
  JOINING_TYPE_D		= 2,
  JOINING_TYPE_C		= JOINING_TYPE_D,
  JOINING_GROUP_ALAPH		= 3,
  JOINING_GROUP_DALATH_RISH	= 4,
  NUM_STATE_MACHINE_COLS	= 5,

  /* We deliberately don't have a JOINING_TYPE_L since that's unused in Unicode. */

  JOINING_TYPE_T = 6,
  JOINING_TYPE_X = 7  /* means: use general-category to choose between U or T. */
};

/*
 * Joining types:
 */

#include "hb-ot-shape-complex-arabic-table.hh"

static unsigned int get_joining_type (hb_codepoint_t u, hb_unicode_general_category_t gen_cat)
{
  if (likely (hb_in_range<hb_codepoint_t> (u, JOINING_TABLE_FIRST, JOINING_TABLE_LAST))) {
    unsigned int j_type = joining_table[u - JOINING_TABLE_FIRST];
    if (likely (j_type != JOINING_TYPE_X))
      return j_type;
  }

  /* Mongolian joining data is not in ArabicJoining.txt yet */
  if (unlikely (hb_in_range<hb_codepoint_t> (u, 0x1800, 0x18AF)))
  {
    /* All letters, SIBE SYLLABLE BOUNDARY MARKER, and NIRUGU are D */
    if (gen_cat == HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER || u == 0x1807 || u == 0x180A)
      return JOINING_TYPE_D;
  }

  if (unlikely (hb_in_range<hb_codepoint_t> (u, 0x200C, 0x200D))) {
    return u == 0x200C ? JOINING_TYPE_U : JOINING_TYPE_C;
  }

  return (FLAG(gen_cat) & (FLAG(HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK) | FLAG(HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK) | FLAG(HB_UNICODE_GENERAL_CATEGORY_FORMAT))) ?
	 JOINING_TYPE_T : JOINING_TYPE_U;
}

static hb_codepoint_t get_arabic_shape (hb_codepoint_t u, unsigned int shape)
{
  if (likely (hb_in_range<hb_codepoint_t> (u, SHAPING_TABLE_FIRST, SHAPING_TABLE_LAST)) && shape < 4)
    return shaping_table[u - SHAPING_TABLE_FIRST][shape];
  return u;
}

static uint16_t get_ligature (hb_codepoint_t first, hb_codepoint_t second)
{
  if (unlikely (!second)) return 0;
  for (unsigned i = 0; i < ARRAY_LENGTH (ligature_table); i++)
    if (ligature_table[i].first == first)
      for (unsigned j = 0; j < ARRAY_LENGTH (ligature_table[i].ligatures); j++)
	if (ligature_table[i].ligatures[j].second == second)
	  return ligature_table[i].ligatures[j].ligature;
  return 0;
}

static const hb_tag_t arabic_syriac_features[] =
{
  HB_TAG('i','n','i','t'),
  HB_TAG('m','e','d','i'),
  HB_TAG('f','i','n','a'),
  HB_TAG('i','s','o','l'),
  /* Syriac */
  HB_TAG('m','e','d','2'),
  HB_TAG('f','i','n','2'),
  HB_TAG('f','i','n','3'),
  HB_TAG_NONE
};


/* Same order as the feature array */
enum {
  INIT,
  MEDI,
  FINA,
  ISOL,

  /* Syriac */
  MED2,
  FIN2,
  FIN3,

  NONE,

  COMMON_NUM_FEATURES = 4,
  SYRIAC_NUM_FEATURES = 7,
  TOTAL_NUM_FEATURES = NONE
};

static const struct arabic_state_table_entry {
	uint8_t prev_action;
	uint8_t curr_action;
	uint16_t next_state;
} arabic_state_table[][NUM_STATE_MACHINE_COLS] =
{
  /*   jt_U,          jt_R,          jt_D,          jg_ALAPH,      jg_DALATH_RISH */

  /* State 0: prev was U, not willing to join. */
  { {NONE,NONE,0}, {NONE,ISOL,1}, {NONE,ISOL,2}, {NONE,ISOL,1}, {NONE,ISOL,6}, },

  /* State 1: prev was R or ISOL/ALAPH, not willing to join. */
  { {NONE,NONE,0}, {NONE,ISOL,1}, {NONE,ISOL,2}, {NONE,FIN2,5}, {NONE,ISOL,6}, },

  /* State 2: prev was D/ISOL, willing to join. */
  { {NONE,NONE,0}, {INIT,FINA,1}, {INIT,FINA,3}, {INIT,FINA,4}, {INIT,FINA,6}, },

  /* State 3: prev was D/FINA, willing to join. */
  { {NONE,NONE,0}, {MEDI,FINA,1}, {MEDI,FINA,3}, {MEDI,FINA,4}, {MEDI,FINA,6}, },

  /* State 4: prev was FINA ALAPH, not willing to join. */
  { {NONE,NONE,0}, {MED2,ISOL,1}, {MED2,ISOL,2}, {MED2,FIN2,5}, {MED2,ISOL,6}, },

  /* State 5: prev was FIN2/FIN3 ALAPH, not willing to join. */
  { {NONE,NONE,0}, {ISOL,ISOL,1}, {ISOL,ISOL,2}, {ISOL,FIN2,5}, {ISOL,ISOL,6}, },

  /* State 6: prev was DALATH/RISH, not willing to join. */
  { {NONE,NONE,0}, {NONE,ISOL,1}, {NONE,ISOL,2}, {NONE,FIN3,5}, {NONE,ISOL,6}, }
};



static void
collect_features_arabic (hb_ot_shape_planner_t *plan)
{
  hb_ot_map_builder_t *map = &plan->map;

  /* For Language forms (in ArabicOT speak), we do the iso/fina/medi/init together,
   * then rlig and calt each in their own stage.  This makes IranNastaliq's ALLAH
   * ligature work correctly. It's unfortunate though...
   *
   * This also makes Arial Bold in Windows7 work.  See:
   * https://bugzilla.mozilla.org/show_bug.cgi?id=644184
   *
   * TODO: Add test cases for these two.
   */

  map->add_bool_feature (HB_TAG('c','c','m','p'));
  map->add_bool_feature (HB_TAG('l','o','c','l'));

  map->add_gsub_pause (NULL);

  unsigned int num_features = plan->props.script == HB_SCRIPT_SYRIAC ? SYRIAC_NUM_FEATURES : COMMON_NUM_FEATURES;
  for (unsigned int i = 0; i < num_features; i++)
    map->add_bool_feature (arabic_syriac_features[i], false);

  map->add_gsub_pause (NULL);

  map->add_bool_feature (HB_TAG('r','l','i','g'));
  map->add_gsub_pause (NULL);

  map->add_bool_feature (HB_TAG('c','a','l','t'));
  map->add_gsub_pause (NULL);

  /* ArabicOT spec enables 'cswh' for Arabic where as for basic shaper it's disabled by default. */
  map->add_bool_feature (HB_TAG('c','s','w','h'));
}


static void
arabic_fallback_shape (hb_font_t *font, hb_buffer_t *buffer)
{
  unsigned int count = buffer->len;
  hb_codepoint_t glyph;

  /* Shape to presentation forms */
  for (unsigned int i = 0; i < count; i++) {
    hb_codepoint_t u = buffer->info[i].codepoint;
    hb_codepoint_t shaped = get_arabic_shape (u, buffer->info[i].arabic_shaping_action());
    if (shaped != u && font->get_glyph (shaped, 0, &glyph))
      buffer->info[i].codepoint = shaped;
  }

  /* Mandatory ligatures */
  buffer->clear_output ();
  for (buffer->idx = 0; buffer->idx + 1 < count;) {
    hb_codepoint_t ligature = get_ligature (buffer->cur().codepoint,
					    buffer->cur(+1).codepoint);
    if (likely (!ligature) || !(font->get_glyph (ligature, 0, &glyph))) {
      buffer->next_glyph ();
      continue;
    }

    buffer->replace_glyphs (2, 1, &ligature);

    /* Technically speaking we can skip marks and stuff, like the GSUB path does.
     * But who cares, we're in fallback! */
  }
  for (; buffer->idx < count;)
      buffer->next_glyph ();
  buffer->swap_buffers ();
}

static void
setup_masks_arabic (const hb_ot_shape_plan_t *plan,
		    hb_buffer_t              *buffer,
		    hb_font_t                *font)
{
  unsigned int count = buffer->len;
  unsigned int prev = 0, state = 0;

  HB_BUFFER_ALLOCATE_VAR (buffer, arabic_shaping_action);

  for (unsigned int i = 0; i < count; i++)
  {
    unsigned int this_type = get_joining_type (buffer->info[i].codepoint, _hb_glyph_info_get_general_category (&buffer->info[i]));

    if (unlikely (this_type == JOINING_TYPE_T)) {
      buffer->info[i].arabic_shaping_action() = NONE;
      continue;
    }

    const arabic_state_table_entry *entry = &arabic_state_table[state][this_type];

    if (entry->prev_action != NONE)
      buffer->info[prev].arabic_shaping_action() = entry->prev_action;

    buffer->info[i].arabic_shaping_action() = entry->curr_action;

    prev = i;
    state = entry->next_state;
  }

  hb_mask_t mask_array[TOTAL_NUM_FEATURES + 1] = {0};
  hb_mask_t total_masks = 0;
  unsigned int num_masks = buffer->props.script == HB_SCRIPT_SYRIAC ? SYRIAC_NUM_FEATURES : COMMON_NUM_FEATURES;
  for (unsigned int i = 0; i < num_masks; i++) {
    mask_array[i] = plan->map.get_1_mask (arabic_syriac_features[i]);
    total_masks |= mask_array[i];
  }

  if (total_masks) {
    /* Has OpenType tables */
    for (unsigned int i = 0; i < count; i++)
      buffer->info[i].mask |= mask_array[buffer->info[i].arabic_shaping_action()];
  } else if (buffer->props.script == HB_SCRIPT_ARABIC) {
    /* Fallback Arabic shaping to Presentation Forms */
    /* Pitfalls:
     * - This path fires if user force-set init/medi/fina/isol off,
     * - If font does not declare script 'arab', well, what to do?
     *   Most probably it's safe to assume that init/medi/fina/isol
     *   still mean Arabic shaping, although they do not have to.
     */
    arabic_fallback_shape (font, buffer);
  }

  HB_BUFFER_DEALLOCATE_VAR (buffer, arabic_shaping_action);
}

const hb_ot_complex_shaper_t _hb_ot_complex_shaper_arabic =
{
  "arabic",
  collect_features_arabic,
  NULL, /* override_features */
  NULL, /* data_create */
  NULL, /* data_destroy */
  NULL, /* normalization_preference */
  setup_masks_arabic,
  true, /* zero_width_attached_marks */
};
