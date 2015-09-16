/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITORD3D11_H
#define MOZILLA_GFX_COMPOSITORD3D11_H

#include "mozilla/gfx/2D.h"
#include "gfx2DGlue.h"
#include "mozilla/layers/Compositor.h"
#include "TextureD3D11.h"
#include <d3d11.h>

class nsWidget;

namespace mozilla {
namespace layers {

#define LOGD3D11(param)

struct VertexShaderConstants
{
  float layerTransform[4][4];
  float projection[4][4];
  float renderTargetOffset[4];
  gfx::Rect textureCoords;
  gfx::Rect layerQuad;
  gfx::Rect maskQuad;
  float vrEyeToSourceUVScale[2];
  float vrEyeToSourceUVOffset[2];
};

struct PixelShaderConstants
{
  float layerColor[4];
  float layerOpacity[4];
};

struct DeviceAttachmentsD3D11;

class CompositorD3D11 : public Compositor
{
public:
  CompositorD3D11(nsIWidget* aWidget);
  ~CompositorD3D11();

  virtual bool Initialize() override;
  virtual void Destroy() override {}

  virtual TextureFactoryIdentifier
    GetTextureFactoryIdentifier() override;

  virtual already_AddRefed<DataTextureSource>
    CreateDataTextureSource(TextureFlags aFlags = TextureFlags::NO_FLAGS) override;

  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize& aSize) override;
  virtual int32_t GetMaxTextureSize() const final;

  virtual void MakeCurrent(MakeCurrentFlags aFlags = 0)  override {}

  virtual already_AddRefed<CompositingRenderTarget>
    CreateRenderTarget(const gfx::IntRect &aRect,
                       SurfaceInitMode aInit) override;

  virtual already_AddRefed<CompositingRenderTarget>
    CreateRenderTargetFromSource(const gfx::IntRect& aRect,
                                 const CompositingRenderTarget* aSource,
                                 const gfx::IntPoint& aSourcePoint) override;

  virtual void SetRenderTarget(CompositingRenderTarget* aSurface) override;
  virtual CompositingRenderTarget* GetCurrentRenderTarget() const override
  {
    return mCurrentRT;
  }

  virtual void SetDestinationSurfaceSize(const gfx::IntSize& aSize) override {}

  /**
   * Declare an offset to use when rendering layers. This will be ignored when
   * rendering to a target instead of the screen.
   */
  virtual void SetScreenRenderOffset(const ScreenPoint& aOffset) override
  {
    if (aOffset.x || aOffset.y) {
      NS_RUNTIMEABORT("SetScreenRenderOffset not supported by CompositorD3D11.");
    }
    // If the offset is 0, 0 that's okay.
  }

  virtual void ClearRect(const gfx::Rect& aRect) override;

  virtual void DrawQuad(const gfx::Rect &aRect,
                        const gfx::Rect &aClipRect,
                        const EffectChain &aEffectChain,
                        gfx::Float aOpacity,
                        const gfx::Matrix4x4& aTransform,
                        const gfx::Rect& aVisibleRect) override;

  /* Helper for when the primary effect is VR_DISTORTION */
  void DrawVRDistortion(const gfx::Rect &aRect,
                        const gfx::Rect &aClipRect,
                        const EffectChain &aEffectChain,
                        gfx::Float aOpacity,
                        const gfx::Matrix4x4 &aTransform);

  /**
   * Start a new frame. If aClipRectIn is null, sets *aClipRectOut to the
   * screen dimensions. 
   */
  virtual void BeginFrame(const nsIntRegion& aInvalidRegion,
                          const gfx::Rect *aClipRectIn,
                          const gfx::Rect& aRenderBounds,
                          gfx::Rect *aClipRectOut = nullptr,
                          gfx::Rect *aRenderBoundsOut = nullptr) override;

  /**
   * Flush the current frame to the screen.
   */
  virtual void EndFrame() override;

  /**
   * Post rendering stuff if the rendering is outside of this Compositor
   * e.g., by Composer2D
   */
  virtual void EndFrameForExternalComposition(const gfx::Matrix& aTransform) override {}

  /**
   * Setup the viewport and projection matrix for rendering
   * to a window of the given dimensions.
   */
  virtual void PrepareViewport(const gfx::IntSize& aSize);
  virtual void PrepareViewport(const gfx::IntSize& aSize, const gfx::Matrix4x4& aProjection,
                               float aZNear, float aZFar);

  virtual bool SupportsPartialTextureUpdate() override { return true; }

#ifdef MOZ_DUMP_PAINTING
  virtual const char* Name() const override { return "Direct3D 11"; }
#endif

  virtual LayersBackend GetBackendType() const override {
    return LayersBackend::LAYERS_D3D11;
  }

  virtual nsIWidget* GetWidget() const override { return mWidget; }

  ID3D11Device* GetDevice() { return mDevice; }

  ID3D11DeviceContext* GetDC() { return mContext; }

private:
  enum Severity {
    Recoverable,
    DebugAssert,
    Critical,
  };

  void HandleError(HRESULT hr, Severity aSeverity = DebugAssert);
  bool Failed(HRESULT hr, Severity aSeverity = DebugAssert);
  bool Succeeded(HRESULT hr, Severity aSeverity = DebugAssert);

  // ensure mSize is up to date with respect to mWidget
  void EnsureSize();
  bool VerifyBufferSize();
  void UpdateRenderTarget();
  bool UpdateConstantBuffers();
  void SetSamplerForFilter(gfx::Filter aFilter);
  void SetPSForEffect(Effect *aEffect, MaskType aMaskType, gfx::SurfaceFormat aFormat);
  void PaintToTarget();
  bool SetBlendMode(gfx::CompositionOp aOp, bool aPremultipled = true);

  virtual gfx::IntSize GetWidgetSize() const override { return mSize; }

  RefPtr<ID3D11DeviceContext> mContext;
  RefPtr<ID3D11Device> mDevice;
  RefPtr<IDXGISwapChain> mSwapChain;
  RefPtr<CompositingRenderTargetD3D11> mDefaultRT;
  RefPtr<CompositingRenderTargetD3D11> mCurrentRT;

  DeviceAttachmentsD3D11* mAttachments;

  nsIWidget* mWidget;

  gfx::IntSize mSize;

  HWND mHwnd;

  D3D_FEATURE_LEVEL mFeatureLevel;

  VertexShaderConstants mVSConstants;
  PixelShaderConstants mPSConstants;
  bool mDisableSequenceForNextFrame;

  gfx::IntRect mInvalidRect;
  // This is the clip rect applied to the default DrawTarget (i.e. the window)
  gfx::IntRect mCurrentClip;
  nsIntRegion mInvalidRegion;
};

}
}

#endif
