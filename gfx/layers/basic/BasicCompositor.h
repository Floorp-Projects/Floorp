/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BASICCOMPOSITOR_H
#define MOZILLA_GFX_BASICCOMPOSITOR_H

#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Triangle.h"
#include "mozilla/gfx/Polygon.h"

namespace mozilla {
namespace layers {

class BasicCompositingRenderTarget : public CompositingRenderTarget {
 public:
  BasicCompositingRenderTarget(gfx::DrawTarget* aDrawTarget,
                               const gfx::IntRect& aRect)
      : CompositingRenderTarget(aRect.TopLeft()),
        mDrawTarget(aDrawTarget),
        mSize(aRect.Size()) {}

  const char* Name() const override { return "BasicCompositingRenderTarget"; }

  gfx::IntSize GetSize() const override { return mSize; }

  void BindRenderTarget();

  gfx::SurfaceFormat GetFormat() const override {
    return mDrawTarget ? mDrawTarget->GetFormat()
                       : gfx::SurfaceFormat(gfx::SurfaceFormat::UNKNOWN);
  }

  RefPtr<gfx::DrawTarget> mDrawTarget;
  gfx::IntSize mSize;
};

class BasicCompositor : public Compositor {
 public:
  BasicCompositor(CompositorBridgeParent* aParent,
                  widget::CompositorWidget* aWidget);

 protected:
  virtual ~BasicCompositor();

 public:
  BasicCompositor* AsBasicCompositor() override { return this; }

  bool Initialize(nsCString* const out_failureReason) override;

  void Destroy() override;

  TextureFactoryIdentifier GetTextureFactoryIdentifier() override;

  already_AddRefed<CompositingRenderTarget> CreateRenderTarget(
      const gfx::IntRect& aRect, SurfaceInitMode aInit) override;

  already_AddRefed<CompositingRenderTarget> CreateRenderTargetFromSource(
      const gfx::IntRect& aRect, const CompositingRenderTarget* aSource,
      const gfx::IntPoint& aSourcePoint) override;

  virtual already_AddRefed<CompositingRenderTarget> CreateRenderTargetForWindow(
      const LayoutDeviceIntRect& aRect,
      const LayoutDeviceIntRegion& aClearRegion, BufferMode aBufferMode);

  already_AddRefed<DataTextureSource> CreateDataTextureSource(
      TextureFlags aFlags = TextureFlags::NO_FLAGS) override;

  already_AddRefed<DataTextureSource> CreateDataTextureSourceAround(
      gfx::DataSourceSurface* aSurface) override;

  already_AddRefed<DataTextureSource> CreateDataTextureSourceAroundYCbCr(
      TextureHost* aTexture) override;

  bool SupportsEffect(EffectTypes aEffect) override;

  bool SupportsLayerGeometry() const override;

  bool ReadbackRenderTarget(CompositingRenderTarget* aSource,
                            AsyncReadbackBuffer* aDest) override;

  already_AddRefed<AsyncReadbackBuffer> CreateAsyncReadbackBuffer(
      const gfx::IntSize& aSize) override;

  bool BlitRenderTarget(CompositingRenderTarget* aSource,
                        const gfx::IntSize& aSourceSize,
                        const gfx::IntSize& aDestSize) override;

  void SetRenderTarget(CompositingRenderTarget* aSource) override {
    mRenderTarget = static_cast<BasicCompositingRenderTarget*>(aSource);
    mRenderTarget->BindRenderTarget();
  }

  already_AddRefed<CompositingRenderTarget> GetWindowRenderTarget()
      const override {
    return do_AddRef(mFullWindowRenderTarget);
  }

  already_AddRefed<CompositingRenderTarget> GetCurrentRenderTarget()
      const override {
    return do_AddRef(mRenderTarget);
  }

  void DrawQuad(const gfx::Rect& aRect, const gfx::IntRect& aClipRect,
                const EffectChain& aEffectChain, gfx::Float aOpacity,
                const gfx::Matrix4x4& aTransform,
                const gfx::Rect& aVisibleRect) override;

  void ClearRect(const gfx::Rect& aRect) override;

  void BeginFrame(const nsIntRegion& aInvalidRegion,
                  const gfx::IntRect* aClipRectIn,
                  const gfx::IntRect& aRenderBounds,
                  const nsIntRegion& aOpaqueRegion,
                  gfx::IntRect* aClipRectOut = nullptr,
                  gfx::IntRect* aRenderBoundsOut = nullptr) override;
  void NormalDrawingDone() override;
  void EndFrame() override;

  bool SupportsPartialTextureUpdate() override { return true; }
  bool CanUseCanvasLayerForSize(const gfx::IntSize& aSize) override {
    return true;
  }
  int32_t GetMaxTextureSize() const override;
  void SetDestinationSurfaceSize(const gfx::IntSize& aSize) override {}

  void SetScreenRenderOffset(const ScreenPoint& aOffset) override {}

  void MakeCurrent(MakeCurrentFlags aFlags = 0) override {}

#ifdef MOZ_DUMP_PAINTING
  const char* Name() const override { return "Basic"; }
#endif  // MOZ_DUMP_PAINTING

  LayersBackend GetBackendType() const override {
    return LayersBackend::LAYERS_BASIC;
  }

  gfx::DrawTarget* GetDrawTarget() { return mDrawTarget; }

  bool IsPendingComposite() override { return mIsPendingEndRemoteDrawing; }

  void FinishPendingComposite() override;

  virtual void RequestAllowFrameRecording(bool aWillRecord) override {
    mRecordFrames = aWillRecord;
  }

 private:
  template <typename Geometry>
  void DrawGeometry(const Geometry& aGeometry, const gfx::Rect& aRect,
                    const gfx::IntRect& aClipRect,
                    const EffectChain& aEffectChain, gfx::Float aOpacity,
                    const gfx::Matrix4x4& aTransform,
                    const gfx::Rect& aVisibleRect, const bool aEnableAA);

  void DrawPolygon(const gfx::Polygon& aPolygon, const gfx::Rect& aRect,
                   const gfx::IntRect& aClipRect,
                   const EffectChain& aEffectChain, gfx::Float aOpacity,
                   const gfx::Matrix4x4& aTransform,
                   const gfx::Rect& aVisibleRect) override;

  void TryToEndRemoteDrawing(bool aForceToEnd = false);

  bool NeedsToDeferEndRemoteDrawing();

  /**
   * Whether or not the compositor should be recording frames.
   *
   * When this returns true, the BasicCompositor will keep the
   * |mFullWindowRenderTarget| as an up-to-date copy of the entire rendered
   * window.
   *
   * This will be true when either we are recording a profile with screenshots
   * enabled or the |LayerManagerComposite| has requested us to record frames
   * for the |CompositionRecorder|.
   */
  bool ShouldRecordFrames() const;

  // The final destination surface
  RefPtr<gfx::DrawTarget> mDrawTarget;
  // The bounds that mDrawTarget occupies in window space.
  gfx::IntRect mDrawTargetBounds;

  // The current render target for drawing
  RefPtr<BasicCompositingRenderTarget> mRenderTarget;

  LayoutDeviceIntRect mInvalidRect;
  LayoutDeviceIntRegion mInvalidRegion;

  uint32_t mMaxTextureSize;
  bool mIsPendingEndRemoteDrawing;
  bool mRecordFrames;

  // mDrawTarget will not be the full window on all platforms. We therefore need
  // to keep a full window render target around when we are capturing
  // screenshots on those platforms.
  RefPtr<BasicCompositingRenderTarget> mFullWindowRenderTarget;
};

BasicCompositor* AssertBasicCompositor(Compositor* aCompositor);

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_BASICCOMPOSITOR_H */
