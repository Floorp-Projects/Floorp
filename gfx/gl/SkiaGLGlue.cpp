/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "skia/include/gpu/GrContext.h"
#include "skia/include/gpu/gl/GrGLInterface.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/DebugOnly.h"

/* SkPostConfig.h includes windows.h, which includes windef.h
 * which redefines min/max. We don't want that. */
#ifdef _WIN32
#undef min
#undef max
#endif

#include "GLContext.h"
#include "SkiaGLGlue.h"

using mozilla::gl::GLContext;
using mozilla::gl::GLFeature;
using mozilla::gl::SkiaGLGlue;

template<typename R, typename... A>
static inline GrGLFunction<R (*)(A...)>
WrapGL(RefPtr<GLContext> aContext, R (GLContext::*aFunc)(A...))
{
  return [aContext, aFunc] (A... args) -> R
  {
    aContext->MakeCurrent();
    return (aContext->*aFunc)(args...);
  };
}

template<typename R, typename... A>
static inline GrGLFunction<R (*)(A...)>
WrapGL(RefPtr<GLContext> aContext, R (*aFunc)(GLContext*, A...))
{
  return [aContext, aFunc] (A... args) -> R
  {
    aContext->MakeCurrent();
    return (*aFunc)(aContext, args...);
  };
}

// Core GL functions required by Ganesh

static const GLubyte*
glGetString_mozilla(GLContext* aContext, GrGLenum aName)
{
    // GLContext only exposes a OpenGL 2.0 style API, so we have to intercept a bunch
    // of checks that Ganesh makes to determine which capabilities are present
    // on the GL implementation and change them to match what GLContext actually exposes.

    if (aName == LOCAL_GL_VERSION) {
        if (aContext->IsGLES()) {
            return reinterpret_cast<const GLubyte*>("OpenGL ES 2.0");
        }
        return reinterpret_cast<const GLubyte*>("2.0");
    } else if (aName == LOCAL_GL_EXTENSIONS) {
        // Only expose the bare minimum extensions we want to support to ensure a functional Ganesh
        // as GLContext only exposes certain extensions
        static bool extensionsStringBuilt = false;
        static char extensionsString[1024];

        if (!extensionsStringBuilt) {
            extensionsString[0] = '\0';

            if (aContext->IsGLES()) {
                // OES is only applicable to GLES2
                if (aContext->IsExtensionSupported(GLContext::OES_packed_depth_stencil)) {
                    strcat(extensionsString, "GL_OES_packed_depth_stencil ");
                }

                if (aContext->IsExtensionSupported(GLContext::OES_rgb8_rgba8)) {
                    strcat(extensionsString, "GL_OES_rgb8_rgba8 ");
                }

                if (aContext->IsExtensionSupported(GLContext::OES_texture_npot)) {
                    strcat(extensionsString, "GL_OES_texture_npot ");
                }

                if (aContext->IsExtensionSupported(GLContext::OES_vertex_array_object)) {
                    strcat(extensionsString, "GL_OES_vertex_array_object ");
                }

                if (aContext->IsSupported(GLFeature::standard_derivatives)) {
                    strcat(extensionsString, "GL_OES_standard_derivatives ");
                }
            } else {
                if (aContext->IsSupported(GLFeature::framebuffer_object)) {
                    strcat(extensionsString, "GL_ARB_framebuffer_object ");
                } else if (aContext->IsExtensionSupported(GLContext::EXT_framebuffer_object)) {
                    strcat(extensionsString, "GL_EXT_framebuffer_object ");
                }

                if (aContext->IsSupported(GLFeature::texture_rg)) {
                    strcat(extensionsString, "GL_ARB_texture_rg ");
                }
            }

            if (aContext->IsExtensionSupported(GLContext::EXT_texture_format_BGRA8888)) {
                strcat(extensionsString, "GL_EXT_texture_format_BGRA8888 ");
            }

            if (aContext->IsExtensionSupported(GLContext::EXT_packed_depth_stencil)) {
                strcat(extensionsString, "GL_EXT_packed_depth_stencil ");
            }

            if (aContext->IsExtensionSupported(GLContext::EXT_bgra)) {
                strcat(extensionsString, "GL_EXT_bgra ");
            }

            if (aContext->IsExtensionSupported(GLContext::EXT_read_format_bgra)) {
                strcat(extensionsString, "GL_EXT_read_format_bgra ");
            }

            extensionsStringBuilt = true;
#ifdef DEBUG
            printf_stderr("Exported SkiaGL extensions: %s\n", extensionsString);
#endif
        }

        return reinterpret_cast<const GLubyte*>(extensionsString);

    } else if (aName == LOCAL_GL_SHADING_LANGUAGE_VERSION) {
        if (aContext->IsGLES()) {
            return reinterpret_cast<const GLubyte*>("OpenGL ES GLSL ES 1.0");
        }
        return reinterpret_cast<const GLubyte*>("1.10");
    }

    return aContext->fGetString(aName);
}

static GrGLInterface* CreateGrGLInterfaceFromGLContext(GLContext* context)
{
    auto *i = new GrGLInterface();

    context->MakeCurrent();

    // We support both desktop GL and GLES2
    if (context->IsGLES()) {
        i->fStandard = kGLES_GrGLStandard;
    } else {
        i->fStandard = kGL_GrGLStandard;
    }

    GrGLFunction<GrGLGetStringProc> getString = WrapGL(context, &glGetString_mozilla);
    GrGLFunction<GrGLGetIntegervProc> getIntegerv = WrapGL(context, &GLContext::fGetIntegerv);

    GrGLExtensions extensions;
    if (!extensions.init(i->fStandard, getString, nullptr, getIntegerv)) {
        delete i;
        return nullptr;
    }

    i->fExtensions.swap(&extensions);

    // Core GL functions required by Ganesh
    i->fFunctions.fActiveTexture = WrapGL(context, &GLContext::fActiveTexture);
    i->fFunctions.fAttachShader = WrapGL(context, &GLContext::fAttachShader);
    i->fFunctions.fBindAttribLocation = WrapGL(context, &GLContext::fBindAttribLocation);
    i->fFunctions.fBindBuffer = WrapGL(context, &GLContext::fBindBuffer);
    i->fFunctions.fBindFramebuffer = WrapGL(context, &GLContext::fBindFramebuffer);
    i->fFunctions.fBindRenderbuffer = WrapGL(context, &GLContext::fBindRenderbuffer);
    i->fFunctions.fBindTexture = WrapGL(context, &GLContext::fBindTexture);
    i->fFunctions.fBlendFunc = WrapGL(context, &GLContext::fBlendFunc);
    i->fFunctions.fBlendColor = WrapGL(context, &GLContext::fBlendColor);
    i->fFunctions.fBlendEquation = WrapGL(context, &GLContext::fBlendEquation);
    i->fFunctions.fBufferData = WrapGL(context, &GLContext::fBufferData);
    i->fFunctions.fBufferSubData = WrapGL(context, &GLContext::fBufferSubData);
    i->fFunctions.fCheckFramebufferStatus = WrapGL(context, &GLContext::fCheckFramebufferStatus);
    i->fFunctions.fClear = WrapGL(context, &GLContext::fClear);
    i->fFunctions.fClearColor = WrapGL(context, &GLContext::fClearColor);
    i->fFunctions.fClearStencil = WrapGL(context, &GLContext::fClearStencil);
    i->fFunctions.fColorMask = WrapGL(context, &GLContext::fColorMask);
    i->fFunctions.fCompileShader = WrapGL(context, &GLContext::fCompileShader);
    i->fFunctions.fCopyTexSubImage2D = WrapGL(context, &GLContext::fCopyTexSubImage2D);
    i->fFunctions.fCreateProgram = WrapGL(context, &GLContext::fCreateProgram);
    i->fFunctions.fCreateShader = WrapGL(context, &GLContext::fCreateShader);
    i->fFunctions.fCullFace = WrapGL(context, &GLContext::fCullFace);
    i->fFunctions.fDeleteBuffers = WrapGL(context, &GLContext::fDeleteBuffers);
    i->fFunctions.fDeleteFramebuffers = WrapGL(context, &GLContext::fDeleteFramebuffers);
    i->fFunctions.fDeleteProgram = WrapGL(context, &GLContext::fDeleteProgram);
    i->fFunctions.fDeleteRenderbuffers = WrapGL(context, &GLContext::fDeleteRenderbuffers);
    i->fFunctions.fDeleteShader = WrapGL(context, &GLContext::fDeleteShader);
    i->fFunctions.fDeleteTextures = WrapGL(context, &GLContext::fDeleteTextures);
    i->fFunctions.fDepthMask = WrapGL(context, &GLContext::fDepthMask);
    i->fFunctions.fDisable = WrapGL(context, &GLContext::fDisable);
    i->fFunctions.fDisableVertexAttribArray = WrapGL(context, &GLContext::fDisableVertexAttribArray);
    i->fFunctions.fDrawArrays = WrapGL(context, &GLContext::fDrawArrays);
    i->fFunctions.fDrawElements = WrapGL(context, &GLContext::fDrawElements);
    i->fFunctions.fDrawRangeElements = WrapGL(context, &GLContext::fDrawRangeElements);
    i->fFunctions.fEnable = WrapGL(context, &GLContext::fEnable);
    i->fFunctions.fEnableVertexAttribArray = WrapGL(context, &GLContext::fEnableVertexAttribArray);
    i->fFunctions.fFinish = WrapGL(context, &GLContext::fFinish);
    i->fFunctions.fFlush = WrapGL(context, &GLContext::fFlush);
    i->fFunctions.fFramebufferRenderbuffer = WrapGL(context, &GLContext::fFramebufferRenderbuffer);
    i->fFunctions.fFramebufferTexture2D = WrapGL(context, &GLContext::fFramebufferTexture2D);
    i->fFunctions.fFrontFace = WrapGL(context, &GLContext::fFrontFace);
    i->fFunctions.fGenBuffers = WrapGL(context, &GLContext::fGenBuffers);
    i->fFunctions.fGenFramebuffers = WrapGL(context, &GLContext::fGenFramebuffers);
    i->fFunctions.fGenRenderbuffers = WrapGL(context, &GLContext::fGenRenderbuffers);
    i->fFunctions.fGetFramebufferAttachmentParameteriv = WrapGL(context, &GLContext::fGetFramebufferAttachmentParameteriv);
    i->fFunctions.fGenTextures = WrapGL(context, &GLContext::fGenTextures);
    i->fFunctions.fGenerateMipmap = WrapGL(context, &GLContext::fGenerateMipmap);
    i->fFunctions.fGetBufferParameteriv = WrapGL(context, &GLContext::fGetBufferParameteriv);
    i->fFunctions.fGetError = WrapGL(context, &GLContext::fGetError);
    i->fFunctions.fGetIntegerv = getIntegerv;
    i->fFunctions.fGetProgramInfoLog = WrapGL(context, &GLContext::fGetProgramInfoLog);
    i->fFunctions.fGetProgramiv = WrapGL(context, &GLContext::fGetProgramiv);
    i->fFunctions.fGetRenderbufferParameteriv = WrapGL(context, &GLContext::fGetRenderbufferParameteriv);
    i->fFunctions.fGetShaderInfoLog = WrapGL(context, &GLContext::fGetShaderInfoLog);
    i->fFunctions.fGetShaderiv = WrapGL(context, &GLContext::fGetShaderiv);
    i->fFunctions.fGetShaderPrecisionFormat = WrapGL(context, &GLContext::fGetShaderPrecisionFormat);
    i->fFunctions.fGetString = getString;
    i->fFunctions.fGetUniformLocation = WrapGL(context, &GLContext::fGetUniformLocation);
    i->fFunctions.fLineWidth = WrapGL(context, &GLContext::fLineWidth);
    i->fFunctions.fLinkProgram = WrapGL(context, &GLContext::fLinkProgram);
    i->fFunctions.fPixelStorei = WrapGL(context, &GLContext::fPixelStorei);
    i->fFunctions.fPolygonMode = WrapGL(context, &GLContext::fPolygonMode);
    i->fFunctions.fReadPixels = WrapGL(context, &GLContext::fReadPixels);
    i->fFunctions.fRenderbufferStorage = WrapGL(context, &GLContext::fRenderbufferStorage);
    i->fFunctions.fScissor = WrapGL(context, &GLContext::fScissor);
    i->fFunctions.fShaderSource = WrapGL(context, &GLContext::fShaderSource);
    i->fFunctions.fStencilFunc = WrapGL(context, &GLContext::fStencilFunc);
    i->fFunctions.fStencilMask = WrapGL(context, &GLContext::fStencilMask);
    i->fFunctions.fStencilOp = WrapGL(context, &GLContext::fStencilOp);
    i->fFunctions.fTexImage2D = WrapGL(context, &GLContext::fTexImage2D);
    i->fFunctions.fTexParameteri = WrapGL(context, &GLContext::fTexParameteri);
    i->fFunctions.fTexParameteriv = WrapGL(context, &GLContext::fTexParameteriv);
    i->fFunctions.fTexSubImage2D = WrapGL(context, &GLContext::fTexSubImage2D);
    i->fFunctions.fUniform1f = WrapGL(context, &GLContext::fUniform1f);
    i->fFunctions.fUniform1i = WrapGL(context, &GLContext::fUniform1i);
    i->fFunctions.fUniform1fv = WrapGL(context, &GLContext::fUniform1fv);
    i->fFunctions.fUniform1iv = WrapGL(context, &GLContext::fUniform1iv);
    i->fFunctions.fUniform2f = WrapGL(context, &GLContext::fUniform2f);
    i->fFunctions.fUniform2i = WrapGL(context, &GLContext::fUniform2i);
    i->fFunctions.fUniform2fv = WrapGL(context, &GLContext::fUniform2fv);
    i->fFunctions.fUniform2iv = WrapGL(context, &GLContext::fUniform2iv);
    i->fFunctions.fUniform3f = WrapGL(context, &GLContext::fUniform3f);
    i->fFunctions.fUniform3i = WrapGL(context, &GLContext::fUniform3i);
    i->fFunctions.fUniform3fv = WrapGL(context, &GLContext::fUniform3fv);
    i->fFunctions.fUniform3iv = WrapGL(context, &GLContext::fUniform3iv);
    i->fFunctions.fUniform4f = WrapGL(context, &GLContext::fUniform4f);
    i->fFunctions.fUniform4i = WrapGL(context, &GLContext::fUniform4i);
    i->fFunctions.fUniform4fv = WrapGL(context, &GLContext::fUniform4fv);
    i->fFunctions.fUniform4iv = WrapGL(context, &GLContext::fUniform4iv);
    i->fFunctions.fUniformMatrix2fv = WrapGL(context, &GLContext::fUniformMatrix2fv);
    i->fFunctions.fUniformMatrix3fv = WrapGL(context, &GLContext::fUniformMatrix3fv);
    i->fFunctions.fUniformMatrix4fv = WrapGL(context, &GLContext::fUniformMatrix4fv);
    i->fFunctions.fUseProgram = WrapGL(context, &GLContext::fUseProgram);
    i->fFunctions.fVertexAttrib1f = WrapGL(context, &GLContext::fVertexAttrib1f);
    i->fFunctions.fVertexAttrib2fv = WrapGL(context, &GLContext::fVertexAttrib2fv);
    i->fFunctions.fVertexAttrib3fv = WrapGL(context, &GLContext::fVertexAttrib3fv);
    i->fFunctions.fVertexAttrib4fv = WrapGL(context, &GLContext::fVertexAttrib4fv);
    i->fFunctions.fVertexAttribPointer = WrapGL(context, &GLContext::fVertexAttribPointer);
    i->fFunctions.fViewport = WrapGL(context, &GLContext::fViewport);

    // Required for either desktop OpenGL 2.0 or OpenGL ES 2.0
    i->fFunctions.fStencilFuncSeparate = WrapGL(context, &GLContext::fStencilFuncSeparate);
    i->fFunctions.fStencilMaskSeparate = WrapGL(context, &GLContext::fStencilMaskSeparate);
    i->fFunctions.fStencilOpSeparate = WrapGL(context, &GLContext::fStencilOpSeparate);

    // GLContext supports glMapBuffer
    i->fFunctions.fMapBuffer = WrapGL(context, &GLContext::fMapBuffer);
    i->fFunctions.fUnmapBuffer = WrapGL(context, &GLContext::fUnmapBuffer);

    // GLContext supports glRenderbufferStorageMultisample/glBlitFramebuffer
    i->fFunctions.fRenderbufferStorageMultisample = WrapGL(context, &GLContext::fRenderbufferStorageMultisample);
    i->fFunctions.fBlitFramebuffer = WrapGL(context, &GLContext::fBlitFramebuffer);

    // GLContext supports glCompressedTexImage2D
    i->fFunctions.fCompressedTexImage2D = WrapGL(context, &GLContext::fCompressedTexImage2D);

    // GL_OES_vertex_array_object
    i->fFunctions.fBindVertexArray = WrapGL(context, &GLContext::fBindVertexArray);
    i->fFunctions.fDeleteVertexArrays = WrapGL(context, &GLContext::fDeleteVertexArrays);
    i->fFunctions.fGenVertexArrays = WrapGL(context, &GLContext::fGenVertexArrays);

    // Desktop GL
    i->fFunctions.fGetTexLevelParameteriv = WrapGL(context, &GLContext::fGetTexLevelParameteriv);
    i->fFunctions.fDrawBuffer = WrapGL(context, &GLContext::fDrawBuffer);
    i->fFunctions.fReadBuffer = WrapGL(context, &GLContext::fReadBuffer);

    // Desktop OpenGL > 1.5
    i->fFunctions.fGenQueries = WrapGL(context, &GLContext::fGenQueries);
    i->fFunctions.fDeleteQueries = WrapGL(context, &GLContext::fDeleteQueries);
    i->fFunctions.fBeginQuery = WrapGL(context, &GLContext::fBeginQuery);
    i->fFunctions.fEndQuery = WrapGL(context, &GLContext::fEndQuery);
    i->fFunctions.fGetQueryiv = WrapGL(context, &GLContext::fGetQueryiv);
    i->fFunctions.fGetQueryObjectiv = WrapGL(context, &GLContext::fGetQueryObjectiv);
    i->fFunctions.fGetQueryObjectuiv = WrapGL(context, &GLContext::fGetQueryObjectuiv);

    // Desktop OpenGL > 2.0
    i->fFunctions.fDrawBuffers = WrapGL(context, &GLContext::fDrawBuffers);

    return i;
}

SkiaGLGlue::SkiaGLGlue(GLContext* context)
    : mGLContext(context)
{
    mGrGLInterface.reset(CreateGrGLInterfaceFromGLContext(mGLContext));
    mGrContext.reset(GrContext::Create(kOpenGL_GrBackend, (GrBackendContext)mGrGLInterface.get()));
}

SkiaGLGlue::~SkiaGLGlue()
{
    /*
     * These members have inter-dependencies, but do not keep each other alive, so
     * destruction order is very important here: mGrContext uses mGrGLInterface, and
     * through it, uses mGLContext
     */
    mGrContext = nullptr;
    if (mGrGLInterface) {
      // Ensure that no references to the GLContext remain, even if the GrContext still lives.
      mGrGLInterface->fFunctions = GrGLInterface::Functions();
      mGrGLInterface = nullptr;
    }
    mGLContext = nullptr;
}
