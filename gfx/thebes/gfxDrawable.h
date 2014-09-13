/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_DRAWABLE_H
#define GFX_DRAWABLE_H

#include "nsAutoPtr.h"
#include "gfxRect.h"
#include "gfxMatrix.h"
#include "GraphicsFilter.h"
#include "mozilla/gfx/2D.h"

class gfxASurface;
class gfxContext;
class gfxPattern;

/**
 * gfxDrawable
 * An Interface representing something that has an intrinsic size and can draw
 * itself repeatedly.
 */
class gfxDrawable {
    NS_INLINE_DECL_REFCOUNTING(gfxDrawable)
public:
    explicit gfxDrawable(const gfxIntSize aSize)
     : mSize(aSize) {}

    /**
     * Draw into aContext filling aFillRect, possibly repeating, using aFilter.
     * aTransform is a userspace to "image"space matrix. For example, if Draw
     * draws using a gfxPattern, this is the matrix that should be set on the
     * pattern prior to rendering it.
     *  @return whether drawing was successful
     */
    virtual bool Draw(gfxContext* aContext,
                        const gfxRect& aFillRect,
                        bool aRepeat,
                        const GraphicsFilter& aFilter,
                        gfxFloat aOpacity = 1.0,
                        const gfxMatrix& aTransform = gfxMatrix()) = 0;
    virtual bool DrawWithSamplingRect(gfxContext* aContext,
                                      const gfxRect& aFillRect,
                                      const gfxRect& aSamplingRect,
                                      bool aRepeat,
                                      const GraphicsFilter& aFilter,
                                      gfxFloat aOpacity = 1.0)
    {
        return false;
    }

    virtual gfxIntSize Size() { return mSize; }

protected:
    // Protected destructor, to discourage deletion outside of Release():
    virtual ~gfxDrawable() {}

    const gfxIntSize mSize;
};

/**
 * gfxSurfaceDrawable
 * A convenience implementation of gfxDrawable for surfaces.
 */
class gfxSurfaceDrawable : public gfxDrawable {
public:
    gfxSurfaceDrawable(mozilla::gfx::SourceSurface* aSurface, const gfxIntSize aSize,
                       const gfxMatrix aTransform = gfxMatrix());
    virtual ~gfxSurfaceDrawable() {}

    virtual bool Draw(gfxContext* aContext,
                        const gfxRect& aFillRect,
                        bool aRepeat,
                        const GraphicsFilter& aFilter,
                        gfxFloat aOpacity = 1.0,
                        const gfxMatrix& aTransform = gfxMatrix());
    virtual bool DrawWithSamplingRect(gfxContext* aContext,
                                      const gfxRect& aFillRect,
                                      const gfxRect& aSamplingRect,
                                      bool aRepeat,
                                      const GraphicsFilter& aFilter,
                                      gfxFloat aOpacity = 1.0);
    
protected:
    void DrawInternal(gfxContext* aContext,
                      const gfxRect& aFillRect,
                      const mozilla::gfx::IntRect& aSamplingRect,
                      bool aRepeat,
                      const GraphicsFilter& aFilter,
                      gfxFloat aOpacity,
                      const gfxMatrix& aTransform = gfxMatrix());

    mozilla::RefPtr<mozilla::gfx::SourceSurface> mSourceSurface;
    const gfxMatrix mTransform;
};

/**
 * gfxDrawingCallback
 * A simple drawing functor.
 */
class gfxDrawingCallback {
    NS_INLINE_DECL_REFCOUNTING(gfxDrawingCallback)
protected:
    // Protected destructor, to discourage deletion outside of Release():
    virtual ~gfxDrawingCallback() {}

public:
    /**
     * Draw into aContext filling aFillRect using aFilter.
     * aTransform is a userspace to "image"space matrix. For example, if Draw
     * draws using a gfxPattern, this is the matrix that should be set on the
     * pattern prior to rendering it.
     *  @return whether drawing was successful
     */
    virtual bool operator()(gfxContext* aContext,
                              const gfxRect& aFillRect,
                              const GraphicsFilter& aFilter,
                              const gfxMatrix& aTransform = gfxMatrix()) = 0;

};

/**
 * gfxCallbackDrawable
 * A convenience implementation of gfxDrawable for callbacks.
 */
class gfxCallbackDrawable : public gfxDrawable {
public:
    gfxCallbackDrawable(gfxDrawingCallback* aCallback, const gfxIntSize aSize);
    virtual ~gfxCallbackDrawable() {}

    virtual bool Draw(gfxContext* aContext,
                        const gfxRect& aFillRect,
                        bool aRepeat,
                        const GraphicsFilter& aFilter,
                        gfxFloat aOpacity = 1.0,
                        const gfxMatrix& aTransform = gfxMatrix());

protected:
    already_AddRefed<gfxSurfaceDrawable> MakeSurfaceDrawable(const GraphicsFilter aFilter = GraphicsFilter::FILTER_FAST);

    nsRefPtr<gfxDrawingCallback> mCallback;
    nsRefPtr<gfxSurfaceDrawable> mSurfaceDrawable;
};

/**
 * gfxPatternDrawable
 * A convenience implementation of gfxDrawable for patterns.
 */
class gfxPatternDrawable : public gfxDrawable {
public:
    gfxPatternDrawable(gfxPattern* aPattern,
                       const gfxIntSize aSize);
    virtual ~gfxPatternDrawable();

    virtual bool Draw(gfxContext* aContext,
                        const gfxRect& aFillRect,
                        bool aRepeat,
                        const GraphicsFilter& aFilter,
                        gfxFloat aOpacity = 1.0,
                        const gfxMatrix& aTransform = gfxMatrix());

protected:
    already_AddRefed<gfxCallbackDrawable> MakeCallbackDrawable();

    nsRefPtr<gfxPattern> mPattern;
};

#endif /* GFX_DRAWABLE_H */
