
#line 1 "../../src/hb-ot-shape-complex-indic-machine.rl"
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


#line 38 "hb-ot-shape-complex-indic-machine.hh.tmp"
static const unsigned char _indic_syllable_machine_trans_keys[] = {
	5u, 5u, 1u, 2u, 1u, 2u, 5u, 5u, 1u, 5u, 1u, 2u, 5u, 5u, 1u, 13u, 
	4u, 11u, 4u, 11u, 5u, 11u, 1u, 10u, 1u, 10u, 10u, 10u, 10u, 10u, 4u, 10u, 
	5u, 10u, 8u, 10u, 5u, 10u, 6u, 10u, 9u, 10u, 4u, 11u, 1u, 13u, 4u, 10u, 
	4u, 10u, 5u, 10u, 4u, 10u, 5u, 10u, 8u, 10u, 10u, 10u, 10u, 10u, 4u, 10u, 
	4u, 10u, 5u, 10u, 4u, 10u, 5u, 10u, 8u, 10u, 10u, 10u, 10u, 10u, 0
};

static const char _indic_syllable_machine_key_spans[] = {
	1, 2, 2, 1, 5, 2, 1, 13, 
	8, 8, 7, 10, 10, 1, 1, 7, 
	6, 3, 6, 5, 2, 8, 13, 7, 
	7, 6, 7, 6, 3, 1, 1, 7, 
	7, 6, 7, 6, 3, 1, 1
};

static const unsigned char _indic_syllable_machine_index_offsets[] = {
	0, 2, 5, 8, 10, 16, 19, 21, 
	35, 44, 53, 61, 72, 83, 85, 87, 
	95, 102, 106, 113, 119, 122, 131, 145, 
	153, 161, 168, 176, 183, 187, 189, 191, 
	199, 207, 214, 222, 229, 233, 235
};

static const char _indic_syllable_machine_indicies[] = {
	1, 0, 2, 2, 0, 4, 4, 3, 
	5, 3, 4, 4, 3, 3, 5, 3, 
	7, 7, 6, 8, 6, 2, 10, 11, 
	9, 9, 9, 9, 9, 9, 9, 9, 
	12, 12, 9, 14, 15, 16, 16, 17, 
	18, 19, 20, 13, 21, 15, 16, 16, 
	17, 18, 19, 20, 13, 15, 16, 16, 
	17, 18, 19, 20, 13, 2, 2, 13, 
	13, 13, 22, 22, 13, 18, 19, 13, 
	2, 2, 13, 13, 13, 13, 13, 13, 
	18, 19, 13, 19, 13, 23, 13, 24, 
	25, 13, 13, 17, 18, 19, 13, 25, 
	13, 13, 17, 18, 19, 13, 17, 18, 
	19, 13, 26, 13, 13, 17, 18, 19, 
	13, 27, 27, 13, 18, 19, 13, 18, 
	19, 13, 14, 28, 16, 16, 17, 18, 
	19, 20, 13, 2, 2, 11, 13, 13, 
	22, 22, 13, 18, 19, 13, 12, 12, 
	13, 30, 5, 31, 32, 33, 34, 35, 
	29, 4, 5, 31, 32, 33, 34, 35, 
	29, 5, 31, 32, 33, 34, 35, 29, 
	36, 37, 29, 29, 33, 34, 35, 29, 
	37, 29, 29, 33, 34, 35, 29, 33, 
	34, 35, 29, 35, 29, 38, 29, 40, 
	8, 41, 41, 42, 43, 44, 39, 7, 
	8, 41, 41, 42, 43, 44, 39, 8, 
	41, 41, 42, 43, 44, 39, 45, 46, 
	39, 39, 42, 43, 44, 39, 46, 39, 
	39, 42, 43, 44, 39, 42, 43, 44, 
	39, 44, 39, 47, 39, 0
};

static const char _indic_syllable_machine_trans_targs[] = {
	7, 1, 8, 7, 25, 2, 7, 33, 
	5, 7, 21, 23, 31, 7, 9, 11, 
	0, 15, 13, 14, 18, 10, 12, 7, 
	16, 17, 19, 20, 22, 7, 24, 3, 
	4, 26, 29, 30, 27, 28, 7, 7, 
	32, 6, 34, 37, 38, 35, 36, 7
};

static const char _indic_syllable_machine_trans_actions[] = {
	1, 0, 2, 3, 2, 0, 4, 2, 
	0, 7, 2, 2, 2, 8, 2, 0, 
	0, 0, 0, 0, 0, 2, 0, 9, 
	0, 0, 0, 0, 0, 10, 2, 0, 
	0, 0, 0, 0, 0, 0, 11, 12, 
	2, 0, 0, 0, 0, 0, 0, 13
};

static const char _indic_syllable_machine_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 5, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0
};

static const char _indic_syllable_machine_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 6, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0
};

static const unsigned char _indic_syllable_machine_eof_trans[] = {
	1, 1, 4, 4, 4, 7, 7, 0, 
	14, 14, 14, 14, 14, 14, 14, 14, 
	14, 14, 14, 14, 14, 14, 14, 30, 
	30, 30, 30, 30, 30, 30, 30, 40, 
	40, 40, 40, 40, 40, 40, 40
};

static const int indic_syllable_machine_start = 7;
static const int indic_syllable_machine_first_final = 7;
static const int indic_syllable_machine_error = -1;

static const int indic_syllable_machine_en_main = 7;


#line 38 "../../src/hb-ot-shape-complex-indic-machine.rl"



#line 79 "../../src/hb-ot-shape-complex-indic-machine.rl"


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
  
#line 170 "hb-ot-shape-complex-indic-machine.hh.tmp"
	{
	cs = indic_syllable_machine_start;
	ts = 0;
	te = 0;
	act = 0;
	}

#line 101 "../../src/hb-ot-shape-complex-indic-machine.rl"


  p = 0;
  pe = eof = buffer->len;

  unsigned int last = 0;
  uint8_t syllable_serial = 1;
  
#line 187 "hb-ot-shape-complex-indic-machine.hh.tmp"
	{
	int _slen;
	int _trans;
	const unsigned char *_keys;
	const char *_inds;
	if ( p == pe )
		goto _test_eof;
_resume:
	switch ( _indic_syllable_machine_from_state_actions[cs] ) {
	case 6:
#line 1 "NONE"
	{ts = p;}
	break;
#line 201 "hb-ot-shape-complex-indic-machine.hh.tmp"
	}

	_keys = _indic_syllable_machine_trans_keys + (cs<<1);
	_inds = _indic_syllable_machine_indicies + _indic_syllable_machine_index_offsets[cs];

	_slen = _indic_syllable_machine_key_spans[cs];
	_trans = _inds[ _slen > 0 && _keys[0] <=( info[p].indic_category()) &&
		( info[p].indic_category()) <= _keys[1] ?
		( info[p].indic_category()) - _keys[0] : _slen ];

_eof_trans:
	cs = _indic_syllable_machine_trans_targs[_trans];

	if ( _indic_syllable_machine_trans_actions[_trans] == 0 )
		goto _again;

	switch ( _indic_syllable_machine_trans_actions[_trans] ) {
	case 2:
#line 1 "NONE"
	{te = p+1;}
	break;
	case 9:
#line 72 "../../src/hb-ot-shape-complex-indic-machine.rl"
	{te = p+1;{ process_syllable (consonant_syllable); }}
	break;
	case 11:
#line 73 "../../src/hb-ot-shape-complex-indic-machine.rl"
	{te = p+1;{ process_syllable (vowel_syllable); }}
	break;
	case 13:
#line 74 "../../src/hb-ot-shape-complex-indic-machine.rl"
	{te = p+1;{ process_syllable (standalone_cluster); }}
	break;
	case 7:
#line 75 "../../src/hb-ot-shape-complex-indic-machine.rl"
	{te = p+1;{ process_syllable (non_indic); }}
	break;
	case 8:
#line 72 "../../src/hb-ot-shape-complex-indic-machine.rl"
	{te = p;p--;{ process_syllable (consonant_syllable); }}
	break;
	case 10:
#line 73 "../../src/hb-ot-shape-complex-indic-machine.rl"
	{te = p;p--;{ process_syllable (vowel_syllable); }}
	break;
	case 12:
#line 74 "../../src/hb-ot-shape-complex-indic-machine.rl"
	{te = p;p--;{ process_syllable (standalone_cluster); }}
	break;
	case 1:
#line 72 "../../src/hb-ot-shape-complex-indic-machine.rl"
	{{p = ((te))-1;}{ process_syllable (consonant_syllable); }}
	break;
	case 3:
#line 73 "../../src/hb-ot-shape-complex-indic-machine.rl"
	{{p = ((te))-1;}{ process_syllable (vowel_syllable); }}
	break;
	case 4:
#line 74 "../../src/hb-ot-shape-complex-indic-machine.rl"
	{{p = ((te))-1;}{ process_syllable (standalone_cluster); }}
	break;
#line 263 "hb-ot-shape-complex-indic-machine.hh.tmp"
	}

_again:
	switch ( _indic_syllable_machine_to_state_actions[cs] ) {
	case 5:
#line 1 "NONE"
	{ts = 0;}
	break;
#line 272 "hb-ot-shape-complex-indic-machine.hh.tmp"
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _indic_syllable_machine_eof_trans[cs] > 0 ) {
		_trans = _indic_syllable_machine_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	}

#line 110 "../../src/hb-ot-shape-complex-indic-machine.rl"

}

HB_END_DECLS

#endif /* HB_OT_SHAPE_COMPLEX_INDIC_MACHINE_HH */
