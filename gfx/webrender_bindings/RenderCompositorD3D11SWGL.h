/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERCOMPOSITOR_D3D11_H
#define MOZILLA_GFX_RENDERCOMPOSITOR_D3D11_H

#include "mozilla/webrender/RenderCompositor.h"
#include "mozilla/layers/TextureD3D11.h"
#include "mozilla/layers/CompositorD3D11.h"

namespace mozilla {

namespace wr {

class RenderCompositorD3D11SWGL : public RenderCompositor {
 public:
  static UniquePtr<RenderCompositor> Create(
      RefPtr<widget::CompositorWidget>&& aWidget, nsACString& aError);

  RenderCompositorD3D11SWGL(layers::CompositorD3D11* aCompositor,
                            RefPtr<widget::CompositorWidget>&& aWidget,
                            void* aContext);
  virtual ~RenderCompositorD3D11SWGL();

  void* swgl() const override { return mContext; }

  bool MakeCurrent() override;

  bool BeginFrame() override;
  void CancelFrame() override;
  RenderedFrameId EndFrame(const nsTArray<DeviceIntRect>& aDirtyRects) final;

  void Pause() override;
  bool Resume() override;

  bool SurfaceOriginIsTopLeft() override { return true; }

  LayoutDeviceIntSize GetBufferSize() override;

  // Should we support this?
  bool SupportsExternalBufferTextures() const override { return false; }

  layers::WebRenderBackend BackendType() const override {
    return layers::WebRenderBackend::SOFTWARE;
  }
  layers::WebRenderCompositor CompositorType() const override {
    return layers::WebRenderCompositor::D3D11;
  }

  // Interface for wr::Compositor
  CompositorCapabilities GetCompositorCapabilities() override;

  bool ShouldUseNativeCompositor() override { return true; }

  void CompositorBeginFrame() {}
  void CompositorEndFrame();
  void Bind(wr::NativeTileId aId, wr::DeviceIntPoint* aOffset, uint32_t* aFboId,
            wr::DeviceIntRect aDirtyRect,
            wr::DeviceIntRect aValidRect) override;
  void Unbind() override;
  bool MapTile(wr::NativeTileId aId, wr::DeviceIntRect aDirtyRect,
               wr::DeviceIntRect aValidRect, void** aData,
               int32_t* aStride) override;
  void UnmapTile() override;
  void CreateSurface(wr::NativeSurfaceId aId, wr::DeviceIntPoint aVirtualOffset,
                     wr::DeviceIntSize aTileSize, bool aIsOpaque) override;
  void CreateExternalSurface(wr::NativeSurfaceId aId, bool aIsOpaque) override {
  }
  void DestroySurface(NativeSurfaceId aId) override;
  void CreateTile(wr::NativeSurfaceId, int32_t aX, int32_t aY) override;
  void DestroyTile(wr::NativeSurfaceId, int32_t aX, int32_t aY) override;
  void AttachExternalImage(wr::NativeSurfaceId aId,
                           wr::ExternalImageId aExternalImage) override {}
  void AddSurface(wr::NativeSurfaceId aId,
                  const wr::CompositorSurfaceTransform& aTransform,
                  wr::DeviceIntRect aClipRect,
                  wr::ImageRendering aImageRendering) override;
  void EnableNativeCompositor(bool aEnable) {}
  void DeInit() override {}

  bool MaybeReadback(const gfx::IntSize& aReadbackSize,
                     const wr::ImageFormat& aReadbackFormat,
                     const Range<uint8_t>& aReadbackBuffer,
                     bool* aNeedsYFlip) override;

  // TODO: Screenshots etc

  struct TileKey {
    TileKey(int32_t aX, int32_t aY) : mX(aX), mY(aY) {}

    int32_t mX;
    int32_t mY;
  };

 private:
  RefPtr<layers::CompositorD3D11> mCompositor;
  void* mContext = nullptr;

  struct Tile {
    // Each tile retains a texture, and a DataSourceSurface of the
    // same size. We draw into the source surface, and then copy the
    // changed area into the texture.
    // TODO: We should investigate using a D3D11 USAGE_STAGING texture
    // that we map for the upload instead, to see if it performs better.
    RefPtr<layers::DataTextureSourceD3D11> mTexture;
    RefPtr<DataSourceSurface> mSurface;
    gfx::Rect mValidRect;

    struct KeyHashFn {
      std::size_t operator()(const TileKey& aId) const {
        return HashGeneric(aId.mX, aId.mY);
      }
    };
  };

  struct Surface {
    explicit Surface(wr::DeviceIntSize aTileSize, bool aIsOpaque)
        : mTileSize(aTileSize), mIsOpaque(aIsOpaque) {}
    gfx::IntSize TileSize() {
      return gfx::IntSize(mTileSize.width, mTileSize.height);
    }

    // External images can change size depending on which image
    // is attached, so mTileSize will be 0,0 when mIsExternal
    // is true.
    wr::DeviceIntSize mTileSize;
    bool mIsOpaque;
    bool mIsExternal = false;
    std::unordered_map<TileKey, Tile, Tile::KeyHashFn> mTiles;

    struct IdHashFn {
      std::size_t operator()(const wr::NativeSurfaceId& aId) const {
        return HashGeneric(wr::AsUint64(aId));
      }
    };
  };
  std::unordered_map<wr::NativeSurfaceId, Surface, Surface::IdHashFn> mSurfaces;

  // Temporary state held between MapTile and UnmapTile
  Tile mCurrentTile;
  gfx::IntRect mCurrentTileDirty;

  // The set of surfaces added to be composited for the current frame
  struct FrameSurface {
    wr::NativeSurfaceId mId;
    gfx::Matrix4x4 mTransform;
    gfx::IntRect mClipRect;
    gfx::SamplingFilter mFilter;
  };
  nsTArray<FrameSurface> mFrameSurfaces;
};

static inline bool operator==(const RenderCompositorD3D11SWGL::TileKey& a0,
                              const RenderCompositorD3D11SWGL::TileKey& a1) {
  return a0.mX == a1.mX && a0.mY == a1.mY;
}

}  // namespace wr
}  // namespace mozilla

#endif
