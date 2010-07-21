/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2010 Mozilla Foundation
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
 * The Initial Developer of the Original Code is the Mozilla Foundation
 *
 * Contributor(s):
 *	Bas Schouten <bschouten@mozilla.com>
 */
#define INITGUID

#include "cairo.h"
#include "cairo-d2d-private.h"
#include "cairo-dwrite-private.h"

extern "C" {
#include "cairo-win32.h"
#include "cairo-analysis-surface-private.h"
}

// Required for using placement new.
#include <new>

ID2D1Factory *D2DSurfFactory::mFactoryInstance = NULL;
ID3D10Device1 *D3D10Factory::mDeviceInstance = NULL;

#define CAIRO_INT_STATUS_SUCCESS (cairo_int_status_t)CAIRO_STATUS_SUCCESS

// Contains our cache usage - perhaps this should be made threadsafe.
static int cache_usage = 0;

/**
 * Create a similar surface which will blend effectively to
 * another surface. For D2D, this will create another texture.
 * Within the types we use blending is always easy.
 *
 * \param surface Surface this needs to be similar to
 * \param content Content type of the new surface
 * \param width Width of the new surface
 * \param height Height of the new surface
 * \return New surface
 */
static cairo_surface_t*
_cairo_d2d_create_similar(void			*surface,
			  cairo_content_t	 content,
			  int			 width,
			  int			 height);

/**
 * Release all the data held by a surface, the surface structure
 * itsself will be freed by cairo.
 *
 * \param surface Surface to clean up
 */
static cairo_status_t
_cairo_d2d_finish(void	    *surface);

/**
 * Get a read-only image surface that contains the pixel data
 * of a D2D surface.
 *
 * \param abstract_surface D2D surface to acquire the image from
 * \param image_out Pointer to where we should store the image surface pointer
 * \param image_extra Pointer where to store extra data we want to know about
 * at the point of release.
 * \return CAIRO_STATUS_SUCCESS for success
 */
static cairo_status_t
_cairo_d2d_acquire_source_image(void                    *abstract_surface,
				cairo_image_surface_t  **image_out,
				void                   **image_extra);

/**
 * Release a read-only image surface that was obtained using acquire_source_image
 *
 * \param abstract_surface D2D surface to acquire the image from
 * \param image_out Pointer to where we should store the image surface pointer
 * \param image_extra Pointer where to store extra data we want to know about
 * at the point of release.
 * \return CAIRO_STATUS_SUCCESS for success
 */
static void
_cairo_d2d_release_source_image(void                   *abstract_surface,
				cairo_image_surface_t  *image,
				void                   *image_extra);

/**
 * Get a read-write image surface that contains the pixel data
 * of a D2D surface.
 *
 * \param abstract_surface D2D surface to acquire the image from
 * \param image_out Pointer to where we should store the image surface pointer
 * \param image_extra Pointer where to store extra data we want to know about
 * at the point of release.
 * \return CAIRO_STATUS_SUCCESS for success
 */
static cairo_status_t
_cairo_d2d_acquire_dest_image(void                    *abstract_surface,
			      cairo_rectangle_int_t   *interest_rect,
			      cairo_image_surface_t  **image_out,
			      cairo_rectangle_int_t   *image_rect,
			      void                   **image_extra);

/**
 * Release a read-write image surface that was obtained using acquire_source_image
 *
 * \param abstract_surface D2D surface to acquire the image from
 * \param image_out Pointer to where we should store the image surface pointer
 * \param image_extra Pointer where to store extra data we want to know about
 * at the point of release.
 * \return CAIRO_STATUS_SUCCESS for success
 */
static void
_cairo_d2d_release_dest_image(void                    *abstract_surface,
			      cairo_rectangle_int_t   *interest_rect,
			      cairo_image_surface_t   *image,
			      cairo_rectangle_int_t   *image_rect,
			      void                    *image_extra);

/**
 * Flush this surface, only after this operation is the related hardware texture
 * guaranteed to contain all the results of the executed drawing operations.
 *
 * \param surface D2D surface to flush
 * \return CAIRO_STATUS_SUCCESS or CAIRO_SURFACE_TYPE_MISMATCH
 */
static cairo_status_t
_cairo_d2d_flush(void                  *surface);

/**
 * Fill a path on this D2D surface.
 *
 * \param surface The surface to apply this operation to, must be
 * a D2D surface
 * \param op The operator to use
 * \param source The source pattern to fill this path with
 * \param path The path to fill
 * \param fill_rule The fill rule to uses on the path
 * \param tolerance The tolerance applied to the filling
 * \param antialias The anti-alias mode to use
 * \param clip The clip of this operation
 * \return Return code, this can be CAIRO_ERROR_SURFACE_TYPE_MISMATCH,
 * CAIRO_INT_STATUS_UNSUPPORTED or CAIRO_STATUS_SUCCESS
 */
static cairo_int_status_t
_cairo_d2d_fill(void			*surface,
		cairo_operator_t	 op,
		const cairo_pattern_t	*source,
		cairo_path_fixed_t	*path,
		cairo_fill_rule_t	 fill_rule,
		double			 tolerance,
		cairo_antialias_t	 antialias,
		cairo_clip_t		*clip);

/**
 * Paint this surface, applying the operation to the entire surface
 *
 * \param surface The surface to apply this operation to, must be
 * a D2D surface
 * \param op Operator to use when painting
 * \param source The pattern to fill this surface with, source of the op
 * \param clip The clip of this operation
 * \return Return code, this can be CAIRO_ERROR_SURFACE_TYPE_MISMATCH,
 * CAIRO_INT_STATUS_UNSUPPORTED or CAIRO_STATUS_SUCCESS
 */
static cairo_int_status_t
_cairo_d2d_paint(void			*surface,
		 cairo_operator_t	 op,
		 const cairo_pattern_t	*source,
		 cairo_clip_t		*clip);

/**
 * Paint something on the surface applying a certain mask to that
 * source.
 *
 * \param surface The surface to apply this oepration to, must be
 * a D2D surface
 * \param op Operator to use
 * \param source Source for this operation
 * \param mask Pattern to mask source with
 * \param clip The clip of this operation
 * \return Return code, this can be CAIRO_ERROR_SURFACE_TYPE_MISMATCH,
 * CAIRO_INT_STATUS_UNSUPPORTED or CAIRO_STATUS_SUCCESS
 */
static cairo_int_status_t
_cairo_d2d_mask(void			*surface,
		cairo_operator_t	 op,
		const cairo_pattern_t	*source,
		const cairo_pattern_t	*mask,
		cairo_clip_t		*clip);

/**
 * Show a glyph run on the target D2D surface.
 *
 * \param surface The surface to apply this oepration to, must be
 * a D2D surface
 * \param op Operator to use
 * \param source Source for this operation
 * \param glyphs Glyphs to draw
 * \param num_gluphs Amount of glyphs stored at glyphs
 * \param scaled_font Scaled font to draw
 * \param remaining_glyphs Pointer to store amount of glyphs still
 * requiring drawing.
 * \param clip The clip of this operation
 * \return CAIRO_ERROR_SURFACE_TYPE_MISMATCH, CAIRO_ERROR_FONT_TYPE_MISMATCH,
 * CAIRO_INT_STATUS_UNSUPPORTED or CAIRO_STATUS_SUCCESS
 */
static cairo_int_status_t
_cairo_d2d_show_glyphs (void			*surface,
			cairo_operator_t	 op,
			const cairo_pattern_t	*source,
			cairo_glyph_t		*glyphs,
			int			 num_glyphs,
			cairo_scaled_font_t	*scaled_font,
			cairo_clip_t		*clip,
			int			*remaining_glyphs);

/**
 * Get the extents of this surface.
 *
 * \param surface D2D surface to get the extents for
 * \param extents Pointer to where to store the extents
 * \param CAIRO_ERROR_SURFACE_TYPE_MISTMATCH or CAIRO_STATUS_SUCCESS
 */
static cairo_bool_t
_cairo_d2d_getextents(void		       *surface,
		      cairo_rectangle_int_t    *extents);


/**
 * Stroke a path on this D2D surface.
 *
 * \param surface The surface to apply this operation to, must be
 * a D2D surface
 * \param op The operator to use
 * \param source The source pattern to fill this path with
 * \param path The path to stroke
 * \param style The style of the stroke
 * \param ctm A logical to device matrix, since the path might be in
 * device space the miter angle and such are not, hence we need to
 * be aware of the transformation to apply correct stroking.
 * \param ctm_inverse Inverse of ctm, used to transform the path back
 * to logical space.
 * \param tolerance Tolerance to stroke with
 * \param antialias Antialias mode to use
 * \param clip The clip of this operation
 * \return Return code, this can be CAIRO_ERROR_SURFACE_TYPE_MISMATCH,
 * CAIRO_INT_STATUS_UNSUPPORTED or CAIRO_STATUS_SUCCESS
 */
static cairo_int_status_t
_cairo_d2d_stroke(void			*surface,
		  cairo_operator_t	 op,
		  const cairo_pattern_t	*source,
		  cairo_path_fixed_t	*path,
		  cairo_stroke_style_t	*style,
		  cairo_matrix_t	*ctm,
		  cairo_matrix_t	*ctm_inverse,
		  double		 tolerance,
		  cairo_antialias_t	 antialias,
		  cairo_clip_t		*clip);

static const cairo_surface_backend_t cairo_d2d_surface_backend = {
    CAIRO_SURFACE_TYPE_D2D,
    _cairo_d2d_create_similar, /* create_similar */
    _cairo_d2d_finish, /* finish */
    _cairo_d2d_acquire_source_image, /* acquire_source_image */
    _cairo_d2d_release_source_image, /* release_source_image */
    _cairo_d2d_acquire_dest_image, /* acquire_dest_image */
    _cairo_d2d_release_dest_image, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* create_span_renderer */
    NULL, /* check_span_renderer */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_d2d_getextents, /* get_extents */
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    _cairo_d2d_flush, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */
    _cairo_d2d_paint, /* paint */
    _cairo_d2d_mask, /* mask */
    _cairo_d2d_stroke, /* stroke */
    _cairo_d2d_fill, /* fill */
    _cairo_d2d_show_glyphs, /* show_glyphs */
    NULL, /* snapshot */
    NULL
};

/*
 * Helper functions.
 */

/* This clears a new D2D surface in case the VRAM was reused from an existing surface
 * and is therefor not empty, this must be called outside of drawing state! */
static void
_d2d_clear_surface(cairo_d2d_surface_t *surf)
{
    surf->rt->BeginDraw();
    surf->rt->Clear(D2D1::ColorF(0, 0));
    surf->rt->EndDraw();
}

static D2D1_POINT_2F
_d2d_point_from_cairo_point(const cairo_point_t *point)
{
    return D2D1::Point2F(_cairo_fixed_to_float(point->x),
			 _cairo_fixed_to_float(point->y));
}

static D2D1_COLOR_F
_cairo_d2d_color_from_cairo_color(const cairo_color_t &color)
{
    return D2D1::ColorF((FLOAT)color.red, 
			(FLOAT)color.green, 
			(FLOAT)color.blue,
			(FLOAT)color.alpha);
}

/**
 * Gets the surface buffer texture for window surfaces whose backbuffer
 * is not directly usable as a bitmap.
 *
 * \param surface D2D surface.
 * \return Buffer texture
 */
static ID3D10Texture2D*
_cairo_d2d_get_buffer_texture(cairo_d2d_surface_t *surface) 
{
    if (!surface->bufferTexture) {
	IDXGISurface *surf;
	DXGI_SURFACE_DESC surfDesc;
	surface->surface->QueryInterface(&surf);
	surf->GetDesc(&surfDesc);
	CD3D10_TEXTURE2D_DESC softDesc(surfDesc.Format, surfDesc.Width, surfDesc.Height);
        softDesc.MipLevels = 1;
	softDesc.Usage = D3D10_USAGE_DEFAULT;
	softDesc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
	D3D10Factory::Device()->CreateTexture2D(&softDesc, NULL, &surface->bufferTexture);
    }
    return surface->bufferTexture;
}

/**
 * Ensure that the surface has an up-to-date surface bitmap. Used for
 * window surfaces which cannot have a surface bitmap directly related
 * to their backbuffer for some reason.
 * You cannot create a bitmap around a backbuffer surface for reason (it will 
 * fail with an E_INVALIDARG). Meaning they need a special texture to store 
 * their graphical data which is wrapped by a D2D bitmap if a window surface 
 * is ever used in a surface pattern. All other D2D surfaces use a texture as 
 * their backing store so can have a bitmap directly.
 *
 * \param surface D2D surface.
 */
static void _cairo_d2d_update_surface_bitmap(cairo_d2d_surface_t *d2dsurf)
{
    if (!d2dsurf->backBuf && d2dsurf->rt->GetPixelFormat().format != DXGI_FORMAT_A8_UNORM) {
	return;
    }
    
    if (!d2dsurf->surfaceBitmap) {
	d2dsurf->rt->CreateBitmap(d2dsurf->rt->GetPixelSize(),
				  D2D1::BitmapProperties(d2dsurf->rt->GetPixelFormat()),
				  &d2dsurf->surfaceBitmap);
    }

    d2dsurf->surfaceBitmap->CopyFromRenderTarget(NULL, d2dsurf->rt, NULL);
}

/**
 * Present the backbuffer for a surface create for an HWND. This needs
 * to be called when the owner of the original window surface wants to
 * actually present the executed drawing operations to the screen.
 *
 * \param surface D2D surface.
 */
void cairo_d2d_present_backbuffer(cairo_surface_t *surface)
{
    if (surface->type != CAIRO_SURFACE_TYPE_D2D) {
	return;
    }
    cairo_d2d_surface_t *d2dsurf = reinterpret_cast<cairo_d2d_surface_t*>(surface);
    _cairo_d2d_flush(d2dsurf);
    if (d2dsurf->dxgiChain) {
	d2dsurf->dxgiChain->Present(0, 0);
	D3D10Factory::Device()->Flush();
    }
}

struct d2d_clip
{
    enum clip_type {LAYER, AXIS_ALIGNED_CLIP};
    d2d_clip * const prev;
    const enum clip_type type;
    d2d_clip(d2d_clip *prev, clip_type type) : prev(prev), type(type) { }
};

static RefPtr<ID2D1PathGeometry>
_cairo_d2d_create_path_geometry_for_path(cairo_path_fixed_t *path,
					 cairo_fill_rule_t fill_rule,
					 D2D1_FIGURE_BEGIN type);


static cairo_bool_t
box_is_integer (cairo_box_t *box)
{
    return _cairo_fixed_is_integer(box->p1.x) &&
	_cairo_fixed_is_integer(box->p1.y) &&
	_cairo_fixed_is_integer(box->p2.x) &&
	_cairo_fixed_is_integer(box->p2.y);
}

static cairo_status_t
push_clip (cairo_d2d_surface_t *d2dsurf, cairo_clip_path_t *clip_path)
{
    cairo_box_t box;
    if (_cairo_path_fixed_is_box(&clip_path->path, &box)) {

	assert(box.p1.y < box.p2.y);

	D2D1_ANTIALIAS_MODE mode;
	if (box_is_integer (&box)) {
	    mode = D2D1_ANTIALIAS_MODE_ALIASED;
	} else {
	    mode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
	}
	d2dsurf->rt->PushAxisAlignedClip (
		D2D1::RectF(
		    _cairo_fixed_to_float(box.p1.x),
		    _cairo_fixed_to_float(box.p1.y),
		    _cairo_fixed_to_float(box.p2.x),
		    _cairo_fixed_to_float(box.p2.y)),
		mode);

	d2dsurf->d2d_clip = new d2d_clip (d2dsurf->d2d_clip, d2d_clip::AXIS_ALIGNED_CLIP);
    } else {
	HRESULT hr;
	RefPtr<ID2D1PathGeometry> geom = _cairo_d2d_create_path_geometry_for_path (&clip_path->path,
							clip_path->fill_rule,
							D2D1_FIGURE_BEGIN_FILLED);
	RefPtr<ID2D1Layer> layer;

	hr = d2dsurf->rt->CreateLayer (&layer);

	D2D1_LAYER_OPTIONS options = D2D1_LAYER_OPTIONS_NONE;
	if (d2dsurf->base.content == CAIRO_CONTENT_COLOR) {
	    options = D2D1_LAYER_OPTIONS_INITIALIZE_FOR_CLEARTYPE;
	}

	d2dsurf->rt->PushLayer(D2D1::LayerParameters(
		    D2D1::InfiniteRect(),
		    geom,
		    D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
		    D2D1::IdentityMatrix(),
		    1.0,
		    0,
		    options),
		layer);

	d2dsurf->d2d_clip = new d2d_clip(d2dsurf->d2d_clip, d2d_clip::LAYER);
   }
    if (!d2dsurf->d2d_clip)
	return _cairo_error(CAIRO_STATUS_NO_MEMORY);
    return CAIRO_STATUS_SUCCESS;
}

static void
pop_clip (cairo_d2d_surface_t *d2dsurf)
{
    d2d_clip *current_clip = d2dsurf->d2d_clip;

    /* pop the clip from the render target */
    if (current_clip->type == d2d_clip::LAYER) {
	d2dsurf->rt->PopLayer();
    } else if (current_clip->type == d2d_clip::AXIS_ALIGNED_CLIP) {
	d2dsurf->rt->PopAxisAlignedClip();
    }

    /* pop it from our own stack */
    d2dsurf->d2d_clip = current_clip->prev;
    delete current_clip;
}

/* intersect clip_paths until we reach head */
static cairo_status_t
clipper_intersect_clip_path_recursive (cairo_d2d_surface_t *d2dsurf,
	cairo_clip_path_t *head,
	cairo_clip_path_t *clip_path)
{
    cairo_status_t status;

    if (clip_path->prev != head) {
	status =
	    clipper_intersect_clip_path_recursive (d2dsurf,
		    head,
		    clip_path->prev);
	if (unlikely (status))
	    return status;
    }
    return push_clip(d2dsurf, clip_path);

}

/* pop all of the clipping layers and reset the clip */
static void
reset_clip (cairo_d2d_surface_t *d2dsurf)
{
    cairo_clip_path_t *current_clip_path = d2dsurf->clip.path;
    while (current_clip_path != NULL) {
	pop_clip (d2dsurf);
	current_clip_path = current_clip_path->prev;
    }

    _cairo_clip_reset (&d2dsurf->clip);
}

/* finds the lowest common ancestor of a and b */
static cairo_clip_path_t *
find_common_ancestor(cairo_clip_path_t *a, cairo_clip_path_t *b)
{
    int a_depth = 0, b_depth = 0;

    cairo_clip_path_t *x;

    /* find the depths of the clip_paths */
    x = a;
    while (x) {
	a_depth++;
	x = x->prev;
    }

    x = b;
    while (x) {
	b_depth++;
	x = x->prev;
    }

    /* rewind the deeper chain to the depth of the shallowest chain */
    while (b_depth < a_depth && a) {
	a = a->prev;
	a_depth--;
    }

    while (a_depth < b_depth && b) {
	b = b->prev;
	b_depth--;
    }

    /* walk back until we find a common ancesstor */

    /* b will be null if and only if a is null because the depths
     * must match at this point */
    while (a) {
	if (a == b)
	    return a;

	a = a->prev;
	b = b->prev;
    }

    /* a will be NULL */
    return a;
}

cairo_status_t
_cairo_d2d_set_clip (cairo_d2d_surface_t *d2dsurf, cairo_clip_t *clip)
{
    if (clip == NULL) {
	reset_clip (d2dsurf);
	return CAIRO_STATUS_SUCCESS;
    }

    if (clip != NULL && clip->path == d2dsurf->clip.path)
	return CAIRO_STATUS_SUCCESS;

    cairo_clip_path_t *current_clip_path = d2dsurf->clip.path;
    cairo_clip_path_t *new_clip_path = clip->path;
    cairo_clip_path_t *ancestor = find_common_ancestor (current_clip_path, new_clip_path);

    /* adjust the clip to the common ancestor */
    while (current_clip_path != ancestor) {
	pop_clip (d2dsurf);
	current_clip_path = current_clip_path->prev;
    }

    /* we now have a common parent (current_clip_path) for the clip */

    /* replace the old clip */
    _cairo_clip_reset (&d2dsurf->clip);
    _cairo_clip_init_copy (&d2dsurf->clip, clip);

    /* push the new clip paths up to current_clip_path */
    if (current_clip_path != clip->path)
	return clipper_intersect_clip_path_recursive (d2dsurf, current_clip_path, clip->path);
    else
	return CAIRO_STATUS_SUCCESS;
}

/**
 * Enter the state where the surface is ready for drawing. This will guarantee
 * the surface is in the correct state, and the correct clipping area is pushed.
 *
 * \param surface D2D surface
 */
static void _begin_draw_state(cairo_d2d_surface_t* surface)
{
    if (!surface->isDrawing) {
	surface->rt->BeginDraw();
	surface->isDrawing = true;
    }
}

/* External helper called from dwrite code.
 * Will hopefully go away when/if that code
 * moves into here */
void
_cairo_d2d_begin_draw_state(cairo_d2d_surface_t *d2dsurf)
{
	_begin_draw_state(d2dsurf);
}

/**
 * Get a D2D matrix from a cairo matrix. Note that D2D uses row vectors where cairo
 * uses column vectors. Hence the transposition.
 *
 * \param Cairo matrix
 * \return D2D matrix
 */
static D2D1::Matrix3x2F
_cairo_d2d_matrix_from_matrix(const cairo_matrix_t *matrix)
{
    return D2D1::Matrix3x2F((FLOAT)matrix->xx,
			    (FLOAT)matrix->yx,
			    (FLOAT)matrix->xy,
			    (FLOAT)matrix->yy,
			    (FLOAT)matrix->x0,
			    (FLOAT)matrix->y0);
}

/**
 * Create a D2D stroke style interface for a cairo stroke style object. Must be
 * released when the calling function is finished with it.
 *
 * \param style Cairo stroke style object
 * \return D2D StrokeStyle interface
 */
static RefPtr<ID2D1StrokeStyle>
_cairo_d2d_create_strokestyle_for_stroke_style(const cairo_stroke_style_t *style)
{
    D2D1_CAP_STYLE line_cap = D2D1_CAP_STYLE_FLAT;
    switch (style->line_cap) {
	case CAIRO_LINE_CAP_BUTT:
	    line_cap = D2D1_CAP_STYLE_FLAT;
	    break;
	case CAIRO_LINE_CAP_SQUARE:
	    line_cap = D2D1_CAP_STYLE_SQUARE;
	    break;
	case CAIRO_LINE_CAP_ROUND:
	    line_cap = D2D1_CAP_STYLE_ROUND;
	    break;
    }

    D2D1_LINE_JOIN line_join = D2D1_LINE_JOIN_MITER;
    switch (style->line_join) {
	case CAIRO_LINE_JOIN_MITER:
	    line_join = D2D1_LINE_JOIN_MITER;
	    break;
	case CAIRO_LINE_JOIN_ROUND:
	    line_join = D2D1_LINE_JOIN_ROUND;
	    break;
	case CAIRO_LINE_JOIN_BEVEL:
	    line_join = D2D1_LINE_JOIN_BEVEL;
	    break;
    }

    FLOAT *dashes = NULL;
    if (style->num_dashes) {
	dashes = new FLOAT[style->num_dashes];
	for (unsigned int i = 0; i < style->num_dashes; i++) {
	    dashes[i] = (FLOAT)style->dash[i];
	}
    }

    D2D1_DASH_STYLE dashStyle = D2D1_DASH_STYLE_SOLID;
    if (dashes) {
	dashStyle = D2D1_DASH_STYLE_CUSTOM;
    }

    RefPtr<ID2D1StrokeStyle> strokeStyle;
    D2DSurfFactory::Instance()->CreateStrokeStyle(D2D1::StrokeStyleProperties(line_cap, 
									      line_cap,
									      line_cap, 
									      line_join, 
									      (FLOAT)style->miter_limit,
									      dashStyle,
									      (FLOAT)style->dash_offset),
						  dashes,
						  style->num_dashes,
						  &strokeStyle);
    delete [] dashes;
    return strokeStyle;
}

static int _d2d_compute_bitmap_mem_size(ID2D1Bitmap *bitmap)
{
    D2D1_SIZE_U size = bitmap->GetPixelSize();
    int bytes_per_pixel = bitmap->GetPixelFormat().format == DXGI_FORMAT_A8_UNORM ? 1 : 4;
    return size.width * size.height * bytes_per_pixel;
}

cairo_user_data_key_t bitmap_key_nonextend;
cairo_user_data_key_t bitmap_key_extend;
cairo_user_data_key_t bitmap_key_snapshot;

struct cached_bitmap {
    /** The cached bitmap */
    RefPtr<ID2D1Bitmap> bitmap;
    /** The cached bitmap is dirty and needs its data refreshed */
    bool dirty;
    /** Order of snapshot detach/release bitmap called not guaranteed, single threaded refcount for now */
    int refs;
};

/** 
 * This is called when user data on a surface is replaced or the surface is
 * destroyed.
 */
static void _d2d_release_bitmap(void *bitmap)
{
    cached_bitmap *existingBitmap = (cached_bitmap*)bitmap;
    if (!--existingBitmap->refs) {
	cache_usage -= _d2d_compute_bitmap_mem_size(existingBitmap->bitmap);
	delete existingBitmap;
    }
}

/**
 * Via a little trick this is just used to determine when a surface has been
 * modified.
 */
static void _d2d_snapshot_detached(cairo_surface_t *surface)
{
    cached_bitmap *existingBitmap = (cached_bitmap*)cairo_surface_get_user_data(surface, &bitmap_key_snapshot);
    if (existingBitmap) {
	existingBitmap->dirty = true;
    }
    if (!--existingBitmap->refs) {
	cache_usage -= _d2d_compute_bitmap_mem_size(existingBitmap->bitmap);
        delete existingBitmap;
    }
    cairo_surface_destroy(surface);
}

/**
 * This function will calculate the part of srcSurf which will possibly be used within
 * the boundaries of d2dsurf given the current transformation mat. This is used to
 * determine what the minimal part of a surface is that needs to be uploaded.
 *
 * \param d2dsurf D2D surface
 * \param srcSurf Source surface for operation
 * \param mat Transformation matrix applied to source
 */
static void
_cairo_d2d_calculate_visible_rect(cairo_d2d_surface_t *d2dsurf, cairo_image_surface_t *srcSurf,
				  cairo_matrix_t *mat,
				  int *x, int *y, unsigned int *width, unsigned int *height)
{
    /** Leave room for extend_none space, 2 pixels */
    UINT32 maxSize = d2dsurf->rt->GetMaximumBitmapSize() - 2;

    /* Transform this surface to image surface space */
    cairo_matrix_t invMat = *mat;
    if (_cairo_matrix_is_invertible(mat)) {
	/* If this is not invertible it will be rank zero, and invMat = mat is fine */
	cairo_matrix_invert(&invMat);
    }

    RefPtr<IDXGISurface> surf;
    d2dsurf->surface->QueryInterface(&surf);
    DXGI_SURFACE_DESC desc;
    surf->GetDesc(&desc);

    double leftMost = 0;
    double rightMost = desc.Width;
    double topMost = 0;
    double bottomMost = desc.Height;

    _cairo_matrix_transform_bounding_box(&invMat, &leftMost, &topMost, &rightMost, &bottomMost, NULL);

    leftMost -= 1;
    topMost -= 1;
    rightMost += 1;
    bottomMost += 1;

    /* Calculate the offsets into the source image and the width of the part required */
    if ((UINT32)srcSurf->width > maxSize) {
	*x = (int)MAX(0, floor(leftMost));
	/* Ensure that we get atleast 1 column of pixels as source, this will make EXTEND_PAD work */
	if (*x < srcSurf->width) {
	    *width = (unsigned int)MIN(MAX(1, ceil(rightMost - *x)), srcSurf->width - *x);
	} else {
	    *x = srcSurf->width - 1;
	    *width = 1;
	}
    } else {
	*x = 0;
	*width = srcSurf->width;
    }

    if ((UINT32)srcSurf->height > maxSize) {
	*y = (int)MAX(0, floor(topMost));
	/* Ensure that we get atleast 1 row of pixels as source, this will make EXTEND_PAD work */
	if (*y < srcSurf->height) {
	    *height = (unsigned int)MIN(MAX(1, ceil(bottomMost - *y)), srcSurf->height - *y);
	} else {
	    *y = srcSurf->height - 1;
	    *height = 1;
	}
    } else {
	*y = 0;
	*height = srcSurf->height;
    }
}

/**
 * This creates an ID2D1Brush that will fill with the correct pattern.
 * This function passes a -strong- reference to the caller, the brush
 * needs to be released, even if it is not unique.
 *
 * \param d2dsurf Surface to create a brush for
 * \param pattern The pattern to create a brush for
 * \param unique We cache the bitmap/color brush for speed. If this
 * needs a brush that is unique (i.e. when more than one is needed),
 * this will make the function return a seperate brush.
 * \return A brush object
 */
RefPtr<ID2D1Brush>
_cairo_d2d_create_brush_for_pattern(cairo_d2d_surface_t *d2dsurf, 
				    const cairo_pattern_t *pattern,
				    bool unique)
{
    if (pattern->type == CAIRO_PATTERN_TYPE_SOLID) {
	cairo_solid_pattern_t *sourcePattern =
	    (cairo_solid_pattern_t*)pattern;
	D2D1_COLOR_F color = _cairo_d2d_color_from_cairo_color(sourcePattern->color);
	if (unique) {
	    RefPtr<ID2D1SolidColorBrush> brush;
	    d2dsurf->rt->CreateSolidColorBrush(color,
					       &brush);
	    return brush;
	} else {
	    if (d2dsurf->solidColorBrush->GetColor().a != color.a ||
		d2dsurf->solidColorBrush->GetColor().r != color.r ||
		d2dsurf->solidColorBrush->GetColor().g != color.g ||
		d2dsurf->solidColorBrush->GetColor().b != color.b) {
		d2dsurf->solidColorBrush->SetColor(color);
	    }
	    return d2dsurf->solidColorBrush;
	}

    } else if (pattern->type == CAIRO_PATTERN_TYPE_LINEAR) {
	cairo_matrix_t mat = pattern->matrix;
	/**
	 * Cairo views this matrix as the transformation of the destination
	 * when the pattern is imposed. We see this differently, D2D transformation
	 * moves the pattern over the destination.
	 */
	cairo_matrix_invert(&mat);

	D2D1_BRUSH_PROPERTIES brushProps =
	    D2D1::BrushProperties(1.0, _cairo_d2d_matrix_from_matrix(&mat));
	cairo_linear_pattern_t *sourcePattern =
	    (cairo_linear_pattern_t*)pattern;

	D2D1_GRADIENT_STOP *stops = 
	    new D2D1_GRADIENT_STOP[sourcePattern->base.n_stops];
	for (unsigned int i = 0; i < sourcePattern->base.n_stops; i++) {
	    stops[i].position = (FLOAT)sourcePattern->base.stops[i].offset;
	    stops[i].color = 
		_cairo_d2d_color_from_cairo_color(sourcePattern->base.stops[i].color);
	}
	RefPtr<ID2D1GradientStopCollection> stopCollection;
	d2dsurf->rt->CreateGradientStopCollection(stops, sourcePattern->base.n_stops, &stopCollection);
	RefPtr<ID2D1LinearGradientBrush> brush;
	d2dsurf->rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(_d2d_point_from_cairo_point(&sourcePattern->p1),
										   _d2d_point_from_cairo_point(&sourcePattern->p2)),
					       brushProps,
					       stopCollection,
					       &brush);
	delete [] stops;
	return brush;

    } else if (pattern->type == CAIRO_PATTERN_TYPE_RADIAL) {
	cairo_matrix_t mat = pattern->matrix;
	cairo_matrix_invert(&mat);

	D2D1_BRUSH_PROPERTIES brushProps =
	    D2D1::BrushProperties(1.0, _cairo_d2d_matrix_from_matrix(&mat));
	cairo_radial_pattern_t *sourcePattern =
	    (cairo_radial_pattern_t*)pattern;
	
	if ((sourcePattern->c1.x != sourcePattern->c2.x ||
	    sourcePattern->c1.y != sourcePattern->c2.y) &&
	    sourcePattern->r1 != 0) {
		/**
		 * In this particular case there's no way to deal with this!
		 * \todo Create an image surface with the gradient and use that.
		 */
		return NULL;
	}
	D2D_POINT_2F center =
	    _d2d_point_from_cairo_point(&sourcePattern->c2);
	D2D_POINT_2F origin =
	    _d2d_point_from_cairo_point(&sourcePattern->c1);
	origin.x -= center.x;
	origin.y -= center.y;

	D2D1_GRADIENT_STOP *stops = 
	    new D2D1_GRADIENT_STOP[sourcePattern->base.n_stops];
	for (unsigned int i = 0; i < sourcePattern->base.n_stops; i++) {
	    stops[i].position = (FLOAT)sourcePattern->base.stops[i].offset;
	    stops[i].color = 
		_cairo_d2d_color_from_cairo_color(sourcePattern->base.stops[i].color);
	}
	RefPtr<ID2D1GradientStopCollection> stopCollection;
	d2dsurf->rt->CreateGradientStopCollection(stops, sourcePattern->base.n_stops, &stopCollection);
	RefPtr<ID2D1RadialGradientBrush> brush;

	d2dsurf->rt->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(center,
										   origin,
										   _cairo_fixed_to_float(sourcePattern->r2),
										   _cairo_fixed_to_float(sourcePattern->r2)),
					       brushProps,
					       stopCollection,
					       &brush);
	delete [] stops;
	return brush;

    } else if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_matrix_t mat = pattern->matrix;
	cairo_matrix_invert(&mat);

	cairo_surface_pattern_t *surfacePattern =
	    (cairo_surface_pattern_t*)pattern;
	D2D1_EXTEND_MODE extendMode;

	cairo_user_data_key_t *key = &bitmap_key_extend;

	if (pattern->extend == CAIRO_EXTEND_NONE) {
	    extendMode = D2D1_EXTEND_MODE_CLAMP;
	    key = &bitmap_key_nonextend;
	    /** 
	     * For image surfaces we create a slightly larger bitmap with
	     * a transparent border around it for this case. Need to translate
	     * for that.
	     */
	    if (surfacePattern->surface->type == CAIRO_SURFACE_TYPE_IMAGE) {
		cairo_matrix_translate(&mat, -1.0, -1.0);
	    }
	} else if (pattern->extend == CAIRO_EXTEND_REPEAT) {
	    extendMode = D2D1_EXTEND_MODE_WRAP;
	} else if (pattern->extend == CAIRO_EXTEND_REFLECT) {
	    extendMode = D2D1_EXTEND_MODE_MIRROR;
	} else {
	    extendMode = D2D1_EXTEND_MODE_CLAMP;
	}

	RefPtr<ID2D1Bitmap> sourceBitmap;
	bool partial = false;
	int xoffset = 0;
	int yoffset = 0;
	unsigned int width;
	unsigned int height;
	unsigned char *data = NULL;
 	unsigned int stride = 0;

	if (surfacePattern->surface->type == CAIRO_SURFACE_TYPE_D2D) {
	    /**
	     * \todo We need to somehow get a rectangular transparent
	     * border here too!!
	     */
	    cairo_d2d_surface_t *srcSurf = 
		reinterpret_cast<cairo_d2d_surface_t*>(surfacePattern->surface);
	    
	    if (srcSurf == d2dsurf) {
		/* D2D cannot deal with self-copy. We should add an optimized
		 * codepath for self-copy in the easy cases that does ping-pong like
		 * scroll does. See bug 579215. For now fallback.
		 */
		return NULL;
	    }

	    _cairo_d2d_update_surface_bitmap(srcSurf);
	    sourceBitmap = srcSurf->surfaceBitmap;

	    _cairo_d2d_flush(srcSurf);
	} else if (surfacePattern->surface->type == CAIRO_SURFACE_TYPE_IMAGE) {
	    cairo_image_surface_t *srcSurf = 
		reinterpret_cast<cairo_image_surface_t*>(surfacePattern->surface);
	    D2D1_ALPHA_MODE alpha;
	    if (srcSurf->format == CAIRO_FORMAT_ARGB32 ||
		srcSurf->format == CAIRO_FORMAT_A8) {
		alpha = D2D1_ALPHA_MODE_PREMULTIPLIED;
	    } else {
		alpha = D2D1_ALPHA_MODE_IGNORE;
	    }

	    data = srcSurf->data;
	    stride = srcSurf->stride;

	    /* This is used as a temporary surface for resampling surfaces larget than maxSize. */
	    pixman_image_t *pix_image = NULL;

	    DXGI_FORMAT format;
	    unsigned int Bpp;
	    if (srcSurf->format == CAIRO_FORMAT_ARGB32) {
		format = DXGI_FORMAT_B8G8R8A8_UNORM;
		Bpp = 4;
	    } else if (srcSurf->format == CAIRO_FORMAT_RGB24) {
		format = DXGI_FORMAT_B8G8R8A8_UNORM;
		Bpp = 4;
	    } else if (srcSurf->format == CAIRO_FORMAT_A8) {
		format = DXGI_FORMAT_A8_UNORM;
		Bpp = 1;
	    } else {
		return NULL;
	    }

	    /** Leave room for extend_none space, 2 pixels */
	    UINT32 maxSize = d2dsurf->rt->GetMaximumBitmapSize() - 2;

	    if ((UINT32)srcSurf->width > maxSize || (UINT32)srcSurf->height > maxSize) {
		if (pattern->extend == CAIRO_EXTEND_REPEAT ||
		    pattern->extend == CAIRO_EXTEND_REFLECT) {
		    // XXX - we don't have code to deal with these yet.
		    return NULL;
		}

		/* We cannot fit this image directly into a texture, start doing tricks to draw correctly anyway. */
		partial = true;

		/* First we check which part of the image is inside the viewable area. */
  		_cairo_d2d_calculate_visible_rect(d2dsurf, srcSurf, &mat, &xoffset, &yoffset, &width, &height);

	        cairo_matrix_translate(&mat, xoffset, yoffset);

		if (width > maxSize || height > maxSize) {
		    /*
		     * We cannot upload the required part of the surface directly, we're going to create
		     * a version which is downsampled to a smaller size by pixman and then uploaded.
		     *
		     * We need to size it to at least the diagonal size of this surface, in order to prevent ever
		     * upsampling this again when drawing it to the surface. We want the resized surface
		     * to be as small as possible to limit pixman required fill rate.
                     *
                     * Note this isn't necessarily perfect. Imagine having a 5x5 pixel destination and
                     * a 10x5 image containing a line of blackpixels, white pixels, black pixels, if you rotate
                     * this by 45 degrees and scale it to a size of 5x5 pixels and composite it to the destination,
                     * the composition will require all 10 original columns to do the best possible sampling.
		     */
		    RefPtr<IDXGISurface> surf;
		    d2dsurf->surface->QueryInterface(&surf);
		    DXGI_SURFACE_DESC desc;
		    surf->GetDesc(&desc);

		    unsigned int minSize = (unsigned int)ceil(sqrt(pow((float)desc.Width, 2) + pow((float)desc.Height, 2)));
		    
		    unsigned int newWidth = MIN(minSize, MIN(width, maxSize));
		    unsigned int newHeight = MIN(minSize, MIN(height, maxSize));
		    double xRatio = (double)width / newWidth;
		    double yRatio = (double)height / newHeight;

		    if (newWidth > maxSize || newHeight > maxSize) {
			/*
			 * Okay, the diagonal of our surface is big enough to require a sampling larger
			 * than the maximum texture size. This is where we give up.
			 */
			return NULL;
  		    }

		    /* Create a temporary surface to hold the downsampled image */
		    pix_image = pixman_image_create_bits(srcSurf->pixman_format,
							 newWidth,
							 newHeight,
							 NULL,
							 -1);

		    /* Set the transformation to downsample and call pixman_image_composite to downsample */
		    pixman_transform_t transform;
		    pixman_transform_init_scale(&transform, pixman_double_to_fixed(xRatio), pixman_double_to_fixed(yRatio));
		    pixman_transform_translate(&transform, NULL, pixman_int_to_fixed(xoffset), pixman_int_to_fixed(yoffset));

		    pixman_image_set_transform(srcSurf->pixman_image, &transform);
		    pixman_image_composite(PIXMAN_OP_SRC, srcSurf->pixman_image, NULL, pix_image, 0, 0, 0, 0, 0, 0, newWidth, newHeight);

		    /* Adjust the pattern transform to the used temporary surface */
		    cairo_matrix_scale(&mat, xRatio, yRatio);

		    data = (unsigned char*)pixman_image_get_data(pix_image);
		    stride = pixman_image_get_stride(pix_image);

		    /* Into this image we actually have no offset */
		    xoffset = 0;
		    yoffset = 0;
		    width = newWidth;
		    height = newHeight;
  		}
	    } else {
		width = srcSurf->width;
		height = srcSurf->height;
	    }

	    cached_bitmap *cachebitmap = NULL;

	    if (!partial) {
		cachebitmap = 
		    (cached_bitmap*)cairo_surface_get_user_data(
		    surfacePattern->surface,
		    key);
	    }

	    if (cachebitmap) {
		sourceBitmap = cachebitmap->bitmap;
		if (cachebitmap->dirty) {
		    D2D1_RECT_U rect;
		    /* No need to take partial uploading into account - partially uploaded surfaces are never cached. */
		    if (pattern->extend == CAIRO_EXTEND_NONE) {
			rect = D2D1::RectU(1, 1, srcSurf->width + 1, srcSurf->height + 1);
		    } else {
			rect = D2D1::RectU(0, 0, srcSurf->width, srcSurf->height);
		    }
		    sourceBitmap->CopyFromMemory(&rect,
						 srcSurf->data,
						 srcSurf->stride);
		    cairo_surface_t *nullSurf =
			_cairo_null_surface_create(CAIRO_CONTENT_COLOR_ALPHA);
		    cachebitmap->refs++;
		    cachebitmap->dirty = false;
		    cairo_surface_set_user_data(nullSurf,
						&bitmap_key_snapshot,
						cachebitmap,
						NULL);
		    _cairo_surface_attach_snapshot(surfacePattern->surface,
						   nullSurf,
						   _d2d_snapshot_detached);
		}
	    } else {
		if (pattern->extend != CAIRO_EXTEND_NONE) {
		    d2dsurf->rt->CreateBitmap(D2D1::SizeU(width, height),
							  data + yoffset * stride + xoffset * Bpp,
							  stride,
							  D2D1::BitmapProperties(D2D1::PixelFormat(format,
												   alpha)),
					      &sourceBitmap);
		} else {
		    /**
		     * Trick here, we create a temporary rectangular
		     * surface with 1 pixel margin on each side. This
		     * provides a rectangular transparent border, that
		     * will ensure CLAMP acts as EXTEND_NONE. Perhaps
		     * this could be further optimized by not memsetting
		     * the whole array.
		     */
		    unsigned int tmpWidth = width + 2;
		    unsigned int tmpHeight = height + 2;
		    unsigned char *tmp = new unsigned char[tmpWidth * tmpHeight * Bpp];
		    memset(tmp, 0, tmpWidth * tmpHeight * Bpp);
		    for (unsigned int y = 0; y < height; y++) {
			memcpy(
			    tmp + tmpWidth * Bpp * y + tmpWidth * Bpp + Bpp, 
			    data + yoffset * stride + y * stride + xoffset * Bpp, 
			    width * Bpp);
		    }

		    d2dsurf->rt->CreateBitmap(D2D1::SizeU(tmpWidth, tmpHeight),
					      tmp,
					      tmpWidth * Bpp,
					      D2D1::BitmapProperties(D2D1::PixelFormat(format,
										       D2D1_ALPHA_MODE_PREMULTIPLIED)),
					      &sourceBitmap);
		    delete [] tmp;
		}

		if (!partial) {
		    cached_bitmap *cachebitmap = new cached_bitmap;
		    /* We can cache it if it isn't a partial bitmap */
		    cachebitmap->dirty = false;
		    cachebitmap->bitmap = sourceBitmap;
                    /*
                     * This will start out with two references, one on the snapshot
                     * and one more in the user data structure.
                     */
		    cachebitmap->refs = 2;
		    cairo_surface_set_user_data(surfacePattern->surface,
						key,
						cachebitmap,
						_d2d_release_bitmap);
		    cairo_surface_t *nullSurf =
			_cairo_null_surface_create(CAIRO_CONTENT_COLOR_ALPHA);
		    cairo_surface_set_user_data(nullSurf,
						&bitmap_key_snapshot,
						cachebitmap,
						NULL);
		    _cairo_surface_attach_snapshot(surfacePattern->surface,
						   nullSurf,
						   _d2d_snapshot_detached);
		    cache_usage += _d2d_compute_bitmap_mem_size(sourceBitmap);
		}
		if (pix_image) {
		    pixman_image_unref(pix_image);
  		}
	    }
	} else {
	    return NULL;
	}
	D2D1_BITMAP_BRUSH_PROPERTIES bitProps;
	
	if (surfacePattern->base.filter == CAIRO_FILTER_NEAREST) {
	    bitProps = D2D1::BitmapBrushProperties(extendMode, 
						   extendMode,
						   D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
	} else {
	    bitProps = D2D1::BitmapBrushProperties(extendMode,
						   extendMode,
						   D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
	}
	if (unique) {
	    RefPtr<ID2D1BitmapBrush> bitBrush;
	    D2D1_BRUSH_PROPERTIES brushProps =
		D2D1::BrushProperties(1.0, _cairo_d2d_matrix_from_matrix(&mat));
	    d2dsurf->rt->CreateBitmapBrush(sourceBitmap, 
					   &bitProps,
					   &brushProps,
					   &bitBrush);
	    return bitBrush;
	} else {
	    D2D1_MATRIX_3X2_F matrix = _cairo_d2d_matrix_from_matrix(&mat);

	    if (d2dsurf->bitmapBrush) {
		d2dsurf->bitmapBrush->SetTransform(matrix);

		if (surfacePattern->base.filter == CAIRO_FILTER_NEAREST) {
		    d2dsurf->bitmapBrush->SetInterpolationMode(D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
		} else {
		    d2dsurf->bitmapBrush->SetInterpolationMode(D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
		}

		d2dsurf->bitmapBrush->SetBitmap(sourceBitmap);
		d2dsurf->bitmapBrush->SetExtendModeX(extendMode);
		d2dsurf->bitmapBrush->SetExtendModeY(extendMode);
	    } else {
		D2D1_BRUSH_PROPERTIES brushProps =
		    D2D1::BrushProperties(1.0, _cairo_d2d_matrix_from_matrix(&mat));
		d2dsurf->rt->CreateBitmapBrush(sourceBitmap,
					       &bitProps,
					       &brushProps,
					       &d2dsurf->bitmapBrush);
	    }
	    return d2dsurf->bitmapBrush;
	}
    } else {
	return NULL;
    }
}


/** Path Conversion */

/**
 * Structure to use for the closure, containing all needed data.
 */
struct path_conversion {
    /** Geometry sink that we need to write to */
    ID2D1GeometrySink *sink;
    /** 
     * If this figure is active, cairo doesn't always send us a close. But
     * we do need to end this figure if it didn't.
     */
    bool figureActive;
    /**
     * Current point, D2D has no explicit move so we need to track moved for
     * the next begin.
     */
    cairo_point_t current_point;
    /** The type of figure begin for this geometry instance */
    D2D1_FIGURE_BEGIN type;
};

static cairo_status_t
_cairo_d2d_path_move_to(void		 *closure,
			const cairo_point_t *point)
{
    path_conversion *pathConvert =
	static_cast<path_conversion*>(closure);
    if (pathConvert->figureActive) {
	pathConvert->sink->EndFigure(D2D1_FIGURE_END_OPEN);
	pathConvert->figureActive = false;
    }

    pathConvert->current_point = *point;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_d2d_path_line_to(void		    *closure,
			const cairo_point_t *point)
{
    path_conversion *pathConvert =
	static_cast<path_conversion*>(closure);
    if (!pathConvert->figureActive) {
	pathConvert->sink->BeginFigure(_d2d_point_from_cairo_point(&pathConvert->current_point),
				       pathConvert->type);
	pathConvert->figureActive = true;
    }

    D2D1_POINT_2F d2dpoint = _d2d_point_from_cairo_point(point);

    pathConvert->sink->AddLine(d2dpoint);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_d2d_path_curve_to(void	  *closure,
			 const cairo_point_t *p0,
			 const cairo_point_t *p1,
			 const cairo_point_t *p2)
{
    path_conversion *pathConvert =
	static_cast<path_conversion*>(closure);
    if (!pathConvert->figureActive) {
	pathConvert->sink->BeginFigure(_d2d_point_from_cairo_point(&pathConvert->current_point),
				       D2D1_FIGURE_BEGIN_FILLED);
	pathConvert->figureActive = true;
    }

    pathConvert->sink->AddBezier(D2D1::BezierSegment(_d2d_point_from_cairo_point(p0),
						     _d2d_point_from_cairo_point(p1),
						     _d2d_point_from_cairo_point(p2)));
	
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_d2d_path_close(void *closure)
{
    path_conversion *pathConvert =
	static_cast<path_conversion*>(closure);

    if (!pathConvert->figureActive) {
	pathConvert->sink->BeginFigure(_d2d_point_from_cairo_point(&pathConvert->current_point),
				       pathConvert->type);
	/**
	 * In this case we mean a single point. For D2D this means we need to add an infinitely
	 * small line here to get that effect.
	 */
	pathConvert->sink->AddLine(_d2d_point_from_cairo_point(&pathConvert->current_point));
    }

    pathConvert->sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    pathConvert->figureActive = false;
    return CAIRO_STATUS_SUCCESS;
}

/**
 * Create an ID2D1PathGeometry for a cairo_path_fixed_t
 *
 * \param path Path to create a geometry for
 * \param fill_rule Fill rule to use
 * \param type Figure begin type to use
 * \return A D2D geometry
 */
static RefPtr<ID2D1PathGeometry>
_cairo_d2d_create_path_geometry_for_path(cairo_path_fixed_t *path, 
					 cairo_fill_rule_t fill_rule,
					 D2D1_FIGURE_BEGIN type)
{
    RefPtr<ID2D1PathGeometry> d2dpath;
    D2DSurfFactory::Instance()->CreatePathGeometry(&d2dpath);
    RefPtr<ID2D1GeometrySink> sink;
    d2dpath->Open(&sink);
    D2D1_FILL_MODE fillMode = D2D1_FILL_MODE_WINDING;
    if (fill_rule == CAIRO_FILL_RULE_WINDING) {
	fillMode = D2D1_FILL_MODE_WINDING;
    } else if (fill_rule == CAIRO_FILL_RULE_EVEN_ODD) {
	fillMode = D2D1_FILL_MODE_ALTERNATE;
    }
    sink->SetFillMode(fillMode);

    path_conversion pathConvert;
    pathConvert.type = type;
    pathConvert.sink = sink;
    pathConvert.figureActive = false;
    _cairo_path_fixed_interpret(path,
				CAIRO_DIRECTION_FORWARD,
				_cairo_d2d_path_move_to,
				_cairo_d2d_path_line_to,
				_cairo_d2d_path_curve_to,
				_cairo_d2d_path_close,
				&pathConvert);
    if (pathConvert.figureActive) {
	sink->EndFigure(D2D1_FIGURE_END_OPEN);
    }
    sink->Close();
    return d2dpath;
}

static cairo_bool_t
clip_contains_only_boxes (cairo_clip_t *clip)
{
    cairo_bool_t is_boxes = TRUE;

    if (clip) {
	cairo_box_t clip_box;
	cairo_clip_path_t *path = clip->path;

	while (path) {
	    is_boxes &= _cairo_path_fixed_is_box(&path->path, &clip_box);
	    path = path->prev;
	}
    }
    return is_boxes;
}

static cairo_int_status_t
_cairo_d2d_clear_box (cairo_d2d_surface_t *d2dsurf,
		 cairo_clip_t *clip,
		 cairo_box_t *box)
{
    if (clip_contains_only_boxes (clip)) {
	/* clear the box using axis aligned clips */
	d2dsurf->rt->PushAxisAlignedClip(D2D1::RectF(_cairo_fixed_to_float(box->p1.x),
		    _cairo_fixed_to_float(box->p1.y),
		    _cairo_fixed_to_float(box->p2.x),
		    _cairo_fixed_to_float(box->p2.y)),
		D2D1_ANTIALIAS_MODE_ALIASED);
	d2dsurf->rt->Clear(D2D1::ColorF(0, 0));
	d2dsurf->rt->PopAxisAlignedClip();

	return CAIRO_INT_STATUS_SUCCESS;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_d2d_clear (cairo_d2d_surface_t *d2dsurf,
		 cairo_clip_t *clip)
{
    cairo_region_t *region;
    cairo_int_status_t status;

    if (!clip) {
	/* no clip so clear everything */
	_begin_draw_state(d2dsurf);
	reset_clip(d2dsurf);
	d2dsurf->rt->Clear(D2D1::ColorF(0, 0));

	return CAIRO_INT_STATUS_SUCCESS;
    }

    status = _cairo_clip_get_region (clip, &region);
    if (status)
	return status;

    /* We now have a region, we'll clear it one rectangle at a time */
    _begin_draw_state(d2dsurf);

    reset_clip(d2dsurf);

    if (region) {
	int num_rects;
	int i;

	num_rects = cairo_region_num_rectangles (region);

	for (i = 0; i < num_rects; i++) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (region, i, &rect);

	    d2dsurf->rt->PushAxisAlignedClip(
		    D2D1::RectF((FLOAT)rect.x,
			        (FLOAT)rect.y,
				(FLOAT)rect.x + rect.width,
				(FLOAT)rect.y + rect.height),
		    D2D1_ANTIALIAS_MODE_ALIASED);

	    d2dsurf->rt->Clear(D2D1::ColorF(0, 0));

	    d2dsurf->rt->PopAxisAlignedClip();
	}

    }

    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_operator_t _cairo_d2d_simplify_operator(cairo_operator_t op,
						     const cairo_pattern_t *source)
{
    if (op == CAIRO_OPERATOR_SOURCE) {
	/** Operator over is easier for D2D! If the source if opaque, change */
	if (source->type == CAIRO_PATTERN_TYPE_SURFACE) {
	    const cairo_surface_pattern_t *surfpattern =
		reinterpret_cast<const cairo_surface_pattern_t*>(source);
	    if (surfpattern->surface->content == CAIRO_CONTENT_COLOR) {
		return CAIRO_OPERATOR_OVER;
	    }
	} else if (source->type == CAIRO_PATTERN_TYPE_SOLID) {
	    const cairo_solid_pattern_t *solidpattern =
		reinterpret_cast<const cairo_solid_pattern_t*>(source);
	    if (solidpattern->color.alpha == 1.0) {
		return CAIRO_OPERATOR_OVER;
	    }
	}
    }
    return op;
}

// Implementation
static cairo_surface_t*
_cairo_d2d_create_similar(void			*surface,
			  cairo_content_t	 content,
			  int			 width,
			  int			 height)
{
    cairo_d2d_surface_t *d2dsurf = static_cast<cairo_d2d_surface_t*>(surface);
    cairo_d2d_surface_t *newSurf = static_cast<cairo_d2d_surface_t*>(malloc(sizeof(cairo_d2d_surface_t)));
    
    new (newSurf) cairo_d2d_surface_t();
    _cairo_surface_init(&newSurf->base, &cairo_d2d_surface_backend, content);


    D2D1_SIZE_U sizePixels;
    D2D1_SIZE_F size;
    HRESULT hr;

    sizePixels.width = width;
    sizePixels.height = height;
    FLOAT dpiX;
    FLOAT dpiY;

    d2dsurf->rt->GetDpi(&dpiX, &dpiY);

    D2D1_ALPHA_MODE alpha;

    if (content == CAIRO_CONTENT_COLOR) {
	alpha = D2D1_ALPHA_MODE_IGNORE;
    } else {
	alpha = D2D1_ALPHA_MODE_PREMULTIPLIED;
    }

    size.width = sizePixels.width * dpiX;
    size.height = sizePixels.height * dpiY;
    D2D1_BITMAP_PROPERTIES bitProps = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN,
									       alpha));
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
								       D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN,
											 alpha),
								       dpiX,
								       dpiY,
								       D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE);

    if (sizePixels.width < 1) {
	sizePixels.width = 1;
    }
    if (sizePixels.height < 1) {
	sizePixels.height = 1;
    }
    RefPtr<IDXGISurface> oldDxgiSurface;
    d2dsurf->surface->QueryInterface(&oldDxgiSurface);
    DXGI_SURFACE_DESC origDesc;

    oldDxgiSurface->GetDesc(&origDesc);

    CD3D10_TEXTURE2D_DESC desc(origDesc.Format,
			       sizePixels.width,
			       sizePixels.height);

    if (content == CAIRO_CONTENT_ALPHA) {
	desc.Format = DXGI_FORMAT_A8_UNORM;
    }

    desc.MipLevels = 1;
    desc.Usage = D3D10_USAGE_DEFAULT;
    desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D10_RESOURCE_MISC_GDI_COMPATIBLE;
    RefPtr<ID3D10Texture2D> texture;
    RefPtr<IDXGISurface> dxgiSurface;

    hr = D3D10Factory::Device()->CreateTexture2D(&desc, NULL, &texture);
    if (FAILED(hr)) {
	goto FAIL_CREATESIMILAR;
    }

    newSurf->surface = texture;

    // Create the DXGI surface.
    hr = newSurf->surface->QueryInterface(IID_IDXGISurface, (void**)&dxgiSurface);
    if (FAILED(hr)) {
	goto FAIL_CREATESIMILAR;
    }
    hr = D2DSurfFactory::Instance()->CreateDxgiSurfaceRenderTarget(dxgiSurface,
								   props,
								   &newSurf->rt);

    if (FAILED(hr)) {
	goto FAIL_CREATESIMILAR;
    }

    if (desc.Format != DXGI_FORMAT_A8_UNORM) {
	/* For some reason creation of shared bitmaps for A8 UNORM surfaces
	 * doesn't work even though the documentation suggests it does. The
	 * function will return an error if we try */
	hr = newSurf->rt->CreateSharedBitmap(IID_IDXGISurface,
					     dxgiSurface,
					     &bitProps,
					     &newSurf->surfaceBitmap);
	if (FAILED(hr)) {
	    goto FAIL_CREATESIMILAR;
	}
    }

    newSurf->rt->CreateSolidColorBrush(D2D1::ColorF(0, 1.0), &newSurf->solidColorBrush);

    _d2d_clear_surface(newSurf);

    return reinterpret_cast<cairo_surface_t*>(newSurf);

FAIL_CREATESIMILAR:
    /** Ensure we call our surfaces desctructor */
    newSurf->~cairo_d2d_surface_t();
    free(newSurf);
    return _cairo_surface_create_in_error(_cairo_error(CAIRO_STATUS_NO_MEMORY));
}

static cairo_status_t
_cairo_d2d_finish(void	    *surface)
{
    cairo_d2d_surface_t *d2dsurf = static_cast<cairo_d2d_surface_t*>(surface);

    reset_clip(d2dsurf);

    d2dsurf->~cairo_d2d_surface_t();
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_d2d_acquire_source_image(void                    *abstract_surface,
				cairo_image_surface_t  **image_out,
				void                   **image_extra)
{
    cairo_d2d_surface_t *d2dsurf = static_cast<cairo_d2d_surface_t*>(abstract_surface);
    _cairo_d2d_flush(d2dsurf);

    HRESULT hr;
    D2D1_SIZE_U size = d2dsurf->rt->GetPixelSize();

    RefPtr<ID3D10Texture2D> softTexture;

    RefPtr<IDXGISurface> dxgiSurface;
    d2dsurf->surface->QueryInterface(&dxgiSurface);
    DXGI_SURFACE_DESC desc;

    dxgiSurface->GetDesc(&desc);

    CD3D10_TEXTURE2D_DESC softDesc(desc.Format, desc.Width, desc.Height);

    /**
     * We can't actually map our backing store texture, so we create one in CPU memory, and then
     * tell D3D to copy the data from our surface there, readback is expensive, we -never-
     * -ever- want this to happen.
     */
    softDesc.MipLevels = 1;
    softDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE | D3D10_CPU_ACCESS_READ;
    softDesc.Usage = D3D10_USAGE_STAGING;
    softDesc.BindFlags = 0;
    hr = D3D10Factory::Device()->CreateTexture2D(&softDesc, NULL, &softTexture);
    if (FAILED(hr)) {
	return CAIRO_STATUS_NO_MEMORY;
    }

    D3D10Factory::Device()->CopyResource(softTexture, d2dsurf->surface);

    D3D10_MAPPED_TEXTURE2D data;
    hr = softTexture->Map(0, D3D10_MAP_READ_WRITE, 0, &data);
    if (FAILED(hr)) {
	return (cairo_status_t)CAIRO_INT_STATUS_UNSUPPORTED;
    }
    *image_out = 
	(cairo_image_surface_t*)_cairo_image_surface_create_for_data_with_content((unsigned char*)data.pData,
										  d2dsurf->base.content,
										  size.width,
										  size.height,
										  data.RowPitch);
    *image_extra = softTexture.forget();

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_d2d_release_source_image(void                   *abstract_surface,
				cairo_image_surface_t  *image,
				void                   *image_extra)
{
    if (((cairo_surface_t*)abstract_surface)->type != CAIRO_SURFACE_TYPE_D2D) {
	return;
    }
    cairo_d2d_surface_t *d2dsurf = static_cast<cairo_d2d_surface_t*>(abstract_surface);

    cairo_surface_destroy(&image->base);
    ID3D10Texture2D *softTexture = (ID3D10Texture2D*)image_extra;
    
    softTexture->Unmap(0);
    softTexture->Release();
    softTexture = NULL;
}

static cairo_status_t
_cairo_d2d_acquire_dest_image(void                    *abstract_surface,
			      cairo_rectangle_int_t   *interest_rect,
			      cairo_image_surface_t  **image_out,
			      cairo_rectangle_int_t   *image_rect,
			      void                   **image_extra)
{
    cairo_d2d_surface_t *d2dsurf = static_cast<cairo_d2d_surface_t*>(abstract_surface);
    _cairo_d2d_flush(d2dsurf);

    HRESULT hr;
    D2D1_SIZE_U size = d2dsurf->rt->GetPixelSize();

    RefPtr<ID3D10Texture2D> softTexture;


    RefPtr<IDXGISurface> dxgiSurface;
    d2dsurf->surface->QueryInterface(&dxgiSurface);
    DXGI_SURFACE_DESC desc;

    dxgiSurface->GetDesc(&desc);

    CD3D10_TEXTURE2D_DESC softDesc(desc.Format, desc.Width, desc.Height);

    image_rect->width = desc.Width;
    image_rect->height = desc.Height;
    image_rect->x = image_rect->y = 0;

    softDesc.MipLevels = 1;
    softDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE | D3D10_CPU_ACCESS_READ;
    softDesc.Usage = D3D10_USAGE_STAGING;
    softDesc.BindFlags = 0;
    hr = D3D10Factory::Device()->CreateTexture2D(&softDesc, NULL, &softTexture);
    if (FAILED(hr)) {
	return CAIRO_STATUS_NO_MEMORY;
    }
    D3D10Factory::Device()->CopyResource(softTexture, d2dsurf->surface);

    D3D10_MAPPED_TEXTURE2D data;
    hr = softTexture->Map(0, D3D10_MAP_READ_WRITE, 0, &data);
    if (FAILED(hr)) {
	return (cairo_status_t)CAIRO_INT_STATUS_UNSUPPORTED;
    }
    *image_out = 
	(cairo_image_surface_t*)_cairo_image_surface_create_for_data_with_content((unsigned char*)data.pData,
										  CAIRO_CONTENT_COLOR_ALPHA,
										  size.width,
										  size.height,
										  data.RowPitch);
    *image_extra = softTexture.forget();

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_d2d_release_dest_image(void                    *abstract_surface,
			      cairo_rectangle_int_t   *interest_rect,
			      cairo_image_surface_t   *image,
			      cairo_rectangle_int_t   *image_rect,
			      void                    *image_extra)
{
    cairo_d2d_surface_t *d2dsurf = static_cast<cairo_d2d_surface_t*>(abstract_surface);

    ID3D10Texture2D *softTexture = (ID3D10Texture2D*)image_extra;
    D2D1_POINT_2U point;
    point.x = 0;
    point.y = 0;
    D2D1_RECT_U rect;
    D2D1_SIZE_U size = d2dsurf->rt->GetPixelSize();
    rect.left = rect.top = 0;
    rect.right = size.width;
    rect.bottom = size.height;

    cairo_surface_destroy(&image->base);

    softTexture->Unmap(0);
    D3D10Factory::Device()->CopyResource(d2dsurf->surface, softTexture);
    softTexture->Release();
}


static cairo_status_t
_cairo_d2d_flush(void                  *surface)
{
    cairo_d2d_surface_t *d2dsurf = static_cast<cairo_d2d_surface_t*>(surface);

    if (d2dsurf->isDrawing) {
	reset_clip(d2dsurf);
	HRESULT hr = d2dsurf->rt->EndDraw();
	d2dsurf->isDrawing = false;
    }

    return CAIRO_STATUS_SUCCESS;
}



static cairo_int_status_t
_cairo_d2d_paint(void			*surface,
		 cairo_operator_t	 op,
		 const cairo_pattern_t	*source,
		 cairo_clip_t		*clip)
{
    cairo_d2d_surface_t *d2dsurf = static_cast<cairo_d2d_surface_t*>(surface);
    cairo_int_status_t status;

    op = _cairo_d2d_simplify_operator(op, source);

    if (op == CAIRO_OPERATOR_CLEAR) {
	return _cairo_d2d_clear(d2dsurf, clip);
    }


    _begin_draw_state(d2dsurf);
    status = (cairo_int_status_t)_cairo_d2d_set_clip (d2dsurf, clip);

    if (unlikely(status))
	return status;

    d2dsurf->rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

    RefPtr<ID2D1Brush> brush = _cairo_d2d_create_brush_for_pattern(d2dsurf,
								   source);
    
    if (!brush) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    if (op == CAIRO_OPERATOR_OVER) {
	D2D1_SIZE_F size = d2dsurf->rt->GetSize();
	d2dsurf->rt->FillRectangle(D2D1::RectF((FLOAT)0,
					       (FLOAT)0,
					       (FLOAT)size.width,
					       (FLOAT)size.height),
				   brush);
    } else if (op == CAIRO_OPERATOR_SOURCE) {
	D2D1_SIZE_F size = d2dsurf->rt->GetSize();
	d2dsurf->rt->Clear(D2D1::ColorF(0, 0));
	d2dsurf->rt->FillRectangle(D2D1::RectF((FLOAT)0,
					       (FLOAT)0,
					       (FLOAT)size.width,
					       (FLOAT)size.height),
				   brush);
    } else {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_d2d_mask(void			*surface,
		cairo_operator_t	 op,
		const cairo_pattern_t	*source,
		const cairo_pattern_t	*mask,
		cairo_clip_t		*clip)
{
    cairo_d2d_surface_t *d2dsurf = static_cast<cairo_d2d_surface_t*>(surface);
    cairo_rectangle_int_t extents;

    cairo_int_status_t status;

    _begin_draw_state(d2dsurf);
    status = (cairo_int_status_t)_cairo_d2d_set_clip (d2dsurf, clip);

    if (unlikely (status))
	return status;


    status = (cairo_int_status_t)_cairo_surface_mask_extents (&d2dsurf->base,
		    op, source,
		    mask,
		    clip, &extents);
    if (unlikely (status))
	    return status;


    RefPtr<ID2D1Brush> brush = _cairo_d2d_create_brush_for_pattern(d2dsurf, source);
    if (!brush) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    D2D1_RECT_F rect = D2D1::RectF(0,
				   0,
				   (FLOAT)d2dsurf->rt->GetPixelSize().width,
				   (FLOAT)d2dsurf->rt->GetPixelSize().height);

    rect.left = (FLOAT)extents.x;
    rect.right = (FLOAT)(extents.x + extents.width);
    rect.top = (FLOAT)extents.y;
    rect.bottom = (FLOAT)(extents.y + extents.height);


    if (mask->type == CAIRO_PATTERN_TYPE_SOLID) {
	cairo_solid_pattern_t *solidPattern =
	    (cairo_solid_pattern_t*)mask;
	if (solidPattern->content = CAIRO_CONTENT_ALPHA) {
	    brush->SetOpacity((FLOAT)solidPattern->color.alpha);
	    d2dsurf->rt->FillRectangle(rect,
				       brush);
	    brush->SetOpacity(1.0);
	    return CAIRO_INT_STATUS_SUCCESS;
	}
    }

    RefPtr<ID2D1Brush> opacityBrush = _cairo_d2d_create_brush_for_pattern(d2dsurf, mask, true);
    if (!opacityBrush) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (!d2dsurf->maskLayer) {
	d2dsurf->rt->CreateLayer(&d2dsurf->maskLayer);
    }
    d2dsurf->rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),
						 0,
						 D2D1_ANTIALIAS_MODE_ALIASED,
						 D2D1::IdentityMatrix(),
						 1.0,
						 opacityBrush),
			   d2dsurf->maskLayer);

    d2dsurf->rt->FillRectangle(rect,
			       brush);
    d2dsurf->rt->PopLayer();
    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_d2d_stroke(void			*surface,
		  cairo_operator_t	 op,
		  const cairo_pattern_t	*source,
		  cairo_path_fixed_t	*path,
		  cairo_stroke_style_t	*style,
		  cairo_matrix_t	*ctm,
		  cairo_matrix_t	*ctm_inverse,
		  double		 tolerance,
		  cairo_antialias_t	 antialias,
		  cairo_clip_t		*clip)
{
    cairo_int_status_t status;

    cairo_d2d_surface_t *d2dsurf = static_cast<cairo_d2d_surface_t*>(surface);

    op = _cairo_d2d_simplify_operator(op, source);

    if (op != CAIRO_OPERATOR_OVER && op != CAIRO_OPERATOR_ADD) {
	/** 
	 * We don't really support ADD yet. True ADD support requires getting
	 * the tesselated mesh from D2D, and blending that using D3D which has
	 * an add operator available.
	 */
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    _begin_draw_state(d2dsurf);

    status = (cairo_int_status_t)_cairo_d2d_set_clip (d2dsurf, clip);

    if (unlikely (status))
	return status;


    if (antialias == CAIRO_ANTIALIAS_NONE) {
	d2dsurf->rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    } else {
	d2dsurf->rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
    RefPtr<ID2D1StrokeStyle> strokeStyle = _cairo_d2d_create_strokestyle_for_stroke_style(style);

    if (!strokeStyle) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    D2D1::Matrix3x2F mat = _cairo_d2d_matrix_from_matrix(ctm);

    _cairo_path_fixed_transform(path, ctm_inverse);

    d2dsurf->rt->SetTransform(mat);

    cairo_box_t box;

    RefPtr<ID2D1Brush> brush = _cairo_d2d_create_brush_for_pattern(d2dsurf,
								   source);
    if (!brush) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (_cairo_path_fixed_is_box(path, &box)) {
	float x1 = _cairo_fixed_to_float(box.p1.x);    
	float y1 = _cairo_fixed_to_float(box.p1.y);    
	float x2 = _cairo_fixed_to_float(box.p2.x);    
	float y2 = _cairo_fixed_to_float(box.p2.y);

	d2dsurf->rt->DrawRectangle(D2D1::RectF(x1,
					       y1,
					       x2,
					       y2),
				   brush,
				   (FLOAT)style->line_width,
				   strokeStyle);

    } else {
	RefPtr<ID2D1Geometry> d2dpath = _cairo_d2d_create_path_geometry_for_path(path, 
			    							 CAIRO_FILL_RULE_WINDING, 
										 D2D1_FIGURE_BEGIN_HOLLOW);
	d2dsurf->rt->DrawGeometry(d2dpath, brush, (FLOAT)style->line_width, strokeStyle);
    }

    _cairo_path_fixed_transform(path, ctm);
    d2dsurf->rt->SetTransform(D2D1::Matrix3x2F::Identity());
    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_d2d_fill(void			*surface,
		cairo_operator_t	 op,
		const cairo_pattern_t	*source,
		cairo_path_fixed_t	*path,
		cairo_fill_rule_t	 fill_rule,
		double			 tolerance,
		cairo_antialias_t	 antialias,
		cairo_clip_t		*clip)
{
    cairo_int_status_t status;

    cairo_d2d_surface_t *d2dsurf = static_cast<cairo_d2d_surface_t*>(surface);

    op = _cairo_d2d_simplify_operator(op, source);

    if (op != CAIRO_OPERATOR_OVER && op != CAIRO_OPERATOR_ADD) {
	/** 
	 * We don't really support ADD yet. True ADD support requires getting
	 * the tesselated mesh from D2D, and blending that using D3D which has
	 * an add operator available.
	 */
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    _begin_draw_state(d2dsurf);
    status = (cairo_int_status_t)_cairo_d2d_set_clip (d2dsurf, clip);

    if (unlikely(status))
	return status;


    if (antialias == CAIRO_ANTIALIAS_NONE) {
	d2dsurf->rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    } else {
	d2dsurf->rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }

    cairo_box_t box;

    if (op == CAIRO_OPERATOR_CLEAR) {
	if (_cairo_path_fixed_is_box(path, &box)) {
	    return _cairo_d2d_clear_box (d2dsurf, clip, &box);
	} else {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    }

    if (_cairo_path_fixed_is_box(path, &box)) {
	float x1 = _cairo_fixed_to_float(box.p1.x);
	float y1 = _cairo_fixed_to_float(box.p1.y);    
	float x2 = _cairo_fixed_to_float(box.p2.x);    
	float y2 = _cairo_fixed_to_float(box.p2.y);
	RefPtr<ID2D1Brush> brush = _cairo_d2d_create_brush_for_pattern(d2dsurf,
								       source);
	if (!brush) {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}

	d2dsurf->rt->FillRectangle(D2D1::RectF(x1,
					       y1,
					       x2,
					       y2),
				   brush);
    } else {
	RefPtr<ID2D1Geometry> d2dpath = _cairo_d2d_create_path_geometry_for_path(path, fill_rule, D2D1_FIGURE_BEGIN_FILLED);

	RefPtr<ID2D1Brush> brush = _cairo_d2d_create_brush_for_pattern(d2dsurf,
								       source);
	if (!brush) {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
	d2dsurf->rt->FillGeometry(d2dpath, brush);
    }
    return CAIRO_INT_STATUS_SUCCESS;
}


static cairo_int_status_t
_cairo_d2d_show_glyphs (void			*surface,
			cairo_operator_t	 op,
			const cairo_pattern_t	*source,
			cairo_glyph_t		*glyphs,
			int			 num_glyphs,
			cairo_scaled_font_t	*scaled_font,
			cairo_clip_t            *clip,
			int			*remaining_glyphs)
{
    if (((cairo_surface_t*)surface)->type != CAIRO_SURFACE_TYPE_D2D) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    cairo_d2d_surface_t *d2dsurf = static_cast<cairo_d2d_surface_t*>(surface);
    if (!d2dsurf->textRenderingInit) {
	RefPtr<IDWriteRenderingParams> params;
	DWriteFactory::Instance()->CreateRenderingParams(&params);
	d2dsurf->rt->SetTextRenderingParams(params);
	d2dsurf->textRenderingInit = true;
    }
    cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (scaled_font->backend->type == CAIRO_FONT_TYPE_DWRITE) {
        status = (cairo_int_status_t)
	    _cairo_dwrite_show_glyphs_on_d2d_surface(surface, op, source, glyphs, num_glyphs, scaled_font, clip);
    }

    return status;
}


static cairo_bool_t
_cairo_d2d_getextents(void		       *surface,
		      cairo_rectangle_int_t    *extents)
{
    cairo_d2d_surface_t *d2dsurf = static_cast<cairo_d2d_surface_t*>(surface);
    extents->x = 0;
    extents->y = 0;
    D2D1_SIZE_U size = d2dsurf->rt->GetPixelSize(); 
    extents->width = size.width;
    extents->height = size.height;
    return TRUE;
}


/** Helper functions. */

cairo_surface_t*
cairo_d2d_surface_create_for_hwnd(HWND wnd,
				  cairo_content_t content)
{
    if (!D3D10Factory::Device() || !D2DSurfFactory::Instance()) {
	/**
	 * FIXME: In the near future we can use cairo_device_t to pass in a
	 * device.
	 */
	return _cairo_surface_create_in_error(_cairo_error(CAIRO_STATUS_NO_DEVICE));
    }

    cairo_d2d_surface_t *newSurf = static_cast<cairo_d2d_surface_t*>(malloc(sizeof(cairo_d2d_surface_t)));
    new (newSurf) cairo_d2d_surface_t();

    _cairo_surface_init(&newSurf->base, &cairo_d2d_surface_backend, content);

    RECT rc;
    HRESULT hr;

    newSurf->isDrawing = false;
    ::GetClientRect(wnd, &rc);

    FLOAT dpiX;
    FLOAT dpiY;
    D2D1_SIZE_U sizePixels;
    D2D1_SIZE_F size;

    dpiX = 96;
    dpiY = 96;


    sizePixels.width = rc.right - rc.left;
    sizePixels.height = rc.bottom - rc.top;

    if (!sizePixels.width) {
	sizePixels.width = 1;
    }
    if (!sizePixels.height) {
	sizePixels.height = 1;
    }
    ID3D10Device1 *device = D3D10Factory::Device();
    RefPtr<IDXGIDevice> dxgiDevice;
    RefPtr<IDXGIAdapter> dxgiAdapter;
    RefPtr<IDXGIFactory> dxgiFactory;
    D2D1_RENDER_TARGET_PROPERTIES props;    
    D2D1_BITMAP_PROPERTIES bitProps;

    device->QueryInterface(&dxgiDevice);
    dxgiDevice->GetAdapter(&dxgiAdapter);
    dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));

    DXGI_SWAP_CHAIN_DESC swapDesc;
    ::ZeroMemory(&swapDesc, sizeof(swapDesc));

    swapDesc.BufferDesc.Width = sizePixels.width;
    swapDesc.BufferDesc.Height = sizePixels.height;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SampleDesc.Quality = 0;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 1;
    swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE;
    swapDesc.OutputWindow = wnd;
    swapDesc.Windowed = TRUE;

    /**
     * Create a swap chain, this swap chain will contain the backbuffer for
     * the window we draw to. The front buffer is the full screen front
     * buffer.
     */
    hr = dxgiFactory->CreateSwapChain(dxgiDevice, &swapDesc, &newSurf->dxgiChain);

    /**
     * We do not want DXGI to intercept alt-enter events and make the window go
     * fullscreen! This shouldn't be in the cairo backend but controlled through
     * the device. See comments on mozilla bug 553603.
     */
    dxgiFactory->MakeWindowAssociation(wnd, DXGI_MWA_NO_WINDOW_CHANGES);

    if (FAILED(hr)) {
	goto FAIL_HWND;
    }
    /** Get the backbuffer surface from the swap chain */
    hr = newSurf->dxgiChain->GetBuffer(0,
	                               IID_PPV_ARGS(&newSurf->backBuf));

    if (FAILED(hr)) {
	goto FAIL_HWND;
    }

    newSurf->backBuf->QueryInterface(&newSurf->surface);

    size.width = sizePixels.width * dpiX;
    size.height = sizePixels.height * dpiY;

    props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
					 D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
					 dpiX,
					 dpiY,
					 D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE);
    hr = D2DSurfFactory::Instance()->CreateDxgiSurfaceRenderTarget(newSurf->backBuf,
								   props,
								   &newSurf->rt);
    if (FAILED(hr)) {
	goto FAIL_HWND;
    }

    bitProps = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, 
				      D2D1_ALPHA_MODE_PREMULTIPLIED));
    
    newSurf->rt->CreateSolidColorBrush(D2D1::ColorF(0, 1.0), &newSurf->solidColorBrush);

    _d2d_clear_surface(newSurf);

    return reinterpret_cast<cairo_surface_t*>(newSurf);

FAIL_HWND:
    newSurf->~cairo_d2d_surface_t();
    free(newSurf);
    return _cairo_surface_create_in_error(_cairo_error(CAIRO_STATUS_NO_MEMORY));
}

cairo_surface_t *
cairo_d2d_surface_create(cairo_format_t format,
                         int width,
                         int height)
{
    if (!D3D10Factory::Device() || !D2DSurfFactory::Instance()) {
	/**
	 * FIXME: In the near future we can use cairo_device_t to pass in a
	 * device.
	 */
	return _cairo_surface_create_in_error(_cairo_error(CAIRO_STATUS_NO_DEVICE));
    }
    cairo_d2d_surface_t *newSurf = static_cast<cairo_d2d_surface_t*>(malloc(sizeof(cairo_d2d_surface_t)));
    new (newSurf) cairo_d2d_surface_t();

    DXGI_FORMAT dxgiformat = DXGI_FORMAT_B8G8R8A8_UNORM;
    D2D1_ALPHA_MODE alpha = D2D1_ALPHA_MODE_PREMULTIPLIED;
    if (format == CAIRO_FORMAT_ARGB32) {
	_cairo_surface_init(&newSurf->base, &cairo_d2d_surface_backend, CAIRO_CONTENT_COLOR_ALPHA);
    } else if (format == CAIRO_FORMAT_RGB24) {
	_cairo_surface_init(&newSurf->base, &cairo_d2d_surface_backend, CAIRO_CONTENT_COLOR);
	alpha = D2D1_ALPHA_MODE_IGNORE;
    } else {
	_cairo_surface_init(&newSurf->base, &cairo_d2d_surface_backend, CAIRO_CONTENT_ALPHA);
	dxgiformat = DXGI_FORMAT_A8_UNORM;
    }


    newSurf->format = format;

    D2D1_SIZE_U sizePixels;
    HRESULT hr;

    sizePixels.width = width;
    sizePixels.height = height;

    CD3D10_TEXTURE2D_DESC desc(
	dxgiformat,
	sizePixels.width,
	sizePixels.height
	);
    desc.MipLevels = 1;
    desc.Usage = D3D10_USAGE_DEFAULT;
    desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D10_RESOURCE_MISC_GDI_COMPATIBLE;
    
    RefPtr<ID3D10Texture2D> texture;
    RefPtr<IDXGISurface> dxgiSurface;
    D2D1_BITMAP_PROPERTIES bitProps;
    D2D1_RENDER_TARGET_PROPERTIES props;

    hr = D3D10Factory::Device()->CreateTexture2D(&desc, NULL, &texture);

    if (FAILED(hr)) {
	goto FAIL_CREATE;
    }

    newSurf->surface = texture;

    /** Create the DXGI surface. */
    hr = newSurf->surface->QueryInterface(IID_IDXGISurface, (void**)&dxgiSurface);
    if (FAILED(hr)) {
	goto FAIL_CREATE;
    }

    props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
					 D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, alpha));
    props.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
    hr = D2DSurfFactory::Instance()->CreateDxgiSurfaceRenderTarget(dxgiSurface,
								   props,
								   &newSurf->rt);

    if (FAILED(hr)) {
	goto FAIL_CREATE;
    }

    bitProps = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, 
				      alpha));

    if (dxgiformat != DXGI_FORMAT_A8_UNORM) {
	/* For some reason creation of shared bitmaps for A8 UNORM surfaces
	 * doesn't work even though the documentation suggests it does. The
	 * function will return an error if we try */
	hr = newSurf->rt->CreateSharedBitmap(IID_IDXGISurface,
					     dxgiSurface,
					     &bitProps,
					     &newSurf->surfaceBitmap);

	if (FAILED(hr)) {
	    goto FAIL_CREATE;
	}
    }

    newSurf->rt->CreateSolidColorBrush(D2D1::ColorF(0, 1.0), &newSurf->solidColorBrush);

    _d2d_clear_surface(newSurf);

    return reinterpret_cast<cairo_surface_t*>(newSurf);

FAIL_CREATE:
    newSurf->~cairo_d2d_surface_t();
    free(newSurf);
    return _cairo_surface_create_in_error(_cairo_error(CAIRO_STATUS_NO_MEMORY));
}

void cairo_d2d_scroll(cairo_surface_t *surface, int x, int y, cairo_rectangle_t *clip)
{
    if (surface->type != CAIRO_SURFACE_TYPE_D2D) {
        return;
    }
    cairo_d2d_surface_t *d2dsurf = reinterpret_cast<cairo_d2d_surface_t*>(surface);

    /** For now, we invalidate our storing texture with this operation. */
    D2D1_POINT_2U point;
    D3D10_BOX rect;
    rect.front = 0;
    rect.back = 1;

    RefPtr<IDXGISurface> dxgiSurface;
    d2dsurf->surface->QueryInterface(&dxgiSurface);
    DXGI_SURFACE_DESC desc;

    dxgiSurface->GetDesc(&desc);

    /**
     * It's important we constrain the size of the clip region to the area of
     * the surface. If we don't we might get a box that goes outside the
     * surface, and CopySubresourceRegion will become very angry with us.
     * It will cause a device failure and subsequent drawing will break.
     */
    clip->x = MAX(clip->x, 0);
    clip->y = MAX(clip->y, 0);
    clip->width = MIN(clip->width, desc.Width - clip->x);
    clip->height = MIN(clip->height, desc.Height - clip->y);

    if (x < 0) {
	point.x = (UINT32)clip->x;
	rect.left = (UINT)(clip->x - x);
	rect.right = (UINT)(clip->x + clip->width);
    } else {
	point.x = (UINT32)(clip->x + x);
	rect.left = (UINT)clip->x;
	rect.right = (UINT32)(clip->x + clip->width - x);
    }
    if (y < 0) {
	point.y = (UINT32)clip->y;
	rect.top = (UINT)(clip->y - y);
	rect.bottom = (UINT)(clip->y + clip->height);
    } else {
	point.y = (UINT32)(clip->y + y);
	rect.top = (UINT)clip->y;
	rect.bottom = (UINT)(clip->y + clip->height - y);
    }
    ID3D10Texture2D *texture = _cairo_d2d_get_buffer_texture(d2dsurf);

    D3D10Factory::Device()->CopyResource(texture, d2dsurf->surface);
    D3D10Factory::Device()->CopySubresourceRegion(d2dsurf->surface,
						  0,
						  point.x,
						  point.y,
						  0,
						  texture,
						  0,
						  &rect);

}

cairo_bool_t
cairo_d2d_has_support()
{
    /**
     * FIXME: We should be able to fix this in the near future when we pass in
     * a cairo_device_t to our surface creation functions.
     */
    if (!D3D10Factory::Device() || !D2DSurfFactory::Instance()) {
	return false;
    }
    return true;
}

HDC
cairo_d2d_get_dc(cairo_surface_t *surface, cairo_bool_t retain_contents)
{
    if (surface->type != CAIRO_SURFACE_TYPE_D2D) {
        return NULL;
    }
    cairo_d2d_surface_t *d2dsurf = reinterpret_cast<cairo_d2d_surface_t*>(surface);

    /* We'll pop the clip here manually so that we'll stay in drawing state if we
     * already are, we need to ensure d2dsurf->isDrawing manually then though 
     */

    /* Clips aren't allowed as per MSDN docs */
    reset_clip(d2dsurf);

    if (!d2dsurf->isDrawing) {
      /* GetDC must be called between BeginDraw/EndDraw */
      d2dsurf->rt->BeginDraw();
      d2dsurf->isDrawing = true;
    }

    RefPtr<ID2D1GdiInteropRenderTarget> interopRT;

    d2dsurf->rt->QueryInterface(&interopRT);

    HDC retval;
    HRESULT rv;

    rv = interopRT->GetDC(retain_contents ? D2D1_DC_INITIALIZE_MODE_COPY :
	D2D1_DC_INITIALIZE_MODE_CLEAR, &retval);

    if (FAILED(rv)) {
	return NULL;
    }

    return retval;
}

void
cairo_d2d_release_dc(cairo_surface_t *surface, const cairo_rectangle_int_t *updated_rect)
{
    if (surface->type != CAIRO_SURFACE_TYPE_D2D) {
        return;
    }
    cairo_d2d_surface_t *d2dsurf = reinterpret_cast<cairo_d2d_surface_t*>(surface);

    RefPtr<ID2D1GdiInteropRenderTarget> interopRT;

    d2dsurf->rt->QueryInterface(&interopRT);

    if (!updated_rect) {
	interopRT->ReleaseDC(NULL);
	return;
    }
    
    RECT r;
    r.left = updated_rect->x;
    r.top = updated_rect->y;
    r.right = r.left + updated_rect->width;
    r.bottom = r.top + updated_rect->height;

    interopRT->ReleaseDC(&r);
}

int
cairo_d2d_get_image_surface_cache_usage()
{
  return _cairo_atomic_int_get(&cache_usage);
}
