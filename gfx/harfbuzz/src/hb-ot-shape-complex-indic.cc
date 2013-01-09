/*
 * Copyright © 2011,2012  Google, Inc.
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

#include "hb-ot-shape-complex-indic-private.hh"
#include "hb-ot-layout-private.hh"


/*
 * Global Indic shaper options.
 */

struct indic_options_t
{
  int initialized : 1;
  int uniscribe_bug_compatible : 1;
};

union indic_options_union_t {
  int i;
  indic_options_t opts;
};
ASSERT_STATIC (sizeof (int) == sizeof (indic_options_union_t));

static indic_options_union_t
indic_options_init (void)
{
  indic_options_union_t u;
  u.i = 0;
  u.opts.initialized = 1;

  char *c = getenv ("HB_OT_INDIC_OPTIONS");
  u.opts.uniscribe_bug_compatible = c && strstr (c, "uniscribe-bug-compatible");

  return u;
}

static inline indic_options_t
indic_options (void)
{
  static indic_options_union_t options;

  if (unlikely (!options.i)) {
    /* This is idempotent and threadsafe. */
    options = indic_options_init ();
  }

  return options.opts;
}


/*
 * Indic configurations.  Note that we do not want to keep every single script-specific
 * behavior in these tables necessarily.  This should mainly be used for per-script
 * properties that are cheaper keeping here, than in the code.  Ie. if, say, one and
 * only one script has an exception, that one script can be if'ed directly in the code,
 * instead of adding a new flag in these structs.
 */

enum base_position_t {
  BASE_POS_FIRST,
  BASE_POS_LAST
};
enum reph_position_t {
  REPH_POS_DEFAULT     = POS_BEFORE_POST,

  REPH_POS_AFTER_MAIN  = POS_AFTER_MAIN,
  REPH_POS_BEFORE_SUB  = POS_BEFORE_SUB,
  REPH_POS_AFTER_SUB   = POS_AFTER_SUB,
  REPH_POS_BEFORE_POST = POS_BEFORE_POST,
  REPH_POS_AFTER_POST  = POS_AFTER_POST
};
enum reph_mode_t {
  REPH_MODE_IMPLICIT,  /* Reph formed out of initial Ra,H sequence. */
  REPH_MODE_EXPLICIT,  /* Reph formed out of initial Ra,H,ZWJ sequence. */
  REPH_MODE_VIS_REPHA, /* Encoded Repha character, no reordering needed. */
  REPH_MODE_LOG_REPHA  /* Encoded Repha character, needs reordering. */
};
struct indic_config_t
{
  hb_script_t     script;
  bool            has_old_spec;
  hb_codepoint_t  virama;
  base_position_t base_pos;
  reph_position_t reph_pos;
  reph_mode_t     reph_mode;
};

static const indic_config_t indic_configs[] =
{
  /* Default.  Should be first. */
  {HB_SCRIPT_INVALID,	false,     0,BASE_POS_LAST, REPH_POS_DEFAULT,    REPH_MODE_IMPLICIT},
  {HB_SCRIPT_DEVANAGARI,true, 0x094D,BASE_POS_LAST, REPH_POS_BEFORE_POST,REPH_MODE_IMPLICIT},
  {HB_SCRIPT_BENGALI,	true, 0x09CD,BASE_POS_LAST, REPH_POS_AFTER_SUB,  REPH_MODE_IMPLICIT},
  {HB_SCRIPT_GURMUKHI,	true, 0x0A4D,BASE_POS_LAST, REPH_POS_BEFORE_SUB, REPH_MODE_IMPLICIT},
  {HB_SCRIPT_GUJARATI,	true, 0x0ACD,BASE_POS_LAST, REPH_POS_BEFORE_POST,REPH_MODE_IMPLICIT},
  {HB_SCRIPT_ORIYA,	true, 0x0B4D,BASE_POS_LAST, REPH_POS_AFTER_MAIN, REPH_MODE_IMPLICIT},
  {HB_SCRIPT_TAMIL,	true, 0x0BCD,BASE_POS_LAST, REPH_POS_AFTER_POST, REPH_MODE_IMPLICIT},
  {HB_SCRIPT_TELUGU,	true, 0x0C4D,BASE_POS_LAST, REPH_POS_AFTER_POST, REPH_MODE_EXPLICIT},
  {HB_SCRIPT_KANNADA,	true, 0x0CCD,BASE_POS_LAST, REPH_POS_AFTER_POST, REPH_MODE_IMPLICIT},
  {HB_SCRIPT_MALAYALAM,	true, 0x0D4D,BASE_POS_LAST, REPH_POS_AFTER_MAIN, REPH_MODE_LOG_REPHA},
  {HB_SCRIPT_SINHALA,	false,0x0DCA,BASE_POS_FIRST,REPH_POS_AFTER_MAIN, REPH_MODE_EXPLICIT},
  {HB_SCRIPT_KHMER,	false,0x17D2,BASE_POS_FIRST,REPH_POS_DEFAULT,    REPH_MODE_VIS_REPHA},
  /* Myanmar does not have the "old_indic" behavior, even though it has a "new" tag. */
  {HB_SCRIPT_MYANMAR,	false,0x1039,BASE_POS_LAST, REPH_POS_DEFAULT,    REPH_MODE_EXPLICIT},
};



/*
 * Indic shaper.
 */

struct feature_list_t {
  hb_tag_t tag;
  hb_bool_t is_global;
};

static const feature_list_t
indic_features[] =
{
  /*
   * Basic features.
   * These features are applied in order, one at a time, after initial_reordering.
   */
  {HB_TAG('n','u','k','t'), true},
  {HB_TAG('a','k','h','n'), true},
  {HB_TAG('r','p','h','f'), false},
  {HB_TAG('r','k','r','f'), true},
  {HB_TAG('p','r','e','f'), false},
  {HB_TAG('h','a','l','f'), false},
  {HB_TAG('b','l','w','f'), false},
  {HB_TAG('a','b','v','f'), false},
  {HB_TAG('p','s','t','f'), false},
  {HB_TAG('c','f','a','r'), false},
  {HB_TAG('c','j','c','t'), true},
  {HB_TAG('v','a','t','u'), true},
  /*
   * Other features.
   * These features are applied all at once, after final_reordering.
   */
  {HB_TAG('i','n','i','t'), false},
  {HB_TAG('p','r','e','s'), true},
  {HB_TAG('a','b','v','s'), true},
  {HB_TAG('b','l','w','s'), true},
  {HB_TAG('p','s','t','s'), true},
  {HB_TAG('h','a','l','n'), true},
  /* Positioning features, though we don't care about the types. */
  {HB_TAG('d','i','s','t'), true},
  {HB_TAG('a','b','v','m'), true},
  {HB_TAG('b','l','w','m'), true},
};

/*
 * Must be in the same order as the indic_features array.
 */
enum {
  _NUKT,
  _AKHN,
  RPHF,
  _RKRF,
  PREF,
  HALF,
  BLWF,
  ABVF,
  PSTF,
  CFAR,
  _CJCT,
  _VATU,

  INIT,
  _PRES,
  _ABVS,
  _BLWS,
  _PSTS,
  _HALN,
  _DIST,
  _ABVM,
  _BLWM,

  INDIC_NUM_FEATURES,
  INDIC_BASIC_FEATURES = INIT /* Don't forget to update this! */
};

static void
setup_syllables (const hb_ot_shape_plan_t *plan,
		 hb_font_t *font,
		 hb_buffer_t *buffer);
static void
initial_reordering (const hb_ot_shape_plan_t *plan,
		    hb_font_t *font,
		    hb_buffer_t *buffer);
static void
final_reordering (const hb_ot_shape_plan_t *plan,
		  hb_font_t *font,
		  hb_buffer_t *buffer);

static void
collect_features_indic (hb_ot_shape_planner_t *plan)
{
  hb_ot_map_builder_t *map = &plan->map;

  /* Do this before any lookups have been applied. */
  map->add_gsub_pause (setup_syllables);

  map->add_bool_feature (HB_TAG('l','o','c','l'));
  /* The Indic specs do not require ccmp, but we apply it here since if
   * there is a use of it, it's typically at the beginning. */
  map->add_bool_feature (HB_TAG('c','c','m','p'));


  unsigned int i = 0;
  map->add_gsub_pause (initial_reordering);
  for (; i < INDIC_BASIC_FEATURES; i++) {
    map->add_bool_feature (indic_features[i].tag, indic_features[i].is_global);
    map->add_gsub_pause (NULL);
  }
  map->add_gsub_pause (final_reordering);
  for (; i < INDIC_NUM_FEATURES; i++) {
    map->add_bool_feature (indic_features[i].tag, indic_features[i].is_global);
  }
}

static void
override_features_indic (hb_ot_shape_planner_t *plan)
{
  /* Uniscribe does not apply 'kern'. */
  if (indic_options ().uniscribe_bug_compatible)
    plan->map.add_feature (HB_TAG('k','e','r','n'), 0, true);

  plan->map.add_feature (HB_TAG('l','i','g','a'), 0, true);
}


struct would_substitute_feature_t
{
  inline void init (const hb_ot_map_t *map, hb_tag_t feature_tag)
  {
    map->get_stage_lookups (0/*GSUB*/,
			    map->get_feature_stage (0/*GSUB*/, feature_tag),
			    &lookups, &count);
  }

  inline bool would_substitute (hb_codepoint_t    *glyphs,
				unsigned int       glyphs_count,
				bool               zero_context,
				hb_face_t         *face) const
  {
    for (unsigned int i = 0; i < count; i++)
      if (hb_ot_layout_lookup_would_substitute_fast (face, lookups[i].index, glyphs, glyphs_count, zero_context))
	return true;
    return false;
  }

  private:
  const hb_ot_map_t::lookup_map_t *lookups;
  unsigned int count;
};

struct indic_shape_plan_t
{
  ASSERT_POD ();

  inline bool get_virama_glyph (hb_font_t *font, hb_codepoint_t *pglyph) const
  {
    hb_codepoint_t glyph = virama_glyph;
    if (unlikely (virama_glyph == (hb_codepoint_t) -1))
    {
      if (!config->virama || !font->get_glyph (config->virama, 0, &glyph))
	glyph = 0;
      /* Technically speaking, the spec says we should apply 'locl' to virama too.
       * Maybe one day... */

      /* Our get_glyph() function needs a font, so we can't get the virama glyph
       * during shape planning...  Instead, overwrite it here.  It's safe.  Don't worry! */
      (const_cast<indic_shape_plan_t *> (this))->virama_glyph = glyph;
    }

    *pglyph = glyph;
    return glyph != 0;
  }

  const indic_config_t *config;

  bool is_old_spec;
  hb_codepoint_t virama_glyph;

  would_substitute_feature_t rphf;
  would_substitute_feature_t pref;
  would_substitute_feature_t blwf;
  would_substitute_feature_t pstf;

  hb_mask_t mask_array[INDIC_NUM_FEATURES];
};

static void *
data_create_indic (const hb_ot_shape_plan_t *plan)
{
  indic_shape_plan_t *indic_plan = (indic_shape_plan_t *) calloc (1, sizeof (indic_shape_plan_t));
  if (unlikely (!indic_plan))
    return NULL;

  indic_plan->config = &indic_configs[0];
  for (unsigned int i = 1; i < ARRAY_LENGTH (indic_configs); i++)
    if (plan->props.script == indic_configs[i].script) {
      indic_plan->config = &indic_configs[i];
      break;
    }

  indic_plan->is_old_spec = indic_plan->config->has_old_spec && ((plan->map.chosen_script[0] & 0x000000FF) != '2');
  indic_plan->virama_glyph = (hb_codepoint_t) -1;

  indic_plan->rphf.init (&plan->map, HB_TAG('r','p','h','f'));
  indic_plan->pref.init (&plan->map, HB_TAG('p','r','e','f'));
  indic_plan->blwf.init (&plan->map, HB_TAG('b','l','w','f'));
  indic_plan->pstf.init (&plan->map, HB_TAG('p','s','t','f'));

  for (unsigned int i = 0; i < ARRAY_LENGTH (indic_plan->mask_array); i++)
    indic_plan->mask_array[i] = indic_features[i].is_global ? 0 : plan->map.get_1_mask (indic_features[i].tag);

  return indic_plan;
}

static void
data_destroy_indic (void *data)
{
  free (data);
}

static indic_position_t
consonant_position_from_face (const indic_shape_plan_t *indic_plan,
			      hb_codepoint_t *glyphs, unsigned int glyphs_len,
			      hb_face_t      *face)
{
  bool zero_context = indic_plan->is_old_spec ? false : true;
  if (indic_plan->pref.would_substitute (glyphs, glyphs_len, zero_context, face)) return POS_BELOW_C;
  if (indic_plan->blwf.would_substitute (glyphs, glyphs_len, zero_context, face)) return POS_BELOW_C;
  if (indic_plan->pstf.would_substitute (glyphs, glyphs_len, zero_context, face)) return POS_POST_C;
  return POS_BASE_C;
}


enum syllable_type_t {
  consonant_syllable,
  vowel_syllable,
  standalone_cluster,
  broken_cluster,
  non_indic_cluster,
};

#include "hb-ot-shape-complex-indic-machine.hh"


static void
setup_masks_indic (const hb_ot_shape_plan_t *plan HB_UNUSED,
		   hb_buffer_t              *buffer,
		   hb_font_t                *font HB_UNUSED)
{
  HB_BUFFER_ALLOCATE_VAR (buffer, indic_category);
  HB_BUFFER_ALLOCATE_VAR (buffer, indic_position);

  /* We cannot setup masks here.  We save information about characters
   * and setup masks later on in a pause-callback. */

  unsigned int count = buffer->len;
  for (unsigned int i = 0; i < count; i++)
    set_indic_properties (buffer->info[i]);
}

static void
setup_syllables (const hb_ot_shape_plan_t *plan HB_UNUSED,
		 hb_font_t *font HB_UNUSED,
		 hb_buffer_t *buffer)
{
  find_syllables (buffer);
}

static int
compare_indic_order (const hb_glyph_info_t *pa, const hb_glyph_info_t *pb)
{
  int a = pa->indic_position();
  int b = pb->indic_position();

  return a < b ? -1 : a == b ? 0 : +1;
}



static void
update_consonant_positions (const hb_ot_shape_plan_t *plan,
			    hb_font_t         *font,
			    hb_buffer_t       *buffer)
{
  const indic_shape_plan_t *indic_plan = (const indic_shape_plan_t *) plan->data;

  unsigned int consonant_pos = indic_plan->is_old_spec ? 0 : 1;
  hb_codepoint_t glyphs[2];
  if (indic_plan->get_virama_glyph (font, &glyphs[1 - consonant_pos]))
  {
    hb_face_t *face = font->face;
    unsigned int count = buffer->len;
    for (unsigned int i = 0; i < count; i++)
      if (buffer->info[i].indic_position() == POS_BASE_C) {
	glyphs[consonant_pos] = buffer->info[i].codepoint;
	buffer->info[i].indic_position() = consonant_position_from_face (indic_plan, glyphs, 2, face);
      }
  }
}


/* Rules from:
 * https://www.microsoft.com/typography/otfntdev/devanot/shaping.aspx */

static void
initial_reordering_consonant_syllable (const hb_ot_shape_plan_t *plan,
				       hb_face_t *face,
				       hb_buffer_t *buffer,
				       unsigned int start, unsigned int end)
{
  const indic_shape_plan_t *indic_plan = (const indic_shape_plan_t *) plan->data;
  hb_glyph_info_t *info = buffer->info;


  /* 1. Find base consonant:
   *
   * The shaping engine finds the base consonant of the syllable, using the
   * following algorithm: starting from the end of the syllable, move backwards
   * until a consonant is found that does not have a below-base or post-base
   * form (post-base forms have to follow below-base forms), or that is not a
   * pre-base reordering Ra, or arrive at the first consonant. The consonant
   * stopped at will be the base.
   *
   *   o If the syllable starts with Ra + Halant (in a script that has Reph)
   *     and has more than one consonant, Ra is excluded from candidates for
   *     base consonants.
   */

  unsigned int base = end;
  bool has_reph = false;

  {
    /* -> If the syllable starts with Ra + Halant (in a script that has Reph)
     *    and has more than one consonant, Ra is excluded from candidates for
     *    base consonants. */
    unsigned int limit = start;
    if (indic_plan->mask_array[RPHF] &&
	start + 3 <= end &&
	(
	 (indic_plan->config->reph_mode == REPH_MODE_IMPLICIT && !is_joiner (info[start + 2])) ||
	 (indic_plan->config->reph_mode == REPH_MODE_EXPLICIT && info[start + 2].indic_category() == OT_ZWJ)
	))
    {
      /* See if it matches the 'rphf' feature. */
      hb_codepoint_t glyphs[2] = {info[start].codepoint, info[start + 1].codepoint};
      if (indic_plan->rphf.would_substitute (glyphs, ARRAY_LENGTH (glyphs), true, face))
      {
	limit += 2;
	while (limit < end && is_joiner (info[limit]))
	  limit++;
	base = start;
	has_reph = true;
      }
    } else if (indic_plan->config->reph_mode == REPH_MODE_LOG_REPHA && info[start].indic_category() == OT_Repha)
    {
	limit += 1;
	while (limit < end && is_joiner (info[limit]))
	  limit++;
	base = start;
	has_reph = true;
    }

    switch (indic_plan->config->base_pos)
    {
      default:
        assert (false);
	/* fallthrough */

      case BASE_POS_LAST:
      {
	/* -> starting from the end of the syllable, move backwards */
	unsigned int i = end;
	bool seen_below = false;
	do {
	  i--;
	  /* -> until a consonant is found */
	  if (is_consonant (info[i]))
	  {
	    /* -> that does not have a below-base or post-base form
	     * (post-base forms have to follow below-base forms), */
	    if (info[i].indic_position() != POS_BELOW_C &&
		(info[i].indic_position() != POS_POST_C || seen_below))
	    {
	      base = i;
	      break;
	    }
	    if (info[i].indic_position() == POS_BELOW_C)
	      seen_below = true;

	    /* -> or that is not a pre-base reordering Ra,
	     *
	     * IMPLEMENTATION NOTES:
	     *
	     * Our pre-base reordering Ra's are marked POS_BELOW, so will be skipped
	     * by the logic above already.
	     */

	    /* -> or arrive at the first consonant. The consonant stopped at will
	     * be the base. */
	    base = i;
	  }
	  else
	  {
	    /* A ZWJ after a Halant stops the base search, and requests an explicit
	     * half form.
	     * A ZWJ before a Halant, requests a subjoined form instead, and hence
	     * search continues.  This is particularly important for Bengali
	     * sequence Ra,H,Ya that should form Ya-Phalaa by subjoining Ya. */
	    if (start < i &&
		info[i].indic_category() == OT_ZWJ &&
		info[i - 1].indic_category() == OT_H)
	      break;
	  }
	} while (i > limit);
      }
      break;

      case BASE_POS_FIRST:
      {
	/* In scripts without half forms (eg. Khmer), the first consonant is always the base. */

	if (!has_reph)
	  base = limit;

	/* Find the last base consonant that is not blocked by ZWJ.  If there is
	 * a ZWJ right before a base consonant, that would request a subjoined form. */
	for (unsigned int i = limit; i < end; i++)
	  if (is_consonant (info[i]) && info[i].indic_position() == POS_BASE_C)
	  {
	    if (limit < i && info[i - 1].indic_category() == OT_ZWJ)
	      break;
	    else
	      base = i;
	  }

	/* Mark all subsequent consonants as below. */
	for (unsigned int i = base + 1; i < end; i++)
	  if (is_consonant (info[i]) && info[i].indic_position() == POS_BASE_C)
	    info[i].indic_position() = POS_BELOW_C;
      }
      break;
    }

    /* -> If the syllable starts with Ra + Halant (in a script that has Reph)
     *    and has more than one consonant, Ra is excluded from candidates for
     *    base consonants.
     *
     *  Only do this for unforced Reph. (ie. not for Ra,H,ZWJ. */
    if (has_reph && base == start && start - limit <= 2) {
      /* Have no other consonant, so Reph is not formed and Ra becomes base. */
      has_reph = false;
    }
  }

  if (base < end)
    info[base].indic_position() = POS_BASE_C;


  /* 2. Decompose and reorder Matras:
   *
   * Each matra and any syllable modifier sign in the cluster are moved to the
   * appropriate position relative to the consonant(s) in the cluster. The
   * shaping engine decomposes two- or three-part matras into their constituent
   * parts before any repositioning. Matra characters are classified by which
   * consonant in a conjunct they have affinity for and are reordered to the
   * following positions:
   *
   *   o Before first half form in the syllable
   *   o After subjoined consonants
   *   o After post-form consonant
   *   o After main consonant (for above marks)
   *
   * IMPLEMENTATION NOTES:
   *
   * The normalize() routine has already decomposed matras for us, so we don't
   * need to worry about that.
   */


  /* 3.  Reorder marks to canonical order:
   *
   * Adjacent nukta and halant or nukta and vedic sign are always repositioned
   * if necessary, so that the nukta is first.
   *
   * IMPLEMENTATION NOTES:
   *
   * We don't need to do this: the normalize() routine already did this for us.
   */


  /* Reorder characters */

  for (unsigned int i = start; i < base; i++)
    info[i].indic_position() = MIN (POS_PRE_C, (indic_position_t) info[i].indic_position());

  if (base < end)
    info[base].indic_position() = POS_BASE_C;

  /* Mark final consonants.  A final consonant is one appearing after a matra,
   * like in Khmer. */
  for (unsigned int i = base + 1; i < end; i++)
    if (info[i].indic_category() == OT_M) {
      for (unsigned int j = i + 1; j < end; j++)
        if (is_consonant (info[j])) {
	  info[j].indic_position() = POS_FINAL_C;
	  break;
	}
      break;
    }

  /* Handle beginning Ra */
  if (has_reph)
    info[start].indic_position() = POS_RA_TO_BECOME_REPH;

  /* For old-style Indic script tags, move the first post-base Halant after
   * last consonant. */
  if (indic_plan->is_old_spec) {
    for (unsigned int i = base + 1; i < end; i++)
      if (info[i].indic_category() == OT_H) {
        unsigned int j;
        for (j = end - 1; j > i; j--)
	  if (is_consonant (info[j]))
	    break;
	if (j > i) {
	  /* Move Halant to after last consonant. */
	  hb_glyph_info_t t = info[i];
	  memmove (&info[i], &info[i + 1], (j - i) * sizeof (info[0]));
	  info[j] = t;
	}
        break;
      }
  }

  /* Attach misc marks to previous char to move with them. */
  {
    indic_position_t last_pos = POS_START;
    for (unsigned int i = start; i < end; i++)
    {
      if ((FLAG (info[i].indic_category()) & (JOINER_FLAGS | FLAG (OT_N) | FLAG (OT_RS) | HALANT_OR_COENG_FLAGS)))
      {
	info[i].indic_position() = last_pos;
	if (unlikely (info[i].indic_category() == OT_H &&
		      info[i].indic_position() == POS_PRE_M))
	{
	  /*
	   * Uniscribe doesn't move the Halant with Left Matra.
	   * TEST: U+092B,U+093F,U+094DE
	   * We follow.  This is important for the Sinhala
	   * U+0DDA split matra since it decomposes to U+0DD9,U+0DCA
	   * where U+0DD9 is a left matra and U+0DCA is the virama.
	   * We don't want to move the virama with the left matra.
	   * TEST: U+0D9A,U+0DDA
	   */
	  for (unsigned int j = i; j > start; j--)
	    if (info[j - 1].indic_position() != POS_PRE_M) {
	      info[i].indic_position() = info[j - 1].indic_position();
	      break;
	    }
	}
      } else if (info[i].indic_position() != POS_SMVD) {
        last_pos = (indic_position_t) info[i].indic_position();
      }
    }
  }
  /* Re-attach ZWJ, ZWNJ, and halant to next char, for after-base consonants. */
  {
    unsigned int last_halant = end;
    for (unsigned int i = base + 1; i < end; i++)
      if (is_halant_or_coeng (info[i]))
        last_halant = i;
      else if (is_consonant (info[i])) {
	for (unsigned int j = last_halant; j < i; j++)
	  if (info[j].indic_position() != POS_SMVD)
	    info[j].indic_position() = info[i].indic_position();
      }
  }

  {
    /* Things are out-of-control for post base positions, they may shuffle
     * around like crazy, so merge clusters.  For pre-base stuff, we handle
     * cluster issues in final reordering. */
    buffer->merge_clusters (base, end);
    /* Sit tight, rock 'n roll! */
    hb_bubble_sort (info + start, end - start, compare_indic_order);
    /* Find base again */
    base = end;
    for (unsigned int i = start; i < end; i++)
      if (info[i].indic_position() == POS_BASE_C) {
        base = i;
	break;
      }
  }

  /* Setup masks now */

  {
    hb_mask_t mask;

    /* Reph */
    for (unsigned int i = start; i < end && info[i].indic_position() == POS_RA_TO_BECOME_REPH; i++)
      info[i].mask |= indic_plan->mask_array[RPHF];

    /* Pre-base */
    mask = indic_plan->mask_array[HALF];
    for (unsigned int i = start; i < base; i++)
      info[i].mask  |= mask;
    /* Base */
    mask = 0;
    if (base < end)
      info[base].mask |= mask;
    /* Post-base */
    mask = indic_plan->mask_array[BLWF] | indic_plan->mask_array[ABVF] | indic_plan->mask_array[PSTF];
    for (unsigned int i = base + 1; i < end; i++)
      info[i].mask  |= mask;
  }

  if (indic_plan->mask_array[PREF] && base + 2 < end)
  {
    /* Find a Halant,Ra sequence and mark it for pre-base reordering processing. */
    for (unsigned int i = base + 1; i + 1 < end; i++) {
      hb_codepoint_t glyphs[2] = {info[i].codepoint, info[i + 1].codepoint};
      if (indic_plan->pref.would_substitute (glyphs, ARRAY_LENGTH (glyphs), true, face))
      {
	info[i++].mask |= indic_plan->mask_array[PREF];
	info[i++].mask |= indic_plan->mask_array[PREF];

	/* Mark the subsequent stuff with 'cfar'.  Used in Khmer.
	 * Read the feature spec.
	 * This allows distinguishing the following cases with MS Khmer fonts:
	 * U+1784,U+17D2,U+179A,U+17D2,U+1782
	 * U+1784,U+17D2,U+1782,U+17D2,U+179A
	 */
	for (; i < end; i++)
	  info[i].mask |= indic_plan->mask_array[CFAR];

	break;
      }
    }
  }

  /* Apply ZWJ/ZWNJ effects */
  for (unsigned int i = start + 1; i < end; i++)
    if (is_joiner (info[i])) {
      bool non_joiner = info[i].indic_category() == OT_ZWNJ;
      unsigned int j = i;

      do {
	j--;

	/* A ZWJ disables CJCT, however, it's mere presence is enough
	 * to disable ligation.  No explicit action needed. */

	/* A ZWNJ disables HALF. */
	if (non_joiner)
	  info[j].mask &= ~indic_plan->mask_array[HALF];

      } while (j > start && !is_consonant (info[j]));
    }
}


static void
initial_reordering_vowel_syllable (const hb_ot_shape_plan_t *plan,
				   hb_face_t *face,
				   hb_buffer_t *buffer,
				   unsigned int start, unsigned int end)
{
  /* We made the vowels look like consonants.  So let's call the consonant logic! */
  initial_reordering_consonant_syllable (plan, face, buffer, start, end);
}

static void
initial_reordering_standalone_cluster (const hb_ot_shape_plan_t *plan,
				       hb_face_t *face,
				       hb_buffer_t *buffer,
				       unsigned int start, unsigned int end)
{
  /* We treat NBSP/dotted-circle as if they are consonants, so we should just chain.
   * Only if not in compatibility mode that is... */

  if (indic_options ().uniscribe_bug_compatible)
  {
    /* For dotted-circle, this is what Uniscribe does:
     * If dotted-circle is the last glyph, it just does nothing.
     * Ie. It doesn't form Reph. */
    if (buffer->info[end - 1].indic_category() == OT_DOTTEDCIRCLE)
      return;
  }

  initial_reordering_consonant_syllable (plan, face, buffer, start, end);
}

static void
initial_reordering_broken_cluster (const hb_ot_shape_plan_t *plan,
				   hb_face_t *face,
				   hb_buffer_t *buffer,
				   unsigned int start, unsigned int end)
{
  /* We already inserted dotted-circles, so just call the standalone_cluster. */
  initial_reordering_standalone_cluster (plan, face, buffer, start, end);
}

static void
initial_reordering_non_indic_cluster (const hb_ot_shape_plan_t *plan HB_UNUSED,
				      hb_face_t *face HB_UNUSED,
				      hb_buffer_t *buffer HB_UNUSED,
				      unsigned int start HB_UNUSED, unsigned int end HB_UNUSED)
{
  /* Nothing to do right now.  If we ever switch to using the output
   * buffer in the reordering process, we'd need to next_glyph() here. */
}


static void
initial_reordering_syllable (const hb_ot_shape_plan_t *plan,
			     hb_face_t *face,
			     hb_buffer_t *buffer,
			     unsigned int start, unsigned int end)
{
  syllable_type_t syllable_type = (syllable_type_t) (buffer->info[start].syllable() & 0x0F);
  switch (syllable_type) {
  case consonant_syllable:	initial_reordering_consonant_syllable (plan, face, buffer, start, end); return;
  case vowel_syllable:		initial_reordering_vowel_syllable     (plan, face, buffer, start, end); return;
  case standalone_cluster:	initial_reordering_standalone_cluster (plan, face, buffer, start, end); return;
  case broken_cluster:		initial_reordering_broken_cluster     (plan, face, buffer, start, end); return;
  case non_indic_cluster:	initial_reordering_non_indic_cluster  (plan, face, buffer, start, end); return;
  }
}

static inline void
insert_dotted_circles (const hb_ot_shape_plan_t *plan HB_UNUSED,
		       hb_font_t *font,
		       hb_buffer_t *buffer)
{
  /* Note: This loop is extra overhead, but should not be measurable. */
  bool has_broken_syllables = false;
  unsigned int count = buffer->len;
  for (unsigned int i = 0; i < count; i++)
    if ((buffer->info[i].syllable() & 0x0F) == broken_cluster) {
      has_broken_syllables = true;
      break;
    }
  if (likely (!has_broken_syllables))
    return;


  hb_codepoint_t dottedcircle_glyph;
  if (!font->get_glyph (0x25CC, 0, &dottedcircle_glyph))
    return;

  hb_glyph_info_t dottedcircle = {0};
  dottedcircle.codepoint = 0x25CC;
  set_indic_properties (dottedcircle);
  dottedcircle.codepoint = dottedcircle_glyph;

  buffer->clear_output ();

  buffer->idx = 0;
  unsigned int last_syllable = 0;
  while (buffer->idx < buffer->len)
  {
    unsigned int syllable = buffer->cur().syllable();
    syllable_type_t syllable_type = (syllable_type_t) (syllable & 0x0F);
    if (unlikely (last_syllable != syllable && syllable_type == broken_cluster))
    {
      last_syllable = syllable;

      hb_glyph_info_t info = dottedcircle;
      info.cluster = buffer->cur().cluster;
      info.mask = buffer->cur().mask;
      info.syllable() = buffer->cur().syllable();

      /* Insert dottedcircle after possible Repha. */
      while (buffer->idx < buffer->len &&
	     last_syllable == buffer->cur().syllable() &&
	     buffer->cur().indic_category() == OT_Repha)
        buffer->next_glyph ();

      buffer->output_info (info);
    }
    else
      buffer->next_glyph ();
  }

  buffer->swap_buffers ();
}

static void
initial_reordering (const hb_ot_shape_plan_t *plan,
		    hb_font_t *font,
		    hb_buffer_t *buffer)
{
  update_consonant_positions (plan, font, buffer);
  insert_dotted_circles (plan, font, buffer);

  hb_glyph_info_t *info = buffer->info;
  unsigned int count = buffer->len;
  if (unlikely (!count)) return;
  unsigned int last = 0;
  unsigned int last_syllable = info[0].syllable();
  for (unsigned int i = 1; i < count; i++)
    if (last_syllable != info[i].syllable()) {
      initial_reordering_syllable (plan, font->face, buffer, last, i);
      last = i;
      last_syllable = info[last].syllable();
    }
  initial_reordering_syllable (plan, font->face, buffer, last, count);
}

static void
final_reordering_syllable (const hb_ot_shape_plan_t *plan,
			   hb_buffer_t *buffer,
			   unsigned int start, unsigned int end)
{
  const indic_shape_plan_t *indic_plan = (const indic_shape_plan_t *) plan->data;
  hb_glyph_info_t *info = buffer->info;

  /* 4. Final reordering:
   *
   * After the localized forms and basic shaping forms GSUB features have been
   * applied (see below), the shaping engine performs some final glyph
   * reordering before applying all the remaining font features to the entire
   * cluster.
   */

  /* Find base again */
  unsigned int base;
  for (base = start; base < end; base++)
    if (info[base].indic_position() >= POS_BASE_C) {
      if (start < base && info[base].indic_position() > POS_BASE_C)
        base--;
      break;
    }


  /*   o Reorder matras:
   *
   *     If a pre-base matra character had been reordered before applying basic
   *     features, the glyph can be moved closer to the main consonant based on
   *     whether half-forms had been formed. Actual position for the matra is
   *     defined as “after last standalone halant glyph, after initial matra
   *     position and before the main consonant”. If ZWJ or ZWNJ follow this
   *     halant, position is moved after it.
   */

  if (start + 1 < end && start < base) /* Otherwise there can't be any pre-base matra characters. */
  {
    /* If we lost track of base, alas, position before last thingy. */
    unsigned int new_pos = base == end ? base - 2 : base - 1;

    /* Malayalam / Tamil do not have "half" forms or explicit virama forms.
     * The glyphs formed by 'half' are Chillus or ligated explicit viramas.
     * We want to position matra after them.
     */
    if (buffer->props.script != HB_SCRIPT_MALAYALAM && buffer->props.script != HB_SCRIPT_TAMIL)
    {
      while (new_pos > start &&
	     !(is_one_of (info[new_pos], (FLAG (OT_M) | FLAG (OT_H) | FLAG (OT_Coeng)))))
	new_pos--;

      /* If we found no Halant we are done.
       * Otherwise only proceed if the Halant does
       * not belong to the Matra itself! */
      if (is_halant_or_coeng (info[new_pos]) &&
	  info[new_pos].indic_position() != POS_PRE_M)
      {
	/* -> If ZWJ or ZWNJ follow this halant, position is moved after it. */
	if (new_pos + 1 < end && is_joiner (info[new_pos + 1]))
	  new_pos++;
      }
      else
        new_pos = start; /* No move. */
    }

    if (start < new_pos && info[new_pos].indic_position () != POS_PRE_M)
    {
      /* Now go see if there's actually any matras... */
      for (unsigned int i = new_pos; i > start; i--)
	if (info[i - 1].indic_position () == POS_PRE_M)
	{
	  unsigned int old_pos = i - 1;
	  hb_glyph_info_t tmp = info[old_pos];
	  memmove (&info[old_pos], &info[old_pos + 1], (new_pos - old_pos) * sizeof (info[0]));
	  info[new_pos] = tmp;
	  new_pos--;
	}
      buffer->merge_clusters (new_pos, MIN (end, base + 1));
    } else {
      for (unsigned int i = start; i < base; i++)
	if (info[i].indic_position () == POS_PRE_M) {
	  buffer->merge_clusters (i, MIN (end, base + 1));
	  break;
	}
    }
  }


  /*   o Reorder reph:
   *
   *     Reph’s original position is always at the beginning of the syllable,
   *     (i.e. it is not reordered at the character reordering stage). However,
   *     it will be reordered according to the basic-forms shaping results.
   *     Possible positions for reph, depending on the script, are; after main,
   *     before post-base consonant forms, and after post-base consonant forms.
   */

  /* If there's anything after the Ra that has the REPH pos, it ought to be halant.
   * Which means that the font has failed to ligate the Reph.  In which case, we
   * shouldn't move. */
  if (start + 1 < end &&
      info[start].indic_position() == POS_RA_TO_BECOME_REPH &&
      info[start + 1].indic_position() != POS_RA_TO_BECOME_REPH)
  {
    unsigned int new_reph_pos;
    reph_position_t reph_pos = indic_plan->config->reph_pos;

    /* XXX Figure out old behavior too */

    /*       1. If reph should be positioned after post-base consonant forms,
     *          proceed to step 5.
     */
    if (reph_pos == REPH_POS_AFTER_POST)
    {
      goto reph_step_5;
    }

    /*       2. If the reph repositioning class is not after post-base: target
     *          position is after the first explicit halant glyph between the
     *          first post-reph consonant and last main consonant. If ZWJ or ZWNJ
     *          are following this halant, position is moved after it. If such
     *          position is found, this is the target position. Otherwise,
     *          proceed to the next step.
     *
     *          Note: in old-implementation fonts, where classifications were
     *          fixed in shaping engine, there was no case where reph position
     *          will be found on this step.
     */
    {
      new_reph_pos = start + 1;
      while (new_reph_pos < base && !is_halant_or_coeng (info[new_reph_pos]))
	new_reph_pos++;

      if (new_reph_pos < base && is_halant_or_coeng (info[new_reph_pos])) {
	/* ->If ZWJ or ZWNJ are following this halant, position is moved after it. */
	if (new_reph_pos + 1 < base && is_joiner (info[new_reph_pos + 1]))
	  new_reph_pos++;
	goto reph_move;
      }
    }

    /*       3. If reph should be repositioned after the main consonant: find the
     *          first consonant not ligated with main, or find the first
     *          consonant that is not a potential pre-base reordering Ra.
     */
    if (reph_pos == REPH_POS_AFTER_MAIN)
    {
      new_reph_pos = base;
      /* XXX Skip potential pre-base reordering Ra. */
      while (new_reph_pos + 1 < end && info[new_reph_pos + 1].indic_position() <= POS_AFTER_MAIN)
	new_reph_pos++;
      if (new_reph_pos < end)
        goto reph_move;
    }

    /*       4. If reph should be positioned before post-base consonant, find
     *          first post-base classified consonant not ligated with main. If no
     *          consonant is found, the target position should be before the
     *          first matra, syllable modifier sign or vedic sign.
     */
    /* This is our take on what step 4 is trying to say (and failing, BADLY). */
    if (reph_pos == REPH_POS_AFTER_SUB)
    {
      new_reph_pos = base;
      while (new_reph_pos < end &&
	     !( FLAG (info[new_reph_pos + 1].indic_position()) & (FLAG (POS_POST_C) | FLAG (POS_AFTER_POST) | FLAG (POS_SMVD))))
	new_reph_pos++;
      if (new_reph_pos < end)
        goto reph_move;
    }

    /*       5. If no consonant is found in steps 3 or 4, move reph to a position
     *          immediately before the first post-base matra, syllable modifier
     *          sign or vedic sign that has a reordering class after the intended
     *          reph position. For example, if the reordering position for reph
     *          is post-main, it will skip above-base matras that also have a
     *          post-main position.
     */
    reph_step_5:
    {
      /* Copied from step 2. */
      new_reph_pos = start + 1;
      while (new_reph_pos < base && !is_halant_or_coeng (info[new_reph_pos]))
	new_reph_pos++;

      if (new_reph_pos < base && is_halant_or_coeng (info[new_reph_pos])) {
	/* ->If ZWJ or ZWNJ are following this halant, position is moved after it. */
	if (new_reph_pos + 1 < base && is_joiner (info[new_reph_pos + 1]))
	  new_reph_pos++;
	goto reph_move;
      }
    }

    /*       6. Otherwise, reorder reph to the end of the syllable.
     */
    {
      new_reph_pos = end - 1;
      while (new_reph_pos > start && info[new_reph_pos].indic_position() == POS_SMVD)
	new_reph_pos--;

      /*
       * If the Reph is to be ending up after a Matra,Halant sequence,
       * position it before that Halant so it can interact with the Matra.
       * However, if it's a plain Consonant,Halant we shouldn't do that.
       * Uniscribe doesn't do this.
       * TEST: U+0930,U+094D,U+0915,U+094B,U+094D
       */
      if (!indic_options ().uniscribe_bug_compatible &&
	  unlikely (is_halant_or_coeng (info[new_reph_pos]))) {
	for (unsigned int i = base + 1; i < new_reph_pos; i++)
	  if (info[i].indic_category() == OT_M) {
	    /* Ok, got it. */
	    new_reph_pos--;
	  }
      }
      goto reph_move;
    }

    reph_move:
    {
      /* Yay, one big cluster! Merge before moving. */
      buffer->merge_clusters (start, end);

      /* Move */
      hb_glyph_info_t reph = info[start];
      memmove (&info[start], &info[start + 1], (new_reph_pos - start) * sizeof (info[0]));
      info[new_reph_pos] = reph;
    }
  }


  /*   o Reorder pre-base reordering consonants:
   *
   *     If a pre-base reordering consonant is found, reorder it according to
   *     the following rules:
   */

  if (indic_plan->mask_array[PREF] && base + 1 < end) /* Otherwise there can't be any pre-base reordering Ra. */
  {
    for (unsigned int i = base + 1; i < end; i++)
      if ((info[i].mask & indic_plan->mask_array[PREF]) != 0)
      {
	/*       1. Only reorder a glyph produced by substitution during application
	 *          of the <pref> feature. (Note that a font may shape a Ra consonant with
	 *          the feature generally but block it in certain contexts.)
	 */
	if (i + 1 == end || (info[i + 1].mask & indic_plan->mask_array[PREF]) == 0)
	{
	  /*
	   *       2. Try to find a target position the same way as for pre-base matra.
	   *          If it is found, reorder pre-base consonant glyph.
	   *
	   *       3. If position is not found, reorder immediately before main
	   *          consonant.
	   */

	  unsigned int new_pos = base;
	  /* Malayalam / Tamil do not have "half" forms or explicit virama forms.
	   * The glyphs formed by 'half' are Chillus or ligated explicit viramas.
	   * We want to position matra after them.
	   */
	  if (buffer->props.script != HB_SCRIPT_MALAYALAM && buffer->props.script != HB_SCRIPT_TAMIL)
	  {
	    while (new_pos > start &&
		   !(is_one_of (info[new_pos - 1], FLAG(OT_M) | HALANT_OR_COENG_FLAGS)))
	      new_pos--;

	    /* In Khmer coeng model, a V,Ra can go *after* matras.  If it goes after a
	     * split matra, it should be reordered to *before* the left part of such matra. */
	    if (new_pos > start && info[new_pos - 1].indic_category() == OT_M)
	    {
	      unsigned int old_pos = i;
	      for (unsigned int i = base + 1; i < old_pos; i++)
		if (info[i].indic_category() == OT_M)
		{
		  new_pos--;
		  break;
		}
	    }
	  }

	  if (new_pos > start && is_halant_or_coeng (info[new_pos - 1]))
	    /* -> If ZWJ or ZWNJ follow this halant, position is moved after it. */
	    if (new_pos < end && is_joiner (info[new_pos]))
	      new_pos++;

	  {
	    unsigned int old_pos = i;
	    buffer->merge_clusters (new_pos, old_pos + 1);
	    hb_glyph_info_t tmp = info[old_pos];
	    memmove (&info[new_pos + 1], &info[new_pos], (old_pos - new_pos) * sizeof (info[0]));
	    info[new_pos] = tmp;
	  }
	}

        break;
      }
  }


  /* Apply 'init' to the Left Matra if it's a word start. */
  if (info[start].indic_position () == POS_PRE_M &&
      (!start ||
       !(FLAG (_hb_glyph_info_get_general_category (&info[start - 1])) &
	 FLAG_RANGE (HB_UNICODE_GENERAL_CATEGORY_FORMAT, HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK))))
    info[start].mask |= indic_plan->mask_array[INIT];


  /*
   * Finish off the clusters and go home!
   */
  if (indic_options ().uniscribe_bug_compatible)
  {
    /* Uniscribe merges the entire cluster.
     * This means, half forms are submerged into the main consonants cluster.
     * This is unnecessary, and makes cursor positioning harder, but that's what
     * Uniscribe does. */
    buffer->merge_clusters (start, end);
  }
}


static void
final_reordering (const hb_ot_shape_plan_t *plan,
		  hb_font_t *font HB_UNUSED,
		  hb_buffer_t *buffer)
{
  unsigned int count = buffer->len;
  if (unlikely (!count)) return;

  hb_glyph_info_t *info = buffer->info;
  unsigned int last = 0;
  unsigned int last_syllable = info[0].syllable();
  for (unsigned int i = 1; i < count; i++)
    if (last_syllable != info[i].syllable()) {
      final_reordering_syllable (plan, buffer, last, i);
      last = i;
      last_syllable = info[last].syllable();
    }
  final_reordering_syllable (plan, buffer, last, count);

  /* Zero syllables now... */
  for (unsigned int i = 0; i < count; i++)
    info[i].syllable() = 0;

  HB_BUFFER_DEALLOCATE_VAR (buffer, indic_category);
  HB_BUFFER_DEALLOCATE_VAR (buffer, indic_position);
}


static hb_ot_shape_normalization_mode_t
normalization_preference_indic (const hb_segment_properties_t *props HB_UNUSED)
{
  return HB_OT_SHAPE_NORMALIZATION_MODE_COMPOSED_DIACRITICS_NO_SHORT_CIRCUIT;
}

static bool
decompose_indic (const hb_ot_shape_normalize_context_t *c,
		 hb_codepoint_t  ab,
		 hb_codepoint_t *a,
		 hb_codepoint_t *b)
{
  switch (ab)
  {
    /* Don't decompose these. */
    case 0x0931  : return false;
    case 0x0B94  : return false;


    /*
     * Decompose split matras that don't have Unicode decompositions.
     */

    case 0x0F77  : *a = 0x0FB2; *b= 0x0F81; return true;
    case 0x0F79  : *a = 0x0FB3; *b= 0x0F81; return true;
    case 0x17BE  : *a = 0x17C1; *b= 0x17BE; return true;
    case 0x17BF  : *a = 0x17C1; *b= 0x17BF; return true;
    case 0x17C0  : *a = 0x17C1; *b= 0x17C0; return true;
    case 0x17C4  : *a = 0x17C1; *b= 0x17C4; return true;
    case 0x17C5  : *a = 0x17C1; *b= 0x17C5; return true;
    case 0x1925  : *a = 0x1920; *b= 0x1923; return true;
    case 0x1926  : *a = 0x1920; *b= 0x1924; return true;
    case 0x1B3C  : *a = 0x1B42; *b= 0x1B3C; return true;
    case 0x1112E  : *a = 0x11127; *b= 0x11131; return true;
    case 0x1112F  : *a = 0x11127; *b= 0x11132; return true;
#if 0
    /* This one has no decomposition in Unicode, but needs no decomposition either. */
    /* case 0x0AC9  : return false; */
    case 0x0B57  : *a = no decomp, -> RIGHT; return true;
    case 0x1C29  : *a = no decomp, -> LEFT; return true;
    case 0xA9C0  : *a = no decomp, -> RIGHT; return true;
    case 0x111BF  : *a = no decomp, -> ABOVE; return true;
#endif
  }

  if ((ab == 0x0DDA || hb_in_range<hb_codepoint_t> (ab, 0x0DDC, 0x0DDE)))
  {
    /*
     * Sinhala split matras...  Let the fun begin.
     *
     * These four characters have Unicode decompositions.  However, Uniscribe
     * decomposes them "Khmer-style", that is, it uses the character itself to
     * get the second half.  The first half of all four decompositions is always
     * U+0DD9.
     *
     * Now, there are buggy fonts, namely, the widely used lklug.ttf, that are
     * broken with Uniscribe.  But we need to support them.  As such, we only
     * do the Uniscribe-style decomposition if the character is transformed into
     * its "sec.half" form by the 'pstf' feature.  Otherwise, we fall back to
     * Unicode decomposition.
     *
     * Note that we can't unconditionally use Unicode decomposition.  That would
     * break some other fonts, that are designed to work with Uniscribe, and
     * don't have positioning features for the Unicode-style decomposition.
     *
     * Argh...
     *
     * The Uniscribe behavior is now documented in the newly published Sinhala
     * spec in 2012:
     *
     *   http://www.microsoft.com/typography/OpenTypeDev/sinhala/intro.htm#shaping
     */

    const indic_shape_plan_t *indic_plan = (const indic_shape_plan_t *) c->plan->data;

    hb_codepoint_t glyph;

    if (indic_options ().uniscribe_bug_compatible ||
	(c->font->get_glyph (ab, 0, &glyph) &&
	 indic_plan->pstf.would_substitute (&glyph, 1, true, c->font->face)))
    {
      /* Ok, safe to use Uniscribe-style decomposition. */
      *a = 0x0DD9;
      *b = ab;
      return true;
    }
  }

  return c->unicode->decompose (ab, a, b);
}

static bool
compose_indic (const hb_ot_shape_normalize_context_t *c,
	       hb_codepoint_t  a,
	       hb_codepoint_t  b,
	       hb_codepoint_t *ab)
{
  /* Avoid recomposing split matras. */
  if (HB_UNICODE_GENERAL_CATEGORY_IS_MARK (c->unicode->general_category (a)))
    return false;

  /* Composition-exclusion exceptions that we want to recompose. */
  if (a == 0x09AF && b == 0x09BC) { *ab = 0x09DF; return true; }

  return c->unicode->compose (a, b, ab);
}


const hb_ot_complex_shaper_t _hb_ot_complex_shaper_indic =
{
  "indic",
  collect_features_indic,
  override_features_indic,
  data_create_indic,
  data_destroy_indic,
  NULL, /* preprocess_text */
  normalization_preference_indic,
  decompose_indic,
  compose_indic,
  setup_masks_indic,
  false, /* zero_width_attached_marks */
  false, /* fallback_position */
};
