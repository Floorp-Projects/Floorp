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
#ifndef CAIRO_D2D_PRIVATE_H
#define CAIRO_D2D_PRIVATE_H

#ifdef CAIRO_HAS_D2D_SURFACE

#include <windows.h>
#include <d2d1.h>
#include <d3d10.h>
#include <dxgi.h>

extern "C" {
#include "cairoint.h"
}

#include "cairo-win32-refptr.h"

struct _cairo_d2d_surface {
    _cairo_d2d_surface() : clipRect(NULL), clipping(false), isDrawing(false),
	textRenderingInit(true)
    { }
    ~_cairo_d2d_surface()
    {
	delete clipRect;
    }

    cairo_surface_t base;
    /** Render target of the texture we render to */
    RefPtr<ID2D1RenderTarget> rt;
    /** Surface containing our backstore */
    RefPtr<ID3D10Resource> surface;
    /** 
     * Surface used to temporarily store our surface if a bitmap isn't available
     * straight from our render target surface.
     */
    RefPtr<ID3D10Texture2D> bufferTexture;
    /** Backbuffer surface hwndrt renders to (NULL if not a window surface) */
    RefPtr<IDXGISurface> backBuf;
    /** Bitmap shared with texture and rendered to by rt */
    RefPtr<ID2D1Bitmap> surfaceBitmap;
    /** Swap chain holding our backbuffer (NULL if not a window surface) */
    RefPtr<IDXGISwapChain> dxgiChain;
    /** Window handle of the window we belong to */
    HWND hwnd;
    /** Format of the surface */
    cairo_format_t format;
    /** Geometry used for clipping when complex clipping is required */
    RefPtr<ID2D1Geometry> clipMask;
    /** Clip rectangle for axis aligned rectangular clips */
    D2D1_RECT_F *clipRect;
    /** Clip layer used for pushing geometry clip mask */
    RefPtr<ID2D1Layer> clipLayer;
    /** Mask layer used by surface_mask to push opacity masks */
    RefPtr<ID2D1Layer> maskLayer;
    /**
     * Layer used for clipping when tiling, and also for clearing out geometries
     * - lazily initialized 
     */
    RefPtr<ID2D1Layer> helperLayer;
    /** If this layer currently is clipping, used to prevent excessive push/pops */
    bool clipping;
    /** Brush used for bitmaps */
    RefPtr<ID2D1BitmapBrush> bitmapBrush;
    /** Brush used for solid colors */
    RefPtr<ID2D1SolidColorBrush> solidColorBrush;
    /** Indicates if our render target is currently in drawing mode */
    bool isDrawing;
    /** Indicates if text rendering is initialized */
    bool textRenderingInit;
};
typedef struct _cairo_d2d_surface cairo_d2d_surface_t;

typedef HRESULT (WINAPI*D2D1CreateFactoryFunc)(
    __in D2D1_FACTORY_TYPE factoryType,
    __in REFIID iid,
    __in_opt CONST D2D1_FACTORY_OPTIONS *pFactoryOptions,
    __out void **factory
);

typedef HRESULT (WINAPI*D3D10CreateDevice1Func)(
  IDXGIAdapter *pAdapter,
  D3D10_DRIVER_TYPE DriverType,
  HMODULE Software,
  UINT Flags,
  D3D10_FEATURE_LEVEL1 HardwareLevel,
  UINT SDKVersion,
  ID3D10Device1 **ppDevice
);

class D2DSurfFactory
{
public:
    static ID2D1Factory *Instance()
    {
	if (!mFactoryInstance) {
	    D2D1CreateFactoryFunc createD2DFactory = (D2D1CreateFactoryFunc)
		GetProcAddress(LoadLibraryW(L"d2d1.dll"), "D2D1CreateFactory");
	    if (createD2DFactory) {
		D2D1_FACTORY_OPTIONS options;
#ifdef DEBUG
		options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#else
		options.debugLevel = D2D1_DEBUG_LEVEL_NONE;
#endif
		createD2DFactory(
		    D2D1_FACTORY_TYPE_SINGLE_THREADED,
		    __uuidof(ID2D1Factory),
		    &options,
		    (void**)&mFactoryInstance);
	    }
	}
	return mFactoryInstance;
    }
private:
    static ID2D1Factory *mFactoryInstance;
};

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
class D3D10Factory
{
public:
    static ID3D10Device1 *Device()
    {
	if (!mDeviceInstance) {
	    D3D10CreateDevice1Func createD3DDevice = (D3D10CreateDevice1Func)
		GetProcAddress(LoadLibraryA("d3d10_1.dll"), "D3D10CreateDevice1");
	    if (createD3DDevice) {
		HRESULT hr = createD3DDevice(
		    NULL, 
		    D3D10_DRIVER_TYPE_HARDWARE,
		    NULL,
		    D3D10_CREATE_DEVICE_BGRA_SUPPORT |
		    D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
		    D3D10_FEATURE_LEVEL_10_1,
		    D3D10_1_SDK_VERSION,
		    &mDeviceInstance);
		if (FAILED(hr)) {
		    HRESULT hr = createD3DDevice(
			NULL, 
			D3D10_DRIVER_TYPE_HARDWARE,
			NULL,
			D3D10_CREATE_DEVICE_BGRA_SUPPORT |
			D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
			D3D10_FEATURE_LEVEL_10_0,
			D3D10_1_SDK_VERSION,
			&mDeviceInstance);
		    if (FAILED(hr)) {
			/* TODO: D3D10Level9 might be slower than GDI */
			HRESULT hr = createD3DDevice(
			    NULL, 
			    D3D10_DRIVER_TYPE_HARDWARE,
			    NULL,
			    D3D10_CREATE_DEVICE_BGRA_SUPPORT |
			    D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
			    D3D10_FEATURE_LEVEL_9_3,
			    D3D10_1_SDK_VERSION,
			    &mDeviceInstance);

		    }
		}
		if (SUCCEEDED(hr)) {
		    mDeviceInstance->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP);
		}
	    }
	}
	return mDeviceInstance;
    }
private:
    static ID3D10Device1 *mDeviceInstance;
};


RefPtr<ID2D1Brush>
_cairo_d2d_create_brush_for_pattern(cairo_d2d_surface_t *d2dsurf, 
			            const cairo_pattern_t *pattern,
				    unsigned int lastrun,
				    unsigned int *runs_remaining,
				    bool *pushed_clip,
				    bool unique = false);
#endif /* CAIRO_HAS_D2D_SURFACE */
#endif /* CAIRO_D2D_PRIVATE_H */
