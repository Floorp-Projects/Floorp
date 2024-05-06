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

#include "cairoint.h"
#include "cairo-win32-refptr.hpp"
#include <dwrite_3.h>
#include <d2d1.h>

#ifdef __MINGW32__
#include "dw-extra.h"
#else
typedef DWRITE_COLOR_GLYPH_RUN1 DWRITE_COLOR_GLYPH_RUN1_WORKAROUND;
#endif

/* If d2d1_3.h header required for color fonts is not available,
 * include our own version containing just the functions we need.
 */

#if HAVE_D2D1_3_H
#include <d2d1_3.h>
#else
#include "d2d1-extra.h"
#endif

// DirectWrite is not available on all platforms.
typedef HRESULT (WINAPI*DWriteCreateFactoryFunc)(
  DWRITE_FACTORY_TYPE factoryType,
  REFIID iid,
  IUnknown **factory
);

/* #cairo_scaled_font_t implementation */
struct _cairo_dwrite_scaled_font {
    cairo_scaled_font_t base;
    cairo_matrix_t mat;
    cairo_matrix_t mat_inverse;
    cairo_antialias_t antialias_mode;
    IDWriteRenderingParams *rendering_params;
    DWRITE_MEASURING_MODE measuring_mode;
};
typedef struct _cairo_dwrite_scaled_font cairo_dwrite_scaled_font_t;

class DWriteFactory
{
public:
    static RefPtr<IDWriteFactory> Instance()
    {
	if (!mFactoryInstance) {
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
	    DWriteCreateFactoryFunc createDWriteFactory = (DWriteCreateFactoryFunc)
		GetProcAddress(LoadLibraryW(L"dwrite.dll"), "DWriteCreateFactory");
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
	    if (createDWriteFactory) {
		HRESULT hr = createDWriteFactory(
		    DWRITE_FACTORY_TYPE_SHARED,
		    __uuidof(IDWriteFactory),
		    reinterpret_cast<IUnknown**>(&mFactoryInstance));
		assert(SUCCEEDED(hr));
	    }
	}
	return mFactoryInstance;
    }

    static RefPtr<IDWriteFactory1> Instance1()
    {
	if (!mFactoryInstance1) {
	    if (Instance()) {
		Instance()->QueryInterface(&mFactoryInstance1);
	    }
	}
	return mFactoryInstance1;
    }

    static RefPtr<IDWriteFactory2> Instance2()
    {
	if (!mFactoryInstance2) {
	    if (Instance()) {
		Instance()->QueryInterface(&mFactoryInstance2);
	    }
	}
	return mFactoryInstance2;
    }

    static RefPtr<IDWriteFactory3> Instance3()
    {
	if (!mFactoryInstance3) {
	    if (Instance()) {
		Instance()->QueryInterface(&mFactoryInstance3);
	    }
	}
	return mFactoryInstance3;
    }

    static RefPtr<IDWriteFactory4> Instance4()
    {
	if (!mFactoryInstance4) {
	    if (Instance()) {
		Instance()->QueryInterface(&mFactoryInstance4);
	    }
	}
	return mFactoryInstance4;
    }

    static RefPtr<IDWriteFontCollection> SystemCollection()
    {
	if (!mSystemCollection) {
	    if (Instance()) {
		HRESULT hr = Instance()->GetSystemFontCollection(&mSystemCollection);
		assert(SUCCEEDED(hr));
	    }
	}
	return mSystemCollection;
    }

    static RefPtr<IDWriteFontFamily> FindSystemFontFamily(const WCHAR *aFamilyName)
    {
	UINT32 idx;
	BOOL found;
	if (!SystemCollection()) {
	    return NULL;
	}
	SystemCollection()->FindFamilyName(aFamilyName, &idx, &found);
	if (!found) {
	    return NULL;
	}

	RefPtr<IDWriteFontFamily> family;
	SystemCollection()->GetFontFamily(idx, &family);
	return family;
    }

    static RefPtr<IDWriteRenderingParams> DefaultRenderingParams()
    {
	if (!mDefaultRenderingParams) {
	    if (Instance()) {
		Instance()->CreateRenderingParams(&mDefaultRenderingParams);
	    }
	}
	return mDefaultRenderingParams;
    }

private:
    static RefPtr<IDWriteFactory> mFactoryInstance;
    static RefPtr<IDWriteFactory1> mFactoryInstance1;
    static RefPtr<IDWriteFactory2> mFactoryInstance2;
    static RefPtr<IDWriteFactory3> mFactoryInstance3;
    static RefPtr<IDWriteFactory4> mFactoryInstance4;
    static RefPtr<IDWriteFontCollection> mSystemCollection;
    static RefPtr<IDWriteRenderingParams> mDefaultRenderingParams;
};

class AutoDWriteGlyphRun : public DWRITE_GLYPH_RUN
{
    static const int kNumAutoGlyphs = 256;

public:
    AutoDWriteGlyphRun() {
        glyphCount = 0;
    }

    ~AutoDWriteGlyphRun() {
        if (glyphCount > kNumAutoGlyphs) {
            delete[] glyphIndices;
            delete[] glyphAdvances;
            delete[] glyphOffsets;
        }
    }

    void allocate(int aNumGlyphs) {
        glyphCount = aNumGlyphs;
        if (aNumGlyphs <= kNumAutoGlyphs) {
            glyphIndices = &mAutoIndices[0];
            glyphAdvances = &mAutoAdvances[0];
            glyphOffsets = &mAutoOffsets[0];
        } else {
            glyphIndices = new UINT16[aNumGlyphs];
            glyphAdvances = new FLOAT[aNumGlyphs];
            glyphOffsets = new DWRITE_GLYPH_OFFSET[aNumGlyphs];
        }
    }

private:
    DWRITE_GLYPH_OFFSET mAutoOffsets[kNumAutoGlyphs];
    FLOAT               mAutoAdvances[kNumAutoGlyphs];
    UINT16              mAutoIndices[kNumAutoGlyphs];
};

/* #cairo_font_face_t implementation */
struct _cairo_dwrite_font_face {
    cairo_font_face_t base;
    IDWriteFontFace *dwriteface; /* Can't use RefPtr because this struct is malloc'd.  */
    cairo_bool_t have_color;
    IDWriteRenderingParams *rendering_params; /* Can't use RefPtr because this struct is malloc'd.  */
    DWRITE_MEASURING_MODE measuring_mode;
};
typedef struct _cairo_dwrite_font_face cairo_dwrite_font_face_t;
