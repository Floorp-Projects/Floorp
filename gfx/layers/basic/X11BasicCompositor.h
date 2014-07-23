/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_X11BASICCOMPOSITOR_H
#define MOZILLA_GFX_X11BASICCOMPOSITOR_H

#include "mozilla/layers/BasicCompositor.h"
#include "mozilla/layers/X11TextureSourceBasic.h"
#include "mozilla/layers/TextureHostBasic.h"
#include "gfxXlibSurface.h"
#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace layers {

// TextureSource for Image-backed surfaces.
class X11DataTextureSourceBasic : public DataTextureSource
                                , public TextureSourceBasic
{
public:
  X11DataTextureSourceBasic() {};

  virtual bool Update(gfx::DataSourceSurface* aSurface,
                      nsIntRegion* aDestRegion = nullptr,
                      gfx::IntPoint* aSrcOffset = nullptr) MOZ_OVERRIDE;

  virtual TextureSourceBasic* AsSourceBasic() MOZ_OVERRIDE;

  virtual gfx::SourceSurface* GetSurface(gfx::DrawTarget* aTarget) MOZ_OVERRIDE;

  virtual void DeallocateDeviceData() MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;

private:
  // We are going to buffer layer content on this xlib draw target
  RefPtr<mozilla::gfx::DrawTarget> mBufferDrawTarget;
};

class X11BasicCompositor : public BasicCompositor
{
public:

  X11BasicCompositor(nsIWidget *aWidget) : BasicCompositor(aWidget) {}

  virtual TemporaryRef<DataTextureSource>
  CreateDataTextureSource(TextureFlags aFlags = TextureFlags::NO_FLAGS) MOZ_OVERRIDE;
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_X11BASICCOMPOSITOR_H */
