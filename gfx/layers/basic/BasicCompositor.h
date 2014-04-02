/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BASICCOMPOSITOR_H
#define MOZILLA_GFX_BASICCOMPOSITOR_H

#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/gfx/2D.h"
#include "nsAutoPtr.h"

class gfxContext;

namespace mozilla {
namespace layers {

class BasicCompositingRenderTarget : public CompositingRenderTarget
{
public:
  BasicCompositingRenderTarget(gfx::DrawTarget* aDrawTarget, const gfx::IntRect& aRect)
    : CompositingRenderTarget(aRect.TopLeft())
    , mDrawTarget(aDrawTarget)
    , mSize(aRect.Size())
  { }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE
  {
    return mDrawTarget ? mDrawTarget->GetFormat()
                       : gfx::SurfaceFormat(gfx::SurfaceFormat::UNKNOWN);
  }

  RefPtr<gfx::DrawTarget> mDrawTarget;
  gfx::IntSize mSize;
};

class BasicCompositor : public Compositor
{
public:
  BasicCompositor(nsIWidget *aWidget);

  virtual ~BasicCompositor();


  virtual bool Initialize() MOZ_OVERRIDE { return true; };

  virtual void Destroy() MOZ_OVERRIDE;

  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() MOZ_OVERRIDE
  {
    return TextureFactoryIdentifier(LayersBackend::LAYERS_BASIC,
                                    XRE_GetProcessType(),
                                    GetMaxTextureSize());
  }

  virtual TemporaryRef<CompositingRenderTarget>
  CreateRenderTarget(const gfx::IntRect &aRect, SurfaceInitMode aInit) MOZ_OVERRIDE;

  virtual TemporaryRef<CompositingRenderTarget>
  CreateRenderTargetFromSource(const gfx::IntRect &aRect,
                               const CompositingRenderTarget *aSource,
                               const gfx::IntPoint &aSourcePoint) MOZ_OVERRIDE;

  virtual TemporaryRef<DataTextureSource>
  CreateDataTextureSource(TextureFlags aFlags = 0) MOZ_OVERRIDE;

  virtual bool SupportsEffect(EffectTypes aEffect) MOZ_OVERRIDE;

  virtual void SetRenderTarget(CompositingRenderTarget *aSource) MOZ_OVERRIDE
  {
    mRenderTarget = static_cast<BasicCompositingRenderTarget*>(aSource);
  }
  virtual CompositingRenderTarget* GetCurrentRenderTarget() const MOZ_OVERRIDE
  {
    return mRenderTarget;
  }

  virtual void DrawQuad(const gfx::Rect& aRect,
                        const gfx::Rect& aClipRect,
                        const EffectChain &aEffectChain,
                        gfx::Float aOpacity,
                        const gfx::Matrix4x4 &aTransform) MOZ_OVERRIDE;

  virtual void BeginFrame(const nsIntRegion& aInvalidRegion,
                          const gfx::Rect *aClipRectIn,
                          const gfx::Matrix& aTransform,
                          const gfx::Rect& aRenderBounds,
                          gfx::Rect *aClipRectOut = nullptr,
                          gfx::Rect *aRenderBoundsOut = nullptr) MOZ_OVERRIDE;
  virtual void EndFrame() MOZ_OVERRIDE;
  virtual void EndFrameForExternalComposition(const gfx::Matrix& aTransform) MOZ_OVERRIDE
  {
    NS_RUNTIMEABORT("We shouldn't ever hit this");
  }
  virtual void AbortFrame() MOZ_OVERRIDE;

  virtual bool SupportsPartialTextureUpdate() { return true; }
  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize &aSize) MOZ_OVERRIDE { return true; }
  virtual int32_t GetMaxTextureSize() const MOZ_OVERRIDE { return INT32_MAX; }
  virtual void SetDestinationSurfaceSize(const gfx::IntSize& aSize) MOZ_OVERRIDE { }
  virtual void SetTargetContext(gfx::DrawTarget* aTarget) MOZ_OVERRIDE
  {
    mCopyTarget = aTarget;
  }
  
  virtual void SetScreenRenderOffset(const ScreenPoint& aOffset) MOZ_OVERRIDE {
  }

  virtual void MakeCurrent(MakeCurrentFlags aFlags = 0) { }

  virtual void PrepareViewport(const gfx::IntSize& aSize,
                               const gfx::Matrix& aWorldTransform) MOZ_OVERRIDE { }

  virtual const char* Name() const { return "Basic"; }

  virtual LayersBackend GetBackendType() const MOZ_OVERRIDE {
    return LayersBackend::LAYERS_BASIC;
  }

  virtual nsIWidget* GetWidget() const MOZ_OVERRIDE { return mWidget; }

  gfx::DrawTarget *GetDrawTarget() { return mDrawTarget; }

private:
  // Widget associated with this compositor
  nsIWidget *mWidget;
  nsIntSize mWidgetSize;

  // The final destination surface
  RefPtr<gfx::DrawTarget> mDrawTarget;
  // The current render target for drawing
  RefPtr<BasicCompositingRenderTarget> mRenderTarget;
  // An optional destination target to copy the results
  // to after drawing is completed.
  RefPtr<gfx::DrawTarget> mCopyTarget;

  gfx::IntRect mInvalidRect;
  nsIntRegion mInvalidRegion;
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_BASICCOMPOSITOR_H */
