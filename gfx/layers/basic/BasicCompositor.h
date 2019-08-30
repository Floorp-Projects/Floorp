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

#ifdef XP_MACOSX
class MacIOSurface;
#endif

namespace mozilla {
namespace layers {

class BasicCompositingRenderTarget : public CompositingRenderTarget {
 public:
  BasicCompositingRenderTarget(gfx::DrawTarget* aDrawTarget,
                               const gfx::IntRect& aRect,
                               const gfx::IntPoint& aClipSpaceOrigin)
      : CompositingRenderTarget(aRect.TopLeft()),
        mDrawTarget(aDrawTarget),
        mSize(aRect.Size()),
        mClipSpaceOrigin(aClipSpaceOrigin) {}

  const char* Name() const override { return "BasicCompositingRenderTarget"; }

  gfx::IntSize GetSize() const override { return mSize; }

  // The point that DrawGeometry's aClipRect is relative to. Will be (0, 0) for
  // root render targets and equal to GetOrigin() for non-root render targets.
  gfx::IntPoint GetClipSpaceOrigin() const { return mClipSpaceOrigin; }

  void BindRenderTarget();

  gfx::SurfaceFormat GetFormat() const override {
    return mDrawTarget ? mDrawTarget->GetFormat()
                       : gfx::SurfaceFormat(gfx::SurfaceFormat::UNKNOWN);
  }

  RefPtr<gfx::DrawTarget> mDrawTarget;
  gfx::IntSize mSize;
  gfx::IntPoint mClipSpaceOrigin;
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

  virtual already_AddRefed<CompositingRenderTarget> CreateRootRenderTarget(
      gfx::DrawTarget* aDrawTarget, const gfx::IntRect& aDrawTargetRect,
      const gfx::IntRegion& aClearRegion);

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

  Maybe<gfx::IntRect> BeginFrameForWindow(const nsIntRegion& aInvalidRegion,
                                          const Maybe<gfx::IntRect>& aClipRect,
                                          const gfx::IntRect& aRenderBounds,
                                          const nsIntRegion& aOpaqueRegion,
                                          NativeLayer* aNativeLayer) override;
  Maybe<gfx::IntRect> BeginFrameForTarget(
      const nsIntRegion& aInvalidRegion, const Maybe<gfx::IntRect>& aClipRect,
      const gfx::IntRect& aRenderBounds, const nsIntRegion& aOpaqueRegion,
      gfx::DrawTarget* aTarget, const gfx::IntRect& aTargetBounds) override;

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

  void TryToEndRemoteDrawing();
  void EndRemoteDrawing();

  bool NeedsToDeferEndRemoteDrawing();

  /**
   * Whether or not the compositor should be recording frames.
   *
   * When this returns true, the BasicCompositor will keep the
   * |mFullWindowRenderTarget| as an up-to-date copy of the entire rendered
   * window. This copy is maintained in NormalDrawingDone().
   *
   * This will be true when either we are recording a profile with screenshots
   * enabled or the |LayerManagerComposite| has requested us to record frames
   * for the |CompositionRecorder|.
   */
  bool ShouldRecordFrames() const;

  bool NeedToRecreateFullWindowRenderTarget() const;

  // When rendering to a back buffer, this is the front buffer that the contents
  // of the back buffer need to be copied to. Only non-null between
  // BeginFrameForWindow and EndRemoteDrawing, and only when using a back
  // buffer.
  RefPtr<gfx::DrawTarget> mFrontBuffer;

  // The current render target for drawing
  RefPtr<BasicCompositingRenderTarget> mRenderTarget;

  // The native layer that we're currently rendering to, if any.
  // Non-null only between BeginFrameForWindow and EndFrame if
  // BeginFrameForWindow has been called with a non-null aNativeLayer.
  RefPtr<NativeLayer> mCurrentNativeLayer;

#ifdef XP_MACOSX
  // The MacIOSurface that we're currently rendering to, if any.
  // Non-null in the same cases as mCurrentNativeLayer.
  RefPtr<MacIOSurface> mCurrentIOSurface;
#endif

  gfx::IntRegion mInvalidRegion;

  uint32_t mMaxTextureSize;
  bool mIsPendingEndRemoteDrawing;
  bool mRecordFrames;

  // Only true between BeginFrameForTarget and EndFrame. Tells EndFrame which of
  // the BeginFrameForXYZ methods has been called.
  bool mFrameIsForTarget = false;

  // mDrawTarget will not be the full window on all platforms. We therefore need
  // to keep a full window render target around when we are capturing
  // screenshots on those platforms.
  RefPtr<BasicCompositingRenderTarget> mFullWindowRenderTarget;
};

BasicCompositor* AssertBasicCompositor(Compositor* aCompositor);

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_BASICCOMPOSITOR_H */
