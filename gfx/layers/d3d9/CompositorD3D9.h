/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITORD3D9_H
#define MOZILLA_GFX_COMPOSITORD3D9_H

#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/TextureD3D9.h"
#include "DeviceManagerD3D9.h"

class nsWidget;

namespace mozilla {
namespace layers {

class CompositorD3D9 : public Compositor
{
public:
  CompositorD3D9(nsIWidget *aWidget);
  ~CompositorD3D9();

  virtual bool Initialize() MOZ_OVERRIDE;
  virtual void Destroy() MOZ_OVERRIDE {}

  virtual TextureFactoryIdentifier
    GetTextureFactoryIdentifier() MOZ_OVERRIDE;

  virtual bool CanUseCanvasLayerForSize(const gfxIntSize &aSize) MOZ_OVERRIDE;
  virtual int32_t GetMaxTextureSize() const MOZ_FINAL;

  virtual void SetTargetContext(gfxContext *aTarget) MOZ_OVERRIDE
  {
    mTarget = aTarget;
  }

  virtual void MakeCurrent(MakeCurrentFlags aFlags = 0) MOZ_OVERRIDE {}

  virtual TemporaryRef<CompositingRenderTarget>
    CreateRenderTarget(const gfx::IntRect &aRect,
                       SurfaceInitMode aInit) MOZ_OVERRIDE;

  virtual TemporaryRef<CompositingRenderTarget>
    CreateRenderTargetFromSource(const gfx::IntRect &aRect,
                                 const CompositingRenderTarget *aSource) MOZ_OVERRIDE;

  virtual void SetRenderTarget(CompositingRenderTarget *aSurface);
  virtual CompositingRenderTarget* GetCurrentRenderTarget() MOZ_OVERRIDE
  {
    return mCurrentRT;
  }

  virtual void SetDestinationSurfaceSize(const gfx::IntSize& aSize) MOZ_OVERRIDE {}

  virtual void DrawQuad(const gfx::Rect &aRect, const gfx::Rect &aClipRect,
                        const EffectChain &aEffectChain,
                        gfx::Float aOpacity, const gfx::Matrix4x4 &aTransform,
                        const gfx::Point &aOffset) MOZ_OVERRIDE;

  virtual void BeginFrame(const gfx::Rect *aClipRectIn,
                          const gfxMatrix& aTransform,
                          const gfx::Rect& aRenderBounds,
                          gfx::Rect *aClipRectOut = nullptr,
                          gfx::Rect *aRenderBoundsOut = nullptr) MOZ_OVERRIDE;

  virtual void EndFrame() MOZ_OVERRIDE;

  virtual void EndFrameForExternalComposition(const gfxMatrix& aTransform) MOZ_OVERRIDE {}

  virtual void AbortFrame() MOZ_OVERRIDE {}

  virtual void PrepareViewport(const gfx::IntSize& aSize,
                               const gfxMatrix& aWorldTransform) MOZ_OVERRIDE;

  virtual bool SupportsPartialTextureUpdate() MOZ_OVERRIDE{ return true; }

#ifdef MOZ_DUMP_PAINTING
  virtual const char* Name() const MOZ_OVERRIDE { return "Direct3D9"; }
#endif

  virtual void NotifyLayersTransaction() MOZ_OVERRIDE {}

  virtual nsIWidget* GetWidget() const MOZ_OVERRIDE { return mWidget; }
  virtual const nsIntSize& GetWidgetSize() MOZ_OVERRIDE
  {
    NS_ASSERTION(false, "Getting the widget size on windows causes some kind of resizing of buffers. "
                        "You should not do that outside of BeginFrame, so the best we can do is return "
                        "the last size we got, that might not be up to date. So you probably shouldn't "
                        "use this method.");
    return mSize;
  }

  IDirect3DDevice9* device() const { return mDeviceManager->device(); }

  /**
   * Declare an offset to use when rendering layers. This will be ignored when
   * rendering to a target instead of the screen.
   */
  virtual void SetScreenRenderOffset(const ScreenPoint& aOffset) MOZ_OVERRIDE
  {
    if (aOffset.x || aOffset.y) {
      NS_RUNTIMEABORT("SetScreenRenderOffset not supported by CompositorD3D9.");
    }
    // If the offset is 0, 0 that's okay.
  }

   virtual TemporaryRef<DataTextureSource>
     CreateDataTextureSource(TextureFlags aFlags = 0) MOZ_OVERRIDE { return nullptr; } 
private:
  // ensure mSize is up to date with respect to mWidget
  void EnsureSize();
  void PaintToTarget();
  void SetMask(const EffectChain &aEffectChain, uint32_t aMaskTexture);

  void ReportFailure(const nsACString &aMsg, HRESULT aCode);

  /* Device manager instance for this compositor */
  nsRefPtr<DeviceManagerD3D9> mDeviceManager;

  /* Swap chain associated with this compositor */
  nsRefPtr<SwapChainD3D9> mSwapChain;

  /* Widget associated with this layer manager */
  nsIWidget *mWidget;

  /*
   * Context target, NULL when drawing directly to our swap chain.
   */
  nsRefPtr<gfxContext> mTarget;

  RefPtr<CompositingRenderTargetD3D9> mDefaultRT;
  RefPtr<CompositingRenderTargetD3D9> mCurrentRT;

  nsIntSize mSize;
};

}
}

#endif
