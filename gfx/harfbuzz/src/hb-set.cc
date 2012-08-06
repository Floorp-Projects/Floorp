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

#include "hb-set-private.hh"



/* Public API */


hb_set_t *
hb_set_create ()
{
  hb_set_t *set;

  if (!(set = hb_object_create<hb_set_t> ()))
    return hb_set_get_empty ();

  set->clear ();

  return set;
}

hb_set_t *
hb_set_get_empty (void)
{
  static const hb_set_t _hb_set_nil = {
    HB_OBJECT_HEADER_STATIC,

    {0} /* elts */
  };

  return const_cast<hb_set_t *> (&_hb_set_nil);
}

hb_set_t *
hb_set_reference (hb_set_t *set)
{
  return hb_object_reference (set);
}

void
hb_set_destroy (hb_set_t *set)
{
  if (!hb_object_destroy (set)) return;

  set->fini ();

  free (set);
}

hb_bool_t
hb_set_set_user_data (hb_set_t        *set,
			 hb_user_data_key_t *key,
			 void *              data,
			 hb_destroy_func_t   destroy,
			 hb_bool_t           replace)
{
  return hb_object_set_user_data (set, key, data, destroy, replace);
}

void *
hb_set_get_user_data (hb_set_t        *set,
			 hb_user_data_key_t *key)
{
  return hb_object_get_user_data (set, key);
}


hb_bool_t
hb_set_allocation_successful (hb_set_t  *set HB_UNUSED)
{
  return true;
}

void
hb_set_clear (hb_set_t *set)
{
  set->clear ();
}

hb_bool_t
hb_set_empty (hb_set_t *set)
{
  return set->empty ();
}

hb_bool_t
hb_set_has (hb_set_t       *set,
	    hb_codepoint_t  codepoint)
{
  return set->has (codepoint);
}

void
hb_set_add (hb_set_t       *set,
	    hb_codepoint_t  codepoint)
{
  set->add (codepoint);
}

void
hb_set_del (hb_set_t       *set,
	    hb_codepoint_t  codepoint)
{
  set->del (codepoint);
}

hb_bool_t
hb_set_equal (hb_set_t *set,
	      hb_set_t *other)
{
  return set->equal (other);
}

void
hb_set_set (hb_set_t *set,
	    hb_set_t *other)
{
  set->set (other);
}

void
hb_set_union (hb_set_t *set,
	      hb_set_t *other)
{
  set->union_ (other);
}

void
hb_set_intersect (hb_set_t *set,
		  hb_set_t *other)
{
  set->intersect (other);
}

void
hb_set_subtract (hb_set_t *set,
		 hb_set_t *other)
{
  set->subtract (other);
}

void
hb_set_symmetric_difference (hb_set_t *set,
			     hb_set_t *other)
{
  set->symmetric_difference (other);
}

hb_codepoint_t
hb_set_min (hb_set_t *set)
{
  return set->get_min ();
}

hb_codepoint_t
hb_set_max (hb_set_t *set)
{
  return set->get_max ();
}

hb_bool_t
hb_set_next (hb_set_t       *set,
	     hb_codepoint_t *codepoint)
{
  return set->next (codepoint);
}
