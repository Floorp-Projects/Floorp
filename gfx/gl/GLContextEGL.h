/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTEGL_H_
#define GLCONTEXTEGL_H_

#include "GLContext.h"
#include "GLLibraryEGL.h"

class gfxASurface;
namespace mozilla {
namespace widget {
class CompositorWidget;
}  // namespace widget
namespace gl {

class GLContextEGL : public GLContext {
  friend class TextureImageEGL;

  static already_AddRefed<GLContextEGL> CreateGLContext(
      CreateContextFlags flags, const SurfaceCaps& caps, bool isOffscreen,
      EGLConfig config, EGLSurface surface, nsACString* const out_failureId);

 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GLContextEGL, override)
  GLContextEGL(CreateContextFlags flags, const SurfaceCaps& caps,
               bool isOffscreen, EGLConfig config, EGLSurface surface,
               EGLContext context);

  ~GLContextEGL();

  virtual GLContextType GetContextType() const override {
    return GLContextType::EGL;
  }

  static GLContextEGL* Cast(GLContext* gl) {
    MOZ_ASSERT(gl->GetContextType() == GLContextType::EGL);
    return static_cast<GLContextEGL*>(gl);
  }

  bool Init() override;

  virtual bool IsDoubleBuffered() const override { return mIsDoubleBuffered; }

  void SetIsDoubleBuffered(bool aIsDB) { mIsDoubleBuffered = aIsDB; }

  virtual bool IsANGLE() const override {
    return GLLibraryEGL::Get()->IsANGLE();
  }

  virtual bool IsWARP() const override { return GLLibraryEGL::Get()->IsWARP(); }

  virtual bool BindTexImage() override;

  virtual bool ReleaseTexImage() override;

  void SetEGLSurfaceOverride(EGLSurface surf);
  EGLSurface GetEGLSurfaceOverride() { return mSurfaceOverride; }

  virtual bool MakeCurrentImpl() const override;

  virtual bool IsCurrentImpl() const override;

  virtual bool RenewSurface(widget::CompositorWidget* aWidget) override;

  virtual void ReleaseSurface() override;

  Maybe<SymbolLoader> GetSymbolLoader() const override;

  virtual bool SwapBuffers() override;

  virtual void GetWSIInfo(nsCString* const out) const override;

  // hold a reference to the given surface
  // for the lifetime of this context.
  void HoldSurface(gfxASurface* aSurf);

  EGLSurface GetEGLSurface() const { return mSurface; }

  EGLDisplay GetEGLDisplay() const { return GLLibraryEGL::Get()->Display(); }

  bool BindTex2DOffscreen(GLContext* aOffscreen);
  void UnbindTex2DOffscreen(GLContext* aOffscreen);
  void BindOffscreenFramebuffer();

  void Destroy();

  static already_AddRefed<GLContextEGL> CreateEGLPBufferOffscreenContext(
      CreateContextFlags flags, const gfx::IntSize& size,
      const SurfaceCaps& minCaps, nsACString* const out_FailureId);

#if defined(MOZ_WAYLAND) || defined(MOZ_WIDGET_ANDROID)
  static EGLSurface CreateEGLSurfaceForCompositorWidget(
      widget::CompositorWidget* aCompositorWidget, const EGLConfig aConfig);
#endif
 protected:
  friend class GLContextProviderEGL;
  friend class GLContextEGLFactory;

  virtual void OnMarkDestroyed() override;

 public:
  const EGLConfig mConfig;

 protected:
  const RefPtr<GLLibraryEGL> mEgl;
  EGLSurface mSurface;
  const EGLSurface mFallbackSurface;

 public:
  const EGLContext mContext;

 protected:
  EGLSurface mSurfaceOverride = EGL_NO_SURFACE;
  RefPtr<gfxASurface> mThebesSurface;
  bool mBound = false;

  bool mIsPBuffer = false;
  bool mIsDoubleBuffered = false;
  bool mCanBindToTexture = false;
  bool mShareWithEGLImage = false;
  bool mOwnsContext = true;

  static EGLSurface CreatePBufferSurfaceTryingPowerOfTwo(
      EGLConfig config, EGLenum bindToTextureFormat, gfx::IntSize& pbsize);
#if defined(MOZ_WAYLAND)
  static EGLSurface CreateWaylandBufferSurface(EGLConfig config,
                                               gfx::IntSize& pbsize);
#endif
#if defined(MOZ_WIDGET_ANDROID)
 public:
  EGLSurface CreateCompatibleSurface(void* aWindow);
#endif  // defined(MOZ_WIDGET_ANDROID)
};

bool CreateConfig(EGLConfig* config, int32_t depth, bool enableDepthBuffer);

}  // namespace gl
}  // namespace mozilla

#endif  // GLCONTEXTEGL_H_
