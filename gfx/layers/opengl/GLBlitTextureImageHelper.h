/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLBLITTEXTUREIMAGEHELPER_H_
#define GLBLITTEXTUREIMAGEHELPER_H_

#include "mozilla/Attributes.h"
#include "GLContextTypes.h"
#include "GLConsts.h"
#include "mozilla/gfx/Rect.h"

namespace mozilla {
namespace gl {
    class GLContext;
    class TextureImage;
} // namespace gl
namespace layers {

class CompositorOGL;

class GLBlitTextureImageHelper final
{
    // The GLContext is the sole owner of the GLBlitTextureImageHelper.
    CompositorOGL* mCompositor;

    // lazy-initialized things
    GLuint mBlitProgram, mBlitFramebuffer;
    void UseBlitProgram();
    void SetBlitFramebufferForDestTexture(GLuint aTexture);

public:

    explicit GLBlitTextureImageHelper(CompositorOGL *gl);
    ~GLBlitTextureImageHelper();

    /**
     * Copy a rectangle from one TextureImage into another.  The
     * source and destination are given in integer coordinates, and
     * will be converted to texture coordinates.
     *
     * For the source texture, the wrap modes DO apply -- it's valid
     * to use REPEAT or PAD and expect appropriate behaviour if the source
     * rectangle extends beyond its bounds.
     *
     * For the destination texture, the wrap modes DO NOT apply -- the
     * destination will be clipped by the bounds of the texture.
     *
     * Note: calling this function will cause the following OpenGL state
     * to be changed:
     *
     *   - current program
     *   - framebuffer binding
     *   - viewport
     *   - blend state (will be enabled at end)
     *   - scissor state (will be enabled at end)
     *   - vertex attrib 0 and 1 (pointer and enable state [enable state will be disabled at exit])
     *   - array buffer binding (will be 0)
     *   - active texture (will be 0)
     *   - texture 0 binding
     */
    void BlitTextureImage(gl::TextureImage *aSrc, const gfx::IntRect& aSrcRect,
                          gl::TextureImage *aDst, const gfx::IntRect& aDstRect);
};

} // namespace layers
} // namespace mozilla

#endif // GLBLITTEXTUREIMAGEHELPER_H_
