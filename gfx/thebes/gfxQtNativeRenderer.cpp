/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxQtNativeRenderer.h"
#include "gfxContext.h"
#include "gfxXlibSurface.h"

nsresult
gfxQtNativeRenderer::Draw(gfxContext* ctx, nsIntSize size,
                          uint32_t flags, Screen* screen, Visual* visual)
{
    Display *dpy = DisplayOfScreen(screen);
    bool isOpaque = (flags & DRAW_IS_OPAQUE) ? true : false;
    int screenNumber = screen - ScreenOfDisplay(dpy, 0);

    if (!isOpaque) {
        int depth = 32;
        XVisualInfo vinfo;
        int foundVisual = XMatchVisualInfo(dpy, screenNumber,
                                           depth, TrueColor,
                                           &vinfo);
        if (!foundVisual)
            return NS_ERROR_FAILURE;

        visual = vinfo.visual;
    }

    nsRefPtr<gfxXlibSurface> xsurf =
        gfxXlibSurface::Create(screen, visual,
                               gfxIntSize(size.width, size.height));

    if (!isOpaque) {
        nsRefPtr<gfxContext> tempCtx = new gfxContext(xsurf);
        tempCtx->SetOperator(gfxContext::OPERATOR_CLEAR);
        tempCtx->Paint();
    }

    nsresult rv = DrawWithXlib(xsurf.get(), nsIntPoint(0, 0), nullptr, 0);

    if (NS_FAILED(rv))
        return rv;

    ctx->SetSource(xsurf);
    ctx->Paint();

    return rv;
}
