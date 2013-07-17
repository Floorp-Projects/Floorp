/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathUtils.h"

#include "gfxQuartzNativeDrawing.h"
#include "gfxQuartzSurface.h"
#include "cairo-quartz.h"
#include "mozilla/gfx/2D.h"
// see cairo-quartz-surface.c for the complete list of these
enum {
    kPrivateCGCompositeSourceOver = 2
};

using namespace mozilla::gfx;
using namespace mozilla;

// private Quartz routine needed here
extern "C" {
    CG_EXTERN void CGContextSetCompositeOperation(CGContextRef, int);
}

gfxQuartzNativeDrawing::gfxQuartzNativeDrawing(gfxContext* ctx,
                                               const gfxRect& nativeRect,
                                               gfxFloat aBackingScale)
    : mContext(ctx)
    , mNativeRect(nativeRect)
    , mBackingScale(aBackingScale)
{
    mNativeRect.RoundOut();
}

CGContextRef
gfxQuartzNativeDrawing::BeginNativeDrawing()
{
    NS_ASSERTION(!mQuartzSurface, "BeginNativeDrawing called when drawing already in progress");

    if (!mContext->IsCairo()) {
      DrawTarget *dt = mContext->GetDrawTarget();
      if (mContext->GetDrawTarget()->IsDualDrawTarget()) {
        IntSize backingSize(NSToIntFloor(mNativeRect.width * mBackingScale),
                            NSToIntFloor(mNativeRect.height * mBackingScale));

       if (backingSize.IsEmpty())
          return nullptr;

        mDrawTarget = Factory::CreateDrawTarget(BACKEND_COREGRAPHICS, backingSize, FORMAT_B8G8R8A8);

        Matrix transform;
        transform.Scale(mBackingScale, mBackingScale);
        transform.Translate(-mNativeRect.x, -mNativeRect.y);

        mDrawTarget->SetTransform(transform);
        dt = mDrawTarget;
      }

      mCGContext = mBorrowedContext.Init(dt);
      MOZ_ASSERT(mCGContext);
      return mCGContext;
    }

    gfxPoint deviceOffset;
    nsRefPtr<gfxASurface> surf = mContext->CurrentSurface(&deviceOffset.x, &deviceOffset.y);
    if (!surf || surf->CairoStatus())
        return nullptr;

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
            return nullptr;

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
        nsIntSize backingSize(NSToIntFloor(mNativeRect.width * mBackingScale),
                              NSToIntFloor(mNativeRect.height * mBackingScale));
        mQuartzSurface = new gfxQuartzSurface(backingSize,
                                              gfxASurface::ImageFormatARGB32);
        if (mQuartzSurface->CairoStatus())
            return nullptr;
        mSurfaceContext = new gfxContext(mQuartzSurface);

        // grab the CGContextRef
        mCGContext = cairo_quartz_get_cg_context_with_clip(mSurfaceContext->GetCairo());
        CGContextScaleCTM(mCGContext, mBackingScale, mBackingScale);
        CGContextTranslateCTM(mCGContext, -mNativeRect.X(), -mNativeRect.Y());
    }

    return mCGContext;
}

void
gfxQuartzNativeDrawing::EndNativeDrawing()
{
    NS_ASSERTION(mCGContext, "EndNativeDrawing called without BeginNativeDrawing");

    if (mBorrowedContext.cg) {
        MOZ_ASSERT(!mContext->IsCairo());
        mBorrowedContext.Finish();
        if (mDrawTarget) {
          DrawTarget *dest = mContext->GetDrawTarget();
          RefPtr<SourceSurface> source = mDrawTarget->Snapshot();

          IntSize backingSize(NSToIntFloor(mNativeRect.width * mBackingScale),
                              NSToIntFloor(mNativeRect.height * mBackingScale));

          Matrix oldTransform = dest->GetTransform();
          Matrix newTransform = oldTransform;
          newTransform.Translate(mNativeRect.x, mNativeRect.y);
          newTransform.Scale(1.0f / mBackingScale, 1.0f / mBackingScale);

          dest->SetTransform(newTransform);

          dest->DrawSurface(source,
                            gfx::Rect(0, 0, backingSize.width, backingSize.height),
                            gfx::Rect(0, 0, backingSize.width, backingSize.height));


          dest->SetTransform(oldTransform);
        }
        return;
    }

    cairo_quartz_finish_cg_context_with_clip(mSurfaceContext->GetCairo());
    mQuartzSurface->MarkDirty();
    if (mSurfaceContext != mContext) {
        gfxContextMatrixAutoSaveRestore save(mContext);

        // Copy back to destination
        mContext->Translate(mNativeRect.TopLeft());
        mContext->Scale(1.0f / mBackingScale, 1.0f / mBackingScale);
        mContext->DrawSurface(mQuartzSurface, mQuartzSurface->GetSize());
    }
}
