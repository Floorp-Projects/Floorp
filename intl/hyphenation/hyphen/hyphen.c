/* Libhnj is dual licensed under LGPL and MPL. Boilerplate for both
 * licenses follows.
 */

/* LibHnj - a library for high quality hyphenation and justification
 * Copyright (C) 1998 Raph Levien, 
 * 	     (C) 2001 ALTLinux, Moscow (http://www.alt-linux.org), 
 *           (C) 2001 Peter Novodvorsky (nidd@cs.msu.su)
 *           (C) 2006, 2007, 2008, 2010 László Németh (nemeth at OOo)
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
#include <stdlib.h> /* for NULL, malloc */
#include <stdio.h>  /* for fprintf */
#include <string.h> /* for strdup */
#include <limits.h> /* for INT_MAX */

#ifdef UNX
#include <unistd.h> /* for exit */
#endif

#define noVERBOSE

/* calculate hyphenmin values with long ligature length (2 or 3 characters
 * instead of 1 or 2) for comparison with hyphenation without ligatures */
#define noLONG_LIGATURE

#ifdef LONG_LIGATURE
#define LIG_xx	1
#define LIG_xxx	2
#else
#define LIG_xx	0
#define LIG_xxx	1
#endif

#include "hnjalloc.h"
#include "hyphen.h"

static char *
hnj_strdup (const char *s)
{
  char *newstr;
  int l;

  l = strlen (s);
  newstr = (char *) hnj_malloc (l + 1);
  memcpy (newstr, s, l);
  newstr[l] = 0;
  return newstr;
}

/* remove cross-platform text line end characters */
void hnj_strchomp(char * s)
{
  int k = strlen(s);
  if ((k > 0) && ((*(s+k-1)=='\r') || (*(s+k-1)=='\n'))) *(s+k-1) = '\0';
  if ((k > 1) && (*(s+k-2) == '\r')) *(s+k-2) = '\0';
}

/* a little bit of a hash table implementation. This simply maps strings
   to state numbers */

typedef struct _HashTab HashTab;
typedef struct _HashEntry HashEntry;

/* A cheap, but effective, hack. */
#define HASH_SIZE 31627

struct _HashTab {
  HashEntry *entries[HASH_SIZE];
};

struct _HashEntry {
  HashEntry *next;
  char *key;
  int val;
};

/* a char* hash function from ASU - adapted from Gtk+ */
static unsigned int
hnj_string_hash (const char *s)
{
  const char *p;
  unsigned int h=0, g;
  for(p = s; *p != '\0'; p += 1) {
    h = ( h << 4 ) + *p;
    if ( ( g = h & 0xf0000000 ) ) {
      h = h ^ (g >> 24);
      h = h ^ g;
    }
  }
  return h /* % M */;
}

static HashTab *
hnj_hash_new (void)
{
  HashTab *hashtab;
  int i;

  hashtab = (HashTab *) hnj_malloc (sizeof(HashTab));
  for (i = 0; i < HASH_SIZE; i++)
    hashtab->entries[i] = NULL;

  return hashtab;
}

static void
hnj_hash_free (HashTab *hashtab)
{
  int i;
  HashEntry *e, *next;

  for (i = 0; i < HASH_SIZE; i++)
    for (e = hashtab->entries[i]; e; e = next)
      {
	next = e->next;
	hnj_free (e->key);
	hnj_free (e);
      }

  hnj_free (hashtab);
}

/* assumes that key is not already present! */
static void
hnj_hash_insert (HashTab *hashtab, const char *key, int val)
{
  int i;
  HashEntry *e;

  i = hnj_string_hash (key) % HASH_SIZE;
  e = (HashEntry *) hnj_malloc (sizeof(HashEntry));
  e->next = hashtab->entries[i];
  e->key = hnj_strdup (key);
  e->val = val;
  hashtab->entries[i] = e;
}

/* return val if found, otherwise -1 */
static int
hnj_hash_lookup (HashTab *hashtab, const char *key)
{
  int i;
  HashEntry *e;
  i = hnj_string_hash (key) % HASH_SIZE;
  for (e = hashtab->entries[i]; e; e = e->next)
    if (!strcmp (key, e->key))
      return e->val;
  return -1;
}

/* Get the state number, allocating a new state if necessary. */
static int
hnj_get_state (HyphenDict *dict, HashTab *hashtab, const char *string)
{
  int state_num;

  state_num = hnj_hash_lookup (hashtab, string);

  if (state_num >= 0)
    return state_num;

  hnj_hash_insert (hashtab, string, dict->num_states);
  /* predicate is true if dict->num_states is a power of two */
  if (!(dict->num_states & (dict->num_states - 1)))
    {
      dict->states = (HyphenState *) hnj_realloc (dict->states,
				  (dict->num_states << 1) *
				  sizeof(HyphenState));
    }
  dict->states[dict->num_states].match = NULL;
  dict->states[dict->num_states].repl = NULL;
  dict->states[dict->num_states].fallback_state = -1;
  dict->states[dict->num_states].num_trans = 0;
  dict->states[dict->num_states].trans = NULL;
  return dict->num_states++;
}

/* add a transition from state1 to state2 through ch - assumes that the
   transition does not already exist */
static void
hnj_add_trans (HyphenDict *dict, int state1, int state2, char ch)
{
  int num_trans;

  num_trans = dict->states[state1].num_trans;
  if (num_trans == 0)
    {
      dict->states[state1].trans = (HyphenTrans *) hnj_malloc (sizeof(HyphenTrans));
    }
  else if (!(num_trans & (num_trans - 1)))
    {
      dict->states[state1].trans = (HyphenTrans *) hnj_realloc (dict->states[state1].trans,
						(num_trans << 1) *
						sizeof(HyphenTrans));
    }
  dict->states[state1].trans[num_trans].ch = ch;
  dict->states[state1].trans[num_trans].new_state = state2;
  dict->states[state1].num_trans++;
}

#ifdef VERBOSE
HashTab *global[1];

static char *
get_state_str (int state, int level)
{
  int i;
  HashEntry *e;

  for (i = 0; i < HASH_SIZE; i++)
    for (e = global[level]->entries[i]; e; e = e->next)
      if (e->val == state)
	return e->key;
  return NULL;
}
#endif

void hnj_hyphen_load_line(char * buf, HyphenDict * dict, HashTab * hashtab) {
  int i, j;
  char word[MAX_CHARS];
  char pattern[MAX_CHARS];
  char * repl;
  signed char replindex;
  signed char replcut;
  int state_num = 0;
  int last_state;
  char ch;
  int found;

	  if (strncmp(buf, "LEFTHYPHENMIN", 13) == 0) {
	    dict->lhmin = atoi(buf + 13);
	    return;
	  } else if (strncmp(buf, "RIGHTHYPHENMIN", 14) == 0) {
	    dict->rhmin = atoi(buf + 14);
	    return;
	  } else if (strncmp(buf, "COMPOUNDLEFTHYPHENMIN", 21) == 0) {
	    dict->clhmin = atoi(buf + 21);
	    return;
	  } else if (strncmp(buf, "COMPOUNDRIGHTHYPHENMIN", 22) == 0) {
	    dict->crhmin = atoi(buf + 22);
	    return;
	  } else if (strncmp(buf, "NOHYPHEN", 8) == 0) {
	    char * space = buf + 8;
	    while (*space != '\0' && (*space == ' ' || *space == '\t')) space++;
	    if (*buf != '\0') dict->nohyphen = hnj_strdup(space);
	    if (dict->nohyphen) {
	        char * nhe = dict->nohyphen + strlen(dict->nohyphen) - 1;
	        *nhe = 0;
	        for (nhe = nhe - 1; nhe > dict->nohyphen; nhe--) {
	                if (*nhe == ',') {
	                    dict->nohyphenl++;
	                    *nhe = 0;
	                }
	        }
	    }
	    return;
	  } 
	  j = 0;
	  pattern[j] = '0';
          repl = strchr(buf, '/');
          replindex = 0;
          replcut = 0;
          if (repl) {
            char * index = strchr(repl + 1, ',');
            *repl = '\0';
            if (index) {
                char * index2 = strchr(index + 1, ',');
                *index = '\0';
                if (index2) {
                    *index2 = '\0';
                    replindex = (signed char) atoi(index + 1) - 1;
                    replcut = (signed char) atoi(index2 + 1);                
                }
            } else {
                hnj_strchomp(repl + 1);
                replindex = 0;
                replcut = (signed char) strlen(buf);
            }
            repl = hnj_strdup(repl + 1);
          }
	  for (i = 0; (unsigned char)buf[i] > (unsigned char)' '; i++)
	    {
	      if (buf[i] >= '0' && buf[i] <= '9')
		pattern[j] = buf[i];
	      else
		{
		  word[j] = buf[i];
		  pattern[++j] = '0';
		}
	    }
	  word[j] = '\0';
	  pattern[j + 1] = '\0';

          i = 0;
	  if (!repl) {
	    /* Optimize away leading zeroes */
            for (; pattern[i] == '0'; i++);
          } else {
            if (*word == '.') i++;
            /* convert UTF-8 char. positions of discretionary hyph. replacements to 8-bit */
            if (dict->utf8) {
                int pu = -1;        /* unicode character position */
                int ps = -1;        /* unicode start position (original replindex) */
                size_t pc = (*word == '.') ? 1: 0; /* 8-bit character position */
                for (; pc < (strlen(word) + 1); pc++) {
                /* beginning of an UTF-8 character (not '10' start bits) */
                    if ((((unsigned char) word[pc]) >> 6) != 2) pu++;
                    if ((ps < 0) && (replindex == pu)) {
                        ps = replindex;
                        replindex = (signed char) pc;
                    }
                    if ((ps >= 0) && ((pu - ps) == replcut)) {
                        replcut = (signed char) (pc - replindex);
                        break;
                    }
                }
                if (*word == '.') replindex--;
            }
          }

#ifdef VERBOSE
	  printf ("word %s pattern %s, j = %d  repl: %s\n", word, pattern + i, j, repl);
#endif
	  found = hnj_hash_lookup (hashtab, word);
	  state_num = hnj_get_state (dict, hashtab, word);
	  dict->states[state_num].match = hnj_strdup (pattern + i);
	  dict->states[state_num].repl = repl;
	  dict->states[state_num].replindex = replindex;
          if (!replcut) {
            dict->states[state_num].replcut = (signed char) strlen(word);
          } else {
            dict->states[state_num].replcut = replcut;
          }

	  /* now, put in the prefix transitions */
          for (; found < 0 && j > 0; --j)
	    {
	      last_state = state_num;
	      ch = word[j - 1];
	      word[j - 1] = '\0';
	      found = hnj_hash_lookup (hashtab, word);
	      state_num = hnj_get_state (dict, hashtab, word);
	      hnj_add_trans (dict, state_num, last_state, ch);
	    }
}

HyphenDict *
hnj_hyphen_load (const char *fn)
{
  HyphenDict *result;
  FILE *f;
  f = fopen (fn, "r");
  if (f == NULL)
    return NULL;

  result = hnj_hyphen_load_file(f);

  fclose(f);
  return result;
}

HyphenDict *
hnj_hyphen_load_file (FILE *f)
{
  HyphenDict *dict[2];
  HashTab *hashtab;
  char buf[MAX_CHARS];
  int nextlevel = 0;
  int i, j, k;
  HashEntry *e;
  int state_num = 0;
/* loading one or two dictionaries (separated by NEXTLEVEL keyword) */
for (k = 0; k < 2; k++) { 
  hashtab = hnj_hash_new ();
#ifdef VERBOSE
  global[k] = hashtab;
#endif
  hnj_hash_insert (hashtab, "", 0);
  dict[k] = (HyphenDict *) hnj_malloc (sizeof(HyphenDict));
  dict[k]->num_states = 1;
  dict[k]->states = (HyphenState *) hnj_malloc (sizeof(HyphenState));
  dict[k]->states[0].match = NULL;
  dict[k]->states[0].repl = NULL;
  dict[k]->states[0].fallback_state = -1;
  dict[k]->states[0].num_trans = 0;
  dict[k]->states[0].trans = NULL;
  dict[k]->nextlevel = NULL;
  dict[k]->lhmin = 0;
  dict[k]->rhmin = 0;
  dict[k]->clhmin = 0;
  dict[k]->crhmin = 0;
  dict[k]->nohyphen = NULL;
  dict[k]->nohyphenl = 0;

  /* read in character set info */
  if (k == 0) {
    for (i=0;i<MAX_NAME;i++) dict[k]->cset[i]= 0;
    if (fgets(dict[k]->cset,  sizeof(dict[k]->cset),f) != NULL) {
      for (i=0;i<MAX_NAME;i++)
        if ((dict[k]->cset[i] == '\r') || (dict[k]->cset[i] == '\n'))
          dict[k]->cset[i] = 0;
    } else {
      dict[k]->cset[0] = 0;
    }
    dict[k]->utf8 = (strcmp(dict[k]->cset, "UTF-8") == 0);
  } else {
    strncpy(dict[k]->cset, dict[0]->cset, sizeof(dict[k]->cset)-1);
    dict[k]->cset[sizeof(dict[k]->cset)-1] = '\0';
    dict[k]->utf8 = dict[0]->utf8;
  }

  if (k == 0 || nextlevel) {
    while (fgets (buf, sizeof(buf), f) != NULL) {
      if (strncmp(buf, "NEXTLEVEL", 9) == 0) {
	nextlevel = 1;
	break;
      } else if (buf[0] != '%') hnj_hyphen_load_line(buf, dict[k], hashtab);
    }
  } else if (k == 1) {
    /* default first level: hyphen and ASCII apostrophe */
    if (!dict[0]->utf8) hnj_hyphen_load_line("NOHYPHEN ',-\n", dict[k], hashtab);
    else hnj_hyphen_load_line("NOHYPHEN ',\xe2\x80\x93,\xe2\x80\x99,-\n", dict[k], hashtab);
    strncpy(buf, "1-1\n", MAX_CHARS-1); /* buf rewritten by hnj_hyphen_load here */
    buf[MAX_CHARS-1] = '\0';
    hnj_hyphen_load_line(buf, dict[k], hashtab); /* remove hyphen */
    hnj_hyphen_load_line("1'1\n", dict[k], hashtab); /* ASCII apostrophe */
    if (dict[0]->utf8) {
      hnj_hyphen_load_line("1\xe2\x80\x93" "1\n", dict[k], hashtab); /* endash */
      hnj_hyphen_load_line("1\xe2\x80\x99" "1\n", dict[k], hashtab); /* apostrophe */
    }
  }

  /* Could do unioning of matches here (instead of the preprocessor script).
     If we did, the pseudocode would look something like this:

     foreach state in the hash table
        foreach i = [1..length(state) - 1]
           state to check is substr (state, i)
           look it up
           if found, and if there is a match, union the match in.

     It's also possible to avoid the quadratic blowup by doing the
     search in order of increasing state string sizes - then you
     can break the loop after finding the first match.

     This step should be optional in any case - if there is a
     preprocessed rule table, it's always faster to use that.

*/

  /* put in the fallback states */
  for (i = 0; i < HASH_SIZE; i++)
    for (e = hashtab->entries[i]; e; e = e->next)
      {
	if (*(e->key)) for (j = 1; 1; j++)
	  {          
	    state_num = hnj_hash_lookup (hashtab, e->key + j);
	    if (state_num >= 0)
	      break;
	  }
        /* KBH: FIXME state 0 fallback_state should always be -1? */
	if (e->val)
	  dict[k]->states[e->val].fallback_state = state_num;
      }
#ifdef VERBOSE
  for (i = 0; i < HASH_SIZE; i++)
    for (e = hashtab->entries[i]; e; e = e->next)
      {
	printf ("%d string %s state %d, fallback=%d\n", i, e->key, e->val,
		dict[k]->states[e->val].fallback_state);
	for (j = 0; j < dict[k]->states[e->val].num_trans; j++)
	  printf (" %c->%d\n", dict[k]->states[e->val].trans[j].ch,
		  dict[k]->states[e->val].trans[j].new_state);
      }
#endif

#ifndef VERBOSE
  hnj_hash_free (hashtab);
#endif
  state_num = 0;
}
  if (nextlevel) dict[0]->nextlevel = dict[1];
  else {
    dict[1] -> nextlevel = dict[0];
    dict[1]->lhmin = dict[0]->lhmin;
    dict[1]->rhmin = dict[0]->rhmin;
    dict[1]->clhmin = (dict[0]->clhmin) ? dict[0]->clhmin : ((dict[0]->lhmin) ? dict[0]->lhmin : 3);
    dict[1]->crhmin = (dict[0]->crhmin) ? dict[0]->crhmin : ((dict[0]->rhmin) ? dict[0]->rhmin : 3);
#ifdef VERBOSE
    HashTab *r = global[0];
    global[0] = global[1];
    global[1] = r;
#endif
    return dict[1];
  }
  return dict[0];
}

void hnj_hyphen_free (HyphenDict *dict)
{
  int state_num;
  HyphenState *hstate;

  for (state_num = 0; state_num < dict->num_states; state_num++)
    {
      hstate = &dict->states[state_num];
      if (hstate->match)
	hnj_free (hstate->match);
      if (hstate->repl)
	hnj_free (hstate->repl);
      if (hstate->trans)
	hnj_free (hstate->trans);
    }
  if (dict->nextlevel) hnj_hyphen_free(dict->nextlevel);

  if (dict->nohyphen) hnj_free(dict->nohyphen);

  hnj_free (dict->states);

  hnj_free (dict);
}

#define MAX_WORD 256

int hnj_hyphen_hyphenate (HyphenDict *dict,
			   const char *word, int word_size,
			   char *hyphens)
{
  char *prep_word;
  int i, j, k;
  int state;
  char ch;
  HyphenState *hstate;
  char *match;
  int offset;

  prep_word = (char*) hnj_malloc (word_size + 3);

  j = 0;
  prep_word[j++] = '.';

  for (i = 0; i < word_size; i++) {
    if (word[i] <= '9' && word[i] >= '0') {
      prep_word[j++] = '.';
    } else {
      prep_word[j++] = word[i];
    }
  }

  prep_word[j++] = '.';
  prep_word[j] = '\0';

  for (i = 0; i < word_size + 5; i++)
    hyphens[i] = '0';

#ifdef VERBOSE
  printf ("prep_word = %s\n", prep_word);
#endif

  /* now, run the finite state machine */
  state = 0;
  for (i = 0; i < j; i++)
    {
      ch = prep_word[i];
      for (;;)
	{

	  if (state == -1) {
            /* return 1; */
	    /*  KBH: FIXME shouldn't this be as follows? */
            state = 0;
            goto try_next_letter;
          }          

#ifdef VERBOSE
	  char *state_str;
	  state_str = get_state_str (state, 0);

	  for (k = 0; k < i - strlen (state_str); k++)
	    putchar (' ');
	  printf ("%s", state_str);
#endif

	  hstate = &dict->states[state];
	  for (k = 0; k < hstate->num_trans; k++)
	    if (hstate->trans[k].ch == ch)
	      {
		state = hstate->trans[k].new_state;
		goto found_state;
	      }
	  state = hstate->fallback_state;
#ifdef VERBOSE
	  printf (" falling back, fallback_state %d\n", state);
#endif
	}
    found_state:
#ifdef VERBOSE
      printf ("found state %d\n",state);
#endif
      /* Additional optimization is possible here - especially,
	 elimination of trailing zeroes from the match. Leading zeroes
	 have already been optimized. */
      match = dict->states[state].match;
      /* replacing rules not handled by hyphen_hyphenate() */
      if (match && !dict->states[state].repl)
	{
	  offset = i + 1 - strlen (match);
#ifdef VERBOSE
	  for (k = 0; k < offset; k++)
	    putchar (' ');
	  printf ("%s\n", match);
#endif
	  /* This is a linear search because I tried a binary search and
	     found it to be just a teeny bit slower. */
	  for (k = 0; match[k]; k++)
	    if (hyphens[offset + k] < match[k])
	      hyphens[offset + k] = match[k];
	}

      /* KBH: we need this to make sure we keep looking in a word */
      /* for patterns even if the current character is not known in state 0 */
      /* since patterns for hyphenation may occur anywhere in the word */
      try_next_letter: ;

    }
#ifdef VERBOSE
  for (i = 0; i < j; i++)
    putchar (hyphens[i]);
  putchar ('\n');
#endif

  for (i = 0; i < j - 4; i++)
#if 0
    if (hyphens[i + 1] & 1)
      hyphens[i] = '-';
#else
    hyphens[i] = hyphens[i + 1];
#endif
  hyphens[0] = '0';
  for (; i < word_size; i++)
    hyphens[i] = '0';
  hyphens[word_size] = '\0';

  hnj_free (prep_word);
    
  return 0;    
}

/* Unicode ligature length */
int hnj_ligature(unsigned char c) {
    switch (c) {
        case 0x80:			/* ff */
        case 0x81:			/* fi */
        case 0x82: return LIG_xx;	/* fl */
        case 0x83:			/* ffi */
        case 0x84: return LIG_xxx;	/* ffl */
        case 0x85:			/* long st */
        case 0x86: return LIG_xx;	/* st */
    }
    return 0;
}

/* character length of the first n byte of the input word */
int hnj_hyphen_strnlen(const char * word, int n, int utf8)
{
    int i = 0;
    int j = 0;
    while (j < n && word[j] != '\0') {
      i++;
      /* Unicode ligature support */
      if (utf8 && ((unsigned char) word[j] == 0xEF) && ((unsigned char) word[j + 1] == 0xAC))  {
        i += hnj_ligature(word[j + 2]);
      }
      for (j++; utf8 && (word[j] & 0xc0) == 0x80; j++);
    }
    return i;
}

int hnj_hyphen_lhmin(int utf8, const char *word, int word_size, char * hyphens,
	char *** rep, int ** pos, int ** cut, int lhmin)
{
    int i = 1, j;

    /* Unicode ligature support */
    if (utf8 && ((unsigned char) word[0] == 0xEF) && ((unsigned char) word[1] == 0xAC))  {
      i += hnj_ligature(word[2]);
    }

    /* ignore numbers */
    for (j = 0; word[j] <= '9' && word[j] >= '0'; j++) i--;

    for (j = 0; i < lhmin && word[j] != '\0'; i++) do {
      /* check length of the non-standard part */
      if (*rep && *pos && *cut && (*rep)[j]) {
        char * rh = strchr((*rep)[j], '=');
        if (rh && (hnj_hyphen_strnlen(word, j - (*pos)[j] + 1, utf8) +
          hnj_hyphen_strnlen((*rep)[j], rh - (*rep)[j], utf8)) < lhmin) {
            free((*rep)[j]);
            (*rep)[j] = NULL;
            hyphens[j] = '0';
          }
       } else {
         hyphens[j] = '0';
       }
       j++;

       /* Unicode ligature support */
       if (utf8 && ((unsigned char) word[j] == 0xEF) && ((unsigned char) word[j + 1] == 0xAC))  {
         i += hnj_ligature(word[j + 2]);
       }
    } while (utf8 && (word[j] & 0xc0) == 0x80);
    return 0;
}

int hnj_hyphen_rhmin(int utf8, const char *word, int word_size, char * hyphens,
	char *** rep, int ** pos, int ** cut, int rhmin)
{
    int i = 0;
    int j;

    /* ignore numbers */
    for (j = word_size - 1; j > 0 && word[j] <= '9' && word[j] >= '0'; j--) i--;

    for (j = word_size - 1; i < rhmin && j > 0; j--) {
      /* check length of the non-standard part */
      if (*rep && *pos && *cut && (*rep)[j]) {
        char * rh = strchr((*rep)[j], '=');
        if (rh && (hnj_hyphen_strnlen(word + j - (*pos)[j] + (*cut)[j] + 1, 100, utf8) +
          hnj_hyphen_strnlen(rh + 1, strlen(rh + 1), utf8)) < rhmin) {
            free((*rep)[j]);
            (*rep)[j] = NULL;
            hyphens[j] = '0';
          }
       } else {
         hyphens[j] = '0';
       }
       if (!utf8 || (word[j] & 0xc0) == 0xc0 || (word[j] & 0x80) != 0x80) i++;
    }
    return 0;
}

/* recursive function for compound level hyphenation */
int hnj_hyphen_hyph_(HyphenDict *dict, const char *word, int word_size,
    char * hyphens, char *** rep, int ** pos, int ** cut,
    int clhmin, int crhmin, int lend, int rend)
{
  char *prep_word;
  int i, j, k;
  int state;
  char ch;
  HyphenState *hstate;
  char *match;
  char *repl;
  signed char replindex;
  signed char replcut;
  int offset;
  int * matchlen;
  int * matchindex;
  char ** matchrepl;  
  int isrepl = 0;
  int nHyphCount;

  size_t prep_word_size = word_size + 3;
  prep_word = (char*) hnj_malloc (prep_word_size);
  matchlen = (int*) hnj_malloc ((word_size + 3) * sizeof(int));
  matchindex = (int*) hnj_malloc ((word_size + 3) * sizeof(int));
  matchrepl = (char**) hnj_malloc ((word_size + 3) * sizeof(char *));

  j = 0;
  prep_word[j++] = '.';
  
  for (i = 0; i < word_size; i++) {
    if (word[i] <= '9' && word[i] >= '0') {
      prep_word[j++] = '.';
    } else {
      prep_word[j++] = word[i];
    }
  }



  prep_word[j++] = '.';
  prep_word[j] = '\0';

  for (i = 0; i < j; i++)
    hyphens[i] = '0';    

#ifdef VERBOSE
  printf ("prep_word = %s\n", prep_word);
#endif

  /* now, run the finite state machine */
  state = 0;
  for (i = 0; i < j; i++)
    {
      ch = prep_word[i];
      for (;;)
	{

	  if (state == -1) {
            /* return 1; */
	    /*  KBH: FIXME shouldn't this be as follows? */
            state = 0;
            goto try_next_letter;
          }          

#ifdef VERBOSE
	  char *state_str;
	  state_str = get_state_str (state, 1);

	  for (k = 0; k < i - strlen (state_str); k++)
	    putchar (' ');
	  printf ("%s", state_str);
#endif

	  hstate = &dict->states[state];
	  for (k = 0; k < hstate->num_trans; k++)
	    if (hstate->trans[k].ch == ch)
	      {
		state = hstate->trans[k].new_state;
		goto found_state;
	      }
	  state = hstate->fallback_state;
#ifdef VERBOSE
	  printf (" falling back, fallback_state %d\n", state);
#endif
	}
    found_state:
#ifdef VERBOSE
      printf ("found state %d\n",state);
#endif
      /* Additional optimization is possible here - especially,
	 elimination of trailing zeroes from the match. Leading zeroes
	 have already been optimized. */
      match = dict->states[state].match;
      repl = dict->states[state].repl;
      replindex = dict->states[state].replindex;
      replcut = dict->states[state].replcut;
      /* replacing rules not handled by hyphen_hyphenate() */
      if (match)
	{
	  offset = i + 1 - strlen (match);
#ifdef VERBOSE
	  for (k = 0; k < offset; k++)
	    putchar (' ');
	  printf ("%s (%s)\n", match, repl);
#endif
          if (repl) {
            if (!isrepl) for(; isrepl < word_size; isrepl++) {
                matchrepl[isrepl] = NULL;
                matchindex[isrepl] = -1;
            }
            matchlen[offset + replindex] = replcut;
          }
	  /* This is a linear search because I tried a binary search and
	     found it to be just a teeny bit slower. */
	  for (k = 0; match[k]; k++) {
	    if ((hyphens[offset + k] < match[k])) {
	      hyphens[offset + k] = match[k];
              if (match[k]&1) {
                matchrepl[offset + k] = repl;
                if (repl && (k >= replindex) && (k <= replindex + replcut)) {
                    matchindex[offset + replindex] = offset + k;
                }
              }
            }
          }
          
	}

      /* KBH: we need this to make sure we keep looking in a word */
      /* for patterns even if the current character is not known in state 0 */
      /* since patterns for hyphenation may occur anywhere in the word */
      try_next_letter: ;

    }
#ifdef VERBOSE
  for (i = 0; i < j; i++)
    putchar (hyphens[i]);
  putchar ('\n');
#endif

  for (i = 0; i < j - 3; i++)
#if 0
    if (hyphens[i + 1] & 1)
      hyphens[i] = '-';
#else
    hyphens[i] = hyphens[i + 1];
#endif
  for (; i < word_size; i++)
    hyphens[i] = '0';
  hyphens[word_size] = '\0';

       /* now create a new char string showing hyphenation positions */
       /* count the hyphens and allocate space for the new hyphenated string */
       nHyphCount = 0;
       for (i = 0; i < word_size; i++)
          if (hyphens[i]&1)
             nHyphCount++;
       j = 0;
       for (i = 0; i < word_size; i++) {
           if (isrepl && (matchindex[i] >= 0) && matchrepl[matchindex[i]]) { 
                if (rep && pos && cut) {
                    if (!*rep)
                        *rep = (char **) calloc(word_size, sizeof(char *));
                    if (!*pos)
                        *pos = (int *) calloc(word_size, sizeof(int));
                    if (!*cut) {
                        *cut = (int *) calloc(word_size, sizeof(int));
                    }
                    (*rep)[matchindex[i] - 1] = hnj_strdup(matchrepl[matchindex[i]]);
                    (*pos)[matchindex[i] - 1] = matchindex[i] - i;
                    (*cut)[matchindex[i] - 1] = matchlen[i];
                }
                j += strlen(matchrepl[matchindex[i]]);
                i += matchlen[i] - 1;
          }
       }

  hnj_free (matchrepl);
  hnj_free (matchlen);
  hnj_free (matchindex);

  /* recursive hyphenation of the first (compound) level segments */
  if (dict->nextlevel) {
     char ** rep2;
     int * pos2;
     int * cut2;
     char * hyphens2;
     int begin = 0;

     rep2 = (char**) hnj_malloc (word_size * sizeof(char *));
     pos2 = (int*) hnj_malloc (word_size * sizeof(int));
     cut2 = (int*) hnj_malloc (word_size * sizeof(int));
     hyphens2 = (char*) hnj_malloc (word_size + 3);
     for (i = 0; i < word_size; i++) rep2[i] = NULL;
     for (i = 0; i < word_size; i++) if 
        (hyphens[i]&1 || (begin > 0 && i + 1 == word_size)) {
        if (i - begin > 0) {
            int hyph = 0;
            prep_word[i + 2] = '\0';
            /* non-standard hyphenation at compound boundary (Schiffahrt) */
            if (rep && *rep && *pos && *cut && (*rep)[i]) {
                char * l = strchr((*rep)[i], '=');
                size_t offset = 2 + i - (*pos)[i];
                strncpy(prep_word + offset, (*rep)[i], prep_word_size - offset - 1);
                prep_word[prep_word_size - 1] = '\0';
                if (l) {
                    hyph = (l - (*rep)[i]) - (*pos)[i];
                    prep_word[2 + i + hyph] = '\0';
                }
            }
            hnj_hyphen_hyph_(dict, prep_word + begin + 1, i - begin + 1 + hyph,
                hyphens2, &rep2, &pos2, &cut2, clhmin,
                crhmin, (begin > 0 ? 0 : lend), (hyphens[i]&1 ? 0 : rend));
            for (j = 0; j < i - begin; j++) {
                hyphens[begin + j] = hyphens2[j];
                if (rep2[j] && rep && pos && cut) {
                    if (!*rep && !*pos && !*cut) {
                        int k;
                        *rep = (char **) malloc(sizeof(char *) * word_size);
                        *pos = (int *) malloc(sizeof(int) * word_size);
                        *cut = (int *) malloc(sizeof(int) * word_size);
                        for (k = 0; k < word_size; k++) {
                            (*rep)[k] = NULL;
                            (*pos)[k] = 0;
                            (*cut)[k] = 0;
                        }
                    }
                    (*rep)[begin + j] = rep2[j];
                    (*pos)[begin + j] = pos2[j];
                    (*cut)[begin + j] = cut2[j];
                }
            }
            prep_word[i + 2] = word[i + 1];
            if (*rep && *pos && *cut && (*rep)[i]) {
                size_t offset = 1;
                strncpy(prep_word + offset, word, prep_word_size - offset - 1);
                prep_word[prep_word_size - 1] = '\0';
            }
        }
        begin = i + 1;
        for (j = 0; j < word_size; j++) rep2[j] = NULL;
     }
     
     /* non-compound */
     if (begin == 0) {
        hnj_hyphen_hyph_(dict->nextlevel, word, word_size,
            hyphens, rep, pos, cut, clhmin, crhmin, lend, rend);
        if (!lend) hnj_hyphen_lhmin(dict->utf8, word, word_size, hyphens,
            rep, pos, cut, clhmin);
        if (!rend) hnj_hyphen_rhmin(dict->utf8, word, word_size, hyphens,
            rep, pos, cut, crhmin);
     }
     
     free(rep2);
     free(cut2);
     free(pos2);
     free(hyphens2);
  }

  hnj_free (prep_word);
  return 0;
}

/* UTF-8 normalization of hyphen and non-standard positions */
int hnj_hyphen_norm(const char *word, int word_size, char * hyphens,
	char *** rep, int ** pos, int ** cut)
{
  int i, j, k;
  if ((((unsigned char) word[0]) >> 6) == 2) {
    fprintf(stderr, "error - bad, non UTF-8 input: %s\n", word);
    return 1;
  }

  /* calculate UTF-8 character positions */
  for (i = 0, j = -1; i < word_size; i++) {
    /* beginning of an UTF-8 character (not '10' start bits) */
    if ((((unsigned char) word[i]) >> 6) != 2) j++;
    hyphens[j] = hyphens[i];
    if (rep && pos && cut && *rep && *pos && *cut) {
        int l = (*pos)[i];
        (*pos)[j] = 0;
        for (k = 0; k < l; k++) {
            if ((((unsigned char) word[i - k]) >> 6) != 2) (*pos)[j]++;
        }
        k = i - l + 1;
        l = k + (*cut)[i];
        (*cut)[j] = 0;        
        for (; k < l; k++) {
            if ((((unsigned char) word[k]) >> 6) != 2) (*cut)[j]++;
        }
        (*rep)[j] = (*rep)[i];
        if (j < i) {
            (*rep)[i] = NULL;
            (*pos)[i] = 0;
            (*cut)[i] = 0;
        }
    }
  }
  hyphens[j + 1] = '\0';
#ifdef VERBOSE
  printf ("nums: %s\n", hyphens);
#endif
  return 0;
}

/* get the word with all possible hyphenations (output: hyphword) */
void hnj_hyphen_hyphword(const char * word, int word_size, const char * hyphens,
    char * hyphword, char *** rep, int ** pos, int ** cut)
{
  
  if (word_size <= 0 || word_size > INT_MAX / 2) {
    hyphword[0] = '\0';
    return;
  }
  
  /* hyphword buffer size must be at least 2 * l */
  int hyphword_size = 2 * word_size - 1;

  int nonstandard = 0;
  if (*rep && *pos && *cut) {
    nonstandard = 1;
  }

  int i;
  int j = 0;
  for (i = 0; i < word_size && j < hyphword_size; i++) {
    hyphword[j++] = word[i];
    if (hyphens[i]&1 && j < hyphword_size) {
      if (nonstandard && (*rep)[i] && j >= (*pos)[i]) {
        /* non-standard */
        j -= (*pos)[i];
        char *s = (*rep)[i];
        while (*s && j < hyphword_size) {
          hyphword[j++] = *s++;
        }
        i += (*cut)[i] - (*pos)[i];
      } else {
        /* standard */
        hyphword[j++] = '=';
      }
    }
  }
  hyphword[j] = '\0';
}


/* main api function with default hyphenmin parameters */
int hnj_hyphen_hyphenate2 (HyphenDict *dict,
			   const char *word, int word_size, char * hyphens,
			   char *hyphword, char *** rep, int ** pos, int ** cut)
{
  hnj_hyphen_hyph_(dict, word, word_size, hyphens, rep, pos, cut,
    dict->clhmin, dict->crhmin, 1, 1);
  hnj_hyphen_lhmin(dict->utf8, word, word_size,
    hyphens, rep, pos, cut, (dict->lhmin > 0 ? dict->lhmin : 2));
  hnj_hyphen_rhmin(dict->utf8, word, word_size,
    hyphens, rep, pos, cut, (dict->rhmin > 0 ? dict->rhmin : 2));

  /* nohyphen */
  if (dict->nohyphen) {
    char * nh = dict->nohyphen;
    int nhi;
    for (nhi = 0; nhi <= dict->nohyphenl; nhi++) {
        char * nhy = (char *) strstr(word, nh);
        while (nhy) {
            hyphens[nhy - word + strlen(nh) - 1] = '0';
            if (nhy - word  - 1 >= 0) hyphens[nhy - word - 1] = '0';
            nhy = (char *) strstr(nhy + 1, nh);
        }
        nh = nh + strlen(nh) + 1;
    }
  }

  if (hyphword) hnj_hyphen_hyphword(word, word_size, hyphens, hyphword, rep, pos, cut);
  if (dict->utf8) return hnj_hyphen_norm(word, word_size, hyphens, rep, pos, cut);
#ifdef VERBOSE
  printf ("nums: %s\n", hyphens);
#endif
  return 0;
}

/* previous main api function with hyphenmin parameters */
int hnj_hyphen_hyphenate3 (HyphenDict *dict,
	const char *word, int word_size, char * hyphens,
	char *hyphword, char *** rep, int ** pos, int ** cut,
	int lhmin, int rhmin, int clhmin, int crhmin)
{
  lhmin = (lhmin > dict->lhmin) ? lhmin : dict->lhmin;
  rhmin = (rhmin > dict->rhmin) ? rhmin : dict->rhmin;
  clhmin = (clhmin > dict->clhmin) ? clhmin : dict->clhmin;
  crhmin = (crhmin > dict->crhmin) ? crhmin : dict->crhmin;
  hnj_hyphen_hyph_(dict, word, word_size, hyphens, rep, pos, cut,
    clhmin, crhmin, 1, 1);
  hnj_hyphen_lhmin(dict->utf8, word, word_size, hyphens,
    rep, pos, cut, (lhmin > 0 ? lhmin : 2));
  hnj_hyphen_rhmin(dict->utf8, word, word_size, hyphens,
    rep, pos, cut, (rhmin > 0 ? rhmin : 2));
  if (hyphword) hnj_hyphen_hyphword(word, word_size, hyphens, hyphword, rep, pos, cut);

  /* nohyphen */
  if (dict->nohyphen) {
    char * nh = dict->nohyphen;
    int nhi;
    for (nhi = 0; nhi <= dict->nohyphenl; nhi++) {
        char * nhy = (char *) strstr(word, nh);
        while (nhy) {
            hyphens[nhy - word + strlen(nh) - 1] = 0;
            if (nhy - word  - 1 >= 0) hyphens[nhy - word - 1] = 0;
            nhy = (char *) strstr(nhy + 1, nh);
        }
        nh = nh + strlen(nh) + 1;
    }
  }

  if (dict->utf8) return hnj_hyphen_norm(word, word_size, hyphens, rep, pos, cut);
  return 0;
}
