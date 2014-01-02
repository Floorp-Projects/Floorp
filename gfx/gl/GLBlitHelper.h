/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLBLITHELPER_H_
#define GLBLITHELPER_H_

#include "GLContextTypes.h"
#include "GLConsts.h"
#include "nsSize.h"
#include "mozilla/Attributes.h"
#include "mozilla/gfx/Point.h"

namespace mozilla {
namespace gl {

class GLContext;

/**
 * Helper function that creates a 2D texture aSize.width x aSize.height with
 * storage type specified by aFormats. Returns GL texture object id.
 *
 * See mozilla::gl::CreateTexture.
 */
GLuint CreateTextureForOffscreen(GLContext* aGL, const GLFormats& aFormats,
                                 const gfx::IntSize& aSize);

/**
 * Helper function that creates a 2D texture aSize.width x aSize.height with
 * storage type aInternalFormat. Returns GL texture object id.
 *
 * Initialize textyre parameters to:
 *    GL_TEXTURE_MIN_FILTER = GL_LINEAR
 *    GL_TEXTURE_MAG_FILTER = GL_LINEAR
 *    GL_TEXTURE_WRAP_S = GL_CLAMP_TO_EDGE
 *    GL_TEXTURE_WRAP_T = GL_CLAMP_TO_EDGE
 */
GLuint CreateTexture(GLContext* aGL, GLenum aInternalFormat, GLenum aFormat,
                     GLenum aType, const gfx::IntSize& aSize);

/**
 * Helper function to create, potentially, multisample render buffers suitable
 * for offscreen rendering. Buffers of size aSize.width x aSize.height with
 * storage specified by aFormat. returns GL render buffer object id.
 */
GLuint CreateRenderbuffer(GLContext* aGL, GLenum aFormat, GLsizei aSamples,
                          const gfx::IntSize& aSize);

/**
 * Helper function to create, potentially, multisample render buffers suitable
 * for offscreen rendering. Buffers of size aSize.width x aSize.height with
 * storage specified by aFormats. GL render buffer object ids are returned via
 * aColorMSRB, aDepthRB, and aStencilRB
 */
void CreateRenderbuffersForOffscreen(GLContext* aGL, const GLFormats& aFormats,
                                     const gfx::IntSize& aSize, bool aMultisample,
                                     GLuint* aColorMSRB, GLuint* aDepthRB,
                                     GLuint* aStencilRB);


/** Buffer blitting helper */
class GLBlitHelper MOZ_FINAL
{
    // The GLContext is the sole owner of the GLBlitHelper.
    GLContext* mGL;

    GLuint mTexBlit_Buffer;
    GLuint mTexBlit_VertShader;
    GLuint mTex2DBlit_FragShader;
    GLuint mTex2DRectBlit_FragShader;
    GLuint mTex2DBlit_Program;
    GLuint mTex2DRectBlit_Program;

    void UseBlitProgram();
    void SetBlitFramebufferForDestTexture(GLuint aTexture);

    bool UseTexQuadProgram(GLenum target, const gfx::IntSize& srcSize);
    bool InitTexQuadProgram(GLenum target = LOCAL_GL_TEXTURE_2D);
    void DeleteTexBlitProgram();

public:

    GLBlitHelper(GLContext* gl);
    ~GLBlitHelper();

    // If you don't have |srcFormats| for the 2nd definition,
    // then you'll need the framebuffer_blit extensions to use
    // the first BlitFramebufferToFramebuffer.
    void BlitFramebufferToFramebuffer(GLuint srcFB, GLuint destFB,
                                      const gfx::IntSize& srcSize,
                                      const gfx::IntSize& destSize);
    void BlitFramebufferToFramebuffer(GLuint srcFB, GLuint destFB,
                                      const gfx::IntSize& srcSize,
                                      const gfx::IntSize& destSize,
                                      const GLFormats& srcFormats);
    void BlitTextureToFramebuffer(GLuint srcTex, GLuint destFB,
                                  const gfx::IntSize& srcSize,
                                  const gfx::IntSize& destSize,
                                  GLenum srcTarget = LOCAL_GL_TEXTURE_2D);
    void BlitFramebufferToTexture(GLuint srcFB, GLuint destTex,
                                  const gfx::IntSize& srcSize,
                                  const gfx::IntSize& destSize,
                                  GLenum destTarget = LOCAL_GL_TEXTURE_2D);
    void BlitTextureToTexture(GLuint srcTex, GLuint destTex,
                              const gfx::IntSize& srcSize,
                              const gfx::IntSize& destSize,
                              GLenum srcTarget = LOCAL_GL_TEXTURE_2D,
                              GLenum destTarget = LOCAL_GL_TEXTURE_2D);
};

}
}

#endif // GLBLITHELPER_H_
