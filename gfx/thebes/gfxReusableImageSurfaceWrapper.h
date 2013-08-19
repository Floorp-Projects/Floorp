/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFXMEMCOWSURFACEWRAPPER
#define GFXMEMCOWSURFACEWRAPPER

#include "gfxReusableSurfaceWrapper.h"

class gfxImageSurface;

/**
 * A cross-thread capable implementation of gfxReusableSurfaceWrapper based
 * on gfxImageSurface.
 */
class gfxReusableImageSurfaceWrapper : public gfxReusableSurfaceWrapper {
public:
  gfxReusableImageSurfaceWrapper(gfxImageSurface* aSurface);
  ~gfxReusableImageSurfaceWrapper();

  const unsigned char* GetReadOnlyData() const MOZ_OVERRIDE;
  gfxASurface::gfxImageFormat Format() MOZ_OVERRIDE;
  gfxReusableSurfaceWrapper* GetWritable(gfxImageSurface** aSurface) MOZ_OVERRIDE;
  void ReadLock() MOZ_OVERRIDE;
  void ReadUnlock() MOZ_OVERRIDE;

  Type GetType()
  {
    return TYPE_IMAGE;
  }

private:
  nsRefPtr<gfxImageSurface>         mSurface;
};

#endif // GFXMEMCOWSURFACEWRAPPER
