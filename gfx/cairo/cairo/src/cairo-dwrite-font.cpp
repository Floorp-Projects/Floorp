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

extern "C" {
#include "cairoint.h"

#include "cairo-win32-private.h"
#include "cairo-surface-private.h"
#include "cairo-clip-private.h"
}
#include "cairo-d2d-private.h"
#include "cairo-dwrite-private.h"
#include <float.h>

typedef HRESULT (WINAPI*D2D1CreateFactoryFunc)(
    __in D2D1_FACTORY_TYPE factoryType,
    __in REFIID iid,
    __in_opt CONST D2D1_FACTORY_OPTIONS *pFactoryOptions,
    __out void **factory
);

#define CAIRO_INT_STATUS_SUCCESS (cairo_int_status_t)CAIRO_STATUS_SUCCESS

// Forward declarations
cairo_int_status_t
_dwrite_draw_glyphs_to_gdi_surface_d2d(cairo_win32_surface_t *surface,
				       DWRITE_MATRIX *transform,
				       DWRITE_GLYPH_RUN *run,
				       COLORREF color,
				       const RECT &area);

class D2DFactory
{
public:
    static ID2D1Factory *Instance()
    {
	if (!mFactoryInstance) {
	    D2D1CreateFactoryFunc createD2DFactory = (D2D1CreateFactoryFunc)
		GetProcAddress(LoadLibraryW(L"d2d1.dll"), "D2D1CreateFactory");
	    if (createD2DFactory) {
		D2D1_FACTORY_OPTIONS options;
		options.debugLevel = D2D1_DEBUG_LEVEL_NONE;
		createD2DFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
				 __uuidof(ID2D1Factory),
				 &options,
				 (void**)&mFactoryInstance);
	    }
	}
	return mFactoryInstance;
    }

    static ID2D1DCRenderTarget *RenderTarget()
    {
	if (!mRenderTarget) {
	    if (!Instance()) {
		return NULL;
	    }
	    // Create a DC render target.
	    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(
		    DXGI_FORMAT_B8G8R8A8_UNORM,
		    D2D1_ALPHA_MODE_PREMULTIPLIED),
		0,
		0,
		D2D1_RENDER_TARGET_USAGE_NONE,
		D2D1_FEATURE_LEVEL_DEFAULT
		);

	    Instance()->CreateDCRenderTarget(&props, &mRenderTarget);
	}
	return mRenderTarget;
    }

private:
    static ID2D1Factory *mFactoryInstance;
    static ID2D1DCRenderTarget *mRenderTarget;
};

IDWriteFactory *DWriteFactory::mFactoryInstance = NULL;
IDWriteFontCollection *DWriteFactory::mSystemCollection = NULL;
ID2D1Factory *D2DFactory::mFactoryInstance = NULL;
ID2D1DCRenderTarget *D2DFactory::mRenderTarget = NULL;

/* cairo_font_face_t implementation */
struct _cairo_dwrite_font_face {
    cairo_font_face_t base;
    IDWriteFont *font;
    IDWriteFontFace *dwriteface;
};
typedef struct _cairo_dwrite_font_face cairo_dwrite_font_face_t;

/* cairo_scaled_font_t implementation */
struct _cairo_dwrite_scaled_font {
    cairo_scaled_font_t base;
    DWRITE_MATRIX dwritematrix;
    cairo_matrix_t ctm_inverse;
};
typedef struct _cairo_dwrite_scaled_font cairo_dwrite_scaled_font_t;

/* Functions cairo_font_face_backend_t */
static cairo_status_t
_cairo_dwrite_font_face_create_for_toy (cairo_toy_font_face_t   *toy_face,
					cairo_font_face_t      **font_face);
static void
_cairo_dwrite_font_face_destroy (void *font_face);

static cairo_status_t
_cairo_dwrite_font_face_scaled_font_create (void			*abstract_face,
					    const cairo_matrix_t	*font_matrix,
					    const cairo_matrix_t	*ctm,
					    const cairo_font_options_t *options,
					    cairo_scaled_font_t **font);

const cairo_font_face_backend_t _cairo_dwrite_font_face_backend = {
    CAIRO_FONT_TYPE_DWRITE,
    _cairo_dwrite_font_face_create_for_toy,
    _cairo_dwrite_font_face_destroy,
    _cairo_dwrite_font_face_scaled_font_create
};

/* Functions cairo_scaled_font_backend_t */

void _cairo_dwrite_scaled_font_fini(void *scaled_font);

cairo_warn cairo_int_status_t
_cairo_dwrite_scaled_glyph_init(void			     *scaled_font,
				cairo_scaled_glyph_t	     *scaled_glyph,
				cairo_scaled_glyph_info_t    info);

cairo_warn cairo_int_status_t
_cairo_dwrite_scaled_show_glyphs(void			*scaled_font,
				 cairo_operator_t	 op,
				 const cairo_pattern_t	*pattern,
				 cairo_surface_t	*surface,
				 int			 source_x,
				 int			 source_y,
				 int			 dest_x,
				 int			 dest_y,
				 unsigned int		 width,
				 unsigned int		 height,
				 cairo_glyph_t		*glyphs,
				 int			 num_glyphs,
				 int			*remaining_glyphs);

cairo_int_status_t
_cairo_dwrite_load_truetype_table(void		       *scaled_font,
				  unsigned long         tag,
				  long                  offset,
				  unsigned char        *buffer,
				  unsigned long        *length);

unsigned long
_cairo_dwrite_ucs4_to_index(void			     *scaled_font,
			    uint32_t			ucs4);

const cairo_scaled_font_backend_t _cairo_dwrite_scaled_font_backend = {
    CAIRO_FONT_TYPE_DWRITE,
    _cairo_dwrite_scaled_font_fini,
    _cairo_dwrite_scaled_glyph_init,
    NULL,
    _cairo_dwrite_ucs4_to_index,
    _cairo_dwrite_scaled_show_glyphs,
    _cairo_dwrite_load_truetype_table,
    NULL,
};

/* Helper functions for cairo_dwrite_scaled_glyph_init */
cairo_int_status_t 
_cairo_dwrite_scaled_font_init_glyph_metrics 
    (cairo_dwrite_scaled_font_t *scaled_font, cairo_scaled_glyph_t *scaled_glyph);

cairo_int_status_t 
_cairo_dwrite_scaled_font_init_glyph_surface
    (cairo_dwrite_scaled_font_t *scaled_font, cairo_scaled_glyph_t *scaled_glyph);

cairo_int_status_t 
_cairo_dwrite_scaled_font_init_glyph_path
    (cairo_dwrite_scaled_font_t *scaled_font, cairo_scaled_glyph_t *scaled_glyph);

/* implement the font backend interface */

static cairo_status_t
_cairo_dwrite_font_face_create_for_toy (cairo_toy_font_face_t   *toy_face,
					cairo_font_face_t      **font_face)
{
    uint16_t *face_name;
    int face_name_len;
    cairo_status_t status;

    if (!DWriteFactory::Instance()) {
	return (cairo_status_t)CAIRO_INT_STATUS_UNSUPPORTED;
    }

    status = _cairo_utf8_to_utf16 (toy_face->family, -1,
				   &face_name, &face_name_len);

    IDWriteFontFamily *family = DWriteFactory::FindSystemFontFamily(face_name);
    if (!family) {
	*font_face = (cairo_font_face_t*)&_cairo_font_face_nil;
	return CAIRO_STATUS_FONT_TYPE_MISMATCH;
    }
    free (face_name);

    DWRITE_FONT_WEIGHT weight;
    switch (toy_face->weight) {
    case CAIRO_FONT_WEIGHT_BOLD:
	weight = DWRITE_FONT_WEIGHT_BOLD;
	break;
    case CAIRO_FONT_WEIGHT_NORMAL:
    default:
	weight = DWRITE_FONT_WEIGHT_NORMAL;
	break;
    }

    DWRITE_FONT_STYLE style;
    switch (toy_face->slant) {
    case CAIRO_FONT_SLANT_ITALIC:
	style = DWRITE_FONT_STYLE_ITALIC;
	break;
    case CAIRO_FONT_SLANT_OBLIQUE:
	style = DWRITE_FONT_STYLE_OBLIQUE;
	break;
    case CAIRO_FONT_SLANT_NORMAL:
    default:
	style = DWRITE_FONT_STYLE_NORMAL;
	break;
    }

    cairo_dwrite_font_face_t *face = (cairo_dwrite_font_face_t*)malloc(sizeof(cairo_dwrite_font_face_t));
    HRESULT hr = family->GetFirstMatchingFont(weight, DWRITE_FONT_STRETCH_NORMAL, style, &face->font);
    if (SUCCEEDED(hr)) {
	// Cannot use C++ style new since cairo deallocates this.
	*font_face = (cairo_font_face_t*)face;
        
	_cairo_font_face_init (&(*(_cairo_dwrite_font_face**)font_face)->base, &_cairo_dwrite_font_face_backend);
    } else {
	free(face);
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_dwrite_font_face_destroy (void *font_face)
{
    cairo_dwrite_font_face_t *dwrite_font_face = static_cast<cairo_dwrite_font_face_t*>(font_face);
    if (dwrite_font_face->dwriteface)
	dwrite_font_face->dwriteface->Release();
    if (dwrite_font_face->font)
	dwrite_font_face->font->Release();
}

static cairo_status_t
_cairo_dwrite_font_face_scaled_font_create (void			*abstract_face,
					    const cairo_matrix_t	*font_matrix,
					    const cairo_matrix_t	*ctm,
					    const cairo_font_options_t  *options,
					    cairo_scaled_font_t **font)
{
    cairo_dwrite_font_face_t *font_face = static_cast<cairo_dwrite_font_face_t*>(abstract_face);

    // Must do malloc and not C++ new, since Cairo frees this.
    cairo_dwrite_scaled_font_t *dwriteFont = (cairo_dwrite_scaled_font_t*)malloc(sizeof(cairo_dwrite_scaled_font_t));
    *font = reinterpret_cast<cairo_scaled_font_t*>(dwriteFont);
    _cairo_scaled_font_init(&dwriteFont->base, &font_face->base, font_matrix, ctm, options, &_cairo_dwrite_scaled_font_backend);

    dwriteFont->dwritematrix.dx = (FLOAT)font_matrix->x0;
    dwriteFont->dwritematrix.dy = (FLOAT)font_matrix->y0;
    dwriteFont->dwritematrix.m11 = (FLOAT)font_matrix->xx;
    dwriteFont->dwritematrix.m12 = (FLOAT)font_matrix->xy;
    dwriteFont->dwritematrix.m21 = (FLOAT)font_matrix->yx;
    dwriteFont->dwritematrix.m22 = (FLOAT)font_matrix->yy;

    cairo_font_extents_t extents;

    DWRITE_FONT_METRICS metrics;
    font_face->dwriteface->GetMetrics(&metrics);

    extents.ascent = (FLOAT)metrics.ascent / metrics.designUnitsPerEm;
    extents.descent = (FLOAT)metrics.descent / metrics.designUnitsPerEm;
    extents.height = (FLOAT)(metrics.ascent + metrics.descent + metrics.lineGap) / metrics.designUnitsPerEm;
    extents.max_x_advance = 14.0;
    extents.max_y_advance = 0.0;
	
    dwriteFont->ctm_inverse = dwriteFont->base.ctm;
    cairo_matrix_invert (&dwriteFont->ctm_inverse);
    /* Change these since we apply the transformation to an inverted
     * y axis.
     */
    dwriteFont->ctm_inverse.xy = - dwriteFont->ctm_inverse.xy;
    dwriteFont->ctm_inverse.yx = - dwriteFont->ctm_inverse.yx;

    return _cairo_scaled_font_set_metrics (*font, &extents);
}

/* Implementation cairo_dwrite_scaled_font_backend_t */
void
_cairo_dwrite_scaled_font_fini(void *scaled_font)
{
}

static cairo_int_status_t
_cairo_dwrite_scaled_glyph_init(void			     *scaled_font,
				cairo_scaled_glyph_t	     *scaled_glyph,
				cairo_scaled_glyph_info_t    info)
{
    cairo_dwrite_scaled_font_t *scaled_dwrite_font = static_cast<cairo_dwrite_scaled_font_t*>(scaled_font);
    cairo_int_status_t status;

    if ((info & CAIRO_SCALED_GLYPH_INFO_METRICS) != 0) {
	status = _cairo_dwrite_scaled_font_init_glyph_metrics (scaled_dwrite_font, scaled_glyph);
	if (status)
	    return status;
    }

    if (info & CAIRO_SCALED_GLYPH_INFO_SURFACE) {
	status = _cairo_dwrite_scaled_font_init_glyph_surface (scaled_dwrite_font, scaled_glyph);
	if (status)
	    return status;
    }

    if ((info & CAIRO_SCALED_GLYPH_INFO_PATH) != 0) {
	status = _cairo_dwrite_scaled_font_init_glyph_path (scaled_dwrite_font, scaled_glyph);
	if (status)
	    return status;
    }

    return CAIRO_INT_STATUS_SUCCESS;
}

unsigned long
_cairo_dwrite_ucs4_to_index(void			     *scaled_font,
			    uint32_t		      ucs4)
{
    cairo_dwrite_scaled_font_t *dwritesf = static_cast<cairo_dwrite_scaled_font_t*>(scaled_font);
    cairo_dwrite_font_face_t *face = reinterpret_cast<cairo_dwrite_font_face_t*>(dwritesf->base.font_face);

    UINT16 index;
    face->dwriteface->GetGlyphIndicesA(&ucs4, 1, &index);
    return index;
}

cairo_warn cairo_int_status_t
_cairo_dwrite_scaled_show_glyphs(void			*scaled_font,
				 cairo_operator_t	 op,
				 const cairo_pattern_t	*pattern,
				 cairo_surface_t	*generic_surface,
				 int			 source_x,
				 int			 source_y,
				 int			 dest_x,
				 int			 dest_y,
				 unsigned int		 width,
				 unsigned int		 height,
				 cairo_glyph_t		*glyphs,
				 int			 num_glyphs,
				 int			*remaining_glyphs)
{
    cairo_win32_surface_t *surface = (cairo_win32_surface_t *)generic_surface;
    cairo_int_status_t status;

    if (width == 0 || height == 0)
	return (cairo_int_status_t)CAIRO_STATUS_SUCCESS;

    if (_cairo_surface_is_win32 (generic_surface) &&
	surface->format == CAIRO_FORMAT_RGB24 &&
	op == CAIRO_OPERATOR_OVER) {

	status = (cairo_int_status_t)cairo_dwrite_show_glyphs_on_surface (surface, op, pattern,
									  glyphs, num_glyphs, 
									  (cairo_scaled_font_t*)scaled_font, NULL);

	return status;
    } else {
	cairo_dwrite_scaled_font_t *dwritesf =
	    static_cast<cairo_dwrite_scaled_font_t*>(scaled_font);
	UINT16 *indices = new UINT16[num_glyphs];
	DWRITE_GLYPH_OFFSET *offsets = new DWRITE_GLYPH_OFFSET[num_glyphs];
	FLOAT *advances = new FLOAT[num_glyphs];
	BOOL transform = FALSE;

	if (dwritesf->base.ctm.xy == 0 && dwritesf->base.ctm.yx == 0 &&
	    dwritesf->base.ctm.xx == 1.0 && dwritesf->base.ctm.yy == 1.0) {
	    for (int i = 0; i < num_glyphs; i++) {
		indices[i] = (WORD) glyphs[i].index;
		offsets[i].ascenderOffset = -(FLOAT)(glyphs[i].y - dest_y);
		offsets[i].advanceOffset = (FLOAT)(glyphs[i].x - dest_x);
		advances[i] = 0.0;
	    }
	} else {
	    transform = TRUE;

	    for (int i = 0; i < num_glyphs; i++) {
		indices[i] = (WORD) glyphs[i].index;
		double x = glyphs[i].x;
		double y = -glyphs[i].y;
		cairo_matrix_transform_point(&dwritesf->ctm_inverse, &x, &y);
		// Since we will multiply by our ctm matrix later for rotation effects
		// and such, adjust positions by the inverse matrix now.
		offsets[i].ascenderOffset = (FLOAT)y;
		offsets[i].advanceOffset = (FLOAT)x;
		advances[i] = 0.0;
	    }
	}
	if (generic_surface->device_transform.xx != 1.0) {
	    int a = 1;
	}
	DWRITE_GLYPH_RUN run;
	run.bidiLevel = 0;
	run.fontEmSize = (FLOAT)dwritesf->base.font_matrix.yy;
	run.fontFace = ((cairo_dwrite_font_face_t*)dwritesf->base.font_face)->dwriteface;
	run.glyphIndices = indices;
	run.glyphCount = num_glyphs;
	run.isSideways = FALSE;
	run.glyphOffsets = offsets;
	run.glyphAdvances = advances;
    	IDWriteGlyphRunAnalysis *analysis;
	if (!transform) {
	    DWriteFactory::Instance()->CreateGlyphRunAnalysis(&run,
							      1.0f,
							      NULL,
							      DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC,
							      DWRITE_MEASURING_MODE_NATURAL,
							      0,
							      0,
							      &analysis);
	} else {
	    DWRITE_MATRIX dwmatrix;
	    dwmatrix.dx = (FLOAT)dwritesf->base.ctm.x0;
	    dwmatrix.dy = (FLOAT)dwritesf->base.ctm.y0;
	    dwmatrix.m11 = (FLOAT)dwritesf->base.ctm.xx;
	    dwmatrix.m12 = (FLOAT)dwritesf->base.ctm.xy;
	    dwmatrix.m21 = (FLOAT)dwritesf->base.ctm.yx;
	    dwmatrix.m22 = (FLOAT)dwritesf->base.ctm.yy;
	    DWriteFactory::Instance()->CreateGlyphRunAnalysis(&run,
							      1.0f,
							      &dwmatrix,
							      DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC,
							      DWRITE_MEASURING_MODE_NATURAL,
							      0,
							      0,
							      &analysis);
	}

	RECT r;
	r.left = 0;
	r.top = 0;
	r.right = width;
	r.bottom = height;

	BYTE *surface = new BYTE[width * height * 3];

	analysis->CreateAlphaTexture(DWRITE_TEXTURE_CLEARTYPE_3x1, &r, surface, width * height * 3);

	cairo_image_surface_t *mask_surface = 
	    (cairo_image_surface_t*)cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

	cairo_surface_flush(&mask_surface->base);

	for (unsigned int y = 0; y < height; y++) {
	    for (unsigned int x = 0; x < width; x++) {
		mask_surface->data[y * mask_surface->stride + x * 4] = surface[y * width * 3 + x * 3 + 2];
		mask_surface->data[y * mask_surface->stride + x * 4 + 1] = surface[y * width * 3 + x * 3 + 1];
		mask_surface->data[y * mask_surface->stride + x * 4 + 2] = surface[y * width * 3 + x * 3];
		mask_surface->data[y * mask_surface->stride + x * 4 + 3] = surface[y * width * 3 + x * 3 + 2];
	    }
	}
	cairo_surface_mark_dirty(&mask_surface->base);

	pixman_image_set_component_alpha(mask_surface->pixman_image, 1);

	cairo_surface_pattern_t mask;
	_cairo_pattern_init_for_surface (&mask, &mask_surface->base);

	status = (cairo_int_status_t)_cairo_surface_composite (op, pattern,
							       &mask.base,
							       generic_surface,
							       source_x, source_y,
							       0, 0,
							       dest_x, dest_y,
							       width, height);

	_cairo_pattern_fini (&mask.base);

	analysis->Release();
	delete [] surface;
	delete [] indices;
	delete [] offsets;
	delete [] advances;

	cairo_surface_destroy (&mask_surface->base);
	*remaining_glyphs = 0;

	return (cairo_int_status_t)CAIRO_STATUS_SUCCESS;
    }
}

/* cairo_dwrite_scaled_glyph_init helper function bodies */
cairo_int_status_t 
_cairo_dwrite_scaled_font_init_glyph_metrics(cairo_dwrite_scaled_font_t *scaled_font, 
					     cairo_scaled_glyph_t *scaled_glyph)
{
    UINT16 charIndex = (UINT16)_cairo_scaled_glyph_index (scaled_glyph);
    cairo_dwrite_font_face_t *font_face = (cairo_dwrite_font_face_t*)scaled_font->base.font_face;
    cairo_text_extents_t extents;

    DWRITE_GLYPH_METRICS metrics;
    DWRITE_FONT_METRICS fontMetrics;
    font_face->dwriteface->GetMetrics(&fontMetrics);
    HRESULT hr = font_face->dwriteface->GetDesignGlyphMetrics(&charIndex, 1, &metrics);
    if (FAILED(hr)) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    // TODO: Treat swap_xy.
    extents.width = (FLOAT)(metrics.advanceWidth - metrics.leftSideBearing - metrics.rightSideBearing) / fontMetrics.designUnitsPerEm;
    extents.height = (FLOAT)(metrics.advanceHeight - metrics.topSideBearing - metrics.bottomSideBearing) / fontMetrics.designUnitsPerEm;
    extents.x_advance = (FLOAT)metrics.advanceWidth / fontMetrics.designUnitsPerEm;
    extents.x_bearing = (FLOAT)metrics.leftSideBearing / fontMetrics.designUnitsPerEm;
    extents.y_advance = 0.0;
    extents.y_bearing = (FLOAT)(metrics.topSideBearing - metrics.verticalOriginY) / fontMetrics.designUnitsPerEm;
    _cairo_scaled_glyph_set_metrics (scaled_glyph,
				     &scaled_font->base,
				     &extents);
    return CAIRO_INT_STATUS_SUCCESS;
}

/**
 * Stack-based helper implementing IDWriteGeometrySink.
 * Used to determine the path of the glyphs.
 */

class GeometryRecorder : public IDWriteGeometrySink
{
public:
    GeometryRecorder(cairo_path_fixed_t *aCairoPath) 
	: mCairoPath(aCairoPath) {}

    // IUnknown interface
    IFACEMETHOD(QueryInterface)(IID const& iid, OUT void** ppObject)
    {
        if (iid != __uuidof(IDWriteGeometrySink))
            return E_NOINTERFACE;

	*ppObject = static_cast<IDWriteGeometrySink*>(this);

        return S_OK;
    }

    IFACEMETHOD_(ULONG, AddRef)()
    {
        return 1;
    }

    IFACEMETHOD_(ULONG, Release)()
    {
        return 1;
    }

    IFACEMETHODIMP_(void) SetFillMode(D2D1_FILL_MODE fillMode)
    {
	return;
    }

    STDMETHODIMP Close()
    {
	return S_OK;
    }

    IFACEMETHODIMP_(void) SetSegmentFlags(D2D1_PATH_SEGMENT vertexFlags)
    {
	return;
    }
    
    cairo_fixed_t GetFixedX(const D2D1_POINT_2F &point)
    {
	unsigned int control_word;
	_controlfp_s(&control_word, _CW_DEFAULT, MCW_PC);
	return _cairo_fixed_from_double(point.x);
    }

    cairo_fixed_t GetFixedY(const D2D1_POINT_2F &point)
    {
	unsigned int control_word;
	_controlfp_s(&control_word, _CW_DEFAULT, MCW_PC);
	return _cairo_fixed_from_double(point.y);
    }

    IFACEMETHODIMP_(void) BeginFigure(
	D2D1_POINT_2F startPoint, 
	D2D1_FIGURE_BEGIN figureBegin) 
    {
	mStartPoint = startPoint;
	cairo_status_t status = _cairo_path_fixed_move_to(mCairoPath, 
							  GetFixedX(startPoint),
							  GetFixedY(startPoint));
    }

    IFACEMETHODIMP_(void) EndFigure(    
	D2D1_FIGURE_END figureEnd) 
    {
	if (figureEnd == D2D1_FIGURE_END_CLOSED) {
	    cairo_status_t status = _cairo_path_fixed_line_to(mCairoPath,
							      GetFixedX(mStartPoint), 
							      GetFixedY(mStartPoint));
	}
    }

    IFACEMETHODIMP_(void) AddBeziers(
	const D2D1_BEZIER_SEGMENT *beziers,
        UINT beziersCount)
    {
	for (unsigned int i = 0; i < beziersCount; i++) {
	    cairo_status_t status = _cairo_path_fixed_curve_to(mCairoPath,
							       GetFixedX(beziers[i].point1),
							       GetFixedY(beziers[i].point1),
							       GetFixedX(beziers[i].point2),
							       GetFixedY(beziers[i].point2),
							       GetFixedX(beziers[i].point3),
							       GetFixedY(beziers[i].point3));
	}	
    }

    IFACEMETHODIMP_(void) AddLines(
	const D2D1_POINT_2F *points,
        UINT pointsCount)
    {
	for (unsigned int i = 0; i < pointsCount; i++) {
	    cairo_status_t status = _cairo_path_fixed_line_to(mCairoPath, 
		GetFixedX(points[i]), 
		GetFixedY(points[i]));
	}
    }

private:
    cairo_path_fixed_t *mCairoPath;
    D2D1_POINT_2F mStartPoint;
};

cairo_int_status_t 
_cairo_dwrite_scaled_font_init_glyph_path(cairo_dwrite_scaled_font_t *scaled_font, 
				          cairo_scaled_glyph_t *scaled_glyph)
{
    cairo_path_fixed_t *path;
    path = _cairo_path_fixed_create();
    GeometryRecorder recorder(path);

    DWRITE_GLYPH_OFFSET offset;
    offset.advanceOffset = 0;
    offset.ascenderOffset = 0;
    UINT16 glyphId = (UINT16)_cairo_scaled_glyph_index(scaled_glyph);
    FLOAT advance = 0.0;
    cairo_dwrite_font_face_t *dwriteff = (cairo_dwrite_font_face_t*)scaled_font->base.font_face;
    dwriteff->dwriteface->GetGlyphRunOutline((FLOAT)scaled_font->base.font_matrix.yy,
					     &glyphId,
					     &advance,
					     &offset,
					     1,
					     FALSE,
					     FALSE,
					     &recorder);
    _cairo_path_fixed_close_path(path);

    /* Now apply our transformation to the drawn path. */
    _cairo_path_fixed_transform(path, &scaled_font->base.ctm);
    
    _cairo_scaled_glyph_set_path (scaled_glyph,
				  &scaled_font->base,
				  path);
    return CAIRO_INT_STATUS_SUCCESS;
}

/* Helper function also stolen from cairo-win32-font.c */

/* Compute an alpha-mask from a monochrome RGB24 image
 */
static cairo_surface_t *
_compute_a8_mask (cairo_win32_surface_t *mask_surface)
{
    cairo_image_surface_t *image24 = (cairo_image_surface_t *)mask_surface->image;
    cairo_image_surface_t *image8;
    int i, j;

    if (image24->base.status)
	return cairo_surface_reference (&image24->base);

    image8 = (cairo_image_surface_t *)cairo_image_surface_create (CAIRO_FORMAT_A8,
								  image24->width, image24->height);
    if (image8->base.status)
	return &image8->base;

    for (i = 0; i < image24->height; i++) {
	uint32_t *p = (uint32_t *) (image24->data + i * image24->stride);
	unsigned char *q = (unsigned char *) (image8->data + i * image8->stride);

	for (j = 0; j < image24->width; j++) {
	    *q = 255 - ((*p & 0x0000ff00) >> 8);
	    p++;
	    q++;
	}
    }

    return &image8->base;
}

cairo_int_status_t 
_cairo_dwrite_scaled_font_init_glyph_surface(cairo_dwrite_scaled_font_t *scaled_font, 
					     cairo_scaled_glyph_t	*scaled_glyph)
{
    cairo_int_status_t status;
    cairo_glyph_t glyph;
    cairo_win32_surface_t *surface;
    cairo_t *cr;
    cairo_surface_t *image;
    int width, height;
    double x1, y1, x2, y2;

    x1 = _cairo_fixed_integer_floor (scaled_glyph->bbox.p1.x);
    y1 = _cairo_fixed_integer_floor (scaled_glyph->bbox.p1.y);
    x2 = _cairo_fixed_integer_ceil (scaled_glyph->bbox.p2.x);
    y2 = _cairo_fixed_integer_ceil (scaled_glyph->bbox.p2.y);
    width = (int)(x2 - x1);
    height = (int)(y2 - y1);

    glyph.index = _cairo_scaled_glyph_index (scaled_glyph);
    glyph.x = -x1;
    glyph.y = -y1;

    surface = (cairo_win32_surface_t *)
	cairo_win32_surface_create_with_dib (CAIRO_FORMAT_RGB24, width, height);

    cr = cairo_create (&surface->base);
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_paint (cr);
    status = (cairo_int_status_t)cairo_status (cr);
    cairo_destroy(cr);
    if (status)
	goto FAIL;

    DWRITE_GLYPH_RUN run;
    FLOAT advance = 0;
    UINT16 index = (UINT16)glyph.index;
    DWRITE_GLYPH_OFFSET offset;
    double x = glyph.x;
    double y = -glyph.y;
    cairo_matrix_transform_point(&scaled_font->ctm_inverse, &x, &y);
    offset.advanceOffset = (FLOAT)x;
    offset.ascenderOffset = (FLOAT)y;

    RECT area;
    area.top = 0;
    area.bottom = height;
    area.left = 0;
    area.right = width;

    run.glyphCount = 1;
    run.glyphAdvances = &advance;
    run.fontFace = ((cairo_dwrite_font_face_t*)scaled_font->base.font_face)->dwriteface;
    run.fontEmSize = (FLOAT)scaled_font->base.font_matrix.yy;
    run.bidiLevel = 0;
    run.glyphIndices = &index;
    run.isSideways = FALSE;
    run.glyphOffsets = &offset;

    DWRITE_MATRIX matrix;
    matrix.m11 = (FLOAT)scaled_font->base.ctm.xx;
    matrix.m12 = (FLOAT)scaled_font->base.ctm.yx;
    matrix.m21 = (FLOAT)scaled_font->base.ctm.xy;
    matrix.m22 = (FLOAT)scaled_font->base.ctm.yy;
    matrix.dx = (FLOAT)scaled_font->base.ctm.x0;
    matrix.dy = (FLOAT)scaled_font->base.ctm.y0;

    status = _dwrite_draw_glyphs_to_gdi_surface_d2d (surface, &matrix, &run, RGB(0,0,0), area);
    if (status)
	goto FAIL;

    GdiFlush();

    image = _compute_a8_mask (surface);
    status = (cairo_int_status_t)image->status;
    if (status)
	goto FAIL;

    cairo_surface_set_device_offset (image, -x1, -y1);
    _cairo_scaled_glyph_set_surface (scaled_glyph,
                                     &scaled_font->base,
                                     (cairo_image_surface_t *) image);

  FAIL:
    cairo_surface_destroy (&surface->base);

    return status;
}

cairo_int_status_t
_cairo_dwrite_load_truetype_table(void		   *scaled_font,
				  unsigned long         tag,
				  long                  offset,
				  unsigned char        *buffer,
				  unsigned long        *length)
{
    cairo_dwrite_scaled_font_t *dwritesf = static_cast<cairo_dwrite_scaled_font_t*>(scaled_font);
    cairo_dwrite_font_face_t *face = reinterpret_cast<cairo_dwrite_font_face_t*>(dwritesf->base.font_face);

    const void *data;
    UINT32 size;
    void *tableContext;
    BOOL exists;
    face->dwriteface->TryGetFontTable(tag,
				      &data,
				      &size,
				      &tableContext,
				      &exists);

    if (!exists) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    *length = size;
    if (buffer) {
	memcpy(buffer, data, size);
    }
    if (tableContext) {
	face->dwriteface->ReleaseFontTable(tableContext);
    }
    return (cairo_int_status_t)CAIRO_STATUS_SUCCESS;
}

// WIN32 Helper Functions
cairo_font_face_t*
cairo_dwrite_font_face_create_for_dwrite_fontface(void* dwrite_font, void* dwrite_font_face)
{
    IDWriteFont *dwritefont = static_cast<IDWriteFont*>(dwrite_font);
    IDWriteFontFace *dwriteface = static_cast<IDWriteFontFace*>(dwrite_font_face);
    cairo_dwrite_font_face_t *face = new cairo_dwrite_font_face_t;
    cairo_font_face_t *font_face;

    dwriteface->AddRef();
    
    face->dwriteface = dwriteface;
    face->font = NULL;

    font_face = (cairo_font_face_t*)face;
    
    _cairo_font_face_init (&((cairo_dwrite_font_face_t*)font_face)->base, &_cairo_dwrite_font_face_backend);

    return font_face;
}

cairo_int_status_t
_dwrite_draw_glyphs_to_gdi_surface_gdi(cairo_win32_surface_t *surface,
				       DWRITE_MATRIX *transform,
				       DWRITE_GLYPH_RUN *run,
				       COLORREF color,
				       const RECT &area)
{
    IDWriteGdiInterop *gdiInterop;
    DWriteFactory::Instance()->GetGdiInterop(&gdiInterop);
    IDWriteBitmapRenderTarget *rt;

    IDWriteRenderingParams *params;
    DWriteFactory::Instance()->CreateRenderingParams(&params);

    gdiInterop->CreateBitmapRenderTarget(surface->dc, 
					 area.right - area.left, 
					 area.bottom - area.top, 
					 &rt);

    if (transform) {
	rt->SetCurrentTransform(transform);
    }
    BitBlt(rt->GetMemoryDC(),
	   0, 0,
	   area.right - area.left, area.bottom - area.top,
	   surface->dc,
	   area.left, area.top, 
	   SRCCOPY | NOMIRRORBITMAP);
    HRESULT hr = rt->DrawGlyphRun(0, 0, DWRITE_MEASURING_MODE_NATURAL, run, params, color);
    BitBlt(surface->dc,
	   area.left, area.top,
	   area.right - area.left, area.bottom - area.top,
	   rt->GetMemoryDC(),
	   0, 0, 
	   SRCCOPY | NOMIRRORBITMAP);
    params->Release();
    rt->Release();
    gdiInterop->Release();
    return CAIRO_INT_STATUS_SUCCESS;
}

cairo_int_status_t
_dwrite_draw_glyphs_to_gdi_surface_d2d(cairo_win32_surface_t *surface,
				       DWRITE_MATRIX *transform,
				       DWRITE_GLYPH_RUN *run,
				       COLORREF color,
				       const RECT &area)
{
    HRESULT rv;

    ID2D1DCRenderTarget *rt = D2DFactory::RenderTarget();

    rv = rt->BindDC(surface->dc, &area);

    printf("Rendering to surface: %p\n", surface->dc);

    if (FAILED(rv)) {
	rt->Release();
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    // D2D uses 0x00RRGGBB not 0x00BBGGRR like COLORREF.
    color = (color & 0xFF) << 16 |
	(color & 0xFF00) |
	(color & 0xFF0000) >> 16;
    ID2D1SolidColorBrush *brush;
    rv = rt->CreateSolidColorBrush(D2D1::ColorF(color, 1.0), &brush);

    if (FAILED(rv)) {
	rt->Release();
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (transform) {
	rt->SetTransform(D2D1::Matrix3x2F(transform->m11,
					  transform->m12,
					  transform->m21,
					  transform->m22,
					  transform->dx,
					  transform->dy));
    }
    rt->BeginDraw();
    rt->DrawGlyphRun(D2D1::Point2F(0, 0), run, brush);
    rt->EndDraw();
    if (transform) {
	rt->SetTransform(D2D1::Matrix3x2F::Identity());
    }
    brush->Release();
    if (FAILED(rv)) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    return CAIRO_INT_STATUS_SUCCESS;
}

/* Surface helper function */
cairo_public cairo_int_status_t
cairo_dwrite_show_glyphs_on_surface(void			*surface,
				    cairo_operator_t	 op,
				    const cairo_pattern_t	*source,
				    cairo_glyph_t		*glyphs,
				    int			 num_glyphs,
				    cairo_scaled_font_t	*scaled_font,
				    cairo_rectangle_int_t	*extents)
{
    // TODO: Check font & surface for types.
    cairo_dwrite_scaled_font_t *dwritesf = reinterpret_cast<cairo_dwrite_scaled_font_t*>(scaled_font);
    cairo_dwrite_font_face_t *dwriteff = reinterpret_cast<cairo_dwrite_font_face_t*>(scaled_font->font_face);
    cairo_win32_surface_t *dst = reinterpret_cast<cairo_win32_surface_t*>(surface);
    cairo_int_status_t status;

    /* If we have a fallback mask clip set on the dst, we have
     * to go through the fallback path, but only if we're not
     * doing this for printing */
    if (dst->base.clip  &&
	(dst->base.clip->mode != CAIRO_CLIP_MODE_REGION ||
	 dst->base.clip->surface != NULL))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* We can only handle dwrite fonts */
    if (cairo_scaled_font_get_type (scaled_font) != CAIRO_FONT_TYPE_DWRITE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* We can only handle opaque solid color sources */
    if (!_cairo_pattern_is_opaque_solid(source))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* We can only handle operator SOURCE or OVER with the destination
     * having no alpha */
    if ((op != CAIRO_OPERATOR_SOURCE && op != CAIRO_OPERATOR_OVER) ||
	(dst->format != CAIRO_FORMAT_RGB24) && (dst->format != CAIRO_FORMAT_ARGB32))
	return CAIRO_INT_STATUS_UNSUPPORTED;


    /* It is vital that dx values for dxy_buf are calculated from the delta of
     * _logical_ x coordinates (not user x coordinates) or else the sum of all
     * previous dx values may start to diverge from the current glyph's x
     * coordinate due to accumulated rounding error. As a result strings could
     * be painted shorter or longer than expected. */

    UINT16 *indices = new UINT16[num_glyphs];
    DWRITE_GLYPH_OFFSET *offsets = new DWRITE_GLYPH_OFFSET[num_glyphs];
    FLOAT *advances = new FLOAT[num_glyphs];
    BOOL transform = FALSE;
    /* Needed to calculate bounding box for efficient blitting */
    INT32 smallestX = INT_MAX;
    INT32 largestX = 0;
    INT32 smallestY = INT_MAX;
    INT32 largestY = 0;
    for (int i = 0; i < num_glyphs; i++) {
	if (glyphs[i].x < smallestX) {
	    smallestX = (INT32)glyphs[i].x;
	}
	if (glyphs[i].x > largestX) {
	    largestX = (INT32)glyphs[i].x;
	}
	if (glyphs[i].y < smallestY) {
	    smallestY = (INT32)glyphs[i].y;
	}
	if (glyphs[i].y > largestY) {
	    largestY = (INT32)glyphs[i].y;
	}
    }
    RECT fontArea;
    fontArea.left = smallestX - (INT32)dwritesf->dwritematrix.m22;
    fontArea.right = largestX + (INT32)dwritesf->dwritematrix.m22 * 2;
    fontArea.top = smallestY - (INT32)dwritesf->dwritematrix.m22;
    fontArea.bottom = largestY + (INT32)dwritesf->dwritematrix.m22 * 2;
    if (fontArea.left < 0)
	fontArea.left = 0;
    if (fontArea.top > 0)
	fontArea.top = 0;
    if (fontArea.bottom > dst->extents.height) {
	fontArea.bottom = dst->extents.height;
    }
    if (fontArea.right > dst->extents.width) {
	fontArea.right = dst->extents.width;
    }
    if (fontArea.right <= fontArea.left ||
	fontArea.bottom <= fontArea.top) {
	return CAIRO_INT_STATUS_SUCCESS;
    }
    if (fontArea.right > dst->extents.width) {
	fontArea.right = dst->extents.width;
    }
    if (fontArea.bottom > dst->extents.height) {
	fontArea.bottom = dst->extents.height;
    }
    if (scaled_font->ctm.xy == 0 && scaled_font->ctm.yx == 0 &&
	scaled_font->ctm.xx == 1.0 && scaled_font->ctm.yy == 1.0) {
	cairo_matrix_t mat = scaled_font->ctm;
	cairo_matrix_invert (&mat);
	for (int i = 0; i < num_glyphs; i++) {
	    indices[i] = (WORD) glyphs[i].index;
	    // Since we will multiply by our ctm matrix later for rotation effects
	    // and such, adjust positions by the inverse matrix now.
	    offsets[i].ascenderOffset = (FLOAT)(fontArea.top - glyphs[i].y);
	    offsets[i].advanceOffset = (FLOAT)(glyphs[i].x - fontArea.left);
	    advances[i] = 0.0;
	}
    } else {
	transform = TRUE;

	for (int i = 0; i < num_glyphs; i++) {
	    indices[i] = (WORD) glyphs[i].index;
	    double x = glyphs[i].x - fontArea.left;
	    double y = fontArea.top - glyphs[i].y;
	    cairo_matrix_transform_point(&dwritesf->ctm_inverse, &x, &y);
	    // Since we will multiply by our ctm matrix later for rotation effects
	    // and such, adjust positions by the inverse matrix now.
	    offsets[i].ascenderOffset = (FLOAT)y;
	    offsets[i].advanceOffset = (FLOAT)x;
	    advances[i] = 0.0;
	}
    }

    DWRITE_GLYPH_RUN run;
    run.bidiLevel = 0;
    run.fontEmSize = (FLOAT)scaled_font->font_matrix.yy;
    run.fontFace = dwriteff->dwriteface;
    run.glyphIndices = indices;
    run.glyphCount = num_glyphs;
    run.isSideways = FALSE;
    run.glyphOffsets = offsets;
    run.glyphAdvances = advances;
    
    cairo_solid_pattern_t *solid_pattern = (cairo_solid_pattern_t *)source;
    COLORREF color = RGB(((int)solid_pattern->color.red_short) >> 8,
		((int)solid_pattern->color.green_short) >> 8,
		((int)solid_pattern->color.blue_short) >> 8);

    DWRITE_MATRIX matrix;
    matrix.m11 = (FLOAT)scaled_font->ctm.xx;
    matrix.m12 = (FLOAT)scaled_font->ctm.yx;
    matrix.m21 = (FLOAT)scaled_font->ctm.xy;
    matrix.m22 = (FLOAT)scaled_font->ctm.yy;
    matrix.dx = (FLOAT)scaled_font->ctm.x0;
    matrix.dy = (FLOAT)scaled_font->ctm.y0;

    DWRITE_MATRIX *mat;
    if (transform) {
	mat = &matrix;
    } else {
	mat = NULL;
    }

    RECT area;
    area.left = dst->extents.x;
    area.top = dst->extents.y;
    area.right = area.left + dst->extents.width;
    area.bottom = area.top + dst->extents.height;

#ifdef CAIRO_TRY_D2D_TO_GDI
    status = _dwrite_draw_glyphs_to_gdi_surface_d2d(dst,
						    mat,
						    &run,
						    color,
						    fontArea);

    if (status == (cairo_status_t)CAIRO_INT_STATUS_UNSUPPORTED) {
#endif
	status = _dwrite_draw_glyphs_to_gdi_surface_gdi(dst,
							mat,
							&run,
							color,
							fontArea);
#ifdef CAIRO_TRY_D2D_TO_GDI
    }
#endif

    delete [] indices;
    delete [] offsets;
    delete [] advances;

    return CAIRO_INT_STATUS_SUCCESS;
}

#if CAIRO_HAS_D2D_SURFACE
/* Surface helper function */
cairo_int_status_t
cairo_dwrite_show_glyphs_on_d2d_surface(void			*surface,
					cairo_operator_t	 op,
					const cairo_pattern_t	*source,
					cairo_glyph_t		*glyphs,
					int			 num_glyphs,
					cairo_scaled_font_t	*scaled_font,
					cairo_rectangle_int_t	*extents)
{
    // TODO: Check font & surface for types.
    cairo_dwrite_scaled_font_t *dwritesf = reinterpret_cast<cairo_dwrite_scaled_font_t*>(scaled_font);
    cairo_dwrite_font_face_t *dwriteff = reinterpret_cast<cairo_dwrite_font_face_t*>(scaled_font->font_face);
    cairo_d2d_surface_t *dst = reinterpret_cast<cairo_d2d_surface_t*>(surface);

    /* We can only handle dwrite fonts */
    if (cairo_scaled_font_get_type (scaled_font) != CAIRO_FONT_TYPE_DWRITE)
	return CAIRO_INT_STATUS_UNSUPPORTED;


    /* We can only handle operator SOURCE or OVER with the destination
     * having no alpha */
    if (op != CAIRO_OPERATOR_SOURCE && op != CAIRO_OPERATOR_OVER)
	return CAIRO_INT_STATUS_UNSUPPORTED;


    D2D1_TEXT_ANTIALIAS_MODE cleartype = D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE;

    if (dst->base.content != CAIRO_CONTENT_COLOR) {
	cleartype = D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE;
    }
    switch (scaled_font->options.antialias) {
	case CAIRO_ANTIALIAS_DEFAULT:
	    dst->rt->SetTextAntialiasMode(cleartype);
	    break;
	case CAIRO_ANTIALIAS_NONE:
	    dst->rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_ALIASED);
	    break;
	case CAIRO_ANTIALIAS_GRAY:
	    dst->rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
	    break;
	case CAIRO_ANTIALIAS_SUBPIXEL:
	    dst->rt->SetTextAntialiasMode(cleartype);
	    break;
    }

    /* It is vital that dx values for dxy_buf are calculated from the delta of
     * _logical_ x coordinates (not user x coordinates) or else the sum of all
     * previous dx values may start to diverge from the current glyph's x
     * coordinate due to accumulated rounding error. As a result strings could
     * be painted shorter or longer than expected. */

    UINT16 *indices = new UINT16[num_glyphs];
    DWRITE_GLYPH_OFFSET *offsets = new DWRITE_GLYPH_OFFSET[num_glyphs];
    FLOAT *advances = new FLOAT[num_glyphs];
    BOOL transform = FALSE;

    if (scaled_font->ctm.xy == 0 && scaled_font->ctm.yx == 0 &&
	scaled_font->ctm.xx == 1.0 && scaled_font->ctm.yy == 1.0) {
	for (int i = 0; i < num_glyphs; i++) {
	    indices[i] = (WORD) glyphs[i].index;
	    // Since we will multiply by our ctm matrix later for rotation effects
	    // and such, adjust positions by the inverse matrix now.
	    offsets[i].ascenderOffset = -(FLOAT)(glyphs[i].y);
	    offsets[i].advanceOffset = (FLOAT)(glyphs[i].x);
	    advances[i] = 0.0;
	}
    } else {
	transform = TRUE;
	// D2D does not invert y axis.
	dwritesf->ctm_inverse.xy = - dwritesf->ctm_inverse.xy;
	dwritesf->ctm_inverse.yx = - dwritesf->ctm_inverse.yx;

	for (int i = 0; i < num_glyphs; i++) {
	    indices[i] = (WORD) glyphs[i].index;
	    double x = glyphs[i].x;
	    double y = glyphs[i].y;
	    cairo_matrix_transform_point(&dwritesf->ctm_inverse, &x, &y);
	    // Since we will multiply by our ctm matrix later for rotation effects
	    // and such, adjust positions by the inverse matrix now.
	    offsets[i].ascenderOffset = -(FLOAT)y;
	    offsets[i].advanceOffset = (FLOAT)x;
	    advances[i] = 0.0;
	}
	dwritesf->ctm_inverse.xy = - dwritesf->ctm_inverse.xy;
	dwritesf->ctm_inverse.yx = - dwritesf->ctm_inverse.yx;
    }

    DWRITE_GLYPH_RUN run;
    run.bidiLevel = 0;
    run.fontEmSize = (FLOAT)scaled_font->font_matrix.yy;
    run.fontFace = dwriteff->dwriteface;
    run.glyphIndices = indices;
    run.glyphCount = num_glyphs;
    run.isSideways = FALSE;
    run.glyphOffsets = offsets;
    run.glyphAdvances = advances;
    

    D2D1::Matrix3x2F mat((FLOAT)scaled_font->ctm.xx,
			 (FLOAT)scaled_font->ctm.yx,
			 (FLOAT)scaled_font->ctm.xy,
			 (FLOAT)scaled_font->ctm.yy,
			 (FLOAT)scaled_font->ctm.x0,
			 (FLOAT)scaled_font->ctm.y0);
	
    if (transform) {
	dst->rt->SetTransform(mat);
    }
    unsigned int last_run = 0;
    unsigned int runs_remaining = 1;
    bool pushed_clip = false;

    while (runs_remaining) {
	ID2D1Brush *brush = _cairo_d2d_create_brush_for_pattern(dst,
								source,
								last_run++,
								&runs_remaining,
								&pushed_clip);
	if (!brush) {
	    delete [] indices;
	    delete [] offsets;
	    delete [] advances;
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
        dst->rt->DrawGlyphRun(D2D1::Point2F(0, 0), &run, brush);
	brush->Release();
	if (pushed_clip) {
	    dst->rt->PopLayer();
	}
    }
    if (transform) {
	dst->rt->SetTransform(D2D1::Matrix3x2F::Identity());
    }

    delete [] indices;
    delete [] offsets;
    delete [] advances;

    return CAIRO_INT_STATUS_SUCCESS;
}

#endif
