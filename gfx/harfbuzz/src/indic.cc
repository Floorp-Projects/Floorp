/*
 * Copyright © 2012  Google, Inc.
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

int
main (void)
{
  hb_unicode_funcs_t *funcs = hb_unicode_funcs_get_default ();

  printf ("There are split matras without a Unicode decomposition:\n");
  for (hb_codepoint_t u = 0; u < 0x110000; u++)
  {
    unsigned int type = get_indic_categories (u);

    unsigned int category = type & 0x0F;
    unsigned int position = type >> 4;

    hb_unicode_general_category_t cat = hb_unicode_general_category (funcs, u);
    unsigned int ccc = hb_unicode_combining_class (funcs, u);
    if (category == OT_M && ccc)
      printf ("U+%04X %d\n", u, ccc);

//    hb_codepoint_t a, b;
//    if (!hb_unicode_decompose (funcs, u, &a, &b))
//      printf ("U+%04X %x %x\n", u, category, position);
  }
}
