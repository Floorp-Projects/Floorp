/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SURFACE_FACTORY_H_
#define SURFACE_FACTORY_H_

#include <queue>
#include "SurfaceTypes.h"
#include "gfxPoint.h"

namespace mozilla {
namespace gfx {
// Forward:
class SharedSurface;

class SurfaceFactory
{
protected:
    SurfaceCaps mCaps;
    SharedSurfaceType mType;

    SurfaceFactory(SharedSurfaceType type, const SurfaceCaps& caps)
        : mCaps(caps)
        , mType(type)
    {}

public:
    virtual ~SurfaceFactory();

protected:
    virtual SharedSurface* CreateShared(const gfx::IntSize& size) = 0;

    std::queue<SharedSurface*> mScraps;

public:
    SharedSurface* NewSharedSurface(const gfx::IntSize& size);

    // Auto-deletes surfs of the wrong type.
    void Recycle(SharedSurface*& surf);

    const SurfaceCaps& Caps() const {
        return mCaps;
    }

    SharedSurfaceType Type() const {
        return mType;
    }
};

}   // namespace gfx
}   // namespace mozilla

#endif  // SURFACE_FACTORY_H_
