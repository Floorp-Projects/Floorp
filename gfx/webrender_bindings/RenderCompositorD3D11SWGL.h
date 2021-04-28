/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERCOMPOSITOR_D3D11_H
#define MOZILLA_GFX_RENDERCOMPOSITOR_D3D11_H

#include "mozilla/layers/ScreenshotGrabber.h"
#include "mozilla/layers/TextureD3D11.h"
#include "mozilla/layers/CompositorD3D11.h"
#include "mozilla/webrender/RenderCompositorLayersSWGL.h"

namespace mozilla {

namespace wr {

class SurfaceD3D11SWGL;

class RenderCompositorD3D11SWGL : public RenderCompositorLayersSWGL {
 public:
  static UniquePtr<RenderCompositor> Create(
      const RefPtr<widget::CompositorWidget>& aWidget, nsACString& aError);

  RenderCompositorD3D11SWGL(layers::CompositorD3D11* aCompositor,
                            const RefPtr<widget::CompositorWidget>& aWidget,
                            void* aContext);
  virtual ~RenderCompositorD3D11SWGL();

  void Pause() override;
  bool Resume() override;

  GLenum IsContextLost(bool aForce) override;

  layers::WebRenderCompositor CompositorType() const override {
    return layers::WebRenderCompositor::D3D11;
  }
  RenderCompositorD3D11SWGL* AsRenderCompositorD3D11SWGL() override {
    return this;
  }

  bool BeginFrame() override;

  bool MaybeReadback(const gfx::IntSize& aReadbackSize,
                     const wr::ImageFormat& aReadbackFormat,
                     const Range<uint8_t>& aReadbackBuffer,
                     bool* aNeedsYFlip) override;

  layers::CompositorD3D11* GetCompositorD3D11() {
    return mCompositor->AsCompositorD3D11();
  }

  ID3D11Device* GetDevice() { return GetCompositorD3D11()->GetDevice(); }

 private:
  already_AddRefed<ID3D11Texture2D> CreateStagingTexture(
      const gfx::IntSize aSize);
  already_AddRefed<DataSourceSurface> CreateStagingSurface(
      const gfx::IntSize aSize);

  void HandleExternalImage(RenderTextureHost* aExternalImage,
                           FrameSurface& aFrameSurface) override;
  UniquePtr<RenderCompositorLayersSWGL::Surface> DoCreateSurface(
      wr::DeviceIntSize aTileSize, bool aIsOpaque) override;
  UniquePtr<RenderCompositorLayersSWGL::Tile> DoCreateTile(
      Surface* aSurface) override;

  class TileD3D11 : public RenderCompositorLayersSWGL::Tile {
   public:
    TileD3D11(layers::DataTextureSourceD3D11* aTexture,
              ID3D11Texture2D* aStagingTexture,
              DataSourceSurface* aDataSourceSurface, Surface* aOwner,
              RenderCompositorD3D11SWGL* aRenderCompositor);
    virtual ~TileD3D11() {}

    bool Map(wr::DeviceIntRect aDirtyRect, wr::DeviceIntRect aValidRect,
             void** aData, int32_t* aStride) override;
    void Unmap(const gfx::IntRect& aDirtyRect) override;
    layers::DataTextureSource* GetTextureSource() override { return mTexture; }
    bool IsValid() override;

   private:
    RefPtr<layers::DataTextureSourceD3D11> mTexture;
    RefPtr<ID3D11Texture2D> mStagingTexture;
    RefPtr<DataSourceSurface> mSurface;
    SurfaceD3D11SWGL* mOwner;
    RenderCompositorD3D11SWGL* mRenderCompositor;
  };

  enum UploadMode {
    Upload_Immediate,
    Upload_Staging,
    Upload_StagingNoBlock,
    Upload_StagingPooled
  };
  UploadMode GetUploadMode();
  UploadMode mUploadMode = Upload_Staging;

  RefPtr<ID3D11Texture2D> mCurrentStagingTexture;
  bool mCurrentStagingTextureIsTemp = false;
};

class SurfaceD3D11SWGL : public RenderCompositorLayersSWGL::Surface {
 public:
  SurfaceD3D11SWGL(wr::DeviceIntSize aTileSize, bool aIsOpaque);
  virtual ~SurfaceD3D11SWGL() {}

  SurfaceD3D11SWGL* AsSurfaceD3D11SWGL() override { return this; }

  nsTArray<RefPtr<ID3D11Texture2D>> mStagingPool;
};

}  // namespace wr
}  // namespace mozilla

#endif
