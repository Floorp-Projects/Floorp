/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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
 *	Carl D. Worth <cworth@isi.edu>
 */

#include "cairoint.h"

/* First we implement a global font cache for named fonts. */

typedef struct {
    cairo_cache_entry_base_t base;
    const char *family;
    cairo_font_slant_t slant;
    cairo_font_weight_t weight;    
} cairo_font_cache_key_t;

typedef struct {
    cairo_font_cache_key_t key;
    cairo_unscaled_font_t *unscaled;
} cairo_font_cache_entry_t;

static unsigned long
_font_cache_hash (void *cache, void *key)
{
    unsigned long hash;
    cairo_font_cache_key_t *in;
    in = (cairo_font_cache_key_t *) key;

    /* 1607 and 1451 are just a couple random primes. */
    hash = _cairo_hash_string (in->family);
    hash += ((unsigned long) in->slant) * 1607;
    hash += ((unsigned long) in->weight) * 1451;
    return hash;
}


static int
_font_cache_keys_equal (void *cache,
			void *k1,
			void *k2)
{
    cairo_font_cache_key_t *a, *b;
    a = (cairo_font_cache_key_t *) k1;
    b = (cairo_font_cache_key_t *) k2;
    
    return (strcmp (a->family, b->family) == 0) 
	&& (a->weight == b->weight)
	&& (a->slant == b->slant);
}


static cairo_status_t
_font_cache_create_entry (void *cache,
			  void *key,
			  void **return_value)
{
    const cairo_font_backend_t *backend = CAIRO_FONT_BACKEND_DEFAULT;
    cairo_font_cache_key_t *k;
    cairo_font_cache_entry_t *entry;
    k = (cairo_font_cache_key_t *) key;

    /* XXX: The current freetype backend may return NULL, (for example
     * if no fonts are installed), but I would like to guarantee that
     * the toy API always returns at least *some* font, so I would
     * like to build in some sort fo font here, (even a really lame,
     * ugly one if necessary). */

    entry = malloc (sizeof (cairo_font_cache_entry_t));
    if (entry == NULL)
	goto FAIL;

    entry->key.slant = k->slant;
    entry->key.weight = k->weight;
    entry->key.family = strdup(k->family);
    if (entry->key.family == NULL)
	goto FREE_ENTRY;

    entry->unscaled = backend->create (k->family, k->slant, k->weight);
    if (entry->unscaled == NULL)
	goto FREE_FAMILY;
    
    /* Not sure how to measure backend font mem; use a simple count for now.*/
    entry->key.base.memory = 1;
    *return_value = entry;
    return CAIRO_STATUS_SUCCESS;
    
 FREE_FAMILY:
    free ((void *) entry->key.family);

 FREE_ENTRY:
    free (entry);
   
 FAIL:
    return CAIRO_STATUS_NO_MEMORY;
}

static void
_font_cache_destroy_entry (void *cache,
			   void *entry)
{
    cairo_font_cache_entry_t *e;
    
    e = (cairo_font_cache_entry_t *) entry;
    _cairo_unscaled_font_destroy (e->unscaled);
    free ((void *) e->key.family);
    free (e);
}

static void 
_font_cache_destroy_cache (void *cache)
{
    free (cache);
}

static const cairo_cache_backend_t cairo_font_cache_backend = {
    _font_cache_hash,
    _font_cache_keys_equal,
    _font_cache_create_entry,
    _font_cache_destroy_entry,
    _font_cache_destroy_cache
};

static void
_lock_global_font_cache (void)
{
    /* FIXME: implement locking. */
}

static void
_unlock_global_font_cache (void)
{
    /* FIXME: implement locking. */
}

static cairo_cache_t *
_global_font_cache = NULL;

static cairo_cache_t *
_get_global_font_cache (void)
{
    if (_global_font_cache == NULL) {
	_global_font_cache = malloc (sizeof (cairo_cache_t));
	
	if (_global_font_cache == NULL)
	    goto FAIL;
	
	if (_cairo_cache_init (_global_font_cache,
			       &cairo_font_cache_backend,
			       CAIRO_FONT_CACHE_NUM_FONTS_DEFAULT))
	    goto FAIL;
    }

    return _global_font_cache;
    
 FAIL:
    if (_global_font_cache)
	free (_global_font_cache);
    _global_font_cache = NULL;
    return NULL;
}


/* Now the internal "unscaled + scale" font API */

cairo_unscaled_font_t *
_cairo_unscaled_font_create (const char           *family, 
			     cairo_font_slant_t   slant, 
			     cairo_font_weight_t  weight)
{
    cairo_cache_t * cache;
    cairo_font_cache_key_t key;
    cairo_font_cache_entry_t *font;
    cairo_status_t status;

    _lock_global_font_cache ();
    cache = _get_global_font_cache ();
    if (cache == NULL) {
	_unlock_global_font_cache ();
	return NULL;
    }

    key.family = family;
    key.slant = slant;
    key.weight = weight;
    
    status = _cairo_cache_lookup (cache, &key, (void **) &font);
    if (status) {
	_unlock_global_font_cache ();
	return NULL;
    }

    _cairo_unscaled_font_reference (font->unscaled);
    _unlock_global_font_cache ();
    return font->unscaled;
}

void
_cairo_font_init (cairo_font_t *scaled, 
		  cairo_font_scale_t *scale, 
		  cairo_unscaled_font_t *unscaled)
{
    scaled->scale = *scale;
    scaled->unscaled = unscaled;
    scaled->refcount = 1;
}

cairo_status_t
_cairo_unscaled_font_init (cairo_unscaled_font_t 	*font, 
			   const cairo_font_backend_t	*backend)
{
    font->refcount = 1;
    font->backend = backend;
    return CAIRO_STATUS_SUCCESS;
}


cairo_status_t
_cairo_unscaled_font_text_to_glyphs (cairo_unscaled_font_t 	*font,
				     cairo_font_scale_t 	*scale,
				     const unsigned char 	*utf8, 
				     cairo_glyph_t 		**glyphs, 
				     int 			*num_glyphs)
{
    return font->backend->text_to_glyphs (font, scale, utf8, glyphs, num_glyphs);
}

cairo_status_t
_cairo_unscaled_font_glyph_extents (cairo_unscaled_font_t	*font,
				    cairo_font_scale_t 		*scale,			   
				    cairo_glyph_t 		*glyphs,
				    int 			num_glyphs,
				    cairo_text_extents_t 	*extents)
{
    return font->backend->glyph_extents(font, scale, glyphs, num_glyphs, extents);
}


cairo_status_t
_cairo_unscaled_font_glyph_bbox (cairo_unscaled_font_t	*font,
				 cairo_font_scale_t	*scale,
				 cairo_glyph_t          *glyphs,
				 int                    num_glyphs,
				 cairo_box_t		*bbox)
{
    return font->backend->glyph_bbox (font, scale, glyphs, num_glyphs, bbox);
}

cairo_status_t
_cairo_unscaled_font_show_glyphs (cairo_unscaled_font_t	*font,
				  cairo_font_scale_t	*scale,
				  cairo_operator_t       operator,
				  cairo_surface_t        *source,
				  cairo_surface_t        *surface,
				  int                    source_x,
				  int                    source_y,
				  cairo_glyph_t          *glyphs,
				  int                    num_glyphs)
{
    cairo_status_t status;
    if (surface->backend->show_glyphs != NULL) {
	status = surface->backend->show_glyphs (font, scale, operator, source, 
						surface, source_x, source_y,
						glyphs, num_glyphs);
	if (status == CAIRO_STATUS_SUCCESS)
	    return status;
    }

    /* Surface display routine either does not exist or failed. */
    return font->backend->show_glyphs (font, scale, operator, source, 
				       surface, source_x, source_y,
				       glyphs, num_glyphs);
}

cairo_status_t
_cairo_unscaled_font_glyph_path (cairo_unscaled_font_t	*font,
				 cairo_font_scale_t 	*scale,
				 cairo_glyph_t		*glyphs, 
				 int			num_glyphs,
				 cairo_path_t        	*path)
{
    return font->backend->glyph_path (font, scale, glyphs, num_glyphs, path);
}

cairo_status_t
_cairo_unscaled_font_font_extents (cairo_unscaled_font_t	*font,
				   cairo_font_scale_t		*scale,
				   cairo_font_extents_t		*extents)
{
    return font->backend->font_extents(font, scale, extents);
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

    if (font->backend)
	font->backend->destroy (font);
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

    if (font->unscaled)
	_cairo_unscaled_font_destroy (font->unscaled);

    free (font);
}

void
cairo_font_set_transform (cairo_font_t *font, 
			  cairo_matrix_t *matrix)
{
    double dummy;
    cairo_matrix_get_affine (matrix,
			     &font->scale.matrix[0][0],
			     &font->scale.matrix[0][1],
			     &font->scale.matrix[1][0],
			     &font->scale.matrix[1][1],
			     &dummy, &dummy);    
}

void
cairo_font_current_transform (cairo_font_t *font, 
			      cairo_matrix_t *matrix)
{
    cairo_matrix_set_affine (matrix,
			     font->scale.matrix[0][0],
			     font->scale.matrix[0][1],
			     font->scale.matrix[1][0],
			     font->scale.matrix[1][1],
			     0, 0);
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
