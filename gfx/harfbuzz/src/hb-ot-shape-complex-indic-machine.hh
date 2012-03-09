
#line 1 "hb-ot-shape-complex-indic-machine.rl"
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

#ifndef HB_OT_SHAPE_COMPLEX_INDIC_MACHINE_HH
#define HB_OT_SHAPE_COMPLEX_INDIC_MACHINE_HH

#include "hb-private.hh"

HB_BEGIN_DECLS


#line 38 "hb-ot-shape-complex-indic-machine.hh"
static const unsigned char _indic_syllable_machine_trans_keys[] = {
	0u, 0u, 5u, 5u, 1u, 2u, 1u, 2u, 5u, 5u, 1u, 5u, 5u, 5u, 1u, 2u, 
	0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 
	0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 
	0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 
	0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 0u, 12u, 
	0u, 12u, 0
};

static const char _indic_syllable_machine_key_spans[] = {
	0, 1, 2, 2, 1, 5, 1, 2, 
	13, 13, 13, 13, 13, 13, 13, 13, 
	13, 13, 13, 13, 13, 13, 13, 13, 
	13, 13, 13, 13, 13, 13, 13, 13, 
	13, 13, 13, 13, 13, 13, 13, 13, 
	13
};

static const short _indic_syllable_machine_index_offsets[] = {
	0, 0, 2, 5, 8, 10, 16, 18, 
	21, 35, 49, 63, 77, 91, 105, 119, 
	133, 147, 161, 175, 189, 203, 217, 231, 
	245, 259, 273, 287, 301, 315, 329, 343, 
	357, 371, 385, 399, 413, 427, 441, 455, 
	469
};

static const char _indic_syllable_machine_indicies[] = {
	0, 1, 2, 2, 1, 3, 3, 
	1, 4, 1, 2, 2, 1, 1, 0, 
	1, 5, 1, 6, 6, 1, 7, 6, 
	8, 9, 1, 1, 1, 1, 1, 1, 
	1, 1, 10, 1, 11, 12, 13, 14, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	15, 1, 16, 17, 18, 19, 20, 21, 
	22, 22, 23, 24, 25, 26, 27, 1, 
	16, 17, 18, 19, 20, 28, 22, 22, 
	23, 24, 25, 26, 27, 1, 29, 30, 
	31, 32, 33, 1, 34, 35, 36, 37, 
	38, 1, 39, 1, 29, 30, 31, 32, 
	1, 1, 34, 35, 36, 37, 38, 1, 
	39, 1, 29, 30, 31, 32, 1, 1, 
	1, 1, 36, 37, 38, 1, 39, 1, 
	29, 30, 31, 32, 40, 2, 1, 1, 
	36, 37, 38, 1, 39, 1, 29, 30, 
	31, 32, 1, 2, 1, 1, 36, 37, 
	38, 1, 39, 1, 29, 30, 31, 32, 
	1, 1, 1, 1, 1, 1, 38, 1, 
	39, 1, 29, 30, 31, 32, 1, 1, 
	1, 1, 1, 1, 41, 1, 39, 1, 
	29, 30, 31, 32, 1, 1, 1, 1, 
	1, 1, 1, 1, 39, 1, 42, 43, 
	44, 45, 46, 4, 47, 47, 48, 49, 
	50, 1, 51, 1, 42, 43, 44, 45, 
	1, 4, 47, 47, 48, 49, 50, 1, 
	51, 1, 42, 43, 44, 45, 1, 1, 
	1, 1, 48, 49, 50, 1, 51, 1, 
	42, 43, 44, 45, 52, 3, 1, 1, 
	48, 49, 50, 1, 51, 1, 42, 43, 
	44, 45, 1, 3, 1, 1, 48, 49, 
	50, 1, 51, 1, 42, 43, 44, 45, 
	1, 1, 1, 1, 1, 1, 50, 1, 
	51, 1, 42, 43, 44, 45, 1, 1, 
	1, 1, 1, 1, 53, 1, 51, 1, 
	42, 43, 44, 45, 1, 1, 1, 1, 
	1, 1, 1, 1, 51, 1, 16, 17, 
	18, 19, 1, 21, 22, 22, 23, 24, 
	25, 26, 27, 1, 16, 6, 6, 19, 
	1, 1, 54, 54, 1, 24, 25, 1, 
	27, 1, 16, 6, 6, 19, 1, 1, 
	1, 1, 1, 24, 25, 1, 27, 1, 
	16, 17, 18, 19, 1, 1, 1, 1, 
	1, 1, 25, 1, 27, 1, 16, 17, 
	18, 19, 1, 1, 1, 1, 1, 1, 
	55, 1, 27, 1, 16, 17, 18, 19, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	27, 1, 16, 17, 18, 19, 56, 57, 
	1, 1, 23, 24, 25, 1, 27, 1, 
	16, 17, 18, 19, 1, 57, 1, 1, 
	23, 24, 25, 1, 27, 1, 16, 17, 
	18, 19, 1, 1, 1, 1, 23, 24, 
	25, 1, 27, 1, 16, 17, 18, 19, 
	1, 58, 1, 1, 23, 24, 25, 1, 
	27, 1, 16, 17, 18, 19, 1, 1, 
	59, 59, 1, 24, 25, 1, 27, 1, 
	16, 17, 18, 19, 1, 1, 1, 1, 
	1, 24, 25, 1, 27, 1, 16, 6, 
	6, 9, 1, 1, 54, 54, 1, 24, 
	25, 1, 10, 1, 0
};

static const char _indic_syllable_machine_trans_targs[] = {
	2, 0, 14, 22, 3, 7, 10, 9, 
	11, 12, 20, 9, 10, 11, 12, 20, 
	9, 10, 11, 12, 28, 29, 6, 34, 
	31, 32, 37, 20, 40, 9, 10, 11, 
	12, 13, 1, 5, 15, 17, 18, 20, 
	16, 19, 9, 10, 11, 12, 21, 4, 
	23, 25, 26, 20, 24, 27, 30, 33, 
	35, 36, 38, 39
};

static const char _indic_syllable_machine_trans_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 1, 1, 1, 1, 
	2, 2, 2, 2, 0, 0, 0, 0, 
	0, 0, 0, 2, 0, 3, 3, 3, 
	3, 0, 0, 0, 0, 0, 0, 3, 
	0, 0, 4, 4, 4, 4, 0, 0, 
	0, 0, 0, 4, 0, 0, 0, 0, 
	0, 0, 0, 0
};

static const char _indic_syllable_machine_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 2, 2, 3, 3, 3, 3, 
	3, 3, 3, 3, 4, 4, 4, 4, 
	4, 4, 4, 4, 2, 2, 2, 2, 
	2, 2, 2, 2, 2, 2, 2, 2, 
	2
};

static const int indic_syllable_machine_start = 8;
static const int indic_syllable_machine_first_final = 8;
static const int indic_syllable_machine_error = 0;

static const int indic_syllable_machine_en_main = 8;


#line 38 "hb-ot-shape-complex-indic-machine.rl"



#line 83 "hb-ot-shape-complex-indic-machine.rl"



static void
set_cluster (hb_buffer_t *buffer,
	     unsigned int start, unsigned int end)
{
  unsigned int cluster = buffer->info[start].cluster;

  for (unsigned int i = start + 1; i < end; i++)
    cluster = MIN (cluster, buffer->info[i].cluster);
  for (unsigned int i = start; i < end; i++)
    buffer->info[i].cluster = cluster;
}

static void
find_syllables (const hb_ot_map_t *map, hb_buffer_t *buffer, hb_mask_t *mask_array)
{
  unsigned int p, pe, eof;
  int cs;
  
#line 194 "hb-ot-shape-complex-indic-machine.hh"
	{
	cs = indic_syllable_machine_start;
	}

#line 106 "hb-ot-shape-complex-indic-machine.rl"


  p = 0;
  pe = eof = buffer->len;

  unsigned int last = 0;
  
#line 207 "hb-ot-shape-complex-indic-machine.hh"
	{
	int _slen;
	int _trans;
	const unsigned char *_keys;
	const char *_inds;
	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	_keys = _indic_syllable_machine_trans_keys + (cs<<1);
	_inds = _indic_syllable_machine_indicies + _indic_syllable_machine_index_offsets[cs];

	_slen = _indic_syllable_machine_key_spans[cs];
	_trans = _inds[ _slen > 0 && _keys[0] <=( buffer->info[p].indic_category()) &&
		( buffer->info[p].indic_category()) <= _keys[1] ?
		( buffer->info[p].indic_category()) - _keys[0] : _slen ];

	cs = _indic_syllable_machine_trans_targs[_trans];

	if ( _indic_syllable_machine_trans_actions[_trans] == 0 )
		goto _again;

	switch ( _indic_syllable_machine_trans_actions[_trans] ) {
	case 2:
#line 62 "hb-ot-shape-complex-indic-machine.rl"
	{ found_consonant_syllable (map, buffer, mask_array, last, p); }
#line 67 "hb-ot-shape-complex-indic-machine.rl"
	{ set_cluster (buffer, last, p); last = p; }
	break;
	case 3:
#line 63 "hb-ot-shape-complex-indic-machine.rl"
	{ found_vowel_syllable (map, buffer, mask_array, last, p); }
#line 67 "hb-ot-shape-complex-indic-machine.rl"
	{ set_cluster (buffer, last, p); last = p; }
	break;
	case 4:
#line 64 "hb-ot-shape-complex-indic-machine.rl"
	{ found_standalone_cluster (map, buffer, mask_array, last, p); }
#line 67 "hb-ot-shape-complex-indic-machine.rl"
	{ set_cluster (buffer, last, p); last = p; }
	break;
	case 1:
#line 65 "hb-ot-shape-complex-indic-machine.rl"
	{ found_non_indic (map, buffer, mask_array, last, p); }
#line 67 "hb-ot-shape-complex-indic-machine.rl"
	{ set_cluster (buffer, last, p); last = p; }
	break;
#line 256 "hb-ot-shape-complex-indic-machine.hh"
	}

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	switch ( _indic_syllable_machine_eof_actions[cs] ) {
	case 2:
#line 62 "hb-ot-shape-complex-indic-machine.rl"
	{ found_consonant_syllable (map, buffer, mask_array, last, p); }
#line 67 "hb-ot-shape-complex-indic-machine.rl"
	{ set_cluster (buffer, last, p); last = p; }
	break;
	case 3:
#line 63 "hb-ot-shape-complex-indic-machine.rl"
	{ found_vowel_syllable (map, buffer, mask_array, last, p); }
#line 67 "hb-ot-shape-complex-indic-machine.rl"
	{ set_cluster (buffer, last, p); last = p; }
	break;
	case 4:
#line 64 "hb-ot-shape-complex-indic-machine.rl"
	{ found_standalone_cluster (map, buffer, mask_array, last, p); }
#line 67 "hb-ot-shape-complex-indic-machine.rl"
	{ set_cluster (buffer, last, p); last = p; }
	break;
	case 1:
#line 65 "hb-ot-shape-complex-indic-machine.rl"
	{ found_non_indic (map, buffer, mask_array, last, p); }
#line 67 "hb-ot-shape-complex-indic-machine.rl"
	{ set_cluster (buffer, last, p); last = p; }
	break;
#line 292 "hb-ot-shape-complex-indic-machine.hh"
	}
	}

	_out: {}
	}

#line 114 "hb-ot-shape-complex-indic-machine.rl"

}

HB_END_DECLS

#endif /* HB_OT_SHAPE_COMPLEX_INDIC_MACHINE_HH */
