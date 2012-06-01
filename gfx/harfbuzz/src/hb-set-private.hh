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

#ifndef HB_SET_PRIVATE_HH
#define HB_SET_PRIVATE_HH

#include "hb-private.hh"
#include "hb-set.h"
#include "hb-object-private.hh"


/* TODO Make this faster and memmory efficient. */

struct _hb_set_t
{
  inline void init (void) {
    clear ();
  }
  inline void fini (void) {
  }
  inline void clear (void) {
    memset (elts, 0, sizeof elts);
  }
  inline bool empty (void) const {
    for (unsigned int i = 0; i < ARRAY_LENGTH (elts); i++)
      if (elts[i])
        return false;
    return true;
  }
  inline void add (hb_codepoint_t g)
  {
    if (unlikely (g == SENTINEL)) return;
    if (unlikely (g > MAX_G)) return;
    elt (g) |= mask (g);
  }
  inline void del (hb_codepoint_t g)
  {
    if (unlikely (g > MAX_G)) return;
    elt (g) &= ~mask (g);
  }
  inline bool has (hb_codepoint_t g) const
  {
    if (unlikely (g > MAX_G)) return false;
    return !!(elt (g) & mask (g));
  }
  inline bool intersects (hb_codepoint_t first,
			  hb_codepoint_t last) const
  {
    if (unlikely (first > MAX_G)) return false;
    if (unlikely (last  > MAX_G)) last = MAX_G;
    unsigned int end = last + 1;
    for (hb_codepoint_t i = first; i < end; i++)
      if (has (i))
        return true;
    return false;
  }
  inline bool equal (const hb_set_t *other) const
  {
    for (unsigned int i = 0; i < ELTS; i++)
      if (elts[i] != other->elts[i])
        return false;
    return true;
  }
  inline void set (const hb_set_t *other)
  {
    for (unsigned int i = 0; i < ELTS; i++)
      elts[i] = other->elts[i];
  }
  inline void union_ (const hb_set_t *other)
  {
    for (unsigned int i = 0; i < ELTS; i++)
      elts[i] |= other->elts[i];
  }
  inline void intersect (const hb_set_t *other)
  {
    for (unsigned int i = 0; i < ELTS; i++)
      elts[i] &= other->elts[i];
  }
  inline void subtract (const hb_set_t *other)
  {
    for (unsigned int i = 0; i < ELTS; i++)
      elts[i] &= ~other->elts[i];
  }
  inline void symmetric_difference (const hb_set_t *other)
  {
    for (unsigned int i = 0; i < ELTS; i++)
      elts[i] ^= other->elts[i];
  }
  inline bool next (hb_codepoint_t *codepoint)
  {
    if (unlikely (*codepoint == SENTINEL)) {
      hb_codepoint_t i = get_min ();
      if (i != SENTINEL) {
        *codepoint = i;
	return true;
      } else
        return false;
    }
    for (hb_codepoint_t i = *codepoint + 1; i < MAX_G + 1; i++)
      if (has (i)) {
        *codepoint = i;
	return true;
      }
    return false;
  }
  inline hb_codepoint_t get_min (void) const
  {
    for (unsigned int i = 0; i < ELTS; i++)
      if (elts[i])
	for (unsigned int j = 0; i < BITS; j++)
	  if (elts[i] & (1 << j))
	    return i * BITS + j;
    return SENTINEL;
  }
  inline hb_codepoint_t get_max (void) const
  {
    for (unsigned int i = ELTS; i; i--)
      if (elts[i - 1])
	for (unsigned int j = BITS; j; j--)
	  if (elts[i - 1] & (1 << (j - 1)))
	    return (i - 1) * BITS + (j - 1);
    return SENTINEL;
  }

  typedef uint32_t elt_t;
  static const unsigned int MAX_G = 65536 - 1; /* XXX Fix this... */
  static const unsigned int SHIFT = 5;
  static const unsigned int BITS = (1 << SHIFT);
  static const unsigned int MASK = BITS - 1;
  static const unsigned int ELTS = (MAX_G + 1 + (BITS - 1)) / BITS;
  static  const hb_codepoint_t SENTINEL = (hb_codepoint_t) -1;

  elt_t &elt (hb_codepoint_t g) { return elts[g >> SHIFT]; }
  elt_t elt (hb_codepoint_t g) const { return elts[g >> SHIFT]; }
  elt_t mask (hb_codepoint_t g) const { return elt_t (1) << (g & MASK); }

  hb_object_header_t header;
  elt_t elts[ELTS]; /* XXX 8kb */

  ASSERT_STATIC (sizeof (elt_t) * 8 == BITS);
  ASSERT_STATIC (sizeof (elt_t) * 8 * ELTS > MAX_G);
};



#endif /* HB_SET_PRIVATE_HH */
