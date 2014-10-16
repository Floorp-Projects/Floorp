/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_X11TEXTURESOURCEBASIC__H
#define MOZILLA_GFX_X11TEXTURESOURCEBASIC__H

#include "mozilla/layers/BasicCompositor.h"
#include "mozilla/layers/TextureHostBasic.h"
#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace layers {

class BasicCompositor;

// TextureSource for Xlib-backed surfaces.
class X11TextureSourceBasic
  : public TextureSourceBasic
  , public TextureSource
{
public:
  X11TextureSourceBasic(BasicCompositor* aCompositor, gfxXlibSurface* aSurface);

  virtual X11TextureSourceBasic* AsSourceBasic() MOZ_OVERRIDE { return this; }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;

  virtual gfx::SourceSurface* GetSurface(gfx::DrawTarget* aTarget) MOZ_OVERRIDE;

  virtual void DeallocateDeviceData() MOZ_OVERRIDE { }

  virtual void SetCompositor(Compositor* aCompositor);

  static gfx::SurfaceFormat ContentTypeToSurfaceFormat(gfxContentType aType);

protected:
  RefPtr<BasicCompositor> mCompositor;
  RefPtr<gfxXlibSurface> mSurface;
  RefPtr<gfx::SourceSurface> mSourceSurface;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_X11TEXTURESOURCEBASIC__H
