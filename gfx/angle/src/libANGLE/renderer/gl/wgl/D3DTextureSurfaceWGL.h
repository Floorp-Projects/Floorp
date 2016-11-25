
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// D3DTextureSurfaceWGL.h: WGL implementation of egl::Surface for D3D texture interop.

#ifndef LIBANGLE_RENDERER_GL_WGL_D3DTEXTIRESURFACEWGL_H_
#define LIBANGLE_RENDERER_GL_WGL_D3DTEXTIRESURFACEWGL_H_

#include "libANGLE/renderer/gl/SurfaceGL.h"

#include <GL/wglext.h>

namespace rx
{

class FunctionsGL;
class FunctionsWGL;
class DisplayWGL;
class StateManagerGL;
struct WorkaroundsGL;

class D3DTextureSurfaceWGL : public SurfaceGL
{
  public:
    D3DTextureSurfaceWGL(const egl::SurfaceState &state,
                         RendererGL *renderer,
                         EGLClientBuffer clientBuffer,
                         DisplayWGL *display,
                         HGLRC wglContext,
                         HDC deviceContext,
                         const FunctionsGL *functionsGL,
                         const FunctionsWGL *functionsWGL);
    ~D3DTextureSurfaceWGL() override;

    static egl::Error ValidateD3DTextureClientBuffer(EGLClientBuffer clientBuffer);

    egl::Error initialize() override;
    egl::Error makeCurrent() override;
    egl::Error unMakeCurrent() override;

    egl::Error swap() override;
    egl::Error postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height) override;
    egl::Error querySurfacePointerANGLE(EGLint attribute, void **value) override;
    egl::Error bindTexImage(gl::Texture *texture, EGLint buffer) override;
    egl::Error releaseTexImage(EGLint buffer) override;
    void setSwapInterval(EGLint interval) override;

    EGLint getWidth() const override;
    EGLint getHeight() const override;

    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;

    FramebufferImpl *createDefaultFramebuffer(const gl::FramebufferState &data) override;

  private:
    EGLClientBuffer mClientBuffer;

    RendererGL *mRenderer;

    DisplayWGL *mDisplay;
    StateManagerGL *mStateManager;
    const WorkaroundsGL &mWorkarounds;
    const FunctionsGL *mFunctionsGL;
    const FunctionsWGL *mFunctionsWGL;

    HGLRC mWGLContext;
    HDC mDeviceContext;

    size_t mWidth;
    size_t mHeight;

    HANDLE mDeviceHandle;
    IUnknown *mObject;
    HANDLE mBoundObjectTextureHandle;
    HANDLE mBoundObjectRenderbufferHandle;

    GLuint mRenderbufferID;
    GLuint mFramebufferID;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_WGL_D3DTEXTIRESURFACEWGL_H_
