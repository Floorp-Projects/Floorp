// vim:set ts=4 sts=4 sw=4 et cin:
/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SHARED_QUARTZSURFACE_H
#define GFX_SHARED_QUARTZSURFACE_H

#include "gfxBaseSharedMemorySurface.h"
#include "gfxQuartzSurface.h"

class gfxSharedQuartzSurface : public gfxBaseSharedMemorySurface<gfxQuartzSurface, gfxSharedQuartzSurface>
{
  typedef gfxBaseSharedMemorySurface<gfxQuartzSurface, gfxSharedQuartzSurface> Super;
  friend class gfxBaseSharedMemorySurface<gfxQuartzSurface, gfxSharedQuartzSurface>;
private:
    gfxSharedQuartzSurface(const gfxIntSize& aSize, long aStride, 
                           gfxImageFormat aFormat, 
                           const mozilla::ipc::Shmem& aShmem)
      : Super(aSize, aStride, aFormat, aShmem)
    {}
};

#endif /* GFX_SHARED_QUARTZSURFACE_H */
