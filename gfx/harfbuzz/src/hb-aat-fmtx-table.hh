/*
 * Copyright © 2018  Ebrahim Byagowi
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

#ifndef HB_AAT_FMTX_TABLE_HH
#define HB_AAT_FMTX_TABLE_HH

#include "hb-aat-layout-common-private.hh"

/*
 * fmtx -- Font Metrics
 * https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6fmtx.html
 */
#define HB_AAT_TAG_fmtx HB_TAG('f','m','t','x')


namespace AAT {


struct fmtx
{
  static const hb_tag_t tableTag = HB_AAT_TAG_fmtx;

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this)));
  }

  FixedVersion<>version;		/* Version (set to 0x00020000). */
  HBUINT32	glyphIndex;		/* The glyph whose points represent the metrics. */
  HBUINT8	horizontalBefore;	/* Point number for the horizontal ascent. */
  HBUINT8	horizontalAfter;	/* Point number for the horizontal descent. */
  HBUINT8	horizontalCaretHead;	/* Point number for the horizontal caret head. */
  HBUINT8	horizontalCaretBase;	/* Point number for the horizontal caret base. */
  HBUINT8	verticalBefore;		/* Point number for the vertical ascent. */
  HBUINT8	verticalAfter;		/* Point number for the vertical descent. */
  HBUINT8	verticalCaretHead;	/* Point number for the vertical caret head. */
  HBUINT8	verticalCaretBase;	/* Point number for the vertical caret base. */
  public:
  DEFINE_SIZE_STATIC (16);
};

} /* namespace AAT */


#endif /* HB_AAT_FMTX_TABLE_HH */
