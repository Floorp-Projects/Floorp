/*
 * Copyright © 2009  Red Hat, Inc.
 * Copyright © 2011 Codethink Limited
 * Copyright © 2010,2011  Google, Inc.
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
 * Red Hat Author(s): Behdad Esfahbod
 * Codethink Author(s): Ryan Lortie
 * Google Author(s): Behdad Esfahbod
 */

#include "hb-private.hh"

#include "hb-unicode-private.hh"



/*
 * hb_unicode_funcs_t
 */

static unsigned int
hb_unicode_combining_class_nil (hb_unicode_funcs_t *ufuncs    HB_UNUSED,
				hb_codepoint_t      unicode   HB_UNUSED,
				void               *user_data HB_UNUSED)
{
  return 0;
}

static unsigned int
hb_unicode_eastasian_width_nil (hb_unicode_funcs_t *ufuncs    HB_UNUSED,
				hb_codepoint_t      unicode   HB_UNUSED,
				void               *user_data HB_UNUSED)
{
  return 1;
}

static hb_unicode_general_category_t
hb_unicode_general_category_nil (hb_unicode_funcs_t *ufuncs    HB_UNUSED,
				 hb_codepoint_t      unicode   HB_UNUSED,
				 void               *user_data HB_UNUSED)
{
  return HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER;
}

static hb_codepoint_t
hb_unicode_mirroring_nil (hb_unicode_funcs_t *ufuncs    HB_UNUSED,
			  hb_codepoint_t      unicode   HB_UNUSED,
			  void               *user_data HB_UNUSED)
{
  return unicode;
}

static hb_script_t
hb_unicode_script_nil (hb_unicode_funcs_t *ufuncs    HB_UNUSED,
		       hb_codepoint_t      unicode   HB_UNUSED,
		       void               *user_data HB_UNUSED)
{
  return HB_SCRIPT_UNKNOWN;
}

static hb_bool_t
hb_unicode_compose_nil (hb_unicode_funcs_t *ufuncs    HB_UNUSED,
			hb_codepoint_t      a         HB_UNUSED,
			hb_codepoint_t      b         HB_UNUSED,
			hb_codepoint_t     *ab        HB_UNUSED,
			void               *user_data HB_UNUSED)
{
  /* TODO handle Hangul jamo here? */
  return FALSE;
}

static hb_bool_t
hb_unicode_decompose_nil (hb_unicode_funcs_t *ufuncs    HB_UNUSED,
			  hb_codepoint_t      ab        HB_UNUSED,
			  hb_codepoint_t     *a         HB_UNUSED,
			  hb_codepoint_t     *b         HB_UNUSED,
			  void               *user_data HB_UNUSED)
{
  /* TODO handle Hangul jamo here? */
  return FALSE;
}


hb_unicode_funcs_t _hb_unicode_funcs_nil = {
  HB_OBJECT_HEADER_STATIC,

  NULL, /* parent */
  TRUE, /* immutable */
  {
#define HB_UNICODE_FUNC_IMPLEMENT(name) hb_unicode_##name##_nil,
    HB_UNICODE_FUNCS_IMPLEMENT_CALLBACKS
#undef HB_UNICODE_FUNC_IMPLEMENT
  }
};


hb_unicode_funcs_t *
hb_unicode_funcs_get_default (void)
{
  return &_hb_unicode_funcs_default;
}

hb_unicode_funcs_t *
hb_unicode_funcs_create (hb_unicode_funcs_t *parent)
{
  hb_unicode_funcs_t *ufuncs;

  if (!(ufuncs = hb_object_create<hb_unicode_funcs_t> ()))
    return &_hb_unicode_funcs_nil;

  if (!parent)
    parent = &_hb_unicode_funcs_nil;

  hb_unicode_funcs_make_immutable (parent);
  ufuncs->parent = hb_unicode_funcs_reference (parent);

  ufuncs->func = parent->func;

  /* We can safely copy user_data from parent since we hold a reference
   * onto it and it's immutable.  We should not copy the destroy notifiers
   * though. */
  ufuncs->user_data = parent->user_data;

  return ufuncs;
}

hb_unicode_funcs_t *
hb_unicode_funcs_get_empty (void)
{
  return &_hb_unicode_funcs_nil;
}

hb_unicode_funcs_t *
hb_unicode_funcs_reference (hb_unicode_funcs_t *ufuncs)
{
  return hb_object_reference (ufuncs);
}

void
hb_unicode_funcs_destroy (hb_unicode_funcs_t *ufuncs)
{
  if (!hb_object_destroy (ufuncs)) return;

#define HB_UNICODE_FUNC_IMPLEMENT(name) \
  if (ufuncs->destroy.name) ufuncs->destroy.name (ufuncs->user_data.name);
    HB_UNICODE_FUNCS_IMPLEMENT_CALLBACKS
#undef HB_UNICODE_FUNC_IMPLEMENT

  hb_unicode_funcs_destroy (ufuncs->parent);

  free (ufuncs);
}

hb_bool_t
hb_unicode_funcs_set_user_data (hb_unicode_funcs_t *ufuncs,
			        hb_user_data_key_t *key,
			        void *              data,
			        hb_destroy_func_t   destroy,
				hb_bool_t           replace)
{
  return hb_object_set_user_data (ufuncs, key, data, destroy, replace);
}

void *
hb_unicode_funcs_get_user_data (hb_unicode_funcs_t *ufuncs,
			        hb_user_data_key_t *key)
{
  return hb_object_get_user_data (ufuncs, key);
}


void
hb_unicode_funcs_make_immutable (hb_unicode_funcs_t *ufuncs)
{
  if (hb_object_is_inert (ufuncs))
    return;

  ufuncs->immutable = TRUE;
}

hb_bool_t
hb_unicode_funcs_is_immutable (hb_unicode_funcs_t *ufuncs)
{
  return ufuncs->immutable;
}

hb_unicode_funcs_t *
hb_unicode_funcs_get_parent (hb_unicode_funcs_t *ufuncs)
{
  return ufuncs->parent ? ufuncs->parent : &_hb_unicode_funcs_nil;
}


#define HB_UNICODE_FUNC_IMPLEMENT(name)						\
										\
void										\
hb_unicode_funcs_set_##name##_func (hb_unicode_funcs_t		   *ufuncs,	\
				    hb_unicode_##name##_func_t	    func,	\
				    void			   *user_data,	\
				    hb_destroy_func_t		    destroy)	\
{										\
  if (ufuncs->immutable)							\
    return;									\
										\
  if (ufuncs->destroy.name)							\
    ufuncs->destroy.name (ufuncs->user_data.name);				\
										\
  if (func) {									\
    ufuncs->func.name = func;							\
    ufuncs->user_data.name = user_data;						\
    ufuncs->destroy.name = destroy;						\
  } else {									\
    ufuncs->func.name = ufuncs->parent->func.name;				\
    ufuncs->user_data.name = ufuncs->parent->user_data.name;			\
    ufuncs->destroy.name = NULL;						\
  }										\
}

    HB_UNICODE_FUNCS_IMPLEMENT_CALLBACKS
#undef HB_UNICODE_FUNC_IMPLEMENT


#define HB_UNICODE_FUNC_IMPLEMENT(return_type, name)				\
										\
return_type									\
hb_unicode_##name (hb_unicode_funcs_t *ufuncs,					\
		   hb_codepoint_t      unicode)					\
{										\
  return ufuncs->func.name (ufuncs, unicode, ufuncs->user_data.name);		\
}
    HB_UNICODE_FUNCS_IMPLEMENT_CALLBACKS_SIMPLE
#undef HB_UNICODE_FUNC_IMPLEMENT

hb_bool_t
hb_unicode_compose (hb_unicode_funcs_t *ufuncs,
		    hb_codepoint_t      a,
		    hb_codepoint_t      b,
		    hb_codepoint_t     *ab)
{
  *ab = 0;
  return ufuncs->func.compose (ufuncs, a, b, ab, ufuncs->user_data.compose);
}

hb_bool_t
hb_unicode_decompose (hb_unicode_funcs_t *ufuncs,
		      hb_codepoint_t      ab,
		      hb_codepoint_t     *a,
		      hb_codepoint_t     *b)
{
  *a = ab; *b = 0;
  return ufuncs->func.decompose (ufuncs, ab, a, b, ufuncs->user_data.decompose);
}



unsigned int
_hb_unicode_modified_combining_class (hb_unicode_funcs_t *ufuncs,
				      hb_codepoint_t      unicode)
{
  int c = hb_unicode_combining_class (ufuncs, unicode);

  if (unlikely (hb_in_range<int> (c, 27, 33)))
  {
    /* Modify the combining-class to suit Arabic better.  See:
     * http://unicode.org/faq/normalization.html#8
     * http://unicode.org/faq/normalization.html#9
     */
    c = c == 33 ? 27 : c + 1;
  }
  else if (unlikely (hb_in_range<int> (c, 10, 25)))
  {
    /* The equivalent fix for Hebrew is more complex.
     *
     * We permute the "fixed-position" classes 10-25 into the order
     * described in the SBL Hebrew manual:
     *
     * http://www.sbl-site.org/Fonts/SBLHebrewUserManual1.5x.pdf
     *
     * (as recommended by:
     *  http://forum.fontlab.com/archive-old-microsoft-volt-group/vista-and-diacritic-ordering-t6751.0.html)
     *
     * More details here:
     * https://bugzilla.mozilla.org/show_bug.cgi?id=662055
     */
    static const int permuted_hebrew_classes[25 - 10 + 1] = {
      /* 10 sheva */        22,
      /* 11 hataf segol */  15,
      /* 12 hataf patah */  16,
      /* 13 hataf qamats */ 17,
      /* 14 hiriq */        23,
      /* 15 tsere */        18,
      /* 16 segol */        19,
      /* 17 patah */        20,
      /* 18 qamats */       21,
      /* 19 holam */        14,
      /* 20 qubuts */       24,
      /* 21 dagesh */       12,
      /* 22 meteg */        25,
      /* 23 rafe */         13,
      /* 24 shin dot */     10,
      /* 25 sin dot */      11,
    };
    c = permuted_hebrew_classes[c - 10];
  }

  return c;
}

