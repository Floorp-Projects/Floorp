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
  explicit gfxReusableImageSurfaceWrapper(gfxImageSurface* aSurface);
protected:
  ~gfxReusableImageSurfaceWrapper();

public:
  const unsigned char* GetReadOnlyData() const override;
  gfxImageFormat Format() override;
  gfxReusableSurfaceWrapper* GetWritable(gfxImageSurface** aSurface) override;
  void ReadLock() override;
  void ReadUnlock() override;

  Type GetType() override
  {
    return TYPE_IMAGE;
  }

private:
  nsRefPtr<gfxImageSurface>         mSurface;
};

#endif // GFXMEMCOWSURFACEWRAPPER
