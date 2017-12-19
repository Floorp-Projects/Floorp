/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_FRAMEBUFFER_H_
#define MOZ_FRAMEBUFFER_H_

#include "gfx2DGlue.h"
#include "GLConsts.h"
#include "GLContextTypes.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"

namespace mozilla {
namespace gl {

class MozFramebuffer final
{
    const WeakPtr<GLContext> mWeakGL;
public:
    const gfx::IntSize mSize;
    const uint32_t mSamples;
    const GLuint mFB;
    const GLenum mColorTarget;
private:
    const GLuint mColorName;
    const GLuint mDepthRB;
    const GLuint mStencilRB;

public:
    static UniquePtr<MozFramebuffer> Create(GLContext* gl, const gfx::IntSize& size,
                                            uint32_t samples, bool depthStencil);

    static UniquePtr<MozFramebuffer> CreateWith(GLContext* gl, const gfx::IntSize& size,
                                                uint32_t samples, bool depthStencil,
                                                GLenum colorTarget, GLuint colorName);
private:
    MozFramebuffer(GLContext* gl, const gfx::IntSize& size, uint32_t samples,
                   bool depthStencil, GLenum colorTarget, GLuint colorName);
public:
    ~MozFramebuffer();

    GLuint ColorTex() const {
        if (mColorTarget == LOCAL_GL_RENDERBUFFER)
            return 0;
        return mColorName;
    }
};

} // namespace gl
} // namespace mozilla

#endif  // MOZ_FRAMEBUFFER_H_
