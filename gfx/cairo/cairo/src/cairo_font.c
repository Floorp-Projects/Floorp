/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat Inc.
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
 *	Carl D. Worth <cworth@cworth.org>
 *      Graydon Hoare <graydon@redhat.com>
 *      Owen Taylor <otaylor@redhat.com>
 */

#include "cairoint.h"

/* Now the internal "unscaled + scale" font API */

cairo_private cairo_status_t
_cairo_font_create (const char           *family, 
		    cairo_font_slant_t   slant, 
		    cairo_font_weight_t  weight,
		    cairo_font_scale_t   *sc,
		    cairo_font_t         **font)
{
    const cairo_font_backend_t *backend = CAIRO_FONT_BACKEND_DEFAULT;

    return backend->create (family, slant, weight, sc, font);
}

void
_cairo_font_init (cairo_font_t               *font, 
		  cairo_font_scale_t         *scale,
		  const cairo_font_backend_t *backend)
{
    font->scale = *scale;
    font->refcount = 1;
    font->backend = backend;
}

void
_cairo_unscaled_font_init (cairo_unscaled_font_t      *font, 
			   const cairo_font_backend_t *backend)
{
    font->refcount = 1;
    font->backend = backend;
}

cairo_status_t
_cairo_font_text_to_glyphs (cairo_font_t           *font,
			    const unsigned char    *utf8, 
			    cairo_glyph_t 	  **glyphs, 
			    int 		   *num_glyphs)
{
    return font->backend->text_to_glyphs (font, utf8, glyphs, num_glyphs);
}

cairo_status_t
_cairo_font_glyph_extents (cairo_font_t	        *font,
			   cairo_glyph_t 	*glyphs,
			   int 			num_glyphs,
			   cairo_text_extents_t *extents)
{
    return font->backend->glyph_extents(font, glyphs, num_glyphs, extents);
}


cairo_status_t
_cairo_font_glyph_bbox (cairo_font_t	*font,
			cairo_glyph_t  *glyphs,
			int             num_glyphs,
			cairo_box_t	*bbox)
{
    return font->backend->glyph_bbox (font, glyphs, num_glyphs, bbox);
}

cairo_status_t
_cairo_font_show_glyphs (cairo_font_t	        *font,
			 cairo_operator_t       operator,
			 cairo_pattern_t        *pattern,
			 cairo_surface_t        *surface,
			 int                    source_x,
			 int                    source_y,
			 int			dest_x,
			 int			dest_y,
			 unsigned int		width,
			 unsigned int		height,
			 cairo_glyph_t          *glyphs,
			 int                    num_glyphs)
{
    cairo_status_t status;
    if (surface->backend->show_glyphs != NULL) {
	status = surface->backend->show_glyphs (font, operator, pattern, 
						surface,
						source_x, source_y,
						dest_x, dest_y,
						width, height,
						glyphs, num_glyphs);
	if (status == CAIRO_STATUS_SUCCESS)
	    return status;
    }

    /* Surface display routine either does not exist or failed. */
    return font->backend->show_glyphs (font, operator, pattern, 
				       surface,
				       source_x, source_y,
				       dest_x, dest_y,
				       width, height,
				       glyphs, num_glyphs);
}

cairo_status_t
_cairo_font_glyph_path (cairo_font_t	   *font,
			cairo_glyph_t	   *glyphs, 
			int		    num_glyphs,
			cairo_path_t       *path)
{
    return font->backend->glyph_path (font, glyphs, num_glyphs, path);
}

void
_cairo_font_get_glyph_cache_key (cairo_font_t            *font,
				 cairo_glyph_cache_key_t *key)
{
  font->backend->get_glyph_cache_key (font, key);
}

cairo_status_t
_cairo_font_font_extents (cairo_font_t	       *font,
			  cairo_font_extents_t *extents)
{
    return font->backend->font_extents (font, extents);
}

void
_cairo_unscaled_font_reference (cairo_unscaled_font_t *font)
{
    font->refcount++;
}

void
_cairo_unscaled_font_destroy (cairo_unscaled_font_t *font)
{    
    if (--(font->refcount) > 0)
	return;

    font->backend->destroy_unscaled_font (font);
}



/* Public font API follows. */

void
cairo_font_reference (cairo_font_t *font)
{
    font->refcount++;
}

void
cairo_font_destroy (cairo_font_t *font)
{
    if (--(font->refcount) > 0)
	return;

    font->backend->destroy_font (font);
}

/**
 * cairo_font_extents:
 * @font: a #cairo_font_t
 * @font_matrix: the font transformation for which this font was
 *    created. (See cairo_transform_font()). This is needed
 *    properly convert the metrics from the font into user space.
 * @extents: a #cairo_font_extents_t which to store the retrieved extents.
 * 
 * Gets the metrics for a #cairo_font_t. 
 * 
 * Return value: %CAIRO_STATUS_SUCCESS on success. Otherwise, an
 *  error such as %CAIRO_STATUS_NO_MEMORY.
 **/
cairo_status_t
cairo_font_extents (cairo_font_t         *font,
		    cairo_matrix_t       *font_matrix,
		    cairo_font_extents_t *extents)
{
    cairo_int_status_t status;
    double  font_scale_x, font_scale_y;

    status = _cairo_font_font_extents (font, extents);

    if (!CAIRO_OK (status))
      return status;
    
    _cairo_matrix_compute_scale_factors (font_matrix,
					 &font_scale_x, &font_scale_y,
					 /* XXX */ 1);
    
    /* 
     * The font responded in unscaled units, scale by the font
     * matrix scale factors to get to user space
     */
    
    extents->ascent *= font_scale_y;
    extents->descent *= font_scale_y;
    extents->height *= font_scale_y;
    extents->max_x_advance *= font_scale_x;
    extents->max_y_advance *= font_scale_y;
      
    return status;
}

/**
 * cairo_font_glyph_extents:
 * @font: a #cairo_font_t
 * @font_matrix: the font transformation for which this font was
 *    created. (See cairo_transform_font()). This is needed
 *    properly convert the metrics from the font into user space.
 * @glyphs: an array of glyph IDs with X and Y offsets.
 * @num_glyphs: the number of glyphs in the @glyphs array
 * @extents: a #cairo_text_extents_t which to store the retrieved extents.
 * 
 * cairo_font_glyph_extents() gets the overall metrics for a string of
 * glyphs. The X and Y offsets in @glyphs are taken from an origin of 0,0. 
 **/
void
cairo_font_glyph_extents (cairo_font_t          *font,
			  cairo_matrix_t        *font_matrix,
			  cairo_glyph_t         *glyphs, 
			  int                   num_glyphs,
			  cairo_text_extents_t  *extents)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_glyph_t origin_glyph;
    cairo_text_extents_t origin_extents;
    int i;
    double min_x = 0.0, min_y = 0.0, max_x = 0.0, max_y = 0.0;
    double x_pos = 0.0, y_pos = 0.0;
    int set = 0;

    if (!num_glyphs)
    {
	extents->x_bearing = 0.0;
	extents->y_bearing = 0.0;
	extents->width = 0.0;
	extents->height = 0.0;
	extents->x_advance = 0.0;
	extents->y_advance = 0.0;
	
	return;
    }

    for (i = 0; i < num_glyphs; i++)
    {
	double		x, y;
	double		wm, hm;
	
	origin_glyph = glyphs[i];
	origin_glyph.x = 0.0;
	origin_glyph.y = 0.0;
	status = _cairo_font_glyph_extents (font,
					    &origin_glyph, 1,
					    &origin_extents);
	
	/*
	 * Transform font space metrics into user space metrics
	 * by running the corners through the font matrix and
	 * expanding the bounding box as necessary
	 */
	x = origin_extents.x_bearing;
	y = origin_extents.y_bearing;
	cairo_matrix_transform_point (font_matrix,
				      &x, &y);

	for (hm = 0.0; hm <= 1.0; hm += 1.0)
	    for (wm = 0.0; wm <= 1.0; wm += 1.0)
	    {
		x = origin_extents.x_bearing + origin_extents.width * wm;
		y = origin_extents.y_bearing + origin_extents.height * hm;
		cairo_matrix_transform_point (font_matrix,
					      &x, &y);
		x += glyphs[i].x;
		y += glyphs[i].y;
		if (!set)
		{
		    min_x = max_x = x;
		    min_y = max_y = y;
		    set = 1;
		}
		else
		{
		    if (x < min_x) min_x = x;
		    if (x > max_x) max_x = x;
		    if (y < min_y) min_y = y;
		    if (y > max_y) max_y = y;
		}
	    }

	x = origin_extents.x_advance;
	y = origin_extents.y_advance;
	cairo_matrix_transform_point (font_matrix,
				      &x, &y);
	x_pos = glyphs[i].x + x;
	y_pos = glyphs[i].y + y;
    }

    extents->x_bearing = min_x - glyphs[0].x;
    extents->y_bearing = min_y - glyphs[0].y;
    extents->width = max_x - min_x;
    extents->height = max_y - min_y;
    extents->x_advance = x_pos - glyphs[0].x;
    extents->y_advance = y_pos - glyphs[0].y;
}

/* Now we implement functions to access a default global image & metrics
 * cache. 
 */

unsigned long
_cairo_glyph_cache_hash (void *cache, void *key)
{
    cairo_glyph_cache_key_t *in;
    in = (cairo_glyph_cache_key_t *) key;
    return 
	((unsigned long) in->unscaled) 
	^ ((unsigned long) in->scale.matrix[0][0]) 
	^ ((unsigned long) in->scale.matrix[0][1]) 
	^ ((unsigned long) in->scale.matrix[1][0]) 
	^ ((unsigned long) in->scale.matrix[1][1])
        ^ (in->flags * 1451) /* 1451 is just an abitrary prime */
	^ in->index;
}

int
_cairo_glyph_cache_keys_equal (void *cache,
			       void *k1,
			       void *k2)
{
    cairo_glyph_cache_key_t *a, *b;
    a = (cairo_glyph_cache_key_t *) k1;
    b = (cairo_glyph_cache_key_t *) k2;
    return (a->index == b->index)
	&& (a->unscaled == b->unscaled)
	&& (a->flags == b->flags)
	&& (a->scale.matrix[0][0] == b->scale.matrix[0][0])
	&& (a->scale.matrix[0][1] == b->scale.matrix[0][1])
	&& (a->scale.matrix[1][0] == b->scale.matrix[1][0])
	&& (a->scale.matrix[1][1] == b->scale.matrix[1][1]);
}


static cairo_status_t
_image_glyph_cache_create_entry (void *cache,
				 void *key,
				 void **return_value)
{
    cairo_glyph_cache_key_t *k = (cairo_glyph_cache_key_t *) key;
    cairo_image_glyph_cache_entry_t *im;
    cairo_status_t status;

    im = calloc (1, sizeof (cairo_image_glyph_cache_entry_t));
    if (im == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    im->key = *k;    
    status = im->key.unscaled->backend->create_glyph (im);

    if (status != CAIRO_STATUS_SUCCESS) {
	free (im);
	return status;
    }

    _cairo_unscaled_font_reference (im->key.unscaled);

    im->key.base.memory = 
	sizeof (cairo_image_glyph_cache_entry_t) 
	+ (im->image ? 
	   sizeof (cairo_image_surface_t) 
	   + 28 * sizeof (int) /* rough guess at size of pixman image structure */
	   + (im->image->height * im->image->stride) : 0);

    *return_value = im;

    return CAIRO_STATUS_SUCCESS;
}


static void
_image_glyph_cache_destroy_entry (void *cache,
				  void *value)
{
    cairo_image_glyph_cache_entry_t *im;

    im = (cairo_image_glyph_cache_entry_t *) value;
    _cairo_unscaled_font_destroy (im->key.unscaled);
    cairo_surface_destroy (&(im->image->base));
    free (im); 
}

static void 
_image_glyph_cache_destroy_cache (void *cache)
{
    free (cache);
}

static const cairo_cache_backend_t cairo_image_cache_backend = {
    _cairo_glyph_cache_hash,
    _cairo_glyph_cache_keys_equal,
    _image_glyph_cache_create_entry,
    _image_glyph_cache_destroy_entry,
    _image_glyph_cache_destroy_cache
};

void
_cairo_lock_global_image_glyph_cache()
{
    /* FIXME: implement locking. */
}

void
_cairo_unlock_global_image_glyph_cache()
{
    /* FIXME: implement locking. */
}

static cairo_cache_t *
_global_image_glyph_cache = NULL;

cairo_cache_t *
_cairo_get_global_image_glyph_cache ()
{
    if (_global_image_glyph_cache == NULL) {
	_global_image_glyph_cache = malloc (sizeof (cairo_cache_t));
	
	if (_global_image_glyph_cache == NULL)
	    goto FAIL;
	
	if (_cairo_cache_init (_global_image_glyph_cache,
			       &cairo_image_cache_backend,
			       CAIRO_IMAGE_GLYPH_CACHE_MEMORY_DEFAULT))
	    goto FAIL;
    }

    return _global_image_glyph_cache;
    
 FAIL:
    if (_global_image_glyph_cache)
	free (_global_image_glyph_cache);
    _global_image_glyph_cache = NULL;
    return NULL;
}
