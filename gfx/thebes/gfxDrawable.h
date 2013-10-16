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

class gfxASurface;
class gfxImageSurface;
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
    gfxDrawable(const gfxIntSize aSize)
     : mSize(aSize) {}
    virtual ~gfxDrawable() {}

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
                        const gfxMatrix& aTransform = gfxMatrix()) = 0;
    virtual already_AddRefed<gfxImageSurface> GetAsImageSurface() { return nullptr; }
    virtual gfxIntSize Size() { return mSize; }

protected:
    const gfxIntSize mSize;
};

/**
 * gfxSurfaceDrawable
 * A convenience implementation of gfxDrawable for surfaces.
 */
class gfxSurfaceDrawable : public gfxDrawable {
public:
    gfxSurfaceDrawable(gfxASurface* aSurface, const gfxIntSize aSize,
                       const gfxMatrix aTransform = gfxMatrix());
    virtual ~gfxSurfaceDrawable() {}

    virtual bool Draw(gfxContext* aContext,
                        const gfxRect& aFillRect,
                        bool aRepeat,
                        const GraphicsFilter& aFilter,
                        const gfxMatrix& aTransform = gfxMatrix());
    
    virtual already_AddRefed<gfxImageSurface> GetAsImageSurface();

protected:
    nsRefPtr<gfxASurface> mSurface;
    const gfxMatrix mTransform;
};

/**
 * gfxDrawingCallback
 * A simple drawing functor.
 */
class gfxDrawingCallback {
    NS_INLINE_DECL_REFCOUNTING(gfxDrawingCallback)
public:
    virtual ~gfxDrawingCallback() {}

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
                        const gfxMatrix& aTransform = gfxMatrix());

protected:
    already_AddRefed<gfxCallbackDrawable> MakeCallbackDrawable();

    nsRefPtr<gfxPattern> mPattern;
};

#endif /* GFX_DRAWABLE_H */
