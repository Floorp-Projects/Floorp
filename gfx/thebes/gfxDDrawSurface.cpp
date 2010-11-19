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

#include "gfxDDrawSurface.h"
#include "gfxContext.h"
#include "gfxPlatform.h"

#include "cairo.h"
#include "cairo-ddraw.h"

#include "nsString.h"

gfxDDrawSurface::gfxDDrawSurface(LPDIRECTDRAW lpdd,
                                 const gfxIntSize& size, gfxImageFormat imageFormat)
{
    if (!CheckSurfaceSize(size))
        return;

    cairo_surface_t *surf = cairo_ddraw_surface_create(lpdd, (cairo_format_t)imageFormat,
                                                       size.width, size.height);
    Init(surf);
}

gfxDDrawSurface::gfxDDrawSurface(gfxDDrawSurface * psurf, const RECT & rect)
{
    cairo_surface_t *surf = cairo_ddraw_surface_create_alias(psurf->CairoSurface(),
                                                             rect.left, rect.top,
                                                             rect.right - rect.left,
                                                             rect.bottom - rect.top);
    Init(surf);
}

gfxDDrawSurface::gfxDDrawSurface(cairo_surface_t *csurf)
{
    Init(csurf, PR_TRUE);
}

gfxDDrawSurface::~gfxDDrawSurface()
{
}

LPDIRECTDRAWSURFACE gfxDDrawSurface::GetDDSurface()
{
    return cairo_ddraw_surface_get_ddraw_surface(CairoSurface());
}

already_AddRefed<gfxImageSurface>
gfxDDrawSurface::GetAsImageSurface()
{
    cairo_surface_t *isurf = cairo_ddraw_surface_get_image(CairoSurface());
    if (!isurf)
        return nsnull;

    nsRefPtr<gfxASurface> asurf = gfxASurface::Wrap(isurf);
    gfxImageSurface *imgsurf = (gfxImageSurface*) asurf.get();
    NS_ADDREF(imgsurf);
    return imgsurf;
}

nsresult gfxDDrawSurface::BeginPrinting(const nsAString& aTitle,
                                          const nsAString& aPrintToFileName)
{
    return NS_OK;
}

nsresult gfxDDrawSurface::EndPrinting()
{
    return NS_OK;
}

nsresult gfxDDrawSurface::AbortPrinting()
{
    return NS_OK;
}

nsresult gfxDDrawSurface::BeginPage()
{
    return NS_OK;
}

nsresult gfxDDrawSurface::EndPage()
{
    return NS_OK;
}

PRInt32 gfxDDrawSurface::GetDefaultContextFlags() const
{
    return 0;
}
