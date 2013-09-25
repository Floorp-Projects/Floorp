// vim:set ts=4 sts=4 sw=4 et cin:
/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SHARED_IMAGESURFACE_H
#define GFX_SHARED_IMAGESURFACE_H

#include "gfxBaseSharedMemorySurface.h"

class gfxSharedImageSurface : public gfxBaseSharedMemorySurface<gfxImageSurface, gfxSharedImageSurface>
{
  typedef gfxBaseSharedMemorySurface<gfxImageSurface, gfxSharedImageSurface> Super;
  friend class gfxBaseSharedMemorySurface<gfxImageSurface, gfxSharedImageSurface>;
private:
    gfxSharedImageSurface(const gfxIntSize& aSize, long aStride, 
                          gfxImageFormat aFormat, 
                          const mozilla::ipc::Shmem& aShmem)
      : Super(aSize, aStride, aFormat, aShmem)
    {}
};

#endif /* GFX_SHARED_IMAGESURFACE_H */
