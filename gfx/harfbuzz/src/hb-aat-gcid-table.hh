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

#ifndef HB_AAT_GCID_TABLE_HH
#define HB_AAT_GCID_TABLE_HH

#include "hb-aat-layout-common-private.hh"

/*
 * gcid -- Glyphs CIDs
 * https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6gcid.html
 */
#define HB_AAT_TAG_gcid HB_TAG('g','c','i','d')


namespace AAT {


struct gcid
{
  static const hb_tag_t tableTag = HB_AAT_TAG_gcid;

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) && CIDs.sanitize (c)));
  }

  protected:
  HBUINT16	version;	/* Version number (set to 0) */
  HBUINT16	format;		/* Data format (set to 0) */
  HBUINT32	size;		/* Size of the table, including header */
  HBUINT16	registry;	/* The registry ID */
  HBUINT8	registryName[64];
				/* The registry name in ASCII */
  HBUINT16	order;		/* The order ID */
  HBUINT8	orderName[64];	/* The order name in ASCII */
  HBUINT16	supplementVersion;
				/* The supplement version */
  ArrayOf<HBUINT16>
		CIDs;		/* The CIDs for the glyphs in the font,
				 * starting with glyph 0. If a glyph does not correspond
				 * to a CID in the identified collection, 0xFFFF is used.
				 * This should not exceed the number of glyphs in the font. */
  public:
  DEFINE_SIZE_ARRAY (144, CIDs);
};

} /* namespace AAT */


#endif /* HB_AAT_GCID_TABLE_HH */
