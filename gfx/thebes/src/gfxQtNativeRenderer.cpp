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

#include "gfxQtPlatform.h"

#include "cairo.h"
#include <QWidget>

typedef struct {
    gfxQtNativeRenderer* mRenderer;
    nsresult               mRV;
} NativeRenderingClosure;


static cairo_bool_t
NativeRendering(void *closure,
                QWidget * drawable,
                short offset_x, short offset_y,
                QRect * rectangles, unsigned int num_rects)
{
    NativeRenderingClosure* cl = (NativeRenderingClosure*)closure;
    nsresult rv = cl->mRenderer->
        NativeDraw(drawable, offset_x, offset_y,
                   rectangles, num_rects);
    cl->mRV = rv;
    return NS_SUCCEEDED(rv);
}


nsresult
gfxQtNativeRenderer::Draw(gfxContext* ctx, int width, int height,
                          PRUint32 flags, DrawOutput* output)
{
    NativeRenderingClosure closure = { this, NS_OK };

    if (output) {
        output->mSurface = NULL;
        output->mUniformAlpha = PR_FALSE;
        output->mUniformColor = PR_FALSE;
    }

#if 0 // FIXME

    cairo_gdk_drawing_result_t result;
    // Make sure result.surface is null to start with; we rely on it
    // being non-null meaning that a surface actually got allocated.
    result.surface = NULL;

    int cairoFlags = 0;
    if (flags & DRAW_SUPPORTS_OFFSET) {
        cairoFlags |= CAIRO_GDK_DRAWING_SUPPORTS_OFFSET;
    }
    if (flags & DRAW_SUPPORTS_CLIP_RECT) {
        cairoFlags |= CAIRO_GDK_DRAWING_SUPPORTS_CLIP_RECT;
    }
    if (flags & DRAW_SUPPORTS_CLIP_LIST) {
        cairoFlags |= CAIRO_GDK_DRAWING_SUPPORTS_CLIP_LIST;
    }
    if (flags & DRAW_SUPPORTS_ALTERNATE_SCREEN) {
        cairoFlags |= CAIRO_GDK_DRAWING_SUPPORTS_ALTERNATE_SCREEN;
    }
    if (flags & DRAW_SUPPORTS_NONDEFAULT_VISUAL) {
        cairoFlags |= CAIRO_GDK_DRAWING_SUPPORTS_NONDEFAULT_VISUAL;
    }

    cairo_draw_with_gdk(ctx->GetCairo(),
                        gfxPlatformGtk::GetPlatform()->GetGdkDrawable(ctx->OriginalSurface()),
                        NativeRendering, 
                        &closure, width, height,
                        (flags & DRAW_IS_OPAQUE) ? CAIRO_GDK_DRAWING_OPAQUE : CAIRO_GDK_DRAWING_TRANSPARENT,
                        (cairo_gdk_drawing_support_t)cairoFlags,
                        output ? &result : NULL);

    if (NS_FAILED(closure.mRV)) {
        if (result.surface) {
            NS_ASSERTION(output, "How did that happen?");
            cairo_surface_destroy (result.surface);
        }
        return closure.mRV;
    }

    if (output) {
        if (result.surface) {
            output->mSurface = gfxASurface::Wrap(result.surface);
            if (!output->mSurface) {
                cairo_surface_destroy (result.surface);
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }

        output->mUniformAlpha = result.uniform_alpha;
        output->mUniformColor = result.uniform_color;
        output->mColor = gfxRGBA(result.r, result.g, result.b, result.alpha);
    }
#endif

    return NS_OK;
}
