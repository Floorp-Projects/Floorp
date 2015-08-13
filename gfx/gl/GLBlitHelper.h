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

namespace layers {
class Image;
class PlanarYCbCrImage;
class GrallocImage;
class SurfaceTextureImage;
class EGLImageImage;
} // namespace layers

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
                     GLenum aType, const gfx::IntSize& aSize, bool linear = true);

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
class GLBlitHelper final
{
    enum Channel
    {
        Channel_Y = 0,
        Channel_Cb,
        Channel_Cr,
        Channel_Max,
    };

    /**
     * BlitTex2D is used to copy blit the content of a GL_TEXTURE_2D object,
     * BlitTexRect is used to copy blit the content of a GL_TEXTURE_RECT object,
     * The difference between BlitTex2D and BlitTexRect is the texture type, which affect
     * the fragment shader a bit.
     *
     * ConvertGralloc is used to color convert copy blit the GrallocImage into a
     * normal RGB texture by egl_image_external extension
     * ConvertPlnarYcbCr is used to color convert copy blit the PlanarYCbCrImage
     * into a normal RGB texture by create textures of each color channel, and
     * convert it in GPU.
     * Convert type is created for canvas.
     */
    enum BlitType
    {
        BlitTex2D,
        BlitTexRect,
        ConvertGralloc,
        ConvertPlanarYCbCr,
        ConvertSurfaceTexture,
        ConvertEGLImage
    };
    // The GLContext is the sole owner of the GLBlitHelper.
    GLContext* mGL;

    GLuint mTexBlit_Buffer;
    GLuint mTexBlit_VertShader;
    GLuint mTex2DBlit_FragShader;
    GLuint mTex2DRectBlit_FragShader;
    GLuint mTex2DBlit_Program;
    GLuint mTex2DRectBlit_Program;

    GLint mYFlipLoc;

    GLint mTextureTransformLoc;

    // Data for image blit path
    GLuint mTexExternalBlit_FragShader;
    GLuint mTexYUVPlanarBlit_FragShader;
    GLuint mTexExternalBlit_Program;
    GLuint mTexYUVPlanarBlit_Program;
    GLuint mFBO;
    GLuint mSrcTexY;
    GLuint mSrcTexCb;
    GLuint mSrcTexCr;
    GLuint mSrcTexEGL;
    GLint mYTexScaleLoc;
    GLint mCbCrTexScaleLoc;
    int mTexWidth;
    int mTexHeight;

    // Cache some uniform values
    float mCurYScale;
    float mCurCbCrScale;

    void UseBlitProgram();
    void SetBlitFramebufferForDestTexture(GLuint aTexture);

    bool UseTexQuadProgram(BlitType target, const gfx::IntSize& srcSize);
    bool InitTexQuadProgram(BlitType target = BlitTex2D);
    void DeleteTexBlitProgram();
    void BindAndUploadYUVTexture(Channel which, uint32_t width, uint32_t height, void* data, bool allocation);
    void BindAndUploadEGLImage(EGLImage image, GLuint target);

#ifdef MOZ_WIDGET_GONK
    bool BlitGrallocImage(layers::GrallocImage* grallocImage);
#endif
    bool BlitPlanarYCbCrImage(layers::PlanarYCbCrImage* yuvImage);
#ifdef MOZ_WIDGET_ANDROID
    // Blit onto the current FB.
    bool BlitSurfaceTextureImage(layers::SurfaceTextureImage* stImage);
    bool BlitEGLImageImage(layers::EGLImageImage* eglImage);
#endif

    explicit GLBlitHelper(GLContext* gl);

    friend class GLContext;

public:
    ~GLBlitHelper();

    // If you don't have |srcFormats| for the 2nd definition,
    // then you'll need the framebuffer_blit extensions to use
    // the first BlitFramebufferToFramebuffer.
    void BlitFramebufferToFramebuffer(GLuint srcFB, GLuint destFB,
                                      const gfx::IntSize& srcSize,
                                      const gfx::IntSize& destSize,
                                      bool internalFBs = false);
    void BlitFramebufferToFramebuffer(GLuint srcFB, GLuint destFB,
                                      const gfx::IntSize& srcSize,
                                      const gfx::IntSize& destSize,
                                      const GLFormats& srcFormats,
                                      bool internalFBs = false);
    void BlitTextureToFramebuffer(GLuint srcTex, GLuint destFB,
                                  const gfx::IntSize& srcSize,
                                  const gfx::IntSize& destSize,
                                  GLenum srcTarget = LOCAL_GL_TEXTURE_2D,
                                  bool internalFBs = false);
    void BlitFramebufferToTexture(GLuint srcFB, GLuint destTex,
                                  const gfx::IntSize& srcSize,
                                  const gfx::IntSize& destSize,
                                  GLenum destTarget = LOCAL_GL_TEXTURE_2D,
                                  bool internalFBs = false);
    void BlitTextureToTexture(GLuint srcTex, GLuint destTex,
                              const gfx::IntSize& srcSize,
                              const gfx::IntSize& destSize,
                              GLenum srcTarget = LOCAL_GL_TEXTURE_2D,
                              GLenum destTarget = LOCAL_GL_TEXTURE_2D);
    bool BlitImageToFramebuffer(layers::Image* srcImage, const gfx::IntSize& destSize,
                                GLuint destFB, OriginPos destOrigin);
    bool BlitImageToTexture(layers::Image* srcImage, const gfx::IntSize& destSize,
                            GLuint destTex, GLenum destTarget, OriginPos destOrigin);
};

} // namespace gl
} // namespace mozilla

#endif // GLBLITHELPER_H_
