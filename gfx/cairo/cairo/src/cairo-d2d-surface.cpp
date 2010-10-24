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

#define CAIRO_INT_STATUS_SUCCESS (cairo_int_status_t)CAIRO_STATUS_SUCCESS

struct Vertex
{
    float position[2];
};

// This factory is not device dependent, we can store it. But will clear it
// if there are no devices left needing it.
static ID2D1Factory *sD2DFactory = NULL;
static HMODULE sD2DModule;

static void
_cairo_d2d_release_factory()
{
    int refcnt = sD2DFactory->Release();
    if (!refcnt) {
	// Once the last reference goes, free the library.
	sD2DFactory = NULL;
	FreeLibrary(sD2DModule);
    }
}

/**
 * Set a blending mode for an operator. This will also return a boolean that
 * reports if for this blend mode the entire surface needs to be blended. This
 * is true whenever the DEST blend is not ONE when src alpha is 0.
 */
static cairo_int_status_t
_cairo_d2d_set_operator(cairo_d2d_device_t *device,
			cairo_operator_t op)
{
    assert(op < MAX_OPERATORS);
    if (op >= MAX_OPERATORS) {
	// Eep! Someone forgot to update MAX_OPERATORS probably.
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (device->mBlendStates[op]) {
	device->mD3D10Device->OMSetBlendState(device->mBlendStates[op], NULL, 0xffffffff);
	return CAIRO_INT_STATUS_SUCCESS;
    }

    D3D10_BLEND_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.BlendEnable[0] = TRUE;
    desc.AlphaToCoverageEnable = FALSE;
    desc.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;

    switch (op) {
	case CAIRO_OPERATOR_OVER:
	    desc.BlendOp = desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
	    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
	    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_ONE;
	    break;
	case CAIRO_OPERATOR_ADD:
	    desc.BlendOp = desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
	    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_ONE;
	    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_ONE;
	    break;
	case CAIRO_OPERATOR_IN:
	    desc.BlendOp = desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
	    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_ZERO;
	    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_DEST_ALPHA;
	    break;
	case CAIRO_OPERATOR_OUT:
	    desc.BlendOp = desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
	    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_ZERO;
	    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_INV_DEST_ALPHA;
	    break;
	case CAIRO_OPERATOR_ATOP:
	    desc.BlendOp = desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
	    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
	    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_DEST_ALPHA;
	    break;
	case CAIRO_OPERATOR_DEST:
	    desc.BlendOp = desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
	    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_ONE;
	    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_ZERO;
	    break;
	case CAIRO_OPERATOR_DEST_OVER:
	    desc.BlendOp = desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
	    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_ONE;
	    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_INV_DEST_ALPHA;
	    break;
	case CAIRO_OPERATOR_DEST_IN:
	    desc.BlendOp = desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
	    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_SRC_ALPHA;
	    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_ZERO;
	    break;
	case CAIRO_OPERATOR_DEST_OUT:
	    desc.BlendOp = desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
	    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
	    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_ZERO;
	    break;
	case CAIRO_OPERATOR_DEST_ATOP:
	    desc.BlendOp = desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
	    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_SRC_ALPHA;
	    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_INV_DEST_ALPHA;
	    break;
	case CAIRO_OPERATOR_XOR:
	    desc.BlendOp = desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
	    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
	    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_INV_DEST_ALPHA;
	    break;
	default:
	    return CAIRO_INT_STATUS_UNSUPPORTED;
    };
    device->mD3D10Device->CreateBlendState(&desc, &device->mBlendStates[op]);

    device->mD3D10Device->OMSetBlendState(device->mBlendStates[op], NULL, 0xffffffff);
    return CAIRO_INT_STATUS_SUCCESS;
}

cairo_device_t *
cairo_d2d_create_device_from_d3d10device(ID3D10Device1 *d3d10device)
{
    HRESULT hr;
    D3D10_RASTERIZER_DESC rastDesc;
    D3D10_INPUT_ELEMENT_DESC layout[] =
    {
	{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3D10_PASS_DESC passDesc;
    ID3D10EffectTechnique *technique;
    Vertex vertices[] = { {0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {1.0, 1.0} };
    CD3D10_BUFFER_DESC bufferDesc(sizeof(vertices), D3D10_BIND_VERTEX_BUFFER);
    D3D10_SUBRESOURCE_DATA data;

    cairo_d2d_device_t *device = new cairo_d2d_device_t;

    device->mD3D10Device = d3d10device;

    device->mD3D10_1 = LoadLibraryA("d3d10_1.dll");
    D3D10CreateEffectFromMemoryFunc createEffect = (D3D10CreateEffectFromMemoryFunc)
	GetProcAddress(device->mD3D10_1, "D3D10CreateEffectFromMemory");
    D2D1CreateFactoryFunc createD2DFactory;

    if (!createEffect) {
	goto FAILED;
    }

    if (!sD2DFactory) {
	sD2DModule = LoadLibraryW(L"d2d1.dll");
	createD2DFactory = (D2D1CreateFactoryFunc)
	    GetProcAddress(sD2DModule, "D2D1CreateFactory");
	if (!createD2DFactory) {
	    goto FAILED;
	}
	D2D1_FACTORY_OPTIONS options;
#ifdef DEBUG
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#else
	options.debugLevel = D2D1_DEBUG_LEVEL_NONE;
#endif
	hr = createD2DFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
			      __uuidof(ID2D1Factory),
			      &options,
			      (void**)&sD2DFactory);
	if (FAILED(hr)) {
	    goto FAILED;
	}
    } else {
	sD2DFactory->AddRef();
    }

    device->mD3D10Device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP);
    createEffect((void*)g_main, sizeof(g_main), 0, device->mD3D10Device, NULL, &device->mSampleEffect);

    technique = device->mSampleEffect->GetTechniqueByName("SampleTexture");
    technique->GetPassByIndex(0)->GetDesc(&passDesc);


    hr = device->mD3D10Device->CreateInputLayout(layout,
						 sizeof(layout) / sizeof(D3D10_INPUT_ELEMENT_DESC),
						 passDesc.pIAInputSignature,
						 passDesc.IAInputSignatureSize,
						 &device->mInputLayout);
    if (FAILED(hr)) {
	goto FAILED;
    }

    data.pSysMem = (void*)vertices;
    hr = device->mD3D10Device->CreateBuffer(&bufferDesc, &data, &device->mQuadBuffer);
    if (FAILED(hr)) {
	goto FAILED;
    }

    memset(&rastDesc, 0, sizeof(rastDesc));
    rastDesc.CullMode = D3D10_CULL_NONE;
    rastDesc.FillMode = D3D10_FILL_SOLID;
    rastDesc.DepthClipEnable = TRUE;
    hr = device->mD3D10Device->CreateRasterizerState(&rastDesc, &device->mRasterizerState);
    if (FAILED(hr)) {
	goto FAILED;
    }
    device->base.refcount = 1;
    device->mVRAMUsage = 0;

    return &device->base;
FAILED:
    delete &device->base;
    return NULL;
}

cairo_device_t *
cairo_d2d_create_device()
{
    HMODULE d3d10module = LoadLibraryA("d3d10_1.dll");
    D3D10CreateDevice1Func createD3DDevice = (D3D10CreateDevice1Func)
	GetProcAddress(d3d10module, "D3D10CreateDevice1");

    if (!createD3DDevice) {
	return NULL;
    }

    RefPtr<ID3D10Device1> d3ddevice;

    /**
     * On usage of D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS:
     * documentation on D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS
     * can be misleading. In fact, that flag gives no such indication. I pointed this
     * out to Bas in my email. However, Microsoft is in fact using this flag to
     * indicate "light weight" DX applications. By light weight they are essentially
     * referring to applications that are not games. The idea is that when you create
     * a DX game, the driver assumes that you will pretty much have a single instance
     * and therefore it doesn't try to hold back when it comes to GPU resource
     * allocation as long as it can crank out performance. In other words, the
     * priority in regular DX applications is to make that one application run as fast
     * as you can. For "light weight" applications, including D2D applications, the
     * priorities are a bit different. Now you are no longer going to have a single
     * (or very few) instances. You can have a lot of them (say, for example, a
     * separate DX context/device per browser tab). In such cases, the GPU resource
     * allocation scheme changes.
     */
    HRESULT hr = createD3DDevice(
	NULL, 
	D3D10_DRIVER_TYPE_HARDWARE,
	NULL,
	D3D10_CREATE_DEVICE_BGRA_SUPPORT |
	D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
	D3D10_FEATURE_LEVEL_10_1,
	D3D10_1_SDK_VERSION,
	&d3ddevice);
    if (FAILED(hr)) {
	hr = createD3DDevice(
	    NULL, 
	    D3D10_DRIVER_TYPE_HARDWARE,
	    NULL,
	    D3D10_CREATE_DEVICE_BGRA_SUPPORT |
	    D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
	    D3D10_FEATURE_LEVEL_10_0,
	    D3D10_1_SDK_VERSION,
	    &d3ddevice);
	if (FAILED(hr)) {
	    /* This is not guaranteed to be too fast! */
	    hr = createD3DDevice(
		NULL, 
		D3D10_DRIVER_TYPE_HARDWARE,
		NULL,
		D3D10_CREATE_DEVICE_BGRA_SUPPORT |
		D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
		D3D10_FEATURE_LEVEL_9_3,
		D3D10_1_SDK_VERSION,
		&d3ddevice);

	}
    }
    if (FAILED(hr)) {
	return NULL;
    }

    cairo_device_t *device = cairo_d2d_create_device_from_d3d10device(d3ddevice);

    // Free our reference to the modules. The created device should have its own.
    FreeLibrary(d3d10module);
    return device;
}

int
cairo_release_device(cairo_device_t *device)
{
    int newrefcnt = --device->refcount;
    if (!newrefcnt) {
	// Call the correct destructor
	cairo_d2d_device_t *d2d_device = reinterpret_cast<cairo_d2d_device_t*>(device);
        HMODULE d3d10_1 = d2d_device->mD3D10_1;
	delete d2d_device;
	_cairo_d2d_release_factory();
        FreeLibrary(d3d10_1);
    }
    return newrefcnt;
}

int
cairo_addref_device(cairo_device_t *device)
{
    return ++device->refcount;
}

void
cairo_d2d_finish_device(cairo_device_t *device)
{
    cairo_d2d_device_t *d2d_device = reinterpret_cast<cairo_d2d_device_t*>(device);
    // Here it becomes interesting, this flush method is generally called when
    // interop is going on between our device and another device. The
    // synchronisation between these devices is not always that great. The
    // device flush method may flush the device's command queue, but it gives
    // no guarantee that the device will actually be done with those commands,
    // and so the surface may still not be complete when the external device
    // chooses to use it. The EVENT query will actually tell us when the GPU
    // is completely done with our commands.
    D3D10_QUERY_DESC queryDesc;
    queryDesc.MiscFlags = 0;
    queryDesc.Query = D3D10_QUERY_EVENT;
    RefPtr<ID3D10Query> query;

    d2d_device->mD3D10Device->CreateQuery(&queryDesc, &query);

    // QUERY_EVENT does not use Begin(). It's disabled.
    query->End();

    BOOL done = FALSE;
    while (!done) {
	// This will return S_OK and done = FALSE when the GPU is not done, and
	// S_OK and done = TRUE when the GPU is done. Any other return value
	// means we need to break out or risk an infinite loop.
	if (FAILED(query->GetData(&done, sizeof(BOOL), 0))) {
	    break;
	}
	if (FAILED(d2d_device->mD3D10Device->GetDeviceRemovedReason())) {
	    break;
	}
    }
}

ID3D10Device1*
cairo_d2d_device_get_device(cairo_device_t *device)
{
    cairo_d2d_device_t *d2d_device = reinterpret_cast<cairo_d2d_device_t*>(device);
    return d2d_device->mD3D10Device;  
}

static void
_cairo_d2d_setup_for_blend(cairo_d2d_device_t *device)
{
    device->mD3D10Device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    device->mD3D10Device->IASetInputLayout(device->mInputLayout);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    ID3D10Buffer *buff = device->mQuadBuffer;
    device->mD3D10Device->IASetVertexBuffers(0, 1, &buff, &stride, &offset);

    device->mD3D10Device->RSSetState(device->mRasterizerState);
}

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

static void
_cairo_d2d_round_out_to_int_rect(cairo_rectangle_int_t *rect, double x1, double y1, double x2, double y2)
{
    rect->x = (int)floor(x1);
    rect->y = (int)floor(y1);
    rect->width = (int)ceil(x2) - rect->x;
    rect->height = (int)ceil(y2) - rect->y;    
}

static int
_cairo_d2d_compute_surface_mem_size(cairo_d2d_surface_t *surface)
{
    int size = surface->rt->GetPixelSize().width * surface->rt->GetPixelSize().height;
    size *= surface->rt->GetPixelFormat().format == DXGI_FORMAT_A8_UNORM ? 1 : 4;
    return size;
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
	RefPtr<IDXGISurface> surf;
	DXGI_SURFACE_DESC surfDesc;
	surface->surface->QueryInterface(&surf);
	surf->GetDesc(&surfDesc);
	CD3D10_TEXTURE2D_DESC softDesc(surfDesc.Format, surfDesc.Width, surfDesc.Height);
        softDesc.MipLevels = 1;
	softDesc.Usage = D3D10_USAGE_DEFAULT;
	softDesc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
	surface->device->mD3D10Device->CreateTexture2D(&softDesc, NULL, &surface->bufferTexture);
	surface->device->mVRAMUsage += _cairo_d2d_compute_surface_mem_size(surface);
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
	d2dsurf->device->mD3D10Device->Flush();
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
 * Returns the inverse matrix for a D2D1 matrix. We cannot use the Invert function
 * on the Matrix3x2F function class since it's statically linked and we'd have to
 * lookup the symbol in the library. Doing this ourselves is easier.
 *
 * \param mat matrix
 * \return inverse of matrix mat
 */
static D2D1::Matrix3x2F
_cairo_d2d_invert_matrix(const D2D1::Matrix3x2F &mat)
{
    float inv_det =  (1 / mat.Determinant());

    return D2D1::Matrix3x2F(mat._22 * inv_det,
			    -mat._12 * inv_det,
			    -mat._21 * inv_det,
			    mat._11 * inv_det,
			    (mat._21 * mat._32 - mat._22 * mat._31) * inv_det,
			    -(mat._11 * mat._32 - mat._12 * mat._31) * inv_det);
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
	    line_join = D2D1_LINE_JOIN_MITER_OR_BEVEL;
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
	    /* D2D seems to specify dash lengths in units of
	     * line width instead of the more traditional approach
	     * that cairo and many other APIs use where the unit
	     * is in pixels or someother constant unit. */
	    dashes[i] = (FLOAT) (style->dash[i] / style->line_width);
	}
    }

    D2D1_DASH_STYLE dashStyle = D2D1_DASH_STYLE_SOLID;
    if (dashes) {
	dashStyle = D2D1_DASH_STYLE_CUSTOM;
    }

    RefPtr<ID2D1StrokeStyle> strokeStyle;
    sD2DFactory->CreateStrokeStyle(D2D1::StrokeStyleProperties(line_cap, 
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
    cached_bitmap()
    {
	sD2DFactory->AddRef();
    }

    ~cached_bitmap()
    {
	// Clear bitmap out first because it depends on the factory.
	bitmap = NULL;
	_cairo_d2d_release_factory();
    }

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

static double
_cairo_d2d_point_dist(const cairo_point_double_t &p1, const cairo_point_double_t &p2)
{
    return hypot(p2.x - p1.x, p2.y - p1.y);
}

static void
_cairo_d2d_normalize_point(cairo_point_double_t *p)
{
    double length = hypot(p->x, p->y);
    p->x /= length;
    p->y /= length;
}

static cairo_point_double_t
_cairo_d2d_subtract_point(const cairo_point_double_t &p1, const cairo_point_double_t &p2)
{
    cairo_point_double_t p = {p1.x - p2.x, p1.y - p2.y};
    return p;
}

static double
_cairo_d2d_dot_product(const cairo_point_double_t &p1, const cairo_point_double_t &p2)
{
    return p1.x * p2.x + p1.y * p2.y;
}

static RefPtr<ID2D1Brush>
_cairo_d2d_create_radial_gradient_brush(cairo_d2d_surface_t *d2dsurf,
					cairo_radial_pattern_t *source_pattern)
{
    cairo_matrix_t inv_mat = source_pattern->base.base.matrix;
    if (_cairo_matrix_is_invertible(&inv_mat)) {
	/* If this is not invertible it will be rank zero, and invMat = mat is fine */
	cairo_matrix_invert(&inv_mat);
    }

    D2D1_BRUSH_PROPERTIES brushProps =
	D2D1::BrushProperties(1.0, _cairo_d2d_matrix_from_matrix(&inv_mat));

    if ((source_pattern->c1.x != source_pattern->c2.x ||
	source_pattern->c1.y != source_pattern->c2.y) &&
	source_pattern->r1 != 0) {
	    /**
	     * In this particular case there's no way to deal with this!
	     * \todo Create an image surface with the gradient and use that.
	     */
	    return NULL;
    }

    D2D_POINT_2F center =
	_d2d_point_from_cairo_point(&source_pattern->c2);
    D2D_POINT_2F origin =
	_d2d_point_from_cairo_point(&source_pattern->c1);
    origin.x -= center.x;
    origin.y -= center.y;

    float outer_radius = _cairo_fixed_to_float(source_pattern->r2);
    float inner_radius = _cairo_fixed_to_float(source_pattern->r1);
    int num_stops = source_pattern->base.n_stops;
    int repeat_count = 1;
    D2D1_GRADIENT_STOP *stops;

    if (source_pattern->base.base.extend == CAIRO_EXTEND_REPEAT || source_pattern->base.base.extend == CAIRO_EXTEND_REFLECT) {
	bool reflected = false;
	bool reflect = source_pattern->base.base.extend == CAIRO_EXTEND_REFLECT;

	RefPtr<IDXGISurface> surf;
	d2dsurf->surface->QueryInterface(&surf);
	DXGI_SURFACE_DESC desc;
	surf->GetDesc(&desc);

	// Calculate the largest distance the origin could be from the edge after inverse
	// transforming by the pattern transformation.
	cairo_point_double_t top_left, top_right, bottom_left, bottom_right, gradient_center;
	top_left.x = bottom_left.x = top_left.y = top_right.y = 0;
	top_right.x = bottom_right.x = desc.Width;
	bottom_right.y = bottom_left.y = desc.Height;

	gradient_center.x = _cairo_fixed_to_float(source_pattern->c1.x);
	gradient_center.y = _cairo_fixed_to_float(source_pattern->c1.y);

	// Transform surface corners into pattern coordinates.
	cairo_matrix_transform_point(&source_pattern->base.base.matrix, &top_left.x, &top_left.y);
	cairo_matrix_transform_point(&source_pattern->base.base.matrix, &top_right.x, &top_right.y);
	cairo_matrix_transform_point(&source_pattern->base.base.matrix, &bottom_left.x, &bottom_left.y);
	cairo_matrix_transform_point(&source_pattern->base.base.matrix, &bottom_right.x, &top_left.y);

	// Find the corner furthest away from the gradient center in pattern space.
	double largest = MAX(_cairo_d2d_point_dist(top_left, gradient_center), _cairo_d2d_point_dist(top_right, gradient_center));
	largest = MAX(largest, _cairo_d2d_point_dist(bottom_left, gradient_center));
	largest = MAX(largest, _cairo_d2d_point_dist(bottom_right, gradient_center));

	unsigned int minSize = (unsigned int)ceil(largest);

	// Calculate how often we need to repeat on the inside (for filling the inner radius)
	// and on the outside (for covering the entire surface) and create the appropriate number
	// of stops.
	float gradient_length = outer_radius - inner_radius;
	int inner_repeat = (int)ceil(inner_radius / gradient_length);
	int outer_repeat = (int)MAX(1, ceil((minSize - inner_radius) / gradient_length));
	num_stops *= (inner_repeat + outer_repeat);
	stops = new D2D1_GRADIENT_STOP[num_stops];

	// Change outer_radius to the true outer radius after taking into account the needed
	// repeats.
	outer_radius = (inner_repeat + outer_repeat) * gradient_length;

	float stop_scale = 1.0f / (inner_repeat + outer_repeat);

	float inner_position = (inner_repeat * gradient_length) / outer_radius;
	if (reflect) {
	    // We start out reflected (meaning reflected starts as false since it will
	    // immediately be inverted) if the inner_repeats are uneven.
	    reflected = !(inner_repeat & 0x1);

	    for (int i = 0; i < num_stops; i++) {
		if (!(i % source_pattern->base.n_stops)) {
		    // Reflect here
		    reflected = !reflected;
		}
		// Calculate the repeat count.
		int repeat = i / source_pattern->base.n_stops;
		// Take the stop that we're using in the pattern.
		int stop = i % source_pattern->base.n_stops;
		if (reflected) {
		    // Take the stop from the opposite side when reflected.
		    stop = source_pattern->base.n_stops - stop - 1;
		    // When reflected take 1 - offset as the offset.
		    stops[i].position = (FLOAT)((repeat + 1.0f - source_pattern->base.stops[stop].offset) * stop_scale);
		} else {
		    stops[i].position = (FLOAT)((repeat + source_pattern->base.stops[stop].offset) * stop_scale);
		}
		stops[i].color =
		    _cairo_d2d_color_from_cairo_color(source_pattern->base.stops[stop].color);
	    }
	} else {
	    // Simple case, we don't need to reflect.
	    for (int i = 0; i < num_stops; i++) {
		// Calculate which repeat this is.
		int repeat = i / source_pattern->base.n_stops;
		// Calculate which stop this would be in the original pattern
		cairo_gradient_stop_t *stop = &source_pattern->base.stops[i % source_pattern->base.n_stops];
		stops[i].position = (FLOAT)((repeat + stop->offset) * stop_scale);
		stops[i].color = _cairo_d2d_color_from_cairo_color(stop->color);
	    }
	}
    } else if (source_pattern->base.base.extend == CAIRO_EXTEND_PAD) {
	float offset_factor = (outer_radius - inner_radius) / outer_radius;
	float global_offset = inner_radius / outer_radius;

	stops = new D2D1_GRADIENT_STOP[num_stops];

	// If the inner radius is not 0 we need to scale and offset the stops.
	for (unsigned int i = 0; i < source_pattern->base.n_stops; i++) {
	    cairo_gradient_stop_t *stop = &source_pattern->base.stops[i];
	    stops[i].position = (FLOAT)(global_offset + stop->offset * offset_factor);
	    stops[i].color = _cairo_d2d_color_from_cairo_color(stop->color);
	}
    } else if (source_pattern->base.base.extend == CAIRO_EXTEND_NONE) {
	float offset_factor = (outer_radius - inner_radius) / outer_radius;
	float global_offset = inner_radius / outer_radius;
        
        num_stops++; // Add a stop on the outer radius.
	if (inner_radius != 0) {
	    num_stops++; // Add a stop on the inner radius.
	}

	stops = new D2D1_GRADIENT_STOP[num_stops];

	// If the inner radius is not 0 we need to scale and offset the stops and put a stop before the inner_radius
	// of a transparent color.
	int i = 0;
	if (inner_radius != 0) {
	    stops[i].position = global_offset;
	    stops[i].color = D2D1::ColorF(0, 0);
	    i++;
	}
	for (unsigned int j = 0; j < source_pattern->base.n_stops; j++, i++) {
	    cairo_gradient_stop_t *stop = &source_pattern->base.stops[j];
	    stops[i].position = (FLOAT)(global_offset + stop->offset * offset_factor);
	    stops[i].color = _cairo_d2d_color_from_cairo_color(stop->color);
	}
	stops[i].position = 1.0f;
	stops[i].color = D2D1::ColorF(0, 0);
    } else {
	return NULL;
    }

    RefPtr<ID2D1GradientStopCollection> stopCollection;
    d2dsurf->rt->CreateGradientStopCollection(stops, num_stops, &stopCollection);
    RefPtr<ID2D1RadialGradientBrush> brush;

    d2dsurf->rt->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(center,
									       origin,
									       outer_radius,
									       outer_radius),
					   brushProps,
					   stopCollection,
					   &brush);
    delete [] stops;
    return brush;
}

static RefPtr<ID2D1Brush>
_cairo_d2d_create_linear_gradient_brush(cairo_d2d_surface_t *d2dsurf,
					cairo_linear_pattern_t *source_pattern)
{
    if (source_pattern->p1.x == source_pattern->p2.x &&
	source_pattern->p1.y == source_pattern->p2.y) {
	// Cairo behavior in this situation is to draw a solid color the size of the last stop.
	RefPtr<ID2D1SolidColorBrush> brush;
	d2dsurf->rt->CreateSolidColorBrush(
	    _cairo_d2d_color_from_cairo_color(source_pattern->base.stops[source_pattern->base.n_stops - 1].color),
	    &brush);
	return brush;
    }

    cairo_matrix_t inv_mat = source_pattern->base.base.matrix;
    /**
     * Cairo views this matrix as the transformation of the destination
     * when the pattern is imposed. We see this differently, D2D transformation
     * moves the pattern over the destination.
     */
    if (_cairo_matrix_is_invertible(&inv_mat)) {
	/* If this is not invertible it will be rank zero, and invMat = mat is fine */
	cairo_matrix_invert(&inv_mat);
    }
    D2D1_BRUSH_PROPERTIES brushProps =
	D2D1::BrushProperties(1.0, _cairo_d2d_matrix_from_matrix(&inv_mat));
    cairo_point_double_t p1, p2;
    p1.x = _cairo_fixed_to_float(source_pattern->p1.x);
    p1.y = _cairo_fixed_to_float(source_pattern->p1.y);
    p2.x = _cairo_fixed_to_float(source_pattern->p2.x);
    p2.y = _cairo_fixed_to_float(source_pattern->p2.y);

    D2D1_GRADIENT_STOP *stops;
    int num_stops = source_pattern->base.n_stops;
    if (source_pattern->base.base.extend == CAIRO_EXTEND_REPEAT || source_pattern->base.base.extend == CAIRO_EXTEND_REFLECT) {

	RefPtr<IDXGISurface> surf;
	d2dsurf->surface->QueryInterface(&surf);
	DXGI_SURFACE_DESC desc;
	surf->GetDesc(&desc);

	// Get this when the points are not transformed yet.
	double gradient_length = _cairo_d2d_point_dist(p1, p2);

	// Calculate the repeat count needed;
	cairo_point_double_t top_left, top_right, bottom_left, bottom_right;
	top_left.x = bottom_left.x = top_left.y = top_right.y = 0;
	top_right.x = bottom_right.x = desc.Width;
	bottom_right.y = bottom_left.y = desc.Height;
	// Transform the corners of our surface to pattern space.
	cairo_matrix_transform_point(&source_pattern->base.base.matrix, &top_left.x, &top_left.y);
	cairo_matrix_transform_point(&source_pattern->base.base.matrix, &top_right.x, &top_right.y);
	cairo_matrix_transform_point(&source_pattern->base.base.matrix, &bottom_left.x, &bottom_left.y);
	cairo_matrix_transform_point(&source_pattern->base.base.matrix, &bottom_right.x, &top_left.y);

	cairo_point_double_t u;
	// Unit vector of the gradient direction.
	u = _cairo_d2d_subtract_point(p2, p1);
	_cairo_d2d_normalize_point(&u);

	// (corner - p1) . u = |corner - p1| cos(a) where a is the angle between the two vectors.
	// Coincidentally |corner - p1| cos(a) is actually also the distance our gradient needs to cover since
	// at this point on the gradient line it will be perpendicular to the line running from the gradient
	// line through the corner.

	double max_dist, min_dist;
	max_dist = MAX(_cairo_d2d_dot_product(u, _cairo_d2d_subtract_point(top_left, p1)),
	               _cairo_d2d_dot_product(u, _cairo_d2d_subtract_point(top_right, p1)));
	max_dist = MAX(max_dist, _cairo_d2d_dot_product(u, _cairo_d2d_subtract_point(bottom_left, p1)));
	max_dist = MAX(max_dist, _cairo_d2d_dot_product(u, _cairo_d2d_subtract_point(bottom_right, p1)));
	min_dist = MIN(_cairo_d2d_dot_product(u, _cairo_d2d_subtract_point(top_left, p1)),
	               _cairo_d2d_dot_product(u, _cairo_d2d_subtract_point(top_right, p1)));
	min_dist = MIN(min_dist, _cairo_d2d_dot_product(u, _cairo_d2d_subtract_point(bottom_left, p1)));
	min_dist = MIN(min_dist, _cairo_d2d_dot_product(u, _cairo_d2d_subtract_point(bottom_right, p1)));

	min_dist = MAX(-min_dist, 0);

	// Repeats after gradient start.
	int after_repeat = (int)ceil(max_dist / gradient_length);
	int before_repeat = (int)ceil(min_dist / gradient_length);
	num_stops *= (after_repeat + before_repeat);

	p2.x = p1.x + u.x * after_repeat * gradient_length;
	p2.y = p1.y + u.y * after_repeat * gradient_length;
	p1.x = p1.x - u.x * before_repeat * gradient_length;
	p1.y = p1.y - u.y * before_repeat * gradient_length;

	float stop_scale = 1.0f / (float)(after_repeat + before_repeat);
	float begin_position = (float)before_repeat / (float)(after_repeat + before_repeat);

	stops = new D2D1_GRADIENT_STOP[num_stops];
	if (source_pattern->base.base.extend == CAIRO_EXTEND_REFLECT) {
	    // We start out reflected (meaning reflected starts as false since it will
	    // immediately be inverted) if the inner_repeats are uneven.
	    bool reflected = !(before_repeat & 0x1);

	    for (int i = 0; i < num_stops; i++) {
		if (!(i % source_pattern->base.n_stops)) {
		    // Reflect here
		    reflected = !reflected;
		}
		// Calculate the repeat count.
		int repeat = i / source_pattern->base.n_stops;
		// Take the stop that we're using in the pattern.
		int stop = i % source_pattern->base.n_stops;
		if (reflected) {
		    // Take the stop from the opposite side when reflected.
		    stop = source_pattern->base.n_stops - stop - 1;
		    // When reflected take 1 - offset as the offset.
		    stops[i].position = (FLOAT)((repeat + 1.0f - source_pattern->base.stops[stop].offset) * stop_scale);
		} else {
		    stops[i].position = (FLOAT)((repeat + source_pattern->base.stops[stop].offset) * stop_scale);
		}
		stops[i].color =
		    _cairo_d2d_color_from_cairo_color(source_pattern->base.stops[stop].color);
	    }
	} else {
	    // Simple case, we don't need to reflect.
	    for (int i = 0; i < num_stops; i++) {
		// Calculate which repeat this is.
		int repeat = i / source_pattern->base.n_stops;
		// Calculate which stop this would be in the original pattern
		cairo_gradient_stop_t *stop = &source_pattern->base.stops[i % source_pattern->base.n_stops];
		stops[i].position = (FLOAT)((repeat + stop->offset) * stop_scale);
		stops[i].color = _cairo_d2d_color_from_cairo_color(stop->color);
	    }
	}
    } else if (source_pattern->base.base.extend == CAIRO_EXTEND_PAD) {
	stops = new D2D1_GRADIENT_STOP[source_pattern->base.n_stops];
	for (unsigned int i = 0; i < source_pattern->base.n_stops; i++) {
	    cairo_gradient_stop_t *stop = &source_pattern->base.stops[i];
	    stops[i].position = (FLOAT)stop->offset;
	    stops[i].color = _cairo_d2d_color_from_cairo_color(stop->color);
	}
    } else if (source_pattern->base.base.extend == CAIRO_EXTEND_NONE) {
	num_stops += 2;
	stops = new D2D1_GRADIENT_STOP[num_stops];
	stops[0].position = 0;
	stops[0].color = D2D1::ColorF(0, 0);
	for (unsigned int i = 1; i < source_pattern->base.n_stops + 1; i++) {
	    cairo_gradient_stop_t *stop = &source_pattern->base.stops[i - 1];
	    stops[i].position = (FLOAT)stop->offset;
	    stops[i].color = _cairo_d2d_color_from_cairo_color(stop->color);
	}
	stops[source_pattern->base.n_stops + 1].position = 1.0f;
	stops[source_pattern->base.n_stops + 1].color = D2D1::ColorF(0, 0);
    }
    RefPtr<ID2D1GradientStopCollection> stopCollection;
    d2dsurf->rt->CreateGradientStopCollection(stops, num_stops, &stopCollection);
    RefPtr<ID2D1LinearGradientBrush> brush;
    d2dsurf->rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F((FLOAT)p1.x, (FLOAT)p1.y),
									       D2D1::Point2F((FLOAT)p2.x, (FLOAT)p2.y)),
					   brushProps,
					   stopCollection,
					   &brush);
    delete [] stops;
    return brush;
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
	cairo_linear_pattern_t *source_pattern =
	    (cairo_linear_pattern_t*)pattern;
	return _cairo_d2d_create_linear_gradient_brush(d2dsurf, source_pattern);
    } else if (pattern->type == CAIRO_PATTERN_TYPE_RADIAL) {
	cairo_radial_pattern_t *source_pattern =
	    (cairo_radial_pattern_t*)pattern;
	return _cairo_d2d_create_radial_gradient_brush(d2dsurf, source_pattern);
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
	     * We create a slightly larger bitmap with a transparent border
	     * around it for this case. Need to translate for that.
	     */
	    cairo_matrix_translate(&mat, -1.0, -1.0);
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
	    _cairo_d2d_flush(srcSurf);

	    if (pattern->extend == CAIRO_EXTEND_NONE) {
		ID2D1Bitmap *srcSurfBitmap = srcSurf->surfaceBitmap;
		d2dsurf->rt->CreateBitmap(
		    D2D1::SizeU(srcSurfBitmap->GetPixelSize().width + 2,
				srcSurfBitmap->GetPixelSize().height + 2),
		    D2D1::BitmapProperties(srcSurfBitmap->GetPixelFormat()),
		    &sourceBitmap);
		D2D1_POINT_2U point = D2D1::Point2U(1, 1);
		sourceBitmap->CopyFromBitmap(&point, srcSurfBitmap, NULL);
	    } else {
		sourceBitmap = srcSurf->surfaceBitmap;
	    }

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
    sD2DFactory->CreatePathGeometry(&d2dpath);
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

cairo_operator_t _cairo_d2d_simplify_operator(cairo_operator_t op,
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
    
    /* CreateTexture2D does not support D3D10_RESOURCE_MISC_GDI_COMPATIBLE with DXGI_FORMAT_A8_UNORM */
    if (desc.Format != DXGI_FORMAT_A8_UNORM)
	desc.MiscFlags = D3D10_RESOURCE_MISC_GDI_COMPATIBLE;

    RefPtr<ID3D10Texture2D> texture;
    RefPtr<IDXGISurface> dxgiSurface;

    hr = d2dsurf->device->mD3D10Device->CreateTexture2D(&desc, NULL, &texture);
    if (FAILED(hr)) {
	goto FAIL_CREATESIMILAR;
    }

    newSurf->surface = texture;

    // Create the DXGI surface.
    hr = newSurf->surface->QueryInterface(IID_IDXGISurface, (void**)&dxgiSurface);
    if (FAILED(hr)) {
	goto FAIL_CREATESIMILAR;
    }

    D2D1_RENDER_TARGET_USAGE usage = (desc.MiscFlags & D3D10_RESOURCE_MISC_GDI_COMPATIBLE) ?
					  D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE
					: D2D1_RENDER_TARGET_USAGE_NONE;

    hr = sD2DFactory->CreateDxgiSurfaceRenderTarget(dxgiSurface,
						    D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
										 D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN,
												   alpha),
										 dpiX,
										 dpiY,
										 usage),
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

    newSurf->device = d2dsurf->device;
    cairo_addref_device(&newSurf->device->base);
    newSurf->device->mVRAMUsage += _cairo_d2d_compute_surface_mem_size(newSurf);

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

    d2dsurf->device->mVRAMUsage -= _cairo_d2d_compute_surface_mem_size(d2dsurf);
    if (d2dsurf->bufferTexture) {
	d2dsurf->device->mVRAMUsage -= _cairo_d2d_compute_surface_mem_size(d2dsurf);
    }

    reset_clip(d2dsurf);

    cairo_release_device(&d2dsurf->device->base);
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
    hr = d2dsurf->device->mD3D10Device->CreateTexture2D(&softDesc, NULL, &softTexture);
    if (FAILED(hr)) {
	return _cairo_error(CAIRO_STATUS_NO_MEMORY);
    }

    d2dsurf->device->mD3D10Device->CopyResource(softTexture, d2dsurf->surface);

    D3D10_MAPPED_TEXTURE2D data;
    hr = softTexture->Map(0, D3D10_MAP_READ_WRITE, 0, &data);
    if (FAILED(hr)) {
	return _cairo_error(CAIRO_STATUS_NO_DEVICE);
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
    hr = d2dsurf->device->mD3D10Device->CreateTexture2D(&softDesc, NULL, &softTexture);
    if (FAILED(hr)) {
	return _cairo_error(CAIRO_STATUS_NO_MEMORY);
    }
    d2dsurf->device->mD3D10Device->CopyResource(softTexture, d2dsurf->surface);

    D3D10_MAPPED_TEXTURE2D data;
    hr = softTexture->Map(0, D3D10_MAP_READ_WRITE, 0, &data);
    if (FAILED(hr)) {
	return _cairo_error(CAIRO_STATUS_NO_DEVICE);
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
    d2dsurf->device->mD3D10Device->CopyResource(d2dsurf->surface, softTexture);
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
_cairo_d2d_copy_surface(cairo_d2d_surface_t *dst,
			cairo_d2d_surface_t *src,
			cairo_point_int_t *translation,
			cairo_region_t *region)
{
    RefPtr<IDXGISurface> dstSurface;
    dst->surface->QueryInterface(&dstSurface);
    RefPtr<IDXGISurface> srcSurface;
    src->surface->QueryInterface(&srcSurface);
    DXGI_SURFACE_DESC srcDesc, dstDesc;

    srcSurface->GetDesc(&srcDesc);
    dstSurface->GetDesc(&dstDesc);

    cairo_rectangle_int_t clip_rect;
    clip_rect.x = 0;
    clip_rect.y = 0;
    clip_rect.width = dstDesc.Width;
    clip_rect.height = dstDesc.Height;
    
    cairo_int_status_t rv = CAIRO_INT_STATUS_SUCCESS;

    _cairo_d2d_flush(dst);
    ID3D10Resource *srcResource = src->surface;
    if (src->surface.get() == dst->surface.get()) {
	// Self-copy
	srcResource = _cairo_d2d_get_buffer_texture(dst);
	src->device->mD3D10Device->CopyResource(srcResource, src->surface);
    } else {
	// Need to flush the source too if it's a different surface.
        _cairo_d2d_flush(src);
    }

    // One copy for each rectangle in the final clipping region.
    for (int i = 0; i < cairo_region_num_rectangles(region); i++) {
	D3D10_BOX rect;
	cairo_rectangle_int_t area_to_copy;

	cairo_region_get_rectangle(region, i, &area_to_copy);

	cairo_rectangle_int_t transformed_rect = { area_to_copy.x + translation->x,
						   area_to_copy.y + translation->y,
						   area_to_copy.width, area_to_copy.height };
	cairo_rectangle_int_t surface_rect = { 0, 0, srcDesc.Width, srcDesc.Height };


	if (!_cairo_rectangle_contains(&surface_rect, &transformed_rect)) {
	    /* We cannot do any sort of extend, in the future a little bit of extra code could
	     * allow us to support EXTEND_NONE.
	     */
	    rv = CAIRO_INT_STATUS_UNSUPPORTED;
	    break;
	}

	rect.front = 0;
	rect.back = 1;
	rect.left = transformed_rect.x;
	rect.top = transformed_rect.y;
	rect.right = transformed_rect.x + transformed_rect.width;
	rect.bottom = transformed_rect.y + transformed_rect.height;

	src->device->mD3D10Device->CopySubresourceRegion(dst->surface,
							 0,
							 area_to_copy.x,
							 area_to_copy.y,
							 0,
							 srcResource,
							 0,
							 &rect);
    }

    return rv;
}

static cairo_int_status_t
_cairo_d2d_blend_surface(cairo_d2d_surface_t *dst,
			 cairo_d2d_surface_t *src,
		 	 const cairo_matrix_t *transform,
			 cairo_box_t *box,
			 cairo_clip_t *clip,
                         cairo_filter_t filter,
			 float opacity)
{
    if (dst == src) {
	// We cannot do self-blend.
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    cairo_int_status_t rv = CAIRO_INT_STATUS_SUCCESS;

    _begin_draw_state(dst);
    _cairo_d2d_set_clip(dst, clip);
    _cairo_d2d_flush(src);
    D2D1_SIZE_U sourceSize = src->surfaceBitmap->GetPixelSize();


    double x1, x2, y1, y2;
    if (box) {
	_cairo_box_to_doubles(box, &x1, &y1, &x2, &y2);
    } else {
	x1 = y1 = 0;
	x2 = dst->rt->GetSize().width;
	y2 = dst->rt->GetSize().height;
    }

    if (clip) {
	const cairo_rectangle_int_t *clipExtent = _cairo_clip_get_extents(clip);
	x1 = MAX(x1, clipExtent->x);
	x2 = MIN(x2, clipExtent->x + clipExtent->width);
	y1 = MAX(y1, clipExtent->y);
	y2 = MIN(y2, clipExtent->y + clipExtent->height);
    }

    // We should be in drawing state for this.
    _begin_draw_state(dst);
    _cairo_d2d_set_clip (dst, clip);
    D2D1_RECT_F rectSrc;
    rectSrc.left = (float)(x1 * transform->xx + transform->x0);
    rectSrc.top = (float)(y1 * transform->yy + transform->y0);
    rectSrc.right = (float)(x2 * transform->xx + transform->x0);
    rectSrc.bottom = (float)(y2 * transform->yy + transform->y0);

    if (rectSrc.left < 0 || rectSrc.top < 0 || rectSrc.right < 0 || rectSrc.bottom < 0 ||
	rectSrc.right > sourceSize.width || rectSrc.bottom > sourceSize.height ||
	rectSrc.left > sourceSize.width || rectSrc.top > sourceSize.height) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    D2D1_RECT_F rectDst;
    rectDst.left = (float)x1;
    rectDst.top = (float)y1;
    rectDst.right = (float)x2;
    rectDst.bottom = (float)y2;

    // Bug 599658 - if the src rect is inverted in either axis D2D is fine with
    // this but it does not actually invert the bitmap. This is an easy way
    // of doing that.
    D2D1_MATRIX_3X2_F matrix = D2D1::IdentityMatrix();
    bool needsTransform = false;
    if (rectSrc.left > rectSrc.right) {
	rectDst.left = -rectDst.left;
	rectDst.right = -rectDst.right;
	matrix._11 = -1.0;
	needsTransform = true;
    }
    if (rectSrc.top > rectSrc.bottom) {
	rectDst.top = -rectDst.top;
	rectDst.bottom = -rectDst.bottom;
	matrix._22 = -1.0;
	needsTransform = true;
    }

    D2D1_BITMAP_INTERPOLATION_MODE interpMode =
      D2D1_BITMAP_INTERPOLATION_MODE_LINEAR;

    if (needsTransform) {
	dst->rt->SetTransform(matrix);
    }

    if (filter == CAIRO_FILTER_NEAREST) {
      interpMode = D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
    }

    dst->rt->DrawBitmap(src->surfaceBitmap,
			rectDst,
			opacity,
			interpMode,
			rectSrc);
    if (needsTransform) {
	dst->rt->SetTransform(D2D1::IdentityMatrix());
    }

    return rv;
}
/**
 * This function will text if we can use GPU mem cpy to execute an operation with
 * a surface pattern. If box is NULL it will operate on the entire dst surface.
 */
static cairo_int_status_t
_cairo_d2d_try_fastblit(cairo_d2d_surface_t *dst,
		        cairo_surface_t *src,
		        cairo_box_t *box,
		        const cairo_matrix_t *matrix,
		        cairo_clip_t *clip,
		        cairo_operator_t op,
                        cairo_filter_t filter,
			float opacity = 1.0f)
{
    if (op == CAIRO_OPERATOR_OVER && src->content == CAIRO_CONTENT_COLOR) {
	op = CAIRO_OPERATOR_SOURCE;
    }
    if (op != CAIRO_OPERATOR_SOURCE && op != CAIRO_OPERATOR_OVER) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    /* For now we do only D2D sources */
    if (src->type != CAIRO_SURFACE_TYPE_D2D) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    cairo_d2d_surface_t *d2dsrc = reinterpret_cast<cairo_d2d_surface_t*>(src);
    if (op == CAIRO_OPERATOR_OVER && matrix->xy == 0 && matrix->yx == 0) {
	return _cairo_d2d_blend_surface(dst, d2dsrc, matrix, box, clip, filter, opacity);
    }
    
    if (op == CAIRO_OPERATOR_OVER || opacity != 1.0f) {
	// Past this point we will never get into a situation where we can
	// support OVER.
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    
    cairo_point_int_t translation;
    if ((box && !box_is_integer(box)) ||
	!_cairo_matrix_is_integer_translation(matrix, &translation.x, &translation.y)) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    cairo_rectangle_int_t rect;
    if (box) {
	_cairo_box_round_to_rectangle(box, &rect);
    } else {
	rect.x = rect.y = 0;
	rect.width = dst->rt->GetPixelSize().width;
	rect.height = dst->rt->GetPixelSize().height;
    }
    
    if (d2dsrc->device != dst->device) {
	// This doesn't work between different devices.
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    /* Region we need to clip this operation to */
    cairo_region_t *clipping_region = NULL;
    cairo_region_t *region;
    if (clip) {
	_cairo_clip_get_region(clip, &clipping_region);

	if (!clipping_region) {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
	region = cairo_region_copy(clipping_region);

	cairo_region_intersect_rectangle(region, &rect);

	if (cairo_region_is_empty(region)) {
	    // Nothing to do.
	    return CAIRO_INT_STATUS_SUCCESS;
	}
    } else {
	region = cairo_region_create_rectangle(&rect);
	// Areas outside of the surface do not matter.
	cairo_rectangle_int_t surface_rect = { 0, 0,
					       dst->rt->GetPixelSize().width,
					       dst->rt->GetPixelSize().height };
	cairo_region_intersect_rectangle(region, &surface_rect);
    }

    cairo_int_status_t rv = _cairo_d2d_copy_surface(dst, d2dsrc, &translation, region);

    cairo_region_destroy(region);
    
    return rv;
}

RefPtr<ID2D1RenderTarget> _cairo_d2d_get_temp_rt(cairo_d2d_surface_t *surf, cairo_clip_t *clip)
{
    RefPtr<ID3D10Texture2D> texture = _cairo_d2d_get_buffer_texture(surf);
    RefPtr<ID2D1RenderTarget> new_rt;
    RefPtr<IDXGISurface> dxgiSurface;
    texture->QueryInterface(&dxgiSurface);
    HRESULT hr;

    _cairo_d2d_flush(surf);

    if (!surf) {
	return NULL;
    }

    D2D1_RENDER_TARGET_PROPERTIES props = 
	D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
				     D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));
    hr = sD2DFactory->CreateDxgiSurfaceRenderTarget(dxgiSurface,
						    props,
						    &new_rt);

    if (FAILED(hr)) {
	return NULL;
    }

    new_rt->BeginDraw();
    new_rt->Clear(D2D1::ColorF(0, 0));

    // Since this is a fresh surface there's no point in doing clever things to
    // keep the clip path around until a certain depth. So we just do a straight-
    // forward push of all clip paths in the tree, similar to what the normal
    // clip code does, but a little less clever.
    if (clip) {
	cairo_clip_path_t *path = clip->path;
	while (path) {
	    cairo_box_t clip_box;
	    if (_cairo_path_fixed_is_box(&path->path, &clip_box)) {
		// If this does not have a region it could be none-pixel aligned.
		D2D1_ANTIALIAS_MODE aaMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
		if (box_is_integer(&clip_box)) {
		    aaMode = D2D1_ANTIALIAS_MODE_ALIASED;
		}
		new_rt->PushAxisAlignedClip(D2D1::RectF(_cairo_fixed_to_float(clip_box.p1.x),
							_cairo_fixed_to_float(clip_box.p1.y),
							_cairo_fixed_to_float(clip_box.p2.x),
							_cairo_fixed_to_float(clip_box.p2.y)),
					    aaMode);
	    } else {
		HRESULT hr;
		RefPtr<ID2D1PathGeometry> geom = _cairo_d2d_create_path_geometry_for_path (&path->path,
											   path->fill_rule,
											   D2D1_FIGURE_BEGIN_FILLED);
		RefPtr<ID2D1Layer> layer;

		hr = new_rt->CreateLayer (&layer);

		D2D1_LAYER_OPTIONS options = D2D1_LAYER_OPTIONS_NONE;

		new_rt->PushLayer(D2D1::LayerParameters(
					D2D1::InfiniteRect(),
					geom,
					D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
					D2D1::IdentityMatrix(),
					1.0,
					0,
					options),
				  layer);
	    }
	    path = path->prev;
	}
    }
    return new_rt;
}

cairo_int_status_t _cairo_d2d_blend_temp_surface(cairo_d2d_surface_t *surf, cairo_operator_t op, ID2D1RenderTarget *rt, cairo_clip_t *clip, const cairo_rectangle_int_t *bounds)
{
    int numPaths = 0;
    if (clip) {
	cairo_clip_path_t *path = clip->path;
	while (path) {
	    numPaths++;
	    path = path->prev;
	}
	
	cairo_clip_path_t **paths = new cairo_clip_path_t*[numPaths];

	numPaths = 0;
	path = clip->path;
	while (path) {
	    paths[numPaths++] = path;
	    path = path->prev;
	}	

	for (int i = numPaths - 1; i >= 0; i--) {
	    if (paths[i]->flags & CAIRO_CLIP_PATH_IS_BOX) {
		rt->PopAxisAlignedClip();
	    } else {
		rt->PopLayer();
	    }
	}
	delete [] paths;
    }
    rt->EndDraw();
    HRESULT hr;

    RefPtr<ID3D10Texture2D> srcTexture = _cairo_d2d_get_buffer_texture(surf);
    RefPtr<ID3D10Texture2D> dstTexture;

    surf->surface->QueryInterface(&dstTexture);
    ID3D10Device *device = surf->device->mD3D10Device;

    if (!surf->buffer_rt_view) {
	hr = device->CreateRenderTargetView(dstTexture, NULL, &surf->buffer_rt_view);
	if (FAILED(hr)) {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    }

    if (!surf->buffer_sr_view) {
	hr = device->CreateShaderResourceView(srcTexture, NULL, &surf->buffer_sr_view);
	if (FAILED(hr)) {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    }

    cairo_int_status_t status;

    status = _cairo_d2d_set_operator(surf->device, op);

    if (unlikely(status)) {
	return status;
    }

    D3D10_TEXTURE2D_DESC tDesc;
    dstTexture->GetDesc(&tDesc);
    D3D10_VIEWPORT vp;
    vp.Height = tDesc.Height;
    vp.MinDepth = 0;
    vp.MaxDepth = 1.0;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = tDesc.Width;
    device->RSSetViewports(1, &vp);

    ID3D10Effect *effect = surf->device->mSampleEffect;

    ID3D10RenderTargetView *rtViewPtr = surf->buffer_rt_view;
    device->OMSetRenderTargets(1, &rtViewPtr, 0);
    ID3D10EffectVectorVariable *quadDesc = effect->GetVariableByName("QuadDesc")->AsVector();
    ID3D10EffectVectorVariable *texCoords = effect->GetVariableByName("TexCoords")->AsVector();

    float quadDescVal[] = { -1.0f, 1.0f, 2.0f, -2.0f };
    float texCoordsVal[] = { 0.0, 0.0, 1.0f, 1.0f };
    if (bounds && _cairo_operator_bounded_by_mask(op)) {
	quadDescVal[0] = -1.0f + ((float)bounds->x / (float)tDesc.Width) * 2.0f;
	quadDescVal[1] = 1.0f - ((float)bounds->y / (float)tDesc.Height) * 2.0f;
	quadDescVal[2] = ((float)bounds->width / (float)tDesc.Width) * 2.0f;
	quadDescVal[3] = -((float)bounds->height / (float)tDesc.Height) * 2.0f;
	texCoordsVal[0] = (float)bounds->x / (float)tDesc.Width;
	texCoordsVal[1] = (float)bounds->y / (float)tDesc.Height;
	texCoordsVal[2] = (float)bounds->width / (float)tDesc.Width;
	texCoordsVal[3] = (float)bounds->height / (float)tDesc.Height;
    }
    quadDesc->SetFloatVector(quadDescVal);
    texCoords->SetFloatVector(texCoordsVal);

    _cairo_d2d_setup_for_blend(surf->device);
    ID3D10EffectTechnique *technique = effect->GetTechniqueByName("SampleTexture");
    technique->GetPassByIndex(0)->Apply(0);

    ID3D10ShaderResourceView *srViewPtr = surf->buffer_sr_view;
    device->PSSetShaderResources(0, 1, &srViewPtr);

    device->Draw(4, 0);

#ifdef DEBUG
    // Quiet down some info messages from D3D10 debug layer
    srViewPtr = NULL;
    device->PSSetShaderResources(0, 1, &srViewPtr);
    rtViewPtr = NULL;
    device->OMSetRenderTargets(1, &rtViewPtr, 0); 
#endif
    return CAIRO_INT_STATUS_SUCCESS;
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

    if (op == CAIRO_OPERATOR_SOURCE) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (op == CAIRO_OPERATOR_CLEAR) {
	return _cairo_d2d_clear(d2dsurf, clip);
    }

    if (source->type == CAIRO_PATTERN_TYPE_SURFACE) {
	const cairo_surface_pattern_t *surf_pattern = 
	    reinterpret_cast<const cairo_surface_pattern_t*>(source);

	status = _cairo_d2d_try_fastblit(d2dsurf, surf_pattern->surface,
				         NULL, &source->matrix, clip,
                                         op, source->filter);

	if (status != CAIRO_INT_STATUS_UNSUPPORTED) {
	    return status;
	}
    }
    RefPtr<ID2D1RenderTarget> target_rt = d2dsurf->rt;
#ifndef ALWAYS_MANUAL_COMPOSITE
    if (op != CAIRO_OPERATOR_OVER) {
#endif
	target_rt = _cairo_d2d_get_temp_rt(d2dsurf, clip);
	if (!target_rt) {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
#ifndef ALWAYS_MANUAL_COMPOSITE
    } else {
	_begin_draw_state(d2dsurf);
	status = (cairo_int_status_t)_cairo_d2d_set_clip (d2dsurf, clip);

	if (unlikely(status))
	    return status;
    }
#endif

    target_rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

    RefPtr<ID2D1Brush> brush = _cairo_d2d_create_brush_for_pattern(d2dsurf,
								   source);
    
    if (!brush) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    D2D1_SIZE_F size = target_rt->GetSize();
    target_rt->FillRectangle(D2D1::RectF((FLOAT)0,
					 (FLOAT)0,
					 (FLOAT)size.width,
					 (FLOAT)size.height),
			     brush);

    if (target_rt.get() != d2dsurf->rt.get()) {
	return _cairo_d2d_blend_temp_surface(d2dsurf, op, target_rt, clip);
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

    status = (cairo_int_status_t)_cairo_surface_mask_extents (&d2dsurf->base,
		    op, source,
		    mask,
		    clip, &extents);
    if (unlikely (status))
	    return status;


    D2D1_RECT_F rect = D2D1::RectF(0,
				   0,
				   (FLOAT)d2dsurf->rt->GetPixelSize().width,
				   (FLOAT)d2dsurf->rt->GetPixelSize().height);

    rect.left = (FLOAT)extents.x;
    rect.right = (FLOAT)(extents.x + extents.width);
    rect.top = (FLOAT)extents.y;
    rect.bottom = (FLOAT)(extents.y + extents.height);

    bool isSolidAlphaMask = false;
    float solidAlphaValue = 1.0f;

    if (mask->type == CAIRO_PATTERN_TYPE_SOLID) {
	cairo_solid_pattern_t *solidPattern =
	    (cairo_solid_pattern_t*)mask;
	if (solidPattern->content = CAIRO_CONTENT_ALPHA) {
	    isSolidAlphaMask = true;
	    solidAlphaValue = solidPattern->color.alpha;
	}
    }

    if (isSolidAlphaMask) {
	if (source->type == CAIRO_PATTERN_TYPE_SURFACE) {
	    const cairo_surface_pattern_t *surf_pattern = 
		reinterpret_cast<const cairo_surface_pattern_t*>(source);
	    cairo_box_t box;
	    _cairo_box_from_rectangle(&box, &extents);
	    cairo_int_status_t rv = _cairo_d2d_try_fastblit(d2dsurf,
							    surf_pattern->surface,
							    &box,
							    &source->matrix,
							    clip,
							    op,
                                                            source->filter,
							    solidAlphaValue);
	    if (rv != CAIRO_INT_STATUS_UNSUPPORTED) {
		return rv;
	    }
	}
    }

    RefPtr<ID2D1Brush> brush = _cairo_d2d_create_brush_for_pattern(d2dsurf, source);
    if (!brush) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    RefPtr<ID2D1RenderTarget> target_rt = d2dsurf->rt;
#ifndef ALWAYS_MANUAL_COMPOSITE
    if (op != CAIRO_OPERATOR_OVER) {
#endif
	target_rt = _cairo_d2d_get_temp_rt(d2dsurf, clip);
	if (!target_rt) {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
#ifndef ALWAYS_MANUAL_COMPOSITE
    } else {
	_begin_draw_state(d2dsurf);
	status = (cairo_int_status_t)_cairo_d2d_set_clip (d2dsurf, clip);

	if (unlikely(status))
	    return status;
    }
#endif

    if (isSolidAlphaMask) {
	brush->SetOpacity(solidAlphaValue);
	target_rt->FillRectangle(rect,
				 brush);
	brush->SetOpacity(1.0);

	if (target_rt.get() != d2dsurf->rt.get()) {
	    return _cairo_d2d_blend_temp_surface(d2dsurf, op, target_rt, clip);
	}
	return CAIRO_INT_STATUS_SUCCESS;
    }

    RefPtr<ID2D1Brush> opacityBrush = _cairo_d2d_create_brush_for_pattern(d2dsurf, mask, true);
    if (!opacityBrush) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (!d2dsurf->maskLayer) {
	d2dsurf->rt->CreateLayer(&d2dsurf->maskLayer);
    }
    target_rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),
					       0,
					       D2D1_ANTIALIAS_MODE_ALIASED,
					       D2D1::IdentityMatrix(),
					       1.0,
					       opacityBrush),
			 d2dsurf->maskLayer);

    target_rt->FillRectangle(rect,
			     brush);
    target_rt->PopLayer();

    if (target_rt.get() != d2dsurf->rt.get()) {
	return _cairo_d2d_blend_temp_surface(d2dsurf, op, target_rt, clip);
    }
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

    if (op == CAIRO_OPERATOR_SOURCE) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    RefPtr<ID2D1RenderTarget> target_rt = d2dsurf->rt;
#ifndef ALWAYS_MANUAL_COMPOSITE
    if (op != CAIRO_OPERATOR_OVER) {
#endif
	target_rt = _cairo_d2d_get_temp_rt(d2dsurf, clip);
	if (!target_rt) {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
#ifndef ALWAYS_MANUAL_COMPOSITE
    } else {
	_begin_draw_state(d2dsurf);
	status = (cairo_int_status_t)_cairo_d2d_set_clip (d2dsurf, clip);

	if (unlikely(status))
	    return status;
    }
#endif

    if (antialias == CAIRO_ANTIALIAS_NONE) {
	target_rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    } else {
	target_rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
    RefPtr<ID2D1StrokeStyle> strokeStyle = _cairo_d2d_create_strokestyle_for_stroke_style(style);

    if (!strokeStyle) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    D2D1::Matrix3x2F mat = _cairo_d2d_matrix_from_matrix(ctm);
    RefPtr<ID2D1Geometry> d2dpath = _cairo_d2d_create_path_geometry_for_path(path, 
		    							     CAIRO_FILL_RULE_WINDING, 
									     D2D1_FIGURE_BEGIN_FILLED);
    D2D1::Matrix3x2F inverse_mat = _cairo_d2d_invert_matrix(mat);
    
    RefPtr<ID2D1TransformedGeometry> trans_geom;
    sD2DFactory->CreateTransformedGeometry(d2dpath, &inverse_mat, &trans_geom);

    target_rt->SetTransform(mat);

    RefPtr<ID2D1Brush> brush = _cairo_d2d_create_brush_for_pattern(d2dsurf,
								   source);
    if (!brush) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    target_rt->DrawGeometry(trans_geom, brush, (FLOAT)style->line_width, strokeStyle);

    target_rt->SetTransform(D2D1::Matrix3x2F::Identity());

    if (target_rt.get() != d2dsurf->rt.get()) {
	D2D1_RECT_F bounds;
	trans_geom->GetWidenedBounds((FLOAT)style->line_width, strokeStyle, mat, &bounds);
	cairo_rectangle_int_t bound_rect;
	_cairo_d2d_round_out_to_int_rect(&bound_rect, bounds.left, bounds.top, bounds.right, bounds.bottom);
	return _cairo_d2d_blend_temp_surface(d2dsurf, op, target_rt, clip, &bound_rect);
    }

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
    cairo_box_t box;
    bool is_box = _cairo_path_fixed_is_box(path, &box);

    if (is_box && source->type == CAIRO_PATTERN_TYPE_SURFACE) {
	const cairo_surface_pattern_t *surf_pattern = 
	    reinterpret_cast<const cairo_surface_pattern_t*>(source);
	cairo_int_status_t rv = _cairo_d2d_try_fastblit(d2dsurf, surf_pattern->surface,
						        &box, &source->matrix, clip, op,
                                                        source->filter);

	if (rv != CAIRO_INT_STATUS_UNSUPPORTED) {
	    return rv;
	}
    }

    op = _cairo_d2d_simplify_operator(op, source);

    if (op == CAIRO_OPERATOR_SOURCE) {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (op == CAIRO_OPERATOR_CLEAR) {
	if (_cairo_path_fixed_is_box(path, &box)) {
	    _begin_draw_state(d2dsurf);
	    status = (cairo_int_status_t)_cairo_d2d_set_clip (d2dsurf, clip);

	    if (unlikely(status))
		return status;

	    return _cairo_d2d_clear_box (d2dsurf, clip, &box);
	} else {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    }

    RefPtr<ID2D1RenderTarget> target_rt = d2dsurf->rt;
    
#ifndef ALWAYS_MANUAL_COMPOSITE
    if (op != CAIRO_OPERATOR_OVER) {
#endif
	target_rt = _cairo_d2d_get_temp_rt(d2dsurf, clip);
	if (!target_rt) {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
#ifndef ALWAYS_MANUAL_COMPOSITE
    } else {
	_begin_draw_state(d2dsurf);
	status = (cairo_int_status_t)_cairo_d2d_set_clip (d2dsurf, clip);

	if (unlikely(status))
	    return status;
    }
#endif

    if (antialias == CAIRO_ANTIALIAS_NONE) {
	target_rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    } else {
	target_rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }

    if (is_box) {
	float x1 = _cairo_fixed_to_float(box.p1.x);
	float y1 = _cairo_fixed_to_float(box.p1.y);    
	float x2 = _cairo_fixed_to_float(box.p2.x);    
	float y2 = _cairo_fixed_to_float(box.p2.y);
	RefPtr<ID2D1Brush> brush = _cairo_d2d_create_brush_for_pattern(d2dsurf,
								       source);
	if (!brush) {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}

	target_rt->FillRectangle(D2D1::RectF(x1,
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
	target_rt->FillGeometry(d2dpath, brush);
    }

    if (target_rt.get() != d2dsurf->rt.get()) {
	double x1, y1, x2, y2;
	_cairo_path_fixed_bounds(path, &x1, &y1, &x2, &y2);
	cairo_rectangle_int_t bounds;
	_cairo_d2d_round_out_to_int_rect(&bounds, x1, y1, x2, y2);
	return _cairo_d2d_blend_temp_surface(d2dsurf, op, target_rt, clip, &bounds);
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
cairo_d2d_surface_create_for_hwnd(cairo_device_t *cairo_device,
				  HWND wnd,
				  cairo_content_t content)
{
    cairo_d2d_device_t *d2d_device = reinterpret_cast<cairo_d2d_device_t*>(cairo_device);
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
    ID3D10Device1 *device = d2d_device->mD3D10Device;
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
	                               IID_PPV_ARGS(&newSurf->surface));

    if (FAILED(hr)) {
	goto FAIL_HWND;
    }

    newSurf->surface->QueryInterface(&newSurf->backBuf);

    size.width = sizePixels.width * dpiX;
    size.height = sizePixels.height * dpiY;

    props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
					 D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
					 dpiX,
					 dpiY,
					 D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE);
    hr = sD2DFactory->CreateDxgiSurfaceRenderTarget(newSurf->backBuf,
								   props,
								   &newSurf->rt);
    if (FAILED(hr)) {
	goto FAIL_HWND;
    }

    bitProps = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, 
				      D2D1_ALPHA_MODE_PREMULTIPLIED));
    
    newSurf->rt->CreateSolidColorBrush(D2D1::ColorF(0, 1.0), &newSurf->solidColorBrush);

    _d2d_clear_surface(newSurf);

    newSurf->device = d2d_device;
    cairo_addref_device(cairo_device);
    d2d_device->mVRAMUsage += _cairo_d2d_compute_surface_mem_size(newSurf);

    return reinterpret_cast<cairo_surface_t*>(newSurf);

FAIL_HWND:
    newSurf->~cairo_d2d_surface_t();
    free(newSurf);
    return _cairo_surface_create_in_error(_cairo_error(CAIRO_STATUS_NO_MEMORY));
}

cairo_surface_t *
cairo_d2d_surface_create(cairo_device_t *device,
			 cairo_format_t format,
                         int width,
                         int height)
{
    if (width == 0 || height == 0) {
	return _cairo_surface_create_in_error(_cairo_error(CAIRO_STATUS_INVALID_SIZE));
    }

    cairo_d2d_device_t *d2d_device = reinterpret_cast<cairo_d2d_device_t*>(device);
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

    /* CreateTexture2D does not support D3D10_RESOURCE_MISC_GDI_COMPATIBLE with DXGI_FORMAT_A8_UNORM */
    if (desc.Format != DXGI_FORMAT_A8_UNORM)
	desc.MiscFlags = D3D10_RESOURCE_MISC_GDI_COMPATIBLE;

    RefPtr<ID3D10Texture2D> texture;
    RefPtr<IDXGISurface> dxgiSurface;
    D2D1_BITMAP_PROPERTIES bitProps;
    D2D1_RENDER_TARGET_PROPERTIES props;

    hr = d2d_device->mD3D10Device->CreateTexture2D(&desc, NULL, &texture);

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

    if (desc.MiscFlags & D3D10_RESOURCE_MISC_GDI_COMPATIBLE)
	props.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

    hr = sD2DFactory->CreateDxgiSurfaceRenderTarget(dxgiSurface,
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

    newSurf->device = d2d_device;
    cairo_addref_device(device);
    d2d_device->mVRAMUsage += _cairo_d2d_compute_surface_mem_size(newSurf);

    return reinterpret_cast<cairo_surface_t*>(newSurf);

FAIL_CREATE:
    newSurf->~cairo_d2d_surface_t();
    free(newSurf);
    return _cairo_surface_create_in_error(_cairo_error(CAIRO_STATUS_NO_MEMORY));
}

cairo_surface_t *
cairo_d2d_surface_create_for_handle(cairo_device_t *device, HANDLE handle, cairo_content_t content)
{
    if (!device) {
	return _cairo_surface_create_in_error(_cairo_error(CAIRO_STATUS_NO_DEVICE));
    }

    cairo_d2d_device_t *d2d_device = reinterpret_cast<cairo_d2d_device_t*>(device);
    cairo_d2d_surface_t *newSurf = static_cast<cairo_d2d_surface_t*>(malloc(sizeof(cairo_d2d_surface_t)));
    new (newSurf) cairo_d2d_surface_t();

    cairo_status_t status = CAIRO_STATUS_NO_MEMORY;
    HRESULT hr;
    RefPtr<ID3D10Texture2D> texture;
    RefPtr<IDXGISurface> dxgiSurface;
    D2D1_BITMAP_PROPERTIES bitProps;
    D2D1_RENDER_TARGET_PROPERTIES props;
    DXGI_FORMAT format;
    DXGI_SURFACE_DESC desc;

    hr = d2d_device->mD3D10Device->OpenSharedResource(handle,
						      __uuidof(ID3D10Resource),
						      (void**)&newSurf->surface);

    if (FAILED(hr)) {
	goto FAIL_CREATEHANDLE;
    }

    hr = newSurf->surface->QueryInterface(&dxgiSurface);

    if (FAILED(hr)) {
	goto FAIL_CREATEHANDLE;
    }

    dxgiSurface->GetDesc(&desc);
    format = desc.Format;
    
    D2D1_ALPHA_MODE alpha = D2D1_ALPHA_MODE_PREMULTIPLIED;
    if (format == DXGI_FORMAT_B8G8R8A8_UNORM) {
	if (content == CAIRO_CONTENT_ALPHA) {
	    status = CAIRO_STATUS_INVALID_CONTENT;
	    goto FAIL_CREATEHANDLE;
	}
	_cairo_surface_init(&newSurf->base, &cairo_d2d_surface_backend, content);
	if (content == CAIRO_CONTENT_COLOR) {
	    alpha = D2D1_ALPHA_MODE_IGNORE;
	}
    } else if (format == DXGI_FORMAT_A8_UNORM) {
	if (content != CAIRO_CONTENT_ALPHA) {
	    status = CAIRO_STATUS_INVALID_CONTENT;
	    goto FAIL_CREATEHANDLE;
	}
	_cairo_surface_init(&newSurf->base, &cairo_d2d_surface_backend, CAIRO_CONTENT_ALPHA);
    } else {
	status = CAIRO_STATUS_INVALID_FORMAT;
	// We don't know how to support this format!
	goto FAIL_CREATEHANDLE;
    }

    props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
					 D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, alpha));

    hr = sD2DFactory->CreateDxgiSurfaceRenderTarget(dxgiSurface,
						    props,
						    &newSurf->rt);

    if (FAILED(hr)) {
	goto FAIL_CREATEHANDLE;
    }

    bitProps = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, 
				      alpha));

    if (format != DXGI_FORMAT_A8_UNORM) {
	/* For some reason creation of shared bitmaps for A8 UNORM surfaces
	 * doesn't work even though the documentation suggests it does. The
	 * function will return an error if we try */
	hr = newSurf->rt->CreateSharedBitmap(IID_IDXGISurface,
					     dxgiSurface,
					     &bitProps,
					     &newSurf->surfaceBitmap);

	if (FAILED(hr)) {
	    goto FAIL_CREATEHANDLE;
	}
    }

    newSurf->rt->CreateSolidColorBrush(D2D1::ColorF(0, 1.0), &newSurf->solidColorBrush);

    newSurf->device = d2d_device;
    cairo_addref_device(device);
    d2d_device->mVRAMUsage += _cairo_d2d_compute_surface_mem_size(newSurf);

    return &newSurf->base;
   
FAIL_CREATEHANDLE:
    newSurf->~cairo_d2d_surface_t();
    free(newSurf);
    return _cairo_surface_create_in_error(_cairo_error(status));
}

cairo_surface_t *
cairo_d2d_surface_create_for_texture(cairo_device_t *device,
				     ID3D10Texture2D *texture,
				     cairo_content_t content)
{
    cairo_d2d_device_t *d2d_device = reinterpret_cast<cairo_d2d_device_t*>(device);
    cairo_d2d_surface_t *newSurf = static_cast<cairo_d2d_surface_t*>(malloc(sizeof(cairo_d2d_surface_t)));
    new (newSurf) cairo_d2d_surface_t();

    D2D1_ALPHA_MODE alpha = D2D1_ALPHA_MODE_PREMULTIPLIED;
    if (content == CAIRO_CONTENT_COLOR) {
	_cairo_surface_init(&newSurf->base, &cairo_d2d_surface_backend, CAIRO_CONTENT_COLOR);
	alpha = D2D1_ALPHA_MODE_IGNORE;
    } else {
	_cairo_surface_init(&newSurf->base, &cairo_d2d_surface_backend, content);
    }

    D2D1_SIZE_U sizePixels;
    HRESULT hr;

    D3D10_TEXTURE2D_DESC desc;
    RefPtr<IDXGISurface> dxgiSurface;
    D2D1_BITMAP_PROPERTIES bitProps;
    D2D1_RENDER_TARGET_PROPERTIES props;

    texture->GetDesc(&desc);

    sizePixels.width = desc.Width;
    sizePixels.height = desc.Height;

    newSurf->surface = texture;

    /** Create the DXGI surface. */
    hr = newSurf->surface->QueryInterface(IID_IDXGISurface, (void**)&dxgiSurface);
    if (FAILED(hr)) {
	goto FAIL_CREATE;
    }

    props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
					 D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, alpha));

    if (desc.MiscFlags & D3D10_RESOURCE_MISC_GDI_COMPATIBLE)
	props.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

    hr = sD2DFactory->CreateDxgiSurfaceRenderTarget(dxgiSurface,
						    props,
						    &newSurf->rt);

    if (FAILED(hr)) {
	goto FAIL_CREATE;
    }

    bitProps = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, 
				      alpha));

    if (content != CAIRO_CONTENT_ALPHA) {
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

    newSurf->device = d2d_device;
    cairo_addref_device(device);
    d2d_device->mVRAMUsage += _cairo_d2d_compute_surface_mem_size(newSurf);

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

    d2dsurf->device->mD3D10Device->CopyResource(texture, d2dsurf->surface);
    d2dsurf->device->mD3D10Device->CopySubresourceRegion(d2dsurf->surface,
						  0,
						  point.x,
						  point.y,
						  0,
						  texture,
						  0,
						  &rect);

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

    /* This QueryInterface call will always succeed even if the
     * the render target doesn't support the ID2D1GdiInteropRenderTarget
     * interface */
    d2dsurf->rt->QueryInterface(&interopRT);

    HDC dc;
    HRESULT rv;

    rv = interopRT->GetDC(retain_contents ? D2D1_DC_INITIALIZE_MODE_COPY :
	D2D1_DC_INITIALIZE_MODE_CLEAR, &dc);

    if (FAILED(rv)) {
	return NULL;
    }

    return dc;
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

int
cairo_d2d_get_surface_vram_usage(cairo_device_t *device)
{
    cairo_d2d_device_t *d2d_device = reinterpret_cast<cairo_d2d_device_t*>(device);
    return d2d_device->mVRAMUsage;
}
