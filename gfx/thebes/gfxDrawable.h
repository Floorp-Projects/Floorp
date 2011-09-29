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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Markus Stange.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef GFX_DRAWABLE_H
#define GFX_DRAWABLE_H

#include "nsISupportsImpl.h"
#include "nsAutoPtr.h"
#include "gfxTypes.h"
#include "gfxRect.h"
#include "gfxColor.h"
#include "gfxMatrix.h"
#include "gfxPattern.h"

class gfxASurface;
class gfxContext;

/**
 * gfxDrawable
 * An Interface representing something that has an intrinsic size and can draw
 * itself repeatedly.
 */
class THEBES_API gfxDrawable {
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
                        const gfxPattern::GraphicsFilter& aFilter,
                        const gfxMatrix& aTransform = gfxMatrix()) = 0;
    virtual gfxIntSize Size() { return mSize; }

protected:
    const gfxIntSize mSize;
};

/**
 * gfxSurfaceDrawable
 * A convenience implementation of gfxDrawable for surfaces.
 */
class THEBES_API gfxSurfaceDrawable : public gfxDrawable {
public:
    gfxSurfaceDrawable(gfxASurface* aSurface, const gfxIntSize aSize,
                       const gfxMatrix aTransform = gfxMatrix());
    virtual ~gfxSurfaceDrawable() {}

    virtual bool Draw(gfxContext* aContext,
                        const gfxRect& aFillRect,
                        bool aRepeat,
                        const gfxPattern::GraphicsFilter& aFilter,
                        const gfxMatrix& aTransform = gfxMatrix());

protected:
    nsRefPtr<gfxASurface> mSurface;
    const gfxMatrix mTransform;
};

/**
 * gfxDrawingCallback
 * A simple drawing functor.
 */
class THEBES_API gfxDrawingCallback {
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
                              const gfxPattern::GraphicsFilter& aFilter,
                              const gfxMatrix& aTransform = gfxMatrix()) = 0;

};

/**
 * gfxSurfaceDrawable
 * A convenience implementation of gfxDrawable for callbacks.
 */
class THEBES_API gfxCallbackDrawable : public gfxDrawable {
public:
    gfxCallbackDrawable(gfxDrawingCallback* aCallback, const gfxIntSize aSize);
    virtual ~gfxCallbackDrawable() {}

    virtual bool Draw(gfxContext* aContext,
                        const gfxRect& aFillRect,
                        bool aRepeat,
                        const gfxPattern::GraphicsFilter& aFilter,
                        const gfxMatrix& aTransform = gfxMatrix());

protected:
    already_AddRefed<gfxSurfaceDrawable> MakeSurfaceDrawable(const gfxPattern::GraphicsFilter aFilter = gfxPattern::FILTER_FAST);

    nsRefPtr<gfxDrawingCallback> mCallback;
    nsRefPtr<gfxSurfaceDrawable> mSurfaceDrawable;
};

/**
 * gfxPatternDrawable
 * A convenience implementation of gfxDrawable for patterns.
 */
class THEBES_API gfxPatternDrawable : public gfxDrawable {
public:
    gfxPatternDrawable(gfxPattern* aPattern,
                       const gfxIntSize aSize);
    virtual ~gfxPatternDrawable() {}

    virtual bool Draw(gfxContext* aContext,
                        const gfxRect& aFillRect,
                        bool aRepeat,
                        const gfxPattern::GraphicsFilter& aFilter,
                        const gfxMatrix& aTransform = gfxMatrix());

protected:
    already_AddRefed<gfxCallbackDrawable> MakeCallbackDrawable();

    nsRefPtr<gfxPattern> mPattern;
};

#endif /* GFX_DRAWABLE_H */
