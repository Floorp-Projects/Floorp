/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTEGL_H_
#define GLCONTEXTEGL_H_

#include "GLContext.h"
#include "GLLibraryEGL.h"

#ifdef MOZ_WIDGET_GONK
#include "HwcComposer2D.h"
#endif

namespace mozilla {
namespace gl {

class GLContextEGL : public GLContext
{
    friend class TextureImageEGL;

    static already_AddRefed<GLContextEGL>
    CreateGLContext(const SurfaceCaps& caps,
                    GLContextEGL *shareContext,
                    bool isOffscreen,
                    EGLConfig config,
                    EGLSurface surface);

public:
    MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GLContextEGL)
    GLContextEGL(const SurfaceCaps& caps,
                 GLContext* shareContext,
                 bool isOffscreen,
                 EGLConfig config,
                 EGLSurface surface,
                 EGLContext context);

    ~GLContextEGL();

    virtual GLContextType GetContextType() const MOZ_OVERRIDE { return GLContextType::EGL; }

    static GLContextEGL* Cast(GLContext* gl) {
        MOZ_ASSERT(gl->GetContextType() == GLContextType::EGL);
        return static_cast<GLContextEGL*>(gl);
    }

    bool Init();

    virtual bool IsDoubleBuffered() const MOZ_OVERRIDE {
        return mIsDoubleBuffered;
    }

    void SetIsDoubleBuffered(bool aIsDB) {
        mIsDoubleBuffered = aIsDB;
    }

    virtual bool SupportsRobustness() const MOZ_OVERRIDE {
        return sEGLLibrary.HasRobustness();
    }

    virtual bool IsANGLE() const MOZ_OVERRIDE {
        return sEGLLibrary.IsANGLE();
    }

    virtual bool BindTexImage() MOZ_OVERRIDE;

    virtual bool ReleaseTexImage() MOZ_OVERRIDE;

    void SetEGLSurfaceOverride(EGLSurface surf);

    virtual bool MakeCurrentImpl(bool aForce) MOZ_OVERRIDE;

    virtual bool IsCurrent() MOZ_OVERRIDE;

    virtual bool RenewSurface() MOZ_OVERRIDE;

    virtual void ReleaseSurface() MOZ_OVERRIDE;

    virtual bool SetupLookupFunction() MOZ_OVERRIDE;

    virtual bool SwapBuffers() MOZ_OVERRIDE;

    // hold a reference to the given surface
    // for the lifetime of this context.
    void HoldSurface(gfxASurface *aSurf);

    EGLContext GetEGLContext() {
        return mContext;
    }

    bool BindTex2DOffscreen(GLContext *aOffscreen);
    void UnbindTex2DOffscreen(GLContext *aOffscreen);
    void BindOffscreenFramebuffer();

    static already_AddRefed<GLContextEGL>
    CreateEGLPixmapOffscreenContext(const gfxIntSize& size);

    static already_AddRefed<GLContextEGL>
    CreateEGLPBufferOffscreenContext(const gfxIntSize& size);

protected:
    friend class GLContextProviderEGL;

    EGLConfig  mConfig;
    EGLSurface mSurface;
    EGLSurface mSurfaceOverride;
    EGLContext mContext;
    nsRefPtr<gfxASurface> mThebesSurface;
    bool mBound;

    bool mIsPBuffer;
    bool mIsDoubleBuffered;
    bool mCanBindToTexture;
    bool mShareWithEGLImage;
#ifdef MOZ_WIDGET_GONK
    nsRefPtr<HwcComposer2D> mHwc;
#endif

    static EGLSurface CreatePBufferSurfaceTryingPowerOfTwo(EGLConfig config,
                                                           EGLenum bindToTextureFormat,
                                                           gfxIntSize& pbsize);
};

}
}

#endif // GLCONTEXTEGL_H_
