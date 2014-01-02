/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLReadTexImageHelper.h"
#include "GLContext.h"
#include "OGLShaderProgram.h"
#include "gfxTypes.h"


namespace mozilla {
namespace gl {

GLReadTexImageHelper::GLReadTexImageHelper(GLContext* gl)
    : mGL(gl)
{
    mPrograms[0] = 0;
    mPrograms[1] = 0;
    mPrograms[2] = 0;
    mPrograms[3] = 0;
}

GLReadTexImageHelper::~GLReadTexImageHelper()
{
    mGL->fDeleteProgram(mPrograms[0]);
    mGL->fDeleteProgram(mPrograms[1]);
    mGL->fDeleteProgram(mPrograms[2]);
    mGL->fDeleteProgram(mPrograms[3]);
}

static const GLchar
readTextureImageVS[] =
    "attribute vec2 aVertex;\n"
    "attribute vec2 aTexCoord;\n"
    "varying vec2 vTexCoord;\n"
    "void main() { gl_Position = vec4(aVertex, 0, 1); vTexCoord = aTexCoord; }";

static const GLchar
readTextureImageFS_TEXTURE_2D[] =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec2 vTexCoord;\n"
    "uniform sampler2D uTexture;\n"
    "void main() { gl_FragColor = texture2D(uTexture, vTexCoord); }";


static const GLchar
readTextureImageFS_TEXTURE_2D_BGRA[] =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec2 vTexCoord;\n"
    "uniform sampler2D uTexture;\n"
    "void main() { gl_FragColor = texture2D(uTexture, vTexCoord).bgra; }";

static const GLchar
readTextureImageFS_TEXTURE_EXTERNAL[] =
    "#extension GL_OES_EGL_image_external : require\n"
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec2 vTexCoord;\n"
    "uniform samplerExternalOES uTexture;\n"
    "void main() { gl_FragColor = texture2D(uTexture, vTexCoord); }";

static const GLchar
readTextureImageFS_TEXTURE_RECTANGLE[] =
    "#extension GL_ARB_texture_rectangle\n"
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec2 vTexCoord;\n"
    "uniform sampler2DRect uTexture;\n"
    "void main() { gl_FragColor = texture2DRect(uTexture, vTexCoord).bgra; }";

GLuint
GLReadTexImageHelper::TextureImageProgramFor(GLenum aTextureTarget, int aShader) {
    int variant = 0;
    const GLchar* readTextureImageFS = nullptr;
    if (aTextureTarget == LOCAL_GL_TEXTURE_2D)
    {
        if (aShader == layers::BGRALayerProgramType ||
            aShader == layers::BGRXLayerProgramType)
        {   // Need to swizzle R/B.
            readTextureImageFS = readTextureImageFS_TEXTURE_2D_BGRA;
            variant = 1;
        }
        else
        {
            readTextureImageFS = readTextureImageFS_TEXTURE_2D;
            variant = 0;
        }
    } else if (aTextureTarget == LOCAL_GL_TEXTURE_EXTERNAL) {
        readTextureImageFS = readTextureImageFS_TEXTURE_EXTERNAL;
        variant = 2;
    } else if (aTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE) {
        readTextureImageFS = readTextureImageFS_TEXTURE_RECTANGLE;
        variant = 3;
    }

    /* This might be overkill, but assure that we don't access out-of-bounds */
    MOZ_ASSERT((size_t) variant < ArrayLength(mPrograms));
    if (!mPrograms[variant]) {
        GLuint vs = mGL->fCreateShader(LOCAL_GL_VERTEX_SHADER);
        const GLchar* vsSourcePtr = &readTextureImageVS[0];
        mGL->fShaderSource(vs, 1, &vsSourcePtr, nullptr);
        mGL->fCompileShader(vs);

        GLuint fs = mGL->fCreateShader(LOCAL_GL_FRAGMENT_SHADER);
        mGL->fShaderSource(fs, 1, &readTextureImageFS, nullptr);
        mGL->fCompileShader(fs);

        GLuint program = mGL->fCreateProgram();
        mGL->fAttachShader(program, vs);
        mGL->fAttachShader(program, fs);
        mGL->fBindAttribLocation(program, 0, "aVertex");
        mGL->fBindAttribLocation(program, 1, "aTexCoord");
        mGL->fLinkProgram(program);

        GLint success;
        mGL->fGetProgramiv(program, LOCAL_GL_LINK_STATUS, &success);

        if (!success) {
            mGL->fDeleteProgram(program);
            program = 0;
        }

        mGL->fDeleteShader(vs);
        mGL->fDeleteShader(fs);

        mPrograms[variant] = program;
    }

    return mPrograms[variant];
}

bool
GLReadTexImageHelper::DidGLErrorOccur(const char* str)
{
    GLenum error = mGL->fGetError();
    if (error != LOCAL_GL_NO_ERROR) {
        printf_stderr("GL ERROR: %s (0x%04x) %s\n",
                      mGL->GLErrorToString(error), error, str);
        return true;
    }

    return false;
}

bool
GLReadTexImageHelper::ReadBackPixelsIntoSurface(gfxImageSurface* aSurface, const gfxIntSize& aSize) {
    MOZ_ASSERT(aSurface->Format() == gfxImageFormatARGB32);
    MOZ_ASSERT(aSurface->Width() >= aSize.width);
    MOZ_ASSERT(aSurface->Height() >= aSize.height);
    MOZ_ASSERT(aSurface->Stride() == 4 * aSurface->Width());

    GLint oldPackAlignment;
    mGL->fGetIntegerv(LOCAL_GL_PACK_ALIGNMENT, &oldPackAlignment);

    if (oldPackAlignment != 4)
        mGL->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, 4);

    mGL->fReadPixels(0, 0, aSize.width, aSize.height,
                     LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE,
                     aSurface->Data());

    bool result = DidGLErrorOccur("when reading pixels into surface");

    if (oldPackAlignment != 4)
        mGL->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, oldPackAlignment);

    return result;
}

#define CLEANUP_IF_GLERROR_OCCURRED(x)                                      \
    if (DidGLErrorOccur(x)) {                                               \
        isurf = nullptr;                                                    \
        break;                                                              \
    }

already_AddRefed<gfxImageSurface>
GLReadTexImageHelper::ReadTexImage(GLuint aTextureId,
                                   GLenum aTextureTarget,
                                   const gfxIntSize& aSize,
           /* ShaderProgramType */ int aShaderProgram,
                                   bool aYInvert)
{
    // Check aShaderProgram is in bounds for a layers::ShaderProgramType
    MOZ_ASSERT(0 <= aShaderProgram && aShaderProgram < NumProgramTypes);

    MOZ_ASSERT(aTextureTarget == LOCAL_GL_TEXTURE_2D ||
               aTextureTarget == LOCAL_GL_TEXTURE_EXTERNAL ||
               aTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB);

    mGL->MakeCurrent();

    /* Allocate resulting image surface */
    nsRefPtr<gfxImageSurface> isurf = new gfxImageSurface(aSize, gfxImageFormatARGB32);
    if (!isurf || isurf->CairoStatus()) {
        return nullptr;
    }

    realGLboolean oldBlend, oldScissor;
    GLint oldrb, oldfb, oldprog, oldTexUnit, oldTex;
    GLuint rb, fb;

    do {
        /* Save current GL state */
        oldBlend = mGL->fIsEnabled(LOCAL_GL_BLEND);
        oldScissor = mGL->fIsEnabled(LOCAL_GL_SCISSOR_TEST);

        mGL->fGetIntegerv(LOCAL_GL_RENDERBUFFER_BINDING, &oldrb);
        mGL->fGetIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, &oldfb);
        mGL->fGetIntegerv(LOCAL_GL_CURRENT_PROGRAM, &oldprog);
        mGL->fGetIntegerv(LOCAL_GL_ACTIVE_TEXTURE, &oldTexUnit);
        mGL->fActiveTexture(LOCAL_GL_TEXTURE0);
        switch (aTextureTarget) {
        case LOCAL_GL_TEXTURE_2D:
            mGL->fGetIntegerv(LOCAL_GL_TEXTURE_BINDING_2D, &oldTex);
            break;
        case LOCAL_GL_TEXTURE_EXTERNAL:
            mGL->fGetIntegerv(LOCAL_GL_TEXTURE_BINDING_EXTERNAL, &oldTex);
            break;
        case LOCAL_GL_TEXTURE_RECTANGLE:
            mGL->fGetIntegerv(LOCAL_GL_TEXTURE_BINDING_RECTANGLE, &oldTex);
            break;
        default: /* Already checked above */
            break;
        }

        /* Set required GL state */
        mGL->fDisable(LOCAL_GL_BLEND);
        mGL->fDisable(LOCAL_GL_SCISSOR_TEST);

        mGL->PushViewportRect(nsIntRect(0, 0, aSize.width, aSize.height));

        /* Setup renderbuffer */
        mGL->fGenRenderbuffers(1, &rb);
        mGL->fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, rb);

        GLenum rbInternalFormat =
            mGL->IsGLES2()
                ? (mGL->IsExtensionSupported(GLContext::OES_rgb8_rgba8) ? LOCAL_GL_RGBA8 : LOCAL_GL_RGBA4)
                : LOCAL_GL_RGBA;
        mGL->fRenderbufferStorage(LOCAL_GL_RENDERBUFFER, rbInternalFormat, aSize.width, aSize.height);
        CLEANUP_IF_GLERROR_OCCURRED("when binding and creating renderbuffer");

        /* Setup framebuffer */
        mGL->fGenFramebuffers(1, &fb);
        mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, fb);
        mGL->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                                      LOCAL_GL_RENDERBUFFER, rb);
        CLEANUP_IF_GLERROR_OCCURRED("when binding and creating framebuffer");

        MOZ_ASSERT(mGL->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER) == LOCAL_GL_FRAMEBUFFER_COMPLETE);

        /* Setup vertex and fragment shader */
        layers::ShaderProgramType shaderProgram = (layers::ShaderProgramType) aShaderProgram;
        GLuint program = TextureImageProgramFor(aTextureTarget, shaderProgram);
        MOZ_ASSERT(program);

        mGL->fUseProgram(program);
        CLEANUP_IF_GLERROR_OCCURRED("when using program");
        mGL->fUniform1i(mGL->fGetUniformLocation(program, "uTexture"), 0);
        CLEANUP_IF_GLERROR_OCCURRED("when setting uniform location");

        /* Setup quad geometry */
        mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
        mGL->fEnableVertexAttribArray(0);
        mGL->fEnableVertexAttribArray(1);

        float w = (aTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE) ? (float) aSize.width : 1.0f;
        float h = (aTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE) ? (float) aSize.height : 1.0f;


        const float
        vertexArray[4*2] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            -1.0f,  1.0f,
            1.0f,  1.0f
         };
        mGL->fVertexAttribPointer(0, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, vertexArray);

        const float u0 = 0.0f;
        const float u1 = w;
        const float v0 = aYInvert ? h : 0.0f;
        const float v1 = aYInvert ? 0.0f : h;
        const float texCoordArray[8] = { u0, v0,
                                         u1, v0,
                                         u0, v1,
                                         u1, v1 };
        mGL->fVertexAttribPointer(1, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, texCoordArray);

        /* Bind the texture */
        if (aTextureId) {
            mGL->fBindTexture(aTextureTarget, aTextureId);
            CLEANUP_IF_GLERROR_OCCURRED("when binding texture");
        }

        /* Draw quad */
        mGL->fClearColor(1.0f, 0.0f, 1.0f, 1.0f);
        mGL->fClear(LOCAL_GL_COLOR_BUFFER_BIT);
        CLEANUP_IF_GLERROR_OCCURRED("when clearing color buffer");

        mGL->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);
        CLEANUP_IF_GLERROR_OCCURRED("when drawing texture");

        mGL->fDisableVertexAttribArray(1);
        mGL->fDisableVertexAttribArray(0);

        /* Read-back draw results */
        ReadBackPixelsIntoSurface(isurf, aSize);
        CLEANUP_IF_GLERROR_OCCURRED("when reading pixels into surface");
    } while (false);

    /* Restore GL state */
//cleanup:
    mGL->fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, oldrb);
    mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, oldfb);
    mGL->fUseProgram(oldprog);

    // note that deleting 0 has no effect in any of these calls
    mGL->fDeleteRenderbuffers(1, &rb);
    mGL->fDeleteFramebuffers(1, &fb);

    if (oldBlend)
        mGL->fEnable(LOCAL_GL_BLEND);

    if (oldScissor)
        mGL->fEnable(LOCAL_GL_SCISSOR_TEST);

    if (aTextureId)
        mGL->fBindTexture(aTextureTarget, oldTex);

    if (oldTexUnit != LOCAL_GL_TEXTURE0)
        mGL->fActiveTexture(oldTexUnit);

    mGL->PopViewportRect();

    return isurf.forget();
}

#undef CLEANUP_IF_GLERROR_OCCURRED


}
}
