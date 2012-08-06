/*
 * Copyright © 2009  Red Hat, Inc.
 * Copyright © 2011  Codethink Limited
 * Copyright © 2010,2011,2012  Google, Inc.
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

static hb_unicode_combining_class_t
hb_unicode_combining_class_nil (hb_unicode_funcs_t *ufuncs    HB_UNUSED,
				hb_codepoint_t      unicode   HB_UNUSED,
				void               *user_data HB_UNUSED)
{
  return HB_UNICODE_COMBINING_CLASS_NOT_REORDERED;
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
  return false;
}

static hb_bool_t
hb_unicode_decompose_nil (hb_unicode_funcs_t *ufuncs    HB_UNUSED,
			  hb_codepoint_t      ab        HB_UNUSED,
			  hb_codepoint_t     *a         HB_UNUSED,
			  hb_codepoint_t     *b         HB_UNUSED,
			  void               *user_data HB_UNUSED)
{
  return false;
}


static unsigned int
hb_unicode_decompose_compatibility_nil (hb_unicode_funcs_t *ufuncs     HB_UNUSED,
					hb_codepoint_t      u          HB_UNUSED,
					hb_codepoint_t     *decomposed HB_UNUSED,
					void               *user_data  HB_UNUSED)
{
  return 0;
}


hb_unicode_funcs_t *
hb_unicode_funcs_get_default (void)
{
  return const_cast<hb_unicode_funcs_t *> (&_hb_unicode_funcs_default);
}

hb_unicode_funcs_t *
hb_unicode_funcs_create (hb_unicode_funcs_t *parent)
{
  hb_unicode_funcs_t *ufuncs;

  if (!(ufuncs = hb_object_create<hb_unicode_funcs_t> ()))
    return hb_unicode_funcs_get_empty ();

  if (!parent)
    parent = hb_unicode_funcs_get_empty ();

  hb_unicode_funcs_make_immutable (parent);
  ufuncs->parent = hb_unicode_funcs_reference (parent);

  ufuncs->func = parent->func;

  /* We can safely copy user_data from parent since we hold a reference
   * onto it and it's immutable.  We should not copy the destroy notifiers
   * though. */
  ufuncs->user_data = parent->user_data;

  return ufuncs;
}


extern HB_INTERNAL const hb_unicode_funcs_t _hb_unicode_funcs_nil;
const hb_unicode_funcs_t _hb_unicode_funcs_nil = {
  HB_OBJECT_HEADER_STATIC,

  NULL, /* parent */
  true, /* immutable */
  {
#define HB_UNICODE_FUNC_IMPLEMENT(name) hb_unicode_##name##_nil,
    HB_UNICODE_FUNCS_IMPLEMENT_CALLBACKS
#undef HB_UNICODE_FUNC_IMPLEMENT
  }
};

hb_unicode_funcs_t *
hb_unicode_funcs_get_empty (void)
{
  return const_cast<hb_unicode_funcs_t *> (&_hb_unicode_funcs_nil);
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

  ufuncs->immutable = true;
}

hb_bool_t
hb_unicode_funcs_is_immutable (hb_unicode_funcs_t *ufuncs)
{
  return ufuncs->immutable;
}

hb_unicode_funcs_t *
hb_unicode_funcs_get_parent (hb_unicode_funcs_t *ufuncs)
{
  return ufuncs->parent ? ufuncs->parent : hb_unicode_funcs_get_empty ();
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
  return ufuncs->name (unicode);						\
}
HB_UNICODE_FUNCS_IMPLEMENT_CALLBACKS_SIMPLE
#undef HB_UNICODE_FUNC_IMPLEMENT

hb_bool_t
hb_unicode_compose (hb_unicode_funcs_t *ufuncs,
		    hb_codepoint_t      a,
		    hb_codepoint_t      b,
		    hb_codepoint_t     *ab)
{
  return ufuncs->compose (a, b, ab);
}

hb_bool_t
hb_unicode_decompose (hb_unicode_funcs_t *ufuncs,
		      hb_codepoint_t      ab,
		      hb_codepoint_t     *a,
		      hb_codepoint_t     *b)
{
  return ufuncs->decompose (ab, a, b);
}

unsigned int
hb_unicode_decompose_compatibility (hb_unicode_funcs_t *ufuncs,
				    hb_codepoint_t      u,
				    hb_codepoint_t     *decomposed)
{
  return ufuncs->decompose_compatibility (u, decomposed);
}


const uint8_t
_hb_modified_combining_class[256] =
{
  0, /* HB_UNICODE_COMBINING_CLASS_NOT_REORDERED */
  1, /* HB_UNICODE_COMBINING_CLASS_OVERLAY */
  2, 3, 4, 5, 6,
  7, /* HB_UNICODE_COMBINING_CLASS_NUKTA */
  8, /* HB_UNICODE_COMBINING_CLASS_KANA_VOICING */
  9, /* HB_UNICODE_COMBINING_CLASS_VIRAMA */

  /* Hebrew */

  /*
   * We permute the "fixed-position" classes 10-26 into the order
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
  22, /* HB_UNICODE_COMBINING_CLASS_CCC10 sheva */
  15, /* HB_UNICODE_COMBINING_CLASS_CCC11 hataf segol */
  16, /* HB_UNICODE_COMBINING_CLASS_CCC12 hataf patah*/
  17, /* HB_UNICODE_COMBINING_CLASS_CCC13 hataf qamats */
  23, /* HB_UNICODE_COMBINING_CLASS_CCC14 hiriq */
  18, /* HB_UNICODE_COMBINING_CLASS_CCC15 tsere */
  19, /* HB_UNICODE_COMBINING_CLASS_CCC16 segol */
  20, /* HB_UNICODE_COMBINING_CLASS_CCC17 patah */
  21, /* HB_UNICODE_COMBINING_CLASS_CCC18 qamats */
  14, /* HB_UNICODE_COMBINING_CLASS_CCC19 holam */
  24, /* HB_UNICODE_COMBINING_CLASS_CCC20 qubuts */
  12, /* HB_UNICODE_COMBINING_CLASS_CCC21 dagesh */
  25, /* HB_UNICODE_COMBINING_CLASS_CCC22 meteg */
  13, /* HB_UNICODE_COMBINING_CLASS_CCC23 rafe */
  10, /* HB_UNICODE_COMBINING_CLASS_CCC24 shin dot */
  11, /* HB_UNICODE_COMBINING_CLASS_CCC25 sin dot */

  26, /* HB_UNICODE_COMBINING_CLASS_CCC26 */

  /* Arabic */

  /*
   * Modify to move Shadda (ccc=33) before other marks.  See:
   * http://unicode.org/faq/normalization.html#8
   * http://unicode.org/faq/normalization.html#9
   */
  28, /* HB_UNICODE_COMBINING_CLASS_CCC27 */
  29, /* HB_UNICODE_COMBINING_CLASS_CCC28 */
  30, /* HB_UNICODE_COMBINING_CLASS_CCC29 */
  31, /* HB_UNICODE_COMBINING_CLASS_CCC30 */
  32, /* HB_UNICODE_COMBINING_CLASS_CCC31 */
  33, /* HB_UNICODE_COMBINING_CLASS_CCC32 */
  27, /* HB_UNICODE_COMBINING_CLASS_CCC33 shadda */

  34, /* HB_UNICODE_COMBINING_CLASS_CCC34 */
  35, /* HB_UNICODE_COMBINING_CLASS_CCC35 */

  /* Syriac */
  36, /* HB_UNICODE_COMBINING_CLASS_CCC36 */

  37, 38, 39,
  40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
  60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
  80, 81, 82, 83,

  /* Telugu */

  /*
   * Modify Telugu length marks (ccc=84, ccc=91).
   * These are the only matras in the main Indic scripts range that have
   * a non-zero ccc.  That makes them reorder with the Halant that is
   * ccc=9.  Just zero them, we don't need them in our Indic shaper.
   */
  0, /* HB_UNICODE_COMBINING_CLASS_CCC84 */
  85, 86, 87, 88, 89, 90,
  0, /* HB_UNICODE_COMBINING_CLASS_CCC91 */
  92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102,

  /* Thai */

  /*
   * Modify U+0E38 and U+0E39 (ccc=104) to be reordered before U+0E3A (ccc=9).
   * Uniscribe does this too.
   */
  3, /* HB_UNICODE_COMBINING_CLASS_CCC103 */

  104, 105, 106,
  107, /* HB_UNICODE_COMBINING_CLASS_CCC107 */
  108, 109, 110, 111, 112, 113, 114, 115, 116, 117,

  /* Lao */
  118, /* HB_UNICODE_COMBINING_CLASS_CCC118 */
  119, 120, 121,
  122, /* HB_UNICODE_COMBINING_CLASS_CCC122 */
  123, 124, 125, 126, 127, 128,

  /* Tibetan */
  129, /* HB_UNICODE_COMBINING_CLASS_CCC129 */
  130, /* HB_UNICODE_COMBINING_CLASS_CCC130 */
  131,
  132, /* HB_UNICODE_COMBINING_CLASS_CCC133 */
  133, 134, 135, 136, 137, 138, 139,


  140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
  150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
  160, 161, 162, 163, 164, 165, 166, 167, 168, 169,
  170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
  180, 181, 182, 183, 184, 185, 186, 187, 188, 189,
  190, 191, 192, 193, 194, 195, 196, 197, 198, 199,

  200, /* HB_UNICODE_COMBINING_CLASS_ATTACHED_BELOW_LEFT */
  201,
  202, /* HB_UNICODE_COMBINING_CLASS_ATTACHED_BELOW */
  203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213,
  214, /* HB_UNICODE_COMBINING_CLASS_ATTACHED_ABOVE */
  215,
  216, /* HB_UNICODE_COMBINING_CLASS_ATTACHED_ABOVE_RIGHT */
  217,
  218, /* HB_UNICODE_COMBINING_CLASS_BELOW_LEFT */
  219,
  220, /* HB_UNICODE_COMBINING_CLASS_BELOW */
  221,
  222, /* HB_UNICODE_COMBINING_CLASS_BELOW_RIGHT */
  223,
  224, /* HB_UNICODE_COMBINING_CLASS_LEFT */
  225,
  226, /* HB_UNICODE_COMBINING_CLASS_RIGHT */
  227,
  228, /* HB_UNICODE_COMBINING_CLASS_ABOVE_LEFT */
  229,
  230, /* HB_UNICODE_COMBINING_CLASS_ABOVE */
  231,
  232, /* HB_UNICODE_COMBINING_CLASS_ABOVE_RIGHT */
  233, /* HB_UNICODE_COMBINING_CLASS_DOUBLE_BELOW */
  234, /* HB_UNICODE_COMBINING_CLASS_DOUBLE_ABOVE */
  235, 236, 237, 238, 239,
  240, /* HB_UNICODE_COMBINING_CLASS_IOTA_SUBSCRIPT */
  241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
  255, /* HB_UNICODE_COMBINING_CLASS_INVALID */
};
