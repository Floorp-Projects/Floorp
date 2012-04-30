/*
 * Copyright © 2009  Red Hat, Inc.
 * Copyright © 2011  Codethink Limited
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
 * Red Hat Author(s): Behdad Esfahbod
 * Codethink Author(s): Ryan Lortie
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_H_IN
#error "Include <hb.h> instead."
#endif

#ifndef HB_UNICODE_H
#define HB_UNICODE_H

#include "hb-common.h"

HB_BEGIN_DECLS


/*
 * hb_unicode_funcs_t
 */

typedef struct _hb_unicode_funcs_t hb_unicode_funcs_t;


/*
 * just give me the best implementation you've got there.
 */
hb_unicode_funcs_t *
hb_unicode_funcs_get_default (void);


hb_unicode_funcs_t *
hb_unicode_funcs_create (hb_unicode_funcs_t *parent);

hb_unicode_funcs_t *
hb_unicode_funcs_get_empty (void);

hb_unicode_funcs_t *
hb_unicode_funcs_reference (hb_unicode_funcs_t *ufuncs);

void
hb_unicode_funcs_destroy (hb_unicode_funcs_t *ufuncs);

hb_bool_t
hb_unicode_funcs_set_user_data (hb_unicode_funcs_t *ufuncs,
			        hb_user_data_key_t *key,
			        void *              data,
			        hb_destroy_func_t   destroy,
				hb_bool_t           replace);


void *
hb_unicode_funcs_get_user_data (hb_unicode_funcs_t *ufuncs,
			        hb_user_data_key_t *key);


void
hb_unicode_funcs_make_immutable (hb_unicode_funcs_t *ufuncs);

hb_bool_t
hb_unicode_funcs_is_immutable (hb_unicode_funcs_t *ufuncs);

hb_unicode_funcs_t *
hb_unicode_funcs_get_parent (hb_unicode_funcs_t *ufuncs);


/*
 * funcs
 */

/* typedefs */

typedef unsigned int			(*hb_unicode_combining_class_func_t)	(hb_unicode_funcs_t *ufuncs,
										 hb_codepoint_t      unicode,
										 void               *user_data);
typedef unsigned int			(*hb_unicode_eastasian_width_func_t)	(hb_unicode_funcs_t *ufuncs,
										 hb_codepoint_t      unicode,
										 void               *user_data);
typedef hb_unicode_general_category_t	(*hb_unicode_general_category_func_t)	(hb_unicode_funcs_t *ufuncs,
										 hb_codepoint_t      unicode,
										 void               *user_data);
typedef hb_codepoint_t			(*hb_unicode_mirroring_func_t)		(hb_unicode_funcs_t *ufuncs,
										 hb_codepoint_t      unicode,
										 void               *user_data);
typedef hb_script_t			(*hb_unicode_script_func_t)		(hb_unicode_funcs_t *ufuncs,
										 hb_codepoint_t      unicode,
										 void               *user_data);

typedef hb_bool_t			(*hb_unicode_compose_func_t)		(hb_unicode_funcs_t *ufuncs,
										 hb_codepoint_t      a,
										 hb_codepoint_t      b,
										 hb_codepoint_t     *ab,
										 void               *user_data);
typedef hb_bool_t			(*hb_unicode_decompose_func_t)		(hb_unicode_funcs_t *ufuncs,
										 hb_codepoint_t      ab,
										 hb_codepoint_t     *a,
										 hb_codepoint_t     *b,
										 void               *user_data);

/* setters */

void
hb_unicode_funcs_set_combining_class_func (hb_unicode_funcs_t *ufuncs,
					   hb_unicode_combining_class_func_t combining_class_func,
					   void *user_data, hb_destroy_func_t destroy);

void
hb_unicode_funcs_set_eastasian_width_func (hb_unicode_funcs_t *ufuncs,
					   hb_unicode_eastasian_width_func_t eastasian_width_func,
					   void *user_data, hb_destroy_func_t destroy);

void
hb_unicode_funcs_set_general_category_func (hb_unicode_funcs_t *ufuncs,
					    hb_unicode_general_category_func_t general_category_func,
					    void *user_data, hb_destroy_func_t destroy);

void
hb_unicode_funcs_set_mirroring_func (hb_unicode_funcs_t *ufuncs,
				     hb_unicode_mirroring_func_t mirroring_func,
				     void *user_data, hb_destroy_func_t destroy);

void
hb_unicode_funcs_set_script_func (hb_unicode_funcs_t *ufuncs,
				  hb_unicode_script_func_t script_func,
				  void *user_data, hb_destroy_func_t destroy);

void
hb_unicode_funcs_set_compose_func (hb_unicode_funcs_t *ufuncs,
				   hb_unicode_compose_func_t compose_func,
				   void *user_data, hb_destroy_func_t destroy);

void
hb_unicode_funcs_set_decompose_func (hb_unicode_funcs_t *ufuncs,
				     hb_unicode_decompose_func_t decompose_func,
				     void *user_data, hb_destroy_func_t destroy);


/* accessors */

unsigned int
hb_unicode_combining_class (hb_unicode_funcs_t *ufuncs,
			    hb_codepoint_t unicode);

unsigned int
hb_unicode_eastasian_width (hb_unicode_funcs_t *ufuncs,
			    hb_codepoint_t unicode);

hb_unicode_general_category_t
hb_unicode_general_category (hb_unicode_funcs_t *ufuncs,
			     hb_codepoint_t unicode);

hb_codepoint_t
hb_unicode_mirroring (hb_unicode_funcs_t *ufuncs,
		      hb_codepoint_t unicode);

hb_script_t
hb_unicode_script (hb_unicode_funcs_t *ufuncs,
		   hb_codepoint_t unicode);

hb_bool_t
hb_unicode_compose (hb_unicode_funcs_t *ufuncs,
		    hb_codepoint_t      a,
		    hb_codepoint_t      b,
		    hb_codepoint_t     *ab);
hb_bool_t
hb_unicode_decompose (hb_unicode_funcs_t *ufuncs,
		      hb_codepoint_t      ab,
		      hb_codepoint_t     *a,
		      hb_codepoint_t     *b);

HB_END_DECLS

#endif /* HB_UNICODE_H */
