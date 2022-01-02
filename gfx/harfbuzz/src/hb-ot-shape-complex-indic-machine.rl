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

#include "hb.hh"

enum indic_syllable_type_t {
  indic_consonant_syllable,
  indic_vowel_syllable,
  indic_standalone_cluster,
  indic_symbol_cluster,
  indic_broken_cluster,
  indic_non_indic_cluster,
};

%%{
  machine indic_syllable_machine;
  alphtype unsigned char;
  write exports;
  write data;
}%%

%%{

export C    = 1;
export V    = 2;
export N    = 3;
export H    = 4;
export ZWNJ = 5;
export ZWJ  = 6;
export M    = 7;
export SM   = 8;
export A    = 10;
export PLACEHOLDER = 11;
export DOTTEDCIRCLE = 12;
export RS    = 13;
export Repha = 15;
export Ra    = 16;
export CM    = 17;
export Symbol= 18;
export CS    = 19;

c = (C | Ra);			# is_consonant
n = ((ZWNJ?.RS)? (N.N?)?);	# is_consonant_modifier
z = ZWJ|ZWNJ;			# is_joiner
reph = (Ra H | Repha);		# possible reph

cn = c.ZWJ?.n?;
forced_rakar = ZWJ H ZWJ Ra;
symbol = Symbol.N?;
matra_group = z*.M.N?.(H | forced_rakar)?;
syllable_tail = (z?.SM.SM?.ZWNJ?)? A*;
halant_group = (z?.H.(ZWJ.N?)?);
final_halant_group = halant_group | H.ZWNJ;
medial_group = CM?;
halant_or_matra_group = (final_halant_group | matra_group*);

complex_syllable_tail = (halant_group.cn)* medial_group halant_or_matra_group syllable_tail;

consonant_syllable =	(Repha|CS)? cn complex_syllable_tail;
vowel_syllable =	reph? V.n? (ZWJ | complex_syllable_tail);
standalone_cluster =	((Repha|CS)? PLACEHOLDER | reph? DOTTEDCIRCLE).n? complex_syllable_tail;
symbol_cluster =	symbol syllable_tail;
broken_cluster =	reph? n? complex_syllable_tail;
other =			any;

main := |*
	consonant_syllable	=> { found_syllable (indic_consonant_syllable); };
	vowel_syllable		=> { found_syllable (indic_vowel_syllable); };
	standalone_cluster	=> { found_syllable (indic_standalone_cluster); };
	symbol_cluster		=> { found_syllable (indic_symbol_cluster); };
	broken_cluster		=> { found_syllable (indic_broken_cluster); };
	other			=> { found_syllable (indic_non_indic_cluster); };
*|;


}%%

#define found_syllable(syllable_type) \
  HB_STMT_START { \
    if (0) fprintf (stderr, "syllable %d..%d %s\n", ts, te, #syllable_type); \
    for (unsigned int i = ts; i < te; i++) \
      info[i].syllable() = (syllable_serial << 4) | syllable_type; \
    syllable_serial++; \
    if (unlikely (syllable_serial == 16)) syllable_serial = 1; \
  } HB_STMT_END

static void
find_syllables_indic (hb_buffer_t *buffer)
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

  unsigned int syllable_serial = 1;
  %%{
    write exec;
  }%%
}

#undef found_syllable

#endif /* HB_OT_SHAPE_COMPLEX_INDIC_MACHINE_HH */
