/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc
 * Copyright © 2006 Red Hat, Inc
 * Copyright © 2007 Adrian Johnson
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Kristian Høgsberg <krh@redhat.com>
 *	Carl Worth <cworth@cworth.org>
 *	Adrian Johnson <ajohnson@redneon.com>
 */

#ifndef CAIRO_PDF_OPERATORS_H
#define CAIRO_PDF_OPERATORS_H

#include "cairo-compiler-private.h"
#include "cairo-types-private.h"

typedef cairo_status_t (*cairo_pdf_operators_use_font_subset_t) (unsigned int  font_id,
								 unsigned int  subset_id,
								 void         *closure);

typedef struct _cairo_pdf_operators {
    cairo_output_stream_t *stream;
    cairo_matrix_t cairo_to_pdf;
    cairo_scaled_font_subsets_t *font_subsets;
    cairo_pdf_operators_use_font_subset_t use_font_subset;
    void *use_font_subset_closure;
} cairo_pdf_operators_t;

cairo_private void
_cairo_pdf_operators_init (cairo_pdf_operators_t       *pdf_operators,
			   cairo_output_stream_t       *stream,
			   cairo_matrix_t 	       *cairo_to_pdf,
			   cairo_scaled_font_subsets_t *font_subsets);

cairo_private void
_cairo_pdf_operators_fini (cairo_pdf_operators_t       *pdf_operators);

cairo_private void
_cairo_pdf_operators_set_font_subsets_callback (cairo_pdf_operators_t 		     *pdf_operators,
						cairo_pdf_operators_use_font_subset_t use_font_subset,
						void				     *closure);

cairo_private void
_cairo_pdf_operators_set_stream (cairo_pdf_operators_t 	 *pdf_operators,
				 cairo_output_stream_t   *stream);


cairo_private void
_cairo_pdf_operators_set_cairo_to_pdf_matrix (cairo_pdf_operators_t *pdf_operators,
					      cairo_matrix_t 	    *cairo_to_pdf);

cairo_private cairo_int_status_t
_cairo_pdf_operators_clip (cairo_pdf_operators_t 	*pdf_operators,
			   cairo_path_fixed_t		*path,
			   cairo_fill_rule_t		 fill_rule);

cairo_private cairo_int_status_t
_cairo_pdf_operators_stroke (cairo_pdf_operators_t 	*pdf_operators,
			     cairo_path_fixed_t		*path,
			     cairo_stroke_style_t	*style,
			     cairo_matrix_t		*ctm,
			     cairo_matrix_t		*ctm_inverse);

cairo_private cairo_int_status_t
_cairo_pdf_operators_fill (cairo_pdf_operators_t 	*pdf_operators,
			   cairo_path_fixed_t		*path,
			   cairo_fill_rule_t	 	fill_rule);

cairo_private cairo_int_status_t
_cairo_pdf_operators_fill_stroke (cairo_pdf_operators_t 	*pdf_operators,
				  cairo_path_fixed_t		*path,
				  cairo_fill_rule_t	 	 fill_rule,
				  cairo_stroke_style_t	        *style,
				  cairo_matrix_t		*ctm,
				  cairo_matrix_t		*ctm_inverse);

cairo_private cairo_int_status_t
_cairo_pdf_operators_show_glyphs (cairo_pdf_operators_t *pdf_operators,
				  cairo_glyph_t		*glyphs,
				  int			 num_glyphs,
				  cairo_scaled_font_t	*scaled_font);

#endif /* CAIRO_PDF_OPERATORS_H */
