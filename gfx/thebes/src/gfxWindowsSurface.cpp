/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "gfxWindowsSurface.h"
#include "gfxContext.h"
#include "gfxPlatform.h"

#include "cairo.h"
#include "cairo-win32.h"

#include "nsString.h"

gfxWindowsSurface::gfxWindowsSurface(HWND wnd) :
    mOwnsDC(PR_TRUE), mForPrinting(PR_FALSE), mWnd(wnd)
{
    mDC = ::GetDC(mWnd);
    Init(cairo_win32_surface_create(mDC));
}

gfxWindowsSurface::gfxWindowsSurface(HDC dc, PRUint32 flags) :
    mOwnsDC(PR_FALSE), mForPrinting(PR_FALSE), mDC(dc), mWnd(nsnull)
{
    if (flags & FLAG_TAKE_DC)
        mOwnsDC = PR_TRUE;

#ifdef NS_PRINTING
    if (flags & FLAG_FOR_PRINTING) {
        Init(cairo_win32_printing_surface_create(mDC));
        mForPrinting = PR_TRUE;
    } else
#endif
        Init(cairo_win32_surface_create(mDC));
}

gfxWindowsSurface::gfxWindowsSurface(const gfxIntSize& size, gfxImageFormat imageFormat) :
    mOwnsDC(PR_FALSE), mForPrinting(PR_FALSE), mWnd(nsnull)
{
    if (!CheckSurfaceSize(size))
        return;

    cairo_surface_t *surf = cairo_win32_surface_create_with_dib((cairo_format_t)imageFormat,
                                                                size.width, size.height);
    Init(surf);

    if (CairoStatus() == 0)
        mDC = cairo_win32_surface_get_dc(CairoSurface());
    else
        mDC = nsnull;
}

gfxWindowsSurface::gfxWindowsSurface(HDC dc, const gfxIntSize& size, gfxImageFormat imageFormat) :
    mOwnsDC(PR_FALSE), mForPrinting(PR_FALSE), mWnd(nsnull)
{
    if (!CheckSurfaceSize(size))
        return;

    cairo_surface_t *surf = cairo_win32_surface_create_with_ddb(dc, (cairo_format_t)imageFormat,
                                                                size.width, size.height);
    Init(surf);

    if (CairoStatus() == 0)
        mDC = cairo_win32_surface_get_dc(CairoSurface());
    else
        mDC = nsnull;
}


gfxWindowsSurface::gfxWindowsSurface(cairo_surface_t *csurf) :
    mOwnsDC(PR_FALSE), mForPrinting(PR_FALSE), mWnd(nsnull)
{
    if (cairo_surface_status(csurf) == 0)
        mDC = cairo_win32_surface_get_dc(csurf);
    else
        mDC = nsnull;

    if (cairo_surface_get_type(csurf) == CAIRO_SURFACE_TYPE_WIN32_PRINTING)
        mForPrinting = PR_TRUE;

    Init(csurf, PR_TRUE);
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

already_AddRefed<gfxImageSurface>
gfxWindowsSurface::GetImageSurface()
{
    if (!mSurfaceValid) {
        NS_WARNING ("GetImageSurface on an invalid (null) surface; who's calling this without checking for surface errors?");
        return nsnull;
    }

    NS_ASSERTION(CairoSurface() != nsnull, "CairoSurface() shouldn't be nsnull when mSurfaceValid is TRUE!");

    if (mForPrinting)
        return nsnull;

    cairo_surface_t *isurf = cairo_win32_surface_get_image(CairoSurface());
    if (!isurf)
        return nsnull;

    nsRefPtr<gfxASurface> asurf = gfxASurface::Wrap(isurf);
    gfxImageSurface *imgsurf = (gfxImageSurface*) asurf.get();
    NS_ADDREF(imgsurf);
    return imgsurf;
}

already_AddRefed<gfxWindowsSurface>
gfxWindowsSurface::OptimizeToDDB(HDC dc, const gfxIntSize& size, gfxImageFormat format)
{
    if (mForPrinting)
        return nsnull;

    if (format != ImageFormatRGB24)
        return nsnull;

    nsRefPtr<gfxWindowsSurface> wsurf = new gfxWindowsSurface(dc, size, format);
    if (wsurf->CairoStatus() != 0)
        return nsnull;

    gfxContext tmpCtx(wsurf);
    tmpCtx.SetOperator(gfxContext::OPERATOR_SOURCE);
    tmpCtx.SetSource(this);
    tmpCtx.Paint();

    gfxWindowsSurface *raw = (gfxWindowsSurface*) (wsurf.get());
    NS_ADDREF(raw);
    return raw;
}

nsresult gfxWindowsSurface::BeginPrinting(const nsAString& aTitle,
                                          const nsAString& aPrintToFileName)
{
#define DOC_TITLE_LENGTH 30
    DOCINFOW docinfo;

    nsString titleStr(aTitle);
    if (titleStr.Length() > DOC_TITLE_LENGTH) {
        titleStr.SetLength(DOC_TITLE_LENGTH-3);
        titleStr.AppendLiteral("...");
    }

    nsString docName(aPrintToFileName);
    docinfo.cbSize = sizeof(docinfo);
    docinfo.lpszDocName = titleStr.Length() > 0 ? titleStr.get() : L"Mozilla Document";
    docinfo.lpszOutput = docName.Length() > 0 ? docName.get() : nsnull;
    docinfo.lpszDatatype = NULL;
    docinfo.fwType = 0;

    ::StartDocW(mDC, &docinfo);

    return NS_OK;
}

nsresult gfxWindowsSurface::EndPrinting()
{
    int result = ::EndDoc(mDC);
    if (result <= 0)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

nsresult gfxWindowsSurface::AbortPrinting()
{
    int result = ::AbortDoc(mDC);
    if (result <= 0)
        return NS_ERROR_FAILURE;
    return NS_OK;
}

nsresult gfxWindowsSurface::BeginPage()
{
    int result = ::StartPage(mDC);
    if (result <= 0)
        return NS_ERROR_FAILURE;
    return NS_OK;
}

nsresult gfxWindowsSurface::EndPage()
{
    if (mForPrinting)
        cairo_surface_show_page(CairoSurface());
    int result = ::EndPage(mDC);
    if (result <= 0)
        return NS_ERROR_FAILURE;
    return NS_OK;
}

PRInt32 gfxWindowsSurface::GetDefaultContextFlags() const
{
    if (mForPrinting)
        return gfxContext::FLAG_SIMPLIFY_OPERATORS |
               gfxContext::FLAG_DISABLE_SNAPPING;

    return 0;
}
