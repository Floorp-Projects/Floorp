/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERCOMPOSITOR_EGL_H
#define MOZILLA_GFX_RENDERCOMPOSITOR_EGL_H

#include "GLTypes.h"
#include "mozilla/webrender/RenderCompositor.h"

namespace mozilla {

namespace wr {

class RenderCompositorEGL : public RenderCompositor {
 public:
  static UniquePtr<RenderCompositor> Create(
      RefPtr<widget::CompositorWidget> aWidget);

#ifdef MOZ_WIDGET_ANDROID
  explicit RenderCompositorEGL(RefPtr<widget::CompositorWidget> aWidget);
#elif defined(MOZ_WAYLAND)
  RenderCompositorEGL(RefPtr<gl::GLContext> aGL,
                      RefPtr<widget::CompositorWidget> aWidget);
#endif
  virtual ~RenderCompositorEGL();

  bool BeginFrame() override;
  void EndFrame() override;
  void WaitForGPU() override;
  void Pause() override;
  bool Resume() override;

  gl::GLContext* gl() const override;

  bool MakeCurrent() override;

  bool UseANGLE() const override { return false; }

  LayoutDeviceIntSize GetBufferSize() override;

 protected:
#ifdef MOZ_WAYLAND
  static already_AddRefed<gl::GLContext> CreateGLContext();
#endif
  EGLSurface CreateEGLSurface();

  void DestroyEGLSurface();

#ifdef MOZ_WAYLAND
  const RefPtr<gl::GLContext> mGL;
#endif
  EGLSurface mEGLSurface;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDERCOMPOSITOR_EGL_H
