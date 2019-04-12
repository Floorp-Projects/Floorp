/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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
class X11DataTextureSourceBasic : public DataTextureSource,
                                  public TextureSourceBasic {
 public:
  X11DataTextureSourceBasic(){};

  const char* Name() const override { return "X11DataTextureSourceBasic"; }

  bool Update(gfx::DataSourceSurface* aSurface,
              nsIntRegion* aDestRegion = nullptr,
              gfx::IntPoint* aSrcOffset = nullptr) override;

  TextureSourceBasic* AsSourceBasic() override;

  gfx::SourceSurface* GetSurface(gfx::DrawTarget* aTarget) override;

  void DeallocateDeviceData() override;

  gfx::IntSize GetSize() const override;

  gfx::SurfaceFormat GetFormat() const override;

 private:
  // We are going to buffer layer content on this xlib draw target
  RefPtr<mozilla::gfx::DrawTarget> mBufferDrawTarget;
};

class X11BasicCompositor : public BasicCompositor {
 public:
  explicit X11BasicCompositor(CompositorBridgeParent* aParent,
                              widget::CompositorWidget* aWidget)
      : BasicCompositor(aParent, aWidget) {}

  already_AddRefed<DataTextureSource> CreateDataTextureSource(
      TextureFlags aFlags = TextureFlags::NO_FLAGS) override;

  already_AddRefed<DataTextureSource> CreateDataTextureSourceAround(
      gfx::DataSourceSurface* aSurface) override {
    return nullptr;
  }

  void EndFrame() override;
};

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_X11BASICCOMPOSITOR_H */
