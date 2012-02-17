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

#include "hb-ot-shape-private.hh"
#include "hb-ot-shape-complex-private.hh"


/*
 * HIGHLEVEL DESIGN:
 *
 * This file exports one main function: _hb_ot_shape_normalize().
 *
 * This function closely reflects the Unicode Normalization Algorithm,
 * yet it's different.  The shaper an either prefer decomposed (NFD) or
 * composed (NFC).
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
 *     with previous base.  This needs the itemizer to have this knowledge too.
 *     We need ot provide assistance to the itemizer.
 *
 *   - When a font does not support a character but supports its decomposition,
 *     well, use the decomposition.
 *
 *   - The Indic shaper requests decomposed output.  This will handle splitting
 *     matra for the Indic shaper.
 */

static void
output_glyph (hb_ot_shape_context_t *c,
	      hb_codepoint_t glyph)
{
  hb_buffer_t *buffer = c->buffer;

  buffer->output_glyph (glyph);
  hb_glyph_info_set_unicode_props (&buffer->out_info[buffer->out_len - 1], buffer->unicode);
}

static bool
decompose (hb_ot_shape_context_t *c,
	   bool shortest,
	   hb_codepoint_t ab)
{
  hb_codepoint_t a, b, glyph;

  if (!hb_unicode_decompose (c->buffer->unicode, ab, &a, &b) ||
      (b && !hb_font_get_glyph (c->font, b, 0, &glyph)))
    return FALSE;

  bool has_a = hb_font_get_glyph (c->font, a, 0, &glyph);
  if (shortest && has_a) {
    /* Output a and b */
    output_glyph (c, a);
    if (b)
      output_glyph (c, b);
    return TRUE;
  }

  if (decompose (c, shortest, a)) {
    if (b)
      output_glyph (c, b);
    return TRUE;
  }

  if (has_a) {
    output_glyph (c, a);
    if (b)
      output_glyph (c, b);
    return TRUE;
  }

  return FALSE;
}

static void
decompose_current_glyph (hb_ot_shape_context_t *c,
			 bool shortest)
{
  if (decompose (c, shortest, c->buffer->info[c->buffer->idx].codepoint))
    c->buffer->skip_glyph ();
  else
    c->buffer->next_glyph ();
}

static void
decompose_single_char_cluster (hb_ot_shape_context_t *c,
			       bool will_recompose)
{
  hb_codepoint_t glyph;

  /* If recomposing and font supports this, we're good to go */
  if (will_recompose && hb_font_get_glyph (c->font, c->buffer->info[c->buffer->idx].codepoint, 0, &glyph)) {
    c->buffer->next_glyph ();
    return;
  }

  decompose_current_glyph (c, will_recompose);
}

static void
decompose_multi_char_cluster (hb_ot_shape_context_t *c,
			      unsigned int end)
{
  /* TODO Currently if there's a variation-selector we give-up, it's just too hard. */
  for (unsigned int i = c->buffer->idx; i < end; i++)
    if (unlikely (is_variation_selector (c->buffer->info[i].codepoint))) {
      while (c->buffer->idx < end)
	c->buffer->next_glyph ();
      return;
    }

  while (c->buffer->idx < end)
    decompose_current_glyph (c, FALSE);
}

static int
compare_combining_class (const hb_glyph_info_t *pa, const hb_glyph_info_t *pb)
{
  unsigned int a = pa->combining_class();
  unsigned int b = pb->combining_class();

  return a < b ? -1 : a == b ? 0 : +1;
}

void
_hb_ot_shape_normalize (hb_ot_shape_context_t *c)
{
  hb_buffer_t *buffer = c->buffer;
  bool recompose = !hb_ot_shape_complex_prefer_decomposed (c->plan->shaper);
  bool has_multichar_clusters = FALSE;
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
      if (buffer->info[buffer->idx].cluster != buffer->info[end].cluster)
        break;

    if (buffer->idx + 1 == end)
      decompose_single_char_cluster (c, recompose);
    else {
      decompose_multi_char_cluster (c, end);
      has_multichar_clusters = TRUE;
    }
  }
  buffer->swap_buffers ();


  /* Technically speaking, two characters with ccc=0 may combine.  But all
   * those cases are in languages that the indic module handles (which expects
   * decomposed), or in Hangul jamo, which again, we want decomposed anyway.
   * So we don't bother combining across cluster boundaries.  This is a huge
   * performance saver if the compose() callback is slow.
   *
   * TODO: Am I right about Hangul?  If I am, we should add a Hangul module
   * that requests decomposed.  If for Hangul we end up wanting composed, we
   * can do that in the Hangul module.
   */

  if (!has_multichar_clusters)
    return; /* Done! */


  /* Second round, reorder (inplace) */

  count = buffer->len;
  for (unsigned int i = 0; i < count; i++)
  {
    if (buffer->info[i].combining_class() == 0)
      continue;

    unsigned int end;
    for (end = i + 1; end < count; end++)
      if (buffer->info[end].combining_class() == 0)
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
    if (buffer->info[buffer->idx].combining_class() == 0) {
      starter = buffer->out_len;
      buffer->next_glyph ();
      continue;
    }

    hb_codepoint_t composed, glyph;
    if ((buffer->out_info[buffer->out_len - 1].combining_class() >=
	 buffer->info[buffer->idx].combining_class()) ||
	!hb_unicode_compose (c->buffer->unicode,
			     buffer->out_info[starter].codepoint,
			     buffer->info[buffer->idx].codepoint,
			     &composed) ||
	!hb_font_get_glyph (c->font, composed, 0, &glyph))
    {
      /* Blocked, or doesn't compose. */
      buffer->next_glyph ();
      continue;
    }

    /* Composes. Modify starter and carry on. */
    buffer->out_info[starter].codepoint = composed;
    hb_glyph_info_set_unicode_props (&buffer->out_info[starter], buffer->unicode);

    buffer->skip_glyph ();
  }
  buffer->swap_buffers ();

}

