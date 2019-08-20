/*
 * Copyright © 2019  Ebrahim Byagowi
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
 */

#ifndef HB_AAT_LAYOUT_OPBD_TABLE_HH
#define HB_AAT_LAYOUT_OPBD_TABLE_HH

#include "hb-aat-layout-common.hh"
#include "hb-open-type.hh"

/*
 * opbd -- Optical Bounds
 * https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6opbd.html
 */
#define HB_AAT_TAG_opbd HB_TAG('o','p','b','d')


namespace AAT {

struct OpticalBounds
{
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this)));
  }

  HBINT16	leftSide;
  HBINT16	topSide;
  HBINT16	rightSide;
  HBINT16	bottomSide;
  public:
  DEFINE_SIZE_STATIC (8);
};

struct opbd
{
  static constexpr hb_tag_t tableTag = HB_AAT_TAG_opbd;

  bool get_optical_bounds (hb_font_t *font, hb_codepoint_t glyph_id,
			   hb_position_t *left, hb_position_t *top,
			   hb_position_t *right, hb_position_t *bottom) const
  {
    const OffsetTo<OpticalBounds> *bounds_offset = lookupTable.get_value (glyph_id, font->face->get_num_glyphs ());
    if (!bounds_offset) return false;
    const OpticalBounds &bounds = this+*bounds_offset;
    switch (format)
    {
    case 0:
      *left = font->em_scale_x (bounds.leftSide);
      *top = font->em_scale_y (bounds.topSide);
      *right = font->em_scale_x (bounds.rightSide);
      *bottom = font->em_scale_y (bounds.bottomSide);
      return true;
    case 1:
      hb_position_t ignore;
      return font->get_glyph_contour_point (glyph_id, bounds.leftSide, left, &ignore) &&
	     font->get_glyph_contour_point (glyph_id, bounds.topSide, &ignore, top) &&
	     font->get_glyph_contour_point (glyph_id, bounds.rightSide, right, &ignore) &&
	     font->get_glyph_contour_point (glyph_id, bounds.bottomSide, &ignore, bottom);
    default:
      return false;
    }
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) &&
			  version.major == 1 &&
			  lookupTable.sanitize (c, this)));
  }

  protected:
  FixedVersion<>version;	/* Version number of the optical bounds
				 * table (0x00010000 for the current version). */
  HBUINT16	format;		/* Format of the optical bounds table.
				 * Format 0 indicates distance and Format 1 indicates
				 * control point. */
  Lookup<OffsetTo<OpticalBounds>>
		lookupTable;	/* Lookup table associating glyphs with the four
				 * int16 values for the left-side, top-side,
				 * right-side, and bottom-side optical bounds. */
  public:
  DEFINE_SIZE_MIN (8);
};

} /* namespace AAT */


#endif /* HB_AAT_LAYOUT_OPBD_TABLE_HH */
