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
 * The Original Code is Thebes gfx.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matthew Gregan <kinetik@flim.org>
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

#include "nsMathUtils.h"

#include "gfxQuartzNativeDrawing.h"
#include "gfxQuartzSurface.h"
#include "cairo-quartz.h"
// see cairo-quartz-surface.c for the complete list of these
enum {
    kPrivateCGCompositeSourceOver = 2
};

// private Quartz routine needed here
extern "C" {
    CG_EXTERN void CGContextSetCompositeOperation(CGContextRef, int);
}

gfxQuartzNativeDrawing::gfxQuartzNativeDrawing(gfxContext* ctx,
                                               const gfxRect& nativeRect)
    : mContext(ctx), mNativeRect(nativeRect)
{
    mNativeRect.RoundOut();
}

CGContextRef
gfxQuartzNativeDrawing::BeginNativeDrawing()
{
    NS_ASSERTION(!mQuartzSurface, "BeginNativeDrawing called when drawing already in progress");

    gfxPoint deviceOffset;
    nsRefPtr<gfxASurface> surf = mContext->CurrentSurface(&deviceOffset.x, &deviceOffset.y);
    if (!surf || surf->CairoStatus())
        return nsnull;

    // if this is a native Quartz surface, we don't have to redirect
    // rendering to our own CGContextRef; in most cases, we are able to
    // use the CGContextRef from the surface directly.  we can extend
    // this to support offscreen drawing fairly easily in the future.
    if (surf->GetType() == gfxASurface::SurfaceTypeQuartz &&
        (surf->GetContentType() == gfxASurface::CONTENT_COLOR ||
         (surf->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA))) {
        mQuartzSurface = static_cast<gfxQuartzSurface*>(surf.get());
        mSurfaceContext = mContext;

        // grab the CGContextRef
        mCGContext = cairo_quartz_get_cg_context_with_clip(mSurfaceContext->GetCairo());
        if (!mCGContext)
            return nsnull;

        gfxMatrix m = mContext->CurrentMatrix();
        CGContextTranslateCTM(mCGContext, deviceOffset.x, deviceOffset.y);

        // I -think- that this context will always have an identity
        // transform (since we don't maintain a transform on it in
        // cairo-land, and instead push/pop as needed)

        gfxFloat x0 = m.x0;
        gfxFloat y0 = m.y0;

        // We round x0/y0 if we don't have a scale, because otherwise things get
        // rendered badly
        // XXX how should we be rounding x0/y0?
        if (!m.HasNonTranslationOrFlip()) {
            x0 = floor(x0 + 0.5);
            y0 = floor(y0 + 0.5);
        }

        CGContextConcatCTM(mCGContext, CGAffineTransformMake(m.xx, m.yx,
                                                             m.xy, m.yy,
                                                             x0, y0));

        // bug 382049 - need to explicity set the composite operation to sourceOver
        CGContextSetCompositeOperation(mCGContext, kPrivateCGCompositeSourceOver);
    } else {
        mQuartzSurface = new gfxQuartzSurface(mNativeRect.size,
                                              gfxASurface::ImageFormatARGB32);
        if (mQuartzSurface->CairoStatus())
            return nsnull;
        mSurfaceContext = new gfxContext(mQuartzSurface);

        // grab the CGContextRef
        mCGContext = cairo_quartz_get_cg_context_with_clip(mSurfaceContext->GetCairo());
        CGContextTranslateCTM(mCGContext, -mNativeRect.X(), -mNativeRect.Y());
    }

    return mCGContext;
}

void
gfxQuartzNativeDrawing::EndNativeDrawing()
{
    NS_ASSERTION(mQuartzSurface, "EndNativeDrawing called without BeginNativeDrawing");

    cairo_quartz_finish_cg_context_with_clip(mSurfaceContext->GetCairo());
    mQuartzSurface->MarkDirty();
    if (mSurfaceContext != mContext) {
        gfxContextMatrixAutoSaveRestore save(mContext);

        // Copy back to destination
        mContext->Translate(mNativeRect.TopLeft());
        mContext->DrawSurface(mQuartzSurface, mNativeRect.size);
    }
}
