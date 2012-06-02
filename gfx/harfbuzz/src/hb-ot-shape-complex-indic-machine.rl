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

#ifndef HB_OT_SHAPE_COMPLEX_INDIC_MACHINE_HH
#define HB_OT_SHAPE_COMPLEX_INDIC_MACHINE_HH

#include "hb-private.hh"

HB_BEGIN_DECLS

%%{
  machine indic_syllable_machine;
  alphtype unsigned char;
  write data;
}%%

%%{

# Same order as enum indic_category_t.  Not sure how to avoid duplication.
X    = 0;
C    = 1;
Ra   = 2;
V    = 3;
N    = 4;
H    = 5;
ZWNJ = 6;
ZWJ  = 7;
M    = 8;
SM   = 9;
VD   = 10;
A    = 11;
NBSP = 12;
DOTTEDCIRCLE = 13;

c = C | Ra;
n = N N?;
z = ZWJ|ZWNJ;
matra_group = M N? H?;
syllable_tail = SM? (VD VD?)?;
place_holder = NBSP | DOTTEDCIRCLE;


consonant_syllable =	(c.n? (H.z?|z.H))* c.n? A? (H.z? | matra_group*)? syllable_tail;
vowel_syllable =	(Ra H)? V n? (z?.H.c | ZWJ.c)* matra_group* syllable_tail;
standalone_cluster =	(Ra H)? place_holder n? (z? H c)* matra_group* syllable_tail;
other =			any;

main := |*
	consonant_syllable	=> { process_syllable (consonant_syllable); };
	vowel_syllable		=> { process_syllable (vowel_syllable); };
	standalone_cluster	=> { process_syllable (standalone_cluster); };
	other			=> { process_syllable (non_indic); };
*|;


}%%

#define process_syllable(func) \
  HB_STMT_START { \
    /* fprintf (stderr, "syllable %d..%d %s\n", last, p+1, #func); */ \
    for (unsigned int i = last; i < p+1; i++) \
      info[i].syllable() = syllable_serial; \
    PASTE (initial_reordering_, func) (map, buffer, mask_array, last, p+1); \
    last = p+1; \
    syllable_serial++; \
    if (unlikely (!syllable_serial)) syllable_serial++; \
  } HB_STMT_END

static void
find_syllables (const hb_ot_map_t *map, hb_buffer_t *buffer, hb_mask_t *mask_array)
{
  unsigned int p, pe, eof, ts, te, act;
  int cs;
  hb_glyph_info_t *info = buffer->info;
  %%{
    write init;
    getkey info[p].indic_category();
  }%%

  p = 0;
  pe = eof = buffer->len;

  unsigned int last = 0;
  uint8_t syllable_serial = 1;
  %%{
    write exec;
  }%%
}

HB_END_DECLS

#endif /* HB_OT_SHAPE_COMPLEX_INDIC_MACHINE_HH */
