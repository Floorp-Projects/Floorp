/* Hyphen - hyphenation library using converted TeX hyphenation patterns
 *
 * (C) 1998 Raph Levien
 * (C) 2001 ALTLinux, Moscow
 * (C) 2006, 2007, 2008 László Németh
 *
 * This was part of libHnj library by Raph Levien.
 *
 * Peter Novodvorsky from ALTLinux cut hyphenation part from libHnj
 * to use it in OpenOffice.org.
 *
 * Non-standard and compound word hyphenation support by László Németh.
 * 
 * License is the original LibHnj license:
 *
 * LibHnj is dual licensed under LGPL and MPL. Boilerplate for both
 * licenses follows.
 */

/* LibHnj - a library for high quality hyphenation and justification
 * Copyright (C) 1998 Raph Levien
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA  02111-1307  USA.
*/

/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 */
#ifndef __HYPHEN_H__
#define __HYPHEN_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _HyphenDict HyphenDict;
typedef struct _HyphenState HyphenState;
typedef struct _HyphenTrans HyphenTrans;
#define MAX_CHARS 100
#define MAX_NAME 20

struct _HyphenDict {
  /* user options */
  char lhmin;    /* lefthyphenmin: min. hyph. distance from the left side */
  char rhmin;    /* righthyphenmin: min. hyph. distance from the right side */
  char clhmin;   /* min. hyph. distance from the left compound boundary */
  char crhmin;   /* min. hyph. distance from the right compound boundary */
  char * nohyphen; /* comma separated list of characters or character
                    sequences with forbidden hyphenation */
  int nohyphenl; /* count of elements in nohyphen */
  /* system variables */
  int num_states;
  char cset[MAX_NAME];
  int utf8;
  HyphenState *states;
  HyphenDict *nextlevel;
};

struct _HyphenState {
  char *match;
  char *repl;
  signed char replindex;
  signed char replcut;
  int fallback_state;
  int num_trans;
  HyphenTrans *trans;
};

struct _HyphenTrans {
  char ch;
  int new_state;
};

HyphenDict *hnj_hyphen_load (const char *fn);
void hnj_hyphen_free (HyphenDict *dict);

/* obsolete, use hnj_hyphen_hyphenate2() or *hyphenate3() functions) */
int hnj_hyphen_hyphenate (HyphenDict *dict,
			   const char *word, int word_size,
			   char *hyphens);

/*

 int hnj_hyphen_hyphenate2(): non-standard hyphenation.

 (It supports Catalan, Dutch, German, Hungarian, Norwegian, Swedish
  etc. orthography, see documentation.)
 
 input data:
 word:      input word
 word_size: byte length of the input word
 
 hyphens:   allocated character buffer (size = word_size + 5)
 hyphenated_word: allocated character buffer (size ~ word_size * 2) or NULL
 rep, pos, cut: pointers (point to the allocated and _zeroed_ buffers
                (size=word_size) or with NULL value) or NULL

 output data:
 hyphens:   hyphenation vector (hyphenation points signed with odd numbers)
 hyphenated_word: hyphenated input word (hyphens signed with `='),
                  optional (NULL input)
 rep:       NULL (only standard hyph.), or replacements (hyphenation points
            signed with `=' in replacements);
 pos:       NULL, or difference of the actual position and the beginning
            positions of the change in input words;
 cut:       NULL, or counts of the removed characters of the original words
            at hyphenation,

 Note: rep, pos, cut are complementary arrays to the hyphens, indexed with the
       character positions of the input word.

 For example:
 Schiffahrt -> Schiff=fahrt,
 pattern: f1f/ff=f,1,2
 output: rep[5]="ff=f", pos[5] = 1, cut[5] = 2

 Note: hnj_hyphen_hyphenate2() can allocate rep, pos, cut (word_size
       length arrays):

 char ** rep = NULL;
 int * pos = NULL;
 int * cut = NULL;
 char hyphens[MAXWORDLEN];
 hnj_hyphen_hyphenate2(dict, "example", 7, hyphens, NULL, &rep, &pos, &cut);
 
 See example in the source distribution.

*/

int hnj_hyphen_hyphenate2 (HyphenDict *dict,
        const char *word, int word_size, char * hyphens,
        char *hyphenated_word, char *** rep, int ** pos, int ** cut);

/* like hnj_hyphen_hyphenate2, but with hyphenmin parameters */
/* lhmin: lefthyphenmin
 * rhmin: righthyphenmin
 * clhmin: compoundlefthyphemin
 * crhmin: compoundrighthyphenmin
 * (see documentation) */

int hnj_hyphen_hyphenate3 (HyphenDict *dict,
	const char *word, int word_size, char * hyphens,
	char *hyphword, char *** rep, int ** pos, int ** cut,
	int lhmin, int rhmin, int clhmin, int crhmin);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __HYPHEN_H__ */
