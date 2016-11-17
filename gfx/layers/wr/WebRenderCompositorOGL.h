/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_WEBRENDERCOMPOSITOROGL_H
#define MOZILLA_GFX_WEBRENDERCOMPOSITOROGL_H

#include "GLContextTypes.h"             // for GLContext, etc
#include "GLDefs.h"                     // for GLuint, LOCAL_GL_TEXTURE_2D, etc
#include "mozilla/layers/Compositor.h"  // for SurfaceInitMode, Compositor, etc

namespace mozilla {
namespace layers {

class WebRenderCompositorOGL final : public Compositor
{
  typedef mozilla::gl::GLContext GLContext;

public:
  explicit WebRenderCompositorOGL(GLContext* aGLContext);

protected:
  virtual ~WebRenderCompositorOGL();

public:
  virtual already_AddRefed<DataTextureSource>
  CreateDataTextureSource(TextureFlags aFlags = TextureFlags::NO_FLAGS) override;

  virtual bool Initialize(nsCString* const out_failureReason) override;

  virtual void Destroy() override;

  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() override
  {
    TextureFactoryIdentifier result =
      TextureFactoryIdentifier(LayersBackend::LAYERS_OPENGL,
                               XRE_GetProcessType(),
                               GetMaxTextureSize(),
                               true,
                               SupportsPartialTextureUpdate());
    return result;
  }

  virtual already_AddRefed<CompositingRenderTarget>
  CreateRenderTarget(const gfx::IntRect &aRect, SurfaceInitMode aInit) override { return nullptr; }

  virtual already_AddRefed<CompositingRenderTarget>
  CreateRenderTargetFromSource(const gfx::IntRect &aRect,
                               const CompositingRenderTarget *aSource,
                               const gfx::IntPoint &aSourcePoint) override { return nullptr; }

  virtual void SetRenderTarget(CompositingRenderTarget *aSurface) override { }

  virtual CompositingRenderTarget* GetCurrentRenderTarget() const override { return nullptr; }

  virtual void DrawQuad(const gfx::Rect& aRect,
                        const gfx::IntRect& aClipRect,
                        const EffectChain &aEffectChain,
                        gfx::Float aOpacity,
                        const gfx::Matrix4x4& aTransform,
                        const gfx::Rect& aVisibleRect) override { }

  virtual void DrawTriangle(const gfx::TexturedTriangle& aTriangle,
                            const gfx::IntRect& aClipRect,
                            const EffectChain& aEffectChain,
                            gfx::Float aOpacity,
                            const gfx::Matrix4x4& aTransform,
                            const gfx::Rect& aVisibleRect) override { }

  virtual void ClearRect(const gfx::Rect& aRect) override { }

  virtual void BeginFrame(const nsIntRegion& aInvalidRegion,
                          const gfx::IntRect *aClipRectIn,
                          const gfx::IntRect& aRenderBounds,
                          const nsIntRegion& aOpaqueRegion,
                          gfx::IntRect *aClipRectOut = nullptr,
                          gfx::IntRect *aRenderBoundsOut = nullptr) override { }

  virtual void EndFrame() override { }

  virtual void EndFrameForExternalComposition(const gfx::Matrix& aTransform) override { }

  virtual bool SupportsPartialTextureUpdate() override;

  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize &aSize) override
  {
    if (!mGLContext)
      return false;
    int32_t maxSize = GetMaxTextureSize();
    return aSize <= gfx::IntSize(maxSize, maxSize);
  }

  virtual int32_t GetMaxTextureSize() const override;

  virtual void SetDestinationSurfaceSize(const gfx::IntSize& aSize) override { }
  virtual void SetScreenRenderOffset(const ScreenPoint& aOffset) override { }

  virtual void MakeCurrent(MakeCurrentFlags aFlags = 0) override;

#ifdef MOZ_DUMP_PAINTING
  virtual const char* Name() const override { return "WROGL"; }
#endif // MOZ_DUMP_PAINTING

  virtual LayersBackend GetBackendType() const override {
    return LayersBackend::LAYERS_OPENGL;
  }

  virtual bool IsValid() const override { return true; }

  GLContext* gl() const { return mGLContext; }

private:
  RefPtr<GLContext> mGLContext;

  bool mDestroyed;
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_WEBRENDERCOMPOSITOROGL_H */
