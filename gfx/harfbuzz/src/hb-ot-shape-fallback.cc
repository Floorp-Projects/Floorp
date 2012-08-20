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

#include "hb-ot-shape-fallback-private.hh"

static void
zero_mark_advances (hb_buffer_t *buffer,
		    unsigned int start,
		    unsigned int end)
{
  for (unsigned int i = start; i < end; i++)
    if (_hb_glyph_info_get_general_category (&buffer->info[i]) == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK)
    {
      buffer->pos[i].x_advance = 0;
      buffer->pos[i].y_advance = 0;
    }
}

static unsigned int
recategorize_combining_class (unsigned int modified_combining_class)
{
  if (modified_combining_class >= 200)
    return modified_combining_class;

  /* This should be kept in sync with modified combining class mapping
   * from hb-unicode.cc. */
  switch (modified_combining_class)
  {

    /* Hebrew */

    case HB_MODIFIED_COMBINING_CLASS_CCC10: /* sheva */
    case HB_MODIFIED_COMBINING_CLASS_CCC11: /* hataf segol */
    case HB_MODIFIED_COMBINING_CLASS_CCC12: /* hataf patah */
    case HB_MODIFIED_COMBINING_CLASS_CCC13: /* hataf qamats */
    case HB_MODIFIED_COMBINING_CLASS_CCC14: /* hiriq */
    case HB_MODIFIED_COMBINING_CLASS_CCC15: /* tsere */
    case HB_MODIFIED_COMBINING_CLASS_CCC16: /* segol */
    case HB_MODIFIED_COMBINING_CLASS_CCC17: /* patah */
    case HB_MODIFIED_COMBINING_CLASS_CCC18: /* qamats */
    case HB_MODIFIED_COMBINING_CLASS_CCC20: /* qubuts */
    case HB_MODIFIED_COMBINING_CLASS_CCC22: /* meteg */
      return HB_UNICODE_COMBINING_CLASS_BELOW;

    case HB_MODIFIED_COMBINING_CLASS_CCC23: /* rafe */
      return HB_UNICODE_COMBINING_CLASS_ATTACHED_ABOVE;

    case HB_MODIFIED_COMBINING_CLASS_CCC24: /* shin dot */
      return HB_UNICODE_COMBINING_CLASS_ABOVE_RIGHT;

    case HB_MODIFIED_COMBINING_CLASS_CCC25: /* sin dot */
    case HB_MODIFIED_COMBINING_CLASS_CCC19: /* holam */
      return HB_UNICODE_COMBINING_CLASS_ABOVE_LEFT;

    case HB_MODIFIED_COMBINING_CLASS_CCC26: /* point varika */
      return HB_UNICODE_COMBINING_CLASS_ABOVE;

    case HB_MODIFIED_COMBINING_CLASS_CCC21: /* dagesh */
      break;


    /* Arabic and Syriac */

    case HB_MODIFIED_COMBINING_CLASS_CCC27: /* fathatan */
    case HB_MODIFIED_COMBINING_CLASS_CCC28: /* dammatan */
    case HB_MODIFIED_COMBINING_CLASS_CCC30: /* fatha */
    case HB_MODIFIED_COMBINING_CLASS_CCC31: /* damma */
    case HB_MODIFIED_COMBINING_CLASS_CCC33: /* shadda */
    case HB_MODIFIED_COMBINING_CLASS_CCC34: /* sukun */
    case HB_MODIFIED_COMBINING_CLASS_CCC35: /* superscript alef */
    case HB_MODIFIED_COMBINING_CLASS_CCC36: /* superscript alaph */
      return HB_UNICODE_COMBINING_CLASS_ABOVE;

    case HB_MODIFIED_COMBINING_CLASS_CCC29: /* kasratan */
    case HB_MODIFIED_COMBINING_CLASS_CCC32: /* kasra */
      return HB_UNICODE_COMBINING_CLASS_BELOW;


    /* Thai */

    /* Note: to be useful we also need to position U+0E3A that has ccc=9 (virama).
     * But viramas can be both above and below based on the codepoint / script. */

    case HB_MODIFIED_COMBINING_CLASS_CCC103: /* sara u / sara uu */
      return HB_UNICODE_COMBINING_CLASS_BELOW;

    case HB_MODIFIED_COMBINING_CLASS_CCC107: /* mai */
      return HB_UNICODE_COMBINING_CLASS_ABOVE;


    /* Lao */

    case HB_MODIFIED_COMBINING_CLASS_CCC118: /* sign u / sign uu */
      return HB_UNICODE_COMBINING_CLASS_BELOW;

    case HB_MODIFIED_COMBINING_CLASS_CCC122: /* mai */
      return HB_UNICODE_COMBINING_CLASS_ABOVE;


    /* Tibetan */

    case HB_MODIFIED_COMBINING_CLASS_CCC129: /* sign aa */
      return HB_UNICODE_COMBINING_CLASS_BELOW;

    case HB_MODIFIED_COMBINING_CLASS_CCC130: /* sign i*/
      return HB_UNICODE_COMBINING_CLASS_ABOVE;

    case HB_MODIFIED_COMBINING_CLASS_CCC132: /* sign u */
      return HB_UNICODE_COMBINING_CLASS_BELOW;

  }

  return modified_combining_class;
}

static inline void
position_mark (const hb_ot_shape_plan_t *plan,
	       hb_font_t *font,
	       hb_buffer_t  *buffer,
	       hb_glyph_extents_t &base_extents,
	       unsigned int i,
	       unsigned int combining_class)
{
  hb_glyph_extents_t mark_extents;
  if (!font->get_glyph_extents (buffer->info[i].codepoint,
				&mark_extents))
    return;

  hb_position_t y_gap = font->y_scale / 16;

  hb_glyph_position_t &pos = buffer->pos[i];
  pos.x_offset = pos.y_offset = 0;


  /* We dont position LEFT and RIGHT marks. */

  /* X positioning */
  switch (combining_class)
  {
    case HB_UNICODE_COMBINING_CLASS_DOUBLE_BELOW:
    case HB_UNICODE_COMBINING_CLASS_DOUBLE_ABOVE:
      if (buffer->props.direction == HB_DIRECTION_LTR) {
	pos.x_offset += base_extents.x_bearing - mark_extents.width / 2 - mark_extents.x_bearing;
        break;
      } else if (buffer->props.direction == HB_DIRECTION_RTL) {
	pos.x_offset += base_extents.x_bearing + base_extents.width - mark_extents.width / 2 - mark_extents.x_bearing;
        break;
      }
      /* Fall through */

    case HB_UNICODE_COMBINING_CLASS_ATTACHED_BELOW:
    case HB_UNICODE_COMBINING_CLASS_ATTACHED_ABOVE:
    case HB_UNICODE_COMBINING_CLASS_BELOW:
    case HB_UNICODE_COMBINING_CLASS_ABOVE:
      /* Center align. */
      pos.x_offset += base_extents.x_bearing + (base_extents.width - mark_extents.width) / 2 - mark_extents.x_bearing;
      break;

    case HB_UNICODE_COMBINING_CLASS_ATTACHED_BELOW_LEFT:
    case HB_UNICODE_COMBINING_CLASS_BELOW_LEFT:
    case HB_UNICODE_COMBINING_CLASS_ABOVE_LEFT:
      /* Left align. */
      pos.x_offset += base_extents.x_bearing - mark_extents.x_bearing;
      break;

    case HB_UNICODE_COMBINING_CLASS_ATTACHED_ABOVE_RIGHT:
    case HB_UNICODE_COMBINING_CLASS_BELOW_RIGHT:
    case HB_UNICODE_COMBINING_CLASS_ABOVE_RIGHT:
      /* Right align. */
      pos.x_offset += base_extents.x_bearing + base_extents.width - mark_extents.width - mark_extents.x_bearing;
      break;
  }

  /* Y positioning */
  switch (combining_class)
  {
    case HB_UNICODE_COMBINING_CLASS_DOUBLE_BELOW:
    case HB_UNICODE_COMBINING_CLASS_BELOW_LEFT:
    case HB_UNICODE_COMBINING_CLASS_BELOW:
    case HB_UNICODE_COMBINING_CLASS_BELOW_RIGHT:
      /* Add gap, fall-through. */
      base_extents.height -= y_gap;

    case HB_UNICODE_COMBINING_CLASS_ATTACHED_BELOW_LEFT:
    case HB_UNICODE_COMBINING_CLASS_ATTACHED_BELOW:
      pos.y_offset += base_extents.y_bearing + base_extents.height - mark_extents.y_bearing;
      base_extents.height += mark_extents.height;
      break;

    case HB_UNICODE_COMBINING_CLASS_DOUBLE_ABOVE:
    case HB_UNICODE_COMBINING_CLASS_ABOVE_LEFT:
    case HB_UNICODE_COMBINING_CLASS_ABOVE:
    case HB_UNICODE_COMBINING_CLASS_ABOVE_RIGHT:
      /* Add gap, fall-through. */
      base_extents.y_bearing += y_gap;
      base_extents.height -= y_gap;

    case HB_UNICODE_COMBINING_CLASS_ATTACHED_ABOVE:
    case HB_UNICODE_COMBINING_CLASS_ATTACHED_ABOVE_RIGHT:
      pos.y_offset += base_extents.y_bearing - (mark_extents.y_bearing + mark_extents.height);
      base_extents.y_bearing -= mark_extents.height;
      base_extents.height += mark_extents.height;
      break;
  }
}

static inline void
position_around_base (const hb_ot_shape_plan_t *plan,
		      hb_font_t *font,
		      hb_buffer_t  *buffer,
		      unsigned int base,
		      unsigned int end)
{
  hb_glyph_extents_t base_extents;
  if (!font->get_glyph_extents (buffer->info[base].codepoint,
				&base_extents))
  {
    /* If extents don't work, zero marks and go home. */
    zero_mark_advances (buffer, base + 1, end);
    return;
  }
  base_extents.x_bearing += buffer->pos[base].x_offset;
  base_extents.y_bearing += buffer->pos[base].y_offset;

  /* XXX Handle ligature component positioning... */
  HB_UNUSED bool is_ligature = is_a_ligature (buffer->info[base]);

  hb_position_t x_offset = 0, y_offset = 0;
  if (HB_DIRECTION_IS_FORWARD (buffer->props.direction)) {
    x_offset -= buffer->pos[base].x_advance;
    y_offset -= buffer->pos[base].y_advance;
  }
  unsigned int last_combining_class = 255;
  hb_glyph_extents_t cluster_extents;
  for (unsigned int i = base + 1; i < end; i++)
    if (_hb_glyph_info_get_general_category (&buffer->info[i]) == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK)
    {
      unsigned int this_combining_class = recategorize_combining_class (_hb_glyph_info_get_modified_combining_class (&buffer->info[i]));
      if (this_combining_class != last_combining_class)
        cluster_extents = base_extents;

      position_mark (plan, font, buffer, base_extents, i, this_combining_class);

      buffer->pos[i].x_advance = 0;
      buffer->pos[i].y_advance = 0;
      buffer->pos[i].x_offset += x_offset;
      buffer->pos[i].y_offset += y_offset;

      /* combine cluster extents. */

      last_combining_class = this_combining_class;
    } else {
      if (HB_DIRECTION_IS_FORWARD (buffer->props.direction)) {
	x_offset -= buffer->pos[i].x_advance;
	y_offset -= buffer->pos[i].y_advance;
      } else {
	x_offset += buffer->pos[i].x_advance;
	y_offset += buffer->pos[i].y_advance;
      }
    }


}

static inline void
position_cluster (const hb_ot_shape_plan_t *plan,
		  hb_font_t *font,
		  hb_buffer_t  *buffer,
		  unsigned int start,
		  unsigned int end)
{
  if (end - start < 2)
    return;

  /* Find the base glyph */
  for (unsigned int i = start; i < end; i++)
    if (is_a_ligature (buffer->info[i]) ||
        !(FLAG (_hb_glyph_info_get_general_category (&buffer->info[i])) &
	  (FLAG (HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK) |
	   FLAG (HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK) |
	   FLAG (HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK))))
    {
      position_around_base (plan, font, buffer, i, end);
      break;
    }
}

void
_hb_ot_shape_fallback_position (const hb_ot_shape_plan_t *plan,
				hb_font_t *font,
				hb_buffer_t  *buffer)
{
  unsigned int start = 0;
  unsigned int last_cluster = buffer->info[0].cluster;
  unsigned int count = buffer->len;
  for (unsigned int i = 1; i < count; i++)
    if (buffer->info[i].cluster != last_cluster) {
      position_cluster (plan, font, buffer, start, i);
      start = i;
      last_cluster = buffer->info[i].cluster;
    }
  position_cluster (plan, font, buffer, start, count);
}
