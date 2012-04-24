/*
 * Copyright © 2009  Red Hat, Inc.
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
 */

#ifndef HB_H_IN
#error "Include <hb.h> instead."
#endif

#ifndef HB_FONT_H
#define HB_FONT_H

#include "hb-common.h"
#include "hb-blob.h"

HB_BEGIN_DECLS


typedef struct _hb_face_t hb_face_t;
typedef struct _hb_font_t hb_font_t;

/*
 * hb_face_t
 */

hb_face_t *
hb_face_create (hb_blob_t    *blob,
		unsigned int  index);

typedef hb_blob_t * (*hb_reference_table_func_t)  (hb_face_t *face, hb_tag_t tag, void *user_data);

/* calls destroy() when not needing user_data anymore */
hb_face_t *
hb_face_create_for_tables (hb_reference_table_func_t  reference_table,
			   void                      *user_data,
			   hb_destroy_func_t          destroy);

hb_face_t *
hb_face_get_empty (void);

hb_face_t *
hb_face_reference (hb_face_t *face);

void
hb_face_destroy (hb_face_t *face);

hb_bool_t
hb_face_set_user_data (hb_face_t          *face,
		       hb_user_data_key_t *key,
		       void *              data,
		       hb_destroy_func_t   destroy,
		       hb_bool_t           replace);


void *
hb_face_get_user_data (hb_face_t          *face,
		       hb_user_data_key_t *key);

void
hb_face_make_immutable (hb_face_t *face);

hb_bool_t
hb_face_is_immutable (hb_face_t *face);


hb_blob_t *
hb_face_reference_table (hb_face_t *face,
			 hb_tag_t   tag);

hb_blob_t *
hb_face_reference_blob (hb_face_t *face);

void
hb_face_set_index (hb_face_t    *face,
		   unsigned int  index);

unsigned int
hb_face_get_index (hb_face_t    *face);

void
hb_face_set_upem (hb_face_t    *face,
		  unsigned int  upem);

unsigned int
hb_face_get_upem (hb_face_t *face);


/*
 * hb_font_funcs_t
 */

typedef struct _hb_font_funcs_t hb_font_funcs_t;

hb_font_funcs_t *
hb_font_funcs_create (void);

hb_font_funcs_t *
hb_font_funcs_get_empty (void);

hb_font_funcs_t *
hb_font_funcs_reference (hb_font_funcs_t *ffuncs);

void
hb_font_funcs_destroy (hb_font_funcs_t *ffuncs);

hb_bool_t
hb_font_funcs_set_user_data (hb_font_funcs_t    *ffuncs,
			     hb_user_data_key_t *key,
			     void *              data,
			     hb_destroy_func_t   destroy,
			     hb_bool_t           replace);


void *
hb_font_funcs_get_user_data (hb_font_funcs_t    *ffuncs,
			     hb_user_data_key_t *key);


void
hb_font_funcs_make_immutable (hb_font_funcs_t *ffuncs);

hb_bool_t
hb_font_funcs_is_immutable (hb_font_funcs_t *ffuncs);

/* funcs */

typedef struct _hb_glyph_extents_t
{
  hb_position_t x_bearing;
  hb_position_t y_bearing;
  hb_position_t width;
  hb_position_t height;
} hb_glyph_extents_t;


/* func types */

typedef hb_bool_t (*hb_font_get_glyph_func_t) (hb_font_t *font, void *font_data,
					       hb_codepoint_t unicode, hb_codepoint_t variation_selector,
					       hb_codepoint_t *glyph,
					       void *user_data);


typedef hb_position_t (*hb_font_get_glyph_advance_func_t) (hb_font_t *font, void *font_data,
							   hb_codepoint_t glyph,
							   void *user_data);
typedef hb_font_get_glyph_advance_func_t hb_font_get_glyph_h_advance_func_t;
typedef hb_font_get_glyph_advance_func_t hb_font_get_glyph_v_advance_func_t;

typedef hb_bool_t (*hb_font_get_glyph_origin_func_t) (hb_font_t *font, void *font_data,
						      hb_codepoint_t glyph,
						      hb_position_t *x, hb_position_t *y,
						      void *user_data);
typedef hb_font_get_glyph_origin_func_t hb_font_get_glyph_h_origin_func_t;
typedef hb_font_get_glyph_origin_func_t hb_font_get_glyph_v_origin_func_t;

typedef hb_position_t (*hb_font_get_glyph_kerning_func_t) (hb_font_t *font, void *font_data,
							   hb_codepoint_t first_glyph, hb_codepoint_t second_glyph,
							   void *user_data);
typedef hb_font_get_glyph_kerning_func_t hb_font_get_glyph_h_kerning_func_t;
typedef hb_font_get_glyph_kerning_func_t hb_font_get_glyph_v_kerning_func_t;


typedef hb_bool_t (*hb_font_get_glyph_extents_func_t) (hb_font_t *font, void *font_data,
						       hb_codepoint_t glyph,
						       hb_glyph_extents_t *extents,
						       void *user_data);
typedef hb_bool_t (*hb_font_get_glyph_contour_point_func_t) (hb_font_t *font, void *font_data,
							     hb_codepoint_t glyph, unsigned int point_index,
							     hb_position_t *x, hb_position_t *y,
							     void *user_data);


/* func setters */

void
hb_font_funcs_set_glyph_func (hb_font_funcs_t *ffuncs,
			      hb_font_get_glyph_func_t glyph_func,
			      void *user_data, hb_destroy_func_t destroy);

void
hb_font_funcs_set_glyph_h_advance_func (hb_font_funcs_t *ffuncs,
					hb_font_get_glyph_h_advance_func_t func,
					void *user_data, hb_destroy_func_t destroy);
void
hb_font_funcs_set_glyph_v_advance_func (hb_font_funcs_t *ffuncs,
					hb_font_get_glyph_v_advance_func_t func,
					void *user_data, hb_destroy_func_t destroy);

void
hb_font_funcs_set_glyph_h_origin_func (hb_font_funcs_t *ffuncs,
				       hb_font_get_glyph_h_origin_func_t func,
				       void *user_data, hb_destroy_func_t destroy);
void
hb_font_funcs_set_glyph_v_origin_func (hb_font_funcs_t *ffuncs,
				       hb_font_get_glyph_v_origin_func_t func,
				       void *user_data, hb_destroy_func_t destroy);

void
hb_font_funcs_set_glyph_h_kerning_func (hb_font_funcs_t *ffuncs,
					hb_font_get_glyph_h_kerning_func_t func,
					void *user_data, hb_destroy_func_t destroy);
void
hb_font_funcs_set_glyph_v_kerning_func (hb_font_funcs_t *ffuncs,
					hb_font_get_glyph_v_kerning_func_t func,
					void *user_data, hb_destroy_func_t destroy);

void
hb_font_funcs_set_glyph_extents_func (hb_font_funcs_t *ffuncs,
				      hb_font_get_glyph_extents_func_t func,
				      void *user_data, hb_destroy_func_t destroy);
void
hb_font_funcs_set_glyph_contour_point_func (hb_font_funcs_t *ffuncs,
					    hb_font_get_glyph_contour_point_func_t func,
					    void *user_data, hb_destroy_func_t destroy);


/* func dispatch */

hb_bool_t
hb_font_get_glyph (hb_font_t *font,
		   hb_codepoint_t unicode, hb_codepoint_t variation_selector,
		   hb_codepoint_t *glyph);

hb_position_t
hb_font_get_glyph_h_advance (hb_font_t *font,
			     hb_codepoint_t glyph);
hb_position_t
hb_font_get_glyph_v_advance (hb_font_t *font,
			     hb_codepoint_t glyph);

hb_bool_t
hb_font_get_glyph_h_origin (hb_font_t *font,
			    hb_codepoint_t glyph,
			    hb_position_t *x, hb_position_t *y);
hb_bool_t
hb_font_get_glyph_v_origin (hb_font_t *font,
			    hb_codepoint_t glyph,
			    hb_position_t *x, hb_position_t *y);

hb_position_t
hb_font_get_glyph_h_kerning (hb_font_t *font,
			     hb_codepoint_t left_glyph, hb_codepoint_t right_glyph);
hb_position_t
hb_font_get_glyph_v_kerning (hb_font_t *font,
			     hb_codepoint_t top_glyph, hb_codepoint_t bottom_glyph);

hb_bool_t
hb_font_get_glyph_extents (hb_font_t *font,
			   hb_codepoint_t glyph,
			   hb_glyph_extents_t *extents);

hb_bool_t
hb_font_get_glyph_contour_point (hb_font_t *font,
				 hb_codepoint_t glyph, unsigned int point_index,
				 hb_position_t *x, hb_position_t *y);


/* high-level funcs, with fallback */

void
hb_font_get_glyph_advance_for_direction (hb_font_t *font,
					 hb_codepoint_t glyph,
					 hb_direction_t direction,
					 hb_position_t *x, hb_position_t *y);
void
hb_font_get_glyph_origin_for_direction (hb_font_t *font,
					hb_codepoint_t glyph,
					hb_direction_t direction,
					hb_position_t *x, hb_position_t *y);
void
hb_font_add_glyph_origin_for_direction (hb_font_t *font,
					hb_codepoint_t glyph,
					hb_direction_t direction,
					hb_position_t *x, hb_position_t *y);
void
hb_font_subtract_glyph_origin_for_direction (hb_font_t *font,
					     hb_codepoint_t glyph,
					     hb_direction_t direction,
					     hb_position_t *x, hb_position_t *y);

void
hb_font_get_glyph_kerning_for_direction (hb_font_t *font,
					 hb_codepoint_t first_glyph, hb_codepoint_t second_glyph,
					 hb_direction_t direction,
					 hb_position_t *x, hb_position_t *y);

hb_bool_t
hb_font_get_glyph_extents_for_origin (hb_font_t *font,
				      hb_codepoint_t glyph,
				      hb_direction_t direction,
				      hb_glyph_extents_t *extents);

hb_bool_t
hb_font_get_glyph_contour_point_for_origin (hb_font_t *font,
					    hb_codepoint_t glyph, unsigned int point_index,
					    hb_direction_t direction,
					    hb_position_t *x, hb_position_t *y);


/*
 * hb_font_t
 */

/* Fonts are very light-weight objects */

hb_font_t *
hb_font_create (hb_face_t *face);

hb_font_t *
hb_font_create_sub_font (hb_font_t *parent);

hb_font_t *
hb_font_get_empty (void);

hb_font_t *
hb_font_reference (hb_font_t *font);

void
hb_font_destroy (hb_font_t *font);

hb_bool_t
hb_font_set_user_data (hb_font_t          *font,
		       hb_user_data_key_t *key,
		       void *              data,
		       hb_destroy_func_t   destroy,
		       hb_bool_t           replace);


void *
hb_font_get_user_data (hb_font_t          *font,
		       hb_user_data_key_t *key);

void
hb_font_make_immutable (hb_font_t *font);

hb_bool_t
hb_font_is_immutable (hb_font_t *font);

hb_font_t *
hb_font_get_parent (hb_font_t *font);

hb_face_t *
hb_font_get_face (hb_font_t *font);


void
hb_font_set_funcs (hb_font_t         *font,
		   hb_font_funcs_t   *klass,
		   void              *font_data,
		   hb_destroy_func_t  destroy);

/* Be *very* careful with this function! */
void
hb_font_set_funcs_data (hb_font_t         *font,
		        void              *font_data,
		        hb_destroy_func_t  destroy);


void
hb_font_set_scale (hb_font_t *font,
		   int x_scale,
		   int y_scale);

void
hb_font_get_scale (hb_font_t *font,
		   int *x_scale,
		   int *y_scale);

/*
 * A zero value means "no hinting in that direction"
 */
void
hb_font_set_ppem (hb_font_t *font,
		  unsigned int x_ppem,
		  unsigned int y_ppem);

void
hb_font_get_ppem (hb_font_t *font,
		  unsigned int *x_ppem,
		  unsigned int *y_ppem);


HB_END_DECLS

#endif /* HB_FONT_H */
