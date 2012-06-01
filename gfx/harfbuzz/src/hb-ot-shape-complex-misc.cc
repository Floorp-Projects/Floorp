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


/* TODO Add kana, and other small shapers here */

/* When adding trivial shapers, eg. kana, hangul, etc, we can either
 * add a full shaper enum value for them, or switch on the script in
 * the default complex shaper.  The former is faster, so I think that's
 * what we would do, and hence the default complex shaper shall remain
 * empty.
 */

void
_hb_ot_shape_complex_collect_features_default (hb_ot_map_builder_t *map HB_UNUSED,
					       const hb_segment_properties_t *props HB_UNUSED)
{
}

hb_ot_shape_normalization_mode_t
_hb_ot_shape_complex_normalization_preference_default (void)
{
  return HB_OT_SHAPE_NORMALIZATION_MODE_COMPOSED_DIACRITICS;
}

void
_hb_ot_shape_complex_setup_masks_default (hb_ot_map_t *map HB_UNUSED,
					  hb_buffer_t *buffer HB_UNUSED,
					  hb_font_t *font HB_UNUSED)
{
}



/* Hangul shaper */

static const hb_tag_t hangul_features[] =
{
  HB_TAG('l','j','m','o'),
  HB_TAG('v','j','m','o'),
  HB_TAG('t','j','m','o'),
};

void
_hb_ot_shape_complex_collect_features_hangul (hb_ot_map_builder_t *map,
					      const hb_segment_properties_t *props HB_UNUSED)
{
  for (unsigned int i = 0; i < ARRAY_LENGTH (hangul_features); i++)
    map->add_bool_feature (hangul_features[i]);
}

hb_ot_shape_normalization_mode_t
_hb_ot_shape_complex_normalization_preference_hangul (void)
{
  return HB_OT_SHAPE_NORMALIZATION_MODE_COMPOSED_FULL;
}

void
_hb_ot_shape_complex_setup_masks_hangul (hb_ot_map_t *map HB_UNUSED,
					 hb_buffer_t *buffer HB_UNUSED,
					 hb_font_t *font HB_UNUSED)
{
}



/* Thai / Lao shaper */

void
_hb_ot_shape_complex_collect_features_thai (hb_ot_map_builder_t *map HB_UNUSED,
					    const hb_segment_properties_t *props HB_UNUSED)
{
}

hb_ot_shape_normalization_mode_t
_hb_ot_shape_complex_normalization_preference_thai (void)
{
  return HB_OT_SHAPE_NORMALIZATION_MODE_COMPOSED_FULL;
}

void
_hb_ot_shape_complex_setup_masks_thai (hb_ot_map_t *map HB_UNUSED,
				       hb_buffer_t *buffer,
				       hb_font_t *font HB_UNUSED)
{
  /* The following is NOT specified in the MS OT Thai spec, however, it seems
   * to be what Uniscribe and other engines implement.  According to Eric Muller:
   *
   * When you have a sara am, decompose it in nikhahit + sara a, *and* mode the
   * nihka hit backwards over any *tone* mark (0E48-0E4B).
   *
   * <0E14, 0E4B, 0E33> -> <0E14, 0E4D, 0E4B, 0E32>
   *
   * This reordering is legit only when the nikhahit comes from a sara am, not
   * when it's there to start with. The string <0E14, 0E4B, 0E4D> is probably
   * not what a u↪ser wanted, but the rendering is nevertheless nikhahit above
   * chattawa.
   *
   * Same for Lao.
   */

  /*
   * Here are the characters of significance:
   *
   *			Thai	Lao
   * SARA AM:		U+0E33	U+0EB3
   * SARA AA:		U+0E32	U+0EB2
   * Nikhahit:		U+0E4D	U+0ECD
   *
   * Tone marks:
   * Thai:	<0E48..0E4B> CCC=107
   * Lao:	<0EC8..0ECB> CCC=122
   *
   * Note how the Lao versions are the same as Thai + 0x80.
   */

  /* We only get one script at a time, so a script-agnostic implementation
   * is adequate here. */
#define IS_SARA_AM(x) (((x) & ~0x0080) == 0x0E33)
#define NIKHAHIT_FROM_SARA_AM(x) ((x) - 0xE33 + 0xE4D)
#define SARA_AA_FROM_SARA_AM(x) ((x) - 1)
#define IS_TONE_MARK(x) (((x) & ~0x0083) == 0x0E48)

  buffer->clear_output ();
  unsigned int count = buffer->len;
  for (buffer->idx = 0; buffer->idx < count;)
  {
    hb_codepoint_t u = buffer->cur().codepoint;
    if (likely (!IS_SARA_AM (u))) {
      buffer->next_glyph ();
      continue;
    }

    /* Is SARA AM. Decompose and reorder. */
    hb_codepoint_t decomposed[2] = {hb_codepoint_t (NIKHAHIT_FROM_SARA_AM (u)),
				    hb_codepoint_t (SARA_AA_FROM_SARA_AM (u))};
    buffer->replace_glyphs (1, 2, decomposed);
    if (unlikely (buffer->in_error))
      return;

    /* Ok, let's see... */
    unsigned int end = buffer->out_len;
    unsigned int start = end - 2;
    while (start > 0 && IS_TONE_MARK (buffer->out_info[start - 1].codepoint))
      start--;

    /* Move Nikhahit (end-2) to the beginning */
    hb_glyph_info_t t = buffer->out_info[end - 2];
    memmove (buffer->out_info + start + 1,
	     buffer->out_info + start,
	     sizeof (buffer->out_info[0]) * (end - start - 2));
    buffer->out_info[start] = t;

    /* XXX Make this easier! */
    /* Make cluster */
    for (; start > 0 && buffer->out_info[start - 1].cluster == buffer->out_info[start].cluster; start--)
      ;
    for (; buffer->idx < count;)
      if (buffer->cur().cluster == buffer->prev().cluster)
        buffer->next_glyph ();
      else
        break;
    end = buffer->out_len;

    buffer->merge_out_clusters (start, end);
  }
  buffer->swap_buffers ();
}
