/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTEGL_H_
#define GLCONTEXTEGL_H_

#include "GLContext.h"
#include "GLLibraryEGL.h"

class nsIWidget;

namespace mozilla {
namespace gl {

class GLContextEGL : public GLContext
{
    friend class TextureImageEGL;

    static already_AddRefed<GLContextEGL>
    CreateGLContext(CreateContextFlags flags,
                    const SurfaceCaps& caps,
                    GLContextEGL *shareContext,
                    bool isOffscreen,
                    EGLConfig config,
                    EGLSurface surface);

public:
    MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GLContextEGL, override)
    GLContextEGL(const SurfaceCaps& caps,
                 GLContext* shareContext,
                 bool isOffscreen,
                 EGLConfig config,
                 EGLSurface surface,
                 EGLContext context);

    ~GLContextEGL();

    virtual GLContextType GetContextType() const override { return GLContextType::EGL; }

    static GLContextEGL* Cast(GLContext* gl) {
        MOZ_ASSERT(gl->GetContextType() == GLContextType::EGL);
        return static_cast<GLContextEGL*>(gl);
    }

    static EGLSurface CreateSurfaceForWindow(nsIWidget* aWidget);

    static void DestroySurface(EGLSurface aSurface);

    bool Init() override;

    virtual bool IsDoubleBuffered() const override {
        return mIsDoubleBuffered;
    }

    void SetIsDoubleBuffered(bool aIsDB) {
        mIsDoubleBuffered = aIsDB;
    }

    virtual bool SupportsRobustness() const override {
        return sEGLLibrary.HasRobustness();
    }

    virtual bool IsANGLE() const override {
        return sEGLLibrary.IsANGLE();
    }

    virtual bool IsWARP() const override {
        return sEGLLibrary.IsWARP();
    }

    virtual bool BindTexImage() override;

    virtual bool ReleaseTexImage() override;

    void SetEGLSurfaceOverride(EGLSurface surf);

    virtual bool MakeCurrentImpl(bool aForce) override;

    virtual bool IsCurrent() override;

    virtual bool RenewSurface() override;

    virtual void ReleaseSurface() override;

    virtual bool SetupLookupFunction() override;

    virtual bool SwapBuffers() override;

    // hold a reference to the given surface
    // for the lifetime of this context.
    void HoldSurface(gfxASurface *aSurf);

    EGLSurface GetEGLSurface() const {
        return mSurface;
    }

    EGLDisplay GetEGLDisplay() const {
        return sEGLLibrary.Display();
    }

    bool BindTex2DOffscreen(GLContext *aOffscreen);
    void UnbindTex2DOffscreen(GLContext *aOffscreen);
    void BindOffscreenFramebuffer();

    static already_AddRefed<GLContextEGL>
    CreateEGLPixmapOffscreenContext(const gfx::IntSize& size);

    static already_AddRefed<GLContextEGL>
    CreateEGLPBufferOffscreenContext(CreateContextFlags flags,
                                     const gfx::IntSize& size,
                                     const SurfaceCaps& minCaps);

protected:
    friend class GLContextProviderEGL;

public:
    const EGLConfig  mConfig;
protected:
    EGLSurface mSurface;
public:
    const EGLContext mContext;
protected:
    EGLSurface mSurfaceOverride;
    RefPtr<gfxASurface> mThebesSurface;
    bool mBound;

    bool mIsPBuffer;
    bool mIsDoubleBuffered;
    bool mCanBindToTexture;
    bool mShareWithEGLImage;
    bool mOwnsContext;

    static EGLSurface CreatePBufferSurfaceTryingPowerOfTwo(EGLConfig config,
                                                           EGLenum bindToTextureFormat,
                                                           gfx::IntSize& pbsize);
};

} // namespace gl
} // namespace mozilla

#endif // GLCONTEXTEGL_H_
