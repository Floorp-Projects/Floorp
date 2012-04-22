/*
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
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_SHAPE_COMPLEX_PRIVATE_HH
#define HB_OT_SHAPE_COMPLEX_PRIVATE_HH

#include "hb-private.hh"

#include "hb-ot-map-private.hh"
#include "hb-ot-shape-normalize-private.hh"



/* buffer var allocations, used during the entire shaping process */
#define general_category() var1.u8[0] /* unicode general_category (hb_unicode_general_category_t) */
#define combining_class() var1.u8[1] /* unicode combining_class (uint8_t) */

/* buffer var allocations, used by complex shapers */
#define complex_var_persistent_u8_0()	var2.u8[0]
#define complex_var_persistent_u8_1()	var2.u8[1]
#define complex_var_persistent_u16()	var2.u16[0]
#define complex_var_temporary_u8_0()	var2.u8[2]
#define complex_var_temporary_u8_1()	var2.u8[3]
#define complex_var_temporary_u16()	var2.u16[1]


#define HB_COMPLEX_SHAPERS_IMPLEMENT_SHAPERS \
  HB_COMPLEX_SHAPER_IMPLEMENT (default) /* should be first */ \
  HB_COMPLEX_SHAPER_IMPLEMENT (arabic) \
  HB_COMPLEX_SHAPER_IMPLEMENT (hangul) \
  HB_COMPLEX_SHAPER_IMPLEMENT (indic) \
  HB_COMPLEX_SHAPER_IMPLEMENT (thai) \
  /* ^--- Add new shapers here */

enum hb_ot_complex_shaper_t {
#define HB_COMPLEX_SHAPER_IMPLEMENT(name) hb_ot_complex_shaper_##name,
  HB_COMPLEX_SHAPERS_IMPLEMENT_SHAPERS
  /* Just here to avoid enum trailing comma: */
  hb_ot_complex_shaper_generic = hb_ot_complex_shaper_default
#undef HB_COMPLEX_SHAPER_IMPLEMENT
};

static inline hb_ot_complex_shaper_t
hb_ot_shape_complex_categorize (const hb_segment_properties_t *props)
{
  switch ((int) props->script)
  {
    default:
      return hb_ot_complex_shaper_default;


    /* Unicode-1.1 additions */
    case HB_SCRIPT_ARABIC:
    case HB_SCRIPT_MONGOLIAN:
    case HB_SCRIPT_SYRIAC:

    /* Unicode-5.0 additions */
    case HB_SCRIPT_NKO:

    /* Unicode-6.0 additions */
    case HB_SCRIPT_MANDAIC:

      return hb_ot_complex_shaper_arabic;


    /* Unicode-1.1 additions */
    case HB_SCRIPT_HANGUL:

      return hb_ot_complex_shaper_hangul;


    /* Unicode-1.1 additions */
    case HB_SCRIPT_THAI:
    case HB_SCRIPT_LAO:

      return hb_ot_complex_shaper_thai;



    /* ^--- Add new shapers here */


#if 0
    /* Note:
     *
     * These disabled scripts are listed in ucd/IndicSyllabicCategory.txt, but according
     * to Martin Hosken and Jonathan Kew do not require complex shaping.
     *
     * TODO We should automate figuring out which scripts do not need complex shaping
     *
     * TODO We currently keep data for these scripts in our indic table.  Need to fix the
     * generator to not do that.
     */


    /* Simple? */

    /* Unicode-3.2 additions */
    case HB_SCRIPT_BUHID:
    case HB_SCRIPT_HANUNOO:

    /* Unicode-5.1 additions */
    case HB_SCRIPT_SAURASHTRA:

    /* Unicode-5.2 additions */
    case HB_SCRIPT_MEETEI_MAYEK:

    /* Unicode-6.0 additions */
    case HB_SCRIPT_BATAK:
    case HB_SCRIPT_BRAHMI:


    /* Simple */

    /* Unicode-1.1 additions */
    /* TODO These two need their own shaper I guess? */
    case HB_SCRIPT_LAO:
    case HB_SCRIPT_THAI:

    /* Unicode-2.0 additions */
    case HB_SCRIPT_TIBETAN:

    /* Unicode-3.2 additions */
    case HB_SCRIPT_TAGALOG:
    case HB_SCRIPT_TAGBANWA:

    /* Unicode-4.0 additions */
    case HB_SCRIPT_LIMBU:
    case HB_SCRIPT_TAI_LE:

    /* Unicode-4.1 additions */
    case HB_SCRIPT_SYLOTI_NAGRI:

    /* Unicode-5.0 additions */
    case HB_SCRIPT_PHAGS_PA:

    /* Unicode-5.1 additions */
    case HB_SCRIPT_KAYAH_LI:

    /* Unicode-5.2 additions */
    case HB_SCRIPT_TAI_VIET:


    /* May need Indic treatment in the future? */

    /* Unicode-3.0 additions */
    case HB_SCRIPT_MYANMAR:


#endif

    /* Unicode-1.1 additions */
    case HB_SCRIPT_BENGALI:
    case HB_SCRIPT_DEVANAGARI:
    case HB_SCRIPT_GUJARATI:
    case HB_SCRIPT_GURMUKHI:
    case HB_SCRIPT_KANNADA:
    case HB_SCRIPT_MALAYALAM:
    case HB_SCRIPT_ORIYA:
    case HB_SCRIPT_TAMIL:
    case HB_SCRIPT_TELUGU:

    /* Unicode-3.0 additions */
    case HB_SCRIPT_KHMER:
    case HB_SCRIPT_SINHALA:

    /* Unicode-4.1 additions */
    case HB_SCRIPT_BUGINESE:
    case HB_SCRIPT_KHAROSHTHI:
    case HB_SCRIPT_NEW_TAI_LUE:

    /* Unicode-5.0 additions */
    case HB_SCRIPT_BALINESE:

    /* Unicode-5.1 additions */
    case HB_SCRIPT_CHAM:
    case HB_SCRIPT_LEPCHA:
    case HB_SCRIPT_REJANG:
    case HB_SCRIPT_SUNDANESE:

    /* Unicode-5.2 additions */
    case HB_SCRIPT_JAVANESE:
    case HB_SCRIPT_KAITHI:
    case HB_SCRIPT_TAI_THAM:

    /* Unicode-6.1 additions */
    case HB_SCRIPT_CHAKMA:
    case HB_SCRIPT_SHARADA:
    case HB_SCRIPT_TAKRI:

      return hb_ot_complex_shaper_indic;
  }
}



/*
 * collect_features()
 *
 * Called during shape_plan().
 *
 * Shapers should use map to add their features and callbacks.
 */

typedef void hb_ot_shape_complex_collect_features_func_t (hb_ot_map_builder_t *map, const hb_segment_properties_t  *props);
#define HB_COMPLEX_SHAPER_IMPLEMENT(name) \
  HB_INTERNAL hb_ot_shape_complex_collect_features_func_t _hb_ot_shape_complex_collect_features_##name;
  HB_COMPLEX_SHAPERS_IMPLEMENT_SHAPERS
#undef HB_COMPLEX_SHAPER_IMPLEMENT

static inline void
hb_ot_shape_complex_collect_features (hb_ot_complex_shaper_t shaper,
				      hb_ot_map_builder_t *map,
				      const hb_segment_properties_t  *props)
{
  switch (shaper) {
    default:
#define HB_COMPLEX_SHAPER_IMPLEMENT(name) \
    case hb_ot_complex_shaper_##name:	_hb_ot_shape_complex_collect_features_##name (map, props); return;
    HB_COMPLEX_SHAPERS_IMPLEMENT_SHAPERS
#undef HB_COMPLEX_SHAPER_IMPLEMENT
  }
}


/*
 * normalization_preference()
 *
 * Called during shape_execute().
 *
 * Shapers should return TRUE if it prefers decomposed (NFD) input rather than precomposed (NFC).
 */

typedef hb_ot_shape_normalization_mode_t hb_ot_shape_complex_normalization_preference_func_t (void);
#define HB_COMPLEX_SHAPER_IMPLEMENT(name) \
  HB_INTERNAL hb_ot_shape_complex_normalization_preference_func_t _hb_ot_shape_complex_normalization_preference_##name;
  HB_COMPLEX_SHAPERS_IMPLEMENT_SHAPERS
#undef HB_COMPLEX_SHAPER_IMPLEMENT

static inline hb_ot_shape_normalization_mode_t
hb_ot_shape_complex_normalization_preference (hb_ot_complex_shaper_t shaper)
{
  switch (shaper) {
    default:
#define HB_COMPLEX_SHAPER_IMPLEMENT(name) \
    case hb_ot_complex_shaper_##name:	return _hb_ot_shape_complex_normalization_preference_##name ();
    HB_COMPLEX_SHAPERS_IMPLEMENT_SHAPERS
#undef HB_COMPLEX_SHAPER_IMPLEMENT
  }
}


/* setup_masks()
 *
 * Called during shape_execute().
 *
 * Shapers should use map to get feature masks and set on buffer.
 */

typedef void hb_ot_shape_complex_setup_masks_func_t (hb_ot_map_t *map, hb_buffer_t *buffer, hb_font_t *font);
#define HB_COMPLEX_SHAPER_IMPLEMENT(name) \
  HB_INTERNAL hb_ot_shape_complex_setup_masks_func_t _hb_ot_shape_complex_setup_masks_##name;
  HB_COMPLEX_SHAPERS_IMPLEMENT_SHAPERS
#undef HB_COMPLEX_SHAPER_IMPLEMENT

static inline void
hb_ot_shape_complex_setup_masks (hb_ot_complex_shaper_t shaper,
				 hb_ot_map_t *map,
				 hb_buffer_t *buffer,
				 hb_font_t *font)
{
  switch (shaper) {
    default:
#define HB_COMPLEX_SHAPER_IMPLEMENT(name) \
    case hb_ot_complex_shaper_##name:	_hb_ot_shape_complex_setup_masks_##name (map, buffer, font); return;
    HB_COMPLEX_SHAPERS_IMPLEMENT_SHAPERS
#undef HB_COMPLEX_SHAPER_IMPLEMENT
  }
}



#endif /* HB_OT_SHAPE_COMPLEX_PRIVATE_HH */
