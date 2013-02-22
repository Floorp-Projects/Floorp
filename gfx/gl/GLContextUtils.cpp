/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"

#include "mozilla/Preferences.h"
#include "mozilla/Assertions.h"
#include "mozilla/StandardInteger.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace gl {

static const char kTexBlit_VertShaderSource[] = "\
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

static const char kTexBlit_FragShaderSource[] = "\
uniform sampler2D uTexUnit;                         \n\
                                                    \n\
varying vec2 vTexCoord;                             \n\
                                                    \n\
void main(void) {                                   \n\
    gl_FragColor = texture2D(uTexUnit, vTexCoord);  \n\
}                                                   \n\
";

// Allowed to be destructive of state we restore in functions below.
bool
GLContext::UseTexQuadProgram()
{
    bool success = false;

    // Use do-while(false) to let us break on failure
    do {
        if (mTexBlit_Program) {
            // Already have it...
            success = true;
            break;
        }

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
        fGenBuffers(1, &mTexBlit_Buffer);
        fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mTexBlit_Buffer);

        const size_t vertsSize = sizeof(verts);
        MOZ_ASSERT(vertsSize >= 3 * sizeof(GLfloat)); // Make sure we have a sane size.
        fBufferData(LOCAL_GL_ARRAY_BUFFER, vertsSize, verts, LOCAL_GL_STATIC_DRAW);

        fEnableVertexAttribArray(0);
        fVertexAttribPointer(0,
                             2,
                             LOCAL_GL_FLOAT,
                             false,
                             0,
                             nullptr);

        MOZ_ASSERT(!mTexBlit_VertShader);
        MOZ_ASSERT(!mTexBlit_FragShader);

        const char* vertShaderSource = kTexBlit_VertShaderSource;
        const char* fragShaderSource = kTexBlit_FragShaderSource;

        mTexBlit_VertShader = fCreateShader(LOCAL_GL_VERTEX_SHADER);
        fShaderSource(mTexBlit_VertShader, 1, &vertShaderSource, nullptr);
        fCompileShader(mTexBlit_VertShader);

        mTexBlit_FragShader = fCreateShader(LOCAL_GL_FRAGMENT_SHADER);
        fShaderSource(mTexBlit_FragShader, 1, &fragShaderSource, nullptr);
        fCompileShader(mTexBlit_FragShader);

        mTexBlit_Program = fCreateProgram();
        fAttachShader(mTexBlit_Program, mTexBlit_VertShader);
        fAttachShader(mTexBlit_Program, mTexBlit_FragShader);
        fBindAttribLocation(mTexBlit_Program, 0, "aPosition");
        fLinkProgram(mTexBlit_Program);

        if (DebugMode()) {
            GLint status = 0;
            fGetShaderiv(mTexBlit_VertShader, LOCAL_GL_COMPILE_STATUS, &status);
            if (status != LOCAL_GL_TRUE) {
                NS_ERROR("Vert shader compilation failed.");

                GLint length = 0;
                fGetShaderiv(mTexBlit_VertShader, LOCAL_GL_INFO_LOG_LENGTH, &length);
                if (!length) {
                    printf_stderr("No shader info log available.\n");
                    break;
                }

                nsAutoArrayPtr<char> buffer(new char[length]);
                fGetShaderInfoLog(mTexBlit_VertShader, length, nullptr, buffer);

                printf_stderr("Shader info log (%d bytes): %s\n", length, buffer.get());
                break;
            }

            status = 0;
            fGetShaderiv(mTexBlit_FragShader, LOCAL_GL_COMPILE_STATUS, &status);
            if (status != LOCAL_GL_TRUE) {
                NS_ERROR("Frag shader compilation failed.");

                GLint length = 0;
                fGetShaderiv(mTexBlit_FragShader, LOCAL_GL_INFO_LOG_LENGTH, &length);
                if (!length) {
                    printf_stderr("No shader info log available.\n");
                    break;
                }

                nsAutoArrayPtr<char> buffer(new char[length]);
                fGetShaderInfoLog(mTexBlit_FragShader, length, nullptr, buffer);

                printf_stderr("Shader info log (%d bytes): %s\n", length, buffer.get());
                break;
            }
        }

        GLint status = 0;
        fGetProgramiv(mTexBlit_Program, LOCAL_GL_LINK_STATUS, &status);
        if (status != LOCAL_GL_TRUE) {
            if (DebugMode()) {
                NS_ERROR("Linking blit program failed.");
                GLint length = 0;
                fGetProgramiv(mTexBlit_Program, LOCAL_GL_INFO_LOG_LENGTH, &length);
                if (!length) {
                    printf_stderr("No program info log available.\n");
                    break;
                }

                nsAutoArrayPtr<char> buffer(new char[length]);
                fGetProgramInfoLog(mTexBlit_Program, length, nullptr, buffer);

                printf_stderr("Program info log (%d bytes): %s\n", length, buffer.get());
            }
            break;
        }

        MOZ_ASSERT(fGetAttribLocation(mTexBlit_Program, "aPosition") == 0);
        GLuint texUnitLoc = fGetUniformLocation(mTexBlit_Program, "uTexUnit");

        // Set uniforms here:
        fUseProgram(mTexBlit_Program);
        fUniform1i(texUnitLoc, 0);

        success = true;
    } while (false);

    if (!success) {
        NS_ERROR("Creating program for texture blit failed!");

        // Clean up:
        DeleteTexBlitProgram();
        return false;
    }

    fUseProgram(mTexBlit_Program);
    fEnableVertexAttribArray(0);
    fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mTexBlit_Buffer);
    fVertexAttribPointer(0,
                         2,
                         LOCAL_GL_FLOAT,
                         false,
                         0,
                         nullptr);
    return true;
}

void
GLContext::DeleteTexBlitProgram()
{
    if (mTexBlit_Buffer) {
        fDeleteBuffers(1, &mTexBlit_Buffer);
        mTexBlit_Buffer = 0;
    }
    if (mTexBlit_VertShader) {
        fDeleteShader(mTexBlit_VertShader);
        mTexBlit_VertShader = 0;
    }
    if (mTexBlit_FragShader) {
        fDeleteShader(mTexBlit_FragShader);
        mTexBlit_FragShader = 0;
    }
    if (mTexBlit_Program) {
        fDeleteProgram(mTexBlit_Program);
        mTexBlit_Program = 0;
    }
}

void
GLContext::BlitFramebufferToFramebuffer(GLuint srcFB, GLuint destFB,
                                        const gfxIntSize& srcSize,
                                        const gfxIntSize& destSize)
{
    MOZ_ASSERT(!srcFB || fIsFramebuffer(srcFB));
    MOZ_ASSERT(!destFB || fIsFramebuffer(destFB));

    MOZ_ASSERT(IsExtensionSupported(EXT_framebuffer_blit) ||
               IsExtensionSupported(ANGLE_framebuffer_blit));

    ScopedFramebufferBinding boundFB(this);
    ScopedGLState scissor(this, LOCAL_GL_SCISSOR_TEST, false);

    BindUserReadFBO(srcFB);
    BindUserDrawFBO(destFB);

    fBlitFramebuffer(0, 0,  srcSize.width,  srcSize.height,
                     0, 0, destSize.width, destSize.height,
                     LOCAL_GL_COLOR_BUFFER_BIT,
                     LOCAL_GL_NEAREST);
}

void
GLContext::BlitTextureToFramebuffer(GLuint srcTex, GLuint destFB,
                                    const gfxIntSize& srcSize,
                                    const gfxIntSize& destSize)
{
    MOZ_ASSERT(fIsTexture(srcTex));
    MOZ_ASSERT(!destFB || fIsFramebuffer(destFB));

    if (IsExtensionSupported(EXT_framebuffer_blit) ||
        IsExtensionSupported(ANGLE_framebuffer_blit))
    {
        ScopedFramebufferTexture srcWrapper(this, srcTex);
        MOZ_ASSERT(srcWrapper.IsComplete());

        BlitFramebufferToFramebuffer(srcWrapper.FB(), destFB,
                                     srcSize, destSize);
        return;
    }


    ScopedFramebufferBinding boundFB(this, destFB);

    GLuint boundTexUnit = 0;
    GetUIntegerv(LOCAL_GL_ACTIVE_TEXTURE, &boundTexUnit);
    fActiveTexture(LOCAL_GL_TEXTURE0);

    GLuint boundTex = 0;
    GetUIntegerv(LOCAL_GL_TEXTURE_BINDING_2D, &boundTex);
    fBindTexture(LOCAL_GL_TEXTURE_2D, srcTex);

    GLuint boundProgram = 0;
    GetUIntegerv(LOCAL_GL_CURRENT_PROGRAM, &boundProgram);

    GLuint boundBuffer = 0;
    GetUIntegerv(LOCAL_GL_ARRAY_BUFFER_BINDING, &boundBuffer);

    /*
     * fGetVertexAttribiv takes:
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

    fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_ENABLED, &attrib0_enabled);
    fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_SIZE, &attrib0_size);
    fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_STRIDE, &attrib0_stride);
    fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_TYPE, &attrib0_type);
    fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &attrib0_normalized);
    fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &attrib0_bufferBinding);
    fGetVertexAttribPointerv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_POINTER, &attrib0_pointer);
    // Note that uniform values are program state, so we don't need to rebind those.

    ScopedGLState blend       (this, LOCAL_GL_BLEND,      false);
    ScopedGLState cullFace    (this, LOCAL_GL_CULL_FACE,  false);
    ScopedGLState depthTest   (this, LOCAL_GL_DEPTH_TEST, false);
    ScopedGLState dither      (this, LOCAL_GL_DITHER,     false);
    ScopedGLState polyOffsFill(this, LOCAL_GL_POLYGON_OFFSET_FILL,      false);
    ScopedGLState sampleAToC  (this, LOCAL_GL_SAMPLE_ALPHA_TO_COVERAGE, false);
    ScopedGLState sampleCover (this, LOCAL_GL_SAMPLE_COVERAGE, false);
    ScopedGLState scissor     (this, LOCAL_GL_SCISSOR_TEST,    false);
    ScopedGLState stencil     (this, LOCAL_GL_STENCIL_TEST,    false);

    realGLboolean colorMask[4];
    fGetBooleanv(LOCAL_GL_COLOR_WRITEMASK, colorMask);
    fColorMask(LOCAL_GL_TRUE,
               LOCAL_GL_TRUE,
               LOCAL_GL_TRUE,
               LOCAL_GL_TRUE);

    GLint viewport[4];
    fGetIntegerv(LOCAL_GL_VIEWPORT, viewport);
    fViewport(0, 0, destSize.width, destSize.height);

    // Does destructive things to (only!) what we just saved above.
    bool good = UseTexQuadProgram();
    if (!good) {
        // We're up against the wall, so bail.
        // This should really be MOZ_CRASH(why) or MOZ_RUNTIME_ASSERT(good).
        printf_stderr("[%s:%d] Fatal Error: Failed to prepare to blit texture->framebuffer.\n",
                      __FILE__, __LINE__);
        MOZ_CRASH();
    }
    fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);

    fViewport(viewport[0], viewport[1],
              viewport[2], viewport[3]);

    fColorMask(colorMask[0],
               colorMask[1],
               colorMask[2],
               colorMask[3]);

    if (attrib0_enabled)
        fEnableVertexAttribArray(0);

    fBindBuffer(LOCAL_GL_ARRAY_BUFFER, attrib0_bufferBinding);
    fVertexAttribPointer(0,
                         attrib0_size,
                         attrib0_type,
                         attrib0_normalized,
                         attrib0_stride,
                         attrib0_pointer);

    fBindBuffer(LOCAL_GL_ARRAY_BUFFER, boundBuffer);

    fUseProgram(boundProgram);
    fBindTexture(LOCAL_GL_TEXTURE_2D, boundTex);
    fActiveTexture(boundTexUnit);
}

void
GLContext::BlitFramebufferToTexture(GLuint srcFB, GLuint destTex,
                                    const gfxIntSize& srcSize,
                                    const gfxIntSize& destSize)
{
    MOZ_ASSERT(!srcFB || fIsFramebuffer(srcFB));
    MOZ_ASSERT(fIsTexture(destTex));

    if (IsExtensionSupported(EXT_framebuffer_blit) ||
        IsExtensionSupported(ANGLE_framebuffer_blit))
    {
        ScopedFramebufferTexture destWrapper(this, destTex);
        MOZ_ASSERT(destWrapper.IsComplete());

        BlitFramebufferToFramebuffer(srcFB, destWrapper.FB(),
                                     srcSize, destSize);
        return;
    }

    GLuint boundTex = 0;
    GetUIntegerv(LOCAL_GL_TEXTURE_BINDING_2D, &boundTex);
    fBindTexture(LOCAL_GL_TEXTURE_2D, destTex);

    ScopedFramebufferBinding boundFB(this, srcFB);
    ScopedGLState scissor(this, LOCAL_GL_SCISSOR_TEST, false);

    fCopyTexSubImage2D(LOCAL_GL_TEXTURE_2D, 0,
                       0, 0,
                       0, 0,
                       srcSize.width, srcSize.height);

    fBindTexture(LOCAL_GL_TEXTURE_2D, boundTex);
}

void
GLContext::BlitTextureToTexture(GLuint srcTex, GLuint destTex,
                                const gfxIntSize& srcSize,
                                const gfxIntSize& destSize)
{
    MOZ_ASSERT(fIsTexture(srcTex));
    MOZ_ASSERT(fIsTexture(destTex));

    if (mTexBlit_UseDrawNotCopy) {
        // Draw is texture->framebuffer
        ScopedFramebufferTexture destWrapper(this, destTex);
        MOZ_ASSERT(destWrapper.IsComplete());

        BlitTextureToFramebuffer(srcTex, destWrapper.FB(),
                                 srcSize, destSize);
        return;
    }

    // Generally, just use the CopyTexSubImage path
    ScopedFramebufferTexture srcWrapper(this, srcTex);
    MOZ_ASSERT(srcWrapper.IsComplete());

    BlitFramebufferToTexture(srcWrapper.FB(), destTex,
                             srcSize, destSize);
}

uint32_t GetBitsPerTexel(GLenum format, GLenum type)
{
    // If there is no defined format or type, we're not taking up any memory
    if (!format || !type) {
        return 0;
    }

    if (format == LOCAL_GL_DEPTH_COMPONENT) {
        if (type == LOCAL_GL_UNSIGNED_SHORT)
            return 2;
        else if (type == LOCAL_GL_UNSIGNED_INT)
            return 4;
    } else if (format == LOCAL_GL_DEPTH_STENCIL) {
        if (type == LOCAL_GL_UNSIGNED_INT_24_8_EXT)
            return 4;
    }

    if (type == LOCAL_GL_UNSIGNED_BYTE || type == LOCAL_GL_FLOAT) {
        int multiplier = type == LOCAL_GL_FLOAT ? 32 : 8;
        switch (format) {
            case LOCAL_GL_ALPHA:
            case LOCAL_GL_LUMINANCE:
                return 1 * multiplier;
            case LOCAL_GL_LUMINANCE_ALPHA:
                return 2 * multiplier;
            case LOCAL_GL_RGB:
                return 3 * multiplier;
            case LOCAL_GL_RGBA:
                return 4 * multiplier;
            case LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1:
            case LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1:
                return 2;
            case LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            case LOCAL_GL_ATC_RGB:
            case LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1:
            case LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1:
                return 4;
            case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            case LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA:
            case LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA:
                return 8;
            default:
                break;
        }
    } else if (type == LOCAL_GL_UNSIGNED_SHORT_4_4_4_4 ||
               type == LOCAL_GL_UNSIGNED_SHORT_5_5_5_1 ||
               type == LOCAL_GL_UNSIGNED_SHORT_5_6_5)
    {
        return 16;
    }

    MOZ_ASSERT(false);
    return 0;
}


} /* namespace gl */
} /* namespace mozilla */
