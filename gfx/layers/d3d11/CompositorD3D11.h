/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITORD3D11_H
#define MOZILLA_GFX_COMPOSITORD3D11_H

#include "mozilla/gfx/2D.h"
#include "gfx2DGlue.h"
#include "mozilla/layers/Compositor.h"
#include "TextureD3D11.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include "ShaderDefinitionsD3D11.h"

class nsWidget;

namespace mozilla {
namespace layers {

#define LOGD3D11(param)

class DeviceAttachmentsD3D11;

class CompositorD3D11 : public Compositor {
 public:
  explicit CompositorD3D11(widget::CompositorWidget* aWidget);
  virtual ~CompositorD3D11();

  CompositorD3D11* AsCompositorD3D11() override { return this; }

  bool Initialize(nsCString* const out_failureReason) override;

  already_AddRefed<DataTextureSource> CreateDataTextureSource(
      TextureFlags aFlags = TextureFlags::NO_FLAGS) override;

  int32_t GetMaxTextureSize() const final;

  already_AddRefed<CompositingRenderTarget> CreateRenderTarget(
      const gfx::IntRect& aRect, SurfaceInitMode aInit) override;

  void SetRenderTarget(CompositingRenderTarget* aSurface) override;
  already_AddRefed<CompositingRenderTarget> GetCurrentRenderTarget()
      const override {
    return do_AddRef(mCurrentRT);
  }
  already_AddRefed<CompositingRenderTarget> GetWindowRenderTarget()
      const override;

  bool ReadbackRenderTarget(CompositingRenderTarget* aSource,
                            AsyncReadbackBuffer* aDest) override;
  already_AddRefed<AsyncReadbackBuffer> CreateAsyncReadbackBuffer(
      const gfx::IntSize& aSize) override;

  bool BlitRenderTarget(CompositingRenderTarget* aSource,
                        const gfx::IntSize& aSourceSize,
                        const gfx::IntSize& aDestSize) override;

  void SetDestinationSurfaceSize(const gfx::IntSize& aSize) override {}

  void ClearRect(const gfx::Rect& aRect);

  void DrawQuad(const gfx::Rect& aRect, const gfx::IntRect& aClipRect,
                const EffectChain& aEffectChain, gfx::Float aOpacity,
                const gfx::Matrix4x4& aTransform,
                const gfx::Rect& aVisibleRect) override;

  /**
   * Start a new frame.
   */
  Maybe<gfx::IntRect> BeginFrameForWindow(
      const nsIntRegion& aInvalidRegion, const Maybe<gfx::IntRect>& aClipRect,
      const gfx::IntRect& aRenderBounds,
      const nsIntRegion& aOpaqueRegion) override;

  /**
   * Flush the current frame to the screen.
   */
  void EndFrame() override;

  void CancelFrame(bool aNeedFlush = true) override;

  /**
   * Setup the viewport and projection matrix for rendering
   * to a window of the given dimensions.
   */
  virtual void PrepareViewport(const gfx::IntSize& aSize);
  virtual void PrepareViewport(const gfx::IntSize& aSize,
                               const gfx::Matrix4x4& aProjection, float aZNear,
                               float aZFar);

#ifdef MOZ_DUMP_PAINTING
  const char* Name() const override { return "Direct3D 11"; }
#endif

  // For TextureSourceProvider.
  ID3D11Device* GetD3D11Device() const override { return mDevice; }

  ID3D11Device* GetDevice() { return mDevice; }

  ID3D11DeviceContext* GetDC() { return mContext; }

  virtual void RequestAllowFrameRecording(bool aWillRecord) override {
    mAllowFrameRecording = aWillRecord;
  }

  void Readback(gfx::DrawTarget* aDrawTarget) {
    mTarget = aDrawTarget;
    mTargetBounds = gfx::IntRect();
    PaintToTarget();
    mTarget = nullptr;
  }

  SyncObjectHost* GetSyncObject();

 private:
  enum Severity {
    Recoverable,
    DebugAssert,
    Critical,
  };

  void HandleError(HRESULT hr, Severity aSeverity = DebugAssert);

  // Same as Failed(), except the severity is critical (with no abort) and
  // a string prefix must be provided.
  bool Failed(HRESULT hr, const char* aContext);

  // ensure mSize is up to date with respect to mWidget
  void EnsureSize();
  bool VerifyBufferSize();
  bool UpdateRenderTarget();
  bool UpdateConstantBuffers();
  void SetSamplerForSamplingFilter(gfx::SamplingFilter aSamplingFilter);

  ID3D11PixelShader* GetPSForEffect(Effect* aEffect);
  Maybe<gfx::IntRect> BeginFrame(const nsIntRegion& aInvalidRegion,
                                 const Maybe<gfx::IntRect>& aClipRect,
                                 const gfx::IntRect& aRenderBounds,
                                 const nsIntRegion& aOpaqueRegion);
  void PaintToTarget();
  RefPtr<ID3D11Texture2D> CreateTexture(const gfx::IntRect& aRect,
                                        const CompositingRenderTarget* aSource,
                                        const gfx::IntPoint& aSourcePoint);

  void PrepareStaticVertexBuffer();

  void Draw(const gfx::Rect& aGeometry, const gfx::Rect* aTexCoords);

  void Present();

  template <typename VertexType>
  void SetVertexBuffer(ID3D11Buffer* aBuffer);

  /**
   * Whether or not the recorder should be recording frames.
   *
   * When this returns true, the CompositorD3D11 will allocate and return window
   * render targets from |GetWindowRenderTarget|, which otherwise returns
   * nullptr.
   *
   * This will be true when either we are recording a profile with screenshots
   * enabled or the |LayerManagerComposite| has requested us to record frames
   * for the |CompositionRecorder|.
   */
  bool ShouldAllowFrameRecording() const;

  // The DrawTarget from BeginFrameForTarget, which EndFrame needs to copy the
  // window contents into.
  // Only non-null between BeginFrameForTarget and EndFrame.
  RefPtr<gfx::DrawTarget> mTarget;
  gfx::IntRect mTargetBounds;

  RefPtr<ID3D11DeviceContext> mContext;
  RefPtr<ID3D11Device> mDevice;
  RefPtr<IDXGISwapChain> mSwapChain;
  RefPtr<CompositingRenderTargetD3D11> mDefaultRT;
  RefPtr<CompositingRenderTargetD3D11> mCurrentRT;
  mutable RefPtr<CompositingRenderTargetD3D11> mWindowRTCopy;

  RefPtr<ID3D11Query> mQuery;
  RefPtr<ID3D11Query> mRecycledQuery;

  RefPtr<DeviceAttachmentsD3D11> mAttachments;

  LayoutDeviceIntSize mSize;

  // The size that we passed to ResizeBuffers to set
  // the swapchain buffer size.
  LayoutDeviceIntSize mBufferSize;

  HWND mHwnd;

  D3D_FEATURE_LEVEL mFeatureLevel;

  VertexShaderConstants mVSConstants;
  PixelShaderConstants mPSConstants;
  bool mDisableSequenceForNextFrame;
  bool mAllowPartialPresents;
  bool mIsDoubleBuffered;

  gfx::IntRegion mFrontBufferInvalid;
  gfx::IntRegion mBackBufferInvalid;
  // This is the clip rect applied to the default DrawTarget (i.e. the window)
  gfx::IntRect mCurrentClip;

  bool mVerifyBuffersFailed;
  bool mUseMutexOnPresent;
  bool mAllowFrameRecording;
};

namespace TexSlot {
static const int RGB = 0;
static const int Y = 1;
static const int Cb = 2;
static const int Cr = 3;
static const int RGBWhite = 4;
static const int Mask = 5;
static const int Backdrop = 6;
}  // namespace TexSlot

}  // namespace layers
}  // namespace mozilla

#endif
