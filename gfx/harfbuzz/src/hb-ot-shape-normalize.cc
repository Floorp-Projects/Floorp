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

#include "hb-ot-shape-normalize-private.hh"
#include "hb-ot-shape-private.hh"


/*
 * HIGHLEVEL DESIGN:
 *
 * This file exports one main function: _hb_ot_shape_normalize().
 *
 * This function closely reflects the Unicode Normalization Algorithm,
 * yet it's different.
 *
 * Each shaper specifies whether it prefers decomposed (NFD) or composed (NFC).
 * The logic however tries to use whatever the font can support.
 *
 * In general what happens is that: each grapheme is decomposed in a chain
 * of 1:2 decompositions, marks reordered, and then recomposed if desired,
 * so far it's like Unicode Normalization.  However, the decomposition and
 * recomposition only happens if the font supports the resulting characters.
 *
 * The goals are:
 *
 *   - Try to render all canonically equivalent strings similarly.  To really
 *     achieve this we have to always do the full decomposition and then
 *     selectively recompose from there.  It's kinda too expensive though, so
 *     we skip some cases.  For example, if composed is desired, we simply
 *     don't touch 1-character clusters that are supported by the font, even
 *     though their NFC may be different.
 *
 *   - When a font has a precomposed character for a sequence but the 'ccmp'
 *     feature in the font is not adequate, use the precomposed character
 *     which typically has better mark positioning.
 *
 *   - When a font does not support a combining mark, but supports it precomposed
 *     with previous base, use that.  This needs the itemizer to have this
 *     knowledge too.  We need to provide assistance to the itemizer.
 *
 *   - When a font does not support a character but supports its decomposition,
 *     well, use the decomposition (preferring the canonical decomposition, but
 *     falling back to the compatibility decomposition if necessary).  The
 *     compatibility decomposition is really nice to have, for characters like
 *     ellipsis, or various-sized space characters.
 *
 *   - The complex shapers can customize the compose and decompose functions to
 *     offload some of their requirements to the normalizer.  For example, the
 *     Indic shaper may want to disallow recomposing of two matras.
 *
 *   - We try compatibility decomposition if decomposing through canonical
 *     decomposition alone failed to find a sequence that the font supports.
 *     We don't try compatibility decomposition recursively during the canonical
 *     decomposition phase.  This has minimal impact.  There are only a handful
 *     of Greek letter that have canonical decompositions that include characters
 *     with compatibility decomposition.  Those can be found using this command:
 *
 *     egrep  "`echo -n ';('; grep ';<' UnicodeData.txt | cut -d';' -f1 | tr '\n' '|'; echo ') '`" UnicodeData.txt
 */

static void
output_glyph (hb_buffer_t *buffer, hb_codepoint_t glyph)
{
  buffer->output_glyph (glyph);
  _hb_glyph_info_set_unicode_props (&buffer->prev(), buffer->unicode);
}

static bool
decompose (hb_font_t *font, hb_buffer_t *buffer,
	   bool shortest,
	   hb_codepoint_t ab)
{
  hb_codepoint_t a, b, glyph;

  if (!buffer->unicode->decompose (ab, &a, &b) ||
      (b && !font->get_glyph (b, 0, &glyph)))
    return false;

  bool has_a = font->get_glyph (a, 0, &glyph);
  if (shortest && has_a) {
    /* Output a and b */
    output_glyph (buffer, a);
    if (b)
      output_glyph (buffer, b);
    return true;
  }

  if (decompose (font, buffer, shortest, a)) {
    if (b)
      output_glyph (buffer, b);
    return true;
  }

  if (has_a) {
    output_glyph (buffer, a);
    if (b)
      output_glyph (buffer, b);
    return true;
  }

  return false;
}

static bool
decompose_compatibility (hb_font_t *font, hb_buffer_t *buffer,
			 hb_codepoint_t u)
{
  unsigned int len, i;
  hb_codepoint_t decomposed[HB_UNICODE_MAX_DECOMPOSITION_LEN];

  len = buffer->unicode->decompose_compatibility (u, decomposed);
  if (!len)
    return false;

  hb_codepoint_t glyph;
  for (i = 0; i < len; i++)
    if (!font->get_glyph (decomposed[i], 0, &glyph))
      return false;

  for (i = 0; i < len; i++)
    output_glyph (buffer, decomposed[i]);

  return true;
}

static void
decompose_current_character (hb_font_t *font, hb_buffer_t *buffer,
			     bool shortest)
{
  hb_codepoint_t glyph;

  /* Kind of a cute waterfall here... */
  if (shortest && font->get_glyph (buffer->cur().codepoint, 0, &glyph))
    buffer->next_glyph ();
  else if (decompose (font, buffer, shortest, buffer->cur().codepoint))
    buffer->skip_glyph ();
  else if (!shortest && font->get_glyph (buffer->cur().codepoint, 0, &glyph))
    buffer->next_glyph ();
  else if (decompose_compatibility (font, buffer, buffer->cur().codepoint))
    buffer->skip_glyph ();
  else
    buffer->next_glyph ();
}

static void
decompose_multi_char_cluster (hb_font_t *font, hb_buffer_t *buffer,
			      unsigned int end)
{
  /* TODO Currently if there's a variation-selector we give-up, it's just too hard. */
  for (unsigned int i = buffer->idx; i < end; i++)
    if (unlikely (buffer->unicode->is_variation_selector (buffer->info[i].codepoint))) {
      while (buffer->idx < end)
	buffer->next_glyph ();
      return;
    }

  while (buffer->idx < end)
    decompose_current_character (font, buffer, false);
}

static int
compare_combining_class (const hb_glyph_info_t *pa, const hb_glyph_info_t *pb)
{
  unsigned int a = _hb_glyph_info_get_modified_combining_class (pa);
  unsigned int b = _hb_glyph_info_get_modified_combining_class (pb);

  return a < b ? -1 : a == b ? 0 : +1;
}

void
_hb_ot_shape_normalize (hb_font_t *font, hb_buffer_t *buffer,
			hb_ot_shape_normalization_mode_t mode)
{
  bool recompose = mode != HB_OT_SHAPE_NORMALIZATION_MODE_DECOMPOSED;
  bool has_multichar_clusters = false;
  unsigned int count;

  /* We do a fairly straightforward yet custom normalization process in three
   * separate rounds: decompose, reorder, recompose (if desired).  Currently
   * this makes two buffer swaps.  We can make it faster by moving the last
   * two rounds into the inner loop for the first round, but it's more readable
   * this way. */


  /* First round, decompose */

  buffer->clear_output ();
  count = buffer->len;
  for (buffer->idx = 0; buffer->idx < count;)
  {
    unsigned int end;
    for (end = buffer->idx + 1; end < count; end++)
      if (buffer->cur().cluster != buffer->info[end].cluster)
        break;

    if (buffer->idx + 1 == end)
      decompose_current_character (font, buffer, recompose);
    else {
      decompose_multi_char_cluster (font, buffer, end);
      has_multichar_clusters = true;
    }
  }
  buffer->swap_buffers ();


  if (mode != HB_OT_SHAPE_NORMALIZATION_MODE_COMPOSED_FULL && !has_multichar_clusters)
    return; /* Done! */


  /* Second round, reorder (inplace) */

  count = buffer->len;
  for (unsigned int i = 0; i < count; i++)
  {
    if (_hb_glyph_info_get_modified_combining_class (&buffer->info[i]) == 0)
      continue;

    unsigned int end;
    for (end = i + 1; end < count; end++)
      if (_hb_glyph_info_get_modified_combining_class (&buffer->info[end]) == 0)
        break;

    /* We are going to do a bubble-sort.  Only do this if the
     * sequence is short.  Doing it on long sequences can result
     * in an O(n^2) DoS. */
    if (end - i > 10) {
      i = end;
      continue;
    }

    hb_bubble_sort (buffer->info + i, end - i, compare_combining_class);

    i = end;
  }


  if (!recompose)
    return;

  /* Third round, recompose */

  /* As noted in the comment earlier, we don't try to combine
   * ccc=0 chars with their previous Starter. */

  buffer->clear_output ();
  count = buffer->len;
  unsigned int starter = 0;
  buffer->next_glyph ();
  while (buffer->idx < count)
  {
    hb_codepoint_t composed, glyph;
    if (/* If mode is NOT COMPOSED_FULL (ie. it's COMPOSED_DIACRITICS), we don't try to
	 * compose a CCC=0 character with it's preceding starter. */
	(mode == HB_OT_SHAPE_NORMALIZATION_MODE_COMPOSED_FULL ||
	 _hb_glyph_info_get_modified_combining_class (&buffer->cur()) != 0) &&
	/* If there's anything between the starter and this char, they should have CCC
	 * smaller than this character's. */
	(starter == buffer->out_len - 1 ||
	 _hb_glyph_info_get_modified_combining_class (&buffer->prev()) < _hb_glyph_info_get_modified_combining_class (&buffer->cur())) &&
	/* And compose. */
	buffer->unicode->compose (buffer->out_info[starter].codepoint,
				  buffer->cur().codepoint,
				  &composed) &&
	/* And the font has glyph for the composite. */
	font->get_glyph (composed, 0, &glyph))
    {
      /* Composes. */
      buffer->next_glyph (); /* Copy to out-buffer. */
      if (unlikely (buffer->in_error))
        return;
      buffer->merge_out_clusters (starter, buffer->out_len);
      buffer->out_len--; /* Remove the second composble. */
      buffer->out_info[starter].codepoint = composed; /* Modify starter and carry on. */
      _hb_glyph_info_set_unicode_props (&buffer->out_info[starter], buffer->unicode);

      continue;
    }

    /* Blocked, or doesn't compose. */
    buffer->next_glyph ();

    if (_hb_glyph_info_get_modified_combining_class (&buffer->prev()) == 0)
      starter = buffer->out_len - 1;
  }
  buffer->swap_buffers ();

}
