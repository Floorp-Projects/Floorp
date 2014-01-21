/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxWindowsSurface.h"
#include "gfxContext.h"
#include "gfxPlatform.h"

#include "cairo.h"
#include "cairo-win32.h"

#include "nsString.h"

gfxWindowsSurface::gfxWindowsSurface(HWND wnd, uint32_t flags) :
    mOwnsDC(true), mForPrinting(false), mWnd(wnd)
{
    mDC = ::GetDC(mWnd);
    InitWithDC(flags);
}

gfxWindowsSurface::gfxWindowsSurface(HDC dc, uint32_t flags) :
    mOwnsDC(false), mForPrinting(false), mDC(dc), mWnd(nullptr)
{
    if (flags & FLAG_TAKE_DC)
        mOwnsDC = true;

#ifdef NS_PRINTING
    if (flags & FLAG_FOR_PRINTING) {
        Init(cairo_win32_printing_surface_create(mDC));
        mForPrinting = true;
    } else
#endif
    InitWithDC(flags);
}

gfxWindowsSurface::gfxWindowsSurface(IDirect3DSurface9 *surface, uint32_t flags) :
    mOwnsDC(false), mForPrinting(false), mDC(0), mWnd(nullptr)
{
    cairo_surface_t *surf = cairo_win32_surface_create_with_d3dsurface9(surface);
    Init(surf);
}


void
gfxWindowsSurface::MakeInvalid(gfxIntSize& size)
{
    size = gfxIntSize(-1, -1);
}

gfxWindowsSurface::gfxWindowsSurface(const gfxIntSize& realSize, gfxImageFormat imageFormat) :
    mOwnsDC(false), mForPrinting(false), mWnd(nullptr)
{
    gfxIntSize size(realSize);
    if (!CheckSurfaceSize(size))
        MakeInvalid(size);

    cairo_surface_t *surf = cairo_win32_surface_create_with_dib((cairo_format_t)imageFormat,
                                                                size.width, size.height);

    Init(surf);

    if (CairoStatus() == CAIRO_STATUS_SUCCESS) {
        mDC = cairo_win32_surface_get_dc(CairoSurface());
        RecordMemoryUsed(size.width * size.height * 4 + sizeof(gfxWindowsSurface));
    } else {
        mDC = nullptr;
    }
}

gfxWindowsSurface::gfxWindowsSurface(HDC dc, const gfxIntSize& realSize, gfxImageFormat imageFormat) :
    mOwnsDC(false), mForPrinting(false), mWnd(nullptr)
{
    gfxIntSize size(realSize);
    if (!CheckSurfaceSize(size))
        MakeInvalid(size);

    cairo_surface_t *surf = cairo_win32_surface_create_with_ddb(dc, (cairo_format_t)imageFormat,
                                                                size.width, size.height);

    Init(surf);

    if (mSurfaceValid) {
        // DDBs will generally only use 3 bytes per pixel when RGB24
        int bytesPerPixel = ((imageFormat == gfxImageFormatRGB24) ? 3 : 4);
        RecordMemoryUsed(size.width * size.height * bytesPerPixel + sizeof(gfxWindowsSurface));
    }

    if (CairoStatus() == 0)
        mDC = cairo_win32_surface_get_dc(CairoSurface());
    else
        mDC = nullptr;
}

gfxWindowsSurface::gfxWindowsSurface(cairo_surface_t *csurf) :
    mOwnsDC(false), mForPrinting(false), mWnd(nullptr)
{
    if (cairo_surface_status(csurf) == 0)
        mDC = cairo_win32_surface_get_dc(csurf);
    else
        mDC = nullptr;

    if (cairo_surface_get_type(csurf) == CAIRO_SURFACE_TYPE_WIN32_PRINTING)
        mForPrinting = true;

    Init(csurf, true);
}

void
gfxWindowsSurface::InitWithDC(uint32_t flags)
{
    if (flags & FLAG_IS_TRANSPARENT) {
        Init(cairo_win32_surface_create_with_alpha(mDC));
    } else {
        Init(cairo_win32_surface_create(mDC));
    }
}

already_AddRefed<gfxASurface>
gfxWindowsSurface::CreateSimilarSurface(gfxContentType aContent,
                                        const gfxIntSize& aSize)
{
    if (!mSurface || !mSurfaceValid) {
        return nullptr;
    }

    cairo_surface_t *surface;
    if (!mForPrinting && GetContentType() == GFX_CONTENT_COLOR_ALPHA) {
        // When creating a similar surface to a transparent surface, ensure
        // the new surface uses a DIB. cairo_surface_create_similar won't
        // use  a DIB for a GFX_CONTENT_COLOR surface if this surface doesn't
        // have a DIB (e.g. if we're a transparent window surface). But
        // we need a DIB to perform well if the new surface is composited into
        // a surface that's the result of create_similar(GFX_CONTENT_COLOR_ALPHA)
        // (e.g. a backbuffer for the window) --- that new surface *would*
        // have a DIB.
        surface =
          cairo_win32_surface_create_with_dib(cairo_format_t(gfxPlatform::GetPlatform()->OptimalFormatForContent(aContent)),
                                              aSize.width, aSize.height);
    } else {
        surface =
          cairo_surface_create_similar(mSurface, cairo_content_t(aContent),
                                       aSize.width, aSize.height);
    }

    if (cairo_surface_status(surface)) {
        cairo_surface_destroy(surface);
        return nullptr;
    }

    nsRefPtr<gfxASurface> result = Wrap(surface);
    cairo_surface_destroy(surface);
    return result.forget();
}

gfxWindowsSurface::~gfxWindowsSurface()
{
    if (mOwnsDC) {
        if (mWnd)
            ::ReleaseDC(mWnd, mDC);
        else
            ::DeleteDC(mDC);
    }
}

HDC
gfxWindowsSurface::GetDCWithClip(gfxContext *ctx)
{
    return cairo_win32_get_dc_with_clip (ctx->GetCairo());
}

HDC
gfxWindowsSurface::GetDC()
{
    return cairo_win32_surface_get_dc (CairoSurface());
}


already_AddRefed<gfxImageSurface>
gfxWindowsSurface::GetAsImageSurface()
{
    if (!mSurfaceValid) {
        NS_WARNING ("GetImageSurface on an invalid (null) surface; who's calling this without checking for surface errors?");
        return nullptr;
    }

    NS_ASSERTION(CairoSurface() != nullptr, "CairoSurface() shouldn't be nullptr when mSurfaceValid is TRUE!");

    if (mForPrinting)
        return nullptr;

    cairo_surface_t *isurf = cairo_win32_surface_get_image(CairoSurface());
    if (!isurf)
        return nullptr;

    nsRefPtr<gfxImageSurface> result = gfxASurface::Wrap(isurf).downcast<gfxImageSurface>();
    result->SetOpaqueRect(GetOpaqueRect());

    return result.forget();
}

nsresult
gfxWindowsSurface::BeginPrinting(const nsAString& aTitle,
                                 const nsAString& aPrintToFileName)
{
#ifdef NS_PRINTING
#define DOC_TITLE_LENGTH (MAX_PATH-1)
    DOCINFOW docinfo;

    nsString titleStr(aTitle);
    if (titleStr.Length() > DOC_TITLE_LENGTH) {
        titleStr.SetLength(DOC_TITLE_LENGTH-3);
        titleStr.AppendLiteral("...");
    }

    nsString docName(aPrintToFileName);
    docinfo.cbSize = sizeof(docinfo);
    docinfo.lpszDocName = titleStr.Length() > 0 ? titleStr.get() : L"Mozilla Document";
    docinfo.lpszOutput = docName.Length() > 0 ? docName.get() : nullptr;
    docinfo.lpszDatatype = nullptr;
    docinfo.fwType = 0;

    ::StartDocW(mDC, &docinfo);

    return NS_OK;
#else
    return NS_ERROR_FAILURE;
#endif
}

nsresult
gfxWindowsSurface::EndPrinting()
{
#ifdef NS_PRINTING
    int result = ::EndDoc(mDC);
    if (result <= 0)
        return NS_ERROR_FAILURE;

    return NS_OK;
#else
    return NS_ERROR_FAILURE;
#endif
}

nsresult
gfxWindowsSurface::AbortPrinting()
{
#ifdef NS_PRINTING
    int result = ::AbortDoc(mDC);
    if (result <= 0)
        return NS_ERROR_FAILURE;
    return NS_OK;
#else
    return NS_ERROR_FAILURE;
#endif
}

nsresult
gfxWindowsSurface::BeginPage()
{
#ifdef NS_PRINTING
    int result = ::StartPage(mDC);
    if (result <= 0)
        return NS_ERROR_FAILURE;
    return NS_OK;
#else
    return NS_ERROR_FAILURE;
#endif
}

nsresult
gfxWindowsSurface::EndPage()
{
#ifdef NS_PRINTING
    if (mForPrinting)
        cairo_surface_show_page(CairoSurface());
    int result = ::EndPage(mDC);
    if (result <= 0)
        return NS_ERROR_FAILURE;
    return NS_OK;
#else
    return NS_ERROR_FAILURE;
#endif
}

int32_t
gfxWindowsSurface::GetDefaultContextFlags() const
{
    if (mForPrinting)
        return gfxContext::FLAG_SIMPLIFY_OPERATORS |
               gfxContext::FLAG_DISABLE_SNAPPING |
               gfxContext::FLAG_DISABLE_COPY_BACKGROUND;

    return 0;
}

const gfxIntSize 
gfxWindowsSurface::GetSize() const
{
    if (!mSurfaceValid) {
        NS_WARNING ("GetImageSurface on an invalid (null) surface; who's calling this without checking for surface errors?");
        return gfxIntSize(-1, -1);
    }

    NS_ASSERTION(mSurface != nullptr, "CairoSurface() shouldn't be nullptr when mSurfaceValid is TRUE!");

    return gfxIntSize(cairo_win32_surface_get_width(mSurface),
                      cairo_win32_surface_get_height(mSurface));
}

gfxMemoryLocation
gfxWindowsSurface::GetMemoryLocation() const
{
    return GFX_MEMORY_IN_PROCESS_NONHEAP;
}
