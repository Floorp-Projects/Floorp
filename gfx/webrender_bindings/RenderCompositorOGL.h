/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERCOMPOSITOR_OGL_H
#define MOZILLA_GFX_RENDERCOMPOSITOR_OGL_H

#include "GLTypes.h"
#include "mozilla/webrender/RenderCompositor.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {

namespace layers {
class NativeLayerRootSnapshotter;
class NativeLayerRoot;
class NativeLayer;
class SurfacePoolHandle;
}  // namespace layers

namespace wr {

class RenderCompositorOGL : public RenderCompositor {
 public:
  static UniquePtr<RenderCompositor> Create(
      RefPtr<widget::CompositorWidget>&& aWidget);

  RenderCompositorOGL(RefPtr<gl::GLContext>&& aGL,
                      RefPtr<widget::CompositorWidget>&& aWidget);
  virtual ~RenderCompositorOGL();

  bool BeginFrame() override;
  RenderedFrameId EndFrame(const nsTArray<DeviceIntRect>& aDirtyRects) final;
  bool WaitForGPU() override;
  void Pause() override;
  bool Resume() override;

  gl::GLContext* gl() const override { return mGL; }

  bool UseANGLE() const override { return false; }

  LayoutDeviceIntSize GetBufferSize() override;

  bool ShouldUseNativeCompositor() override;
  uint32_t GetMaxUpdateRects() override;

  // Does the readback for the ShouldUseNativeCompositor() case.
  bool MaybeReadback(const gfx::IntSize& aReadbackSize,
                     const wr::ImageFormat& aReadbackFormat,
                     const Range<uint8_t>& aReadbackBuffer) override;

  // Interface for wr::Compositor
  void CompositorBeginFrame() override;
  void CompositorEndFrame() override;
  void Bind(wr::NativeTileId aId, wr::DeviceIntPoint* aOffset, uint32_t* aFboId,
            wr::DeviceIntRect aDirtyRect,
            wr::DeviceIntRect aValidRect) override;
  void Unbind() override;
  void CreateSurface(wr::NativeSurfaceId aId, wr::DeviceIntPoint aVirtualOffset,
                     wr::DeviceIntSize aTileSize, bool aIsOpaque) override;
  void DestroySurface(NativeSurfaceId aId) override;
  void CreateTile(wr::NativeSurfaceId aId, int32_t aX, int32_t aY) override;
  void DestroyTile(wr::NativeSurfaceId aId, int32_t aX, int32_t aY) override;
  void AddSurface(wr::NativeSurfaceId aId, wr::DeviceIntPoint aPosition,
                  wr::DeviceIntRect aClipRect) override;
  CompositorCapabilities GetCompositorCapabilities() override;

  struct TileKey {
    TileKey(int32_t aX, int32_t aY) : mX(aX), mY(aY) {}

    int32_t mX;
    int32_t mY;
  };

 protected:
  void InsertFrameDoneSync();

  RefPtr<gl::GLContext> mGL;

  // Can be null.
  RefPtr<layers::NativeLayerRoot> mNativeLayerRoot;
  UniquePtr<layers::NativeLayerRootSnapshotter> mNativeLayerRootSnapshotter;
  RefPtr<layers::NativeLayer> mNativeLayerForEntireWindow;
  RefPtr<layers::SurfacePoolHandle> mSurfacePoolHandle;

  struct TileKeyHashFn {
    std::size_t operator()(const TileKey& aId) const {
      return HashGeneric(aId.mX, aId.mY);
    }
  };

  struct Surface {
    explicit Surface(wr::DeviceIntSize aTileSize, bool aIsOpaque)
        : mTileSize(aTileSize), mIsOpaque(aIsOpaque) {}
    gfx::IntSize TileSize() {
      return gfx::IntSize(mTileSize.width, mTileSize.height);
    }

    wr::DeviceIntSize mTileSize;
    bool mIsOpaque;
    std::unordered_map<TileKey, RefPtr<layers::NativeLayer>, TileKeyHashFn>
        mNativeLayers;
  };

  struct SurfaceIdHashFn {
    std::size_t operator()(const wr::NativeSurfaceId& aId) const {
      return HashGeneric(wr::AsUint64(aId));
    }
  };

  // Used in native compositor mode:
  RefPtr<layers::NativeLayer> mCurrentlyBoundNativeLayer;
  nsTArray<RefPtr<layers::NativeLayer>> mAddedLayers;
  uint64_t mTotalPixelCount = 0;
  uint64_t mAddedPixelCount = 0;
  uint64_t mAddedClippedPixelCount = 0;
  uint64_t mDrawnPixelCount = 0;
  gfx::IntRect mVisibleBounds;
  std::unordered_map<wr::NativeSurfaceId, Surface, SurfaceIdHashFn> mSurfaces;
  TimeStamp mBeginFrameTimeStamp;

  // Used to apply back-pressure in WaitForGPU().
  GLsync mPreviousFrameDoneSync;
  GLsync mThisFrameDoneSync;
};

static inline bool operator==(const RenderCompositorOGL::TileKey& a0,
                              const RenderCompositorOGL::TileKey& a1) {
  return a0.mX == a1.mX && a0.mY == a1.mY;
}

}  // namespace wr
}  // namespace mozilla

#endif
