/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *      Graydon Hoare <graydon@redhat.com>
 *	Owen Taylor <otaylor@redhat.com>
 */

#include "cairo-ft-private.h"

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

/* This is the max number of FT_face objects we keep open at once
 */
#define MAX_OPEN_FACES 10

/*
 * The simple 2x2 matrix is converted into separate scale and shape
 * factors so that hinting works right
 */

typedef struct {
    double  x_scale, y_scale;
    double  shape[2][2];
} ft_font_transform_t;

/* 
 * We create an object that corresponds to a single font on the disk;
 * (identified by a filename/id pair) these are shared between all
 * fonts using that file.  For cairo_ft_font_create_for_ft_face(), we
 * just create a one-off version with a permanent face value.
 */

typedef struct {
    cairo_unscaled_font_t base;

    int from_face;	/* from cairo_ft_font_create_for_ft_face()? */
    FT_Face face;	/* provided or cached face */

    /* only set if from_face is false */
    FT_Library library;
    char *filename;
    int id;

    /* We temporarily scale the unscaled font as neede */
    int have_scale;
    cairo_font_scale_t current_scale;
    double x_scale;		/* Extracted X scale factor */
    double y_scale;             /* Extracted Y scale factor */
    
    int lock;		/* count of how many times this font has been locked */
} ft_unscaled_font_t;

const cairo_font_backend_t cairo_ft_font_backend;

static ft_unscaled_font_t *
_ft_unscaled_font_create_from_face (FT_Face face)
{
    ft_unscaled_font_t *unscaled = malloc (sizeof(ft_unscaled_font_t));
    if (!unscaled)
	return NULL;
	
    unscaled->from_face = 1;
    unscaled->face = face;

    unscaled->library = NULL;
    unscaled->filename = NULL;
    unscaled->id = 0;
    
    unscaled->have_scale = 0;
    unscaled->lock = 0;

    _cairo_unscaled_font_init ((cairo_unscaled_font_t *)unscaled,
			       &cairo_ft_font_backend);
    return unscaled;
}

static ft_unscaled_font_t *
_ft_unscaled_font_create_from_filename (FT_Library  library,
					const char *filename,
					int         id)
{
    ft_unscaled_font_t *unscaled;
    char *new_filename;
    
    new_filename = strdup (filename);
    if (!new_filename)
	return NULL;

    unscaled = malloc (sizeof (ft_unscaled_font_t));
    if (!unscaled) {
	free (new_filename);
	return NULL;
    }
	
    unscaled->from_face = 0;
    unscaled->face = NULL;

    unscaled->library = library;
    unscaled->filename = new_filename;
    unscaled->id = id;
    
    unscaled->have_scale = 0;
    unscaled->lock = 0;
    
    _cairo_unscaled_font_init ((cairo_unscaled_font_t *)unscaled,
			       &cairo_ft_font_backend);
    return unscaled;
}

/*
 * We keep a global cache from [file/id] => [ft_unscaled_font_t]. This
 * hash isn't limited in size. However, we limit the number of
 * FT_Face objects we keep around; when we've exceeeded that
 * limit and need to create a new FT_Face, we dump the FT_Face from
 * a random ft_unscaled_font_t.
 */

typedef struct {
    cairo_cache_entry_base_t base;
    char *filename;
    int id;
} cairo_ft_cache_key_t;

typedef struct {
    cairo_ft_cache_key_t key;
    ft_unscaled_font_t *unscaled;
} cairo_ft_cache_entry_t;

typedef struct {
    cairo_cache_t base;
    FT_Library lib;
    int n_faces;		/* Number of open FT_Face objects */
} ft_cache_t;

static unsigned long
_ft_font_cache_hash (void *cache, void *key)
{
    cairo_ft_cache_key_t *in = (cairo_ft_cache_key_t *) key;
    unsigned long hash;

    /* 1607 is just a random prime. */
    hash = _cairo_hash_string (in->filename);
    hash += ((unsigned long) in->id) * 1607;
	
    return hash;
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

    return strcmp (a->filename, b->filename) == 0 &&
	a->id == b->id;
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

    entry->unscaled = _ft_unscaled_font_create_from_filename (ftcache->lib,
							      k->filename,
							      k->id);
    if (!entry->unscaled) {
	free (entry);
	return CAIRO_STATUS_NO_MEMORY;
    }
    
    entry->key.base.memory = 0;
    entry->key.filename = entry->unscaled->filename;
    entry->key.id = entry->unscaled->id;
    
    *return_entry = entry;

    return CAIRO_STATUS_SUCCESS;
}

/* Entries are never spontaneously destroyed; but only when
 * we remove them from the cache specifically. We free entry->unscaled
 * in the code that removes the entry from the cache
 */
static void
_ft_font_cache_destroy_entry (void *cache,
			      void *entry)
{    
    cairo_ft_cache_entry_t *e = (cairo_ft_cache_entry_t *) entry;

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
			       0)) /* No memory limit */
	    goto FAIL;
	
	if (FT_Init_FreeType (&_global_ft_cache->lib)) 
	    goto FAIL;
	_global_ft_cache->n_faces = 0;
    }
    return &_global_ft_cache->base;

 FAIL:
    if (_global_ft_cache)
	free (_global_ft_cache);
    _global_ft_cache = NULL;
    return NULL;
}

/* Finds or creates a ft_unscaled_font for the filename/id from pattern.
 * Returns a new reference to the unscaled font.
 */
static ft_unscaled_font_t *
_ft_unscaled_font_get_for_pattern (FcPattern *pattern)
{
    cairo_ft_cache_entry_t *entry;
    cairo_ft_cache_key_t key;
    cairo_cache_t *cache;
    cairo_status_t status;
    FcChar8 *filename;
    int created_entry;
    
    if (FcPatternGetString (pattern, FC_FILE, 0, &filename) != FcResultMatch)
	return NULL;
    key.filename = (char *)filename;
	    
    if (FcPatternGetInteger (pattern, FC_INDEX, 0, &key.id) != FcResultMatch)
	return NULL;
    
    _lock_global_ft_cache ();
    cache = _get_global_ft_cache ();
    if (cache == NULL) {
	_unlock_global_ft_cache ();
	return NULL;
    }

    status = _cairo_cache_lookup (cache, &key, (void **) &entry, &created_entry);
    _unlock_global_ft_cache ();
    if (status)
	return NULL;

    if (!created_entry)
	_cairo_unscaled_font_reference ((cairo_unscaled_font_t *)entry->unscaled);

    return entry->unscaled;
}

static int
_has_unlocked_face (void *entry)
{
    cairo_ft_cache_entry_t *e = entry;

    return (e->unscaled->lock == 0 && e->unscaled->face);
}

/* Ensures that an unscaled font has a face object. If we exceed
 * MAX_OPEN_FACES, try to close some.
 */
static FT_Face
_ft_unscaled_font_lock_face (ft_unscaled_font_t *unscaled)
{
    ft_cache_t *ftcache;
    FT_Face face = NULL;

    if (unscaled->face) {
	unscaled->lock++;
	return unscaled->face;
    }

    assert (!unscaled->from_face);
    
    _lock_global_ft_cache ();
    ftcache = (ft_cache_t *) _get_global_ft_cache ();
    assert (ftcache != NULL);
    
    while (ftcache->n_faces >= MAX_OPEN_FACES) {
	cairo_ft_cache_entry_t *entry;
    
	entry = _cairo_cache_random_entry ((cairo_cache_t *)ftcache, _has_unlocked_face);
	if (entry) {
	    FT_Done_Face (entry->unscaled->face);
	    entry->unscaled->face = NULL;
	    entry->unscaled->have_scale = 0;
	    ftcache->n_faces--;
	} else {
	    break;
	}
    }

    if (FT_New_Face (ftcache->lib,
		     unscaled->filename,
		     unscaled->id,
		     &face) != FT_Err_Ok)
	goto FAIL;

    unscaled->face = face;
    unscaled->lock++;
    ftcache->n_faces++;

 FAIL:
    _unlock_global_ft_cache ();
    return face;
}

/* Unlock unscaled font locked with _ft_unscaled_font_lock_face
 */
static void
_ft_unscaled_font_unlock_face (ft_unscaled_font_t *unscaled)
{
    assert (unscaled->lock > 0);
    
    unscaled->lock--;
}

static void
_compute_transform (ft_font_transform_t *sf,
		    cairo_font_scale_t  *sc)
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

/* Temporarily scales an unscaled font to the give scale. We catch
 * scaling to the same size, since changing a FT_Face is expensive.
 */
static void
_ft_unscaled_font_set_scale (ft_unscaled_font_t *unscaled,
			     cairo_font_scale_t *scale)
{
    ft_font_transform_t sf;
    FT_Matrix mat;

    assert (unscaled->face != NULL);
    
    if (unscaled->have_scale &&
	scale->matrix[0][0] == unscaled->current_scale.matrix[0][0] &&
	scale->matrix[0][1] == unscaled->current_scale.matrix[0][1] &&
	scale->matrix[1][0] == unscaled->current_scale.matrix[1][0] &&
	scale->matrix[1][1] == unscaled->current_scale.matrix[1][1])
	return;

    unscaled->have_scale = 1;
    unscaled->current_scale = *scale;
	
    _compute_transform (&sf, scale);

    unscaled->x_scale = sf.x_scale;
    unscaled->y_scale = sf.y_scale;
	
    mat.xx = DOUBLE_TO_16_16(sf.shape[0][0]);
    mat.yx = - DOUBLE_TO_16_16(sf.shape[0][1]);
    mat.xy = - DOUBLE_TO_16_16(sf.shape[1][0]);
    mat.yy = DOUBLE_TO_16_16(sf.shape[1][1]);

    FT_Set_Transform(unscaled->face, &mat, NULL);
    
    FT_Set_Pixel_Sizes(unscaled->face,
		       (FT_UInt) sf.x_scale,
		       (FT_UInt) sf.y_scale);
}

/* implement the font backend interface */

typedef struct {
    cairo_font_t base;
    FcPattern *pattern;
    int load_flags;
    ft_unscaled_font_t *unscaled;
} cairo_ft_font_t;

/* for compatibility with older freetype versions */
#ifndef FT_LOAD_TARGET_MONO
#define FT_LOAD_TARGET_MONO  FT_LOAD_MONOCHROME
#endif

/* The load flags passed to FT_Load_Glyph control aspects like hinting and
 * antialiasing. Here we compute them from the fields of a FcPattern.
 */
static int
_get_load_flags (FcPattern *pattern)
{
    FcBool antialias, hinting, autohint;
#ifdef FC_HINT_STYLE    
    int hintstyle;
#endif    
    int load_flags = 0;

    /* disable antialiasing if requested */
    if (FcPatternGetBool (pattern,
			  FC_ANTIALIAS, 0, &antialias) != FcResultMatch)
	antialias = FcTrue;
    
    if (antialias)
	load_flags |= FT_LOAD_NO_BITMAP;
    else
	load_flags |= FT_LOAD_TARGET_MONO;
    
    /* disable hinting if requested */
    if (FcPatternGetBool (pattern,
			  FC_HINTING, 0, &hinting) != FcResultMatch)
 	hinting = FcTrue;
    
#ifdef FC_HINT_STYLE    
    if (FcPatternGetInteger (pattern, FC_HINT_STYLE, 0, &hintstyle) != FcResultMatch)
	hintstyle = FC_HINT_FULL;

    if (!hinting || hintstyle == FC_HINT_NONE)
	load_flags |= FT_LOAD_NO_HINTING;
    
    switch (hintstyle) {
    case FC_HINT_SLIGHT:
    case FC_HINT_MEDIUM:
	load_flags |= FT_LOAD_TARGET_LIGHT;
	break;
    default:
	load_flags |= FT_LOAD_TARGET_NORMAL;
	break;
    }
#else /* !FC_HINT_STYLE */
    if (!hinting)
	load_flags |= FT_LOAD_NO_HINTING;
#endif /* FC_FHINT_STYLE */
    
    /* force autohinting if requested */
    if (FcPatternGetBool (pattern,
			  FC_AUTOHINT, 0, &autohint) != FcResultMatch)
	autohint = FcFalse;
    
    if (autohint)
	load_flags |= FT_LOAD_FORCE_AUTOHINT;
    
    return load_flags;
}

/* Like the public cairo_ft_font_create, but takes a cairo_font_scale_t,
 * rather than a cairo_font_t
 */
static cairo_font_t *
_ft_font_create (FcPattern          *pattern,
		 cairo_font_scale_t *scale)
{    
    cairo_ft_font_t *f = NULL;
    ft_unscaled_font_t *unscaled = NULL;

    unscaled = _ft_unscaled_font_get_for_pattern (pattern);
    if (unscaled == NULL)
	return NULL;

    f = malloc (sizeof(cairo_ft_font_t));
    if (f == NULL) 
	goto FREE_UNSCALED;

    f->unscaled = unscaled;
    f->pattern = pattern;
    FcPatternReference (pattern);
    f->load_flags = _get_load_flags (pattern);

    _cairo_font_init ((cairo_font_t *)f, scale, &cairo_ft_font_backend);

    return (cairo_font_t *)f;

 FREE_UNSCALED:
    _cairo_unscaled_font_destroy ((cairo_unscaled_font_t *)unscaled);

    return NULL;
}

static cairo_status_t
_cairo_ft_font_create (const char          *family, 
                       cairo_font_slant_t   slant, 
                       cairo_font_weight_t  weight,
		       cairo_font_scale_t  *scale,
		       cairo_font_t       **font)
{
    FcPattern *pattern, *resolved;
    cairo_font_t *new_font;
    FcResult result;
    int fcslant;
    int fcweight;
    ft_font_transform_t sf;

    pattern = FcPatternCreate ();
    if (!pattern)
	return CAIRO_STATUS_NO_MEMORY;

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

    if (!FcPatternAddString (pattern, FC_FAMILY, family))
	goto FREE_PATTERN;
    if (!FcPatternAddInteger (pattern, FC_SLANT, fcslant))
	goto FREE_PATTERN;
    if (!FcPatternAddInteger (pattern, FC_WEIGHT, fcweight))
	goto FREE_PATTERN;

    _compute_transform (&sf, scale);

    FcPatternAddInteger (pattern, FC_PIXEL_SIZE, sf.y_scale);

    FcConfigSubstitute (NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute (pattern);
    
    resolved = FcFontMatch (NULL, pattern, &result);
    if (!resolved)
	goto FREE_PATTERN;

    new_font = _ft_font_create (resolved, scale);

    FcPatternDestroy (resolved);
    FcPatternDestroy (pattern);

    if (new_font) {
	*font = new_font;
	return CAIRO_STATUS_SUCCESS;
    } else {
	return CAIRO_STATUS_NO_MEMORY; /* A guess */
    }

 FREE_PATTERN:
    FcPatternDestroy (pattern);

    return CAIRO_STATUS_NO_MEMORY;
}

static void 
_cairo_ft_font_destroy_font (void *abstract_font)
{
    cairo_ft_font_t * font = abstract_font;
  
    if (font == NULL)
        return;
  
    if (font->pattern != NULL)
        FcPatternDestroy (font->pattern);

    _cairo_unscaled_font_destroy ((cairo_unscaled_font_t *)font->unscaled);

    free (font);
}

static void 
_cairo_ft_font_destroy_unscaled_font (void *abstract_font)
{
    ft_unscaled_font_t *unscaled  = abstract_font;

    if (!unscaled->from_face) {
	cairo_cache_t *cache;
	cairo_ft_cache_key_t key;
	
	_lock_global_ft_cache ();
	cache = _get_global_ft_cache ();
	assert (cache);

	key.filename = unscaled->filename;
	key.id = unscaled->id;
	
	_cairo_cache_remove (cache, &key);
	
	_unlock_global_ft_cache ();
    }
    
    if (unscaled == NULL)
        return;

    if (!unscaled->from_face && unscaled->face)
	FT_Done_Face (unscaled->face);

    if (unscaled->filename)
	free (unscaled->filename);
  
    free (unscaled);
}

static void
_cairo_ft_font_get_glyph_cache_key (void                    *abstract_font,
				    cairo_glyph_cache_key_t *key)
{
    cairo_ft_font_t *font = abstract_font;

    key->unscaled = (cairo_unscaled_font_t *)font->unscaled;
    key->scale = font->base.scale;
    key->flags = font->load_flags;
}

static cairo_status_t 
_cairo_ft_font_text_to_glyphs (void			*abstract_font,
			       const unsigned char	*utf8,
			       cairo_glyph_t		**glyphs, 
			       int			*nglyphs)
{
    double x = 0., y = 0.;
    size_t i;
    uint32_t *ucs4 = NULL;
    cairo_ft_font_t *font = abstract_font;
    FT_Face face;
    cairo_glyph_cache_key_t key;
    cairo_image_glyph_cache_entry_t *val;
    cairo_cache_t *cache = NULL;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    _cairo_ft_font_get_glyph_cache_key (font, &key);

    status = _cairo_utf8_to_ucs4 (utf8, -1, &ucs4, nglyphs);
    if (!CAIRO_OK (status))
	return status;

    face = cairo_ft_font_lock_face ((cairo_font_t *)font);
    if (!face) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto FAIL1;
    }

    _cairo_lock_global_image_glyph_cache ();
    cache = _cairo_get_global_image_glyph_cache ();
    if (cache == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto FAIL2;
    }

    *glyphs = (cairo_glyph_t *) malloc ((*nglyphs) * (sizeof (cairo_glyph_t)));
    if (*glyphs == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto FAIL2;
    }

    for (i = 0; i < *nglyphs; i++)
    {            
        (*glyphs)[i].index = FT_Get_Char_Index (face, ucs4[i]);
	(*glyphs)[i].x = x;
	(*glyphs)[i].y = y;
	
	val = NULL;
	key.index = (*glyphs)[i].index;

	if (_cairo_cache_lookup (cache, &key, (void **) &val, NULL) 
	    != CAIRO_STATUS_SUCCESS || val == NULL)
	    continue;

        x += val->extents.x_advance;
        y += val->extents.y_advance;
    }

 FAIL2:
    if (cache)
	_cairo_unlock_global_image_glyph_cache ();

    cairo_ft_font_unlock_face ((cairo_font_t *)font);
    
 FAIL1:
    free (ucs4);
    
    return status;
}


static cairo_status_t 
_cairo_ft_font_font_extents (void			*abstract_font,
                             cairo_font_extents_t	*extents)
{
    cairo_ft_font_t *font = abstract_font;
    FT_Face face;
    FT_Size_Metrics *metrics;
    
    face = _ft_unscaled_font_lock_face (font->unscaled);
    if (!face)
	return CAIRO_STATUS_NO_MEMORY;

    metrics = &face->size->metrics;

    _ft_unscaled_font_set_scale (font->unscaled, &font->base.scale);
    
    /*
     * Get to unscaled metrics so that the upper level can get back to
     * user space
     */
    extents->ascent =        DOUBLE_FROM_26_6(metrics->ascender) / font->unscaled->y_scale;
    extents->descent =       DOUBLE_FROM_26_6(metrics->descender) / font->unscaled->y_scale;
    extents->height =        DOUBLE_FROM_26_6(metrics->height) / font->unscaled->y_scale;
    extents->max_x_advance = DOUBLE_FROM_26_6(metrics->max_advance) / font->unscaled->x_scale;

    /* FIXME: this doesn't do vertical layout atm. */
    extents->max_y_advance = 0.0;

    _ft_unscaled_font_unlock_face (font->unscaled);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_ft_font_glyph_extents (void			*abstract_font,
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
    
    _cairo_ft_font_get_glyph_cache_key (font, &key);

    for (i = 0; i < num_glyphs; i++)
    {
	img = NULL;
	key.index = glyphs[i].index;
	if (_cairo_cache_lookup (cache, &key, (void **) &img, NULL) 
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

    _cairo_ft_font_get_glyph_cache_key (font, &key);
    
    for (i = 0; i < num_glyphs; i++)
    {

	img = NULL;
	key.index = glyphs[i].index;

	if (_cairo_cache_lookup (cache, &key, (void **) &img, NULL) 
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
                            cairo_operator_t    	operator,
                            cairo_pattern_t     	*pattern,
			    cairo_surface_t     	*surface,
			    int                 	source_x,
			    int                 	source_y,
			    int				dest_x,
			    int				dest_y,
			    unsigned int		width,
			    unsigned int		height,
                            const cairo_glyph_t 	*glyphs,
                            int                 	num_glyphs)
{
    cairo_image_glyph_cache_entry_t *img;
    cairo_cache_t *cache;
    cairo_glyph_cache_key_t key;
    cairo_ft_font_t *font = abstract_font;
    cairo_surface_pattern_t glyph_pattern;
    cairo_status_t status;
    int x, y;
    int i;

    _cairo_lock_global_image_glyph_cache ();
    cache = _cairo_get_global_image_glyph_cache();

    if (cache == NULL
	|| font == NULL 
        || pattern == NULL 
        || surface == NULL 
        || glyphs == NULL) {
	_cairo_unlock_global_image_glyph_cache ();
        return CAIRO_STATUS_NO_MEMORY;
    }

    key.unscaled = (cairo_unscaled_font_t *)font->unscaled;
    key.scale = font->base.scale;
    key.flags = font->load_flags;

    for (i = 0; i < num_glyphs; i++)
    {
	img = NULL;
	key.index = glyphs[i].index;

	if (_cairo_cache_lookup (cache, &key, (void **) &img, NULL) 
	    != CAIRO_STATUS_SUCCESS 
	    || img == NULL 
	    || img->image == NULL)
	    continue;
   
	x = (int) floor (glyphs[i].x + 0.5);
	y = (int) floor (glyphs[i].y + 0.5);

	_cairo_pattern_init_for_surface (&glyph_pattern, &(img->image->base));

	status = _cairo_surface_composite (operator, pattern, 
					   &glyph_pattern.base, 
					   surface,
					   x + img->size.x,
					   y + img->size.y,
					   0, 0, 
					   x + img->size.x, 
					   y + img->size.y, 
					   (double) img->size.width,
					   (double) img->size.height);

	_cairo_pattern_fini (&glyph_pattern.base);

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
                           cairo_glyph_t		*glyphs, 
                           int				num_glyphs,
                           cairo_path_t			*path)
{
    int i;
    cairo_ft_font_t *font = abstract_font;
    FT_GlyphSlot glyph;
    FT_Face face;
    FT_Error error;
    FT_Outline_Funcs outline_funcs = {
	_move_to,
	_line_to,
	_conic_to,
	_cubic_to,
	0, /* shift */
	0, /* delta */
    };
    
    face = cairo_ft_font_lock_face (abstract_font);
    if (!face)
	return CAIRO_STATUS_NO_MEMORY;

    glyph = face->glyph;

    for (i = 0; i < num_glyphs; i++)
    {
	FT_Matrix invert_y = {
	    DOUBLE_TO_16_16 (1.0), 0,
	    0, DOUBLE_TO_16_16 (-1.0),
	};

	error = FT_Load_Glyph (font->unscaled->face, glyphs[i].index, font->load_flags | FT_LOAD_NO_BITMAP);
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

    cairo_ft_font_unlock_face (abstract_font);
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t 
_cairo_ft_font_create_glyph (cairo_image_glyph_cache_entry_t *val)    
{
    ft_unscaled_font_t *unscaled = (ft_unscaled_font_t *)val->key.unscaled;
    FT_GlyphSlot glyphslot;
    unsigned int width, height, stride;
    FT_Face face;
    FT_Outline *outline;
    FT_BBox cbox;
    FT_Bitmap bitmap;
    FT_Glyph_Metrics *metrics;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    glyphslot = unscaled->face->glyph;
    metrics = &glyphslot->metrics;

    face = _ft_unscaled_font_lock_face (unscaled);
    if (!face)
	return CAIRO_STATUS_NO_MEMORY;

    _ft_unscaled_font_set_scale (unscaled, &val->key.scale);

    if (FT_Load_Glyph (face, val->key.index, val->key.flags) != 0) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto FAIL;
    }

    /*
     * Note: the font's coordinate system is upside down from ours, so the
     * Y coordinates of the bearing and advance need to be negated.
     *
     * Scale metrics back to glyph space from the scaled glyph space returned
     * by FreeType
     */

    val->extents.x_bearing = DOUBLE_FROM_26_6 (metrics->horiBearingX) / unscaled->x_scale;
    val->extents.y_bearing = -DOUBLE_FROM_26_6 (metrics->horiBearingY) / unscaled->y_scale;

    val->extents.width  = DOUBLE_FROM_26_6 (metrics->width) / unscaled->x_scale;
    val->extents.height = DOUBLE_FROM_26_6 (metrics->height) / unscaled->y_scale;

    /*
     * use untransformed advance values
     * XXX uses horizontal advance only at present;
     should provide FT_LOAD_VERTICAL_LAYOUT
     */

    val->extents.x_advance = DOUBLE_FROM_26_6 (face->glyph->metrics.horiAdvance) / unscaled->x_scale;
    val->extents.y_advance = 0 / unscaled->y_scale;
    
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
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto FAIL;
	}
	
	FT_Outline_Translate (outline, -cbox.xMin, -cbox.yMin);
	
	if (FT_Outline_Get_Bitmap (glyphslot->library, outline, &bitmap) != 0) {
	    free (bitmap.buffer);
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto FAIL;
	}
	
	val->image = (cairo_image_surface_t *)
	cairo_image_surface_create_for_data ((char *) bitmap.buffer,
					     CAIRO_FORMAT_A8,
					     width, height, stride);
	if (val->image == NULL) {
	    free (bitmap.buffer);
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto FAIL;
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
    
 FAIL:
    _ft_unscaled_font_unlock_face (unscaled);
    
    return status;
}

const cairo_font_backend_t cairo_ft_font_backend = {
    _cairo_ft_font_create,
    _cairo_ft_font_destroy_font,
    _cairo_ft_font_destroy_unscaled_font,
    _cairo_ft_font_font_extents,
    _cairo_ft_font_text_to_glyphs,
    _cairo_ft_font_glyph_extents,
    _cairo_ft_font_glyph_bbox,
    _cairo_ft_font_show_glyphs,
    _cairo_ft_font_glyph_path,
    _cairo_ft_font_get_glyph_cache_key,
    _cairo_ft_font_create_glyph
};

/* implement the platform-specific interface */

/**
 * cairo_ft_font_create:
 * @pattern: A fully resolved fontconfig
 *   pattern. A pattern can be resolved, by, among other things, calling
 *   FcConfigSubstitute(), FcDefaultSubstitute(), then
 *   FcFontMatch(). Cairo will call FcPatternReference() on this
 *   pattern, so you should not further modify the pattern, but you can
 *   release your reference to the pattern with FcPatternDestroy() if
 *   you no longer need to access it.
 * @scale: The scale at which this font will be used. The
 *   scale is given by multiplying the font matrix (see
 *   cairo_transform_font()) by the current transformation matrix.
 *   The translation elements of the resulting matrix are ignored.
 * 
 * Creates a new font for the FreeType font backend based on a
 * fontconfig pattern. This font can then be used with
 * cairo_set_font(), cairo_font_glyph_extents(), or FreeType backend
 * specific functions like cairo_ft_font_lock_face().
 *
 * Return value: a newly created #cairo_font_t. Free with
 *  cairo_font_destroy() when you are done using it.
 **/
cairo_font_t *
cairo_ft_font_create (FcPattern      *pattern,
		      cairo_matrix_t *scale)
{    
    cairo_font_scale_t sc;
    double tx, ty;

    cairo_matrix_get_affine (scale,
			     &sc.matrix[0][0], &sc.matrix[0][1],
			     &sc.matrix[1][0], &sc.matrix[1][1],
			     &tx, &ty);

    return _ft_font_create (pattern, &sc);
}

/**
 * cairo_ft_font_create_for_ft_face:
 * @face: A FreeType face object, already opened. This must
 *   be kept around until the font object's refcount drops to
 *   zero and it is freed. The font object can be kept alive by
 *   internal caching, so it's safest to keep the face object
 *   around forever.
 * @load_flags: The flags to pass to FT_Load_Glyph when loading
 *   glyphs from the font. These flags control aspects of
 *   rendering such as hinting and antialiasing. See the FreeType
 *   docs for full information.
 * @scale: The scale at which this font will be used. The
 *   scale is given by multiplying the font matrix (see
 *   cairo_transform_font()) by the current transformation matrix.
 *   The translation elements of the resulting matrix are ignored.
 * 
 * Creates a new font forthe FreeType font backend from a pre-opened
 * FreeType face. This font can then be used with cairo_set_font(),
 * cairo_font_glyph_extents(), or FreeType backend specific
 * functions like cairo_ft_font_lock_face() Cairo will determine the
 * pixel size and transformation from the @scale parameter and call
 * FT_Set_Transform() and FT_Set_Pixel_Sizes().
 * 
 * Return value: a newly created #cairo_font_t. Free with
 *  cairo_font_destroy() when you are done using it.
 **/
cairo_font_t *
cairo_ft_font_create_for_ft_face (FT_Face         face,
				  int             load_flags,
				  cairo_matrix_t *scale)
{
    cairo_ft_font_t *f = NULL;
    ft_unscaled_font_t *unscaled = NULL;
    cairo_font_scale_t sc;
    double tx, ty;

    unscaled = _ft_unscaled_font_create_from_face (face);
    if (unscaled == NULL)
	return NULL;

    f = malloc (sizeof(cairo_ft_font_t));
    if (f == NULL) 
	goto FREE_UNSCALED;

    f->unscaled = unscaled;
    f->pattern = NULL;
    f->load_flags = load_flags;

    cairo_matrix_get_affine (scale,
			     &sc.matrix[0][0], &sc.matrix[0][1],
			     &sc.matrix[1][0], &sc.matrix[1][1],
			     &tx, &ty);

    _cairo_font_init ((cairo_font_t *)f, &sc, &cairo_ft_font_backend);

    return (cairo_font_t *)f;

 FREE_UNSCALED:
    _cairo_unscaled_font_destroy ((cairo_unscaled_font_t *)unscaled);

    return NULL;
}


/**
 * cairo_ft_font_lock_face:
 * @ft_font: A #cairo_font_t from the FreeType font backend. Such an
 *   object can be created with cairo_ft_font_create() or
 *   cairo_ft_font_create_for_ft_face(). On some platforms the font from
 *   cairo_current_font() will also be a FreeType font, but using this
 *   functionality with fonts you don't create yourself is not
 *   recommended.
 * 
 * cairo_ft_font_lock_face() gets the #FT_Face object from a FreeType
 * backend font and scales it appropriately for the font. You must
 * release the face with cairo_ft_font_unlock_face()
 * when you are done using it.  Since the #FT_Face object can be
 * shared between multiple #cairo_font_t objects, you must not
 * lock any other font objects until you unlock this one. A count is
 * kept of the number of times cairo_ft_font_lock_face() is
 * called. cairo_ft_font_unlock_face() must be called the same number
 * of times.
 *
 * You must be careful when using this function in a library or in a
 * threaded application, because other threads may lock faces that
 * share the same #FT_Face object. For this reason, you must call
 * cairo_ft_lock() before locking any face objects, and
 * cairo_ft_unlock() after you are done. (These functions are not yet
 * implemented, so this function cannot be currently safely used in a
 * threaded application.)
 
 * Return value: The #FT_Face object for @font, scaled appropriately.
 **/
FT_Face
cairo_ft_font_lock_face (cairo_font_t *abstract_font)
{
    cairo_ft_font_t *font = (cairo_ft_font_t *) abstract_font;
    FT_Face face;

    face = _ft_unscaled_font_lock_face (font->unscaled);
    if (!face)
	return NULL;
    
    _ft_unscaled_font_set_scale (font->unscaled, &font->base.scale);

    return face;
}

/**
 * cairo_ft_font_unlock_face:
 * @ft_font: A #cairo_font_t from the FreeType font backend. Such an
 *   object can be created with cairo_ft_font_create() or
 *   cairo_ft_font_create_for_ft_face(). On some platforms the font from
 *   cairo_current_font() will also be a FreeType font, but using this
 *   functionality with fonts you don't create yourself is not
 *   recommended.
 * 
 * Releases a face obtained with cairo_ft_font_lock_face(). See the
 * documentation for that function for full details.
 **/
void
cairo_ft_font_unlock_face (cairo_font_t *abstract_font)
{
    cairo_ft_font_t *font = (cairo_ft_font_t *) abstract_font;

    _ft_unscaled_font_unlock_face (font->unscaled);
}

/**
 * cairo_ft_font_get_pattern:
 * @ft_font: A #cairo_font_t from the FreeType font backend. Such an
 *   object can be created with cairo_ft_font_create() or
 *   cairo_ft_font_create_for_ft_face(). On some platforms the font from
 *   cairo_current_font() will also be a FreeType font, but using this
 *   functionality with fonts you don't create yourself is not
 *   recommended.
 * 
 * cairo_ft_font_get_pattern() gets the #FcPattern for a FreeType
 * backend font. 

 * Return value: The #FcPattenr for @font. The return value is owned
 *   by the font, so you must not modify it, and must call
 *   FcPatternReference() to keep a persistant reference to the
 *   pattern. If the font was created with cairo_ft_font_create_for_ft_face()
 *   returns %NULL.
 **/
FcPattern *
cairo_ft_font_get_pattern (cairo_font_t *abstract_font)
{
    cairo_ft_font_t *font = (cairo_ft_font_t *) abstract_font;

    if (font == NULL)
        return NULL;

    return font->pattern;
}

/* We expose our unscaled font implementation internally for the the
 * PDF backend, which needs to keep track of the the different
 * fonts-on-disk used by a document, so it can embed them.
 */
cairo_unscaled_font_t *
_cairo_ft_font_get_unscaled_font (cairo_font_t *abstract_font)
{
    cairo_ft_font_t *font = (cairo_ft_font_t *) abstract_font;

    return (cairo_unscaled_font_t *)font->unscaled;
}

/* This differs from _cairo_ft_scaled_font_lock_face in that it doesn't
 * set the scale on the face, but just returns it at the last scale.
 */
FT_Face
_cairo_ft_unscaled_font_lock_face (cairo_unscaled_font_t *unscaled_font)
{
    return _ft_unscaled_font_lock_face ((ft_unscaled_font_t *)unscaled_font);
}

void
_cairo_ft_unscaled_font_unlock_face (cairo_unscaled_font_t *unscaled_font)
{
    _ft_unscaled_font_unlock_face ((ft_unscaled_font_t *)unscaled_font);
}
