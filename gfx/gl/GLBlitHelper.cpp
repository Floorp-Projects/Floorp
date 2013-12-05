/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLBlitHelper.h"
#include "GLContext.h"
#include "ScopedGLHelpers.h"
#include "mozilla/Preferences.h"

namespace mozilla {
namespace gl {

GLBlitHelper::GLBlitHelper(GLContext* gl)
    : mGL(gl)
    , mTexBlit_Buffer(0)
    , mTexBlit_VertShader(0)
    , mTex2DBlit_FragShader(0)
    , mTex2DRectBlit_FragShader(0)
    , mTex2DBlit_Program(0)
    , mTex2DRectBlit_Program(0)
{
}

GLBlitHelper::~GLBlitHelper()
{
    DeleteTexBlitProgram();
}

// Allowed to be destructive of state we restore in functions below.
bool
GLBlitHelper::InitTexQuadProgram(GLenum target)
{
    const char kTexBlit_VertShaderSource[] = "\
    attribute vec2 aPosition;                   \n\
                                                \n\
    varying vec2 vTexCoord;                     \n\
                                                \n\
    void main(void) {                           \n\
        vTexCoord = aPosition;                  \n\
        vec2 vertPos = aPosition * 2.0 - 1.0;   \n\
        gl_Position = vec4(vertPos, 0.0, 1.0);  \n\
    }                                           \n\
    ";

    const char kTex2DBlit_FragShaderSource[] = "\
    #ifdef GL_FRAGMENT_PRECISION_HIGH                   \n\
        precision highp float;                          \n\
    #else                                               \n\
        precision mediump float;                        \n\
    #endif                                              \n\
                                                        \n\
    uniform sampler2D uTexUnit;                         \n\
                                                        \n\
    varying vec2 vTexCoord;                             \n\
                                                        \n\
    void main(void) {                                   \n\
        gl_FragColor = texture2D(uTexUnit, vTexCoord);  \n\
    }                                                   \n\
    ";

    const char kTex2DRectBlit_FragShaderSource[] = "\
    #ifdef GL_FRAGMENT_PRECISION_HIGH                             \n\
        precision highp float;                                    \n\
    #else                                                         \n\
        precision mediump float;                                  \n\
    #endif                                                        \n\
                                                                  \n\
    uniform sampler2D uTexUnit;                                   \n\
    uniform vec2 uTexCoordMult;                                   \n\
                                                                  \n\
    varying vec2 vTexCoord;                                       \n\
                                                                  \n\
    void main(void) {                                             \n\
        gl_FragColor = texture2DRect(uTexUnit,                    \n\
                                    vTexCoord * uTexCoordMult);  \n\
    }                                                             \n\
    ";

    MOZ_ASSERT(target == LOCAL_GL_TEXTURE_2D ||
               target == LOCAL_GL_TEXTURE_RECTANGLE_ARB);
    bool success = false;

    GLuint *programPtr;
    GLuint *fragShaderPtr;
    const char* fragShaderSource;
    if (target == LOCAL_GL_TEXTURE_2D) {
        programPtr = &mTex2DBlit_Program;
        fragShaderPtr = &mTex2DBlit_FragShader;
        fragShaderSource = kTex2DBlit_FragShaderSource;
    } else {
        programPtr = &mTex2DRectBlit_Program;
        fragShaderPtr = &mTex2DRectBlit_FragShader;
        fragShaderSource = kTex2DRectBlit_FragShaderSource;
    }

    GLuint& program = *programPtr;
    GLuint& fragShader = *fragShaderPtr;

    // Use do-while(false) to let us break on failure
    do {
        if (program) {
            // Already have it...
            success = true;
            break;
        }

        if (!mTexBlit_Buffer) {

            /* CCW tri-strip:
             * 2---3
             * | \ |
             * 0---1
             */
            GLfloat verts[] = {
                0.0f, 0.0f,
                1.0f, 0.0f,
                0.0f, 1.0f,
                1.0f, 1.0f
            };

            MOZ_ASSERT(!mTexBlit_Buffer);
            mGL->fGenBuffers(1, &mTexBlit_Buffer);
            mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mTexBlit_Buffer);

            const size_t vertsSize = sizeof(verts);
            // Make sure we have a sane size.
            MOZ_ASSERT(vertsSize >= 3 * sizeof(GLfloat));
            mGL->fBufferData(LOCAL_GL_ARRAY_BUFFER, vertsSize, verts, LOCAL_GL_STATIC_DRAW);
        }

        if (!mTexBlit_VertShader) {

            const char* vertShaderSource = kTexBlit_VertShaderSource;

            mTexBlit_VertShader = mGL->fCreateShader(LOCAL_GL_VERTEX_SHADER);
            mGL->fShaderSource(mTexBlit_VertShader, 1, &vertShaderSource, nullptr);
            mGL->fCompileShader(mTexBlit_VertShader);
        }

        MOZ_ASSERT(!fragShader);
        fragShader = mGL->fCreateShader(LOCAL_GL_FRAGMENT_SHADER);
        mGL->fShaderSource(fragShader, 1, &fragShaderSource, nullptr);
        mGL->fCompileShader(fragShader);

        program = mGL->fCreateProgram();
        mGL->fAttachShader(program, mTexBlit_VertShader);
        mGL->fAttachShader(program, fragShader);
        mGL->fBindAttribLocation(program, 0, "aPosition");
        mGL->fLinkProgram(program);

        if (mGL->DebugMode()) {
            GLint status = 0;
            mGL->fGetShaderiv(mTexBlit_VertShader, LOCAL_GL_COMPILE_STATUS, &status);
            if (status != LOCAL_GL_TRUE) {
                NS_ERROR("Vert shader compilation failed.");

                GLint length = 0;
                mGL->fGetShaderiv(mTexBlit_VertShader, LOCAL_GL_INFO_LOG_LENGTH, &length);
                if (!length) {
                    printf_stderr("No shader info log available.\n");
                    break;
                }

                nsAutoArrayPtr<char> buffer(new char[length]);
                mGL->fGetShaderInfoLog(mTexBlit_VertShader, length, nullptr, buffer);

                printf_stderr("Shader info log (%d bytes): %s\n", length, buffer.get());
                break;
            }

            status = 0;
            mGL->fGetShaderiv(fragShader, LOCAL_GL_COMPILE_STATUS, &status);
            if (status != LOCAL_GL_TRUE) {
                NS_ERROR("Frag shader compilation failed.");

                GLint length = 0;
                mGL->fGetShaderiv(fragShader, LOCAL_GL_INFO_LOG_LENGTH, &length);
                if (!length) {
                    printf_stderr("No shader info log available.\n");
                    break;
                }

                nsAutoArrayPtr<char> buffer(new char[length]);
                mGL->fGetShaderInfoLog(fragShader, length, nullptr, buffer);

                printf_stderr("Shader info log (%d bytes): %s\n", length, buffer.get());
                break;
            }
        }

        GLint status = 0;
        mGL->fGetProgramiv(program, LOCAL_GL_LINK_STATUS, &status);
        if (status != LOCAL_GL_TRUE) {
            if (mGL->DebugMode()) {
                NS_ERROR("Linking blit program failed.");
                GLint length = 0;
                mGL->fGetProgramiv(program, LOCAL_GL_INFO_LOG_LENGTH, &length);
                if (!length) {
                    printf_stderr("No program info log available.\n");
                    break;
                }

                nsAutoArrayPtr<char> buffer(new char[length]);
                mGL->fGetProgramInfoLog(program, length, nullptr, buffer);

                printf_stderr("Program info log (%d bytes): %s\n", length, buffer.get());
            }
            break;
        }

        MOZ_ASSERT(mGL->fGetAttribLocation(program, "aPosition") == 0);
        GLint texUnitLoc = mGL->fGetUniformLocation(program, "uTexUnit");
        MOZ_ASSERT(texUnitLoc != -1, "uniform not found");

        // Set uniforms here:
        mGL->fUseProgram(program);
        mGL->fUniform1i(texUnitLoc, 0);

        success = true;
    } while (false);

    if (!success) {
        NS_ERROR("Creating program for texture blit failed!");

        // Clean up:
        DeleteTexBlitProgram();
        return false;
    }

    mGL->fUseProgram(program);
    mGL->fEnableVertexAttribArray(0);
    mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mTexBlit_Buffer);
    mGL->fVertexAttribPointer(0,
                              2,
                              LOCAL_GL_FLOAT,
                              false,
                              0,
                              nullptr);
    return true;
}

bool
GLBlitHelper::UseTexQuadProgram(GLenum target, const gfxIntSize& srcSize)
{
    if (!InitTexQuadProgram(target)) {
        return false;
    }

    if (target == LOCAL_GL_TEXTURE_RECTANGLE_ARB) {
        GLint texCoordMultLoc = mGL->fGetUniformLocation(mTex2DRectBlit_Program, "uTexCoordMult");
        MOZ_ASSERT(texCoordMultLoc != -1, "uniform not found");
        mGL->fUniform2f(texCoordMultLoc, srcSize.width, srcSize.height);
    }

    return true;
}

void
GLBlitHelper::DeleteTexBlitProgram()
{
    if (mTexBlit_Buffer) {
        mGL->fDeleteBuffers(1, &mTexBlit_Buffer);
        mTexBlit_Buffer = 0;
    }
    if (mTexBlit_VertShader) {
        mGL->fDeleteShader(mTexBlit_VertShader);
        mTexBlit_VertShader = 0;
    }
    if (mTex2DBlit_FragShader) {
        mGL->fDeleteShader(mTex2DBlit_FragShader);
        mTex2DBlit_FragShader = 0;
    }
    if (mTex2DRectBlit_FragShader) {
        mGL->fDeleteShader(mTex2DRectBlit_FragShader);
        mTex2DRectBlit_FragShader = 0;
    }
    if (mTex2DBlit_Program) {
        mGL->fDeleteProgram(mTex2DBlit_Program);
        mTex2DBlit_Program = 0;
    }
    if (mTex2DRectBlit_Program) {
        mGL->fDeleteProgram(mTex2DRectBlit_Program);
        mTex2DRectBlit_Program = 0;
    }
}

void
GLBlitHelper::BlitFramebufferToFramebuffer(GLuint srcFB, GLuint destFB,
                                        const gfxIntSize& srcSize,
                                        const gfxIntSize& destSize)
{
    MOZ_ASSERT(!srcFB || mGL->fIsFramebuffer(srcFB));
    MOZ_ASSERT(!destFB || mGL->fIsFramebuffer(destFB));

    MOZ_ASSERT(mGL->IsSupported(GLFeature::framebuffer_blit));

    ScopedBindFramebuffer boundFB(mGL);
    ScopedGLState scissor(mGL, LOCAL_GL_SCISSOR_TEST, false);

    mGL->BindReadFB(srcFB);
    mGL->BindDrawFB(destFB);

    mGL->fBlitFramebuffer(0, 0,  srcSize.width,  srcSize.height,
                          0, 0, destSize.width, destSize.height,
                          LOCAL_GL_COLOR_BUFFER_BIT,
                          LOCAL_GL_NEAREST);
}

void
GLBlitHelper::BlitFramebufferToFramebuffer(GLuint srcFB, GLuint destFB,
                                        const gfxIntSize& srcSize,
                                        const gfxIntSize& destSize,
                                        const GLFormats& srcFormats)
{
    MOZ_ASSERT(!srcFB || mGL->fIsFramebuffer(srcFB));
    MOZ_ASSERT(!destFB || mGL->fIsFramebuffer(destFB));

    if (mGL->IsSupported(GLFeature::framebuffer_blit)) {
        BlitFramebufferToFramebuffer(srcFB, destFB,
                                     srcSize, destSize);
        return;
    }

    GLuint tex = mGL->CreateTextureForOffscreen(srcFormats, srcSize);
    MOZ_ASSERT(tex);

    BlitFramebufferToTexture(srcFB, tex, srcSize, srcSize);
    BlitTextureToFramebuffer(tex, destFB, srcSize, destSize);

    mGL->fDeleteTextures(1, &tex);
}

void
GLBlitHelper::BlitTextureToFramebuffer(GLuint srcTex, GLuint destFB,
                                    const gfxIntSize& srcSize,
                                    const gfxIntSize& destSize,
                                    GLenum srcTarget)
{
    MOZ_ASSERT(mGL->fIsTexture(srcTex));
    MOZ_ASSERT(!destFB || mGL->fIsFramebuffer(destFB));

    if (mGL->IsSupported(GLFeature::framebuffer_blit)) {
        ScopedFramebufferForTexture srcWrapper(mGL, srcTex, srcTarget);
        MOZ_ASSERT(srcWrapper.IsComplete());

        BlitFramebufferToFramebuffer(srcWrapper.FB(), destFB,
                                     srcSize, destSize);
        return;
    }


    ScopedBindFramebuffer boundFB(mGL, destFB);
    // UseTexQuadProgram initializes a shader that reads
    // from texture unit 0.
    ScopedBindTextureUnit boundTU(mGL, LOCAL_GL_TEXTURE0);
    ScopedBindTexture boundTex(mGL, srcTex, srcTarget);

    GLuint boundProgram = 0;
    mGL->GetUIntegerv(LOCAL_GL_CURRENT_PROGRAM, &boundProgram);

    GLuint boundBuffer = 0;
    mGL->GetUIntegerv(LOCAL_GL_ARRAY_BUFFER_BINDING, &boundBuffer);

    /*
     * mGL->fGetVertexAttribiv takes:
     *  VERTEX_ATTRIB_ARRAY_ENABLED
     *  VERTEX_ATTRIB_ARRAY_SIZE,
     *  VERTEX_ATTRIB_ARRAY_STRIDE,
     *  VERTEX_ATTRIB_ARRAY_TYPE,
     *  VERTEX_ATTRIB_ARRAY_NORMALIZED,
     *  VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
     *  CURRENT_VERTEX_ATTRIB
     *
     * CURRENT_VERTEX_ATTRIB is vertex shader state. \o/
     * Others appear to be vertex array state,
     * or alternatively in the internal vertex array state
     * for a buffer object.
    */

    GLint attrib0_enabled = 0;
    GLint attrib0_size = 0;
    GLint attrib0_stride = 0;
    GLint attrib0_type = 0;
    GLint attrib0_normalized = 0;
    GLint attrib0_bufferBinding = 0;
    void* attrib0_pointer = nullptr;

    mGL->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_ENABLED, &attrib0_enabled);
    mGL->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_SIZE, &attrib0_size);
    mGL->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_STRIDE, &attrib0_stride);
    mGL->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_TYPE, &attrib0_type);
    mGL->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &attrib0_normalized);
    mGL->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &attrib0_bufferBinding);
    mGL->fGetVertexAttribPointerv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_POINTER, &attrib0_pointer);
    // Note that uniform values are program state, so we don't need to rebind those.

    ScopedGLState blend       (mGL, LOCAL_GL_BLEND,      false);
    ScopedGLState cullFace    (mGL, LOCAL_GL_CULL_FACE,  false);
    ScopedGLState depthTest   (mGL, LOCAL_GL_DEPTH_TEST, false);
    ScopedGLState dither      (mGL, LOCAL_GL_DITHER,     false);
    ScopedGLState polyOffsFill(mGL, LOCAL_GL_POLYGON_OFFSET_FILL,      false);
    ScopedGLState sampleAToC  (mGL, LOCAL_GL_SAMPLE_ALPHA_TO_COVERAGE, false);
    ScopedGLState sampleCover (mGL, LOCAL_GL_SAMPLE_COVERAGE, false);
    ScopedGLState scissor     (mGL, LOCAL_GL_SCISSOR_TEST,    false);
    ScopedGLState stencil     (mGL, LOCAL_GL_STENCIL_TEST,    false);

    realGLboolean colorMask[4];
    mGL->fGetBooleanv(LOCAL_GL_COLOR_WRITEMASK, colorMask);
    mGL->fColorMask(LOCAL_GL_TRUE,
                    LOCAL_GL_TRUE,
                    LOCAL_GL_TRUE,
                    LOCAL_GL_TRUE);

    GLint viewport[4];
    mGL->fGetIntegerv(LOCAL_GL_VIEWPORT, viewport);
    mGL->fViewport(0, 0, destSize.width, destSize.height);

    // Does destructive things to (only!) what we just saved above.
    bool good = UseTexQuadProgram(srcTarget, srcSize);
    if (!good) {
        // We're up against the wall, so bail.
        // This should really be MOZ_CRASH(why) or MOZ_RUNTIME_ASSERT(good).
        printf_stderr("[%s:%d] Fatal Error: Failed to prepare to blit texture->framebuffer.\n",
                      __FILE__, __LINE__);
        MOZ_CRASH();
    }
    mGL->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);

    mGL->fViewport(viewport[0], viewport[1],
                   viewport[2], viewport[3]);

    mGL->fColorMask(colorMask[0],
                    colorMask[1],
                    colorMask[2],
                    colorMask[3]);

    if (attrib0_enabled)
        mGL->fEnableVertexAttribArray(0);

    mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, attrib0_bufferBinding);
    mGL->fVertexAttribPointer(0,
                              attrib0_size,
                              attrib0_type,
                              attrib0_normalized,
                              attrib0_stride,
                              attrib0_pointer);

    mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, boundBuffer);

    mGL->fUseProgram(boundProgram);
}

void
GLBlitHelper::BlitFramebufferToTexture(GLuint srcFB, GLuint destTex,
                                    const gfxIntSize& srcSize,
                                    const gfxIntSize& destSize,
                                    GLenum destTarget)
{
    MOZ_ASSERT(!srcFB || mGL->fIsFramebuffer(srcFB));
    MOZ_ASSERT(mGL->fIsTexture(destTex));

    if (mGL->IsSupported(GLFeature::framebuffer_blit)) {
        ScopedFramebufferForTexture destWrapper(mGL, destTex, destTarget);

        BlitFramebufferToFramebuffer(srcFB, destWrapper.FB(),
                                     srcSize, destSize);
        return;
    }

    ScopedBindTexture autoTex(mGL, destTex, destTarget);
    ScopedBindFramebuffer boundFB(mGL, srcFB);
    ScopedGLState scissor(mGL, LOCAL_GL_SCISSOR_TEST, false);

    mGL->fCopyTexSubImage2D(destTarget, 0,
                       0, 0,
                       0, 0,
                       srcSize.width, srcSize.height);
}

void
GLBlitHelper::BlitTextureToTexture(GLuint srcTex, GLuint destTex,
                                const gfxIntSize& srcSize,
                                const gfxIntSize& destSize,
                                GLenum srcTarget, GLenum destTarget)
{
    MOZ_ASSERT(mGL->fIsTexture(srcTex));
    MOZ_ASSERT(mGL->fIsTexture(destTex));

    // Generally, just use the CopyTexSubImage path
    ScopedFramebufferForTexture srcWrapper(mGL, srcTex, srcTarget);

    BlitFramebufferToTexture(srcWrapper.FB(), destTex,
                             srcSize, destSize, destTarget);
}

}
}
