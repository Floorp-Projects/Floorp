/*
 * Copyright © 2003 Red Hat Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Red Hat Inc. not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. Red Hat Inc. makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 *
 * RED HAT INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL RED HAT INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Graydon Hoare <graydon@redhat.com>
 */

#include "cairoint.h"
#include "cairo-ft.h"

#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_IMAGE_H

#define DOUBLE_TO_26_6(d) ((FT_F26Dot6)((d) * 64.0))
#define DOUBLE_FROM_26_6(t) ((double)(t) / 64.0)
#define DOUBLE_TO_16_16(d) ((FT_Fixed)((d) * 65536.0))
#define DOUBLE_FROM_16_16(t) ((double)(t) / 65536.0)

/* 
 * First we make a private, sharable implementation object which can be
 * stored both in a private cache and in public font objects (including
 * those connected to fonts we don't own)
 */

typedef struct {
    int refcount;

    FT_Face face;
    int owns_face;

} ft_font_val_t;

/*
 * The simple 2x2 matrix is converted into separate scale and shape
 * factors so that hinting works right
 */

typedef struct {
    double  x_scale, y_scale;
    double  shape[2][2];
} ft_font_transform_t;

static ft_font_val_t *
_create_from_face (FT_Face face, int owns_face)
{
    ft_font_val_t *tmp = malloc (sizeof(ft_font_val_t));
    if (tmp) {
	tmp->refcount = 1;
	tmp->face = face;
	tmp->owns_face = owns_face;
	FT_Set_Char_Size (face,
			  DOUBLE_TO_26_6 (1.0),
			  DOUBLE_TO_26_6 (1.0),
			  0, 0);
    }
    return tmp;
}

static void
_reference_font_val (ft_font_val_t *f)
{
    f->refcount++;
}

static void
_destroy_font_val (ft_font_val_t *f)
{
    if (--(f->refcount) > 0)
	return;

    if (f->owns_face)
	FT_Done_Face (f->face);

    free (f);
}

static ft_font_val_t *
_create_from_library_and_pattern (FT_Library ft_library, FcPattern *pattern)
{
    ft_font_val_t *f = NULL;
    char *filename = NULL;
    int owns_face = 0;
    FT_Face face = NULL;
    FcPattern *resolved = NULL;
    FcResult result = FcResultMatch;

    if (pattern == NULL)
	goto FAIL;

    FcConfigSubstitute (0, pattern, FcMatchPattern);
    FcDefaultSubstitute (pattern);
    
    resolved = FcFontMatch (0, pattern, &result);
    if (!resolved)
	goto FAIL;

    if (result != FcResultMatch)
	goto FREE_RESOLVED;
    
    /* If the pattern has an FT_Face object, use that. */
    if (FcPatternGetFTFace (resolved, FC_FT_FACE, 0, &face) != FcResultMatch
        || face == NULL)
    {
        /* otherwise it had better have a filename */
        result = FcPatternGetString (resolved, FC_FILE, 0, (FcChar8 **)(&filename));
      
        if (result == FcResultMatch)
            if (FT_New_Face (ft_library, filename, 0, &face))
		goto FREE_RESOLVED;
      
        if (face == NULL)
	    goto FREE_RESOLVED;

        owns_face = 1;
    }

    f = _create_from_face (face, owns_face);

    FcPatternDestroy (resolved);
    return f;
    
 FREE_RESOLVED:
    if (resolved)
	FcPatternDestroy (resolved);

 FAIL:
    return NULL;
}


/* 
 * We then make the user-exposed structure out of one of these impls, such
 * that it is reasonably cheap to copy and/or destroy. Unfortunately this
 * duplicates a certain amount of the caching machinery in the font cache,
 * but that's unavoidable as we also provide an FcPattern resolution API,
 * which is not part of cairo's generic font finding system.
 */

typedef struct {
    cairo_unscaled_font_t base;
    FcPattern *pattern;
    ft_font_val_t *val;
} cairo_ft_font_t;

/* 
 * We then make a key and entry type which are compatible with the generic
 * cache system. This cache serves to share single ft_font_val_t instances
 * between fonts (or between font lifecycles).
 */

typedef struct {
    cairo_cache_entry_base_t base;
    FcPattern *pattern;
} cairo_ft_cache_key_t;

typedef struct {
    cairo_ft_cache_key_t key;
    ft_font_val_t *val;
} cairo_ft_cache_entry_t;

/* 
 * Then we create a cache which maps FcPattern keys to the refcounted
 * ft_font_val_t values.
 */

typedef struct {
    cairo_cache_t base;
    FT_Library lib;
} ft_cache_t;


static unsigned long
_ft_font_cache_hash (void *cache, void *key)
{
    cairo_ft_cache_key_t *in;
    in = (cairo_ft_cache_key_t *) key;
    return FcPatternHash (in->pattern);
}

static int
_ft_font_cache_keys_equal (void *cache,
			   void *k1,
			   void *k2)
{
    cairo_ft_cache_key_t *a;
    cairo_ft_cache_key_t *b;
    a = (cairo_ft_cache_key_t *) k1;
    b = (cairo_ft_cache_key_t *) k2;
    
    return FcPatternEqual (a->pattern, b->pattern);
}


static cairo_status_t
_ft_font_cache_create_entry (void *cache,
			     void *key,
			     void **return_entry)
{
    ft_cache_t *ftcache = (ft_cache_t *) cache;
    cairo_ft_cache_key_t *k = (cairo_ft_cache_key_t *) key;
    cairo_ft_cache_entry_t *entry;

    entry = malloc (sizeof (cairo_ft_cache_entry_t));
    if (entry == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    entry->key.pattern = FcPatternDuplicate (k->pattern);
    if (!entry->key.pattern) {
	free (entry);
	return CAIRO_STATUS_NO_MEMORY;
    }

    entry->val = _create_from_library_and_pattern (ftcache->lib, entry->key.pattern);
    entry->key.base.memory = 1;

    *return_entry = entry;

    return CAIRO_STATUS_SUCCESS;
}

static void
_ft_font_cache_destroy_entry (void *cache,
			      void *entry)
{    
    cairo_ft_cache_entry_t *e = (cairo_ft_cache_entry_t *) entry;
    FcPatternDestroy (e->key.pattern);
    _destroy_font_val (e->val);
    free (e);
}

static void 
_ft_font_cache_destroy_cache (void *cache)
{
    ft_cache_t *fc = (ft_cache_t *) cache;
    FT_Done_FreeType (fc->lib);
    free (fc);
}

static const cairo_cache_backend_t _ft_font_cache_backend = {
    _ft_font_cache_hash,
    _ft_font_cache_keys_equal,
    _ft_font_cache_create_entry,
    _ft_font_cache_destroy_entry,
    _ft_font_cache_destroy_cache
};

static ft_cache_t *_global_ft_cache = NULL;

static void
_lock_global_ft_cache (void)
{
    /* FIXME: Perform locking here. */
}

static void
_unlock_global_ft_cache (void)
{
    /* FIXME: Perform locking here. */
}

static cairo_cache_t *
_get_global_ft_cache (void)
{
    if (_global_ft_cache == NULL)
    {
	_global_ft_cache = malloc (sizeof(ft_cache_t));	
	if (!_global_ft_cache)
	    goto FAIL;

	if (_cairo_cache_init (&_global_ft_cache->base,
			       &_ft_font_cache_backend,
			       CAIRO_FT_CACHE_NUM_FONTS_DEFAULT))
	    goto FAIL;
	
	if (FT_Init_FreeType (&_global_ft_cache->lib)) 
	    goto FAIL;
    }
    return &_global_ft_cache->base;

 FAIL:
    if (_global_ft_cache)
	free (_global_ft_cache);
    _global_ft_cache = NULL;
    return NULL;
}

/* implement the backend interface */

const cairo_font_backend_t cairo_ft_font_backend;

static cairo_unscaled_font_t *
_cairo_ft_font_create (const char           *family, 
                       cairo_font_slant_t   slant, 
                       cairo_font_weight_t  weight)
{
    cairo_status_t status;
    cairo_ft_font_t *font = NULL;
    int fcslant;
    int fcweight;
    cairo_cache_t *cache;
    cairo_ft_cache_entry_t *entry;
    cairo_ft_cache_key_t key;

    key.pattern = FcPatternCreate ();
    if (key.pattern == NULL)
	goto FAIL;

    font = malloc (sizeof (cairo_ft_font_t));
    if (font == NULL)
	goto FREE_PATTERN;

    switch (weight)
    {
    case CAIRO_FONT_WEIGHT_BOLD:
        fcweight = FC_WEIGHT_BOLD;
        break;
    case CAIRO_FONT_WEIGHT_NORMAL:
    default:
        fcweight = FC_WEIGHT_MEDIUM;
        break;
    }

    switch (slant)
    {
    case CAIRO_FONT_SLANT_ITALIC:
        fcslant = FC_SLANT_ITALIC;
        break;
    case CAIRO_FONT_SLANT_OBLIQUE:
	fcslant = FC_SLANT_OBLIQUE;
        break;
    case CAIRO_FONT_SLANT_NORMAL:
    default:
        fcslant = FC_SLANT_ROMAN;
        break;
    }

    FcPatternAddString (key.pattern, FC_FAMILY, family);
    FcPatternAddInteger (key.pattern, FC_SLANT, fcslant);
    FcPatternAddInteger (key.pattern, FC_WEIGHT, fcweight);

    if (_cairo_unscaled_font_init (&font->base, &cairo_ft_font_backend))
	goto FREE_PATTERN;

    _lock_global_ft_cache ();
    cache = _get_global_ft_cache ();
    if (cache == NULL) {
	_unlock_global_ft_cache ();
	goto FREE_PATTERN;
    }    

    status = _cairo_cache_lookup (cache, &key, (void **) &entry);
    _unlock_global_ft_cache ();

    if (status)
	goto FREE_PATTERN;

    font->pattern = FcPatternDuplicate (entry->key.pattern);
    if (font->pattern == NULL)
	goto FREE_PATTERN;

    font->val = entry->val;
    _reference_font_val (font->val);
    
    return &font->base;  

 FREE_PATTERN:
    FcPatternDestroy (key.pattern);

 FAIL:
    return NULL;

}


static void 
_cairo_ft_font_destroy (void *abstract_font)
{
    cairo_ft_font_t * font = abstract_font;
  
    if (font == NULL)
        return;
  
    if (font->pattern != NULL)
        FcPatternDestroy (font->pattern);

    _destroy_font_val (font->val);

    free (font);
}

static void 
_utf8_to_ucs4 (char const *utf8, 
               FT_ULong **ucs4, 
               int *nchars)
{
    int len = 0, step = 0;
    int n = 0, alloc = 0;
    FcChar32 u = 0;

    if (utf8 == NULL || ucs4 == NULL || nchars == NULL)
        return;

    len = strlen (utf8);
    alloc = len;
    *ucs4 = malloc (sizeof (FT_ULong) * alloc);
    if (*ucs4 == NULL)
        return;
  
    while (len && (step = FcUtf8ToUcs4(utf8, &u, len)) > 0)
    {
        if (n == alloc)
        {
            alloc *= 2;
            *ucs4 = realloc (*ucs4, sizeof (FT_ULong) * alloc);
            if (*ucs4 == NULL)
                return;
        }	  
        (*ucs4)[n++] = u;
        len -= step;
        utf8 += step;
    }
    *nchars = n;
}

/*
 * Split a matrix into the component pieces of scale and shape
 */

static void
_cairo_ft_font_compute_transform (ft_font_transform_t *sf, cairo_font_scale_t *sc)
{
    cairo_matrix_t normalized;
    double tx, ty;
    
    /* The font matrix has x and y "scale" components which we extract and
     * use as character scale values. These influence the way freetype
     * chooses hints, as well as selecting different bitmaps in
     * hand-rendered fonts. We also copy the normalized matrix to
     * freetype's transformation.
     */

    cairo_matrix_set_affine (&normalized,
			     sc->matrix[0][0],
			     sc->matrix[0][1],
			     sc->matrix[1][0],
			     sc->matrix[1][1], 
			     0, 0);

    _cairo_matrix_compute_scale_factors (&normalized, 
					 &sf->x_scale, &sf->y_scale,
					 /* XXX */ 1);
    cairo_matrix_scale (&normalized, 1.0 / sf->x_scale, 1.0 / sf->y_scale);
    cairo_matrix_get_affine (&normalized, 
			     &sf->shape[0][0], &sf->shape[0][1],
			     &sf->shape[1][0], &sf->shape[1][1],
			     &tx, &ty);
}

/*
 * Set the font transformation
 */

static void
_cairo_ft_font_install_transform (ft_font_transform_t *sf, FT_Face face)
{
    FT_Matrix mat;

    mat.xx = DOUBLE_TO_16_16(sf->shape[0][0]);
    mat.yx = -DOUBLE_TO_16_16(sf->shape[0][1]);
    mat.xy = -DOUBLE_TO_16_16(sf->shape[1][0]);
    mat.yy = DOUBLE_TO_16_16(sf->shape[1][1]);  

    FT_Set_Transform(face, &mat, NULL);

    FT_Set_Char_Size(face,
		     (FT_F26Dot6) (sf->x_scale * 64.0),
		     (FT_F26Dot6) (sf->y_scale * 64.0),
		     0, 0);
}

static void
_install_font_scale (cairo_font_scale_t *sc, FT_Face face)
{
    cairo_matrix_t normalized;
    double x_scale, y_scale;
    double xx, xy, yx, yy, tx, ty;
    FT_Matrix mat;
    
    /* The font matrix has x and y "scale" components which we extract and
     * use as character scale values. These influence the way freetype
     * chooses hints, as well as selecting different bitmaps in
     * hand-rendered fonts. We also copy the normalized matrix to
     * freetype's transformation.
     */

    cairo_matrix_set_affine (&normalized,
			     sc->matrix[0][0],
			     sc->matrix[0][1],
			     sc->matrix[1][0],
			     sc->matrix[1][1], 
			     0, 0);

    _cairo_matrix_compute_scale_factors (&normalized, &x_scale, &y_scale,
					 /* XXX */ 1);
    cairo_matrix_scale (&normalized, 1.0 / x_scale, 1.0 / y_scale);
    cairo_matrix_get_affine (&normalized, 
                             &xx /* 00 */ , &yx /* 01 */, 
                             &xy /* 10 */, &yy /* 11 */, 
                             &tx, &ty);

    mat.xx = DOUBLE_TO_16_16(xx);
    mat.xy = -DOUBLE_TO_16_16(xy);
    mat.yx = -DOUBLE_TO_16_16(yx);
    mat.yy = DOUBLE_TO_16_16(yy);  

    FT_Set_Transform(face, &mat, NULL);

    FT_Set_Pixel_Sizes(face,
		       (FT_UInt) x_scale,
		       (FT_UInt) y_scale);
}

static cairo_status_t 
_cairo_ft_font_text_to_glyphs (void			*abstract_font,
			       cairo_font_scale_t	*sc,
			       const unsigned char	*utf8,
			       cairo_glyph_t		**glyphs, 
			       int			*nglyphs)
{
    double x = 0., y = 0.;
    size_t i;
    FT_ULong *ucs4 = NULL;
    cairo_ft_font_t *font = abstract_font;
    FT_Face face = font->val->face;
    cairo_glyph_cache_key_t key;
    cairo_image_glyph_cache_entry_t *val;
    cairo_cache_t *cache;

    key.unscaled = &font->base;
    key.scale = *sc;

    _utf8_to_ucs4 (utf8, &ucs4, nglyphs);

    if (ucs4 == NULL)
        return CAIRO_STATUS_NO_MEMORY;

    *glyphs = (cairo_glyph_t *) malloc ((*nglyphs) * (sizeof (cairo_glyph_t)));
    if (*glyphs == NULL)
    {
        free (ucs4);
        return CAIRO_STATUS_NO_MEMORY;
    }

    _cairo_lock_global_image_glyph_cache ();
    cache = _cairo_get_global_image_glyph_cache ();
    if (cache == NULL) {
	_cairo_unlock_global_image_glyph_cache ();
	return CAIRO_STATUS_NO_MEMORY;
    }

    for (i = 0; i < *nglyphs; i++)
    {            
        (*glyphs)[i].index = FT_Get_Char_Index (face, ucs4[i]);
	(*glyphs)[i].x = x;
	(*glyphs)[i].y = y;
	
	val = NULL;
	key.index = (*glyphs)[i].index;

	if (_cairo_cache_lookup (cache, &key, (void **) &val) 
	    != CAIRO_STATUS_SUCCESS || val == NULL)
	    continue;

        x += val->extents.x_advance;
        y += val->extents.y_advance;
    }
    _cairo_unlock_global_image_glyph_cache ();

    free (ucs4);
    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t 
_cairo_ft_font_font_extents (void			*abstract_font,
			     cairo_font_scale_t   	*sc,
                             cairo_font_extents_t	*extents)
{
    cairo_ft_font_t *font = abstract_font;
    FT_Face face = font->val->face;
    FT_Size_Metrics *metrics = &face->size->metrics;
    ft_font_transform_t sf;

    _cairo_ft_font_compute_transform (&sf, sc);
    _cairo_ft_font_install_transform (&sf, face);

    /*
     * Get to unscaled metrics so that the upper level can get back to
     * user space
     */
    extents->ascent =        DOUBLE_FROM_26_6(metrics->ascender) / sf.y_scale;
    extents->descent =       DOUBLE_FROM_26_6(metrics->descender) / sf.y_scale;
    extents->height =        DOUBLE_FROM_26_6(metrics->height) / sf.y_scale;
    extents->max_x_advance = DOUBLE_FROM_26_6(metrics->max_advance) / sf.x_scale;

    /* FIXME: this doesn't do vertical layout atm. */
    extents->max_y_advance = 0.0;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_ft_font_glyph_extents (void			*abstract_font,
			      cairo_font_scale_t 	*sc,
                              cairo_glyph_t		*glyphs, 
                              int			num_glyphs,
			      cairo_text_extents_t	*extents)
{
    int i;
    cairo_ft_font_t *font = abstract_font;
    cairo_point_double_t origin;
    cairo_point_double_t glyph_min, glyph_max;
    cairo_point_double_t total_min, total_max;

    cairo_image_glyph_cache_entry_t *img = NULL;
    cairo_cache_t *cache;
    cairo_glyph_cache_key_t key;

    if (num_glyphs == 0)
    {
	extents->x_bearing = 0.0;
	extents->y_bearing = 0.0;
	extents->width  = 0.0;
	extents->height = 0.0;
	extents->x_advance = 0.0;
	extents->y_advance = 0.0;

	return CAIRO_STATUS_SUCCESS;
    }

    origin.x = glyphs[0].x;
    origin.y = glyphs[0].y;

    _cairo_lock_global_image_glyph_cache ();
    cache = _cairo_get_global_image_glyph_cache ();
    if (cache == NULL) {
	_cairo_unlock_global_image_glyph_cache ();
	return CAIRO_STATUS_NO_MEMORY;
    }
    
    key.unscaled = &font->base;
    key.scale = *sc;

    for (i = 0; i < num_glyphs; i++)
    {
	img = NULL;
	key.index = glyphs[i].index;
	if (_cairo_cache_lookup (cache, &key, (void **) &img) 
	    != CAIRO_STATUS_SUCCESS || img == NULL)
	    continue;
	
	/* XXX: Need to add code here to check the font's FcPattern
           for FC_VERTICAL_LAYOUT and if set get vertBearingX/Y
           instead. This will require that
           cairo_ft_font_create_for_ft_face accept an
           FcPattern. */
	glyph_min.x = glyphs[i].x + img->extents.x_bearing;
	glyph_min.y = glyphs[i].y + img->extents.y_bearing;
	glyph_max.x = glyph_min.x + img->extents.width;
	glyph_max.y = glyph_min.y + img->extents.height;
    
	if (i==0) {
	    total_min = glyph_min;
	    total_max = glyph_max;
	} else {
	    if (glyph_min.x < total_min.x)
		total_min.x = glyph_min.x;
	    if (glyph_min.y < total_min.y)
		total_min.y = glyph_min.y;

	    if (glyph_max.x > total_max.x)
		total_max.x = glyph_max.x;
	    if (glyph_max.y > total_max.y)
		total_max.y = glyph_max.y;
	}
    }
    _cairo_unlock_global_image_glyph_cache ();

    extents->x_bearing = (total_min.x - origin.x);
    extents->y_bearing = (total_min.y - origin.y);
    extents->width     = (total_max.x - total_min.x);
    extents->height    = (total_max.y - total_min.y);
    extents->x_advance = glyphs[i-1].x + (img == NULL ? 0 : img->extents.x_advance) - origin.x;
    extents->y_advance = glyphs[i-1].y + (img == NULL ? 0 : img->extents.y_advance) - origin.y;

    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t 
_cairo_ft_font_glyph_bbox (void		       		*abstract_font,
			   cairo_font_scale_t 		*sc,
			   const cairo_glyph_t 		*glyphs,
			   int                 		num_glyphs,
			   cairo_box_t         		*bbox)
{
    cairo_image_glyph_cache_entry_t *img;
    cairo_cache_t *cache;
    cairo_glyph_cache_key_t key;
    cairo_ft_font_t *font = abstract_font;

    cairo_fixed_t x1, y1, x2, y2;
    int i;

    bbox->p1.x = bbox->p1.y = CAIRO_MAXSHORT << 16;
    bbox->p2.x = bbox->p2.y = CAIRO_MINSHORT << 16;

    _cairo_lock_global_image_glyph_cache ();
    cache = _cairo_get_global_image_glyph_cache();

    if (cache == NULL 
	|| font == NULL
	|| glyphs == NULL) {
	_cairo_unlock_global_image_glyph_cache ();
        return CAIRO_STATUS_NO_MEMORY;
    }

    key.unscaled = &font->base;
    key.scale = *sc;

    for (i = 0; i < num_glyphs; i++)
    {

	img = NULL;
	key.index = glyphs[i].index;

	if (_cairo_cache_lookup (cache, &key, (void **) &img) 
	    != CAIRO_STATUS_SUCCESS || img == NULL)
	    continue;

	x1 = _cairo_fixed_from_double (glyphs[i].x + img->size.x);
	y1 = _cairo_fixed_from_double (glyphs[i].y + img->size.y);
	x2 = x1 + _cairo_fixed_from_double (img->size.width);
	y2 = y1 + _cairo_fixed_from_double (img->size.height);
	
	if (x1 < bbox->p1.x)
	    bbox->p1.x = x1;
	
	if (y1 < bbox->p1.y)
	    bbox->p1.y = y1;
	
	if (x2 > bbox->p2.x)
	    bbox->p2.x = x2;
	
	if (y2 > bbox->p2.y)
	    bbox->p2.y = y2;
    }
    _cairo_unlock_global_image_glyph_cache ();

    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t 
_cairo_ft_font_show_glyphs (void			*abstract_font,
			    cairo_font_scale_t 		*sc,
                            cairo_operator_t    	operator,
                            cairo_surface_t     	*source,
			    cairo_surface_t     	*surface,
			    int                 	source_x,
			    int                 	source_y,
                            const cairo_glyph_t 	*glyphs,
                            int                 	num_glyphs)
{
    cairo_image_glyph_cache_entry_t *img;
    cairo_cache_t *cache;
    cairo_glyph_cache_key_t key;
    cairo_ft_font_t *font = abstract_font;
    cairo_status_t status;

    double x, y;
    int i;

    _cairo_lock_global_image_glyph_cache ();
    cache = _cairo_get_global_image_glyph_cache();

    if (cache == NULL
	|| font == NULL 
        || source == NULL 
        || surface == NULL 
        || glyphs == NULL) {
	_cairo_unlock_global_image_glyph_cache ();
        return CAIRO_STATUS_NO_MEMORY;
    }

    key.unscaled = &font->base;
    key.scale = *sc;

    for (i = 0; i < num_glyphs; i++)
    {
	img = NULL;
	key.index = glyphs[i].index;

	if (_cairo_cache_lookup (cache, &key, (void **) &img) 
	    != CAIRO_STATUS_SUCCESS 
	    || img == NULL 
	    || img->image == NULL)
	    continue;
   
	x = glyphs[i].x;
	y = glyphs[i].y;

	status = _cairo_surface_composite (operator, source, 
					   &(img->image->base), 
					   surface,
					   source_x + x + img->size.x,
					   source_y + y + img->size.y,
					   0, 0, 
					   x + img->size.x, 
					   y + img->size.y, 
					   (double) img->size.width,
					   (double) img->size.height);

	if (status) {
	    _cairo_unlock_global_image_glyph_cache ();	    
	    return status;
	}
    }  
    _cairo_unlock_global_image_glyph_cache ();
    return CAIRO_STATUS_SUCCESS;
}


static int
_move_to (FT_Vector *to, void *closure)
{
    cairo_path_t *path = closure;
    cairo_point_t point;

    point.x = _cairo_fixed_from_26_6 (to->x);
    point.y = _cairo_fixed_from_26_6 (to->y);

    _cairo_path_close_path (path);
    _cairo_path_move_to (path, &point);

    return 0;
}

static int
_line_to (FT_Vector *to, void *closure)
{
    cairo_path_t *path = closure;
    cairo_point_t point;

    point.x = _cairo_fixed_from_26_6 (to->x);
    point.y = _cairo_fixed_from_26_6 (to->y);

    _cairo_path_line_to (path, &point);

    return 0;
}

static int
_conic_to (FT_Vector *control, FT_Vector *to, void *closure)
{
    cairo_path_t *path = closure;

    cairo_point_t p0, p1, p2, p3;
    cairo_point_t conic;

    _cairo_path_current_point (path, &p0);

    conic.x = _cairo_fixed_from_26_6 (control->x);
    conic.y = _cairo_fixed_from_26_6 (control->y);

    p3.x = _cairo_fixed_from_26_6 (to->x);
    p3.y = _cairo_fixed_from_26_6 (to->y);

    p1.x = p0.x + 2.0/3.0 * (conic.x - p0.x);
    p1.y = p0.y + 2.0/3.0 * (conic.y - p0.y);

    p2.x = p3.x + 2.0/3.0 * (conic.x - p3.x);
    p2.y = p3.y + 2.0/3.0 * (conic.y - p3.y);

    _cairo_path_curve_to (path,
			  &p1, &p2, &p3);

    return 0;
}

static int
_cubic_to (FT_Vector *control1, FT_Vector *control2, FT_Vector *to, void *closure)
{
    cairo_path_t *path = closure;
    cairo_point_t p0, p1, p2;

    p0.x = _cairo_fixed_from_26_6 (control1->x);
    p0.y = _cairo_fixed_from_26_6 (control1->y);

    p1.x = _cairo_fixed_from_26_6 (control2->x);
    p1.y = _cairo_fixed_from_26_6 (control2->y);

    p2.x = _cairo_fixed_from_26_6 (to->x);
    p2.y = _cairo_fixed_from_26_6 (to->y);

    _cairo_path_curve_to (path, &p0, &p1, &p2);

    return 0;
}

static cairo_status_t 
_cairo_ft_font_glyph_path (void				*abstract_font,
			   cairo_font_scale_t 		*sc,
                           cairo_glyph_t		*glyphs, 
                           int				num_glyphs,
                           cairo_path_t			*path)
{
    int i;
    cairo_ft_font_t *font = abstract_font;
    FT_GlyphSlot glyph;
    FT_Error error;
    FT_Outline_Funcs outline_funcs = {
	_move_to,
	_line_to,
	_conic_to,
	_cubic_to,
	0, /* shift */
	0, /* delta */
    };

    glyph = font->val->face->glyph;

    _install_font_scale (sc, font->val->face);

    for (i = 0; i < num_glyphs; i++)
    {
	FT_Matrix invert_y = {
	    DOUBLE_TO_16_16 (1.0), 0,
	    0, DOUBLE_TO_16_16 (-1.0),
	};

	error = FT_Load_Glyph (font->val->face, glyphs[i].index, FT_LOAD_DEFAULT);
	/* XXX: What to do in this error case? */
	if (error)
	    continue;
	/* XXX: Do we want to support bitmap fonts here? */
	if (glyph->format == ft_glyph_format_bitmap)
	    continue;

	/* Font glyphs have an inverted Y axis compared to cairo. */
	FT_Outline_Transform (&glyph->outline, &invert_y);
	FT_Outline_Translate (&glyph->outline,
			      DOUBLE_TO_26_6(glyphs[i].x),
			      DOUBLE_TO_26_6(glyphs[i].y));
	FT_Outline_Decompose (&glyph->outline, &outline_funcs, path);
    }
    _cairo_path_close_path (path);
    
    return CAIRO_STATUS_SUCCESS;
}


static cairo_status_t 
_cairo_ft_font_create_glyph(cairo_image_glyph_cache_entry_t 	*val)    
{
    cairo_ft_font_t *font = (cairo_ft_font_t *)val->key.unscaled;
    FT_GlyphSlot glyphslot;
    unsigned int width, height, stride;
    FT_Outline *outline;
    FT_BBox cbox;
    FT_Bitmap bitmap;
    FT_Glyph_Metrics *metrics;
    ft_font_transform_t sf;

    glyphslot = font->val->face->glyph;
    metrics = &glyphslot->metrics;

    _cairo_ft_font_compute_transform (&sf, &val->key.scale);
    _cairo_ft_font_install_transform (&sf, font->val->face);

    if (FT_Load_Glyph (font->val->face, val->key.index, FT_LOAD_DEFAULT) != 0)
	return CAIRO_STATUS_NO_MEMORY;

    /*
     * Note: the font's coordinate system is upside down from ours, so the
     * Y coordinates of the bearing and advance need to be negated.
     *
     * Scale metrics back to glyph space from the scaled glyph space returned
     * by FreeType
     */

    val->extents.x_bearing = DOUBLE_FROM_26_6 (metrics->horiBearingX) / sf.x_scale;
    val->extents.y_bearing = -DOUBLE_FROM_26_6 (metrics->horiBearingY) / sf.y_scale;

    val->extents.width  = DOUBLE_FROM_26_6 (metrics->width) / sf.x_scale;
    val->extents.height = DOUBLE_FROM_26_6 (metrics->height) / sf.y_scale;

    /*
     * use untransformed advance values
     * XXX uses horizontal advance only at present;
     should provide FT_LOAD_VERTICAL_LAYOUT
     */

    val->extents.x_advance = DOUBLE_FROM_26_6 (font->val->face->glyph->metrics.horiAdvance) / sf.x_scale;
    val->extents.y_advance = 0 / sf.y_scale;
    
    outline = &glyphslot->outline;

    FT_Outline_Get_CBox (outline, &cbox);

    cbox.xMin &= -64;
    cbox.yMin &= -64;
    cbox.xMax = (cbox.xMax + 63) & -64;
    cbox.yMax = (cbox.yMax + 63) & -64;
    
    width = (unsigned int) ((cbox.xMax - cbox.xMin) >> 6);
    height = (unsigned int) ((cbox.yMax - cbox.yMin) >> 6);
    stride = (width + 3) & -4;
    
    if (width * height == 0) 
	val->image = NULL;
    else	
    {

	bitmap.pixel_mode = ft_pixel_mode_grays;
	bitmap.num_grays  = 256;
	bitmap.width = width;
	bitmap.rows = height;
	bitmap.pitch = stride;   
	bitmap.buffer = calloc (1, stride * height);
	
	if (bitmap.buffer == NULL) {
	    return CAIRO_STATUS_NO_MEMORY;
	};
	
	FT_Outline_Translate (outline, -cbox.xMin, -cbox.yMin);
	
	if (FT_Outline_Get_Bitmap (glyphslot->library, outline, &bitmap) != 0) {
	    free (bitmap.buffer);
	    return CAIRO_STATUS_NO_MEMORY;
	}
	
	val->image = (cairo_image_surface_t *)
	cairo_image_surface_create_for_data ((char *) bitmap.buffer,
					     CAIRO_FORMAT_A8,
					     width, height, stride);
	if (val->image == NULL) {
	    free (bitmap.buffer);
	    return CAIRO_STATUS_NO_MEMORY;
	}
	
	_cairo_image_surface_assume_ownership_of_data (val->image);
    }

    /*
     * Note: the font's coordinate system is upside down from ours, so the
     * Y coordinate of the control box needs to be negated.
     */

    val->size.width = (unsigned short) width;
    val->size.height = (unsigned short) height;
    val->size.x =   (short) (cbox.xMin >> 6);
    val->size.y = - (short) (cbox.yMax >> 6);
    
    return CAIRO_STATUS_SUCCESS;
}

const cairo_font_backend_t cairo_ft_font_backend = {
    _cairo_ft_font_create,
    _cairo_ft_font_destroy,
    _cairo_ft_font_font_extents,
    _cairo_ft_font_text_to_glyphs,
    _cairo_ft_font_glyph_extents,
    _cairo_ft_font_glyph_bbox,
    _cairo_ft_font_show_glyphs,
    _cairo_ft_font_glyph_path,
    _cairo_ft_font_create_glyph
};


/* implement the platform-specific interface */

cairo_font_t *
cairo_ft_font_create (FT_Library ft_library, FcPattern *pattern)
{    
    cairo_font_scale_t scale;
    cairo_font_t *scaled;
    cairo_ft_font_t *f = NULL;
    ft_font_val_t *v = NULL;
    FcPattern *dup;

    scale.matrix[0][0] = 1.;
    scale.matrix[0][1] = 0.;
    scale.matrix[1][0] = 0.;
    scale.matrix[1][1] = 1.;

    scaled = malloc (sizeof (cairo_font_t));
    if (scaled == NULL)
	goto FAIL;

    dup = FcPatternDuplicate(pattern);    
    if (dup == NULL)
	goto FREE_SCALED;

    v = _create_from_library_and_pattern (ft_library, pattern);
    if (v == NULL)
	goto FREE_PATTERN;

    f = malloc (sizeof(cairo_ft_font_t));
    if (f == NULL) 
	goto FREE_VAL;

    if (_cairo_unscaled_font_init (&f->base, &cairo_ft_font_backend))
	goto FREE_VAL;

    f->pattern = dup;
    f->val = v;

    _cairo_font_init (scaled, &scale, &f->base);

    return scaled;

 FREE_VAL:
    _destroy_font_val (v);

 FREE_PATTERN:
    FcPatternDestroy (dup);

 FREE_SCALED:
    free (scaled);

 FAIL:
    return NULL;
}

cairo_font_t *
cairo_ft_font_create_for_ft_face (FT_Face face)
{
    cairo_font_scale_t scale;
    cairo_font_t *scaled;
    cairo_ft_font_t *f = NULL;
    ft_font_val_t *v = NULL;

    scale.matrix[0][0] = 1.;
    scale.matrix[0][1] = 0.;
    scale.matrix[1][0] = 0.;
    scale.matrix[1][1] = 1.;

    scaled = malloc (sizeof (cairo_font_t));
    if (scaled == NULL)
	goto FAIL;

    v = _create_from_face (face, 0);
    if (v == NULL)
	goto FREE_SCALED;

    f = malloc (sizeof(cairo_ft_font_t));
    if (f == NULL) 
	goto FREE_VAL;

    _cairo_unscaled_font_init (&f->base, &cairo_ft_font_backend);
    f->pattern = NULL;
    f->val = v;

    _cairo_font_init (scaled, &scale, &f->base);

    return scaled;

 FREE_VAL:
    _destroy_font_val (v);

 FREE_SCALED:
    free (scaled);

 FAIL:
    return NULL;
}

FT_Face
cairo_ft_font_face (cairo_font_t *abstract_font)
{
    cairo_ft_font_t *font = (cairo_ft_font_t *) abstract_font->unscaled;

    if (font == NULL || font->val == NULL)
        return NULL;

    return font->val->face;
}

FcPattern *
cairo_ft_font_pattern (cairo_font_t *abstract_font)
{
    cairo_ft_font_t *font = (cairo_ft_font_t *) abstract_font->unscaled;

    if (font == NULL)
        return NULL;

    return font->pattern;
}
