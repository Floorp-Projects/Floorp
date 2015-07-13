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

  virtual gfx::IntSize GetSize() const override { return mSize; }

  void BindRenderTarget();

  virtual gfx::SurfaceFormat GetFormat() const override
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
  explicit BasicCompositor(nsIWidget *aWidget);

protected:
  virtual ~BasicCompositor();

public:
  virtual bool Initialize() override;

  virtual void Destroy() override;

  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() override
  {
    return TextureFactoryIdentifier(LayersBackend::LAYERS_BASIC,
                                    XRE_GetProcessType(),
                                    GetMaxTextureSize());
  }

  virtual already_AddRefed<CompositingRenderTarget>
  CreateRenderTarget(const gfx::IntRect &aRect, SurfaceInitMode aInit) override;

  virtual already_AddRefed<CompositingRenderTarget>
  CreateRenderTargetFromSource(const gfx::IntRect &aRect,
                               const CompositingRenderTarget *aSource,
                               const gfx::IntPoint &aSourcePoint) override;

  virtual already_AddRefed<DataTextureSource>
  CreateDataTextureSource(TextureFlags aFlags = TextureFlags::NO_FLAGS) override;

  virtual bool SupportsEffect(EffectTypes aEffect) override;

  virtual void SetRenderTarget(CompositingRenderTarget *aSource) override
  {
    mRenderTarget = static_cast<BasicCompositingRenderTarget*>(aSource);
    mRenderTarget->BindRenderTarget();
  }
  virtual CompositingRenderTarget* GetCurrentRenderTarget() const override
  {
    return mRenderTarget;
  }

  virtual void DrawQuad(const gfx::Rect& aRect,
                        const gfx::Rect& aClipRect,
                        const EffectChain &aEffectChain,
                        gfx::Float aOpacity,
                        const gfx::Matrix4x4& aTransform,
                        const gfx::Rect& aVisibleRect) override;

  virtual void ClearRect(const gfx::Rect& aRect) override;

  virtual void BeginFrame(const nsIntRegion& aInvalidRegion,
                          const gfx::Rect *aClipRectIn,
                          const gfx::Rect& aRenderBounds,
                          gfx::Rect *aClipRectOut = nullptr,
                          gfx::Rect *aRenderBoundsOut = nullptr) override;
  virtual void EndFrame() override;
  virtual void EndFrameForExternalComposition(const gfx::Matrix& aTransform) override
  {
    NS_RUNTIMEABORT("We shouldn't ever hit this");
  }

  virtual bool SupportsPartialTextureUpdate() override { return true; }
  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize &aSize) override { return true; }
  virtual int32_t GetMaxTextureSize() const override;
  virtual void SetDestinationSurfaceSize(const gfx::IntSize& aSize) override { }
  
  virtual void SetScreenRenderOffset(const ScreenPoint& aOffset) override {
  }

  virtual void MakeCurrent(MakeCurrentFlags aFlags = 0) override { }

  virtual void PrepareViewport(const gfx::IntSize& aSize) override { }

#ifdef MOZ_DUMP_PAINTING
  virtual const char* Name() const override { return "Basic"; }
#endif // MOZ_DUMP_PAINTING

  virtual LayersBackend GetBackendType() const override {
    return LayersBackend::LAYERS_BASIC;
  }

  virtual nsIWidget* GetWidget() const override { return mWidget; }

  gfx::DrawTarget *GetDrawTarget() { return mDrawTarget; }

private:

  virtual gfx::IntSize GetWidgetSize() const override { return mWidgetSize; }

  // Widget associated with this compositor
  nsIWidget *mWidget;
  gfx::IntSize mWidgetSize;

  // The final destination surface
  RefPtr<gfx::DrawTarget> mDrawTarget;
  // The current render target for drawing
  RefPtr<BasicCompositingRenderTarget> mRenderTarget;

  gfx::IntRect mInvalidRect;
  nsIntRegion mInvalidRegion;

  uint32_t mMaxTextureSize;
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_BASICCOMPOSITOR_H */
