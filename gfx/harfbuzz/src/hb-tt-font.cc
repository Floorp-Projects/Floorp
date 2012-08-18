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

#include "hb-font-private.hh" /* Shall be first since may include windows.h */

#include "hb-open-type-private.hh"

#include "hb-ot-hhea-table.hh"
#include "hb-ot-hmtx-table.hh"

#include "hb-blob.h"

#include <string.h>



#if 0
struct hb_tt_font_t
{
  const struct hhea *hhea;
  hb_blob_t *hhea_blob;
};


static hb_tt_font_t *
_hb_tt_font_create (hb_font_t *font)
{
  /* TODO Remove this object altogether */
  hb_tt_font_t *tt = (hb_tt_font_t *) calloc (1, sizeof (hb_tt_font_t));

  tt->hhea_blob = Sanitizer<hhea>::sanitize (font->face->reference_table (HB_OT_TAG_hhea));
  tt->hhea = Sanitizer<hhea>::lock_instance (tt->hhea_blob);

  return tt;
}

static void
_hb_tt_font_destroy (hb_tt_font_t *tt)
{
  hb_blob_destroy (tt->hhea_blob);

  free (tt);
}

static inline const hhea&
_get_hhea (hb_face_t *face)
{
//  return likely (face->tt && face->tt->hhea) ? *face->tt->hhea : Null(hhea);
}


/*
 * hb_tt_font_funcs_t
 */

#endif
