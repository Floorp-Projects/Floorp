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
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is Novell.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   romaxa@gmail.com
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

#include "gfxQtNativeRenderer.h"
#include "gfxContext.h"
#include "gfxXlibSurface.h"

nsresult
gfxQtNativeRenderer::Draw(gfxContext* ctx, nsIntSize size,
                          PRUint32 flags, Screen* screen, Visual* visual,
                          DrawOutput* output)
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

    nsresult rv = DrawWithXlib(xsurf.get(), nsIntPoint(0, 0), NULL, 0);

    if (NS_FAILED(rv))
        return rv;

    ctx->SetSource(xsurf);
    ctx->Paint();

    return rv;
}
