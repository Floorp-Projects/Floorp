/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SurfaceFactory.h"

#include "SharedSurface.h"

namespace mozilla {
namespace gfx {

SurfaceFactory::~SurfaceFactory()
{
    while (!mScraps.empty()) {
        SharedSurface* cur = mScraps.front();
        mScraps.pop();

        delete cur;
    }
}

SharedSurface*
SurfaceFactory::NewSharedSurface(const gfxIntSize& size)
{
    // Attempt to reuse an old surface.
    while (!mScraps.empty()) {
        SharedSurface* cur = mScraps.front();
        mScraps.pop();
        if (cur->Size() == size)
            return cur;

        // Destroy old surfaces of the wrong size.
        delete cur;
    }

    SharedSurface* ret = CreateShared(size);

    return ret;
}

// Auto-deletes surfs of the wrong type.
void
SurfaceFactory::Recycle(SharedSurface*& surf)
{
    if (!surf)
        return;

    if (surf->Type() == mType) {
        mScraps.push(surf);
    } else {
        delete surf;
    }

    surf = nullptr;
}

} /* namespace gfx */
} /* namespace mozilla */
